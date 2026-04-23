#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/entities/synth/common/suffix.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

namespace PhantomLedger::entities::synth::family {

using taxonomies::identifiers::Bank;
using taxonomies::identifiers::Role;

[[nodiscard]] inline identifier::Key id(identifier::PersonId person) {
  return identifier::make(Role::family, Bank::external,
                          common::familySuffix(person));
}

} // namespace PhantomLedger::entities::synth::family
