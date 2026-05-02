#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>

namespace PhantomLedger::entities::synth::products {

namespace personas = ::PhantomLedger::personas;
namespace enums = ::PhantomLedger::taxonomies::enums;

static_assert(enums::isIndexable(personas::kTypes));

struct PerPersonaP {
  std::array<double, personas::kTypeCount> byType{};

  [[nodiscard]] constexpr double at(personas::Type type) const noexcept {
    return byType[enums::toIndex(type)];
  }
};

} // namespace PhantomLedger::entities::synth::products
