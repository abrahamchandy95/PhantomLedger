#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/synth/common/suffix.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

namespace PhantomLedger::entities::synth::family {

using identifiers::Bank;
using identifiers::Role;

[[nodiscard]] inline entity::Key id(entity::PersonId person) {
  return entity::makeKey(Role::family, Bank::external,
                         common::familySuffix(person));
}

} // namespace PhantomLedger::entities::synth::family
