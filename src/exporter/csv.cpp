#include "phantomledger/exporter/csv.hpp"

#include <array>
#include <charconv>
#include <stdexcept>
#include <string>
#include <system_error>

namespace PhantomLedger::exporter::csv {

namespace {

constexpr std::size_t kStreamBufferBytes = 1u << 20; // 1 MiB

[[nodiscard]] std::ofstream
openWriteOrThrow(const std::filesystem::path &path) {
  if (auto parent = path.parent_path(); !parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec && !std::filesystem::exists(parent)) {
      throw std::runtime_error{"csv::Writer: cannot create directory '" +
                               parent.string() + "': " + ec.message()};
    }
  }

  std::ofstream out{path, std::ios::binary | std::ios::trunc};
  if (!out) {
    throw std::runtime_error{"csv::Writer: cannot open file '" + path.string() +
                             "' for writing"};
  }
  return out;
}

template <class T> void writeIntegerTo(std::ofstream &out, T v) {
  // 24 chars is enough for any 64-bit integer including sign and NUL.
  std::array<char, 24> buf{};
  const auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), v);
  if (ec != std::errc{}) {
    throw std::runtime_error{"csv::Writer: failed to format integer"};
  }
  out.write(buf.data(), end - buf.data());
}

void writeDoubleTo(std::ofstream &out, double v) {
  // 32 chars is enough for any normal double in shortest-round-trip form.
  std::array<char, 32> buf{};
  const auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), v);
  if (ec != std::errc{}) {
    throw std::runtime_error{"csv::Writer: failed to format double"};
  }
  out.write(buf.data(), end - buf.data());

  const std::string_view rendered{buf.data(),
                                  static_cast<std::size_t>(end - buf.data())};
  if (rendered.find_first_of(".eEnN") == std::string_view::npos) {
    out.put('.');
    out.put('0');
  }
}

} // namespace

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

Writer::Writer(const std::filesystem::path &path)
    : out_{openWriteOrThrow(path)}, buffer_(kStreamBufferBytes) {
  out_.rdbuf()->pubsetbuf(buffer_.data(),
                          static_cast<std::streamsize>(buffer_.size()));
}

// -----------------------------------------------------------------------
// Quoting
// -----------------------------------------------------------------------

bool Writer::needsQuoting(std::string_view s) noexcept {
  return s.find_first_of(",\"\r\n") != std::string_view::npos;
}

void Writer::writeEscaped(std::string_view s) {
  if (!needsQuoting(s)) {
    out_.write(s.data(), static_cast<std::streamsize>(s.size()));
    return;
  }

  out_.put('"');
  std::size_t pos = 0;
  while (pos < s.size()) {
    const std::size_t next = s.find('"', pos);
    if (next == std::string_view::npos) {
      out_.write(s.data() + pos, static_cast<std::streamsize>(s.size() - pos));
      break;
    }
    out_.write(s.data() + pos, static_cast<std::streamsize>(next - pos));
    out_.put('"');
    out_.put('"');
    pos = next + 1;
  }
  out_.put('"');
}

void Writer::writeSeparatorIfNeeded() {
  if (firstCell_) {
    firstCell_ = false;
  } else {
    out_.put(',');
  }
}

// -----------------------------------------------------------------------
// Cell overloads
// -----------------------------------------------------------------------

Writer &Writer::cell(std::string_view v) {
  writeSeparatorIfNeeded();
  writeEscaped(v);
  return *this;
}

Writer &Writer::cellEmpty() {
  writeSeparatorIfNeeded();
  return *this;
}

Writer &Writer::cell(std::int64_t v) {
  writeSeparatorIfNeeded();
  writeIntegerTo(out_, v);
  return *this;
}

Writer &Writer::cell(std::uint64_t v) {
  writeSeparatorIfNeeded();
  writeIntegerTo(out_, v);
  return *this;
}

Writer &Writer::cell(double v) {
  writeSeparatorIfNeeded();
  writeDoubleTo(out_, v);
  return *this;
}

Writer &Writer::cell(bool v) {
  writeSeparatorIfNeeded();
  // Python's csv module writes bools via str(True)/str(False).
  static constexpr std::string_view kTrue{"True"};
  static constexpr std::string_view kFalse{"False"};
  const auto s = v ? kTrue : kFalse;
  out_.write(s.data(), static_cast<std::streamsize>(s.size()));
  return *this;
}

// -----------------------------------------------------------------------
// Header / row termination
// -----------------------------------------------------------------------

void Writer::writeHeader(std::span<const std::string_view> header) {
  for (auto h : header) {
    cell(h);
  }
  endRow();
}

void Writer::writeHeader(std::initializer_list<std::string_view> header) {
  writeHeader(std::span<const std::string_view>{header.begin(), header.size()});
}

void Writer::endRow() {
  out_.put('\r');
  out_.put('\n');
  firstCell_ = true;
}

} // namespace PhantomLedger::exporter::csv
