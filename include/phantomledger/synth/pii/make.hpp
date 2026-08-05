#pragma once

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/pii.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/geo/residence.hpp"
#include "phantomledger/synth/pii/identity.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"

#include <array>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::synth::pii {

namespace geo = ::PhantomLedger::synth::geo;
namespace geoent = ::PhantomLedger::entity::geography;
namespace fam = ::PhantomLedger::relationships::family;

// Reproduce the family HOUSEHOLD partition byte-identically to the transfer
// stage (`family::build` draws it from `RngFactory{plan.seed()}` on the
// `{"family","households"}` lane; `plan.seed() == the run seed == worldSeed`,
// and production uses `kDefaultHouseholds`). This gives the home-placement
// layer the SAME households the family graph uses, so spouses and coresident
// dependants share one address. It draws only on a dedicated factory — never
// the shared entity stream.
//
// SHARED, not duplicated: `parties::relocation` needs this exact partition so
// a household relocates as a unit. Deriving it twice from one seed is the
// class of duplication that silently drifts when one copy is touched.
[[nodiscard]] inline fam::Partition
reproduceHouseholds(std::uint64_t worldSeed,
                    const entity::behavior::Assignment &personas) {
  const auto personCount =
      static_cast<std::uint32_t>(personas.byPerson.size());
  auto rng = random::RngFactory{worldSeed}.rng({"family", "households"});
  return fam::partition(fam::kDefaultHouseholds, rng, personCount);
}

class Generator {
public:
  Generator(random::Rng &rng, const entity::behavior::Assignment &personas,
            const IdentityContext &context)
      : rng_(&rng), personas_(&personas), context_(context),
        homeGeoFactory_(context.worldSeed), residence_(geo::geography()),
        households_(reproduceHouseholds(context.worldSeed, personas)) {
    assert(context_.pools != nullptr &&
           "pii::Generator: IdentityContext::pools must be set");
    // H2 step 2a (single age axis): the Dob RENDERS from the carrier
    // drawn on the isolated {"dob", personId} lanes — the shared
    // entity stream draws no dob (authority U-7).
    assert(context_.birthDates != nullptr &&
           context_.birthDates->size() == personas.byPerson.size() &&
           "pii::Generator: IdentityContext::birthDates must carry one "
           "birth date per person (Pack::birthDates)");
  }

  [[nodiscard]] entity::pii::Roster make();

private:
  [[nodiscard]] entity::pii::Record buildRecord(entity::PersonId person);

  // Reproduce the family HOUSEHOLD partition byte-identically to the
  // transfer stage (family::build draws it from RngFactory{plan.seed()}
  // on the {"family","households"} lane; plan.seed() == the run seed ==
  // worldSeed, and production uses kDefaultHouseholds). This gives the
  // home-placement layer the SAME households the family graph uses, so
  // spouses/coresident dependants share one address. It draws only on a
  // dedicated factory — never the shared entity stream.
  //
  // relocation-2026-07: this is now `pii::reproduceHouseholds` (below, free)
  // because the relocation schedule needs the SAME partition — a household
  // moves as a unit, so relocating per person would split coresidents across
  // areas and falsify an invariant `party-geography-2026-07` declares
  // binding. TWO derivations of one seed drift; there is one.
  [[nodiscard]] static fam::Partition
  reproduceHouseholds(std::uint64_t worldSeed,
                      const entity::behavior::Assignment &personas) {
    return ::PhantomLedger::synth::pii::reproduceHouseholds(worldSeed,
                                                            personas);
  }

  // A population-weighted home area for `person` from the EMBEDDED world
  // catalogue, drawn on the isolated {"home-geo", <household>} lane so
  // every member of a household resolves to the SAME area (coresidence)
  // and home placement never perturbs the shared entity stream.
  [[nodiscard]] geoent::GeoAreaId homeAreaFor(entity::PersonId person,
                                              locale::Country country) {
    const auto idx = static_cast<std::size_t>(person) - 1;
    const std::uint32_t household =
        idx < households_.householdOf.size()
            ? households_.householdOf[idx]
            : static_cast<std::uint32_t>(idx); // lone fallback
    std::array<char, 20> buf{};
    const auto [ptr, ec] = std::to_chars(
        buf.data(), buf.data() + buf.size(),
        static_cast<unsigned long long>(household));
    (void)ec;
    const std::string_view hhStr(buf.data(),
                                 static_cast<std::size_t>(ptr - buf.data()));
    auto rng = homeGeoFactory_.rng({"home-geo", hhStr});
    return residence_.sample(rng, country);
  }

  // H2 step 2a: the carrier's calendar date rendered as the record's
  // Dob — the SAME age the SSA cohort and the persona timeline read.
  [[nodiscard]] entity::pii::Dob dobFromCarrier(entity::PersonId person) const {
    const auto &bd = (*context_.birthDates)[static_cast<std::size_t>(person) - 1];
    return entity::pii::Dob{
        static_cast<std::int16_t>(bd.year),
        static_cast<std::uint8_t>(bd.month),
        static_cast<std::uint8_t>(bd.day),
    };
  }

  random::Rng *rng_;
  const entity::behavior::Assignment *personas_;
  IdentityContext context_;
  random::RngFactory homeGeoFactory_;
  geo::ResidenceSampler residence_;
  fam::Partition households_;
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
  // H2 step 2a: dob comes from the single-age-axis carrier, NOT the
  // shared stream (the two in-stream dob draws are gone — every draw
  // after this point in the stream shifted once, absorbed by the H2
  // re-pin).
  rec.dob = dobFromCarrier(person);
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
