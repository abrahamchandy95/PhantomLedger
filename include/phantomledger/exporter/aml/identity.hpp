#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <string>
#include <string_view>

namespace PhantomLedger::exporter::aml::identity {

[[nodiscard]] std::string customerType(::PhantomLedger::personas::Type persona);
[[nodiscard]] std::string
maritalStatus(::PhantomLedger::entity::PersonId personId,
              ::PhantomLedger::personas::Type persona);
[[nodiscard]] std::string networthCode(::PhantomLedger::personas::Type persona);
[[nodiscard]] std::string incomeCode(::PhantomLedger::personas::Type persona);
[[nodiscard]] std::string occupation(::PhantomLedger::entity::PersonId personId,
                                     ::PhantomLedger::personas::Type persona);
[[nodiscard]] std::string riskRating(bool isFraud, bool isMule, bool isVictim);
[[nodiscard]] ::PhantomLedger::time::TimePoint
onboardingDate(::PhantomLedger::entity::PersonId personId,
               ::PhantomLedger::time::TimePoint simStart);

struct NameRecord {
  std::string id;
  std::string firstName;
  std::string middleName;
  std::string lastName;
};

[[nodiscard]] NameRecord
nameForPerson(::PhantomLedger::entity::PersonId personId);

[[nodiscard]] NameRecord nameForCounterparty(std::string_view counterpartyId);

[[nodiscard]] NameRecord nameForBank(std::string_view bankId);

[[nodiscard]] std::string routingNumberForId(std::string_view bankId);

struct AddressRecord {
  std::string id;
  std::string streetLine1;
  std::string streetLine2;
  std::string city;
  std::string state;
  std::string postalCode;
  std::string country;
  std::string addressType;
  bool isHighRiskGeo = false;
};

[[nodiscard]] AddressRecord
addressForPerson(::PhantomLedger::entity::PersonId personId);

[[nodiscard]] AddressRecord
addressForCounterparty(std::string_view counterpartyId);

[[nodiscard]] AddressRecord addressForBank(std::string_view bankId);

} // namespace PhantomLedger::exporter::aml::identity
