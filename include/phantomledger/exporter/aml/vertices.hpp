#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/infra/synth/devices_output.hpp"
#include "phantomledger/infra/synth/ips_output.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <set>
#include <span>
#include <string>
#include <vector>

namespace PhantomLedger::exporter::aml::vertices {

struct SharedContext {

  std::set<std::string> counterpartyIds;

  std::vector<::PhantomLedger::personas::Type> personaByPerson;

  std::unordered_map<::PhantomLedger::entity::Key, std::int64_t>
      lastTransactionByAccount;
};

[[nodiscard]] SharedContext buildSharedContext(
    const ::PhantomLedger::pipeline::Entities &entities,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns);

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

void writeCountryRows(::PhantomLedger::exporter::csv::Writer &w);

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

void writeMinhashIdRows(::PhantomLedger::exporter::csv::Writer &w,
                        const std::set<std::string> &minhashIds);

} // namespace PhantomLedger::exporter::aml::vertices
