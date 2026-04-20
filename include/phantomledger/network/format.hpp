#pragma once

#include "phantomledger/network/ipv4.hpp"

#include <string>

namespace PhantomLedger::network {

[[nodiscard]] inline std::string format(Ipv4 ip) {
  return std::to_string(ip.octet1()) + "." + std::to_string(ip.octet2()) + "." +
         std::to_string(ip.octet3()) + "." + std::to_string(ip.octet4());
}

} // namespace PhantomLedger::network
