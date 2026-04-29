#pragma once

#include "phantomledger/relationships/social/config.hpp"
#include "phantomledger/spending/market/commerce/contacts.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::relationships::social {

struct BuildInputs {
  std::uint32_t personCount = 0;
  std::span<const std::uint8_t> hubFlags;
  std::uint64_t baseSeed = 0;
};

[[nodiscard]] spending::market::commerce::Contacts
build(const Social &cfg, const BuildInputs &inputs);

} // namespace PhantomLedger::relationships::social
