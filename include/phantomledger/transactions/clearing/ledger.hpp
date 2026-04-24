#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/liquidity.hpp"
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

  // Sentinel with dual meaning:
  //   - Returned by findAccount() when the key is not registered.
  //   - Passed by callers to the Index-based transfer API to signal
  //     "this leg is external".
  static constexpr Index invalid = std::numeric_limits<Index>::max();

  Ledger() = default;

  void initialize(Index count);

  void addAccount(const entities::identifier::Key &id, Index idx);
  void createHub(Index idx) noexcept;

  // --- Buffer / protection setup ---
  //
  // setProtection() records the protection type and writes the buffer
  // amount into the correct underlying slot. Mutual exclusivity is
  // enforced: setting a protection type clears any buffer previously
  // held under a different type.
  //
  // For ProtectionType::loc, setProtection() stores the credit limit
  // in the overdraft slot so it feeds through totalLiquidity() and
  // the funding check unchanged. The APR and billing day live in the
  // dedicated LOC state vectors and are set via setLoc().
  void setProtection(Index idx, ProtectionType type,
                     double bufferAmount) noexcept;
  void setBankTier(Index idx, BankTier tier,
                   double overdraftFeeAmount) noexcept;
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
  [[nodiscard]] double liquidity(const entities::identifier::Key &id) const;
  [[nodiscard]] double availableCash(const entities::identifier::Key &id) const;

  [[nodiscard]] Index findAccount(const entities::identifier::Key &id) const;
  [[nodiscard]] Index size() const noexcept { return size_; }

  void setOverdraftOnly(Index idx, double limit) noexcept;

  // --- Liquidity sink & emission mode ---
  //
  // The authoritative pre-fraud replay runs with emitLiquidity=true
  // and a sink that records events into the liquidity-event collection
  // for later Transaction construction. The post-fraud replay runs the
  // full (legit + fraud) sequence with emitLiquidity=false so it
  // reaches the same final balance state without re-emitting fees.
  void setLiquiditySink(LiquiditySink sink) noexcept;
  void setEmitLiquidity(bool emit) noexcept;
  [[nodiscard]] bool emitLiquidity() const noexcept { return emitLiquidity_; }

  // --- Transfer API ---
  //
  // Key-based: does two hash lookups per leg. Preferred for one-shot
  // or debug code paths.
  [[nodiscard]] TransferDecision
  transfer(const entities::identifier::Key &src,
           const entities::identifier::Key &dst, double amount,
           channels::Tag channel = channels::none);

  template <channels::ChannelEnum Enum>
  [[nodiscard]] TransferDecision transfer(const entities::identifier::Key &src,
                                          const entities::identifier::Key &dst,
                                          double amount, Enum channel) {
    return transfer(src, dst, amount, channels::tag(channel));
  }

  // Index-based fast path. Caller pre-resolves via findAccount() and
  // passes `Ledger::invalid` for external legs. Preferred for the hot
  // simulation loop.
  //
  // Neither variant consults the clock, so LOC accrual is skipped and
  // overdraft fees are not emitted. Use transferAt() for full replay.
  [[nodiscard]] TransferDecision transfer(Index srcIdx, Index dstIdx,
                                          double amount,
                                          channels::Tag channel) noexcept;

  template <channels::ChannelEnum Enum>
  [[nodiscard]] TransferDecision
  transfer(Index srcIdx, Index dstIdx, double amount, Enum channel) noexcept {
    return transfer(srcIdx, dstIdx, amount, channels::tag(channel));
  }

  // Full-replay transfer. Advances the LOC dollar-seconds integral on
  // both legs (if applicable), applies the debit/credit, and — when
  // emitLiquidity_ is true and the source account crosses into
  // negative cash — debits the overdraft fee and fires a
  // LiquidityEvent to the configured sink.
  //
  // Channels in the 0x90 (Liquidity) block bypass the funding check
  // so fee collection can run against already-overdrawn accounts.
  [[nodiscard]] TransferDecision transferAt(Index srcIdx, Index dstIdx,
                                            double amount,
                                            channels::Tag channel,
                                            std::int64_t timestamp) noexcept;

  template <channels::ChannelEnum Enum>
  [[nodiscard]] TransferDecision transferAt(Index srcIdx, Index dstIdx,
                                            double amount, Enum channel,
                                            std::int64_t timestamp) noexcept {
    return transferAt(srcIdx, dstIdx, amount, channels::tag(channel),
                      timestamp);
  }

  // Sweep pass: for every LOC account whose billing period has
  // elapsed since its last interest event, compute interest from the
  // dollar-seconds integral, debit the account, and fire a
  // LOC_INTEREST LiquidityEvent. Designed to be called at end-of-day
  // from the simulator driver.
  //
  // The default billing period is 30 days. The per-account billingDay
  // field is preserved for future use with calendar-aware billing.
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

  // Core debit/credit logic shared by transfer() and transferAt().
  // Does not touch LOC accrual or emit fees. Returns the cash value
  // of the source account BEFORE the debit (0 on external source) so
  // the caller in transferAt() can detect an overdraft crossing.
  [[nodiscard]] TransferDecision applyTransfer(Index srcIdx, Index dstIdx,
                                               double amount,
                                               channels::Tag channel,
                                               double &srcCashBefore) noexcept;

  // LOC dollar-seconds integration. If protectionType_[idx] is loc
  // and cash_[idx] was negative during [lastUpdate, timestamp],
  // integrate -cash * dt into locDollarSecondsIntegral_[idx].
  void updateLocAccrual(Index idx, std::int64_t timestamp) noexcept;

  // Emit an overdraft fee for a COURTESY/LINKED account whose cash
  // just crossed from >= 0 to < 0. Debits overdraftFeeAmount_[idx]
  // from cash (bypassing the funding check) and calls the sink.
  void emitOverdraftFee(Index idx, std::int64_t timestamp);

  Index size_ = 0;

  // Existing per-account state.
  std::vector<double> cash_;
  std::vector<double> overdrafts_;
  std::vector<double> linked_;
  std::vector<double> courtesy_;
  std::vector<std::uint8_t> flags_;
  std::unordered_map<entities::identifier::Key, Index> internalAccounts_;

  // Reverse lookup for event emission (Index -> Key).
  std::vector<entities::identifier::Key> accountKeys_;

  // Protection type, bank tier, and overdraft fee amount per account.
  std::vector<ProtectionType> protectionType_;
  std::vector<BankTier> bankTier_;
  std::vector<double> overdraftFeeAmount_;

  // Line-of-credit state (only meaningful when protectionType_ == loc).
  //   locApr_                       : annualized rate
  //   locBillingDay_                : day of month (1-28)
  //   locDollarSecondsIntegral_     : running sum of -cash * dt
  //   locLastUpdateTs_              : epoch seconds of last accrual
  //   locLastBillingTs_             : epoch seconds of last interest event
  std::vector<double> locApr_;
  std::vector<std::int32_t> locBillingDay_;
  std::vector<double> locDollarSecondsIntegral_;
  std::vector<std::int64_t> locLastUpdateTs_;
  std::vector<std::int64_t> locLastBillingTs_;

  // Liquidity emission state.
  LiquiditySink liquiditySink_;
  bool emitLiquidity_ = true;
};

} // namespace PhantomLedger::clearing
