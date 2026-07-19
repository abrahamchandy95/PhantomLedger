#pragma once

#include "phantomledger/entities/holdings/cards.hpp"
#include "phantomledger/entities/counterparties/directory.hpp"
#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/parties/pii.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/synth/accounts/pack.hpp"
#include "phantomledger/synth/landlords/pack.hpp"
#include "phantomledger/synth/people/pack.hpp"
#include "phantomledger/synth/personas/pack.hpp"

namespace PhantomLedger::pipeline {

struct People {
  synth::people::Pack roster;
  entity::pii::Roster pii;
  synth::personas::Pack personas;
};

// pipeline/holdings.hpp
struct Holdings {
  synth::accounts::Pack accounts;
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
