#include "phantomledger/exporter/aml_txn_edges/export.hpp"

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/aml_txn_edges/edges.hpp"
#include "phantomledger/exporter/aml_txn_edges/schema.hpp"
#include "phantomledger/exporter/aml_txn_edges/vertices.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/labels.hpp"

#include <filesystem>
#include <span>

namespace PhantomLedger::exporter::aml_txn_edges {

namespace {

namespace amlTxnSchema = ::PhantomLedger::exporter::schema::aml_txn_edges;
namespace amlShared = ::PhantomLedger::exporter::aml::vertices;
namespace amlSar = ::PhantomLedger::exporter::aml::sar;
namespace tx_ns = ::PhantomLedger::transactions;
namespace cmn = ::PhantomLedger::exporter::common;
namespace lbl = ::PhantomLedger::exporter::labels;

using cmn::openTable;

} // namespace

Summary exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
                  const std::filesystem::path &outDir, const Options &options) {
  const auto &pools = cmn::requirePools(options, "aml_txn_edges");

  const auto &people = result.people;
  const auto &holdings = result.holdings;
  const auto &infra = result.infra;

  const auto &postedTxns = result.transfers.ledger.posted.txns;
  const auto *postedBook = result.transfers.ledger.posted.book.get();
  const auto txns = std::span<const tx_ns::Transaction>{postedTxns};

  const auto root = outDir / "aml_txn_edges";
  const auto vDir = root / "vertices";
  const auto eDir = root / "edges";
  std::filesystem::create_directories(vDir);
  std::filesystem::create_directories(eDir);

  const auto simStart = cmn::deriveSimStart(txns);
  const auto simEnd = cmn::deriveSimEnd(txns);

  const auto sharedCtx =
      amlShared::buildSharedContext(people, holdings, txns, pools);

  const auto sarSubjects = amlSar::buildSarSubjectIndex(
      people.roster.roster, people.roster.topology, holdings.accounts.registry,
      holdings.accounts.ownership);
  const auto sars = amlSar::generateSars(sarSubjects, txns);

  const auto bundle =
      derived::buildBundle(people, holdings, txns, std::span(sars));

  const auto chainRows = lbl::buildChains(txns);
  const lbl::ShellInputs shellInputs{
      .registry = holdings.accounts.registry,
      .ownership = holdings.accounts.ownership,
      .topology = people.roster.topology,
  };
  const auto shellRows = lbl::buildShells(shellInputs);

  {
    auto w = openTable(vDir, amlTxnSchema::kCustomer);
    vertices::writeCustomerRows(w, people, sharedCtx, simStart);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kAccount);
    vertices::writeInternalAccountRows(w, holdings, postedBook, simStart);
    vertices::writeExternalCounterpartyAccountRows(w, sharedCtx, simStart);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kCounterparty);
    vertices::writeCounterpartyRows(w, sharedCtx);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kBank);
    vertices::writeBankRows(w, sharedCtx);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kDevice);
    vertices::writeDeviceRows(w, infra.devices);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kIp);
    vertices::writeIpRows(w, infra.ips);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kFullName);
    vertices::writeFullNameRows(w, people, sharedCtx);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kEmail);
    vertices::writeEmailRows(w, people);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kPhone);
    vertices::writePhoneRows(w, people);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kDob);
    vertices::writeDobRows(w, people);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kGovtId);
    vertices::writeGovtIdRows(w, people);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kAddress);
    vertices::writeAddressRows(w, people, sharedCtx);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kWatchlist);
    vertices::writeWatchlistRows(w, people, simStart);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kAlert);
    vertices::writeAlertRows(w, bundle);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kDisposition);
    vertices::writeDispositionRows(w, bundle);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kSar);
    vertices::writeSarRows(w, std::span(sars));
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kCtr);
    vertices::writeCtrRows(w, bundle);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kMinHashBucket);
    vertices::writeMinHashBucketRows(w, people, sharedCtx);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kInvestigationCase);
    vertices::writeInvestigationCaseRows(w, bundle);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kEvidenceArtifact);
    vertices::writeEvidenceArtifactRows(w, bundle);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kBusiness);
    vertices::writeBusinessRows(w, bundle);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kChain);
    lbl::writeChainRows(w, std::span<const lbl::ChainRow>(chainRows));
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kShellAccount);
    lbl::writeShellAccountRows(
        w, std::span<const lbl::ShellAccountRow>(shellRows));
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kInvestigationCaseTxn);
    vertices::writeInvestigationCaseTxnRows(w, bundle, txns);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kConnectedComponent);
  }

  {
    auto w = openTable(eDir, amlTxnSchema::kOwns);
    edges::writeOwnsRows(w, holdings, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kTransacted);
    edges::writeTransactedRows(w, txns);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kTransactionChainLabel);
    lbl::writeTransactionChainLabelRows(w, txns);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kInvolvesCounterparty);
    edges::writeInvolvesCounterpartyRows(w, txns, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kBanksAt);
    edges::writeBanksAtRows(w, sharedCtx, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kOnWatchlist);
    edges::writeOnWatchlistRows(w, people, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kSubjectOfSar);
    edges::writeSubjectOfSarRows(w, std::span(sars));
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kFiledCtr);
    edges::writeFiledCtrRows(w, bundle);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kAlertOn);
    edges::writeAlertOnRows(w, bundle);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kDispositionedAs);
    edges::writeDispositionedAsRows(w, bundle);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kEscalatedTo);
    edges::writeEscalatedToRows(w, bundle, std::span(sars));
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kContainsAlert);
    edges::writeContainsAlertRows(w, bundle);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kResultedIn);
    edges::writeResultedInRows(w, bundle, std::span(sars));
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasEvidence);
    edges::writeHasEvidenceRows(w, bundle);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kContainsPromotedTxn);
    edges::writeContainsPromotedTxnRows(w, bundle);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kPromotedTxnAccount);
    edges::writePromotedTxnAccountRows(w, bundle, txns);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kSignerOf);
    edges::writeSignerOfRows(w, bundle, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kBeneficialOwnerOf);
    edges::writeBeneficialOwnerOfRows(w, bundle, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kControls);
    edges::writeControlsRows(w, bundle, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kBusinessOwnsAccount);
    edges::writeBusinessOwnsAccountRows(w, bundle, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasName);
    edges::writeHasNameRows(w, people, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasAddress);
    edges::writeHasAddressRows(w, people, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasEmail);
    edges::writeHasEmailRows(w, people, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasPhone);
    edges::writeHasPhoneRows(w, people, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasDob);
    edges::writeHasDobRows(w, people);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasId);
    edges::writeHasIdRows(w, people);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kUsesDevice);
    edges::writeUsesDeviceRows(w, infra.devices);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kUsesIp);
    edges::writeUsesIpRows(w, infra.ips);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kInBucket);
    edges::writeInBucketRows(w, people, sharedCtx, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kAccountFlowAgg);
    edges::writeAccountFlowAggRows(w, bundle);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kAccountLinkComm);
    edges::writeAccountLinkCommRows(w, bundle);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kInCluster);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kSameAs);
  }

  (void)options.showTransactions;
  (void)simEnd;

  Summary s;
  s.customerCount = people.roster.roster.count;
  s.internalAccountCount =
      cmn::countInternalAccounts(holdings.accounts.registry);
  s.counterpartyCount = sharedCtx.counterpartyIds.size();
  s.totalTxnCount = txns.size();
  s.illicitTxnCount = cmn::countIllicitTxns(txns);
  s.fraudRingCount = people.roster.topology.rings.size();
  s.soloFraudCount = cmn::countSoloFraud(people.roster.roster);
  s.sarsFiledCount = sars.size();

  s.alertCount = bundle.alerts.size();
  s.ctrCount = bundle.ctrs.size();
  s.caseCount = bundle.cases.size();
  s.businessCount = bundle.businesses.size();
  s.flowAggEdgeCount = bundle.flowAgg.size();
  s.linkCommEdgeCount = bundle.linkComm.size();
  s.chainCount = chainRows.size();
  s.shellCount = shellRows.size();
  return s;
}

} // namespace PhantomLedger::exporter::aml_txn_edges
