#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::csv {

class Writer {
public:
  explicit Writer(const std::filesystem::path &path);

  Writer(const Writer &) = delete;
  Writer &operator=(const Writer &) = delete;
  Writer(Writer &&) = default;
  Writer &operator=(Writer &&) = default;
  ~Writer() = default;

  void writeHeader(std::span<const std::string_view> header);

  void writeHeader(std::initializer_list<std::string_view> header);

  // ------------------------------------------------------------------
  // Type-overloaded cell writers
  // ------------------------------------------------------------------

  Writer &cell(std::string_view v);
  Writer &cell(const std::string &v) { return cell(std::string_view{v}); }
  Writer &cell(const char *v) { return cell(std::string_view{v}); }

  Writer &cell(std::int64_t v);
  Writer &cell(std::int32_t v) { return cell(static_cast<std::int64_t>(v)); }
  Writer &cell(std::uint64_t v);
  Writer &cell(std::uint32_t v) { return cell(static_cast<std::uint64_t>(v)); }
  Writer &cell(std::int8_t v) { return cell(static_cast<std::int64_t>(v)); }
  Writer &cell(std::uint8_t v) { return cell(static_cast<std::uint64_t>(v)); }
  Writer &cell(std::int16_t v) { return cell(static_cast<std::int64_t>(v)); }
  Writer &cell(std::uint16_t v) { return cell(static_cast<std::uint64_t>(v)); }

  Writer &cell(double v);
  Writer &cell(float v) { return cell(static_cast<double>(v)); }

  Writer &cell(bool v);

  Writer &cellEmpty();

  template <class... Args> void writeRow(Args &&...args) {
    (cell(std::forward<Args>(args)), ...);
    endRow();
  }

  void endRow();

  void flush() { out_.flush(); }

private:
  void writeSeparatorIfNeeded();
  void writeEscaped(std::string_view s);
  [[nodiscard]] static bool needsQuoting(std::string_view s) noexcept;

  std::ofstream out_;
  std::vector<char> buffer_; // owned 1 MiB streambuf backing store
  bool firstCell_ = true;
};

} // namespace PhantomLedger::exporter::csv
