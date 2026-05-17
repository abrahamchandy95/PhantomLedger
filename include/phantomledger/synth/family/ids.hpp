#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/synth/suffix.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

namespace PhantomLedger::synth::family {

[[nodiscard]] inline entity::Key id(entity::PersonId person) {
  namespace pl_synth = ::PhantomLedger::synth;

  return entity::makeKey(identifiers::Role::family, identifiers::Bank::external,
                         pl_synth::familySuffix(person));
}

} // namespace PhantomLedger::synth::family
