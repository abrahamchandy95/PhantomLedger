#pragma once

#include "phantomledger/app/options.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"

namespace PhantomLedger::app::setup {

[[nodiscard]] ::PhantomLedger::synth::pii::PoolSet
buildPoolSet(const RunOptions &opts,
             const ::PhantomLedger::synth::pii::LocaleMix &mix);

[[nodiscard]] ::PhantomLedger::pipeline::stages::entities::EntitySynthesis
buildEntitySynthesis(const RunOptions &opts,
                     const ::PhantomLedger::synth::pii::PoolSet &pools,
                     const ::PhantomLedger::synth::pii::LocaleMix &mix,
                     ::PhantomLedger::time::TimePoint simStart);

} // namespace PhantomLedger::app::setup
