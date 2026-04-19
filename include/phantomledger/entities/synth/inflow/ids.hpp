#pragma once

#include "phantomledger/entities/identifier/bank.hpp"
#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/entities/identifier/role.hpp"
#include "phantomledger/entities/synth/common/suffix.hpp"

namespace PhantomLedger::entities::synth::inflow {

[[nodiscard]] inline identifier::Key businessId(identifier::PersonId person) {
  return identifier::make(identifier::Role::business,
                          identifier::Bank::internal,
                          common::suffix("business_operating", person));
}

[[nodiscard]] inline identifier::Key brokerageId(identifier::PersonId person) {
  return identifier::make(identifier::Role::brokerage,
                          identifier::Bank::internal,
                          common::suffix("brokerage_custody", person));
}

[[nodiscard]] inline bool isBusinessId(const identifier::Key &id) noexcept {
  return id.role == identifier::Role::business &&
         id.bank == identifier::Bank::internal;
}

[[nodiscard]] inline bool isBrokerageId(const identifier::Key &id) noexcept {
  return id.role == identifier::Role::brokerage &&
         id.bank == identifier::Bank::internal;
}

} // namespace PhantomLedger::entities::synth::inflow
