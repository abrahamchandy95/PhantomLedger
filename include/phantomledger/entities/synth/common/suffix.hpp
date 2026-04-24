#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entropy/crypto/blake2b.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace PhantomLedger::entities::synth::common {

using identifiers::Bank;
using identifiers::Role;

[[nodiscard]] inline std::uint64_t
readLe64(const std::array<std::uint8_t, 8> &bytes) noexcept {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    value |= static_cast<std::uint64_t>(bytes[i]) << (i * 8U);
  }
  return value;
}

[[nodiscard]] inline std::uint64_t suffix(std::string_view scope,
                                          entity::PersonId person) {
  std::array<char, 64> buffer{};
  std::size_t pos = 0;

  std::memcpy(buffer.data() + pos, scope.data(), scope.size());
  pos += scope.size();
  buffer[pos++] = '|';

  pos +=
      encoding::write(buffer.data() + pos,
                      entity::makeKey(Role::customer, Bank::internal, person));

  std::array<std::uint8_t, 8> digest{};
  const bool ok =
      crypto::blake2b::digest(buffer.data(), pos, digest.data(), digest.size());
  if (!ok) {
    throw std::runtime_error("blake2b digest failed");
  }

  return readLe64(digest);
}

[[nodiscard]] inline std::uint64_t familySuffix(entity::PersonId person) {
  std::array<char, 64> buffer{};
  const auto written = encoding::write(
      buffer.data(), entity::makeKey(Role::customer, Bank::internal, person));

  std::array<std::uint8_t, 8> digest{};
  const bool ok = crypto::blake2b::digest(buffer.data(), written, digest.data(),
                                          digest.size());
  if (!ok) {
    throw std::runtime_error("blake2b digest failed");
  }

  return readLe64(digest);
}

} // namespace PhantomLedger::entities::synth::common
