#pragma once

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/pii.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/geo/residence.hpp"
#include "phantomledger/synth/pii/identity.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"

#include <array>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::synth::pii {

namespace geo = ::PhantomLedger::synth::geo;
namespace geoent = ::PhantomLedger::entity::geography;

class Generator {
public:
  Generator(random::Rng &rng, const entity::behavior::Assignment &personas,
            const IdentityContext &context)
      : rng_(&rng), personas_(&personas), context_(context),
        homeGeoFactory_(context.geoSeed), residence_(geo::geography()) {
    assert(context_.pools != nullptr &&
           "pii::Generator: IdentityContext::pools must be set");
  }

  [[nodiscard]] entity::pii::Roster make();

private:
  [[nodiscard]] entity::pii::Record buildRecord(entity::PersonId person);

  // geo-causal-v1: a population-weighted home area for `person` from the
  // EMBEDDED world catalogue, drawn on the isolated {"home-geo",
  // <person>} lane so it never perturbs the shared entity stream.
  // invalidGeoArea when the person's country has no residential area in
  // the catalogue. (G1: per-person; becomes per-household when the
  // household partition is unified.)
  [[nodiscard]] geoent::GeoAreaId homeAreaFor(entity::PersonId person,
                                              locale::Country country) {
    std::array<char, 20> idBuf{};
    const auto [ptr, ec] =
        std::to_chars(idBuf.data(), idBuf.data() + idBuf.size(),
                      static_cast<unsigned long long>(person));
    (void)ec;
    const std::string_view idStr(idBuf.data(),
                                 static_cast<std::size_t>(ptr - idBuf.data()));
    auto rng = homeGeoFactory_.rng({"home-geo", idStr});
    return residence_.sample(rng, country);
  }

  random::Rng *rng_;
  const entity::behavior::Assignment *personas_;
  IdentityContext context_;
  random::RngFactory homeGeoFactory_;
  geo::ResidenceSampler residence_;
};

inline entity::pii::Record Generator::buildRecord(entity::PersonId person) {
  const auto country = sampleCountry(*rng_, context_.localeMix);
  const auto &pool = context_.pools->forCountry(country);

  entity::pii::Record rec{};
  rec.country = country;
  rec.email = sampleEmail(person);
  rec.name = sampleName(*rng_, pool);
  rec.ssn = sampleSsn(*rng_, country);
  rec.phone = samplePhone(*rng_, country);
  rec.dob =
      sampleDob(*rng_, personas_->byPerson[person - 1], context_.simStart);
  rec.address = sampleAddress(*rng_, pool);
  rec.address.geoArea = homeAreaFor(person, country);
  return rec;
}

inline entity::pii::Roster Generator::make() {
  const auto population = personas_->byPerson.size();
  entity::pii::Roster out;
  out.records.reserve(population);
  for (std::uint64_t p = 1; p <= population; ++p) {
    out.records.push_back(buildRecord(static_cast<entity::PersonId>(p)));
  }
  return out;
}

[[nodiscard]] inline entity::pii::Roster
make(random::Rng &rng, const entity::behavior::Assignment &personas,
     const IdentityContext &context) {
  return Generator{rng, personas, context}.make();
}

} // namespace PhantomLedger::synth::pii
