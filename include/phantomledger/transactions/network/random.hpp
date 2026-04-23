#pragma once

#include "format.hpp"
#include "ipv4.hpp"
#include "phantomledger/entropy/random/rng.hpp"

#include <cstdint>
#include <string>

namespace PhantomLedger::network {

[[nodiscard]] inline Ipv4 randomIpv4(random::Rng &rng) {
  return Ipv4::pack(static_cast<std::uint8_t>(rng.uniformInt(11, 223)),
                    static_cast<std::uint8_t>(rng.uniformInt(0, 256)),
                    static_cast<std::uint8_t>(rng.uniformInt(0, 256)),
                    static_cast<std::uint8_t>(rng.uniformInt(1, 255)));
}

[[nodiscard]] inline std::string randomIp(random::Rng &rng) {
  return format(randomIpv4(rng));
}

} // namespace PhantomLedger::network
