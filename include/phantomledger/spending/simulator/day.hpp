#pragma once

#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/spending/config/burst.hpp"
#include "phantomledger/spending/config/exploration.hpp"
#include "phantomledger/spending/config/liquidity.hpp"
#include "phantomledger/spending/config/picking.hpp"
#include "phantomledger/spending/dynamics/config.hpp"
#include "phantomledger/spending/dynamics/population/advance.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/spending/simulator/plan.hpp"
#include "phantomledger/spending/simulator/state.hpp"

#include <cstdint>

namespace PhantomLedger::spending::simulator {

void runDay(const market::Market &market, Engine &engine, const RunPlan &plan,
            RunState &state, dynamics::population::Cohort &cohort,
            const dynamics::Config &dynamicsCfg,
            const config::BurstBehavior &burst,
            const config::ExplorationHabits &exploration,
            const config::LiquidityConstraints &liquidity,
            const math::seasonal::Config &seasonal, std::uint32_t dayIndex);

} // namespace PhantomLedger::spending::simulator
