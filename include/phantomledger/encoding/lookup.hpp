#pragma once

#include "phantomledger/encoding/layout.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace PhantomLedger::encoding {

using identifiers::Bank;
using identifiers::Role;

namespace enumTax = ::PhantomLedger::taxonomies::enums;

inline constexpr std::size_t kRoleCount = identifiers::kRoleCount;

inline constexpr std::size_t kBankCount = enumTax::toIndex(Bank::external) + 1;

[[nodiscard]] constexpr std::size_t toIndex(Role role) noexcept {
  return enumTax::toIndex(role);
}

[[nodiscard]] constexpr std::size_t toIndex(Bank bank) noexcept {
  return enumTax::toIndex(bank);
}

namespace detail {

struct IdentityLayoutEntry {
  Role role;
  Bank bank;
  Layout layout;
};

inline constexpr auto kIdentityLayoutEntries =
    std::to_array<IdentityLayoutEntry>({
        {Role::customer, Bank::internal, kCustomer},
        {Role::account, Bank::internal, kAccount},

        {Role::merchant, Bank::internal, kMerchant},
        {Role::merchant, Bank::external, kMerchantExternal},

        {Role::employer, Bank::internal, kEmployer},
        {Role::employer, Bank::external, kEmployerExternal},

        // Generic key lookup only.
        // Landlord typed lookup lives in landlord.hpp.
        {Role::landlord, Bank::internal, kLandlordIndividualInternal},
        {Role::landlord, Bank::external, kLandlordExternal},

        {Role::client, Bank::internal, kClient},
        {Role::client, Bank::external, kClientExternal},

        {Role::platform, Bank::external, kPlatformExternal},
        {Role::processor, Bank::external, kProcessorExternal},
        {Role::family, Bank::external, kFamilyExternal},

        {Role::business, Bank::internal, kBusinessInternal},
        {Role::business, Bank::external, kBusinessExternal},

        {Role::brokerage, Bank::internal, kBrokerageInternal},
        {Role::brokerage, Bank::external, kBrokerageExternal},

        {Role::card, Bank::internal, kCardLiability},
    });

[[nodiscard]] consteval bool entriesAreValid() {
  std::array<std::uint8_t, kRoleCount * kBankCount> seen{};

  for (const auto entry : kIdentityLayoutEntries) {
    const auto roleIndex = toIndex(entry.role);
    const auto bankIndex = toIndex(entry.bank);

    if (roleIndex >= kRoleCount || bankIndex >= kBankCount) {
      return false;
    }

    if (!identifiers::allows(entry.role, entry.bank)) {
      return false;
    }

    if (entry.layout.prefix.empty()) {
      return false;
    }

    const auto flatIndex = roleIndex * kBankCount + bankIndex;
    ++seen[flatIndex];

    if (seen[flatIndex] != 1) {
      return false;
    }
  }

  return true;
}

[[nodiscard]] consteval auto identityLayoutTable() {
  std::array<std::array<Layout, kBankCount>, kRoleCount> out{};

  for (auto &row : out) {
    row.fill(kInvalid);
  }

  for (const auto entry : kIdentityLayoutEntries) {
    out[toIndex(entry.role)][toIndex(entry.bank)] = entry.layout;
  }

  return out;
}

static_assert(entriesAreValid());

inline constexpr auto kIdentityLayouts = identityLayoutTable();

} // namespace detail

[[nodiscard]] constexpr Layout layout(const entity::Key &id) noexcept {
  return detail::kIdentityLayouts[toIndex(id.role)][toIndex(id.bank)];
}

[[nodiscard]] inline Layout checkedLayout(const entity::Key &id) {
  const auto spec = layout(id);
  if (spec.prefix.empty()) {
    throw std::invalid_argument("invalid role/bank combination");
  }
  return spec;
}

} // namespace PhantomLedger::encoding
