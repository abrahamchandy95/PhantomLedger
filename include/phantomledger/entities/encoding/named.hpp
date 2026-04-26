#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <cstdint>
#include <string>

namespace PhantomLedger::encoding {

[[nodiscard]] inline std::string customerId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::customer, entity::Bank::internal, number));
}

[[nodiscard]] inline std::string accountId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::account, entity::Bank::internal, number));
}

[[nodiscard]] inline std::string merchantId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::merchant, entity::Bank::internal, number));
}

[[nodiscard]] inline std::string merchantExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::merchant, entity::Bank::external, number));
}

[[nodiscard]] inline std::string cardLiabilityId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::card, entity::Bank::internal, number));
}

[[nodiscard]] inline std::string employerId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::employer, entity::Bank::internal, number));
}

[[nodiscard]] inline std::string employerExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::employer, entity::Bank::external, number));
}

[[nodiscard]] inline std::string clientId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::client, entity::Bank::internal, number));
}

[[nodiscard]] inline std::string clientExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::client, entity::Bank::external, number));
}

[[nodiscard]] inline std::string platformExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::platform, entity::Bank::external, number));
}

[[nodiscard]] inline std::string processorExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::processor, entity::Bank::external, number));
}

[[nodiscard]] inline std::string familyExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::family, entity::Bank::external, number));
}

[[nodiscard]] inline std::string businessExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::business, entity::Bank::external, number));
}

[[nodiscard]] inline std::string businessOperatingId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::business, entity::Bank::internal, number));
}

[[nodiscard]] inline std::string brokerageExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::brokerage, entity::Bank::external, number));
}

[[nodiscard]] inline std::string brokerageCustodyId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::brokerage, entity::Bank::internal, number));
}

} // namespace PhantomLedger::encoding
