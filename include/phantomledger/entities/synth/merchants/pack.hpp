#pragma once

#include "phantomledger/entities/merchants/catalog.hpp"
#include "phantomledger/entities/merchants/categories.hpp"

namespace PhantomLedger::entities::synth::merchants {

struct Pack {
  entities::merchants::Categories categories;
  entities::merchants::Catalog catalog;
};

} // namespace PhantomLedger::entities::synth::merchants
