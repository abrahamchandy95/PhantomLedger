#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

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
  static constexpr Index invalid = std::numeric_limits<Index>::max();

  Ledger() = default;

  void initialize(Index count);

  void addAccount(const entities::identifier::Key &id, Index idx);
  void createHub(Index idx) noexcept;

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

  Index size_ = 0;
  std::vector<double> cash_;
  std::vector<double> overdrafts_;
  std::vector<double> linked_;
  std::vector<double> courtesy_;
  std::vector<std::uint8_t> flags_;
  std::unordered_map<entities::identifier::Key, Index> internalAccounts_;
};

} // namespace PhantomLedger::clearing
