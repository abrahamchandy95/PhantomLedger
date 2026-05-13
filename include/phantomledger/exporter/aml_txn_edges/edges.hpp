#pragma once

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/infra/synth/devices_output.hpp"
#include "phantomledger/infra/synth/ips_output.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>

namespace PhantomLedger::exporter::aml_txn_edges::edges {

namespace shared = ::PhantomLedger::exporter::aml::vertices;

void writeOwnsRows(::PhantomLedger::exporter::csv::Writer &w,
                   const ::PhantomLedger::pipeline::Entities &entities,
                   ::PhantomLedger::time::TimePoint simStart);

void writeTransactedRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> postedTxns);

void writeInvolvesCounterpartyRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::transactions::Transaction> postedTxns,
    ::PhantomLedger::time::TimePoint simStart);

void writeBanksAtRows(::PhantomLedger::exporter::csv::Writer &w,
                      const shared::SharedContext &ctx,
                      ::PhantomLedger::time::TimePoint simStart);

void writeOnWatchlistRows(::PhantomLedger::exporter::csv::Writer &w,
                          const ::PhantomLedger::pipeline::Entities &entities,
                          ::PhantomLedger::time::TimePoint simStart);

void writeSubjectOfSarRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars);

void writeFiledCtrRows(::PhantomLedger::exporter::csv::Writer &w,
                       const derived::Bundle &bundle);

void writeAlertOnRows(::PhantomLedger::exporter::csv::Writer &w,
                      const derived::Bundle &bundle);

void writeDispositionedAsRows(::PhantomLedger::exporter::csv::Writer &w,
                              const derived::Bundle &bundle);

void writeEscalatedToRows(
    ::PhantomLedger::exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars);

void writeContainsAlertRows(::PhantomLedger::exporter::csv::Writer &w,
                            const derived::Bundle &bundle);

void writeResultedInRows(
    ::PhantomLedger::exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars);

void writeHasEvidenceRows(::PhantomLedger::exporter::csv::Writer &w,
                          const derived::Bundle &bundle);

void writeContainsPromotedTxnRows(::PhantomLedger::exporter::csv::Writer &w,
                                  const derived::Bundle &bundle);

void writePromotedTxnAccountRows(
    ::PhantomLedger::exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const ::PhantomLedger::transactions::Transaction> postedTxns);

void writeSignerOfRows(::PhantomLedger::exporter::csv::Writer &w,
                       const derived::Bundle &bundle,
                       ::PhantomLedger::time::TimePoint simStart);

void writeBeneficialOwnerOfRows(::PhantomLedger::exporter::csv::Writer &w,
                                const derived::Bundle &bundle,
                                ::PhantomLedger::time::TimePoint simStart);

void writeControlsRows(::PhantomLedger::exporter::csv::Writer &w,
                       const derived::Bundle &bundle,
                       ::PhantomLedger::time::TimePoint simStart);

void writeBusinessOwnsAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                                  const derived::Bundle &bundle,
                                  ::PhantomLedger::time::TimePoint simStart);

void writeHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      ::PhantomLedger::time::TimePoint simStart);

void writeHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                         const ::PhantomLedger::pipeline::Entities &entities,
                         ::PhantomLedger::time::TimePoint simStart);

void writeHasEmailRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       ::PhantomLedger::time::TimePoint simStart);

void writeHasPhoneRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       ::PhantomLedger::time::TimePoint simStart);

void writeHasDobRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::pipeline::Entities &entities);

void writeHasIdRows(::PhantomLedger::exporter::csv::Writer &w,
                    const ::PhantomLedger::pipeline::Entities &entities);

void writeUsesDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices);

void writeUsesIpRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::infra::synth::ips::Output &ips);

void writeInBucketRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const shared::SharedContext &ctx,
                       ::PhantomLedger::time::TimePoint simStart);

void writeAccountFlowAggRows(::PhantomLedger::exporter::csv::Writer &w,
                             const derived::Bundle &bundle);

void writeAccountLinkCommRows(::PhantomLedger::exporter::csv::Writer &w,
                              const derived::Bundle &bundle);

} // namespace PhantomLedger::exporter::aml_txn_edges::edges
