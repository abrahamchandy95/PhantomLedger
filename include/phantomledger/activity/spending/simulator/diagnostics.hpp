#pragma once

#include <cstdio>

namespace PhantomLedger::spending::simulator {

void resetEmissionDiagnostics() noexcept;

void dumpEmissionDiagnostics(std::FILE *out) noexcept;

} // namespace PhantomLedger::spending::simulator
