#pragma once

#include "phantomledger/exporter/aml/minhash.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/transactions/record.hpp"

#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::aml::edges {

namespace ent = ::PhantomLedger::entity;
namespace pipe = ::PhantomLedger::pipeline;
namespace txn = ::PhantomLedger::transactions;
namespace exporter = ::PhantomLedger::exporter;
namespace time_ns = ::PhantomLedger::time;
namespace synth = ::PhantomLedger::synth;

struct TransactionEdgeBundle {
  using AcctTxnRow = std::pair<ent::Key, std::size_t>;
  using CpTxnRow = std::tuple<ent::Key, std::size_t, std::string>;

  std::vector<AcctTxnRow> sendRows;
  std::vector<AcctTxnRow> receiveRows;
  std::vector<CpTxnRow> cpSendRows;
  std::vector<CpTxnRow> cpReceiveRows;
  std::set<std::pair<ent::Key, ent::Key>> sentToCpPairs;
  std::set<std::pair<ent::Key, ent::Key>> receivedFromCpPairs;
  std::set<ent::Key> cpSenders;
  std::set<ent::Key> cpReceivers;
};

[[nodiscard]] TransactionEdgeBundle
classifyTransactionEdges(const pipe::Entities &entities,
                         std::span<const txn::Transaction> finalTxns,
                         const vertices::SharedContext &ctx);

struct MinhashVertexSets {
  std::set<exporter::aml::minhash::BucketId> name;
  std::set<exporter::aml::minhash::BucketId> address;
  std::set<exporter::aml::minhash::BucketId> street;
  std::set<std::string> city;
  std::set<std::string> state;
};

[[nodiscard]] MinhashVertexSets
collectMinhashVertexSets(const pipe::Entities &entities,
                         const vertices::SharedContext &ctx);

void writeCustomerHasAccountRows(exporter::csv::Writer &w,
                                 const pipe::Entities &entities);

void writeAccountHasPrimaryCustomerRows(exporter::csv::Writer &w,
                                        const pipe::Entities &entities);

void writeAcctTxnRows(exporter::csv::Writer &w,
                      std::span<const TransactionEdgeBundle::AcctTxnRow> rows);

void writeCpTxnRows(exporter::csv::Writer &w,
                    std::span<const TransactionEdgeBundle::CpTxnRow> rows);

void writeAcctCpPairRows(exporter::csv::Writer &w,
                         const std::set<std::pair<ent::Key, ent::Key>> &pairs);

void writeUsesDeviceRows(exporter::csv::Writer &w,
                         const synth::infra::devices::Output &devices);

void writeLoggedFromRows(exporter::csv::Writer &w,
                         const pipe::Entities &entities,
                         const synth::infra::devices::Output &devices);

// ── Identity-by-id edges — pool-free ──

void writeCustomerHasNameRows(exporter::csv::Writer &w,
                              const pipe::Entities &entities,
                              time_ns::TimePoint simStart);

void writeCustomerHasAddressRows(exporter::csv::Writer &w,
                                 const pipe::Entities &entities,
                                 time_ns::TimePoint simStart);

void writeCustomerAssociatedWithCountryRows(exporter::csv::Writer &w,
                                            const pipe::Entities &entities,
                                            time_ns::TimePoint simStart);

void writeAccountHasNameRows(exporter::csv::Writer &w,
                             const pipe::Entities &entities,
                             time_ns::TimePoint simStart);

void writeAccountHasAddressRows(exporter::csv::Writer &w,
                                const pipe::Entities &entities,
                                time_ns::TimePoint simStart);

void writeAccountAssociatedWithCountryRows(exporter::csv::Writer &w,
                                           const pipe::Entities &entities,
                                           time_ns::TimePoint simStart);

void writeAddressInCountryRows(exporter::csv::Writer &w,
                               const pipe::Entities &entities,
                               const vertices::SharedContext &ctx,
                               time_ns::TimePoint simStart);

void writeCounterpartyHasNameRows(exporter::csv::Writer &w,
                                  const vertices::SharedContext &ctx,
                                  time_ns::TimePoint simStart);

void writeCounterpartyHasAddressRows(exporter::csv::Writer &w,
                                     const vertices::SharedContext &ctx,
                                     time_ns::TimePoint simStart);

void writeCounterpartyAssociatedWithCountryRows(
    exporter::csv::Writer &w, const vertices::SharedContext &ctx);

void writeCustomerMatchesWatchlistRows(exporter::csv::Writer &w,
                                       const pipe::Entities &entities);

void writeReferencesRows(exporter::csv::Writer &w,
                         std::span<const exporter::aml::sar::SarRecord> sars);

void writeSarCoversRows(exporter::csv::Writer &w,
                        std::span<const exporter::aml::sar::SarRecord> sars);

void writeBeneficiaryBankRows(exporter::csv::Writer &w,
                              const std::set<ent::Key> &cpReceivers);

void writeOriginatorBankRows(exporter::csv::Writer &w,
                             const std::set<ent::Key> &cpSenders);

void writeBankAssociatedWithCountryRows(exporter::csv::Writer &w,
                                        const vertices::SharedContext &ctx);

void writeBankHasAddressRows(exporter::csv::Writer &w,
                             const vertices::SharedContext &ctx,
                             time_ns::TimePoint simStart);

void writeBankHasNameRows(exporter::csv::Writer &w,
                          const vertices::SharedContext &ctx,
                          time_ns::TimePoint simStart);

// ── Minhash-shingle edges — need name/address content from the pool ──

void writeCustomerHasNameMinhashRows(exporter::csv::Writer &w,
                                     const pipe::Entities &entities,
                                     const vertices::SharedContext &ctx);

void writeCustomerHasAddressMinhashRows(exporter::csv::Writer &w,
                                        const pipe::Entities &entities,
                                        const vertices::SharedContext &ctx);

void writeCustomerHasAddressStreetLine1MinhashRows(
    exporter::csv::Writer &w, const pipe::Entities &entities,
    const vertices::SharedContext &ctx);

void writeCustomerHasAddressCityMinhashRows(exporter::csv::Writer &w,
                                            const pipe::Entities &entities,
                                            const vertices::SharedContext &ctx);

void writeCustomerHasAddressStateMinhashRows(
    exporter::csv::Writer &w, const pipe::Entities &entities,
    const vertices::SharedContext &ctx);

void writeAccountHasNameMinhashRows(exporter::csv::Writer &w,
                                    const pipe::Entities &entities,
                                    const vertices::SharedContext &ctx);

void writeCounterpartyHasNameMinhashRows(exporter::csv::Writer &w,
                                         const vertices::SharedContext &ctx);

void writeResolvesToRows(exporter::csv::Writer &w,
                         const pipe::Entities &entities,
                         time_ns::TimePoint simStart);

} // namespace PhantomLedger::exporter::aml::edges
