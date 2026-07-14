#pragma once

#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/transactions/record.hpp"

#include <concepts>
#include <cstdint>
#include <span>

namespace PhantomLedger::pipeline::chunk {

template <class S>
concept Sink = requires(S s, const S cs, const Span &span,
                        std::span<const transactions::Transaction> txns) {
  { s.beginSpan(span) } -> std::same_as<void>;
  { s.append(txns) } -> std::same_as<void>;
  { s.endSpan(span) } -> std::same_as<void>;
  { s.finish() } -> std::same_as<void>;
  { cs.rowsWritten() } -> std::convertible_to<std::uint64_t>;
};

// Counts rows and discards them. Used by tests and by dry runs that
// only want the volume forecast and the per-chunk memory watermark.
class NullSink {
public:
  void beginSpan(const Span &) noexcept {}

  void append(std::span<const transactions::Transaction> txns) noexcept {
    rows_ += txns.size();
  }

  void endSpan(const Span &) noexcept {}
  void finish() noexcept {}

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept { return rows_; }

private:
  std::uint64_t rows_ = 0;
};

static_assert(Sink<NullSink>);

} // namespace PhantomLedger::pipeline::chunk
