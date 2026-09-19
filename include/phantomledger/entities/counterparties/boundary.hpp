#pragma once

#include <cstdint>

namespace PhantomLedger::entity::boundary {

// What an external endpoint represents in the customer-ledger projection.
// Generic counterparties preserve the legacy behavior for employers,
// merchants, billers, and other external parties whose rail contract has not
// been specialized.
enum class Kind : std::uint8_t {
  genericCounterparty,
  atmTerminal,
  cashDepository,
  checkCapture,
  cryptoVenue,
};

// Direction is relative to the modeled customer book, not to the external
// service point.  The values are bits because a venue may support both legs.
enum class Flow : std::uint8_t {
  inbound = 1U << 0U,
  outbound = 1U << 1U,
};

[[nodiscard]] constexpr std::uint8_t bit(Flow flow) noexcept {
  return static_cast<std::uint8_t>(flow);
}

inline constexpr std::uint8_t kBothFlows =
    static_cast<std::uint8_t>(bit(Flow::inbound) | bit(Flow::outbound));

struct Policy {
  Kind kind = Kind::genericCounterparty;
  std::uint8_t allowedFlows = kBothFlows;

  constexpr bool operator==(const Policy &) const noexcept = default;
};

inline constexpr Policy kGenericPolicy{};

[[nodiscard]] constexpr bool allows(Policy policy, Flow flow) noexcept {
  return (policy.allowedFlows & bit(flow)) != 0U;
}

[[nodiscard]] constexpr bool typed(Policy policy) noexcept {
  return policy.kind != Kind::genericCounterparty;
}

} // namespace PhantomLedger::entity::boundary
