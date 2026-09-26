// tests/test_bank_ledger.cpp
//
// THE BANK-GL GATE (bank-gl-2026-09).
//
// The defect this exists for: card interest, card late fees, overdraft fees
// and overdraft line-of-credit interest paid three accounts typed as external
// business deposits (`Role::business` on `Bank::external`, registered
// external). At the 200,000-person corpus scale they took 2.8M payments, the
// card issuer's from 73% of card accounts. Every posting is kept; its contra
// is now the bank's income GL of its kind (`entity::gl`): internal, ownerless,
// never seeded, never a fraud or mule role, exported as its own account type.
//
// LEG A runs the run-golden gate world (pop 2,000, 60 days from 2025-01-01,
// seed 3405691582, family on) WITHOUT fraud:
//
//   A1. THE SHARED ENTITY STREAM. The next u64 after the build equals the
//       pre-round build's. Registration is draw-free, so a stage that started
//       drawing (or stopped) reds this. It also covers makeCatalog's draw
//       count.
//   A2. ONLY THE POSTING DESTINATION AND THE COPIED SESSION MOVED. The
//       order-free digest of every row, with the target masked on the four
//       posting kinds and the device and IP masked on the two liquidity
//       kinds, equals the pre-round build's, and so do the row count and the
//       per-kind posting counts. Fraud is off because the camouflage P2P pool
//       lost the three retired keys, which re-points ring cover transfers
//       and cascades through balances; with no camouflage the pool is never
//       read, so nothing may move. Paired with a domain predicate: every
//       posting row has a finite positive amount, an in-window timestamp, a
//       source of the right kind (a card for card postings, a deposit
//       account for liquidity postings) and the GL of its kind as target.
//
// LEG B runs the same world WITH fraud (the production shape):
//
//   B1. ROUTING. Every fee and interest row targets the GL of its kind, and
//       no other row names a GL as source or target. The retired keys are
//       not registered and no row names them.
//   B2. TYPING. Exactly four GL records, one contiguous block, internal,
//       ownerless, with no fraud, mule, victim or shell flag. Each opens at
//       0.00 with no overdraft, linked, courtesy or LOC buffer.
//   B3. NEVER A FRAUD OR MULE ROLE. No fraud-flagged row and no camouflage
//       row touches a GL (and the leg has both, so the check has data), and
//       the camouflage pool's own predicate rejects every GL while it admits
//       customer deposit accounts. The row count alone cannot see a pool
//       leak here: about 440 camouflage rows over a pool of thousands put
//       well under one expected row on four GLs. Added at review: the
//       predicate admits exactly the customer deposit accounts
//       (`Role::account`, the only destination legitimate P2P pays), and
//       every camouflage P2P row lands on one. The exclusion-list predicate
//       it replaced put 162 of 263 camouflage P2P rows on other people's
//       credit-card accounts.
//   B4. NO CUSTOMER SESSION ON A SYSTEM POSTING. No posting row carries a
//       device or an IP. The pre-round build copied the triggering row's
//       session onto every overdraft fee (346 rows at this configuration).
//   B5. BALANCE ACCOUNTING. The leg's rows replayed through the opening book
//       the way the post-fraud replay builds the posted book: each GL closes
//       at exactly the sum of the rows booked to it, finite and positive.
//
// PART C checks the exporter-facing typing that does not need a world: the
// `GL` rendering and its parse round trip, the AML account type, the
// channel-to-GL mapping, and that mule-ml never turns a posting's unassigned
// address (0.0.0.0) into an IP edge or a canonical IP. Without that guard,
// dropping the copied IP would link every overdrafting deposit account to one
// shared 0.0.0.0 vertex.

#include "gate_world.hpp"
#include "test_support.hpp"
#include "window_leg_support.hpp"

#include "phantomledger/encoding/external.hpp"
#include "phantomledger/encoding/parse.hpp"
#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/holdings/general_ledger.hpp"
#include "phantomledger/exporter/common/support.hpp"
#include "phantomledger/exporter/mule_ml/canonical.hpp"
#include "phantomledger/exporter/mule_ml/infra_edges.hpp"
#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;
namespace gl = pl::entity::gl;

using Txn = pl::transactions::Transaction;
using Key = pl::entity::Key;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

constexpr std::uint64_t kGoldenSeed = 3405691582ULL;

// A1/A2: measured on the pre-round build (the staged unknown-counterparty
// tree, exported with `git checkout-index` and built separately) and on this
// build; the two agree. The digest is the order-free sum of a per-row hash of
// every row of the fraud-free leg with the posting target and the liquidity
// session masked.
// RE-PINNED by counterparty-sizes-2026-09 (employer and landlord sizes). At
// this configuration the payroll roster went from 5 employers to 1,453, so
// the worker-weighted pay cadence went from those 5 draws (38% weekly, 62%
// biweekly, no semimonthly or monthly payer) to the configured law. Salary
// posting jitter draws on the shared stream per payday, so the stream after
// the income pass moves, and spending follows paydays: forcing the retired
// 38/62 cadence mix into this build restores 189,767 of the pre-round
// 190,402 fraud-free rows (a diagnostic, not shipped). With income off the
// stream is unmoved (test_counterparty_sizes sub-gate A pins it), so the
// income pass is the only source of the movement below.
// Pre-round values: stream 498e4bde6c6f83ea, digest cf3b75546c4c76da,
// 190,402 rows, postings 1,397 / 412 / 346 / 41.
//
// RE-PINNED by outlets-frequency-2026-09 (biller lane, chain outlets,
// category frequency). The shared stream pin holds, so no entity-stage
// value moved. Measured per step on this leg: pre-round 146,901 rows
// (506f29e98edda485); the biller lane alone 180,215 (06c21bed75515875);
// with outlets 163,281 (24aeedf8683530c4); with the category law, below.
// The step moves are NOT the mechanisms' size: January is byte-identical
// across the biller-lane step, and two re-randomizations of the biller lane
// alone (a diagnostic, not shipped) land at 145,906 and 180,226. The monthly
// commerce evolver spends a data-dependent number of draws on the session
// rng that then draws every day frame and population-dynamics multiplier,
// so this 60-day leg keeps January and lands February almost anywhere in
// that range whenever a biller or favourite set changes (registered in
// docs/fraud_model_audit.md).
// Values at that round's close: digest 0d570247c6b3bdb4 over 155,131 rows,
// postings 1,463 / 467 / 251 / 34.
//
// RE-PINNED by evolver-lanes-2026-09, which closes that coupling. The shared
// stream pin holds: both steps run inside the spending session. Per step on
// this leg: the monthly evolver on its own per-person, per-month lanes,
// 179,735 rows (d0e908064223de61; postings 1,423 / 470 / 286 / 34, January
// byte-identical); then the card lifecycle rows routed on their own per-card
// lanes instead of the session rng, below. Neither step changes a law. Each
// takes a data-dependent run of draws off the session rng, which then draws
// a different day-shock sequence (one Gamma(1.3) multiplier per day on every
// spender), and the row count follows the shock sum: in the run-golden
// binary, spending rows per unit of summed shock read 2,400 / 2,375 / 2,412
// before the round and after each step. test_merchant_churn sub-gate G now
// holds the session rng's position fixed under a biller and favourite
// stress, so a row move on this leg is a mechanism's size again.
constexpr std::uint64_t kSharedStreamNext = 0x9e0a89591a4d861fULL;
constexpr std::uint64_t kMaskedDigest = 0x7ff1cbfc35d35b98ULL;
constexpr std::size_t kRows = 174'319;
constexpr std::size_t kCardInterestRows = 1'495;
constexpr std::size_t kCardFeeRows = 459;
constexpr std::size_t kOverdraftFeeRows = 285;
constexpr std::size_t kLocInterestRows = 39;

// The three keys the postings paid before this round.
const Key kRetiredFeeCollection = pl::entity::makeKey(
    pl::entity::Role::business, pl::entity::Bank::external, 0xFFFF'FF01ULL);
const Key kRetiredOdLoc = pl::entity::makeKey(
    pl::entity::Role::business, pl::entity::Bank::external, 0xFFFF'FF02ULL);
const Key kRetiredCardIssuer = pl::entity::makeKey(
    pl::entity::Role::business, pl::entity::Bank::external, 3'000'000'001ULL);

[[nodiscard]] bool isRetired(Key key) {
  return key == kRetiredFeeCollection || key == kRetiredOdLoc ||
         key == kRetiredCardIssuer;
}

[[nodiscard]] std::uint64_t splitmix(std::uint64_t value) {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

[[nodiscard]] std::uint64_t keyWord(Key key) {
  return splitmix(key.number ^ (static_cast<std::uint64_t>(key.role) << 56U) ^
                  (static_cast<std::uint64_t>(key.bank) << 48U));
}

[[nodiscard]] std::uint64_t maskedRowHash(const Txn &row) {
  const bool maskTarget = gl::isPosting(row.session.channel);
  const bool maskSession = channels::isLiquidity(row.session.channel);
  std::uint64_t h = splitmix(keyWord(row.source));
  h = splitmix(h ^ (maskTarget ? 0x5A5A5A5AULL : keyWord(row.target)));
  h = splitmix(h ^ std::bit_cast<std::uint64_t>(row.amount));
  h = splitmix(h ^ static_cast<std::uint64_t>(row.timestamp));
  h = splitmix(h ^ row.session.channel.value);
  h = splitmix(h ^ row.fraud.flag);
  h = splitmix(h ^ static_cast<std::uint64_t>(row.fraud.type));
  if (!maskSession) {
    const auto &device = row.session.deviceId;
    h = splitmix(h ^ static_cast<std::uint64_t>(device.ownerType));
    h = splitmix(h ^ device.ownerId);
    h = splitmix(h ^ device.slot);
    h = splitmix(h ^ row.session.ipAddress.value);
  }
  return h;
}

[[nodiscard]] bool hasSession(const Txn &row) {
  return row.session.deviceId.assigned() || row.session.ipAddress.value != 0;
}

[[nodiscard]] const char *kindName(Key account) {
  if (account == gl::account(gl::Income::cardInterest)) {
    return "card interest income";
  }
  if (account == gl::account(gl::Income::cardFees)) {
    return "card fee income";
  }
  if (account == gl::account(gl::Income::depositFees)) {
    return "deposit fee income";
  }
  if (account == gl::account(gl::Income::creditLineInterest)) {
    return "credit-line interest income";
  }
  return "not a GL";
}

struct Leg {
  pltest::LegResult result;
  std::unique_ptr<pltest::GateWorld> world;
};

[[nodiscard]] Leg runWorld(const pl::synth::pii::PoolSet &poolSet,
                           bool withFraud) {
  pltest::LegOptions options{};
  options.population = 2'000;
  options.window =
      pl::time::Window{.start = pl::time::makeTime({2025, 1, 1}), .days = 60};
  options.seed = kGoldenSeed;
  options.withFamily = true;
  options.withFraud = withFraud;

  Leg leg;
  leg.result = pltest::runLeg(poolSet, options);

  // The same world runLeg built (GateWorld is deterministic for one spec),
  // rebuilt here because the leg tears its own down.
  pltest::WorldSpec spec;
  spec.seed = options.seed;
  spec.window = options.window;
  spec.population = options.population;
  spec.fraudProfile = pltest::scaledFraudProfile();
  leg.world = std::make_unique<pltest::GateWorld>(poolSet, spec);
  return leg;
}

// ------------------------------------------------------------------ LEG A

void runLegA(const pl::synth::pii::PoolSet &poolSet) {
  std::printf("\n##### LEG A: run-golden world, fraud off #####\n");
  const auto leg = runWorld(poolSet, /*withFraud=*/false);
  const auto &rows = leg.result.rows;
  const auto &world = *leg.world;
  const auto window = world.plan.calendar();
  const auto start = pl::time::toEpochSeconds(window.startDate);
  const auto end = start + static_cast<std::int64_t>(window.days) * 86'400;

  // A1.
  auto sharedStream = world.rng;
  const auto sharedNext = sharedStream.nextU64();
  auto freshStream = pl::random::Rng::fromSeed(kGoldenSeed);
  std::printf("  shared stream next u64 after the build %016llx (pinned "
              "%016llx)\n",
              static_cast<unsigned long long>(sharedNext),
              static_cast<unsigned long long>(kSharedStreamNext));
  check(sharedNext == kSharedStreamNext,
        "A1: the shared entity stream moved off the pre-round build; a stage "
        "drew from it that did not before (or stopped drawing)");
  check(sharedNext != freshStream.nextU64(),
        "A1: the build drew nothing from the shared stream, so the pin above "
        "would pass on no data");

  std::uint64_t digest = 0;
  std::map<Key, std::size_t> byGl;
  std::size_t outOfDomain = 0;
  for (const auto &row : rows) {
    digest += maskedRowHash(row);
    if (!gl::isPosting(row.session.channel)) {
      continue;
    }
    ++byGl[row.target];
    const bool cardKind =
        channels::is(row.session.channel, channels::Credit::interest) ||
        channels::is(row.session.channel, channels::Credit::lateFee);
    const bool sourceOk =
        row.source.bank == pl::entity::Bank::internal &&
        (cardKind ? row.source.role == pl::entity::Role::card
                  : (row.source.role == pl::entity::Role::account ||
                     row.source.role == pl::entity::Role::business));
    const bool sane = std::isfinite(row.amount) && row.amount > 0.0 &&
                      row.timestamp >= start && row.timestamp < end &&
                      sourceOk &&
                      row.target == gl::incomeAccountFor(row.session.channel);
    outOfDomain += sane ? 0U : 1U;
  }

  const auto count = [&](gl::Income kind) {
    const auto it = byGl.find(gl::account(kind));
    return it == byGl.end() ? std::size_t{0} : it->second;
  };
  std::printf("  rows %zu (pinned %zu), masked digest %016llx (pinned "
              "%016llx)\n",
              rows.size(), kRows, static_cast<unsigned long long>(digest),
              static_cast<unsigned long long>(kMaskedDigest));
  std::printf("  postings: card interest %zu (%zu), card fees %zu (%zu), "
              "overdraft fees %zu (%zu), LOC interest %zu (%zu)\n",
              count(gl::Income::cardInterest), kCardInterestRows,
              count(gl::Income::cardFees), kCardFeeRows,
              count(gl::Income::depositFees), kOverdraftFeeRows,
              count(gl::Income::creditLineInterest), kLocInterestRows);

  // A2.
  check(digest == kMaskedDigest && rows.size() == kRows,
        "A2: a row changed in something other than a posting's destination "
        "or a liquidity posting's session (digest or row count moved off the "
        "pre-round build)");
  check(count(gl::Income::cardInterest) == kCardInterestRows &&
            count(gl::Income::cardFees) == kCardFeeRows &&
            count(gl::Income::depositFees) == kOverdraftFeeRows &&
            count(gl::Income::creditLineInterest) == kLocInterestRows,
        "A2: a posting kind's row count moved off the pre-round build (or a "
        "posting landed off its GL)");
  check(outOfDomain == 0,
        "A2: " + std::to_string(outOfDomain) +
            " posting rows are outside their domain (finite positive amount, "
            "in-window timestamp, a card or deposit source by kind, the GL of "
            "their kind as target)");
}

// ------------------------------------------------------------------ LEG B

void runLegB(const pl::synth::pii::PoolSet &poolSet) {
  std::printf("\n##### LEG B: run-golden world, fraud on #####\n");
  const auto leg = runWorld(poolSet, /*withFraud=*/true);
  const auto &rows = leg.result.rows;
  const auto &world = *leg.world;
  const auto &lookup = world.holdings.accounts.lookup;
  const auto &records = world.holdings.accounts.registry.records;

  // B1: every endpoint is registered, and the retired keys are not.
  pl::pipeline::validateTransactionAccounts(lookup, rows);
  check(!lookup.byId.contains(kRetiredFeeCollection) &&
            !lookup.byId.contains(kRetiredOdLoc) &&
            !lookup.byId.contains(kRetiredCardIssuer),
        "B1: a retired fee, LOC or card-issuer key is still registered");

  std::size_t postings = 0;
  std::size_t offGl = 0;
  std::size_t strayGl = 0;
  std::size_t onRetired = 0;
  std::size_t withSession = 0;
  std::size_t liquidityPostings = 0;
  std::size_t fraudRows = 0;
  std::size_t camouflageRows = 0;
  std::size_t fraudOnGl = 0;
  std::size_t camouflageOnGl = 0;
  std::size_t camouflageP2p = 0;
  std::size_t camouflageP2pOffDeposit = 0;
  std::map<Key, std::size_t> rowsByGl;
  std::map<Key, std::unordered_set<Key>> payersByGl;

  for (const auto &row : rows) {
    const bool touchesGl =
        gl::isGeneralLedger(row.source) || gl::isGeneralLedger(row.target);
    onRetired += isRetired(row.source) || isRetired(row.target) ? 1U : 0U;
    if (row.fraud.flag != 0) {
      ++fraudRows;
      fraudOnGl += touchesGl ? 1U : 0U;
    }
    if (channels::isCamouflage(row.session.channel)) {
      ++camouflageRows;
      camouflageOnGl += touchesGl ? 1U : 0U;
    }
    if (channels::is(row.session.channel, channels::Camouflage::p2p)) {
      ++camouflageP2p;
      camouflageP2pOffDeposit +=
          row.target.role == pl::entity::Role::account ? 0U : 1U;
    }
    if (!gl::isPosting(row.session.channel)) {
      strayGl += touchesGl ? 1U : 0U;
      continue;
    }
    ++postings;
    offGl += row.target == gl::incomeAccountFor(row.session.channel) &&
                     !gl::isGeneralLedger(row.source)
                 ? 0U
                 : 1U;
    withSession += hasSession(row) ? 1U : 0U;
    liquidityPostings += channels::isLiquidity(row.session.channel) ? 1U : 0U;
    ++rowsByGl[row.target];
    payersByGl[row.target].insert(row.source);
  }

  // Card owners live in the card registry, deposit owners in the account
  // registry.
  const std::size_t cardAccounts = world.holdings.creditCards.records.size();
  std::size_t depositAccounts = 0;
  for (const auto &record : records) {
    if (record.owner == pl::entity::invalidPerson) {
      continue;
    }
    depositAccounts += record.id.role == pl::entity::Role::account ||
                               record.id.role == pl::entity::Role::business
                           ? 1U
                           : 0U;
  }
  std::printf("  rows %zu, postings %zu, fraud rows %zu, camouflage rows "
              "%zu\n",
              rows.size(), postings, fraudRows, camouflageRows);
  for (const auto account : gl::kIncomeAccounts) {
    const auto rendered = pl::encoding::format(account);
    const auto payers = payersByGl[account].size();
    const bool card = account == gl::account(gl::Income::cardInterest) ||
                      account == gl::account(gl::Income::cardFees);
    const auto base = card ? cardAccounts : depositAccounts;
    std::printf("    %s %-28s rows %6zu, payers %5zu (%.4f of %zu %s "
                "accounts)\n",
                std::string{rendered.view()}.c_str(), kindName(account),
                rowsByGl[account], payers,
                base == 0
                    ? 0.0
                    : static_cast<double>(payers) / static_cast<double>(base),
                base, card ? "card" : "deposit");
  }

  // B1.
  check(postings > 0 && rowsByGl.size() == gl::kIncomeAccounts.size(),
        "B1: a posting kind has no rows, so the routing check below would "
        "pass on no data");
  check(offGl == 0, "B1: " + std::to_string(offGl) +
                        " fee or interest rows do not target the GL of their "
                        "kind (or start at a GL)");
  check(strayGl == 0, "B1: " + std::to_string(strayGl) +
                          " rows that are not fee or interest postings touch "
                          "a GL");
  check(onRetired == 0, "B1: " + std::to_string(onRetired) +
                            " rows name a retired fee, LOC or issuer key");

  // B2.
  std::size_t first = records.size();
  std::size_t last = 0;
  std::size_t glRecords = 0;
  std::size_t badRecords = 0;
  for (std::size_t i = 0; i < records.size(); ++i) {
    if (!gl::isGeneralLedger(records[i].id)) {
      continue;
    }
    first = std::min(first, i);
    last = i;
    ++glRecords;
    const bool typed = records[i].id.bank == pl::entity::Bank::internal &&
                       records[i].owner == pl::entity::invalidPerson &&
                       records[i].flags == 0;
    badRecords += typed ? 0U : 1U;
  }
  std::printf("  GL records %zu from record %zu\n", glRecords, first);
  check(glRecords == gl::kIncomeAccounts.size() &&
            last - first + 1U == glRecords,
        "B2: the four GLs must be registered once each, as one contiguous "
        "registry block");
  check(badRecords == 0, "B2: " + std::to_string(badRecords) +
                             " GL records are external, owned or carry a "
                             "fraud, mule, victim or shell flag");

  auto opening = *world.initialBook;
  std::size_t badOpening = 0;
  for (const auto account : gl::kIncomeAccounts) {
    const auto idx = opening.findAccount(account);
    const bool clean =
        idx != pl::clearing::Ledger::invalid && opening.cash(idx) == 0.0 &&
        opening.overdraft(idx) == 0.0 && opening.linked(idx) == 0.0 &&
        opening.courtesy(idx) == 0.0 &&
        opening.protectionType(idx) == pl::clearing::ProtectionType::none;
    badOpening += clean ? 0U : 1U;
  }
  check(badOpening == 0, "B2: " + std::to_string(badOpening) +
                             " GLs have no opening-book slot, were seeded, or "
                             "carry a buffer");

  // B3.
  check(fraudRows > 0 && camouflageRows > 0,
        "B3: the leg has no fraud or no camouflage rows, so the checks below "
        "would pass on no data");
  check(fraudOnGl == 0,
        "B3: " + std::to_string(fraudOnGl) + " fraud-flagged rows touch a GL");
  check(camouflageOnGl == 0, "B3: " + std::to_string(camouflageOnGl) +
                                 " camouflage rows touch a GL");
  std::size_t eligibleGls = 0;
  for (const auto account : gl::kIncomeAccounts) {
    eligibleGls += pl::transfers::fraud::camouflageEligible(account) ? 1U : 0U;
  }
  std::size_t eligibleDeposits = 0;
  for (const auto &record : records) {
    if (record.owner != pl::entity::invalidPerson &&
        record.id.role == pl::entity::Role::account) {
      eligibleDeposits +=
          pl::transfers::fraud::camouflageEligible(record.id) ? 1U : 0U;
    }
  }
  check(eligibleGls == 0, "B3: " + std::to_string(eligibleGls) +
                              " GLs are in the camouflage pool");
  check(eligibleDeposits > 0,
        "B3: the camouflage pool admits no customer deposit account, so the "
        "GL exclusion above would pass on an empty pool");
  std::size_t poolMismatch = 0;
  for (const auto &record : records) {
    const bool deposit = record.id.role == pl::entity::Role::account;
    poolMismatch +=
        pl::transfers::fraud::camouflageEligible(record.id) == deposit ? 0U
                                                                       : 1U;
  }
  std::printf("  camouflage P2P rows %zu, off a customer deposit account "
              "%zu; pool predicate disagrees with Role::account on %zu of "
              "%zu records\n",
              camouflageP2p, camouflageP2pOffDeposit, poolMismatch,
              records.size());
  check(poolMismatch == 0,
        "B3: the camouflage pool predicate admits " +
            std::to_string(poolMismatch) +
            " registry records that are not customer deposit accounts (or "
            "rejects one that is)");
  check(camouflageP2p > 0,
        "B3: the leg has no camouflage P2P rows, so the destination check "
        "below would pass on no data");
  check(camouflageP2pOffDeposit == 0,
        "B3: " + std::to_string(camouflageP2pOffDeposit) +
            " camouflage P2P rows pay something other than a customer "
            "deposit account, a destination no legitimate P2P row pays");

  // B4.
  std::printf("  posting rows with a device or IP: %zu of %zu (liquidity "
              "postings %zu)\n",
              withSession, postings, liquidityPostings);
  check(liquidityPostings > 0,
        "B4: the leg has no liquidity postings, the only kind that ever "
        "copied a session, so the check below would pass on no data");
  check(withSession == 0, "B4: " + std::to_string(withSession) +
                              " fee or interest rows carry a device or IP");

  // B5: the posted-book construction (the post-fraud replay: the opening
  // book, liquidity emission off, the settled rows replayed in funds order).
  auto posted = *world.initialBook;
  auto replayRng = pl::random::Rng::fromSeed(kGoldenSeed ^ 0xB5ULL);
  pl::transfers::legit::ledger::ChronoReplayAccumulator accumulator(
      &posted, &replayRng,
      pl::transfers::legit::ledger::ChronoReplayAccumulator::
          defaultFundingBehavior(),
      /*emitLiquidityEvents=*/false);
  accumulator.extend(rows, /*presorted=*/false);
  const auto booked = accumulator.takeTxns();
  std::unordered_map<Key, double> bookedToGl;
  for (const auto &row : booked) {
    if (gl::isGeneralLedger(row.target)) {
      bookedToGl[row.target] += row.amount;
    }
  }
  std::size_t badBalance = 0;
  for (const auto account : gl::kIncomeAccounts) {
    const auto balance = posted.liquidity(account);
    const auto expected = bookedToGl[account];
    const bool ok = std::isfinite(balance) && balance > 0.0 &&
                    std::abs(balance - expected) <=
                        1e-9 * std::max(1.0, std::abs(expected));
    std::printf("    %-28s closes at %.2f, rows booked to it sum to %.2f\n",
                kindName(account), balance, expected);
    badBalance += ok ? 0U : 1U;
  }
  check(badBalance == 0,
        "B5: " + std::to_string(badBalance) +
            " GLs do not close at the sum of their booked rows (or are not "
            "finite and positive)");
}

// ------------------------------------------------------------------ PART C

void runPartC() {
  std::printf("\n##### PART C: typing without a world #####\n");
  std::unordered_set<Key> distinct;
  for (const auto account : gl::kIncomeAccounts) {
    distinct.insert(account);
    const auto rendered = pl::encoding::format(account);
    const auto view = rendered.view();
    const auto parsed = pl::encoding::parseKey(view);
    check(view.size() == 10 && view.substr(0, 2) == "GL",
          "C: a GL renders as " + std::string{view} + ", not GL + 8 digits");
    check(parsed.has_value() && *parsed == account,
          "C: " + std::string{view} + " does not parse back to its key");
    check(!pl::encoding::isExternal(account) && !pl::encoding::isExternal(view),
          "C: " + std::string{view} + " reads as external");
    check(pl::exporter::common::accountType(account, false) == "general_ledger",
          "C: the AML account type of " + std::string{view} +
              " is not general_ledger");
  }
  check(distinct.size() == 4, "C: the four GLs are not distinct");
  check(!pl::identifiers::allows(pl::entity::Role::ledger,
                                 pl::entity::Bank::external),
        "C: Role::ledger must be internal only");

  // The mapping covers exactly the four posting channels, one GL each.
  std::size_t mapped = 0;
  std::unordered_set<Key> targets;
  for (std::uint16_t value = 0; value < 256; ++value) {
    const channels::Tag tag{static_cast<std::uint8_t>(value)};
    const auto account = gl::incomeAccountFor(tag);
    check(pl::entity::valid(account) == gl::isPosting(tag),
          "C: incomeAccountFor and isPosting disagree on channel " +
              std::to_string(value));
    if (pl::entity::valid(account)) {
      ++mapped;
      targets.insert(account);
    }
  }
  check(mapped == 4 && targets.size() == 4,
        "C: the channel-to-GL mapping must send four channels to four GLs");

  // mule-ml: five fee rows with no session and one customer row with an IP,
  // all from one deposit account.
  namespace mule_ml = pl::exporter::mule_ml;
  const auto deposit = pl::entity::makeKey(pl::entity::Role::account,
                                           pl::entity::Bank::internal, 77);
  Txn fee{};
  fee.source = deposit;
  fee.target = gl::account(gl::Income::depositFees);
  fee.amount = 35.0;
  fee.timestamp = 1'700'000'000;
  fee.session.channel = channels::tag(channels::Liquidity::overdraftFee);
  Txn spend = fee;
  spend.target = pl::entity::makeKey(pl::entity::Role::merchant,
                                     pl::entity::Bank::external, 5);
  spend.session.channel = channels::tag(channels::Legit::merchant);
  spend.session.ipAddress = pl::network::Ipv4::pack(10, 1, 2, 3);

  mule_ml::detail::EdgeMap edges;
  mule_ml::CanonicalAccumulator canonical;
  for (int i = 0; i < 5; ++i) {
    mule_ml::detail::accumulateIpEdge(edges, fee);
    canonical.observe(fee);
  }
  check(edges.empty(), "C: mule-ml wrote an IP edge for a posting with no "
                       "session");
  mule_ml::detail::accumulateIpEdge(edges, spend);
  canonical.observe(spend);
  check(edges.size() == 1,
        "C: mule-ml must hold exactly one IP edge, the customer row's");
  const std::array<Key, 1> party{deposit};
  const auto resolved = canonical.resolve(party, {});
  check(resolved.at(deposit).ipAddress == "10.1.2.3",
        "C: mule-ml's canonical IP for an overdrafting account is " +
            resolved.at(deposit).ipAddress +
            ", not the customer's own address");
}

} // namespace

int main() {
  const auto poolSet = pltest::buildPoolSet(kGoldenSeed);
  runPartC();
  runLegA(poolSet);
  runLegB(poolSet);

  if (g_failures != 0) {
    std::printf("\ntest_bank_ledger: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("\ntest_bank_ledger: all checks passed\n");
  return 0;
}
