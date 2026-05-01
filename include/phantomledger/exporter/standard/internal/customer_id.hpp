#pragma once

#include "phantomledger/entities/encoding/device.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/transactions/devices/identity.hpp"

#include <stdexcept>
#include <string>

namespace PhantomLedger::exporter::standard::detail {

[[nodiscard]] inline std::string
customerIdFor(::PhantomLedger::entity::PersonId p) {
  return ::PhantomLedger::encoding::format(::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::customer,
      ::PhantomLedger::entity::Bank::internal, p));
}

[[nodiscard]] inline std::string
renderDeviceId(::PhantomLedger::devices::Identity id) {
  using ::PhantomLedger::devices::OwnerType;
  switch (id.ownerType) {
  case OwnerType::person: {
    const auto customerKey = ::PhantomLedger::entity::makeKey(
        ::PhantomLedger::entity::Role::customer,
        ::PhantomLedger::entity::Bank::internal, id.ownerId);
    return ::PhantomLedger::encoding::deviceId(customerKey, id.slot);
  }
  case OwnerType::ring:
    return ::PhantomLedger::encoding::fraudDeviceId(
        static_cast<std::uint32_t>(id.ownerId));
  case OwnerType::legitShared:
    return ::PhantomLedger::encoding::legitDeviceId(id.ownerId);
  }
  throw std::logic_error("renderDeviceId: unknown OwnerType");
}

} // namespace PhantomLedger::exporter::standard::detail
