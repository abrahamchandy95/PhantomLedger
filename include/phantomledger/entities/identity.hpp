#pragma once
/*
 * Deep identity — name, SSN, DOB, address.
 */

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace PhantomLedger::entity::identity {

// --- Name --------------------------------------------------------

/// First/last name carried as small indices into the constexpr name
/// pools defined in `entities/synth/identity/name.hpp`. Resolve via
/// `synth::identity::firstName(name)` / `synth::identity::lastName(name)`.
struct Name {
  std::uint16_t firstIdx = 0;
  std::uint16_t lastIdx = 0;
};

// --- SSN ---------------------------------------------------------

inline constexpr std::size_t kSsnSize = 11; // "666-NN-NNNN"

/// Synthetic SSN in canonical "AAA-GG-SSSS" format. The area number
/// is always "666" — never issued by SSA, so format-valid but
/// collision-free with real values.
struct Ssn {
  std::array<char, kSsnSize> bytes{};

  [[nodiscard]] constexpr std::string_view view() const noexcept {
    return {bytes.data(), bytes.size()};
  }
};

// --- DOB ---------------------------------------------------------

/// Packed date of birth. Distinct from `time::CalendarDate` only for
/// storage compactness — convertible both ways.
struct Dob {
  std::int16_t year = 0;
  std::uint8_t month = 0; // 1..12
  std::uint8_t day = 0;   // 1..31

  [[nodiscard]] constexpr time::CalendarDate toCalendarDate() const noexcept {
    return {static_cast<int>(year), static_cast<unsigned>(month),
            static_cast<unsigned>(day)};
  }

  [[nodiscard]] static constexpr Dob
  fromCalendarDate(time::CalendarDate d) noexcept {
    return {static_cast<std::int16_t>(d.year),
            static_cast<std::uint8_t>(d.month),
            static_cast<std::uint8_t>(d.day)};
  }
};

// --- Address -----------------------------------------------------

/// Street, city, state, ZIP, country — fully pool-indexed.
///
/// `streetNumber`     literal house number, 100..9999 typical.
/// `streetNameIdx`    -> `synth::identity::kStreetNamePool`
/// `streetSuffixIdx`  -> `synth::identity::kStreetSuffixPool`
/// `zipTableIdx`      -> `synth::identity::kZipTable` (zip + city + state)
/// `country`          discriminator for future internationalization.
struct Address {
  std::uint16_t streetNumber = 0;
  std::uint8_t streetNameIdx = 0;
  std::uint8_t streetSuffixIdx = 0;
  std::uint16_t zipTableIdx = 0;
  locale::Country country = locale::kDefaultCountry;
};

// --- Record / Roster ---------------------------------------------

struct Record {
  Name name{};
  Ssn ssn{};
  Dob dob{};
  Address address{};
};

/// Per-person deep-identity store keyed by 1-based `PersonId`:
/// `records[p - 1]` holds person `p`'s row.
struct Roster {
  std::vector<Record> records;

  [[nodiscard]] std::size_t size() const noexcept { return records.size(); }
  [[nodiscard]] bool empty() const noexcept { return records.empty(); }

  [[nodiscard]] const Record &at(PersonId p) const noexcept {
    return records[p - 1];
  }

  [[nodiscard]] const Name &name(PersonId p) const noexcept {
    return records[p - 1].name;
  }
  [[nodiscard]] const Ssn &ssn(PersonId p) const noexcept {
    return records[p - 1].ssn;
  }
  [[nodiscard]] const Dob &dob(PersonId p) const noexcept {
    return records[p - 1].dob;
  }
  [[nodiscard]] const Address &address(PersonId p) const noexcept {
    return records[p - 1].address;
  }
};

} // namespace PhantomLedger::entity::identity
