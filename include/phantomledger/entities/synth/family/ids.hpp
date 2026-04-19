#pragma once

#include "phantomledger/entities/identifier/bank.hpp"
#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/entities/identifier/role.hpp"
#include "phantomledger/entities/synth/common/suffix.hpp"

namespace PhantomLedger::entities::synth::family {

[[nodiscard]] inline identifier::Key id(identifier::PersonId person) {
  return identifier::make(identifier::Role::family, identifier::Bank::external,
                          common::familySuffix(person));
}

} // namespace PhantomLedger::entities::synth::family
