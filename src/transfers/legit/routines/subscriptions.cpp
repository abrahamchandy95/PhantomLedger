#include "phantomledger/transfers/legit/routines/subscriptions.hpp"

#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/channels/subscriptions/debits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
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

// H3 part 3c-ii + H1 DEFECT FIX (authority U-8 addendum): the U-6 CPI
// wiring for subscriptions had landed in the parallel channels emitter
// (transfers/channels/subscriptions/debits.cpp) — which production
// never calls — while THIS emitter (passes::addSubscriptions, the only
// production path) drafted and screened the raw calibration-dollar
// sub.amount. Same semantics as the channels emitter now: the kPricePool
// price realizes at the DEBIT month's CPI level; screen and draft share
// the nominal amount.
[[nodiscard]] double nominalAmount(const core::Sub &sub,
                                   std::int64_t timestamp) noexcept {
  const auto year =
      time::toCalendarDate(time::fromEpochSeconds(timestamp)).year;
  return primitives::utils::roundMoney(
      sub.amount * ::PhantomLedger::synth::econ::priceScale(year));
}

// H3 part 3c-ii: subscriptions are CONTRACTUAL — the estate keeps
// paying them until ACCOUNT CLOSURE (death + settlement). Keyed by the
// subscriber's primary deposit (the only account subs debit).
[[nodiscard]] std::unordered_map<entity::Key, std::int64_t>
closeEpochByAccount(const blueprints::LegitBlueprint &plan,
                    const entity::account::Registry &registry) {
  std::unordered_map<entity::Key, std::int64_t> out;

  const auto *pack = plan.personas().pack;
  if (pack == nullptr || pack->timelines.empty()) {
    return out; // the filter stands down
  }

  out.reserve(plan.primaryAcctRecordIx().size());
  for (const auto &[person, recordIx] : plan.primaryAcctRecordIx()) {
    if (person == 0 || person > pack->timelines.size() ||
        recordIx >= registry.records.size()) {
      continue;
    }
    out.emplace(
        registry.records[recordIx].id,
        time::toEpochSeconds(pack->timelines[person - 1].death) +
            static_cast<std::int64_t>(synth::pii::kSettlementDays) * 86'400);
  }

  return out;
}

[[nodiscard]] transactions::Draft draftFrom(const core::Sub &sub,
                                            double amount,
                                            std::int64_t timestamp,
                                            channels::Tag channel) noexcept {
  return transactions::Draft{
      .source = sub.deposit,
      .destination = sub.biller,
      .amount = amount,
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

  const auto closures = closeEpochByAccount(plan, registry);

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
      const auto &sub = subs[candidate.subIdx];

      // H3: skip closed accounts here — the month's cycle-timestamp
      // draws already burned inside candidates(), and this loop draws
      // nothing, so the shared rng stream is byte-identical.
      if (const auto it = closures.find(sub.deposit);
          it != closures.end() && candidate.ts >= it->second) {
        continue;
      }

      screen_.advanceThrough(candidate.ts, /*inclusive=*/true);

      const double amount = nominalAmount(sub, candidate.ts);
      if (!screen_.acceptTransfer(ledger::KeyedTransfer{
              .source = sub.deposit,
              .destination = sub.biller,
              .amount = amount,
              .channel = channel,
              .timestamp = candidate.ts,
          })) {
        continue;
      }

      out.push_back(txf_.make(draftFrom(sub, amount, candidate.ts, channel)));
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::subscriptions
