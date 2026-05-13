#include "phantomledger/exporter/aml_txn_edges/export.hpp"

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/aml_txn_edges/edges.hpp"
#include "phantomledger/exporter/aml_txn_edges/schema.hpp"
#include "phantomledger/exporter/aml_txn_edges/vertices.hpp"
#include "phantomledger/exporter/common/framework.hpp"

#include <filesystem>
#include <span>

namespace PhantomLedger::exporter::aml_txn_edges {

namespace {

namespace amlTxnSchema = ::PhantomLedger::exporter::schema::aml_txn_edges;
namespace amlShared = ::PhantomLedger::exporter::aml::vertices;
namespace amlSar = ::PhantomLedger::exporter::aml::sar;
namespace tx_ns = ::PhantomLedger::transactions;
namespace cmn = ::PhantomLedger::exporter::common;

using cmn::openTable;

} // namespace

Summary exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
                  const std::filesystem::path &outDir, const Options &options) {
  const auto &pools = cmn::requirePools(options, "aml_txn_edges");
  const auto &entities = result.entities;
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

  const auto sharedCtx = amlShared::buildSharedContext(entities, txns, pools);

  const auto sarSubjects = amlSar::buildSarSubjectIndex(
      entities.people.roster, entities.people.topology,
      entities.accounts.registry, entities.accounts.ownership);
  const auto sars = amlSar::generateSars(sarSubjects, txns);

  const auto bundle =
      derived::buildBundle(entities, txns, sharedCtx, std::span(sars));

  // ── Vertices ──
  {
    auto w = openTable(vDir, amlTxnSchema::kCustomer);
    vertices::writeCustomerRows(w, entities, sharedCtx, simStart);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kAccount);
    vertices::writeAccountRows(w, entities, postedBook, sharedCtx, simStart);
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
    vertices::writeFullNameRows(w, entities, sharedCtx);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kEmail);
    vertices::writeEmailRows(w, entities);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kPhone);
    vertices::writePhoneRows(w, entities);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kDob);
    vertices::writeDobRows(w, entities);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kGovtId);
    vertices::writeGovtIdRows(w, entities);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kAddress);
    vertices::writeAddressRows(w, entities, sharedCtx);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kWatchlist);
    vertices::writeWatchlistRows(w, entities, simStart);
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
    vertices::writeMinHashBucketRows(w, entities, sharedCtx);
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
    auto w = openTable(vDir, amlTxnSchema::kInvestigationCaseTxn);
    vertices::writeInvestigationCaseTxnRows(w, bundle, txns);
  }
  {
    auto w = openTable(vDir, amlTxnSchema::kConnectedComponent);
  }

  // ── Edges ──
  {
    auto w = openTable(eDir, amlTxnSchema::kOwns);
    edges::writeOwnsRows(w, entities, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kTransacted);
    edges::writeTransactedRows(w, txns);
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
    edges::writeOnWatchlistRows(w, entities, simStart);
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
    edges::writeHasNameRows(w, entities, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasAddress);
    edges::writeHasAddressRows(w, entities, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasEmail);
    edges::writeHasEmailRows(w, entities, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasPhone);
    edges::writeHasPhoneRows(w, entities, simStart);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasDob);
    edges::writeHasDobRows(w, entities);
  }
  {
    auto w = openTable(eDir, amlTxnSchema::kHasId);
    edges::writeHasIdRows(w, entities);
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
    edges::writeInBucketRows(w, entities, sharedCtx, simStart);
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

  // ── Summary ──

  Summary s;
  cmn::fillBaseCounts(s, entities, txns, sharedCtx.counterpartyIds.size(),
                      sars.size());
  s.alertCount = bundle.alerts.size();
  s.ctrCount = bundle.ctrs.size();
  s.caseCount = bundle.cases.size();
  s.businessCount = bundle.businesses.size();
  s.flowAggEdgeCount = bundle.flowAgg.size();
  s.linkCommEdgeCount = bundle.linkComm.size();
  return s;
}

} // namespace PhantomLedger::exporter::aml_txn_edges
