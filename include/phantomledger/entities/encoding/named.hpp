#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifier/make.hpp"

#include <cstdint>
#include <string>

namespace PhantomLedger::encoding {

[[nodiscard]] inline std::string customerId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::customer,
                                           entities::identifier::Bank::internal,
                                           number));
}

[[nodiscard]] inline std::string accountId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::account,
                                           entities::identifier::Bank::internal,
                                           number));
}

[[nodiscard]] inline std::string merchantId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::merchant,
                                           entities::identifier::Bank::internal,
                                           number));
}

[[nodiscard]] inline std::string merchantExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::merchant,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string employerId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::employer,
                                           entities::identifier::Bank::internal,
                                           number));
}

[[nodiscard]] inline std::string employerExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::employer,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string clientId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::client,
                                           entities::identifier::Bank::internal,
                                           number));
}

[[nodiscard]] inline std::string clientExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::client,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string platformExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::platform,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string processorExternalId(std::uint64_t number) {
  return format(
      entities::identifier::make(entities::identifier::Role::processor,
                                 entities::identifier::Bank::external, number));
}

[[nodiscard]] inline std::string familyExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::family,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string businessExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::business,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string businessOperatingId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::business,
                                           entities::identifier::Bank::internal,
                                           number));
}

[[nodiscard]] inline std::string brokerageExternalId(std::uint64_t number) {
  return format(
      entities::identifier::make(entities::identifier::Role::brokerage,
                                 entities::identifier::Bank::external, number));
}

[[nodiscard]] inline std::string brokerageCustodyId(std::uint64_t number) {
  return format(
      entities::identifier::make(entities::identifier::Role::brokerage,
                                 entities::identifier::Bank::internal, number));
}

} // namespace PhantomLedger::encoding
