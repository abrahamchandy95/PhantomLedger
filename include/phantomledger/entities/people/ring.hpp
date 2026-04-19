#pragma once

#include <cstdint>

namespace PhantomLedger::entities::people {

struct Slice {
  std::uint32_t offset = 0;
  std::uint32_t size = 0;
};

struct Ring {
  std::uint32_t id = 0;
  Slice members;
  Slice frauds;
  Slice mules;
  Slice victims;
};

} // namespace PhantomLedger::entities::people
