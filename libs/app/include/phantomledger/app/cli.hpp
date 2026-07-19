#pragma once

#include "phantomledger/app/options.hpp"

#include <cstdio>

namespace PhantomLedger::app::cli {

void printUsage(const char *prog, std::FILE *stream) noexcept;

[[nodiscard]] ::PhantomLedger::app::RunOptions parse(int argc, char **argv);

} // namespace PhantomLedger::app::cli
