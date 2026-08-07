#include "phantomledger/transfers/legit/routines/paychecks.hpp"

#include "phantomledger/activity/spending/actors/instruments.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace PhantomLedger::transfers::legit::routines::paychecks {

namespace {

namespace instruments = ::PhantomLedger::activity::spending::actors;

/* MEASURED, not declared: the merchant channel's share of a deposit account's
 * total outflow, over the four seed/shape legs the endpoint gate runs —
 * 0.2442 / 0.2592 / 0.2343 / 0.2366, mean 0.2436.
 *
 * IT IS THE CONVERSION BETWEEN THE TWO LAWS, and it is why the forwarded
 * fraction cannot just be the spend-law share. `depositRoute` spreads only the
 * MERCHANT channel across the region; bills, external transfers, P2P,
 * subscriptions, ATM, insurance premiums, loan and tax obligations all still
 * leave the primary unconditionally. So a secondary carrying share `s` of the
 * spread bears `s * kMerchantShareOfDepositOutflow` of the person's total
 * outflow, and forwarding `s` of income outright would overshoot by ~4x and
 * make secondaries net-POSITIVE — the same defect with the sign flipped.
 *
 * REGISTERED as a level that will drift: it is a ratio between two channel
 * mixes, and any round that changes the bill/subscription/ATM load or the
 * merchant rate moves it. It is deliberately a single constant rather than a
 * per-person quantity, because the splitter runs BEFORE the spending fold and
 * cannot observe a person's realized mix without reading a book that does not
 * exist yet. */
inline constexpr double kMerchantShareOfDepositOutflow = 0.2436;

/* A floor and a ceiling on the forwarded share, so a degenerate weight ratio
 * cannot forward a person's whole paycheck or none of it. DECLARED. */
inline constexpr double kMinForwardFraction = 0.02;
inline constexpr double kMaxForwardFraction = 0.60;

/* Rotate the destination across the person's secondaries DRAW-FREE, keyed on
 * the paycheck's own timestamp. One row per paycheck either way, so a person
 * with three secondaries funds all three over their paydays while emitting
 * exactly as many rows as a person with one — which is what keeps this pass's
 * row count, and therefore its jitter-draw count, independent of the account
 * topology. Same discipline, and the same reason, as `pickInstrumentSlot`. */
[[nodiscard]] std::uint64_t rowMix(const entity::Key &primary,
                                   std::int64_t timestamp,
                                   std::uint64_t domain) noexcept {
  return instruments::instruments::splitmix(
      domain ^ static_cast<std::uint64_t>(timestamp) ^
      instruments::instruments::splitmix(
          static_cast<std::uint64_t>(primary.number)));
}

inline constexpr std::uint64_t kRotateDomain = 0x5350'4C49'5400'0001ULL;
inline constexpr std::uint64_t kJitterDomain = 0x5350'4C49'5400'0002ULL;

[[nodiscard]] std::size_t rotateSlot(const entity::Key &primary,
                                     std::int64_t timestamp,
                                     std::size_t count) noexcept {
  if (count <= 1) {
    return 0;
  }
  return static_cast<std::size_t>(rowMix(primary, timestamp, kRotateDomain) %
                                  count);
}

/* THE POSTING LAG IS DRAW-FREE, AND THAT IS A CORRECTION THAT COST AN ENGINE
 * DIVERGENCE TO FIND.
 *
 * It used to be `rng.uniformInt(5, 31)` per emitted row. A per-row draw makes
 * the value depend on the row's POSITION in the emission sequence, and the
 * monolithic engine sees the whole window's payday-inbound stream in one pass
 * while the windowed engine sees one window's worth at a time — so the two
 * assign different lags to the same paycheck and `test_arch_equivalence` reds
 * with a SEMANTIC divergence (measured: first differing row at index 54, a
 * `selfTransfer` the two engines timestamped differently).
 *
 * Relocating the draw to an isolated seed did not fix it and could not: any
 * sequential stream restarts at the top of every window. The fix is to have no
 * stream — a hash of (primary account, paycheck timestamp) is a pure function
 * of world state, so it is identical in a 60-day run and in the 20-year run
 * that contains it. That is the same prefix-identity property `instruments.hpp`
 * and `Router::liveIpFor` are built around, and CLAUDE.md `attacker-infra
 * -2026-07` rule 3 states it directly: draw-free is what confines the change,
 * stateless is what keeps the two engines in lockstep.
 *
 * The distribution is unchanged: a uniform integer in [5, 31). */
[[nodiscard]] std::int64_t postingLagMinutes(const entity::Key &primary,
                                             std::int64_t timestamp) noexcept {
  return 5 + static_cast<std::int64_t>(
                 rowMix(primary, timestamp, kJitterDomain) % 26U);
}

} // namespace

SplitsByPrimary planSplitters(const blueprints::LegitBlueprint &plan,
                              const entity::account::Ownership &ownership,
                              const entity::account::Registry &registry) {
  SplitsByPrimary splitsByPrimary;
  if (plan.persons().empty()) {
    return splitsByPrimary;
  }
  splitsByPrimary.reserve(plan.persons().size() / 2);

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

    /* THE REGION IS ROLE- AND BANK-FILTERED AND THE PRIMARY IS SLOT 0, which
     * is not a preference — it must be byte-identical to the region
     * `buildCensusScratch` hands the spending router, or income would be
     * forwarded to accounts the spread never reaches and withheld from ones
     * it does. A proprietor's `Role::business` account sits in the same
     * ownership slice and is deliberately excluded from both. */
    entity::Key primary{};
    std::vector<entity::Key> secondaries;

    for (auto idx = start; idx < end; ++idx) {
      const auto recordIx = ownership.byPersonIndex[idx];
      const auto &record = registry.records[recordIx];
      if (hubSet.contains(record.id)) {
        continue;
      }
      if (!entity::valid(primary)) {
        primary = record.id;
        continue;
      }
      if (record.id.role != primary.role || record.id.bank != primary.bank) {
        continue;
      }
      secondaries.push_back(record.id);
    }

    if (!entity::valid(primary) || secondaries.empty()) {
      continue;
    }

    /* THE FRACTION IS DERIVED, NOT DRAWN, AND IT IS DERIVED FROM THE SPEND
     * LAW ITSELF. `pickInstrumentSlot` weights the region with
     * `instruments::weightFor` — a rank hash of the account NUMBER — so the
     * share of merchant spend that leaves the non-primary slots is exactly
     * computable here, draw-free, from the same function the router uses.
     * Multiplying by the measured merchant share of outflow converts it into
     * a share of INCOME, which is what makes a secondary break even rather
     * than merely receive something.
     *
     * The old code drew this from a declared 0.10-0.35 band, which happened
     * to straddle break-even and told nobody why. Deriving it means the two
     * halves cannot drift apart when the Zipf exponent or the account-count
     * law moves. */
    const auto count = secondaries.size() + 1U;
    double total = 0.0;
    double primaryWeight = 0.0;
    {
      const auto personIndex = static_cast<std::uint32_t>(person - 1);
      primaryWeight = instruments::instruments::weightFor(
          personIndex, primary, count, instruments::kInstrumentZipfAlpha);
      total = primaryWeight;
      for (const auto &key : secondaries) {
        total += instruments::instruments::weightFor(
            personIndex, key, count, instruments::kInstrumentZipfAlpha);
      }
    }
    if (!(total > 0.0)) {
      continue;
    }

    const double spreadShare = 1.0 - (primaryWeight / total);
    double fraction = spreadShare * kMerchantShareOfDepositOutflow;
    fraction = std::clamp(fraction, kMinForwardFraction, kMaxForwardFraction);

    /* The owner's death instant, so the posting lag cannot carry a
     * self-transfer past it. See `Split::eligibleUntilExcl`. */
    auto eligibleUntil = std::numeric_limits<std::int64_t>::max();
    if (const auto *pack = plan.personas().pack; pack != nullptr) {
      const auto row = static_cast<std::size_t>(person) - 1U;
      if (row < pack->timelines.size()) {
        eligibleUntil = time::toEpochSeconds(pack->timelines[row].death);
      }
    }

    splitsByPrimary.emplace(
        primary, Split{std::move(secondaries), fraction, eligibleUntil});
  }

  return splitsByPrimary;
}

std::vector<transactions::Transaction>
emitSplitTransfers(const transactions::Factory &txf,
                   const SplitsByPrimary &splits,
                   std::span<const transactions::Transaction> existingTxns) {
  std::vector<transactions::Transaction> out;
  if (splits.empty() || existingTxns.empty()) {
    return out;
  }

  const auto selfTransferChannel = channels::tag(channels::Legit::selfTransfer);

  out.reserve(existingTxns.size() / 4);

  for (const auto &txn : existingTxns) {
    /* EVERY PAYDAY-INBOUND CHANNEL, NOT SALARY ALONE, and the widening is
     * what reaches the people the salary-only form could not.
     *
     * `TxnStreams` already collects this view with `isPaydayInbound`, so
     * re-narrowing it to `Legit::salary` here dropped benefits, pensions and
     * self-employment revenue. The people that excluded are exactly the ones
     * with no employer — retirees, students, business owners — and their
     * secondary accounts were the residual that survived widening coverage:
     * measured 14.5% / 17.1% / 16.3% / 16.7% of secondaries still receiving
     * NO credit at all, against 31.8-40.3% before. A non-salaried person's
     * deposit spread is no less real than a salaried one's.
     *
     * This raises the emitted row count, which raises the jitter-draw count
     * below — and that is now harmless precisely because both halves of this
     * pass moved to an isolated lane. On the shared routine stream it would
     * have been the `merchant-churn` rule 2 hazard again. */
    if (!channels::isPaydayInbound(txn.session.channel)) {
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

    /* ONE ROW PER PAYCHECK WHATEVER THE ACCOUNT COUNT, so the jitter draw
     * below fires exactly as often as it did before the pool replaced the
     * single secondary. The destination rotates instead. */
    const auto slot =
        rotateSlot(txn.target, txn.timestamp, it->second.secondaries.size());

    /* THE LAG MAY NOT CARRY THE TRANSFER PAST THE OWNER'S DEATH. The credit
     * itself is always inside the person's life — every income emitter stops
     * at `tl.death` — but 5-30 minutes later need not be, and a dead person
     * moving money between their own accounts is what `test_membership`
     * forbids outright. Dropped rather than clamped: a sweep that cannot
     * happen did not happen, and clamping would pile rows onto the death
     * instant. */
    const auto ts =
        txn.timestamp + postingLagMinutes(txn.target, txn.timestamp) * 60;
    if (ts >= it->second.eligibleUntilExcl) {
      continue;
    }

    out.push_back(txf.make(transactions::Draft{
        .source = txn.target,
        .destination = it->second.secondaries[slot],
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
