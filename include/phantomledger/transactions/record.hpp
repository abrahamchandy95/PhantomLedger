#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <tuple>

namespace PhantomLedger::transactions {

struct Fraud {
  std::uint8_t flag = 0;
  std::optional<std::uint32_t> ringId;

  std::optional<std::uint32_t> chainId;

  fraud::FraudType type = fraud::FraudType::none;

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

// The semantic funds-transfer key. NOT a total order over rows — distinct
// rows can tie here while differing in fraud/session fields. Use this for
// funds-key equality censuses (the tie register); use the Comparator (or
// auditKey) for every sort and merge.
[[nodiscard]] inline auto fundsKey(const Transaction &tx) noexcept {
  return std::tie(tx.timestamp, tx.source, tx.target, tx.amount);
}

[[nodiscard]] inline auto auditKey(const Transaction &tx) noexcept {
  return std::tie(tx.timestamp, tx.source, tx.target, tx.amount, tx.fraud.flag,
                  tx.fraud.ringId, tx.fraud.chainId, tx.session.channel,
                  tx.session.deviceId, tx.session.ipAddress);
}

} // namespace detail

// S10 ordering re-pin: the funds-transfer REPLAY ORDER is the funds key
// (timestamp, source, target, amount) TOTALIZED by the remaining audit
// fields as tie-breakers — the audit key, whose first four fields ARE the
// funds key. The soak-scale tie register found adjacent funds-key ties,
// which made tie placement merge-history-dependent across architectures;
// with the audit-key extension the order is content-determined and total.
// Rows that still compare equal are byte-identical, so their permutation
// cannot affect output. Scope::fundsTransfer therefore orders by the
// audit key; the two scopes now agree and are both kept for call-site
// intent.
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
      return detail::auditKey(lhs) < detail::auditKey(rhs);

    case Scope::fullAudit:
      return detail::auditKey(lhs) < detail::auditKey(rhs);
    }

    return false;
  }

private:
  Scope scope_;
};

} // namespace PhantomLedger::transactions
