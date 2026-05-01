#pragma once

#include "phantomledger/entities/identifiers.hpp"

namespace PhantomLedger::entities::synth::products::institutional {

inline constexpr std::uint64_t kBaseSerial = 1'000'000'000ULL;

[[nodiscard]] constexpr ::PhantomLedger::entity::Key mortgageLender() noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::business,
      ::PhantomLedger::entity::Bank::external, kBaseSerial + 1);
}

[[nodiscard]] constexpr ::PhantomLedger::entity::Key autoLender() noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::business,
      ::PhantomLedger::entity::Bank::external, kBaseSerial + 2);
}

[[nodiscard]] constexpr ::PhantomLedger::entity::Key
studentServicer() noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::business,
      ::PhantomLedger::entity::Bank::external, kBaseSerial + 3);
}

[[nodiscard]] constexpr ::PhantomLedger::entity::Key irsTreasury() noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::business,
      ::PhantomLedger::entity::Bank::external, kBaseSerial + 4);
}

[[nodiscard]] constexpr ::PhantomLedger::entity::Key autoCarrier() noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::business,
      ::PhantomLedger::entity::Bank::external, kBaseSerial + 5);
}

[[nodiscard]] constexpr ::PhantomLedger::entity::Key homeCarrier() noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::business,
      ::PhantomLedger::entity::Bank::external, kBaseSerial + 6);
}

[[nodiscard]] constexpr ::PhantomLedger::entity::Key lifeCarrier() noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::business,
      ::PhantomLedger::entity::Bank::external, kBaseSerial + 7);
}

} // namespace PhantomLedger::entities::synth::products::institutional
