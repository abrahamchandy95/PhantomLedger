#pragma once

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>

namespace PhantomLedger::exporter::aml_txn_edges::vertices {

namespace clearing = ::PhantomLedger::clearing;
namespace exporter = ::PhantomLedger::exporter;
namespace pipe = ::PhantomLedger::pipeline;
namespace shared = ::PhantomLedger::exporter::aml::vertices;
namespace synth = ::PhantomLedger::synth;
namespace time_ns = ::PhantomLedger::time;
namespace txns = ::PhantomLedger::transactions;

void writeCustomerRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                       const shared::SharedContext &ctx,
                       time_ns::TimePoint simStart);

void writeAccountRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                      const clearing::Ledger *finalBook,
                      const shared::SharedContext &ctx,
                      time_ns::TimePoint simStart);

void writeCounterpartyRows(exporter::csv::Writer &w,
                           const shared::SharedContext &ctx);

void writeBankRows(exporter::csv::Writer &w, const shared::SharedContext &ctx);

void writeDeviceRows(exporter::csv::Writer &w,
                     const synth::infra::devices::Output &devices);

void writeIpRows(exporter::csv::Writer &w,
                 const synth::infra::ips::Output &ips);

void writeFullNameRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                       const shared::SharedContext &ctx);

void writeEmailRows(exporter::csv::Writer &w, const pipe::Entities &entities);

void writePhoneRows(exporter::csv::Writer &w, const pipe::Entities &entities);

void writeDobRows(exporter::csv::Writer &w, const pipe::Entities &entities);

void writeGovtIdRows(exporter::csv::Writer &w, const pipe::Entities &entities);

void writeAddressRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                      const shared::SharedContext &ctx);

void writeWatchlistRows(exporter::csv::Writer &w,
                        const pipe::Entities &entities,
                        time_ns::TimePoint simStart);

void writeAlertRows(exporter::csv::Writer &w, const derived::Bundle &bundle);

void writeDispositionRows(exporter::csv::Writer &w,
                          const derived::Bundle &bundle);

void writeSarRows(exporter::csv::Writer &w,
                  std::span<const exporter::aml::sar::SarRecord> sars);

void writeCtrRows(exporter::csv::Writer &w, const derived::Bundle &bundle);

void writeMinHashBucketRows(exporter::csv::Writer &w,
                            const pipe::Entities &entities,
                            const shared::SharedContext &ctx);

void writeInvestigationCaseRows(exporter::csv::Writer &w,
                                const derived::Bundle &bundle);

void writeEvidenceArtifactRows(exporter::csv::Writer &w,
                               const derived::Bundle &bundle);

void writeBusinessRows(exporter::csv::Writer &w, const derived::Bundle &bundle);

void writeInvestigationCaseTxnRows(
    exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const txns::Transaction> postedTxns);

} // namespace PhantomLedger::exporter::aml_txn_edges::vertices
