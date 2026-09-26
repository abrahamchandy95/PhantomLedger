// tests/test_counterparty_sizes.cpp
//
// THE EMPLOYER AND LANDLORD SIZE GATE (counterparty-sizes-2026-09).
//
// The defect this exists for: payroll and rent picked uniformly over 25 and
// 12 counterparties per 10,000 people, so at 200,000 people about 480
// employers each paid about 308 people and 240 landlords each collected from
// about 292 tenants, "individual" landlords included. Both rosters are now a
// published size distribution thinned to the population
// (synth/counterparties/size_law.hpp), and every pick goes through it.
//
// The pure law (tables, counts, tails, the pool and its draw contract) is
// checked in test_counterparties. This file drives the samplers and the
// corpus:
//
//   A. BURN PARITY. The shared entity stream after buildLandlords and
//      buildCounterparties equals a verbatim copy of the retired loops at
//      five populations, and the clients drawn next are identical. DISARM: the
//      same build without the burns must disagree. The whole run-golden
//      build with income off leaves the shared stream where the pre-round
//      build left it (pinned).
//   B. THE REAL SAMPLERS at 200,000 people, no ledger (the style of
//      test_card_merchant_graph sub-gate G): EmploymentInitializer over
//      148,000 people and initializeLease over 70,000 payers, on the real
//      lanes. DISARMS: a uniform pick over the same employer roster, and the
//      retired 240-landlord roster. The camouflage salary mimic runs through
//      the real generator on the same pool: its employers' headcount matches
//      legitimate payroll's, and every row from an employer with legitimate
//      payees posts on that employer's own schedule. DISARMS: the same
//      generator over a uniform pool, and on schedules from the fraud
//      factory.
//   C. CHURN. 20,000 job chains over 20 years never re-pick the current
//      employer, and every job interval equals the retired uniform pick's on
//      the same lane (the lane-alignment proof). Timing is printed.
//   D. THE CORPUS at the run-golden configuration (pop 2,000, 60 days, seed
//      3405691582) and over 365 days: salary, benefit, rent and camouflage
//      salary rows land in their pools, camouflage salary posts on its
//      employer's schedule, and no camouflage P2P row pays an employer or a
//      landlord. This is the domain predicate beside the run golden's
//      digest.
//   E. MEMORY at 500,000 people: the new rosters' resident bytes and the
//      registry growth, printed and bounded.

#include "gate_world.hpp"
#include "test_support.hpp"
#include "window_leg_support.hpp"

#include "phantomledger/activity/income/timestamps.hpp"
#include "phantomledger/activity/recurring/employment.hpp"
#include "phantomledger/activity/recurring/lease.hpp"
#include "phantomledger/entities/counterparties/cash_points.hpp"
#include "phantomledger/entities/counterparties/institutional_accounts.hpp"
#include "phantomledger/entities/counterparties/sized_pool.hpp"
#include "phantomledger/pipeline/world_footprint.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/synth/counterparties/make.hpp"
#include "phantomledger/synth/counterparties/size_law.hpp"
#include "phantomledger/synth/landlords/make.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/fraud/camouflage.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;
namespace cps = pl::counterparties;
namespace sizes = pl::synth::counterparties::sizes;
namespace recur = pl::activity::recurring;
namespace channels = pl::channels;
namespace entityStage = pl::pipeline::stages::entities;

using pl::entity::Key;
using pl::entity::counterparty::SizedKeys;
using pl::entity::counterparty::SizedPool;
using LandlordType = pl::entity::landlord::Type;

constexpr std::uint64_t kSeed = 3405691582ULL;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

[[nodiscard]] bool sameState(const pl::random::Rng &a,
                             const pl::random::Rng &b) {
  return a.engine().state_hi() == b.engine().state_hi() &&
         a.engine().state_lo() == b.engine().state_lo();
}

[[nodiscard]] SizedKeys employerPool(
    const pl::entity::counterparty::Directory &directory) {
  return SizedKeys{.keys = directory.employers.accounts.external,
                   .law = directory.employers.pool};
}

[[nodiscard]] SizedKeys landlordPool(const pl::synth::landlords::Pack &pack) {
  SizedKeys out{.keys = {}, .law = pack.roster.pool};
  out.keys.reserve(pack.roster.records.size());
  for (const auto &record : pack.roster.records) {
    out.keys.push_back(record.accountId);
  }
  return out;
}

[[nodiscard]] SizedKeys uniformOver(std::vector<Key> keys) {
  const std::array<SizedPool::Class, 1> one{
      {{static_cast<std::uint32_t>(keys.size()), 1.0}}};
  return SizedKeys{.keys = std::move(keys),
                   .law = SizedPool::build(one, SizedPool::npos, {})};
}

// The epoch days an employer's payroll posts on in [start, endExcl): its pay
// dates plus the posting lag, as SalaryCalculator's rows post
// (timestamps::jittered, which adds no day jitter for salary).
[[nodiscard]] std::set<std::int64_t>
postingDays(const recur::PayrollSchedule &schedule, pl::time::TimePoint start,
            pl::time::TimePoint endExcl) {
  std::set<std::int64_t> out;
  for (const auto &payDate :
       recur::paydatesForProfile(schedule, start, endExcl)) {
    out.insert(pl::time::toEpochSeconds(
                   pl::time::addDays(payDate, schedule.postingLagDays)) /
               86'400);
  }
  return out;
}

// On an employer's schedule: one of its posting days, inside the salary
// posting hours.
[[nodiscard]] bool onSchedule(const std::set<std::int64_t> &days,
                              std::int64_t ts) {
  constexpr auto kJitter =
      pl::activity::income::timestamps::kSalaryTimestampJitter;
  const auto hour = (ts % 86'400) / 3'600;
  return days.contains(ts / 86'400) && hour >= kJitter.hourStart &&
         hour < kJitter.hourEndExcl;
}

// ------------------------------------------------------------------ A

// Verbatim copies of the retired entity-stage loops (make.hpp and
// landlords/make.hpp before this round).
namespace legacy {

void landlords(pl::random::Rng &rng, int population) {
  const int total = std::max(
      3, static_cast<int>(std::round(
             12.0 * (static_cast<double>(population) / 10000.0))));
  const std::array<double, 3> mix{0.38, 0.15, 0.47};
  const std::array<double, 3> inBank{0.06, 0.04, 0.01};
  const auto cdf = pl::probability::distributions::buildCdf(mix);
  for (int i = 0; i < total; ++i) {
    const auto idx =
        pl::probability::distributions::sampleIndex(cdf, rng.nextDouble());
    (void)rng.coin(inBank[idx]);
  }
}

[[nodiscard]] int scaled(double perTenK, int floor, int population) {
  return std::max(floor, static_cast<int>(std::round(
                             perTenK * (static_cast<double>(population) /
                                        10'000.0))));
}

void employers(pl::random::Rng &rng, int population) {
  const int total = scaled(25.0, 5, population);
  for (int i = 0; i < total; ++i) {
    (void)rng.coin(0.04);
  }
}

} // namespace legacy

// The next u64 the run-golden gate world's shared stream hands out after the
// whole build (entities, products, infra, blueprint, opening book, market and
// obligations) with income OFF. Measured on the pre-round build (the staged
// atm-spread tree, exported with `git checkout-index` and built separately)
// and on this build; the two agree. With income on it moves, because salary
// posting jitter draws on the shared stream per payday and each employer's
// cadence is its own: that is the declared cascade this round's re-pins
// absorb (test_bank_ledger, test_remote_payees, test_product_providers).
constexpr std::uint64_t kIncomeFreeStreamNext = 0xddfd735e74a97cd2ULL;

void gateA(const pl::synth::pii::PoolSet &poolSet) {
  std::printf("\n##### A: burn parity on the shared entity stream #####\n");
  constexpr std::array<int, 5> kPops{1, 300, 2'000, 20'791, 200'000};

  for (const int pop : kPops) {
    auto live = pl::random::Rng::fromSeed(kSeed);
    const auto pack = entityStage::buildLandlords(live, pop, kSeed);
    const auto directory = entityStage::buildCounterparties(live, pop);

    auto reference = pl::random::Rng::fromSeed(kSeed);
    legacy::landlords(reference, pop);
    legacy::employers(reference, pop);
    pl::entity::counterparty::BankSplit clients;
    pl::synth::counterparties::detail::fillBankSplit(
        reference, pl::entity::Role::client, legacy::scaled(250.0, 25, pop),
        0.02, clients);

    // DISARM: the same clients without either burn.
    auto bare = pl::random::Rng::fromSeed(kSeed);
    pl::entity::counterparty::BankSplit bareClients;
    pl::synth::counterparties::detail::fillBankSplit(
        bare, pl::entity::Role::client, legacy::scaled(250.0, 25, pop), 0.02,
        bareClients);

    const bool parity = sameState(live, reference);
    const bool sameClients =
        clients.internal == directory.clients.accounts.internal &&
        clients.external == directory.clients.accounts.external;
    // The state is the sensitive check: at small populations both client
    // splits can come out with the same internal positions.
    const bool disarmRed = !sameState(bare, reference);
    const bool bareClientsMoved = bareClients.internal != clients.internal;
    std::printf("  pop %7d: %6zu landlords, %6zu employers; state parity %s, "
                "clients identical %s, burn-free disarm %s (its clients "
                "%s)\n",
                pop, pack.roster.records.size(),
                directory.employers.accounts.external.size(),
                parity ? "yes" : "NO", sameClients ? "yes" : "NO",
                disarmRed ? "red" : "GREEN",
                bareClientsMoved ? "moved" : "coincide");
    check(parity, "A: the shared stream after the counterparty build is off "
                  "the retired loops at pop " +
                      std::to_string(pop));
    check(sameClients,
          "A: the clients drawn after the burn moved at pop " +
              std::to_string(pop));
    check(disarmRed, "A: skipping the burns did not move the stream, so the "
                     "parity check cannot fail at pop " +
                         std::to_string(pop));
  }

  pltest::WorldSpec spec;
  spec.seed = kSeed;
  spec.window = pl::time::Window{.start = pl::time::makeTime({2025, 1, 1}),
                                 .days = 60};
  spec.population = 2'000;
  spec.fraudProfile = pltest::scaledFraudProfile();
  spec.withIncome = false;
  const pltest::GateWorld world(poolSet, spec);
  auto shared = world.rng;
  const auto next = shared.nextU64();
  auto fresh = pl::random::Rng::fromSeed(kSeed);
  std::printf("  run-golden world, income off: shared stream next %016llx "
              "(pinned %016llx), registry %zu records\n",
              static_cast<unsigned long long>(next),
              static_cast<unsigned long long>(kIncomeFreeStreamNext),
              world.holdings.accounts.registry.records.size());
  check(next == kIncomeFreeStreamNext,
        "A: the shared stream after the income-free build moved off the "
        "pre-round build");
  check(next != fresh.nextU64(),
        "A: the build drew nothing from the shared stream, so the pin above "
        "would pass on no data");
}

// ------------------------------------------------------------------ B

constexpr int kScalePop = 200'000;
constexpr std::size_t kWorkers = 148'000; // 0.74 x 200,000
constexpr std::size_t kRenters = 70'000;  // 0.35 x 200,000

struct EmployerCensus {
  std::vector<std::uint32_t> payees;
  // The schedule EmploymentInitializer handed each used employer's payees.
  std::unordered_map<Key, recur::PayrollSchedule> schedules;
  double federalShare = 0.0;
  double hundredPlusShare = 0.0;
  double onePayeeShare = 0.0;
  double under20Share = 0.0;
  std::size_t used = 0;
  std::uint32_t largestPrivate = 0;
  double topTenShare = 0.0;
};

[[nodiscard]] EmployerCensus
censusEmployers(const SizedKeys &employers, std::size_t federal,
                std::size_t under20End, const pl::random::RngFactory &factory) {
  const recur::JobRules jobRules{};
  const recur::PayrollRules payrollRules{};
  const recur::EmploymentInitializer init(jobRules, payrollRules, factory);
  const recur::SalarySource salary = [] { return 54'000.0; };
  const auto start = pl::time::makeTime({2025, 1, 1});

  EmployerCensus out;
  out.payees.assign(employers.size(), 0);
  for (std::size_t p = 1; p <= kWorkers; ++p) {
    const auto job = init(std::to_string(p), start, employers, salary);
    const auto ix = employers.indexOf(job.employerAcct);
    PL_CHECK(ix != SizedPool::npos);
    ++out.payees[ix];
    out.schedules.try_emplace(job.employerAcct, job.payroll);
  }

  std::size_t hundredPlus = 0;
  std::size_t one = 0;
  std::size_t under20 = 0;
  for (std::size_t i = 0; i < out.payees.size(); ++i) {
    const auto n = out.payees[i];
    out.used += n > 0 ? 1U : 0U;
    one += n == 1 ? 1U : 0U;
    hundredPlus += n >= 100 ? n : 0U;
    under20 += i < under20End ? n : 0U;
  }
  // Private members sit before the government classes.
  std::uint32_t largestPrivate = 0;
  for (std::size_t i = 0; i < federal && i < out.payees.size(); ++i) {
    largestPrivate = std::max(largestPrivate, out.payees[i]);
  }
  auto sorted = out.payees;
  std::ranges::sort(sorted, std::greater<>{});
  double top10 = 0.0;
  for (std::size_t i = 0; i < 10 && i < sorted.size(); ++i) {
    top10 += sorted[i];
  }

  const double workers = static_cast<double>(kWorkers);
  out.federalShare =
      federal < out.payees.size() ? out.payees[federal] / workers : 0.0;
  out.hundredPlusShare = static_cast<double>(hundredPlus) / workers;
  out.onePayeeShare =
      out.used == 0 ? 0.0
                    : static_cast<double>(one) / static_cast<double>(out.used);
  out.under20Share = static_cast<double>(under20) / workers;
  out.largestPrivate = largestPrivate;
  out.topTenShare = top10 / workers;
  return out;
}

struct LandlordCensus {
  std::vector<std::uint32_t> tenants;
  std::uint32_t maxTenants = 0;
  std::uint32_t maxIndividualTenants = 0;
  double topFiftyShare = 0.0;
  double onePayerShare = 0.0;
  std::size_t used = 0;
  std::array<double, 3> typeShare{};
};

[[nodiscard]] LandlordCensus
censusLandlords(const SizedKeys &landlords,
                const std::vector<LandlordType> &types, std::size_t topFirst,
                const pl::random::RngFactory &factory) {
  const recur::LeaseRules rules{};
  const recur::RentSource rent = [] { return 1'500.0; };
  const auto start = pl::time::makeTime({2025, 1, 1});

  LandlordCensus out;
  out.tenants.assign(landlords.size(), 0);
  for (std::size_t p = 1; p <= kRenters; ++p) {
    const auto key = std::to_string(p);
    auto outer = factory.rng({"lease_init", key});
    const auto lease = recur::initializeLease(
        rules, factory, outer,
        recur::LeaseInitInput{.payerKey = key,
                              .startDate = start,
                              .landlords = &landlords,
                              .rentSource = rent});
    const auto ix = landlords.indexOf(lease.landlordAcct);
    PL_CHECK(ix != SizedPool::npos);
    ++out.tenants[ix];
    ++out.typeShare[static_cast<std::size_t>(types[ix])];
  }

  std::size_t one = 0;
  std::size_t topFifty = 0;
  for (std::size_t i = 0; i < out.tenants.size(); ++i) {
    const auto n = out.tenants[i];
    out.used += n > 0 ? 1U : 0U;
    one += n == 1 ? 1U : 0U;
    out.maxTenants = std::max(out.maxTenants, n);
    if (types[i] == LandlordType::individual) {
      out.maxIndividualTenants = std::max(out.maxIndividualTenants, n);
    }
    topFifty += i >= topFirst ? n : 0U;
  }
  for (auto &share : out.typeShare) {
    share /= static_cast<double>(kRenters);
  }
  out.topFiftyShare =
      static_cast<double>(topFifty) / static_cast<double>(kRenters);
  out.onePayerShare =
      out.used == 0 ? 0.0
                    : static_cast<double>(one) / static_cast<double>(out.used);
  return out;
}

void gateB() {
  std::printf("\n##### B: the real samplers at pop %d, no ledger #####\n",
              kScalePop);
  const pl::random::RngFactory factory{kSeed};

  auto rng = pl::random::Rng::fromSeed(kSeed);
  const auto directory = pl::synth::counterparties::make(rng, kScalePop);
  const auto employers = employerPool(directory);
  const auto law = sizes::employerLaw(kScalePop, 0.74);
  std::size_t federal = 0;
  for (std::size_t c = 0; c < sizes::kFederalClass; ++c) {
    federal += law.classes[c].members;
  }
  const std::size_t under20End =
      law.classes[0].members + law.classes[1].members + law.classes[2].members;

  const auto armed = censusEmployers(employers, federal, under20End, factory);
  const double pFederal = sizes::employerClasses()[sizes::kFederalClass].mass;
  const double sigma =
      std::sqrt(pFederal * (1.0 - pFederal) / static_cast<double>(kWorkers));
  std::printf("  %zu employers, %zu used; federal payer %u (share %.5f, law "
              "%.5f +- %.5f); largest private %u; top 10 %.4f\n",
              employers.size(), armed.used, armed.payees[federal],
              armed.federalShare, pFederal, 4.0 * sigma, armed.largestPrivate,
              armed.topTenShare);
  std::printf("  workers at 100+ payee employers %.4f; P(one payee | used) "
              "%.4f; under-20 classes %.4f\n",
              armed.hundredPlusShare, armed.onePayeeShare, armed.under20Share);
  // MulePatternLearner's hub registry starts at 2,048 payments a year, which
  // a biweekly payroll (26 credits a year) reaches at 79 payees.
  std::size_t hubEmployers = 0;
  for (const auto n : armed.payees) {
    hubEmployers += n >= 79 ? 1U : 0U;
  }
  std::printf("  employers with 79+ payees (about 2,048 biweekly credits a "
              "year): %zu\n",
              hubEmployers);

  check(std::abs(armed.federalShare - pFederal) <= 4.0 * sigma,
        "B: the federal payer's share is off its law by more than 4 sigma");
  check(armed.hundredPlusShare >= 0.08 && armed.hundredPlusShare <= 0.16,
        "B: the share of workers at 100+ payee employers left [0.08, 0.16]");
  check(armed.onePayeeShare >= 0.45 && armed.onePayeeShare <= 0.62,
        "B: P(one payee | used) left [0.45, 0.62]");
  check(armed.used >= 50'000, "B: fewer than 50,000 employers pay anyone");
  check(std::abs(armed.under20Share - 0.139) <= 0.005,
        "B: the under-20 classes' share of workers left 0.139 +- 0.005");

  // DISARM: the same roster picked uniformly.
  const auto flat = uniformOver(employers.keys);
  const auto disarmed = censusEmployers(flat, federal, under20End, factory);
  const bool federalRed =
      std::abs(disarmed.federalShare - pFederal) > 4.0 * sigma;
  const bool hundredRed = disarmed.hundredPlusShare < 0.08;
  std::printf("  DISARM uniform pick: federal share %.6f, 100+ share %.4f -> "
              "%s\n",
              disarmed.federalShare, disarmed.hundredPlusShare,
              federalRed && hundredRed ? "red" : "GREEN");
  check(federalRed && hundredRed,
        "B: a uniform pick over the same roster passes the size checks, so "
        "they cannot see the law");

  // The camouflage salary mimic through the real generator: one ring of
  // 20,000 accounts, salary only, against the same pool, with its employer
  // schedules derived from `schedules`. Headcount is the legitimate payee
  // count above, so the legitimate side's mean is size-biased (a worker's
  // employer's headcount).
  const auto camoStart = pl::time::makeTime({2025, 1, 1});
  constexpr std::int32_t kCamoDays = 60;
  const auto camouflageSalary = [&](const SizedKeys &pool,
                                    const pl::random::RngFactory &schedules) {
    namespace fraud = pl::transfers::fraud;
    fraud::Plan ring;
    ring.ringId = 1;
    for (std::uint64_t i = 1; i <= 20'000; ++i) {
      ring.fraudAccounts.push_back(pl::entity::makeKey(
          pl::entity::Role::account, pl::entity::Bank::internal, i));
    }
    auto camoRng = factory.rng({"test", "camouflage-salary"});
    const fraud::AccountPools pools{
        .depositAccounts = {}, .billerAccounts = {}, .employers = &pool};
    fraud::CamouflageContext ctx{
        .execution = {.txf = pl::transactions::Factory(camoRng),
                      .rng = &camoRng,
                      .factory = &factory},
        .window = {.start = camoStart, .days = kCamoDays},
        .accounts = &pools,
        .payrollFactory = &schedules,
    };
    const fraud::camouflage::Rates salaryOnly{
        .smallP2pPerDayP = 0.0, .billMonthlyP = 0.0, .salaryInboundP = 1.0};
    return fraud::camouflage::generate(ctx, ring, salaryOnly);
  };
  const auto camouflageHeadcount =
      [&](const std::vector<pl::transactions::Transaction> &rows) {
        std::set<std::pair<Key, Key>> pairs;
        for (const auto &row : rows) {
          pairs.emplace(row.source, row.target);
        }
        double sum = 0.0;
        for (const auto &[source, target] : pairs) {
          (void)target;
          const auto ix = employers.indexOf(source);
          PL_CHECK(ix != SizedPool::npos);
          sum += armed.payees[ix];
        }
        return pairs.empty() ? 0.0 : sum / static_cast<double>(pairs.size());
      };
  double sizeBiased = 0.0;
  for (const auto n : armed.payees) {
    sizeBiased += static_cast<double>(n) * n;
  }
  sizeBiased /= static_cast<double>(kWorkers);
  const auto camoRows = camouflageSalary(employers, factory);
  const double camoArmed = camouflageHeadcount(camoRows) / sizeBiased;
  const double camoUniform =
      camouflageHeadcount(camouflageSalary(flat, factory)) / sizeBiased;
  std::printf("  camouflage salary employers: mean headcount ratio %.3f "
              "against legitimate payees (uniform-pick disarm %.4f)\n",
              camoArmed, camoUniform);
  check(camoArmed >= 0.5 && camoArmed <= 2.0,
        "B: camouflage salary employers are not the size of legitimate ones "
        "(headcount ratio outside [0.5, 2.0])");
  check(camoUniform < 0.1,
        "B: a uniform camouflage pick passes the headcount band, so it "
        "cannot see the law");

  // The camouflage schedule. Every camouflage salary row paid by an employer
  // with legitimate payees posts on a day, and in the hours, that employer's
  // payroll posts, read off the schedule EmploymentInitializer handed its
  // payees. A mule paid on dates its employer pays nobody else is a label
  // any detector comparing an employer's payees can read. DISARM: the same
  // generator with its schedules derived from the fraud factory, independent
  // of the employer's own. That is the law of the retired per-mule schedule
  // draw, and the wiring mistake the injector's two factories invite.
  struct ScheduleCensus {
    std::size_t rows = 0, rowsOff = 0, pairs = 0, pairsOff = 0;
  };
  const auto scheduleCensus =
      [&](const std::vector<pl::transactions::Transaction> &rows) {
        const auto camoEnd = pl::time::addDays(camoStart, kCamoDays);
        std::unordered_map<Key, std::set<std::int64_t>> days;
        std::map<std::pair<Key, Key>, bool> pairOff;
        ScheduleCensus out;
        for (const auto &row : rows) {
          const auto it = armed.schedules.find(row.source);
          if (it == armed.schedules.end()) {
            continue; // no legitimate payee to compare against
          }
          auto [own, fresh] = days.try_emplace(row.source);
          if (fresh) {
            own->second = postingDays(it->second, camoStart, camoEnd);
          }
          const bool off = !onSchedule(own->second, row.timestamp);
          ++out.rows;
          out.rowsOff += off ? 1U : 0U;
          auto &pair = pairOff[{row.source, row.target}];
          pair = pair || off;
        }
        out.pairs = pairOff.size();
        for (const auto &[pair, off] : pairOff) {
          (void)pair;
          out.pairsOff += off ? 1U : 0U;
        }
        return out;
      };
  const auto ownSchedule = scheduleCensus(camoRows);
  const pl::random::RngFactory fraudFactory{kSeed ^ 0x9E3779B97F4A7C15ULL};
  const auto wrongSchedule =
      scheduleCensus(camouflageSalary(employers, fraudFactory));
  const double wrongPairShare =
      wrongSchedule.pairs == 0
          ? 0.0
          : static_cast<double>(wrongSchedule.pairsOff) /
                static_cast<double>(wrongSchedule.pairs);
  std::printf("  camouflage salary schedule: %zu pairs with a legitimate "
              "co-payee, %zu of their %zu rows off the employer's own posting "
              "days; DISARM fraud-factory schedules: %zu of %zu pairs (%.3f) "
              "and %zu of %zu rows off -> %s\n",
              ownSchedule.pairs, ownSchedule.rowsOff, ownSchedule.rows,
              wrongSchedule.pairsOff, wrongSchedule.pairs, wrongPairShare,
              wrongSchedule.rowsOff, wrongSchedule.rows,
              wrongPairShare > 0.5 ? "red" : "GREEN");
  check(ownSchedule.rows > 0 && ownSchedule.rowsOff == 0,
        "B: a camouflage salary row posts off its employer's own payroll "
        "schedule (or no row has a legitimate co-payee to check)");
  check(wrongPairShare > 0.5,
        "B: camouflage salary on schedules independent of the employer's "
        "passes the schedule check, so it cannot see the schedule");

  // Landlords.
  auto landlordRng = pl::random::Rng::fromSeed(kSeed);
  const auto pack =
      pl::synth::landlords::makePack(landlordRng, kScalePop, kSeed);
  const auto landlords = landlordPool(pack);
  std::vector<LandlordType> types;
  types.reserve(pack.roster.records.size());
  std::array<double, 3> predicted{};
  for (std::size_t i = 0; i < pack.roster.records.size(); ++i) {
    const auto type = pack.roster.records[i].type;
    types.push_back(type);
    predicted[static_cast<std::size_t>(type)] += landlords.law.weight(i);
  }
  const std::size_t topFirst = landlords.size() - sizes::kNmhc2024Owners;

  const auto renters = censusLandlords(landlords, types, topFirst, factory);
  const double topMass =
      sizes::landlordClasses()[sizes::kLandlordTailClass].mass;
  std::printf("  %zu landlords (%zu in-bank), %zu used; max tenants %u, max "
              "at an individual landlord %u; Top-50 share %.4f (law %.4f); "
              "P(one tenant | used) %.4f\n",
              landlords.size(), pack.internals.size(), renters.used,
              renters.maxTenants, renters.maxIndividualTenants,
              renters.topFiftyShare, topMass, renters.onePayerShare);
  std::printf("  renter type mix %.4f / %.4f / %.4f against the roster's "
              "%.4f / %.4f / %.4f\n",
              renters.typeShare[0], renters.typeShare[1], renters.typeShare[2],
              predicted[0], predicted[1], predicted[2]);
  check(renters.maxTenants >= 100, "B: no landlord reaches 100 tenants");
  check(std::abs(renters.topFiftyShare - topMass) <= 0.004,
        "B: the Top-50 owners' share of renters left its law +- 0.004");
  for (std::size_t t = 0; t < 3; ++t) {
    check(std::abs(renters.typeShare[t] - predicted[t]) <= 0.03,
          "B: a landlord type's share of renters is off the roster by more "
          "than 3 points");
  }
  check(renters.maxIndividualTenants <= 12,
        "B: an individual landlord collects from more than 12 tenants");

  // DISARM: the retired roster, 240 landlords picked uniformly.
  std::vector<Key> retiredKeys;
  std::vector<LandlordType> retiredTypes;
  const auto retiredCdf =
      pl::probability::distributions::buildCdf(std::array{0.38, 0.15, 0.47});
  auto typeRng = pl::random::Rng::fromSeed(kSeed);
  for (std::uint64_t i = 1; i <= 240; ++i) {
    retiredKeys.push_back(pl::entity::makeKey(pl::entity::Role::landlord,
                                              pl::entity::Bank::external, i));
    retiredTypes.push_back(
        pl::landlords::kTypes[pl::probability::distributions::sampleIndex(
            retiredCdf, typeRng.nextDouble())]);
  }
  const auto retired = uniformOver(std::move(retiredKeys));
  const auto retiredCensus =
      censusLandlords(retired, retiredTypes, retired.size(), factory);
  std::printf("  DISARM retired 240-landlord roster: max at an individual "
              "landlord %u -> %s\n",
              retiredCensus.maxIndividualTenants,
              retiredCensus.maxIndividualTenants > 12 ? "red" : "GREEN");
  check(retiredCensus.maxIndividualTenants > 12,
        "B: the retired roster passes the individual-landlord bound");
}

// ------------------------------------------------------------------ C

void gateC() {
  std::printf("\n##### C: job churn, 20,000 chains over 20 years #####\n");
  const pl::random::RngFactory factory{kSeed};
  auto rng = pl::random::Rng::fromSeed(kSeed);
  const auto directory = pl::synth::counterparties::make(rng, kScalePop);
  const auto employers = employerPool(directory);

  const recur::JobRules jobRules{};
  const recur::PayrollRules payrollRules{};
  const recur::SalaryGrowthRules growthRules{};
  const recur::SalaryGrowthModel growth(growthRules, factory);
  const recur::EmploymentInitializer init(jobRules, payrollRules, factory);
  const recur::EmploymentAdvancer advance(jobRules, payrollRules, growth,
                                          factory);
  const recur::SalarySource salary = [] { return 54'000.0; };
  const auto start = pl::time::makeTime({2005, 1, 1});
  const auto end = pl::time::addDays(start, 7'305);
  const std::size_t n = employers.size();

  std::size_t switches = 0;
  std::size_t repeats = 0;
  std::size_t misaligned = 0;
  std::map<int, std::size_t> switchHistogram;
  for (std::size_t p = 1; p <= 20'000; ++p) {
    const auto id = std::to_string(p);
    auto state = init(id, start, employers, salary);

    // The retired uniform pick on the same lane, then the same interval draw.
    auto legacyInit = factory.rng({"employment_init", id});
    (void)legacyInit.choiceIndex(n);
    const auto legacyFirst =
        recur::growth::sampleBackdatedInterval(legacyInit, start,
                                               jobRules.tenure);
    misaligned += (legacyFirst.start != state.start ||
                   legacyFirst.end != state.end)
                      ? 1U
                      : 0U;

    while (state.end < end) {
      const auto switchId = std::to_string(state.switchIndex);
      auto advRng = factory.rng({"employment_advance", id, switchId});
      const auto next = advance(advRng, id, state.end, employers, state);
      repeats += next.employerAcct == state.employerAcct ? 1U : 0U;

      auto legacyAdv = factory.rng({"employment_advance", id, switchId});
      (void)legacyAdv.choiceIndex(n - 1);
      const auto legacyNext = recur::growth::sampleForwardInterval(
          legacyAdv, state.end, jobRules.tenure);
      misaligned += (legacyNext.start != next.start ||
                     legacyNext.end != next.end)
                        ? 1U
                        : 0U;

      state = next;
      ++switches;
    }
    ++switchHistogram[state.switchIndex];
  }
  std::printf("  %zu switches; repeats %zu; intervals off the retired lane "
              "%zu\n",
              switches, repeats, misaligned);
  for (const auto &[count, people] : switchHistogram) {
    std::printf("    %2d switches: %zu people\n", count, people);
  }
  check(switches > 0, "C: no job switch happened, so the checks are empty");
  check(repeats == 0, "C: a job switch re-picked the current employer");
  check(misaligned == 0, "C: a job interval moved off the retired lane, so "
                         "the pick does not spend the draw it replaced");

  // Timing, printed: the exclusion is O(1) in the roster size.
  const auto timePicks = [&](const SizedKeys &pool) {
    auto timing = pl::random::Rng::fromSeed(kSeed);
    std::uint64_t sink = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < 1'000'000; ++i) {
      const auto &current = pool.keys[i % pool.size()];
      sink += recur::growth::pickSizedDifferent(timing, pool, current).number;
    }
    const auto t1 = std::chrono::steady_clock::now();
    PL_CHECK(sink != 0);
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / 1e6;
  };
  auto smallRng = pl::random::Rng::fromSeed(kSeed);
  const auto small =
      employerPool(pl::synth::counterparties::make(smallRng, 2'000));
  auto largeRng = pl::random::Rng::fromSeed(kSeed);
  const auto large =
      employerPool(pl::synth::counterparties::make(largeRng, 500'000));
  std::printf("  pickSizedDifferent: %.1f ns at %zu employers, %.1f ns at %zu "
              "(the retired std::find scanned the roster)\n",
              timePicks(small), small.size(), timePicks(large), large.size());
}

// ------------------------------------------------------------------ D

struct CorpusLeg {
  const char *name;
  std::int32_t days;
};

void corpusLeg(const pl::synth::pii::PoolSet &poolSet, const CorpusLeg &leg) {
  std::printf("\n##### D: corpus, pop 2,000 x %d days #####\n", leg.days);
  const pl::time::Window window{.start = pl::time::makeTime({2025, 1, 1}),
                                .days = leg.days};

  pltest::LegOptions options{};
  options.population = 2'000;
  options.window = window;
  options.seed = kSeed;
  options.withFamily = true;
  options.withBaseRoutines = true;
  const auto result = pltest::runLeg(poolSet, options);

  pltest::WorldSpec spec;
  spec.seed = options.seed;
  spec.window = options.window;
  spec.population = options.population;
  spec.fraudProfile = pltest::scaledFraudProfile();
  spec.withIncome = false;
  const pltest::GateWorld world(poolSet, spec);
  const auto &employers = world.plan.counterparties().employers;
  const auto &landlords = world.plan.counterparties().landlords;
  const auto &typeOf = world.plan.counterparties().landlordTypeOf;
  const auto fallbackEmployer = cps::cash::fallbackEmployer();
  const auto fallbackLandlord = cps::cash::fallbackLandlord();

  const auto isGovernment = [](const Key &key) {
    return std::ranges::find(cps::kGovernment, key) != cps::kGovernment.end();
  };

  std::unordered_map<Key, std::unordered_set<Key>> legitPayees;
  std::set<std::pair<Key, Key>> camoPairs;
  std::unordered_set<Key> rentPayees;
  std::unordered_map<Key, LandlordType> payerLandlordType;
  std::size_t salaryRows = 0, salaryOffPool = 0, salaryOnGovernment = 0;
  std::size_t benefitRows = 0, benefitOffGovernment = 0;
  std::size_t rentRows = 0, rentOffRoster = 0;
  std::size_t camoP2p = 0, camoP2pOnRoles = 0;

  for (const auto &t : result.rows) {
    const auto channel = t.session.channel;
    if (channel == channels::tag(channels::Legit::salary)) {
      ++salaryRows;
      const bool pooled = employers.indexOf(t.source) != SizedPool::npos ||
                          t.source == fallbackEmployer;
      salaryOffPool += pooled ? 0U : 1U;
      salaryOnGovernment += isGovernment(t.source) ? 1U : 0U;
      legitPayees[t.source].insert(t.target);
    } else if (channel == channels::tag(channels::Government::socialSecurity) ||
               channel == channels::tag(channels::Government::disability)) {
      ++benefitRows;
      benefitOffGovernment += isGovernment(t.source) ? 0U : 1U;
    } else if (channels::isRent(channel)) {
      ++rentRows;
      const bool rostered =
          t.target.role == pl::entity::Role::landlord &&
          (landlords.indexOf(t.target) != SizedPool::npos ||
           t.target == fallbackLandlord);
      rentOffRoster += rostered ? 0U : 1U;
      if (const auto it = typeOf.find(t.target); it != typeOf.end()) {
        rentPayees.insert(t.source);
        payerLandlordType[t.source] = it->second;
      }
    } else if (channel == channels::tag(channels::Camouflage::salary)) {
      camoPairs.emplace(t.source, t.target);
    } else if (channel == channels::tag(channels::Camouflage::p2p)) {
      ++camoP2p;
      const auto role = t.target.role;
      camoP2pOnRoles += role == pl::entity::Role::employer ||
                                role == pl::entity::Role::landlord
                            ? 1U
                            : 0U;
    }
  }

  // Headcount = distinct legitimate payees of an employer in this corpus.
  std::size_t maxPayees = 0;
  double payeeSum = 0.0;
  double sizeBiased = 0.0;
  for (const auto &[employer, payees] : legitPayees) {
    (void)employer;
    const auto h = static_cast<double>(payees.size());
    maxPayees = std::max(maxPayees, payees.size());
    payeeSum += h;
    sizeBiased += h * h;
  }
  const double meanPayees =
      legitPayees.empty() ? 0.0 : payeeSum / legitPayees.size();
  const double legitHeadcount = payeeSum == 0.0 ? 0.0 : sizeBiased / payeeSum;
  double camoHeadcount = 0.0;
  for (const auto &[employer, ring] : camoPairs) {
    (void)ring;
    const auto it = legitPayees.find(employer);
    camoHeadcount +=
        it == legitPayees.end() ? 0.0 : static_cast<double>(it->second.size());
  }
  camoHeadcount = camoPairs.empty() ? 0.0 : camoHeadcount / camoPairs.size();
  std::size_t camoOffPool = 0;
  for (const auto &[employer, ring] : camoPairs) {
    (void)ring;
    camoOffPool += employers.indexOf(employer) == SizedPool::npos ? 1U : 0U;
  }
  const double ratio =
      legitHeadcount == 0.0 ? 0.0 : camoHeadcount / legitHeadcount;
  // DISARM: the headcount a uniform pick over the pool would meet.
  const double uniformRatio =
      legitHeadcount == 0.0
          ? 0.0
          : (payeeSum / static_cast<double>(employers.size())) /
                legitHeadcount;

  // Rent-row type mix against the roster's law, within 4 sigma of the payer
  // count (a payer's landlord is fixed within the lease, so rows cluster).
  std::array<double, 3> payerMix{};
  for (const auto &[payer, type] : payerLandlordType) {
    (void)payer;
    payerMix[static_cast<std::size_t>(type)] += 1.0;
  }
  const double payers = static_cast<double>(payerLandlordType.size());
  std::array<double, 3> predicted{};
  for (std::size_t i = 0; i < landlords.size(); ++i) {
    const auto it = typeOf.find(landlords.keys[i]);
    if (it != typeOf.end()) {
      predicted[static_cast<std::size_t>(it->second)] +=
          landlords.law.weight(i);
    }
  }
  std::size_t typeOffLaw = 0;
  for (std::size_t t = 0; t < 3; ++t) {
    payerMix[t] = payers == 0.0 ? 0.0 : payerMix[t] / payers;
    const double se =
        std::sqrt(predicted[t] * (1.0 - predicted[t]) / std::max(1.0, payers));
    typeOffLaw += std::abs(payerMix[t] - predicted[t]) > 4.0 * se ? 1U : 0U;
  }

  // Camouflage salary posts on its employer's own schedule, derived here as
  // legitimate payroll derives it (the run seed's factory, the default
  // rules). The legitimate rows prove the derivation: none may fall off it.
  // Then no camouflage row paid by an employer with legitimate payees may
  // either, which reaches the harness injector's payrollSeed. The same rows
  // scored against fraud-factory schedules show the check has power at
  // this leg's handful of pairs.
  const pl::random::RngFactory payrollFactory{options.seed};
  const pl::random::RngFactory fraudFactory{options.seed ^
                                            0x9E3779B97F4A7C15ULL};
  using DayCache = std::unordered_map<Key, std::set<std::int64_t>>;
  DayCache ownDays;
  DayCache wrongDays;
  const auto daysOf =
      [&](DayCache &cache, const pl::random::RngFactory &schedules,
          const Key &employer) -> const std::set<std::int64_t> & {
    auto [it, fresh] = cache.try_emplace(employer);
    if (fresh) {
      it->second = postingDays(recur::samplePayrollProfile(
                                   recur::PayrollRules{}, schedules, employer),
                               window.start, window.endExcl());
    }
    return it->second;
  };
  std::size_t salaryOffSchedule = 0;
  std::size_t camoScheduled = 0, camoOffSchedule = 0, camoOffWrong = 0;
  for (const auto &t : result.rows) {
    const auto channel = t.session.channel;
    if (channel == channels::tag(channels::Legit::salary)) {
      salaryOffSchedule +=
          onSchedule(daysOf(ownDays, payrollFactory, t.source), t.timestamp)
              ? 0U
              : 1U;
    } else if (channel == channels::tag(channels::Camouflage::salary) &&
               legitPayees.contains(t.source)) {
      ++camoScheduled;
      camoOffSchedule +=
          onSchedule(daysOf(ownDays, payrollFactory, t.source), t.timestamp)
              ? 0U
              : 1U;
      camoOffWrong +=
          onSchedule(daysOf(wrongDays, fraudFactory, t.source), t.timestamp)
              ? 0U
              : 1U;
    }
  }

  // Registered but never paid.
  std::unordered_set<Key> paidLandlords;
  for (const auto &t : result.rows) {
    if (channels::isRent(t.session.channel)) {
      paidLandlords.insert(t.target);
    }
  }
  const double unpaidEmployers =
      1.0 - static_cast<double>(legitPayees.size()) /
                static_cast<double>(employers.size());
  const double unpaidLandlords =
      1.0 - static_cast<double>(paidLandlords.size()) /
                static_cast<double>(landlords.size());

  // The camouflage P2P pool: every employer and landlord record is outside
  // it, however large the rosters grew.
  const auto &records = world.holdings.accounts.registry.records;
  std::size_t roleRecords = 0;
  std::size_t roleAdmitted = 0;
  for (const auto &record : records) {
    const auto role = record.id.role;
    if (role == pl::entity::Role::employer ||
        role == pl::entity::Role::landlord) {
      ++roleRecords;
      roleAdmitted += pl::transfers::fraud::camouflageEligible(record.id);
    }
  }

  std::printf("  rows %zu; salary %zu (off pool %zu, on a benefit payer %zu); "
              "benefits %zu (off kGovernment %zu); rent %zu (off roster %zu)\n",
              result.rows.size(), salaryRows, salaryOffPool,
              salaryOnGovernment, benefitRows, benefitOffGovernment, rentRows,
              rentOffRoster);
  std::printf("  employers %zu, paid %zu (never paid %.4f); max payees %zu, "
              "mean %.3f, ratio %.2f; landlords %zu, paid %zu (never paid "
              "%.4f)\n",
              employers.size(), legitPayees.size(), unpaidEmployers, maxPayees,
              meanPayees, meanPayees == 0.0 ? 0.0 : maxPayees / meanPayees,
              landlords.size(), paidLandlords.size(), unpaidLandlords);
  std::printf("  rent payers %zu, landlord type mix %.4f / %.4f / %.4f "
              "against the roster's %.4f / %.4f / %.4f\n",
              payerLandlordType.size(), payerMix[0], payerMix[1], payerMix[2],
              predicted[0], predicted[1], predicted[2]);
  // Printed, not banded: at pop 2,000 the roster holds more employers than
  // workers, so a uniform pick already scores about 0.3, and the leg has
  // about 20 camouflage salary pairs. Sub-gate B bands it at scale.
  std::printf("  camouflage salary pairs %zu (off the pool %zu): mean "
              "legitimate headcount %.3f against %.3f for legitimate payees, "
              "ratio %.3f (uniform-pick counterfactual %.4f)\n",
              camoPairs.size(), camoOffPool, camoHeadcount, legitHeadcount,
              ratio, uniformRatio);
  std::printf("  salary rows off their employer's derived schedule %zu; "
              "camouflage salary rows with a legitimate co-payee %zu, off "
              "the employer's schedule %zu (against fraud-factory schedules "
              "%zu)\n",
              salaryOffSchedule, camoScheduled, camoOffSchedule, camoOffWrong);
  std::printf("  camouflage P2P rows %zu, on an employer or landlord %zu; "
              "employer and landlord records %zu of %zu (%.4f), admitted to "
              "the P2P pool %zu\n",
              camoP2p, camoP2pOnRoles, roleRecords, records.size(),
              static_cast<double>(roleRecords) /
                  static_cast<double>(records.size()),
              roleAdmitted);

  const std::string at = std::string{" ("} + leg.name + ")";
  check(salaryRows > 0 && benefitRows > 0 && rentRows > 0,
        "D: a salary, benefit or rent row is missing, so the checks below "
        "would pass on no data" + at);
  check(salaryOffPool == 0, "D: a salary row's source is not in the employer "
                            "pool" + at);
  check(salaryOnGovernment == 0,
        "D: a salary row is paid by the SSA or disability key" + at);
  check(benefitOffGovernment == 0,
        "D: a benefit row's source is not a kGovernment payer" + at);
  check(rentOffRoster == 0,
        "D: a rent row pays something other than a rostered landlord" + at);
  check(typeOffLaw == 0,
        "D: the rent payers' landlord type mix is off the roster's law by "
        "more than 4 sigma" + at);
  check(meanPayees > 0.0 && maxPayees / meanPayees >= 5.0,
        "D: the busiest employer is not five times the mean, so payroll "
        "reads as uniform" + at);
  check(!camoPairs.empty() && camoOffPool == 0,
        "D: a camouflage salary row is paid by something outside the "
        "employer pool (or there are none to check)" + at);
  check(salaryOffSchedule == 0,
        "D: a legitimate salary row is off its employer's derived schedule, "
        "so the camouflage schedule check below reads the wrong schedule" +
            at);
  check(camoScheduled > 0 && camoOffSchedule == 0,
        "D: a camouflage salary row posts off its employer's own payroll "
        "schedule (or none has a legitimate co-payee to check)" + at);
  check(camoOffWrong > 0,
        "D: every camouflage salary row also sits on schedules from the "
        "fraud factory, so the schedule check has no power here" + at);
  check(camoP2p > 0, "D: no camouflage P2P rows, so the destination check "
                     "would pass on no data" + at);
  check(camoP2pOnRoles == 0,
        "D: a camouflage P2P row pays an employer or a landlord, a "
        "destination legitimate P2P never pays" + at);
  check(roleRecords > 0 && roleAdmitted == 0,
        "D: an employer or landlord record is admitted to the camouflage "
        "P2P pool" + at);
  // What the filter is holding back: an unfiltered registry pick would put
  // this share of camouflage P2P rows on an employer or a landlord.
  check(static_cast<double>(roleRecords) /
                static_cast<double>(records.size()) >
            0.05,
        "D: employers and landlords are under 5% of the registry, so the P2P "
        "exclusion has nothing to hold back" + at);
}

void gateD(const pl::synth::pii::PoolSet &poolSet) {
  corpusLeg(poolSet, {"run-golden 60 days", 60});
  corpusLeg(poolSet, {"365 days", 365});
}

// ------------------------------------------------------------------ E

void gateE() {
  std::printf("\n##### E: memory at pop 500,000 #####\n");
  constexpr int kPop = 500'000;
  auto rng = pl::random::Rng::fromSeed(kSeed);
  const auto pack = entityStage::buildLandlords(rng, kPop, kSeed);
  const auto directory = entityStage::buildCounterparties(rng, kPop);

  const auto employers = employerPool(directory);
  const auto landlords = landlordPool(pack);
  const std::size_t legacyEmployers = 1'250;
  const std::size_t legacyLandlords = 600;
  const std::size_t added =
      employers.size() + landlords.size() - legacyEmployers - legacyLandlords;

  namespace fp = pl::pipeline::diagnostics::footprint;
  const std::size_t registry =
      added * sizeof(pl::entity::account::Record) +
      fp::hashMapBytesFromSize(added,
                               sizeof(std::pair<const Key, std::uint32_t>));
  const std::size_t typeMap = fp::hashMapBytesFromSize(
      landlords.size(), sizeof(std::pair<const Key, LandlordType>));
  const std::size_t directoryBytes = fp::directoryBytes(directory);
  const std::size_t landlordBytes = fp::landlordsBytes(pack);
  // The blueprint's two pools, and the fold's employer copy.
  const std::size_t access =
      employers.heapBytes() + landlords.heapBytes() + typeMap +
      employers.heapBytes();
  const std::size_t total = registry + directoryBytes + landlordBytes + access;

  const auto mb = [](std::size_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
  };
  std::printf("  employers %zu (was %zu), landlords %zu (was %zu); registry "
              "grows by %zu records\n",
              employers.size(), legacyEmployers, landlords.size(),
              legacyLandlords, added);
  std::printf("  resident: registry + lookup %.1f MB, directory %.1f MB, "
              "landlord pack %.1f MB, blueprint and fold copies %.1f MB; "
              "total %.1f MB (pools alone %.1f KB)\n",
              mb(registry), mb(directoryBytes), mb(landlordBytes), mb(access),
              mb(total),
              static_cast<double>(directory.employers.pool.heapBytes() +
                                  pack.roster.pool.heapBytes()) /
                  1024.0);
  check(employers.law.heapBytes() + landlords.law.heapBytes() < 16 * 1024,
        "E: the size laws store more than their class structure");
  check(mb(total) < 128.0,
        "E: the new rosters cost more than 128 MB resident at pop 500,000");
}

} // namespace

int main() {
  std::printf("=== Employer and landlord sizes ===\n");
  const auto poolSet = pltest::buildPoolSet(kSeed);
  gateA(poolSet);
  gateB();
  gateC();
  gateD(poolSet);
  gateE();

  if (g_failures > 0) {
    std::printf("\ntest_counterparty_sizes: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("\nAll employer and landlord size checks passed.\n");
  return 0;
}
