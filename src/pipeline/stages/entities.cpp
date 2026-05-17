#include "phantomledger/pipeline/stages/entities.hpp"

#include "phantomledger/entities/synth/family/pick.hpp"
#include "phantomledger/entities/synth/people/make.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/accounts/assign.hpp"
#include "phantomledger/synth/accounts/business_owners.hpp"
#include "phantomledger/synth/accounts/make.hpp"
#include "phantomledger/synth/cards/issue.hpp"
#include "phantomledger/synth/cards/seeds.hpp"
#include "phantomledger/synth/counterparties/make.hpp"
#include "phantomledger/synth/landlords/make.hpp"
#include "phantomledger/synth/merchants/make.hpp"
#include "phantomledger/synth/personas/make.hpp"
#include "phantomledger/synth/pii/make.hpp"
#include "phantomledger/taxonomies/counterparties/accounts.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

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

[[nodiscard]] synth::people::Pack
buildPeople(pl::random::Rng &rng, std::int32_t population,
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
              const sy::personas::Mix &mix) {
  const std::uint64_t personasSeed = rng.nextU64();
  return sy::personas::makePack(rng, people.roster.count, personasSeed, mix);
}

[[nodiscard]] entity::pii::Roster buildPii(pl::random::Rng &rng,
                                           const sy::personas::Pack &personas,
                                           sy::pii::IdentityContext identity) {
  assert(identity.pools != nullptr &&
         "buildPii: IdentityContext::pools must be set. main is the sole "
         "owner of the PoolSet pointer.");
  return sy::pii::make(rng, personas.assignment, identity);
}

[[nodiscard]] entity::merchant::Catalog
buildMerchants(pl::random::Rng &rng, std::int32_t population,
               const sy::merchants::GenerationPlan &plan) {
  pl::primitives::validate::nonNegative("population", population);
  pl::primitives::validate::require(plan);
  return sy::merchants::makeCatalog(rng, population, plan);
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
                 const sy::cards::IssuanceRules &issuance) {
  return sy::cards::issue(sy::cards::deriveCardSeed(topLevelSeed),
                          personas.table, people.roster.count, issuance);
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
namespace family_synth = pl::entities::synth::family;
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

/// Per-person external payees: family members, friends, ad-hoc P2P targets.
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

} // namespace PhantomLedger::pipeline::stages::entities
