#include "phantomledger/exporter/aml/export.hpp"

#include "phantomledger/exporter/aml/edges.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/schema.hpp"
#include "phantomledger/exporter/aml/streaming.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/labels.hpp"

#include <optional>
#include <span>
#include <utility>

namespace PhantomLedger::exporter::aml {

namespace {

namespace amlSchema = ::PhantomLedger::exporter::schema::aml;
namespace cmn = ::PhantomLedger::exporter::common;
namespace lbl = ::PhantomLedger::exporter::labels;

using cmn::openTable;

} // namespace

Summary
exportFromArtifacts(const ::PhantomLedger::pipeline::SimulationResult &world,
                    const ::PhantomLedger::clearing::Ledger *postedBook,
                    const Options &options, StreamedArtifacts artifacts) {
  const auto &people = world.people;
  const auto &holdings = world.holdings;
  const auto &infra = world.infra;

  // The stream is replay-sorted, so its first timestamp IS the corpus
  // minimum — identical to deriveSimStart, including the empty fallback.
  const auto simStart =
      (artifacts.rows > 0)
          ? ::PhantomLedger::time::fromEpochSeconds(artifacts.firstTs)
          : ::PhantomLedger::time::fromEpochSeconds(cmn::kFallbackEpoch);

  // Loud failure for a missing pool set, as before; the writers read the
  // pools through the context built by the streaming half.
  (void)cmn::requirePools(options, "aml");

  const auto &ctx = artifacts.ctx;

  const auto sarSubjects =
      ::PhantomLedger::exporter::aml::sar::buildSarSubjectIndex(
          people.roster.roster, people.roster.topology,
          holdings.accounts.registry, holdings.accounts.ownership);
  const auto sars = ::PhantomLedger::exporter::aml::sar::generateSars(
      sarSubjects, artifacts.fraudGroups);

  const auto minhashSets = edges::collectMinhashVertexSets(people, ctx);

  const auto chainRows = lbl::finalizeChains(artifacts.chainGroups);
  const auto shellRows = lbl::finalizeShells(artifacts.shellStats);

  // Direct-table mirrors keep the historical tree naming
  // (aml_vertices_<stem> / aml_edges_<stem> in the target schema).
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
  const cmn::TableTarget vtxTarget{
      .pg = vtxMirror.has_value() ? &*vtxMirror : nullptr,
      .capture = options.capture};

  {
    auto w = openTable(vtxTarget, amlSchema::kCustomer);
    vertices::writeCustomerRows(w, people, ctx, simStart);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kAccount);
    const auto rows =
        vertices::buildInternalAccountRows(holdings, postedBook, ctx, simStart);
    vertices::writeAccountRows(w, rows);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kCounterparty);
    vertices::writeCounterpartyRows(w, ctx);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kName);
    vertices::writeNameRows(w, people, ctx);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kAddress);
    vertices::writeAddressRows(w, people, ctx);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kCountry);
    vertices::writeCountryRows(w, people);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kDevice);
    vertices::writeDeviceRows(w, infra.devices, infra.ips);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kSar);
    vertices::writeSarRows(w, sars);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kBank);
    vertices::writeBankRows(w, ctx);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kWatchlist);
    vertices::writeWatchlistRows(w, people, simStart);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kChain);
    lbl::writeChainRows(w, std::span<const lbl::ChainRow>(chainRows));
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kShellAccount);
    lbl::writeShellAccountRows(
        w, std::span<const lbl::ShellAccountRow>(shellRows));
  }

  {
    auto w = openTable(vtxTarget, amlSchema::kNameMinhash);
    vertices::writeMinhashIdRows(w, minhashSets.name);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kAddressMinhash);
    vertices::writeMinhashIdRows(w, minhashSets.address);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kStreetLine1Minhash);
    vertices::writeMinhashIdRows(w, minhashSets.street);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kCityMinhash);
    vertices::writeMinhashIdRows(w, minhashSets.city);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kStateMinhash);
    vertices::writeMinhashIdRows(w, minhashSets.state);
  }
  {
    auto w = openTable(vtxTarget, amlSchema::kConnectedComponent);
    (void)w;
  }

  const cmn::TableTarget edgeTarget{
      .pg = edgeMirror.has_value() ? &*edgeMirror : nullptr,
      .capture = options.capture};

  {
    auto w = openTable(edgeTarget, amlSchema::kCustomerHasAccount);
    edges::writeCustomerHasAccountRows(w, holdings);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kAccountHasPrimaryCustomer);
    edges::writeAccountHasPrimaryCustomerRows(w, holdings);
  }

  {
    auto w = openTable(edgeTarget, amlSchema::kSentTransactionToCounterparty);
    edges::writeAcctCpPairRows(w, artifacts.edgeSets.sentToCpPairs);
  }
  {
    auto w =
        openTable(edgeTarget, amlSchema::kReceivedTransactionFromCounterparty);
    edges::writeAcctCpPairRows(w, artifacts.edgeSets.receivedFromCpPairs);
  }

  {
    auto w = openTable(edgeTarget, amlSchema::kUsesDevice);
    edges::writeUsesDeviceRows(w, infra.devices);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kLoggedFrom);
    edges::writeLoggedFromRows(w, holdings, infra.devices);
  }

  {
    auto w = openTable(edgeTarget, amlSchema::kCustomerHasName);
    edges::writeCustomerHasNameRows(w, people, simStart);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kCustomerHasAddress);
    edges::writeCustomerHasAddressRows(w, people, simStart);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kCustomerAssociatedWithCountry);
    edges::writeCustomerAssociatedWithCountryRows(w, people, simStart);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kAccountHasName);
    edges::writeAccountHasNameRows(w, holdings, simStart);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kAccountHasAddress);
    edges::writeAccountHasAddressRows(w, holdings, simStart);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kAccountAssociatedWithCountry);
    edges::writeAccountAssociatedWithCountryRows(w, people, holdings, simStart);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kAddressInCountry);
    edges::writeAddressInCountryRows(w, people, ctx, simStart);
  }

  {
    auto w = openTable(edgeTarget, amlSchema::kCounterpartyHasName);
    edges::writeCounterpartyHasNameRows(w, ctx, simStart);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kCounterpartyHasAddress);
    edges::writeCounterpartyHasAddressRows(w, ctx, simStart);
  }
  {
    auto w =
        openTable(edgeTarget, amlSchema::kCounterpartyAssociatedWithCountry);
    edges::writeCounterpartyAssociatedWithCountryRows(w, ctx);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kBeneficiaryBank);
    edges::writeBeneficiaryBankRows(w, artifacts.edgeSets.cpReceivers);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kOriginatorBank);
    edges::writeOriginatorBankRows(w, artifacts.edgeSets.cpSenders);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kBankAssociatedWithCountry);
    edges::writeBankAssociatedWithCountryRows(w, ctx);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kBankHasAddress);
    edges::writeBankHasAddressRows(w, ctx, simStart);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kBankHasName);
    edges::writeBankHasNameRows(w, ctx, simStart);
  }

  {
    auto w = openTable(edgeTarget, amlSchema::kCustomerMatchesWatchlist);
    edges::writeCustomerMatchesWatchlistRows(w, people);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kReferences);
    edges::writeReferencesRows(w, sars);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kSarCovers);
    edges::writeSarCoversRows(w, sars);
  }

  {
    auto w = openTable(edgeTarget, amlSchema::kCustomerHasNameMinhash);
    edges::writeCustomerHasNameMinhashRows(w, people, ctx);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kCustomerHasAddressMinhash);
    edges::writeCustomerHasAddressMinhashRows(w, people, ctx);
  }
  {
    auto w = openTable(edgeTarget,
                       amlSchema::kCustomerHasAddressStreetLine1Minhash);
    edges::writeCustomerHasAddressStreetLine1MinhashRows(w, people, ctx);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kCustomerHasAddressCityMinhash);
    edges::writeCustomerHasAddressCityMinhashRows(w, people, ctx);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kCustomerHasAddressStateMinhash);
    edges::writeCustomerHasAddressStateMinhashRows(w, people, ctx);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kAccountHasNameMinhash);
    edges::writeAccountHasNameMinhashRows(w, people, holdings, ctx);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kCounterpartyHasNameMinhash);
    edges::writeCounterpartyHasNameMinhashRows(w, ctx);
  }

  {
    auto w = openTable(edgeTarget, amlSchema::kResolvesTo);
    edges::writeResolvesToRows(w, holdings, simStart);
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kSameAs);
    (void)w;
  }
  {
    auto w = openTable(edgeTarget, amlSchema::kCustomerInConnectedComponent);
    (void)w;
  }

  Summary summary;
  summary.customerCount = people.roster.roster.count;
  summary.internalAccountCount =
      cmn::countInternalAccounts(holdings.accounts.registry);
  summary.counterpartyCount = ctx.counterpartyIds.size();
  summary.totalTxnCount = static_cast<std::size_t>(artifacts.rows);
  summary.illicitTxnCount = static_cast<std::size_t>(artifacts.illicitRows);
  summary.fraudRingCount = people.roster.topology.rings.size();
  summary.soloFraudCount = cmn::countSoloFraud(people.roster.roster);
  summary.sarsFiledCount = sars.size();
  summary.chainCount = chainRows.size();
  summary.shellCount = shellRows.size();
  return summary;
}

Summary exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
                  const Options &options) {
  const auto &pools = cmn::requirePools(options, "aml");

  const auto &postedTxns = result.transfers.ledger.posted.txns;

  // The corpus path runs the SAME streaming sink the windowed engine
  // uses, as one batch — one code path, two engines.
  StreamingAmlExport sink({
      .people = &result.people,
      .holdings = &result.holdings,
      .piiPools = &pools,
      .pgMirror = options.pgMirror,
      .capture = options.capture,
  });
  sink.append(std::span<const ::PhantomLedger::transactions::Transaction>{
      postedTxns.data(), postedTxns.size()});
  sink.finish();

  return exportFromArtifacts(result, result.transfers.ledger.posted.book.get(),
                             options, sink.takeArtifacts());
}

} // namespace PhantomLedger::exporter::aml
