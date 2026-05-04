#include "phantomledger/entities/synth/products/obligation_emission.hpp"

#include <utility>

namespace PhantomLedger::entities::synth::products {

void appendObligation(
    ::PhantomLedger::entity::product::ObligationStream &stream,
    ::PhantomLedger::entity::product::ObligationEvent event) {
  stream.append(std::move(event));
}

} // namespace PhantomLedger::entities::synth::products
