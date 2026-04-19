#pragma once

#include "phantomledger/entities/counterparties/pool.hpp"
#include "phantomledger/entities/identifier/bank.hpp"
#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/identifier/role.hpp"
#include "phantomledger/entities/synth/counterparties/config.hpp"
#include "phantomledger/entities/synth/counterparties/scale.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::synth::counterparties {
namespace detail {

inline void append(std::vector<identifier::Key> &out, identifier::Role role,
                   int count) {
  out.reserve(out.size() + static_cast<std::size_t>(count));
  for (int i = 1; i <= count; ++i) {
    out.push_back(identifier::make(role, identifier::Bank::external,
                                   static_cast<std::uint64_t>(i)));
  }
}

} // namespace detail

[[nodiscard]] inline entities::counterparties::Pool
makePool(int population, const Config &cfg = {}) {
  entities::counterparties::Pool out;

  detail::append(out.employerIds, identifier::Role::employer,
                 scale(cfg.perTenK.employers, population, cfg.floor.employers));
  detail::append(
      out.clientPayerIds, identifier::Role::client,
      scale(cfg.perTenK.clientPayers, population, cfg.floor.clientPayers));
  detail::append(out.platformIds, identifier::Role::platform,
                 scale(cfg.perTenK.platforms, population, cfg.floor.platforms));
  detail::append(
      out.processorIds, identifier::Role::processor,
      scale(cfg.perTenK.processors, population, cfg.floor.processors));
  detail::append(out.ownerBusinessIds, identifier::Role::business,
                 scale(cfg.perTenK.ownerBusinesses, population,
                       cfg.floor.ownerBusinesses));
  detail::append(
      out.brokerageIds, identifier::Role::brokerage,
      scale(cfg.perTenK.brokerages, population, cfg.floor.brokerages));

  out.allExternals.reserve(out.employerIds.size() + out.clientPayerIds.size() +
                           out.platformIds.size() + out.processorIds.size() +
                           out.ownerBusinessIds.size() +
                           out.brokerageIds.size());

  out.allExternals.insert(out.allExternals.end(), out.employerIds.begin(),
                          out.employerIds.end());
  out.allExternals.insert(out.allExternals.end(), out.clientPayerIds.begin(),
                          out.clientPayerIds.end());
  out.allExternals.insert(out.allExternals.end(), out.platformIds.begin(),
                          out.platformIds.end());
  out.allExternals.insert(out.allExternals.end(), out.processorIds.begin(),
                          out.processorIds.end());
  out.allExternals.insert(out.allExternals.end(), out.ownerBusinessIds.begin(),
                          out.ownerBusinessIds.end());
  out.allExternals.insert(out.allExternals.end(), out.brokerageIds.begin(),
                          out.brokerageIds.end());

  return out;
}

} // namespace PhantomLedger::entities::synth::counterparties
