#pragma once

#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/entities/synth/pii/identity.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <cassert>
#include <cstdint>

namespace PhantomLedger::entities::synth::pii {

class Generator {
public:
  Generator(random::Rng &rng, const entity::behavior::Assignment &personas,
            const IdentityContext &context) noexcept
      : rng_(&rng), personas_(&personas), context_(context) {
    assert(context_.pools != nullptr &&
           "pii::Generator: IdentityContext::pools must be set");
  }

  [[nodiscard]] entity::pii::Roster make();

private:
  [[nodiscard]] entity::pii::Record buildRecord(entity::PersonId person);

  random::Rng *rng_;
  const entity::behavior::Assignment *personas_;
  IdentityContext context_;
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

} // namespace PhantomLedger::entities::synth::pii
