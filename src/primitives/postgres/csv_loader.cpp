#include "phantomledger/primitives/postgres/csv_loader.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace PhantomLedger::postgres {

namespace {

[[nodiscard]] std::vector<std::string> parseHeader(const std::string &line,
                                                   const std::string &where) {
  std::vector<std::string> cols;
  std::stringstream ss{line};
  std::string cell;
  while (std::getline(ss, cell, ',')) {
    if (cell.size() >= 2 && cell.front() == '"' && cell.back() == '"') {
      cell = cell.substr(1, cell.size() - 2);
    }
    if (cell.empty()) {
      throw Error("postgres: empty column name in header of " + where);
    }
    cols.push_back(cell);
  }
  if (cols.empty()) {
    throw Error("postgres: missing or empty header in " + where);
  }

  std::unordered_set<std::string> used;
  for (auto &col : cols) {
    if (used.contains(col)) {
      int n = 2;
      std::string candidate = col + "_" + std::to_string(n);
      while (used.contains(candidate)) {
        candidate = col + "_" + std::to_string(++n);
      }
      col = candidate;
    }
    used.insert(col);
  }
  return cols;
}

} // namespace

namespace {

[[nodiscard]] std::string qualifiedName(Connection &conn,
                                        std::string_view schema,
                                        std::string_view table) {
  if (schema.empty()) {
    return conn.escapeIdentifier(table);
  }
  return conn.escapeIdentifier(schema) + "." + conn.escapeIdentifier(table);
}

} // namespace

TableReport loadCsvTable(Connection &conn, const std::filesystem::path &file,
                         std::string_view table, std::string_view schema,
                         LoadOptions options) {
  std::ifstream in{file, std::ios::binary};
  if (!in) {
    throw Error("postgres: cannot open " + file.string());
  }

  std::string header;
  if (!std::getline(in, header)) {
    throw Error("postgres: cannot read header of " + file.string());
  }
  if (!header.empty() && header.back() == '\r') {
    header.pop_back();
  }
  const auto cols = parseHeader(header, file.string());

  if (!schema.empty()) {
    conn.exec("CREATE SCHEMA IF NOT EXISTS " + conn.escapeIdentifier(schema));
  }
  const auto qTable = qualifiedName(conn, schema, table);
  std::string ddl;
  for (const auto &col : cols) {
    ddl += (ddl.empty() ? "" : ", ") + conn.escapeIdentifier(col) + " text";
  }
  conn.exec("DROP TABLE IF EXISTS " + qTable);
  conn.exec(std::string{"CREATE "} + (options.unlogged ? "UNLOGGED " : "") +
            "TABLE " + qTable + " (" + ddl + ")");

  in.clear();
  in.seekg(0);
  CopyIn copy{conn,
              "COPY " + qTable + " FROM STDIN WITH (FORMAT csv, HEADER true)"};
  std::vector<char> buf(1024 * 1024);
  std::uint64_t newlines = 0;
  while (in.read(buf.data(), static_cast<std::streamsize>(buf.size())) ||
         in.gcount() > 0) {
    const auto got = static_cast<std::size_t>(in.gcount());
    newlines += static_cast<std::uint64_t>(
        std::count(buf.data(), buf.data() + got, '\n'));
    copy.put(buf.data(), got);
  }
  copy.done();

  return TableReport{std::string{table}, newlines == 0 ? 0 : newlines - 1};
}

std::vector<TableReport>
loadCsvDirectory(Connection &conn, const std::filesystem::path &dir,
                 std::span<const std::string_view> skip,
                 std::string_view schema, LoadOptions options) {
  namespace fs = std::filesystem;
  std::vector<fs::path> files;
  for (const auto &entry : fs::directory_iterator(dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".csv") {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());

  std::vector<TableReport> reports;
  reports.reserve(files.size());
  for (const auto &file : files) {
    const auto stem = file.stem().string();
    const bool skipped = std::find(skip.begin(), skip.end(),
                                   std::string_view{stem}) != skip.end();
    if (skipped) {
      continue;
    }
    reports.push_back(loadCsvTable(conn, file, stem, schema, options));
  }
  return reports;
}

std::vector<TableReport> loadCsvTree(Connection &conn,
                                     const std::filesystem::path &root,
                                     std::string_view schema,
                                     std::span<const std::string_view> skip,
                                     LoadOptions options) {
  namespace fs = std::filesystem;

  struct Entry {
    std::string table;
    fs::path file;
    std::string leafStem;
  };
  std::vector<Entry> entries;
  for (const auto &node : fs::recursive_directory_iterator(root)) {
    if (!node.is_regular_file() || node.path().extension() != ".csv") {
      continue;
    }
    const auto rel = fs::relative(node.path(), root);
    std::string table;
    for (const auto &part : rel.parent_path()) {
      table += part.string() + "_";
    }
    table += rel.stem().string();
    entries.push_back(
        Entry{std::move(table), node.path(), rel.stem().string()});
  }
  std::sort(entries.begin(), entries.end(),
            [](const Entry &a, const Entry &b) { return a.table < b.table; });

  std::vector<TableReport> reports;
  reports.reserve(entries.size());
  for (const auto &entry : entries) {
    const bool skipped =
        std::find(skip.begin(), skip.end(), std::string_view{entry.leafStem}) !=
        skip.end();
    if (skipped) {
      continue;
    }
    reports.push_back(
        loadCsvTable(conn, entry.file, entry.table, schema, options));
  }
  return reports;
}

} // namespace PhantomLedger::postgres
