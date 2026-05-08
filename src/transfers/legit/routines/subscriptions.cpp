#include "phantomledger/transfers/legit/routines/subscriptions.hpp"

#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/channels/subscriptions/debits.hpp"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::subscriptions {
namespace {

namespace core = ::PhantomLedger::transfers::subscriptions;

[[nodiscard]] std::span<const entity::Key>
billerAccounts(const blueprints::LegitBlueprint &plan) noexcept {
  return std::span<const entity::Key>(
      plan.counterparties().billerAccounts.data(),
      plan.counterparties().billerAccounts.size());
}

[[nodiscard]] std::vector<core::SubscriberAccount>
subscriberAccounts(const blueprints::LegitBlueprint &plan,
                   const entity::account::Registry &registry) {
  std::vector<core::SubscriberAccount> out;
  out.reserve(plan.persons().size());

  for (const auto person : plan.persons()) {
    const auto it = plan.primaryAcctRecordIx().find(person);
    if (it == plan.primaryAcctRecordIx().end()) {
      continue;
    }
    const auto recordIx = it->second;
    if (recordIx >= registry.records.size()) {
      continue;
    }

    out.push_back(core::SubscriberAccount{
        .person = person,
        .deposit = registry.records[recordIx].id,
    });
  }

  return out;
}

[[nodiscard]] std::vector<core::Sub>
buildBundles(const blueprints::LegitBlueprint &plan,
             const entity::account::Registry &registry,
             const core::BundleRules &rules,
             std::span<const entity::Key> billers) {
  const auto subscribers = subscriberAccounts(plan, registry);
  if (subscribers.empty() || billers.empty()) {
    return {};
  }

  const random::RngFactory subFactory{plan.seed()};
  const core::BundleBuilder builder{rules, subFactory};
  return builder.build(
      core::SubscriberAccounts{std::span<const core::SubscriberAccount>(
          subscribers.data(), subscribers.size())},
      core::BillerDirectory{billers},
      core::AccountExclusions{.hubAccounts = &plan.counterparties().hubSet});
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
                           ledger::SeededScreen &screen)
    : rng_{rng}, txf_{txf}, screen_{screen} {
  bundleRules_.validate();
  scheduleRules_.validate();
}

DebitEmitter::DebitEmitter(random::Rng &rng, const transactions::Factory &txf,
                           ledger::SeededScreen &screen, BundleRules rules)
    : DebitEmitter(rng, txf, screen) {
  bundleRules(std::move(rules));
}

DebitEmitter &DebitEmitter::bundleRules(BundleRules rules) {
  rules.validate();
  bundleRules_ = rules;
  return *this;
}

DebitEmitter &DebitEmitter::scheduleRules(ScheduleRules rules) {
  rules.validate();
  scheduleRules_ = rules;
  return *this;
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

  const auto subs = buildBundles(plan, registry, bundleRules_, billers);
  if (subs.empty()) {
    return out;
  }

  const auto channel = channels::tag(channels::Legit::subscription);
  const core::ScheduleSampler schedule{
      std::span<const time::TimePoint>(plan.monthStarts().data(),
                                       plan.monthStarts().size()),
      plan.calendar().window(), scheduleRules_};

  out.reserve(schedule.monthStarts().size() * subs.size() / 2U);

  for (const auto &monthStart : schedule.monthStarts()) {
    const auto month = schedule.candidates(rng_, monthStart, subs);
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
