#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/taxonomies/locale/us_state.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

namespace PhantomLedger::entities::synth::pii {

using namespace ::PhantomLedger::taxonomies::enums;
namespace detail {

[[nodiscard]] constexpr std::size_t digits10(std::uint64_t v) noexcept {
  if (v == 0)
    return 1;
  std::size_t n = 0;
  while (v > 0) {
    v /= 10ULL;
    ++n;
  }
  return n;
}

} // namespace detail

static_assert(
    entity::pii::kCustomerDigitWidth >=
        detail::digits10(static_cast<std::uint64_t>(
            std::numeric_limits<entity::PersonId>::max())),
    "kCustomerDigitWidth is too narrow to encode the largest PersonId "
    "without truncation. Either widen kCustomerDigitWidth in "
    "entities/pii.hpp or narrow PersonId in entities/identifiers.hpp.");

namespace detail {

/// Write `value` as exactly `width` decimal digits, zero-padded.
inline void writeZeroPadded(char *dest, std::uint64_t value,
                            std::size_t width) noexcept {
  for (std::size_t i = 0; i < width; ++i) {
    dest[width - 1 - i] = static_cast<char>('0' + (value % 10ULL));
    value /= 10ULL;
  }
}

inline void copyTruncate(char *dest, std::size_t bufSize,
                         std::string_view s) noexcept {
  const std::size_t n = std::min(bufSize, s.size());
  std::memcpy(dest, s.data(), n);
  if (n < bufSize) {
    std::memset(dest + n, 0, bufSize - n);
  }
}

[[nodiscard]] inline std::uint32_t pickIndex(random::Rng &rng,
                                             std::size_t n) noexcept {
  if (n == 0) {
    return 0;
  }
  return static_cast<std::uint32_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(n)));
}

} // namespace detail

// --- Email -------------------------------------------------------

[[nodiscard]] inline entity::pii::Email
sampleEmail(entity::PersonId person) noexcept {
  using namespace entity::pii;

  Email out{};
  std::memcpy(out.bytes.data(), kEmailPrefix.data(), kEmailPrefix.size());
  detail::writeZeroPadded(out.bytes.data() + kEmailPrefix.size(),
                          static_cast<std::uint64_t>(person),
                          kCustomerDigitWidth);
  std::memcpy(out.bytes.data() + kEmailPrefix.size() + kCustomerDigitWidth,
              kEmailSuffix.data(), kEmailSuffix.size());
  return out;
}

// --- Phone -------------------------------------------------------

namespace detail {

struct PhoneShape {
  std::string_view callingCode;
  int digitCount = 10;
};

[[nodiscard]] constexpr PhoneShape phoneShape(locale::Country c) noexcept {
  using locale::Country;
  switch (c) {
  case Country::us:
  case Country::ca:
    return {"+1", 10};
  case Country::gb:
    return {"+44", 10};
  case Country::au:
    return {"+61", 9};
  case Country::de:
    return {"+49", 11};
  case Country::fr:
    return {"+33", 9};
  case Country::es:
    return {"+34", 9};
  case Country::it:
    return {"+39", 10};
  case Country::nl:
    return {"+31", 9};
  case Country::br:
    return {"+55", 11};
  case Country::mx:
    return {"+52", 10};
  case Country::in:
    return {"+91", 10};
  case Country::jp:
    return {"+81", 10};
  case Country::cn:
    return {"+86", 11};
  case Country::kr:
    return {"+82", 10};
  case Country::ru:
    return {"+7", 10};
  }
  return {"+1", 10};
}

} // namespace detail

[[nodiscard]] inline entity::pii::Phone samplePhone(random::Rng &rng,
                                                    locale::Country country) {
  using namespace entity::pii;
  const auto shape = detail::phoneShape(country);

  std::array<char, 15> digitBuf{};
  digitBuf[0] = static_cast<char>('0' + rng.uniformInt(2, 10)); // 2..9
  for (int i = 1; i < shape.digitCount; ++i) {
    digitBuf[static_cast<std::size_t>(i)] =
        static_cast<char>('0' + rng.uniformInt(0, 10));
  }

  Phone out{};
  std::memcpy(out.bytes.data(), shape.callingCode.data(),
              shape.callingCode.size());
  std::memcpy(out.bytes.data() + shape.callingCode.size(), digitBuf.data(),
              static_cast<std::size_t>(shape.digitCount));
  return out;
}

// --- Name --------------------------------------------------------

[[nodiscard]] inline entity::pii::Name sampleName(random::Rng &rng,
                                                  const LocalePool &pool) {
  entity::pii::Name out;
  out.firstIdx = static_cast<std::uint16_t>(
      detail::pickIndex(rng, pool.firstNames.size()));
  out.lastIdx =
      static_cast<std::uint16_t>(detail::pickIndex(rng, pool.lastNames.size()));
  if (pool.middleNames.empty()) {
    out.middleIdx = entity::pii::kNoMiddleIdx;
  } else {
    out.middleIdx = static_cast<std::uint16_t>(
        detail::pickIndex(rng, pool.middleNames.size()));
  }
  return out;
}

// --- SSN / national identifier -----------------------------------

[[nodiscard]] inline entity::pii::Ssn sampleSsn(random::Rng &rng,
                                                locale::Country country) {
  using namespace entity::pii;

  // Buffer we'll truncate-copy into the fixed-width Ssn at the end.
  std::array<char, 16> tmp{};
  std::size_t len = 0;
  const auto add = [&](char ch) {
    if (len < tmp.size())
      tmp[len++] = ch;
  };
  const auto addDigit = [&](int low, int high) {
    add(static_cast<char>('0' + rng.uniformInt(low, high)));
  };
  const auto addDigits = [&](std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
      addDigit(0, 10);
  };
  const auto addLetter = [&]() {
    add(static_cast<char>('A' + rng.uniformInt(0, 26)));
  };

  switch (country) {
  case locale::Country::us:
    // SSN: 666-NN-NNNN. Group [01,99], serial [0001,9999].
    add('6');
    add('6');
    add('6');
    add('-');
    addDigit(0, 10);
    addDigit((tmp[len - 1] == '0') ? 1 : 0, 10); // avoid "00"
    add('-');
    addDigits(4);
    break;

  case locale::Country::gb:
    // NINO: 2 letters + 6 digits + 1 letter, formatted "AA NN NN NN A".
    addLetter();
    addLetter();
    add(' ');
    addDigits(2);
    add(' ');
    addDigits(2);
    add(' ');
    addDigits(2);
    add(' ');
    addLetter();
    break;

  case locale::Country::in:
    // Aadhaar: 12 digits, grouped "NNNN NNNN NNNN".
    addDigits(4);
    add(' ');
    addDigits(4);
    add(' ');
    addDigits(4);
    break;

  case locale::Country::ca:
    // SIN: 9 digits "NNN-NNN-NNN".
    addDigits(3);
    add('-');
    addDigits(3);
    add('-');
    addDigits(3);
    break;

  default:
    // Generic: 11 digit national-id pattern with two dashes.
    addDigits(3);
    add('-');
    addDigits(2);
    add('-');
    addDigits(4);
    break;
  }

  Ssn out{};
  detail::copyTruncate(out.bytes.data(), out.bytes.size(),
                       std::string_view{tmp.data(), len});
  return out;
}

// --- DOB ---------------------------------------------------------

using PersonaType = ::PhantomLedger::personas::Type;

namespace detail {

struct AgeBand {
  int min = 0;
  int max = 0;
};

[[nodiscard]] constexpr AgeBand bandFor(PersonaType t) noexcept {
  switch (t) {
  case PersonaType::student:
    return {16, 34};
  case PersonaType::retiree:
    return {65, 99};
  case PersonaType::freelancer:
    return {25, 65};
  case PersonaType::smallBusiness:
    return {30, 70};
  case PersonaType::highNetWorth:
    return {35, 80};
  case PersonaType::salaried:
    return {22, 65};
  }
  return {25, 65};
}

} // namespace detail

[[nodiscard]] inline entity::pii::Dob
sampleDob(random::Rng &rng, PersonaType persona, time::TimePoint simStart) {
  const auto band = detail::bandFor(persona);
  const auto age = static_cast<int>(
      rng.uniformInt(band.min, static_cast<std::int64_t>(band.max) + 1));
  const auto offsetDays = static_cast<int>(rng.uniformInt(0, 365));
  const auto dobTp = time::addDays(simStart, -(age * 365 + offsetDays));
  const auto cd = time::toCalendarDate(dobTp);
  return entity::pii::Dob{
      static_cast<std::int16_t>(cd.year),
      static_cast<std::uint8_t>(cd.month),
      static_cast<std::uint8_t>(cd.day),
  };
}

// --- US ZIP synthesis (no external data) -------------------------

namespace detail {

/// Format a numeric ZIP as a zero-padded 5-character string. ZIPs
/// in the New England states all start with 0 and need padding.
inline std::string formatZip(std::uint32_t zip) {
  std::string out(5, '0');
  for (int i = 4; i >= 0 && zip > 0; --i) {
    out[static_cast<std::size_t>(i)] = static_cast<char>('0' + (zip % 10));
    zip /= 10;
  }
  return out;
}

} // namespace detail

[[nodiscard]] inline std::string
samplePostalCodeForState(random::Rng &rng, locale::us::State state) {
  const auto range = locale::us::zipRangeFor(state);
  const auto value = static_cast<std::uint32_t>(
      rng.uniformInt(static_cast<std::int64_t>(range.low),
                     static_cast<std::int64_t>(range.high) + 1));
  return detail::formatZip(value);
}

[[nodiscard]] inline locale::us::State
sampleUsStateByPopulation(random::Rng &rng) {
  std::uint64_t total = 0;
  for (std::size_t i = 0; i < locale::us::kStateCount; ++i) {
    total +=
        locale::us::populationBasisPoints(static_cast<locale::us::State>(i));
  }
  const auto draw = static_cast<std::uint64_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(total)));
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < locale::us::kStateCount; ++i) {
    acc += locale::us::populationBasisPoints(static_cast<locale::us::State>(i));
    if (draw < acc) {
      return static_cast<locale::us::State>(i);
    }
  }
  return locale::us::State::ca; // unreachable; falls through to most-populous
}

// --- Address -----------------------------------------------------

[[nodiscard]] inline entity::pii::Address
sampleAddress(random::Rng &rng, const LocalePool &pool) {
  entity::pii::Address out;
  out.streetIdx = detail::pickIndex(rng, pool.streets.size());
  out.zipTableIdx = detail::pickIndex(rng, pool.zipTable.size());
  return out;
}

// --- Locale assignment -------------------------------------------

struct LocaleMix {
  std::array<double, locale::kCountryCount> weights{};

  [[nodiscard]] static constexpr LocaleMix usOnly() noexcept {
    LocaleMix m{};
    m.weights[toIndex(locale::Country::us)] = 1.0;
    return m;
  }

  [[nodiscard]] static constexpr LocaleMix usBankDefault() noexcept {
    LocaleMix m{};
    using locale::Country;
    m.weights[toIndex(Country::us)] = 96.00;
    m.weights[toIndex(Country::mx)] = 1.00;
    m.weights[toIndex(Country::in)] = 0.60;
    m.weights[toIndex(Country::cn)] = 0.50;
    m.weights[toIndex(Country::kr)] = 0.30;
    m.weights[toIndex(Country::gb)] = 0.40;
    m.weights[toIndex(Country::ca)] = 0.40;
    m.weights[toIndex(Country::br)] = 0.20;
    m.weights[toIndex(Country::de)] = 0.10;
    m.weights[toIndex(Country::jp)] = 0.10;
    m.weights[toIndex(Country::fr)] = 0.10;
    m.weights[toIndex(Country::es)] = 0.10;
    m.weights[toIndex(Country::it)] = 0.10;
    m.weights[toIndex(Country::nl)] = 0.05;
    m.weights[toIndex(Country::au)] = 0.05;
    m.weights[toIndex(Country::ru)] = 0.05;
    return m;
  }
};

/// Sample one country from the configured `LocaleMix` via inverse
/// CDF. Caller is expected to have at least one positive weight.
[[nodiscard]] inline locale::Country sampleCountry(random::Rng &rng,
                                                   const LocaleMix &mix) {
  double total = 0.0;
  for (auto w : mix.weights)
    total += w;

  const double u = rng.nextDouble() * total;
  double acc = 0.0;
  for (std::size_t i = 0; i < mix.weights.size(); ++i) {
    acc += mix.weights[i];
    if (u < acc) {
      return static_cast<locale::Country>(i);
    }
  }
  return locale::kDefaultCountry;
}

} // namespace PhantomLedger::entities::synth::pii
