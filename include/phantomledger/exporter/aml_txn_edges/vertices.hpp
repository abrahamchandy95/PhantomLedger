#pragma once

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/infra/synth/devices_output.hpp"
#include "phantomledger/infra/synth/ips_output.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>

namespace PhantomLedger::exporter::aml_txn_edges::vertices {

namespace shared = ::PhantomLedger::exporter::aml::vertices;

void writeCustomerRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const shared::SharedContext &ctx,
                       ::PhantomLedger::time::TimePoint simStart);

void writeAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const ::PhantomLedger::clearing::Ledger *finalBook,
                      const shared::SharedContext &ctx,
                      ::PhantomLedger::time::TimePoint simStart);

void writeCounterpartyRows(::PhantomLedger::exporter::csv::Writer &w,
                           const shared::SharedContext &ctx);

void writeBankRows(::PhantomLedger::exporter::csv::Writer &w,
                   const shared::SharedContext &ctx);

void writeDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices);

void writeIpRows(::PhantomLedger::exporter::csv::Writer &w,
                 const ::PhantomLedger::infra::synth::ips::Output &ips);

void writeFullNameRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const shared::SharedContext &ctx);

void writeEmailRows(::PhantomLedger::exporter::csv::Writer &w,
                    const ::PhantomLedger::pipeline::Entities &entities);

void writePhoneRows(::PhantomLedger::exporter::csv::Writer &w,
                    const ::PhantomLedger::pipeline::Entities &entities);

void writeDobRows(::PhantomLedger::exporter::csv::Writer &w,
                  const ::PhantomLedger::pipeline::Entities &entities);

void writeGovtIdRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::pipeline::Entities &entities);

void writeAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const shared::SharedContext &ctx);

void writeWatchlistRows(::PhantomLedger::exporter::csv::Writer &w,
                        const ::PhantomLedger::pipeline::Entities &entities,
                        ::PhantomLedger::time::TimePoint simStart);

void writeAlertRows(::PhantomLedger::exporter::csv::Writer &w,
                    const derived::Bundle &bundle);

void writeDispositionRows(::PhantomLedger::exporter::csv::Writer &w,
                          const derived::Bundle &bundle);

void writeSarRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars);

void writeCtrRows(::PhantomLedger::exporter::csv::Writer &w,
                  const derived::Bundle &bundle);

void writeMinHashBucketRows(::PhantomLedger::exporter::csv::Writer &w,
                            const ::PhantomLedger::pipeline::Entities &entities,
                            const shared::SharedContext &ctx);

void writeInvestigationCaseRows(::PhantomLedger::exporter::csv::Writer &w,
                                const derived::Bundle &bundle);

void writeEvidenceArtifactRows(::PhantomLedger::exporter::csv::Writer &w,
                               const derived::Bundle &bundle);

void writeBusinessRows(::PhantomLedger::exporter::csv::Writer &w,
                       const derived::Bundle &bundle);

void writeInvestigationCaseTxnRows(
    ::PhantomLedger::exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const ::PhantomLedger::transactions::Transaction> postedTxns);

} // namespace PhantomLedger::exporter::aml_txn_edges::vertices
