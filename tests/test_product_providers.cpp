// tests/test_product_providers.cpp
//
// THE PROVIDER-MARKET GATE (institutional-providers-2026-09).
//
// The defects this exists for: every mortgage, auto loan, student loan and
// auto, home and life policy in the population paid ONE fixed key per
// product, so six accounts collected 5.6M payments at the corpus scale; and
// every mortgage paid the STUDENT-LOAN servicer (Bug A). Each contract now
// draws its provider once, at issuance, from a cited national market-share
// table on its own {"product-provider", market, person} lane.
//
// LEG A drives the real ObligationSynthesis over 200,000 personas with no
// ledger (the direct-sampler style of test_card_merchant_graph sub-gate G), so
// the share checks run at a scale where they can fail:
//
//   A0. THE TABLES reproduce their citations, and the declared 1/rank tail
//       reproduces the research's independent HHI figures.
//   A1. INVARIANCE. Production markets against ProviderMarkets::singleton(),
//       whose pick() makes no draw at all: every obligation event, loan term
//       and policy field except the counterparty key must be identical. A
//       picker that read the per-person portfolio stream only when it draws
//       would shift them. A draw BOTH legs make passes A1; that is A7's job.
//   A2. DOMAIN. Every contract pays a provider of its own market; the Bug A
//       predicate (no mortgage on a student key, no student loan on a
//       mortgage key) is explicit.
//   A3. ANTI-HUB. The largest provider in each market stays within 4 sigma
//       of its declared share and under an absolute ceiling. DISARM: the
//       singleton markets score 1.0 and must fail the same check.
//   A4. SHAPE. The top-10 cumulative share sits within 4 sigma of the table.
//   A5. REGISTRATION. The registered providers are exactly the issued ones,
//       external and ownerless, appended after every existing record.
//   A6. PARITY. The windowed replay equals the materialized slice, keys
//       included.
//   A7. THE PORTFOLIO-STREAM PIN. A digest of every key-free product value
//       equals the pre-round build's, with a domain predicate beside it.
//
// LEG B runs a full gate world at the run-golden configuration (pop 2,000,
// 60 days, seed 3405691582) and checks the corpus rows: product rows land on
// their market, premiums and claims on the payer's own carrier, no
// external-unknown row on the retired catch-all (unknown-counterparty-2026-09),
// and no camouflage P2P row on a lender or insurer. B5 pins the shared entity
// stream's position after the build to the pre-round build's.
//
// Why A7 and B5 exist: the run golden is re-pinned in the round this lands,
// so a stream shift would be absorbed into its new digest. Both pins were
// measured on the pre-round HEAD and on this build and agree. DISARMS: one
// extra portfolio draw per issued mortgage passes A1 (0 field diffs) and reds
// A7; one extra shared draw in buildLandlords reds B5.

#include "gate_world.hpp"
#include "test_support.hpp"
#include "window_leg_support.hpp"

#include "phantomledger/entities/counterparties/institutional_accounts.hpp"
#include "phantomledger/entities/counterparties/providers.hpp"
#include "phantomledger/entities/counterparties/remote_payees.hpp"
#include "phantomledger/pipeline/acceptance/fingerprint.hpp"
#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/pipeline/stages/products.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/synth/accounts/assign.hpp"
#include "phantomledger/synth/personas/make.hpp"
#include "phantomledger/synth/products/providers.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/legit/ledger/burdens.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;
namespace cps = pl::counterparties;
namespace providers = pl::counterparties::providers;
namespace product = pl::entity::product;
namespace productSynth = pl::synth::products;
namespace productStage = pl::pipeline::stages::products;
namespace channels = pl::channels;

using Market = cps::Market;
using ProductType = product::ProductType;
using PolicyType = product::PolicyType;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

[[nodiscard]] std::optional<Market> marketOfLoan(ProductType type) noexcept {
  switch (type) {
  case ProductType::mortgage:
    return Market::mortgage;
  case ProductType::autoLoan:
    return Market::autoLoan;
  case ProductType::studentLoan:
    return Market::studentLoan;
  default:
    return std::nullopt;
  }
}

[[nodiscard]] Market marketOfPolicy(PolicyType type) noexcept {
  switch (type) {
  case PolicyType::auto_:
    return Market::autoInsurance;
  case PolicyType::home:
    return Market::homeInsurance;
  case PolicyType::life:
    return Market::lifeInsurance;
  }
  return Market::lifeInsurance;
}

constexpr std::array kLoanTypes{ProductType::mortgage, ProductType::autoLoan,
                                ProductType::studentLoan};
constexpr std::array kPolicyTypes{PolicyType::auto_, PolicyType::home,
                                  PolicyType::life};

[[nodiscard]] std::string label(Market market) {
  return std::string{cps::marketName(market)};
}

// ------------------------------------------------------------------ LEG A

constexpr std::uint32_t kPopulation = 200'000;
constexpr std::uint64_t kPersonaSeed = 0x5EED'0F'9A11ULL;

// 4 sigma of a binomial share: a false red about once in 16,000 checks.
constexpr double kSigmas = 4.0;

// The largest real concentration in the research is Nelnet, about 31% of
// federal student borrowers (28.6% of this pool after the private share).
// Every other market's largest firm is under 19%. A provider above 35% of its
// market is a hub the research does not support.
constexpr double kAbsoluteTop1Ceiling = 0.35;

// Pre-existing registry records the synthesis must append after without
// moving: the institutional singletons, a check-payee bank (the entity stage
// registers those last, unknown-counterparty-2026-09), and a block that
// stands in for the rest of the entity stage.
[[nodiscard]] std::vector<pl::entity::Key> seededRecords() {
  std::vector<pl::entity::Key> keys(cps::kAll.begin(), cps::kAll.end());
  keys.push_back(cps::remote::checkBank(1));
  for (std::uint64_t serial = 1; serial <= 500; ++serial) {
    keys.push_back(pl::entity::makeKey(pl::entity::Role::business,
                                       pl::entity::Bank::external, serial));
  }
  return keys;
}

struct ProductsWorld {
  pl::pipeline::People people;
  pl::pipeline::Holdings holdings;
  productStage::ObligationSynthesis synthesis;
  product::ObligationStream full;
};

[[nodiscard]] pl::time::Window legAWindow() {
  return pl::time::Window{.start = pl::time::makeTime({2024, 1, 1}),
                          .days = 366};
}

void buildProductsWorld(ProductsWorld &world,
                        const productSynth::ProviderMarkets &markets) {
  auto rng = pl::random::Rng::fromSeed(kPersonaSeed);
  world.people.personas =
      pl::synth::personas::makePack(rng, kPopulation, kPersonaSeed);

  const auto seeded = seededRecords();
  pl::synth::accounts::addAccounts(world.holdings.accounts, seeded,
                                   /*external=*/true);

  world.synthesis.providerMarkets(markets);
  const auto window = legAWindow();
  world.synthesis.synthesize(world.people, world.holdings, window);
  world.full = world.synthesis.generateWindow(world.people, window,
                                              window.start, window.endExcl());
}

[[nodiscard]] std::span<const product::ObligationEvent>
allEvents(const product::ObligationStream &stream) {
  return stream.between(pl::time::TimePoint::min(), pl::time::TimePoint::max());
}

[[nodiscard]] bool sameExceptKey(const product::ObligationEvent &a,
                                 const product::ObligationEvent &b) {
  return a.personId == b.personId && a.direction == b.direction &&
         a.amount == b.amount && a.timestamp == b.timestamp &&
         a.channel == b.channel && a.productType == b.productType &&
         a.productId == b.productId;
}

[[nodiscard]] bool sameEvent(const product::ObligationEvent &a,
                             const product::ObligationEvent &b) {
  return sameExceptKey(a, b) && a.counterpartyAcct == b.counterpartyAcct;
}

[[nodiscard]] bool sameTerms(const product::InstallmentTerms &a,
                             const product::InstallmentTerms &b) {
  return a.lateP == b.lateP && a.lateDaysMin == b.lateDaysMin &&
         a.lateDaysMax == b.lateDaysMax && a.missP == b.missP &&
         a.partialP == b.partialP && a.cureP == b.cureP &&
         a.partialMinFrac == b.partialMinFrac &&
         a.partialMaxFrac == b.partialMaxFrac && a.clusterMult == b.clusterMult;
}

[[nodiscard]] bool samePolicyExceptCarrier(const product::InsurancePolicy &a,
                                           const product::InsurancePolicy &b) {
  return a.policyType == b.policyType && a.monthlyPremium == b.monthlyPremium &&
         a.billingDay == b.billingDay && a.annualClaimP == b.annualClaimP;
}

// Contracts per provider ordinal, per market. Loans are keyed by their
// events (every event of one contract must carry the same key, and a loan
// with no in-window event cannot be seen here); policies come from the
// ledger.
struct MarketCounts {
  std::array<std::map<std::uint32_t, std::size_t>, cps::kMarketCount> byOrdinal;
  std::size_t inconsistentContracts = 0;

  [[nodiscard]] std::size_t contracts(Market market) const {
    std::size_t n = 0;
    for (const auto &[ordinal, count] : byOrdinal[toIndex(market)]) {
      n += count;
    }
    return n;
  }

  [[nodiscard]] static std::size_t toIndex(Market market) {
    return pl::taxonomies::enums::toIndex(market);
  }
};

[[nodiscard]] std::uint32_t ordinalOf(pl::entity::Key key) {
  return static_cast<std::uint32_t>((key.number - providers::kBaseSerial) %
                                    providers::kBlockSize);
}

[[nodiscard]] MarketCounts countContracts(const ProductsWorld &world) {
  MarketCounts out;

  std::map<std::pair<pl::entity::PersonId, ProductType>, pl::entity::Key>
      loanKey;
  for (const auto &event : allEvents(world.full)) {
    if (!marketOfLoan(event.productType).has_value()) {
      continue;
    }
    const auto [it, inserted] = loanKey.emplace(
        std::pair{event.personId, event.productType}, event.counterpartyAcct);
    if (!inserted && it->second != event.counterpartyAcct) {
      ++out.inconsistentContracts;
    }
  }
  for (const auto &[contract, key] : loanKey) {
    if (const auto market = providers::marketOf(key); market.has_value()) {
      ++out.byOrdinal[MarketCounts::toIndex(*market)][ordinalOf(key)];
    }
  }

  world.holdings.portfolios.insurance().forEach(
      [&](pl::entity::PersonId, const product::InsuranceHoldings &holdings) {
        holdings.forEach([&](const product::InsurancePolicy &policy) {
          if (const auto market = providers::marketOf(policy.carrierAcct);
              market.has_value()) {
            ++out.byOrdinal[MarketCounts::toIndex(*market)]
                           [ordinalOf(policy.carrierAcct)];
          }
        });
      });

  return out;
}

struct Concentration {
  std::size_t contracts = 0;
  std::size_t providersUsed = 0;
  double top1 = 0.0;  // the largest provider, whoever it is
  double top10 = 0.0; // ordinals 1..10, the table's top 10
  double hhi = 0.0;
};

[[nodiscard]] Concentration concentration(const MarketCounts &counts,
                                          Market market) {
  Concentration out;
  const auto &byOrdinal = counts.byOrdinal[MarketCounts::toIndex(market)];
  out.contracts = counts.contracts(market);
  out.providersUsed = byOrdinal.size();
  if (out.contracts == 0) {
    return out;
  }
  const auto n = static_cast<double>(out.contracts);
  for (const auto &[ordinal, count] : byOrdinal) {
    const double share = static_cast<double>(count) / n;
    out.top1 = std::max(out.top1, share);
    out.hhi += share * share * 1e4;
    if (ordinal <= 10U) {
      out.top10 += share;
    }
  }
  return out;
}

[[nodiscard]] double binomialSigma(double p, std::size_t n) {
  return n == 0 ? 1.0 : std::sqrt(p * (1.0 - p) / static_cast<double>(n));
}

// A3 as a predicate, so the disarm leg can assert it FAILS.
[[nodiscard]] bool antiHubHolds(const Concentration &c, double declaredTop1) {
  const double sigma = binomialSigma(declaredTop1, c.contracts);
  return c.contracts > 0 && c.top1 <= declaredTop1 + kSigmas * sigma &&
         c.top1 < kAbsoluteTop1Ceiling;
}

void gateA0Tables(const productSynth::ProviderMarkets &markets) {
  std::printf("\n=== A0: the cited tables ===\n");

  const auto near = [](double a, double b, double tol) {
    return std::abs(a - b) <= tol;
  };
  // The cited anchors, as the research file reports them.
  check(near(markets.share(Market::autoInsurance, 1), 0.1864, 1e-9),
        "A0: auto insurance top-1 must be NAIC's 18.64%");
  check(near(markets.cumulativeShare(Market::autoInsurance, 10), 0.7688, 5e-4),
        "A0: auto insurance top 10 must be NAIC's 76.88%");
  check(near(markets.cumulativeShare(Market::homeInsurance, 10), 0.6248, 5e-4),
        "A0: home insurance top 10 must be NAIC's 62.48%");
  check(near(markets.cumulativeShare(Market::lifeInsurance, 10),
             0.4579 / 0.9924, 5e-4),
        "A0: life top 10 must be NAIC's 45.79% of the top-125 mass");
  check(near(markets.cumulativeShare(Market::lifeInsurance, 25),
             0.7283 / 0.9924, 5e-4),
        "A0: life top 25 must be NAIC's 72.83% of the top-125 mass");
  check(near(markets.share(Market::mortgage, 1), 0.073, 1e-9),
        "A0: the largest mortgage servicer must be FSOC's 7.3%");
  check(near(markets.cumulativeShare(Market::mortgage, 10), 0.541, 1.5e-3),
        "A0: mortgage top 10 must be FSOC's 54.1%");
  check(near(markets.cumulativeShare(Market::mortgage, 20), 0.709, 5e-4),
        "A0: mortgage top 20 must be FSOC's 70.9%");
  check(near(markets.cumulativeShare(Market::autoLoan, 5), 0.248, 5e-4),
        "A0: auto-loan top 5 must be the cited 24.8% (about 25%)");
  check(near(markets.share(Market::studentLoan, 1), 0.31 * (1.0 - 0.076), 1e-9),
        "A0: Nelnet must hold 31% of the federal (non-private) mass");

  // The 1/rank tail is a declared CHOICE. The research reports HHI for four
  // markets independently of the rank tables (auto about 1,000, home about
  // 620, life about 300, mortgage about 350), so the tail is checked
  // against a number it was not fitted to.
  struct HhiAnchor {
    Market market;
    double hhi;
  };
  constexpr std::array kHhi{
      HhiAnchor{Market::autoInsurance, 1'000.0},
      HhiAnchor{Market::homeInsurance, 620.0},
      HhiAnchor{Market::lifeInsurance, 300.0},
      HhiAnchor{Market::mortgage, 350.0},
  };
  for (const auto &anchor : kHhi) {
    double hhi = 0.0;
    for (std::uint32_t ordinal = 1; ordinal <= markets.poolSize(anchor.market);
         ++ordinal) {
      const double share = markets.share(anchor.market, ordinal);
      hhi += share * share * 1e4;
    }
    std::printf("  %-15s pool %3u  table HHI %6.0f  research HHI about %4.0f\n",
                label(anchor.market).c_str(), markets.poolSize(anchor.market),
                hhi, anchor.hhi);
    check(std::abs(hhi - anchor.hhi) <= 0.10 * anchor.hhi,
          "A0: " + label(anchor.market) + " table HHI " + std::to_string(hhi) +
              " must sit within 10% of the research's " +
              std::to_string(anchor.hhi));
  }
}

void gateA1Invariance(const ProductsWorld &prod, const ProductsWorld &single) {
  std::printf(
      "\n=== A1: provider values never reach the portfolio stream ===\n");

  const auto fullA = allEvents(prod.full);
  const auto fullB = allEvents(single.full);
  check(fullA.size() == fullB.size(),
        "A1: the full-window event count moved with the provider tables (" +
            std::to_string(fullA.size()) + " vs " +
            std::to_string(fullB.size()) + ")");
  std::size_t eventDiffs = 0;
  std::size_t loanEvents = 0;
  std::size_t keyDiffs = 0;
  for (std::size_t i = 0; i < std::min(fullA.size(), fullB.size()); ++i) {
    eventDiffs += sameExceptKey(fullA[i], fullB[i]) ? 0U : 1U;
    if (marketOfLoan(fullA[i].productType).has_value()) {
      ++loanEvents;
      keyDiffs +=
          fullA[i].counterpartyAcct == fullB[i].counterpartyAcct ? 0U : 1U;
    }
  }

  const auto sliceA = allEvents(prod.holdings.portfolios.obligations());
  const auto sliceB = allEvents(single.holdings.portfolios.obligations());
  check(sliceA.size() == sliceB.size(),
        "A1: the burden-slice event count moved with the provider tables");
  for (std::size_t i = 0; i < std::min(sliceA.size(), sliceB.size()); ++i) {
    eventDiffs += sameExceptKey(sliceA[i], sliceB[i]) ? 0U : 1U;
  }

  std::size_t loanDiffs = 0;
  std::size_t policyDiffs = 0;
  const auto &loansA = prod.holdings.portfolios.loans();
  const auto &loansB = single.holdings.portfolios.loans();
  const auto &insA = prod.holdings.portfolios.insurance();
  const auto &insB = single.holdings.portfolios.insurance();
  check(loansA.size() == loansB.size() && insA.size() == insB.size(),
        "A1: the number of issued loans or policy holders moved");
  for (pl::entity::PersonId person = 1; person <= kPopulation; ++person) {
    for (const auto type : kLoanTypes) {
      const auto *a = loansA.get(person, type);
      const auto *b = loansB.get(person, type);
      if ((a == nullptr) != (b == nullptr) ||
          (a != nullptr && !sameTerms(*a, *b))) {
        ++loanDiffs;
      }
    }
    const auto *ha = insA.get(person);
    const auto *hb = insB.get(person);
    if ((ha == nullptr) != (hb == nullptr)) {
      ++policyDiffs;
      continue;
    }
    if (ha == nullptr) {
      continue;
    }
    for (const auto type : kPolicyTypes) {
      const auto *pa = ha->get(type);
      const auto *pb = hb->get(type);
      if ((pa == nullptr) != (pb == nullptr) ||
          (pa != nullptr && !samePolicyExceptCarrier(*pa, *pb))) {
        ++policyDiffs;
      }
    }
  }

  std::printf("  events %zu (burden slice %zu), field diffs %zu; loan "
              "events %zu, key diffs %zu; loans %zu, loan diffs %zu; policy "
              "holders %zu, policy diffs %zu\n",
              fullA.size(), sliceA.size(), eventDiffs, loanEvents, keyDiffs,
              loansA.size(), loanDiffs, insA.size(), policyDiffs);
  check(eventDiffs == 0 && loanDiffs == 0 && policyDiffs == 0,
        "A1: swapping the provider tables moved " + std::to_string(eventDiffs) +
            " event fields, " + std::to_string(loanDiffs) + " loan terms and " +
            std::to_string(policyDiffs) +
            " policy fields. The provider pick must stay on its own lane");
  // Not vacuous: the two legs must disagree on the keys themselves.
  check(keyDiffs > loanEvents / 2,
        "A1: production and singleton keys agree on most loan events, so the "
        "invariance above proves nothing");
}

void gateA2Domain(const ProductsWorld &world) {
  std::printf("\n=== A2: every contract pays its own market ===\n");

  std::size_t loanEvents = 0;
  std::size_t wrongMarket = 0;
  std::size_t mortgageOnStudent = 0;
  std::size_t studentOnMortgage = 0;
  std::size_t taxOffIrs = 0;
  const auto irs = cps::key(cps::Tax::irsTreasury);
  for (const auto &event : allEvents(world.full)) {
    const auto expected = marketOfLoan(event.productType);
    if (!expected.has_value()) {
      taxOffIrs += event.counterpartyAcct == irs ? 0U : 1U;
      continue;
    }
    ++loanEvents;
    const auto actual = providers::marketOf(event.counterpartyAcct);
    wrongMarket += actual == expected ? 0U : 1U;
    if (event.productType == ProductType::mortgage &&
        actual == Market::studentLoan) {
      ++mortgageOnStudent;
    }
    if (event.productType == ProductType::studentLoan &&
        actual == Market::mortgage) {
      ++studentOnMortgage;
    }
  }

  std::size_t policies = 0;
  std::size_t wrongCarrier = 0;
  world.holdings.portfolios.insurance().forEach(
      [&](pl::entity::PersonId, const product::InsuranceHoldings &holdings) {
        holdings.forEach([&](const product::InsurancePolicy &policy) {
          ++policies;
          wrongCarrier += providers::marketOf(policy.carrierAcct) ==
                                  marketOfPolicy(policy.policyType)
                              ? 0U
                              : 1U;
        });
      });

  std::printf("  loan events %zu (wrong market %zu), policies %zu (wrong "
              "carrier market %zu), non-IRS tax events %zu\n",
              loanEvents, wrongMarket, policies, wrongCarrier, taxOffIrs);
  check(loanEvents > 0 && policies > 0, "A2: the leg issued no contracts");
  check(wrongMarket == 0,
        "A2: " + std::to_string(wrongMarket) +
            " loan events pay a key outside their own market");
  check(mortgageOnStudent == 0 && studentOnMortgage == 0,
        "A2 (Bug A): mortgages and student loans must never share a servicer "
        "(mortgage on student key " +
            std::to_string(mortgageOnStudent) + ", student on mortgage key " +
            std::to_string(studentOnMortgage) + ")");
  check(wrongCarrier == 0, "A2: " + std::to_string(wrongCarrier) +
                               " policies carry a key outside their market");
  check(taxOffIrs == 0, "A2: the IRS stays one account; " +
                            std::to_string(taxOffIrs) +
                            " tax events pay something else");
}

void gateA3A4Shape(const ProductsWorld &prod, const ProductsWorld &single) {
  std::printf("\n=== A3/A4: anti-hub and shape, pop %u ===\n", kPopulation);

  const auto &markets = prod.synthesis.providerMarkets();
  const auto prodCounts = countContracts(prod);
  const auto singleCounts = countContracts(single);
  check(prodCounts.inconsistentContracts == 0,
        "A3: " + std::to_string(prodCounts.inconsistentContracts) +
            " loans changed provider between two of their own payments");

  std::printf("  %-15s %9s %5s %8s %8s %8s %8s %6s\n", "market", "contracts",
              "used", "top1", "declared", "top10", "declared", "HHI");
  for (const auto market : cps::kMarkets) {
    const auto c = concentration(prodCounts, market);
    const double declaredTop1 = markets.share(market, 1);
    const double declaredTop10 = markets.cumulativeShare(market, 10);
    std::printf("  %-15s %9zu %5zu %8.4f %8.4f %8.4f %8.4f %6.0f\n",
                label(market).c_str(), c.contracts, c.providersUsed, c.top1,
                declaredTop1, c.top10, declaredTop10, c.hhi);

    // A3. The anti-hub bound, armed by the disarm below.
    check(
        antiHubHolds(c, declaredTop1),
        "A3: " + label(market) + " largest provider holds " +
            std::to_string(c.top1) + " of " + std::to_string(c.contracts) +
            " contracts against a declared " + std::to_string(declaredTop1) +
            " (4 sigma " +
            std::to_string(kSigmas * binomialSigma(declaredTop1, c.contracts)) +
            ", ceiling " + std::to_string(kAbsoluteTop1Ceiling) + ")");

    // DISARM: the pre-round shape, one provider per market, must fail.
    const auto disarmed = concentration(singleCounts, market);
    check(disarmed.top1 == 1.0 && !antiHubHolds(disarmed, declaredTop1),
          "A3 disarm: the singleton " + label(market) +
              " market must score 1.0 and FAIL the anti-hub check (scored " +
              std::to_string(disarmed.top1) + ")");

    // A4. The top-10 cumulative is a binomial over contracts, so it gets a
    // two-sided band around the table.
    const double sigma10 = binomialSigma(declaredTop10, c.contracts);
    check(std::abs(c.top10 - declaredTop10) <= kSigmas * sigma10,
          "A4: " + label(market) + " top-10 share " + std::to_string(c.top10) +
              " is outside 4 sigma of the declared " +
              std::to_string(declaredTop10));
  }
}

void gateA5Registration(const ProductsWorld &world) {
  std::printf("\n=== A5: registration ===\n");

  const auto seeded = seededRecords();
  const auto &records = world.holdings.accounts.registry.records;

  // The oracle: re-pick every issued contract's provider from the ledgers.
  const auto &markets = world.synthesis.providerMarkets();
  std::set<pl::entity::Key> issued;
  std::map<std::pair<pl::entity::PersonId, ProductType>, pl::entity::Key>
      loanOracle;
  std::size_t policiesOffOracle = 0;
  for (pl::entity::PersonId person = 1; person <= kPopulation; ++person) {
    const productSynth::ProviderPicker picker{markets, world.synthesis.seed(),
                                              person, nullptr};
    for (const auto type : kLoanTypes) {
      if (world.holdings.portfolios.loans().get(person, type) != nullptr) {
        const auto key = picker.pick(*marketOfLoan(type));
        issued.insert(key);
        loanOracle.emplace(std::pair{person, type}, key);
      }
    }
    if (const auto *holdings =
            world.holdings.portfolios.insurance().get(person)) {
      holdings->forEach([&](const product::InsurancePolicy &policy) {
        const auto key = picker.pick(marketOfPolicy(policy.policyType));
        issued.insert(key);
        policiesOffOracle += key == policy.carrierAcct ? 0U : 1U;
      });
    }
  }
  check(policiesOffOracle == 0,
        "A5: " + std::to_string(policiesOffOracle) +
            " policies do not carry their own lane's pick");

  std::size_t eventsOffOracle = 0;
  for (const auto &event : allEvents(world.full)) {
    if (!marketOfLoan(event.productType).has_value()) {
      continue;
    }
    const auto it = loanOracle.find({event.personId, event.productType});
    if (it == loanOracle.end() || it->second != event.counterpartyAcct) {
      ++eventsOffOracle;
    }
  }
  check(eventsOffOracle == 0,
        "A5: " + std::to_string(eventsOffOracle) +
            " loan events do not pay their contract's own lane pick");

  check(records.size() == seeded.size() + issued.size(),
        "A5: the registry must grow by exactly the " +
            std::to_string(issued.size()) + " issued providers (grew by " +
            std::to_string(records.size() - seeded.size()) + ")");
  std::size_t movedSeeded = 0;
  for (std::size_t i = 0; i < std::min(seeded.size(), records.size()); ++i) {
    movedSeeded += records[i].id == seeded[i] ? 0U : 1U;
  }
  check(movedSeeded == 0, "A5: " + std::to_string(movedSeeded) +
                              " existing registry records changed index");

  std::vector<pl::entity::Key> appended;
  std::size_t badRecords = 0;
  for (std::size_t i = seeded.size(); i < records.size(); ++i) {
    const auto &record = records[i];
    appended.push_back(record.id);
    const bool external =
        (record.flags &
         pl::entity::account::bit(pl::entity::account::Flag::external)) != 0;
    if (!providers::isProvider(record.id) || !external ||
        record.owner != pl::entity::invalidPerson) {
      ++badRecords;
    }
  }
  check(badRecords == 0, "A5: " + std::to_string(badRecords) +
                             " appended records are not external ownerless "
                             "providers");
  check(std::is_sorted(appended.begin(), appended.end()),
        "A5: providers must register in (market, ordinal) order");
  check(std::set<pl::entity::Key>(appended.begin(), appended.end()) == issued,
        "A5: the registered providers must equal the issued providers (no "
        "orphans, none missing)");

  std::printf("  seeded %zu, issued providers %zu, registry %zu, loan events "
              "off their lane pick %zu\n",
              seeded.size(), issued.size(), records.size(), eventsOffOracle);
}

void gateA6Parity(const ProductsWorld &world) {
  std::printf("\n=== A6: windowed replay parity ===\n");
  const auto window = legAWindow();

  const auto t0 = pl::time::addDays(window.start, 90);
  const auto t1 = pl::time::addDays(t0, 61);
  const auto replay =
      world.synthesis.generateWindow(world.people, window, t0, t1);
  const auto replayed = allEvents(replay);
  const auto slice = world.full.between(t0, t1);
  std::size_t diffs = replayed.size() == slice.size() ? 0U : 1U;
  for (std::size_t i = 0; i < std::min(replayed.size(), slice.size()); ++i) {
    diffs += sameEvent(replayed[i], slice[i]) ? 0U : 1U;
  }

  // The materialized burden slice from synthesize() against the same slice
  // of the replayed whole window.
  const auto burden = allEvents(world.holdings.portfolios.obligations());
  const auto fullBurden = world.full.between(
      window.start,
      pl::time::addDays(window.start,
                        30 *
                            pl::transfers::legit::ledger::kBurdenWindowMonths));
  std::size_t burdenDiffs = burden.size() == fullBurden.size() ? 0U : 1U;
  for (std::size_t i = 0; i < std::min(burden.size(), fullBurden.size()); ++i) {
    burdenDiffs += sameEvent(burden[i], fullBurden[i]) ? 0U : 1U;
  }

  std::printf("  replay [day 90, day 151) %zu events vs slice %zu, diffs %zu; "
              "burden slice %zu events, diffs %zu\n",
              replayed.size(), slice.size(), diffs, burden.size(), burdenDiffs);
  check(!replayed.empty(), "A6: the replay window is empty");
  check(diffs == 0, "A6: the windowed replay differs from the materialized "
                    "slice, provider keys included");
  check(burdenDiffs == 0,
        "A6: the materialized burden slice differs from the replay");
}

// A7. A1 compares two worlds built by the SAME binary, so a draw both of its
// legs make passes it with 0 diffs: a pick that read the portfolio stream
// outside the pool-size branch, or any emitter change. This pins the
// key-free products to the pre-round build instead: HEAD 843f447, one lender
// and one insurer per product, measured by a probe running this digest
// verbatim. This round's build gives the same value. A round that moves the
// product stream on purpose re-measures it.
constexpr std::uint64_t kKeyFreeProductDigest = 0xda72a2306ca4622eULL;

// FNV-1a over every product value the provider pick must not reach: each
// obligation event's fields except counterpartyAcct (whole window, then the
// burden slice), then per person each loan's terms and each policy's fields
// except carrierAcct.
[[nodiscard]] std::uint64_t keyFreeProductDigest(const ProductsWorld &world) {
  namespace fp = pl::pipeline::acceptance::detail;
  auto hash = fp::kFnvOffset;
  const auto mix = [&hash](std::uint64_t value) {
    hash = fp::fnvMix(hash, value);
  };
  const auto mixEvents =
      [&mix](std::span<const product::ObligationEvent> events) {
        mix(events.size());
        for (const auto &event : events) {
          mix(event.personId);
          mix(static_cast<std::uint64_t>(event.direction));
          mix(fp::bitsOf(event.amount));
          mix(static_cast<std::uint64_t>(
              event.timestamp.time_since_epoch().count()));
          mix(event.channel.value);
          mix(static_cast<std::uint64_t>(event.productType));
          mix(event.productId);
        }
      };
  mixEvents(allEvents(world.full));
  mixEvents(allEvents(world.holdings.portfolios.obligations()));

  const auto &loans = world.holdings.portfolios.loans();
  const auto &insurance = world.holdings.portfolios.insurance();
  for (pl::entity::PersonId person = 1; person <= kPopulation; ++person) {
    for (const auto type : kLoanTypes) {
      const auto *terms = loans.get(person, type);
      mix(terms != nullptr ? 1U : 0U);
      if (terms != nullptr) {
        mix(fp::bitsOf(terms->lateP));
        mix(static_cast<std::uint64_t>(terms->lateDaysMin));
        mix(static_cast<std::uint64_t>(terms->lateDaysMax));
        mix(fp::bitsOf(terms->missP));
        mix(fp::bitsOf(terms->partialP));
        mix(fp::bitsOf(terms->cureP));
        mix(fp::bitsOf(terms->partialMinFrac));
        mix(fp::bitsOf(terms->partialMaxFrac));
        mix(fp::bitsOf(terms->clusterMult));
      }
    }
    const auto *holdings = insurance.get(person);
    mix(holdings != nullptr ? 1U : 0U);
    if (holdings == nullptr) {
      continue;
    }
    for (const auto type : kPolicyTypes) {
      const auto *policy = holdings->get(type);
      mix(policy != nullptr ? 1U : 0U);
      if (policy != nullptr) {
        mix(static_cast<std::uint64_t>(policy->policyType));
        mix(fp::bitsOf(policy->monthlyPremium));
        mix(static_cast<std::uint64_t>(policy->billingDay));
        mix(fp::bitsOf(policy->annualClaimP));
      }
    }
  }
  return hash;
}

void gateA7StreamPin(const ProductsWorld &world) {
  std::printf(
      "\n=== A7: the key-free products match the pre-round build ===\n");

  const auto digest = keyFreeProductDigest(world);

  // The digest's domain predicate: a byte pin answers "has this changed",
  // never "is this sane".
  const auto window = legAWindow();
  std::size_t eventsOutOfDomain = 0;
  for (const auto &event : allEvents(world.full)) {
    const bool sane = event.personId >= 1 && event.personId <= kPopulation &&
                      std::isfinite(event.amount) && event.amount > 0.0 &&
                      event.timestamp >= window.start &&
                      event.timestamp < window.endExcl() &&
                      event.productType != ProductType::unknown;
    eventsOutOfDomain += sane ? 0U : 1U;
  }
  std::size_t policiesOutOfDomain = 0;
  world.holdings.portfolios.insurance().forEach(
      [&](pl::entity::PersonId, const product::InsuranceHoldings &holdings) {
        holdings.forEach([&](const product::InsurancePolicy &policy) {
          const bool sane = std::isfinite(policy.monthlyPremium) &&
                            policy.monthlyPremium > 0.0 &&
                            policy.billingDay >= 1 && policy.billingDay <= 28 &&
                            policy.annualClaimP >= 0.0 &&
                            policy.annualClaimP <= 1.0;
          policiesOutOfDomain += sane ? 0U : 1U;
        });
      });

  std::printf("  key-free digest %016llx (pinned %016llx); events out of "
              "domain %zu, policies out of domain %zu\n",
              static_cast<unsigned long long>(digest),
              static_cast<unsigned long long>(kKeyFreeProductDigest),
              eventsOutOfDomain, policiesOutOfDomain);
  check(digest == kKeyFreeProductDigest,
        "A7: the key-free product digest moved off the pre-round build, so a "
        "draw reached the portfolio stream (or an emitter changed)");
  check(eventsOutOfDomain == 0 && policiesOutOfDomain == 0,
        "A7: " + std::to_string(eventsOutOfDomain) + " events and " +
            std::to_string(policiesOutOfDomain) +
            " policies are outside their domain (person, finite positive "
            "amount, in-window timestamp; premium, billing day 1-28, claim "
            "probability)");
}

void runLegA() {
  std::printf("\n##### LEG A: products only, %u personas #####\n", kPopulation);

  const auto &production = productSynth::ProviderMarkets::production();
  gateA0Tables(production);

  ProductsWorld prod;
  buildProductsWorld(prod, production);
  ProductsWorld single;
  buildProductsWorld(single, productSynth::ProviderMarkets::singleton());

  gateA1Invariance(prod, single);
  gateA2Domain(prod);
  gateA3A4Shape(prod, single);
  gateA5Registration(prod);
  gateA6Parity(prod);
  gateA7StreamPin(prod);
}

// ------------------------------------------------------------------ LEG B

constexpr std::uint64_t kGoldenSeed = 3405691582ULL;

// B5. Leg A never sees the shared entity stream, and the run golden is
// re-pinned in the same round this code lands, so a draw added to it would be
// absorbed into the new pin. This is the next u64 the gate world's shared Rng
// hands out once the build (entities, products, infra, blueprint, opening
// book, income, market and obligation prep) is done. Measured on HEAD 843f447
// and on this round's build; the two agree.
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
// Pre-round value 498e4bde6c6f83ea.
constexpr std::uint64_t kSharedStreamNext = 0x9e0a89591a4d861fULL;

void runLegB() {
  std::printf("\n##### LEG B: gate world at the run-golden config #####\n");

  const auto poolSet = pltest::buildPoolSet(kGoldenSeed);
  const pl::time::Window window{.start = pl::time::makeTime({2025, 1, 1}),
                                .days = 60};

  pltest::LegOptions options{};
  options.population = 2'000;
  options.window = window;
  options.seed = kGoldenSeed;
  options.withFamily = true;
  const auto result = pltest::runLeg(poolSet, options);

  // The same world runLeg built (GateWorld is deterministic for one spec),
  // rebuilt here because the leg tears its own down.
  pltest::WorldSpec spec;
  spec.seed = options.seed;
  spec.window = options.window;
  spec.population = options.population;
  spec.fraudProfile = pltest::scaledFraudProfile();
  const pltest::GateWorld world(poolSet, spec);

  // B5: a copy, so the world's own stream stays where the build left it.
  auto sharedStream = world.rng;
  const auto sharedNext = sharedStream.nextU64();
  auto freshStream = pl::random::Rng::fromSeed(kGoldenSeed);
  std::printf("  shared stream next u64 after the build %016llx (pinned "
              "%016llx)\n",
              static_cast<unsigned long long>(sharedNext),
              static_cast<unsigned long long>(kSharedStreamNext));
  check(sharedNext == kSharedStreamNext,
        "B5: the shared entity stream moved off the pre-round build; a stage "
        "drew from it that did not before (or stopped drawing)");
  check(sharedNext != freshStream.nextU64(),
        "B5: the build drew nothing from the shared stream, so the pin above "
        "would pass on no data");

  // B4: every endpoint, providers included, is a registered account.
  pl::pipeline::validateTransactionAccounts(world.holdings.accounts.lookup,
                                            result.rows);

  // Providers register after every entity-stage record.
  const auto &records = world.holdings.accounts.registry.records;
  std::size_t firstProvider = records.size();
  std::size_t providerRecords = 0;
  std::size_t providersBeforeEnd = 0;
  for (std::size_t i = 0; i < records.size(); ++i) {
    if (providers::isProvider(records[i].id)) {
      firstProvider = std::min(firstProvider, i);
      ++providerRecords;
    } else if (firstProvider < records.size()) {
      ++providersBeforeEnd;
    }
  }
  check(providerRecords > 0, "B: the world registered no providers");
  check(providersBeforeEnd == 0,
        "B: " + std::to_string(providersBeforeEnd) +
            " non-provider records sit after the first provider; providers "
            "must append after every entity-stage record");

  std::unordered_map<pl::entity::Key, pl::entity::PersonId> ownerOf;
  for (const auto &record : records) {
    if (record.owner != pl::entity::invalidPerson) {
      ownerOf.emplace(record.id, record.owner);
    }
  }
  const auto &insurance = world.holdings.portfolios.insurance();
  const auto carrierOf = [&](pl::entity::Key account, bool claimable) {
    std::set<pl::entity::Key> out;
    const auto it = ownerOf.find(account);
    if (it == ownerOf.end()) {
      return out;
    }
    if (const auto *holdings = insurance.get(it->second)) {
      holdings->forEach([&](const product::InsurancePolicy &policy) {
        if (!claimable || policy.policyType != PolicyType::life) {
          out.insert(policy.carrierAcct);
        }
      });
    }
    return out;
  };

  const auto tagOf = [](auto value) { return channels::tag(value).value; };
  std::map<std::uint8_t, Market> productMarket{
      {tagOf(channels::Product::mortgage), Market::mortgage},
      {tagOf(channels::Product::autoLoan), Market::autoLoan},
      {tagOf(channels::Product::studentLoan), Market::studentLoan},
  };

  std::size_t productRows = 0, productWrong = 0;
  std::size_t premiumRows = 0, premiumWrong = 0;
  std::size_t claimRows = 0, claimWrong = 0;
  std::size_t externalRows = 0, externalOnRetired = 0;
  std::size_t camouflageRows = 0, camouflageOnProvider = 0;
  std::size_t retiredKeyRows = 0;
  const auto retiredCatchAll = cps::retiredExternalUnknown();
  const auto oldSentinel = pl::entity::makeKey(pl::entity::Role::merchant,
                                               pl::entity::Bank::external, 1);

  for (const auto &row : result.rows) {
    const auto channel = row.session.channel;
    for (const std::uint64_t offset : {1, 2, 3, 5, 6, 7}) {
      const auto retired = pl::entity::makeKey(pl::entity::Role::business,
                                               pl::entity::Bank::external,
                                               providers::kBaseSerial + offset);
      retiredKeyRows +=
          row.source == retired || row.target == retired ? 1U : 0U;
    }

    if (const auto it = productMarket.find(channel.value);
        it != productMarket.end()) {
      ++productRows;
      productWrong += providers::marketOf(row.target) == it->second ? 0U : 1U;
      continue;
    }
    if (channel.value == tagOf(channels::Insurance::premium)) {
      ++premiumRows;
      premiumWrong +=
          carrierOf(row.source, false).contains(row.target) ? 0U : 1U;
      continue;
    }
    if (channel.value == tagOf(channels::Insurance::claim)) {
      ++claimRows;
      claimWrong += carrierOf(row.target, true).contains(row.source) ? 0U : 1U;
      continue;
    }
    if (channel.value == tagOf(channels::Legit::externalUnknown) &&
        row.fraud.flag == 0) {
      ++externalRows;
      externalOnRetired += row.target == retiredCatchAll ? 1U : 0U;
      continue;
    }
    if (channels::isCamouflage(channel)) {
      ++camouflageRows;
      camouflageOnProvider += providers::isProvider(row.target) ? 1U : 0U;
    }
  }
  std::size_t oldSentinelRows = 0;
  for (const auto &row : result.rows) {
    if (row.target == oldSentinel &&
        row.session.channel.value == tagOf(channels::Legit::externalUnknown)) {
      ++oldSentinelRows;
    }
  }

  std::printf("  rows %zu; providers registered %zu from record %zu\n",
              result.rows.size(), providerRecords, firstProvider);
  std::printf(
      "  product rows %zu (off-market %zu); premiums %zu (not the "
      "payer's carrier %zu); claims %zu (not the payee's carrier %zu)\n",
      productRows, productWrong, premiumRows, premiumWrong, claimRows,
      claimWrong);
  std::printf("  legit external-unknown rows %zu (on the retired catch-all "
              "%zu); camouflage rows %zu (on a provider %zu)\n",
              externalRows, externalOnRetired, camouflageRows,
              camouflageOnProvider);

  check(productRows > 0 && premiumRows > 0 && externalRows > 0 &&
            camouflageRows > 0,
        "B: the leg is missing product, premium, external-unknown or "
        "camouflage rows, so a check below would pass on no data");
  check(productWrong == 0, "B1: " + std::to_string(productWrong) +
                               " loan rows pay outside their own market");
  check(premiumWrong == 0, "B1: " + std::to_string(premiumWrong) +
                               " premiums do not pay the payer's own carrier");
  check(claimWrong == 0, "B1: " + std::to_string(claimWrong) +
                             " claims are not paid by the payee's own carrier");
  // unknown-counterparty-2026-09 retired the catch-all this check used to
  // require; test_remote_payees owns where those rows go now.
  check(externalOnRetired == 0 && oldSentinelRows == 0,
        "B2: no legit external-unknown row may pay the retired catch-all (" +
            std::to_string(externalOnRetired) + " on it, " +
            std::to_string(oldSentinelRows) + " on merchant 1)");
  check(camouflageOnProvider == 0,
        "B3: " + std::to_string(camouflageOnProvider) +
            " camouflage rows land on a lender or insurer");
  check(retiredKeyRows == 0, "B: " + std::to_string(retiredKeyRows) +
                                 " rows still touch a retired singleton key");
}

} // namespace

int main() {
  std::printf("=== Product provider markets ===\n");
  runLegA();
  runLegB();
  if (g_failures != 0) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nAll product provider checks passed.\n");
  return 0;
}
