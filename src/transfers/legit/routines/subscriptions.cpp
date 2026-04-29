#include "phantomledger/transfers/legit/routines/subscriptions.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
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

} // namespace

void Config::validate() const {
  namespace v = primitives::validate;
  v::ge("minPerPerson", minPerPerson, 0);
  v::ge("maxPerPerson", maxPerPerson, minPerPerson);
  v::between("debitP", debitP, 0.0, 1.0);
  v::ge("dayJitter", dayJitter, 0);
}

std::vector<transactions::Transaction>
generate(random::Rng &rng, const blueprints::LegitBuildPlan &plan,
         const transactions::Factory &txf,
         const entity::account::Registry &registry, clearing::Ledger *book,
         std::span<const transactions::Transaction> baseTxns,
         bool baseTxnsSorted, Config cfg) {
  cfg.validate();

  std::vector<transactions::Transaction> out;
  if (plan.monthStarts.empty()) {
    return out;
  }

  const auto merchantAccts =
      std::span<const entity::Key>(plan.counterparties.billerAccounts.data(),
                                   plan.counterparties.billerAccounts.size());
  if (merchantAccts.empty()) {
    return out;
  }

  const auto endExcl = plan.startDate + time::Days{static_cast<int>(plan.days)};
  const auto channel = channels::tag(channels::Legit::subscription);
  const std::int64_t startEpoch = time::toEpochSeconds(plan.startDate);
  const std::int64_t endExclEpoch = time::toEpochSeconds(endExcl);

  // ---- Resolve seeded base txns (same shape as ATM / internal) ----
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

  // ---- Per-person sub roster ----
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

  if (active.empty()) {
    return out;
  }

  // ---- Candidate generation ----
  //

  std::vector<Candidate> candidates;
  std::size_t maxCandidates = 0;
  for (const auto &p : active) {
    maxCandidates += p.subs.size();
  }
  maxCandidates *= plan.monthStarts.size();
  candidates.reserve(maxCandidates);

  for (const auto &monthAnchor : plan.monthStarts) {
    const std::int64_t monthEpoch = time::toEpochSeconds(monthAnchor);

    for (const auto &person : active) {
      for (const auto &sub : person.subs) {
        std::int32_t day = std::min<std::int32_t>(sub.billingDay, 28);

        const auto jitter = static_cast<std::int32_t>(
            rng.uniformInt(-static_cast<std::int64_t>(cfg.dayJitter),
                           static_cast<std::int64_t>(cfg.dayJitter) + 1));
        day = std::clamp<std::int32_t>(day + jitter, 1, 28);

        const auto hour = static_cast<std::int32_t>(rng.uniformInt(0, 7));
        const auto minute = static_cast<std::int32_t>(rng.uniformInt(0, 61));

        const std::int64_t ts = monthEpoch +
                                static_cast<std::int64_t>(day - 1) * 86400 +
                                static_cast<std::int64_t>(hour) * 3600 +
                                static_cast<std::int64_t>(minute) * 60;

        if (ts < startEpoch || ts >= endExclEpoch) {
          continue;
        }

        candidates.push_back(Candidate{
            .timestamp = ts,
            .depositAcct = person.depositAcct,
            .merchantAcct = sub.merchantAcct,
            .amount = sub.amount,
        });
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

    if (book != nullptr) {
      const auto srcIdx = book->findAccount(cand.depositAcct);
      const auto dstIdx = book->findAccount(cand.merchantAcct);
      const auto decision = book->transferAt(srcIdx, dstIdx, cand.amount,
                                             channel, cand.timestamp);
      if (!decision.accepted()) {
        continue;
      }
    }

    out.push_back(txf.make(transactions::Draft{
        .source = cand.depositAcct,
        .destination = cand.merchantAcct,
        .amount = cand.amount,
        .timestamp = cand.timestamp,
        .isFraud = 0,
        .ringId = -1,
        .channel = channel,
    }));
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::subscriptions
