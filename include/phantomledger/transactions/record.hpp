#pragma once

#include "phantomledger/devices/identity.hpp"
#include "phantomledger/entities/identity.hpp"
#include "phantomledger/network/ipv4.hpp"
#include "phantomledger/transactions/channel.hpp"

#include <cstdint>

namespace PhantomLedger::transactions {

struct Fraud {
  std::uint8_t flag = 0;
  std::optional<std::uint32_t> ringId;
  auto operator<=>(const Fraud &) const = default;
};

struct Session {
  devices::Identity deviceId;
  network::Ipv4 ipAddress;
  Channel channel = Channel::unknown;
};

struct Transaction {
  entities::Identity source;
  entities::Identity target;
  double amount = 0.0;
  std::int64_t timestamp = 0;

  Fraud fraud;
  Session session;
};

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
      return compareFundsTransfer(lhs, rhs);

    case Scope::fullAudit:
      return compareFullAudit(lhs, rhs);
    }
    return false;
  }

private:
  [[nodiscard]] static bool
  compareFundsTransfer(const Transaction &lhs,
                       const Transaction &rhs) noexcept {
    return std::tie(lhs.timestamp, lhs.source, lhs.target, lhs.amount) <
           std::tie(rhs.timestamp, rhs.source, rhs.target, rhs.amount);
  }

  [[nodiscard]] static bool compareFullAudit(const Transaction &lhs,
                                             const Transaction &rhs) noexcept {
    return std::tie(lhs.timestamp, lhs.source, lhs.target, lhs.amount,
                    lhs.fraud.flag, lhs.fraud.ringId, lhs.session.channel,
                    lhs.session.deviceId, lhs.session.ipAddress) <
           std::tie(rhs.timestamp, rhs.source, rhs.target, rhs.amount,
                    rhs.fraud.flag, rhs.fraud.ringId, rhs.session.channel,
                    rhs.session.deviceId, rhs.session.ipAddress);
  }

  Scope scope_;
};

} // namespace PhantomLedger::transactions
