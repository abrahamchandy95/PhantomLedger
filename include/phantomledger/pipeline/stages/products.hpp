#pragma once

#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"

namespace PhantomLedger::pipeline::stages::products {

void synthesize(
    ::PhantomLedger::pipeline::Entities &entities,
    ::PhantomLedger::time::Window window,
    const ::PhantomLedger::pipeline::stages::entities::ProductSynthesis
        &products);

} // namespace PhantomLedger::pipeline::stages::products
