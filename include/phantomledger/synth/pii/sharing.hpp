#pragma once
#include "phantomledger/primitives/validate/checks.hpp"
#include <cstddef>

namespace PhantomLedger::synth::pii {

struct Sharing {

  struct Probability {
    double phone = 0.40;
    double email = 0.40;
    double address = 0.25;
    double surname = 0.30;
  } probability;
  struct Coverage {
    double phone = 0.40;
    double email = 0.40;
    double address = 0.30;
    double surname = 0.40;
  } coverage;
  struct Limits {
    std::size_t minMembers = 2;
    std::size_t maxMembersPerValue = 6;
  } limits;
  bool enabled = true;
  bool includeVictims = false;
  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("sharing.probability.phone", probability.phone); });
    r.check([&] { v::unit("sharing.probability.email", probability.email); });
    r.check(
        [&] { v::unit("sharing.probability.address", probability.address); });
    r.check(
        [&] { v::unit("sharing.probability.surname", probability.surname); });
    r.check([&] { v::unit("sharing.coverage.phone", coverage.phone); });
    r.check([&] { v::unit("sharing.coverage.email", coverage.email); });
    r.check([&] { v::unit("sharing.coverage.address", coverage.address); });
    r.check([&] { v::unit("sharing.coverage.surname", coverage.surname); });
    r.check([&] {
      v::ge("sharing.limits.maxMembersPerValue", limits.maxMembersPerValue,
            std::size_t{2});
    });
  }
};
} // namespace PhantomLedger::synth::pii
