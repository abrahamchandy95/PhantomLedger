#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/infra/router.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/government/disability.hpp"
#include "phantomledger/transfers/government/retirement.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace PhantomLedger::config {
struct Personas;
struct Events;
struct Merchants;
struct Landlords;
struct Social;
} // namespace PhantomLedger::config

namespace PhantomLedger::clearing {
struct BalanceRules;
} // namespace PhantomLedger::clearing

namespace PhantomLedger::recurring {
struct Policy;
} // namespace PhantomLedger::recurring

namespace PhantomLedger::entity::card {
struct IssuancePolicy;
} // namespace PhantomLedger::entity::card

namespace PhantomLedger::entity::product {
class PortfolioRegistry;
} // namespace PhantomLedger::entity::product

namespace PhantomLedger::entities::synth::personas {
struct Pack;
} // namespace PhantomLedger::entities::synth::personas

namespace PhantomLedger::transfers::credit_cards {
struct IssuerPolicy;
struct CardholderBehavior;
} // namespace PhantomLedger::transfers::credit_cards

namespace PhantomLedger::transfers::legit::blueprints {

// ---------------------------------------------------------------------------
// Timeline — temporal & stochastic state
// ---------------------------------------------------------------------------

struct Timeline {
  time::Window window{};
  random::Rng *rng = nullptr;
};

// ---------------------------------------------------------------------------
// Network — instantiated actors & directories
// ---------------------------------------------------------------------------

struct Network {
  const entity::account::Registry *accounts = nullptr;
  const entity::account::Lookup *accountsLookup = nullptr;
  const entity::account::Ownership *ownership = nullptr;

  const entity::merchant::Catalog *merchants = nullptr;
  const entity::landlord::Roster *landlords = nullptr;
  const entity::landlord::Index *landlordsIndex = nullptr;

  const entity::product::PortfolioRegistry *portfolios = nullptr;
};

// ---------------------------------------------------------------------------
// Macro — high-level run scale and optional behavioral inputs
// ---------------------------------------------------------------------------

struct PopulationScale {
  std::uint32_t count = 0;
  std::uint64_t seed = 0;
};

struct HubSelection {
  double fraction = 0.01;
};

struct GovernmentPrograms {
  ::PhantomLedger::transfers::government::RetirementTerms retirement{};
  ::PhantomLedger::transfers::government::DisabilityTerms disability{};
};

struct Macro {
  PopulationScale population{};
  HubSelection hubSelection{};

  const config::Personas *personas = nullptr;
  const config::Events *events = nullptr;
  const config::Merchants *merchantsCfg = nullptr;
  const config::Landlords *landlordsCfg = nullptr;

  const GovernmentPrograms *government = nullptr;
};

// ---------------------------------------------------------------------------
// CreditCardProfile — credit-card defaults bundled into specifications
// ---------------------------------------------------------------------------

struct CreditCardProfile {
  const transfers::credit_cards::IssuerPolicy *terms = nullptr;
  const transfers::credit_cards::CardholderBehavior *habits = nullptr;
};

// ---------------------------------------------------------------------------
// Specifications — policies used while building legitimate transfers
// ---------------------------------------------------------------------------

struct Specifications {
  const recurring::Policy *recurringPolicy = nullptr;
  const config::Social *social = nullptr;
  const clearing::BalanceRules *balances = nullptr;
  const entity::card::IssuancePolicy *creditIssuance = nullptr;
  CreditCardProfile ccProfile{};
};

// ---------------------------------------------------------------------------
// CCState — optional credit-card registry handle
// ---------------------------------------------------------------------------

struct CCState {
  const entity::card::Registry *cards = nullptr;

  [[nodiscard]] bool enabled() const noexcept {
    return cards != nullptr && !cards->records.empty();
  }
};

// ---------------------------------------------------------------------------
// Overrides — pipeline-supplied glue from upstream stages
// ---------------------------------------------------------------------------

struct Overrides {
  const infra::Router *infra = nullptr;
  const entities::synth::personas::Pack *personas = nullptr;
  const entity::counterparty::Pool *counterpartyPools = nullptr;
};

// ---------------------------------------------------------------------------
// Blueprint — top-level container
// ---------------------------------------------------------------------------

struct Blueprint {
  Timeline timeline{};
  Network network{};
  Macro macro{};
  Specifications specs{};
  Overrides overrides{};
  CCState ccState{};
};

// ---------------------------------------------------------------------------
// TransfersPayload — output of the legitimate transfer build phase
// ---------------------------------------------------------------------------

struct TransfersPayload {
  std::vector<transactions::Transaction> candidateTxns;
  std::vector<entity::Key> hubAccounts;
  std::vector<entity::Key> billerAccounts;
  std::vector<entity::Key> employers;
  std::unique_ptr<clearing::Ledger> initialBook;
  std::vector<transactions::Transaction> replaySortedTxns;
};

} // namespace PhantomLedger::transfers::legit::blueprints
