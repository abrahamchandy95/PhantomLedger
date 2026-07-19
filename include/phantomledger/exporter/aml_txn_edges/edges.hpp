#pragma once

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdint>
#include <set>
#include <span>
#include <utility>

namespace PhantomLedger::exporter::aml_txn_edges::edges {

void writeOwnsRows(csv::Writer &w, const pipeline::Holdings &holdings,
                   time::TimePoint simStart);

// Streaming seam: emits one Transacted row per transaction, carrying the
// 1-based corpus index (== row_seq) across batches. The span overload
// delegates with a fresh index — one emission body, two corpus stores.
void writeTransactedRows(csv::Writer &w,
                         std::span<const transactions::Transaction> txnsBatch,
                         std::size_t &nextIndex1);

void writeTransactedRows(csv::Writer &w,
                         std::span<const transactions::Transaction> postedTxns);

// Streaming seam for the involves-counterparty edge: the per-row
// accumulator collects distinct (internal account, external
// counterparty) pairs — pair-scale, bounded like the aml edge sets —
// and the finisher writes them in set (sorted) order, so output is
// layout-independent. The span overload composes the two.
using AcctCpPairs = std::set<std::pair<entity::Key, entity::Key>>;

void accumulateInvolvesCounterparty(AcctCpPairs &pairs,
                                    const transactions::Transaction &tx);

void writeInvolvesCounterpartyRows(csv::Writer &w, const AcctCpPairs &pairs,
                                   time::TimePoint simStart);

void writeInvolvesCounterpartyRows(
    csv::Writer &w, std::span<const transactions::Transaction> postedTxns,
    time::TimePoint simStart);

void writeBanksAtRows(csv::Writer &w, const aml::vertices::SharedContext &ctx,
                      time::TimePoint simStart);

void writeOnWatchlistRows(csv::Writer &w, const pipeline::People &people,
                          time::TimePoint simStart);

void writeSubjectOfSarRows(csv::Writer &w,
                           std::span<const aml::sar::SarRecord> sars);

void writeFiledCtrRows(csv::Writer &w, const derived::Bundle &bundle);

void writeAlertOnRows(csv::Writer &w, const derived::Bundle &bundle);

void writeDispositionedAsRows(csv::Writer &w, const derived::Bundle &bundle);

void writeEscalatedToRows(csv::Writer &w, const derived::Bundle &bundle,
                          std::span<const aml::sar::SarRecord> sars);

void writeContainsAlertRows(csv::Writer &w, const derived::Bundle &bundle);

void writeResultedInRows(csv::Writer &w, const derived::Bundle &bundle,
                         std::span<const aml::sar::SarRecord> sars);

void writeHasEvidenceRows(csv::Writer &w, const derived::Bundle &bundle);

void writeContainsPromotedTxnRows(csv::Writer &w,
                                  const derived::Bundle &bundle);

void writePromotedTxnAccountRows(
    csv::Writer &w, const derived::Bundle &bundle,
    std::span<const transactions::Transaction> postedTxns);

// Bounded-memory twin: promoted rows resolved through the fraud-scale
// retention map instead of the retained corpus (every promoted index is
// a fraud index by construction). totalRows preserves the span
// overload's validity filter exactly.
void writePromotedTxnAccountRows(csv::Writer &w, const derived::Bundle &bundle,
                                 const derived::FraudTxnByIndex &fraudTxns,
                                 std::uint64_t totalRows);

void writeSignerOfRows(csv::Writer &w, const derived::Bundle &bundle,
                       time::TimePoint simStart);

void writeBeneficialOwnerOfRows(csv::Writer &w, const derived::Bundle &bundle,
                                time::TimePoint simStart);

void writeControlsRows(csv::Writer &w, const derived::Bundle &bundle,
                       time::TimePoint simStart);

void writeBusinessOwnsAccountRows(csv::Writer &w, const derived::Bundle &bundle,
                                  time::TimePoint simStart);

void writeHasNameRows(csv::Writer &w, const pipeline::People &people,
                      time::TimePoint simStart);

void writeHasAddressRows(csv::Writer &w, const pipeline::People &people,
                         time::TimePoint simStart);

void writeHasEmailRows(csv::Writer &w, const pipeline::People &people,
                       time::TimePoint simStart);

void writeHasPhoneRows(csv::Writer &w, const pipeline::People &people,
                       time::TimePoint simStart);

void writeHasDobRows(csv::Writer &w, const pipeline::People &people);

void writeHasIdRows(csv::Writer &w, const pipeline::People &people);

void writeUsesDeviceRows(csv::Writer &w,
                         const synth::infra::devices::Output &devices);

void writeUsesIpRows(csv::Writer &w, const synth::infra::ips::Output &ips);

void writeInBucketRows(csv::Writer &w, const pipeline::People &people,
                       const aml::vertices::SharedContext &ctx,
                       time::TimePoint simStart);

void writeAccountFlowAggRows(csv::Writer &w, const derived::Bundle &bundle);

void writeAccountLinkCommRows(csv::Writer &w, const derived::Bundle &bundle);

} // namespace PhantomLedger::exporter::aml_txn_edges::edges
