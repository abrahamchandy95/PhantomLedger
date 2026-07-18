#include "phantomledger/primitives/postgres/txn_readback.hpp"

#include "phantomledger/encoding/parse.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/names.hpp"

#include <libpq-fe.h>

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <string>

namespace PhantomLedger::postgres {

namespace {

constexpr const char *kCursorName = "pl_txn_scan";
constexpr int kFetchBatchRows = 4096;

[[nodiscard]] std::uint64_t parseU64Field(std::string_view text,
                                          const char *what) {
  std::uint64_t value = 0;
  const auto [end, ec] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || end != text.data() + text.size()) {
    throw Error(std::string{"txn_readback: bad "} + what + " '" +
                std::string{text} + "'");
  }
  return value;
}

// PQgetvalue is NUL-terminated; strtod under the default "C" locale is
// correctly rounded, so shortest-round-trip float8 text decodes to the
// exact double the sink encoded.
[[nodiscard]] double parseAmountField(const char *text) {
  errno = 0;
  char *end = nullptr;
  const double value = std::strtod(text, &end);
  if (end == text || *end != '\0' || errno == ERANGE) {
    throw Error(std::string{"txn_readback: bad amount '"} + text + "'");
  }
  return value;
}

[[nodiscard]] ::PhantomLedger::entity::Key
parseKeyField(std::string_view text) {
  const auto key = encoding::parseKey(text);
  if (!key.has_value()) {
    throw Error("txn_readback: unparseable account id '" + std::string{text} +
                "'");
  }
  return *key;
}

// The sink writes channels::name(tag) (validated unique, never empty
// for a posted row — the column is NOT NULL and an empty CSV field
// would COPY as NULL), so any unparseable text is table corruption,
// not a decodable value.
[[nodiscard]] ::PhantomLedger::channels::Tag parseChannelField(
    const char *text) {
  const auto tag = ::PhantomLedger::channels::parse(text);
  if (!tag.has_value()) {
    throw Error(std::string{"txn_readback: unknown channel '"} + text + "'");
  }
  return *tag;
}

} // namespace

std::int64_t parseLedgerTimestamp(std::string_view text) {
  const auto fail = [&]() -> Error {
    return Error("txn_readback: bad ledger timestamp '" + std::string{text} +
                 "'");
  };

  if (text.size() != 19 || text[4] != '-' || text[7] != '-' ||
      text[10] != ' ' || text[13] != ':' || text[16] != ':') {
    throw fail();
  }

  const auto number = [&](std::size_t pos, std::size_t len) -> int {
    int value = 0;
    for (std::size_t i = 0; i < len; ++i) {
      const char c = text[pos + i];
      if (c < '0' || c > '9') {
        throw fail();
      }
      value = value * 10 + (c - '0');
    }
    return value;
  };

  return time::toEpochSeconds(time::makeTime(
      {number(0, 4), static_cast<unsigned>(number(5, 2)),
       static_cast<unsigned>(number(8, 2))},
      {number(11, 2), number(14, 2), number(17, 2)}));
}

StreamTxnBounds queryStreamBounds(Connection &conn, const std::string &table) {
  StreamTxnBounds out;

  const auto quoted = conn.escapeIdentifier(table);
  out.rows =
      parseU64Field(conn.queryValue("SELECT count(*) FROM " + quoted), "count");
  if (out.rows == 0) {
    return out;
  }

  conn.exec("SET DateStyle = 'ISO, YMD'");
  out.minTs =
      parseLedgerTimestamp(conn.queryValue("SELECT min(ts) FROM " + quoted));
  out.maxTs =
      parseLedgerTimestamp(conn.queryValue("SELECT max(ts) FROM " + quoted));
  return out;
}

// ------------------------------------------------------- TransactionScan

TransactionScan::TransactionScan(Connection &conn, const std::string &table)
    : conn_(&conn) {
  conn_->exec("BEGIN READ ONLY");
  // Canonical text regardless of server configuration: shortest
  // round-trip float8 output, ISO timestamps.
  conn_->exec("SET LOCAL extra_float_digits = 3");
  conn_->exec("SET LOCAL DateStyle = 'ISO, YMD'");
  conn_->exec(std::string{"DECLARE "} + kCursorName +
              " NO SCROLL CURSOR FOR SELECT row_seq, src_acct, dst_acct, "
              "amount, ts, is_fraud, channel FROM " +
              conn_->escapeIdentifier(table) + " ORDER BY row_seq");
  active_ = true;
}

TransactionScan::~TransactionScan() {
  if (batch_ != nullptr) {
    PQclear(batch_);
    batch_ = nullptr;
  }
  if (active_) {
    // Abandoned mid-stream: drop the cursor with its transaction. The
    // connection stays usable for the caller.
    try {
      conn_->exec("ROLLBACK");
    } catch (...) {
      // Destructor must not throw; a broken connection surfaces on the
      // caller's next use.
    }
  }
}

bool TransactionScan::fetchBatch() {
  if (batch_ != nullptr) {
    PQclear(batch_);
    batch_ = nullptr;
  }

  const std::string sql = "FETCH FORWARD " + std::to_string(kFetchBatchRows) +
                          " FROM " + kCursorName;
  PGresult *result = PQexec(conn_->raw(), sql.c_str());
  if (result == nullptr || PQresultStatus(result) != PGRES_TUPLES_OK) {
    std::string message =
        result != nullptr ? PQresultErrorMessage(result) : "no result";
    if (result != nullptr) {
      PQclear(result);
    }
    throw Error("txn_readback: FETCH failed: " + message);
  }

  if (PQntuples(result) == 0) {
    PQclear(result);
    conn_->exec(std::string{"CLOSE "} + kCursorName);
    conn_->exec("COMMIT");
    active_ = false;
    return false;
  }

  batch_ = result;
  rowsInBatch_ = PQntuples(result);
  rowInBatch_ = 0;
  return true;
}

bool TransactionScan::next(StreamTxnRow &out) {
  if (!active_) {
    return false;
  }
  if (batch_ == nullptr || rowInBatch_ == rowsInBatch_) {
    if (!fetchBatch()) {
      return false;
    }
  }

  const int i = rowInBatch_++;
  const auto field = [&](int col) -> const char * {
    return PQgetvalue(batch_, i, col);
  };

  out.rowSeq = parseU64Field(field(0), "row_seq");
  out.sourceRendered.assign(field(1));
  out.targetRendered.assign(field(2));
  out.source = parseKeyField(out.sourceRendered);
  out.target = parseKeyField(out.targetRendered);
  out.amount = parseAmountField(field(3));
  out.timestamp = parseLedgerTimestamp(field(4));
  out.fraudFlag = static_cast<std::uint8_t>(parseU64Field(field(5), "is_fraud"));
  out.channel = parseChannelField(field(6));

  ++rows_;
  return true;
}

} // namespace PhantomLedger::postgres
