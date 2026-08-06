#include "phantomledger/pipeline/stages/entities.hpp"

#include "phantomledger/entities/counterparties/institutional_accounts.hpp"
#include "phantomledger/entities/counterparties/merchant_ownership.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/accounts/assign.hpp"
#include "phantomledger/synth/accounts/business_owners.hpp"
#include "phantomledger/synth/accounts/make.hpp"
#include "phantomledger/synth/cards/issue.hpp"
#include "phantomledger/synth/cards/seeds.hpp"
#include "phantomledger/synth/counterparties/make.hpp"
#include "phantomledger/synth/family/pick.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/geo/residence.hpp"
#include "phantomledger/synth/landlords/make.hpp"
#include "phantomledger/synth/merchants/lifecycle.hpp"
#include "phantomledger/synth/merchants/make.hpp"
#include "phantomledger/synth/merchants/place.hpp"
#include "phantomledger/synth/people/make.hpp"
#include "phantomledger/synth/personas/dob.hpp"
#include "phantomledger/synth/personas/join.hpp"
#include "phantomledger/synth/personas/make.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/synth/pii/correlate.hpp"
#include "phantomledger/synth/pii/make.hpp"
#include "phantomledger/synth/pii/relocation_build.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <ranges>
#include <span>
#include <vector>

namespace PhantomLedger::pipeline::stages::entities {

[[nodiscard]] sy::pii::IdentityContext
defaultStart(sy::pii::IdentityContext identity,
             pl::time::TimePoint fallbackStart) {
  if (identity.simStart == pl::time::TimePoint{}) {
    identity.simStart = fallbackStart;
  }
  return identity;
}

[[nodiscard]] sy::people::Pack buildPeople(pl::random::Rng &rng,
                                           std::int32_t population,
                                           const synth::people::Fraud &fraud) {
  pl::primitives::validate::nonNegative("population", population);
  return synth::people::make(rng, population, fraud);
}

[[nodiscard]] sy::accounts::Pack
buildAccounts(pl::random::Rng &rng, const synth::people::Pack &people,
              std::int32_t population, const sy::accounts::Sizing &sizing) {
  pl::primitives::validate::nonNegative("population", population);
  pl::primitives::validate::require(sizing);
  return sy::accounts::makePack(rng, people.roster,
                                sizing.maxAccountsPerPerson);
}

[[nodiscard]] sy::personas::Pack
buildPersonas(pl::random::Rng &rng, const synth::people::Pack &people,
              const sy::pii::IdentityContext &identity,
              const sy::personas::Mix &mix) {
  const std::uint64_t personasSeed = rng.nextU64();
  auto pack =
      sy::personas::makePack(rng, people.roster.count, personasSeed, mix);

  /* The JOIN-COHORT carrier FIRST: a BEA-sized joiner count, one isolated
   * {"join-cohort", personId} draw per joiner (authority U-8 addendum).
   * identity.windowDays == 0 (direct unit harnesses) leaves everyone a member
   * from the start, and the dob and timeline anchors below then reduce to a
   * no-cohort shape. */
  pack.joinDays = sy::personas::join_cohort::deriveJoinDays(
      identity.worldSeed, people.roster.count,
      pl::time::Window{.start = identity.simStart,
                       .days = identity.windowDays});

  /* The single-age-axis carrier, drawn on the isolated {"dob", personId}
   * lanes (worldSeed factory) — never the shared entity stream, so the
   * assignment and profiles above are untouched. PII rendering, SSA cohorts
   * and the persona timeline all read it; a joiner anchors at their JOIN
   * date. */
  pack.birthDates = sy::personas::birthDates(
      identity.worldSeed, identity.simStart, pack.assignment, pack.joinDays);

  /* The persona timeline per person, on the isolated
   * {"persona-era", personId} lanes off the same factory. Salary
   * selection/spans, SSA onset and revenue gating read personaAt. The
   * seed-state clamps and the lifespan's alive invariant bind at each person's
   * own anchor — the join date for the cohort — so a joiner dies strictly
   * after joining. */
  pack.timelines = sy::personas::timeline::deriveAll(
      identity.worldSeed, identity.simStart, pack.assignment, pack.birthDates,
      pack.joinDays);
  return pack;
}

[[nodiscard]] entity::pii::Roster
buildPii(pl::random::Rng &rng, const sy::personas::Pack &personas,
         sy::pii::IdentityContext identity,
         const entity::person::Topology &topology,
         const sy::pii::Sharing &sharing) {
  assert(identity.pools != nullptr &&
         "buildPii: IdentityContext::pools must be set. main is the sole "
         "owner of the PoolSet pointer.");
  /* The roster renders each Dob from the pack's carrier (single age axis), so
   * the Generator draws no dob on the shared stream. */
  identity.birthDates = &personas.birthDates;
  auto pii = sy::pii::make(rng, personas.assignment, identity);
  sy::pii::correlateRingPii(rng, topology, sharing, pii);
  return pii;
}

[[nodiscard]] pl::entity::parties::relocation::Schedule buildRelocation(
    const entity::pii::Roster &pii,
    const std::vector<pl::entity::geography::GeoAreaId> &initialAreas,
    const sy::personas::Pack &personas, std::uint64_t worldSeed,
    pl::time::Window window) {
  // The per-person country, read off the live roster. The destination
  // constraint needs it: a relocation stays inside the origin's country
  // because `country` drives locale, PII format and the whole identity layer,
  // none of which can move mid-run.
  std::vector<locale::Country> countries;
  countries.reserve(pii.records.size());
  for (const auto &record : pii.records) {
    countries.push_back(record.country);
  }

  // The SAME household partition home placement used — shared, not
  // re-derived. Real coresidents must move together.
  const auto households =
      sy::pii::reproduceHouseholds(worldSeed, personas.assignment);

  static const pl::synth::geo::ResidenceSampler residence{
      pl::synth::geo::geography()};

  sy::pii::relocation::Inputs in{};
  in.worldSeed = worldSeed;
  in.initialAreas = initialAreas;
  in.householdOf = households.householdOf;
  in.countries = countries;
  in.catalog = &pl::synth::geo::geography();
  in.residence = &residence;
  in.window = window;
  return sy::pii::relocation::build(in);
}

[[nodiscard]] entity::merchant::Catalog
buildMerchants(pl::random::Rng &rng, std::int32_t population,
               std::uint64_t geoSeed, pl::time::Window window,
               const sy::merchants::GenerationPlan &plan,
               const pl::synth::econ::MacroSeries *macro) {
  pl::primitives::validate::nonNegative("population", population);
  pl::primitives::validate::require(plan);
  auto catalog = sy::merchants::makeCatalog(rng, population, plan);
  /* Geography and lifecycle both ride isolated per-merchant lanes (geoSeed),
   * never the shared entity stream `rng`. That keeps a change to either one
   * from perturbing any other entity-stage value; it does NOT make either
   * corpus-neutral, because selection sees a time-varying live set. */
  sy::merchants::placeGeography(catalog, geoSeed);
  /* This order, for this reason: the BASE catalogue is the incumbent cohort
   * (live when the window opens), and the replacements appended next are the
   * births that keep the live count from decaying as incumbents die. Both the
   * append and the interval assignment ride isolated per-record lanes off
   * geoSeed, so the shared entity stream `rng` is untouched. */
  sy::merchants::appendChurnReplacements(catalog, window, geoSeed, plan);
  sy::merchants::assignLifecycle(catalog, window, geoSeed, macro);
  return catalog;
}

[[nodiscard]] sy::landlords::Pack
buildLandlords(pl::random::Rng &rng, std::int32_t population,
               const sy::landlords::GenerationPlan &plan) {
  pl::primitives::validate::nonNegative("population", population);
  pl::primitives::validate::require(plan);
  return sy::landlords::makePack(rng, population, plan);
}

[[nodiscard]] entity::card::Registry
issueCreditCards(const sy::personas::Pack &personas,
                 const synth::people::Pack &people, std::uint64_t topLevelSeed,
                 const sy::cards::IssuanceRules &issuance, int startYear) {
  return sy::cards::issue(sy::cards::deriveCardSeed(topLevelSeed),
                          personas.table, people.roster.count, issuance,
                          startYear);
}

[[nodiscard]] entity::counterparty::Directory
buildCounterparties(pl::random::Rng &rng, std::int32_t population,
                    const sy::counterparties::CounterpartyTargets &targets) {
  pl::primitives::validate::nonNegative("population", population);
  pl::primitives::validate::require(targets);
  return sy::counterparties::make(rng, population, targets);
}

namespace {

namespace cps_tax = ::PhantomLedger::counterparties;
namespace family_synth = pl::synth::family;
namespace family_rt = pl::transfers::legit::routines::family;
namespace legit_ldg = pl::transfers::legit::ledger;

using Key = entity::Key;
using AccountsPack = sy::accounts::Pack;

template <std::ranges::range Records, typename Projection>
[[nodiscard]] std::vector<Key> keysFromRecords(const Records &records,
                                               Projection proj) {
  return records | std::views::transform(proj) |
         std::ranges::to<std::vector<Key>>();
}

void registerExternal(AccountsPack &accounts, std::span<const Key> keys) {
  sy::accounts::addAccounts(accounts, keys, /*external=*/true);
}

void registerInternal(AccountsPack &accounts, std::span<const Key> keys) {
  sy::accounts::addAccounts(accounts, keys, /*external=*/false);
}

void registerSystemAccounts(AccountsPack &accounts) {
  const auto keys = std::to_array<Key>({
      legit_ldg::bankFeeCollectionKey(),
      legit_ldg::bankOdLocKey(),
      entity::makeKey(entity::Role::merchant, entity::Bank::external, 1ULL),
  });
  registerExternal(accounts, keys);
}

void registerTaxonomyCounterparties(AccountsPack &accounts) {
  registerExternal(accounts, std::span<const Key>{cps_tax::kAll});
}

void registerMerchants(AccountsPack &accounts,
                       const entity::merchant::Catalog &merchants) {
  const auto keys = keysFromRecords(
      merchants.records, [](const auto &rec) { return rec.counterpartyId; });
  registerExternal(accounts, keys);
}

void registerLandlords(AccountsPack &accounts,
                       const sy::landlords::Pack &landlords) {
  const auto keys = keysFromRecords(
      landlords.roster.records, [](const auto &rec) { return rec.accountId; });
  registerExternal(accounts, keys);
}

void registerCounterpartyDirectory(AccountsPack &accounts,
                                   const entity::counterparty::Directory &cps) {
  registerExternal(accounts, std::span<const Key>{cps.employers.accounts.all});
  registerExternal(accounts, std::span<const Key>{cps.clients.accounts.all});
  registerExternal(accounts, std::span<const Key>{cps.external.platforms});
  registerExternal(accounts, std::span<const Key>{cps.external.processors});
  registerExternal(accounts,
                   std::span<const Key>{cps.external.ownerBusinesses});
  registerExternal(accounts, std::span<const Key>{cps.external.brokerages});
}

void registerCreditCards(AccountsPack &accounts,
                         const entity::card::Registry &cards) {
  const auto keys =
      keysFromRecords(cards.records, [](const auto &rec) { return rec.key; });
  registerInternal(accounts, keys);
}

/* Per-person external payees: family members, friends, ad-hoc P2P targets. */
void registerPerPersonPayees(AccountsPack &accounts,
                             const entity::person::Roster &people) {
  constexpr double externalP = family_rt::kDefaultCounterpartyRouting.externalP;
  const auto perPerson =
      family_synth::plan(people, accounts.ownership, externalP);
  const auto keys =
      perPerson | std::views::join | std::ranges::to<std::vector<Key>>();
  registerExternal(accounts, keys);
}

} // namespace

void finalizeAccountRegistry(pl::pipeline::Holdings &holdings,
                             const pl::pipeline::Counterparties &cpsData,
                             const pl::pipeline::People &peopleData) {
  auto &accounts = holdings.accounts;

  registerSystemAccounts(accounts);
  registerTaxonomyCounterparties(accounts);
  registerMerchants(accounts, cpsData.merchants);
  registerLandlords(accounts, cpsData.landlords);
  registerCounterpartyDirectory(accounts, cpsData.counterparties);
  registerCreditCards(accounts, holdings.creditCards);
  registerPerPersonPayees(accounts, peopleData.roster.roster);
}

void synthesizeBusinessOwners(pl::pipeline::Holdings &holdings,
                              const pl::pipeline::People &peopleData,
                              pl::random::Rng &rng,
                              const sy::accounts::BusinessOwnerPlan &plan) {
  sy::accounts::assignBusinessOwners(holdings.accounts,
                                     peopleData.roster.roster, rng, plan);
}

/* Runs AFTER business owners exist, because the proprietor cohort IS the
 * business-owner cohort — "the owner banks their business here" is what makes
 * the institution able to hold a beneficial-owner record at all.
 *
 * DRAW-FREE: it takes no Rng. The cohort is READ back out of the account
 * registry rather than threaded from `assignBusinessOwners`, so this consumes
 * nothing and moves no corpus byte. Sorted and deduplicated because
 * `ownership::ownerFor` indexes positionally, and an unordered cohort would
 * make the merchant->owner mapping depend on registry iteration order instead
 * of on world state. */
void assignMerchantOwners(pl::entity::merchant::Catalog &merchants,
                          const pl::entity::account::Registry &accounts,
                          double coverage) {
  namespace ownership = pl::entity::merchant::ownership;

  std::vector<pl::entity::PersonId> owners;
  owners.reserve(accounts.records.size() / 8U + 1U);
  for (const auto &record : accounts.records) {
    if (record.id.role == pl::entity::Role::business &&
        record.owner != pl::entity::invalidPerson) {
      owners.push_back(record.owner);
    }
  }
  std::sort(owners.begin(), owners.end());
  owners.erase(std::unique(owners.begin(), owners.end()), owners.end());

  const std::span<const pl::entity::PersonId> cohort(owners.data(),
                                                     owners.size());
  for (auto &record : merchants.records) {
    record.owner = ownership::ownerFor(record.counterpartyId, cohort, coverage);
  }
}

} // namespace PhantomLedger::pipeline::stages::entities
