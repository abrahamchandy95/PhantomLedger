#pragma once
//
// phantomledger/exporter/mule_ml/registry_maps.hpp
//
// Registry-derived lookup maps shared by the corpus-based exportAll()
// path and the windowed StreamingMuleMlExport sink, so the two engines
// derive party/ownership structure from one definition.
//

#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <unordered_map>
#include <vector>

namespace PhantomLedger::exporter::mule_ml {

using AccountsByPerson =
    std::unordered_map<::PhantomLedger::entity::PersonId,
                       std::vector<::PhantomLedger::entity::Key>>;

using AccountToOwner = std::unordered_map<::PhantomLedger::entity::Key,
                                          ::PhantomLedger::entity::PersonId>;

[[nodiscard]] inline AccountsByPerson buildAccountsByPerson(
    const ::PhantomLedger::entity::account::Registry &registry) {
  AccountsByPerson out;

  for (const auto &record : registry.records) {
    if (record.owner == ::PhantomLedger::entity::invalidPerson) {
      continue;
    }

    out[record.owner].push_back(record.id);
  }

  return out;
}

[[nodiscard]] inline AccountToOwner buildAccountToOwner(
    const ::PhantomLedger::entity::account::Registry &registry) {
  AccountToOwner out;
  out.reserve(registry.records.size());

  for (const auto &record : registry.records) {
    if (record.owner != ::PhantomLedger::entity::invalidPerson) {
      out.emplace(record.id, record.owner);
    }
  }

  return out;
}

[[nodiscard]] inline std::vector<::PhantomLedger::entity::Key>
collectPartyIds(const ::PhantomLedger::entity::account::Registry &registry) {
  std::vector<::PhantomLedger::entity::Key> out;
  out.reserve(registry.records.size());

  for (const auto &record : registry.records) {
    out.push_back(record.id);
  }

  return out;
}

} // namespace PhantomLedger::exporter::mule_ml
