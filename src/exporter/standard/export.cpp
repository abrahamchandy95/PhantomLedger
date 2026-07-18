#include "phantomledger/exporter/standard/export.hpp"

#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/standard/accounts.hpp"
#include "phantomledger/exporter/standard/aggregates.hpp"
#include "phantomledger/exporter/standard/counterparties.hpp"
#include "phantomledger/exporter/standard/infra.hpp"
#include "phantomledger/exporter/standard/merchants.hpp"
#include "phantomledger/exporter/standard/people.hpp"
#include "phantomledger/exporter/standard/pii.hpp"
#include "phantomledger/exporter/standard/schema.hpp"
#include "phantomledger/exporter/standard/transfers.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/synth/pii/membership_filter.hpp"

#include <cstddef>

namespace PhantomLedger::exporter::standard {

namespace schema = ::PhantomLedger::exporter::schema;
namespace pii = ::PhantomLedger::synth::pii;

namespace {

// One rendering, one destination (common::Table): a direct PostgreSQL
// table when the target's mirror is armed (plus the test capture when
// one is installed).
template <class Body>
void emit(const common::TableTarget &target, const schema::Table &table,
          Body body) {
  auto w = common::openTable(target, table);
  body(w);
}

void exportEntityResolution(const common::TableTarget &target,
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
  emit(target, schema::kErCustomer,
       [&](W &w) { writeCustomerRows(w, roster, membership); });
  emit(target, schema::kErAccount,
       [&](W &w) { writeAccountRows(w, registry); });

  // Deduplicated phone / email vertices.
  emit(target, schema::kPhone,
       [&](W &w) { writePhoneVertexRows(w, piiRoster); });
  emit(target, schema::kEmail,
       [&](W &w) { writeEmailVertexRows(w, piiRoster); });

  // PII value vertices.
  emit(target, schema::kName,
       [&](W &w) { writeNameVertexRows(w, pools, piiRoster); });
  emit(target, schema::kBirthdate,
       [&](W &w) { writeBirthdateVertexRows(w, piiRoster); });
  emit(target, schema::kStreetAddress,
       [&](W &w) { writeStreetVertexRows(w, pools, piiRoster); });
  emit(target, schema::kCity,
       [&](W &w) { writeCityVertexRows(w, pools, piiRoster); });
  emit(target, schema::kState,
       [&](W &w) { writeStateVertexRows(w, pools, piiRoster); });
  emit(target, schema::kPostcode,
       [&](W &w) { writePostcodeVertexRows(w, pools, piiRoster); });

  // PII edges (customer -> value).
  emit(target, schema::kHasName,
       [&](W &w) { writeHasNameRows(w, pools, piiRoster); });
  emit(target, schema::kHasBirthdate,
       [&](W &w) { writeHasBirthdateRows(w, piiRoster); });
  emit(target, schema::kHasStreetAddress,
       [&](W &w) { writeHasStreetRows(w, pools, piiRoster); });
  emit(target, schema::kHasCity,
       [&](W &w) { writeHasCityRows(w, pools, piiRoster); });
  emit(target, schema::kHasState,
       [&](W &w) { writeHasStateRows(w, pools, piiRoster); });
  emit(target, schema::kHasPostcode,
       [&](W &w) { writeHasPostcodeRows(w, pools, piiRoster); });

  emit(target, schema::kHasDeviceEr,
       [&](W &w) { writeHasDeviceEdgeRows(w, infra.devices, membership); });
  emit(target, schema::kHasIpEr,
       [&](W &w) { writeHasIpEdgeRows(w, infra.ips, membership); });

  emit(target, schema::kNameMinhash,
       [&](W &w) { writeNameMinhashVertexRows(w, pools, piiRoster); });
  emit(target, schema::kHasNameMinhash,
       [&](W &w) { writeHasNameMinhashRows(w, pools, piiRoster); });
  emit(target, schema::kAddressMinhash,
       [&](W &w) { writeAddressMinhashVertexRows(w, pools, piiRoster); });
  emit(target, schema::kHasAddressMinhash,
       [&](W &w) { writeHasAddressMinhashRows(w, pools, piiRoster); });
  emit(target, schema::kStreetMinhash,
       [&](W &w) { writeStreetMinhashVertexRows(w, pools, piiRoster); });
  emit(target, schema::kHasStreetMinhash,
       [&](W &w) { writeHasStreetMinhashRows(w, pools, piiRoster); });
}

} // namespace

void exportEntities(const ::PhantomLedger::pipeline::SimulationResult &result,
                    const Options &options) {
  const common::TableTarget target{.pg = options.pgMirror,
                                   .capture = options.capture};

  const auto &people = result.people;
  const auto &holdings = result.holdings;
  const auto &cps = result.counterparties;
  const auto &infra = result.infra;
  using W = ::PhantomLedger::exporter::csv::Writer;

  const auto population = static_cast<std::size_t>(people.roster.roster.count);
  const pii::Membership membership(population, options.window, options.growth);

  emit(target, schema::kPerson,
       [&](W &w) { writePersonRows(w, people.roster.roster); });
  emit(target, schema::kAccountNumber,
       [&](W &w) { writeAccountNumberRows(w, holdings.accounts.registry); });
  emit(target, schema::kPhone, [&](W &w) { writePhoneRows(w, people.pii); });
  emit(target, schema::kEmail, [&](W &w) { writeEmailRows(w, people.pii); });
  emit(target, schema::kDevice,
       [&](W &w) { writeDeviceRows(w, infra.devices); });
  emit(target, schema::kIpAddress,
       [&](W &w) { writeIpAddressRows(w, infra.ips); });
  emit(target, schema::kMerchant,
       [&](W &w) { writeMerchantRows(w, cps.merchants); });
  emit(target, schema::kExternalAccount, [&](W &w) {
    writeExternalAccountRows(w, holdings.accounts.registry, cps.merchants,
                             cps.landlords.roster);
  });
  emit(target, schema::kHasAccount,
       [&](W &w) { writeHasAccountRows(w, holdings.accounts.registry); });
  emit(target, schema::kHasPhone,
       [&](W &w) { writeHasPhoneRows(w, people.pii); });
  emit(target, schema::kHasEmail,
       [&](W &w) { writeHasEmailRows(w, people.pii); });
  emit(target, schema::kHasUsed,
       [&](W &w) { writeHasUsedRows(w, infra.devices); });
  emit(target, schema::kHasIp, [&](W &w) { writeHasIpRows(w, infra.ips); });

  if (options.emitEntityResolution && options.piiPools != nullptr) {
    exportEntityResolution(target, people, holdings, infra, *options.piiPools,
                           membership);
  }
}

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const Options &options) {
  const common::TableTarget target{.pg = options.pgMirror,
                                   .capture = options.capture};

  const auto &people = result.people;
  const auto &holdings = result.holdings;
  const auto &postedTxns = result.transfers.ledger.posted.txns;
  using W = ::PhantomLedger::exporter::csv::Writer;

  const auto population = static_cast<std::size_t>(people.roster.roster.count);
  const pii::Membership membership(population, options.window, options.growth);

  const auto visibleTxns =
      pii::filterByMembership(postedTxns, holdings.accounts.registry,
                              holdings.accounts.lookup, membership);

  exportEntities(result, options);

  emit(target, schema::kHasPaid,
       [&](W &w) { writeHasPaidRows(w, visibleTxns); });

  // Temporal flow aggregates (fixed-width bins). Mirrors writeHasPaidRows'
  // input (visibleTxns) so flow-agg pairs align with HAS_PAID edges. num_bins
  // scales with options.window; bin width defaults to 14 days (bi-weekly).
  emit(target, schema::kAccountFlowAggBin, [&](W &w) {
    flow_agg::writeAccountFlowAggRows(w, visibleTxns, options.window);
  });

  // The raw ledger is deliberately NOT a table here: its stem is
  // "transactions", the streamed corpus table's name — the canonical
  // stream (row_seq/span_index, sinks::Postgres) IS the ledger.
}

} // namespace PhantomLedger::exporter::standard
