#pragma once

#include "phantomledger/taxonomies/enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::counterparties {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

enum class Government : std::uint8_t {
  ssa = 0,
  disability = 1,
};

inline constexpr auto kGovernmentAccounts = std::to_array<Government>({
    Government::ssa,
    Government::disability,
});

inline constexpr std::size_t kGovernmentAccountCount =
    kGovernmentAccounts.size();

enum class Insurance : std::uint8_t {
  autoCarrier = 0,
  homeCarrier = 1,
  lifeCarrier = 2,
};

inline constexpr auto kInsuranceAccounts = std::to_array<Insurance>({
    Insurance::autoCarrier,
    Insurance::homeCarrier,
    Insurance::lifeCarrier,
});

inline constexpr std::size_t kInsuranceAccountCount = kInsuranceAccounts.size();

enum class Lending : std::uint8_t {
  mortgage = 0,
  autoLoan = 1,
  studentServicer = 2,
};

inline constexpr auto kLendingAccounts = std::to_array<Lending>({
    Lending::mortgage,
    Lending::autoLoan,
    Lending::studentServicer,
});

inline constexpr std::size_t kLendingAccountCount = kLendingAccounts.size();

enum class Tax : std::uint8_t {
  irsTreasury = 0,
};

inline constexpr auto kTaxAccounts = std::to_array<Tax>({
    Tax::irsTreasury,
});

inline constexpr std::size_t kTaxAccountCount = kTaxAccounts.size();

static_assert(enumTax::isIndexable(kGovernmentAccounts));
static_assert(enumTax::isIndexable(kInsuranceAccounts));
static_assert(enumTax::isIndexable(kLendingAccounts));
static_assert(enumTax::isIndexable(kTaxAccounts));

} // namespace PhantomLedger::counterparties
