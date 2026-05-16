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

#include <span>

namespace PhantomLedger::exporter::aml_txn_edges::edges {

void writeOwnsRows(csv::Writer &w, const pipeline::Holdings &holdings,
                   time::TimePoint simStart);

void writeTransactedRows(csv::Writer &w,
                         std::span<const transactions::Transaction> postedTxns);

void writeTransactionChainLabelRows(
    csv::Writer &w, std::span<const transactions::Transaction> postedTxns);

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
