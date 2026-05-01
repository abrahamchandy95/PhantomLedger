#pragma once

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/infra/synth/devices_output.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"

#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::aml::edges {

struct TransactionEdgeBundle {
  using AcctTxnRow = std::pair<std::string, std::string>;
  using CpTxnRow = std::tuple<std::string, std::string, std::string>;

  std::vector<AcctTxnRow> sendRows;
  std::vector<AcctTxnRow> receiveRows;
  std::vector<CpTxnRow> cpSendRows;
  std::vector<CpTxnRow> cpReceiveRows;
  std::set<std::pair<std::string, std::string>> sentToCpPairs;
  std::set<std::pair<std::string, std::string>> receivedFromCpPairs;
  std::set<std::string> cpSenders;
  std::set<std::string> cpReceivers;
};

[[nodiscard]] TransactionEdgeBundle classifyTransactionEdges(
    const ::PhantomLedger::pipeline::Entities &entities,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns);

struct MinhashVertexSets {
  std::set<std::string> name;
  std::set<std::string> address;
  std::set<std::string> street;
  std::set<std::string> city;
  std::set<std::string> state;
};

[[nodiscard]] MinhashVertexSets
collectMinhashVertexSets(const ::PhantomLedger::pipeline::Entities &entities,
                         const vertices::SharedContext &ctx);

void writeCustomerHasAccountRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities);

void writeAccountHasPrimaryCustomerRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities);

void writeAcctTxnRows(::PhantomLedger::exporter::csv::Writer &w,
                      std::span<const TransactionEdgeBundle::AcctTxnRow> rows);

void writeCpTxnRows(::PhantomLedger::exporter::csv::Writer &w,
                    std::span<const TransactionEdgeBundle::CpTxnRow> rows);

void writeAcctCpPairRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const std::set<std::pair<std::string, std::string>> &pairs);

void writeUsesDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices);

void writeLoggedFromRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::infra::synth::devices::Output &devices);

void writeCustomerHasNameRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities,
    ::PhantomLedger::time::TimePoint simStart);

void writeCustomerHasAddressRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities,
    ::PhantomLedger::time::TimePoint simStart);

void writeCustomerAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities,
    ::PhantomLedger::time::TimePoint simStart);

void writeAccountHasNameRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities,
    ::PhantomLedger::time::TimePoint simStart);

void writeAccountHasAddressRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities,
    ::PhantomLedger::time::TimePoint simStart);

void writeAccountAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities,
    ::PhantomLedger::time::TimePoint simStart);

void writeAddressInCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities,
    const vertices::SharedContext &ctx,
    ::PhantomLedger::time::TimePoint simStart);

void writeCounterpartyHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                                  const vertices::SharedContext &ctx,
                                  ::PhantomLedger::time::TimePoint simStart);

void writeCounterpartyHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                                     const vertices::SharedContext &ctx,
                                     ::PhantomLedger::time::TimePoint simStart);

void writeCounterpartyAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const vertices::SharedContext &ctx);

void writeCustomerMatchesWatchlistRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities);

void writeReferencesRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars);

void writeSarCoversRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars);

void writeBeneficiaryBankRows(::PhantomLedger::exporter::csv::Writer &w,
                              const std::set<std::string> &cpReceivers);

void writeOriginatorBankRows(::PhantomLedger::exporter::csv::Writer &w,
                             const std::set<std::string> &cpSenders);

void writeBankAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const vertices::SharedContext &ctx);

void writeBankHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                             const vertices::SharedContext &ctx,
                             ::PhantomLedger::time::TimePoint simStart);

void writeBankHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                          const vertices::SharedContext &ctx,
                          ::PhantomLedger::time::TimePoint simStart);

void writeCustomerHasNameMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities);

void writeCustomerHasAddressMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities);

void writeCustomerHasAddressStreetLine1MinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities);

void writeCustomerHasAddressCityMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities);

void writeCustomerHasAddressStateMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities);

void writeAccountHasNameMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::pipeline::Entities &entities);

void writeCounterpartyHasNameMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const vertices::SharedContext &ctx);

void writeResolvesToRows(::PhantomLedger::exporter::csv::Writer &w,
                         const ::PhantomLedger::pipeline::Entities &entities,
                         ::PhantomLedger::time::TimePoint simStart);

} // namespace PhantomLedger::exporter::aml::edges
