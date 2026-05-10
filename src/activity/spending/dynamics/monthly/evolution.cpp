#include "phantomledger/activity/spending/dynamics/monthly/evolution.hpp"

namespace PhantomLedger::spending::dynamics::monthly {

void evolveAll(random::Rng &rng, const math::evolution::Config &cfg,
               market::commerce::View &commerce, std::uint32_t totalPersons) {
  auto &contacts = commerce.contactsMutable();

  for (std::uint32_t personIdx = 0; personIdx < totalPersons; ++personIdx) {
    const auto row = contacts.rowOfMutable(personIdx);
    math::evolution::evolveContacts(rng, cfg,
                                    math::evolution::ContactRow{
                                        .row = row,
                                        .personIdx = personIdx,
                                        .nPeople = totalPersons,
                                    });
  }

  // TODO(structural): favorites add/drop pass once the Favorites
  // CSR supports variable-length rows. When wiring this up, build
  // a math::evolution::MerchantPool from commerce.merchCdf() and
  // pass it to evolveFavorites().
}

} // namespace PhantomLedger::spending::dynamics::monthly
