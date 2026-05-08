#pragma once

#include "phantomledger/run/options.hpp"

#include <cstdio>

namespace PhantomLedger::cli {

void printUsage(const char *prog, std::FILE *stream) noexcept;

[[nodiscard]] ::PhantomLedger::run::RunOptions parse(int argc, char **argv);

} // namespace PhantomLedger::cli
