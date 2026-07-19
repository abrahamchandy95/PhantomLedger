#pragma once

#include "phantomledger/diagnostics/logger.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/infra.hpp"

#include <cstddef>
#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

// Per-pack resident-byte estimates for the built world — the
// measurement side of RAM R2 (derive-don't-store, see
// docs/ram_derive_dont_store.md): before any pack is made regenerable,
// this report shows which packs actually dominate residency at a given
// scale. Estimates are exact for vector storage (capacity x element
// size) and approximate for hash maps (node and bucket overheads vary
// by standard library); the Router's internals are private, so its
// line is derived from the inputs it was built from. Silent unless the
// `mem` topic is enabled (make run-mem).

namespace PhantomLedger::pipeline::diagnostics {

namespace footprint {

// Separately-allocated hash node carries at least a next pointer and a
// cached hash on both libc++ and libstdc++.
inline constexpr std::size_t kHashNodeOverheadBytes = 16;

template <typename T>
[[nodiscard]] inline std::size_t vectorBytes(const std::vector<T> &v) noexcept {
  return v.capacity() * sizeof(T);
}

template <typename Map>
[[nodiscard]] inline std::size_t hashMapBytes(const Map &m) noexcept {
  return m.size() *
             (sizeof(typename Map::value_type) + kHashNodeOverheadBytes) +
         m.bucket_count() * sizeof(void *);
}

// Ledgers that expose only size(): assume load factor ~1.
[[nodiscard]] constexpr std::size_t
hashMapBytesFromSize(std::size_t entries, std::size_t valueBytes) noexcept {
  return entries * (valueBytes + kHashNodeOverheadBytes + sizeof(void *));
}

template <typename K, typename T>
[[nodiscard]] inline std::size_t
personPoolBytes(const std::unordered_map<K, std::vector<T>> &pool) noexcept {
  std::size_t heap = 0;
  for (const auto &[key, items] : pool) {
    heap += vectorBytes(items);
  }
  return hashMapBytes(pool) + heap;
}

[[nodiscard]] inline std::size_t
rosterBytes(const synth::people::Pack &pack) noexcept {
  const auto &t = pack.topology;
  return vectorBytes(pack.roster.flags) + vectorBytes(t.memberStore) +
         vectorBytes(t.fraudStore) + vectorBytes(t.muleStore) +
         vectorBytes(t.victimStore) + vectorBytes(t.rings);
}

[[nodiscard]] inline std::size_t
personasBytes(const synth::personas::Pack &pack) noexcept {
  return vectorBytes(pack.assignment.byPerson) + vectorBytes(pack.table.byPerson);
}

[[nodiscard]] inline std::size_t
accountsBytes(const synth::accounts::Pack &pack) noexcept {
  return vectorBytes(pack.registry.records) + hashMapBytes(pack.lookup.byId) +
         vectorBytes(pack.ownership.byPersonOffset) +
         vectorBytes(pack.ownership.byPersonIndex);
}

[[nodiscard]] inline std::size_t
cardsBytes(const entity::card::Registry &reg) noexcept {
  return vectorBytes(reg.records) + hashMapBytes(reg.byKey) +
         vectorBytes(reg.byPerson);
}

// Insurance and loan TERMS: compact per-holder maps.
[[nodiscard]] inline std::size_t
portfolioTermsBytes(const entity::product::PortfolioRegistry &reg) noexcept {
  using InsuranceEntry =
      std::pair<const entity::PersonId, entity::product::InsuranceHoldings>;
  using LoanEntry = std::pair<const entity::product::detail::PersonProductKey,
                              entity::product::InstallmentTerms>;
  return hashMapBytesFromSize(reg.insurance().size(), sizeof(InsuranceEntry)) +
         hashMapBytesFromSize(reg.loans().size(), sizeof(LoanEntry));
}

// The precomputed whole-window obligation schedule: the one pack that
// scales with population x window months, not population alone.
[[nodiscard]] inline std::size_t
obligationStreamBytes(const entity::product::PortfolioRegistry &reg) noexcept {
  return reg.obligations().size() * sizeof(entity::product::ObligationEvent);
}

[[nodiscard]] inline std::size_t
landlordsBytes(const synth::landlords::Pack &pack) noexcept {
  std::size_t index = 0;
  for (const auto &cls : pack.index.byClass) {
    index += vectorBytes(cls);
  }
  return vectorBytes(pack.roster.records) + index +
         vectorBytes(pack.internals) + vectorBytes(pack.externals);
}

[[nodiscard]] inline std::size_t
directoryBytes(const entity::counterparty::Directory &d) noexcept {
  const auto splitBytes = [](const entity::counterparty::BankSplit &s) {
    return vectorBytes(s.internal) + vectorBytes(s.external) +
           vectorBytes(s.all);
  };
  return splitBytes(d.employers.accounts) + splitBytes(d.clients.accounts) +
         vectorBytes(d.external.platforms) + vectorBytes(d.external.processors) +
         vectorBytes(d.external.ownerBusinesses) +
         vectorBytes(d.external.brokerages);
}

[[nodiscard]] inline std::size_t
devicesBytes(const synth::infra::devices::Output &out) noexcept {
  return vectorBytes(out.records) + vectorBytes(out.usages) +
         personPoolBytes(out.byPerson) + hashMapBytes(out.ringMap);
}

[[nodiscard]] inline std::size_t
ipsBytes(const synth::infra::ips::Output &out) noexcept {
  return vectorBytes(out.records) + vectorBytes(out.usages) +
         personPoolBytes(out.byPerson) + hashMapBytes(out.ringMap);
}

[[nodiscard]] inline std::size_t ringPlansBytes(
    const std::unordered_map<std::uint32_t, synth::infra::RingPlan> &plans)
    noexcept {
  std::size_t nested = 0;
  for (const auto &[ringId, plan] : plans) {
    nested +=
        vectorBytes(plan.sharedDeviceMembers) + vectorBytes(plan.sharedIpMembers);
  }
  return hashMapBytes(plans) + nested;
}

// The Router owns copies of the per-person device/IP pools, an
// account->owner map, and two sticky-index vectors — all private, so
// this reconstructs the estimate from the same inputs the build used.
[[nodiscard]] inline std::size_t
routerApproxBytes(std::size_t accountCount, std::size_t personCount,
                  const synth::infra::devices::Output &devices,
                  const synth::infra::ips::Output &ips) noexcept {
  using OwnerEntry = std::pair<const entity::Key, entity::PersonId>;
  return hashMapBytesFromSize(accountCount, sizeof(OwnerEntry)) +
         personPoolBytes(devices.byPerson) + personPoolBytes(ips.byPerson) +
         2 * personCount * sizeof(std::size_t);
}

} // namespace footprint

// One report, printed after the world build: which packs hold the
// resident bytes. `worldTotal` is the derive-don't-store target; the
// posted corpus never appears here because it streams.
inline void logWorldFootprint(const People &people, const Holdings &holdings,
                              const Counterparties &cps, const Infra &infra) {
  namespace logging = ::PhantomLedger::diagnostics;
  if (!logging::Logger::instance().enabled(logging::Level::info,
                                           logging::Topic::mem)) {
    return;
  }

  namespace fp = footprint;

  struct Item {
    const char *name;
    std::size_t bytes;
  };

  const Item items[] = {
      {"people.roster", fp::rosterBytes(people.roster)},
      {"people.pii", fp::vectorBytes(people.pii.records)},
      {"people.personas", fp::personasBytes(people.personas)},
      {"holdings.accounts", fp::accountsBytes(holdings.accounts)},
      {"holdings.creditCards", fp::cardsBytes(holdings.creditCards)},
      {"portfolios.terms", fp::portfolioTermsBytes(holdings.portfolios)},
      {"portfolios.obligations",
       fp::obligationStreamBytes(holdings.portfolios)},
      {"cps.merchants", fp::vectorBytes(cps.merchants.records)},
      {"cps.landlords", fp::landlordsBytes(cps.landlords)},
      {"cps.directory", fp::directoryBytes(cps.counterparties)},
      {"infra.devices", fp::devicesBytes(infra.devices)},
      {"infra.ips", fp::ipsBytes(infra.ips)},
      {"infra.ringPlans", fp::ringPlansBytes(infra.ringPlans)},
      {"infra.router~", fp::routerApproxBytes(
                            holdings.accounts.registry.records.size(),
                            static_cast<std::size_t>(people.roster.roster.count),
                            infra.devices, infra.ips)},
  };

  const auto mb = [](std::size_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
  };

  std::size_t total = 0;
  for (const auto &item : items) {
    total += item.bytes;
  }
  const auto personCount =
      static_cast<std::size_t>(people.roster.roster.count);

  std::fprintf(stderr, "[mem] world footprint (estimated resident bytes; "
                       "hash overheads approximate, ~ = derived):\n");
  for (const auto &item : items) {
    std::fprintf(stderr, "[mem]   %-24s %10.2f MB\n", item.name,
                 mb(item.bytes));
  }
  std::fprintf(stderr,
               "[mem]   %-24s %10.2f MB  (~%.0f B/person at %zu people)\n",
               "worldTotal", mb(total),
               personCount == 0
                   ? 0.0
                   : static_cast<double>(total) /
                         static_cast<double>(personCount),
               personCount);
}

} // namespace PhantomLedger::pipeline::diagnostics
