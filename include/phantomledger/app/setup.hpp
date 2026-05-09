#pragma once

#include "phantomledger/app/options.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

namespace PhantomLedger::app::setup {

[[nodiscard]] ::PhantomLedger::entities::synth::pii::PoolSet
buildPoolSet(const RunOptions &opts,
             const ::PhantomLedger::entities::synth::pii::LocaleMix &mix);

[[nodiscard]] ::PhantomLedger::pipeline::stages::entities::EntitySynthesis
buildEntitySynthesis(
    const RunOptions &opts,
    const ::PhantomLedger::entities::synth::pii::PoolSet &pools,
    const ::PhantomLedger::entities::synth::pii::LocaleMix &mix,
    ::PhantomLedger::time::TimePoint simStart);

} // namespace PhantomLedger::app::setup
