#include "phantomledger/exporter/aml/edges.hpp"

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/exporter/aml/identity.hpp"
#include "phantomledger/exporter/aml/minhash.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/common/support.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"

#include <cassert>
#include <cstdio>
#include <map>
#include <ranges>
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
namespace exporter = ::PhantomLedger::exporter;
namespace synth = ::PhantomLedger::synth;

[[nodiscard]] const pii_ns::PoolSet &
poolsFor(const vertices::SharedContext &ctx) noexcept {
  assert(ctx.pools != nullptr &&
         "SharedContext::pools is null — was the context built via "
         "buildSharedContext(people, holdings, txns, pools)?");
  return *ctx.pools;
}

[[nodiscard]] auto allPersonIds(const pipe::People &people) {
  return std::views::iota(1u, people.roster.roster.count + 1);
}

[[nodiscard]] std::size_t
estimateRowCapacity(const pipe::People &people,
                    const vertices::SharedContext &ctx) noexcept {
  return static_cast<std::size_t>(people.roster.roster.count) +
         ctx.counterpartyIds.size() + 21U;
}

} // namespace

TransactionEdgeBundle
classifyTransactionEdges(std::span<const tx_ns::Transaction> finalTxns,
                         const vertices::SharedContext &ctx) {
  TransactionEdgeBundle out;
  const auto &usPool = poolsFor(ctx).forCountry(loc::Country::us);

  std::unordered_map<ent::Key, std::string> cpNames;

  out.sendRows.reserve(finalTxns.size());
  out.receiveRows.reserve(finalTxns.size());

  const auto cpNameFor = [&](const ent::Key &k) -> const std::string & {
    auto it = cpNames.find(k);
    if (it == cpNames.end()) {
      auto name = std::string{
          identity::nameForCounterparty(exporter::common::renderKey(k), usPool)
              .firstName};
      it = cpNames.emplace(k, std::move(name)).first;
    }
    return it->second;
  };

  std::size_t idx = 1;
  for (const auto &tx : finalTxns) {
    const bool srcExt = exporter::common::isExternalKey(tx.source);
    const bool dstExt = exporter::common::isExternalKey(tx.target);

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

  return out;
}

MinhashVertexSets collectMinhashVertexSets(const pipe::People &people,
                                           const vertices::SharedContext &ctx) {
  MinhashVertexSets out;
  const auto &pools = poolsFor(ctx);
  const auto &usPool = pools.forCountry(loc::Country::us);
  const auto &pii = people.pii;

  std::string addrScratch;

  for (ent::PersonId p : allPersonIds(people)) {
    const auto nm = identity::nameForPerson(p, pii, pools);
    const auto addr = identity::addressForPerson(p, pii, pools);

    for (auto &id : minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      out.name.insert(std::move(id));
    }

    const auto fullAddr = exporter::common::joinAddress(addrScratch, addr);
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

void writeCustomerHasAccountRows(exporter::csv::Writer &w,
                                 const pipe::Holdings &holdings) {
  exporter::common::forEachInternalOwnership(
      holdings, [&](ent::PersonId pid, const ent::Key &k) {
        w.writeRow(exporter::common::renderCustomerId(pid),
                   exporter::common::renderKey(k));
      });
}

void writeAccountHasPrimaryCustomerRows(exporter::csv::Writer &w,
                                        const pipe::Holdings &holdings) {
  exporter::common::forEachInternalOwnership(
      holdings, [&](ent::PersonId pid, const ent::Key &k) {
        w.writeRow(exporter::common::renderKey(k),
                   exporter::common::renderCustomerId(pid));
      });
}

void writeAcctTxnRows(exporter::csv::Writer &w,
                      std::span<const TransactionEdgeBundle::AcctTxnRow> rows) {
  for (const auto &[acct, idx] : rows) {
    w.writeRow(exporter::common::renderKey(acct), transactionId(idx));
  }
}

void writeCpTxnRows(exporter::csv::Writer &w,
                    std::span<const TransactionEdgeBundle::CpTxnRow> rows) {
  for (const auto &[cp, idx, name] : rows) {
    w.writeRow(exporter::common::renderKey(cp), transactionId(idx), name);
  }
}

void writeAcctCpPairRows(exporter::csv::Writer &w,
                         const std::set<std::pair<ent::Key, ent::Key>> &pairs) {
  for (const auto &[acct, cp] : pairs) {
    w.writeRow(exporter::common::renderKey(acct),
               exporter::common::renderKey(cp));
  }
}

void writeUsesDeviceRows(exporter::csv::Writer &w,
                         const synth::infra::devices::Output &devices) {
  std::map<std::pair<ent::PersonId, std::string>, exporter::common::SeenWindow>
      agg;

  for (const auto &usage : devices.usages) {
    const std::string idStr{
        exporter::common::renderDeviceId(usage.deviceId).view()};
    exporter::common::recordSeen(agg[{usage.personId, idStr}], usage.firstSeen,
                                 usage.lastSeen);
  }

  for (const auto &[key, slot] : agg) {
    w.writeRow(exporter::common::renderCustomerId(key.first), key.second,
               t_ns::formatTimestamp(slot.firstSeen),
               t_ns::formatTimestamp(slot.lastSeen), slot.count);
  }
}

void writeLoggedFromRows(exporter::csv::Writer &w,
                         const pipe::Holdings &holdings,
                         const synth::infra::devices::Output &devices) {
  std::map<std::pair<ent::Key, std::string>, exporter::common::SeenWindow> agg;

  std::unordered_map<ent::PersonId, std::vector<ent::Key>> accountsByPerson;
  exporter::common::forEachInternalOwnership(
      holdings, [&](ent::PersonId pid, const ent::Key &k) {
        accountsByPerson[pid].push_back(k);
      });

  for (const auto &usage : devices.usages) {
    const auto it = accountsByPerson.find(usage.personId);
    if (it == accountsByPerson.end()) {
      continue;
    }
    const std::string idStr{
        exporter::common::renderDeviceId(usage.deviceId).view()};
    for (const auto &acctKey : it->second) {
      exporter::common::recordSeen(agg[{acctKey, idStr}], usage.firstSeen,
                                   usage.lastSeen);
    }
  }

  for (const auto &[key, slot] : agg) {
    w.writeRow(exporter::common::renderKey(key.first), key.second,
               t_ns::formatTimestamp(slot.firstSeen),
               t_ns::formatTimestamp(slot.lastSeen), slot.count);
  }
}

void writeCustomerHasNameRows(exporter::csv::Writer &w,
                              const pipe::People &people,
                              t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p : allPersonIds(people)) {
    w.writeRow(exporter::common::renderCustomerId(p),
               identity::nameIdForPerson(p), ts);
  }
}

void writeCustomerHasAddressRows(exporter::csv::Writer &w,
                                 const pipe::People &people,
                                 t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p : allPersonIds(people)) {
    w.writeRow(exporter::common::renderCustomerId(p),
               identity::addressIdForPerson(p), ts);
  }
}

void writeCustomerAssociatedWithCountryRows(exporter::csv::Writer &w,
                                            const pipe::People &people,
                                            t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p : allPersonIds(people)) {
    w.writeRow(exporter::common::renderCustomerId(p),
               loc::code(people.pii.at(p).country), ts);
  }
}

void writeAccountHasNameRows(exporter::csv::Writer &w,
                             const pipe::Holdings &holdings,
                             t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  exporter::common::forEachInternalOwnership(holdings, [&](ent::PersonId pid,
                                                           const ent::Key &k) {
    w.writeRow(exporter::common::renderKey(k), identity::nameIdForPerson(pid),
               std::string_view{"primary"}, ts);
  });
}

void writeAccountHasAddressRows(exporter::csv::Writer &w,
                                const pipe::Holdings &holdings,
                                t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  exporter::common::forEachInternalOwnership(
      holdings, [&](ent::PersonId pid, const ent::Key &k) {
        w.writeRow(exporter::common::renderKey(k),
                   identity::addressIdForPerson(pid),
                   std::string_view{"mailing"}, ts);
      });
}

void writeAccountAssociatedWithCountryRows(exporter::csv::Writer &w,
                                           const pipe::People &people,
                                           const pipe::Holdings &holdings,
                                           t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &rec : holdings.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      continue;
    }
    const auto countryCode = (rec.owner == ent::invalidPerson)
                                 ? exporter::common::kUsCountry
                                 : loc::code(people.pii.at(rec.owner).country);
    w.writeRow(exporter::common::renderKey(rec.id), countryCode, ts);
  }
}

void writeAddressInCountryRows(exporter::csv::Writer &w,
                               const pipe::People &people,
                               const vertices::SharedContext &ctx,
                               t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  std::unordered_set<std::string> emitted;
  emitted.reserve(estimateRowCapacity(people, ctx));

  const auto emit = [&](identity::StackString<32> id,
                        std::string_view countryCode) {
    if (emitted.emplace(id.view()).second) {
      w.writeRow(id, countryCode, ts);
    }
  };

  for (ent::PersonId p : allPersonIds(people)) {
    emit(identity::addressIdForPerson(p), loc::code(people.pii.at(p).country));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emit(identity::addressIdForCounterparty(cpId),
         exporter::common::kUsCountry);
  }
  for (const auto &bankId : ctx.bankIds) {
    emit(identity::addressIdForBank(bankId), exporter::common::kUsCountry);
  }
}

void writeCounterpartyHasNameRows(exporter::csv::Writer &w,
                                  const vertices::SharedContext &ctx,
                                  t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &cpId : ctx.counterpartyIds) {
    w.writeRow(cpId, identity::nameIdForCounterparty(cpId), ts);
  }
}

void writeCounterpartyHasAddressRows(exporter::csv::Writer &w,
                                     const vertices::SharedContext &ctx,
                                     t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &cpId : ctx.counterpartyIds) {
    w.writeRow(cpId, identity::addressIdForCounterparty(cpId), ts);
  }
}

void writeCounterpartyAssociatedWithCountryRows(
    exporter::csv::Writer &w, const vertices::SharedContext &ctx) {
  for (const auto &cpId : ctx.counterpartyIds) {
    w.writeRow(cpId, exporter::common::kUsCountry);
  }
}

void writeCustomerMatchesWatchlistRows(exporter::csv::Writer &w,
                                       const pipe::People &people) {
  const auto &roster = people.roster.roster;
  std::string watchlistId;
  for (ent::PersonId p : allPersonIds(people)) {
    if (!roster.has(p, ent::person::Flag::fraud) &&
        !roster.has(p, ent::person::Flag::mule)) {
      continue;
    }
    const auto cid = exporter::common::renderCustomerId(p);
    const auto wlView =
        exporter::common::makeWatchlistId(watchlistId, cid.view());

    w.writeRow(cid, wlView);
  }
}

void writeReferencesRows(exporter::csv::Writer &w,
                         std::span<const exporter::aml::sar::SarRecord> sars) {
  for (const auto &sar : sars) {
    const auto n = sar.subjectPersonIds.size();
    for (std::size_t i = 0; i < n; ++i) {
      w.writeRow(sar.sarId,
                 exporter::common::renderCustomerId(sar.subjectPersonIds[i]),
                 sar.subjectRoles[i]);
    }
  }
}

void writeSarCoversRows(exporter::csv::Writer &w,
                        std::span<const exporter::aml::sar::SarRecord> sars) {
  for (const auto &sar : sars) {
    const auto n = sar.coveredAccountIds.size();
    for (std::size_t i = 0; i < n; ++i) {
      w.writeRow(sar.sarId,
                 exporter::common::renderKey(sar.coveredAccountIds[i]),
                 sar.coveredAmounts[i]);
    }
  }
}

void writeBeneficiaryBankRows(exporter::csv::Writer &w,
                              const std::set<ent::Key> &cpReceivers) {
  for (const auto &cpKey : cpReceivers) {
    const auto rendered = exporter::common::renderKey(cpKey);
    w.writeRow(counterpartyBankId(rendered), rendered);
  }
}

void writeOriginatorBankRows(exporter::csv::Writer &w,
                             const std::set<ent::Key> &cpSenders) {
  for (const auto &cpKey : cpSenders) {
    const auto rendered = exporter::common::renderKey(cpKey);
    w.writeRow(counterpartyBankId(rendered), rendered);
  }
}

void writeBankAssociatedWithCountryRows(exporter::csv::Writer &w,
                                        const vertices::SharedContext &ctx) {
  for (const auto &bankId : ctx.bankIds) {
    w.writeRow(bankId, exporter::common::kUsCountry);
  }
}

void writeBankHasAddressRows(exporter::csv::Writer &w,
                             const vertices::SharedContext &ctx,
                             t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &bankId : ctx.bankIds) {
    w.writeRow(bankId, identity::addressIdForBank(bankId), ts);
  }
}

void writeBankHasNameRows(exporter::csv::Writer &w,
                          const vertices::SharedContext &ctx,
                          t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &bankId : ctx.bankIds) {
    w.writeRow(bankId, identity::nameIdForBank(bankId), ts);
  }
}

void writeCustomerHasNameMinhashRows(exporter::csv::Writer &w,
                                     const pipe::People &people,
                                     const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = people.pii;

  std::string nameScratch;
  for (ent::PersonId p : allPersonIds(people)) {
    const auto nm = identity::nameForPerson(p, pii, pools);
    const auto cid = exporter::common::renderCustomerId(p);
    const auto nameStr =
        common::joinName(nameScratch, nm.firstName, nm.lastName);
    for (const auto &mhId :
         minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      w.writeRow(cid, mhId, nameStr);
    }
  }
}

void writeCustomerHasAddressMinhashRows(exporter::csv::Writer &w,
                                        const pipe::People &people,
                                        const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = people.pii;

  std::string addrScratch;
  for (ent::PersonId p : allPersonIds(people)) {
    const auto addr = identity::addressForPerson(p, pii, pools);
    const auto cid = exporter::common::renderCustomerId(p);
    const auto fullAddr = common::joinAddress(addrScratch, addr);
    for (const auto &mhId : minhash::addressMinhashIds(fullAddr)) {
      w.writeRow(cid, mhId, fullAddr);
    }
  }
}

void writeCustomerHasAddressStreetLine1MinhashRows(
    exporter::csv::Writer &w, const pipe::People &people,
    const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = people.pii;

  for (ent::PersonId p : allPersonIds(people)) {
    const auto addr = identity::addressForPerson(p, pii, pools);
    const auto cid = exporter::common::renderCustomerId(p);
    for (const auto &mhId : minhash::streetMinhashIds(addr.streetLine1)) {
      w.writeRow(cid, mhId, addr.streetLine1);
    }
  }
}

void writeCustomerHasAddressCityMinhashRows(
    exporter::csv::Writer &w, const pipe::People &people,
    const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = people.pii;

  for (ent::PersonId p : allPersonIds(people)) {
    const auto addr = identity::addressForPerson(p, pii, pools);
    w.writeRow(exporter::common::renderCustomerId(p),
               minhash::cityMinhashId(addr.city), addr.city);
  }
}

void writeCustomerHasAddressStateMinhashRows(
    exporter::csv::Writer &w, const pipe::People &people,
    const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = people.pii;

  for (ent::PersonId p : allPersonIds(people)) {
    const auto addr = identity::addressForPerson(p, pii, pools);
    w.writeRow(exporter::common::renderCustomerId(p),
               minhash::stateMinhashId(addr.state), addr.state);
  }
}

void writeAccountHasNameMinhashRows(exporter::csv::Writer &w,
                                    const pipe::People &people,
                                    const pipe::Holdings &holdings,
                                    const vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = people.pii;

  struct PerPersonNameMh {
    std::vector<minhash::BucketId> ids;
    std::string name;
  };
  std::unordered_map<ent::PersonId, PerPersonNameMh> mhByPerson;
  mhByPerson.reserve(people.roster.roster.count);
  std::string nameScratch;
  for (ent::PersonId p = 1; p <= people.roster.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p, pii, pools);
    auto &slot = mhByPerson[p];
    slot.ids = minhash::nameMinhashIds(nm.firstName, nm.lastName);
    slot.name.assign(
        exporter::common::joinName(nameScratch, nm.firstName, nm.lastName));
  }

  constexpr std::string_view kAccountNameTypeOf{"primary"};

  exporter::common::forEachInternalOwnership(
      holdings, [&](ent::PersonId pid, const ent::Key &k) {
        const auto it = mhByPerson.find(pid);
        if (it == mhByPerson.end()) {
          return;
        }
        const auto acctStr = exporter::common::renderKey(k);
        for (const auto &mhId : it->second.ids) {
          w.writeRow(acctStr, mhId, kAccountNameTypeOf, it->second.name);
        }
      });
}

void writeCounterpartyHasNameMinhashRows(exporter::csv::Writer &w,
                                         const vertices::SharedContext &ctx) {
  const auto &usPool = poolsFor(ctx).forCountry(loc::Country::us);

  std::string nameScratch;
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto nm = identity::nameForCounterparty(cpId, usPool);
    const auto nameStr =
        exporter::common::joinName(nameScratch, nm.firstName, nm.lastName);
    for (const auto &mhId :
         minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      w.writeRow(cpId, mhId, nameStr);
    }
  }
}

void writeResolvesToRows(exporter::csv::Writer &w,
                         const pipe::Holdings &holdings,
                         t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &rec : holdings.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) == 0) {
      continue;
    }
    if (rec.owner == ent::invalidPerson) {
      continue;
    }
    w.writeRow(exporter::common::renderKey(rec.id),
               exporter::common::renderCustomerId(rec.owner), 0.95,
               std::string_view{"account_link"}, ts);
  }
}

} // namespace PhantomLedger::exporter::aml::edges
