#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::products {

[[nodiscard]] std::int32_t samplePaymentDay(::PhantomLedger::random::Rng &rng);

[[nodiscard]] ::PhantomLedger::time::TimePoint midday(int year, unsigned month,
                                                      unsigned day);

[[nodiscard]] bool inWindow(::PhantomLedger::time::TimePoint value,
                            ::PhantomLedger::time::Window window);

} // namespace PhantomLedger::entities::synth::products
