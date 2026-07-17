#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::exporter::mule_ml {

struct CanonicalPair {
  std::string deviceId;
  std::string ipAddress;
};

using CanonicalMap =
    std::unordered_map<::PhantomLedger::entity::Key, CanonicalPair>;

namespace detail {

// Per-account device/IP frequency histograms over the posted corpus.
// Integer counts, so accumulation order cannot affect the resolved
// canonical pair.
struct AccountHistograms {
  std::unordered_map<::PhantomLedger::devices::Identity, std::uint32_t>
      deviceCounts;
  std::unordered_map<::PhantomLedger::network::Ipv4, std::uint32_t> ipCounts;
};

} // namespace detail

// Entity-scale inputs for canonical resolution (no corpus).
struct CanonicalResolveInputs {
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

// Streaming accumulator: feed one posted row at a time (windowed sink) or
// a whole corpus (buildCanonicalMaps); resolution is identical because the
// histogram counts are order-insensitive and pickMostFrequent tie-breaks
// layout-independently (max count, then smallest key).
class CanonicalAccumulator {
public:
  void observe(const ::PhantomLedger::transactions::Transaction &tx);

  [[nodiscard]] CanonicalMap
  resolve(std::span<const ::PhantomLedger::entity::Key> partyIds,
          const CanonicalResolveInputs &inputs) const;

private:
  std::unordered_map<::PhantomLedger::entity::Key, detail::AccountHistograms>
      perAccount_;
};

// One-shot corpus path (the monolithic exportAll composition).
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
