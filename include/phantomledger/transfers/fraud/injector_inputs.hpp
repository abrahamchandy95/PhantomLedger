#pragma once

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/entities/infra/shared.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/synth/people/fraud.hpp"
#include "phantomledger/synth/personas/timeline.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::transfers::fraud {

struct InjectorServices {
  random::Rng &rng;
  const infra::Router *router = nullptr;
  const infra::SharedInfra *ringInfra = nullptr;

  std::uint64_t fraudSeed = 0;
};

struct InjectorRingView {
  const synth::people::Fraud *profile = nullptr;
  const entity::person::Topology *topology = nullptr;

  // H3 part 3c-ii (authority U-8 addendum): the persona-timeline
  // carrier (PersonId-1 indexed). buildPlan derives each ring's alive
  // horizon — the MINIMUM death epoch over its fraud + mule
  // participants — so ring scheduling never recruits the dead. Empty
  // (packs without the carrier) stands the intervals down.
  std::span<const synth::personas::timeline::Timeline> timelines{};
};

struct InjectorAccountView {
  const entity::account::Registry *registry = nullptr;
  const entity::account::Ownership *ownership = nullptr;
};

struct InjectorLegitCounterparties {
  std::span<const entity::Key> billerAccounts{};
  std::span<const entity::Key> employers{};

  // card-fraud-realism-v2 step b (contract docs/card_fraud_v2_roadmap.md,
  // gate 1 of docs/card_fraud_online_gnn.md): THE MERCHANT ACCEPTANCE
  // POPULATION and the geographic axis that selects within it.
  //
  // THE DEFECT THESE CLOSE: the card and giftCardScam rails currently
  // draw their destination from `billerAccounts` above (the
  // legit-TRANSFER biller/hub pool) while legitimate card purchases
  // route through the market merchant CATALOG. The two populations are
  // disjoint, so every fraud row lands on a merchant with zero
  // legitimate card rows and merchant identity alone classifies the
  // corpus. Sharing ONE acceptance population is what makes the
  // card-fraud corpus a fraud problem rather than a lookup.
  //
  // THE CATALOG ITSELF, not a materialized key list: each Record
  // already carries `counterpartyId`, `location` (invalidGeoArea for an
  // `online` outlet), `footprint` and `weight`. The modality split
  // needs all four — Footprint::online IS the card-not-present
  // acceptance population, and `weight` is what keeps fraud selection
  // on the same size distribution legitimate selection uses. The
  // catalogue is built in the entity stage and outlives the fold, so a
  // borrowed pointer is safe.
  //
  // `homeAreas` is PersonId-1 indexed — the SAME compact carrier the
  // spending market reads (People::homeAreas, deliberately kept alive
  // past the export-pack release) — so card-present fraud decays from
  // the victim's home on the same axis legitimate activity does,
  // instead of a spatially uniform draw that would trade a
  // merchant-identity shortcut for a geographic one.
  //
  // UNREAD BY GENERATION until the step b-2 selection round: a null
  // catalogue and an empty span leave every draw exactly where it is,
  // so the carrier rounds move no golden byte.
  const entity::merchant::Catalog *merchants = nullptr;
  std::span<const entity::geography::GeoAreaId> homeAreas{};
};

} // namespace PhantomLedger::transfers::fraud
