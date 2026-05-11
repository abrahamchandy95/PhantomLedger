#include "phantomledger/exporter/aml/edges.hpp"

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/aml/identity.hpp"
#include "phantomledger/exporter/aml/minhash.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/common/render.hpp"

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
namespace pl = ::PhantomLedger::pipeline;

[[nodiscard]] std::string renderKey(const ent::Key &k) {
  return std::string(::PhantomLedger::encoding::format(k).view());
}

[[nodiscard]] std::string customerIdFor(ent::PersonId p) {
  return std::string(
      ::PhantomLedger::exporter::common::renderCustomerId(p).view());
}

template <class Fn>
void forEachInternalOwnership(const pl::Entities &entities, Fn &&fn) {
  const auto &ownership = entities.accounts.ownership;
  const auto &records = entities.accounts.registry.records;

  const auto numPersons = ownership.byPersonOffset.empty()
                              ? std::size_t{0}
                              : ownership.byPersonOffset.size() - 1;

  for (std::size_t i = 0; i < numPersons; ++i) {
    const auto offset = ownership.byPersonOffset[i];
    const auto end = ownership.byPersonOffset[i + 1];
    const auto pid = static_cast<ent::PersonId>(i + 1);

    for (auto j = offset; j < end; ++j) {
      const auto regIdx = ownership.byPersonIndex[j];
      const auto &rec = records[regIdx];
      if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
        continue;
      }
      fn(pid, rec.id);
    }
  }
}

[[nodiscard]] std::string transactionId(std::size_t idx1) {
  char buf[20];
  std::snprintf(buf, sizeof(buf), "TXN_%012zu", idx1);
  return std::string{buf};
}

[[nodiscard]] bool isExternalKey(const ent::Key &k) noexcept {
  return k.bank == ent::Bank::external;
}

} // namespace

TransactionEdgeBundle
classifyTransactionEdges(const pl::Entities &entities,
                         std::span<const tx_ns::Transaction> finalTxns) {
  TransactionEdgeBundle out;

  std::unordered_map<ent::Key, std::string> cpNames;

  out.sendRows.reserve(finalTxns.size());
  out.receiveRows.reserve(finalTxns.size());

  std::size_t idx = 1;
  for (const auto &tx : finalTxns) {
    const auto txnId = transactionId(idx);
    ++idx;

    const bool srcExt = isExternalKey(tx.source);
    const bool dstExt = isExternalKey(tx.target);

    auto getCpName = [&](const ent::Key &k) -> const std::string & {
      auto it = cpNames.find(k);
      if (it == cpNames.end()) {
        const auto rendered = renderKey(k);
        it = cpNames
                 .emplace(k, identity::nameForCounterparty(rendered).firstName)
                 .first;
      }
      return it->second;
    };

    if (srcExt) {
      const auto srcStr = renderKey(tx.source);
      const auto &nm = getCpName(tx.source);
      out.cpSendRows.emplace_back(srcStr, txnId, nm);
      out.cpSenders.insert(srcStr);
      if (!dstExt) {
        out.receivedFromCpPairs.emplace(renderKey(tx.target), srcStr);
      }
    } else {
      const auto srcStr = renderKey(tx.source);
      out.sendRows.emplace_back(srcStr, txnId);
      if (dstExt) {
        out.sentToCpPairs.emplace(srcStr, renderKey(tx.target));
      }
    }

    if (dstExt) {
      const auto dstStr = renderKey(tx.target);
      const auto &nm = getCpName(tx.target);
      out.cpReceiveRows.emplace_back(dstStr, txnId, nm);
      out.cpReceivers.insert(dstStr);
    } else {
      const auto dstStr = renderKey(tx.target);
      out.receiveRows.emplace_back(dstStr, txnId);
    }
  }

  (void)entities;
  return out;
}

MinhashVertexSets collectMinhashVertexSets(const pl::Entities &entities,
                                           const vertices::SharedContext &ctx) {
  MinhashVertexSets out;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p);
    const auto addr = identity::addressForPerson(p);

    for (auto &id : minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      out.name.insert(std::move(id));
    }

    std::string fullAddr;
    fullAddr.reserve(addr.streetLine1.size() + addr.city.size() +
                     addr.state.size() + addr.postalCode.size() + 4);
    fullAddr.append(addr.streetLine1);
    fullAddr.push_back(' ');
    fullAddr.append(addr.city);
    fullAddr.push_back(' ');
    fullAddr.append(addr.state);
    fullAddr.push_back(' ');
    fullAddr.append(addr.postalCode);
    for (auto &id : minhash::addressMinhashIds(fullAddr)) {
      out.address.insert(std::move(id));
    }

    for (auto &id : minhash::streetMinhashIds(addr.streetLine1)) {
      out.street.insert(std::move(id));
    }
    out.city.insert(minhash::cityMinhashId(addr.city));
    out.state.insert(minhash::stateMinhashId(addr.state));
  }

  // Counterparty side — name only.
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto nm = identity::nameForCounterparty(cpId);
    for (auto &id : minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      out.name.insert(std::move(id));
    }
  }
  return out;
}

void writeCustomerHasAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                                 const pl::Entities &entities) {
  forEachInternalOwnership(entities, [&](ent::PersonId pid, const ent::Key &k) {
    w.writeRow(customerIdFor(pid), renderKey(k));
  });
}

void writeAccountHasPrimaryCustomerRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities) {
  forEachInternalOwnership(entities, [&](ent::PersonId pid, const ent::Key &k) {
    w.writeRow(renderKey(k), customerIdFor(pid));
  });
}

void writeAcctTxnRows(::PhantomLedger::exporter::csv::Writer &w,
                      std::span<const TransactionEdgeBundle::AcctTxnRow> rows) {
  for (const auto &[acct, txn] : rows) {
    w.writeRow(acct, txn);
  }
}

void writeCpTxnRows(::PhantomLedger::exporter::csv::Writer &w,
                    std::span<const TransactionEdgeBundle::CpTxnRow> rows) {
  for (const auto &row : rows) {
    w.writeRow(std::get<0>(row), std::get<1>(row), std::get<2>(row));
  }
}

void writeAcctCpPairRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const std::set<std::pair<std::string, std::string>> &pairs) {
  for (const auto &[acct, cp] : pairs) {
    w.writeRow(acct, cp);
  }
}

namespace {

struct DeviceAgg {
  t_ns::TimePoint firstSeen{};
  t_ns::TimePoint lastSeen{};
  std::uint32_t count = 0;
};

void touchDeviceAgg(DeviceAgg &slot, t_ns::TimePoint fs, t_ns::TimePoint ls) {
  if (slot.count == 0) {
    slot.firstSeen = fs;
    slot.lastSeen = ls;
  } else {
    if (fs < slot.firstSeen) {
      slot.firstSeen = fs;
    }
    if (ls > slot.lastSeen) {
      slot.lastSeen = ls;
    }
  }
  ++slot.count;
}

} // namespace

void writeUsesDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices) {
  std::map<std::pair<ent::PersonId, std::string>, DeviceAgg> agg;

  for (const auto &usage : devices.usages) {
    const std::string idStr{
        ::PhantomLedger::exporter::common::renderDeviceId(usage.deviceId)
            .view()};
    auto &slot = agg[{usage.personId, idStr}];
    touchDeviceAgg(slot, usage.firstSeen, usage.lastSeen);
  }

  for (const auto &[key, slot] : agg) {
    w.writeRow(customerIdFor(key.first), key.second,
               t_ns::formatTimestamp(slot.firstSeen),
               t_ns::formatTimestamp(slot.lastSeen), slot.count);
  }
}

void writeLoggedFromRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities,
    const ::PhantomLedger::infra::synth::devices::Output &devices) {
  std::map<std::pair<ent::Key, std::string>, DeviceAgg> agg;

  std::unordered_map<ent::PersonId, std::vector<ent::Key>> accountsByPerson;
  forEachInternalOwnership(entities, [&](ent::PersonId pid, const ent::Key &k) {
    accountsByPerson[pid].push_back(k);
  });

  for (const auto &usage : devices.usages) {
    const std::string idStr{
        ::PhantomLedger::exporter::common::renderDeviceId(usage.deviceId)
            .view()};
    const auto it = accountsByPerson.find(usage.personId);
    if (it == accountsByPerson.end()) {
      continue;
    }
    for (const auto &acctKey : it->second) {
      auto &slot = agg[{acctKey, idStr}];
      touchDeviceAgg(slot, usage.firstSeen, usage.lastSeen);
    }
  }

  for (const auto &[key, slot] : agg) {
    w.writeRow(renderKey(key.first), key.second,
               t_ns::formatTimestamp(slot.firstSeen),
               t_ns::formatTimestamp(slot.lastSeen), slot.count);
  }
}

void writeCustomerHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                              const pl::Entities &entities,
                              t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    w.writeRow(customerIdFor(p), identity::nameForPerson(p).id, ts);
  }
}

void writeCustomerHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                                 const pl::Entities &entities,
                                 t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    w.writeRow(customerIdFor(p), identity::addressForPerson(p).id, ts);
  }
}

void writeCustomerAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities,
    t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    w.writeRow(customerIdFor(p), std::string_view{"US"}, ts);
  }
}

void writeAccountHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                             const pl::Entities &entities,
                             t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  forEachInternalOwnership(entities, [&](ent::PersonId pid, const ent::Key &k) {
    w.writeRow(renderKey(k), identity::nameForPerson(pid).id,
               std::string_view{"primary"}, ts);
  });
}

void writeAccountHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                                const pl::Entities &entities,
                                t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  forEachInternalOwnership(entities, [&](ent::PersonId pid, const ent::Key &k) {
    w.writeRow(renderKey(k), identity::addressForPerson(pid).id,
               std::string_view{"mailing"}, ts);
  });
}

void writeAccountAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities,
    t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      continue;
    }
    w.writeRow(renderKey(rec.id), std::string_view{"US"}, ts);
  }
}

void writeAddressInCountryRows(::PhantomLedger::exporter::csv::Writer &w,
                               const pl::Entities &entities,
                               const vertices::SharedContext &ctx,
                               t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  std::unordered_set<std::string> emitted;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p);
    if (emitted.insert(addr.id).second) {
      w.writeRow(addr.id, addr.country, ts);
    }
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto addr = identity::addressForCounterparty(cpId);
    if (emitted.insert(addr.id).second) {
      w.writeRow(addr.id, addr.country, ts);
    }
  }
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    const auto addr = identity::addressForBank(bankId);
    if (emitted.insert(addr.id).second) {
      w.writeRow(addr.id, addr.country, ts);
    }
  }
}

void writeCounterpartyHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                                  const vertices::SharedContext &ctx,
                                  t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &cpId : ctx.counterpartyIds) {
    w.writeRow(cpId, identity::nameForCounterparty(cpId).id, ts);
  }
}

void writeCounterpartyHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                                     const vertices::SharedContext &ctx,
                                     t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &cpId : ctx.counterpartyIds) {
    w.writeRow(cpId, identity::addressForCounterparty(cpId).id, ts);
  }
}

void writeCounterpartyAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const vertices::SharedContext &ctx) {
  for (const auto &cpId : ctx.counterpartyIds) {
    w.writeRow(cpId, std::string_view{"US"});
  }
}

void writeCustomerMatchesWatchlistRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities) {
  const auto &roster = entities.people.roster;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    if (!roster.has(p, ent::person::Flag::fraud) &&
        !roster.has(p, ent::person::Flag::mule)) {
      continue;
    }
    const auto cidStr = customerIdFor(p);
    w.writeRow(cidStr, "WL_" + cidStr);
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
      w.writeRow(sar.sarId, renderKey(sar.coveredAccountIds[i]),
                 sar.coveredAmounts[i]);
    }
  }
}

void writeBeneficiaryBankRows(::PhantomLedger::exporter::csv::Writer &w,
                              const std::set<std::string> &cpReceivers) {
  for (const auto &cpId : cpReceivers) {
    w.writeRow(counterpartyBankId(cpId), cpId);
  }
}

void writeOriginatorBankRows(::PhantomLedger::exporter::csv::Writer &w,
                             const std::set<std::string> &cpSenders) {
  for (const auto &cpId : cpSenders) {
    w.writeRow(counterpartyBankId(cpId), cpId);
  }
}

void writeBankAssociatedWithCountryRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const vertices::SharedContext &ctx) {
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    w.writeRow(bankId, std::string_view{"US"});
  }
}

void writeBankHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                             const vertices::SharedContext &ctx,
                             t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    w.writeRow(bankId, identity::addressForBank(bankId).id, ts);
  }
}

void writeBankHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                          const vertices::SharedContext &ctx,
                          t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    w.writeRow(bankId, identity::nameForBank(bankId).id, ts);
  }
}

void writeCustomerHasNameMinhashRows(::PhantomLedger::exporter::csv::Writer &w,
                                     const pl::Entities &entities) {
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p);
    const auto cid = customerIdFor(p);
    for (const auto &mhId :
         minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      w.writeRow(cid, mhId);
    }
  }
}

void writeCustomerHasAddressMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities) {
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p);
    const auto cid = customerIdFor(p);
    std::string fullAddr;
    fullAddr.reserve(addr.streetLine1.size() + addr.city.size() +
                     addr.state.size() + addr.postalCode.size() + 4);
    fullAddr.append(addr.streetLine1);
    fullAddr.push_back(' ');
    fullAddr.append(addr.city);
    fullAddr.push_back(' ');
    fullAddr.append(addr.state);
    fullAddr.push_back(' ');
    fullAddr.append(addr.postalCode);
    for (const auto &mhId : minhash::addressMinhashIds(fullAddr)) {
      w.writeRow(cid, mhId);
    }
  }
}

void writeCustomerHasAddressStreetLine1MinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities) {
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p);
    const auto cid = customerIdFor(p);
    for (const auto &mhId : minhash::streetMinhashIds(addr.streetLine1)) {
      w.writeRow(cid, mhId);
    }
  }
}

void writeCustomerHasAddressCityMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities) {
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p);
    w.writeRow(customerIdFor(p), minhash::cityMinhashId(addr.city));
  }
}

void writeCustomerHasAddressStateMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities) {
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p);
    w.writeRow(customerIdFor(p), minhash::stateMinhashId(addr.state));
  }
}

void writeAccountHasNameMinhashRows(::PhantomLedger::exporter::csv::Writer &w,
                                    const pl::Entities &entities) {
  std::unordered_map<ent::PersonId, std::vector<std::string>> mhByPerson;
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p);
    mhByPerson[p] = minhash::nameMinhashIds(nm.firstName, nm.lastName);
  }

  forEachInternalOwnership(entities, [&](ent::PersonId pid, const ent::Key &k) {
    const auto it = mhByPerson.find(pid);
    if (it == mhByPerson.end()) {
      return;
    }
    const auto acctStr = renderKey(k);
    for (const auto &mhId : it->second) {
      w.writeRow(acctStr, mhId);
    }
  });
}

void writeCounterpartyHasNameMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const vertices::SharedContext &ctx) {
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto nm = identity::nameForCounterparty(cpId);
    for (const auto &mhId :
         minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      w.writeRow(cpId, mhId);
    }
  }
}

void writeResolvesToRows(::PhantomLedger::exporter::csv::Writer &w,
                         const pl::Entities &entities,
                         t_ns::TimePoint simStart) {

  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) == 0) {
      continue;
    }
    if (rec.owner == ent::invalidPerson) {
      continue;
    }
    w.writeRow(renderKey(rec.id), customerIdFor(rec.owner), 0.95,
               std::string_view{"account_link"}, ts);
  }
}

} // namespace PhantomLedger::exporter::aml::edges
