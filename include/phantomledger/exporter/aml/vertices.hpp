#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/entities/infra/devices_output.hpp"
#include "phantomledger/synth/entities/infra/ips_output.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <set>
#include <span>
#include <vector>

namespace PhantomLedger::exporter::aml::vertices {

struct SharedContext {

  std::set<::PhantomLedger::encoding::RenderedKey> counterpartyIds;

  std::set<BankId> bankIds;

  std::vector<::PhantomLedger::personas::Type> personaByPerson;
  std::unordered_map<::PhantomLedger::entity::Key, std::int64_t>
      lastTransactionByAccount;

  const ::PhantomLedger::entities::synth::pii::PoolSet *pools = nullptr;
};

[[nodiscard]] SharedContext buildSharedContext(
    const ::PhantomLedger::pipeline::Entities &entities,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns,
    const ::PhantomLedger::entities::synth::pii::PoolSet &pools);

// ────────── Vertex writers ──────────

void writeCustomerRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const SharedContext &ctx,
                       ::PhantomLedger::time::TimePoint simStart);

void writeAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const ::PhantomLedger::clearing::Ledger *finalBook,
                      const SharedContext &ctx,
                      ::PhantomLedger::time::TimePoint simStart);

void writeCounterpartyRows(::PhantomLedger::exporter::csv::Writer &w,
                           const SharedContext &ctx);

void writeNameRows(::PhantomLedger::exporter::csv::Writer &w,
                   const ::PhantomLedger::pipeline::Entities &entities,
                   const SharedContext &ctx);

void writeAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const SharedContext &ctx);

void writeCountryRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities);

void writeDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices,
    const ::PhantomLedger::infra::synth::ips::Output &ips);

void writeTransactionRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns);

void writeSarRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars);

void writeBankRows(::PhantomLedger::exporter::csv::Writer &w,
                   const SharedContext &ctx);

void writeWatchlistRows(::PhantomLedger::exporter::csv::Writer &w,
                        const ::PhantomLedger::pipeline::Entities &entities,
                        ::PhantomLedger::time::TimePoint simStart);

template <typename Set>
void writeMinhashIdRows(::PhantomLedger::exporter::csv::Writer &w,
                        const Set &minhashIds) {
  for (const auto &id : minhashIds) {
    w.writeRow(id);
  }
}

} // namespace PhantomLedger::exporter::aml::vertices
