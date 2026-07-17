#include "phantomledger/exporter/sinks/run_ledger.hpp"

#include <charconv>
#include <format>

namespace PhantomLedger::exporter::sinks {

namespace {

[[nodiscard]] std::string formatDate(::PhantomLedger::time::CalendarDate d) {
  return std::format("{:04d}-{:02d}-{:02d}", d.year,
                     static_cast<unsigned>(d.month),
                     static_cast<unsigned>(d.day));
}

[[nodiscard]] std::uint64_t parseU64(std::string_view s) {
  std::uint64_t value = 0;
  const auto *end = s.data() + s.size();
  const auto [ptr, ec] = std::from_chars(s.data(), end, value);
  if (ec != std::errc{} || ptr != end) {
    throw std::runtime_error("RunLedger: malformed number in ledger row: " +
                             std::string{s});
  }
  return value;
}

} // namespace

RunLedger::RunLedger(postgres::Connection &conn) : RunLedger(conn, Tables{}) {}

RunLedger::RunLedger(postgres::Connection &conn, Tables tables)
    : conn_(&conn), tables_(std::move(tables)) {}

void RunLedger::ensureTables() {
  const auto manifest = conn_->escapeIdentifier(tables_.manifest);
  const auto spans = conn_->escapeIdentifier(tables_.spans);

  conn_->exec("CREATE TABLE IF NOT EXISTS " + manifest +
              " ("
              " id bigserial PRIMARY KEY,"
              " started timestamptz NOT NULL DEFAULT now(),"
              " finished timestamptz,"
              " seed text NOT NULL,"
              " population integer NOT NULL,"
              " days integer NOT NULL,"
              " start_date text NOT NULL,"
              " txn_rows bigint,"
              " stream_digest text,"
              " config_hash text,"
              " status text NOT NULL DEFAULT 'legacy')");
  // Pre-checkpoint databases lack the two run-state columns; migrate in
  // place. Old rows read status 'legacy' with NULL config_hash, so they
  // can never satisfy a resume lookup.
  conn_->exec("ALTER TABLE " + manifest +
              " ADD COLUMN IF NOT EXISTS config_hash text");
  conn_->exec("ALTER TABLE " + manifest +
              " ADD COLUMN IF NOT EXISTS status text NOT NULL "
              "DEFAULT 'legacy'");

  conn_->exec("CREATE TABLE IF NOT EXISTS " + spans +
              " ("
              " manifest_id bigint NOT NULL,"
              " span_index integer NOT NULL,"
              " txn_rows bigint NOT NULL,"
              " finished timestamptz NOT NULL DEFAULT now(),"
              " span_digest text,"
              " PRIMARY KEY (manifest_id, span_index))");
  conn_->exec("ALTER TABLE " + spans +
              " ADD COLUMN IF NOT EXISTS span_digest text");
}

std::string RunLedger::configHash(std::string_view engine, std::uint64_t seed,
                                  std::int32_t population, std::int64_t days,
                                  ::PhantomLedger::time::CalendarDate start) {
  // Everything that determines the stream bytes, versioned so a future
  // stream-format change invalidates old checkpoints instead of
  // colliding with them.
  const auto canonical =
      std::format("pl-stream-v1|engine={}|seed={}|population={}|days={}|"
                  "start={}",
                  engine, seed, population, days, formatDate(start));

  crypto::blake2b::Stream hasher{LedgerSpanHasher::kDigestBytes};
  std::array<std::uint8_t, LedgerSpanHasher::kDigestBytes> bytes{};
  if (!hasher.update(canonical.data(), canonical.size()) ||
      !hasher.finalize(bytes.data(), bytes.size())) {
    throw std::logic_error("RunLedger::configHash: hash failed");
  }
  return detail::hexDigest(bytes);
}

void RunLedger::supersedeRunning() {
  conn_->exec("UPDATE " + conn_->escapeIdentifier(tables_.manifest) +
              " SET status = 'superseded' WHERE status = 'running'");
  // Span rows are resume state, meaningful only for in-flight runs;
  // after the update there are none, so this bounds pl_run_spans for
  // good (and sweeps up rows from failed/legacy/pre-pruning eras).
  conn_->exec("DELETE FROM " + conn_->escapeIdentifier(tables_.spans) +
              " WHERE manifest_id NOT IN (SELECT id FROM " +
              conn_->escapeIdentifier(tables_.manifest) +
              " WHERE status = 'running')");
}

long long RunLedger::beginRun(const std::string &configHash,
                              std::uint64_t seed, std::int32_t population,
                              std::int64_t days,
                              ::PhantomLedger::time::CalendarDate start) {
  const auto id = conn_->queryValue(
      "INSERT INTO " + conn_->escapeIdentifier(tables_.manifest) +
      " (seed, population, days, start_date, config_hash, status) VALUES ('" +
      std::to_string(seed) + "', " + std::to_string(population) + ", " +
      std::to_string(days) + ", '" + formatDate(start) + "', '" + configHash +
      "', 'running') RETURNING id");
  return static_cast<long long>(parseU64(id));
}

std::optional<ResumePlan>
RunLedger::findResumable(const std::string &configHash) {
  const auto manifest = conn_->escapeIdentifier(tables_.manifest);
  const auto spans = conn_->escapeIdentifier(tables_.spans);

  const auto id = conn_->queryValue(
      "SELECT coalesce(max(id), -1) FROM " + manifest +
      " WHERE status = 'running' AND config_hash = '" + configHash + "'");
  if (id == "-1") {
    return std::nullopt;
  }

  // One round trip for the whole span journal; digests are hex, so the
  // ':' / ';' separators can never appear inside a field.
  const auto packed = conn_->queryValue(
      "SELECT coalesce(string_agg(span_index::text || ':' || txn_rows::text "
      "|| ':' || coalesce(span_digest, ''), ';' ORDER BY span_index), '') "
      "FROM " +
      spans + " WHERE manifest_id = " + id);
  if (packed.empty()) {
    return std::nullopt; // crashed before any span committed
  }

  ResumePlan plan;
  plan.manifestId = static_cast<long long>(parseU64(id));

  std::string_view rest{packed};
  while (!rest.empty()) {
    const auto item = rest.substr(0, rest.find(';'));
    rest.remove_prefix(item.size() + (item.size() < rest.size() ? 1 : 0));

    const auto firstColon = item.find(':');
    const auto secondColon = item.find(':', firstColon + 1);
    if (firstColon == std::string_view::npos ||
        secondColon == std::string_view::npos) {
      throw std::runtime_error("RunLedger: malformed span journal entry: " +
                               std::string{item});
    }
    CommittedSpan span;
    span.index =
        static_cast<std::uint32_t>(parseU64(item.substr(0, firstColon)));
    span.rows =
        parseU64(item.substr(firstColon + 1, secondColon - firstColon - 1));
    span.digest = std::string{item.substr(secondColon + 1)};
    plan.spans.push_back(std::move(span));
  }

  // Resumable means: a contiguous prefix 0..k-1 with a digest for every
  // span. Anything else (legacy rows, reference-engine runs, journal
  // corruption) is not resumable.
  for (std::size_t i = 0; i < plan.spans.size(); ++i) {
    if (plan.spans[i].index != i || plan.spans[i].digest.empty()) {
      return std::nullopt;
    }
    plan.rows += plan.spans[i].rows;
  }
  if (plan.rows == 0) {
    return std::nullopt;
  }
  return plan;
}

bool RunLedger::prepareResume(const ResumePlan &plan,
                              const std::string &txnTable) {
  try {
    const auto table = conn_->escapeIdentifier(txnTable);
    const auto rows = std::to_string(plan.rows);

    // Trim the uncommitted tail: a span whose COPY landed but whose
    // journal row didn't (crash between the two) is deleted here and
    // regenerated identically.
    conn_->exec("DELETE FROM " + table + " WHERE row_seq > " + rows);

    if (conn_->queryValue("SELECT count(*) FROM " + table) != rows) {
      return false;
    }
    if (conn_->queryValue("SELECT coalesce(max(row_seq), 0) FROM " + table) !=
        rows) {
      return false;
    }
    if (conn_->queryValue("SELECT count(DISTINCT row_seq) FROM " + table) !=
        rows) {
      return false;
    }

    // Per-span row profile must match the journal exactly.
    std::string expected;
    for (const auto &span : plan.spans) {
      if (!expected.empty()) {
        expected.push_back(';');
      }
      expected += std::to_string(span.index) + ':' + std::to_string(span.rows);
    }
    const auto profile = conn_->queryValue(
        "SELECT coalesce(string_agg(span_index::text || ':' || cnt::text, "
        "';' ORDER BY span_index), '') FROM (SELECT span_index, count(*) AS "
        "cnt FROM " +
        table + " GROUP BY span_index) q");
    return profile == expected;
  } catch (const postgres::Error &) {
    // Table missing or unreadable: unresumable, never fatal — the
    // caller rewrites in full.
    return false;
  }
}

void RunLedger::recordSpan(long long manifestId, std::uint32_t spanIndex,
                           std::uint64_t rows, const std::string &digest) {
  conn_->exec("INSERT INTO " + conn_->escapeIdentifier(tables_.spans) +
              " (manifest_id, span_index, txn_rows, span_digest) VALUES (" +
              std::to_string(manifestId) + ", " + std::to_string(spanIndex) +
              ", " + std::to_string(rows) + ", " +
              (digest.empty() ? std::string{"NULL"} : "'" + digest + "'") +
              ")");
}

void RunLedger::finishRun(long long manifestId, std::uint64_t rows,
                          const std::string &digest) {
  conn_->exec("UPDATE " + conn_->escapeIdentifier(tables_.manifest) +
              " SET finished = now(), txn_rows = " + std::to_string(rows) +
              ", stream_digest = '" + digest +
              "', status = 'complete' WHERE id = " +
              std::to_string(manifestId));
  pruneSpans(manifestId);
}

void RunLedger::markFailed(long long manifestId) {
  conn_->exec("UPDATE " + conn_->escapeIdentifier(tables_.manifest) +
              " SET status = 'failed' WHERE id = " +
              std::to_string(manifestId));
  pruneSpans(manifestId);
}

void RunLedger::pruneSpans(long long manifestId) {
  conn_->exec("DELETE FROM " + conn_->escapeIdentifier(tables_.spans) +
              " WHERE manifest_id = " + std::to_string(manifestId));
}

} // namespace PhantomLedger::exporter::sinks
