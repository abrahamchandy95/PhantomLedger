#include "phantomledger/exporter/aml/edges.hpp"

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/aml/identity.hpp"
#include "phantomledger/exporter/aml/minhash.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/common/render.hpp"

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
namespace pl = ::PhantomLedger::pipeline;
namespace pii_ns = ::PhantomLedger::entities::synth::pii;

// ──────────────────────────────────────────────────────────────────────
// Small leaf-level helpers
// ──────────────────────────────────────────────────────────────────────

[[nodiscard]] std::string renderKey(const ent::Key &k) {
  return std::string(::PhantomLedger::encoding::format(k).view());
}

[[nodiscard]] std::string customerIdFor(ent::PersonId p) {
  return std::string(
      ::PhantomLedger::exporter::common::renderCustomerId(p).view());
}

[[nodiscard]] std::string transactionId(std::size_t idx1) {
  char buf[20];
  std::snprintf(buf, sizeof(buf), "TXN_%012zu", idx1);
  return std::string{buf};
}

[[nodiscard]] bool isExternalKey(const ent::Key &k) noexcept {
  return k.bank == ent::Bank::external;
}

/// Same accessor pattern as vertices.cpp: every content-using writer
/// goes through this so a hand-built SharedContext with a null pool
/// fails loudly in debug builds.
[[nodiscard]] const pii_ns::LocalePool &
poolFor(const vertices::SharedContext &ctx) noexcept {
  assert(ctx.usPool != nullptr &&
         "SharedContext::usPool is null — was the context built with "
         "buildSharedContext(entities, txns, usPool)?");
  return *ctx.usPool;
}

/// Walk every (personId, internal-account-key) pair.
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

/// Concatenate the four address fields into a single buffer used by
/// the minhash address shingler. Reuses a thread-local buffer so per-
/// row allocation cost is amortized to ~0.
[[nodiscard]] std::string_view joinAddress(std::string &scratch,
                                           const identity::AddressRecord &a) {
  scratch.clear();
  scratch.reserve(a.streetLine1.size() + a.city.size() + a.state.size() +
                  a.postalCode.size() + 4);
  scratch.append(a.streetLine1);
  scratch.push_back(' ');
  scratch.append(a.city);
  scratch.push_back(' ');
  scratch.append(a.state);
  scratch.push_back(' ');
  scratch.append(a.postalCode);
  return scratch;
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// classifyTransactionEdges — needs the pool for counterparty names
// ──────────────────────────────────────────────────────────────────────

TransactionEdgeBundle
classifyTransactionEdges(const pl::Entities &entities,
                         std::span<const tx_ns::Transaction> finalTxns,
                         const vertices::SharedContext &ctx) {
  TransactionEdgeBundle out;
  const auto &usPool = poolFor(ctx);

  // Memoize counterparty business names — the same external key shows
  // up across many transactions, and `nameForCounterparty` does a hash
  // + pool indirection we'd otherwise repeat per row.
  std::unordered_map<ent::Key, std::string> cpNames;

  out.sendRows.reserve(finalTxns.size());
  out.receiveRows.reserve(finalTxns.size());

  const auto cpNameFor = [&](const ent::Key &k) -> const std::string & {
    auto it = cpNames.find(k);
    if (it == cpNames.end()) {
      const auto rendered = renderKey(k);
      auto name = std::string{
          identity::nameForCounterparty(rendered, usPool).firstName};
      it = cpNames.emplace(k, std::move(name)).first;
    }
    return it->second;
  };

  std::size_t idx = 1;
  for (const auto &tx : finalTxns) {
    const auto txnId = transactionId(idx);
    ++idx;

    const bool srcExt = isExternalKey(tx.source);
    const bool dstExt = isExternalKey(tx.target);
    const auto srcStr = renderKey(tx.source);
    const auto dstStr = renderKey(tx.target);

    if (srcExt) {
      out.cpSendRows.emplace_back(srcStr, txnId, cpNameFor(tx.source));
      out.cpSenders.insert(srcStr);
      if (!dstExt) {
        out.receivedFromCpPairs.emplace(dstStr, srcStr);
      }
    } else {
      out.sendRows.emplace_back(srcStr, txnId);
      if (dstExt) {
        out.sentToCpPairs.emplace(srcStr, dstStr);
      }
    }

    if (dstExt) {
      out.cpReceiveRows.emplace_back(dstStr, txnId, cpNameFor(tx.target));
      out.cpReceivers.insert(dstStr);
    } else {
      out.receiveRows.emplace_back(dstStr, txnId);
    }
  }

  (void)entities;
  return out;
}

// ──────────────────────────────────────────────────────────────────────
// collectMinhashVertexSets — needs full name/address content
// ──────────────────────────────────────────────────────────────────────

MinhashVertexSets collectMinhashVertexSets(const pl::Entities &entities,
                                           const vertices::SharedContext &ctx) {
  MinhashVertexSets out;
  const auto &usPool = poolFor(ctx);
  const auto &pii = entities.pii;

  std::string addrScratch;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p, pii, usPool);
    const auto addr = identity::addressForPerson(p, pii, usPool);

    for (auto &id : minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      out.name.insert(std::move(id));
    }

    const auto fullAddr = joinAddress(addrScratch, addr);
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
    const auto nm = identity::nameForCounterparty(cpId, usPool);
    for (auto &id : minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      out.name.insert(std::move(id));
    }
  }
  return out;
}

// ──────────────────────────────────────────────────────────────────────
// Simple ownership edges
// ──────────────────────────────────────────────────────────────────────

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

// ──────────────────────────────────────────────────────────────────────
// Transaction-flow edge writers
// ──────────────────────────────────────────────────────────────────────

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

// ──────────────────────────────────────────────────────────────────────
// Device / network edge writers
// ──────────────────────────────────────────────────────────────────────

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
    touchDeviceAgg(agg[{usage.personId, idStr}], usage.firstSeen,
                   usage.lastSeen);
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
    const auto it = accountsByPerson.find(usage.personId);
    if (it == accountsByPerson.end()) {
      continue;
    }
    const std::string idStr{
        ::PhantomLedger::exporter::common::renderDeviceId(usage.deviceId)
            .view()};
    for (const auto &acctKey : it->second) {
      touchDeviceAgg(agg[{acctKey, idStr}], usage.firstSeen, usage.lastSeen);
    }
  }

  for (const auto &[key, slot] : agg) {
    w.writeRow(renderKey(key.first), key.second,
               t_ns::formatTimestamp(slot.firstSeen),
               t_ns::formatTimestamp(slot.lastSeen), slot.count);
  }
}

// ──────────────────────────────────────────────────────────────────────
// Identity-by-id edges (Camp 1) — pool-free
//
// These all use identity::{name,address}IdFor*  rather than the full
// producers, because we only need the deterministic id string for the
// edge row, not the underlying name/address content.
// ──────────────────────────────────────────────────────────────────────

void writeCustomerHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                              const pl::Entities &entities,
                              t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    w.writeRow(customerIdFor(p), identity::nameIdForPerson(p), ts);
  }
}

void writeCustomerHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                                 const pl::Entities &entities,
                                 t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    w.writeRow(customerIdFor(p), identity::addressIdForPerson(p), ts);
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
    w.writeRow(renderKey(k), identity::nameIdForPerson(pid),
               std::string_view{"primary"}, ts);
  });
}

void writeAccountHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                                const pl::Entities &entities,
                                t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  forEachInternalOwnership(entities, [&](ent::PersonId pid, const ent::Key &k) {
    w.writeRow(renderKey(k), identity::addressIdForPerson(pid),
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
  // Country is always "US" for the AML exporter, so we can emit the
  // edge straight from the address id without producing the full
  // AddressRecord. Dedup set is sized by unique entities, not rows.
  const auto ts = t_ns::formatTimestamp(simStart);
  std::unordered_set<std::string> emitted;
  emitted.reserve(entities.people.roster.count + ctx.counterpartyIds.size() +
                  21U);

  const auto emit = [&](identity::StackString<32> id) {
    if (emitted.emplace(id.view()).second) {
      w.writeRow(id, std::string_view{"US"}, ts);
    }
  };

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    emit(identity::addressIdForPerson(p));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emit(identity::addressIdForCounterparty(cpId));
  }
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    emit(identity::addressIdForBank(bankId));
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
    w.writeRow(bankId, identity::addressIdForBank(bankId), ts);
  }
}

void writeBankHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                          const vertices::SharedContext &ctx,
                          t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    w.writeRow(bankId, identity::nameIdForBank(bankId), ts);
  }
}

// ──────────────────────────────────────────────────────────────────────
// Minhash-shingle edges (Camp 2) — need name/address content
// ──────────────────────────────────────────────────────────────────────

void writeCustomerHasNameMinhashRows(::PhantomLedger::exporter::csv::Writer &w,
                                     const pl::Entities &entities,
                                     const vertices::SharedContext &ctx) {
  const auto &usPool = poolFor(ctx);
  const auto &pii = entities.pii;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p, pii, usPool);
    const auto cid = customerIdFor(p);
    for (const auto &mhId :
         minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      w.writeRow(cid, mhId);
    }
  }
}

void writeCustomerHasAddressMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities,
    const vertices::SharedContext &ctx) {
  const auto &usPool = poolFor(ctx);
  const auto &pii = entities.pii;

  std::string addrScratch;
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p, pii, usPool);
    const auto cid = customerIdFor(p);
    const auto fullAddr = joinAddress(addrScratch, addr);
    for (const auto &mhId : minhash::addressMinhashIds(fullAddr)) {
      w.writeRow(cid, mhId);
    }
  }
}

void writeCustomerHasAddressStreetLine1MinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities,
    const vertices::SharedContext &ctx) {
  const auto &usPool = poolFor(ctx);
  const auto &pii = entities.pii;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p, pii, usPool);
    const auto cid = customerIdFor(p);
    for (const auto &mhId : minhash::streetMinhashIds(addr.streetLine1)) {
      w.writeRow(cid, mhId);
    }
  }
}

void writeCustomerHasAddressCityMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities,
    const vertices::SharedContext &ctx) {
  const auto &usPool = poolFor(ctx);
  const auto &pii = entities.pii;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p, pii, usPool);
    w.writeRow(customerIdFor(p), minhash::cityMinhashId(addr.city));
  }
}

void writeCustomerHasAddressStateMinhashRows(
    ::PhantomLedger::exporter::csv::Writer &w, const pl::Entities &entities,
    const vertices::SharedContext &ctx) {
  const auto &usPool = poolFor(ctx);
  const auto &pii = entities.pii;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto addr = identity::addressForPerson(p, pii, usPool);
    w.writeRow(customerIdFor(p), minhash::stateMinhashId(addr.state));
  }
}

void writeAccountHasNameMinhashRows(::PhantomLedger::exporter::csv::Writer &w,
                                    const pl::Entities &entities,
                                    const vertices::SharedContext &ctx) {
  const auto &usPool = poolFor(ctx);
  const auto &pii = entities.pii;

  // Precompute the minhash id list per person — many accounts share an
  // owner, so this avoids re-shingling for each one.
  std::unordered_map<ent::PersonId, std::vector<std::string>> mhByPerson;
  mhByPerson.reserve(entities.people.roster.count);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p, pii, usPool);
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
  const auto &usPool = poolFor(ctx);

  for (const auto &cpId : ctx.counterpartyIds) {
    const auto nm = identity::nameForCounterparty(cpId, usPool);
    for (const auto &mhId :
         minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      w.writeRow(cpId, mhId);
    }
  }
}

// ──────────────────────────────────────────────────────────────────────
// Graph resolution
// ──────────────────────────────────────────────────────────────────────

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
