#pragma once

#include "phantomledger/taxonomies/personas/types.hpp"

namespace PhantomLedger::relationships::family::predicates {

[[nodiscard]] constexpr bool
isStudent(::PhantomLedger::personas::Type t) noexcept {
  return t == ::PhantomLedger::personas::Type::student;
}

[[nodiscard]] constexpr bool
isRetired(::PhantomLedger::personas::Type t) noexcept {
  return t == ::PhantomLedger::personas::Type::retiree;
}

[[nodiscard]] constexpr bool
isAdult(::PhantomLedger::personas::Type t) noexcept {
  return !isStudent(t);
}

[[nodiscard]] constexpr bool
canSupport(::PhantomLedger::personas::Type t) noexcept {
  using ::PhantomLedger::personas::Type;
  switch (t) {
  case Type::salaried:
  case Type::freelancer:
  case Type::smallBusiness:
  case Type::highNetWorth:
    return true;
  case Type::student:
  case Type::retiree:
    return false;
  }
  return false;
}

[[nodiscard]] constexpr bool
isWorkingAdult(::PhantomLedger::personas::Type t) noexcept {
  return canSupport(t);
}

} // namespace PhantomLedger::relationships::family::predicates
