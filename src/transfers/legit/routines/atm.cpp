#include "phantomledger/transfers/legit/routines/atm.hpp"

#include "phantomledger/encoding/external.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/constants.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>

namespace PhantomLedger::transfers::legit::routines::atm {

namespace {

inline constexpr std::array<double, 18> kAtmAmounts{
    20.0,  40.0,  40.0,  40.0,  60.0,  60.0,  60.0,  80.0,  80.0,
    100.0, 100.0, 100.0, 120.0, 140.0, 160.0, 200.0, 200.0, 300.0,
};

struct Candidate {
  std::int64_t timestamp = 0;
  entity::Key depositAcct{};
  double amount = 0.0;
};

// H1 step 2b (class P + S lattice): the withdrawal scales to the event
// year's CPI level, then RE-SNAPS to the $20 note denomination — a
// 1991 withdrawal is fewer $20s, not scaled $20s (authority U-6).
[[nodiscard]] double nominalAtmAmount(double calibrationAmount,
                                      double priceScale) {
  const double scaled = calibrationAmount * priceScale;
  return std::max(20.0, std::round(scaled / 20.0) * 20.0);
}

[[nodiscard]] bool canAffordAtm(
    const ::PhantomLedger::transfers::legit::ledger::SeededScreen &screen,
    const entity::Key &account, double amount, double priceScale) {
  if (!screen.hasBook()) {
    return true;
  }

  const auto available = screen.liquidity(account);
  // The $40-$120 reserve band is a BEHAVIORAL screen — it scales with
  // the era it screens (authority U-6 invariant-4 sweep).
  const auto reserve =
      std::clamp(amount * 0.50, 40.0 * priceScale, 120.0 * priceScale);
  return available >= amount + reserve;
}

[[nodiscard]] std::vector<entity::Key>
selectActiveUsers(random::Rng &rng, const blueprints::LegitBlueprint &plan,
                  const entity::account::Registry &registry,
                  const Config &cfg) {
  std::vector<entity::Key> activeUsers;
  activeUsers.reserve(plan.persons().size());

  for (const auto person : plan.persons()) {
    const auto it = plan.primaryAcctRecordIx().find(person);
    if (it == plan.primaryAcctRecordIx().end()) {
      continue;
    }

    const auto &record = registry.records[it->second];
    if (plan.counterparties().hubSet.contains(record.id)) {
      continue;
    }
    if (encoding::isExternal(record.id)) {
      continue;
    }
    if (!rng.coin(cfg.userP)) {
      continue;
    }

    activeUsers.push_back(record.id);
  }

  return activeUsers;
}

// H3 (macro-history-v1): the dead withdraw no cash. The map goes
// deposit account -> death epoch through the blueprint pack's timeline
// lane; the EMISSION loop skips dead candidates, so every rng draw
// (user coins, per-month counts, candidate sampling) is byte-identical
// — only emitted rows and their soft-screen debits disappear. Empty
// when the pack carries no timeline lane (the filter stands down).
[[nodiscard]] std::unordered_map<entity::Key, std::int64_t,
                                 std::hash<entity::Key>>
deathEpochByAccount(const blueprints::LegitBlueprint &plan,
                    const entity::account::Registry &registry) {
  std::unordered_map<entity::Key, std::int64_t, std::hash<entity::Key>> out;

  const auto *pack = plan.personas().pack;
  if (pack == nullptr || pack->timelines.empty()) {
    return out;
  }

  out.reserve(plan.persons().size());
  for (const auto person : plan.persons()) {
    if (person == 0 || person > pack->timelines.size()) {
      continue;
    }
    const auto it = plan.primaryAcctRecordIx().find(person);
    if (it == plan.primaryAcctRecordIx().end()) {
      continue;
    }
    out.emplace(registry.records[it->second].id,
                time::toEpochSeconds(pack->timelines[person - 1].death));
  }

  return out;
}

[[nodiscard]] Candidate sampleCandidate(random::Rng &rng,
                                        std::int64_t monthAnchorEpoch,
                                        const entity::Key &depositAcct) {
  const double amount = kAtmAmounts[rng.choiceIndex(kAtmAmounts.size())];

  std::int32_t dayOffset;
  if (rng.nextDouble() < 0.75) {
    dayOffset = static_cast<std::int32_t>(rng.uniformInt(0, 19));
  } else {
    dayOffset = static_cast<std::int32_t>(rng.uniformInt(18, 28));
  }

  const auto hour = static_cast<std::int32_t>(rng.uniformInt(7, 24));
  const auto minute = static_cast<std::int32_t>(rng.uniformInt(0, 61));

  const std::int64_t timestamp =
      monthAnchorEpoch +
      static_cast<std::int64_t>(dayOffset) *
          ::PhantomLedger::time::kSecondsPerDay +
      ::PhantomLedger::time::secondsInDay(hour, minute);

  return Candidate{timestamp, depositAcct, amount};
}

[[nodiscard]] std::vector<Candidate>
makeCandidates(random::Rng &rng, const blueprints::LegitBlueprint &plan,
               std::span<const entity::Key> activeUsers, const Config &cfg) {
  std::vector<Candidate> candidates;
  candidates.reserve(static_cast<std::size_t>(plan.monthStarts().size()) *
                     activeUsers.size() *
                     static_cast<std::size_t>(cfg.withdrawalsPerMonthMax));

  const auto endExcl =
      plan.startDate() + time::Days{static_cast<int>(plan.days())};
  const std::int64_t startEpoch = time::toEpochSeconds(plan.startDate());
  const std::int64_t endExclEpoch = time::toEpochSeconds(endExcl);

  for (const auto &monthAnchor : plan.monthStarts()) {
    const std::int64_t monthAnchorEpoch = time::toEpochSeconds(monthAnchor);

    for (const auto &depositAcct : activeUsers) {
      const auto nWithdrawals = static_cast<std::int32_t>(rng.uniformInt(
          cfg.withdrawalsPerMonthMin,
          static_cast<std::int64_t>(cfg.withdrawalsPerMonthMax) + 1));

      for (std::int32_t i = 0; i < nWithdrawals; ++i) {
        const auto candidate =
            sampleCandidate(rng, monthAnchorEpoch, depositAcct);
        if (candidate.timestamp < startEpoch ||
            candidate.timestamp >= endExclEpoch) {
          continue;
        }

        candidates.push_back(candidate);
      }
    }
  }

  std::ranges::sort(candidates, [](const Candidate &a, const Candidate &b) {
    return a.timestamp < b.timestamp;
  });

  return candidates;
}

} // namespace

void Config::validate() const {
  namespace v = primitives::validate;
  v::between("userP", userP, 0.0, 1.0);
  v::ge("withdrawalsPerMonthMin", withdrawalsPerMonthMin, 1);
  v::ge("withdrawalsPerMonthMax", withdrawalsPerMonthMax,
        withdrawalsPerMonthMin);
}

Generator::Generator(
    random::Rng &rng, const transactions::Factory &txf,
    ::PhantomLedger::transfers::legit::ledger::SeededScreen &screen, Config cfg)
    : rng_{rng}, txf_{txf}, screen_{screen}, cfg_{cfg} {
  cfg_.validate();
}

std::vector<transactions::Transaction>
Generator::generate(const blueprints::LegitBlueprint &plan,
                    const entity::account::Registry &registry) {
  std::vector<transactions::Transaction> out;
  if (plan.monthStarts().empty() || plan.counterparties().hubAccounts.empty()) {
    return out;
  }

  const auto activeUsers = selectActiveUsers(rng_, plan, registry, cfg_);
  if (activeUsers.empty()) {
    return out;
  }

  const auto candidates = makeCandidates(rng_, plan, activeUsers, cfg_);
  const auto deathEpoch = deathEpochByAccount(plan, registry);
  const auto atmNetworkAcct = plan.counterparties().hubAccounts.front();
  const auto channel = channels::tag(channels::Legit::atm);

  out.reserve(candidates.size());
  for (const auto &cand : candidates) {
    screen_.advanceThrough(cand.timestamp, /*inclusive=*/true);

    // H3: the dead withdraw no cash (no draws in this loop — the
    // stream is untouched).
    if (const auto it = deathEpoch.find(cand.depositAcct);
        it != deathEpoch.end() && cand.timestamp >= it->second) {
      continue;
    }

    const double scale = ::PhantomLedger::synth::econ::priceScale(
        time::toCalendarDate(time::fromEpochSeconds(cand.timestamp)).year);
    const double amount = nominalAtmAmount(cand.amount, scale);

    if (!canAffordAtm(screen_, cand.depositAcct, amount, scale)) {
      continue;
    }

    if (!screen_.acceptTransfer(ledger::KeyedTransfer{
            .source = cand.depositAcct,
            .destination = atmNetworkAcct,
            .amount = amount,
            .channel = channel,
            .timestamp = cand.timestamp,
        })) {
      continue;
    }

    out.push_back(txf_.make(transactions::Draft{
        .source = cand.depositAcct,
        .destination = atmNetworkAcct,
        .amount = amount,
        .timestamp = cand.timestamp,
        .isFraud = 0,
        .ringId = -1,
        .channel = channel,
    }));
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::atm
