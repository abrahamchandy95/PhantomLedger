#pragma once

#include "phantomledger/entities/encoding/layout.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace PhantomLedger::encoding {

using identifiers::Bank;

namespace detail {

inline constexpr std::array<Layout, entity::landlord::kTypeCount> kExternal{
    kLandlordIndividual,
    kLandlordSmallLlc,
    kLandlordCorporate,
};

inline constexpr std::array<Layout, entity::landlord::kTypeCount> kInternal{
    kLandlordIndividualInternal,
    kLandlordSmallLlcInternal,
    kLandlordCorporateInternal,
};

} // namespace detail

[[nodiscard]] constexpr Layout layout(entity::landlord::Type type,
                                      Bank bank) noexcept {
  const auto index = ::PhantomLedger::taxonomies::enums::toIndex(type);

  return bank == Bank::internal ? detail::kInternal[index]
                                : detail::kExternal[index];
}

[[nodiscard]] constexpr Layout layout(entity::landlord::Type type) noexcept {
  return layout(type, Bank::external);
}

[[nodiscard]] inline std::string
landlordId(std::uint64_t number, entity::landlord::Type type, Bank bank) {
  return render(layout(type, bank), number);
}

[[nodiscard]] inline std::string landlordExternalId(std::uint64_t number) {
  return render(kLandlordExternal, number);
}

[[nodiscard]] inline std::string landlordIndividualId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::individual, Bank::external);
}

[[nodiscard]] inline std::string
landlordIndividualInternalId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::individual, Bank::internal);
}

[[nodiscard]] inline std::string landlordSmallLlcId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::llcSmall, Bank::external);
}

[[nodiscard]] inline std::string
landlordSmallLlcInternalId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::llcSmall, Bank::internal);
}

[[nodiscard]] inline std::string landlordCorporateId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::corporate, Bank::external);
}

[[nodiscard]] inline std::string
landlordCorporateInternalId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::corporate, Bank::internal);
}

[[nodiscard]] constexpr std::size_t
renderedSize(entity::landlord::Type type, Bank bank, std::uint64_t number) {
  return renderedSize(layout(type, bank), number);
}

inline std::size_t write(char *out, entity::landlord::Type type, Bank bank,
                         std::uint64_t number) {
  return write(out, layout(type, bank), number);
}

} // namespace PhantomLedger::encoding
