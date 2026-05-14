#include "phantomledger/transfers/legit/routines/paychecks.hpp"

#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

namespace PhantomLedger::transfers::legit::routines::paychecks {

SplitsByPrimary planSplitters(random::Rng &rng,
                              const blueprints::LegitBlueprint &plan,
                              const entity::account::Ownership &ownership,
                              const entity::account::Registry &registry) {
  SplitsByPrimary splitsByPrimary;
  if (plan.persons().empty()) {
    return splitsByPrimary;
  }
  splitsByPrimary.reserve(plan.persons().size() / 4);

  const auto &hubSet = plan.counterparties().hubSet;

  for (const auto person : plan.persons()) {
    if (person == entity::invalidPerson ||
        static_cast<std::size_t>(person) >= ownership.byPersonOffset.size()) {
      continue;
    }

    const auto start = ownership.byPersonOffset[person - 1];
    const auto end = ownership.byPersonOffset[person];
    if (end - start < 2) {
      continue;
    }

    entity::Key primary{};
    entity::Key secondary{};
    int found = 0;

    for (auto idx = start; idx < end && found < 2; ++idx) {
      const auto recordIx = ownership.byPersonIndex[idx];
      const auto &record = registry.records[recordIx];
      if (hubSet.contains(record.id)) {
        continue;
      }
      if (found == 0) {
        primary = record.id;
      } else {
        secondary = record.id;
      }
      ++found;
    }

    if (found < 2) {
      continue;
    }

    // 30% inclusion gate.
    if (!rng.coin(0.30)) {
      continue;
    }

    const double fraction = 0.10 + 0.25 * rng.nextDouble();
    splitsByPrimary.emplace(primary, Split{secondary, fraction});
  }

  return splitsByPrimary;
}

std::vector<transactions::Transaction>
emitSplitTransfers(random::Rng &rng, const transactions::Factory &txf,
                   const SplitsByPrimary &splits,
                   std::span<const transactions::Transaction> existingTxns) {
  std::vector<transactions::Transaction> out;
  if (splits.empty() || existingTxns.empty()) {
    return out;
  }

  const auto salaryChannel = channels::tag(channels::Legit::salary);
  const auto selfTransferChannel = channels::tag(channels::Legit::selfTransfer);

  out.reserve(existingTxns.size() / 4);

  for (const auto &txn : existingTxns) {
    if (txn.session.channel != salaryChannel) {
      continue;
    }

    const auto it = splits.find(txn.target);
    if (it == splits.end()) {
      continue;
    }

    const double splitAmt =
        primitives::utils::roundMoney(txn.amount * it->second.fraction);
    if (splitAmt < 10.0) {
      continue;
    }

    const auto delayMinutes = static_cast<std::int64_t>(rng.uniformInt(5, 31));
    const auto ts = txn.timestamp + delayMinutes * 60;

    out.push_back(txf.make(transactions::Draft{
        .source = txn.target,
        .destination = it->second.secondary,
        .amount = splitAmt,
        .timestamp = ts,
        .isFraud = 0,
        .ringId = -1,
        .channel = selfTransferChannel,
    }));
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::paychecks
