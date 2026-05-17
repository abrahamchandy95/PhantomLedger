#pragma once

#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/entities/products/obligation_stream.hpp"

namespace PhantomLedger::synth::products {

void appendObligation(
    ::PhantomLedger::entity::product::ObligationStream &stream,
    ::PhantomLedger::entity::product::ObligationEvent event);

} // namespace PhantomLedger::synth::products
