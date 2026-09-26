#pragma once

/*
  Employer and landlord size laws (counterparty-sizes-2026-09).

  Each table is a published size distribution: a class's REAL member count
  (firms, payers or properties) and its share of the payer volume (jobs or
  rental units). A roster keeps

      N_c = clamp(round(P * s * m_c), 1, F_c)

  members of class c, where P is the population, s the share of people who
  pay the role (workers or renters), m_c the class's share of volume and F_c
  its real member count. A small class therefore gets about one counterparty
  per expected payer and a large class stops at its real size. This is the
  thinning a national sample sees: PhantomLedger places people across the
  whole US geography, so its workers are spread over the national firm stock
  rather than one metro's. Authority rows: docs/fraud_model_audit.md,
  AMENDMENT counterparty-sizes-2026-09.

  Everything here is draw-free.
*/

#include "phantomledger/entities/counterparties/sized_pool.hpp"
#include "phantomledger/entities/counterparties/landlords.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::synth::counterparties::sizes {

using entity::counterparty::SizedPool;

struct SizeRow {
  std::uint64_t members = 0;
  double volume = 0.0;
};

// ---------------------------------------------------------------------------
// Employers: Census SUSB 2022, US all industries (release 2025-04-10),
// enterprise size classes. us_state_naics_detailedsizes_2022.xlsx for the
// rows below 10,000 and us_naicssector_large_emplsize_2022.xlsx for the two
// largest. {firms, employees}.
// ---------------------------------------------------------------------------

inline constexpr std::array<SizeRow, 13> kSusb2022Uniform{{
    {4'029'041, 6'295'615.0}, // <5
    {1'034'135, 6'810'300.0}, // 5-9
    {656'917, 8'844'269.0},   // 10-19 (10-14 428,589 + 15-19 228,328)
    {561'160, 21'976'701.0},  // 20-99
    {93'341, 18'323'671.0},   // 100-499
    {6'974, 4'237'717.0},     // 500-749
    {3'426, 2'963'606.0},     // 750-999
    {3'412, 4'151'589.0},     // 1,000-1,499
    {1'777, 3'069'744.0},     // 1,500-1,999
    {1'091, 2'443'736.0},     // 2,000-2,499
    {2'099, 7'321'331.0},     // 2,500-4,999
    {1'124, 7'786'231.0},     // 5,000-9,999
    {592, 8'139'580.0},       // 10,000-19,999
}};

// 20,000+ employees: one rank-size tail, s_r = 20,000 * (546 / r)^b with b
// solved so the row sums exactly (b about 0.720, a Pareto alpha of 1/b).
inline constexpr SizeRow kSusb2022Top{546, 33'384'317.0};
inline constexpr double kSusb2022TopFloor = 20'000.0;

inline constexpr std::uint64_t kSusb2022Firms = 6'395'635;
inline constexpr double kSusb2022Employment = 135'748'407.0;

// Government payrolls. Jobs: BLS QCEW 2022 annual averages, millions of
// covered jobs. Payers: federal civilian payroll as ONE payer and the 50
// states as equal payers (declared CHOICES); local governments are the
// Census of Governments 2022 count, paid uniformly.
struct GovernmentRow {
  std::uint64_t payers = 0;
  double jobsMillions = 0.0;
};

inline constexpr std::array<GovernmentRow, 3> kGovernment2022{{
    {1, 2.9},       // federal
    {50, 4.5},      // state
    {90'837, 13.6}, // local
}};

inline constexpr double kQcew2022CoveredJobsMillions = 150.0;
inline constexpr double kGovernmentJobsMillions = 2.9 + 4.5 + 13.6;
inline constexpr double kGovernmentShare =
    kGovernmentJobsMillions / kQcew2022CoveredJobsMillions;

inline constexpr std::size_t kEmployerClassCount =
    kSusb2022Uniform.size() + 1 + kGovernment2022.size();
inline constexpr std::size_t kEmployerTailClass = kSusb2022Uniform.size();
inline constexpr std::size_t kFederalClass = kEmployerTailClass + 1;

// ---------------------------------------------------------------------------
// Landlords: HUD/Census Rental Housing Finance Survey 2021 (2020 stock), via
// CRS R47332 (Keightley, 2022). Size columns: 1, 2-4, 5-24, 25-49, 50-99,
// 100-149, 150+ units in the property. Thousands.
// ---------------------------------------------------------------------------

inline constexpr std::size_t kRhfsSizeColumns = 7;

// Table 1, properties.
inline constexpr std::array<double, kRhfsSizeColumns> kRhfs2021Properties{
    16'550, 2'215, 419, 75, 15, 11, 45};
inline constexpr double kRhfs2021AllProperties = 19'328;

// Table 3, the Total row of units.
inline constexpr std::array<double, kRhfsSizeColumns> kRhfs2021Units{
    16'550, 6'065, 5'470, 2'725, 1'055, 1'296, 16'387};
inline constexpr double kRhfs2021AllUnits = 49'547;

enum class RhfsOwner : std::uint8_t {
  individualInvestor,
  trusteeForEstate,
  llpLpLlc,
  tenantInCommon,
  generalPartnership,
  reit,
  realEstateCorporation,
  housingCooperative,
  nonprofit,
  other,
  notReported,
};

inline constexpr std::size_t kRhfsOwnerCount = 11;

// Table 3, units by current ownership of the property, in the order above.
inline constexpr std::array<std::array<double, kRhfsSizeColumns>,
                            kRhfsOwnerCount>
    kRhfs2021UnitsByOwner{{
        {12'003, 3'878, 1'364, 415, 99, 81, 796},
        {341, 377, 197, 68, 1, 6, 53},
        {2'362, 1'043, 2'683, 1'335, 614, 758, 11'225},
        {483, 61, 16, 4, 0, 0, 34},
        {95, 87, 132, 101, 37, 59, 310},
        {110, 19, 19, 12, 8, 16, 610},
        {61, 84, 192, 157, 55, 65, 737},
        {0, 23, 11, 11, 0, 2, 15},
        {224, 32, 275, 196, 75, 101, 256},
        {180, 115, 104, 93, 39, 19, 312},
        {690, 346, 477, 332, 127, 189, 2'039},
    }};

// LLC/LP/LLP and general-partnership owners below 25 units type as llcSmall;
// at 25 units and above they type as corporate.
inline constexpr std::size_t kRhfsFirstLargeColumn = 3;

// NMHC 2024 Top 50 apartment owners: more than 2.4M units, Greystar the
// largest at about 108k. One rank-size row, s_r = 108k * r^-b, carved out of
// the 150+ column (units, and properties in proportion).
inline constexpr std::uint64_t kNmhc2024Owners = 50;
inline constexpr double kNmhc2024Units = 2'400;
inline constexpr double kNmhc2024Rank1Units = 108;

inline constexpr std::size_t kLandlordClassCount = kRhfsSizeColumns + 1;
inline constexpr std::size_t kLandlordTailClass = kRhfsSizeColumns;

using TypeShares = std::array<double, entity::landlord::kTypeCount>;

// ---------------------------------------------------------------------------
// The law
// ---------------------------------------------------------------------------

/// The b with sum_{r=1..n} anchorSize * (anchorRank / r)^b == total, by a
/// fixed-iteration bisection (the reach.hpp pattern), so it is deterministic.
[[nodiscard]] inline double solveRankExponent(std::uint64_t n,
                                              double anchorRank,
                                              double anchorSize, double total) {
  const auto excess = [&](double b) {
    double sum = 0.0;
    for (std::uint64_t r = 1; r <= n; ++r) {
      sum += anchorSize * std::pow(anchorRank / static_cast<double>(r), b);
    }
    return sum - total;
  };

  double lo = 0.0;
  double hi = 8.0;
  const bool risingAtLo = excess(lo) < 0.0;
  if (risingAtLo == (excess(hi) < 0.0)) {
    throw std::invalid_argument("solveRankExponent: no root in [0, 8]");
  }
  for (int i = 0; i < 100; ++i) {
    const double mid = 0.5 * (lo + hi);
    if ((excess(mid) < 0.0) == risingAtLo) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return 0.5 * (lo + hi);
}

struct ClassSpec {
  std::uint64_t realMembers = 0;
  double mass = 0.0;
};

/// The 17 employer classes in roster order: the 13 SUSB rows, the 20,000+
/// tail, then federal, state and local government.
[[nodiscard]] inline std::array<ClassSpec, kEmployerClassCount>
employerClasses() noexcept {
  std::array<ClassSpec, kEmployerClassCount> out{};
  const double privateShare = 1.0 - kGovernmentShare;
  std::size_t c = 0;
  for (const auto &row : kSusb2022Uniform) {
    out[c++] = {row.members, row.volume / kSusb2022Employment * privateShare};
  }
  out[c++] = {kSusb2022Top.members,
              kSusb2022Top.volume / kSusb2022Employment * privateShare};
  for (const auto &row : kGovernment2022) {
    out[c++] = {row.payers,
                row.jobsMillions / kGovernmentJobsMillions * kGovernmentShare};
  }
  return out;
}

/// The 8 landlord classes in roster order: the 7 RHFS columns (150+ less the
/// Top-50 carve-out), then the Top-50 owners.
[[nodiscard]] inline std::array<ClassSpec, kLandlordClassCount>
landlordClasses() {
  double units = 0.0;
  for (const double column : kRhfs2021Units) {
    units += column;
  }

  std::array<ClassSpec, kLandlordClassCount> out{};
  for (std::size_t c = 0; c < kRhfsSizeColumns; ++c) {
    out[c] = {static_cast<std::uint64_t>(kRhfs2021Properties[c] * 1'000.0),
              kRhfs2021Units[c] / units};
  }
  constexpr std::size_t top = kRhfsSizeColumns - 1;
  const double kept = (kRhfs2021Units[top] - kNmhc2024Units) /
                      kRhfs2021Units[top];
  out[top] = {static_cast<std::uint64_t>(
                  std::round(kRhfs2021Properties[top] * 1'000.0 * kept)),
              (kRhfs2021Units[top] - kNmhc2024Units) / units};
  out[kLandlordTailClass] = {kNmhc2024Owners, kNmhc2024Units / units};
  return out;
}

/// Reported units by PL landlord type in each RHFS column (not-reported
/// excluded): individual = individual investor + trustee + tenant in common;
/// llcSmall = LLC/LP/LLP + general partnership below 25 units; corporate =
/// every other reported form.
[[nodiscard]] inline std::array<TypeShares, kRhfsSizeColumns>
rhfsTypeUnits() noexcept {
  using Type = entity::landlord::Type;
  const auto ix = [](Type t) { return static_cast<std::size_t>(t); };
  const auto row = [](RhfsOwner o) {
    return kRhfs2021UnitsByOwner[static_cast<std::size_t>(o)];
  };

  std::array<TypeShares, kRhfsSizeColumns> out{};
  for (std::size_t c = 0; c < kRhfsSizeColumns; ++c) {
    double reported = 0.0;
    for (std::size_t o = 0; o < kRhfsOwnerCount; ++o) {
      if (static_cast<RhfsOwner>(o) != RhfsOwner::notReported) {
        reported += kRhfs2021UnitsByOwner[o][c];
      }
    }
    const double individual = row(RhfsOwner::individualInvestor)[c] +
                              row(RhfsOwner::trusteeForEstate)[c] +
                              row(RhfsOwner::tenantInCommon)[c];
    const double partnership =
        row(RhfsOwner::llpLpLlc)[c] + row(RhfsOwner::generalPartnership)[c];
    const double llcSmall = c < kRhfsFirstLargeColumn ? partnership : 0.0;

    out[c][ix(Type::individual)] = individual;
    out[c][ix(Type::llcSmall)] = llcSmall;
    out[c][ix(Type::corporate)] = reported - individual - llcSmall;
  }
  return out;
}

/// Each landlord class's type mix (unit-weighted, sums to 1). The Top-50
/// owners are corporate; their units leave the 150+ column's corporate
/// share, with the column's not-reported units spread in proportion.
[[nodiscard]] inline std::array<TypeShares, kLandlordClassCount>
landlordTypeShares() noexcept {
  using Type = entity::landlord::Type;
  const auto corporate = static_cast<std::size_t>(Type::corporate);

  auto units = rhfsTypeUnits();
  constexpr std::size_t top = kRhfsSizeColumns - 1;
  double reported = 0.0;
  for (const double v : units[top]) {
    reported += v;
  }
  units[top][corporate] -= kNmhc2024Units * reported / kRhfs2021Units[top];

  std::array<TypeShares, kLandlordClassCount> out{};
  for (std::size_t c = 0; c < kRhfsSizeColumns; ++c) {
    double sum = 0.0;
    for (const double v : units[c]) {
      sum += v;
    }
    for (std::size_t t = 0; t < entity::landlord::kTypeCount; ++t) {
      out[c][t] = units[c][t] / sum;
    }
  }
  out[kLandlordTailClass][corporate] = 1.0;
  return out;
}

/// N_c = clamp(round(P * share * m_c), 1, F_c) for every class.
template <std::size_t K>
[[nodiscard]] std::array<std::uint32_t, K>
rosterCounts(int population, double share,
             const std::array<ClassSpec, K> &classes) {
  std::array<std::uint32_t, K> out{};
  const double payers = static_cast<double>(std::max(0, population)) * share;
  for (std::size_t c = 0; c < K; ++c) {
    const double expected = std::round(payers * classes[c].mass);
    const double capped = std::clamp(
        expected, 1.0, static_cast<double>(classes[c].realMembers));
    out[c] = static_cast<std::uint32_t>(capped);
  }
  return out;
}

/// A roster's classes with their counts, plus the tail's relative weights
/// (its top ranks, in rank order).
struct RosterLaw {
  std::vector<SizedPool::Class> classes;
  std::size_t tailClass = SizedPool::npos;
  std::vector<double> tailWeights;

  [[nodiscard]] std::size_t total() const noexcept {
    std::size_t n = 0;
    for (const auto &cls : classes) {
      n += cls.members;
    }
    return n;
  }

  [[nodiscard]] SizedPool pool() const {
    return SizedPool::build(classes, tailClass, tailWeights);
  }
};

namespace detail {

template <std::size_t K>
[[nodiscard]] RosterLaw
assemble(const std::array<ClassSpec, K> &classes,
         const std::array<std::uint32_t, K> &counts, std::size_t tailClass,
         double anchorRank, double anchorSize, double b) {
  RosterLaw out;
  out.classes.reserve(K);
  for (std::size_t c = 0; c < K; ++c) {
    out.classes.push_back({.members = counts[c], .mass = classes[c].mass});
  }
  out.tailClass = tailClass;
  out.tailWeights.reserve(counts[tailClass]);
  for (std::uint32_t r = 1; r <= counts[tailClass]; ++r) {
    out.tailWeights.push_back(
        anchorSize * std::pow(anchorRank / static_cast<double>(r), b));
  }
  return out;
}

} // namespace detail

[[nodiscard]] inline double employerTailExponent() {
  return solveRankExponent(kSusb2022Top.members,
                           static_cast<double>(kSusb2022Top.members),
                           kSusb2022TopFloor, kSusb2022Top.volume);
}

[[nodiscard]] inline double landlordTailExponent() {
  return solveRankExponent(kNmhc2024Owners, 1.0, kNmhc2024Rank1Units,
                           kNmhc2024Units);
}

[[nodiscard]] inline RosterLaw employerLaw(int population, double workerShare) {
  const auto classes = employerClasses();
  return detail::assemble(classes,
                          rosterCounts(population, workerShare, classes),
                          kEmployerTailClass,
                          static_cast<double>(kSusb2022Top.members),
                          kSusb2022TopFloor, employerTailExponent());
}

[[nodiscard]] inline RosterLaw landlordLaw(int population,
                                           double renterShare) {
  const auto classes = landlordClasses();
  return detail::assemble(classes,
                          rosterCounts(population, renterShare, classes),
                          kLandlordTailClass, 1.0, kNmhc2024Rank1Units,
                          landlordTailExponent());
}

// Every serial the roster can mint stays below the SSA and disability
// employer keys (9,000,001 and 9,000,002, institutional_accounts.hpp).
[[nodiscard]] consteval std::uint64_t employerRealMembers() {
  std::uint64_t n = kSusb2022Top.members;
  for (const auto &row : kSusb2022Uniform) {
    n += row.members;
  }
  for (const auto &row : kGovernment2022) {
    n += row.payers;
  }
  return n;
}
static_assert(employerRealMembers() == 6'486'523);
static_assert(employerRealMembers() < 9'000'001);

} // namespace PhantomLedger::synth::counterparties::sizes
