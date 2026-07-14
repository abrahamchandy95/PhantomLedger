#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/exporter/sinks/golden.hpp"
#include "phantomledger/pipeline/chunk/flush.hpp"
#include "phantomledger/pipeline/chunk/sink.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace PhantomLedger;
using pipeline::chunk::NullSink;
using pipeline::chunk::Schedule;
using pipeline::chunk::Sink;
using pipeline::chunk::Span;
using pipeline::chunk::Tee;
using transactions::Transaction;

namespace {

struct Recorder {
  std::vector<std::uint32_t> begun;
  std::vector<std::uint32_t> ended;
  std::uint64_t rows = 0;
  bool finished = false;

  void beginSpan(const Span &s) { begun.push_back(s.index); }
  void append(std::span<const Transaction> txns) { rows += txns.size(); }
  void endSpan(const Span &s) { ended.push_back(s.index); }
  void finish() { finished = true; }
  [[nodiscard]] std::uint64_t rowsWritten() const noexcept { return rows; }
};

static_assert(Sink<NullSink>);
static_assert(Sink<Recorder>);
static_assert(Sink<Tee<NullSink, Recorder>>);

} // namespace

int main() {
  time::Window run{time::makeTime({2025, 1, 15}), 90};
  const auto sched = Schedule::partition(run, {});
  assert(sched.size() == 4);

  std::vector<Transaction> batch(3); // content-free rows; counting only

  NullSink nullSink;
  Recorder rec;
  for (const auto &span : sched) {
    nullSink.beginSpan(span);
    rec.beginSpan(span);
    nullSink.append(batch);
    rec.append(batch);
    nullSink.endSpan(span);
    rec.endSpan(span);
  }
  nullSink.finish();
  rec.finish();

  assert(nullSink.rowsWritten() == 12);
  assert(rec.rowsWritten() == 12);
  assert(rec.finished);
  assert(rec.begun.size() == 4 && rec.ended.size() == 4);
  for (std::uint32_t i = 0; i < 4; ++i) {
    assert(rec.begun[i] == i && rec.ended[i] == i);
  }

  // Tee forwards the identical protocol to both sinks.
  {
    NullSink n;
    Recorder r;
    Tee tee{n, r};
    for (const auto &span : sched) {
      tee.beginSpan(span);
      tee.append(batch);
      tee.endSpan(span);
    }
    tee.finish();
    assert(n.rowsWritten() == 12 && r.rowsWritten() == 12);
    assert(r.finished && r.begun.size() == 4 && r.ended.size() == 4);
    assert(tee.rowsWritten() == 12);
  }

  // flushUnpartitioned: one span, whole stream, full protocol.
  {
    const auto whole = Schedule::unpartitioned(run);
    Recorder r;
    const auto rows = pipeline::chunk::flushUnpartitioned(
        whole, std::span<const Transaction>{batch.data(), batch.size()}, r);
    assert(rows == 3 && r.finished);
    assert(r.begun.size() == 1 && r.ended.size() == 1 && r.begun[0] == 0);

    bool threw = false;
    Recorder r2;
    try {
      (void)pipeline::chunk::flushUnpartitioned(
          sched, std::span<const Transaction>{batch.data(), batch.size()}, r2);
    } catch (const std::logic_error &) {
      threw = true;
    }
    assert(threw); // flushUnpartitioned stays single-span by contract
  }

  // flushPartitioned: boundary slicing, half-open ownership, tail
  // absorption, sort enforcement, and Golden invariance across
  // schedules (the chunking acceptance criterion in miniature).
  {
    // 62 days from Jan 1: three monthly spans (Jan, Feb, Mar 1-3).
    time::Window window;
    window.start = time::makeTime({.year = 2021, .month = 1, .day = 1});
    window.days = 62;
    const auto sched = pipeline::chunk::Schedule::partition(
        window, pipeline::chunk::Strategy{});
    assert(sched.size() == 3);

    const auto sec = [&](int dayOffset) {
      return (window.start + time::Days{dayOffset}).time_since_epoch().count();
    };

    // Renderable synthetic rows: Golden runs them through the real
    // schema, so keys and ID sequences must be valid.
    const auto mkRow = [](std::uint64_t seq, std::int64_t ts) {
      Transaction tx;
      tx.source =
          entity::makeKey(entity::Role::account, entity::Bank::internal, seq);
      tx.target = entity::makeKey(entity::Role::merchant,
                                  entity::Bank::internal, seq + 100);
      tx.amount = static_cast<double>(seq);
      tx.timestamp = ts;
      return tx;
    };
    std::vector<Transaction> rows;
    rows.push_back(mkRow(1, sec(0)));  // Jan 1        -> span 0
    rows.push_back(mkRow(2, sec(30))); // Jan 31       -> span 0
    rows.push_back(mkRow(3, sec(31))); // Feb 1, 00:00 -> span 1 (half-open)
    rows.push_back(mkRow(4, sec(45))); // mid-Feb      -> span 1
    rows.push_back(mkRow(5, sec(61))); // Mar 3        -> span 2
    rows.push_back(mkRow(6, sec(70))); // past the run -> span 2 (tail)

    struct PerSpan {
      std::vector<std::uint64_t> counts;
    } seen;
    Recorder rec;
    const auto total = pipeline::chunk::flushPartitioned(
        sched, std::span<const Transaction>{rows.data(), rows.size()}, rec,
        [&seen](const pipeline::chunk::Span &span, std::uint64_t n) {
          assert(span.index == seen.counts.size());
          seen.counts.push_back(n);
        });
    assert(total == rows.size());
    assert(seen.counts.size() == 3);
    assert(seen.counts[0] == 2);
    assert(seen.counts[1] == 2);
    assert(seen.counts[2] == 2);

    // Golden invariance: unpartitioned vs monthly schedule, same rows,
    // same digest.
    exporter::sinks::Golden gWhole;
    exporter::sinks::Golden gSpans;
    (void)pipeline::chunk::flushUnpartitioned(
        pipeline::chunk::Schedule::unpartitioned(window),
        std::span<const Transaction>{rows.data(), rows.size()}, gWhole);
    (void)pipeline::chunk::flushPartitioned(
        sched, std::span<const Transaction>{rows.data(), rows.size()}, gSpans);
    assert(gWhole.digest() == gSpans.digest());
    assert(gWhole.rowsWritten() == gSpans.rowsWritten());

    // Unsorted rows are a contract violation, not a silent reorder.
    std::vector<Transaction> shuffled = rows;
    std::swap(shuffled[1], shuffled[4]);
    bool threw = false;
    try {
      Recorder r3;
      (void)pipeline::chunk::flushPartitioned(
          sched, std::span<const Transaction>{shuffled.data(), shuffled.size()},
          r3);
    } catch (const std::logic_error &) {
      threw = true;
    }
    assert(threw);
  }

  std::puts("sink: all assertions passed");
  return 0;
}
