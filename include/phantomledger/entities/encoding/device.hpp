#pragma once

#include "phantomledger/entities/encoding/layout.hpp"
#include "phantomledger/entities/encoding/numeric.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifier/key.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PhantomLedger::encoding {

namespace detail {

inline void writeUnsignedToEnd(char *end, std::uint64_t value) noexcept {
  do {
    *--end = static_cast<char>('0' + (value % 10));
    value /= 10;
  } while (value != 0);
}

} // namespace detail

[[nodiscard]] inline std::string deviceId(std::string_view customerId,
                                          std::uint32_t index) {
  if (index == 0) {
    throw std::invalid_argument("Device index must be >= 1");
  }
  if (customerId.empty() || customerId.front() != 'C') {
    throw std::invalid_argument("deviceId expects a C-prefixed customer ID");
  }

  const auto indexDigits = digits(index);
  const auto customerDigits = customerId.size() - 1; // skip 'C'
  const auto totalSize =
      1 + customerDigits + 1 + indexDigits; // D + digits + _ + index

  std::string out;
  out.resize(totalSize);

  out[0] = 'D';

  if (customerDigits != 0) {
    std::memcpy(out.data() + 1, customerId.data() + 1, customerDigits);
  }

  out[1 + customerDigits] = '_';
  detail::writeUnsignedToEnd(out.data() + totalSize, index);

  return out;
}

[[nodiscard]] inline std::string deviceId(const entity::Key &customer,
                                          std::uint32_t index) {
  if (customer.role != entity::Role::customer ||
      customer.bank != entity::Bank::internal) {
    throw std::invalid_argument(
        "deviceId expects an internal customer identity");
  }
  if (index == 0) {
    throw std::invalid_argument("Device index must be >= 1");
  }

  const auto customerSize = renderedSize(customer);
  const auto indexDigits = digits(index);
  const auto totalSize =
      customerSize + 1 + indexDigits; // customer id + _ + index

  std::string out;
  out.resize(totalSize);

  write(out.data(), customer);
  out[0] = 'D';
  out[customerSize] = '_';
  detail::writeUnsignedToEnd(out.data() + totalSize, index);

  return out;
}

[[nodiscard]] inline std::string fraudDeviceId(std::uint32_t ringId) {
  return render(kFraudDevice, ringId);
}

} // namespace PhantomLedger::encoding
