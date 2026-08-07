#pragma once

#include "phantomledger/app/options.hpp"

#include <expected>
#include <string>

namespace PhantomLedger::app::backend {

struct Config {
  std::string conninfo;
  bool isFileOnly = false;
};

[[nodiscard]] std::expected<Config, std::string>
resolve(const RunOptions &opts);

[[nodiscard]] const char *schemaName(UseCase uc) noexcept;

} // namespace PhantomLedger::app::backend
