#include "phantomledger/synth/products/providers.hpp"

#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace PhantomLedger::synth::products {

namespace {

namespace cps = ::PhantomLedger::counterparties;
namespace enumTax = ::PhantomLedger::taxonomies::enums;

using Market = cps::Market;

// --- The cited tables ---------------------------------------------------
//
// Authority rows: the institutional-providers amendment in
// docs/fraud_model_audit.md. [P] = primary document read, [S] = secondary.
// Every pool size is a declared CHOICE (real markets run to thousands of
// servicers and carriers); the tail past the named ranks is a declared 1/rank
// shape, checked against the research's HHI figures by the gate.

// NAIC Property and Casualty market share report (2025), Private Passenger
// Auto Total, ranks 1-25 [P]. Top 10 76.88%, top 21 87.0%.
constexpr std::array kAutoInsuranceNamed{
    0.1864, 0.1860, 0.1156, 0.1015, 0.0619, 0.0357, 0.0281, 0.0193, 0.0190,
    0.0153, 0.0142, 0.0139, 0.0137, 0.0110, 0.0096, 0.0083, 0.0080, 0.0071,
    0.0062, 0.0047, 0.0044, 0.0043, 0.0042, 0.0042, 0.0041,
};
constexpr std::array kAutoInsuranceTail{ProviderTailAnchor{100, 1.0}};

// NAIC Property and Casualty market share report (2025), Homeowners Multiple
// Peril, ranks 1-25 [P]. Top 5 46.11%, top 10 62.48%.
constexpr std::array kHomeInsuranceNamed{
    0.1869, 0.0942, 0.0702, 0.0551, 0.0546, 0.0515, 0.0457, 0.0250, 0.0219,
    0.0197, 0.0185, 0.0177, 0.0107, 0.0103, 0.0101, 0.0095, 0.0091, 0.0089,
    0.0080, 0.0078, 0.0070, 0.0069, 0.0068, 0.0066, 0.0065,
};
constexpr std::array kHomeInsuranceTail{ProviderTailAnchor{150, 1.0}};

// NAIC market share report for life/fraternal groups (2024), individual life,
// ranks 1-10 [P]. The report also gives top 25 at 72.83% and top 125 at
// 99.24%; both are anchors, and the pool of 125 is renormalized by 0.9924.
constexpr std::array kLifeInsuranceNamed{
    0.0866, 0.0576, 0.0552, 0.0508, 0.0408,
    0.0373, 0.0349, 0.0328, 0.0318, 0.0302,
};
constexpr std::array kLifeInsuranceTail{ProviderTailAnchor{25, 0.7283},
                                        ProviderTailAnchor{125, 0.9924}};

// FSOC 2024 Nonbank Mortgage Servicing Report, Table 1 (Inside Mortgage
// Finance, agency servicing UPB, Q4 2023), ranks 1-20 [P]. Largest 7.3%,
// top 10 54.1%, top 20 70.9%.
constexpr std::array kMortgageNamed{
    0.073, 0.067, 0.067, 0.061, 0.060, 0.054, 0.052, 0.052, 0.031, 0.025,
    0.024, 0.024, 0.023, 0.018, 0.017, 0.015, 0.014, 0.011, 0.011, 0.010,
};
constexpr std::array kMortgageTail{ProviderTailAnchor{300, 1.0}};

// Auto Finance News "Big Wheels" 2025, outstandings over about $1.9T, ranks
// 1-5 [S]: Toyota Financial, GM Financial, Ally, Chase, Capital One. Top 5
// about 25%, matching the research note's "top 5 about 25%" [S].
constexpr std::array kAutoLoanNamed{0.054, 0.054, 0.050, 0.046, 0.044};
constexpr std::array kAutoLoanTail{ProviderTailAnchor{200, 1.0}};

// Student loans: five federal servicers plus ten private lenders.
//  - Private loans are 7.6% of balances (MeasureOne) [S]; Sallie Mae holds
//    about 63% of private originations [S]. The other nine private lenders are
//    the 1/rank tail.
//  - Federal shares of about 45M borrowers: Nelnet 31% (Nelnet 10-K 2024)
//    [P], Aidvantage about 19% [S], MOHELA about 15% [P]. EdFinancial and CRI
//    are uncited; they split the federal remainder equally (CHOICE), which
//    also absorbs the Default Resolution Group, not modelled. Provisional
//    until the FSA "Portfolio by Loan Servicer" table is pulled.
constexpr double kPrivateStudentShare = 0.076;
constexpr double kFederalStudentShare = 1.0 - kPrivateStudentShare;
constexpr std::array kStudentLoanNamed{
    0.31 * kFederalStudentShare,  // Nelnet
    0.19 * kFederalStudentShare,  // Aidvantage
    0.175 * kFederalStudentShare, // EdFinancial (CHOICE)
    0.175 * kFederalStudentShare, // CRI (CHOICE)
    0.15 * kFederalStudentShare,  // MOHELA
    0.63 * kPrivateStudentShare,  // Sallie Mae
};
constexpr std::array kStudentLoanTail{ProviderTailAnchor{15, 1.0}};

constexpr std::array kProductionSpecs{
    ProviderMarketSpec{Market::mortgage, 300, kMortgageNamed, kMortgageTail},
    ProviderMarketSpec{Market::autoLoan, 200, kAutoLoanNamed, kAutoLoanTail},
    ProviderMarketSpec{Market::studentLoan, 15, kStudentLoanNamed,
                       kStudentLoanTail},
    ProviderMarketSpec{Market::autoInsurance, 100, kAutoInsuranceNamed,
                       kAutoInsuranceTail},
    ProviderMarketSpec{Market::homeInsurance, 150, kHomeInsuranceNamed,
                       kHomeInsuranceTail},
    ProviderMarketSpec{Market::lifeInsurance, 125, kLifeInsuranceNamed,
                       kLifeInsuranceTail},
};

constexpr std::array kSingletonTail{ProviderTailAnchor{1, 1.0}};

constexpr std::array kSingletonSpecs{
    ProviderMarketSpec{Market::mortgage, 1, {}, kSingletonTail},
    ProviderMarketSpec{Market::autoLoan, 1, {}, kSingletonTail},
    ProviderMarketSpec{Market::studentLoan, 1, {}, kSingletonTail},
    ProviderMarketSpec{Market::autoInsurance, 1, {}, kSingletonTail},
    ProviderMarketSpec{Market::homeInsurance, 1, {}, kSingletonTail},
    ProviderMarketSpec{Market::lifeInsurance, 1, {}, kSingletonTail},
};

[[noreturn]] void reject(const ProviderMarketSpec &spec,
                         const std::string &why) {
  throw std::invalid_argument("ProviderMarkets[" +
                              std::string{cps::marketName(spec.market)} +
                              "]: " + why);
}

// Per-rank shares, before renormalization.
[[nodiscard]] std::vector<double> rankShares(const ProviderMarketSpec &spec) {
  constexpr double kTolerance = 1e-12;

  if (spec.poolSize == 0 || spec.poolSize > cps::providers::kMaxOrdinal) {
    reject(spec, "pool size must be in 1..99,999");
  }
  if (spec.named.size() > spec.poolSize) {
    reject(spec, "more named shares than the pool holds");
  }

  std::vector<double> shares;
  shares.reserve(spec.poolSize);

  double cumulative = 0.0;
  for (const double share : spec.named) {
    if (!(share > 0.0)) {
      reject(spec, "named shares must be positive");
    }
    if (!shares.empty() && share > shares.back() + kTolerance) {
      reject(spec, "named shares must be nonincreasing");
    }
    shares.push_back(share);
    cumulative += share;
  }

  if (spec.anchors.empty() || spec.anchors.back().rank != spec.poolSize) {
    reject(spec, "the last tail anchor must sit at the pool size");
  }

  auto rank = static_cast<std::uint32_t>(spec.named.size());
  for (const auto &anchor : spec.anchors) {
    if (anchor.rank <= rank) {
      reject(spec, "tail anchors must follow the named ranks in order");
    }
    const double mass = anchor.cumulative - cumulative;
    if (!(mass > 0.0) || anchor.cumulative > 1.0 + kTolerance) {
      reject(spec, "every tail segment needs positive mass and the "
                   "cumulative share cannot exceed 1");
    }

    double harmonic = 0.0;
    for (std::uint32_t r = rank + 1; r <= anchor.rank; ++r) {
      harmonic += 1.0 / static_cast<double>(r);
    }
    for (std::uint32_t r = rank + 1; r <= anchor.rank; ++r) {
      const double share = mass / (static_cast<double>(r) * harmonic);
      if (!shares.empty() && share > shares.back() + kTolerance) {
        reject(spec, "a 1/rank tail segment starts above the share before "
                     "it (rank " +
                         std::to_string(r) + ")");
      }
      shares.push_back(share);
    }

    rank = anchor.rank;
    cumulative = anchor.cumulative;
  }

  return shares;
}

} // namespace

ProviderMarkets::ProviderMarkets(std::span<const ProviderMarketSpec> specs) {
  std::array<bool, cps::kMarketCount> seen{};

  for (const auto &spec : specs) {
    const auto index = enumTax::toIndex(spec.market);
    if (index >= cps::kMarketCount || seen[index]) {
      reject(spec, "each market must appear exactly once");
    }
    seen[index] = true;

    const auto shares = rankShares(spec);
    const double mass = spec.anchors.back().cumulative;

    auto &cdf = cdf_[index];
    cdf.reserve(shares.size());
    double running = 0.0;
    for (const double share : shares) {
      running += share / mass;
      cdf.push_back(running);
    }
    // Exact, so a uniform in [0, 1) always lands inside the pool.
    cdf.back() = 1.0;
  }

  for (std::size_t i = 0; i < seen.size(); ++i) {
    if (!seen[i]) {
      throw std::invalid_argument(
          "ProviderMarkets: market " +
          std::string{cps::marketName(cps::kMarkets[i])} + " has no table");
    }
  }
}

const ProviderMarkets &ProviderMarkets::production() {
  static const ProviderMarkets markets{kProductionSpecs};
  return markets;
}

const ProviderMarkets &ProviderMarkets::singleton() {
  static const ProviderMarkets markets{kSingletonSpecs};
  return markets;
}

const std::vector<double> &ProviderMarkets::cdf(Market market) const noexcept {
  return cdf_[enumTax::toIndex(market)];
}

std::uint32_t ProviderMarkets::poolSize(Market market) const noexcept {
  return static_cast<std::uint32_t>(cdf(market).size());
}

std::uint32_t ProviderMarkets::ordinalFor(Market market,
                                          double u) const noexcept {
  const auto &table = cdf(market);
  const auto it = std::upper_bound(table.begin(), table.end(), u);
  const auto index = std::min<std::size_t>(
      static_cast<std::size_t>(it - table.begin()), table.size() - 1U);
  return static_cast<std::uint32_t>(index) + 1U;
}

double ProviderMarkets::share(Market market, std::uint32_t ordinal) const {
  const auto &table = cdf(market);
  if (ordinal == 0 || ordinal > table.size()) {
    throw std::out_of_range("ProviderMarkets::share: ordinal outside pool");
  }
  return ordinal == 1 ? table[0] : table[ordinal - 1U] - table[ordinal - 2U];
}

double ProviderMarkets::cumulativeShare(Market market,
                                        std::uint32_t rank) const {
  const auto &table = cdf(market);
  if (rank == 0) {
    return 0.0;
  }
  return table[std::min<std::size_t>(rank, table.size()) - 1U];
}

void UsedProviders::mark(Market market, std::uint32_t ordinal) {
  auto &flags = byMarket_[enumTax::toIndex(market)];
  if (flags.size() <= ordinal) {
    flags.resize(static_cast<std::size_t>(ordinal) + 1U, false);
  }
  flags[ordinal] = true;
}

std::vector<::PhantomLedger::entity::Key> UsedProviders::keys() const {
  std::vector<::PhantomLedger::entity::Key> out;
  for (const auto market : cps::kMarkets) {
    const auto &flags = byMarket_[enumTax::toIndex(market)];
    for (std::size_t ordinal = 1; ordinal < flags.size(); ++ordinal) {
      if (flags[ordinal]) {
        out.push_back(
            cps::providers::key(market, static_cast<std::uint32_t>(ordinal)));
      }
    }
  }
  return out;
}

ProviderPicker::ProviderPicker(const ProviderMarkets &markets,
                               std::uint64_t seed,
                               ::PhantomLedger::entity::PersonId person,
                               UsedProviders *used) noexcept
    : markets_{&markets}, seed_{seed}, person_{person}, used_{used} {}

::PhantomLedger::entity::Key ProviderPicker::pick(Market market) const {
  std::uint32_t ordinal = 1;
  if (markets_->poolSize(market) > 1U) {
    const ::PhantomLedger::random::RngFactory factory{seed_};
    auto rng = factory.rng({"product-provider", cps::marketName(market),
                            std::to_string(static_cast<unsigned>(person_))});
    ordinal = markets_->ordinalFor(market, rng.nextDouble());
  }
  if (used_ != nullptr) {
    used_->mark(market, ordinal);
  }
  return cps::providers::key(market, ordinal);
}

} // namespace PhantomLedger::synth::products
