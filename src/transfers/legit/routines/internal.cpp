#include "phantomledger/transfers/legit/routines/internal.hpp"

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <unordered_set>

namespace PhantomLedger::transfers::legit::routines::internal_xfer {

namespace {

inline constexpr std::array<double, 18> kRoundAmounts{
    25.0,  50.0,  50.0,  100.0, 100.0, 100.0,  150.0,  200.0,  200.0,
    250.0, 300.0, 500.0, 500.0, 750.0, 1000.0, 1000.0, 1500.0, 2000.0,
};

[[nodiscard]] std::vector<entity::Key>
eligibleAccountsFor(entity::PersonId person,
                    const entity::account::Ownership &ownership,
                    const entity::account::Registry &registry,
                    const std::unordered_set<entity::Key> &hubSet) {
  std::vector<entity::Key> out;
  if (person == entity::invalidPerson ||
      static_cast<std::size_t>(person) >= ownership.byPersonOffset.size()) {
    return out;
  }

  const auto start = ownership.byPersonOffset[person - 1];
  const auto end = ownership.byPersonOffset[person];
  out.reserve(end - start);

  for (auto idx = start; idx < end; ++idx) {
    const auto recordIx = ownership.byPersonIndex[idx];
    const auto &record = registry.records[recordIx];
    if (hubSet.contains(record.id)) {
      continue;
    }
    out.push_back(record.id);
  }
  return out;
}

[[nodiscard]] entity::Key chooseSource(random::Rng &rng,
                                       std::span<const entity::Key> eligible,
                                       double amount, clearing::Ledger *book) {
  if (eligible.empty()) {
    return entity::Key{};
  }
  if (book == nullptr) {
    return eligible[rng.choiceIndex(eligible.size())];
  }

  entity::Key best{};
  double bestCash = -std::numeric_limits<double>::infinity();
  for (const auto &acct : eligible) {
    const auto cash = book->availableCash(acct);
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
                  clearing::Ledger *book) {

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
  if (book == nullptr || destinations.size() == 1) {
    return destinations[rng.choiceIndex(destinations.size())];
  }

  entity::Key leanest = destinations.front();
  double leanestCash = book->availableCash(leanest);
  for (std::size_t i = 1; i < destinations.size(); ++i) {
    const auto cash = book->availableCash(destinations[i]);
    if (cash < leanestCash) {
      leanestCash = cash;
      leanest = destinations[i];
    }
  }
  return leanest;
}

struct Candidate {
  std::int64_t timestamp = 0;
  std::uint32_t personIdx = 0; // index into activePeople
  double amount = 0.0;
};

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
generate(random::Rng &rng, const blueprints::LegitBuildPlan &plan,
         const transactions::Factory &txf,
         const entity::account::Ownership &ownership,
         const entity::account::Registry &registry, clearing::Ledger *book,
         std::span<const transactions::Transaction> baseTxns,
         bool baseTxnsSorted, Config cfg) {
  cfg.validate();

  std::vector<transactions::Transaction> out;
  if (plan.monthStarts.empty()) {
    return out;
  }

  const auto endExcl = plan.startDate + time::Days{static_cast<int>(plan.days)};
  const auto channel = channels::tag(channels::Legit::selfTransfer);
  const auto &hubSet = plan.counterparties.hubSet;

  // ---- Resolve seeded base txns (same pattern as ATM) ----
  std::vector<transactions::Transaction> sortedHolder;
  std::span<const transactions::Transaction> seeded;
  if (baseTxnsSorted || baseTxns.empty()) {
    seeded = baseTxns;
  } else {
    sortedHolder.assign(baseTxns.begin(), baseTxns.end());
    std::ranges::sort(sortedHolder,
                      [](const transactions::Transaction &a,
                         const transactions::Transaction &b) noexcept {
                        return a.timestamp < b.timestamp;
                      });
    seeded = std::span<const transactions::Transaction>(sortedHolder);
  }
  std::size_t seedIdx = 0;

  // ---- Cohort selection ----

  struct ActivePerson {
    std::vector<entity::Key> eligible;
  };
  std::vector<ActivePerson> activePeople;
  activePeople.reserve(plan.persons.size());

  for (const auto person : plan.persons) {
    auto eligible = eligibleAccountsFor(person, ownership, registry, hubSet);
    if (eligible.size() < 2) {
      continue;
    }
    if (!rng.coin(cfg.activeP)) {
      continue;
    }
    activePeople.push_back(ActivePerson{std::move(eligible)});
  }

  if (activePeople.empty()) {
    return out;
  }

  // ---- Candidate generation ----
  std::vector<Candidate> candidates;
  candidates.reserve(static_cast<std::size_t>(plan.monthStarts.size()) *
                     activePeople.size() *
                     static_cast<std::size_t>(cfg.transfersPerMonthMax));

  const std::int64_t startEpoch = time::toEpochSeconds(plan.startDate);
  const std::int64_t endExclEpoch = time::toEpochSeconds(endExcl);

  for (const auto &monthAnchor : plan.monthStarts) {
    const std::int64_t monthAnchorEpoch = time::toEpochSeconds(monthAnchor);

    for (std::uint32_t i = 0; i < activePeople.size(); ++i) {
      const auto nTransfers = static_cast<std::int32_t>(rng.uniformInt(
          cfg.transfersPerMonthMin,
          static_cast<std::int64_t>(cfg.transfersPerMonthMax) + 1));

      for (std::int32_t k = 0; k < nTransfers; ++k) {
        double amount;
        if (rng.coin(cfg.roundAmountP)) {
          amount = kRoundAmounts[rng.choiceIndex(kRoundAmounts.size())];
        } else {
          const auto raw = probability::distributions::lognormalByMedian(
              rng, cfg.amountMedian, cfg.amountSigma);
          amount = primitives::utils::roundMoney(std::max(10.0, raw));
        }

        std::int32_t dayOffset;
        if (rng.nextDouble() < 0.70) {
          dayOffset = static_cast<std::int32_t>(rng.uniformInt(0, 7));
        } else {
          dayOffset = static_cast<std::int32_t>(rng.uniformInt(6, 28));
        }

        const auto hour = static_cast<std::int32_t>(rng.uniformInt(8, 23));
        const auto minute = static_cast<std::int32_t>(rng.uniformInt(0, 61));

        const std::int64_t ts = monthAnchorEpoch +
                                static_cast<std::int64_t>(dayOffset) * 86400 +
                                static_cast<std::int64_t>(hour) * 3600 +
                                static_cast<std::int64_t>(minute) * 60;

        if (ts < startEpoch || ts >= endExclEpoch) {
          continue;
        }

        candidates.push_back(Candidate{ts, i, amount});
      }
    }
  }

  std::ranges::sort(candidates, [](const Candidate &a, const Candidate &b) {
    return a.timestamp < b.timestamp;
  });

  // ---- Screen + emit ----
  out.reserve(candidates.size());
  for (const auto &cand : candidates) {
    seedIdx =
        clearing::advanceBookThrough(book, seeded, seedIdx, cand.timestamp,
                                     /*inclusive=*/true);

    const auto &eligible = activePeople[cand.personIdx].eligible;
    const auto src = chooseSource(rng, eligible, cand.amount, book);
    if (src == entity::Key{}) {
      continue;
    }
    const auto dst = chooseDestination(rng, src, eligible, book);
    if (dst == entity::Key{}) {
      continue;
    }

    if (book != nullptr) {
      const auto srcIdx = book->findAccount(src);
      const auto dstIdx = book->findAccount(dst);
      const auto decision = book->transferAt(srcIdx, dstIdx, cand.amount,
                                             channel, cand.timestamp);
      if (!decision.accepted()) {
        continue;
      }
    }

    out.push_back(txf.make(transactions::Draft{
        .source = src,
        .destination = dst,
        .amount = cand.amount,
        .timestamp = cand.timestamp,
        .isFraud = 0,
        .ringId = -1,
        .channel = channel,
    }));
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::internal_xfer
