#include "phantomledger/primitives/postgres/connection.hpp"

#include <libpq-fe.h>

namespace PhantomLedger::postgres {

namespace {

struct ResultDeleter {
  void operator()(PGresult *r) const noexcept { PQclear(r); }
};
using Result = std::unique_ptr<PGresult, ResultDeleter>;

[[noreturn]] void throwPg(PGconn *conn, const char *what) {
  throw Error(std::string{"postgres: "} + what + ": " +
              (conn != nullptr ? PQerrorMessage(conn) : "no connection"));
}

} // namespace

void Connection::Deleter::operator()(pg_conn *c) const noexcept { PQfinish(c); }

Connection::Connection(const std::string &conninfo)
    : conn_(PQconnectdb(conninfo.c_str())) {
  if (conn_ == nullptr || PQstatus(conn_.get()) != CONNECTION_OK) {
    throwPg(conn_.get(), "connect failed");
  }
}

void Connection::exec(const std::string &sql) {
  Result res{PQexec(conn_.get(), sql.c_str())};
  if (res == nullptr || PQresultStatus(res.get()) != PGRES_COMMAND_OK) {
    throwPg(conn_.get(), sql.c_str());
  }
}

std::string Connection::queryValue(const std::string &sql) {
  Result res{PQexec(conn_.get(), sql.c_str())};
  if (res == nullptr || PQresultStatus(res.get()) != PGRES_TUPLES_OK ||
      PQntuples(res.get()) < 1 || PQnfields(res.get()) < 1) {
    throwPg(conn_.get(), sql.c_str());
  }
  return PQgetvalue(res.get(), 0, 0);
}

std::string Connection::escapeIdentifier(std::string_view name) const {
  char *quoted = PQescapeIdentifier(conn_.get(), name.data(), name.size());
  if (quoted == nullptr) {
    throwPg(conn_.get(), "escape identifier failed");
  }
  std::string out{quoted};
  PQfreemem(quoted);
  return out;
}

CopyIn::CopyIn(Connection &conn, const std::string &sql) : conn_(&conn) {
  Result res{PQexec(conn_->raw(), sql.c_str())};
  if (res == nullptr || PQresultStatus(res.get()) != PGRES_COPY_IN) {
    throwPg(conn_->raw(), "COPY FROM STDIN failed to start");
  }
  active_ = true;
}

CopyIn::~CopyIn() {
  if (!active_) {
    return;
  }
  // Abort path: never commit a partial batch. Errors are swallowed;
  // this runs during unwinding.
  (void)PQputCopyEnd(conn_->raw(), "aborted: CopyIn destroyed before done()");
  while (PGresult *r = PQgetResult(conn_->raw())) {
    PQclear(r);
  }
}

void CopyIn::put(const char *data, std::size_t len) {
  if (!active_) {
    throw Error("postgres: put() on inactive CopyIn");
  }
  if (PQputCopyData(conn_->raw(), data, static_cast<int>(len)) != 1) {
    throwPg(conn_->raw(), "PQputCopyData failed");
  }
}

void CopyIn::done() {
  if (!active_) {
    throw Error("postgres: done() on inactive CopyIn");
  }
  active_ = false; // even on failure below, the COPY is over
  if (PQputCopyEnd(conn_->raw(), nullptr) != 1) {
    throwPg(conn_->raw(), "PQputCopyEnd failed");
  }
  while (PGresult *raw = PQgetResult(conn_->raw())) {
    Result res{raw};
    if (PQresultStatus(res.get()) != PGRES_COMMAND_OK) {
      throwPg(conn_->raw(), "COPY did not complete");
    }
  }
}

} // namespace PhantomLedger::postgres
