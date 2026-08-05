#pragma once

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/entities/infra/shared.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/synth/people/fraud.hpp"
#include "phantomledger/synth/personas/pack.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
// victimization-v2: cardExposureWeights + the behaviour table it reads.
// Included HERE rather than at each consumer so every site that sees the
// carrier can also derive it.
#include "phantomledger/transfers/fraud/exposure.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::fraud {

struct InjectorServices {
  random::Rng &rng;
  const infra::Router *router = nullptr;
  const infra::SharedInfra *ringInfra = nullptr;

  // attacker-infra-2026-07: the exogenous fraud-infrastructure pool the
  // unauthorized card/ATO rails transact from, borrowed from the world
  // (pipeline::Infra::attackers).
  //
  // THE DEFECT IT CLOSES: `buildCompromisePlans` minted a brand-new
  // device and IP per compromise, so cross-victim endpoint sharing was
  // ZERO BY CONSTRUCTION and the Device/IP layers could carry no message
  // between two victims. Attacker endpoints now come from a pool with a
  // lifetime and a heavy-tailed case load, which is what puts many cards
  // behind one endpoint.
  //
  // nullptr degrades to VICTIM-ENDPOINT attribution for every case — the
  // remote-access/household branch — rather than to a ghost. That keeps
  // standalone callers meaningful without reintroducing the defect, and
  // the production path is asserted non-null by the orchestrator.
  const infra::AttackerInfra *attackers = nullptr;

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
  // THE DEFECT THESE CLOSE: the card and giftCardScam rails drew their
  // destination from `billerAccounts` above (the legit-TRANSFER
  // biller/hub pool) while legitimate card purchases route through the
  // market merchant CATALOG. The two populations were disjoint, so every
  // fraud row landed on a merchant with zero legitimate card rows and
  // merchant identity alone classified the corpus. Sharing ONE
  // acceptance population is what makes the card-fraud corpus a fraud
  // problem rather than a lookup.
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
  const entity::merchant::Catalog *merchants = nullptr;
  std::span<const entity::geography::GeoAreaId> homeAreas{};

  // relocation-2026-07: the home-area HISTORY, and the injector is the one
  // consumer that needs the HISTORY rather than a mutable current value.
  //
  // `buildCompromisePlans` plans the WHOLE WINDOW in one pass, before the fold
  // runs, so there is no "now" for it to read — a case in year 12 must resolve
  // the home the victim occupied in year 12. The spending market can get away
  // with a monthly-refreshed snapshot because it is walked day by day; this is
  // not, and using the snapshot here would attribute every case to the
  // victim's window-start area.
  //
  // Null ⇒ homes never move, and `homeAreas` above answers for all time.
  const entity::parties::relocation::Schedule *relocation = nullptr;

  // victimization-v3 (contract docs/card_fraud_victimization.md D2):
  // THE PERSONAS PACK — persona-at-date, age-at-date and the join
  // cohort, borrowed (it rides the blueprint into the fold and outlives
  // the injection, like the two carriers above).
  //
  // WHAT READS IT: the SCAM rails select victims on a persona x age
  // susceptibility hazard rebuilt AT EACH CASE DATE, grade the loss by
  // the victim's age, and every rail now requires bank MEMBERSHIP at
  // the case date (joined; alive too on the scam rails, where a dead
  // victim is not a typology but an impossibility). See
  // transfers/fraud/susceptibility.hpp for the two opposite age
  // gradients and why exposure is the wrong axis for a scam.
  //
  // ONE POINTER, not three spans, and that is the parity decision: the
  // hazard needs the timeline, the birth date and the join day
  // TOGETHER, so three separate carriers would be three chances for a
  // call site to fill some and not others — on exactly the path
  // test_arch_equivalence guards. A site either passes the pack and
  // gets the whole victim-side model or passes nothing and degrades
  // cleanly to the v2 behaviour.
  //
  // nullptr = no scam tilt, no severity grading, no membership gate.
  const synth::personas::Pack *personas = nullptr;

  // victimization-v2 (contract docs/card_fraud_victimization.md D1):
  // PER-PERSON CARD-EXPOSURE WEIGHTS, PersonId-1 indexed, mean ~1.0,
  // produced by fraud::cardExposureWeights (exposure.hpp).
  //
  // THE DEFECT IT CLOSES: buildCompromisePlans drew victims with
  // rng.choiceIndex — identical hazard for every customer. Real
  // unauthorized card fraud is exposure-driven, and a uniform draw
  // leaves every victim-side feature as noise BY CONSTRUCTION, so a GNN
  // has nothing legitimate to learn on that side of the graph.
  //
  // OWNED BY VALUE, unlike every other member here. The weights are a
  // DERIVED quantity with no home in the world model, so a span would
  // need a caller-side vector at each of the four call sites — four
  // chances to compute it differently, on the exact path
  // test_arch_equivalence guards. Owning it lets ONE factory
  // (FraudEmission::legitCounterparties) derive it for every site from
  // the pack's behaviour table, so a site either passes the pack and
  // gets the right weights or passes nothing and degrades cleanly. The
  // vector is population-sized (~40 KB at 5000 people) and built once
  // per inject.
  //
  // EMPTY leaves selection on the pre-v2 uniform draw, BIT-IDENTICALLY —
  // the picker branches on it — so an unfilled call site moves no golden
  // byte. As with the carriers above, every site that should tilt must
  // be filled or its gates measure a carrier-free world.
  std::vector<double> cardExposure{};
};

} // namespace PhantomLedger::transfers::fraud
