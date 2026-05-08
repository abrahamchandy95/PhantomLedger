#include "phantomledger/exporter/mule_ml/export.hpp"

#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/mule_ml/canonical.hpp"
#include "phantomledger/exporter/mule_ml/infra_edges.hpp"
#include "phantomledger/exporter/mule_ml/party.hpp"
#include "phantomledger/exporter/mule_ml/transfer.hpp"
#include "phantomledger/exporter/schema.hpp"

#include <filesystem>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::exporter::mule_ml {

namespace {

namespace csv = ::PhantomLedger::exporter::csv;
namespace schema = ::PhantomLedger::exporter::schema;

[[nodiscard]] csv::Writer openTable(const std::filesystem::path &outDir,
                                    const schema::Table &table) {
  csv::Writer w{outDir / std::filesystem::path(table.filename)};
  w.writeHeader(table.header);
  return w;
}

[[nodiscard]] std::unordered_map<::PhantomLedger::entity::PersonId,
                                 std::vector<::PhantomLedger::entity::Key>>
buildAccountsByPerson(
    const ::PhantomLedger::entity::account::Registry &registry) {
  std::unordered_map<::PhantomLedger::entity::PersonId,
                     std::vector<::PhantomLedger::entity::Key>>
      out;

  for (const auto &record : registry.records) {
    if (record.owner == ::PhantomLedger::entity::invalidPerson) {
      continue;
    }

    out[record.owner].push_back(record.id);
  }

  return out;
}

[[nodiscard]] std::unordered_map<::PhantomLedger::entity::Key,
                                 ::PhantomLedger::entity::PersonId>
buildAccountToOwner(
    const ::PhantomLedger::entity::account::Registry &registry) {
  std::unordered_map<::PhantomLedger::entity::Key,
                     ::PhantomLedger::entity::PersonId>
      out;
  out.reserve(registry.records.size());

  for (const auto &record : registry.records) {
    if (record.owner != ::PhantomLedger::entity::invalidPerson) {
      out.emplace(record.id, record.owner);
    }
  }

  return out;
}

[[nodiscard]] std::vector<::PhantomLedger::entity::Key>
collectPartyIds(const ::PhantomLedger::entity::account::Registry &registry) {
  std::vector<::PhantomLedger::entity::Key> out;
  out.reserve(registry.records.size());

  for (const auto &record : registry.records) {
    out.push_back(record.id);
  }

  return out;
}

} // namespace

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir, const Options &options) {
  const auto mlDir = outDir / "ml_ready";
  std::filesystem::create_directories(mlDir);

  const auto &entities = result.entities;
  const auto &infra = result.infra;
  const auto &postedTxns = result.transfers.ledger.posted.txns;

  const auto accountsByPerson =
      buildAccountsByPerson(entities.accounts.registry);
  const auto accountToOwner = buildAccountToOwner(entities.accounts.registry);

  const auto partyIds = collectPartyIds(entities.accounts.registry);
  CanonicalInputs canonInputs{};
  canonInputs.finalTxns = postedTxns;
  canonInputs.devicesByPerson = &infra.devices.byPerson;
  canonInputs.ipsByPerson = &infra.ips.byPerson;
  canonInputs.accountToOwner = &accountToOwner;
  const auto canonical = buildCanonicalMaps(partyIds, canonInputs);

  {
    auto w = openTable(mlDir, schema::kMlParty);
    PartyInputs partyInputs{};
    partyInputs.piiPools = options.piiPools;
    partyInputs.canonical = &canonical;
    writePartyRows(w, entities.accounts.registry, entities.people.roster,
                   entities.pii, partyInputs);
  }
  {
    auto w = openTable(mlDir, schema::kMlTransfer);
    writeTransferRows(w, postedTxns);
  }
  {
    auto w = openTable(mlDir, schema::kMlAccountDevice);
    writeAccountDeviceRows(w, postedTxns, infra.devices, accountsByPerson);
  }
  {
    auto w = openTable(mlDir, schema::kMlAccountIp);
    writeAccountIpRows(w, postedTxns, infra.ips, accountsByPerson);
  }
}

} // namespace PhantomLedger::exporter::mule_ml
