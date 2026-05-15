#pragma once

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>

namespace PhantomLedger::exporter::aml_txn_edges::edges {

namespace exporter = ::PhantomLedger::exporter;
namespace pipe = ::PhantomLedger::pipeline;
namespace shared = ::PhantomLedger::exporter::aml::vertices;
namespace synth = ::PhantomLedger::synth;
namespace time_ns = ::PhantomLedger::time;
namespace txns = ::PhantomLedger::transactions;

void writeOwnsRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                   time_ns::TimePoint simStart);

void writeTransactedRows(exporter::csv::Writer &w,
                         std::span<const txns::Transaction> postedTxns);

void writeInvolvesCounterpartyRows(
    exporter::csv::Writer &w, std::span<const txns::Transaction> postedTxns,
    time_ns::TimePoint simStart);

void writeBanksAtRows(exporter::csv::Writer &w,
                      const shared::SharedContext &ctx,
                      time_ns::TimePoint simStart);

void writeOnWatchlistRows(exporter::csv::Writer &w,
                          const pipe::Entities &entities,
                          time_ns::TimePoint simStart);

void writeSubjectOfSarRows(exporter::csv::Writer &w,
                           std::span<const exporter::aml::sar::SarRecord> sars);

void writeFiledCtrRows(exporter::csv::Writer &w, const derived::Bundle &bundle);

void writeAlertOnRows(exporter::csv::Writer &w, const derived::Bundle &bundle);

void writeDispositionedAsRows(exporter::csv::Writer &w,
                              const derived::Bundle &bundle);

void writeEscalatedToRows(exporter::csv::Writer &w,
                          const derived::Bundle &bundle,
                          std::span<const exporter::aml::sar::SarRecord> sars);

void writeContainsAlertRows(exporter::csv::Writer &w,
                            const derived::Bundle &bundle);

void writeResultedInRows(exporter::csv::Writer &w,
                         const derived::Bundle &bundle,
                         std::span<const exporter::aml::sar::SarRecord> sars);

void writeHasEvidenceRows(exporter::csv::Writer &w,
                          const derived::Bundle &bundle);

void writeContainsPromotedTxnRows(exporter::csv::Writer &w,
                                  const derived::Bundle &bundle);

void writePromotedTxnAccountRows(
    exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const transactions::Transaction> postedTxns);

void writeSignerOfRows(exporter::csv::Writer &w, const derived::Bundle &bundle,
                       time_ns::TimePoint simStart);

void writeBeneficialOwnerOfRows(exporter::csv::Writer &w,
                                const derived::Bundle &bundle,
                                time_ns::TimePoint simStart);

void writeControlsRows(exporter::csv::Writer &w, const derived::Bundle &bundle,
                       time_ns::TimePoint simStart);

void writeBusinessOwnsAccountRows(exporter::csv::Writer &w,
                                  const derived::Bundle &bundle,
                                  time_ns::TimePoint simStart);

void writeHasNameRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                      time_ns::TimePoint simStart);

void writeHasAddressRows(exporter::csv::Writer &w,
                         const pipe::Entities &entities,
                         time_ns::TimePoint simStart);

void writeHasEmailRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                       time_ns::TimePoint simStart);

void writeHasPhoneRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                       time_ns::TimePoint simStart);

void writeHasDobRows(exporter::csv::Writer &w, const pipe::Entities &entities);

void writeHasIdRows(exporter::csv::Writer &w, const pipe::Entities &entities);

void writeUsesDeviceRows(exporter::csv::Writer &w,
                         const synth::infra::devices::Output &devices);

void writeUsesIpRows(exporter::csv::Writer &w,
                     const synth::infra::ips::Output &ips);

void writeInBucketRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                       const shared::SharedContext &ctx,
                       time_ns::TimePoint simStart);

void writeAccountFlowAggRows(exporter::csv::Writer &w,
                             const derived::Bundle &bundle);

void writeAccountLinkCommRows(exporter::csv::Writer &w,
                              const derived::Bundle &bundle);

} // namespace PhantomLedger::exporter::aml_txn_edges::edges
