#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>
#include <string>
#include <unordered_map>

namespace PhantomLedger::exporter::mule_ml {

struct CanonicalPair {
  std::string deviceId;
  std::string ipAddress;
};

using CanonicalMap =
    std::unordered_map<::PhantomLedger::entity::Key, CanonicalPair>;

struct CanonicalInputs {
  std::span<const ::PhantomLedger::transactions::Transaction> finalTxns;

  const std::unordered_map<::PhantomLedger::entity::PersonId,
                           std::vector<::PhantomLedger::devices::Identity>>
      *devicesByPerson = nullptr;

  const std::unordered_map<::PhantomLedger::entity::PersonId,
                           std::vector<::PhantomLedger::network::Ipv4>>
      *ipsByPerson = nullptr;

  const std::unordered_map<::PhantomLedger::entity::Key,
                           ::PhantomLedger::entity::PersonId> *accountToOwner =
      nullptr;
};

[[nodiscard]] CanonicalMap
buildCanonicalMaps(std::span<const ::PhantomLedger::entity::Key> partyIds,
                   const CanonicalInputs &inputs);

} // namespace PhantomLedger::exporter::mule_ml
