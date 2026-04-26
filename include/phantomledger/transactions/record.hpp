#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/devices/identity.hpp"
#include "phantomledger/transactions/network/ipv4.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <tuple>

namespace PhantomLedger::transactions {

struct Fraud {
  std::uint8_t flag = 0;
  std::optional<std::uint32_t> ringId;
  constexpr std::strong_ordering operator<=>(const Fraud &) const = default;
};

struct Session {
  devices::Identity deviceId;
  network::Ipv4 ipAddress;
  channels::Tag channel = channels::none;
  constexpr auto operator<=>(const Session &) const = default;
};

struct Transaction {
  entity::Key source;
  entity::Key target;
  double amount = 0.0;
  std::int64_t timestamp = 0;

  Fraud fraud;
  Session session;
};

namespace detail {

[[nodiscard]] inline auto fundsKey(const Transaction &tx) noexcept {
  return std::tie(tx.timestamp, tx.source, tx.target, tx.amount);
}

[[nodiscard]] inline auto auditKey(const Transaction &tx) noexcept {
  return std::tie(tx.timestamp, tx.source, tx.target, tx.amount, tx.fraud.flag,
                  tx.fraud.ringId, tx.session.channel, tx.session.deviceId,
                  tx.session.ipAddress);
}

} // namespace detail

class Comparator {
public:
  enum class Scope : std::uint8_t {
    fundsTransfer,
    fullAudit,
  };

  constexpr explicit Comparator(Scope scope) noexcept : scope_(scope) {}

  [[nodiscard]] bool operator()(const Transaction &lhs,
                                const Transaction &rhs) const noexcept {
    switch (scope_) {
    case Scope::fundsTransfer:
      return detail::fundsKey(lhs) < detail::fundsKey(rhs);

    case Scope::fullAudit:
      return detail::auditKey(lhs) < detail::auditKey(rhs);
    }

    return false;
  }

private:
  Scope scope_;
};

} // namespace PhantomLedger::transactions
