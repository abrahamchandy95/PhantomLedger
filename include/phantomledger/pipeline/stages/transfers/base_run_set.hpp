#pragma once
/*
  Bounded construction of the two disk-backed views of the generation
  prologue's screened base stream.

  The input is already timestamp-sorted. The spending preparation consumes that
  resident view first; BaseRunSet then writes it once to the compact timestamp
  replay spool and derives the full-audit replay view in bounded runs. A run
  boundary is extended through an equal-timestamp group. Because timestamp is
  the first field of transactions::detail::auditKey, sorting each such run by
  fundsLess and concatenating the runs is exactly the same total order as
  sorting the complete input vector at once.

  The target is therefore a SOFT bound: peak staging is at most the target plus
  one complete equal-timestamp group. That exception is required for byte
  identity — splitting an equal-timestamp group would need an external merge.
  Production's simultaneous base rows are population-scale, not corpus-scale,
  so this removes the whole-window replay copy while retaining a small,
  observable high-water mark.
 */

#include "phantomledger/pipeline/stages/transfers/binary_spool.hpp"
#include "phantomledger/pipeline/stages/transfers/replay_spool.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

class BaseRunSet {
public:
  using Transaction = transactions::Transaction;

  static constexpr std::size_t kDefaultAuditRunBytes = std::size_t{32} << 20;
  static constexpr std::size_t kDefaultAuditRunRows =
      (kDefaultAuditRunBytes / sizeof(Transaction)) != 0
          ? kDefaultAuditRunBytes / sizeof(Transaction)
          : 1;

  explicit BaseRunSet(std::span<const Transaction> timestampSortedRows,
                      std::size_t targetAuditRunRows = kDefaultAuditRunRows) {
    if (targetAuditRunRows == 0) {
      throw std::invalid_argument(
          "BaseRunSet requires a non-zero audit-run target");
    }
    if (!std::ranges::is_sorted(
            timestampSortedRows,
            ::PhantomLedger::transfers::legit::ledger::detail::timestampLess)) {
      throw std::invalid_argument(
          "BaseRunSet requires timestamp-sorted input rows");
    }

    timestampReplay_.spool(timestampSortedRows);
    timestampReplay_.seal();

    std::vector<Transaction> auditRun;
    auditRun.reserve(std::min(targetAuditRunRows, timestampSortedRows.size()));

    std::size_t begin = 0;
    while (begin < timestampSortedRows.size()) {
      const auto remaining = timestampSortedRows.size() - begin;
      std::size_t end = begin + std::min(targetAuditRunRows, remaining);

      // Never split an audit-key primary-field tie across adjacent runs.
      while (end < timestampSortedRows.size() &&
             timestampSortedRows[end - 1].timestamp ==
                 timestampSortedRows[end].timestamp) {
        ++end;
      }

      auditRun.assign(
          timestampSortedRows.begin() + static_cast<std::ptrdiff_t>(begin),
          timestampSortedRows.begin() + static_cast<std::ptrdiff_t>(end));
      std::ranges::sort(
          auditRun,
          ::PhantomLedger::transfers::legit::ledger::detail::fundsLess);
      auditReplay_.append(
          std::span<const Transaction>(auditRun.data(), auditRun.size()));
      peakAuditRunRows_ = std::max(peakAuditRunRows_, auditRun.size());
      begin = end;
    }

    auditReplay_.finish();
  }

  BaseRunSet(const BaseRunSet &) = delete;
  BaseRunSet &operator=(const BaseRunSet &) = delete;

  [[nodiscard]] const BaseReplaySpool &timestampReplay() const noexcept {
    return timestampReplay_;
  }

  [[nodiscard]] std::unique_ptr<BinarySpoolCursor> openAuditCursor() {
    return auditReplay_.openCursor();
  }

  [[nodiscard]] std::uint64_t rows() const noexcept {
    return auditReplay_.rowsWritten();
  }

  [[nodiscard]] std::uint64_t timestampBytes() const noexcept {
    return timestampReplay_.bytesSpooled();
  }

  [[nodiscard]] std::uint64_t auditBytes() const noexcept {
    return auditReplay_.bytesSpooled();
  }

  [[nodiscard]] std::size_t peakAuditRunRows() const noexcept {
    return peakAuditRunRows_;
  }

private:
  BaseReplaySpool timestampReplay_;
  BinaryCandidateSpool auditReplay_;
  std::size_t peakAuditRunRows_ = 0;
};

} // namespace PhantomLedger::pipeline::stages::transfers
