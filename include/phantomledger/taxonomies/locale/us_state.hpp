#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace PhantomLedger::locale::us {

enum class State : std::uint8_t {
  al = 0,
  ak,
  az,
  ar,
  ca,
  co,
  ct,
  de,
  fl,
  ga,
  hi,
  id,
  il,
  in,
  ia,
  ks,
  ky,
  la,
  me,
  md,
  ma,
  mi,
  mn,
  ms,
  mo,
  mt,
  ne,
  nv,
  nh,
  nj,
  nm,
  ny,
  nc,
  nd,
  oh,
  ok,
  or_,
  pa,
  ri,
  sc,
  sd,
  tn,
  tx,
  ut,
  vt,
  va,
  wa,
  wv,
  wi,
  wy,
  dc,
};

inline constexpr std::size_t kStateCount = 51;

[[nodiscard]] constexpr std::size_t slot(State s) noexcept {
  return static_cast<std::size_t>(s);
}

namespace detail {

/// USPS two-letter abbreviation per State, indexed by `slot(state)`.
inline constexpr std::array<std::string_view, kStateCount> kStateAbbrev{
    "AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "FL", "GA", "HI",
    "ID", "IL", "IN", "IA", "KS", "KY", "LA", "ME", "MD", "MA", "MI",
    "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ", "NM", "NY", "NC",
    "ND", "OH", "OK", "OR", "PA", "RI", "SC", "SD", "TN", "TX", "UT",
    "VT", "VA", "WA", "WV", "WI", "WY", "DC",
};

/// Full state name per State, indexed by `slot(state)`. Provided
/// for CSV exports that prefer "California" to "CA".
inline constexpr std::array<std::string_view, kStateCount> kStateName{
    "Alabama",        "Alaska",        "Arizona",
    "Arkansas",       "California",    "Colorado",
    "Connecticut",    "Delaware",      "Florida",
    "Georgia",        "Hawaii",        "Idaho",
    "Illinois",       "Indiana",       "Iowa",
    "Kansas",         "Kentucky",      "Louisiana",
    "Maine",          "Maryland",      "Massachusetts",
    "Michigan",       "Minnesota",     "Mississippi",
    "Missouri",       "Montana",       "Nebraska",
    "Nevada",         "New Hampshire", "New Jersey",
    "New Mexico",     "New York",      "North Carolina",
    "North Dakota",   "Ohio",          "Oklahoma",
    "Oregon",         "Pennsylvania",  "Rhode Island",
    "South Carolina", "South Dakota",  "Tennessee",
    "Texas",          "Utah",          "Vermont",
    "Virginia",       "Washington",    "West Virginia",
    "Wisconsin",      "Wyoming",       "District of Columbia",
};

} // namespace detail

[[nodiscard]] constexpr std::string_view abbrev(State s) noexcept {
  return detail::kStateAbbrev[slot(s)];
}

[[nodiscard]] constexpr std::string_view fullName(State s) noexcept {
  return detail::kStateName[slot(s)];
}

[[nodiscard]] constexpr std::optional<State>
parseStateCode(std::string_view code) noexcept {
  if (code.size() != 2) {
    return std::nullopt;
  }
  for (std::size_t i = 0; i < kStateCount; ++i) {
    if (detail::kStateAbbrev[i] == code) {
      return static_cast<State>(i);
    }
  }
  return std::nullopt;
}

// --- Embedded ZIP-range table (synthetic data path) ---------------

struct ZipRange {
  std::uint32_t low;
  std::uint32_t high;
};

namespace detail {

inline constexpr std::array<ZipRange, kStateCount> kStateZipRanges{{
    {35004, 36925}, // AL
    {99501, 99950}, // AK
    {85001, 86556}, // AZ
    {71601, 72959}, // AR  (union of two real spans)
    {90001, 96162}, // CA
    {80001, 81658}, // CO
    {6001, 6928},   // CT
    {19701, 19980}, // DE
    {32004, 34997}, // FL
    {30001, 39901}, // GA
    {96701, 96898}, // HI
    {83201, 83876}, // ID
    {60001, 62999}, // IL
    {46001, 47997}, // IN
    {50001, 52809}, // IA
    {66002, 67954}, // KS
    {40003, 42788}, // KY
    {70001, 71497}, // LA
    {3901, 4992},   // ME
    {20588, 21930}, // MD
    {1001, 5544},   // MA
    {48001, 49971}, // MI
    {55001, 56763}, // MN
    {38601, 39776}, // MS
    {63001, 65899}, // MO
    {59001, 59937}, // MT
    {68001, 69367}, // NE
    {88901, 89883}, // NV
    {3031, 3897},   // NH
    {7001, 8989},   // NJ
    {87001, 88439}, // NM
    {10001, 14975}, // NY
    {27006, 28909}, // NC
    {58001, 58856}, // ND
    {43001, 45999}, // OH
    {73001, 74966}, // OK
    {97001, 97920}, // OR
    {15001, 19640}, // PA
    {2801, 2940},   // RI
    {29001, 29945}, // SC
    {57001, 57799}, // SD
    {37010, 38589}, // TN
    {73301, 88589}, // TX  (large span, includes some non-TX gaps)
    {84001, 84791}, // UT
    {5001, 5907},   // VT
    {20040, 24658}, // VA
    {98001, 99403}, // WA
    {24701, 26886}, // WV
    {53001, 54990}, // WI
    {82001, 83128}, // WY
    {20001, 20599}, // DC
}};

} // namespace detail

[[nodiscard]] constexpr ZipRange zipRangeFor(State s) noexcept {
  return detail::kStateZipRanges[slot(s)];
}

// --- Population weights for realistic state distribution ----------

namespace detail {

inline constexpr std::array<std::uint16_t, kStateCount> kStatePopulationBp{
    150,  //  AL
    22,   //  AK
    218,  //  AZ
    91,   //  AR
    1170, //  CA
    173,  //  CO
    108,  //  CT
    31,   //  DE
    664,  //  FL
    326,  //  GA
    43,   //  HI
    58,   //  ID
    379,  //  IL
    202,  //  IN
    96,   //  IA
    88,   //  KS
    136,  //  KY
    138,  //  LA
    41,   //  ME
    184,  //  MD
    207,  //  MA
    301,  //  MI
    173,  //  MN
    89,   //  MS
    185,  //  MO
    33,   //  MT
    59,   //  NE
    94,   //  NV
    41,   //  NH
    276,  //  NJ
    63,   //  NM
    591,  //  NY
    322,  //  NC
    23,   //  ND
    354,  //  OH
    120,  //  OK
    127,  //  OR
    389,  //  PA
    33,   //  RI
    155,  //  SC
    27,   //  SD
    208,  //  TN
    896,  //  TX
    99,   //  UT
    20,   //  VT
    258,  //  VA
    232,  //  WA
    54,   //  WV
    176,  //  WI
    17,   //  WY
    20,   //  DC
};

} // namespace detail

/// Per-state population share in basis points (1/10000). Pass these
/// to a CDF sampler to draw states proportional to their share of
/// US population — CA at ~1170 (11.7%), WY at ~17 (0.17%). The
/// table sums to ~10000 (100%) modulo rounding.
[[nodiscard]] constexpr std::uint16_t populationBasisPoints(State s) noexcept {
  return detail::kStatePopulationBp[slot(s)];
}

} // namespace PhantomLedger::locale::us
