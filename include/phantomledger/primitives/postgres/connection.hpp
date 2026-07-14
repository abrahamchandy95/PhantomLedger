#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

struct pg_conn;

namespace PhantomLedger::postgres {

class Error final : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class Connection {
public:
  // conninfo per libpq, e.g. "dbname=phantomledger host=localhost".
  explicit Connection(const std::string &conninfo);

  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;
  Connection(Connection &&) noexcept = default;
  Connection &operator=(Connection &&) noexcept = default;
  ~Connection() = default;

  void exec(const std::string &sql);

  [[nodiscard]] std::string queryValue(const std::string &sql);

  // Quote an SQL identifier (table/column name) server-appropriately.
  [[nodiscard]] std::string escapeIdentifier(std::string_view name) const;

  // Escape hatch for callers that need raw libpq (tests, readers).
  [[nodiscard]] pg_conn *raw() const noexcept { return conn_.get(); }

private:
  struct Deleter {
    void operator()(pg_conn *c) const noexcept;
  };
  std::unique_ptr<pg_conn, Deleter> conn_;
};

class CopyIn {
public:
  CopyIn(Connection &conn, const std::string &sql);

  CopyIn(const CopyIn &) = delete;
  CopyIn &operator=(const CopyIn &) = delete;
  CopyIn(CopyIn &&) = delete;
  CopyIn &operator=(CopyIn &&) = delete;

  ~CopyIn();

  void put(const char *data, std::size_t len);
  void done();

  [[nodiscard]] bool active() const noexcept { return active_; }

private:
  Connection *conn_;
  bool active_ = false;
};

} // namespace PhantomLedger::postgres
