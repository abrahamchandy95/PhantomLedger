#include "phantomledger/transfers/legit/routines/subscriptions.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace PhantomLedger::transfers::legit::routines::subscriptions {

namespace {

inline constexpr std::array<double, 18> kPricePool{
    6.99,  7.99,  9.99,  10.99, 11.99, 12.99, 14.99, 15.49, 15.99,
    17.99, 22.99, 24.99, 29.99, 34.99, 39.99, 49.99, 59.99, 99.99,
};

struct PersonSub {
  entity::Key merchantAcct{};
  double amount = 0.0;
  std::uint8_t billingDay = 1; // 1..28
};

struct ActivePerson {
  entity::Key depositAcct{};
  std::vector<PersonSub> subs;
};

struct Candidate {
  std::int64_t timestamp = 0;
  entity::Key depositAcct{};
  entity::Key merchantAcct{};
  double amount = 0.0;
};

[[nodiscard]] std::span<const entity::Key>
merchantAccounts(const blueprints::LegitBuildPlan &plan) noexcept {
  return std::span<const entity::Key>(
      plan.counterparties.billerAccounts.data(),
      plan.counterparties.billerAccounts.size());
}

[[nodiscard]] std::vector<PersonSub>
assignSubscriptions(const random::RngFactory &rngFactory,
                    entity::PersonId person, const Config &cfg,
                    std::span<const entity::Key> merchantAccts) {
  if (merchantAccts.empty()) {
    return {};
  }

  const auto personStr = std::to_string(static_cast<std::uint64_t>(person));
  auto gen = rngFactory.rng({"subscriptions", std::string_view(personStr)});

  const auto nTotal = static_cast<std::int32_t>(gen.uniformInt(
      cfg.minPerPerson, static_cast<std::int64_t>(cfg.maxPerPerson) + 1));

  std::int32_t nDebit = 0;
  for (std::int32_t i = 0; i < nTotal; ++i) {
    if (gen.coin(cfg.debitP)) {
      ++nDebit;
    }
  }
  if (nDebit == 0) {
    return {};
  }

  const auto nPick = static_cast<std::size_t>(std::min<std::int32_t>(
      nDebit, static_cast<std::int32_t>(kPricePool.size())));

  // Without-replacement price draw, by index.
  const auto priceIdx =
      gen.choiceIndices(kPricePool.size(), nPick, /*replace=*/false);

  std::vector<PersonSub> out;
  out.reserve(nPick);
  for (std::size_t i = 0; i < nPick; ++i) {
    const auto merchantIdx = gen.choiceIndex(merchantAccts.size());
    const auto billingDay = static_cast<std::uint8_t>(gen.uniformInt(1, 29));

    out.push_back(PersonSub{
        .merchantAcct = merchantAccts[merchantIdx],
        .amount = kPricePool[priceIdx[i]],
        .billingDay = billingDay,
    });
  }
  return out;
}

[[nodiscard]] std::vector<ActivePerson>
selectActivePeople(const blueprints::LegitBuildPlan &plan,
                   const entity::account::Registry &registry, const Config &cfg,
                   std::span<const entity::Key> merchantAccts) {
  const random::RngFactory subFactory{plan.seed};

  std::vector<ActivePerson> active;
  active.reserve(plan.persons.size());

  for (const auto person : plan.persons) {
    const auto it = plan.primaryAcctRecordIx.find(person);
    if (it == plan.primaryAcctRecordIx.end()) {
      continue;
    }
    const auto &record = registry.records[it->second];
    if (plan.counterparties.hubSet.contains(record.id)) {
      continue;
    }

    auto subs = assignSubscriptions(subFactory, person, cfg, merchantAccts);
    if (subs.empty()) {
      continue;
    }
    active.push_back(ActivePerson{record.id, std::move(subs)});
  }

  return active;
}

[[nodiscard]] std::size_t
maxCandidateCount(std::span<const ActivePerson> active,
                  std::span<const time::TimePoint> monthStarts) noexcept {
  std::size_t maxCandidates = 0;
  for (const auto &person : active) {
    maxCandidates += person.subs.size();
  }
  return maxCandidates * monthStarts.size();
}

[[nodiscard]] Candidate sampleCandidate(random::Rng &rng,
                                        std::int64_t monthEpoch,
                                        const ActivePerson &person,
                                        const PersonSub &sub,
                                        const Config &cfg) {
  std::int32_t day = std::min<std::int32_t>(sub.billingDay, 28);

  const auto jitter = static_cast<std::int32_t>(
      rng.uniformInt(-static_cast<std::int64_t>(cfg.dayJitter),
                     static_cast<std::int64_t>(cfg.dayJitter) + 1));
  day = std::clamp<std::int32_t>(day + jitter, 1, 28);

  const auto hour = static_cast<std::int32_t>(rng.uniformInt(0, 7));
  const auto minute = static_cast<std::int32_t>(rng.uniformInt(0, 61));

  const std::int64_t timestamp = monthEpoch +
                                 static_cast<std::int64_t>(day - 1) * 86400 +
                                 static_cast<std::int64_t>(hour) * 3600 +
                                 static_cast<std::int64_t>(minute) * 60;

  return Candidate{
      .timestamp = timestamp,
      .depositAcct = person.depositAcct,
      .merchantAcct = sub.merchantAcct,
      .amount = sub.amount,
  };
}

[[nodiscard]] std::vector<Candidate>
makeCandidates(random::Rng &rng, const blueprints::LegitBuildPlan &plan,
               std::span<const ActivePerson> active, const Config &cfg) {
  const auto endExcl = plan.startDate + time::Days{static_cast<int>(plan.days)};
  const std::int64_t startEpoch = time::toEpochSeconds(plan.startDate);
  const std::int64_t endExclEpoch = time::toEpochSeconds(endExcl);

  std::vector<Candidate> candidates;
  candidates.reserve(maxCandidateCount(active, plan.monthStarts));

  for (const auto &monthAnchor : plan.monthStarts) {
    const std::int64_t monthEpoch = time::toEpochSeconds(monthAnchor);

    for (const auto &person : active) {
      for (const auto &sub : person.subs) {
        auto candidate = sampleCandidate(rng, monthEpoch, person, sub, cfg);
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

[[nodiscard]] transactions::Draft draftFrom(const Candidate &candidate,
                                            channels::Tag channel) noexcept {
  return transactions::Draft{
      .source = candidate.depositAcct,
      .destination = candidate.merchantAcct,
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
  v::ge("minPerPerson", minPerPerson, 0);
  v::ge("maxPerPerson", maxPerPerson, minPerPerson);
  v::between("debitP", debitP, 0.0, 1.0);
  v::ge("dayJitter", dayJitter, 0);
}

Generator::Generator(random::Rng &rng, const transactions::Factory &txf,
                     ledger::SeededScreen &screen, Config cfg)
    : rng_{rng}, txf_{txf}, screen_{screen}, cfg_{cfg} {
  cfg_.validate();
}

std::vector<transactions::Transaction>
Generator::generate(const blueprints::LegitBuildPlan &plan,
                    const entity::account::Registry &registry) {
  std::vector<transactions::Transaction> out;
  if (plan.monthStarts.empty()) {
    return out;
  }

  const auto billerAccts = merchantAccounts(plan);
  if (billerAccts.empty()) {
    return out;
  }

  const auto active = selectActivePeople(plan, registry, cfg_, billerAccts);
  if (active.empty()) {
    return out;
  }

  const auto candidates = makeCandidates(rng_, plan, active, cfg_);
  const auto channel = channels::tag(channels::Legit::subscription);

  out.reserve(candidates.size());
  for (const auto &candidate : candidates) {
    screen_.advanceThrough(candidate.timestamp, /*inclusive=*/true);

    if (!screen_.acceptTransfer(candidate.depositAcct, candidate.merchantAcct,
                                candidate.amount, channel,
                                candidate.timestamp)) {
      continue;
    }

    out.push_back(txf_.make(draftFrom(candidate, channel)));
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::subscriptions
