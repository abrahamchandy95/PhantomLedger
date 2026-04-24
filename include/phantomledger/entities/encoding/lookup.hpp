#pragma once

#include "phantomledger/entities/encoding/layout.hpp"
#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace PhantomLedger::encoding {

using identifiers::Bank;
using identifiers::Role;

inline constexpr std::size_t kRoleCount =
    static_cast<std::size_t>(Role::brokerage) + 1;

inline constexpr std::size_t kBankCount =
    static_cast<std::size_t>(Bank::external) + 1;

[[nodiscard]] constexpr std::size_t toIndex(Role role) noexcept {
  return static_cast<std::size_t>(role);
}

[[nodiscard]] constexpr std::size_t toIndex(Bank bank) noexcept {
  return static_cast<std::size_t>(bank);
}

// Generic key lookup only.
// Landlord typed lookup lives in landlord.hpp.
inline constexpr std::array<std::array<Layout, kBankCount>, kRoleCount>
    kIdentityLayouts{{
        /* customer   */ {kCustomer, kInvalid},
        /* account    */ {kAccount, kInvalid},
        /* merchant   */ {kMerchant, kMerchantExternal},
        /* employer   */ {kEmployer, kEmployerExternal},
        /* landlord   */ {kLandlordIndividualInternal, kLandlordExternal},
        /* client     */ {kClient, kClientExternal},
        /* platform   */ {kInvalid, kPlatformExternal},
        /* processor  */ {kInvalid, kProcessorExternal},
        /* family     */ {kInvalid, kFamilyExternal},
        /* business   */ {kBusinessInternal, kBusinessExternal},
        /* brokerage  */ {kBrokerageInternal, kBrokerageExternal},
    }};

[[nodiscard]] constexpr Layout layout(const entity::Key &id) noexcept {
  return kIdentityLayouts[toIndex(id.role)][toIndex(id.bank)];
}

[[nodiscard]] inline Layout checkedLayout(const entity::Key &id) {
  const auto spec = layout(id);
  if (spec.prefix.empty()) {
    throw std::invalid_argument("invalid role/bank combination");
  }
  return spec;
}

} // namespace PhantomLedger::encoding
