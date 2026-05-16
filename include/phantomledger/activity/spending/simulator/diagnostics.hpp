#pragma once

#include <cstdio>

namespace PhantomLedger::activity::spending::simulator {

void resetEmissionDiagnostics() noexcept;

void dumpEmissionDiagnostics(std::FILE *out) noexcept;

} // namespace PhantomLedger::activity::spending::simulator
