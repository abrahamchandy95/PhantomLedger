#pragma once

#include "phantomledger/taxonomies/personas/types.hpp"

namespace PhantomLedger::personas {

// --- Predicates --------------------------------------------------

[[nodiscard]] constexpr bool hasEarnedIncome(Type t) noexcept {
  switch (t) {
  case Type::student:
  case Type::retiree:
    return false;

  case Type::freelancer:
  case Type::smallBusiness:
  case Type::highNetWorth:
  case Type::salaried:
    return true;
  }

  return false;
}

} // namespace PhantomLedger::personas
