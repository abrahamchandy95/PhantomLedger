#include "phantomledger/exporter/standard/export.hpp"

#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/ledger.hpp"
#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/exporter/standard/accounts.hpp"
#include "phantomledger/exporter/standard/aggregates.hpp"
#include "phantomledger/exporter/standard/counterparties.hpp"
#include "phantomledger/exporter/standard/infra.hpp"
#include "phantomledger/exporter/standard/merchants.hpp"
#include "phantomledger/exporter/standard/people.hpp"
#include "phantomledger/exporter/standard/pii.hpp"
#include "phantomledger/exporter/standard/transfers.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/synth/pii/membership_filter.hpp"

#include <cstddef>
#include <filesystem>

namespace PhantomLedger::exporter::standard {

namespace schema = ::PhantomLedger::exporter::schema;
namespace pii = ::PhantomLedger::synth::pii;

namespace {

template <class Body>
void emit(const std::filesystem::path &outDir, const schema::Table &table,
          Body body) {
  auto w = common::openTable(outDir, table);
  body(w);
}

void exportEntityResolution(const std::filesystem::path &outDir,
                            const ::PhantomLedger::pipeline::People &people,
                            const ::PhantomLedger::pipeline::Holdings &holdings,
                            const ::PhantomLedger::pipeline::Infra &infra,
                            const ::PhantomLedger::synth::pii::PoolSet &pools,
                            const pii::Membership &membership) {
  const auto &piiRoster = people.pii;
  const auto &roster = people.roster.roster;
  const auto &registry = holdings.accounts.registry;
  using W = ::PhantomLedger::exporter::csv::Writer;

  // Core vertices. customer.csv created_at comes from Membership.
  emit(outDir, schema::kErCustomer,
       [&](W &w) { writeCustomerRows(w, roster, membership); });
  emit(outDir, schema::kErAccount,
       [&](W &w) { writeAccountRows(w, registry); });

  // Deduplicated phone / email vertices.
  emit(outDir, schema::kPhone,
       [&](W &w) { writePhoneVertexRows(w, piiRoster); });
  emit(outDir, schema::kEmail,
       [&](W &w) { writeEmailVertexRows(w, piiRoster); });

  // PII value vertices.
  emit(outDir, schema::kName,
       [&](W &w) { writeNameVertexRows(w, pools, piiRoster); });
  emit(outDir, schema::kBirthdate,
       [&](W &w) { writeBirthdateVertexRows(w, piiRoster); });
  emit(outDir, schema::kStreetAddress,
       [&](W &w) { writeStreetVertexRows(w, pools, piiRoster); });
  emit(outDir, schema::kCity,
       [&](W &w) { writeCityVertexRows(w, pools, piiRoster); });
  emit(outDir, schema::kState,
       [&](W &w) { writeStateVertexRows(w, pools, piiRoster); });
  emit(outDir, schema::kPostcode,
       [&](W &w) { writePostcodeVertexRows(w, pools, piiRoster); });

  // PII edges (customer -> value).
  emit(outDir, schema::kHasName,
       [&](W &w) { writeHasNameRows(w, pools, piiRoster); });
  emit(outDir, schema::kHasBirthdate,
       [&](W &w) { writeHasBirthdateRows(w, piiRoster); });
  emit(outDir, schema::kHasStreetAddress,
       [&](W &w) { writeHasStreetRows(w, pools, piiRoster); });
  emit(outDir, schema::kHasCity,
       [&](W &w) { writeHasCityRows(w, pools, piiRoster); });
  emit(outDir, schema::kHasState,
       [&](W &w) { writeHasStateRows(w, pools, piiRoster); });
  emit(outDir, schema::kHasPostcode,
       [&](W &w) { writeHasPostcodeRows(w, pools, piiRoster); });

  emit(outDir, schema::kHasDeviceEr,
       [&](W &w) { writeHasDeviceEdgeRows(w, infra.devices, membership); });
  emit(outDir, schema::kHasIpEr,
       [&](W &w) { writeHasIpEdgeRows(w, infra.ips, membership); });

  emit(outDir, schema::kNameMinhash,
       [&](W &w) { writeNameMinhashVertexRows(w, pools, piiRoster); });
  emit(outDir, schema::kHasNameMinhash,
       [&](W &w) { writeHasNameMinhashRows(w, pools, piiRoster); });
  emit(outDir, schema::kAddressMinhash,
       [&](W &w) { writeAddressMinhashVertexRows(w, pools, piiRoster); });
  emit(outDir, schema::kHasAddressMinhash,
       [&](W &w) { writeHasAddressMinhashRows(w, pools, piiRoster); });
  emit(outDir, schema::kStreetMinhash,
       [&](W &w) { writeStreetMinhashVertexRows(w, pools, piiRoster); });
  emit(outDir, schema::kHasStreetMinhash,
       [&](W &w) { writeHasStreetMinhashRows(w, pools, piiRoster); });
}

} // namespace

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir, const Options &options) {
  std::filesystem::create_directories(outDir);

  const auto &people = result.people;
  const auto &holdings = result.holdings;
  const auto &cps = result.counterparties;
  const auto &infra = result.infra;
  const auto &postedTxns = result.transfers.ledger.posted.txns;
  using W = ::PhantomLedger::exporter::csv::Writer;

  const auto population = static_cast<std::size_t>(people.roster.roster.count);
  const pii::Membership membership(population, options.window, options.growth);

  const auto visibleTxns =
      pii::filterByMembership(postedTxns, holdings.accounts.registry,
                              holdings.accounts.lookup, membership);

  emit(outDir, schema::kPerson,
       [&](W &w) { writePersonRows(w, people.roster.roster); });
  emit(outDir, schema::kAccountNumber,
       [&](W &w) { writeAccountNumberRows(w, holdings.accounts.registry); });
  emit(outDir, schema::kPhone, [&](W &w) { writePhoneRows(w, people.pii); });
  emit(outDir, schema::kEmail, [&](W &w) { writeEmailRows(w, people.pii); });
  emit(outDir, schema::kDevice,
       [&](W &w) { writeDeviceRows(w, infra.devices); });
  emit(outDir, schema::kIpAddress,
       [&](W &w) { writeIpAddressRows(w, infra.ips); });
  emit(outDir, schema::kMerchant,
       [&](W &w) { writeMerchantRows(w, cps.merchants); });
  emit(outDir, schema::kExternalAccount, [&](W &w) {
    writeExternalAccountRows(w, holdings.accounts.registry, cps.merchants,
                             cps.landlords.roster);
  });
  emit(outDir, schema::kHasAccount,
       [&](W &w) { writeHasAccountRows(w, holdings.accounts.registry); });
  emit(outDir, schema::kHasPhone,
       [&](W &w) { writeHasPhoneRows(w, people.pii); });
  emit(outDir, schema::kHasEmail,
       [&](W &w) { writeHasEmailRows(w, people.pii); });
  emit(outDir, schema::kHasUsed,
       [&](W &w) { writeHasUsedRows(w, infra.devices); });
  emit(outDir, schema::kHasIp, [&](W &w) { writeHasIpRows(w, infra.ips); });

  emit(outDir, schema::kHasPaid,
       [&](W &w) { writeHasPaidRows(w, visibleTxns); });

  // Temporal flow aggregates (fixed-width bins). Mirrors writeHasPaidRows'
  // input (visibleTxns) so flow-agg pairs align with HAS_PAID edges. num_bins
  // scales with options.window; bin width defaults to 14 days (bi-weekly).
  emit(outDir, schema::kAccountFlowAggBin, [&](W &w) {
    flow_agg::writeAccountFlowAggRows(w, visibleTxns, options.window);
  });

  if (options.emitEntityResolution && options.piiPools != nullptr) {
    exportEntityResolution(outDir, people, holdings, infra, *options.piiPools,
                           membership);
  }

  if (options.showTransactions) {
    emit(outDir, schema::kLedger,
         [&](W &w) { common::writeLedgerRows(w, visibleTxns); });
  }
}

} // namespace PhantomLedger::exporter::standard
