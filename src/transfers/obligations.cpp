#include "phantomledger/transfers/obligations.hpp"

#include "phantomledger/transfers/helpers.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace PhantomLedger::transfers::obligations {
namespace {

using Key = entity::Key;
using PersonId = entity::PersonId;
using TimePoint = time::TimePoint;
using entity::product::Direction;
using entity::product::ObligationEvent;
using entity::product::PortfolioRegistry;
using entity::product::ProductType;

// ----------------------------------------------------------------
// Per-account delinquency state
// ----------------------------------------------------------------

struct InstallmentState {
  /// Unpaid balance carried forward from previous cycles. Zero when
  /// the account is current.
  double carryDue = 0.0;

  /// Consecutive cycles with missing or partial payments. Reset to
  /// zero on a full or cure payment.
  int delinquentCycles = 0;
};

struct StateKey {
  PersonId person{};
  ProductType productType = ProductType::unknown;

  [[nodiscard]] bool operator==(const StateKey &) const noexcept = default;
};

struct StateKeyHash {
  [[nodiscard]] std::size_t operator()(const StateKey &k) const noexcept {
    const auto pid = static_cast<std::uint64_t>(k.person);
    const auto pt = static_cast<std::uint64_t>(k.productType);
    return std::hash<std::uint64_t>{}((pid << 8U) ^ pt);
  }
};

// ----------------------------------------------------------------
// Small helpers
// ----------------------------------------------------------------

/// Uniform double in [0, 1). Composed from `uniformInt` because it is
/// the one Rng primitive this module can rely on without asserting
/// the presence of a dedicated u01 method. Uses all 53 mantissa bits
/// of a double so the distribution covers every representable value
/// in the unit interval.
[[nodiscard]] double uniformUnit(random::Rng &rng) noexcept {
  constexpr std::int64_t kMantissa = 1LL << 53;
  const auto i = rng.uniformInt(0, kMantissa);
  return static_cast<double>(i) / static_cast<double>(kMantissa);
}

/// Uniform float in [low, high]. Matches Python `_uniform_between`.
[[nodiscard]] double uniformBetween(random::Rng &rng, double low, double high) {
  if (high < low) {
    throw std::invalid_argument("uniformBetween: high must be >= low");
  }
  if (high == low) {
    return low;
  }
  return low + (high - low) * uniformUnit(rng);
}

/// Light jitter for non-installment events: 0-2 days + 0-11 hours +
/// 0-59 minutes. Matches Python `_add_basic_jitter` exactly (Python
/// uses rng.int(0, 3) -> {0, 1, 2} and rng.int(0, 12) -> {0..11}).
[[nodiscard]] TimePoint addBasicJitter(random::Rng &rng,
                                       TimePoint ts) noexcept {
  const auto days = rng.uniformInt(0, 3);
  const auto hours = rng.uniformInt(0, 12);
  const auto minutes = rng.uniformInt(0, 60);
  return ts + time::Days{days} + time::Hours{hours} + time::Minutes{minutes};
}

/// Timestamp for a realized installment payment. Late payments get a
/// day offset in [lateDaysMin, lateDaysMax] plus hour/minute jitter;
/// on-time payments still get hour/minute jitter within the due day.
[[nodiscard]] TimePoint paymentTimestamp(random::Rng &rng, TimePoint dueTs,
                                         double lateP, std::int32_t lateDaysMin,
                                         std::int32_t lateDaysMax,
                                         bool forceLate) {
  const bool beLate = forceLate || rng.coin(lateP);

  if (beLate) {
    int delayDays = 0;
    if (lateDaysMax > 0) {
      // Python rng.int(lateDaysMin, lateDaysMax + 1) is [lateDaysMin,
      // lateDaysMax]. C++ uniformInt(low, high) is [low, high), so we
      // pass (lateDaysMin, lateDaysMax + 1).
      delayDays =
          static_cast<int>(rng.uniformInt(lateDaysMin, lateDaysMax + 1));
    }
    const auto hour = rng.uniformInt(8, 22);
    const auto minute = rng.uniformInt(0, 60);
    return dueTs + time::Days{delayDays} + time::Hours{hour} +
           time::Minutes{minute};
  }

  const auto hour = rng.uniformInt(6, 18);
  const auto minute = rng.uniformInt(0, 60);
  return dueTs + time::Hours{hour} + time::Minutes{minute};
}

/// Compose a draft with the shared rounding/timestamp helpers.
[[nodiscard]] transactions::Transaction
makeTxn(const transactions::Factory &txf, const Key &src, const Key &dst,
        double amount, TimePoint ts, channels::Tag channel) {
  transactions::Draft draft;
  draft.source = src;
  draft.destination = dst;
  draft.amount = helpers::roundMoney(amount);
  draft.timestamp = helpers::draftTimestamp(ts);
  draft.channel = channel;
  return txf.make(draft);
}

/// Cap an effective probability at 0.95 to guarantee a non-trivial
/// tail on every Bernoulli trial.
[[nodiscard]] constexpr double effectiveProb(double base,
                                             double multiplier) noexcept {
  const double x = base * multiplier;
  return std::clamp(x, 0.0, 0.95);
}

/// Uniform-fraction partial payment amount.
[[nodiscard]] double installmentPaymentAmount(random::Rng &rng, double totalDue,
                                              double partialMinFrac,
                                              double partialMaxFrac) {
  const double frac = uniformBetween(rng, partialMinFrac, partialMaxFrac);
  const double raw = totalDue * frac;
  const double paid = std::min(totalDue, std::max(0.01, raw));
  return helpers::roundMoney(paid);
}

// ----------------------------------------------------------------
// Plain (non-installment) event emission
// ----------------------------------------------------------------

/// Emit a non-installment event with light timestamp jitter. Returns
/// std::nullopt (via an empty-or-one-element vector push) if the
/// jittered timestamp falls outside the active window.
void emitPlainEvent(random::Rng &rng, const transactions::Factory &txf,
                    const ObligationEvent &event, const Key &personAcct,
                    TimePoint endExcl,
                    std::vector<transactions::Transaction> &out) {
  const auto ts = addBasicJitter(rng, event.timestamp);
  if (ts >= endExcl) {
    return;
  }

  Key src{};
  Key dst{};
  if (event.direction == Direction::outflow) {
    src = personAcct;
    dst = event.counterpartyAcct;
  } else {
    src = event.counterpartyAcct;
    dst = personAcct;
  }

  out.push_back(makeTxn(txf, src, dst, event.amount, ts, event.channel));
}

// ----------------------------------------------------------------
// Installment-loan event emission (the state machine)
// ----------------------------------------------------------------

/// Emit one scheduled installment due event as zero or one observed
/// transaction, updating the per-account state in-place.
///
/// Cases:
///   - carry_due > 0 + cure draws -> one cure txn clearing arrears
///   - miss draws                  -> no txn, full due carries forward
///   - partial draws               -> one partial txn; remainder carries
///   - normal                      -> one full scheduled payment
void emitInstallmentEvent(
    const PortfolioRegistry &registry,
    std::unordered_map<StateKey, InstallmentState, StateKeyHash> &stateByKey,
    random::Rng &rng, const transactions::Factory &txf,
    const ObligationEvent &event, const Key &personAcct, TimePoint endExcl,
    std::vector<transactions::Transaction> &out) {

  const auto *terms =
      registry.installmentTerms(event.personId, event.productType);
  if (terms == nullptr) {
    emitPlainEvent(rng, txf, event, personAcct, endExcl, out);
    return;
  }

  const StateKey key{event.personId, event.productType};
  auto &state = stateByKey[key]; // inserts default-constructed state

  const double scheduledAmount = helpers::roundMoney(event.amount);
  const double totalDue = helpers::roundMoney(scheduledAmount + state.carryDue);
  if (totalDue <= 0.0) {
    return;
  }

  // Delinquency multiplier: capped at two applications so the
  // probabilities do not explode once an account has been behind for
  // many cycles. Effectively, a streak of 3+ cycles looks like a
  // streak of 2 for transition purposes.
  double delinquencyMult = 1.0;
  if (state.delinquentCycles > 0) {
    const int exp = std::min(state.delinquentCycles, 2);
    delinquencyMult = std::pow(terms->clusterMult, exp);
  }

  const double effectiveLateP = effectiveProb(terms->lateP, delinquencyMult);
  const double effectiveMissP = effectiveProb(terms->missP, delinquencyMult);
  const double effectivePartialP =
      effectiveProb(terms->partialP,
                    1.0 + (0.5 * static_cast<double>(state.delinquentCycles)));

  // --- Cure branch ---
  //
  // If there's already unpaid balance, test the cure probability
  // first. A successful cure clears arrears and resets delinquency.
  if (state.carryDue > 0.0) {
    const double cureBoost =
        1.0 + (0.25 * static_cast<double>(state.delinquentCycles));
    const double effectiveCureP = std::min(0.98, terms->cureP * cureBoost);

    if (rng.coin(effectiveCureP)) {
      const auto cureTs =
          paymentTimestamp(rng, event.timestamp, std::max(effectiveLateP, 0.75),
                           terms->lateDaysMin, terms->lateDaysMax,
                           /*forceLate=*/true);
      if (cureTs < endExcl) {
        out.push_back(makeTxn(txf, personAcct, event.counterpartyAcct, totalDue,
                              cureTs, event.channel));
        state.carryDue = 0.0;
        state.delinquentCycles = 0;
      }
      return;
    }
  }

  // --- Three-way decision: miss / partial / full ---
  //
  // Draw one u in [0, 1) and walk down the probability ladder. This
  // keeps the three branches mutually exclusive.
  double decisionU = uniformUnit(rng);

  // Missed payment.
  if (decisionU < effectiveMissP) {
    state.carryDue = totalDue;
    state.delinquentCycles += 1;
    return;
  }
  decisionU -= effectiveMissP;

  // Partial payment.
  if (decisionU < effectivePartialP) {
    const double paidAmount = installmentPaymentAmount(
        rng, totalDue, terms->partialMinFrac, terms->partialMaxFrac);
    const double unpaidAmount = helpers::roundMoney(totalDue - paidAmount);

    if (paidAmount > 0.0) {
      const auto partialTs = paymentTimestamp(
          rng, event.timestamp, effectiveLateP, terms->lateDaysMin,
          terms->lateDaysMax, /*forceLate=*/false);
      if (partialTs < endExcl) {
        out.push_back(makeTxn(txf, personAcct, event.counterpartyAcct,
                              paidAmount, partialTs, event.channel));
      }
    }

    state.carryDue = unpaidAmount;
    state.delinquentCycles =
        (unpaidAmount > 0.0) ? state.delinquentCycles + 1 : 0;
    return;
  }

  // Full scheduled payment.
  const auto fullTs =
      paymentTimestamp(rng, event.timestamp, effectiveLateP, terms->lateDaysMin,
                       terms->lateDaysMax, /*forceLate=*/false);
  if (fullTs < endExcl) {
    out.push_back(makeTxn(txf, personAcct, event.counterpartyAcct,
                          scheduledAmount, fullTs, event.channel));
  }

  // If prior carry is still outstanding (scheduled payment only
  // covers the current cycle), delinquency persists. Otherwise, the
  // account is current.
  if (state.carryDue > 0.0) {
    state.delinquentCycles += 1;
  } else {
    state.delinquentCycles = 0;
  }
}

} // namespace

// ----------------------------------------------------------------
// Public API
// ----------------------------------------------------------------

std::vector<transactions::Transaction> emit(const PortfolioRegistry &registry,
                                            TimePoint start, TimePoint endExcl,
                                            const Population &population,
                                            random::Rng &rng,
                                            const transactions::Factory &txf) {
  std::vector<transactions::Transaction> out;

  std::unordered_map<StateKey, InstallmentState, StateKeyHash> stateByKey;

  for (const auto &event : registry.allEvents(start, endExcl)) {
    const auto acctIt = population.primaryAccounts->find(event.personId);
    if (acctIt == population.primaryAccounts->end()) {
      continue;
    }
    const Key personAcct = acctIt->second;

    const bool isInstallmentOutflow =
        event.direction == Direction::outflow &&
        entity::product::isInstallmentLoan(event.productType);

    if (isInstallmentOutflow) {
      emitInstallmentEvent(registry, stateByKey, rng, txf, event, personAcct,
                           endExcl, out);
    } else {
      emitPlainEvent(rng, txf, event, personAcct, endExcl, out);
    }
  }

  // Sort by timestamp for deterministic output. The full audit
  // comparator is unnecessary here because there's no cross-event
  // ordering to break.
  std::sort(out.begin(), out.end(),
            [](const transactions::Transaction &a,
               const transactions::Transaction &b) noexcept {
              return a.timestamp < b.timestamp;
            });

  return out;
}

} // namespace PhantomLedger::transfers::obligations
