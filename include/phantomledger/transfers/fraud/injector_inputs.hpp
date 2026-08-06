#pragma once

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/entities/infra/shared.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/synth/people/fraud.hpp"
#include "phantomledger/synth/personas/pack.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
/*
  cardExposureWeights + the behaviour table it reads. Included HERE rather
  than at each consumer so every site that sees the carrier can derive it.
 */
#include "phantomledger/transfers/fraud/exposure.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::fraud {

struct InjectorServices {
  random::Rng &rng;
  const infra::Router *router = nullptr;
  const infra::SharedInfra *ringInfra = nullptr;

  /* The exogenous fraud-infrastructure pool the unauthorized card/ATO rails
   * transact from, borrowed from the world (pipeline::Infra::attackers). It is
   * a pool with a lifetime and a heavy-tailed case load, which is what puts
   * many cards behind one endpoint; minting a device and IP per compromise
   * makes cross-victim endpoint sharing ZERO BY CONSTRUCTION and leaves the
   * Device/IP layers unable to carry any message between two victims.
   *
   * nullptr degrades to VICTIM-ENDPOINT attribution for every case — the
   * remote-access/household branch — rather than to a ghost, which keeps
   * standalone callers meaningful. The production path is asserted non-null by
   * the orchestrator. */
  const infra::AttackerInfra *attackers = nullptr;

  std::uint64_t fraudSeed = 0;
};

struct InjectorRingView {
  const synth::people::Fraud *profile = nullptr;
  const entity::person::Topology *topology = nullptr;

  /* The persona-timeline carrier, PersonId-1 indexed (authority U-8
   * addendum). buildPlan derives each ring's alive horizon from it — the
   * MINIMUM death epoch over the ring's fraud + mule participants — so ring
   * scheduling never recruits the dead. Empty (packs without the carrier)
   * stands the intervals down. */
  std::span<const synth::personas::timeline::Timeline> timelines{};
};

struct InjectorAccountView {
  const entity::account::Registry *registry = nullptr;
  const entity::account::Ownership *ownership = nullptr;
};

struct InjectorLegitCounterparties {
  std::span<const entity::Key> billerAccounts{};
  std::span<const entity::Key> employers{};

  /* THE MERCHANT ACCEPTANCE POPULATION and the geographic axis that selects
   * within it (docs/card_fraud_v2_roadmap.md, gate 1 of
   * docs/card_fraud_online_gnn.md).
   *
   * The card and giftCardScam rails must NOT draw their destination from
   * `billerAccounts` above: that is the legit-TRANSFER biller/hub pool, while
   * legitimate card purchases route through the market merchant CATALOG. Two
   * disjoint populations put every fraud row on a merchant with zero
   * legitimate card rows, so merchant identity alone classifies the corpus.
   * Sharing ONE acceptance population is what makes this a fraud problem
   * rather than a lookup.
   *
   * THE CATALOG ITSELF, not a materialized key list: each Record already
   * carries `counterpartyId`, `location` (invalidGeoArea for an `online`
   * outlet), `footprint` and `weight`, and the modality split needs all four —
   * Footprint::online IS the card-not-present acceptance population, and
   * `weight` is what keeps fraud selection on the same size distribution
   * legitimate selection uses. The catalogue is built in the entity stage and
   * outlives the fold, so a borrowed pointer is safe.
   *
   * `homeAreas` is PersonId-1 indexed — the SAME compact carrier the spending
   * market reads (People::homeAreas, deliberately kept alive past the
   * export-pack release) — so card-present fraud decays from the victim's home
   * on the same axis legitimate activity does, instead of a spatially uniform
   * draw that would trade a merchant-identity shortcut for a geographic one. */
  const entity::merchant::Catalog *merchants = nullptr;
  std::span<const entity::geography::GeoAreaId> homeAreas{};

  /* The home-area HISTORY. The injector is the one consumer that needs the
   * history rather than a mutable current value: `buildCompromisePlans` plans
   * the WHOLE WINDOW in one pass before the fold runs, so there is no "now" to
   * read and a case in year 12 must resolve the home the victim occupied in
   * year 12. The spending market gets away with a monthly-refreshed snapshot
   * because it is walked day by day; using that snapshot here would attribute
   * every case to the victim's window-start area.
   *
   * Null means homes never move, and `homeAreas` above answers for all time. */
  const entity::parties::relocation::Schedule *relocation = nullptr;

  /* THE PERSONAS PACK — persona-at-date, age-at-date and the join cohort,
   * borrowed (it rides the blueprint into the fold and outlives the injection,
   * like the two carriers above); docs/card_fraud_victimization.md D2.
   *
   * WHAT READS IT: the SCAM rails select victims on a persona x age
   * susceptibility hazard rebuilt AT EACH CASE DATE and grade the loss by the
   * victim's age, and every rail requires bank MEMBERSHIP at the case date
   * (joined — and alive too on the scam rails, where a dead victim is an
   * impossibility rather than a typology). See susceptibility.hpp for the two
   * opposite age gradients and why exposure is the wrong axis for a scam.
   *
   * ONE POINTER, NOT THREE SPANS, and keep it that way: the hazard needs the
   * timeline, the birth date and the join day TOGETHER, so three carriers
   * would be three chances for a call site to fill some and not others — on
   * exactly the path test_arch_equivalence guards. A site either passes the
   * pack and gets the whole victim-side model, or passes nothing.
   *
   * nullptr = no scam tilt, no severity grading, no membership gate. */
  const synth::personas::Pack *personas = nullptr;

  /* PER-PERSON CARD-EXPOSURE WEIGHTS, PersonId-1 indexed, mean ~1.0, produced
   * by fraud::cardExposureWeights (exposure.hpp;
   * docs/card_fraud_victimization.md D1). Without it every customer carries
   * identical hazard, which leaves every victim-side feature as noise BY
   * CONSTRUCTION.
   *
   * OWNED BY VALUE, unlike every other member here, and it must stay so. The
   * weights are a DERIVED quantity with no home in the world model, so a span
   * would need a caller-side vector at each of the four call sites — four
   * chances to compute it differently, on the exact path
   * test_arch_equivalence guards. Owning it lets ONE factory
   * (FraudEmission::legitCounterparties) derive it for every site from the
   * pack's behaviour table. The vector is population-sized (~40 KB at 5,000
   * people) and built once per inject.
   *
   * EMPTY leaves selection on the plain uniform draw BIT-IDENTICALLY, because
   * the picker branches on it, so an unfilled call site moves no golden byte.
   * As with the carriers above, every site that should tilt must be filled or
   * its gates measure a carrier-free world. */
  std::vector<double> cardExposure{};
};

} // namespace PhantomLedger::transfers::fraud
