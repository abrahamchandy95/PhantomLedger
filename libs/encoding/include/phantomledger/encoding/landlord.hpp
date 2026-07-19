#pragma once

#include "phantomledger/encoding/layout.hpp"
#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

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

[[nodiscard]] inline RenderedKey renderToBuffer(Layout layout,
                                                std::uint64_t number) {
  RenderedKey out;
  out.length = static_cast<std::uint8_t>(
      ::PhantomLedger::encoding::write(out.bytes.data(), layout, number));
  return out;
}

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

[[nodiscard]] inline RenderedKey
landlordId(std::uint64_t number, entity::landlord::Type type, Bank bank) {
  return detail::renderToBuffer(layout(type, bank), number);
}

[[nodiscard]] inline RenderedKey landlordExternalId(std::uint64_t number) {
  return detail::renderToBuffer(kLandlordExternal, number);
}

[[nodiscard]] inline RenderedKey landlordIndividualId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::individual, Bank::external);
}

[[nodiscard]] inline RenderedKey
landlordIndividualInternalId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::individual, Bank::internal);
}

[[nodiscard]] inline RenderedKey landlordSmallLlcId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::llcSmall, Bank::external);
}

[[nodiscard]] inline RenderedKey
landlordSmallLlcInternalId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::llcSmall, Bank::internal);
}

[[nodiscard]] inline RenderedKey landlordCorporateId(std::uint64_t number) {
  return landlordId(number, entity::landlord::Type::corporate, Bank::external);
}

[[nodiscard]] inline RenderedKey
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
