#pragma once
/*
 * Ledger — core double-entry bookkeeping.
 *
 * Post-refactor, Ledger owns:
 *   - per-account cash balances
 *   - protection buffer slots (overdraft / linked / courtesy)
 *   - protection-type classification
 *   - bank tier + per-account overdraft fee amount
 *   - the key <-> index map
 *
 * Ledger delegates:
 *   - LOC interest accrual -> LocAccrualTracker
 *   - overdraft fee decision -> OverdraftFeePolicy (free function)
 *   - emission of LiquidityEvents -> LiquiditySink (caller-provided)
 *
 * The clone/restore path now restores LOC tracker state too, so
 * authoritative and post-fraud replays land on identical balances.
 */

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/liquidity.hpp"
#include "phantomledger/transactions/clearing/loc_accrual.hpp"
#include "phantomledger/transactions/clearing/protection.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::clearing {

enum class RejectReason : std::uint8_t {
  invalid,
  unbooked,
  unfunded,
};

class TransferDecision {
public:
  [[nodiscard]] static constexpr TransferDecision accept() noexcept {
    return TransferDecision{};
  }
  [[nodiscard]] static constexpr TransferDecision
  reject(RejectReason reason) noexcept {
    return TransferDecision{reason};
  }

  [[nodiscard]] constexpr bool accepted() const noexcept {
    return !reason_.has_value();
  }
  [[nodiscard]] constexpr bool rejected() const noexcept {
    return reason_.has_value();
  }
  [[nodiscard]] constexpr std::optional<RejectReason>
  rejectReason() const noexcept {
    return reason_;
  }

  [[nodiscard]] RejectReason reason() const {
    if (!reason_.has_value()) {
      throw std::logic_error(
          "accepted transfer does not carry a reject reason");
    }
    return *reason_;
  }

private:
  constexpr TransferDecision() noexcept = default;
  constexpr explicit TransferDecision(RejectReason reason) noexcept
      : reason_(reason) {}

  std::optional<RejectReason> reason_;
};

class Ledger {
public:
  using Index = std::uint32_t;

  // Dual-meaning sentinel: findAccount() returns this for unknown
  // keys; callers pass it to the Index-based transfer API to mark a
  // leg as external.
  static constexpr Index invalid = std::numeric_limits<Index>::max();

  Ledger() = default;

  void initialize(Index count);

  void addAccount(const entity::Key &id, Index idx);
  void createHub(Index idx) noexcept;

  // --- Protection / tier / LOC setup ---

  void setProtection(Index idx, ProtectionType type,
                     double bufferAmount) noexcept;
  void setBankTier(Index idx, BankTier tier,
                   double overdraftFeeAmount) noexcept;

  /// Set LOC terms. Must be called after `setProtection(idx, loc, ...)`,
  /// because setProtection is what registers the account with the
  /// LOC tracker. Passing LOC terms to a non-LOC account has no effect
  /// beyond asserting in debug.
  void setLoc(Index idx, double apr, int billingDay) noexcept;

  [[nodiscard]] ProtectionType protectionType(Index idx) const noexcept;
  [[nodiscard]] BankTier bankTier(Index idx) const noexcept;
  [[nodiscard]] double overdraftFeeAmount(Index idx) const noexcept;

  // --- Balance inspection ---

  [[nodiscard]] double &cash(Index idx) noexcept;
  [[nodiscard]] double &overdraft(Index idx) noexcept;
  [[nodiscard]] double &linked(Index idx) noexcept;
  [[nodiscard]] double &courtesy(Index idx) noexcept;

  [[nodiscard]] double liquidity(Index idx) const noexcept;
  [[nodiscard]] double availableCash(Index idx) const noexcept;
  [[nodiscard]] double liquidity(const entity::Key &id) const;
  [[nodiscard]] double availableCash(const entity::Key &id) const;

  [[nodiscard]] Index findAccount(const entity::Key &id) const;
  [[nodiscard]] Index size() const noexcept { return size_; }

  void setOverdraftOnly(Index idx, double limit) noexcept;

  // --- Liquidity sink / emission mode ---

  void setLiquiditySink(LiquiditySink sink) noexcept;
  void setEmitLiquidity(bool emit) noexcept;
  [[nodiscard]] bool emitLiquidity() const noexcept { return emitLiquidity_; }

  // --- Transfer API ---

  [[nodiscard]] TransferDecision
  transfer(const entity::Key &src, const entity::Key &dst, double amount,
           channels::Tag channel = channels::none);

  template <channels::Enum E>
  [[nodiscard]] TransferDecision transfer(const entity::Key &src,
                                          const entity::Key &dst, double amount,
                                          E channel) {
    return transfer(src, dst, amount, channels::tag(channel));
  }

  [[nodiscard]] TransferDecision transfer(Index srcIdx, Index dstIdx,
                                          double amount,
                                          channels::Tag channel) noexcept;

  template <channels::Enum E>
  [[nodiscard]] TransferDecision transfer(Index srcIdx, Index dstIdx,
                                          double amount, E channel) noexcept {
    return transfer(srcIdx, dstIdx, amount, channels::tag(channel));
  }

  [[nodiscard]] TransferDecision transferAt(Index srcIdx, Index dstIdx,
                                            double amount,
                                            channels::Tag channel,
                                            std::int64_t timestamp) noexcept;

  template <channels::Enum E>
  [[nodiscard]] TransferDecision transferAt(Index srcIdx, Index dstIdx,
                                            double amount, E channel,
                                            std::int64_t timestamp) noexcept {
    return transferAt(srcIdx, dstIdx, amount, channels::tag(channel),
                      timestamp);
  }
  /// Run the LOC billing sweep through `timestamp`. Any matured
  /// account has its interest debited from cash (bypassing the
  /// funding check) and a LOC_INTEREST event fired at the sink when
  /// emitLiquidity_ is true.
  void accrueLocInterestThrough(std::int64_t timestamp) noexcept;

  [[nodiscard]] Ledger clone() const;
  void restore(const Ledger &other);

private:
  enum Flags : std::uint8_t {
    none = 0,
    hub = 1U << 0U,
  };

  [[nodiscard]] bool isHub(Index idx) const noexcept;
  [[nodiscard]] bool isValid(Index idx) const noexcept;
  [[nodiscard]] double totalLiquidity(Index idx) const noexcept;

  [[nodiscard]] TransferDecision applyTransfer(Index srcIdx, Index dstIdx,
                                               double amount,
                                               channels::Tag channel,
                                               double &srcCashBefore) noexcept;

  void debitAndEmit(Index idx, double amount, channels::Tag channel,
                    std::int64_t timestamp);

  Index size_ = 0;

  std::vector<double> cash_;
  std::vector<double> overdrafts_;
  std::vector<double> linked_;
  std::vector<double> courtesy_;
  std::vector<std::uint8_t> flags_;

  std::unordered_map<entity::Key, Index> internalAccounts_;
  std::vector<entity::Key> accountKeys_; ///< reverse lookup

  std::vector<ProtectionType> protectionType_;
  std::vector<BankTier> bankTier_;
  std::vector<double> overdraftFeeAmount_;

  LocAccrualTracker locTracker_;

  LiquiditySink liquiditySink_;
  bool emitLiquidity_ = true;
};

} // namespace PhantomLedger::clearing
