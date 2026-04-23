#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/entities/synth/common/suffix.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

namespace PhantomLedger::entities::synth::inflow {

[[nodiscard]] inline identifier::Key businessId(identifier::PersonId person) {
  return identifier::make(taxonomies::identifiers::Role::business,
                          taxonomies::identifiers::Bank::internal,
                          common::suffix("business_operating", person));
}

[[nodiscard]] inline identifier::Key brokerageId(identifier::PersonId person) {
  return identifier::make(taxonomies::identifiers::Role::brokerage,
                          taxonomies::identifiers::Bank::internal,
                          common::suffix("brokerage_custody", person));
}

[[nodiscard]] inline bool isBusinessId(const identifier::Key &id) noexcept {
  return id.role == taxonomies::identifiers::Role::business &&
         id.bank == taxonomies::identifiers::Bank::internal;
}

[[nodiscard]] inline bool isBrokerageId(const identifier::Key &id) noexcept {
  return id.role == taxonomies::identifiers::Role::brokerage &&
         id.bank == taxonomies::identifiers::Bank::internal;
}

} // namespace PhantomLedger::entities::synth::inflow
