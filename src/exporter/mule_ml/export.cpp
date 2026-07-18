#include "phantomledger/exporter/mule_ml/export.hpp"

#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/mule_ml/canonical.hpp"
#include "phantomledger/exporter/mule_ml/infra_edges.hpp"
#include "phantomledger/exporter/mule_ml/party.hpp"
#include "phantomledger/exporter/mule_ml/registry_maps.hpp"
#include "phantomledger/exporter/mule_ml/transfer.hpp"
#include "phantomledger/exporter/schema.hpp"

#include <filesystem>

namespace PhantomLedger::exporter::mule_ml {

namespace {

namespace schema = ::PhantomLedger::exporter::schema;
namespace common = ::PhantomLedger::exporter::common;

} // namespace

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir, const Options &options) {
  // Empty outDir => no file leg (PostgreSQL-only run); the composed
  // ml_ready subdirectory must then stay empty too, or it would
  // resolve as a relative path in the working directory.
  const bool files = !outDir.empty();
  const auto mlDir = files ? outDir / "ml_ready" : std::filesystem::path{};
  if (files) {
    std::filesystem::create_directories(mlDir);
  }

  // One rendering, two destinations (common::Table): the CSV file when
  // files are enabled, a direct PostgreSQL table when the mirror is
  // armed.
  const common::TableTarget target{.dir = mlDir, .pg = options.pgMirror};

  // SimulationResult no longer carries a god-struct `entities`; reach
  // into the SRP-split sub-domains directly.
  const auto &people = result.people;
  const auto &holdings = result.holdings;
  const auto &infra = result.infra;
  const auto &postedTxns = result.transfers.ledger.posted.txns;

  const auto accountsByPerson =
      buildAccountsByPerson(holdings.accounts.registry);
  const auto accountToOwner = buildAccountToOwner(holdings.accounts.registry);

  const auto partyIds = collectPartyIds(holdings.accounts.registry);
  CanonicalInputs canonInputs{};
  canonInputs.finalTxns = postedTxns;
  canonInputs.devicesByPerson = &infra.devices.byPerson;
  canonInputs.ipsByPerson = &infra.ips.byPerson;
  canonInputs.accountToOwner = &accountToOwner;
  const auto canonical = buildCanonicalMaps(partyIds, canonInputs);

  {
    auto w = common::openTable(target, schema::kMlParty);
    PartyInputs partyInputs{};
    partyInputs.piiPools = options.piiPools;
    partyInputs.canonical = &canonical;
    // `people.roster` is the synth::people::Pack; its inner `.roster` is
    // the entity::person::Roster that writePartyRows expects.
    writePartyRows(w, holdings.accounts.registry, people.roster.roster,
                   people.pii, partyInputs);
  }
  {
    auto w = common::openTable(target, schema::kMlTransfer);
    writeTransferRows(w, postedTxns);
  }
  {
    auto w = common::openTable(target, schema::kMlAccountDevice);
    writeAccountDeviceRows(w, postedTxns, infra.devices, accountsByPerson);
  }
  {
    auto w = common::openTable(target, schema::kMlAccountIp);
    writeAccountIpRows(w, postedTxns, infra.ips, accountsByPerson);
  }
}

} // namespace PhantomLedger::exporter::mule_ml
