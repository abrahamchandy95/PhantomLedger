#pragma once

#include "phantomledger/entities/encoding/numeric.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <cstdint>
#include <stdexcept>

namespace PhantomLedger::encoding {

namespace detail {

inline void writeUnsignedToEnd(char *end, std::uint64_t value) noexcept {
  do {
    *--end = static_cast<char>('0' + (value % 10));
    value /= 10;
  } while (value != 0);
}

inline void validateCustomerForDevice(const entity::Key &customer) {
  if (customer.role != entity::Role::customer ||
      customer.bank != entity::Bank::internal) {
    throw std::invalid_argument(
        "device ID expects an internal customer identity");
  }
}

inline void validateDeviceIndex(std::uint32_t index) {
  if (index == 0) {
    throw std::invalid_argument("Device index must be >= 1");
  }
}

} // namespace detail

[[nodiscard]] inline std::size_t
renderedDeviceIdSize(const entity::Key &customer, std::uint32_t index) {
  detail::validateCustomerForDevice(customer);
  detail::validateDeviceIndex(index);
  return renderedSize(customer) + 1 + digits(index);
}

inline std::size_t write(char *out, const entity::Key &customer,
                         std::uint32_t index) {
  detail::validateCustomerForDevice(customer);
  detail::validateDeviceIndex(index);

  const auto customerSize = renderedSize(customer);
  const auto totalSize = customerSize + 1 + digits(index);

  write(out, customer);
  out[0] = 'D';
  out[customerSize] = '_';
  detail::writeUnsignedToEnd(out + totalSize, index);

  return totalSize;
}

} // namespace PhantomLedger::encoding
