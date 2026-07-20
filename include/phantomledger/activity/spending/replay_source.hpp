#pragma once

#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <span>

// The base-stream replay seam (RAM R2.4b-1, docs/ram_derive_dont_store
// .md): the day driver advances the screen ledger through the base
// stream (income + base routines) day by day, monotone forward. Today
// those rows live in one resident whole-window vector; this interface
// lets the feed come from anywhere that can post the same rows in the
// same order (R2.4b-2: a sequential disk spool), while the CALLER keeps
// the cursor position (RunState::baseIdx) exactly as it does with the
// raw span — the source itself stays stateless and const.

namespace PhantomLedger::activity::spending {

class BaseReplaySource {
public:
  virtual ~BaseReplaySource() = default;

  // Post the rows at [fromIdx, ...) whose timestamps fall inside
  // `bound` into `book` (which may be null — the position still
  // advances), returning the index of the first unposted row. Callers
  // present monotone bounds across calls.
  [[nodiscard]] virtual std::size_t
  postThrough(clearing::Ledger *book, std::size_t fromIdx,
              clearing::TimeBound bound) const = 0;

protected:
  BaseReplaySource() = default;
  BaseReplaySource(const BaseReplaySource &) = default;
  BaseReplaySource &operator=(const BaseReplaySource &) = default;
  BaseReplaySource(BaseReplaySource &&) = default;
  BaseReplaySource &operator=(BaseReplaySource &&) = default;
};

// The resident-vector adapter: byte-for-byte the advanceBookThrough
// call the day driver has always made against the raw span.
class SpanReplaySource final : public BaseReplaySource {
public:
  SpanReplaySource() = default;

  explicit SpanReplaySource(
      std::span<const transactions::Transaction> txns) noexcept
      : txns_(txns) {}

  [[nodiscard]] std::size_t
  postThrough(clearing::Ledger *book, std::size_t fromIdx,
              clearing::TimeBound bound) const override {
    return clearing::advanceBookThrough(book, txns_, fromIdx, bound);
  }

private:
  std::span<const transactions::Transaction> txns_{};
};

} // namespace PhantomLedger::activity::spending
