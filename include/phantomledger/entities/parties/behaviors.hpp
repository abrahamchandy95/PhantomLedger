#pragma once
#include "phantomledger/taxonomies/personas/types.hpp"

#include <vector>

namespace PhantomLedger::entity::behavior {

struct Archetype {
  personas::Type type = personas::Type::salaried;
  personas::Timing timing = personas::Timing::consumer;
  double weight = 1.0;
};

struct Cash {
  double rateMultiplier = 1.0;
  double amountMultiplier = 1.0;
  double initialBalance = 1200.0;
};

struct Card {
  double prob = 0.70;
  double share = 0.70;
  double limit = 3000.0;
};

struct Payday {
  double sensitivity = 0.40;
};

struct Persona {
  Archetype archetype;
  Cash cash;
  Card card;
  Payday payday;
};

/// Persona.type per person, indexed by personId - 1.
struct Assignment {
  std::vector<personas::Type> byPerson;
};

/// Full persona sample per person, indexed by personId - 1.
struct Table {
  std::vector<Persona> byPerson;
};

} // namespace PhantomLedger::entity::behavior
