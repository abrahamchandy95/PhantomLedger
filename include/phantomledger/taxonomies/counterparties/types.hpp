#pragma once

#include "phantomledger/taxonomies/enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

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

// The six contract markets whose counterparty is drawn per contract from a
// national provider pool (institutional-providers-2026-09). Each market is a
// separate key block (entities/counterparties/providers.hpp), and marketName
// is also the name of the market's provider-draw RNG lane, so renaming one is
// a model change.
enum class Market : std::uint8_t {
  mortgage = 0,
  autoLoan = 1,
  studentLoan = 2,
  autoInsurance = 3,
  homeInsurance = 4,
  lifeInsurance = 5,
};

inline constexpr auto kMarkets = std::to_array<Market>({
    Market::mortgage,
    Market::autoLoan,
    Market::studentLoan,
    Market::autoInsurance,
    Market::homeInsurance,
    Market::lifeInsurance,
});

inline constexpr std::size_t kMarketCount = kMarkets.size();

[[nodiscard]] constexpr std::string_view marketName(Market market) noexcept {
  switch (market) {
  case Market::mortgage:
    return "mortgage";
  case Market::autoLoan:
    return "auto_loan";
  case Market::studentLoan:
    return "student_loan";
  case Market::autoInsurance:
    return "auto_insurance";
  case Market::homeInsurance:
    return "home_insurance";
  case Market::lifeInsurance:
    return "life_insurance";
  }
  return "unknown";
}

enum class Tax : std::uint8_t {
  irsTreasury = 0,
};

inline constexpr auto kTaxAccounts = std::to_array<Tax>({
    Tax::irsTreasury,
});

inline constexpr std::size_t kTaxAccountCount = kTaxAccounts.size();

static_assert(enumTax::isIndexable(kGovernmentAccounts));
static_assert(enumTax::isIndexable(kMarkets));
static_assert(enumTax::isIndexable(kTaxAccounts));

} // namespace PhantomLedger::counterparties
