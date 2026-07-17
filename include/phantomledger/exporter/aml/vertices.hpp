#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::exporter::aml::vertices {

struct SharedContext {

  std::set<encoding::RenderedKey> counterpartyIds;

  std::set<BankId> bankIds;

  std::vector<personas::Type> personaByPerson;
  std::unordered_map<entity::Key, std::int64_t> lastTransactionByAccount;

  const synth::pii::PoolSet *pools = nullptr;
};

// Entity-scale context: counterparty ids and banks come from the account
// REGISTRY (external accounts), personas from the assignment — never from
// the corpus. The only corpus-derived field is lastTransactionByAccount,
// fed per row via observeTransaction below.
[[nodiscard]] SharedContext
buildSharedContextEntities(const pipeline::People &people,
                           const pipeline::Holdings &holdings,
                           const synth::pii::PoolSet &pools);

// Per-row accumulation (a per-account timestamp max — order-insensitive),
// shared by the one-shot corpus path and the windowed streaming exporter.
void observeTransaction(SharedContext &ctx,
                        const transactions::Transaction &tx);

// One-shot corpus form: entity context + per-row observation — one code
// path, two engines.
[[nodiscard]] SharedContext
buildSharedContext(const pipeline::People &people,
                   const pipeline::Holdings &holdings,
                   std::span<const transactions::Transaction> finalTxns,
                   const synth::pii::PoolSet &pools);

// ────────── Vertex writers ──────────

void writeCustomerRows(csv::Writer &w, const pipeline::People &people,
                       const SharedContext &ctx, time::TimePoint simStart);

struct InternalAccountRow {
  ::PhantomLedger::encoding::RenderedKey idStr;
  double balance = 0.0;
  time::TimePoint openDate{};
  std::string lastTxnStr;
  std::string_view acctType;
  ::PhantomLedger::encoding::RenderedId<8> branch;
};

[[nodiscard]] std::vector<InternalAccountRow>
buildInternalAccountRows(const pipeline::Holdings &holdings,
                         const clearing::Ledger *finalBook,
                         const SharedContext &ctx, time::TimePoint simStart);

void writeAccountRows(csv::Writer &w, std::span<const InternalAccountRow> rows);

void writeCounterpartyRows(csv::Writer &w, const SharedContext &ctx);

void writeNameRows(csv::Writer &w, const pipeline::People &people,
                   const SharedContext &ctx);

void writeAddressRows(csv::Writer &w, const pipeline::People &people,
                      const SharedContext &ctx);

void writeCountryRows(csv::Writer &w, const pipeline::People &people);

void writeDeviceRows(csv::Writer &w,
                     const synth::infra::devices::Output &devices,
                     const synth::infra::ips::Output &ips);

// Streaming form: `nextIndex1` carries the 1-based transaction id across
// batches so the windowed sink emits identical ids to the one-shot path.
void writeTransactionRows(csv::Writer &w,
                          std::span<const transactions::Transaction> finalTxns,
                          std::size_t &nextIndex1);

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
