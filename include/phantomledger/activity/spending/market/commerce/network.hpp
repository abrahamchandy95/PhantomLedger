#pragma once

#include "phantomledger/entities/merchants.hpp"

#include <cstdint>

namespace PhantomLedger::activity::spending::market::commerce {

class SocialGraph;

struct Network {
  const entity::merchant::Catalog *catalog = nullptr;
  const SocialGraph *social = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return catalog != nullptr && social != nullptr;
  }
};

} // namespace PhantomLedger::activity::spending::market::commerce
