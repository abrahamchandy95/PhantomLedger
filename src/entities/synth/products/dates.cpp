#include "phantomledger/entities/synth/products/dates.hpp"

namespace PhantomLedger::entities::synth::products {

[[nodiscard]] std::int32_t samplePaymentDay(::PhantomLedger::random::Rng &rng) {
  return static_cast<std::int32_t>(rng.uniformInt(1, 29));
}

[[nodiscard]] ::PhantomLedger::time::TimePoint midday(int year, unsigned month,
                                                      unsigned day) {
  return ::PhantomLedger::time::makeTime(
      ::PhantomLedger::time::CalendarDate{year, month, day},
      ::PhantomLedger::time::TimeOfDay{12, 0, 0});
}

[[nodiscard]] bool inWindow(::PhantomLedger::time::TimePoint value,
                            ::PhantomLedger::time::Window window) {
  return value >= window.start && value < window.endExcl();
}

} // namespace PhantomLedger::entities::synth::products
