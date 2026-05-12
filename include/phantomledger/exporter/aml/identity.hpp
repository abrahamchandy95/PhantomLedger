#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::exporter::aml::identity {

template <std::size_t N> struct StackString {
  std::array<char, N> data{};
  std::uint8_t len = 0;

  [[nodiscard]] constexpr std::string_view view() const noexcept {
    return {data.data(), len};
  }
  [[nodiscard]] constexpr bool empty() const noexcept { return len == 0; }

  constexpr operator std::string_view() const noexcept { return view(); }
};

[[nodiscard]] std::string_view
customerType(::PhantomLedger::personas::Type persona) noexcept;

[[nodiscard]] std::string_view
maritalStatus(::PhantomLedger::entity::PersonId personId,
              ::PhantomLedger::personas::Type persona) noexcept;

[[nodiscard]] std::string_view
networthCode(::PhantomLedger::personas::Type persona) noexcept;

[[nodiscard]] std::string_view
incomeCode(::PhantomLedger::personas::Type persona) noexcept;

[[nodiscard]] std::string_view
occupation(::PhantomLedger::entity::PersonId personId,
           ::PhantomLedger::personas::Type persona) noexcept;

[[nodiscard]] std::string_view riskRating(bool isFraud, bool isMule,
                                          bool isVictim) noexcept;

[[nodiscard]] ::PhantomLedger::time::TimePoint
onboardingDate(::PhantomLedger::entity::PersonId personId,
               ::PhantomLedger::time::TimePoint simStart);

struct NameRecord {
  StackString<24> id;
  std::string_view firstName;
  std::string_view middleName;
  std::string_view lastName;
};

struct AddressRecord {
  StackString<32> id;
  std::string_view streetLine1;
  StackString<24> streetLine2;
  std::string_view city;
  std::string_view state;
  std::string_view postalCode;
  std::string_view country;
  std::string_view addressType;
  bool isHighRiskGeo = false;
};

[[nodiscard]] NameRecord
nameForPerson(::PhantomLedger::entity::PersonId personId,
              const ::PhantomLedger::entity::pii::Roster &pii,
              const ::PhantomLedger::entities::synth::pii::PoolSet &pools);

[[nodiscard]] NameRecord nameForCounterparty(
    std::string_view counterpartyId,
    const ::PhantomLedger::entities::synth::pii::LocalePool &usPool);

[[nodiscard]] NameRecord nameForBank(std::string_view bankId);

using RoutingNumber = ::PhantomLedger::encoding::RenderedId<16>;

[[nodiscard]] RoutingNumber
routingNumberForId(std::string_view bankId) noexcept;

[[nodiscard]] AddressRecord
addressForPerson(::PhantomLedger::entity::PersonId personId,
                 const ::PhantomLedger::entity::pii::Roster &pii,
                 const ::PhantomLedger::entities::synth::pii::PoolSet &pools);

[[nodiscard]] AddressRecord addressForCounterparty(
    std::string_view counterpartyId,
    const ::PhantomLedger::entities::synth::pii::LocalePool &usPool);

[[nodiscard]] AddressRecord
addressForBank(std::string_view bankId,
               const ::PhantomLedger::entities::synth::pii::LocalePool &usPool);

[[nodiscard]] StackString<24>
nameIdForPerson(::PhantomLedger::entity::PersonId personId) noexcept;

[[nodiscard]] StackString<32>
addressIdForPerson(::PhantomLedger::entity::PersonId personId) noexcept;

[[nodiscard]] StackString<24>
nameIdForCounterparty(std::string_view counterpartyId) noexcept;

[[nodiscard]] StackString<32>
addressIdForCounterparty(std::string_view counterpartyId) noexcept;

[[nodiscard]] StackString<24> nameIdForBank(std::string_view bankId) noexcept;

[[nodiscard]] StackString<32>
addressIdForBank(std::string_view bankId) noexcept;

} // namespace PhantomLedger::exporter::aml::identity
