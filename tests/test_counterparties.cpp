#include "phantomledger/activity/income/rent.hpp"
#include "phantomledger/activity/income/salary.hpp"
#include "phantomledger/activity/recurring/growth.hpp"
#include "phantomledger/encoding/external.hpp"
#include "phantomledger/entities/counterparties/cash_points.hpp"
#include "phantomledger/entities/counterparties/institutional_accounts.hpp"
#include "phantomledger/entities/counterparties/providers.hpp"
#include "phantomledger/entities/counterparties/remote_payees.hpp"
#include "phantomledger/entities/counterparties/sized_pool.hpp"
#include "phantomledger/entities/holdings/general_ledger.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/counterparties/make.hpp"
#include "phantomledger/synth/counterparties/size_law.hpp"
#include "phantomledger/synth/landlords/make.hpp"
#include "phantomledger/synth/merchants/lifecycle.hpp"
#include "phantomledger/synth/merchants/make.hpp"
#include "phantomledger/synth/merchants/outlets.hpp"
#include "phantomledger/synth/merchants/place.hpp"
#include "phantomledger/synth/products/providers.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <unordered_set>
#include <vector>

using namespace PhantomLedger;

namespace {

namespace providers = counterparties::providers;
using counterparties::Market;

constexpr std::uint64_t kInstitutionalBaseSerial = 1'000'000'000ULL;

constexpr entity::Key institutional(std::uint64_t offset) noexcept {
  return entity::makeKey(entity::Role::business, entity::Bank::external,
                         kInstitutionalBaseSerial + offset);
}

constexpr entity::Key governmentEmployer(std::uint64_t number) noexcept {
  return entity::makeKey(entity::Role::employer, entity::Bank::external,
                         number);
}

// The six population-wide lender and insurer keys retired by
// institutional-providers-2026-09. Nothing may mint them again.
constexpr std::uint64_t kRetiredOffsets[] = {1, 2, 3, 5, 6, 7};

void testKeyValues() {
  PL_CHECK_EQ(counterparties::key(counterparties::Government::ssa),
              governmentEmployer(9'000'001ULL));
  PL_CHECK_EQ(counterparties::key(counterparties::Government::disability),
              governmentEmployer(9'000'002ULL));
  PL_CHECK_EQ(counterparties::key(counterparties::Tax::irsTreasury),
              institutional(4));

  std::printf("  PASS: key values\n");
}

void testAllAreExternal() {
  for (const auto &account : counterparties::kAll) {
    PL_CHECK(encoding::isExternal(account));
  }
  std::printf("  PASS: all keys are external\n");
}

void testAllUnique() {
  std::set<entity::Key> unique;
  for (const auto &account : counterparties::kAll) {
    unique.insert(account);
  }

  PL_CHECK_EQ(unique.size(), counterparties::kAll.size());
  std::printf("  PASS: all keys are unique\n");
}

void testGroupSizes() {
  PL_CHECK_EQ(counterparties::kGovernment.size(), 2U);
  PL_CHECK_EQ(counterparties::kTax.size(), 1U);
  PL_CHECK_EQ(counterparties::kAll.size(), 3U);

  PL_CHECK_EQ(counterparties::kAll.size(),
              counterparties::kGovernment.size() + counterparties::kTax.size());

  std::printf("  PASS: group sizes\n");
}

void testGroupContents() {
  PL_CHECK_EQ(counterparties::kAll[0],
              counterparties::key(counterparties::Government::ssa));
  PL_CHECK_EQ(counterparties::kAll[1],
              counterparties::key(counterparties::Government::disability));
  PL_CHECK_EQ(counterparties::kAll[2],
              counterparties::key(counterparties::Tax::irsTreasury));

  std::printf("  PASS: group contents\n");
}

void testRoleConventions() {
  // Government counterparties are modeled as external employers so that
  // SSA / disability deposits show up as payroll-shaped inbound flows.
  for (const auto &account : counterparties::kGovernment) {
    PL_CHECK(account.role == entity::Role::employer);
    PL_CHECK(account.bank == entity::Bank::external);
  }

  // The tax counterparty is an external business.
  for (const auto &account : counterparties::kTax) {
    PL_CHECK(account.role == entity::Role::business);
    PL_CHECK(account.bank == entity::Bank::external);
  }

  std::printf("  PASS: role conventions\n");
}

void testIsHelper() {
  const auto ssa = counterparties::key(counterparties::Government::ssa);
  const auto disability =
      counterparties::key(counterparties::Government::disability);

  PL_CHECK(counterparties::is(ssa, counterparties::Government::ssa));
  PL_CHECK(!counterparties::is(ssa, counterparties::Government::disability));
  PL_CHECK(
      counterparties::is(disability, counterparties::Government::disability));

  std::printf("  PASS: is helper\n");
}

// The retired offsets must stay retired: no singleton and no provider key
// may reuse one, or an export written before the round would misread.
void testRetiredOffsets() {
  for (const auto offset : kRetiredOffsets) {
    const auto retired = institutional(offset);
    for (const auto &account : counterparties::kAll) {
      PL_CHECK(account != retired);
    }
    PL_CHECK(!providers::isProvider(retired));
  }
  std::printf("  PASS: retired institutional offsets are not reused\n");
}

// Every provider key of every production pool, plus each block's last
// ordinal: unique across markets, external business, and mapped back to
// its own market.
void testProviderKeyLayout() {
  const auto &markets = synth::products::ProviderMarkets::production();
  std::unordered_set<std::uint64_t> serials;
  std::size_t keys = 0;

  for (const auto market : counterparties::kMarkets) {
    const auto pool = markets.poolSize(market);
    PL_CHECK(pool >= 1U && pool <= providers::kMaxOrdinal);
    for (std::uint32_t ordinal = 1; ordinal <= pool; ++ordinal) {
      const auto account = providers::key(market, ordinal);
      PL_CHECK(account.role == entity::Role::business);
      PL_CHECK(account.bank == entity::Bank::external);
      PL_CHECK(encoding::isExternal(account));
      PL_CHECK(providers::marketOf(account) == market);
      PL_CHECK(providers::isProvider(account));
      PL_CHECK(account.number >= 1'000'100'001ULL &&
               account.number <= 1'000'699'999ULL);
      PL_CHECK(serials.insert(account.number).second);
      ++keys;
    }
    const auto last = providers::key(market, providers::kMaxOrdinal);
    PL_CHECK(providers::marketOf(last) == market);
    PL_CHECK(serials.insert(last.number).second);
  }

  std::printf("  PASS: %zu provider keys unique, typed and market-mapped\n",
              keys);
}

void testNonProviders() {
  namespace cash = counterparties::cash;

  PL_CHECK(!providers::isProvider(
      counterparties::key(counterparties::Tax::irsTreasury)));
  PL_CHECK(!providers::isProvider(
      counterparties::key(counterparties::Government::ssa)));
  PL_CHECK(!providers::isProvider(
      counterparties::key(counterparties::Government::disability)));
  PL_CHECK(!providers::isProvider(cash::biller(1)));
  for (const auto gl : entity::gl::kIncomeAccounts) {
    PL_CHECK(!providers::isProvider(gl));
  }
  PL_CHECK(!providers::isProvider(counterparties::retiredExternalUnknown()));
  PL_CHECK(!providers::isProvider(counterparties::remote::checkBank(1)));
  PL_CHECK(!providers::isProvider(counterparties::remote::checkBank(
      counterparties::remote::kMaxCheckBankRank)));
  PL_CHECK(!providers::isProvider(counterparties::remote::p2pPlatform(
      counterparties::remote::P2pPlatform::venmo)));
  // The population-scaled business pools (owner businesses) start at 1.
  for (std::uint64_t serial = 1; serial <= 20'000; ++serial) {
    PL_CHECK(!providers::isProvider(entity::makeKey(
        entity::Role::business, entity::Bank::external, serial)));
  }
  // A provider serial under any other role or bank is not a provider.
  const auto serial = providers::key(Market::mortgage, 1).number;
  PL_CHECK(!providers::isProvider(
      entity::makeKey(entity::Role::merchant, entity::Bank::external, serial)));
  PL_CHECK(!providers::isProvider(
      entity::makeKey(entity::Role::business, entity::Bank::internal, serial)));

  std::printf("  PASS: no other institutional or pooled key is a provider\n");
}

// merchant-churn-2026-07 rule 6: the catch-all used to BE catalogue merchant
// 1. Its retired key, and the funeral homes that took over its funeral flow
// (unknown-counterparty-2026-09, also Role::merchant), must stay clear of
// every catalogue serial, including the churn births of the longest window
// the owner runs (20 years) at the target population.
void testRetiredAndFuneralKeysAreReserved() {
  namespace remote = counterparties::remote;
  const auto retired = counterparties::retiredExternalUnknown();
  PL_CHECK(retired != entity::makeKey(entity::Role::merchant,
                                      entity::Bank::external, 1ULL));
  PL_CHECK(retired.role == entity::Role::merchant);
  PL_CHECK(retired.bank == entity::Bank::external);
  PL_CHECK(!remote::isRemotePayee(retired));

  // The three pools answer only for their own keys.
  PL_CHECK(remote::checkBankRank(remote::checkBank(1000)) == 1000);
  PL_CHECK(!remote::isCheckBank(counterparties::key(
      counterparties::Tax::irsTreasury)));
  PL_CHECK(!remote::isP2pPlatform(entity::makeKey(
      entity::Role::platform, entity::Bank::external, 1ULL)));
  PL_CHECK(remote::isFuneralHome(remote::funeralHome(0, 1)));
  PL_CHECK(!remote::isFuneralHome(entity::makeKey(
      entity::Role::merchant, entity::Bank::external,
      remote::kFuneralHomeBaseSerial + remote::kFuneralHomeAreaStride)));

  auto rng = random::Rng::fromSeed(0xC0FFEEULL);
  auto catalog = synth::merchants::makeCatalog(rng, 500'000);
  const time::Window window{.start = time::makeTime({2005, 1, 1}),
                            .days = 7'305};
  // The buildMerchants order: chain outlets take serials before churn does.
  synth::merchants::placeGeography(catalog, 0xC0FFEEULL);
  synth::merchants::expandOutlets(
      catalog, synth::merchants::coreCountFor(500'000, {}), 0xC0FFEEULL);
  synth::merchants::appendChurnReplacements(catalog, window, 0xC0FFEEULL,
                                            synth::merchants::GenerationPlan{});

  std::uint64_t maxSerial = 0;
  for (const auto &record : catalog.records) {
    PL_CHECK(record.counterpartyId != retired);
    PL_CHECK(!remote::isFuneralHome(record.counterpartyId));
    maxSerial = std::max(maxSerial, record.counterpartyId.number);
  }
  PL_CHECK(maxSerial < counterparties::kRetiredExternalUnknownSerial);
  PL_CHECK(maxSerial < remote::kFuneralHomeBaseSerial);

  std::printf("  PASS: retired catch-all and funeral-home keys clear of %zu "
              "catalogue records (max serial %llu)\n",
              catalog.records.size(),
              static_cast<unsigned long long>(maxSerial));
}


// ---------------------------------------------------------------------------
// counterparty-sizes-2026-09: the employer and landlord size laws. All pure
// and draw-free; the samplers and the corpus are test_counterparty_sizes.
// ---------------------------------------------------------------------------

namespace sizes = synth::counterparties::sizes;
using entity::counterparty::SizedKeys;
using entity::counterparty::SizedPool;

[[nodiscard]] bool near(double got, double want, double tol) {
  return std::abs(got - want) <= tol;
}

// (a) SUSB 2022 and QCEW 2022 reproduce their published totals.
void testSusbTable() {
  std::uint64_t firms = sizes::kSusb2022Top.members;
  double employment = sizes::kSusb2022Top.volume;
  for (const auto &row : sizes::kSusb2022Uniform) {
    firms += row.members;
    employment += row.volume;
  }
  PL_CHECK_EQ(firms, sizes::kSusb2022Firms);
  PL_CHECK_EQ(employment, sizes::kSusb2022Employment);

  const double under20 = (sizes::kSusb2022Uniform[0].volume +
                          sizes::kSusb2022Uniform[1].volume +
                          sizes::kSusb2022Uniform[2].volume) /
                         employment;
  const double over10k =
      (sizes::kSusb2022Uniform[12].volume + sizes::kSusb2022Top.volume) /
      employment;
  PL_CHECK(near(under20, 0.16170, 1e-5));
  PL_CHECK(near(over10k, 0.30589, 1e-5));
  PL_CHECK(sizes::kGovernmentShare == 21.0 / 150.0);

  double mass = 0.0;
  for (const auto &cls : sizes::employerClasses()) {
    mass += cls.mass;
  }
  PL_CHECK(near(mass, 1.0, 1e-12));
  const double federal = sizes::employerClasses()[sizes::kFederalClass].mass;
  PL_CHECK(near(federal, 2.9 / 150.0, 1e-12));

  std::printf("  PASS: SUSB 2022 %llu firms / %.0f jobs; <20 %.5f, 10,000+ "
              "%.5f; government %.4f; federal payer %.5f\n",
              static_cast<unsigned long long>(firms), employment, under20,
              over10k, sizes::kGovernmentShare, federal);
}

// (b) RHFS 2021 (CRS R47332 Tables 1 and 3) reproduces the report's own
// totals and summary sentences, and the PL type mix derives from it.
void testRhfsTable() {
  using Type = entity::landlord::Type;
  constexpr auto ind = static_cast<std::size_t>(Type::individual);
  constexpr auto llc = static_cast<std::size_t>(Type::llcSmall);
  constexpr auto corp = static_cast<std::size_t>(Type::corporate);

  double units = 0.0;
  double properties = 0.0;
  for (std::size_t c = 0; c < sizes::kRhfsSizeColumns; ++c) {
    units += sizes::kRhfs2021Units[c];
    properties += sizes::kRhfs2021Properties[c];
    double cells = 0.0;
    for (const auto &owner : sizes::kRhfs2021UnitsByOwner) {
      cells += owner[c];
    }
    // Table 3 is rounded to thousands per cell.
    PL_CHECK(near(cells, sizes::kRhfs2021Units[c], 2.0));
  }
  PL_CHECK(near(units, sizes::kRhfs2021AllUnits, 1.0));
  PL_CHECK(near(properties, sizes::kRhfs2021AllProperties, 2.0));

  const auto &byOwner = sizes::kRhfs2021UnitsByOwner;
  const auto rowSum = [&](sizes::RhfsOwner owner) {
    double sum = 0.0;
    for (const double v : byOwner[static_cast<std::size_t>(owner)]) {
      sum += v;
    }
    return sum;
  };
  const auto &individual =
      byOwner[static_cast<std::size_t>(sizes::RhfsOwner::individualInvestor)];
  const auto &partnership =
      byOwner[static_cast<std::size_t>(sizes::RhfsOwner::llpLpLlc)];
  const double singleUnitProps =
      sizes::kRhfs2021Properties[0] / sizes::kRhfs2021AllProperties;
  const double fiftyPlus =
      (sizes::kRhfs2021Units[4] + sizes::kRhfs2021Units[5] +
       sizes::kRhfs2021Units[6]) /
      sizes::kRhfs2021AllUnits;
  PL_CHECK(near(singleUnitProps, 0.856, 0.001));
  PL_CHECK(near(fiftyPlus, 0.378, 0.001));
  // The report's summary sentences.
  PL_CHECK(near(rowSum(sizes::RhfsOwner::individualInvestor) / units, 0.376,
                0.001));
  PL_CHECK(near(rowSum(sizes::RhfsOwner::llpLpLlc) / units, 0.404, 0.001));
  PL_CHECK(near((individual[0] + individual[1]) /
                    (sizes::kRhfs2021Units[0] + sizes::kRhfs2021Units[1]),
                0.702, 0.001));
  PL_CHECK(near((partnership[5] + partnership[6]) /
                    (sizes::kRhfs2021Units[5] + sizes::kRhfs2021Units[6]),
                0.678, 0.001));

  // The derived type mix over reported units.
  sizes::TypeShares reported{};
  for (const auto &column : sizes::rhfsTypeUnits()) {
    for (std::size_t t = 0; t < reported.size(); ++t) {
      reported[t] += column[t];
    }
  }
  const double total = reported[ind] + reported[llc] + reported[corp];
  PL_CHECK(near(reported[ind] / total, 0.447, 0.001));
  PL_CHECK(near(reported[llc] / total, 0.141, 0.001));
  PL_CHECK(near(reported[corp] / total, 0.412, 0.001));

  // The pool weights each column by ALL its units, so a column's
  // not-reported units take that column's mix. Not-reported units sit mostly
  // in large, corporate-held properties, so this renter-weighted mix runs
  // below the reported-only aggregate on individuals.
  const auto columns = sizes::rhfsTypeUnits();
  sizes::TypeShares imputed{};
  for (std::size_t c = 0; c < sizes::kRhfsSizeColumns; ++c) {
    const double reportedHere = columns[c][ind] + columns[c][llc] +
                                columns[c][corp];
    for (std::size_t t = 0; t < imputed.size(); ++t) {
      imputed[t] += sizes::kRhfs2021Units[c] / units * columns[c][t] /
                    reportedHere;
    }
  }
  PL_CHECK(near(imputed[ind], 0.433, 0.001));
  PL_CHECK(near(imputed[llc], 0.138, 0.001));
  PL_CHECK(near(imputed[corp], 0.429, 0.001));

  // Carving the Top-50 owners out of the 150+ column moves corporate units
  // between classes and nothing else, so the pool's mix is exactly that.
  const auto classes = sizes::landlordClasses();
  const auto shares = sizes::landlordTypeShares();
  sizes::TypeShares pooled{};
  double classMass = 0.0;
  for (std::size_t c = 0; c < classes.size(); ++c) {
    classMass += classes[c].mass;
    double sum = 0.0;
    for (std::size_t t = 0; t < pooled.size(); ++t) {
      pooled[t] += classes[c].mass * shares[c][t];
      sum += shares[c][t];
    }
    PL_CHECK(near(sum, 1.0, 1e-12));
  }
  PL_CHECK(near(classMass, 1.0, 1e-12));
  for (std::size_t t = 0; t < pooled.size(); ++t) {
    PL_CHECK(near(pooled[t], imputed[t], 1e-9));
  }
  PL_CHECK(shares[sizes::kLandlordTailClass][corp] == 1.0);

  // Correction (b) of the research verification: at most 55% of 5-49 unit
  // tenants route to a professionally managed (corporate, portal) payee.
  const double midCorporate =
      (classes[2].mass * shares[2][corp] + classes[3].mass * shares[3][corp]) /
      (classes[2].mass + classes[3].mass);
  PL_CHECK(midCorporate <= 0.55);

  std::printf("  PASS: RHFS 2021 %.0fk units / %.0fk properties; single-unit "
              "properties %.4f, 50+ units %.4f; reported-unit type mix "
              "%.4f / %.4f / %.4f, pool (renter-weighted) %.4f / %.4f / %.4f; "
              "5-49 unit corporate share %.4f (<= 0.55)\n",
              units, properties, singleUnitProps, fiftyPlus,
              reported[ind] / total, reported[llc] / total,
              reported[corp] / total, pooled[ind], pooled[llc], pooled[corp],
              midCorporate);
}

// (c) The roster counts, exactly. Tied payer shares first: the rosters are
// sized against the payers the generator actually draws.
void testRosterCounts() {
  PL_CHECK(synth::counterparties::EmployerSizing{}.workerShare ==
           activity::income::salary::Rules{}.paidFraction);
  PL_CHECK(synth::landlords::GenerationPlan{}.renterShare ==
           activity::income::rent::Rules{}.paidFraction);

  struct Leg {
    int population;
    std::size_t employers;
    std::size_t landlords;
  };
  constexpr std::array<Leg, 6> kLegs{{
      {0, 17, 8},
      {300, 218, 106},
      {2'000, 1'453, 700},
      {10'000, 6'068, 3'380},
      {200'000, 89'231, 66'658},
      {500'000, 200'556, 155'581},
  }};
  for (const auto &leg : kLegs) {
    const auto employers = sizes::employerLaw(leg.population, 0.74);
    const auto landlords = sizes::landlordLaw(leg.population, 0.35);
    std::printf("    pop %7d: %7zu employers, %7zu landlords\n",
                leg.population, employers.total(), landlords.total());
    PL_CHECK_EQ(employers.total(), leg.employers);
    PL_CHECK_EQ(landlords.total(), leg.landlords);
    PL_CHECK_EQ(employers.pool().size(), leg.employers);
    PL_CHECK_EQ(landlords.pool().size(), leg.landlords);
  }
  std::printf("  PASS: roster counts\n");
}

// (d) The two rank-size tails sum to their rows.
void testRankTails() {
  const double b = sizes::employerTailExponent();
  const double alpha = 1.0 / b;
  double sum = 0.0;
  for (std::uint64_t r = 1; r <= sizes::kSusb2022Top.members; ++r) {
    sum += sizes::kSusb2022TopFloor *
           std::pow(546.0 / static_cast<double>(r), b);
  }
  const double rank1 = sizes::kSusb2022TopFloor * std::pow(546.0, b);
  PL_CHECK(near(sum / sizes::kSusb2022Top.volume, 1.0, 1e-9));
  PL_CHECK(alpha >= 1.35 && alpha <= 1.43);
  // UNFITTED CHECK: the tail is fitted to the row sum and its floor only; the
  // rank-1 size is a consequence, set against Walmart's roughly 1.6M US
  // associates (recalled, not verified).
  PL_CHECK(rank1 >= 1.2e6 && rank1 <= 2.4e6);

  const double beta = sizes::landlordTailExponent();
  double top50 = 0.0;
  for (std::uint64_t r = 1; r <= sizes::kNmhc2024Owners; ++r) {
    top50 += sizes::kNmhc2024Rank1Units *
             std::pow(1.0 / static_cast<double>(r), beta);
  }
  PL_CHECK(near(top50 / sizes::kNmhc2024Units, 1.0, 1e-9));
  PL_CHECK(beta > 0.25 && beta < 0.32);

  std::printf("  PASS: employer tail alpha %.4f, rank 1 %.0f employees "
              "(unfitted check); Top-50 beta %.4f, rank 50 %.1fk units\n",
              alpha, rank1, beta,
              sizes::kNmhc2024Rank1Units * std::pow(50.0, -beta));
}

// (e) Serials: contiguous 1..N, O(1) lookup, and never a government key.
void testRosterSerials() {
  auto rng = random::Rng::fromSeed(0x5E1A1ULL);
  const auto directory = synth::counterparties::make(rng, 2'000);
  const auto &keys = directory.employers.accounts.external;
  PL_CHECK(directory.employers.accounts.internal.empty());
  PL_CHECK(directory.employers.accounts.all == keys);
  PL_CHECK_EQ(directory.employers.pool.size(), keys.size());

  const SizedKeys employers{.keys = keys, .law = directory.employers.pool};
  for (std::size_t i = 0; i < keys.size(); ++i) {
    PL_CHECK(keys[i].role == entity::Role::employer);
    PL_CHECK(keys[i].bank == entity::Bank::external);
    PL_CHECK_EQ(keys[i].number, static_cast<std::uint64_t>(i + 1));
    PL_CHECK_EQ(employers.indexOf(keys[i]), i);
  }
  for (const auto &government : counterparties::kGovernment) {
    PL_CHECK_EQ(employers.indexOf(government), SizedPool::npos);
  }
  PL_CHECK_EQ(employers.indexOf(counterparties::cash::fallbackEmployer()),
              SizedPool::npos);

  auto landlordRng = random::Rng::fromSeed(0x5E1A1ULL);
  const auto pack = synth::landlords::makePack(landlordRng, 2'000, 0x5E1A1ULL);
  const auto &records = pack.roster.records;
  PL_CHECK_EQ(pack.roster.pool.size(), records.size());
  SizedKeys landlords{.keys = {}, .law = pack.roster.pool};
  for (const auto &record : records) {
    landlords.keys.push_back(record.accountId);
  }
  for (std::size_t i = 0; i < records.size(); ++i) {
    PL_CHECK(records[i].accountId.role == entity::Role::landlord);
    PL_CHECK_EQ(records[i].accountId.number, static_cast<std::uint64_t>(i + 1));
    PL_CHECK_EQ(landlords.indexOf(records[i].accountId), i);
  }
  PL_CHECK_EQ(pack.internals.size() + pack.externals.size(), records.size());

  const auto single =
      SizedKeys::single(counterparties::cash::fallbackLandlord());
  PL_CHECK_EQ(single.indexOf(counterparties::cash::fallbackLandlord()), 0U);

  std::printf("  PASS: %zu employer and %zu landlord serials contiguous from "
              "1, O(1) lookup, no government key\n",
              keys.size(), records.size());
}

// (f) The pool reproduces its weights, and the exclusion renormalises over
// the complement exactly.
void testSizedPoolWeights() {
  const auto pool = sizes::employerLaw(2'000, 0.74).pool();
  const std::size_t n = pool.size();

  double mass = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    mass += pool.weight(i);
  }
  PL_CHECK(near(mass, 1.0, 1e-9));

  constexpr std::size_t kGrid = 1'000'000;
  std::vector<std::uint32_t> counts(n, 0);
  for (std::size_t g = 0; g < kGrid; ++g) {
    const double u =
        (static_cast<double>(g) + 0.5) / static_cast<double>(kGrid);
    ++counts[pool.pick(u)];
  }
  double worst = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    worst = std::max(worst, std::abs(static_cast<double>(counts[i]) -
                                     pool.weight(i) * kGrid));
  }
  PL_CHECK(worst <= 1.0 + 1e-6);

  // Exclude the federal employer, the heaviest single member.
  const auto classes = sizes::employerLaw(2'000, 0.74).classes;
  std::size_t federal = 0;
  for (std::size_t c = 0; c < sizes::kFederalClass; ++c) {
    federal += classes[c].members;
  }
  const double wx = pool.weight(federal);
  std::vector<std::uint32_t> excluded(n, 0);
  for (std::size_t g = 0; g < kGrid; ++g) {
    const double u =
        (static_cast<double>(g) + 0.5) / static_cast<double>(kGrid);
    const auto got = pool.pickExcluding(u, federal);
    PL_CHECK(got != federal);
    ++excluded[got];
  }
  double worstZ = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    if (i == federal) {
      continue;
    }
    const double p = pool.weight(i) / (1.0 - wx);
    const double se = std::sqrt(p * (1.0 - p) / kGrid);
    worstZ = std::max(worstZ,
                      std::abs(static_cast<double>(excluded[i]) / kGrid - p) /
                          se);
  }
  PL_CHECK(worstZ <= 4.0);

  // Seeded uniforms too, not only the grid.
  auto rng = random::Rng::fromSeed(0xFEDULL);
  for (int i = 0; i < 200'000; ++i) {
    PL_CHECK(pool.pickExcluding(rng.nextDouble(), federal) != federal);
  }

  std::printf("  PASS: %zu-member pool, worst grid deviation %.3f picks; "
              "federal excluded (weight %.5f), worst renormalised z %.2f\n",
              n, worst, wx, worstZ);
}

// (g) The draw-count contract, against the uniform choiceIndex it replaced.
void testPickDrawContract() {
  namespace growth = activity::recurring::growth;
  const auto same = [](const random::Rng &a, const random::Rng &b) {
    return a.engine().state_hi() == b.engine().state_hi() &&
           a.engine().state_lo() == b.engine().state_lo();
  };

  const auto law = sizes::employerLaw(300, 0.74);
  SizedKeys wide{.keys = {}, .law = law.pool()};
  for (std::size_t i = 0; i < law.total(); ++i) {
    wide.keys.push_back(entity::makeKey(entity::Role::employer,
                                        entity::Bank::external, i + 1));
  }
  const auto one = SizedKeys::single(counterparties::cash::fallbackEmployer());
  const std::array<SizedPool::Class, 2> pairClasses{{{1, 0.7}, {1, 0.3}}};
  const SizedKeys two{
      .keys = {entity::makeKey(entity::Role::employer, entity::Bank::external,
                               1),
               entity::makeKey(entity::Role::employer, entity::Bank::external,
                               2)},
      .law = SizedPool::build(pairClasses, SizedPool::npos, {})};

  for (std::uint64_t seed = 1; seed <= 64; ++seed) {
    auto rng = random::Rng::fromSeed(seed);
    auto twin = rng;

    (void)growth::pickSized(rng, wide);
    (void)twin.choiceIndex(wide.size());
    PL_CHECK(same(rng, twin));

    (void)growth::pickSized(rng, one);
    (void)twin.choiceIndex(1);
    PL_CHECK(same(rng, twin));

    const auto &current = wide.keys[seed % wide.size()];
    const auto next = growth::pickSizedDifferent(rng, wide, current);
    PL_CHECK(next != current);
    (void)twin.choiceIndex(wide.size() - 1);
    PL_CHECK(same(rng, twin));

    // Not in the pool: a plain pick, one draw.
    (void)growth::pickSizedDifferent(rng, wide,
                                     counterparties::cash::fallbackEmployer());
    (void)twin.choiceIndex(wide.size());
    PL_CHECK(same(rng, twin));

    // A one-member complement and a one-member pool: no draw.
    PL_CHECK(growth::pickSizedDifferent(rng, two, two.keys[0]) == two.keys[1]);
    PL_CHECK(growth::pickSizedDifferent(rng, one, one.keys[0]) == one.keys[0]);
    (void)twin.choiceIndex(1);
    PL_CHECK(same(rng, twin));
  }
  std::printf("  PASS: pickSized / pickSizedDifferent spend exactly the "
              "choiceIndex draws they replaced\n");
}

} // namespace

int main() {
  std::printf("=== Counterparty Tests ===\n");
  testKeyValues();
  testAllAreExternal();
  testAllUnique();
  testGroupSizes();
  testGroupContents();
  testRoleConventions();
  testIsHelper();
  testRetiredOffsets();
  testProviderKeyLayout();
  testNonProviders();
  testRetiredAndFuneralKeysAreReserved();
  testSusbTable();
  testRhfsTable();
  testRosterCounts();
  testRankTails();
  testRosterSerials();
  testSizedPoolWeights();
  testPickDrawContract();
  std::printf("All counterparty tests passed.\n\n");
  return 0;
}
