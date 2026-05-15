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
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

namespace PhantomLedger::exporter::aml_txn_edges::edges {

namespace ent = ::PhantomLedger::entity;
namespace tx_ns = ::PhantomLedger::transactions;
namespace t_ns = ::PhantomLedger::time;
namespace pii_ns = ::PhantomLedger::entities::synth::pii;
namespace aml = ::PhantomLedger::exporter::aml;
namespace identity = ::PhantomLedger::exporter::aml::identity;
namespace minhash = ::PhantomLedger::exporter::aml::minhash;
namespace common = ::PhantomLedger::exporter::common;

namespace {

inline constexpr std::string_view kSourceCore = "core_banking";
inline constexpr std::string_view kSourceKyc = "kyc_system";
inline constexpr std::string_view kSourceAml = "aml_system";
inline constexpr std::string_view kSourceCases = "case_mgmt";

[[nodiscard]] const pii_ns::PoolSet &
poolsFor(const shared::SharedContext &ctx) noexcept {
  assert(ctx.pools != nullptr);
  return *ctx.pools;
}

} // namespace

// ──────────────────────────────────────────────────────────────────
// OWNS — Customer → Account
// ──────────────────────────────────────────────────────────────────

void writeOwnsRows(::PhantomLedger::exporter::csv::Writer &w,
                   const ::PhantomLedger::pipeline::Entities &entities,
                   t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  common::forEachInternalOwnership(entities,
                                   [&](ent::PersonId pid, const ent::Key &k) {
                                     w.cell(common::renderCustomerId(pid))
                                         .cell(common::renderKey(k))
                                         .cell(ts)
                                         .cell(kSourceCore)
                                         .cell(ts)
                                         .cellEmpty(); // valid_to — ongoing
                                     w.endRow();
                                   });
}

// ──────────────────────────────────────────────────────────────────
// TRANSACTED — Account → Account, with txn_id discriminator
// ──────────────────────────────────────────────────────────────────

void writeTransactedRows(::PhantomLedger::exporter::csv::Writer &w,
                         std::span<const tx_ns::Transaction> postedTxns) {
  std::size_t idx = 1;
  for (const auto &tx : postedTxns) {
    w.cell(common::renderKey(tx.source))
        .cell(common::renderKey(tx.target))
        .cell(static_cast<std::uint32_t>(idx))
        .cell(t_ns::formatTimestamp(t_ns::fromEpochSeconds(tx.timestamp)))
        .cell(primitives::utils::roundMoney(tx.amount))
        .cell(std::string_view{"USD"})
        .cell(static_cast<std::int64_t>(tx.session.channel.value))
        .cell(common::kUsCountry)
        .cell(derived::isCreditChannel(tx.session.channel) ? 1 : 0)
        .cell(kSourceCore);
    w.endRow();
    ++idx;
  }
}

void writeInvolvesCounterpartyRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const tx_ns::Transaction> postedTxns, t_ns::TimePoint simStart) {
  std::set<std::pair<ent::Key, ent::Key>> pairs;

  for (const auto &tx : postedTxns) {
    const bool srcExt = common::isExternalKey(tx.source);
    const bool dstExt = common::isExternalKey(tx.target);
    if (srcExt && !dstExt) {
      pairs.emplace(tx.target, tx.source); // (account, counterparty)
    } else if (!srcExt && dstExt) {
      pairs.emplace(tx.source, tx.target);
    }
  }

  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &[acct, cp] : pairs) {
    const auto cpRendered = common::renderKey(cp);
    w.cell(common::renderKey(acct))
        .cell(derived::makeCpId(cpRendered.view()))
        .cell(ts)
        .cell(kSourceCore)
        .cell(ts)
        .cellEmpty();
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// BANKS_AT — Counterparty → Bank
// ──────────────────────────────────────────────────────────────────

void writeBanksAtRows(::PhantomLedger::exporter::csv::Writer &w,
                      const shared::SharedContext &ctx,
                      t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto cpVertexId = derived::makeCpId(cpId);
    const auto bankId = aml::counterpartyBankId(cpId);
    w.cell(cpVertexId).cell(bankId).cell(kSourceCore).cell(ts).cellEmpty();
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// ON_WATCHLIST — Customer → Watchlist
// ──────────────────────────────────────────────────────────────────

void writeOnWatchlistRows(::PhantomLedger::exporter::csv::Writer &w,
                          const ::PhantomLedger::pipeline::Entities &entities,
                          t_ns::TimePoint simStart) {
  const auto &roster = entities.people.roster;
  const auto ts = t_ns::formatTimestamp(simStart);
  std::string watchlistId;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    if (!roster.has(p, ent::person::Flag::fraud) &&
        !roster.has(p, ent::person::Flag::mule)) {
      continue;
    }
    const auto cid = common::renderCustomerId(p);
    const auto cidView = cid.view();
    const auto wlView = common::makeWatchlistId(watchlistId, cidView);

    const double matchScore =
        0.5 +
        static_cast<double>(common::stableU64({"WL", cidView}) % 50ULL) / 100.0;

    w.cell(cid)
        .cell(wlView)
        .cell(ts)
        .cell(kSourceAml)
        .cell(matchScore)
        .cellEmpty() // evidence_ref
        .cell(ts)
        .cellEmpty(); // valid_to
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// SUBJECT_OF_SAR — Account → SAR  (one row per covered account)
// ──────────────────────────────────────────────────────────────────

void writeSubjectOfSarRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars) {
  for (const auto &sar : sars) {
    const auto filing = t_ns::formatTimestamp(sar.filingDate);
    for (const auto &acct : sar.coveredAccountIds) {
      w.cell(common::renderKey(acct))
          .cell(sar.sarId)
          .cell(filing)
          .cell(kSourceAml)
          .cellEmpty();
      w.endRow();
    }
  }
}

// ──────────────────────────────────────────────────────────────────
// FILED_CTR — Account → CTR
// ──────────────────────────────────────────────────────────────────

void writeFiledCtrRows(::PhantomLedger::exporter::csv::Writer &w,
                       const derived::Bundle &bundle) {
  for (const auto &c : bundle.ctrs) {
    w.cell(common::renderKey(c.onAccount))
        .cell(c.id)
        .cell(t_ns::formatTimestamp(c.filingDate))
        .cell(kSourceCore);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// ALERT_ON — Alert → Account
// ──────────────────────────────────────────────────────────────────

void writeAlertOnRows(::PhantomLedger::exporter::csv::Writer &w,
                      const derived::Bundle &bundle) {
  for (const auto &a : bundle.alerts) {
    w.cell(a.id)
        .cell(common::renderKey(a.onAccount))
        .cell(t_ns::formatTimestamp(a.createdDate))
        .cell(derived::ruleName(a.rule))
        .cell(derived::ruleSourceSystem(a.rule));
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// DISPOSITIONED_AS — Alert → Disposition
// ──────────────────────────────────────────────────────────────────

void writeDispositionedAsRows(::PhantomLedger::exporter::csv::Writer &w,
                              const derived::Bundle &bundle) {
  for (const auto &d : bundle.dispositions) {
    const auto &alert = bundle.alerts[d.alertIndex];
    w.cell(alert.id)
        .cell(d.id)
        .cell(t_ns::formatTimestamp(d.date))
        .cell(derived::investigatorIdFor(d.investigatorNum))
        .cell(d.notesHash) // rationale_hash
        .cellEmpty()       // evidence_refs
        .cell(kSourceCases)
        .cell(d.confidence);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// ESCALATED_TO — Disposition → SAR
// ──────────────────────────────────────────────────────────────────

void writeEscalatedToRows(
    ::PhantomLedger::exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars) {
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

  for (const auto &d : bundle.dispositions) {
    if (d.outcome != derived::DispositionOutcome::escalated) {
      continue;
    }
    const auto &linked = sarsByAlert[d.alertIndex];
    if (linked.empty()) {
      continue;
    }
    const auto escalationDate = t_ns::formatTimestamp(d.date);
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

// ──────────────────────────────────────────────────────────────────
// CONTAINS_ALERT — InvestigationCase → Alert
// ──────────────────────────────────────────────────────────────────

void writeContainsAlertRows(::PhantomLedger::exporter::csv::Writer &w,
                            const derived::Bundle &bundle) {
  for (const auto &c : bundle.cases) {
    const auto createdAt = t_ns::formatTimestamp(c.openedDate);
    for (const auto ai : c.alertIndices) {
      w.cell(c.id)
          .cell(bundle.alerts[ai].id)
          .cell(createdAt)
          .cell(kSourceCases);
      w.endRow();
    }
  }
}

// ──────────────────────────────────────────────────────────────────
// RESULTED_IN — InvestigationCase → SAR
// ──────────────────────────────────────────────────────────────────

void writeResultedInRows(
    ::PhantomLedger::exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars) {
  for (const auto &c : bundle.cases) {
    const auto createdAt = t_ns::formatTimestamp(c.openedDate);
    for (const auto si : c.sarIndices) {
      w.cell(c.id).cell(sars[si].sarId).cell(createdAt).cell(kSourceCases);
      w.endRow();
    }
  }
}

// ──────────────────────────────────────────────────────────────────
// HAS_EVIDENCE — InvestigationCase → EvidenceArtifact
// ──────────────────────────────────────────────────────────────────

void writeHasEvidenceRows(::PhantomLedger::exporter::csv::Writer &w,
                          const derived::Bundle &bundle) {
  for (const auto &c : bundle.cases) {
    const auto createdAt = t_ns::formatTimestamp(c.openedDate);
    for (const auto ei : c.evidenceIndices) {
      w.cell(c.id)
          .cell(bundle.evidence[ei].id)
          .cell(createdAt)
          .cell(kSourceCases);
      w.endRow();
    }
  }
}

// ──────────────────────────────────────────────────────────────────
// CONTAINS_PROMOTED_TXN — InvestigationCase → InvestigationCaseTxn
// ──────────────────────────────────────────────────────────────────

void writeContainsPromotedTxnRows(::PhantomLedger::exporter::csv::Writer &w,
                                  const derived::Bundle &bundle) {
  for (const auto &c : bundle.cases) {
    const auto createdAt = t_ns::formatTimestamp(c.openedDate);
    for (const auto pi : c.promotedTxnIndices) {
      w.cell(c.id)
          .cell(bundle.promotedTxns[pi].id)
          .cell(createdAt)
          .cell(kSourceCases);
      w.endRow();
    }
  }
}

// ──────────────────────────────────────────────────────────────────
// PROMOTED_TXN_ACCOUNT — InvestigationCaseTxn → Account (×2)
// ──────────────────────────────────────────────────────────────────

void writePromotedTxnAccountRows(
    ::PhantomLedger::exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const tx_ns::Transaction> postedTxns) {
  for (const auto &r : bundle.promotedTxns) {
    if (r.txnIndex == 0 || r.txnIndex > postedTxns.size()) {
      continue;
    }
    const auto &tx = postedTxns[r.txnIndex - 1];
    const auto promotedAt = t_ns::formatTimestamp(r.promotedAt);

    w.cell(r.id)
        .cell(common::renderKey(tx.source))
        .cell(std::string_view{"originator"})
        .cell(promotedAt)
        .cell(kSourceCases);
    w.endRow();

    w.cell(r.id)
        .cell(common::renderKey(tx.target))
        .cell(std::string_view{"beneficiary"})
        .cell(promotedAt)
        .cell(kSourceCases);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// SIGNER_OF / BENEFICIAL_OWNER_OF / CONTROLS / BUSINESS_OWNS_ACCOUNT
// ──────────────────────────────────────────────────────────────────

void writeSignerOfRows(::PhantomLedger::exporter::csv::Writer &w,
                       const derived::Bundle &bundle,
                       t_ns::TimePoint simStart) {
  const auto validFrom = t_ns::formatTimestamp(simStart);
  for (const auto &b : bundle.businesses) {
    w.cell(common::renderCustomerId(b.owner))
        .cell(b.id)
        .cell(t_ns::formatTimestamp(b.effectiveDate))
        .cell(validFrom)
        .cellEmpty();
    w.endRow();
  }
}

void writeBeneficialOwnerOfRows(::PhantomLedger::exporter::csv::Writer &w,
                                const derived::Bundle &bundle,
                                t_ns::TimePoint simStart) {
  const auto validFrom = t_ns::formatTimestamp(simStart);
  for (const auto &b : bundle.businesses) {
    w.cell(common::renderCustomerId(b.owner))
        .cell(b.id)
        .cell(1.0)
        .cell(t_ns::formatTimestamp(b.effectiveDate))
        .cell(validFrom)
        .cellEmpty();
    w.endRow();
  }
}

void writeControlsRows(::PhantomLedger::exporter::csv::Writer &w,
                       const derived::Bundle &bundle,
                       t_ns::TimePoint simStart) {
  const auto validFrom = t_ns::formatTimestamp(simStart);
  for (const auto &b : bundle.businesses) {
    w.cell(common::renderCustomerId(b.owner))
        .cell(b.id)
        .cell(std::string_view{"signing_officer"})
        .cell(t_ns::formatTimestamp(b.effectiveDate))
        .cell(validFrom)
        .cellEmpty();
    w.endRow();
  }
}

void writeBusinessOwnsAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                                  const derived::Bundle &bundle,
                                  t_ns::TimePoint simStart) {
  const auto validFrom = t_ns::formatTimestamp(simStart);
  for (const auto &b : bundle.businesses) {
    w.cell(b.id)
        .cell(common::renderKey(b.accountKey))
        .cell(t_ns::formatTimestamp(b.effectiveDate))
        .cell(validFrom)
        .cellEmpty();
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// HAS_NAME / HAS_ADDRESS / HAS_EMAIL / HAS_PHONE / HAS_DOB / HAS_ID
// ──────────────────────────────────────────────────────────────────

void writeHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    w.cell(common::renderCustomerId(p))
        .cell(identity::nameIdForPerson(p))
        .cell(ts)
        .cell(kSourceKyc);
    w.endRow();
  }
}

void writeHasAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                         const ::PhantomLedger::pipeline::Entities &entities,
                         t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    w.cell(common::renderCustomerId(p))
        .cell(identity::addressIdForPerson(p))
        .cell(ts)
        .cell(kSourceKyc);
    w.endRow();
  }
}

void writeHasEmailRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto cid = common::renderCustomerId(p);
    w.cell(cid)
        .cell(derived::prefixedCustomerId("EML", cid))
        .cell(ts)
        .cell(kSourceKyc);
    w.endRow();
  }
}

void writeHasPhoneRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       t_ns::TimePoint simStart) {
  const auto ts = t_ns::formatTimestamp(simStart);
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto cid = common::renderCustomerId(p);
    w.cell(cid)
        .cell(derived::prefixedCustomerId("PH", cid))
        .cell(ts)
        .cell(kSourceKyc);
    w.endRow();
  }
}

void writeHasDobRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::pipeline::Entities &entities) {
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto cid = common::renderCustomerId(p);
    w.cell(cid).cell(derived::prefixedCustomerId("DOB", cid)).cell(kSourceKyc);
    w.endRow();
  }
}

void writeHasIdRows(::PhantomLedger::exporter::csv::Writer &w,
                    const ::PhantomLedger::pipeline::Entities &entities) {
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto cid = common::renderCustomerId(p);
    w.cell(cid)
        .cell(derived::prefixedCustomerId("GID", cid))
        .cell(kSourceKyc)
        .cell(0.95);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// USES_DEVICE / USES_IP — per-(person, device|ip) aggregation
// ──────────────────────────────────────────────────────────────────

void writeUsesDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices) {
  struct Key {
    ent::PersonId person = 0;
    ::PhantomLedger::devices::Identity device{};
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
  std::unordered_map<Key, common::SeenWindow, KeyHash> agg;
  agg.reserve(devices.usages.size());

  for (const auto &u : devices.usages) {
    common::recordSeen(agg[Key{u.personId, u.deviceId}], u.firstSeen,
                       u.lastSeen);
  }

  for (const auto &[key, slot] : agg) {
    w.cell(common::renderCustomerId(key.person))
        .cell(common::renderDeviceId(key.device))
        .cell(t_ns::formatTimestamp(slot.firstSeen))
        .cell(t_ns::formatTimestamp(slot.lastSeen))
        .cell(kSourceCore)
        .cell(0.9);
    w.endRow();
  }
}

void writeUsesIpRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::infra::synth::ips::Output &ips) {
  struct Key {
    ent::PersonId person = 0;
    ::PhantomLedger::network::Ipv4 ip{};
    constexpr bool operator==(const Key &) const noexcept = default;
  };
  struct KeyHash {
    std::size_t operator()(const Key &k) const noexcept {
      std::uint32_t raw = 0;
      std::memcpy(&raw, &k.ip, sizeof(std::uint32_t));
      return static_cast<std::size_t>(k.person) ^ static_cast<std::size_t>(raw);
    }
  };
  std::unordered_map<Key, common::SeenWindow, KeyHash> agg;
  agg.reserve(ips.usages.size());

  for (const auto &u : ips.usages) {
    common::recordSeen(agg[Key{u.personId, u.ipAddress}], u.firstSeen,
                       u.lastSeen);
  }

  for (const auto &[key, slot] : agg) {
    w.cell(common::renderCustomerId(key.person))
        .cell(::PhantomLedger::network::format(key.ip))
        .cell(t_ns::formatTimestamp(slot.firstSeen))
        .cell(t_ns::formatTimestamp(slot.lastSeen))
        .cell(kSourceCore)
        .cell(0.9);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// IN_BUCKET — Customer → MinHashBucket (5 facets in one file)
// ──────────────────────────────────────────────────────────────────

void writeInBucketRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const shared::SharedContext &ctx,
                       t_ns::TimePoint simStart) {
  const auto &pools = poolsFor(ctx);
  const auto &pii = entities.pii;
  const auto ts = t_ns::formatTimestamp(simStart);

  std::string addrScratch;
  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto cid = common::renderCustomerId(p);
    const auto nm = identity::nameForPerson(p, pii, pools);
    const auto addr = identity::addressForPerson(p, pii, pools);
    const auto fullAddr = common::joinAddress(addrScratch, addr);

    for (const auto &id : minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      w.cell(cid).cell(id).cell(ts).cell(0.9);
      w.endRow();
    }
    for (const auto &id : minhash::addressMinhashIds(fullAddr)) {
      w.cell(cid).cell(id).cell(ts).cell(0.85);
      w.endRow();
    }
    for (const auto &id : minhash::streetMinhashIds(addr.streetLine1)) {
      w.cell(cid).cell(id).cell(ts).cell(0.8);
      w.endRow();
    }
    {
      const auto cityId = minhash::cityMinhashId(addr.city);
      w.cell(cid).cell(std::string_view{cityId}).cell(ts).cell(0.7);
      w.endRow();
    }
    {
      const auto stateId = minhash::stateMinhashId(addr.state);
      w.cell(cid).cell(std::string_view{stateId}).cell(ts).cell(0.6);
      w.endRow();
    }
  }
}

// ──────────────────────────────────────────────────────────────────
// ACCOUNT_FLOW_AGG / ACCOUNT_LINK_COMM — derived edge tables
// ──────────────────────────────────────────────────────────────────

namespace {

// Trailing 12 columns shared by both aggregate edges. Caller has already
// emitted from_id and to_id. LINK_COMM still emits its own `weight` column
// up front; this helper only handles the suffix that's identical between
// the two writers.
void writeAggSuffixCells(::PhantomLedger::exporter::csv::Writer &w,
                         const derived::Bundle &bundle,
                         const derived::AggregateRow &r) {
  const auto windowStart = bundle.simEnd - t_ns::Days{90};
  w.cell(primitives::utils::roundMoney(r.totalAmount))
      .cell(static_cast<std::int32_t>(r.txnCount))
      .cell(t_ns::formatTimestamp(t_ns::fromEpochSeconds(r.firstTs)))
      .cell(t_ns::formatTimestamp(t_ns::fromEpochSeconds(r.lastTs)))
      .cell(primitives::utils::roundMoney(r.amount30d))
      .cell(primitives::utils::roundMoney(r.amount90d))
      .cell(static_cast<std::int32_t>(r.count30d))
      .cell(static_cast<std::int32_t>(r.count90d))
      .cell(t_ns::formatTimestamp(windowStart))
      .cell(t_ns::formatTimestamp(bundle.simEnd))
      .cell(t_ns::formatTimestamp(bundle.simEnd))
      .cell(bundle.derivationRunId);
}

} // namespace

void writeAccountFlowAggRows(::PhantomLedger::exporter::csv::Writer &w,
                             const derived::Bundle &bundle) {
  for (const auto &bucket : bundle.flowAgg) {
    w.cell(common::renderKey(bucket.pair.first))
        .cell(common::renderKey(bucket.pair.second));
    writeAggSuffixCells(w, bundle, bucket.row);
    w.endRow();
  }
}

void writeAccountLinkCommRows(::PhantomLedger::exporter::csv::Writer &w,
                              const derived::Bundle &bundle) {
  for (const auto &bucket : bundle.linkComm) {
    const auto &r = bucket.row;
    const double weight = std::log1p(static_cast<double>(r.txnCount)) *
                          std::log1p(r.totalAmount / 1000.0);
    w.cell(common::renderKey(bucket.pair.first))
        .cell(common::renderKey(bucket.pair.second))
        .cell(primitives::utils::roundMoney(weight));
    writeAggSuffixCells(w, bundle, r);
    w.endRow();
  }
}

} // namespace PhantomLedger::exporter::aml_txn_edges::edges
