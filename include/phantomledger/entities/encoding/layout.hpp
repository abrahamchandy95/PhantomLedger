#pragma once

#include <cstdint>
#include <string_view>

namespace PhantomLedger::encoding {

struct Layout {
  std::string_view prefix;
  std::uint8_t width = 0;
  bool allowZero = false;
};

inline constexpr Layout kInvalid{};

inline constexpr Layout kCustomer{"C", 11};
inline constexpr Layout kAccount{"A", 10};
inline constexpr Layout kMerchant{"M", 8};
inline constexpr Layout kMerchantExternal{"XM", 8};
inline constexpr Layout kCardLiability{"L", 9};

inline constexpr Layout kEmployer{"E", 8};
inline constexpr Layout kEmployerExternal{"XE", 8};

inline constexpr Layout kLandlordExternal{"XL", 8};
inline constexpr Layout kLandlordIndividualInternal{"LI", 7};
inline constexpr Layout kLandlordSmallLlcInternal{"LS", 7};
inline constexpr Layout kLandlordCorporateInternal{"LC", 7};
inline constexpr Layout kLandlordIndividual{"XLI", 7};
inline constexpr Layout kLandlordSmallLlc{"XLS", 7};
inline constexpr Layout kLandlordCorporate{"XLC", 7};

inline constexpr Layout kClient{"IC", 8};
inline constexpr Layout kClientExternal{"XC", 8};

inline constexpr Layout kPlatformExternal{"XP", 8};
inline constexpr Layout kProcessorExternal{"XS", 8};
inline constexpr Layout kFamilyExternal{"XF", 8};
inline constexpr Layout kBusinessInternal{"BOP", 7};
inline constexpr Layout kBusinessExternal{"XO", 8};
inline constexpr Layout kBrokerageInternal{"BRK", 7};
inline constexpr Layout kBrokerageExternal{"XB", 8};

inline constexpr Layout kFraudDevice{"FD", 4, true};

} // namespace PhantomLedger::encoding
