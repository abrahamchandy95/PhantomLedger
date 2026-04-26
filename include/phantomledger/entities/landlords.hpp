#pragma once

#include "phantomledger/entities/identifiers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::entity::landlord {

enum class Class : std::uint8_t {
  individual = 0,
  llcSmall = 1,
  corporate = 2,
};

inline constexpr std::size_t kClassCount = 3;

[[nodiscard]] constexpr std::size_t classIndex(Class kind) noexcept {
  return static_cast<std::size_t>(kind);
}

struct Record {
  entity::Key accountId;
  Class type = Class::individual;
};

struct Roster {
  std::vector<Record> records;
};

/// Inverted view of `Roster::records` keyed by Class. Each inner
/// vector holds offsets into Roster::records for landlords of that
/// class. Built once at synthesis time; immutable after.
struct Index {
  std::array<std::vector<std::uint32_t>, kClassCount> byClass;
};

} // namespace PhantomLedger::entity::landlord
