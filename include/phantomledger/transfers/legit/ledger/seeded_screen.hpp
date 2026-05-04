#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::ledger {

class SeededScreen {
public:
  [[nodiscard]] static SeededScreen
  sorted(clearing::Ledger *book,
         std::span<const transactions::Transaction> seedTxns) {
    return SeededScreen(book, seedTxns);
  }

  [[nodiscard]] static SeededScreen
  unsorted(clearing::Ledger *book,
           std::span<const transactions::Transaction> seedTxns) {
    SeededScreen screen(book, {});
    screen.sortedSeed_.assign(seedTxns.begin(), seedTxns.end());
    std::ranges::sort(screen.sortedSeed_,
                      [](const transactions::Transaction &a,
                         const transactions::Transaction &b) noexcept {
                        return a.timestamp < b.timestamp;
                      });
    screen.seedTxns_ = std::span<const transactions::Transaction>(
        screen.sortedSeed_.data(), screen.sortedSeed_.size());
    return screen;
  }

  SeededScreen(const SeededScreen &) = delete;
  SeededScreen &operator=(const SeededScreen &) = delete;

  SeededScreen(SeededScreen &&other) noexcept
      : book_(other.book_), seedTxns_(other.seedTxns_),
        sortedSeed_(std::move(other.sortedSeed_)), seedIdx_(other.seedIdx_) {
    rebindOwnedSeed();
    other.book_ = nullptr;
    other.seedTxns_ = {};
    other.seedIdx_ = 0;
  }

  SeededScreen &operator=(SeededScreen &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    book_ = other.book_;
    seedTxns_ = other.seedTxns_;
    sortedSeed_ = std::move(other.sortedSeed_);
    seedIdx_ = other.seedIdx_;
    rebindOwnedSeed();

    other.book_ = nullptr;
    other.seedTxns_ = {};
    other.seedIdx_ = 0;

    return *this;
  }

  [[nodiscard]] bool hasBook() const noexcept { return book_ != nullptr; }

  [[nodiscard]] double availableCash(const entity::Key &acct) const {
    return book_ == nullptr ? 0.0 : book_->availableCash(acct);
  }

  void advanceThrough(std::int64_t timestamp, bool inclusive = true) {
    seedIdx_ = clearing::advanceBookThrough(book_, seedTxns_, seedIdx_,
                                            timestamp, inclusive);
  }

  [[nodiscard]] bool acceptTransfer(const entity::Key &source,
                                    const entity::Key &destination,
                                    double amount, channels::Tag channel,
                                    std::int64_t timestamp) {
    if (book_ == nullptr) {
      return true;
    }

    const auto srcIdx = book_->findAccount(source);
    const auto dstIdx = book_->findAccount(destination);
    const auto decision =
        book_->transferAt(srcIdx, dstIdx, amount, channel, timestamp);
    return decision.accepted();
  }

private:
  SeededScreen(clearing::Ledger *book,
               std::span<const transactions::Transaction> seedTxns) noexcept
      : book_(book), seedTxns_(seedTxns) {}

  void rebindOwnedSeed() noexcept {
    if (!sortedSeed_.empty()) {
      seedTxns_ = std::span<const transactions::Transaction>(
          sortedSeed_.data(), sortedSeed_.size());
    }
  }

  clearing::Ledger *book_ = nullptr;
  std::span<const transactions::Transaction> seedTxns_{};
  std::vector<transactions::Transaction> sortedSeed_{};
  std::size_t seedIdx_ = 0;
};

} // namespace PhantomLedger::transfers::legit::ledger
