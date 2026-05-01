#include "phantomledger/exporter/standard/export.hpp"

#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/exporter/standard/accounts.hpp"
#include "phantomledger/exporter/standard/counterparties.hpp"
#include "phantomledger/exporter/standard/infra.hpp"
#include "phantomledger/exporter/standard/merchants.hpp"
#include "phantomledger/exporter/standard/people.hpp"
#include "phantomledger/exporter/standard/pii.hpp"
#include "phantomledger/exporter/standard/transfers.hpp"

#include <filesystem>

namespace PhantomLedger::exporter::standard {

namespace {

namespace csv = ::PhantomLedger::exporter::csv;
namespace schema = ::PhantomLedger::exporter::schema;

[[nodiscard]] csv::Writer openTable(const std::filesystem::path &outDir,
                                    const schema::Table &table) {
  csv::Writer w{outDir / std::filesystem::path(table.filename)};
  w.writeHeader(table.header);
  return w;
}

} // namespace

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir, const Options &options) {
  std::filesystem::create_directories(outDir);

  const auto &entities = result.entities;
  const auto &infra = result.infra;
  const auto &transfers = result.transfers;

  {
    auto w = openTable(outDir, schema::kPerson);
    writePersonRows(w, entities.people.roster);
  }

  {
    auto w = openTable(outDir, schema::kAccountNumber);
    writeAccountNumberRows(w, entities.accounts.registry);
  }

  {
    auto w = openTable(outDir, schema::kPhone);
    writePhoneRows(w, entities.pii);
  }

  {
    auto w = openTable(outDir, schema::kEmail);
    writeEmailRows(w, entities.pii);
  }

  {
    auto w = openTable(outDir, schema::kDevice);
    writeDeviceRows(w, infra.devices);
  }

  {
    auto w = openTable(outDir, schema::kIpAddress);
    writeIpAddressRows(w, infra.ips);
  }

  {
    auto w = openTable(outDir, schema::kMerchant);
    writeMerchantRows(w, entities.merchants.catalog);
  }

  {
    auto w = openTable(outDir, schema::kExternalAccount);
    writeExternalAccountRows(w, entities.accounts.registry,
                             entities.merchants.catalog,
                             entities.landlords.roster);
  }

  {
    auto w = openTable(outDir, schema::kHasAccount);
    writeHasAccountRows(w, entities.accounts.registry);
  }

  {
    auto w = openTable(outDir, schema::kHasPhone);
    writeHasPhoneRows(w, entities.pii);
  }

  {
    auto w = openTable(outDir, schema::kHasEmail);
    writeHasEmailRows(w, entities.pii);
  }

  {
    auto w = openTable(outDir, schema::kHasUsed);
    writeHasUsedRows(w, infra.devices);
  }

  {
    auto w = openTable(outDir, schema::kHasIp);
    writeHasIpRows(w, infra.ips);
  }

  {
    auto w = openTable(outDir, schema::kHasPaid);
    writeHasPaidRows(w, transfers.finalTxns);
  }

  if (options.showTransactions) {
    auto w = openTable(outDir, schema::kLedger);
    writeLedgerRows(w, transfers.finalTxns);
  }
}

} // namespace PhantomLedger::exporter::standard
