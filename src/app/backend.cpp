#include "phantomledger/app/backend.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"

#include <cstdlib>
#include <format>
#include <print>

namespace PhantomLedger::app::backend {

std::expected<Config, std::string> resolve(const RunOptions &opts) {
  Config config;
  config.conninfo =
      pgConninfoWithOverride(opts.pgConninfo, std::getenv("PL_PG"));

  if (const char *env = std::getenv("PL_FILE_ONLY")) {
    const std::string_view value{env};
    config.isFileOnly = !value.empty() && value != "0";
  }

  if (config.isFileOnly) {
    std::println(stderr, "note: PL_FILE_ONLY set — skipping PostgreSQL "
                         "(harness escape); nothing is persisted");
    return config;
  }

  try {
    postgres::Connection probe{config.conninfo};
  } catch (const std::exception &err) {
    return std::unexpected(std::format(
        "PostgreSQL is required and no server answered via '{}'.\n"
        "\n"
        "PhantomLedger streams every corpus into PostgreSQL (table\n"
        "'transactions') during settlement and reads it back for the\n"
        "derived analytics, so a reachable server is part of every run.\n"
        "PostgreSQL is free:\n"
        "\n"
        "  point at any server   PL_PG='host=... port=... dbname=... "
        "user=...'\n"
        "  or install locally    macOS:  brew install postgresql@17\n"
        "                        Debian: sudo apt install postgresql\n"
        "                        then:   createdb phantomledger\n"
        "\n"
        "Reruns with the same seed and config are idempotent: data tables\n"
        "are fully rewritten with byte-identical content.\n"
        "\n"
        "Server said: {}",
        config.conninfo, err.what()));
  }

  return config;
}

const char *schemaName(UseCase uc) noexcept {
  switch (uc) {
  case UseCase::standard:
    return "public schema";
  case UseCase::muleMl:
    return "schema mule_ml";
  case UseCase::aml:
    return "schema aml";
  case UseCase::amlTxnEdges:
    return "schema aml_txn_edges";
  case UseCase::cardFraud:
    return "schema card_fraud";
  }
  return "";
}

} // namespace PhantomLedger::app::backend
