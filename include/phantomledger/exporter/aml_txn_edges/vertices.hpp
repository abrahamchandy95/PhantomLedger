#pragma once

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/chains.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>

namespace PhantomLedger::exporter::aml_txn_edges::vertices {

void writeCustomerRows(csv::Writer &w, const pipeline::People &people,
                       const aml::vertices::SharedContext &ctx,
                       time::TimePoint simStart);

void writeInternalAccountRows(csv::Writer &w,
                              const pipeline::Holdings &holdings,
                              const clearing::Ledger *finalBook,
                              time::TimePoint simStart);

void writeExternalCounterpartyAccountRows(
    csv::Writer &w, const aml::vertices::SharedContext &ctx,
    time::TimePoint simStart);

void writeCounterpartyRows(csv::Writer &w,
                           const aml::vertices::SharedContext &ctx);

void writeBankRows(csv::Writer &w, const aml::vertices::SharedContext &ctx);

void writeDeviceRows(csv::Writer &w,
                     const synth::infra::devices::Output &devices);

void writeIpRows(csv::Writer &w, const synth::infra::ips::Output &ips);

void writeFullNameRows(csv::Writer &w, const pipeline::People &people,
                       const aml::vertices::SharedContext &ctx);

void writeEmailRows(csv::Writer &w, const pipeline::People &people);

void writePhoneRows(csv::Writer &w, const pipeline::People &people);

void writeDobRows(csv::Writer &w, const pipeline::People &people);

void writeGovtIdRows(csv::Writer &w, const pipeline::People &people);

void writeAddressRows(csv::Writer &w, const pipeline::People &people,
                      const aml::vertices::SharedContext &ctx);

void writeWatchlistRows(csv::Writer &w, const pipeline::People &people,
                        time::TimePoint simStart);

void writeAlertRows(csv::Writer &w, const derived::Bundle &bundle);

void writeDispositionRows(csv::Writer &w, const derived::Bundle &bundle);

void writeSarRows(csv::Writer &w, std::span<const aml::sar::SarRecord> sars);

void writeCtrRows(csv::Writer &w, const derived::Bundle &bundle);

void writeMinHashBucketRows(csv::Writer &w, const pipeline::People &people,
                            const aml::vertices::SharedContext &ctx);

void writeInvestigationCaseRows(csv::Writer &w, const derived::Bundle &bundle);

void writeEvidenceArtifactRows(csv::Writer &w, const derived::Bundle &bundle);

void writeBusinessRows(csv::Writer &w, const derived::Bundle &bundle);

void writeChainRows(csv::Writer &w,
                    std::span<const chains::ChainRow> chainRows);

void writeInvestigationCaseTxnRows(
    csv::Writer &w, const derived::Bundle &bundle,
    std::span<const transactions::Transaction> postedTxns);

} // namespace PhantomLedger::exporter::aml_txn_edges::vertices
