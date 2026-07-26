#include "phantomledger/exporter/card_fraud/export.hpp"

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/exporter/card_fraud/derive.hpp"
#include "phantomledger/exporter/card_fraud/schema.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/mule_ml/identity.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/personas/join.hpp"
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
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::card_fraud {

namespace {

namespace cmn = ::PhantomLedger::exporter::common;
namespace sch = ::PhantomLedger::exporter::schema::card_fraud;
namespace ent = ::PhantomLedger::entity;
namespace geo = ::PhantomLedger::synth::geo;
namespace time_ns = ::PhantomLedger::time;

using ::PhantomLedger::entity::person::Flag;

// Party ground truth labels fraud ACTORS (ring member, solo fraudster or
// mule) — victims stay unlabelled (label definition, card-fraud-2026-07).
[[nodiscard]] bool isFraudActor(const ent::person::Roster &roster,
                                ent::PersonId p) {
  return roster.has(p, Flag::fraud) || roster.has(p, Flag::soloFraud) ||
         roster.has(p, Flag::mule);
}

// ------------------------------------------- THE ZEROED LABEL COLUMNS
//
// card-fraud-realism-v2 (gate 1 of docs/card_fraud_online_gnn.md).
// Card.is_fraud, Party.is_fraud, Device.is_blocked and IP.is_blocked are
// FULL-WINDOW entity verdicts: "this card ever carried a flag-1 row".
// Exported as features they answer the training question before the
// model sees a transaction. The columns stay (TF_GNN_v3 loading jobs map
// positionally) and carry this value; the investigative content moves to
// the quarantined kGroundTruthLabel table below.
inline constexpr std::int32_t kLabelWithheld = 0;

// The label overlay: POSITIVES ONLY, joinable 1:1 to the vertex tables
// by (entity_type, entity_id). Bounded by the number of fraud-touched
// entities, which is a small fraction of the roster — accumulated here
// so the single table writer runs once, after every labelled source has
// been walked in its own deterministic order (cards by Key, parties by
// PersonId, devices then IPs in record order).
struct GroundTruthRow {
  std::string_view entityType;
  std::string entityId;
  std::string_view label;
};

} // namespace

Summary exportFromArtifacts(
    const ::PhantomLedger::pipeline::SimulationResult &world,
    const Options &options, StreamedArtifacts artifacts) {
  if (options.piiPools == nullptr) {
    throw std::invalid_argument(
        "card_fraud::exportFromArtifacts requires Options.piiPools "
        "(party identity rendering reads the PII pools)");
  }
  const auto &pools = *options.piiPools;

  const cmn::TableTarget target{.pg = options.pgMirror,
                                .capture = options.capture};

  Summary out;
  out.totalRows = artifacts.rows;
  out.viewRows = artifacts.viewRows;
  out.fraudViewRows = artifacts.fraudViewRows;

  std::vector<GroundTruthRow> groundTruth;

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
      card.writer().writeRow(id, kLabelWithheld);
      if (seen.fraud) {
        groundTruth.push_back({"card", id, "ever_fraud"});
      }

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
    card.close();
    hasCard.close();
  }

  // ------------------- Merchant, Merchant_Assigned, the geo chain
  //
  // geo-causal-v1: a merchant's outlet geography is the WORLD-modeled
  // `entity::merchant::Record.location` (assigned in G1c, population-
  // weighted), resolved through the pinned catalogue — NO longer hashed
  // at export. An `online` merchant (Footprint::online → invalidGeoArea)
  // is geography-free: it gets Merchant + Merchant_Assigned but NO
  // Has_City/Has_State/Has_Zip, exactly like TabFormer's Online rows
  // (which carry no merchant geography). Non-catalog destinations (the
  // fraud rail's biller fallback) likewise have no modeled location and
  // stay geo-free.
  std::unordered_map<ent::Key, const ent::merchant::Record *> merchantByKey;
  merchantByKey.reserve(world.counterparties.merchants.records.size());
  for (const auto &record : world.counterparties.merchants.records) {
    merchantByKey.emplace(record.counterpartyId, &record);
  }

  struct CityInfo {
    std::string name;
    std::string state;
    std::uint32_t population = 0;
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

      const auto mit = merchantByKey.find(key);
      const auto category = mit != merchantByKey.end()
                                ? mit->second->category
                                : derive::fallbackCategory(key);
      assigned.writer().writeRow(id, merchants::name(category));

      // Only a physical outlet with a modeled catalogue area gets a
      // location; online merchants and non-catalog billers are geo-free.
      if (mit == merchantByKey.end() ||
          !geo::geography().contains(mit->second->location)) {
        continue;
      }
      const auto &area = geo::geography().at(mit->second->location);
      const std::string state{area.stateCode};
      const std::string cityName{area.city};
      const std::string zipcode{area.postalAreaCode};
      const std::string cityId = cityName + "_" + state;

      hasState.writer().writeRow(id, state);
      hasCity.writer().writeRow(id, cityId);
      hasZip.writer().writeRow(id, zipcode);

      cities.emplace(cityId, CityInfo{cityName, state, area.population});
      states.insert(state);
      zipToCity.emplace(zipcode, cityId);
    }
    out.merchantCount = artifacts.merchants.size();
    merchant.close();
    assigned.close();
    hasState.close();
    hasCity.close();
    hasZip.close();
  }

  {
    auto city = cmn::openTable(target, sch::kCity);
    auto locatedIn = cmn::openTable(target, sch::kLocatedIn);
    for (const auto &[cityId, info] : cities) {
      city.writer().writeRow(cityId, info.name, info.population);
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
    city.close();
    locatedIn.close();
    state.close();
    zipcode.close();
    assignedTo.close();
  }

  {
    auto category = cmn::openTable(target, sch::kMerchantCategory);
    for (const auto cat : merchants::kCategories) {
      category.writer().writeRow(merchants::name(cat));
    }
    category.close();
  }

  // ----------------------------------------------------------- Party
  const auto &roster = world.people.roster.roster;
  const auto &pii = world.people.pii;
  // H3 part 3c-ii: THE one membership construction path — the Party
  // created_at reports the same [joinTs, ...) axis the standard
  // exporter's customer.csv does (the retired flat-Growth view gone).
  // It is also the reason Party carries no other point-in-time debt:
  // created_at is when the customer relationship began, not a
  // window-wide constant.
  const ::PhantomLedger::synth::pii::Membership membership =
      ::PhantomLedger::synth::personas::join_cohort::membershipOf(
          world.people.personas, options.window);

  {
    auto party = cmn::openTable(target, sch::kParty);
    for (ent::PersonId p = 1; p <= roster.count; ++p) {
      const auto identity =
          p <= pii.records.size()
              ? mule_ml::renderIdentity(pii.records[p - 1], pools)
              : mule_ml::blankIdentity();
      party.writer().writeRow(
          cmn::renderCustomerId(p).view(), kLabelWithheld, derive::genderFor(p),
          identity.dob, "person", identity.name,
          time_ns::formatTimestamp(membership.joinTs(p)));
      if (isFraudActor(roster, p)) {
        groundTruth.push_back({"party",
                               std::string{cmn::renderCustomerId(p).view()},
                               "fraud_actor"});
      }
    }
    out.partyCount = roster.count;
    party.close();
  }

  // Is_Merchant: header-only — no modeled merchant-owning-party link
  // (business owners own accounts, not catalog merchants).
  {
    auto isMerchant = cmn::openTable(target, sch::kIsMerchant);
    isMerchant.close();
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
      hasAddress.close();
      hasPhone.close();
      hasEmail.close();
      hasId.close();
      hasDob.close();
      hasFullName.close();
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
    address.close();
    phone.close();
    email.close();
    idTable.close();
    fullName.close();
    dob.close();
  }

  // Devices and IPs. The modelled flag / blacklist verdicts are
  // whole-window investigative facts, so is_blocked is withheld here and
  // the verdicts go to the ground-truth overlay.
  {
    auto device = cmn::openTable(target, sch::kDevice);
    for (const auto &record : world.infra.devices.records) {
      const auto id = cmn::renderDeviceId(record.identity);
      device.writer().writeRow(id.view(), kLabelWithheld);
      if (record.flagged) {
        groundTruth.push_back({"device", std::string{id.view()}, "flagged"});
      }
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
      const auto id = network::format(record.address);
      ip.writer().writeRow(id.view(), kLabelWithheld);
      if (record.blacklisted) {
        groundTruth.push_back({"ip", std::string{id.view()}, "blacklisted"});
      }
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
    device.close();
    hasDevice.close();
    ip.close();
    hasIp.close();
  }

  // ------------------------------------------ investigative ground truth
  // Outside the feature graph by construction: no TF_GNN_v3 loading job
  // reads this table and no edge points at it. Every row is a
  // whole-window fact, which is exactly why it may not be joined back
  // into a model's inputs.
  {
    auto labels = cmn::openTable(target, sch::kGroundTruthLabel);
    for (const auto &row : groundTruth) {
      labels.writer().writeRow(row.entityType, row.entityId, row.label);
    }
    labels.close();
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
      .pgMirror = options.pgMirror,
      .capture = options.capture,
  });
  sink.append(txns);
  sink.finish();

  return exportFromArtifacts(result, options, sink.takeArtifacts());
}

} // namespace PhantomLedger::exporter::card_fraud
