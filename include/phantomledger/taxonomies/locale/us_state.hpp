#pragma once

#include "phantomledger/taxonomies/enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace PhantomLedger::locale::us {

using namespace ::PhantomLedger::taxonomies::enums;

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

inline constexpr auto kStates = std::to_array<State>({
    State::al,  State::ak, State::az, State::ar, State::ca, State::co,
    State::ct,  State::de, State::fl, State::ga, State::hi, State::id,
    State::il,  State::in, State::ia, State::ks, State::ky, State::la,
    State::me,  State::md, State::ma, State::mi, State::mn, State::ms,
    State::mo,  State::mt, State::ne, State::nv, State::nh, State::nj,
    State::nm,  State::ny, State::nc, State::nd, State::oh, State::ok,
    State::or_, State::pa, State::ri, State::sc, State::sd, State::tn,
    State::tx,  State::ut, State::vt, State::va, State::wa, State::wv,
    State::wi,  State::wy, State::dc,
});

inline constexpr std::size_t kStateCount = kStates.size();

namespace detail {

static_assert(isIndexable(kStates));

inline constexpr auto kStateAbbrev = std::to_array<std::string_view>({
    "AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "FL", "GA", "HI",
    "ID", "IL", "IN", "IA", "KS", "KY", "LA", "ME", "MD", "MA", "MI",
    "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ", "NM", "NY", "NC",
    "ND", "OH", "OK", "OR", "PA", "RI", "SC", "SD", "TN", "TX", "UT",
    "VT", "VA", "WA", "WV", "WI", "WY", "DC",
});

inline constexpr auto kStateName = std::to_array<std::string_view>({
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
});

static_assert(kStateAbbrev.size() == kStateCount);
static_assert(kStateName.size() == kStateCount);

} // namespace detail

[[nodiscard]] constexpr std::string_view abbrev(State state) noexcept {
  return detail::kStateAbbrev[toIndex(state)];
}

[[nodiscard]] constexpr std::string_view fullName(State state) noexcept {
  return detail::kStateName[toIndex(state)];
}

[[nodiscard]] constexpr std::optional<State>
parseStateCode(std::string_view code) noexcept {
  if (code.size() != 2) {
    return std::nullopt;
  }

  for (std::size_t index = 0; index < kStateCount; ++index) {
    if (detail::kStateAbbrev[index] == code) {
      return kStates[index];
    }
  }

  return std::nullopt;
}

// --- Embedded ZIP-range table ------------------------------------

struct ZipRange {
  std::uint32_t low;
  std::uint32_t high;
};

namespace detail {

inline constexpr auto kStateZipRanges = std::to_array<ZipRange>({
    {35004, 36925}, // AL
    {99501, 99950}, // AK
    {85001, 86556}, // AZ
    {71601, 72959}, // AR
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
    {73301, 88589}, // TX
    {84001, 84791}, // UT
    {5001, 5907},   // VT
    {20040, 24658}, // VA
    {98001, 99403}, // WA
    {24701, 26886}, // WV
    {53001, 54990}, // WI
    {82001, 83128}, // WY
    {20001, 20599}, // DC
});

static_assert(kStateZipRanges.size() == kStateCount);

} // namespace detail

[[nodiscard]] constexpr ZipRange zipRangeFor(State state) noexcept {
  return detail::kStateZipRanges[toIndex(state)];
}

// --- Population weights ------------------------------------------

namespace detail {

inline constexpr auto kStatePopulationBp = std::to_array<std::uint16_t>({
    150, 22, 218, 91,  1170, 173, 108, 31,  664, 326, 43,  58,  379,
    202, 96, 88,  136, 138,  41,  184, 207, 301, 173, 89,  185, 33,
    59,  94, 41,  276, 63,   591, 322, 23,  354, 120, 127, 389, 33,
    155, 27, 208, 896, 99,   20,  258, 232, 54,  176, 17,  20,
});

static_assert(kStatePopulationBp.size() == kStateCount);

} // namespace detail

[[nodiscard]] constexpr std::uint16_t
populationBasisPoints(State state) noexcept {
  return detail::kStatePopulationBp[toIndex(state)];
}

} // namespace PhantomLedger::locale::us
