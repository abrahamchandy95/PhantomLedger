#include "phantomledger/exporter/aml/export.hpp"

#include "phantomledger/exporter/aml/edges.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/schema.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/common/ledger.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <algorithm>
#include <filesystem>
#include <span>
#include <stdexcept>

namespace PhantomLedger::exporter::aml {

namespace {

namespace csv = ::PhantomLedger::exporter::csv;
namespace schema = ::PhantomLedger::exporter::schema;
namespace amlSchema = ::PhantomLedger::exporter::schema::aml;
namespace ent = ::PhantomLedger::entity;
namespace tx_ns = ::PhantomLedger::transactions;
namespace pii_ns = ::PhantomLedger::entities::synth::pii;

[[nodiscard]] csv::Writer openTable(const std::filesystem::path &dir,
                                    const schema::Table &table) {
  csv::Writer w{dir / std::filesystem::path(table.filename)};
  w.writeHeader(table.header);
  return w;
}

[[nodiscard]] ::PhantomLedger::time::TimePoint
deriveSimStart(std::span<const tx_ns::Transaction> finalTxns) {
  if (finalTxns.empty()) {
    return ::PhantomLedger::time::fromEpochSeconds(1735689600);
  }

  std::int64_t minTs = finalTxns.front().timestamp;
  for (const auto &tx : finalTxns) {
    if (tx.timestamp < minTs) {
      minTs = tx.timestamp;
    }
  }

  return ::PhantomLedger::time::fromEpochSeconds(minTs);
}

// Resolve the US LocalePool from Options, with a clear error if main()
// forgot to plumb it through. The exporter is US-only by design — every
// customer/counterparty/bank address is written with country="US" — so
// a single locale lookup at the top is all we need.
[[nodiscard]] const pii_ns::LocalePool &requireUsPool(const Options &options) {
  if (options.piiPools == nullptr) {
    throw std::invalid_argument(
        "exporter::aml::exportAll: Options::piiPools is null. "
        "Set it to the PoolSet built by app::setup::buildPoolSet "
        "(see exporter::mule_ml::Options for the same pattern).");
  }
  return options.piiPools->forCountry(::PhantomLedger::locale::Country::us);
}

[[nodiscard]] std::size_t
countInternalAccounts(const ent::account::Registry &registry) noexcept {
  std::size_t count = 0;
  for (const auto &rec : registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) == 0) {
      ++count;
    }
  }
  return count;
}

[[nodiscard]] std::size_t
countSoloFraud(const ent::person::Roster &roster) noexcept {
  std::size_t count = 0;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    if (roster.has(p, ent::person::Flag::soloFraud)) {
      ++count;
    }
  }
  return count;
}

[[nodiscard]] std::size_t
countIllicitTxns(std::span<const tx_ns::Transaction> txns) noexcept {
  return static_cast<std::size_t>(std::count_if(
      txns.begin(), txns.end(), [](const tx_ns::Transaction &tx) noexcept {
        return tx.fraud.flag != 0;
      }));
}

} // namespace

Summary exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
                  const std::filesystem::path &outDir, const Options &options) {
  std::filesystem::create_directories(outDir);

  const auto &entities = result.entities;
  const auto &infra = result.infra;
  const auto &transfers = result.transfers;
  const auto &postedTxns = transfers.ledger.posted.txns;
  const auto *postedBook = transfers.ledger.posted.book.get();

  const auto simStart = deriveSimStart(postedTxns);
  const auto &usPool = requireUsPool(options);

  const auto ctx = vertices::buildSharedContext(entities, postedTxns, usPool);

  const auto sarSubjects =
      ::PhantomLedger::exporter::aml::sar::buildSarSubjectIndex(
          entities.people.roster, entities.people.topology,
          entities.accounts.registry, entities.accounts.ownership);
  const auto sars = ::PhantomLedger::exporter::aml::sar::generateSars(
      sarSubjects, postedTxns);

  const auto txnBundle =
      edges::classifyTransactionEdges(entities, postedTxns, ctx);
  const auto minhashSets = edges::collectMinhashVertexSets(entities, ctx);

  // ──────────────────────────────────────────────────────────────────
  // Vertex tables — one CSV per node type.
  // ──────────────────────────────────────────────────────────────────

  const auto vtxDir = outDir / "aml" / "vertices";
  std::filesystem::create_directories(vtxDir);

  {
    auto w = openTable(vtxDir, amlSchema::kCustomer);
    vertices::writeCustomerRows(w, entities, ctx, simStart);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kAccount);
    vertices::writeAccountRows(w, entities, postedBook, ctx, simStart);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kCounterparty);
    vertices::writeCounterpartyRows(w, ctx);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kName);
    vertices::writeNameRows(w, entities, ctx);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kAddress);
    vertices::writeAddressRows(w, entities, ctx);
  }
  {
    auto w = openTable(vtxDir, amlSchema::kCountry);
    vertices::writeCountryRows(w);
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
    vertices::writeWatchlistRows(w, entities, simStart);
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
    // Empty by design — populated downstream by the graph-resolution stage.
    auto w = openTable(vtxDir, amlSchema::kConnectedComponent);
    (void)w;
  }

  // ──────────────────────────────────────────────────────────────────
  // Edge tables — one CSV per relationship.
  // ──────────────────────────────────────────────────────────────────

  const auto edgeDir = outDir / "aml" / "edges";
  std::filesystem::create_directories(edgeDir);

  // ── Ownership ──
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasAccount);
    edges::writeCustomerHasAccountRows(w, entities);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAccountHasPrimaryCustomer);
    edges::writeAccountHasPrimaryCustomerRows(w, entities);
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
    edges::writeLoggedFromRows(w, entities, infra.devices);
  }

  // ── Customer / account identity ──
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasName);
    edges::writeCustomerHasNameRows(w, entities, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasAddress);
    edges::writeCustomerHasAddressRows(w, entities, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerAssociatedWithCountry);
    edges::writeCustomerAssociatedWithCountryRows(w, entities, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAccountHasName);
    edges::writeAccountHasNameRows(w, entities, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAccountHasAddress);
    edges::writeAccountHasAddressRows(w, entities, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAccountAssociatedWithCountry);
    edges::writeAccountAssociatedWithCountryRows(w, entities, simStart);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAddressInCountry);
    edges::writeAddressInCountryRows(w, entities, ctx, simStart);
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
    edges::writeCustomerMatchesWatchlistRows(w, entities);
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
    edges::writeCustomerHasNameMinhashRows(w, entities, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasAddressMinhash);
    edges::writeCustomerHasAddressMinhashRows(w, entities, ctx);
  }
  {
    auto w =
        openTable(edgeDir, amlSchema::kCustomerHasAddressStreetLine1Minhash);
    edges::writeCustomerHasAddressStreetLine1MinhashRows(w, entities, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasAddressCityMinhash);
    edges::writeCustomerHasAddressCityMinhashRows(w, entities, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerHasAddressStateMinhash);
    edges::writeCustomerHasAddressStateMinhashRows(w, entities, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kAccountHasNameMinhash);
    edges::writeAccountHasNameMinhashRows(w, entities, ctx);
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCounterpartyHasNameMinhash);
    edges::writeCounterpartyHasNameMinhashRows(w, ctx);
  }

  // ── Graph resolution ──
  {
    auto w = openTable(edgeDir, amlSchema::kResolvesTo);
    edges::writeResolvesToRows(w, entities, simStart);
  }
  {
    // SameAs and CustomerInConnectedComponent are empty by design —
    // populated by a downstream entity-resolution pass.
    auto w = openTable(edgeDir, amlSchema::kSameAs);
    (void)w;
  }
  {
    auto w = openTable(edgeDir, amlSchema::kCustomerInConnectedComponent);
    (void)w;
  }

  // ──────────────────────────────────────────────────────────────────
  // Optional debug dump.
  // ──────────────────────────────────────────────────────────────────

  if (options.showTransactions) {
    auto w = openTable(outDir, schema::kLedger);
    ::PhantomLedger::exporter::common::writeLedgerRows(w, postedTxns);
  }

  // ──────────────────────────────────────────────────────────────────
  // Summary
  // ──────────────────────────────────────────────────────────────────

  Summary summary;
  summary.customerCount = entities.people.roster.count;
  summary.internalAccountCount =
      countInternalAccounts(entities.accounts.registry);
  summary.counterpartyCount = ctx.counterpartyIds.size();
  summary.totalTxnCount = postedTxns.size();
  summary.illicitTxnCount = countIllicitTxns(postedTxns);
  summary.fraudRingCount = entities.people.topology.rings.size();
  summary.soloFraudCount = countSoloFraud(entities.people.roster);
  summary.sarsFiledCount = sars.size();
  return summary;
}

} // namespace PhantomLedger::exporter::aml
