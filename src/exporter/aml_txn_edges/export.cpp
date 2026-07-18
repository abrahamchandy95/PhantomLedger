#include "phantomledger/exporter/aml_txn_edges/export.hpp"

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/aml_txn_edges/edges.hpp"
#include "phantomledger/exporter/aml_txn_edges/schema.hpp"
#include "phantomledger/exporter/aml_txn_edges/streaming.hpp"
#include "phantomledger/exporter/aml_txn_edges/vertices.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/labels.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <optional>
#include <span>
#include <utility>

namespace PhantomLedger::exporter::aml_txn_edges {

namespace {

namespace amlTxnSchema = ::PhantomLedger::exporter::schema::aml_txn_edges;
namespace amlSar = ::PhantomLedger::exporter::aml::sar;
namespace tx_ns = ::PhantomLedger::transactions;
namespace cmn = ::PhantomLedger::exporter::common;
namespace lbl = ::PhantomLedger::exporter::labels;
namespace t_ns = ::PhantomLedger::time;

using cmn::openTable;

} // namespace

Summary
exportFromProducts(const ::PhantomLedger::pipeline::SimulationResult &world,
                   const ::PhantomLedger::clearing::Ledger *postedBook,
                   const Options &options, StreamProducts products) {
  (void)cmn::requirePools(options, "aml_txn_edges");

  auto &artifacts = products.artifacts;
  const auto &bundle = products.bundle;
  const std::span<const amlSar::SarRecord> sars{products.sars};

  const auto &people = world.people;
  const auto &holdings = world.holdings;
  const auto &infra = world.infra;

  // Direct-table mirrors keep the historical tree naming
  // (aml_txn_edges_vertices_<stem> / aml_txn_edges_edges_<stem>).
  std::optional<sinks::PgMirror> vtxMirror;
  std::optional<sinks::PgMirror> edgeMirror;
  if (options.pgMirror != nullptr) {
    vtxMirror.emplace(sinks::PgMirror{
        .conninfo = options.pgMirror->conninfo,
        .schema = options.pgMirror->schema,
        .tablePrefix = options.pgMirror->tablePrefix + "vertices_"});
    edgeMirror.emplace(sinks::PgMirror{
        .conninfo = options.pgMirror->conninfo,
        .schema = options.pgMirror->schema,
        .tablePrefix = options.pgMirror->tablePrefix + "edges_"});
  }
  const cmn::TableTarget vTarget{
      .pg = vtxMirror.has_value() ? &*vtxMirror : nullptr,
      .capture = options.capture};
  const cmn::TableTarget eTarget{
      .pg = edgeMirror.has_value() ? &*edgeMirror : nullptr,
      .capture = options.capture};

  const auto simStart =
      (artifacts.rows > 0)
          ? t_ns::fromEpochSeconds(artifacts.firstTs)
          : t_ns::fromEpochSeconds(cmn::kFallbackEpoch);

  const auto &sharedCtx = artifacts.ctx;

  auto chainRows = lbl::finalizeChains(artifacts.chainGroups);
  const auto shellRows = lbl::finalizeShells(artifacts.shellStats);

  {
    auto w = openTable(vTarget, amlTxnSchema::kCustomer);
    vertices::writeCustomerRows(w, people, sharedCtx, simStart);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kAccount);
    vertices::writeInternalAccountRows(w, holdings, postedBook, simStart);
    vertices::writeExternalCounterpartyAccountRows(w, sharedCtx, simStart);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kCounterparty);
    vertices::writeCounterpartyRows(w, sharedCtx);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kBank);
    vertices::writeBankRows(w, sharedCtx);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kDevice);
    vertices::writeDeviceRows(w, infra.devices);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kIp);
    vertices::writeIpRows(w, infra.ips);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kFullName);
    vertices::writeFullNameRows(w, people, sharedCtx);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kEmail);
    vertices::writeEmailRows(w, people);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kPhone);
    vertices::writePhoneRows(w, people);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kDob);
    vertices::writeDobRows(w, people);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kGovtId);
    vertices::writeGovtIdRows(w, people);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kAddress);
    vertices::writeAddressRows(w, people, sharedCtx);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kWatchlist);
    vertices::writeWatchlistRows(w, people, simStart);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kAlert);
    vertices::writeAlertRows(w, bundle);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kDisposition);
    vertices::writeDispositionRows(w, bundle);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kSar);
    vertices::writeSarRows(w, sars);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kCtr);
    vertices::writeCtrRows(w, bundle);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kMinHashBucket);
    vertices::writeMinHashBucketRows(w, people, sharedCtx);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kInvestigationCase);
    vertices::writeInvestigationCaseRows(w, bundle);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kEvidenceArtifact);
    vertices::writeEvidenceArtifactRows(w, bundle);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kBusiness);
    vertices::writeBusinessRows(w, bundle);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kChain);
    lbl::writeChainRows(w, std::span<const lbl::ChainRow>(chainRows));
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kShellAccount);
    lbl::writeShellAccountRows(
        w, std::span<const lbl::ShellAccountRow>(shellRows));
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kInvestigationCaseTxn);
    vertices::writeInvestigationCaseTxnRows(w, bundle, artifacts.fraudTxns,
                                            artifacts.rows);
  }
  {
    auto w = openTable(vTarget, amlTxnSchema::kConnectedComponent);
  }

  {
    auto w = openTable(eTarget, amlTxnSchema::kOwns);
    edges::writeOwnsRows(w, holdings, simStart);
  }
  // TRANSACTED and TRANSACTION_CHAIN_LABEL are streamed by
  // StreamingAmlTxnEdgesExport during the fold.
  {
    auto w = openTable(eTarget, amlTxnSchema::kInvolvesCounterparty);
    edges::writeInvolvesCounterpartyRows(w, artifacts.cpPairs, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kBanksAt);
    edges::writeBanksAtRows(w, sharedCtx, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kOnWatchlist);
    edges::writeOnWatchlistRows(w, people, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kSubjectOfSar);
    edges::writeSubjectOfSarRows(w, sars);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kFiledCtr);
    edges::writeFiledCtrRows(w, bundle);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kAlertOn);
    edges::writeAlertOnRows(w, bundle);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kDispositionedAs);
    edges::writeDispositionedAsRows(w, bundle);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kEscalatedTo);
    edges::writeEscalatedToRows(w, bundle, sars);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kContainsAlert);
    edges::writeContainsAlertRows(w, bundle);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kResultedIn);
    edges::writeResultedInRows(w, bundle, sars);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kHasEvidence);
    edges::writeHasEvidenceRows(w, bundle);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kContainsPromotedTxn);
    edges::writeContainsPromotedTxnRows(w, bundle);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kPromotedTxnAccount);
    edges::writePromotedTxnAccountRows(w, bundle, artifacts.fraudTxns,
                                       artifacts.rows);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kSignerOf);
    edges::writeSignerOfRows(w, bundle, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kBeneficialOwnerOf);
    edges::writeBeneficialOwnerOfRows(w, bundle, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kControls);
    edges::writeControlsRows(w, bundle, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kBusinessOwnsAccount);
    edges::writeBusinessOwnsAccountRows(w, bundle, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kHasName);
    edges::writeHasNameRows(w, people, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kHasAddress);
    edges::writeHasAddressRows(w, people, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kHasEmail);
    edges::writeHasEmailRows(w, people, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kHasPhone);
    edges::writeHasPhoneRows(w, people, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kHasDob);
    edges::writeHasDobRows(w, people);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kHasId);
    edges::writeHasIdRows(w, people);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kUsesDevice);
    edges::writeUsesDeviceRows(w, infra.devices);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kUsesIp);
    edges::writeUsesIpRows(w, infra.ips);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kInBucket);
    edges::writeInBucketRows(w, people, sharedCtx, simStart);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kAccountFlowAgg);
    edges::writeAccountFlowAggRows(w, bundle);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kAccountLinkComm);
    edges::writeAccountLinkCommRows(w, bundle);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kInCluster);
  }
  {
    auto w = openTable(eTarget, amlTxnSchema::kSameAs);
  }

  Summary s;
  s.customerCount = people.roster.roster.count;
  s.internalAccountCount =
      cmn::countInternalAccounts(holdings.accounts.registry);
  s.counterpartyCount = sharedCtx.counterpartyIds.size();
  s.totalTxnCount = static_cast<std::size_t>(artifacts.rows);
  s.illicitTxnCount = static_cast<std::size_t>(artifacts.illicitRows);
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

Summary exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
                  const Options &options) {
  const auto &pools = cmn::requirePools(options, "aml_txn_edges");

  const auto &postedTxns = result.transfers.ledger.posted.txns;
  const auto *postedBook = result.transfers.ledger.posted.book.get();
  const auto txns = std::span<const tx_ns::Transaction>{postedTxns};

  // The SAME sink the windowed engine streams through, run over the
  // retained corpus as one batch — the engines cannot drift.
  StreamingAmlTxnEdgesExport sink({
      .people = &result.people,
      .holdings = &result.holdings,
      .piiPools = &pools,
      .pgMirror = options.pgMirror,
      .capture = options.capture,
  });
  sink.append(txns);
  sink.finish();

  // Assemble the stream products: SARs from the accumulated groups,
  // the corpus-side bundle (the windowed engine builds the identical
  // bundle from PostgreSQL via readback::buildBundle —
  // test_derived_readback pins the parity).
  auto artifacts = sink.takeArtifacts();
  auto sars =
      amlSar::generateSars(result.people, result.holdings,
                           artifacts.fraudGroups);
  auto bundle =
      derived::buildBundle(result.people, result.holdings, txns,
                           std::span<const amlSar::SarRecord>(sars));

  return exportFromProducts(result, postedBook, options,
                            {.artifacts = std::move(artifacts),
                             .bundle = std::move(bundle),
                             .sars = std::move(sars)});
}

} // namespace PhantomLedger::exporter::aml_txn_edges
