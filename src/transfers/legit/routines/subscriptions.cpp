#include "phantomledger/transfers/legit/routines/subscriptions.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/subscriptions/schedule.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace PhantomLedger::transfers::legit::routines::subscriptions {

namespace {

namespace core = ::PhantomLedger::transfers::subscriptions;

struct Candidate {
  std::int64_t ts = 0;
  std::uint32_t subIdx = 0;
};

[[nodiscard]] std::span<const entity::Key>
billerAccounts(const blueprints::LegitBlueprint &plan) noexcept {
  return std::span<const entity::Key>(
      plan.counterparties().billerAccounts.data(),
      plan.counterparties().billerAccounts.size());
}

[[nodiscard]] std::vector<core::Sub>
buildSubscriptionBundles(const blueprints::LegitBlueprint &plan,
                         const entity::account::Registry &registry,
                         const core::BundleRules &rules,
                         std::span<const entity::Key> billers) {
  std::vector<core::Sub> subs;
  if (billers.empty()) {
    return subs;
  }

  const random::RngFactory subFactory{plan.seed()};
  subs.reserve(plan.persons().size() *
               static_cast<std::size_t>(rules.maxPerPerson) / 2U);

  for (const auto person : plan.persons()) {
    const auto it = plan.primaryAcctRecordIx().find(person);
    if (it == plan.primaryAcctRecordIx().end()) {
      continue;
    }
    const auto &record = registry.records[it->second];
    if (plan.counterparties().hubSet.contains(record.id)) {
      continue;
    }

    const auto personStr = std::to_string(static_cast<std::uint64_t>(person));
    auto subRng =
        subFactory.rng({"subscriptions", std::string_view(personStr)});

    core::appendBundle(subRng, rules, record.id, billers, subs);
  }

  return subs;
}

[[nodiscard]] std::vector<Candidate> buildMonthCandidates(
    random::Rng &rng, time::TimePoint monthStart,
    std::span<const core::Sub> subs, std::int64_t windowStartEpoch,
    std::int64_t windowEndExclEpoch, const core::BundleRules &rules) {
  std::vector<Candidate> month;
  month.reserve(subs.size());

  for (std::size_t i = 0; i < subs.size(); ++i) {
    const auto ts =
        core::cycleTimestamp(rng, monthStart, subs[i].day,
                             static_cast<std::uint8_t>(rules.dayJitter));
    if (ts < windowStartEpoch || ts >= windowEndExclEpoch) {
      continue;
    }
    month.push_back(Candidate{
        .ts = ts,
        .subIdx = static_cast<std::uint32_t>(i),
    });
  }

  // Stable sort keeps the screen-replay deterministic for ties.
  std::stable_sort(month.begin(), month.end(),
                   [](const Candidate &a, const Candidate &b) noexcept {
                     return a.ts < b.ts;
                   });

  return month;
}

[[nodiscard]] transactions::Draft draftFrom(const core::Sub &sub,
                                            std::int64_t timestamp,
                                            channels::Tag channel) noexcept {
  return transactions::Draft{
      .source = sub.deposit,
      .destination = sub.biller,
      .amount = sub.amount,
      .timestamp = timestamp,
      .isFraud = 0,
      .ringId = -1,
      .channel = channel,
  };
}

} // namespace

DebitEmitter::DebitEmitter(random::Rng &rng, const transactions::Factory &txf,
                           ledger::SeededScreen &screen, BundleRules rules)
    : rng_{rng}, txf_{txf}, screen_{screen}, rules_{rules} {
  rules_.validate();
}

std::vector<transactions::Transaction>
DebitEmitter::emitDebits(const blueprints::LegitBlueprint &plan,
                         const entity::account::Registry &registry) {
  std::vector<transactions::Transaction> out;
  if (plan.monthStarts().empty()) {
    return out;
  }

  const auto billers = billerAccounts(plan);
  if (billers.empty()) {
    return out;
  }

  const auto subs = buildSubscriptionBundles(plan, registry, rules_, billers);
  if (subs.empty()) {
    return out;
  }

  const auto channel = channels::tag(channels::Legit::subscription);

  const auto endExcl =
      plan.startDate() + time::Days{static_cast<int>(plan.days())};
  const auto windowStartEpoch = time::toEpochSeconds(plan.startDate());
  const auto windowEndExclEpoch = time::toEpochSeconds(endExcl);

  out.reserve(plan.monthStarts().size() * subs.size() / 2U);

  for (const auto &monthStart : plan.monthStarts()) {
    const auto month = buildMonthCandidates(
        rng_, monthStart, subs, windowStartEpoch, windowEndExclEpoch, rules_);
    if (month.empty()) {
      continue;
    }

    for (const auto &candidate : month) {
      screen_.advanceThrough(candidate.ts, /*inclusive=*/true);

      const auto &sub = subs[candidate.subIdx];
      if (!screen_.acceptTransfer(sub.deposit, sub.biller, sub.amount, channel,
                                  candidate.ts)) {
        continue;
      }

      out.push_back(txf_.make(draftFrom(sub, candidate.ts, channel)));
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::subscriptions
