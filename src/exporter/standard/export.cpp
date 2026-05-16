#include "phantomledger/exporter/standard/export.hpp"

#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/ledger.hpp"
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

namespace schema = ::PhantomLedger::exporter::schema;

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir, const Options &options) {
  std::filesystem::create_directories(outDir);

  const auto &people = result.people;
  const auto &holdings = result.holdings;
  const auto &cps = result.counterparties;
  const auto &infra = result.infra;
  const auto &postedTxns = result.transfers.ledger.posted.txns;

  {
    auto w = common::openTable(outDir, schema::kPerson);
    writePersonRows(w, people.roster.roster);
  }
  {
    auto w = common::openTable(outDir, schema::kAccountNumber);
    writeAccountNumberRows(w, holdings.accounts.registry);
  }
  {
    auto w = common::openTable(outDir, schema::kPhone);
    writePhoneRows(w, people.pii);
  }
  {
    auto w = common::openTable(outDir, schema::kEmail);
    writeEmailRows(w, people.pii);
  }
  {
    auto w = common::openTable(outDir, schema::kDevice);
    writeDeviceRows(w, infra.devices);
  }
  {
    auto w = common::openTable(outDir, schema::kIpAddress);
    writeIpAddressRows(w, infra.ips);
  }
  {
    auto w = common::openTable(outDir, schema::kMerchant);
    writeMerchantRows(w, cps.merchants);
  }
  {
    auto w = common::openTable(outDir, schema::kExternalAccount);
    writeExternalAccountRows(w, holdings.accounts.registry, cps.merchants,
                             cps.landlords.roster);
  }
  {
    auto w = common::openTable(outDir, schema::kHasAccount);
    writeHasAccountRows(w, holdings.accounts.registry);
  }
  {
    auto w = common::openTable(outDir, schema::kHasPhone);
    writeHasPhoneRows(w, people.pii);
  }
  {
    auto w = common::openTable(outDir, schema::kHasEmail);
    writeHasEmailRows(w, people.pii);
  }
  {
    auto w = common::openTable(outDir, schema::kHasUsed);
    writeHasUsedRows(w, infra.devices);
  }
  {
    auto w = common::openTable(outDir, schema::kHasIp);
    writeHasIpRows(w, infra.ips);
  }
  {
    auto w = common::openTable(outDir, schema::kHasPaid);
    writeHasPaidRows(w, postedTxns);
  }

  if (options.showTransactions) {
    auto w = common::openTable(outDir, schema::kLedger);
    common::writeLedgerRows(w, postedTxns);
  }
}

} // namespace PhantomLedger::exporter::standard
