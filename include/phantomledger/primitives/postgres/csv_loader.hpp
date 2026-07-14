#pragma once

#include "phantomledger/primitives/postgres/connection.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PhantomLedger::postgres {

struct LoadOptions {
  bool unlogged = true;
};

struct TableReport {
  std::string table;
  std::uint64_t rows = 0;
};

TableReport loadCsvTable(Connection &conn, const std::filesystem::path &file,
                         std::string_view table, std::string_view schema = {},
                         LoadOptions options = {});

std::vector<TableReport>
loadCsvDirectory(Connection &conn, const std::filesystem::path &dir,
                 std::span<const std::string_view> skip,
                 std::string_view schema = {}, LoadOptions options = {});

std::vector<TableReport> loadCsvTree(Connection &conn,
                                     const std::filesystem::path &root,
                                     std::string_view schema,
                                     std::span<const std::string_view> skip,
                                     LoadOptions options = {});

} // namespace PhantomLedger::postgres
