#pragma once

#include "phantomledger/exporter/common/ledger.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/chunk/sink.hpp"
#include "phantomledger/primitives/crypto/blake2b.hpp"
#include "phantomledger/primitives/io/callback_streambuf.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>

namespace PhantomLedger::exporter::sinks {

class Golden {
public:
  static constexpr std::size_t kDigestBytes = 32;

  Golden()
      : streambuf_([this](const char *data, std::size_t size) {
          if (!hasher_.update(data, size)) {
            throw std::logic_error("sinks::Golden: hash update failed");
          }
        }),
        stream_(&streambuf_) {
    writer_.emplace(stream_);
  }

  void beginSpan(const pipeline::chunk::Span &) {
    if (finished_) {
      throw std::logic_error("sinks::Golden: beginSpan after finish");
    }
    if (spanOpen_) {
      throw std::logic_error("sinks::Golden: beginSpan without endSpan");
    }
    spanOpen_ = true;
  }

  void append(std::span<const transactions::Transaction> txns) {
    if (!spanOpen_) {
      throw std::logic_error("sinks::Golden: append requires an open span");
    }
    common::writeLedgerRows(*writer_, txns);
    rows_ += txns.size();
  }

  void endSpan(const pipeline::chunk::Span &) {
    if (!spanOpen_) {
      throw std::logic_error("sinks::Golden: endSpan without beginSpan");
    }
    spanOpen_ = false;
  }

  void finish() {
    if (spanOpen_) {
      throw std::logic_error("sinks::Golden: finish with an open span");
    }
    if (finished_) {
      return;
    }
    stream_.flush(); // drain CallbackStreambuf into the hasher
    if (!hasher_.finalize(bytes_.data(), bytes_.size())) {
      throw std::logic_error("sinks::Golden: hash finalize failed");
    }
    finished_ = true;
  }

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept { return rows_; }

  [[nodiscard]] const std::array<std::uint8_t, kDigestBytes> &
  digestBytes() const {
    requireFinished();
    return bytes_;
  }

  // Lowercase hex of the 32-byte digest; the form goldens are pinned in.
  [[nodiscard]] std::string digest() const {
    requireFinished();
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(kDigestBytes * 2);
    for (const auto b : bytes_) {
      out.push_back(kHex[b >> 4U]);
      out.push_back(kHex[b & 0x0FU]);
    }
    return out;
  }

private:
  void requireFinished() const {
    if (!finished_) {
      throw std::logic_error("sinks::Golden: digest before finish");
    }
  }

  crypto::blake2b::Stream hasher_{kDigestBytes};
  io::CallbackStreambuf streambuf_;
  std::ostream stream_;
  std::optional<csv::Writer> writer_;

  std::array<std::uint8_t, kDigestBytes> bytes_{};
  std::uint64_t rows_ = 0;
  bool spanOpen_ = false;
  bool finished_ = false;
};

static_assert(pipeline::chunk::Sink<Golden>);

} // namespace PhantomLedger::exporter::sinks
