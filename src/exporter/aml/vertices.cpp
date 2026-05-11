#include "phantomledger/exporter/aml/vertices.hpp"

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/aml/identity.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/taxonomies/channels/names.hpp"
#include "phantomledger/transactions/network/format.hpp"

#include <cassert>
#include <cmath>
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

// ──────────────────────────────────────────────────────────────────────
// Small leaf-level helpers
// ──────────────────────────────────────────────────────────────────────

/// Round to 2dp.
[[nodiscard]] double round2(double v) noexcept {
  return std::round(v * 100.0) / 100.0;
}

[[nodiscard]] ::PhantomLedger::encoding::RenderedKey
renderKey(const ent::Key &k) noexcept {
  return ::PhantomLedger::encoding::format(k);
}

[[nodiscard]] std::string_view accountTypeFor(const ent::Key &id,
                                              bool isCard) noexcept {
  if (isCard) {
    return "credit";
  }
  if (id.role == ent::Role::business && id.bank == ent::Bank::internal) {
    return "business_checking";
  }
  if (id.role == ent::Role::brokerage && id.bank == ent::Bank::internal) {
    return "brokerage";
  }
  if (id.role == ent::Role::card) {
    return "credit";
  }
  return "checking";
}

/// "BR042" — bucket the account's `number` into one of 50 synthetic
/// branches. Pure stack buffer; the caller view()s or implicitly
/// converts when passing to csv::Writer.
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

/// Centralized accessor: every writer that reaches the pool goes through
/// this helper, which asserts the SharedContext was built correctly.
/// `buildSharedContext` always sets `usPool`, so this is a debug-only
/// guard against future refactors that might construct a SharedContext
/// by hand.
[[nodiscard]] const pii_ns::LocalePool &
poolFor(const SharedContext &ctx) noexcept {
  assert(ctx.usPool != nullptr &&
         "SharedContext::usPool is null — was the context built with "
         "buildSharedContext(entities, txns, usPool)?");
  return *ctx.usPool;
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// SharedContext build
// ──────────────────────────────────────────────────────────────────────

SharedContext
buildSharedContext(const ::PhantomLedger::pipeline::Entities &entities,
                   std::span<const tx_ns::Transaction> finalTxns,
                   const pii_ns::LocalePool &usPool) {
  SharedContext ctx;
  ctx.usPool = &usPool;

  // Counterparty id set: render every external account key once.
  // `renderKey` is now allocation-free (returns a stack-buffer); the
  // single std::string we allocate is the set element itself.
  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      ctx.counterpartyIds.emplace(renderKey(rec.id).view());
    }
  }

  ctx.personaByPerson = entities.personas.assignment.byPerson;

  // Last-transaction-per-account index, used by writeAccountRows.
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

// ──────────────────────────────────────────────────────────────────────
// Customer rows
// ──────────────────────────────────────────────────────────────────────

namespace {

// Cell-group helpers for the 10-column customer row. Each ≤3 params,
// one logical concern, composed by writeCustomerRows below.
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
                                    t_ns::TimePoint onboardingDate) {
  w.cell(riskRating)
      .cell(std::string_view{"US"})
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

    writeCustomerIdCells(w, customerKey, persona);
    writeDemographicCells(w, p, persona);
    writeRiskAndOriginCells(w, identity::riskRating(isFraud, isMule, isVictim),
                            identity::onboardingDate(p, simStart));
    w.endRow();
  }
}

// ──────────────────────────────────────────────────────────────────────
// Account rows
// ──────────────────────────────────────────────────────────────────────

namespace {

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
  const auto cardIds = buildCardIdSet(entities.creditCards);

  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      continue;
    }

    const auto idStr = renderKey(rec.id);
    const auto openDate = openDateFor(rec, simStart);
    const auto lastTxnStr = lastTxnString(rec.id, ctx);
    const auto acctType = accountTypeFor(rec.id, cardIds.count(rec.id) != 0);
    const auto branch = branchCodeFor(rec.id);

    const double balance =
        (finalBook != nullptr) ? round2(finalBook->liquidity(rec.id)) : 0.0;

    w.writeRow(idStr, idStr, balance, t_ns::formatTimestamp(openDate),
               std::string_view{"active"}, lastTxnStr,
               std::string_view{""}, // close_date — never set in synth
               acctType, std::string_view{"USD"}, branch);
  }
}

// ──────────────────────────────────────────────────────────────────────
// Counterparty rows
// ──────────────────────────────────────────────────────────────────────

void writeCounterpartyRows(::PhantomLedger::exporter::csv::Writer &w,
                           const SharedContext &ctx) {
  const auto &usPool = poolFor(ctx);
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto bankId = counterpartyBankId(cpId);
    const auto cpName = identity::nameForCounterparty(cpId, usPool);
    const auto bankName = identity::nameForBank(bankId);
    w.writeRow(cpId, cpName.firstName, identity::routingNumberForId(bankId),
               bankName.firstName, std::string_view{"US"});
  }
}

// ──────────────────────────────────────────────────────────────────────
// Name / Address rows
//
// Both writers dedup by `id` because the same name/address can be
// referenced multiple times (e.g. shared landlords). The dedup set is
// sized by *unique entities* (persons + counterparties + banks), not
// by row count, so the cost of allocating a std::string per unique
// entry is bounded and not on the hot path.
// ──────────────────────────────────────────────────────────────────────

namespace {

[[nodiscard]] std::size_t
estimateIdentityRowCount(const ::PhantomLedger::pipeline::Entities &entities,
                         const SharedContext &ctx) noexcept {
  // 21 ≈ number of bank rows synthesized from counterparty ids.
  return entities.people.roster.count + ctx.counterpartyIds.size() + 21U;
}

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
  const auto &usPool = poolFor(ctx);

  std::unordered_set<std::string> emitted;
  emitted.reserve(estimateIdentityRowCount(entities, ctx));

  const auto emitName = [&](const identity::NameRecord &n) {
    // n.id is a small inline buffer (StackString). The set still owns
    // its own std::string copy — one alloc per unique entity, not per
    // row — which is fine for AML dedup.
    if (emitted.emplace(n.id.view()).second) {
      w.writeRow(n.id, n.firstName, n.middleName, n.lastName);
    }
  };

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    emitName(identity::nameForPerson(p, entities.pii, usPool));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emitName(identity::nameForCounterparty(cpId, usPool));
  }
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    emitName(identity::nameForBank(bankId));
  }
}

void writeAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const SharedContext &ctx) {
  const auto &usPool = poolFor(ctx);

  std::unordered_set<std::string> emitted;
  emitted.reserve(estimateIdentityRowCount(entities, ctx));

  const auto emitAddr = [&](const identity::AddressRecord &a) {
    if (emitted.emplace(a.id.view()).second) {
      writeAddressRow(w, a);
    }
  };

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    emitAddr(identity::addressForPerson(p, entities.pii, usPool));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emitAddr(identity::addressForCounterparty(cpId, usPool));
  }
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    emitAddr(identity::addressForBank(bankId, usPool));
  }
}

// ──────────────────────────────────────────────────────────────────────
// Country / Bank / Watchlist / Minhash rows
// ──────────────────────────────────────────────────────────────────────

void writeCountryRows(::PhantomLedger::exporter::csv::Writer &w) {
  w.writeRow(std::string_view{"US"}, std::string_view{"United States"}, 0.1);
}

void writeBankRows(::PhantomLedger::exporter::csv::Writer &w,
                   const SharedContext &ctx) {
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    w.writeRow(bankId);
  }
}

void writeWatchlistRows(::PhantomLedger::exporter::csv::Writer &w,
                        const ::PhantomLedger::pipeline::Entities &entities,
                        t_ns::TimePoint simStart) {
  const auto entryDate = t_ns::formatTimestamp(simStart);
  const auto &roster = entities.people.roster;
  // Reused across rows to avoid re-allocating the "WL_..." prefix per row.
  std::string watchlistId;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    if (!roster.has(p, ent::person::Flag::fraud) &&
        !roster.has(p, ent::person::Flag::mule)) {
      continue;
    }
    const auto customerKey =
        ent::makeKey(ent::Role::customer, ent::Bank::internal, p);
    const auto cidBuf = ::PhantomLedger::encoding::format(customerKey);
    const auto cidView = cidBuf.view();

    watchlistId.clear();
    watchlistId.reserve(3 + cidView.size());
    watchlistId.append("WL_").append(cidView);

    w.writeRow(watchlistId, std::string_view{"internal_fraud_list"},
               std::string_view{"fraud_suspect"}, entryDate);
  }
}

void writeMinhashIdRows(::PhantomLedger::exporter::csv::Writer &w,
                        const std::set<std::string> &minhashIds) {
  for (const auto &id : minhashIds) {
    w.writeRow(id);
  }
}

// ──────────────────────────────────────────────────────────────────────
// Device rows
// ──────────────────────────────────────────────────────────────────────

namespace {

struct DeviceSeenWindow {
  t_ns::TimePoint firstSeen{};
  t_ns::TimePoint lastSeen{};
};

using DeviceWindowMap =
    std::unordered_map<::PhantomLedger::devices::Identity, DeviceSeenWindow>;
using DeviceIpMap =
    std::unordered_map<::PhantomLedger::devices::Identity, std::string>;

void recordSeenWindow(DeviceWindowMap &out,
                      const ::PhantomLedger::devices::Identity &dev,
                      t_ns::TimePoint firstSeen, t_ns::TimePoint lastSeen) {
  auto &w = out[dev];
  if (w.firstSeen == t_ns::TimePoint{} || firstSeen < w.firstSeen) {
    w.firstSeen = firstSeen;
  }
  if (lastSeen > w.lastSeen) {
    w.lastSeen = lastSeen;
  }
}

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
  out[dev] = ::PhantomLedger::network::format(personIt->second.front());
}

[[nodiscard]] std::string_view osFor(std::string_view deviceTypeName) noexcept {
  if (deviceTypeName == "android") {
    return "Android";
  }
  if (deviceTypeName == "ios") {
    return "iOS";
  }
  if (deviceTypeName == "web" || deviceTypeName == "browser") {
    return "Browser";
  }
  if (deviceTypeName == "desktop") {
    return "Windows";
  }
  return "Unknown";
}

} // namespace

void writeDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices,
    const ::PhantomLedger::infra::synth::ips::Output &ips) {
  DeviceWindowMap seen;
  DeviceIpMap deviceIpStr;

  for (const auto &usage : devices.usages) {
    recordSeenWindow(seen, usage.deviceId, usage.firstSeen, usage.lastSeen);
    recordOwnerIp(deviceIpStr, usage.deviceId, usage.personId, ips);
  }

  for (const auto &record : devices.records) {
    const auto idBuf =
        ::PhantomLedger::exporter::common::renderDeviceId(record.identity);
    const auto deviceTypeName =
        ::PhantomLedger::infra::synth::name(record.kind);
    const auto os = osFor(deviceTypeName);

    const auto ipIt = deviceIpStr.find(record.identity);
    const std::string ipStr =
        (ipIt != deviceIpStr.end()) ? ipIt->second : std::string{"0.0.0.0"};

    const auto windowIt = seen.find(record.identity);
    const auto window =
        (windowIt != seen.end()) ? windowIt->second : DeviceSeenWindow{};

    w.writeRow(idBuf.view(), deviceTypeName, ipStr, std::string_view{"US"}, os,
               t_ns::formatTimestamp(window.firstSeen),
               t_ns::formatTimestamp(window.lastSeen),
               static_cast<std::uint8_t>(record.flagged));
  }
}

// ──────────────────────────────────────────────────────────────────────
// Transaction / SAR rows
// ──────────────────────────────────────────────────────────────────────

namespace {

[[nodiscard]] bool isCreditChannel(std::string_view name) noexcept {
  static constexpr std::array<std::string_view, 13> kCredit{
      "salary",
      "gov_social_security",
      "gov_pension",
      "gov_disability",
      "tax_refund",
      "insurance_claim",
      "client_ach_credit",
      "card_settlement",
      "platform_payout",
      "owner_draw",
      "investment_inflow",
      "cc_refund",
      "cc_chargeback",
  };
  for (const auto v : kCredit) {
    if (name == v) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::string_view
channelPurpose(std::string_view channel) noexcept {
  if (channel == "salary")
    return "payroll";
  if (channel == "rent" || channel == "rent_ach" || channel == "rent_portal" ||
      channel == "rent_p2p" || channel == "rent_check") {
    return "rent_payment";
  }
  if (channel == "p2p")
    return "peer_transfer";
  if (channel == "bill")
    return "bill_payment";
  if (channel == "merchant" || channel == "card_purchase")
    return "purchase";
  if (channel == "subscription")
    return "subscription";
  if (channel == "atm_withdrawal")
    return "cash_withdrawal";
  if (channel == "self_transfer")
    return "internal_transfer";
  if (channel == "external_unknown")
    return "wire_transfer";
  if (channel == "insurance_premium")
    return "insurance";
  if (channel == "insurance_claim")
    return "insurance_claim";
  if (channel == "mortgage_payment" || channel == "auto_loan_payment" ||
      channel == "student_loan_payment") {
    return "loan_payment";
  }
  if (channel == "tax_estimated_payment" || channel == "tax_balance_due") {
    return "tax_payment";
  }
  if (channel == "tax_refund")
    return "tax_refund";
  if (channel == "gov_social_security" || channel == "gov_pension" ||
      channel == "gov_disability") {
    return "government_benefit";
  }
  if (channel == "cc_payment")
    return "credit_card_payment";
  if (channel == "cc_interest")
    return "interest_charge";
  if (channel == "cc_late_fee")
    return "fee";
  if (channel == "cc_refund")
    return "refund";
  if (channel == "cc_chargeback")
    return "chargeback";
  if (channel == "client_ach_credit" || channel == "card_settlement" ||
      channel == "platform_payout") {
    return "business_income";
  }
  if (channel == "owner_draw")
    return "business_draw";
  if (channel == "investment_inflow")
    return "investment";
  return "other";
}

[[nodiscard]] int riskScore(const tx_ns::Transaction &tx,
                            std::string_view /*channelName*/) noexcept {
  return (tx.fraud.flag != 0) ? 10 : 0;
}

// transactionId is defined in shared.hpp — returns a TransactionId
// stack buffer that flows through csv::Writer via implicit conversion.

} // namespace

void writeTransactionRows(::PhantomLedger::exporter::csv::Writer &w,
                          std::span<const tx_ns::Transaction> finalTxns) {
  std::size_t idx = 1;
  for (const auto &tx : finalTxns) {
    const auto channelName =
        ::PhantomLedger::channels::name(tx.session.channel);
    const auto purpose = channelPurpose(channelName);
    const auto credDeb = isCreditChannel(channelName) ? "C" : "D";
    const double amount = round2(tx.amount);

    w.writeRow(transactionId(idx), std::string_view{credDeb},
               t_ns::formatTimestamp(t_ns::fromEpochSeconds(tx.timestamp)),
               channelName, purpose, riskScore(tx, channelName),
               std::string_view{"USD"}, amount, 1.0, amount);
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
