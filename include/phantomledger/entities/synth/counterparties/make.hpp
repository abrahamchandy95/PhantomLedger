#pragma once

#include "phantomledger/entities/counterparties/pool.hpp"
#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/synth/counterparties/config.hpp"
#include "phantomledger/entities/synth/counterparties/scale.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::synth::counterparties {

using taxonomies::identifiers::Bank;
using taxonomies::identifiers::Role;
namespace detail {

/// Append `count` purely external IDs to `out`.
inline void appendExternal(std::vector<identifier::Key> &out, Role role,
                           int count) {
  out.reserve(out.size() + static_cast<std::size_t>(count));
  for (int i = 1; i <= count; ++i) {
    out.push_back(
        identifier::make(role, Bank::external, static_cast<std::uint64_t>(i)));
  }
}

/// Split a pool of `total` IDs into internal and external based on
/// `inBankP`. Uses separate serial counters for internal and external
/// to avoid ID collisions between the two prefix families.
///
/// Returns the number of internals generated.
inline int splitPool(random::Rng &rng, Role role, int total, double inBankP,
                     std::vector<identifier::Key> &internals,
                     std::vector<identifier::Key> &externals,
                     std::vector<identifier::Key> &combined) {
  internals.reserve(internals.size() + static_cast<std::size_t>(total));
  externals.reserve(externals.size() + static_cast<std::size_t>(total));
  combined.reserve(combined.size() + static_cast<std::size_t>(total));

  std::uint64_t intCounter = 0;
  std::uint64_t extCounter = 0;

  for (int i = 0; i < total; ++i) {
    identifier::Key id;
    if (rng.coin(inBankP)) {
      ++intCounter;
      id = identifier::make(role, Bank::internal, intCounter);
      internals.push_back(id);
    } else {
      ++extCounter;
      id = identifier::make(role, Bank::external, extCounter);
      externals.push_back(id);
    }
    combined.push_back(id);
  }

  return static_cast<int>(intCounter);
}

} // namespace detail

/// Build all counterparty pools scaled to population size.
///
/// Employers and clients are split into internal/external based on
/// `cfg.employerInBankP` and `cfg.clientInBankP`. All other pools
/// remain fully external (platforms, processors, businesses, and
/// brokerages use treasury/commercial banking, not retail).
[[nodiscard]] inline entities::counterparties::Pool
makePool(random::Rng &rng, int population, const Config &cfg = {}) {
  entities::counterparties::Pool out;

  // --- Employers: split by in_bank_p ---
  const int nEmployers =
      scale(cfg.perTenK.employers, population, cfg.floor.employers);
  detail::splitPool(rng, Role::employer, nEmployers, cfg.employerInBankP,
                    out.employerInternalIds, out.employerExternalIds,
                    out.employerIds);

  // --- Clients: split by in_bank_p ---
  const int nClients =
      scale(cfg.perTenK.clientPayers, population, cfg.floor.clientPayers);
  detail::splitPool(rng, Role::client, nClients, cfg.clientInBankP,
                    out.clientInternalIds, out.clientExternalIds,
                    out.clientPayerIds);

  // --- Fully external pools ---
  detail::appendExternal(
      out.platformIds, Role::platform,
      scale(cfg.perTenK.platforms, population, cfg.floor.platforms));
  detail::appendExternal(
      out.processorIds, Role::processor,
      scale(cfg.perTenK.processors, population, cfg.floor.processors));
  detail::appendExternal(out.ownerBusinessIds, Role::business,
                         scale(cfg.perTenK.ownerBusinesses, population,
                               cfg.floor.ownerBusinesses));
  detail::appendExternal(
      out.brokerageIds, Role::brokerage,
      scale(cfg.perTenK.brokerages, population, cfg.floor.brokerages));

  // --- Aggregate external and internal lists ---
  out.allExternals.reserve(
      out.employerExternalIds.size() + out.clientExternalIds.size() +
      out.platformIds.size() + out.processorIds.size() +
      out.ownerBusinessIds.size() + out.brokerageIds.size());

  out.allExternals.insert(out.allExternals.end(),
                          out.employerExternalIds.begin(),
                          out.employerExternalIds.end());
  out.allExternals.insert(out.allExternals.end(), out.clientExternalIds.begin(),
                          out.clientExternalIds.end());
  out.allExternals.insert(out.allExternals.end(), out.platformIds.begin(),
                          out.platformIds.end());
  out.allExternals.insert(out.allExternals.end(), out.processorIds.begin(),
                          out.processorIds.end());
  out.allExternals.insert(out.allExternals.end(), out.ownerBusinessIds.begin(),
                          out.ownerBusinessIds.end());
  out.allExternals.insert(out.allExternals.end(), out.brokerageIds.begin(),
                          out.brokerageIds.end());

  out.allInternals.reserve(out.employerInternalIds.size() +
                           out.clientInternalIds.size());
  out.allInternals.insert(out.allInternals.end(),
                          out.employerInternalIds.begin(),
                          out.employerInternalIds.end());
  out.allInternals.insert(out.allInternals.end(), out.clientInternalIds.begin(),
                          out.clientInternalIds.end());

  return out;
}

} // namespace PhantomLedger::entities::synth::counterparties
