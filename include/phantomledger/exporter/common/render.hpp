#pragma once

#include "phantomledger/entities/encoding/device.hpp"
#include "phantomledger/entities/encoding/layout.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace PhantomLedger::exporter::common {

namespace entity = ::PhantomLedger::entity;
namespace encoding = ::PhantomLedger::encoding;
namespace devices = ::PhantomLedger::devices;
namespace locale = ::PhantomLedger::locale;

using CustomerId = encoding::RenderedId<24>;
using DeviceId = encoding::RenderedId<48>;

inline constexpr auto kUsCountry = locale::code(locale::Country::us);

[[nodiscard]] inline encoding::RenderedKey
renderKey(const entity::Key &k) noexcept {
  return encoding::format(k);
}

[[nodiscard]] inline CustomerId renderCustomerId(entity::PersonId person) {
  const auto key =
      entity::makeKey(entity::Role::customer, entity::Bank::internal, person);
  CustomerId out;
  out.length =
      static_cast<std::uint8_t>(encoding::write(out.bytes.data(), key));
  return out;
}

[[nodiscard]] inline DeviceId renderDeviceId(devices::Identity id) {
  using devices::OwnerType;

  DeviceId out;

  switch (id.ownerType) {
  case OwnerType::none:
    return out;

  case OwnerType::person: {
    const auto customer = entity::makeKey(entity::Role::customer,
                                          entity::Bank::internal, id.ownerId);
    out.length = static_cast<std::uint8_t>(
        encoding::write(out.bytes.data(), customer, id.slot));
    return out;
  }

  case OwnerType::ring:
    out.length = static_cast<std::uint8_t>(
        encoding::write(out.bytes.data(), encoding::kFraudDevice,
                        static_cast<std::uint32_t>(id.ownerId)));
    return out;

  case OwnerType::legitShared:
    out.length = static_cast<std::uint8_t>(
        encoding::write(out.bytes.data(), encoding::kLegitDevice, id.ownerId));
    return out;
  }

  throw std::logic_error("renderDeviceId: unknown OwnerType");
}

} // namespace PhantomLedger::exporter::common
