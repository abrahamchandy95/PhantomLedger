#include "phantomledger/exporter/aml_txn_edges/vertices.hpp"

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
#include "phantomledger/taxonomies/fraud/names.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"

#include <cassert>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace PhantomLedger::exporter::aml_txn_edges::vertices {

namespace clearing = ::PhantomLedger::clearing;
namespace entity = ::PhantomLedger::entity;
namespace exporter = ::PhantomLedger::exporter;
namespace locale = ::PhantomLedger::locale;
namespace network = ::PhantomLedger::network;
namespace personas = ::PhantomLedger::personas;
namespace pipe = ::PhantomLedger::pipeline;
namespace primitives = ::PhantomLedger::primitives;
namespace synth = ::PhantomLedger::synth;
namespace time_ns = ::PhantomLedger::time;
namespace txns = ::PhantomLedger::transactions;
namespace fr = ::PhantomLedger::fraud;

namespace {

[[nodiscard]] const ::PhantomLedger::entities::synth::pii::PoolSet &
poolsFor(const aml::vertices::SharedContext &ctx) noexcept {
  assert(ctx.pools != nullptr);
  return *ctx.pools;
}

[[nodiscard]] personas::Type
resolvedPersona(const aml::vertices::SharedContext &ctx,
                entity::PersonId p) noexcept {
  if (p == 0 || p > ctx.personaByPerson.size()) {
    return personas::Type::salaried;
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

[[nodiscard]] auto allPersonIds(const pipe::People &people) {
  return std::views::iota(1u, people.roster.roster.count + 1);
}

[[nodiscard]] std::size_t
estimateRowCapacity(const pipe::People &people,
                    const aml::vertices::SharedContext &ctx) noexcept {
  return static_cast<std::size_t>(people.roster.roster.count) +
         ctx.counterpartyIds.size() + 21U;
}

void writeGdsZeroCells(exporter::csv::Writer &w) {
  w.cell(0.0)
      .cell(static_cast<std::int32_t>(0))
      .cell(static_cast<std::int32_t>(0))
      .cell(static_cast<std::int32_t>(0))
      .cell(static_cast<std::int32_t>(0))
      .cell(static_cast<std::int32_t>(0))
      .cell(static_cast<std::int32_t>(0))
      .cell(0.0)
      .cell(0.0)
      .cell(static_cast<std::int32_t>(0))
      .cell(0.0)
      .cell(static_cast<std::int32_t>(0))
      .cell(static_cast<std::int32_t>(0))
      .cell(0.0);
}

} // namespace

void writeCustomerRows(exporter::csv::Writer &w, const pipe::People &people,
                       const aml::vertices::SharedContext &ctx,
                       time_ns::TimePoint simStart) {
  const auto &roster = people.roster.roster;
  for (entity::PersonId p : allPersonIds(people)) {
    const auto persona = resolvedPersona(ctx, p);
    const bool isFraud = roster.has(p, entity::person::Flag::fraud);
    const bool isMule = roster.has(p, entity::person::Flag::mule);
    const bool isVictim = roster.has(p, entity::person::Flag::victim);

    const auto cid = exporter::common::renderCustomerId(p);
    w.cell(cid)
        .cell(cid)
        .cell(exporter::aml::identity::customerType(persona))
        .cell(riskScoreFor(isFraud, isMule, isVictim))
        .cell(std::string_view{"active"})
        .cell(time_ns::formatTimestamp(
            exporter::aml::identity::onboardingDate(p, simStart)))
        .cell(locale::code(people.pii.at(p).country));
    w.endRow();
  }
}

void writeInternalAccountRows(exporter::csv::Writer &w,
                              const pipe::Holdings &holdings,
                              const clearing::Ledger *finalBook,
                              time_ns::TimePoint simStart) {
  const auto cardIds = exporter::common::collectCardIds(holdings.creditCards);

  auto is_internal = [](const auto &rec) {
    return (rec.flags &
            entity::account::bit(entity::account::Flag::external)) == 0;
  };

  for (const auto &rec :
       holdings.accounts.registry.records | std::views::filter(is_internal)) {
    const auto idStr = exporter::common::renderKey(rec.id);
    const auto branchBucket =
        static_cast<std::uint32_t>((rec.id.number % 50U) + 1U);
    const auto branch = derived::branchCodeForBucket(branchBucket);
    const auto openDate =
        (rec.owner == entity::invalidPerson)
            ? simStart - time_ns::Days{365}
            : exporter::aml::identity::onboardingDate(rec.owner, simStart);
    const double balance =
        (finalBook != nullptr)
            ? primitives::utils::roundMoney(finalBook->liquidity(rec.id))
            : 0.0;

    w.cell(idStr)
        .cell(idStr)
        .cell(exporter::common::accountType(rec.id, cardIds.count(rec.id) != 0))
        .cell(std::string_view{"active"})
        .cell(time_ns::formatTimestamp(openDate))
        .cell(branch)
        .cell(balance);
    writeGdsZeroCells(w);
    w.endRow();
  }
}

void writeExternalCounterpartyAccountRows(
    exporter::csv::Writer &w, const aml::vertices::SharedContext &ctx,
    time_ns::TimePoint simStart) {
  const auto externalOpen = simStart - time_ns::Days{365};
  const auto externalOpenStr = time_ns::formatTimestamp(externalOpen);
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

void writeCounterpartyRows(exporter::csv::Writer &w,
                           const aml::vertices::SharedContext &ctx) {
  const auto &usPool = poolsFor(ctx).forCountry(locale::Country::us);
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto bankId = exporter::aml::counterpartyBankId(cpId);
    const auto cpName =
        exporter::aml::identity::nameForCounterparty(cpId, usPool);
    const auto bankName = exporter::aml::identity::nameForBank(bankId);
    const auto vertexId = derived::makeCpId(cpId);
    w.cell(vertexId)
        .cell(vertexId)
        .cell(cpName.firstName)
        .cell(exporter::common::kUsCountry)
        .cell(bankName.firstName);
    w.endRow();
  }
}

void writeBankRows(exporter::csv::Writer &w,
                   const aml::vertices::SharedContext &ctx) {
  for (const auto &bankId : ctx.bankIds) {
    const auto name = exporter::aml::identity::nameForBank(bankId);
    const auto routing = exporter::aml::identity::routingNumberForId(bankId);
    w.cell(bankId)
        .cell(bankId)
        .cell(name.firstName)
        .cell(routing)
        .cell(exporter::common::kUsCountry);
    w.endRow();
  }
}

void writeDeviceRows(exporter::csv::Writer &w,
                     const synth::infra::devices::Output &devices) {
  std::unordered_map<::PhantomLedger::devices::Identity,
                     exporter::common::SeenWindow>
      windows;
  windows.reserve(devices.records.size());

  for (const auto &usage : devices.usages) {
    exporter::common::recordSeen(windows[usage.deviceId], usage.firstSeen,
                                 usage.lastSeen);
  }

  for (const auto &record : devices.records) {
    const auto idBuf = exporter::common::renderDeviceId(record.identity);
    const auto typeName = synth::infra::name(record.kind);
    const auto os = exporter::common::osForDeviceType(typeName);

    const auto it = windows.find(record.identity);
    const auto win =
        (it != windows.end()) ? it->second : exporter::common::SeenWindow{};

    w.cell(idBuf)
        .cell(idBuf)
        .cell(typeName)
        .cell(os)
        .cell(time_ns::formatTimestamp(win.firstSeen))
        .cell(time_ns::formatTimestamp(win.lastSeen));
    w.endRow();
  }
}

void writeIpRows(exporter::csv::Writer &w,
                 const synth::infra::ips::Output &ips) {
  for (const auto &rec : ips.records) {
    const auto addrBuf = network::format(rec.address);
    w.cell(addrBuf)
        .cell(addrBuf)
        .cell(exporter::common::kUsCountry)
        .cell(std::string_view{"Unknown"})
        .cell(false);
    w.endRow();
  }
}

void writeFullNameRows(exporter::csv::Writer &w, const pipe::People &people,
                       const aml::vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &usPool = poolsFor(ctx).forCountry(locale::Country::us);

  std::unordered_set<std::string> emitted;
  emitted.reserve(estimateRowCapacity(people, ctx));

  std::string nameScratch;

  const auto emitName = [&](const exporter::aml::identity::NameRecord &n) {
    if (!emitted.emplace(n.id.view()).second) {
      return;
    }
    const auto nameStr =
        exporter::common::joinName(nameScratch, n.firstName, n.lastName);
    w.cell(n.id).cell(nameStr);
    w.endRow();
  };

  for (entity::PersonId p : allPersonIds(people)) {
    emitName(exporter::aml::identity::nameForPerson(p, people.pii, pools));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emitName(exporter::aml::identity::nameForCounterparty(cpId, usPool));
  }
  for (const auto &bankId : ctx.bankIds) {
    emitName(exporter::aml::identity::nameForBank(bankId));
  }
}

void writeEmailRows(exporter::csv::Writer &w, const pipe::People &people) {
  for (entity::PersonId p : allPersonIds(people)) {
    const auto cid = exporter::common::renderCustomerId(p);
    w.cell(derived::prefixedCustomerId("EML", cid))
        .cell(people.pii.at(p).email.view());
    w.endRow();
  }
}

void writePhoneRows(exporter::csv::Writer &w, const pipe::People &people) {
  for (entity::PersonId p : allPersonIds(people)) {
    const auto cid = exporter::common::renderCustomerId(p);
    w.cell(derived::prefixedCustomerId("PH", cid)).cell(derived::phoneFor(p));
    w.endRow();
  }
}

void writeDobRows(exporter::csv::Writer &w, const pipe::People &people) {
  for (entity::PersonId p : allPersonIds(people)) {
    const auto cid = exporter::common::renderCustomerId(p);
    const auto &rec = people.pii.at(p);
    w.cell(derived::prefixedCustomerId("DOB", cid))
        .cell(derived::formatDob(rec.dob.year, rec.dob.month, rec.dob.day));
    w.endRow();
  }
}

void writeGovtIdRows(exporter::csv::Writer &w, const pipe::People &people) {
  for (entity::PersonId p : allPersonIds(people)) {
    const auto cid = exporter::common::renderCustomerId(p);
    const auto hash = derived::makeHashHex({"GID", cid.view()});
    w.cell(derived::prefixedCustomerId("GID", cid))
        .cell(std::string_view{"SSN"})
        .cell(hash);
    w.endRow();
  }
}

void writeAddressRows(exporter::csv::Writer &w, const pipe::People &people,
                      const aml::vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &usPool = poolsFor(ctx).forCountry(locale::Country::us);

  std::unordered_set<std::string> emitted;
  emitted.reserve(estimateRowCapacity(people, ctx));

  const auto emit = [&](const exporter::aml::identity::AddressRecord &a) {
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

  for (entity::PersonId p : allPersonIds(people)) {
    emit(exporter::aml::identity::addressForPerson(p, people.pii, pools));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emit(exporter::aml::identity::addressForCounterparty(cpId, usPool));
  }
  for (const auto &bankId : ctx.bankIds) {
    emit(exporter::aml::identity::addressForBank(bankId, usPool));
  }
}

void writeWatchlistRows(exporter::csv::Writer &w, const pipe::People &people,
                        time_ns::TimePoint simStart) {
  const auto effective = time_ns::formatTimestamp(simStart);
  const auto &roster = people.roster.roster;
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

    w.cell(wlView)
        .cell(std::string_view{"internal_fraud_list"})
        .cell(std::string_view{"fraud_suspect"})
        .cell(matchScore)
        .cell(effective);
    w.endRow();
  }
}

void writeAlertRows(exporter::csv::Writer &w, const derived::Bundle &bundle) {
  for (const auto &a : bundle.alerts) {
    w.cell(a.id)
        .cell(a.id)
        .cell(derived::ruleName(a.rule))
        .cell(time_ns::formatTimestamp(a.createdDate))
        .cell(a.status)
        .cell(derived::rulePriority(a.rule));
    w.endRow();
  }
}

void writeChainRows(exporter::csv::Writer &w,
                    std::span<const chains::ChainRow> chainRows) {
  for (const auto &row : chainRows) {
    w.cell(row.id).cell(row.chainId);

    if (row.ringId.has_value()) {
      w.cell(*row.ringId);
    } else {
      w.cellEmpty();
    }

    w.cell(fr::name(row.typology))
        .cell(row.numHops)
        .cell(primitives::utils::roundMoney(row.principal))
        .cell(primitives::utils::roundMoney(row.finalAmount))
        .cell(primitives::utils::roundMoney(row.totalHaircut))
        .cell(time_ns::formatTimestamp(time_ns::fromEpochSeconds(row.startTs)))
        .cell(time_ns::formatTimestamp(time_ns::fromEpochSeconds(row.endTs)))
        .cell(row.durationSeconds);
    w.endRow();
  }
}

void writeDispositionRows(exporter::csv::Writer &w,
                          const derived::Bundle &bundle) {
  for (const auto &d : bundle.dispositions) {
    w.cell(d.id)
        .cell(d.id)
        .cell(derived::dispositionAction(d.outcome))
        .cell(derived::dispositionCloseCode(d.outcome))
        .cell(derived::investigatorIdFor(d.investigatorNum))
        .cell(time_ns::formatTimestamp(d.date))
        .cell(d.notesHash);
    w.endRow();
  }
}

void writeSarRows(exporter::csv::Writer &w,
                  std::span<const exporter::aml::sar::SarRecord> sars) {
  for (const auto &s : sars) {
    w.cell(s.sarId)
        .cell(s.sarId)
        .cell(time_ns::formatTimestamp(s.filingDate))
        .cell(s.violationType)
        .cell(std::string_view{"filed"});
    w.endRow();
  }
}

void writeCtrRows(exporter::csv::Writer &w, const derived::Bundle &bundle) {
  for (const auto &c : bundle.ctrs) {
    w.cell(c.id)
        .cell(c.id)
        .cell(time_ns::formatTimestamp(c.filingDate))
        .cell(c.amount)
        .cell(derived::branchCodeForBucket(c.branchBucket))
        .cell(derived::tellerIdFor(c.tellerNum));
    w.endRow();
  }
}

void writeMinHashBucketRows(exporter::csv::Writer &w,
                            const pipe::People &people,
                            const aml::vertices::SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &usPool = poolsFor(ctx).forCountry(locale::Country::us);
  const auto &pii = people.pii;

  std::set<exporter::aml::minhash::BucketId> nameSet, addrSet, streetSet;
  std::unordered_set<std::string> citySet, stateSet;
  citySet.reserve(people.roster.roster.count);
  stateSet.reserve(64);

  std::string addrScratch;

  for (entity::PersonId p : allPersonIds(people)) {
    const auto nm = exporter::aml::identity::nameForPerson(p, pii, pools);
    const auto addr = exporter::aml::identity::addressForPerson(p, pii, pools);

    for (auto &id :
         exporter::aml::minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      nameSet.insert(std::move(id));
    }

    const auto fullAddr = exporter::common::joinAddress(addrScratch, addr);
    for (auto &id : exporter::aml::minhash::addressMinhashIds(fullAddr)) {
      addrSet.insert(std::move(id));
    }

    for (auto &id :
         exporter::aml::minhash::streetMinhashIds(addr.streetLine1)) {
      streetSet.insert(std::move(id));
    }
    citySet.insert(exporter::aml::minhash::cityMinhashId(addr.city));
    stateSet.insert(exporter::aml::minhash::stateMinhashId(addr.state));
  }

  for (const auto &cpId : ctx.counterpartyIds) {
    const auto nm = exporter::aml::identity::nameForCounterparty(cpId, usPool);
    for (auto &id :
         exporter::aml::minhash::nameMinhashIds(nm.firstName, nm.lastName)) {
      nameSet.insert(std::move(id));
    }
  }

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

void writeInvestigationCaseRows(exporter::csv::Writer &w,
                                const derived::Bundle &bundle) {
  for (const auto &c : bundle.cases) {
    const auto caseType = (c.kind == derived::CaseKind::ring)
                              ? std::string_view{"ring_investigation"}
                              : std::string_view{"solo_fraud_investigation"};
    w.cell(c.id)
        .cell(c.id)
        .cell(caseType)
        .cell(std::string_view{"open"})
        .cell(time_ns::formatTimestamp(c.openedDate))
        .cellEmpty()
        .cell(c.caseSystem);
    w.endRow();
  }
}

void writeEvidenceArtifactRows(exporter::csv::Writer &w,
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
        .cell(time_ns::formatTimestamp(e.createdAt))
        .cell(e.sourceSystem);
    w.endRow();
  }
}

void writeBusinessRows(exporter::csv::Writer &w,
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
        .cell(exporter::common::kUsCountry)
        .cell(derived::einFor(b.id.view()));
    w.endRow();
  }
}

void writeInvestigationCaseTxnRows(
    exporter::csv::Writer &w, const derived::Bundle &bundle,
    std::span<const txns::Transaction> postedTxns) {

  auto is_valid_txn = [&](const auto &r) {
    return r.txnIndex > 0 && r.txnIndex <= postedTxns.size();
  };

  for (const auto &r : bundle.promotedTxns | std::views::filter(is_valid_txn)) {
    const auto &tx = postedTxns[r.txnIndex - 1];
    const auto srcId = exporter::common::renderKey(tx.source);
    const auto dstId = exporter::common::renderKey(tx.target);
    const auto &caseRec = bundle.cases[r.caseIndex];

    w.cell(r.id)
        .cell(static_cast<std::uint32_t>(r.txnIndex))
        .cell(caseRec.id)
        .cell(srcId)
        .cell(dstId)
        .cell(time_ns::formatTimestamp(time_ns::fromEpochSeconds(tx.timestamp)))
        .cell(primitives::utils::roundMoney(tx.amount))
        .cell(std::string_view{"USD"})
        .cell(static_cast<std::int64_t>(tx.session.channel.value))
        .cell(exporter::common::kUsCountry)
        .cell(derived::isCreditChannel(tx.session.channel) ? 1 : 0)
        .cell(time_ns::formatTimestamp(r.promotedAt))
        .cell(time_ns::formatTimestamp(r.ttlDate));
    w.endRow();
  }
}

} // namespace PhantomLedger::exporter::aml_txn_edges::vertices
