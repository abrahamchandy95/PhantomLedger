#pragma once

#include "phantomledger/entities/encoding/layout.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifier/bank.hpp"
#include "phantomledger/entities/landlords/class.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace PhantomLedger::encoding {

namespace detail {

inline constexpr std::array<Layout, entities::landlords::kClassCount>
    kLandlordExternalLayouts{
        kLandlordIndividual,
        kLandlordSmallLlc,
        kLandlordCorporate,
    };

inline constexpr std::array<Layout, entities::landlords::kClassCount>
    kLandlordInternalLayouts{
        kLandlordIndividualInternal,
        kLandlordSmallLlcInternal,
        kLandlordCorporateInternal,
    };

} // namespace detail

[[nodiscard]] constexpr Layout
layout(entities::landlords::Class kind,
       entities::identifier::Bank bank) noexcept {
  const auto index = entities::landlords::classIndex(kind);
  return bank == entities::identifier::Bank::internal
             ? detail::kLandlordInternalLayouts[index]
             : detail::kLandlordExternalLayouts[index];
}

[[nodiscard]] constexpr Layout
layout(entities::landlords::Class kind) noexcept {
  return layout(kind, entities::identifier::Bank::external);
}

[[nodiscard]] inline std::string landlordId(std::uint64_t number,
                                            entities::landlords::Class kind,
                                            entities::identifier::Bank bank) {
  return render(layout(kind, bank), number);
}

/// Generic external landlord id, not tied to a specific landlord class.
[[nodiscard]] inline std::string landlordExternalId(std::uint64_t number) {
  return render(kLandlordExternal, number);
}

[[nodiscard]] inline std::string landlordIndividualId(std::uint64_t number) {
  return landlordId(number, entities::landlords::Class::individual,
                    entities::identifier::Bank::external);
}

[[nodiscard]] inline std::string
landlordIndividualInternalId(std::uint64_t number) {
  return landlordId(number, entities::landlords::Class::individual,
                    entities::identifier::Bank::internal);
}

[[nodiscard]] inline std::string landlordSmallLlcId(std::uint64_t number) {
  return landlordId(number, entities::landlords::Class::llcSmall,
                    entities::identifier::Bank::external);
}

[[nodiscard]] inline std::string
landlordSmallLlcInternalId(std::uint64_t number) {
  return landlordId(number, entities::landlords::Class::llcSmall,
                    entities::identifier::Bank::internal);
}

[[nodiscard]] inline std::string landlordCorporateId(std::uint64_t number) {
  return landlordId(number, entities::landlords::Class::corporate,
                    entities::identifier::Bank::external);
}

[[nodiscard]] inline std::string
landlordCorporateInternalId(std::uint64_t number) {
  return landlordId(number, entities::landlords::Class::corporate,
                    entities::identifier::Bank::internal);
}

[[nodiscard]] constexpr std::size_t
renderedSize(entities::landlords::Class kind, entities::identifier::Bank bank,
             std::uint64_t number) {
  return renderedSize(layout(kind, bank), number);
}

inline std::size_t write(char *out, entities::landlords::Class kind,
                         entities::identifier::Bank bank,
                         std::uint64_t number) {
  return write(out, layout(kind, bank), number);
}

} // namespace PhantomLedger::encoding
