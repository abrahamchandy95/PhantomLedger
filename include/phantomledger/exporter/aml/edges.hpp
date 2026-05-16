#pragma once

#include "phantomledger/exporter/aml/minhash.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/transactions/record.hpp"

#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::aml::edges {

struct TransactionEdgeBundle {
  using AcctTxnRow = std::pair<entity::Key, std::size_t>;
  using CpTxnRow = std::tuple<entity::Key, std::size_t, std::string>;

  std::vector<AcctTxnRow> sendRows;
  std::vector<AcctTxnRow> receiveRows;
  std::vector<CpTxnRow> cpSendRows;
  std::vector<CpTxnRow> cpReceiveRows;
  std::set<std::pair<entity::Key, entity::Key>> sentToCpPairs;
  std::set<std::pair<entity::Key, entity::Key>> receivedFromCpPairs;
  std::set<entity::Key> cpSenders;
  std::set<entity::Key> cpReceivers;
};

[[nodiscard]] TransactionEdgeBundle
classifyTransactionEdges(std::span<const transactions::Transaction> finalTxns,
                         const vertices::SharedContext &ctx);

struct MinhashVertexSets {
  std::set<minhash::BucketId> name;
  std::set<minhash::BucketId> address;
  std::set<minhash::BucketId> street;
  std::set<std::string> city;
  std::set<std::string> state;
};

[[nodiscard]] MinhashVertexSets
collectMinhashVertexSets(const pipeline::People &people,
                         const vertices::SharedContext &ctx);

void writeCustomerHasAccountRows(exporter::csv::Writer &w,
                                 const pipeline::Holdings &holdings);

void writeAccountHasPrimaryCustomerRows(exporter::csv::Writer &w,
                                        const pipeline::Holdings &holdings);

void writeAcctTxnRows(exporter::csv::Writer &w,
                      std::span<const TransactionEdgeBundle::AcctTxnRow> rows);

void writeCpTxnRows(exporter::csv::Writer &w,
                    std::span<const TransactionEdgeBundle::CpTxnRow> rows);

void writeAcctCpPairRows(
    exporter::csv::Writer &w,
    const std::set<std::pair<entity::Key, entity::Key>> &pairs);

void writeUsesDeviceRows(exporter::csv::Writer &w,
                         const synth::infra::devices::Output &devices);

void writeLoggedFromRows(exporter::csv::Writer &w,
                         const pipeline::Holdings &holdings,
                         const synth::infra::devices::Output &devices);

// ── Identity-by-id edges — pool-free ──

void writeCustomerHasNameRows(exporter::csv::Writer &w,
                              const pipeline::People &people,
                              time::TimePoint simStart);

void writeCustomerHasAddressRows(exporter::csv::Writer &w,
                                 const pipeline::People &people,
                                 time::TimePoint simStart);

void writeCustomerAssociatedWithCountryRows(exporter::csv::Writer &w,
                                            const pipeline::People &people,
                                            time::TimePoint simStart);

void writeAccountHasNameRows(exporter::csv::Writer &w,
                             const pipeline::Holdings &holdings,
                             time::TimePoint simStart);

void writeAccountHasAddressRows(exporter::csv::Writer &w,
                                const pipeline::Holdings &holdings,
                                time::TimePoint simStart);

void writeAccountAssociatedWithCountryRows(exporter::csv::Writer &w,
                                           const pipeline::People &people,
                                           const pipeline::Holdings &holdings,
                                           time::TimePoint simStart);

void writeAddressInCountryRows(exporter::csv::Writer &w,
                               const pipeline::People &people,
                               const vertices::SharedContext &ctx,
                               time::TimePoint simStart);

void writeCounterpartyHasNameRows(exporter::csv::Writer &w,
                                  const vertices::SharedContext &ctx,
                                  time::TimePoint simStart);

void writeCounterpartyHasAddressRows(exporter::csv::Writer &w,
                                     const vertices::SharedContext &ctx,
                                     time::TimePoint simStart);

void writeCounterpartyAssociatedWithCountryRows(
    exporter::csv::Writer &w, const vertices::SharedContext &ctx);

void writeCustomerMatchesWatchlistRows(exporter::csv::Writer &w,
                                       const pipeline::People &people);

void writeReferencesRows(exporter::csv::Writer &w,
                         std::span<const sar::SarRecord> sars);

void writeSarCoversRows(exporter::csv::Writer &w,
                        std::span<const sar::SarRecord> sars);

void writeBeneficiaryBankRows(exporter::csv::Writer &w,
                              const std::set<entity::Key> &cpReceivers);

void writeOriginatorBankRows(exporter::csv::Writer &w,
                             const std::set<entity::Key> &cpSenders);

void writeBankAssociatedWithCountryRows(exporter::csv::Writer &w,
                                        const vertices::SharedContext &ctx);

void writeBankHasAddressRows(exporter::csv::Writer &w,
                             const vertices::SharedContext &ctx,
                             time::TimePoint simStart);

void writeBankHasNameRows(exporter::csv::Writer &w,
                          const vertices::SharedContext &ctx,
                          time::TimePoint simStart);

// ── Minhash-shingle edges — need name/address content from the pool ──

void writeCustomerHasNameMinhashRows(exporter::csv::Writer &w,
                                     const pipeline::People &people,
                                     const vertices::SharedContext &ctx);

void writeCustomerHasAddressMinhashRows(exporter::csv::Writer &w,
                                        const pipeline::People &people,
                                        const vertices::SharedContext &ctx);

void writeCustomerHasAddressStreetLine1MinhashRows(
    exporter::csv::Writer &w, const pipeline::People &people,
    const vertices::SharedContext &ctx);

void writeCustomerHasAddressCityMinhashRows(exporter::csv::Writer &w,
                                            const pipeline::People &people,
                                            const vertices::SharedContext &ctx);

void writeCustomerHasAddressStateMinhashRows(
    exporter::csv::Writer &w, const pipeline::People &people,
    const vertices::SharedContext &ctx);

void writeAccountHasNameMinhashRows(exporter::csv::Writer &w,
                                    const pipeline::People &people,
                                    const pipeline::Holdings &holdings,
                                    const vertices::SharedContext &ctx);

void writeCounterpartyHasNameMinhashRows(exporter::csv::Writer &w,
                                         const vertices::SharedContext &ctx);

void writeResolvesToRows(exporter::csv::Writer &w,
                         const pipeline::Holdings &holdings,
                         time::TimePoint simStart);

} // namespace PhantomLedger::exporter::aml::edges
