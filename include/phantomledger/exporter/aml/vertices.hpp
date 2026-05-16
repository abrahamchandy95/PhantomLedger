#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <set>
#include <span>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::exporter::aml::vertices {

struct SharedContext {

  std::set<encoding::RenderedKey> counterpartyIds;

  std::set<BankId> bankIds;

  std::vector<personas::Type> personaByPerson;
  std::unordered_map<entity::Key, std::int64_t> lastTransactionByAccount;

  const entities::synth::pii::PoolSet *pools = nullptr;
};

[[nodiscard]] SharedContext
buildSharedContext(const pipeline::People &people,
                   const pipeline::Holdings &holdings,
                   std::span<const transactions::Transaction> finalTxns,
                   const entities::synth::pii::PoolSet &pools);

// ────────── Vertex writers ──────────

void writeCustomerRows(csv::Writer &w, const pipeline::People &people,
                       const SharedContext &ctx, time::TimePoint simStart);

void writeAccountRows(csv::Writer &w, const pipeline::Holdings &holdings,
                      const clearing::Ledger *finalBook,
                      const SharedContext &ctx, time::TimePoint simStart);

void writeCounterpartyRows(csv::Writer &w, const SharedContext &ctx);

void writeNameRows(csv::Writer &w, const pipeline::People &people,
                   const SharedContext &ctx);

void writeAddressRows(csv::Writer &w, const pipeline::People &people,
                      const SharedContext &ctx);

void writeCountryRows(csv::Writer &w, const pipeline::People &people);

void writeDeviceRows(csv::Writer &w,
                     const synth::infra::devices::Output &devices,
                     const synth::infra::ips::Output &ips);

void writeTransactionRows(csv::Writer &w,
                          std::span<const transactions::Transaction> finalTxns);

void writeSarRows(csv::Writer &w, std::span<const sar::SarRecord> sars);

void writeBankRows(csv::Writer &w, const SharedContext &ctx);

void writeWatchlistRows(csv::Writer &w, const pipeline::People &people,
                        time::TimePoint simStart);

template <typename Set>
void writeMinhashIdRows(csv::Writer &w, const Set &minhashIds) {
  for (const auto &id : minhashIds) {
    w.writeRow(id);
  }
}

} // namespace PhantomLedger::exporter::aml::vertices
