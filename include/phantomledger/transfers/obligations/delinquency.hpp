#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/entities/products/installment_terms.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace PhantomLedger::transfers::obligations::delinquency {

/// Per-account state tracked across due cycles.
struct State {
  /// Unpaid balance carried forward from prior cycles
  double carryDue = 0.0;

  /// Consecutive cycles with missing or partial payments
  int delinquentCycles = 0;
};

/// Per-(person, productType) keying for the state map.
struct StateKey {
  entity::PersonId person{};
  entity::product::ProductType productType =
      entity::product::ProductType::unknown;

  [[nodiscard]] bool operator==(const StateKey &) const noexcept = default;
};

struct StateKeyHash {
  [[nodiscard]] std::size_t operator()(const StateKey &k) const noexcept {
    const auto pid = static_cast<std::uint64_t>(k.person);
    const auto pt = static_cast<std::uint64_t>(k.productType);
    return std::hash<std::uint64_t>{}((pid << 8U) ^ pt);
  }
};

using StateMap = std::unordered_map<StateKey, State, StateKeyHash>;

/// What the state machine decided to do with a single due cycle.
enum class Action : std::uint8_t {
  noPayment, ///< miss; the entire totalDue carries forward
  cure,      ///< pay totalDue (carry + scheduled) in one transaction
  partial,   ///< pay a fraction of totalDue; remainder carries forward
  full,      ///< pay scheduled; any prior carry remains outstanding
};

/// The result of advancing one due cycle. The emitter consumes this
/// to decide whether to write a transaction and at what amount.
struct Outcome {
  Action action = Action::full;
  double amount = 0.0;         ///< dollars to actually pay; 0 if no_payment
  double effectiveLateP = 0.0; ///< late-probability for timestamp
  bool forceLate = false;      ///< true on cure
};

namespace detail {

/// Uniform double in [0, 1).
[[nodiscard]] inline double uniformUnit(random::Rng &rng) noexcept {
  constexpr std::int64_t kMantissa = 1LL << 53;
  const auto i = rng.uniformInt(0, kMantissa);
  return static_cast<double>(i) / static_cast<double>(kMantissa);
}

[[nodiscard]] inline double uniformBetween(random::Rng &rng, double low,
                                           double high) {
  if (high < low) {
    throw std::invalid_argument("uniformBetween: high must be >= low");
  }
  if (high == low) {
    return low;
  }
  return low + (high - low) * uniformUnit(rng);
}

/// Effective transition probability with a 95% cap so every Bernoulli
/// trial keeps a non-trivial tail.
[[nodiscard]] constexpr double effective(double base,
                                         double multiplier) noexcept {
  const double x = base * multiplier;
  return std::clamp(x, 0.0, 0.95);
}

[[nodiscard]] inline double
partialPaymentAmount(random::Rng &rng, double totalDue,
                     const entity::product::InstallmentTerms &terms) {
  const double frac =
      uniformBetween(rng, terms.partialMinFrac, terms.partialMaxFrac);
  const double raw = totalDue * frac;
  const double paid = std::min(totalDue, std::max(0.01, raw));
  return primitives::utils::roundMoney(paid);
}

} // namespace detail
/*
// The state machine sequencing:
  1. If there's an outstanding carry, test the cure probability
      first. A successful cure clears arrears and short-circuits
      everything else.
  2. Otherwise, draw one uniform u in [0, 1) and walk the
      ladder: miss → partial → full. The branches are mutually
      exclusive by construction.
  3. After the action is chosen, update carryDue and
      delinquentCycles to reflect the post-cycle reality.
*/
[[nodiscard]] inline Outcome
advance(random::Rng &rng, State &state,
        const entity::product::InstallmentTerms &terms,
        double scheduledAmount) {
  const double totalDue =
      primitives::utils::roundMoney(scheduledAmount + state.carryDue);
  if (totalDue <= 0.0) {
    return Outcome{.action = Action::noPayment};
  }

  // Cluster multiplier on transition probabilities. Capped at two
  // applications so a 3+ cycle streak doesn't blow up the math.
  double clusterMult = 1.0;
  if (state.delinquentCycles > 0) {
    const int exp = std::min(state.delinquentCycles, 2);
    clusterMult = std::pow(terms.clusterMult, exp);
  }

  const double effLate = detail::effective(terms.lateP, clusterMult);
  const double effMiss = detail::effective(terms.missP, clusterMult);
  const double effPartial = detail::effective(
      terms.partialP,
      1.0 + (0.5 * static_cast<double>(state.delinquentCycles)));

  // --- Cure branch ---
  if (state.carryDue > 0.0) {
    const double cureBoost =
        1.0 + (0.25 * static_cast<double>(state.delinquentCycles));
    const double effCure = std::min(0.98, terms.cureP * cureBoost);
    if (rng.coin(effCure)) {
      state.carryDue = 0.0;
      state.delinquentCycles = 0;
      return Outcome{
          .action = Action::cure,
          .amount = totalDue,
          .effectiveLateP = std::max(effLate, 0.75),
          .forceLate = true,
      };
    }
  }

  // --- Three-way decision ---
  double decisionU = detail::uniformUnit(rng);

  if (decisionU < effMiss) {
    state.carryDue = totalDue;
    state.delinquentCycles += 1;
    return Outcome{.action = Action::noPayment};
  }
  decisionU -= effMiss;

  if (decisionU < effPartial) {
    const double paid = detail::partialPaymentAmount(rng, totalDue, terms);
    const double unpaid = primitives::utils::roundMoney(totalDue - paid);
    state.carryDue = unpaid;
    state.delinquentCycles = (unpaid > 0.0) ? state.delinquentCycles + 1 : 0;
    return Outcome{
        .action = Action::partial,
        .amount = paid,
        .effectiveLateP = effLate,
    };
  }

  // Full scheduled payment. Any prior carry persists; if it's still
  // outstanding, delinquency continues.
  if (state.carryDue > 0.0) {
    state.delinquentCycles += 1;
  } else {
    state.delinquentCycles = 0;
  }
  return Outcome{
      .action = Action::full,
      .amount = scheduledAmount,
      .effectiveLateP = effLate,
  };
}

} // namespace PhantomLedger::transfers::obligations::delinquency
