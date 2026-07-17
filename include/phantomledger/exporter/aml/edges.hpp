#pragma once

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/common/minhash.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::aml::edges {

namespace minhash = ::PhantomLedger::exporter::common::minhash;

// Bounded accumulation over the transaction stream: distinct
// account/counterparty pairs and counterparty banks. std::set members, so
// every writer that consumes them emits in sorted, layout-independent
// order.
struct TransactionEdgeSets {
  std::set<std::pair<entity::Key, entity::Key>> sentToCpPairs;
  std::set<std::pair<entity::Key, entity::Key>> receivedFromCpPairs;
  std::set<entity::Key> cpSenders;
  std::set<entity::Key> cpReceivers;
};

// Row-scale outputs of one classified transaction. The corpus path
// retains them in TransactionEdgeBundle vectors; the windowed streaming
// exporter writes them straight to the four CSV tables.
class TransactionEdgeEmitter {
public:
  virtual ~TransactionEdgeEmitter() = default;

  virtual void send(const entity::Key &acct, std::size_t idx1) = 0;
  virtual void receive(const entity::Key &acct, std::size_t idx1) = 0;
  virtual void cpSend(const entity::Key &cp, std::size_t idx1,
                      const std::string &name) = 0;
  virtual void cpReceive(const entity::Key &cp, std::size_t idx1,
                         const std::string &name) = 0;
};

// Per-row transaction-edge classification, shared by the one-shot corpus
// bundle and the windowed streaming exporter. Classification is a pure
// key predicate (isExternalKey) — each row is self-contained — and the
// memoized counterparty-name cache plus the bounded sets accumulate
// identically in corpus order, so the two engines' outputs are
// byte-identical. The 1-based transaction index advances internally.
class TransactionEdgeClassifier {
public:
  explicit TransactionEdgeClassifier(const vertices::SharedContext &ctx);

  void observe(const transactions::Transaction &tx,
               TransactionEdgeEmitter &emit);

  [[nodiscard]] const TransactionEdgeSets &sets() const noexcept {
    return sets_;
  }

  [[nodiscard]] TransactionEdgeSets takeSets() noexcept {
    return std::move(sets_);
  }

private:
  [[nodiscard]] const std::string &cpNameFor(const entity::Key &k);

  const vertices::SharedContext *ctx_ = nullptr;
  std::unordered_map<entity::Key, std::string> cpNames_;
  TransactionEdgeSets sets_;
  std::size_t idx_ = 1;
};

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

// Single-row forms (the windowed streaming exporter's per-row writes);
// the span/set writers below delegate to them.
void writeAcctTxnRow(exporter::csv::Writer &w, const entity::Key &acct,
                     std::size_t idx1);

void writeCpTxnRow(exporter::csv::Writer &w, const entity::Key &cp,
                   std::size_t idx1, const std::string &name);

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
