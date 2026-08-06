#pragma once

#include "phantomledger/entities/counterparties/directory.hpp"
#include "phantomledger/entities/counterparties/merchant_ownership.hpp"
#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/accounts/business_owners.hpp"
#include "phantomledger/synth/accounts/pack.hpp"
#include "phantomledger/synth/accounts/sizing.hpp"
#include "phantomledger/synth/cards/issue.hpp"
#include "phantomledger/synth/counterparties/make.hpp"
#include "phantomledger/synth/econ/catalog.hpp"
#include "phantomledger/synth/landlords/make.hpp"
#include "phantomledger/synth/landlords/pack.hpp"
#include "phantomledger/synth/merchants/make.hpp"
#include "phantomledger/synth/people/fraud.hpp"
#include "phantomledger/synth/people/pack.hpp"
#include "phantomledger/synth/personas/kinds.hpp"
#include "phantomledger/synth/personas/pack.hpp"
#include "phantomledger/synth/pii/identity.hpp"
#include "phantomledger/synth/pii/sharing.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline::stages::entities {

namespace pl = ::PhantomLedger;
namespace sy = pl::synth;
namespace entity = pl::entity;

struct EntitySynthesis {
  std::int32_t population;
  sy::pii::IdentityContext identity;
  sy::pii::Sharing piiSharing{};
  sy::people::Fraud fraud{};
  sy::personas::Mix personaMix{};
  sy::accounts::Sizing accountsSizing{};
  sy::merchants::GenerationPlan merchants{};
  sy::landlords::GenerationPlan landlords{};
  sy::counterparties::CounterpartyTargets counterpartyTargets{};
  sy::cards::IssuanceRules cards{};

  sy::accounts::BusinessOwnerPlan businessOwners{};

  void validate(pl::primitives::validate::Report &r) const {
    r.check([&] {
      pl::primitives::validate::nonNegative("population", population);
    });
    piiSharing.validate(r);
    accountsSizing.validate(r);
    merchants.validate(r);
    landlords.validate(r);
    counterpartyTargets.validate(r);
  }
};

[[nodiscard]] sy::pii::IdentityContext
defaultStart(sy::pii::IdentityContext identity,
             pl::time::TimePoint fallbackStart);

[[nodiscard]] synth::people::Pack
buildPeople(pl::random::Rng &rng, std::int32_t population,
            const synth::people::Fraud &fraud = {});

[[nodiscard]] sy::accounts::Pack
buildAccounts(pl::random::Rng &rng, const synth::people::Pack &people,
              std::int32_t population, const sy::accounts::Sizing &sizing = {});

/* The pack carries the single-age-axis birth-date carrier, drawn on the
 * isolated {"dob", personId} lanes off identity.worldSeed at
 * identity.simStart (the persona age bands). `identity` must be the
 * defaultStart()-resolved context the PII build receives, so the carrier and
 * the rendered PII agree. */
[[nodiscard]] sy::personas::Pack
buildPersonas(pl::random::Rng &rng, const synth::people::Pack &people,
              const sy::pii::IdentityContext &identity,
              const sy::personas::Mix &mix = {});

[[nodiscard]] entity::pii::Roster
buildPii(pl::random::Rng &rng, const sy::personas::Pack &personas,
         sy::pii::IdentityContext identity,
         const entity::person::Topology &topology,
         const sy::pii::Sharing &sharing);

/* geoSeed seeds the isolated merchant footprint/location lanes and the
 * lifecycle lane; it never touches `rng`, the shared entity stream. Pass the
 * run seed.
 *
 * `window` is REQUIRED for lifecycle: the catalogue is sized from the window
 * length — a longer run needs more records, because deaths must be replaced by
 * births or the pool thins instead of churning — and birth and death dates are
 * placed inside it. A zero-day window leaves every record always-live. */
[[nodiscard]] entity::merchant::Catalog
buildMerchants(pl::random::Rng &rng, std::int32_t population,
               std::uint64_t geoSeed, pl::time::Window window,
               const sy::merchants::GenerationPlan &plan = {},
               const pl::synth::econ::MacroSeries *macro = nullptr);

[[nodiscard]] sy::landlords::Pack
buildLandlords(pl::random::Rng &rng, std::int32_t population,
               const sy::landlords::GenerationPlan &plan = {});

/* `startYear` is the WINDOW-START year for the credit-limit stock scale
 * (synth::cards::issue); 0 means calibration-flat, for direct tests. */
[[nodiscard]] entity::card::Registry
issueCreditCards(const sy::personas::Pack &personas,
                 const synth::people::Pack &people, std::uint64_t topLevelSeed,
                 const sy::cards::IssuanceRules &issuance = {},
                 int startYear = 0);

[[nodiscard]] entity::counterparty::Directory buildCounterparties(
    pl::random::Rng &rng, std::int32_t population,
    const sy::counterparties::CounterpartyTargets &targets = {});

void finalizeAccountRegistry(pl::pipeline::Holdings &holdings,
                             const pl::pipeline::Counterparties &cps,
                             const pl::pipeline::People &people);

void synthesizeBusinessOwners(pl::pipeline::Holdings &holdings,
                              const pl::pipeline::People &people,
                              pl::random::Rng &rng,
                              const sy::accounts::BusinessOwnerPlan &plan = {});

/* The per-person home-area HISTORY.
 *
 * TAKES NO Rng, deliberately: it draws on its own
 * `{"home-relocation", <group>}` lanes off `worldSeed`, so it cannot perturb
 * the shared entity stream. `initialAreas` must be the `homeAreasOf(pii)`
 * snapshot — tenure 0 is required to be byte-identical to it, which is what
 * makes a zero-move window reproduce an immobile corpus exactly.
 *
 * A zero-day window returns an EMPTY schedule, and every consumer treats that
 * exactly as it treats an unbound `homeAreas` carrier. */
[[nodiscard]] pl::entity::parties::relocation::Schedule buildRelocation(
    const entity::pii::Roster &pii,
    const std::vector<pl::entity::geography::GeoAreaId> &initialAreas,
    const sy::personas::Pack &personas, std::uint64_t worldSeed,
    pl::time::Window window);

/* Stamp each catalog merchant with the proprietor Party the institution holds
 * a beneficial-owner record for, or leave `invalidPerson`. DRAW-FREE — it
 * takes no Rng, so it cannot move the corpus — and it must run AFTER
 * synthesizeBusinessOwners, whose cohort it reads back out of the account
 * registry. Exported as `cf_Is_Merchant`, whose emptiness hard-aborts the
 * downstream loader push. */
void assignMerchantOwners(
    pl::entity::merchant::Catalog &merchants,
    const pl::entity::account::Registry &accounts,
    double coverage =
        pl::entity::merchant::ownership::kBeneficialOwnerCoverage);

} // namespace PhantomLedger::pipeline::stages::entities
