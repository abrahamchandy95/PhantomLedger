#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace PhantomLedger::entity::pii {

// --- Scale knob --------------------------------------------------

inline constexpr std::size_t kCustomerDigitWidth = 10;

// --- Layout constants --------------------------------------------

inline constexpr std::size_t kPhoneSize = 16;
inline constexpr std::size_t kSsnSize = 14;

inline constexpr std::string_view kEmailPrefix = "userc";        // 5 bytes
inline constexpr std::string_view kEmailSuffix = "@example.com"; // 12 bytes

inline constexpr std::size_t kEmailSize =
    kEmailPrefix.size() + kCustomerDigitWidth + kEmailSuffix.size();

inline constexpr std::uint16_t kNoMiddleIdx = 0xFFFF;

struct Phone {
  std::array<char, kPhoneSize> bytes{};

  [[nodiscard]] constexpr std::string_view view() const noexcept {
    std::size_t n = 0;
    while (n < kPhoneSize && bytes[n] != '\0') {
      ++n;
    }
    return {bytes.data(), n};
  }
};

struct Email {
  std::array<char, kEmailSize> bytes{};

  [[nodiscard]] constexpr std::string_view view() const noexcept {
    return {bytes.data(), bytes.size()};
  }
};

struct Name {
  std::uint16_t firstIdx = 0;
  std::uint16_t middleIdx = kNoMiddleIdx;
  std::uint16_t lastIdx = 0;
};

struct Ssn {
  std::array<char, kSsnSize> bytes{};

  [[nodiscard]] constexpr std::string_view view() const noexcept {
    std::size_t n = 0;
    while (n < kSsnSize && bytes[n] != '\0') {
      ++n;
    }
    return {bytes.data(), n};
  }
};

/// Packed date of birth.
struct Dob {
  std::int16_t year = 0;
  std::uint8_t month = 0;
  std::uint8_t day = 0;
};

struct Address {
  std::uint32_t streetIdx = 0;
  std::uint32_t zipTableIdx = 0;
};

// --- Record / Roster ---------------------------------------------

struct Record {
  locale::Country country = locale::kDefaultCountry;
  Phone phone{};
  Email email{};
  Name name{};
  Ssn ssn{};
  Dob dob{};
  Address address{};
};

struct Roster {
  std::vector<Record> records;

  [[nodiscard]] std::size_t size() const noexcept { return records.size(); }
  [[nodiscard]] bool empty() const noexcept { return records.empty(); }

  [[nodiscard]] const Record &at(PersonId p) const noexcept {
    return records[p - 1];
  }
};

} // namespace PhantomLedger::entity::pii
