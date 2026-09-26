// tests/test_remote_payees.cpp
//
// THE RETIRED-CATCH-ALL GATE (unknown-counterparty-2026-09).
//
// The defect this exists for: the spending router's external-unknown slot,
// every P2P payment with no usable contact, and every funeral paid ONE
// account. At the 200,000-person corpus scale it took 3.8M payments from 62%
// of deposit accounts, the largest artifact hub in the graph. Each flow now
// pays the counterparty a bank records for it: a paid check is keyed by the
// payee's bank (FDIC SOD shares), the rest of the slot pays identified remote
// merchants, a no-contact P2P payment goes to the person's P2P platform, and
// a funeral pays a funeral home in the decedent's city.
//
// LEG B runs the gate world at the run-golden configuration (pop 2,000, 60
// days, seed 3405691582, family on):
//
//   B1. THE SHARED ENTITY STREAM. The next u64 after the build equals the
//       pre-round build's. Registration is draw-free, so a stage that started
//       drawing (or stopped) reds this. It also covers makeCatalog's draw
//       count.
//   B2. ONLY THE DESTINATION MOVED. The order-free digest of every legit row,
//       with the target masked on the retired flows, equals the pre-round
//       build's, and so do the legit and retired row counts (re-pinned twice
//       by bank-gl-2026-09; see the constants). Paired with a
//       domain predicate: every retired row has a finite positive amount, an
//       in-window timestamp, an internal source and a destination in one of
//       the four families.
//   B3. NOTHING ON THE RETIRED KEY. No row names it, it is not registered,
//       and every endpoint is registered.
//   B4. REGISTRATION AND CAMOUFLAGE. The new accounts are external, ownerless
//       and one contiguous block; no camouflage row pays one.
//
// LEG C runs pop 10,000 over 365 days, where the shares can fail:
//
//   C1. THE CHECK SHARE of the slot sits within 4 sigma of the DCPC-derived
//       fraction for the year.
//   C2. THE BANK TABLE. The share of person payee slots at the largest bank,
//       and at the top 10, sits within 4 sigma of the SOD table.
//   C3. THE PLATFORM SPLIT. The share of people on Venmo sits within 4
//       sigma of the monthly-actives split, and a no-contact sender pays only
//       their own platform. (The fallback itself does not fire at these
//       configurations: every person holds at least three contacts.)
//   C4. REMOTE MERCHANTS are external online or national-service catalogue
//       outlets, live when the row was emitted. The pick reads exact
//       liveness at emission, and an unfunded deposit debit is re-presented
//       later (up to two retries, `ReplayFundingBehavior`), so a posted row
//       may trail a closure by at most that retry horizon and no more.
//   C5. FUNERALS pay the funeral home of the decedent's own city at the death
//       date, and no home takes more than a handful.
//   C6. THE ANTI-HUB BOUNDS, on rows and on degree. No single account takes
//       more than 15% of the retired rows. No check-payee bank or funeral
//       home is paid by a larger share of the retired rows' payers than the
//       share whose payee slots include the largest bank,
//       1 - (1 - 0.1221)^4 = 0.406, plus 4 sigma.
//       Named counterparties (remote merchants, P2P platforms) may be hubs:
//       their degree is printed, not bounded. DISARMS, scored on the same
//       rows: one catch-all per flow must fail both ceilings (the pre-round
//       single key scores 1.0 and cannot show either is tight), and a fresh
//       payee bank per check must fail the degree ceiling.
//   C7. STICKINESS. A check writer's banks never exceed their payee count.

#include "gate_world.hpp"
#include "test_support.hpp"
#include "window_leg_support.hpp"

#include "phantomledger/entities/counterparties/institutional_accounts.hpp"
#include "phantomledger/entities/counterparties/remote_payees.hpp"
#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/counterparties/remote_payees.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;
namespace cps = pl::counterparties;
namespace keys = pl::counterparties::remote;
namespace remote = pl::synth::counterparties::remote;
namespace channels = pl::channels;

using Txn = pl::transactions::Transaction;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

constexpr std::uint64_t kGoldenSeed = 3405691582ULL;

// 4 sigma of a binomial share: a false red about once in 16,000 checks.
constexpr double kSigmas = 4.0;

// C6, rows. The largest destination a correct build can produce, in rows, is
// the largest payee bank. In a window whose every slot row is a check
// (through 2020) that is its SOD share, 0.1221 of the slot; this leg (2025)
// expects 0.6 x 0.1221 = 0.073. The ceiling leaves sampling room above the
// full-check era and still rejects the one-catch-all-per-flow disarm (about
// 0.60).
constexpr double kMaxAccountShare = 0.15;

// C6, degree: the share of payers whose payee slots include the largest bank.
// A person pays a check hub only through their own payees, so this bounds the
// hub's degree on any window. The gate adds 4 sigma over the payers.
[[nodiscard]] double checkHubReach() {
  return 1.0 - std::pow(1.0 - remote::checkBankShare(1),
                        static_cast<double>(remote::kCheckPayeesPerPerson));
}

[[nodiscard]] const char *kindOf(pl::entity::Key account) {
  return keys::isP2pPlatform(account)   ? "P2P platform"
         : keys::isCheckBank(account)   ? "check-payee bank"
         : keys::isFuneralHome(account) ? "funeral home"
                                        : "remote merchant";
}

// B1/B2: measured on the pre-round build (the staged institutional-providers
// tree) and on this build; the two agreed (legit digest 255b9e814eea12cc over
// 190,275 legit rows). The digest is the order-free sum of a per-row hash of
// every legit row (flag 0, not camouflage) with the target masked on the
// retired flows.
//
// RE-PINNED ONCE by bank-gl-2026-09. That round masks its own two fields
// (see rowHash), and its camouflage pool lost the three retired bank keys,
// which re-points ring cover transfers and moves one legit row here. Under
// the extended mask the bank-gl pre-round build scores 815871576f03eef2 over
// 190,275 legit rows, and so does the bank-gl build with the pre-round
// camouflage pool restored (a diagnostic, not shipped), so nothing else
// moved. test_bank_ledger A2 carries that round's own pre-round proof on a
// fraud-free leg, where the camouflage pool is never read.
//
// RE-PINNED AGAIN by that round's review fix, which restricts the camouflage
// P2P pool to customer deposit accounts (dbfdd62a2aca2846 over 190,276 legit
// rows and 7,902 retired rows, to the values below). The fix changes only the
// pool predicate and deletes a merchant re-pick loop that can no longer fire;
// the fix scores the same with and without the loop, so the movement is the
// camouflage cascade alone.
//
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
// Pre-round values: stream 498e4bde6c6f83ea, digest 75d7339bc4b70b82 over
// 190,273 legit rows and 7,900 retired rows.
//
// RE-PINNED AGAIN by that round's review fix, which pays camouflage salary
// on the picked employer's own schedule instead of one drawn per mule on
// the camo lane. The shared stream is unmoved; the camouflage salary rows
// re-date, ring balances with them, and three legit rows and one retired
// row move in post-fraud settlement. The same build with the per-mule
// schedule restored (a diagnostic, not shipped) scores the previous pin
// exactly (5e32d7ee4bb8dd8a over 146,844 legit rows and 5,862 retired
// rows), so the camouflage cascade is the only source.
//
// RE-PINNED by outlets-frequency-2026-09 (biller lane, chain outlets,
// category frequency); the shared stream pin holds. Per step: pre-round
// 146,841 legit / 5,861 retired rows (492b1f81066db544); the biller lane
// alone 180,141 / 7,385 (b4fcbbc007531919); with outlets 163,217 / 6,512
// (84ef921df3a75660); with the category law 155,065 / 6,117
// (7e66a3922573fb61); with the fraud venue pool on the same category law,
// below. The size of each step is the monthly evolver's rng coupling, not
// the mechanism: see the matching note in test_bank_ledger. The last step
// moves no row count: the only legitimate rows it touches are the 109
// chargeback credits, whose source is the fraud venue (measured: without
// them the legit digest is identical with and without the venue factor).
// Values at that round's close: 530f92353c0e0cb7 over 155,065 legit rows and
// 6,117 retired rows.
//
// RE-PINNED by evolver-lanes-2026-09, which closes the evolver's rng
// coupling; the shared stream pin holds. Per step: the monthly evolver on its
// own lanes, 179,654 / 7,340 (d2c16e608a7a91f0); then the card lifecycle rows
// routed on their own per-card lanes, below. Both steps only change the
// session rng's day-shock sequence: see the matching note in
// test_bank_ledger.
constexpr std::uint64_t kSharedStreamNext = 0x9e0a89591a4d861fULL;
constexpr std::uint64_t kMaskedLegitDigest = 0x60d0fb24bcc79c28ULL;
constexpr std::size_t kLegitRows = 174'278;
constexpr std::size_t kRetiredRows = 7'195;

[[nodiscard]] std::uint64_t splitmix(std::uint64_t value) {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

[[nodiscard]] std::uint64_t keyWord(pl::entity::Key key) {
  return splitmix(key.number ^ (static_cast<std::uint64_t>(key.role) << 56U) ^
                  (static_cast<std::uint64_t>(key.bank) << 48U));
}

// The bank-gl round moved two fields of the bank's fee and interest
// postings: the destination (now the income GL of the posting kind) and, on
// the two liquidity kinds, the customer IP the fee row used to copy (now
// none). Both are masked, so the pin still scopes to what this round moved.
[[nodiscard]] bool isBankPosting(channels::Tag c) {
  return channels::is(c, channels::Credit::interest) ||
         channels::is(c, channels::Credit::lateFee) ||
         channels::isLiquidity(c);
}

[[nodiscard]] std::uint64_t rowHash(const Txn &row, bool maskTarget) {
  const bool posting = isBankPosting(row.session.channel);
  const bool maskIp = channels::isLiquidity(row.session.channel);
  std::uint64_t h = splitmix(keyWord(row.source));
  h = splitmix(h ^ (maskTarget || posting ? 0x5A5A5A5AULL
                                          : keyWord(row.target)));
  h = splitmix(h ^ std::bit_cast<std::uint64_t>(row.amount));
  h = splitmix(h ^ static_cast<std::uint64_t>(row.timestamp));
  h = splitmix(h ^ row.session.channel.value);
  h = splitmix(h ^ row.fraud.flag);
  h = splitmix(h ^ static_cast<std::uint64_t>(row.fraud.type));
  h = splitmix(h ^ (maskIp ? 0U : row.session.ipAddress.value));
  return h;
}

const auto kExternalTag = channels::tag(channels::Legit::externalUnknown).value;
const auto kBillTag = channels::tag(channels::Legit::bill).value;

[[nodiscard]] bool isLegit(const Txn &row) {
  return row.fraud.flag == 0 && !channels::isCamouflage(row.session.channel);
}

// A row of a retired flow: the legit external-unknown channel (the slot and
// the no-contact P2P fallback) or a funeral.
[[nodiscard]] bool isRetiredFlow(const Txn &row) {
  if (row.fraud.flag != 0) {
    return false;
  }
  return row.session.channel.value == kExternalTag ||
         (row.session.channel.value == kBillTag &&
          keys::isFuneralHome(row.target));
}

[[nodiscard]] bool withinSigmas(double observed, double expected,
                                double trials) {
  const double sigma = std::sqrt(expected * (1.0 - expected) / trials);
  return std::abs(observed - expected) <= kSigmas * sigma;
}

struct Leg {
  pltest::LegResult result;
  std::unique_ptr<pltest::GateWorld> world;
};

[[nodiscard]] Leg runWorld(const pl::synth::pii::PoolSet &poolSet,
                           std::int32_t population, int days) {
  pltest::LegOptions options{};
  options.population = population;
  options.window =
      pl::time::Window{.start = pl::time::makeTime({2025, 1, 1}), .days = days};
  options.seed = kGoldenSeed;
  options.withFamily = true;

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

// ------------------------------------------------------------------ LEG B

void runLegB(const pl::synth::pii::PoolSet &poolSet) {
  std::printf("\n##### LEG B: gate world at the run-golden config #####\n");
  const auto leg = runWorld(poolSet, 2'000, 60);
  const auto &rows = leg.result.rows;
  const auto &world = *leg.world;
  const auto window = world.plan.calendar();
  const auto start = pl::time::toEpochSeconds(window.startDate);
  const auto end = start + static_cast<std::int64_t>(window.days) * 86'400;

  // B1.
  auto sharedStream = world.rng;
  const auto sharedNext = sharedStream.nextU64();
  auto freshStream = pl::random::Rng::fromSeed(kGoldenSeed);
  std::printf("  shared stream next u64 after the build %016llx (pinned "
              "%016llx)\n",
              static_cast<unsigned long long>(sharedNext),
              static_cast<unsigned long long>(kSharedStreamNext));
  check(sharedNext == kSharedStreamNext,
        "B1: the shared entity stream moved off the pre-round build; a stage "
        "drew from it that did not before (or stopped drawing)");
  check(sharedNext != freshStream.nextU64(),
        "B1: the build drew nothing from the shared stream, so the pin above "
        "would pass on no data");

  // B3: every endpoint is registered, and the retired key is not.
  pl::pipeline::validateTransactionAccounts(world.holdings.accounts.lookup,
                                            rows);
  const auto retired = cps::retiredExternalUnknown();
  check(!world.holdings.accounts.lookup.byId.contains(retired),
        "B3: the retired catch-all is still registered");

  const auto &catalog = world.cps.merchants;
  std::unordered_map<pl::entity::Key, std::uint32_t> catalogIndex;
  for (std::uint32_t i = 0; i < catalog.records.size(); ++i) {
    catalogIndex.emplace(catalog.records[i].counterpartyId, i);
  }

  std::uint64_t digest = 0;
  std::size_t legitRows = 0;
  std::size_t retiredRows = 0;
  std::size_t outOfDomain = 0;
  std::size_t onRetiredKey = 0;
  std::size_t camouflageOnRemote = 0;
  std::size_t camouflageRows = 0;
  std::map<std::string, std::size_t> byFamily;

  for (const auto &row : rows) {
    onRetiredKey += row.source == retired || row.target == retired ? 1U : 0U;
    if (channels::isCamouflage(row.session.channel)) {
      ++camouflageRows;
      camouflageOnRemote += keys::isRemotePayee(row.target) ? 1U : 0U;
    }
    const bool retiredFlow = isRetiredFlow(row);
    if (isLegit(row)) {
      ++legitRows;
      digest += rowHash(row, retiredFlow);
    }
    if (!retiredFlow) {
      continue;
    }
    ++retiredRows;

    std::string family = "outside every family";
    if (keys::isCheckBank(row.target)) {
      family = "check-payee bank";
    } else if (keys::isP2pPlatform(row.target)) {
      family = "P2P platform";
    } else if (keys::isFuneralHome(row.target)) {
      family = "funeral home";
    } else if (const auto it = catalogIndex.find(row.target);
               it != catalogIndex.end() &&
               remote::RemoteMerchantTable::eligible(
                   catalog.records[it->second]) &&
               catalog.records[it->second].liveAt(row.timestamp)) {
      family = "remote merchant";
    }
    ++byFamily[family];

    const bool sane = std::isfinite(row.amount) && row.amount > 0.0 &&
                      row.timestamp >= start && row.timestamp < end &&
                      row.source.bank == pl::entity::Bank::internal &&
                      row.target.bank == pl::entity::Bank::external &&
                      family != "outside every family";
    outOfDomain += sane ? 0U : 1U;
  }

  std::printf("  rows %zu, legit %zu (pinned %zu), retired flows %zu (pinned "
              "%zu), masked legit digest %016llx (pinned %016llx)\n",
              rows.size(), legitRows, kLegitRows, retiredRows, kRetiredRows,
              static_cast<unsigned long long>(digest),
              static_cast<unsigned long long>(kMaskedLegitDigest));
  for (const auto &[family, count] : byFamily) {
    std::printf("    %-18s %zu\n", family.c_str(), count);
  }

  // B2.
  check(digest == kMaskedLegitDigest && legitRows == kLegitRows &&
            retiredRows == kRetiredRows,
        "B2: a legit row changed in something other than a retired row's "
        "destination (digest, legit count or retired count moved off the "
        "pre-round build)");
  // The P2P platform family is not required: every person holds at least
  // three in-population contacts (Social::effectiveDegree), so the
  // no-contact fallback does not fire at this configuration. C3 checks the
  // platform choice directly.
  check(retiredRows > 0 && byFamily["check-payee bank"] > 0 &&
            byFamily["remote merchant"] > 0 && byFamily["funeral home"] > 0,
        "B2: a destination family is empty, so a domain check below would "
        "pass on no data");
  check(outOfDomain == 0,
        "B2: " + std::to_string(outOfDomain) +
            " retired rows are outside their domain (finite positive amount, "
            "in-window timestamp, internal source, external destination in "
            "one of the four families)");

  // B3.
  check(onRetiredKey == 0, "B3: " + std::to_string(onRetiredKey) +
                               " rows name the retired catch-all");

  // B4.
  const auto &records = world.holdings.accounts.registry.records;
  std::size_t first = records.size();
  std::size_t last = 0;
  std::size_t remoteRecords = 0;
  std::size_t badRecords = 0;
  std::size_t bankRecords = 0;
  std::size_t homeRecords = 0;
  for (std::size_t i = 0; i < records.size(); ++i) {
    if (!keys::isRemotePayee(records[i].id)) {
      continue;
    }
    first = std::min(first, i);
    last = i;
    ++remoteRecords;
    bankRecords += keys::isCheckBank(records[i].id) ? 1U : 0U;
    homeRecords += keys::isFuneralHome(records[i].id) ? 1U : 0U;
    const bool external = pl::entity::account::hasFlag(
        records[i].flags, pl::entity::account::Flag::external);
    badRecords +=
        external && records[i].owner == pl::entity::invalidPerson ? 0U : 1U;
  }
  std::printf("  remote payees registered %zu from record %zu (banks %zu, "
              "platforms %zu, funeral homes %zu); camouflage rows %zu (on a "
              "remote payee %zu)\n",
              remoteRecords, first, bankRecords,
              remoteRecords - bankRecords - homeRecords, homeRecords,
              camouflageRows, camouflageOnRemote);
  check(remoteRecords > 0 && last - first + 1U == remoteRecords,
        "B4: the remote payees must be one contiguous registry block");
  check(badRecords == 0, "B4: " + std::to_string(badRecords) +
                             " remote payees are internal or owned");
  check(camouflageRows > 0 && camouflageOnRemote == 0,
        "B4: " + std::to_string(camouflageOnRemote) +
            " camouflage rows land on a check bank, platform or funeral home "
            "(or the leg has no camouflage rows)");
}

// The latest a posted row can trail its emission: every retry pass the
// funding replay allows, each at the longer of its cure window (plus padding)
// and its blind delay. Read from the production defaults, so a change there
// moves this bound with it.
[[nodiscard]] std::int64_t retryHorizonSeconds(pl::channels::Tag channel) {
  const pl::transfers::legit::ledger::ReplayFundingBehavior funding{};
  std::int64_t horizon = 0;
  for (std::int32_t pass = 0; pass < funding.maxAttemptsFor(channel); ++pass) {
    const std::int64_t cure =
        static_cast<std::int64_t>(funding.cureHoursFor(channel)) * 3600 +
        static_cast<std::int64_t>(funding.paddingMinutesFor(channel)) * 60;
    const std::int64_t blind =
        static_cast<std::int64_t>(funding.blindDelayHoursFor(pass)) * 3600;
    horizon += std::max(cure, blind);
  }
  return horizon;
}

// Live when the row was EMITTED: live at the posted timestamp, or opened
// before it and closed no earlier than one retry horizon before it.
[[nodiscard]] bool liveAtEmission(const pl::entity::merchant::Record &record,
                                  const pl::transactions::Transaction &row) {
  if (record.liveAt(row.timestamp)) {
    return true;
  }
  return record.firstEpoch <= row.timestamp &&
         record.lastEpochExcl <= row.timestamp &&
         row.timestamp - record.lastEpochExcl <
             retryHorizonSeconds(row.session.channel);
}

// ------------------------------------------------------------------ LEG C

void runLegC(const pl::synth::pii::PoolSet &poolSet) {
  constexpr std::int32_t kPopulation = 10'000;
  std::printf("\n##### LEG C: pop %d over 365 days #####\n", kPopulation);
  const auto leg = runWorld(poolSet, kPopulation, 365);
  const auto &rows = leg.result.rows;
  const auto &world = *leg.world;
  const auto &catalog = world.cps.merchants;

  std::unordered_map<pl::entity::Key, pl::entity::PersonId> ownerOf;
  for (const auto &record : world.holdings.accounts.registry.records) {
    if (record.owner != pl::entity::invalidPerson) {
      ownerOf.emplace(record.id, record.owner);
    }
  }
  std::unordered_map<pl::entity::Key, std::uint32_t> catalogIndex;
  for (std::uint32_t i = 0; i < catalog.records.size(); ++i) {
    catalogIndex.emplace(catalog.records[i].counterpartyId, i);
  }

  std::size_t retiredRows = 0;
  std::size_t slotRows = 0;
  std::size_t checkRows = 0;
  std::size_t remoteRows = 0;
  std::size_t remoteBad = 0;
  std::size_t retriedPastClose = 0;
  std::size_t platformRows = 0;
  std::size_t funeralRows = 0;
  std::size_t funeralWrongHome = 0;
  std::size_t unresolved = 0;
  std::size_t onRetiredKey = 0;
  std::map<pl::entity::Key, std::size_t> byAccount;
  std::map<pl::entity::Key, std::size_t> funeralsByHome;
  std::map<pl::entity::PersonId, std::set<pl::entity::Key>> banksByWriter;
  std::map<pl::entity::PersonId, std::set<pl::entity::Key>> platformsBySender;

  // C6 degree: the distinct source accounts of each destination, of every
  // retired row, and of each flow (the per-flow catch-all disarm). The fresh
  // payee disarm draws the bank of a person's n-th check from payee slot
  // kCheckPayeesPerPerson + n, a slot no real payee uses.
  using Sources = std::unordered_set<pl::entity::Key>;
  std::unordered_map<pl::entity::Key, Sources> sourcesOf;
  Sources payers;
  Sources checkSources;
  Sources remoteSources;
  Sources platformSources;
  Sources funeralSources;
  std::unordered_map<std::uint32_t, Sources> freshBankSources;
  std::unordered_map<pl::entity::PersonId, std::uint32_t> checksWritten;

  const auto retired = cps::retiredExternalUnknown();
  const auto &timelines = world.people.personas.timelines;

  for (const auto &row : rows) {
    onRetiredKey += row.source == retired || row.target == retired ? 1U : 0U;
    if (!isRetiredFlow(row)) {
      continue;
    }
    ++retiredRows;
    ++byAccount[row.target];
    sourcesOf[row.target].insert(row.source);
    payers.insert(row.source);

    const auto owner = ownerOf.find(row.source);
    if (owner == ownerOf.end()) {
      ++unresolved;
      continue;
    }
    const auto person = owner->second;

    if (keys::isFuneralHome(row.target)) {
      ++funeralRows;
      ++funeralsByHome[row.target];
      funeralSources.insert(row.source);
      const auto death = pl::time::toEpochSeconds(timelines[person - 1].death);
      const auto area = remote::homeAreaAt(
          world.people.homeAreas, &world.people.relocation, person, death);
      funeralWrongHome += row.target == remote::funeralHomeFor(area, person) &&
                                  keys::funeralHomeArea(row.target) == area
                              ? 0U
                              : 1U;
      continue;
    }
    if (keys::isP2pPlatform(row.target)) {
      ++platformRows;
      platformsBySender[person].insert(row.target);
      platformSources.insert(row.source);
      continue;
    }
    ++slotRows;
    if (keys::isCheckBank(row.target)) {
      ++checkRows;
      banksByWriter[person].insert(row.target);
      checkSources.insert(row.source);
      const auto freshSlot =
          remote::kCheckPayeesPerPerson + checksWritten[person]++;
      freshBankSources[remote::checkBankRankFor(person, freshSlot)].insert(
          row.source);
      continue;
    }
    ++remoteRows;
    remoteSources.insert(row.source);
    const auto it = catalogIndex.find(row.target);
    const bool ok =
        it != catalogIndex.end() &&
        remote::RemoteMerchantTable::eligible(catalog.records[it->second]) &&
        liveAtEmission(catalog.records[it->second], row);
    remoteBad += ok ? 0U : 1U;
    retriedPastClose +=
        ok && !catalog.records[it->second].liveAt(row.timestamp) ? 1U : 0U;
  }

  std::printf("  retired rows %zu: slot %zu (checks %zu, remote merchants "
              "%zu), P2P platform %zu, funerals %zu; unresolved source %zu\n",
              retiredRows, slotRows, checkRows, remoteRows, platformRows,
              funeralRows, unresolved);
  check(slotRows > 0 && checkRows > 0 && remoteRows > 0 && funeralRows > 0 &&
            unresolved == 0,
        "C: a retired flow is empty or a retired row's source has no owner, "
        "so a share check below would pass on no data");
  check(onRetiredKey == 0, "C: " + std::to_string(onRetiredKey) +
                               " rows name the retired catch-all");

  // C1.
  const double expectedCheck = remote::checkFractionOfSlot(2025, 0.05);
  const double checkShare =
      static_cast<double>(checkRows) / static_cast<double>(slotRows);
  std::printf("  C1 check share of the slot %.4f (DCPC-derived %.4f)\n",
              checkShare, expectedCheck);
  check(withinSigmas(checkShare, expectedCheck, static_cast<double>(slotRows)),
        "C1: the check share of the external-unknown slot is off the "
        "DCPC-derived fraction by more than 4 sigma");

  // C2: over person payee slots, which is where the SOD table is drawn.
  std::vector<std::size_t> slotsAtRank(remote::checkBankPoolSize() + 1U, 0);
  const auto personCount = static_cast<pl::entity::PersonId>(kPopulation);
  for (pl::entity::PersonId person = 1; person <= personCount; ++person) {
    for (std::uint32_t slot = 0; slot < remote::kCheckPayeesPerPerson; ++slot) {
      ++slotsAtRank[remote::checkBankRankFor(person, slot)];
    }
  }
  const double slots = static_cast<double>(kPopulation) *
                       static_cast<double>(remote::kCheckPayeesPerPerson);
  std::size_t topTen = 0;
  for (std::uint32_t rank = 1; rank <= 10; ++rank) {
    topTen += slotsAtRank[rank];
  }
  const double top1 = static_cast<double>(slotsAtRank[1]) / slots;
  const double top10 = static_cast<double>(topTen) / slots;
  std::printf("  C2 payee slots at the largest bank %.4f (SOD %.4f), top 10 "
              "%.4f (SOD %.4f)\n",
              top1, remote::checkBankShare(1), top10,
              remote::checkBankCumulativeShare(10));
  check(withinSigmas(top1, remote::checkBankShare(1), slots) &&
            withinSigmas(top10, remote::checkBankCumulativeShare(10), slots),
        "C2: the payee-bank draw is off the FDIC SOD table by more than 4 "
        "sigma");

  // C3. The no-contact fallback does not fire here (every person holds at
  // least three in-population contacts), so the platform choice is checked
  // where it is made: over every person of the leg, since each person uses
  // one platform. Realized fallback rows are printed, and any that do occur
  // must sit on their sender's own platform.
  std::size_t venmoPeople = 0;
  const auto venmo = keys::p2pPlatform(keys::P2pPlatform::venmo);
  for (pl::entity::PersonId person = 1; person <= personCount; ++person) {
    venmoPeople += remote::p2pPlatformFor(person) == venmo ? 1U : 0U;
  }
  const double people = static_cast<double>(kPopulation);
  const double venmoShare = static_cast<double>(venmoPeople) / people;
  const double expectedVenmo =
      remote::p2pPlatformShare(keys::P2pPlatform::venmo);
  std::size_t offOwnPlatform = 0;
  for (const auto &[person, platforms] : platformsBySender) {
    offOwnPlatform += platforms.size() == 1U &&
                              platforms.contains(remote::p2pPlatformFor(person))
                          ? 0U
                          : 1U;
  }
  std::printf("  C3 people on Venmo %.4f (monthly-actives split %.4f); "
              "no-contact P2P rows %zu from %zu senders, off their own "
              "platform %zu\n",
              venmoShare, expectedVenmo, platformRows, platformsBySender.size(),
              offOwnPlatform);
  check(withinSigmas(venmoShare, expectedVenmo, people),
        "C3: the platform split is off the monthly-actives split by more than "
        "4 sigma");
  check(offOwnPlatform == 0, "C3: " + std::to_string(offOwnPlatform) +
                                 " no-contact senders pay a platform other "
                                 "than their own");

  // C4.
  std::printf("  C4 remote-merchant rows %zu: off-domain %zu; re-presented "
              "after the merchant closed, inside the %lld h retry horizon, "
              "%zu\n",
              remoteRows, remoteBad,
              static_cast<long long>(retryHorizonSeconds(
                  pl::channels::tag(pl::channels::Legit::externalUnknown)) /
                  3600),
              retriedPastClose);
  check(remoteBad == 0,
        "C4: " + std::to_string(remoteBad) +
            " remote-merchant rows pay an internal, local or closed merchant");

  // C5.
  std::size_t busiestHome = 0;
  for (const auto &[home, count] : funeralsByHome) {
    busiestHome = std::max(busiestHome, count);
  }
  std::printf("  C5 funerals %zu across %zu homes, busiest home %zu, wrong "
              "home %zu\n",
              funeralRows, funeralsByHome.size(), busiestHome,
              funeralWrongHome);
  check(funeralWrongHome == 0,
        "C5: " + std::to_string(funeralWrongHome) +
            " funerals pay a home outside the decedent's city at death");
  check(busiestHome <= 3,
        "C5: one funeral home took " + std::to_string(busiestHome) +
            " funerals; at this population a home should see one or two");

  // C6.
  std::vector<std::pair<std::size_t, pl::entity::Key>> ranked;
  ranked.reserve(byAccount.size());
  for (const auto &[account, count] : byAccount) {
    ranked.emplace_back(count, account);
  }
  std::ranges::sort(ranked, std::greater<>{});
  std::printf("  C6 retired rows over %zu destination accounts; largest:\n",
              ranked.size());
  for (std::size_t i = 0; i < std::min<std::size_t>(5, ranked.size()); ++i) {
    const auto &[count, account] = ranked[i];
    std::printf("    %-18s serial %llu: %zu (%.4f)\n", kindOf(account),
                static_cast<unsigned long long>(account.number), count,
                static_cast<double>(count) / static_cast<double>(retiredRows));
  }
  const double largest = ranked.empty()
                             ? 1.0
                             : static_cast<double>(ranked.front().first) /
                                   static_cast<double>(retiredRows);
  check(largest <= kMaxAccountShare,
        "C6: one account takes " + std::to_string(largest) +
            " of the retired rows, above the " +
            std::to_string(kMaxAccountShare) + " ceiling");
  // DISARM. The pre-round shape (one key for every retired row) scores 1.0,
  // which any ceiling below 1 rejects, so it cannot show the ceiling is
  // tight. The nearest wrong design can: one catch-all PER FLOW (a single
  // "unknown check payee", a single P2P fallback and a single funeral payee)
  // is what a lazy fix would ship. Its largest account is the largest flow,
  // scored on these same rows, and the ceiling must reject it too.
  const double perFlowDisarm =
      static_cast<double>(
          std::max({checkRows, remoteRows, platformRows, funeralRows})) /
      static_cast<double>(retiredRows);
  std::printf("  C6 disarm, one catch-all per flow: largest account %.4f\n",
              perFlowDisarm);
  check(perFlowDisarm > kMaxAccountShare,
        "C6: the ceiling would pass a one-catch-all-per-flow design (" +
            std::to_string(perFlowDisarm) +
            "), so it is not tight enough to "
            "mean anything");

  // C6, degree. Rows are not what made the catch-all harmful downstream: its
  // DEGREE was (62% of deposit accounts paid it), and degree is what merges
  // components and sinks PageRank. A check hub is small in rows and large in
  // degree, so the row ceiling above cannot see it.
  const double payerCount = static_cast<double>(payers.size());
  const auto degreeOf = [&](std::size_t sources) {
    return payerCount == 0.0 ? 1.0
                             : static_cast<double>(sources) / payerCount;
  };
  std::vector<std::pair<std::size_t, pl::entity::Key>> byDegree;
  byDegree.reserve(sourcesOf.size());
  double boundedDegree = 0.0;
  pl::entity::Key boundedAccount{};
  double namedDegree = 0.0;
  for (const auto &[account, sources] : sourcesOf) {
    byDegree.emplace_back(sources.size(), account);
    const double degree = degreeOf(sources.size());
    const bool named =
        !keys::isCheckBank(account) && !keys::isFuneralHome(account);
    if (named) {
      namedDegree = std::max(namedDegree, degree);
    } else if (degree > boundedDegree) {
      boundedDegree = degree;
      boundedAccount = account;
    }
  }
  std::ranges::sort(byDegree, std::greater<>{});
  const double reach = checkHubReach();
  const double degreeCeiling =
      reach + kSigmas * std::sqrt(reach * (1.0 - reach) / payerCount);
  std::printf("  C6 degree over %zu payers (distinct source accounts); "
              "largest:\n",
              payers.size());
  for (std::size_t i = 0; i < std::min<std::size_t>(6, byDegree.size()); ++i) {
    const auto &[sources, account] = byDegree[i];
    std::printf("    %-18s serial %llu: %zu payers (%.4f)\n", kindOf(account),
                static_cast<unsigned long long>(account.number), sources,
                degreeOf(sources));
  }
  std::printf("  C6 largest check-payee bank or funeral home degree %.4f "
              "(payee-slot reach %.4f, ceiling %.4f); largest named "
              "counterparty %.4f (printed, not bounded)\n",
              boundedDegree, reach, degreeCeiling, namedDegree);
  check(boundedDegree <= degreeCeiling,
        "C6: the " + std::string(kindOf(boundedAccount)) + " at serial " +
            std::to_string(boundedAccount.number) + " is paid by " +
            std::to_string(boundedDegree) +
            " of the payers, above the payee-slot ceiling " +
            std::to_string(degreeCeiling));
  // DISARMS, on degree. One catch-all per flow puts every writer of that
  // flow on one account. A fresh payee bank per check (the research's
  // per-check payee, keyed per bank as here) lets the largest bank's degree
  // grow with the checks a person writes instead of stopping at their payees.
  const double perFlowDegreeDisarm = degreeOf(
      std::max({checkSources.size(), remoteSources.size(),
                platformSources.size(), funeralSources.size()}));
  std::size_t freshLargest = 0;
  for (const auto &[rank, sources] : freshBankSources) {
    freshLargest = std::max(freshLargest, sources.size());
  }
  const double freshPayeeDisarm = degreeOf(freshLargest);
  std::printf("  C6 degree disarms: one catch-all per flow %.4f, a fresh "
              "payee bank per check %.4f\n",
              perFlowDegreeDisarm, freshPayeeDisarm);
  check(perFlowDegreeDisarm > degreeCeiling &&
            freshPayeeDisarm > degreeCeiling,
        "C6: the degree ceiling would pass a one-catch-all-per-flow design (" +
            std::to_string(perFlowDegreeDisarm) +
            ") or a fresh payee bank per check (" +
            std::to_string(freshPayeeDisarm) +
            "), so it is not tight enough to mean anything");

  // C7.
  std::size_t overPayees = 0;
  for (const auto &[person, banks] : banksByWriter) {
    overPayees += banks.size() > remote::kCheckPayeesPerPerson ? 1U : 0U;
  }
  std::printf("  C7 check writers %zu, above %u banks %zu\n",
              banksByWriter.size(), remote::kCheckPayeesPerPerson, overPayees);
  check(overPayees == 0, "C7: " + std::to_string(overPayees) +
                             " check writers pay more banks than payees");
}

} // namespace

int main() {
  std::printf("=== Retired external-unknown catch-all ===\n");
  const auto poolSet = pltest::buildPoolSet(kGoldenSeed);
  runLegB(poolSet);
  runLegC(poolSet);
  if (g_failures != 0) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nAll retired catch-all checks passed.\n");
  return 0;
}
