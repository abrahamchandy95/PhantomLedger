#pragma once

#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace PhantomLedger::entities::synth::pii {

[[nodiscard]] inline entity::pii::Record
buildRecord(entity::PersonId person, random::Rng &rng,
            const entity::behavior::Assignment &personas,
            time::TimePoint simStart, const LocaleMix &mix,
            const PoolSet &pools) {
  const auto country = sampleCountry(rng, mix);
  const auto &pool = pools.forCountry(country);

  entity::pii::Record rec{};
  rec.country = country;
  rec.email = sampleEmail(person);
  rec.name = sampleName(rng, pool);
  rec.ssn = sampleSsn(rng, country);
  rec.phone = samplePhone(rng, country);
  rec.dob = sampleDob(rng, personas.byPerson[person - 1], simStart);
  rec.address = sampleAddress(rng, pool);
  return rec;
}

template <typename Sink>
inline void generate(random::Rng &rng,
                     const entity::behavior::Assignment &personas,
                     time::TimePoint simStart, const LocaleMix &mix,
                     const PoolSet &pools, Sink &&sink) {
  const auto population = personas.byPerson.size();
  for (std::uint64_t p = 1; p <= population; ++p) {
    const auto person = static_cast<entity::PersonId>(p);
    const auto rec = buildRecord(person, rng, personas, simStart, mix, pools);
    sink(person, rec);
  }
}

[[nodiscard]] inline entity::pii::Roster
make(random::Rng &rng, const entity::behavior::Assignment &personas,
     time::TimePoint simStart, const LocaleMix &mix, const PoolSet &pools) {
  entity::pii::Roster out;
  out.records.reserve(personas.byPerson.size());

  generate(rng, personas, simStart, mix, pools,
           [&out](entity::PersonId, const entity::pii::Record &rec) {
             out.records.push_back(rec);
           });

  return out;
}

[[nodiscard]] inline entity::pii::Roster
make(random::Rng &rng, const entity::person::Roster &people,
     const entity::behavior::Assignment &personas, time::TimePoint simStart,
     const LocaleMix &mix, const PoolSet &pools) {
  if (people.count != personas.byPerson.size()) {
    throw std::invalid_argument(
        "pii::make: people roster size != persona assignment size");
  }
  return make(rng, personas, simStart, mix, pools);
}

} // namespace PhantomLedger::entities::synth::pii
