#include "phantomledger/spending/dynamics/monthly/evolution.hpp"

namespace PhantomLedger::spending::dynamics::monthly {

void evolveAll(random::Rng &rng, const math::evolution::Config &cfg,
               market::commerce::View &commerce,
               std::span<const double> /*globalMerchCdf*/,
               std::uint32_t /*totalMerchants*/, std::uint32_t totalPersons) {
  auto &contacts = commerce.contactsMutable();

  for (std::uint32_t personIdx = 0; personIdx < totalPersons; ++personIdx) {
    const auto row = contacts.rowOfMutable(personIdx);
    math::evolution::evolveContacts(rng, cfg, row, personIdx, totalPersons);
  }

  // TODO(structural): favorites add/drop pass once the Favorites
  // CSR supports variable-length rows.
}

} // namespace PhantomLedger::spending::dynamics::monthly
