#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/legit/blueprints/models.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/screenbook.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

namespace PhantomLedger::transfers::family {
struct GraphConfig;
} // namespace PhantomLedger::transfers::family

namespace PhantomLedger::transfers::legit::ledger::passes {

struct GovernmentCounterparties {
  entity::Key ssa{};
  entity::Key disability{};

  [[nodiscard]] bool valid() const noexcept;
};

void addIncome(const blueprints::Blueprint &request,
               const blueprints::LegitBuildPlan &plan,
               const transactions::Factory &txf, TxnStreams &streams,
               const GovernmentCounterparties &govCps = {});

void addRoutines(const blueprints::Blueprint &request,
                 const blueprints::LegitBuildPlan &plan,
                 const entity::account::Ownership &ownership,
                 const entity::account::Registry &registry,
                 const transactions::Factory &txf, TxnStreams &streams,
                 ScreenBook &screen);

void addFamily(const blueprints::Blueprint &request,
               const blueprints::LegitBuildPlan &plan,
               const transactions::Factory &txf, TxnStreams &streams,
               const family::GraphConfig &graphCfg,
               const routines::relatives::FamilyTransferModel &transferModel);

void addCredit(const blueprints::Blueprint &request,
               const blueprints::LegitBuildPlan &plan,
               const transactions::Factory &txf, TxnStreams &streams);

} // namespace PhantomLedger::transfers::legit::ledger::passes
