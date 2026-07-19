#include "phantomledger/exporter/card_fraud/export.hpp"

#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/exporter/card_fraud/derive.hpp"
#include "phantomledger/exporter/card_fraud/schema.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/mule_ml/identity.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/taxonomies/merchants/names.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace PhantomLedger::exporter::card_fraud {

namespace {

namespace cmn = ::PhantomLedger::exporter::common;
namespace sch = ::PhantomLedger::exporter::schema::card_fraud;
namespace ent = ::PhantomLedger::entity;
namespace time_ns = ::PhantomLedger::time;

using ::PhantomLedger::entity::person::Flag;

// Party.is_fraud labels fraud ACTORS (ring member, solo fraudster or
// mule) — victims stay 0 (label definition, card-fraud-2026-07).
[[nodiscard]] bool isFraudActor(const ent::person::Roster &roster,
                                ent::PersonId p) {
  return roster.has(p, Flag::fraud) || roster.has(p, Flag::soloFraud) ||
         roster.has(p, Flag::mule);
}

} // namespace

Summary exportFromArtifacts(
    const ::PhantomLedger::pipeline::SimulationResult &world,
    const Options &options, StreamedArtifacts artifacts) {
  if (options.piiPools == nullptr) {
    throw std::invalid_argument(
        "card_fraud::exportFromArtifacts requires Options.piiPools "
        "(identity rendering and the merchant-geo zip table read it)");
  }
  const auto &pools = *options.piiPools;

  const cmn::TableTarget target{.pg = options.pgMirror,
                                .capture = options.capture};

  Summary out;
  out.totalRows = artifacts.rows;
  out.viewRows = artifacts.viewRows;
  out.fraudViewRows = artifacts.fraudViewRows;

  // ------------------------------------------- Card + Party_Has_Card
  // Credit cards resolve their owner through the card registry; the
  // derived debit cards through the account registry. Ownerless
  // sources (external accounts, if any reach the view) get a Card
  // vertex but no Party_Has_Card edge.
  std::unordered_map<ent::Key, ent::PersonId> accountOwner;
  accountOwner.reserve(world.holdings.accounts.registry.records.size());
  for (const auto &record : world.holdings.accounts.registry.records) {
    accountOwner.emplace(record.id, record.owner);
  }

  {
    auto card = cmn::openTable(target, sch::kCard);
    auto hasCard = cmn::openTable(target, sch::kPartyHasCard);

    for (const auto &[key, seen] : artifacts.cards) {
      const auto id = derive::cardId(key, seen.credit);
      card.writer().writeRow(id,
                             static_cast<std::int32_t>(seen.fraud ? 1 : 0));

      ent::PersonId owner = ent::invalidPerson;
      if (seen.credit) {
        if (const auto *terms = world.holdings.creditCards.forKey(key)) {
          owner = terms->owner;
        }
      } else if (const auto it = accountOwner.find(key);
                 it != accountOwner.end()) {
        owner = it->second;
      }
      if (ent::valid(owner)) {
        hasCard.writer().writeRow(cmn::renderCustomerId(owner).view(), id);
      }
    }
    out.cardCount = artifacts.cards.size();
  }

  // ------------------- Merchant, Merchant_Assigned, the geo chain
  std::unordered_map<ent::Key, merchants::Category> categoryByKey;
  categoryByKey.reserve(world.counterparties.merchants.records.size());
  for (const auto &record : world.counterparties.merchants.records) {
    categoryByKey.emplace(record.counterpartyId, record.category);
  }

  const auto &pool = pools.forCountry(locale::kDefaultCountry);
  const auto &zipTable = pool.zipTable;
  if (zipTable.empty()) {
    throw std::runtime_error("card_fraud::exportFromArtifacts: the PII "
                             "pool's zip table is empty; merchant geography "
                             "derives from it");
  }

  struct CityInfo {
    std::string name;
    std::string state;
  };
  std::map<std::string, CityInfo> cities;
  std::set<std::string> states;
  std::map<std::string, std::string> zipToCity;

  {
    auto merchant = cmn::openTable(target, sch::kMerchant);
    auto assigned = cmn::openTable(target, sch::kMerchantAssigned);
    auto hasState = cmn::openTable(target, sch::kHasState);
    auto hasCity = cmn::openTable(target, sch::kHasCity);
    auto hasZip = cmn::openTable(target, sch::kHasZip);

    for (const auto &key : artifacts.merchants) {
      const auto id = derive::merchantId(key);
      merchant.writer().writeRow(id);

      const auto catIt = categoryByKey.find(key);
      const auto category = catIt != categoryByKey.end()
                                ? catIt->second
                                : derive::fallbackCategory(key);
      assigned.writer().writeRow(id, merchants::name(category));

      const auto &zip =
          zipTable[derive::geoIndexFor(key, zipTable.size())];
      const std::string state{zip.adminCode};
      const std::string cityName{zip.city};
      const std::string zipcode{zip.postalCode};
      const std::string cityId = cityName + "_" + state;

      hasState.writer().writeRow(id, state);
      hasCity.writer().writeRow(id, cityId);
      hasZip.writer().writeRow(id, zipcode);

      cities.emplace(cityId, CityInfo{cityName, state});
      states.insert(state);
      zipToCity.emplace(zipcode, cityId);
    }
    out.merchantCount = artifacts.merchants.size();
  }

  {
    auto city = cmn::openTable(target, sch::kCity);
    auto locatedIn = cmn::openTable(target, sch::kLocatedIn);
    for (const auto &[cityId, info] : cities) {
      city.writer().writeRow(cityId, info.name,
                             derive::populationFor(cityId));
      locatedIn.writer().writeRow(cityId, info.state);
    }
    out.cityCount = cities.size();

    auto state = cmn::openTable(target, sch::kState);
    for (const auto &s : states) {
      state.writer().writeRow(s);
    }
    out.stateCount = states.size();

    auto zipcode = cmn::openTable(target, sch::kZipcode);
    auto assignedTo = cmn::openTable(target, sch::kAssignedTo);
    for (const auto &[zip, cityId] : zipToCity) {
      zipcode.writer().writeRow(zip);
      assignedTo.writer().writeRow(zip, cityId);
    }
    out.zipcodeCount = zipToCity.size();
  }

  {
    auto category = cmn::openTable(target, sch::kMerchantCategory);
    for (const auto cat : merchants::kCategories) {
      category.writer().writeRow(merchants::name(cat));
    }
  }

  // ----------------------------------------------------------- Party
  const auto &roster = world.people.roster.roster;
  const auto &pii = world.people.pii;
  const ::PhantomLedger::synth::pii::Membership membership(
      roster.count, options.window, ::PhantomLedger::synth::pii::Growth{});

  {
    auto party = cmn::openTable(target, sch::kParty);
    for (ent::PersonId p = 1; p <= roster.count; ++p) {
      const auto identity =
          p <= pii.records.size()
              ? mule_ml::renderIdentity(pii.records[p - 1], pools)
              : mule_ml::blankIdentity();
      party.writer().writeRow(
          cmn::renderCustomerId(p).view(),
          static_cast<std::int32_t>(isFraudActor(roster, p) ? 1 : 0),
          derive::genderFor(p), identity.dob, "person", identity.name,
          time_ns::formatTimestamp(membership.joinTs(p)));
    }
    out.partyCount = roster.count;
  }

  // Is_Merchant: header-only — no modeled merchant-owning-party link
  // (business owners own accounts, not catalog merchants).
  {
    auto isMerchant = cmn::openTable(target, sch::kIsMerchant);
    (void)isMerchant;
  }

  // ------------------------------------- PII / investigative layer
  {
    std::set<std::string> addresses;
    std::set<std::string> phones;
    std::set<std::string> emails;
    std::set<std::string> ssns;
    std::set<std::string> names;
    std::set<std::string> dobs;

    {
      auto hasAddress = cmn::openTable(target, sch::kHasAddress);
      auto hasPhone = cmn::openTable(target, sch::kHasPhone);
      auto hasEmail = cmn::openTable(target, sch::kHasEmail);
      auto hasId = cmn::openTable(target, sch::kHasId);
      auto hasDob = cmn::openTable(target, sch::kHasDob);
      auto hasFullName = cmn::openTable(target, sch::kHasFullName);

      for (ent::PersonId p = 1;
           p <= roster.count && p <= pii.records.size(); ++p) {
        const auto &record = pii.records[p - 1];
        const auto identity = mule_ml::renderIdentity(record, pools);
        const auto partyStr = cmn::renderCustomerId(p);

        if (!identity.address.empty()) {
          addresses.insert(identity.address);
          hasAddress.writer().writeRow(identity.address, partyStr.view());
        }
        if (const auto phone = record.phone.view(); !phone.empty()) {
          phones.emplace(phone);
          hasPhone.writer().writeRow(partyStr.view(), phone);
        }
        if (const auto email = record.email.view(); !email.empty()) {
          emails.emplace(email);
          hasEmail.writer().writeRow(partyStr.view(), email);
        }
        if (const auto ssn = record.ssn.view(); !ssn.empty()) {
          ssns.emplace(ssn);
          hasId.writer().writeRow(partyStr.view(), ssn);
        }
        if (!identity.dob.empty()) {
          dobs.insert(identity.dob);
          hasDob.writer().writeRow(partyStr.view(), identity.dob);
        }
        if (!identity.name.empty()) {
          names.insert(identity.name);
          hasFullName.writer().writeRow(partyStr.view(), identity.name);
        }
      }
    }

    auto address = cmn::openTable(target, sch::kAddress);
    for (const auto &a : addresses) {
      address.writer().writeRow(a);
    }
    auto phone = cmn::openTable(target, sch::kPhone);
    for (const auto &v : phones) {
      phone.writer().writeRow(v);
    }
    auto email = cmn::openTable(target, sch::kEmail);
    for (const auto &v : emails) {
      email.writer().writeRow(v);
    }
    auto idTable = cmn::openTable(target, sch::kId);
    for (const auto &v : ssns) {
      idTable.writer().writeRow(v, "ssn");
    }
    auto fullName = cmn::openTable(target, sch::kFullName);
    for (const auto &v : names) {
      fullName.writer().writeRow(v);
    }
    auto dob = cmn::openTable(target, sch::kDob);
    for (const auto &v : dobs) {
      dob.writer().writeRow(v);
    }
  }

  // Devices and IPs carry their modeled flags (flagged / blacklisted)
  // as is_blocked.
  {
    auto device = cmn::openTable(target, sch::kDevice);
    for (const auto &record : world.infra.devices.records) {
      device.writer().writeRow(
          cmn::renderDeviceId(record.identity).view(),
          static_cast<std::int32_t>(record.flagged ? 1 : 0));
    }
    auto hasDevice = cmn::openTable(target, sch::kHasDevice);
    for (ent::PersonId p = 1; p <= roster.count; ++p) {
      const auto it = world.infra.devices.byPerson.find(p);
      if (it == world.infra.devices.byPerson.end()) {
        continue;
      }
      for (const auto &identity : it->second) {
        hasDevice.writer().writeRow(cmn::renderCustomerId(p).view(),
                                    cmn::renderDeviceId(identity).view());
      }
    }

    auto ip = cmn::openTable(target, sch::kIp);
    for (const auto &record : world.infra.ips.records) {
      ip.writer().writeRow(
          network::format(record.address).view(),
          static_cast<std::int32_t>(record.blacklisted ? 1 : 0));
    }
    auto hasIp = cmn::openTable(target, sch::kHasIp);
    for (ent::PersonId p = 1; p <= roster.count; ++p) {
      const auto it = world.infra.ips.byPerson.find(p);
      if (it == world.infra.ips.byPerson.end()) {
        continue;
      }
      for (const auto &addr : it->second) {
        hasIp.writer().writeRow(cmn::renderCustomerId(p).view(),
                                network::format(addr).view());
      }
    }
  }

  return out;
}

Summary exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
                  const Options &options) {
  const auto &postedTxns = result.transfers.ledger.posted.txns;
  const auto txns =
      std::span<const ::PhantomLedger::transactions::Transaction>{postedTxns};

  // The SAME sink the windowed engine streams through, run over the
  // retained corpus as one batch — the engines cannot drift.
  StreamingCardFraudExport sink({
      .cards = &result.holdings.creditCards,
      .merchants = &result.counterparties.merchants,
      .window = options.window,
      .pgMirror = options.pgMirror,
      .capture = options.capture,
  });
  sink.append(txns);
  sink.finish();

  return exportFromArtifacts(result, options, sink.takeArtifacts());
}

} // namespace PhantomLedger::exporter::card_fraud
