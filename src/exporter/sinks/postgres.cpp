#include "phantomledger/exporter/sinks/postgres.hpp"

#include "phantomledger/exporter/common/ledger.hpp"
#include "phantomledger/exporter/schema.hpp"

#include <stdexcept>
#include <utility>

namespace PhantomLedger::exporter::sinks {

namespace {

constexpr std::string_view kColumns =
    "src_acct, dst_acct, amount, ts, is_fraud, ring_id, "
    "device_id, ip_address, channel";
constexpr std::string_view kColumnDdl = "src_acct   text             NOT NULL, "
                                        "dst_acct   text             NOT NULL, "
                                        "amount     double precision NOT NULL, "
                                        "ts         timestamp        NOT NULL, "
                                        "is_fraud   smallint         NOT NULL, "
                                        "ring_id    bigint, "

                                        "device_id  text, "
                                        "ip_address text             NOT NULL, "
                                        "channel    text             NOT NULL";
static_assert(schema::kLedger.header.size() == 9,
              "kLedger changed; update sinks::Postgres DDL and columns");

} // namespace

Postgres::Postgres(Options options)
    : options_(std::move(options)), conn_(options_.conninfo),
      streambuf_([this](const char *d, std::size_t n) {
        if (!copy_.has_value()) {
          throw std::logic_error("sinks::Postgres: bytes without open COPY");
        }
        copy_->put(d, n);
      }),
      stream_(&streambuf_) {
  writer_.emplace(stream_);

  const auto table = conn_.escapeIdentifier(options_.table);
  if (options_.createTable) {
    conn_.exec("DROP TABLE IF EXISTS " + table);
    conn_.exec(std::string{"CREATE "} + (options_.unlogged ? "UNLOGGED " : "") +
               "TABLE " + table + " (" + std::string{kColumnDdl} + ")");
  } else if (options_.truncateFirst) {
    conn_.exec("TRUNCATE " + table);
  }
}

void Postgres::beginSpan(const pipeline::chunk::Span &) {
  if (finished_) {
    throw std::logic_error("sinks::Postgres: beginSpan after finish");
  }
  if (copy_.has_value()) {
    throw std::logic_error("sinks::Postgres: beginSpan without endSpan");
  }
  copy_.emplace(conn_, "COPY " + conn_.escapeIdentifier(options_.table) + " (" +
                           std::string{kColumns} +
                           ") FROM STDIN WITH (FORMAT csv)");
}

void Postgres::append(std::span<const transactions::Transaction> txns) {
  if (!copy_.has_value()) {
    throw std::logic_error("sinks::Postgres: append requires an open span");
  }
  common::writeLedgerRows(*writer_, txns);
  rows_ += txns.size();
}

void Postgres::endSpan(const pipeline::chunk::Span &) {
  if (!copy_.has_value()) {
    throw std::logic_error("sinks::Postgres: endSpan without beginSpan");
  }
  stream_.flush();
  copy_->done();
  copy_.reset();
  ++spans_;
}

void Postgres::finish() {
  if (copy_.has_value()) {
    throw std::logic_error("sinks::Postgres: finish with an open span");
  }
  finished_ = true;
}

} // namespace PhantomLedger::exporter::sinks
