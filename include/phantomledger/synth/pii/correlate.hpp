#pragma once

#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/entities/parties/pii.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/synth/pii/sharing.hpp"

namespace PhantomLedger::synth::pii {

class RingPiiCorrelator {
public:
  RingPiiCorrelator(const entity::person::Topology &topology,
                    const Sharing &config) noexcept;

  void apply(random::Rng &rng, entity::pii::Roster &roster) const;

private:
  const entity::person::Topology &topology_;
  const Sharing &config_;
};

void correlateRingPii(random::Rng &rng,
                      const entity::person::Topology &topology,
                      const Sharing &config, entity::pii::Roster &roster);

} // namespace PhantomLedger::synth::pii
