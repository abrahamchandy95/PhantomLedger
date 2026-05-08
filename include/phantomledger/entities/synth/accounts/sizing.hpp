#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include <cstdint>

namespace PhantomLedger::entities::synth::accounts {

struct Sizing {
  std::int32_t maxAccountsPerPerson = 3;

  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    r.check([&] {
      ::PhantomLedger::primitives::validate::ge("maxAccountsPerPerson",
                                                maxAccountsPerPerson, 1);
    });
  }
};

} // namespace PhantomLedger::entities::synth::accounts
