#pragma once

#include "phantomledger/taxonomies/personas/types.hpp"

#include <utility>

namespace PhantomLedger::personas {

template <class T> struct ByType {
  T student{};
  T retiree{};
  T freelancer{};
  T smallBusiness{};
  T highNetWorth{};
  T salaried{};

  [[nodiscard]] constexpr const T &get(Type type) const noexcept {
    switch (type) {
    case Type::student:
      return student;
    case Type::retiree:
      return retiree;
    case Type::freelancer:
      return freelancer;
    case Type::smallBusiness:
      return smallBusiness;
    case Type::highNetWorth:
      return highNetWorth;
    case Type::salaried:
      return salaried;
    }

    std::unreachable();
  }

  [[nodiscard]] constexpr T &get(Type type) noexcept {
    return const_cast<T &>(std::as_const(*this).get(type));
  }
};

using Rates = ByType<double>;

} // namespace PhantomLedger::personas
