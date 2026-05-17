#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entities/synth/accounts/pack.hpp"
#include "phantomledger/entities/synth/people/pack.hpp"
#include "phantomledger/synth/landlords/pack.hpp"
#include "phantomledger/synth/personas/pack.hpp"

namespace PhantomLedger::pipeline {

struct People {
  entities::synth::people::Pack roster;
  entity::pii::Roster pii;
  synth::personas::Pack personas;
};

// pipeline/holdings.hpp
struct Holdings {
  entities::synth::accounts::Pack accounts;
  entity::card::Registry creditCards;
  entity::product::PortfolioRegistry portfolios;
};

// pipeline/counterparties.hpp
struct Counterparties {
  entity::merchant::Catalog merchants;
  synth::landlords::Pack landlords;
  entity::counterparty::Directory counterparties;
};

} // namespace PhantomLedger::pipeline
