#include "phantomledger/exporter/aml/vertices.hpp"

#include "phantomledger/entities/encoding/render.hpp"
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
#include "phantomledger/transactions/network/format.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace PhantomLedger::exporter::aml::vertices {

namespace {

namespace ent = ::PhantomLedger::entity;
namespace tx_ns = ::PhantomLedger::transactions;
namespace t_ns = ::PhantomLedger::time;
namespace pii_ns = ::PhantomLedger::entities::synth::pii;
namespace loc = ::PhantomLedger::locale;
namespace common = ::PhantomLedger::exporter::common;

using BranchCode = ::PhantomLedger::encoding::RenderedId<8>;

[[nodiscard]] BranchCode branchCodeFor(const ent::Key &id) noexcept {
  const auto bucket = (id.number % 50U) + 1U;
  BranchCode out;
  const auto n = std::snprintf(out.bytes.data(), out.bytes.size(), "BR%03u",
                               static_cast<unsigned>(bucket));
  out.length = static_cast<std::uint8_t>(n > 0 ? n : 0);
  return out;
}

[[nodiscard]] ::PhantomLedger::personas::Type
resolvedPersona(const SharedContext &ctx, ent::PersonId p) noexcept {
  if (p == 0 || p > ctx.personaByPerson.size()) {
    return ::PhantomLedger::personas::Type::salaried;
  }
  return ctx.personaByPerson[p - 1];
}

[[nodiscard]] const pii_ns::PoolSet &
poolsFor(const SharedContext &ctx) noexcept {
  assert(ctx.pools != nullptr &&
         "SharedContext::pools is null — was the context built with "
         "buildSharedContext(entities, txns, pools)?");
  return *ctx.pools;
}

[[nodiscard]] const pii_ns::LocalePool &
usPoolFor(const SharedContext &ctx) noexcept {
  return poolsFor(ctx).forCountry(loc::Country::us);
}

} // namespace

// SharedContext build
SharedContext
buildSharedContext(const ::PhantomLedger::pipeline::Entities &entities,
                   std::span<const tx_ns::Transaction> finalTxns,
                   const pii_ns::PoolSet &pools) {
  SharedContext ctx;
  ctx.pools = &pools;

  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      ctx.counterpartyIds.emplace(common::renderKey(rec.id));
    }
  }

  ctx.bankIds = allBankIds(ctx.counterpartyIds);

  ctx.personaByPerson = entities.personas.assignment.byPerson;

  ctx.lastTransactionByAccount.reserve(finalTxns.size() / 2);
  const auto bump = [&](const ent::Key &account, std::int64_t ts) {
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

inline void writeCustomerIdCells(::PhantomLedger::exporter::csv::Writer &w,
                                 const ent::Key &customerKey,
                                 ::PhantomLedger::personas::Type persona) {
  w.cell(::PhantomLedger::encoding::format(customerKey).view())
      .cell(identity::customerType(persona))
      .cell(std::string_view{"active"});
}

inline void writeDemographicCells(::PhantomLedger::exporter::csv::Writer &w,
                                  ent::PersonId p,
                                  ::PhantomLedger::personas::Type persona) {
  w.cell(identity::maritalStatus(p, persona))
      .cell(identity::networthCode(persona))
      .cell(identity::incomeCode(persona))
      .cell(identity::occupation(p, persona));
}

inline void writeRiskAndOriginCells(::PhantomLedger::exporter::csv::Writer &w,
                                    std::string_view riskRating,
                                    std::string_view originCountry,
                                    t_ns::TimePoint onboardingDate) {
  w.cell(riskRating)
      .cell(originCountry)
      .cell(t_ns::formatTimestamp(onboardingDate));
}

} // namespace

void writeCustomerRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const SharedContext &ctx, t_ns::TimePoint simStart) {
  const auto &peopleRoster = entities.people.roster;
  for (ent::PersonId p = 1; p <= peopleRoster.count; ++p) {
    const auto persona = resolvedPersona(ctx, p);
    const bool isFraud = peopleRoster.has(p, ent::person::Flag::fraud);
    const bool isMule = peopleRoster.has(p, ent::person::Flag::mule);
    const bool isVictim = peopleRoster.has(p, ent::person::Flag::victim);

    const auto customerKey =
        ent::makeKey(ent::Role::customer, ent::Bank::internal, p);

    const auto originCountry = loc::code(entities.pii.at(p).country);

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

[[nodiscard]] std::string lastTxnString(const ent::Key &acct,
                                        const SharedContext &ctx) {
  const auto it = ctx.lastTransactionByAccount.find(acct);
  if (it == ctx.lastTransactionByAccount.end()) {
    return {};
  }
  return t_ns::formatTimestamp(t_ns::fromEpochSeconds(it->second));
}

} // namespace

void writeAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const ::PhantomLedger::clearing::Ledger *finalBook,
                      const SharedContext &ctx, t_ns::TimePoint simStart) {
  const auto cardIds = common::collectCardIds(entities.creditCards);

  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      continue;
    }

    const auto idStr = common::renderKey(rec.id);
    const auto openDate = (rec.owner == ent::invalidPerson)
                              ? simStart - t_ns::Days{365}
                              : identity::onboardingDate(rec.owner, simStart);
    const auto lastTxnStr = lastTxnString(rec.id, ctx);
    const auto acctType =
        common::accountType(rec.id, cardIds.count(rec.id) != 0);
    const auto branch = branchCodeFor(rec.id);

    const double balance =
        (finalBook != nullptr)
            ? primitives::utils::roundMoney(finalBook->liquidity(rec.id))
            : 0.0;

    w.writeRow(idStr, idStr, balance, t_ns::formatTimestamp(openDate),
               std::string_view{"active"}, lastTxnStr,
               std::string_view{""}, // close_date — never set in synth
               acctType, std::string_view{"USD"}, branch);
  }
}

// Counterparty rows
void writeCounterpartyRows(::PhantomLedger::exporter::csv::Writer &w,
                           const SharedContext &ctx) {
  const auto &usPool = usPoolFor(ctx);
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto bankId = counterpartyBankId(cpId);
    const auto cpName = identity::nameForCounterparty(cpId, usPool);
    const auto bankName = identity::nameForBank(bankId);
    w.writeRow(cpId, cpName.firstName, identity::routingNumberForId(bankId),
               bankName.firstName, common::kUsCountry);
  }
}

// Name / Address rows
namespace {

void writeAddressRow(::PhantomLedger::exporter::csv::Writer &w,
                     const identity::AddressRecord &a) {
  w.writeRow(a.id, a.streetLine1, a.streetLine2, a.city, a.state, a.postalCode,
             a.country, a.addressType,
             static_cast<std::uint8_t>(a.isHighRiskGeo));
}

} // namespace

void writeNameRows(::PhantomLedger::exporter::csv::Writer &w,
                   const ::PhantomLedger::pipeline::Entities &entities,
                   const SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &usPool = pools.forCountry(loc::Country::us);

  std::unordered_set<std::string> emitted;
  emitted.reserve(common::estimateIdentityRowCount(entities, ctx));

  const auto emitName = [&](const identity::NameRecord &n) {
    if (emitted.emplace(n.id.view()).second) {
      w.writeRow(n.id, n.firstName, n.middleName, n.lastName);
    }
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

void writeAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const SharedContext &ctx) {
  const auto &pools = poolsFor(ctx);
  const auto &usPool = pools.forCountry(loc::Country::us);

  std::unordered_set<std::string> emitted;
  emitted.reserve(common::estimateIdentityRowCount(entities, ctx));

  const auto emitAddr = [&](const identity::AddressRecord &a) {
    if (emitted.emplace(a.id.view()).second) {
      writeAddressRow(w, a);
    }
  };

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    emitAddr(identity::addressForPerson(p, entities.pii, pools));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emitAddr(identity::addressForCounterparty(cpId, usPool));
  }
  for (const auto &bankId : ctx.bankIds) {
    emitAddr(identity::addressForBank(bankId, usPool));
  }
}

// ──────────────────────────────────────────────────────────────────────
// Country / Bank / Watchlist / Minhash rows
// ──────────────────────────────────────────────────────────────────────

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

static_assert(kCountryRows.size() == loc::kCountryCount,
              "kCountryRows must match locale::kCountries 1:1");

} // namespace

void writeCountryRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities) {
  namespace enumTax = ::PhantomLedger::taxonomies::enums;

  std::array<bool, loc::kCountryCount> seen{};
  seen[enumTax::toIndex(loc::Country::us)] = true;
  for (const auto &rec : entities.pii.records) {
    seen[enumTax::toIndex(rec.country)] = true;
  }

  for (std::size_t i = 0; i < loc::kCountryCount; ++i) {
    if (!seen[i]) {
      continue;
    }
    const auto country = loc::kCountries[i];
    const auto &row = kCountryRows[i];
    w.writeRow(loc::code(country), row.displayName, row.baseRiskScore);
  }
}

void writeBankRows(::PhantomLedger::exporter::csv::Writer &w,
                   const SharedContext &ctx) {
  for (const auto &bankId : ctx.bankIds) {
    w.writeRow(bankId);
  }
}

void writeWatchlistRows(::PhantomLedger::exporter::csv::Writer &w,
                        const ::PhantomLedger::pipeline::Entities &entities,
                        t_ns::TimePoint simStart) {
  const auto entryDate = t_ns::formatTimestamp(simStart);
  const auto &roster = entities.people.roster;
  std::string watchlistId;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    if (!roster.has(p, ent::person::Flag::fraud) &&
        !roster.has(p, ent::person::Flag::mule)) {
      continue;
    }
    const auto customerKey =
        ent::makeKey(ent::Role::customer, ent::Bank::internal, p);
    const auto cidBuf = ::PhantomLedger::encoding::format(customerKey);
    const auto wlView = common::makeWatchlistId(watchlistId, cidBuf.view());

    w.writeRow(wlView, std::string_view{"internal_fraud_list"},
               std::string_view{"fraud_suspect"}, entryDate);
  }
}

namespace {

using DeviceWindowMap =
    std::unordered_map<::PhantomLedger::devices::Identity, common::SeenWindow>;
using DeviceIpMap =
    std::unordered_map<::PhantomLedger::devices::Identity, std::string>;

void recordOwnerIp(DeviceIpMap &out,
                   const ::PhantomLedger::devices::Identity &dev,
                   ent::PersonId person,
                   const ::PhantomLedger::infra::synth::ips::Output &ips) {
  if (out.find(dev) != out.end()) {
    return;
  }
  const auto personIt = ips.byPerson.find(person);
  if (personIt == ips.byPerson.end() || personIt->second.empty()) {
    return;
  }
  // `network::format` returns a stack buffer; materialize the owning
  // std::string at the map-storage boundary.
  const auto ipBuf = ::PhantomLedger::network::format(personIt->second.front());
  out[dev] = std::string{ipBuf.view()};
}

} // namespace

void writeDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices,
    const ::PhantomLedger::infra::synth::ips::Output &ips) {
  DeviceWindowMap seen;
  DeviceIpMap deviceIpStr;

  for (const auto &usage : devices.usages) {
    common::recordSeen(seen[usage.deviceId], usage.firstSeen, usage.lastSeen);
    recordOwnerIp(deviceIpStr, usage.deviceId, usage.personId, ips);
  }

  for (const auto &record : devices.records) {
    const auto idBuf = common::renderDeviceId(record.identity);
    const auto deviceTypeName =
        ::PhantomLedger::infra::synth::name(record.kind);
    const auto os = common::osForDeviceType(deviceTypeName);

    const auto ipIt = deviceIpStr.find(record.identity);
    const std::string ipStr =
        (ipIt != deviceIpStr.end()) ? ipIt->second : std::string{"0.0.0.0"};

    const auto windowIt = seen.find(record.identity);
    const auto window =
        (windowIt != seen.end()) ? windowIt->second : common::SeenWindow{};

    w.writeRow(idBuf.view(), deviceTypeName, ipStr, common::kUsCountry, os,
               t_ns::formatTimestamp(window.firstSeen),
               t_ns::formatTimestamp(window.lastSeen),
               static_cast<std::uint8_t>(record.flagged));
  }
}

// ──────────────────────────────────────────────────────────────────────
// Transaction / SAR rows
// ──────────────────────────────────────────────────────────────────────

namespace {

namespace ch = ::PhantomLedger::channels;

[[nodiscard]] bool isCreditChannel(ch::Tag tag) noexcept {
  return ch::isPaydayInbound(tag) || ch::is(tag, ch::Product::taxRefund) ||
         ch::is(tag, ch::Insurance::claim) || ch::is(tag, ch::Credit::refund) ||
         ch::is(tag, ch::Credit::chargeback);
}

inline constexpr auto kPurposeEntries =
    std::to_array<::PhantomLedger::lookup::Entry<ch::Tag>>({
        {"payroll", ch::tag(ch::Legit::salary)},
        {"purchase", ch::tag(ch::Legit::merchant)},
        {"purchase", ch::tag(ch::Legit::cardPurchase)},
        {"bill_payment", ch::tag(ch::Legit::bill)},
        {"peer_transfer", ch::tag(ch::Legit::p2p)},
        {"wire_transfer", ch::tag(ch::Legit::externalUnknown)},
        {"cash_withdrawal", ch::tag(ch::Legit::atm)},
        {"internal_transfer", ch::tag(ch::Legit::selfTransfer)},
        {"subscription", ch::tag(ch::Legit::subscription)},
        {"business_income", ch::tag(ch::Legit::clientAchCredit)},
        {"business_income", ch::tag(ch::Legit::cardSettlement)},
        {"business_income", ch::tag(ch::Legit::platformPayout)},
        {"business_draw", ch::tag(ch::Legit::ownerDraw)},
        {"investment", ch::tag(ch::Legit::investmentInflow)},
        {"rent_payment", ch::tag(ch::Rent::generic)},
        {"rent_payment", ch::tag(ch::Rent::ach)},
        {"rent_payment", ch::tag(ch::Rent::portal)},
        {"rent_payment", ch::tag(ch::Rent::p2p)},
        {"rent_payment", ch::tag(ch::Rent::check)},
        {"government_benefit", ch::tag(ch::Government::socialSecurity)},
        {"government_benefit", ch::tag(ch::Government::pension)},
        {"government_benefit", ch::tag(ch::Government::disability)},
        {"insurance", ch::tag(ch::Insurance::premium)},
        {"insurance_claim", ch::tag(ch::Insurance::claim)},
        {"loan_payment", ch::tag(ch::Product::mortgage)},
        {"loan_payment", ch::tag(ch::Product::autoLoan)},
        {"loan_payment", ch::tag(ch::Product::studentLoan)},
        {"tax_payment", ch::tag(ch::Product::taxEstimated)},
        {"tax_payment", ch::tag(ch::Product::taxBalanceDue)},
        {"tax_refund", ch::tag(ch::Product::taxRefund)},
        {"credit_card_payment", ch::tag(ch::Credit::payment)},
        {"interest_charge", ch::tag(ch::Credit::interest)},
        {"fee", ch::tag(ch::Credit::lateFee)},
        {"refund", ch::tag(ch::Credit::refund)},
        {"chargeback", ch::tag(ch::Credit::chargeback)},
    });

inline constexpr auto kPurposeTable =
    ::PhantomLedger::lookup::reverseTable<256>(
        kPurposeEntries, [](ch::Tag t) { return t.value; });

[[nodiscard]] std::string_view channelPurpose(ch::Tag tag) noexcept {
  const auto p = kPurposeTable[tag.value];
  return p.empty() ? std::string_view{"other"} : p;
}

[[nodiscard]] int riskScore(const tx_ns::Transaction &tx) noexcept {
  return (tx.fraud.flag != 0) ? 10 : 0;
}

} // namespace

void writeTransactionRows(::PhantomLedger::exporter::csv::Writer &w,
                          std::span<const tx_ns::Transaction> finalTxns) {
  std::size_t idx = 1;
  for (const auto &tx : finalTxns) {
    const auto channelTag = tx.session.channel;
    const auto channelName = ch::name(channelTag);
    const auto purpose = channelPurpose(channelTag);
    const auto credDeb = isCreditChannel(channelTag) ? "C" : "D";
    const double amount = primitives::utils::roundMoney(tx.amount);

    w.writeRow(transactionId(idx), std::string_view{credDeb},
               t_ns::formatTimestamp(t_ns::fromEpochSeconds(tx.timestamp)),
               channelName, purpose, riskScore(tx), std::string_view{"USD"},
               amount, 1.0, amount);
    ++idx;
  }
}

void writeSarRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars) {
  for (const auto &s : sars) {
    w.writeRow(s.sarId, t_ns::formatTimestamp(s.filingDate), s.amountInvolved,
               t_ns::formatTimestamp(s.activityStart),
               t_ns::formatTimestamp(s.activityEnd), s.violationType);
  }
}

} // namespace PhantomLedger::exporter::aml::vertices
