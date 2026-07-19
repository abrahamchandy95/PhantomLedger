#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::locale::us {

namespace detail {

inline constexpr auto kBanks = std::to_array<std::string_view>({
    "JPMorgan Chase Bank",
    "Bank of America",
    "Wells Fargo Bank",
    "Citibank",
    "U.S. Bank",
    "PNC Bank",
    "Goldman Sachs Bank USA",
    "Truist Bank",
    "Capital One",
    "TD Bank",

    "Fifth Third Bank",
    "KeyBank",
    "M&T Bank",
    "Huntington National Bank",
    "Citizens Bank",
    "Regions Bank",
    "Ally Bank",
    "BMO Bank",
    "Northern Trust",
    "State Street Bank",
    "Comerica Bank",
    "Zions Bank",
    "First Citizens Bank",
    "Webster Bank",
    "Frost Bank",
    "Texas Capital Bank",
    "East West Bank",
    "Western Alliance Bank",
    "Pacific Premier Bank",
    "Cathay Bank",
    "Valley National Bank",
    "BankUnited",
    "New York Community Bank",
    "City National Bank",

    "Charles Schwab Bank",
    "Discover Bank",
    "Synchrony Bank",
    "Santander Bank, N.A.",
    "HSBC Bank USA",
    "BNY Mellon",
    "USAA Federal Savings Bank",
    "Apple Bank for Savings",

    "Navy Federal Credit Union",
    "State Employees' Credit Union",
    "PenFed Credit Union",
    "SchoolsFirst Federal Credit Union",
    "Alliant Credit Union",
});

} // namespace detail

inline constexpr std::size_t kBankCount = detail::kBanks.size();

[[nodiscard]] inline constexpr std::string_view
bankName(std::size_t index) noexcept {
  return detail::kBanks[index];
}

[[nodiscard]] inline constexpr std::string_view
bankNameBySeed(std::uint64_t seed) noexcept {
  return detail::kBanks[seed % kBankCount];
}

} // namespace PhantomLedger::locale::us
