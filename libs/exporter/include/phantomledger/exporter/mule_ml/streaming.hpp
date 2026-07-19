#pragma once
//
// phantomledger/exporter/mule_ml/streaming.hpp
//
// Streaming twin of the mule-ml exporter for the windowed engine: a chunk
// sink that consumes settled spans as Phase B folds them and produces
// byte-identical tables to exportAll()'s corpus-based path:
//
//   transfer            streamed row by row with the carried 1-based
//                       transfer id
//   account_device /    per-(account, item) count + first/last timestamp
//   account_ip          accumulated per row (integer min/max/count —
//                       order-insensitive), entity-scale usage ranges
//                       folded in at finish(), written SORTED
//   party               entity-scale except the canonical device/IP pair,
//                       whose per-account frequency histograms accumulate
//                       per row; resolution (max count, then smallest
//                       key) is layout-independent
//
// Retained state is account/pair scale, never transaction scale. Every
// accumulator is the SAME function the corpus path uses (canonical
// CanonicalAccumulator, infra_edges detail accumulators, the shared
// transfer-id writer), so the two paths cannot drift.
//
// Every table goes through common::Table: when Config::pgMirror is
// armed the rendered bytes stream into PostgreSQL directly — the only
// production destination (no files). The transfer table stays open
// (its COPY on its own connection) across the whole fold.
//
// mule-ml applies no membership filter (exportAll consumes the posted
// corpus raw); neither does this sink.
//

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/mule_ml/canonical.hpp"
#include "phantomledger/exporter/mule_ml/infra_edges.hpp"
#include "phantomledger/exporter/mule_ml/party.hpp"
#include "phantomledger/exporter/mule_ml/registry_maps.hpp"
#include "phantomledger/exporter/mule_ml/schema.hpp"
#include "phantomledger/exporter/mule_ml/transfer.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::mule_ml {

class StreamingMuleMlExport {
public:
  struct Config {
    const ::PhantomLedger::entity::account::Registry *registry = nullptr;
    const ::PhantomLedger::entity::person::Roster *roster = nullptr;
    const ::PhantomLedger::entity::pii::Roster *pii = nullptr;
    const synth::infra::devices::Output *devices = nullptr;
    const synth::infra::ips::Output *ips = nullptr;
    const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

    // When set, tables are written directly into PostgreSQL as the
    // bytes the csv::Writer renders — the only production destination.
    const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;

    // Test infrastructure: rendered bytes per table stem.
    common::TableCapture *capture = nullptr;
  };

  explicit StreamingMuleMlExport(Config config)
      : config_(std::move(config)),
        accountsByPerson_(buildAccountsByPerson(*config_.registry)),
        accountToOwner_(buildAccountToOwner(*config_.registry)),
        partyIds_(collectPartyIds(*config_.registry)) {
    target_ = common::TableTarget{.pg = config_.pgMirror,
                                  .capture = config_.capture};
    transfers_.emplace(common::openTable(target_, schema::kMlTransfer));
  }

  void beginSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void append(std::span<const transactions::Transaction> txnsBatch) {
    rows_ += txnsBatch.size();

    writeTransferRows(*transfers_, txnsBatch, nextTransferId_);

    for (const auto &tx : txnsBatch) {
      canonical_.observe(tx);
      detail::accumulateDeviceEdge(deviceEdges_, tx);
      detail::accumulateIpEdge(ipEdges_, tx);
    }
  }

  void endSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void finish() {
    transfers_.reset(); // closes the transfer table (and its COPY)

    detail::addDeviceUsageRanges(deviceEdges_, *config_.devices,
                                 accountsByPerson_);
    {
      auto w = common::openTable(target_, schema::kMlAccountDevice);
      detail::emitSortedRows(w, deviceEdges_);
    }

    detail::addIpUsageRanges(ipEdges_, *config_.ips, accountsByPerson_);
    {
      auto w = common::openTable(target_, schema::kMlAccountIp);
      detail::emitSortedRows(w, ipEdges_);
    }

    const CanonicalResolveInputs resolveInputs{
        .devicesByPerson = &config_.devices->byPerson,
        .ipsByPerson = &config_.ips->byPerson,
        .accountToOwner = &accountToOwner_,
    };
    const auto canonical = canonical_.resolve(partyIds_, resolveInputs);

    {
      auto w = common::openTable(target_, schema::kMlParty);
      PartyInputs partyInputs{};
      partyInputs.piiPools = config_.piiPools;
      partyInputs.canonical = &canonical;
      writePartyRows(w, *config_.registry, *config_.roster, *config_.pii,
                     partyInputs);
    }
  }

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept { return rows_; }

private:
  Config config_;

  AccountsByPerson accountsByPerson_;
  AccountToOwner accountToOwner_;
  std::vector<::PhantomLedger::entity::Key> partyIds_;

  common::TableTarget target_;

  std::optional<common::Table> transfers_;
  std::size_t nextTransferId_ = 1;

  CanonicalAccumulator canonical_;
  detail::EdgeMap deviceEdges_;
  detail::EdgeMap ipEdges_;

  std::uint64_t rows_ = 0;
};

} // namespace PhantomLedger::exporter::mule_ml
