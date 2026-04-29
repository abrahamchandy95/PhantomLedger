#include "phantomledger/transfers/legit/routines/atm.hpp"

#include "phantomledger/entities/encoding/external.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace PhantomLedger::transfers::legit::routines::atm {

namespace {

inline constexpr std::array<double, 18> kAtmAmounts{
    20.0,  40.0,  40.0,  40.0,  60.0,  60.0,  60.0,  80.0,  80.0,
    100.0, 100.0, 100.0, 120.0, 140.0, 160.0, 200.0, 200.0, 300.0,
};

[[nodiscard]] bool canAffordAtm(clearing::Ledger *book,
                                const entity::Key &account, double amount) {
  if (book == nullptr) {
    return true;
  }
  const auto available = book->liquidity(account);
  const auto reserve = std::clamp(amount * 0.50, 40.0, 120.0);
  return available >= amount + reserve;
}

struct Candidate {
  std::int64_t timestamp = 0;
  entity::Key depositAcct{};
  double amount = 0.0;
};

} // namespace

void Config::validate() const {
  namespace v = primitives::validate;
  v::between("userP", userP, 0.0, 1.0);
  v::ge("withdrawalsPerMonthMin", withdrawalsPerMonthMin, 1);
  v::ge("withdrawalsPerMonthMax", withdrawalsPerMonthMax,
        withdrawalsPerMonthMin);
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
  if (plan.monthStarts.empty() || plan.counterparties.hubAccounts.empty()) {
    return out;
  }

  const auto atmNetworkAcct = plan.counterparties.hubAccounts.front();
  const auto endExcl = plan.startDate + time::Days{static_cast<int>(plan.days)};
  const auto channel = channels::tag(channels::Legit::atm);

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

  std::vector<entity::Key> activeUsers;
  activeUsers.reserve(plan.persons.size());

  for (const auto person : plan.persons) {
    const auto it = plan.primaryAcctRecordIx.find(person);
    if (it == plan.primaryAcctRecordIx.end()) {
      continue;
    }
    const auto &record = registry.records[it->second];
    if (plan.counterparties.hubSet.contains(record.id)) {
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

  if (activeUsers.empty()) {
    return out;
  }

  // ---- Candidate generation ----

  std::vector<Candidate> candidates;
  candidates.reserve(static_cast<std::size_t>(plan.monthStarts.size()) *
                     activeUsers.size() *
                     static_cast<std::size_t>(cfg.withdrawalsPerMonthMax));

  const std::int64_t startEpoch = time::toEpochSeconds(plan.startDate);
  const std::int64_t endExclEpoch = time::toEpochSeconds(endExcl);

  for (const auto &monthAnchor : plan.monthStarts) {
    const std::int64_t monthAnchorEpoch = time::toEpochSeconds(monthAnchor);

    for (const auto &depositAcct : activeUsers) {
      // Python: rng.int(min, max + 1) → inclusive.
      const auto nWithdrawals = static_cast<std::int32_t>(rng.uniformInt(
          cfg.withdrawalsPerMonthMin,
          static_cast<std::int64_t>(cfg.withdrawalsPerMonthMax) + 1));

      for (std::int32_t i = 0; i < nWithdrawals; ++i) {
        const double amount = kAtmAmounts[rng.choiceIndex(kAtmAmounts.size())];

        std::int32_t dayOffset;
        if (rng.nextDouble() < 0.75) {
          dayOffset = static_cast<std::int32_t>(rng.uniformInt(0, 19));
        } else {
          dayOffset = static_cast<std::int32_t>(rng.uniformInt(18, 28));
        }

        const auto hour = static_cast<std::int32_t>(rng.uniformInt(7, 24));
        const auto minute = static_cast<std::int32_t>(rng.uniformInt(0, 61));

        const std::int64_t ts = monthAnchorEpoch +
                                static_cast<std::int64_t>(dayOffset) * 86400 +
                                static_cast<std::int64_t>(hour) * 3600 +
                                static_cast<std::int64_t>(minute) * 60;

        if (ts < startEpoch || ts >= endExclEpoch) {
          continue;
        }

        candidates.push_back(Candidate{ts, depositAcct, amount});
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

    if (!canAffordAtm(book, cand.depositAcct, cand.amount)) {
      continue;
    }

    if (book != nullptr) {
      const auto srcIdx = book->findAccount(cand.depositAcct);
      const auto dstIdx = book->findAccount(atmNetworkAcct);
      const auto decision = book->transferAt(srcIdx, dstIdx, cand.amount,
                                             channel, cand.timestamp);
      if (!decision.accepted()) {
        continue;
      }
    }

    out.push_back(txf.make(transactions::Draft{
        .source = cand.depositAcct,
        .destination = atmNetworkAcct,
        .amount = cand.amount,
        .timestamp = cand.timestamp,
        .isFraud = 0,
        .ringId = -1,
        .channel = channel,
    }));
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::atm
