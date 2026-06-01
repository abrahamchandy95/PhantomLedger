#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>
#include <vector>

namespace PhantomLedger::synth::pii {

[[nodiscard]] inline entity::PersonId
ownerOf(const entity::account::Registry &registry,
        const entity::account::Lookup &lookup, entity::Key key) noexcept {
  const auto it = lookup.byId.find(key);
  if (it == lookup.byId.end()) {
    return entity::invalidPerson;
  }
  return registry.records[it->second].owner;
}

[[nodiscard]] inline std::vector<transactions::Transaction>
filterByMembership(std::span<const transactions::Transaction> txns,
                   const entity::account::Registry &registry,
                   const entity::account::Lookup &lookup,
                   const Membership &membership) {
  std::vector<transactions::Transaction> out;
  out.reserve(txns.size());
  for (const auto &tx : txns) {
    const auto srcOwner = ownerOf(registry, lookup, tx.source);
    const auto dstOwner = ownerOf(registry, lookup, tx.target);
    if (!membership.activeAt(srcOwner, tx.timestamp) ||
        !membership.activeAt(dstOwner, tx.timestamp)) {
      continue; // before a joiner endpoint existed
    }
    out.push_back(tx);
  }
  return out;
}

} // namespace PhantomLedger::synth::pii
