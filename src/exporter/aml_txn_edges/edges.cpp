#include "phantomledger/exporter/aml_txn_edges/edges.hpp"

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/exporter/aml/identity.hpp"
#include "phantomledger/exporter/aml/minhash.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/common/hashing.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/common/support.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

namespace PhantomLedger::exporter::aml_txn_edges::edges {

namespace devices = ::PhantomLedger::devices;
namespace entity = ::PhantomLedger::entity;
namespace exporter = ::PhantomLedger::exporter;
namespace network = ::PhantomLedger::network;
namespace pipeline = ::PhantomLedger::pipeline;
namespace primitives = ::PhantomLedger::primitives;
namespace synth = ::PhantomLedger::synth;
namespace time_ns = ::PhantomLedger::time;
namespace txns = ::PhantomLedger::transactions;

namespace {

inline constexpr std::string_view kSourceCore = "core_banking";
inline constexpr std::string_view kSourceKyc = "kyc_system";
inline constexpr std::string_view kSourceAml = "aml_system";
inline constexpr std::string_view kSourceCases = "case_mgmt";

[[nodiscard]] const synth::pii::PoolSet &
poolsFor(const aml::vertices::SharedContext &ctx) noexcept {
  assert(ctx.pools != nullptr);
  return *ctx.pools;
}

[[nodiscard]] auto allPersonIds(const pipeline::People &people) {
  return std::views::iota(1u, people.roster.roster.count + 1);
}

} // namespace

void writeOwnsRows(exporter::csv::Writer &w, const pipeline::Holdings &holdings,
                   time_ns::TimePoint simStart) {
  const auto ts = time_ns::formatTimestamp(simStart);
  exporter::common::forEachInternalOwnership(
      holdings, [&](entity::PersonId pid, const entity::Key &k) {
        w.cell(exporter::common::renderCustomerId(pid))
            .cell(exporter::common::renderKey(k))
            .cell(ts)
            .cell(kSourceCore)
            .cell(ts)
            .cellEmpty();
        w.endRow();
      });
}

void writeTransactedRows(
    exporter::csv::Writer &w,
    std::span<const transactions::Transaction> postedTxns) {
  std::size_t idx = 1;
  for (const auto &tx : postedTxns) {
    w.cell(exporter::common::renderKey(tx.source))
        .cell(exporter::common::renderKey(tx.target))
        .cell(static_cast<std::uint32_t>(idx))
        .cell(time_ns::formatTimestamp(time_ns::fromEpochSeconds(tx.timestamp)))
        .cell(primitives::utils::roundMoney(tx.amount))
        .cell(std::string_view{"USD"})
        .cell(static_cast<std::int64_t>(tx.session.channel.value))
        .cell(exporter::common::kUsCountry)
        .cell(derived::isCreditChannel(tx.session.channel) ? 1 : 0)
        .cell(kSourceCore);
    w.endRow();
    ++idx;
  }
}

void writeInvolvesCounterpartyRows(
    exporter::csv::Writer &w, std::span<const txns::Transaction> postedTxns,
    time_ns::TimePoint simStart) {
  std::set<std::pair<entity::Key, entity::Key>> pairs;

  for (const auto &tx : postedTxns) {
    const bool srcExt = exporter::common::isExternalKey(tx.source);
    const bool dstExt = exporter::common::isExternalKey(tx.target);
    if (srcExt && !dstExt) {
      pairs.emplace(tx.target, tx.source);
    } else if (!srcExt && dstExt) {
      pairs.emplace(tx.source, tx.target);
    }
  }

  const auto ts = time_ns::formatTimestamp(simStart);
  for (const auto &[acct, cp] : pairs) {
    const auto cpRendered = exporter::common::renderKey(cp);
    w.cell(exporter::common::renderKey(acct))
        .cell(derived::makeCpId(cpRendered.view()))
        .cell(ts)
        .cell(kSourceCore)
        .cell(ts)
        .cellEmpty();
    w.endRow();
  }
}

void writeBanksAtRows(exporter::csv::Writer &w,
                      const aml::vertices::SharedContext &ctx,
                      time_ns::TimePoint simStart) {
  const auto ts = time_ns::formatTimestamp(simStart);
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto cpVertexId = derived::makeCpId(cpId);
    const auto bankId = exporter::aml::counterpartyBankId(cpId);
    w.cell(cpVertexId).cell(bankId).cell(kSourceCore).cell(ts).cellEmpty();
    w.endRow();
  }
}

void writeOnWatchlistRows(exporter::csv::Writer &w,
                          const pipeline::People &people,
                          time_ns::TimePoint simStart) {
  const auto &roster = people.roster.roster;
  const auto ts = time_ns::formatTimestamp(simStart);
  std::string watchlistId;

  auto is_on_watchlist = [&](entity::PersonId p) {
    return roster.has(p, entity::person::Flag::fraud) ||
           roster.has(p, entity::person::Flag::mule);
  };

  for (entity::PersonId p :
       allPersonIds(people) | std::views::filter(is_on_watchlist)) {
    const auto cid = exporter::common::renderCustomerId(p);
    const auto cidView = cid.view();
    const auto wlView = exporter::common::makeWatchlistId(watchlistId, cidView);

    const double matchScore =
        0.5 + static_cast<double>(exporter::common::stableU64({"WL", cidView}) %
                                  50ULL) /
                  100.0;

    w.cell(cid)
        .cell(wlView)
        .cell(ts)
        .cell(kSourceAml)
        .cell(matchScore)
        .cellEmpty()
        .cell(ts)
        .cellEmpty();
    w.endRow();
  }
}

void writeSubjectOfSarRows(
    exporter::csv::Writer &w,
    std::span<const exporter::aml::sar::SarRecord> sars) {
  for (const auto &sar : sars) {
    const auto filing = time_ns::formatTimestamp(sar.filingDate);
    for (const auto &acct : sar.coveredAccountIds) {
      w.cell(exporter::common::renderKey(acct))
          .cell(sar.sarId)
          .cell(filing)
          .cell(kSourceAml)
          .cellEmpty();
      w.endRow();
    }
  }
}

void writeFiledCtrRows(exporter::csv::Writer &w,
                       const derived::Bundle &bundle) {
  for (const auto &c : bundle.ctrs) {
    w.cell(exporter::common::renderKey(c.onAccount))
        .cell(c.id)
        .cell(time_ns::formatTimestamp(c.filingDate))
        .cell(kSourceCore);
    w.endRow();
  }
}

void writeAlertOnRows(exporter::csv::Writer &w, const derived::Bundle &bundle) {
  for (const auto &a : bundle.alerts) {
    w.cell(a.id)
        .cell(exporter::common::renderKey(a.onAccount))
        .cell(time_ns::formatTimestamp(a.createdDate))
        .cell(derived::ruleName(a.rule))
        .cell(derived::ruleSourceSystem(a.rule));
    w.endRow();
  }
}

void writeDispositionedAsRows(exporter::csv::Writer &w,
                              const derived::Bundle &bundle) {
  for (const auto &d : bundle.dispositions) {
    const auto &alert = bundle.alerts[d.alertIndex];
    w.cell(alert.id)
        .cell(d.id)
        .cell(time_ns::formatTimestamp(d.date))
        .cell(derived::investigatorIdFor(d.investigatorNum))
        .cell(d.notesHash)
        .cellEmpty()
        .cell(kSourceCases)
        .cell(d.confidence);
    w.endRow();
  }
}

void writeEscalatedToRows(exporter::csv::Writer &w,
                          const derived::Bundle &bundle,
                          std::span<const exporter::aml::sar::SarRecord> sars) {
  std::vector<std::vector<std::size_t>> sarsByAlert(bundle.alerts.size());
  for (const auto &c : bundle.cases) {
    for (const auto ai : c.alertIndices) {
      for (const auto si : c.sarIndices) {
        sarsByAlert[ai].push_back(si);
      }
    }
  }
  for (auto &v : sarsByAlert) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
  }

  auto is_escalated = [](const auto &d) {
    return d.outcome == derived::DispositionOutcome::escalated;
  };

  for (const auto &d : bundle.dispositions | std::views::filter(is_escalated)) {
    const auto &linked = sarsByAlert[d.alertIndex];
    if (linked.empty()) {
      continue;
    }
    const auto escalationDate = time_ns::formatTimestamp(d.date);
    for (const auto si : linked) {
      w.cell(d.id)
          .cell(sars[si].sarId)
          .cell(escalationDate)
          .cell(kSourceCases)
          .cellEmpty();
      w.endRow();
    }
  }
}

void writeContainsAlertRows(exporter::csv::Writer &w,
                            const derived::Bundle &bundle) {
  for (const auto &c : bundle.cases) {
    const auto createdAt = time_ns::formatTimestamp(c.openedDate);
    for (const auto ai : c.alertIndices) {
      w.cell(c.id)
          .cell(bundle.alerts[ai].id)
          .cell(createdAt)
          .cell(kSourceCases);
      w.endRow();
    }
  }
}

void writeResultedInRows(exporter::csv::Writer &w,
                         const derived::Bundle &bundle,
                         std::span<const exporter::aml::sar::SarRecord> sars) {
  for (const auto &c : bundle.cases) {
    const auto createdAt = time_ns::formatTimestamp(c.openedDate);
    for (const auto si : c.sarIndices) {
      w.cell(c.id).cell(sars[si].sarId).cell(createdAt).cell(kSourceCases);
      w.endRow();
    }
  }
}

void writeHasEvidenceRows(exporter::csv::Writer &w,
                          const derived::Bundle &bundle) {
  for (const auto &c : bundle.cases) {
    const auto createdAt = time_ns::formatTimestamp(c.openedDate);
    for (const auto ei : c.evidenceIndices) {
      w.cell(c.id)
          .cell(bundle.evidence[ei].id)
          .cell(createdAt)
          .cell(kSourceCases);
      w.endRow();
    }
  }
}

void writeContainsPromotedTxnRows(exporter::csv::Writer &w,
                                  const derived::Bundle &bundle) {
  for (const auto &c : bundle.cases) {
    const auto createdAt = time_ns::formatTimestamp(c.openedDate);
    for (const auto pi : c.promotedTxnIndices) {
      w.cell(c.id)
          .cell(bundle.promotedTxns[pi].id)
          .cell(createdAt)
          .cell(kSourceCases);
      w.endRow();
    }
  }
}

void writePromotedTxnAccountRows(
    exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const txns::Transaction> postedTxns) {
  auto is_valid_txn = [&](const auto &r) {
    return r.txnIndex > 0 && r.txnIndex <= postedTxns.size();
  };

  for (const auto &r : bundle.promotedTxns | std::views::filter(is_valid_txn)) {
    const auto &tx = postedTxns[r.txnIndex - 1];
    const auto promotedAt = time_ns::formatTimestamp(r.promotedAt);

    w.cell(r.id)
        .cell(exporter::common::renderKey(tx.source))
        .cell(std::string_view{"originator"})
        .cell(promotedAt)
        .cell(kSourceCases);
    w.endRow();

    w.cell(r.id)
        .cell(exporter::common::renderKey(tx.target))
        .cell(std::string_view{"beneficiary"})
        .cell(promotedAt)
        .cell(kSourceCases);
    w.endRow();
  }
}

void writeSignerOfRows(exporter::csv::Writer &w, const derived::Bundle &bundle,
                       time_ns::TimePoint simStart) {
  const auto validFrom = time_ns::formatTimestamp(simStart);
  for (const auto &b : bundle.businesses) {
    w.cell(exporter::common::renderCustomerId(b.owner))
        .cell(b.id)
        .cell(time_ns::formatTimestamp(b.effectiveDate))
        .cell(validFrom)
        .cellEmpty();
    w.endRow();
  }
}

void writeBeneficialOwnerOfRows(exporter::csv::Writer &w,
                                const derived::Bundle &bundle,
                                time_ns::TimePoint simStart) {
  const auto validFrom = time_ns::formatTimestamp(simStart);
  for (const auto &b : bundle.businesses) {
    w.cell(exporter::common::renderCustomerId(b.owner))
        .cell(b.id)
        .cell(1.0)
        .cell(time_ns::formatTimestamp(b.effectiveDate))
        .cell(validFrom)
        .cellEmpty();
    w.endRow();
  }
}

void writeControlsRows(exporter::csv::Writer &w, const derived::Bundle &bundle,
                       time_ns::TimePoint simStart) {
  const auto validFrom = time_ns::formatTimestamp(simStart);
  for (const auto &b : bundle.businesses) {
    w.cell(exporter::common::renderCustomerId(b.owner))
        .cell(b.id)
        .cell(std::string_view{"signing_officer"})
        .cell(time_ns::formatTimestamp(b.effectiveDate))
        .cell(validFrom)
        .cellEmpty();
    w.endRow();
  }
}

void writeBusinessOwnsAccountRows(exporter::csv::Writer &w,
                                  const derived::Bundle &bundle,
                                  time_ns::TimePoint simStart) {
  const auto validFrom = time_ns::formatTimestamp(simStart);
  for (const auto &b : bundle.businesses) {
    w.cell(b.id)
        .cell(exporter::common::renderKey(b.accountKey))
        .cell(time_ns::formatTimestamp(b.effectiveDate))
        .cell(validFrom)
        .cellEmpty();
    w.endRow();
  }
}

void writeHasNameRows(exporter::csv::Writer &w, const pipeline::People &people,
                      time_ns::TimePoint simStart) {
  const auto ts = time_ns::formatTimestamp(simStart);
  for (entity::PersonId p : allPersonIds(people)) {
    w.cell(exporter::common::renderCustomerId(p))
        .cell(exporter::aml::identity::nameIdForPerson(p))
        .cell(ts)
        .cell(kSourceKyc);
    w.endRow();
  }
}

void writeHasAddressRows(exporter::csv::Writer &w,
                         const pipeline::People &people,
                         time_ns::TimePoint simStart) {
  const auto ts = time_ns::formatTimestamp(simStart);
  for (entity::PersonId p : allPersonIds(people)) {
    w.cell(exporter::common::renderCustomerId(p))
        .cell(exporter::aml::identity::addressIdForPerson(p))
        .cell(ts)
        .cell(kSourceKyc);
    w.endRow();
  }
}

void writeHasEmailRows(exporter::csv::Writer &w, const pipeline::People &people,
                       time_ns::TimePoint simStart) {
  const auto ts = time_ns::formatTimestamp(simStart);
  for (entity::PersonId p : allPersonIds(people)) {
    const auto cid = exporter::common::renderCustomerId(p);
    w.cell(cid)
        .cell(derived::prefixedCustomerId("EML", cid))
        .cell(ts)
        .cell(kSourceKyc);
    w.endRow();
  }
}

void writeHasPhoneRows(exporter::csv::Writer &w, const pipeline::People &people,
                       time_ns::TimePoint simStart) {
  const auto ts = time_ns::formatTimestamp(simStart);
  for (entity::PersonId p : allPersonIds(people)) {
    const auto cid = exporter::common::renderCustomerId(p);
    w.cell(cid)
        .cell(derived::prefixedCustomerId("PH", cid))
        .cell(ts)
        .cell(kSourceKyc);
    w.endRow();
  }
}

void writeHasDobRows(exporter::csv::Writer &w, const pipeline::People &people) {
  for (entity::PersonId p : allPersonIds(people)) {
    const auto cid = exporter::common::renderCustomerId(p);
    w.cell(cid).cell(derived::prefixedCustomerId("DOB", cid)).cell(kSourceKyc);
    w.endRow();
  }
}

void writeHasIdRows(exporter::csv::Writer &w, const pipeline::People &people) {
  for (entity::PersonId p : allPersonIds(people)) {
    const auto cid = exporter::common::renderCustomerId(p);
    w.cell(cid)
        .cell(derived::prefixedCustomerId("GID", cid))
        .cell(kSourceKyc)
        .cell(0.95);
    w.endRow();
  }
}

void writeUsesDeviceRows(exporter::csv::Writer &w,
                         const synth::infra::devices::Output &devices) {
  struct Key {
    entity::PersonId person = 0;
    devices::Identity device{};
    constexpr bool operator==(const Key &) const noexcept = default;
  };
  struct KeyHash {
    std::size_t operator()(const Key &k) const noexcept {
      const auto a = static_cast<std::size_t>(k.person);
      const auto b = static_cast<std::size_t>(k.device.ownerId);
      const auto c = static_cast<std::size_t>(k.device.slot);
      const auto d = static_cast<std::size_t>(
          static_cast<std::uint8_t>(k.device.ownerType));
      return a ^ (b << 1) ^ (c << 3) ^ (d << 5);
    }
  };
  std::unordered_map<Key, exporter::common::SeenWindow, KeyHash> agg;
  agg.reserve(devices.usages.size());

  for (const auto &u : devices.usages) {
    exporter::common::recordSeen(agg[Key{u.personId, u.deviceId}], u.firstSeen,
                                 u.lastSeen);
  }

  for (const auto &[key, slot] : agg) {
    w.cell(exporter::common::renderCustomerId(key.person))
        .cell(exporter::common::renderDeviceId(key.device))
        .cell(time_ns::formatTimestamp(slot.firstSeen))
        .cell(time_ns::formatTimestamp(slot.lastSeen))
        .cell(kSourceCore)
        .cell(0.9);
    w.endRow();
  }
}

void writeUsesIpRows(exporter::csv::Writer &w,
                     const synth::infra::ips::Output &ips) {
  struct Key {
    entity::PersonId person = 0;
    network::Ipv4 ip{};
    constexpr bool operator==(const Key &) const noexcept = default;
  };
  struct KeyHash {
    std::size_t operator()(const Key &k) const noexcept {
      std::uint32_t raw = 0;
      std::memcpy(&raw, &k.ip, sizeof(std::uint32_t));
      return static_cast<std::size_t>(k.person) ^ static_cast<std::size_t>(raw);
    }
  };
  std::unordered_map<Key, exporter::common::SeenWindow, KeyHash> agg;
  agg.reserve(ips.usages.size());

  for (const auto &u : ips.usages) {
    exporter::common::recordSeen(agg[Key{u.personId, u.ipAddress}], u.firstSeen,
                                 u.lastSeen);
  }

  for (const auto &[key, slot] : agg) {
    w.cell(exporter::common::renderCustomerId(key.person))
        .cell(network::format(key.ip))
        .cell(time_ns::formatTimestamp(slot.firstSeen))
        .cell(time_ns::formatTimestamp(slot.lastSeen))
        .cell(kSourceCore)
        .cell(0.9);
    w.endRow();
  }
}

void writeInBucketRows(exporter::csv::Writer &w, const pipeline::People &people,
                       const aml::vertices::SharedContext &ctx,
                       time_ns::TimePoint simStart) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = people.pii;
  const auto ts = time_ns::formatTimestamp(simStart);

  std::string addrScratch;
  for (entity::PersonId p : allPersonIds(people)) {
    const auto cid = exporter::common::renderCustomerId(p);
    const auto nm = exporter::aml::identity::nameForPerson(p, pii, pools);
    const auto addr = exporter::aml::identity::addressForPerson(p, pii, pools);
    const auto fullAddr = exporter::common::joinAddress(addrScratch, addr);

    for (const auto &id :
         exporter::aml::minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      w.cell(cid).cell(id).cell(ts).cell(0.9);
      w.endRow();
    }
    for (const auto &id : exporter::aml::minhash::addressMinhashIds(fullAddr)) {
      w.cell(cid).cell(id).cell(ts).cell(0.85);
      w.endRow();
    }
    for (const auto &id :
         exporter::aml::minhash::streetMinhashIds(addr.streetLine1)) {
      w.cell(cid).cell(id).cell(ts).cell(0.8);
      w.endRow();
    }
    {
      const auto cityId = exporter::aml::minhash::cityMinhashId(addr.city);
      w.cell(cid).cell(std::string_view{cityId}).cell(ts).cell(0.7);
      w.endRow();
    }
    {
      const auto stateId = exporter::aml::minhash::stateMinhashId(addr.state);
      w.cell(cid).cell(std::string_view{stateId}).cell(ts).cell(0.6);
      w.endRow();
    }
  }
}

namespace {

void writeAggSuffixCells(exporter::csv::Writer &w,
                         const derived::Bundle &bundle,
                         const derived::AggregateRow &r) {
  const auto windowStart = bundle.simEnd - time_ns::Days{90};
  w.cell(primitives::utils::roundMoney(r.totalAmount))
      .cell(static_cast<std::int32_t>(r.txnCount))
      .cell(time_ns::formatTimestamp(time_ns::fromEpochSeconds(r.firstTs)))
      .cell(time_ns::formatTimestamp(time_ns::fromEpochSeconds(r.lastTs)))
      .cell(primitives::utils::roundMoney(r.amount30d))
      .cell(primitives::utils::roundMoney(r.amount90d))
      .cell(static_cast<std::int32_t>(r.count30d))
      .cell(static_cast<std::int32_t>(r.count90d))
      .cell(time_ns::formatTimestamp(windowStart))
      .cell(time_ns::formatTimestamp(bundle.simEnd))
      .cell(time_ns::formatTimestamp(bundle.simEnd))
      .cell(bundle.derivationRunId);
}

} // namespace

void writeAccountFlowAggRows(exporter::csv::Writer &w,
                             const derived::Bundle &bundle) {
  for (const auto &bucket : bundle.flowAgg) {
    w.cell(exporter::common::renderKey(bucket.pair.first))
        .cell(exporter::common::renderKey(bucket.pair.second));
    writeAggSuffixCells(w, bundle, bucket.row);
    w.endRow();
  }
}

void writeAccountLinkCommRows(exporter::csv::Writer &w,
                              const derived::Bundle &bundle) {
  for (const auto &bucket : bundle.linkComm) {
    const auto &r = bucket.row;
    const double weight = std::log1p(static_cast<double>(r.txnCount)) *
                          std::log1p(r.totalAmount / 1000.0);
    w.cell(exporter::common::renderKey(bucket.pair.first))
        .cell(exporter::common::renderKey(bucket.pair.second))
        .cell(primitives::utils::roundMoney(weight));
    writeAggSuffixCells(w, bundle, r);
    w.endRow();
  }
}

} // namespace PhantomLedger::exporter::aml_txn_edges::edges
