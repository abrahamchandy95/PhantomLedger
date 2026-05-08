#include "phantomledger/transfers/legit/routines/internal.hpp"

#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::internal_xfer {

namespace {

inline constexpr std::array<double, 18> kRoundAmounts{
    25.0,  50.0,  50.0,  100.0, 100.0, 100.0,  150.0,  200.0,  200.0,
    250.0, 300.0, 500.0, 500.0, 750.0, 1000.0, 1000.0, 1500.0, 2000.0,
};

struct ActivePerson {
  std::vector<entity::Key> eligible;
};

struct Candidate {
  std::int64_t timestamp = 0;
  std::size_t personIdx = 0; // index into activePeople
  double amount = 0.0;
};

[[nodiscard]] std::vector<entity::Key>
eligibleAccountsFor(entity::PersonId person,
                    const blueprints::LegitBlueprint &plan) {
  std::vector<entity::Key> out;
  if (plan.allAccounts() == nullptr) {
    return out;
  }

  const auto recordIndices = plan.ownedAccountSlices().recordsFor(person);
  out.reserve(recordIndices.size());

  for (const auto recordIx : recordIndices) {
    if (recordIx >= plan.allAccounts()->records.size()) {
      continue;
    }

    const auto &record = plan.allAccounts()->records[recordIx];
    if (plan.counterparties().hubSet.contains(record.id)) {
      continue;
    }
    out.push_back(record.id);
  }
  return out;
}

[[nodiscard]] std::vector<ActivePerson>
selectActivePeople(random::Rng &rng, const blueprints::LegitBlueprint &plan,
                   const Config &cfg) {
  std::vector<ActivePerson> out;
  out.reserve(plan.persons().size());

  for (const auto person : plan.persons()) {
    auto eligible = eligibleAccountsFor(person, plan);
    if (eligible.size() < 2) {
      continue;
    }
    if (!rng.coin(cfg.activeP)) {
      continue;
    }
    out.push_back(ActivePerson{std::move(eligible)});
  }

  return out;
}

[[nodiscard]] double drawAmount(random::Rng &rng, const Config &cfg) {
  if (rng.coin(cfg.roundAmountP)) {
    return kRoundAmounts[rng.choiceIndex(kRoundAmounts.size())];
  }

  const auto raw = probability::distributions::lognormalByMedian(
      rng, cfg.amountMedian, cfg.amountSigma);
  return primitives::utils::roundMoney(std::max(10.0, raw));
}

[[nodiscard]] std::vector<Candidate>
buildCandidates(random::Rng &rng, const blueprints::LegitBlueprint &plan,
                std::span<const ActivePerson> activePeople, const Config &cfg) {
  std::vector<Candidate> out;

  std::size_t expected = activePeople.size() * plan.monthStarts().size() *
                         static_cast<std::size_t>(cfg.transfersPerMonthMax);
  out.reserve(expected);

  const auto endExcl =
      plan.startDate() + time::Days{static_cast<int>(plan.days())};
  const std::int64_t startEpoch = time::toEpochSeconds(plan.startDate());
  const std::int64_t endExclEpoch = time::toEpochSeconds(endExcl);

  for (const auto &monthAnchor : plan.monthStarts()) {
    const std::int64_t monthEpoch = time::toEpochSeconds(monthAnchor);

    for (std::size_t i = 0; i < activePeople.size(); ++i) {
      const auto n = static_cast<std::int32_t>(rng.uniformInt(
          cfg.transfersPerMonthMin,
          static_cast<std::int64_t>(cfg.transfersPerMonthMax) + 1));

      for (std::int32_t j = 0; j < n; ++j) {
        const auto amount = drawAmount(rng, cfg);
        std::int32_t dayOffset;
        if (rng.nextDouble() < 0.70) {
          dayOffset = static_cast<std::int32_t>(rng.uniformInt(0, 7));
        } else {
          dayOffset = static_cast<std::int32_t>(rng.uniformInt(6, 28));
        }

        const auto hour = static_cast<std::int32_t>(rng.uniformInt(8, 23));
        const auto minute = static_cast<std::int32_t>(rng.uniformInt(0, 61));

        const std::int64_t ts = monthEpoch +
                                static_cast<std::int64_t>(dayOffset) * 86400 +
                                static_cast<std::int64_t>(hour) * 3600 +
                                static_cast<std::int64_t>(minute) * 60;

        if (ts < startEpoch || ts >= endExclEpoch) {
          continue;
        }

        out.push_back(Candidate{ts, i, amount});
      }
    }
  }

  std::ranges::sort(out, [](const Candidate &a, const Candidate &b) {
    return a.timestamp < b.timestamp;
  });
  return out;
}

[[nodiscard]] entity::Key chooseSource(random::Rng &rng,
                                       std::span<const entity::Key> eligible,
                                       double amount,
                                       const ledger::SeededScreen &screen) {
  if (eligible.empty()) {
    return entity::Key{};
  }
  if (!screen.hasBook()) {
    return eligible[rng.choiceIndex(eligible.size())];
  }

  entity::Key best{};
  double bestCash = -std::numeric_limits<double>::infinity();
  for (const auto &acct : eligible) {
    const auto cash = screen.availableCash(acct);
    if (cash >= amount && cash > bestCash) {
      best = acct;
      bestCash = cash;
    }
  }
  return best;
}

[[nodiscard]] entity::Key
chooseDestination(random::Rng &rng, const entity::Key &source,
                  std::span<const entity::Key> eligible,
                  const ledger::SeededScreen &screen) {
  std::array<entity::Key, 16> stack{};
  std::size_t n = 0;
  for (const auto &acct : eligible) {
    if (acct == source) {
      continue;
    }
    if (n < stack.size()) {
      stack[n++] = acct;
    }
  }

  std::vector<entity::Key> heap;
  std::span<const entity::Key> destinations;
  if (n < stack.size()) {
    destinations = std::span<const entity::Key>(stack.data(), n);
  } else {
    heap.assign(eligible.begin(), eligible.end());
    heap.erase(std::remove(heap.begin(), heap.end(), source), heap.end());
    destinations = std::span<const entity::Key>(heap);
  }

  if (destinations.empty()) {
    return entity::Key{};
  }
  if (!screen.hasBook() || destinations.size() == 1) {
    return destinations[rng.choiceIndex(destinations.size())];
  }

  entity::Key leanest = destinations.front();
  double leanestCash = screen.availableCash(leanest);
  for (std::size_t i = 1; i < destinations.size(); ++i) {
    const auto cash = screen.availableCash(destinations[i]);
    if (cash < leanestCash) {
      leanestCash = cash;
      leanest = destinations[i];
    }
  }
  return leanest;
}

[[nodiscard]] transactions::Draft draftFrom(const entity::Key &source,
                                            const entity::Key &destination,
                                            const Candidate &candidate,
                                            channels::Tag channel) noexcept {
  return transactions::Draft{
      .source = source,
      .destination = destination,
      .amount = candidate.amount,
      .timestamp = candidate.timestamp,
      .isFraud = 0,
      .ringId = -1,
      .channel = channel,
  };
}

} // namespace

void Config::validate() const {
  namespace v = primitives::validate;
  v::between("activeP", activeP, 0.0, 1.0);
  v::ge("transfersPerMonthMin", transfersPerMonthMin, 1);
  v::ge("transfersPerMonthMax", transfersPerMonthMax, transfersPerMonthMin);
  v::gt("amountMedian", amountMedian, 0.0);
  v::ge("amountSigma", amountSigma, 0.0);
  v::between("roundAmountP", roundAmountP, 0.0, 1.0);
}

std::vector<transactions::Transaction>
Generator::generate(const blueprints::LegitBlueprint &plan, Config cfg) {
  cfg.validate();

  std::vector<transactions::Transaction> out;
  if (plan.monthStarts().empty()) {
    return out;
  }

  auto activePeople = selectActivePeople(rng_, plan, cfg);
  if (activePeople.empty()) {
    return out;
  }

  auto candidates = buildCandidates(rng_, plan, activePeople, cfg);

  const auto channel = channels::tag(channels::Legit::selfTransfer);
  out.reserve(candidates.size());

  for (const auto &candidate : candidates) {
    screen_.advanceThrough(candidate.timestamp);

    const auto &eligible = activePeople[candidate.personIdx].eligible;
    const auto source = chooseSource(rng_, eligible, candidate.amount, screen_);
    if (source == entity::Key{}) {
      continue;
    }

    const auto destination = chooseDestination(rng_, source, eligible, screen_);
    if (destination == entity::Key{}) {
      continue;
    }

    if (!screen_.acceptTransfer(source, destination, candidate.amount, channel,
                                candidate.timestamp)) {
      continue;
    }

    out.push_back(
        txf_.make(draftFrom(source, destination, candidate, channel)));
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::internal_xfer
