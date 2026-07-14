#include "phantomledger/pipeline/chunk/sink.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace PhantomLedger;
using pipeline::chunk::NullSink;
using pipeline::chunk::Schedule;
using pipeline::chunk::Sink;
using pipeline::chunk::Span;
using transactions::Transaction;

namespace {

// A user-defined sink proving the concept accepts foreign types and
// that the driver-side protocol arrives in order.
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

  std::puts("sink: all assertions passed");
  return 0;
}
