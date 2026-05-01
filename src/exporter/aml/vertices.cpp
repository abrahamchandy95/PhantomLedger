#include "phantomledger/exporter/aml/vertices.hpp"

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/aml/identity.hpp"
#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/exporter/standard/internal/customer_id.hpp"
#include "phantomledger/taxonomies/channels/names.hpp"
#include "phantomledger/transactions/network/format.hpp"

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

/// Round to 2dp.
[[nodiscard]] double round2(double v) noexcept {
  return std::round(v * 100.0) / 100.0;
}

[[nodiscard]] std::string renderKey(const ent::Key &k) {
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

[[nodiscard]] std::string branchCodeFor(const ent::Key &id) {
  const auto bucket = (id.number % 50U) + 1U;
  char buf[8];
  std::snprintf(buf, sizeof(buf), "BR%03u", static_cast<unsigned>(bucket));
  return std::string{buf};
}

[[nodiscard]] ::PhantomLedger::personas::Type
resolvedPersona(const SharedContext &ctx, ent::PersonId p) noexcept {
  if (p == 0 || p > ctx.personaByPerson.size()) {
    return ::PhantomLedger::personas::Type::salaried;
  }
  return ctx.personaByPerson[p - 1];
}

} // namespace

SharedContext
buildSharedContext(const ::PhantomLedger::pipeline::Entities &entities,
                   std::span<const tx_ns::Transaction> finalTxns) {
  SharedContext ctx;

  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      ctx.counterpartyIds.insert(renderKey(rec.id));
    }
  }

  ctx.personaByPerson = entities.personas.assignment.byPerson;

  ctx.lastTransactionByAccount.reserve(finalTxns.size() / 2);
  for (const auto &tx : finalTxns) {
    {
      auto &slot = ctx.lastTransactionByAccount[tx.source];
      if (tx.timestamp > slot) {
        slot = tx.timestamp;
      }
    }
    {
      auto &slot = ctx.lastTransactionByAccount[tx.target];
      if (tx.timestamp > slot) {
        slot = tx.timestamp;
      }
    }
  }
  return ctx;
}

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
    w.writeRow(
        ::PhantomLedger::encoding::format(customerKey),
        identity::customerType(persona), std::string_view{"active"},
        identity::maritalStatus(p, persona), identity::networthCode(persona),
        identity::incomeCode(persona), identity::occupation(p, persona),
        identity::riskRating(isFraud, isMule, isVictim), std::string_view{"US"},
        t_ns::formatTimestamp(identity::onboardingDate(p, simStart)));
  }
}

void writeAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const ::PhantomLedger::clearing::Ledger *finalBook,
                      const SharedContext &ctx, t_ns::TimePoint simStart) {
  // Build credit-card account-id set once.
  std::unordered_set<ent::Key> cardIds;
  for (const auto &rec : entities.creditCards.records) {
    cardIds.insert(rec.key);
  }

  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      continue;
    }

    t_ns::TimePoint openDate;
    if (rec.owner == ent::invalidPerson) {
      openDate = simStart - t_ns::Days{365};
    } else {
      openDate = identity::onboardingDate(rec.owner, simStart);
    }

    double balance = 0.0;
    if (finalBook != nullptr) {
      balance = round2(finalBook->liquidity(rec.id));
    }

    const auto idStr = renderKey(rec.id);
    const auto lastIt = ctx.lastTransactionByAccount.find(rec.id);
    std::string lastTxnStr;
    if (lastIt != ctx.lastTransactionByAccount.end()) {
      lastTxnStr =
          t_ns::formatTimestamp(t_ns::fromEpochSeconds(lastIt->second));
    }

    const bool isCard = cardIds.count(rec.id) != 0;
    const auto acctType = accountTypeFor(rec.id, isCard);
    const auto branch = branchCodeFor(rec.id);

    w.writeRow(idStr, idStr, balance, t_ns::formatTimestamp(openDate),
               std::string_view{"active"}, lastTxnStr,
               std::string_view{""}, // close_date — never set in synth
               acctType, std::string_view{"USD"}, branch);
  }
}

void writeCounterpartyRows(::PhantomLedger::exporter::csv::Writer &w,
                           const SharedContext &ctx) {
  for (const auto &cpId : ctx.counterpartyIds) {
    const auto bankId = counterpartyBankId(cpId);
    const auto bankName = identity::nameForBank(bankId);
    const auto cpName = identity::nameForCounterparty(cpId);
    w.writeRow(cpId, cpName.firstName, identity::routingNumberForId(bankId),
               bankName.firstName, std::string_view{"US"});
  }
}

void writeNameRows(::PhantomLedger::exporter::csv::Writer &w,
                   const ::PhantomLedger::pipeline::Entities &entities,
                   const SharedContext &ctx) {
  std::unordered_set<std::string> emitted;
  emitted.reserve(entities.people.roster.count + ctx.counterpartyIds.size() +
                  21);

  const auto emitName = [&](const identity::NameRecord &n) {
    if (emitted.insert(n.id).second) {
      w.writeRow(n.id, n.firstName, n.middleName, n.lastName);
    }
  };

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    emitName(identity::nameForPerson(p));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emitName(identity::nameForCounterparty(cpId));
  }
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    emitName(identity::nameForBank(bankId));
  }
}

namespace {

void writeAddressRow(::PhantomLedger::exporter::csv::Writer &w,
                     const identity::AddressRecord &a) {
  w.writeRow(a.id, a.streetLine1, a.streetLine2, a.city, a.state, a.postalCode,
             a.country, a.addressType,
             static_cast<std::uint8_t>(a.isHighRiskGeo));
}

} // namespace

void writeAddressRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const SharedContext &ctx) {
  std::unordered_set<std::string> emitted;
  emitted.reserve(entities.people.roster.count + ctx.counterpartyIds.size() +
                  21);

  const auto emitAddr = [&](const identity::AddressRecord &a) {
    if (emitted.insert(a.id).second) {
      writeAddressRow(w, a);
    }
  };

  for (ent::PersonId p = 1; p <= entities.people.roster.count; ++p) {
    emitAddr(identity::addressForPerson(p));
  }
  for (const auto &cpId : ctx.counterpartyIds) {
    emitAddr(identity::addressForCounterparty(cpId));
  }
  for (const auto &bankId : allBankIds(ctx.counterpartyIds)) {
    emitAddr(identity::addressForBank(bankId));
  }
}

void writeCountryRows(::PhantomLedger::exporter::csv::Writer &w) {
  w.writeRow(std::string_view{"US"}, std::string_view{"United States"}, 0.1);
}

void writeDeviceRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::infra::synth::devices::Output &devices,
    const ::PhantomLedger::infra::synth::ips::Output &ips) {
  std::unordered_map<::PhantomLedger::devices::Identity, t_ns::TimePoint>
      firstSeen, lastSeen;
  std::unordered_map<::PhantomLedger::devices::Identity, std::string>
      deviceIpStr;
  for (const auto &usage : devices.usages) {
    auto it = firstSeen.find(usage.deviceId);
    if (it == firstSeen.end() || usage.firstSeen < it->second) {
      firstSeen[usage.deviceId] = usage.firstSeen;
    }
    auto jt = lastSeen.find(usage.deviceId);
    if (jt == lastSeen.end() || usage.lastSeen > jt->second) {
      lastSeen[usage.deviceId] = usage.lastSeen;
    }
    if (deviceIpStr.find(usage.deviceId) == deviceIpStr.end()) {
      // Owner's first IP, if any.
      const auto personIt = ips.byPerson.find(usage.personId);
      if (personIt != ips.byPerson.end() && !personIt->second.empty()) {
        deviceIpStr[usage.deviceId] =
            ::PhantomLedger::network::format(personIt->second.front());
      }
    }
  }

  for (const auto &record : devices.records) {
    const auto idStr =
        ::PhantomLedger::exporter::standard::detail::renderDeviceId(
            record.identity);
    const auto deviceTypeName =
        ::PhantomLedger::infra::synth::name(record.kind);

    std::string_view os = "Unknown";
    if (deviceTypeName == "android") {
      os = "Android";
    } else if (deviceTypeName == "ios") {
      os = "iOS";
    } else if (deviceTypeName == "web" || deviceTypeName == "browser") {
      os = "Browser";
    } else if (deviceTypeName == "desktop") {
      os = "Windows";
    }

    const auto ipIt = deviceIpStr.find(record.identity);
    const std::string ipStr =
        (ipIt != deviceIpStr.end()) ? ipIt->second : std::string{"0.0.0.0"};

    const auto fsIt = firstSeen.find(record.identity);
    const auto lsIt = lastSeen.find(record.identity);
    const t_ns::TimePoint fsTp =
        (fsIt != firstSeen.end()) ? fsIt->second : t_ns::TimePoint{};
    const t_ns::TimePoint lsTp =
        (lsIt != lastSeen.end()) ? lsIt->second : t_ns::TimePoint{};

    w.writeRow(idStr, deviceTypeName, ipStr, std::string_view{"US"}, os,
               t_ns::formatTimestamp(fsTp), t_ns::formatTimestamp(lsTp),
               static_cast<std::uint8_t>(record.flagged));
  }
}

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
                            std::string_view channelName) noexcept {
  if (tx.fraud.flag != 0) {
    return 10;
  }

  (void)channelName;
  return 0;
}

[[nodiscard]] std::string transactionId(std::size_t idx1) {
  char buf[20];
  std::snprintf(buf, sizeof(buf), "TXN_%012zu", idx1);
  return std::string{buf};
}

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
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    if (!roster.has(p, ent::person::Flag::fraud) &&
        !roster.has(p, ent::person::Flag::mule)) {
      continue;
    }
    const auto customerKey =
        ent::makeKey(ent::Role::customer, ent::Bank::internal, p);
    const auto cidStr = ::PhantomLedger::encoding::format(customerKey);
    w.writeRow("WL_" + cidStr, std::string_view{"internal_fraud_list"},
               std::string_view{"fraud_suspect"}, entryDate);
  }
}

void writeMinhashIdRows(::PhantomLedger::exporter::csv::Writer &w,
                        const std::set<std::string> &minhashIds) {
  for (const auto &id : minhashIds) {
    w.writeRow(id);
  }
}

} // namespace PhantomLedger::exporter::aml::vertices
