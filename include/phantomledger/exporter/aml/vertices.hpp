#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/state.hpp"
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

namespace clearing = ::PhantomLedger::clearing;
namespace encoding = ::PhantomLedger::encoding;
namespace entity = ::PhantomLedger::entity;
namespace exporter = ::PhantomLedger::exporter;
namespace personas = ::PhantomLedger::personas;
namespace pipeline = ::PhantomLedger::pipeline;
namespace synth = ::PhantomLedger::synth;
namespace time_ns = ::PhantomLedger::time;
namespace txns = ::PhantomLedger::transactions;

struct SharedContext {

  std::set<encoding::RenderedKey> counterpartyIds;

  std::set<BankId> bankIds;

  std::vector<personas::Type> personaByPerson;
  std::unordered_map<entity::Key, std::int64_t> lastTransactionByAccount;

  const ::PhantomLedger::entities::synth::pii::PoolSet *pools = nullptr;
};

[[nodiscard]] SharedContext
buildSharedContext(const pipeline::Entities &entities,
                   std::span<const txns::Transaction> finalTxns,
                   const ::PhantomLedger::entities::synth::pii::PoolSet &pools);

// ────────── Vertex writers ──────────

void writeCustomerRows(exporter::csv::Writer &w,
                       const pipeline::Entities &entities,
                       const SharedContext &ctx, time_ns::TimePoint simStart);

void writeAccountRows(exporter::csv::Writer &w,
                      const pipeline::Entities &entities,
                      const clearing::Ledger *finalBook,
                      const SharedContext &ctx, time_ns::TimePoint simStart);

void writeCounterpartyRows(exporter::csv::Writer &w, const SharedContext &ctx);

void writeNameRows(exporter::csv::Writer &w, const pipeline::Entities &entities,
                   const SharedContext &ctx);

void writeAddressRows(exporter::csv::Writer &w,
                      const pipeline::Entities &entities,
                      const SharedContext &ctx);

void writeCountryRows(exporter::csv::Writer &w,
                      const pipeline::Entities &entities);

void writeDeviceRows(exporter::csv::Writer &w,
                     const synth::infra::devices::Output &devices,
                     const synth::infra::ips::Output &ips);

void writeTransactionRows(exporter::csv::Writer &w,
                          std::span<const txns::Transaction> finalTxns);

void writeSarRows(exporter::csv::Writer &w,
                  std::span<const exporter::aml::sar::SarRecord> sars);

void writeBankRows(exporter::csv::Writer &w, const SharedContext &ctx);

void writeWatchlistRows(exporter::csv::Writer &w,
                        const pipeline::Entities &entities,
                        time_ns::TimePoint simStart);

template <typename Set>
void writeMinhashIdRows(exporter::csv::Writer &w, const Set &minhashIds) {
  for (const auto &id : minhashIds) {
    w.writeRow(id);
  }
}

} // namespace PhantomLedger::exporter::aml::vertices
