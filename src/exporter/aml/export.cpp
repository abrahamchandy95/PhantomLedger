#include "phantomledger/exporter/aml/export.hpp"

#include "phantomledger/exporter/aml/edges.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/schema.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/ledger.hpp"

#include <filesystem>
#include <span>

namespace PhantomLedger::exporter::aml {

namespace {

namespace schema = ::PhantomLedger::exporter::schema;
namespace amlSchema = ::PhantomLedger::exporter::schema::aml;
namespace cmn = ::PhantomLedger::exporter::common;

using cmn::openTable;

} // namespace

Summary exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
                  const std::filesystem::path &outDir, const Options &options) {
  std::filesystem::create_directories(outDir);

  const auto &people = result.people;
  const auto &holdings = result.holdings;
  const auto &infra = result.infra;
  const auto &transfers = result.transfers;
  const auto &postedTxns = transfers.ledger.posted.txns;
  const auto *postedBook = transfers.ledger.posted.book.get();

  const auto simStart = cmn::deriveSimStart(postedTxns);
  const auto &pools = cmn::requirePools(options, "aml");

  const auto ctx =
      vertices::buildSharedContext(people, holdings, postedTxns, pools);

  const auto sarSubjects =
      ::PhantomLedger::exporter::aml::sar::buildSarSubjectIndex(
          people.roster.roster, people.roster.topology,
          holdings.accounts.registry, holdings.accounts.ownership);
  const auto sars = ::PhantomLedger::exporter::aml::sar::generateSars(
      sarSubjects, postedTxns);

  const auto txnBundle = edges::classifyTransactionEdges(postedTxns, ctx);
  const auto minhashSets = edges::collectMinhashVertexSets(people, ctx);

  const auto vtxDir = outDir / "aml" / "vertices";
  std::filesystem::create_directories(vtxDir);

  {
    auto w = openTable(vtxDir, amlSchema::kCustomer);
    vertices::writeCustomerRows(w, people, ctx, simStart);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kAccount);
    vertices::writeAccountRows(w, holdings, postedBook, ctx, simStart);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kCounterparty);
    vertices::writeCounterpartyRows(w, ctx);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kName);
    vertices::writeNameRows(w, people, ctx);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kAddress);
    vertices::writeAddressRows(w, people, ctx);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kCountry);
    vertices::writeCountryRows(w, people);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kDevice);
    vertices::writeDeviceRows(w, infra.devices, infra.ips);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kTransaction);
    vertices::writeTransactionRows(w, postedTxns);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kSar);
    vertices::writeSarRows(w, sars);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kBank);
    vertices::writeBankRows(w, ctx);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kWatchlist);
    vertices::writeWatchlistRows(w, people, simStart);
  }

  // Minhash-shingle vertex sets (one table per identity facet).
  {
    auto w = openTable(vtxDir, amlSchema::kNameMinhash);
    vertices::writeMinhashIdRows(w, minhashSets.name);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kAddressMinhash);
    vertices::writeMinhashIdRows(w, minhashSets.address);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kStreetLine1Minhash);
    vertices::writeMinhashIdRows(w, minhashSets.street);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kCityMinhash);
    vertices::writeMinhashIdRows(w, minhashSets.city);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kStateMinhash);
    vertices::writeMinhashIdRows(w, minhashSets.state);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kConnectedComponent);
    (void)w;
  }

  // Edge tables — one CSV per relationship.

  const auto edgeDir = outDir / "aml" / "edges";
  std::filesystem::create_directories(edgeDir);

  // ── Ownership ──
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasAccount);
    edges::writeCustomerHasAccountRows(w, holdings);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAccountHasPrimaryCustomer);
    edges::writeAccountHasPrimaryCustomerRows(w, holdings);
  }

  // ── Transaction flows ──
  {
    auto w = openTable(edgeDir, amlSchema::kSendTransaction);
    edges::writeAcctTxnRows(w, txnBundle.sendRows);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kReceiveTransaction);
    edges::writeAcctTxnRows(w, txnBundle.receiveRows);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCounterpartySendTransaction);
    edges::writeCpTxnRows(w, txnBundle.cpSendRows);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCounterpartyReceiveTransaction);
    edges::writeCpTxnRows(w, txnBundle.cpReceiveRows);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kSentTransactionToCounterparty);
    edges::writeAcctCpPairRows(w, txnBundle.sentToCpPairs);
  }
  {
    auto w =
        openTable(edgeDir, amlSchema::kReceivedTransactionFromCounterparty);
    edges::writeAcctCpPairRows(w, txnBundle.receivedFromCpPairs);
  }

  // ── Device / network ──
  {
    auto w = openTable(edgeDir, amlSchema::kUsesDevice);
    edges::writeUsesDeviceRows(w, infra.devices);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kLoggedFrom);
    edges::writeLoggedFromRows(w, holdings, infra.devices);
  }

  // ── Customer / account identity ──
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasName);
    edges::writeCustomerHasNameRows(w, people, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasAddress);
    edges::writeCustomerHasAddressRows(w, people, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerAssociatedWithCountry);
    edges::writeCustomerAssociatedWithCountryRows(w, people, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAccountHasName);
    edges::writeAccountHasNameRows(w, holdings, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAccountHasAddress);
    edges::writeAccountHasAddressRows(w, holdings, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAccountAssociatedWithCountry);
    edges::writeAccountAssociatedWithCountryRows(w, people, holdings, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAddressInCountry);
    edges::writeAddressInCountryRows(w, people, ctx, simStart);
  }

  // ── Counterparty / bank identity ──
  {
    auto w = openTable(edgeDir, amlSchema::kCounterpartyHasName);
    edges::writeCounterpartyHasNameRows(w, ctx, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCounterpartyHasAddress);
    edges::writeCounterpartyHasAddressRows(w, ctx, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCounterpartyAssociatedWithCountry);
    edges::writeCounterpartyAssociatedWithCountryRows(w, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kBeneficiaryBank);
    edges::writeBeneficiaryBankRows(w, txnBundle.cpReceivers);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kOriginatorBank);
    edges::writeOriginatorBankRows(w, txnBundle.cpSenders);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kBankAssociatedWithCountry);
    edges::writeBankAssociatedWithCountryRows(w, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kBankHasAddress);
    edges::writeBankHasAddressRows(w, ctx, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kBankHasName);
    edges::writeBankHasNameRows(w, ctx, simStart);
  }

  // ── Watchlist / SAR ──
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerMatchesWatchlist);
    edges::writeCustomerMatchesWatchlistRows(w, people);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kReferences);
    edges::writeReferencesRows(w, sars);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kSarCovers);
    edges::writeSarCoversRows(w, sars);
  }

  // ── Minhash-shingle edges ──
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasNameMinhash);
    edges::writeCustomerHasNameMinhashRows(w, people, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasAddressMinhash);
    edges::writeCustomerHasAddressMinhashRows(w, people, ctx);
  }
  {
    auto w =
        openTable(edgeDir, amlSchema::kCustomerHasAddressStreetLine1Minhash);
    edges::writeCustomerHasAddressStreetLine1MinhashRows(w, people, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasAddressCityMinhash);
    edges::writeCustomerHasAddressCityMinhashRows(w, people, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasAddressStateMinhash);
    edges::writeCustomerHasAddressStateMinhashRows(w, people, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAccountHasNameMinhash);
    edges::writeAccountHasNameMinhashRows(w, people, holdings, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCounterpartyHasNameMinhash);
    edges::writeCounterpartyHasNameMinhashRows(w, ctx);
  }

  // ── Graph resolution ──
  {
    auto w = openTable(edgeDir, amlSchema::kResolvesTo);
    edges::writeResolvesToRows(w, holdings, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kSameAs);
    (void)w;
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerInConnectedComponent);
    (void)w;
  }

  if (options.showTransactions) {
    auto w = openTable(outDir, schema::kLedger);
    ::PhantomLedger::exporter::common::writeLedgerRows(w, postedTxns);
  }

  Summary summary;
  summary.customerCount = people.roster.roster.count;
  summary.internalAccountCount =
      cmn::countInternalAccounts(holdings.accounts.registry);
  summary.counterpartyCount = ctx.counterpartyIds.size();
  summary.totalTxnCount = postedTxns.size();
  summary.illicitTxnCount = cmn::countIllicitTxns(postedTxns);
  summary.fraudRingCount = people.roster.topology.rings.size();
  summary.soloFraudCount = cmn::countSoloFraud(people.roster.roster);
  summary.sarsFiledCount = sars.size();
  return summary;
}

} // namespace PhantomLedger::exporter::aml
