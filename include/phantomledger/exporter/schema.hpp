#pragma once
//
// phantomledger/exporter/schema.hpp
//
// The shared schema KERNEL only: the Table descriptor type, the
// make() helper, and the one table every exporter shares — the
// streamed corpus ledger. Each exporter's own tables live next to
// their consumer (standard/schema.hpp, mule_ml/schema.hpp,
// aml/schema.hpp, aml_txn_edges/schema.hpp), all in this namespace.
//

#include <array>
#include <span>
#include <string_view>

namespace PhantomLedger::exporter::schema {

struct Table {
  std::string_view filename;
  std::span<const std::string_view> header;
};

namespace detail {

template <std::size_t N>
[[nodiscard]] constexpr Table
make(std::string_view filename,
     const std::array<std::string_view, N> &columns) noexcept {
  return Table{filename, std::span<const std::string_view>{columns}};
}

} // namespace detail

// The streamed corpus ledger — the 'transactions' table's row layout,
// shared by the PostgreSQL sink (rendering), the read-back decode
// contract, and test_postgres' rendering oracle.
inline constexpr std::string_view kLedgerHeader[]{
    "src_acct", "dst_acct",   "amount",    "ts",         "is_fraud",
    "ring_id",  "fraud_type", "device_id", "ip_address", "channel"};
inline constexpr Table kLedger{"transactions.csv", kLedgerHeader};

} // namespace PhantomLedger::exporter::schema
