#include "phantomledger/exporter/aml_txn_edges/vertices.hpp"

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/aml/identity.hpp"
#include "phantomledger/exporter/aml/minhash.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/common/hashing.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"
#include "phantomledger/transactions/network/format.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace PhantomLedger::exporter::aml_txn_edges::vertices {

namespace ent = ::PhantomLedger::entity;
namespace t_ns = ::PhantomLedger::time;
namespace pii_ns = ::PhantomLedger::entities::synth::pii;
namespace loc = ::PhantomLedger::locale;
namespace aml = ::PhantomLedger::exporter::aml;
namespace identity = ::PhantomLedger::exporter::aml::identity;
namespace minhash = ::PhantomLedger::exporter::aml::minhash;
namespace common = ::PhantomLedger::exporter::common;

namespace {

inline constexpr auto kUsCountry = loc::code(loc::Country::us);

[[nodiscard]] double round2(double v) noexcept {
  return std::round(v * 100.0) / 100.0;
}

[[nodiscard]] ::PhantomLedger::encoding::RenderedKey
renderKey(const ent::Key &k) noexcept {
  return ::PhantomLedger::encoding::format(k);
}

[[nodiscard]] const pii_ns::PoolSet &
poolsFor(const shared::SharedContext &ctx) noexcept {
  assert(ctx.pools != nullptr);
  return *ctx.pools;
}

[[nodiscard]] const pii_ns::LocalePool &
usPoolFor(const shared::SharedContext &ctx) noexcept {
  return poolsFor(ctx).forCountry(loc::Country::us);
}

[[nodiscard]] ::PhantomLedger::personas::Type
resolvedPersona(const shared::SharedContext &ctx, ent::PersonId p) noexcept {
  if (p == 0 || p > ctx.personaByPerson.size()) {
    return ::PhantomLedger::personas::Type::salaried;
  }
  return ctx.personaByPerson[p - 1];
}

[[nodiscard]] double riskScoreFor(bool isFraud, bool isMule,
                                  bool isVictim) noexcept {
  if (isFraud) {
    return 0.9;
  }
  if (isMule) {
    return 0.7;
  }
  if (isVictim) {
    return 0.3;
  }
  return 0.1;
}

[[nodiscard]] std::string_view accountType(const ent::Key &id,
                                           bool isCard) noexcept {
  if (isCard || id.role == ent::Role::card) {
    return "credit";
  }
  if (id.role == ent::Role::business && id.bank == ent::Bank::internal) {
    return "business_checking";
  }
  if (id.role == ent::Role::brokerage && id.bank == ent::Bank::internal) {
    return "brokerage";
  }
  return "checking";
}

void writeGdsZeroCells(::PhantomLedger::exporter::csv::Writer &w) {
  w.cell(0.0)                             // pagerank_score
      .cell(static_cast<std::int32_t>(0)) // louvain_community_id
      .cell(static_cast<std::int32_t>(0)) // wcc_component_id
      .cell(static_cast<std::int32_t>(0)) // component_size
      .cell(static_cast<std::int32_t>(0)) // shortest_path_to_mule
      .cell(static_cast<std::int32_t>(0)) // ip_collision_count
      .cell(static_cast<std::int32_t>(0)) // device_collision_count
      .cell(0.0)                          // trans_in_mule_ratio
      .cell(0.0)                          // trans_out_mule_ratio
      .cell(static_cast<std::int32_t>(0)) // multi_hop_mule_count
      .cell(0.0)                          // betweenness
      .cell(static_cast<std::int32_t>(0)) // in_degree
      .cell(static_cast<std::int32_t>(0)) // out_degree
      .cell(0.0);                         // clustering_coeff
}

[[nodiscard]] std::unordered_set<ent::Key>
buildCardIdSet(const ent::card::Registry &cards) {
  std::unordered_set<ent::Key> out;
  out.reserve(cards.records.size());
  for (const auto &rec : cards.records) {
    out.insert(rec.key);
  }
  return out;
}

[[nodiscard]] t_ns::TimePoint openDateFor(const ent::account::Record &rec,
                                          t_ns::TimePoint simStart) {
  if (rec.owner == ent::invalidPerson) {
    return simStart - t_ns::Days{365};
  }
  return identity::onboardingDate(rec.owner, simStart);
}

[[nodiscard]] std::string_view osForDeviceType(std::string_view name) noexcept {
  if (name == "android") {
    return "Android";
  }
  if (name == "ios") {
    return "iOS";
  }
  if (name == "web" || name == "browser") {
    return "Browser";
  }
  if (name == "desktop") {
    return "Windows";
  }
  return "Unknown";
}

[[nodiscard]] std::size_t
estimateIdentityRowCount(const ::PhantomLedger::pipeline::Entities &entities,
                         const shared::SharedContext &ctx) noexcept {
  return entities.people.roster.count + ctx.counterpartyIds.size() + 21U;
}

} // namespace

// ──────────────────────────────────────────────────────────────────
// Customer
// ──────────────────────────────────────────────────────────────────

void writeCustomerRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const shared::SharedContext &ctx,
                       t_ns::TimePoint simStart) {
  const auto &roster = entities.people.roster;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    const auto persona = resolvedPersona(ctx, p);
    const bool isFraud = roster.has(p, ent::person::Flag::fraud);
    const bool isMule = roster.has(p, ent::person::Flag::mule);
    const bool isVictim = roster.has(p, ent::person::Flag::victim);

    const auto cid = common::renderCustomerId(p);
    w.cell(cid)
        .cell(cid)
        .cell(identity::customerType(persona))
        .cell(riskScoreFor(isFraud, isMule, isVictim))
        .cell(std::string_view{"active"})
        .cell(t_ns::formatTimestamp(identity::onboardingDate(p, simStart)))
        .cell(loc::code(entities.pii.at(p).country));
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// Account — internal accounts + synthetic external-counterparty rows
// ──────────────────────────────────────────────────────────────────

void writeAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const ::PhantomLedger::clearing::Ledger *finalBook,
                      const shared::SharedContext &ctx,
                      t_ns::TimePoint simStart) {
  const auto cardIds = buildCardIdSet(entities.creditCards);

  // 1. Internal accounts — real balances, branch derived from id.number.
  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      continue;
    }
    const auto idStr = renderKey(rec.id);
    const auto branchBucket =
        static_cast<std::uint32_t>((rec.id.number % 50U) + 1U);
    const auto branch = derived::branchCodeForBucket(branchBucket);
    const double balance =
        (finalBook != nullptr) ? round2(finalBook->liquidity(rec.id)) : 0.0;

    w.cell(idStr)
        .cell(idStr)
        .cell(accountType(rec.id, cardIds.count(rec.id) != 0))
        .cell(std::string_view{"active"})
        .cell(t_ns::formatTimestamp(openDateFor(rec, simStart)))
        .cell(branch)
        .cell(balance);
    writeGdsZeroCells(w);
    w.endRow();
  }

  // 2. Synthetic Account rows for each external counterparty — every
  //    TRANSACTED edge needs both endpoints to be Account vertices.
  const auto externalOpen = simStart - t_ns::Days{365};
  const auto externalOpenStr = t_ns::formatTimestamp(externalOpen);
  const auto externalBranch = derived::branchCodeForBucket(0);

  for (const auto &cpId : ctx.counterpartyIds) {
    w.cell(cpId)
        .cell(cpId)
        .cell(std::string_view{"external_counterparty"})
        .cell(std::string_view{"active"})
        .cell(externalOpenStr)
        .cell(externalBranch)
        .cell(0.0);
    writeGdsZeroCells(w);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// Counterparty
// ──────────────────────────────────────────────────────────────────

void writeCounterpartyRows(::PhantomLedger::exporter::csv::Writer &w,
                           const shared::SharedContext &ctx) {
  const auto &usPool = usPoolFor(ctx);
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto bankId = aml::counterpartyBankId(cpId);
    const auto cpName = identity::nameForCounterparty(cpId, usPool);
    const auto bankName = identity::nameForBank(bankId);
    const auto vertexId = derived::makeCpId(cpId);
    w.cell(vertexId)
        .cell(vertexId)
        .cell(cpName.firstName)
        .cell(kUsCountry)
        .cell(bankName.firstName);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// Bank
// ──────────────────────────────────────────────────────────────────

void writeBankRows(::PhantomLedger::exporter::csv::Writer &w,
                   const shared::SharedContext &ctx) {
  for (const auto &bankId : ctx.bankIds) {
    const auto name = identity::nameForBank(bankId);
    const auto routing = identity::routingNumberForId(bankId);
    w.cell(bankId)
        .cell(bankId)
        .cell(name.firstName)
        .cell(routing)
        .cell(kUsCountry);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// Device — first/last seen aggregated from usages
// ──────────────────────────────────────────────────────────────────

void writeDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices) {
  struct Window {
    t_ns::TimePoint firstSeen{};
    t_ns::TimePoint lastSeen{};
    bool initialized = false;
  };
  std::unordered_map<::PhantomLedger::devices::Identity, Window> windows;
  windows.reserve(devices.records.size());

  for (const auto &usage : devices.usages) {
    auto &slot = windows[usage.deviceId];
    if (!slot.initialized) {
      slot.firstSeen = usage.firstSeen;
      slot.lastSeen = usage.lastSeen;
      slot.initialized = true;
    } else {
      if (usage.firstSeen < slot.firstSeen) {
        slot.firstSeen = usage.firstSeen;
      }
      if (usage.lastSeen > slot.lastSeen) {
        slot.lastSeen = usage.lastSeen;
      }
    }
  }

  for (const auto &record : devices.records) {
    const auto idBuf = common::renderDeviceId(record.identity);
    const auto typeName = ::PhantomLedger::infra::synth::name(record.kind);
    const auto os = osForDeviceType(typeName);

    const auto it = windows.find(record.identity);
    const auto win = (it != windows.end()) ? it->second : Window{};

    w.cell(idBuf)
        .cell(idBuf)
        .cell(typeName)
        .cell(os)
        .cell(t_ns::formatTimestamp(win.firstSeen))
        .cell(t_ns::formatTimestamp(win.lastSeen));
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// IP
// ──────────────────────────────────────────────────────────────────

void writeIpRows(::PhantomLedger::exporter::csv::Writer &w,
                 const ::PhantomLedger::infra::synth::ips::Output &ips) {
  for (const auto &rec : ips.records) {
    const auto addrBuf = ::PhantomLedger::network::format(rec.address);
    w.cell(addrBuf)
        .cell(addrBuf)
        .cell(kUsCountry)
        .cell(std::string_view{"Unknown"})
        .cell(false);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// FullName — dedup across persons, counterparties, banks
// ──────────────────────────────────────────────────────────────────

void writeFullNameRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const shared::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &usPool = usPoolFor(ctx);

  std::unordered_set<std::string> emitted;
  emitted.reserve(estimateIdentityRowCount(entities, ctx));

  std::string nameScratch;

  const auto emitName = [&](const identity::NameRecord &n) {
    if (!emitted.emplace(n.id.view()).second) {
      return;
    }
    nameScratch.clear();
    nameScratch.reserve(n.firstName.size() + n.lastName.size() + 1);
    nameScratch.append(n.firstName);
    if (!n.firstName.empty() && !n.lastName.empty()) {
      nameScratch.push_back(' ');
    }
    nameScratch.append(n.lastName);
    w.cell(n.id).cell(nameScratch);
    w.endRow();
  };

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    emitName(identity::nameForPerson(p, entities.pii, pools));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emitName(identity::nameForCounterparty(cpId, usPool));
  }
  for (const auto &bankId : ctx.bankIds) {
    emitName(identity::nameForBank(bankId));
  }
}

// ──────────────────────────────────────────────────────────────────
// Email / Phone / DOB / GovtID — one per PersonId
// ──────────────────────────────────────────────────────────────────

void writeEmailRows(::PhantomLedger::exporter::csv::Writer &w,
                    const ::PhantomLedger::pipeline::Entities &entities) {
  const auto &roster = entities.people.roster;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    const auto cid = common::renderCustomerId(p);
    w.cell(derived::prefixedCustomerId("EML", cid))
        .cell(entities.pii.at(p).email.view());
    w.endRow();
  }
}

void writePhoneRows(::PhantomLedger::exporter::csv::Writer &w,
                    const ::PhantomLedger::pipeline::Entities &entities) {
  const auto &roster = entities.people.roster;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    const auto cid = common::renderCustomerId(p);
    w.cell(derived::prefixedCustomerId("PH", cid)).cell(derived::phoneFor(p));
    w.endRow();
  }
}

void writeDobRows(::PhantomLedger::exporter::csv::Writer &w,
                  const ::PhantomLedger::pipeline::Entities &entities) {
  const auto &roster = entities.people.roster;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    const auto cid = common::renderCustomerId(p);
    const auto &rec = entities.pii.at(p);
    w.cell(derived::prefixedCustomerId("DOB", cid))
        .cell(derived::formatDob(rec.dob.year, rec.dob.month, rec.dob.day));
    w.endRow();
  }
}

void writeGovtIdRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::pipeline::Entities &entities) {
  const auto &roster = entities.people.roster;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    const auto cid = common::renderCustomerId(p);
    const auto hash = derived::makeHashHex({"GID", cid.view()});
    w.cell(derived::prefixedCustomerId("GID", cid))
        .cell(std::string_view{"SSN"})
        .cell(hash);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// Address — dedup across persons/counterparties/banks
// ──────────────────────────────────────────────────────────────────

void writeAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const shared::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &usPool = usPoolFor(ctx);

  std::unordered_set<std::string> emitted;
  emitted.reserve(estimateIdentityRowCount(entities, ctx));

  const auto emit = [&](const identity::AddressRecord &a) {
    if (!emitted.emplace(a.id.view()).second) {
      return;
    }
    w.cell(a.id)
        .cell(a.streetLine1)
        .cell(a.city)
        .cell(a.state)
        .cell(a.postalCode)
        .cell(a.country);
    w.endRow();
  };

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    emit(identity::addressForPerson(p, entities.pii, pools));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emit(identity::addressForCounterparty(cpId, usPool));
  }
  for (const auto &bankId : ctx.bankIds) {
    emit(identity::addressForBank(bankId, usPool));
  }
}

// ──────────────────────────────────────────────────────────────────
// Watchlist — one entry per fraud/mule person
// ──────────────────────────────────────────────────────────────────

void writeWatchlistRows(::PhantomLedger::exporter::csv::Writer &w,
                        const ::PhantomLedger::pipeline::Entities &entities,
                        t_ns::TimePoint simStart) {
  const auto effective = t_ns::formatTimestamp(simStart);
  const auto &roster = entities.people.roster;
  std::string watchlistId;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    if (!roster.has(p, ent::person::Flag::fraud) &&
        !roster.has(p, ent::person::Flag::mule)) {
      continue;
    }
    const auto cid = common::renderCustomerId(p);
    const auto cidView = cid.view();
    watchlistId.clear();
    watchlistId.reserve(3 + cidView.size());
    watchlistId.append("WL_").append(cidView);

    const double matchScore =
        0.5 +
        static_cast<double>(common::stableU64({"WL", cidView}) % 50ULL) / 100.0;

    w.cell(watchlistId)
        .cell(std::string_view{"internal_fraud_list"})
        .cell(std::string_view{"fraud_suspect"})
        .cell(matchScore)
        .cell(effective);
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// Alert / Disposition / SAR / CTR — bundle-driven
// ──────────────────────────────────────────────────────────────────

void writeAlertRows(::PhantomLedger::exporter::csv::Writer &w,
                    const derived::Bundle &bundle) {
  for (const auto &a : bundle.alerts) {
    w.cell(a.id)
        .cell(a.id)
        .cell(derived::ruleName(a.rule))
        .cell(t_ns::formatTimestamp(a.createdDate))
        .cell(a.status)
        .cell(derived::rulePriority(a.rule));
    w.endRow();
  }
}

void writeDispositionRows(::PhantomLedger::exporter::csv::Writer &w,
                          const derived::Bundle &bundle) {
  for (const auto &d : bundle.dispositions) {
    w.cell(d.id)
        .cell(d.id)
        .cell(derived::dispositionAction(d.outcome))
        .cell(derived::dispositionCloseCode(d.outcome))
        .cell(derived::investigatorIdFor(d.investigatorNum))
        .cell(t_ns::formatTimestamp(d.date))
        .cell(d.notesHash);
    w.endRow();
  }
}

void writeSarRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars) {
  for (const auto &s : sars) {
    w.cell(s.sarId)
        .cell(s.sarId)
        .cell(t_ns::formatTimestamp(s.filingDate))
        .cell(s.violationType)
        .cell(std::string_view{"filed"});
    w.endRow();
  }
}

void writeCtrRows(::PhantomLedger::exporter::csv::Writer &w,
                  const derived::Bundle &bundle) {
  for (const auto &c : bundle.ctrs) {
    w.cell(c.id)
        .cell(c.id)
        .cell(t_ns::formatTimestamp(c.filingDate))
        .cell(c.amount)
        .cell(derived::branchCodeForBucket(c.branchBucket))
        .cell(derived::tellerIdFor(c.tellerNum));
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// MinHashBucket — collapsed across all 5 facets
// ──────────────────────────────────────────────────────────────────

void writeMinHashBucketRows(::PhantomLedger::exporter::csv::Writer &w,
                            const ::PhantomLedger::pipeline::Entities &entities,
                            const shared::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &usPool = usPoolFor(ctx);
  const auto &pii = entities.pii;

  // BucketId is RenderedId<24>, which has operator<=> but no std::hash
  // specialization. std::set works via the defaulted spaceship; this matches
  // the dedup approach in src/exporter/aml/edges.cpp's MinhashVertexSets.
  std::set<minhash::BucketId> nameSet, addrSet, streetSet;
  std::unordered_set<std::string> citySet, stateSet;

  // std::set has no reserve(); only the unordered city/state sets benefit.
  citySet.reserve(entities.people.roster.count);
  stateSet.reserve(64);

  std::string addrScratch;

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    const auto nm = identity::nameForPerson(p, pii, pools);
    const auto addr = identity::addressForPerson(p, pii, pools);

    for (auto &id : minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      nameSet.insert(std::move(id));
    }

    addrScratch.clear();
    addrScratch.reserve(addr.streetLine1.size() + addr.city.size() +
                        addr.state.size() + addr.postalCode.size() + 4);
    addrScratch.append(addr.streetLine1);
    addrScratch.push_back(' ');
    addrScratch.append(addr.city);
    addrScratch.push_back(' ');
    addrScratch.append(addr.state);
    addrScratch.push_back(' ');
    addrScratch.append(addr.postalCode);
    for (auto &id : minhash::addressMinhashIds(addrScratch)) {
      addrSet.insert(std::move(id));
    }

    for (auto &id : minhash::streetMinhashIds(addr.streetLine1)) {
      streetSet.insert(std::move(id));
    }
    citySet.insert(minhash::cityMinhashId(addr.city));
    stateSet.insert(minhash::stateMinhashId(addr.state));
  }

  for (const auto &cpId : ctx.counterpartyIds) {
    const auto nm = identity::nameForCounterparty(cpId, usPool);
    for (auto &id : minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      nameSet.insert(std::move(id));
    }
  }

  // hash_band: 0=name, 1=address, 2=street, 3=city, 4=state.
  for (const auto &id : nameSet) {
    w.cell(id).cell(id).cell(static_cast<std::int32_t>(0));
    w.endRow();
  }
  for (const auto &id : addrSet) {
    w.cell(id).cell(id).cell(static_cast<std::int32_t>(1));
    w.endRow();
  }
  for (const auto &id : streetSet) {
    w.cell(id).cell(id).cell(static_cast<std::int32_t>(2));
    w.endRow();
  }
  for (const auto &id : citySet) {
    w.cell(std::string_view{id})
        .cell(std::string_view{id})
        .cell(static_cast<std::int32_t>(3));
    w.endRow();
  }
  for (const auto &id : stateSet) {
    w.cell(std::string_view{id})
        .cell(std::string_view{id})
        .cell(static_cast<std::int32_t>(4));
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────
// InvestigationCase / EvidenceArtifact / Business / PromotedTxn
// ──────────────────────────────────────────────────────────────────

void writeInvestigationCaseRows(::PhantomLedger::exporter::csv::Writer &w,
                                const derived::Bundle &bundle) {
  for (const auto &c : bundle.cases) {
    const auto caseType = (c.kind == derived::CaseKind::ring)
                              ? std::string_view{"ring_investigation"}
                              : std::string_view{"solo_fraud_investigation"};
    w.cell(c.id)
        .cell(c.id)
        .cell(caseType)
        .cell(std::string_view{"open"})
        .cell(t_ns::formatTimestamp(c.openedDate))
        .cellEmpty()
        .cell(c.caseSystem);
    w.endRow();
  }
}

void writeEvidenceArtifactRows(::PhantomLedger::exporter::csv::Writer &w,
                               const derived::Bundle &bundle) {
  std::string storeUri;
  for (const auto &e : bundle.evidence) {
    const auto idView = e.id.view();
    storeUri.clear();
    storeUri.reserve(17 + idView.size());
    storeUri.append("s3://aml-evidence/").append(idView);
    w.cell(e.id)
        .cell(e.id)
        .cell(e.artifactType)
        .cell(e.contentHash)
        .cell(storeUri)
        .cell(t_ns::formatTimestamp(e.createdAt))
        .cell(e.sourceSystem);
    w.endRow();
  }
}

void writeBusinessRows(::PhantomLedger::exporter::csv::Writer &w,
                       const derived::Bundle &bundle) {
  std::string legalName;
  for (const auto &b : bundle.businesses) {
    const auto stem = derived::kLegalStems[b.stemIdx];
    legalName.clear();
    legalName.reserve(stem.size() + b.entityType.size() + 16);
    legalName.append(stem).append(" Holdings #");
    char num[4];
    num[0] = static_cast<char>('0' + ((b.numberSuffix / 100U) % 10U));
    num[1] = static_cast<char>('0' + ((b.numberSuffix / 10U) % 10U));
    num[2] = static_cast<char>('0' + (b.numberSuffix % 10U));
    num[3] = ' ';
    legalName.append(num, num + 4);
    legalName.append(b.entityType);

    w.cell(b.id)
        .cell(b.id)
        .cell(legalName)
        .cell(b.entityType)
        .cell(kUsCountry)
        .cell(derived::einFor(b.id.view()));
    w.endRow();
  }
}

void writeInvestigationCaseTxnRows(
    ::PhantomLedger::exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const ::PhantomLedger::transactions::Transaction> postedTxns) {
  for (const auto &r : bundle.promotedTxns) {
    if (r.txnIndex == 0 || r.txnIndex > postedTxns.size()) {
      continue;
    }
    const auto &tx = postedTxns[r.txnIndex - 1];
    const auto srcId = renderKey(tx.source);
    const auto dstId = renderKey(tx.target);
    const auto &caseRec = bundle.cases[r.caseIndex];

    w.cell(r.id)
        .cell(static_cast<std::uint32_t>(r.txnIndex))
        .cell(caseRec.id)
        .cell(srcId)
        .cell(dstId)
        .cell(t_ns::formatTimestamp(t_ns::fromEpochSeconds(tx.timestamp)))
        .cell(round2(tx.amount))
        .cell(std::string_view{"USD"})
        .cell(static_cast<std::int64_t>(tx.session.channel.value))
        .cell(kUsCountry)
        .cell(derived::isCreditChannel(tx.session.channel) ? 1 : 0)
        .cell(t_ns::formatTimestamp(r.promotedAt))
        .cell(t_ns::formatTimestamp(r.ttlDate));
    w.endRow();
  }
}

} // namespace PhantomLedger::exporter::aml_txn_edges::vertices
