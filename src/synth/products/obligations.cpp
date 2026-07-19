#include "phantomledger/synth/products/obligations.hpp"

#include <utility>

namespace PhantomLedger::synth::products {

void appendObligation(
    ::PhantomLedger::entity::product::ObligationStream &stream,
    ::PhantomLedger::entity::product::ObligationEvent event) {
  stream.append(std::move(event));
}

} // namespace PhantomLedger::synth::products
