#pragma once

#include "phantomledger/entities/encoding/layout.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace PhantomLedger::encoding {

using identifiers::Bank;
using identifiers::Role;

namespace detail {

inline constexpr std::array<Layout, entity::landlord::kClassCount>
    kLandlordExternalLayouts{
        kLandlordIndividual,
        kLandlordSmallLlc,
        kLandlordCorporate,
    };

inline constexpr std::array<Layout, entity::landlord::kClassCount>
    kLandlordInternalLayouts{
        kLandlordIndividualInternal,
        kLandlordSmallLlcInternal,
        kLandlordCorporateInternal,
    };

} // namespace detail

[[nodiscard]] constexpr Layout layout(entity::landlord::Class kind,
                                      Bank bank) noexcept {
  const auto index = entity::landlord::classIndex(kind);
  return bank == Bank::internal ? detail::kLandlordInternalLayouts[index]
                                : detail::kLandlordExternalLayouts[index];
}

[[nodiscard]] constexpr Layout layout(entity::landlord::Class kind) noexcept {
  return layout(kind, Bank::external);
}

[[nodiscard]] inline std::string
landlordId(std::uint64_t number, entity::landlord::Class kind, Bank bank) {
  return render(layout(kind, bank), number);
}

/// Generic external landlord id, not tied to a specific landlord class.
[[nodiscard]] inline std::string landlordExternalId(std::uint64_t number) {
  return render(kLandlordExternal, number);
}

[[nodiscard]] inline std::string landlordIndividualId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Class::individual,
                    Bank::external);
}

[[nodiscard]] inline std::string
landlordIndividualInternalId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Class::individual,
                    Bank::internal);
}

[[nodiscard]] inline std::string landlordSmallLlcId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Class::llcSmall, Bank::external);
}

[[nodiscard]] inline std::string
landlordSmallLlcInternalId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Class::llcSmall, Bank::internal);
}

[[nodiscard]] inline std::string landlordCorporateId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Class::corporate, Bank::external);
}

[[nodiscard]] inline std::string
landlordCorporateInternalId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Class::corporate, Bank::internal);
}

[[nodiscard]] constexpr std::size_t
renderedSize(entity::landlord::Class kind, Bank bank, std::uint64_t number) {
  return renderedSize(layout(kind, bank), number);
}

inline std::size_t write(char *out, entity::landlord::Class kind, Bank bank,
                         std::uint64_t number) {
  return write(out, layout(kind, bank), number);
}

} // namespace PhantomLedger::encoding
