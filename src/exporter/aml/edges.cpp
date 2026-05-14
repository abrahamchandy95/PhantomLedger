#include "phantomledger/exporter/aml/edges.hpp"

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/aml/identity.hpp"
#include "phantomledger/exporter/aml/minhash.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/common/support.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"

#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace PhantomLedger::exporter::aml::edges {

namespace {

namespace ent = ::PhantomLedger::entity;
namespace tx_ns = ::PhantomLedger::transactions;
namespace t_ns = ::PhantomLedger::time;
namespace pipe = ::PhantomLedger::pipeline;
namespace pii_ns = ::PhantomLedger::entities::synth::pii;
namespace loc = ::PhantomLedger::locale;
namespace common = ::PhantomLedger::exporter::common;

[[nodiscard]] ::PhantomLedger::exporter::common::CustomerId
customerIdFor(ent::PersonId p) noexcept {
  return ::PhantomLedger::exporter::common::renderCustomerId(p);
}

[[nodiscard]] const pii_ns::PoolSet &
poolsFor(const vertices::SharedContext &ctx) noexcept {
  assert(ctx.pools != nullptr &&
         "SharedContext::pools is null — was the context built with "
         "buildSharedContext(entities, txns, pools)?");
  return *ctx.pools;
}

[[nodiscard]] const pii_ns::LocalePool &
usPoolFor(const vertices::SharedContext &ctx) noexcept {
  return poolsFor(ctx).forCountry(loc::Country::us);
}

} // namespace

TransactionEdgeBundle
classifyTransactionEdges(const pipe::Entities &entities,
                         std::span<const tx_ns::Transaction> finalTxns,
                         const vertices::SharedContext &ctx) {
  TransactionEdgeBundle out;
  const auto &usPool = usPoolFor(ctx);

  std::unordered_map<ent::Key, std::string> cpNames;

  out.sendRows.reserve(finalTxns.size());
  out.receiveRows.reserve(finalTxns.size());

  const auto cpNameFor = [&](const ent::Key &k) -> const std::string & {
    auto it = cpNames.find(k);
    if (it == cpNames.end()) {
      auto name = std::string{
          identity::nameForCounterparty(common::renderKey(k), usPool)
              .firstName};
      it = cpNames.emplace(k, std::move(name)).first;
    }
    return it->second;
  };

  std::size_t idx = 1;
  for (const auto &tx : finalTxns) {
    const bool srcExt = common::isExternalKey(tx.source);
    const bool dstExt = common::isExternalKey(tx.target);

    if (srcExt) {
      out.cpSendRows.emplace_back(tx.source, idx, cpNameFor(tx.source));
      out.cpSenders.insert(tx.source);
      if (!dstExt) {
        out.receivedFromCpPairs.emplace(tx.target, tx.source);
      }
    } else {
      out.sendRows.emplace_back(tx.source, idx);
      if (dstExt) {
        out.sentToCpPairs.emplace(tx.source, tx.target);
      }
    }

    if (dstExt) {
      out.cpReceiveRows.emplace_back(tx.target, idx, cpNameFor(tx.target));
      out.cpReceivers.insert(tx.target);
    } else {
      out.receiveRows.emplace_back(tx.target, idx);
    }

    ++idx;
  }

  (void)entities;
  return out;
}

MinhashVertexSets collectMinhashVertexSets(const pipe::Entities &entities,
                                           const vertices::SharedContext &ctx) {
  MinhashVertexSets out;
  const auto &pools = poolsFor(ctx);
  const auto &usPool = pools.forCountry(loc::Country::us);
  const auto &pii = entities.pii;

  std::string addrScratch;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p, pii, pools);
    const auto addr = identity::addressForPerson(p, pii, pools);

    for (auto &id : minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      out.name.insert(std::move(id));
    }

    const auto fullAddr = common::joinAddress(addrScratch, addr);
    for (auto &id : minhash::addressMinhashIds(fullAddr)) {
      out.address.insert(std::move(id));
    }

    for (auto &id : minhash::streetMinhashIds(addr.streetLine1)) {
      out.street.insert(std::move(id));
    }
    out.city.insert(minhash::cityMinhashId(addr.city));
    out.state.insert(minhash::stateMinhashId(addr.state));
  }

  for (const auto &cpId : ctx.counterpartyIds) {
    const auto nm = identity::nameForCounterparty(cpId, usPool);
    for (auto &id : minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      out.name.insert(std::move(id));
    }
  }
  return out;
}

void writeCustomerHasAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                                 const pipe::Entities &entities) {
  common::forEachInternalOwnership(
      entities, [&](ent::PersonId pid, const ent::Key &k) {
        w.writeRow(customerIdFor(pid), common::renderKey(k));
      });
}

void writeAccountHasPrimaryCustomerRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pipe::Entities &entities) {
  common::forEachInternalOwnership(
      entities, [&](ent::PersonId pid, const ent::Key &k) {
        w.writeRow(common::renderKey(k), customerIdFor(pid));
      });
}

void writeAcctTxnRows(::PhantomLedger::exporter::csv::Writer &w,
                      std::span<const TransactionEdgeBundle::AcctTxnRow> rows) {
  for (const auto &[acct, idx] : rows) {
    w.writeRow(common::renderKey(acct), transactionId(idx));
  }
}

void writeCpTxnRows(::PhantomLedger::exporter::csv::Writer &w,
                    std::span<const TransactionEdgeBundle::CpTxnRow> rows) {
  for (const auto &[cp, idx, name] : rows) {
    w.writeRow(common::renderKey(cp), transactionId(idx), name);
  }
}

void writeAcctCpPairRows(::PhantomLedger::exporter::csv::Writer &w,
                         const std::set<std::pair<ent::Key, ent::Key>> &pairs) {
  for (const auto &[acct, cp] : pairs) {
    w.writeRow(common::renderKey(acct), common::renderKey(cp));
  }
}

void writeUsesDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices) {
  std::map<std::pair<ent::PersonId, std::string>, common::SeenWindow> agg;

  for (const auto &usage : devices.usages) {
    const std::string idStr{
        ::PhantomLedger::exporter::common::renderDeviceId(usage.deviceId)
            .view()};
    common::recordSeen(agg[{usage.personId, idStr}], usage.firstSeen,
                       usage.lastSeen);
  }

  for (const auto &[key, slot] : agg) {
    w.writeRow(customerIdFor(key.first), key.second,
               t_ns::formatTimestamp(slot.firstSeen),
               t_ns::formatTimestamp(slot.lastSeen), slot.count);
  }
}

void writeLoggedFromRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pipe::Entities &entities,
    const ::PhantomLedger::infra::synth::devices::Output &devices) {
  std::map<std::pair<ent::Key, std::string>, common::SeenWindow> agg;

  std::unordered_map<ent::PersonId, std::vector<ent::Key>> accountsByPerson;
  common::forEachInternalOwnership(entities,
                                   [&](ent::PersonId pid, const ent::Key &k) {
                                     accountsByPerson[pid].push_back(k);
                                   });

  for (const auto &usage : devices.usages) {
    const auto it = accountsByPerson.find(usage.personId);
    if (it == accountsByPerson.end()) {
      continue;
    }
    const std::string idStr{
        ::PhantomLedger::exporter::common::renderDeviceId(usage.deviceId)
            .view()};
    for (const auto &acctKey : it->second) {
      common::recordSeen(agg[{acctKey, idStr}], usage.firstSeen,
                         usage.lastSeen);
    }
  }

  for (const auto &[key, slot] : agg) {
    w.writeRow(common::renderKey(key.first), key.second,
               t_ns::formatTimestamp(slot.firstSeen),
               t_ns::formatTimestamp(slot.lastSeen), slot.count);
  }
}

void writeCustomerHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                              const pipe::Entities &entities,
                              t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    w.writeRow(customerIdFor(p), identity::nameIdForPerson(p), ts);
  }
}

void writeCustomerHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                                 const pipe::Entities &entities,
                                 t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    w.writeRow(customerIdFor(p), identity::addressIdForPerson(p), ts);
  }
}

void writeCustomerAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pipe::Entities &entities,
    t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    w.writeRow(customerIdFor(p), loc::code(entities.pii.at(p).country), ts);
  }
}

void writeAccountHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                             const pipe::Entities &entities,
                             t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  common::forEachInternalOwnership(
      entities, [&](ent::PersonId pid, const ent::Key &k) {
        w.writeRow(common::renderKey(k), identity::nameIdForPerson(pid),
                   std::string_view{"primary"}, ts);
      });
}

void writeAccountHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                                const pipe::Entities &entities,
                                t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  common::forEachInternalOwnership(
      entities, [&](ent::PersonId pid, const ent::Key &k) {
        w.writeRow(common::renderKey(k), identity::addressIdForPerson(pid),
                   std::string_view{"mailing"}, ts);
      });
}

void writeAccountAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pipe::Entities &entities,
    t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      continue;
    }
    const auto countryCode =
        (rec.owner == ent::invalidPerson)
            ? common::kUsCountry
            : loc::code(entities.pii.at(rec.owner).country);
    w.writeRow(common::renderKey(rec.id), countryCode, ts);
  }
}

void writeAddressInCountryRows(::PhantomLedger::exporter::csv::Writer &w,
                               const pipe::Entities &entities,
                               const vertices::SharedContext &ctx,
                               t_ns::TimePoint simStart) {

  const auto ts = t_ns::formatTimestamp(simStart);
  std::unordered_set<std::string> emitted;
  emitted.reserve(common::estimateIdentityRowCount(entities, ctx));

  const auto emit = [&](identity::StackString<32> id,
                        std::string_view countryCode) {
    if (emitted.emplace(id.view()).second) {
      w.writeRow(id, countryCode, ts);
    }
  };

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    emit(identity::addressIdForPerson(p),
         loc::code(entities.pii.at(p).country));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emit(identity::addressIdForCounterparty(cpId), common::kUsCountry);
  }
  for (const auto &bankId : ctx.bankIds) {
    emit(identity::addressIdForBank(bankId), common::kUsCountry);
  }
}

void writeCounterpartyHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                                  const vertices::SharedContext &ctx,
                                  t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &cpId : ctx.counterpartyIds) {
    w.writeRow(cpId, identity::nameIdForCounterparty(cpId), ts);
  }
}

void writeCounterpartyHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                                     const vertices::SharedContext &ctx,
                                     t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &cpId : ctx.counterpartyIds) {
    w.writeRow(cpId, identity::addressIdForCounterparty(cpId), ts);
  }
}

void writeCounterpartyAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const vertices::SharedContext &ctx) {
  for (const auto &cpId : ctx.counterpartyIds) {
    w.writeRow(cpId, common::kUsCountry);
  }
}

void writeCustomerMatchesWatchlistRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pipe::Entities &entities) {
  const auto &roster = entities.people.roster;
  std::string watchlistId;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    if (!roster.has(p, ent::person::Flag::fraud) &&
        !roster.has(p, ent::person::Flag::mule)) {
      continue;
    }
    const auto cid = customerIdFor(p);
    const auto wlView = common::makeWatchlistId(watchlistId, cid.view());

    w.writeRow(cid, wlView);
  }
}

void writeReferencesRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars) {
  for (const auto &sar : sars) {
    const auto n = sar.subjectPersonIds.size();
    for (std::size_t i = 0; i < n; ++i) {
      w.writeRow(sar.sarId, customerIdFor(sar.subjectPersonIds[i]),
                 sar.subjectRoles[i]);
    }
  }
}

void writeSarCoversRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars) {
  for (const auto &sar : sars) {
    const auto n = sar.coveredAccountIds.size();
    for (std::size_t i = 0; i < n; ++i) {
      w.writeRow(sar.sarId, common::renderKey(sar.coveredAccountIds[i]),
                 sar.coveredAmounts[i]);
    }
  }
}

void writeBeneficiaryBankRows(::PhantomLedger::exporter::csv::Writer &w,
                              const std::set<ent::Key> &cpReceivers) {
  for (const auto &cpKey : cpReceivers) {
    const auto rendered = common::renderKey(cpKey);
    w.writeRow(counterpartyBankId(rendered), rendered);
  }
}

void writeOriginatorBankRows(::PhantomLedger::exporter::csv::Writer &w,
                             const std::set<ent::Key> &cpSenders) {
  for (const auto &cpKey : cpSenders) {
    const auto rendered = common::renderKey(cpKey);
    w.writeRow(counterpartyBankId(rendered), rendered);
  }
}

void writeBankAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const vertices::SharedContext &ctx) {
  for (const auto &bankId : ctx.bankIds) {
    w.writeRow(bankId, common::kUsCountry);
  }
}

void writeBankHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                             const vertices::SharedContext &ctx,
                             t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &bankId : ctx.bankIds) {
    w.writeRow(bankId, identity::addressIdForBank(bankId), ts);
  }
}

void writeBankHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                          const vertices::SharedContext &ctx,
                          t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &bankId : ctx.bankIds) {
    w.writeRow(bankId, identity::nameIdForBank(bankId), ts);
  }
}

// ──────────────────────────────────────────────────────────────────────
// Minhash-shingle edges (Camp 2) — need name/address content
// ──────────────────────────────────────────────────────────────────────

void writeCustomerHasNameMinhashRows(::PhantomLedger::exporter::csv::Writer &w,
                                     const pipe::Entities &entities,
                                     const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = entities.pii;

  std::string nameScratch;
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p, pii, pools);
    const auto cid = customerIdFor(p);
    const auto nameStr =
        common::joinName(nameScratch, nm.firstName, nm.lastName);
    for (const auto &mhId :
         minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      w.writeRow(cid, mhId, nameStr);
    }
  }
}

void writeCustomerHasAddressMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pipe::Entities &entities,
    const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = entities.pii;

  std::string addrScratch;
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p, pii, pools);
    const auto cid = customerIdFor(p);
    const auto fullAddr = common::joinAddress(addrScratch, addr);
    for (const auto &mhId : minhash::addressMinhashIds(fullAddr)) {
      w.writeRow(cid, mhId, fullAddr);
    }
  }
}

void writeCustomerHasAddressStreetLine1MinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pipe::Entities &entities,
    const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = entities.pii;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p, pii, pools);
    const auto cid = customerIdFor(p);
    for (const auto &mhId : minhash::streetMinhashIds(addr.streetLine1)) {
      w.writeRow(cid, mhId, addr.streetLine1);
    }
  }
}

void writeCustomerHasAddressCityMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pipe::Entities &entities,
    const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = entities.pii;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p, pii, pools);
    w.writeRow(customerIdFor(p), minhash::cityMinhashId(addr.city), addr.city);
  }
}

void writeCustomerHasAddressStateMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pipe::Entities &entities,
    const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = entities.pii;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p, pii, pools);
    w.writeRow(customerIdFor(p), minhash::stateMinhashId(addr.state),
               addr.state);
  }
}

void writeAccountHasNameMinhashRows(::PhantomLedger::exporter::csv::Writer &w,
                                    const pipe::Entities &entities,
                                    const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = entities.pii;

  struct PerPersonNameMh {
    std::vector<minhash::BucketId> ids;
    std::string name;
  };
  std::unordered_map<ent::PersonId, PerPersonNameMh> mhByPerson;
  mhByPerson.reserve(entities.people.roster.count);
  std::string nameScratch;
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p, pii, pools);
    auto &slot = mhByPerson[p];
    slot.ids = minhash::nameMinhashIds(nm.firstName, nm.lastName);
    slot.name.assign(common::joinName(nameScratch, nm.firstName, nm.lastName));
  }

  constexpr std::string_view kAccountNameTypeOf{"primary"};

  common::forEachInternalOwnership(
      entities, [&](ent::PersonId pid, const ent::Key &k) {
        const auto it = mhByPerson.find(pid);
        if (it == mhByPerson.end()) {
          return;
        }
        const auto acctStr = common::renderKey(k);
        for (const auto &mhId : it->second.ids) {
          w.writeRow(acctStr, mhId, kAccountNameTypeOf, it->second.name);
        }
      });
}

void writeCounterpartyHasNameMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const vertices::SharedContext &ctx) {
  const auto &usPool = usPoolFor(ctx);

  std::string nameScratch;
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto nm = identity::nameForCounterparty(cpId, usPool);
    const auto nameStr =
        common::joinName(nameScratch, nm.firstName, nm.lastName);
    for (const auto &mhId :
         minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      w.writeRow(cpId, mhId, nameStr);
    }
  }
}

void writeResolvesToRows(::PhantomLedger::exporter::csv::Writer &w,
                         const pipe::Entities &entities,
                         t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) == 0) {
      continue;
    }
    if (rec.owner == ent::invalidPerson) {
      continue;
    }
    w.writeRow(common::renderKey(rec.id), customerIdFor(rec.owner), 0.95,
               std::string_view{"account_link"}, ts);
  }
}

} // namespace PhantomLedger::exporter::aml::edges
