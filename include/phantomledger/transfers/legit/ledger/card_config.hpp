#pragma once
//
// phantomledger/transfers/legit/ledger/card_config.hpp
//
// Card-lifecycle configuration for the spending engine, shared by both
// architectures: addSpending() feeds it to the one-shot Simulator, the
// windowed composition feeds it to the persistent Session via
// SessionInputs. One constructor, one behavior — the configs cannot
// drift.
//
// This lives in its own header (rather than passes.hpp) because the
// return type drags in the full spending-routine machinery; only the
// two spending compositions need it.
//

#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/routines/spending.hpp"

namespace PhantomLedger::transfers::legit::ledger::passes {

[[nodiscard]] routines::spending::SpendingRoutine::CardLifecycleConfig
buildCardLifecycleConfig(const blueprints::LegitBlueprint &plan,
                         const RoutineResources &resources);

} // namespace PhantomLedger::transfers::legit::ledger::passes
