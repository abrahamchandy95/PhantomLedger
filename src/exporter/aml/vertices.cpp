#include "phantomledger/exporter/aml/vertices.hpp"

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/exporter/aml/identity.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/common/support.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/names.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"
#include "phantomledger/taxonomies/lookup.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace PhantomLedger::exporter::aml::vertices {

namespace {

namespace channels = ::PhantomLedger::channels;
namespace devices = ::PhantomLedger::devices;
namespace entity = ::PhantomLedger::entity;
namespace exporter = ::PhantomLedger::exporter;
namespace locale = ::PhantomLedger::locale;
namespace lookup = ::PhantomLedger::lookup;
namespace personas = ::PhantomLedger::personas;
namespace pipe = ::PhantomLedger::pipeline;
namespace primitives = ::PhantomLedger::primitives;
namespace synth = ::PhantomLedger::synth;
namespace taxonomies = ::PhantomLedger::taxonomies;
namespace time_ns = ::PhantomLedger::time;
namespace txns = ::PhantomLedger::transactions;
;

using BranchCode = ::PhantomLedger::encoding::RenderedId<8>;

auto allPersonIds(const pipeline::Entities &entities) {
  return std::views::iota(1u, entities.people.roster.count + 1);
}

[[nodiscard]] BranchCode branchCodeFor(const entity::Key &id) noexcept {
  const auto bucket = (id.number % 50U) + 1U;
  BranchCode out;
  const auto n = std::snprintf(out.bytes.data(), out.bytes.size(), "BR%03u",
                               static_cast<unsigned>(bucket));
  out.length = static_cast<std::uint8_t>(n > 0 ? n : 0);
  return out;
}

[[nodiscard]] ::PhantomLedger::personas::Type
resolvedPersona(const SharedContext &ctx, entity::PersonId p) noexcept {
  if (p == 0 || p > ctx.personaByPerson.size()) {
    return personas::Type::salaried;
  }
  return ctx.personaByPerson[p - 1];
}

[[nodiscard]] const ::PhantomLedger::entities::synth::pii::PoolSet &
poolsFor(const SharedContext &ctx) noexcept {
  assert(ctx.pools != nullptr &&
         "SharedContext::pools is null — was the context built with "
         "buildSharedContext(entities, txns, pools)?");
  return *ctx.pools;
}

[[nodiscard]] const ::PhantomLedger::entities::synth::pii::LocalePool &
usPoolFor(const SharedContext &ctx) noexcept {
  return poolsFor(ctx).forCountry(locale::Country::us);
}

} // namespace

// SharedContext build
SharedContext buildSharedContext(
    const pipe::Entities &entities,
    std::span<const txns::Transaction> finalTxns,
    const ::PhantomLedger::entities::synth::pii::PoolSet &pools) {
  SharedContext ctx;
  ctx.pools = &pools;

  auto is_external = [](const auto &rec) {
    return (rec.flags &
            entity::account::bit(entity::account::Flag::external)) != 0;
  };

  for (const auto &rec :
       entities.accounts.registry.records | std::views::filter(is_external)) {
    ctx.counterpartyIds.emplace(exporter::common::renderKey(rec.id));
  }

  ctx.bankIds = allBankIds(ctx.counterpartyIds);

  ctx.personaByPerson = entities.personas.assignment.byPerson;

  ctx.lastTransactionByAccount.reserve(finalTxns.size() / 2);
  const auto bump = [&](const entity::Key &account, std::int64_t ts) {
    auto &slot = ctx.lastTransactionByAccount[account];
    if (ts > slot) {
      slot = ts;
    }
  };
  for (const auto &tx : finalTxns) {
    bump(tx.source, tx.timestamp);
    bump(tx.target, tx.timestamp);
  }

  return ctx;
}

// Customer rows
namespace {

inline void writeCustomerIdCells(exporter::csv::Writer &w,
                                 const entity::Key &customerKey,
                                 personas::Type persona) {
  w.cell(::PhantomLedger::encoding::format(customerKey).view())
      .cell(identity::customerType(persona))
      .cell(std::string_view{"active"});
}

inline void writeDemographicCells(exporter::csv::Writer &w, entity::PersonId p,
                                  personas::Type persona) {
  w.cell(identity::maritalStatus(p, persona))
      .cell(identity::networthCode(persona))
      .cell(identity::incomeCode(persona))
      .cell(identity::occupation(p, persona));
}

inline void writeRiskAndOriginCells(exporter::csv::Writer &w,
                                    std::string_view riskRating,
                                    std::string_view originCountry,
                                    time_ns::TimePoint onboardingDate) {
  w.cell(riskRating)
      .cell(originCountry)
      .cell(time_ns::formatTimestamp(onboardingDate));
}

} // namespace

void writeCustomerRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                       const SharedContext &ctx, time_ns::TimePoint simStart) {
  const auto &peopleRoster = entities.people.roster;
  for (entity::PersonId p : allPersonIds(entities)) {
    const auto persona = resolvedPersona(ctx, p);
    const bool isFraud = peopleRoster.has(p, entity::person::Flag::fraud);
    const bool isMule = peopleRoster.has(p, entity::person::Flag::mule);
    const bool isVictim = peopleRoster.has(p, entity::person::Flag::victim);

    const auto customerKey =
        entity::makeKey(entity::Role::customer, entity::Bank::internal, p);

    const auto originCountry = locale::code(entities.pii.at(p).country);

    writeCustomerIdCells(w, customerKey, persona);
    writeDemographicCells(w, p, persona);
    writeRiskAndOriginCells(w, identity::riskRating(isFraud, isMule, isVictim),
                            originCountry,
                            identity::onboardingDate(p, simStart));
    w.endRow();
  }
}

// Account rows
namespace {

[[nodiscard]] std::string lastTxnString(const entity::Key &acct,
                                        const SharedContext &ctx) {
  const auto it = ctx.lastTransactionByAccount.find(acct);
  if (it == ctx.lastTransactionByAccount.end()) {
    return {};
  }
  return time_ns::formatTimestamp(time_ns::fromEpochSeconds(it->second));
}

} // namespace

void writeAccountRows(exporter::csv::Writer &w,
                      const pipeline::Entities &entities,
                      const clearing::Ledger *finalBook,
                      const SharedContext &ctx, time_ns::TimePoint simStart) {
  const auto cardIds = exporter::common::collectCardIds(entities.creditCards);

  auto is_internal = [](const auto &rec) {
    return (rec.flags &
            entity::account::bit(entity::account::Flag::external)) == 0;
  };

  for (const auto &rec :
       entities.accounts.registry.records | std::views::filter(is_internal)) {
    const auto idStr = exporter::common::renderKey(rec.id);
    const auto openDate = (rec.owner == entity::invalidPerson)
                              ? simStart - time_ns::Days{365}
                              : identity::onboardingDate(rec.owner, simStart);
    const auto lastTxnStr = lastTxnString(rec.id, ctx);
    const auto acctType =
        exporter::common::accountType(rec.id, cardIds.count(rec.id) != 0);
    const auto branch = branchCodeFor(rec.id);

    const double balance =
        (finalBook != nullptr)
            ? primitives::utils::roundMoney(finalBook->liquidity(rec.id))
            : 0.0;

    w.writeRow(idStr, idStr, balance, time_ns::formatTimestamp(openDate),
               std::string_view{"active"}, lastTxnStr, std::string_view{""},
               acctType, std::string_view{"USD"}, branch);
  }
}

// Counterparty rows
void writeCounterpartyRows(exporter::csv::Writer &w, const SharedContext &ctx) {
  const auto &usPool = usPoolFor(ctx);
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto bankId = counterpartyBankId(cpId);
    const auto cpName = identity::nameForCounterparty(cpId, usPool);
    const auto bankName = identity::nameForBank(bankId);
    w.writeRow(cpId, cpName.firstName, identity::routingNumberForId(bankId),
               bankName.firstName, exporter::common::kUsCountry);
  }
}

// Name / Address rows
namespace {

void writeAddressRow(exporter::csv::Writer &w,
                     const identity::AddressRecord &a) {
  w.writeRow(a.id, a.streetLine1, a.streetLine2, a.city, a.state, a.postalCode,
             a.country, a.addressType,
             static_cast<std::uint8_t>(a.isHighRiskGeo));
}

} // namespace

void writeNameRows(exporter::csv::Writer &w, const pipe::Entities &entities,
                   const SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &usPool = pools.forCountry(locale::Country::us);

  std::unordered_set<std::string> emitted;
  emitted.reserve(exporter::common::estimateIdentityRowCount(entities, ctx));

  const auto emitName = [&](const identity::NameRecord &n) {
    if (emitted.emplace(n.id.view()).second) {
      w.writeRow(n.id, n.firstName, n.middleName, n.lastName);
    }
  };

  for (entity::PersonId p : allPersonIds(entities)) {
    emitName(identity::nameForPerson(p, entities.pii, pools));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emitName(identity::nameForCounterparty(cpId, usPool));
  }
  for (const auto &bankId : ctx.bankIds) {
    emitName(identity::nameForBank(bankId));
  }
}

void writeAddressRows(exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &usPool = pools.forCountry(locale::Country::us);

  std::unordered_set<std::string> emitted;
  emitted.reserve(exporter::common::estimateIdentityRowCount(entities, ctx));

  const auto emitAddr = [&](const identity::AddressRecord &a) {
    if (emitted.emplace(a.id.view()).second) {
      writeAddressRow(w, a);
    }
  };

  for (entity::PersonId p : allPersonIds(entities)) {
    emitAddr(identity::addressForPerson(p, entities.pii, pools));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emitAddr(identity::addressForCounterparty(cpId, usPool));
  }
  for (const auto &bankId : ctx.bankIds) {
    emitAddr(identity::addressForBank(bankId, usPool));
  }
}

namespace {

struct CountryRow {
  std::string_view displayName;
  double baseRiskScore;
};

inline constexpr auto kCountryRows = std::to_array<CountryRow>({
    {"United States", 0.10},  // us
    {"United Kingdom", 0.10}, // gb
    {"Canada", 0.10},         // ca
    {"Australia", 0.10},      // au
    {"Germany", 0.10},        // de
    {"France", 0.10},         // fr
    {"Spain", 0.15},          // es
    {"Italy", 0.15},          // it
    {"Netherlands", 0.10},    // nl
    {"Brazil", 0.30},         // br
    {"Mexico", 0.30},         // mx
    {"India", 0.20},          // in
    {"Japan", 0.10},          // jp
    {"China", 0.35},          // cn
    {"South Korea", 0.15},    // kr
    {"Russia", 0.60},         // ru
});

static_assert(kCountryRows.size() == locale::kCountryCount,
              "kCountryRows must match locale::kCountries 1:1");

} // namespace

void writeCountryRows(exporter::csv::Writer &w,
                      const pipe::Entities &entities) {
  namespace enumTax = taxonomies::enums;

  std::array<bool, locale::kCountryCount> seen{};
  seen[enumTax::toIndex(locale::Country::us)] = true;
  for (const auto &rec : entities.pii.records) {
    seen[enumTax::toIndex(rec.country)] = true;
  }

  for (std::size_t i = 0; i < locale::kCountryCount; ++i) {
    if (!seen[i]) {
      continue;
    }
    const auto country = locale::kCountries[i];
    const auto &row = kCountryRows[i];
    w.writeRow(locale::code(country), row.displayName, row.baseRiskScore);
  }
}

void writeBankRows(exporter::csv::Writer &w, const SharedContext &ctx) {
  for (const auto &bankId : ctx.bankIds) {
    w.writeRow(bankId);
  }
}

void writeWatchlistRows(exporter::csv::Writer &w,
                        const pipe::Entities &entities,
                        time_ns::TimePoint simStart) {
  const auto entryDate = time_ns::formatTimestamp(simStart);
  const auto &roster = entities.people.roster;
  std::string watchlistId;
  auto is_on_watchlist = [&](entity::PersonId p) {
    return roster.has(p, entity::person::Flag::fraud) ||
           roster.has(p, entity::person::Flag::mule);
  };

  for (entity::PersonId p :
       allPersonIds(entities) | std::views::filter(is_on_watchlist)) {
    const auto customerKey =
        entity::makeKey(entity::Role::customer, entity::Bank::internal, p);
    const auto cidBuf = encoding::format(customerKey);
    const auto wlView =
        exporter::common::makeWatchlistId(watchlistId, cidBuf.view());

    w.writeRow(wlView, std::string_view{"internal_fraud_list"},
               std::string_view{"fraud_suspect"}, entryDate);
  }
}

namespace {

using DeviceWindowMap =
    std::unordered_map<devices::Identity, exporter::common::SeenWindow>;
using DeviceIpMap = std::unordered_map<devices::Identity, std::string>;

void recordOwnerIp(DeviceIpMap &out, const devices::Identity &dev,
                   entity::PersonId person,
                   const synth::infra::ips::Output &ips) {
  if (out.find(dev) != out.end()) {
    return;
  }
  const auto personIt = ips.byPerson.find(person);
  if (personIt == ips.byPerson.end() || personIt->second.empty()) {
    return;
  }
  const auto ipBuf = ::PhantomLedger::network::format(personIt->second.front());
  out[dev] = std::string{ipBuf.view()};
}

} // namespace

void writeDeviceRows(::PhantomLedger::exporter::csv::Writer &w,
                     const synth::infra::devices::Output &devices,
                     const synth::infra::ips::Output &ips) {
  DeviceWindowMap seen;
  DeviceIpMap deviceIpStr;

  for (const auto &usage : devices.usages) {
    exporter::common::recordSeen(seen[usage.deviceId], usage.firstSeen,
                                 usage.lastSeen);
    recordOwnerIp(deviceIpStr, usage.deviceId, usage.personId, ips);
  }

  for (const auto &record : devices.records) {
    const auto idBuf = exporter::common::renderDeviceId(record.identity);
    const auto deviceTypeName = synth::infra::name(record.kind);
    const auto os = exporter::common::osForDeviceType(deviceTypeName);

    const auto ipIt = deviceIpStr.find(record.identity);
    const std::string ipStr =
        (ipIt != deviceIpStr.end()) ? ipIt->second : std::string{"0.0.0.0"};

    const auto windowIt = seen.find(record.identity);
    const auto window = (windowIt != seen.end())
                            ? windowIt->second
                            : exporter::common::SeenWindow{};

    w.writeRow(idBuf.view(), deviceTypeName, ipStr,
               exporter::common::kUsCountry, os,
               time_ns::formatTimestamp(window.firstSeen),
               time_ns::formatTimestamp(window.lastSeen),
               static_cast<std::uint8_t>(record.flagged));
  }
}

// ──────────────────────────────────────────────────────────────────────
// Transaction / SAR rows
// ──────────────────────────────────────────────────────────────────────

namespace {

[[nodiscard]] bool isCreditChannel(channels::Tag tag) noexcept {
  return channels::isPaydayInbound(tag) ||
         channels::is(tag, channels::Product::taxRefund) ||
         channels::is(tag, channels::Insurance::claim) ||
         channels::is(tag, channels::Credit::refund) ||
         channels::is(tag, channels::Credit::chargeback);
}

// C++20: std::to_array
inline constexpr auto kPurposeEntries =
    std::to_array<lookup::Entry<channels::Tag>>({
        {"payroll", channels::tag(channels::Legit::salary)},
        {"purchase", channels::tag(channels::Legit::merchant)},
        {"purchase", channels::tag(channels::Legit::cardPurchase)},
        {"bill_payment", channels::tag(channels::Legit::bill)},
        {"peer_transfer", channels::tag(channels::Legit::p2p)},
        {"wire_transfer", channels::tag(channels::Legit::externalUnknown)},
        {"cash_withdrawal", channels::tag(channels::Legit::atm)},
        {"internal_transfer", channels::tag(channels::Legit::selfTransfer)},
        {"subscription", channels::tag(channels::Legit::subscription)},
        {"business_income", channels::tag(channels::Legit::clientAchCredit)},
        {"business_income", channels::tag(channels::Legit::cardSettlement)},
        {"business_income", channels::tag(channels::Legit::platformPayout)},
        {"business_draw", channels::tag(channels::Legit::ownerDraw)},
        {"investment", channels::tag(channels::Legit::investmentInflow)},
        {"rent_payment", channels::tag(channels::Rent::generic)},
        {"rent_payment", channels::tag(channels::Rent::ach)},
        {"rent_payment", channels::tag(channels::Rent::portal)},
        {"rent_payment", channels::tag(channels::Rent::p2p)},
        {"rent_payment", channels::tag(channels::Rent::check)},
        {"government_benefit",
         channels::tag(channels::Government::socialSecurity)},
        {"government_benefit", channels::tag(channels::Government::pension)},
        {"government_benefit", channels::tag(channels::Government::disability)},
        {"insurance", channels::tag(channels::Insurance::premium)},
        {"insurance_claim", channels::tag(channels::Insurance::claim)},
        {"loan_payment", channels::tag(channels::Product::mortgage)},
        {"loan_payment", channels::tag(channels::Product::autoLoan)},
        {"loan_payment", channels::tag(channels::Product::studentLoan)},
        {"tax_payment", channels::tag(channels::Product::taxEstimated)},
        {"tax_payment", channels::tag(channels::Product::taxBalanceDue)},
        {"tax_refund", channels::tag(channels::Product::taxRefund)},
        {"credit_card_payment", channels::tag(channels::Credit::payment)},
        {"interest_charge", channels::tag(channels::Credit::interest)},
        {"fee", channels::tag(channels::Credit::lateFee)},
        {"refund", channels::tag(channels::Credit::refund)},
        {"chargeback", channels::tag(channels::Credit::chargeback)},
    });

inline constexpr auto kPurposeTable = lookup::reverseTable<256>(
    kPurposeEntries, [](channels::Tag t) { return t.value; });

[[nodiscard]] std::string_view channelPurpose(channels::Tag tag) noexcept {
  const auto p = kPurposeTable[tag.value];
  return p.empty() ? std::string_view{"other"} : p;
}

[[nodiscard]] int riskScore(const txns::Transaction &tx) noexcept {
  return (tx.fraud.flag != 0) ? 10 : 0;
}

} // namespace

void writeTransactionRows(exporter::csv::Writer &w,
                          std::span<const txns::Transaction> finalTxns) {
  std::size_t idx = 1;
  for (const auto &tx : finalTxns) {
    const auto channelTag = tx.session.channel;
    const auto channelName = channels::name(channelTag);
    const auto purpose = channelPurpose(channelTag);
    const auto credDeb = isCreditChannel(channelTag) ? "C" : "D";
    const double amount = primitives::utils::roundMoney(tx.amount);

    w.writeRow(
        transactionId(idx), std::string_view{credDeb},
        time_ns::formatTimestamp(time_ns::fromEpochSeconds(tx.timestamp)),
        channelName, purpose, riskScore(tx), std::string_view{"USD"}, amount,
        1.0, amount);
    ++idx;
  }
}

void writeSarRows(exporter::csv::Writer &w,
                  std::span<const exporter::aml::sar::SarRecord> sars) {
  for (const auto &s : sars) {
    w.writeRow(s.sarId, time_ns::formatTimestamp(s.filingDate),
               s.amountInvolved, time_ns::formatTimestamp(s.activityStart),
               time_ns::formatTimestamp(s.activityEnd), s.violationType);
  }
}

} // namespace PhantomLedger::exporter::aml::vertices
