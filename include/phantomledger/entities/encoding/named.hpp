#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <cstdint>

namespace PhantomLedger::encoding {

[[nodiscard]] inline RenderedKey customerId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::customer, entity::Bank::internal, number));
}

[[nodiscard]] inline RenderedKey accountId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::account, entity::Bank::internal, number));
}

[[nodiscard]] inline RenderedKey merchantId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::merchant, entity::Bank::internal, number));
}

[[nodiscard]] inline RenderedKey merchantExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::merchant, entity::Bank::external, number));
}

[[nodiscard]] inline RenderedKey cardLiabilityId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::card, entity::Bank::internal, number));
}

[[nodiscard]] inline RenderedKey employerId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::employer, entity::Bank::internal, number));
}

[[nodiscard]] inline RenderedKey employerExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::employer, entity::Bank::external, number));
}

[[nodiscard]] inline RenderedKey clientId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::client, entity::Bank::internal, number));
}

[[nodiscard]] inline RenderedKey clientExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::client, entity::Bank::external, number));
}

[[nodiscard]] inline RenderedKey platformExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::platform, entity::Bank::external, number));
}

[[nodiscard]] inline RenderedKey processorExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::processor, entity::Bank::external, number));
}

[[nodiscard]] inline RenderedKey familyExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::family, entity::Bank::external, number));
}

[[nodiscard]] inline RenderedKey businessExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::business, entity::Bank::external, number));
}

[[nodiscard]] inline RenderedKey businessOperatingId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::business, entity::Bank::internal, number));
}

[[nodiscard]] inline RenderedKey brokerageExternalId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::brokerage, entity::Bank::external, number));
}

[[nodiscard]] inline RenderedKey brokerageCustodyId(std::uint64_t number) {
  return format(
      entity::makeKey(entity::Role::brokerage, entity::Bank::internal, number));
}

} // namespace PhantomLedger::encoding
