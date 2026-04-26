#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/synth/common/suffix.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

namespace PhantomLedger::entities::synth::inflow {

[[nodiscard]] inline entity::Key businessId(entity::PersonId person) {
  return entity::makeKey(identifiers::Role::business,
                         identifiers::Bank::internal,
                         common::suffix("business_operating", person));
}

[[nodiscard]] inline entity::Key brokerageId(entity::PersonId person) {
  return entity::makeKey(identifiers::Role::brokerage,
                         identifiers::Bank::internal,
                         common::suffix("brokerage_custody", person));
}

[[nodiscard]] inline bool isBusinessId(const entity::Key &id) noexcept {
  return id.role == identifiers::Role::business &&
         id.bank == identifiers::Bank::internal;
}

[[nodiscard]] inline bool isBrokerageId(const entity::Key &id) noexcept {
  return id.role == identifiers::Role::brokerage &&
         id.bank == identifiers::Bank::internal;
}

} // namespace PhantomLedger::entities::synth::inflow
