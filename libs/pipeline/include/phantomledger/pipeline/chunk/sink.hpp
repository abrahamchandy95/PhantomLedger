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

template <Sink A, Sink B> class Tee {
public:
  Tee(A &a, B &b) noexcept : a_(&a), b_(&b) {}

  void beginSpan(const Span &span) {
    a_->beginSpan(span);
    b_->beginSpan(span);
  }

  void append(std::span<const transactions::Transaction> txns) {
    a_->append(txns);
    b_->append(txns);
  }

  void endSpan(const Span &span) {
    a_->endSpan(span);
    b_->endSpan(span);
  }

  void finish() {
    a_->finish();
    b_->finish();
  }

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept {
    return a_->rowsWritten();
  }

private:
  A *a_;
  B *b_;
};

static_assert(Sink<Tee<NullSink, NullSink>>);

} // namespace PhantomLedger::pipeline::chunk
