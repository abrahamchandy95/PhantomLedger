#include "phantomledger/exporter/card_fraud/export.hpp"

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/exporter/card_fraud/derive.hpp"
#include "phantomledger/exporter/card_fraud/schema.hpp"
#include "phantomledger/exporter/common/minhash.hpp"
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

Summary
exportFromArtifacts(const ::PhantomLedger::pipeline::SimulationResult &world,
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

    std::size_t cardVertices = 0;
    for (const auto &[key, seen] : artifacts.cards) {
      // card-churn-2026-07: ONE VERTEX PER OBSERVED GENERATION. An account
      // whose card was replaced mid-window contributes two `cf_Card` rows and
      // two `cf_Party_Has_Card` edges, which is what makes reissue visible in
      // the graph at all.
      //
      // Observed, never scheduled: a generation with no transaction in the
      // view must not become a vertex, for the same reason `cf_Merchant` is
      // restricted to view-observed merchants — the loader rejects an edge
      // pointing at a vertex a prefix has not written.
      //
      // The generation set is never empty for a card that reached the view:
      // the streaming pass inserts on every row it accepts. The fallback
      // below keeps a direct unit caller (one that populates `cards` by hand)
      // working rather than silently emitting nothing.
      ent::PersonId owner = ent::invalidPerson;
      if (seen.credit) {
        if (const auto *terms = world.holdings.creditCards.forKey(key)) {
          owner = terms->owner;
        }
      } else if (const auto it = accountOwner.find(key);
                 it != accountOwner.end()) {
        owner = it->second;
      }

      const std::set<std::uint32_t> fallback{0U};
      const auto &generations =
          seen.generations.empty() ? fallback : seen.generations;

      for (const auto generation : generations) {
        const auto id = derive::cardId(key, seen.credit, generation);
        card.writer().writeRow(id, kLabelWithheld);
        ++cardVertices;
        if (seen.fraud) {
          // The overlay is entity-level and stays quarantined. Every
          // generation of a fraud-touched account is labelled, because the
          // overlay's unit is the CARD IDENTITY and each generation is one.
          groundTruth.push_back({"card", id, "ever_fraud"});
        }
        if (ent::valid(owner)) {
          hasCard.writer().writeRow(cmn::renderCustomerId(owner).view(), id);
        }
      }
    }
    out.cardCount = cardVertices;
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
  const auto &roster = world.people.roster.roster;
  const auto &pii = world.people.pii;

  std::unordered_map<ent::Key, const ent::merchant::Record *> merchantByKey;
  merchantByKey.reserve(world.counterparties.merchants.records.size());
  for (const auto &record : world.counterparties.merchants.records) {
    merchantByKey.emplace(record.counterpartyId, &record);
  }

  // merchant-coordinates-2026-07: `coords` is the area centroid in decimal
  // degrees, carried alongside the identifiers it already resolves so the
  // City/Zipcode writers below report the SAME point the merchant does. A
  // city or zip is first-writer-wins here exactly as `population` already
  // was, so all three attributes of a City row come from ONE area — never
  // a population from one area and a centroid from another.
  struct Coords {
    double lat = 0.0;
    double lon = 0.0;
  };
  struct CityInfo {
    std::string name;
    std::string state;
    std::uint32_t population = 0;
    Coords coords;
  };
  struct ZipInfo {
    std::string cityId;
    Coords coords;
  };
  std::map<std::string, CityInfo> cities;
  std::set<std::string> states;
  std::map<std::string, ZipInfo> zipToCity;

  {
    auto merchant = cmn::openTable(target, sch::kMerchant);
    auto assigned = cmn::openTable(target, sch::kMerchantAssigned);
    auto hasState = cmn::openTable(target, sch::kHasState);
    auto hasCity = cmn::openTable(target, sch::kHasCity);
    auto hasZip = cmn::openTable(target, sch::kHasZip);
    auto merchantLocation = cmn::openTable(target, sch::kMerchantLocation);

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
      const Coords coords{derive::degreesFromE6(area.latitudeE6),
                          derive::degreesFromE6(area.longitudeE6)};

      hasState.writer().writeRow(id, state);
      hasCity.writer().writeRow(id, cityId);
      hasZip.writer().writeRow(id, zipcode);
      // Row presence IS the has_coordinates mask: only this branch — a
      // catalog merchant with a valid modelled area — reaches it, so an
      // online merchant or non-catalog biller is ABSENT rather than
      // carrying a fake 0,0 that reads as the Gulf of Guinea.
      merchantLocation.writer().writeRow(id, coords.lat, coords.lon);

      cities.emplace(cityId,
                     CityInfo{cityName, state, area.population, coords});
      states.insert(state);
      zipToCity.emplace(zipcode, ZipInfo{cityId, coords});
    }
    out.merchantCount = artifacts.merchants.size();
    merchant.close();
    assigned.close();
    hasState.close();
    hasCity.close();
    hasZip.close();
    merchantLocation.close();
  }

  // ------------------------------------------- PARTY HOME GEOGRAPHY
  //
  // party-geography-2026-07. This pass runs BEFORE the City/State/Zipcode
  // writers below, and the ordering is load-bearing: a party home area the
  // merchant loop never visited must become a City/State/Zipcode VERTEX
  // too, or its edge dangles and the downstream loader rejects it. Party
  // areas are therefore unioned into the same three containers merchant
  // geography fills, which is also why `Assigned_To` and `Located_In` now
  // span both populations.
  //
  // The vertex tables stay correctly classified despite the union: their
  // party half is prefix-invariant (the relocation schedule is world state
  // fixed before the fold) while their merchant half still grows with the
  // stream, so `City` remains keyed-stable and `Zipcode`/`State` remain line
  // subsets.
  //
  // Full roster, matching `cf_Party`'s own loop bound exactly, so no edge
  // can point at a Party vertex that was not written. DRAW-FREE — this reads
  // world state assigned long before any transaction settled.
  //
  // relocation-2026-07: ONE ROW PER TENURE, each stamped with the epoch it
  // began. EVERY tenure is emitted, not just the ones live at window end,
  // because a transaction in year 3 must resolve the year-3 home — and every
  // tenure's area is unioned into the vertex tables for the same
  // dangling-edge reason the merchant pass has. A party who never moves emits
  // exactly one row at the window start, so the pre-round shape is the no-move
  // case rather than a special case.
  {
    auto hasStdCity = cmn::openTable(target, sch::kHasStdCity);
    auto hasStdPostcode = cmn::openTable(target, sch::kHasStdPostcode);
    auto hasStdState = cmn::openTable(target, sch::kHasStdState);

    const auto emitArea = [&](ent::PersonId p, ent::geography::GeoAreaId area,
                              std::int64_t since) {
      if (!geo::geography().contains(area)) {
        return; // no modelled home area: absence is the mask
      }
      const auto &row = geo::geography().at(area);
      const std::string state{row.stateCode};
      const std::string cityName{row.city};
      const std::string zipcode{row.postalAreaCode};
      const std::string cityId = cityName + "_" + state;
      const Coords coords{derive::degreesFromE6(row.latitudeE6),
                          derive::degreesFromE6(row.longitudeE6)};

      const auto partyStr = cmn::renderCustomerId(p);
      hasStdCity.writer().writeRow(partyStr.view(), cityId, since);
      hasStdPostcode.writer().writeRow(partyStr.view(), zipcode, since);
      hasStdState.writer().writeRow(partyStr.view(), state, since);

      cities.emplace(cityId, CityInfo{cityName, state, row.population, coords});
      states.insert(state);
      zipToCity.emplace(zipcode, ZipInfo{cityId, coords});
    };

    const auto windowStart = time::toEpochSeconds(options.window.start);
    const auto *relocation = &world.people.relocation;
    for (ent::PersonId p = 1; p <= roster.count; ++p) {
      if (p > pii.records.size()) {
        break;
      }
      const auto tenures =
          relocation != nullptr ? relocation->tenures(p)
                                : std::span<const ent::parties::relocation::
                                                Tenure>{};
      if (tenures.empty()) {
        // No schedule bound (a direct unit caller, or a zero-day window):
        // fall back to the static home, stamped at the window start. This is
        // the pre-round row exactly.
        emitArea(p, pii.records[p - 1].address.geoArea, windowStart);
        continue;
      }
      for (const auto &tenure : tenures) {
        emitArea(p, tenure.area, tenure.fromEpoch);
      }
    }
    hasStdCity.close();
    hasStdPostcode.close();
    hasStdState.close();
  }

  {
    auto city = cmn::openTable(target, sch::kCity);
    auto locatedIn = cmn::openTable(target, sch::kLocatedIn);
    for (const auto &[cityId, info] : cities) {
      city.writer().writeRow(cityId, info.name, info.population,
                             info.coords.lat, info.coords.lon);
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
    for (const auto &[zip, info] : zipToCity) {
      zipcode.writer().writeRow(zip, info.coords.lat, info.coords.lon);
      assignedTo.writer().writeRow(zip, info.cityId);
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
      party.writer().writeRow(cmn::renderCustomerId(p).view(), kLabelWithheld,
                              derive::genderFor(p), identity.dob, "person",
                              identity.name,
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

  // ------------------------------------------------- Is_Merchant
  //
  // NO LONGER HEADER-ONLY (merchant-ownership-2026-07). The note it
  // carried — "no modeled merchant-owning-party link (business owners own
  // accounts, not catalog merchants)" — was an accurate reading of the
  // world: `Role::business` keys and `Role::merchant` keys were disjoint
  // populations that were never joined.
  //
  // Leaving it empty was not free, and the cost landed OUTSIDE this
  // repository: `tf_gnn_loader_v2` aborts the entire push at
  // `sql/postgres/001_validate_sources.sql` with "cf_Is_Merchant is empty;
  // Party_Is_Merchant edges would not load". The graph schema declares
  // `Party_Is_Merchant(FROM Party, TO Merchant)` with reverse edge
  // `Merchant_Owned_By_Party`, so the semantics is ownership.
  //
  // `Record::owner` now carries the institution's beneficial-owner record,
  // resolved draw-free from the merchant KEY ALONE so that owner-edge
  // presence cannot correlate with footprint, size or geography — which
  // matters because fraud destination selection IS footprint-conditioned.
  // See entities/counterparties/merchant_ownership.hpp for that argument.
  //
  // RESTRICTED TO MERCHANTS OBSERVED IN THE VIEW, because `cf_Merchant`
  // above is built from `artifacts.merchants` and is therefore a GROWING
  // stream-derived vertex set. Emitting the world's whole register would
  // dangle edges at merchant vertices that a prefix export has not
  // written yet — the loader validates exactly that, and
  // test_card_point_in_time classifies this table as a growing line
  // subset for the same reason.
  {
    auto isMerchant = cmn::openTable(target, sch::kIsMerchant);
    for (const auto &key : artifacts.merchants) {
      const auto mit = merchantByKey.find(key);
      if (mit == merchantByKey.end()) {
        continue; // non-catalog degenerate destination: no register entry
      }
      const auto owner = mit->second->owner;
      if (owner == ent::invalidPerson || owner > roster.count) {
        continue;
      }
      isMerchant.writer().writeRow(derive::merchantId(key),
                                   cmn::renderCustomerId(owner).view());
    }
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

      for (ent::PersonId p = 1; p <= roster.count && p <= pii.records.size();
           ++p) {
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
    // Email LSH layer (owner request): bucket vertices plus the
    // Email -> bucket edges, derived from the email string alone via
    // the shared minhash wrapper — deterministic, draw-free, so the
    // corpus stream cannot move.
    namespace minhash = ::PhantomLedger::exporter::common::minhash;
    auto hasEmailMinhash = cmn::openTable(target, sch::kHasEmailMinhash);
    std::set<minhash::BucketId> emailBuckets;
    for (const auto &v : emails) {
      for (const auto &mhId : minhash::emailMinhashIds(v)) {
        hasEmailMinhash.writer().writeRow(v, mhId);
        emailBuckets.insert(mhId);
      }
    }
    auto emailMinhash = cmn::openTable(target, sch::kEmailMinhash);
    for (const auto &b : emailBuckets) {
      emailMinhash.writer().writeRow(b);
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
    hasEmailMinhash.close();
    emailMinhash.close();
    idTable.close();
    fullName.close();
    dob.close();
  }

  // ===================================================================
  // DEVICES AND IPs — THE MESSAGE-PASSING LAYER
  //
  // The vertex universe is the UNION of the synthesized roster and the
  // event-time session endpoints observed in the card view. Unauthorized
  // attackers are exogenous, so their endpoints need not belong to a
  // customer roster; omitting them made "session endpoint absent from
  // cf_Device/cf_IP" a deterministic card-fraud shortcut.
  //
  // The modelled flag / blacklist verdicts are whole-window
  // investigative facts, so is_blocked is withheld here and the verdicts
  // go to the ground-truth overlay.
  //
  // ---------------- Has_Device / Has_IP ARE NO LONGER HEADER-ONLY
  //
  // They shipped empty for four rounds, and the reason was sound at the
  // time: every legitimate endpoint had an owning Party and NO attacker
  // endpoint did, so "no Party edge" was an exact synonym for "attacker
  // endpoint" — a free, perfect label. But that was a property of the
  // GENERATOR, not of the world. Two generator changes removed it
  // (attacker-infra-2026-07), and the fix had to be there rather than
  // here:
  //
  //   1. `infra::enrollment` models the institution's endpoint registry
  //      as INCOMPLETE, which it is in production. A customer endpoint
  //      not on file carries ordinary legitimate rows, so "no Party
  //      edge" no longer implies fraud.
  //   2. The fraud planner puts a declared share of unauthorized cases
  //      on the VICTIM'S OWN endpoint (remote-access / household
  //      compromise) and routes a share of operator sessions through a
  //      RESIDENTIAL PROXY — some other customer's address. So on-file
  //      endpoints carry fraud, and "Party edge present" no longer
  //      implies legitimate.
  //
  // Both directions are now weak, which is what production looks like,
  // and `tests/test_card_endpoint_graph.cpp` SIZES the residual lift and
  // fails if it climbs back toward determinism. Withholding the topology
  // was never free: with `Has_*` empty and no transaction->endpoint edge
  // type in TF_GNN_v3, Device and IP were UNREACHABLE — isolated
  // vertices, zero message passing, the entire endpoint layer inert.
  //
  // WORLD-DERIVED, NOT STREAM-DERIVED. Both tables are built from
  // `world.infra.*.usages` alone, never from `artifacts`.
  // tests/test_card_point_in_time.cpp classifies them as world-derived
  // and requires full-vs-prefix BYTE IDENTITY, which a stream-observed
  // edge set could not satisfy. `enrolled` is a draw-free hash of
  // (party, endpoint), so it is identical in every prefix.
  {
    const auto &attackers = world.infra.attackers;

    std::map<devices::Identity, bool> deviceUniverse;
    for (const auto &record : world.infra.devices.records) {
      auto [it, inserted] =
          deviceUniverse.emplace(record.identity, record.flagged);
      if (!inserted) {
        it->second = it->second || record.flagged;
      }
    }
    // THE LABELLING GAP, CLOSED. Attacker endpoints reach the exporter
    // only through `artifacts`, and this line used to hard-code `false`
    // — so the only `device/flagged` positives in the whole overlay were
    // the five AML ring-shared devices, which enter the card view only
    // through a ring member's LEGITIMATE purchase. Entity-level device
    // ground truth was therefore both vanishing and ANTI-CORRELATED
    // with the transaction label. Attacker infrastructure is now a
    // world-known set, so the verdict can be truthful.
    //
    // A residential-proxy address is deliberately NOT marked: the
    // address belongs to a customer and is not attacker infrastructure,
    // whatever passed through it.
    for (const auto &identity : artifacts.devices) {
      const bool attackerOwned = attackers.ownsDevice(identity);
      auto [it, inserted] = deviceUniverse.try_emplace(identity, attackerOwned);
      if (!inserted) {
        it->second = it->second || attackerOwned;
      }
    }

    auto device = cmn::openTable(target, sch::kDevice);
    for (const auto &[identity, flagged] : deviceUniverse) {
      const auto id = cmn::renderDeviceId(identity);
      device.writer().writeRow(id.view(), kLabelWithheld);
      if (flagged) {
        groundTruth.push_back({"device", std::string{id.view()}, "flagged"});
      }
    }

    // Sorted by (party, endpoint) so the byte order is a property of the
    // world and not of usage-vector order, which future generator work
    // is free to change.
    {
      std::set<std::pair<ent::PersonId, devices::Identity>> onFile;
      for (const auto &usage : world.infra.devices.usages) {
        if (!usage.enrolled || usage.personId == ent::invalidPerson ||
            usage.personId == 0 || usage.personId > roster.count) {
          continue;
        }
        onFile.emplace(usage.personId, usage.deviceId);
      }
      auto hasDevice = cmn::openTable(target, sch::kHasDevice);
      for (const auto &[person, identity] : onFile) {
        hasDevice.writer().writeRow(cmn::renderCustomerId(person).view(),
                                    cmn::renderDeviceId(identity).view());
      }
      hasDevice.close();
    }

    std::map<network::Ipv4, bool> ipUniverse;
    for (const auto &record : world.infra.ips.records) {
      auto [it, inserted] =
          ipUniverse.emplace(record.address, record.blacklisted);
      if (!inserted) {
        it->second = it->second || record.blacklisted;
      }
    }
    for (const auto address : artifacts.ips) {
      const bool attackerOwned = attackers.ownsIp(address);
      auto [it, inserted] = ipUniverse.try_emplace(address, attackerOwned);
      if (!inserted) {
        it->second = it->second || attackerOwned;
      }
    }

    auto ip = cmn::openTable(target, sch::kIp);
    for (const auto &[address, blacklisted] : ipUniverse) {
      const auto id = network::format(address);
      ip.writer().writeRow(id.view(), kLabelWithheld);
      if (blacklisted) {
        groundTruth.push_back({"ip", std::string{id.view()}, "blacklisted"});
      }
    }

    {
      std::set<std::pair<ent::PersonId, network::Ipv4>> onFile;
      for (const auto &usage : world.infra.ips.usages) {
        if (!usage.enrolled || usage.personId == ent::invalidPerson ||
            usage.personId == 0 || usage.personId > roster.count) {
          continue;
        }
        onFile.emplace(usage.personId, usage.ipAddress);
      }
      auto hasIp = cmn::openTable(target, sch::kHasIp);
      for (const auto &[person, address] : onFile) {
        hasIp.writer().writeRow(cmn::renderCustomerId(person).view(),
                                network::format(address).view());
      }
      hasIp.close();
    }

    device.close();
    ip.close();
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
      .registry = &result.holdings.accounts.registry,
      .lookup = &result.holdings.accounts.lookup,
      .membership = ::PhantomLedger::synth::personas::join_cohort::membershipOf(
          result.people.personas, options.window),
      .cards = &result.holdings.creditCards,
      .merchants = &result.counterparties.merchants,
      .pgMirror = options.pgMirror,
      .capture = options.capture,
      .window = options.window,
  });
  sink.append(txns);
  sink.finish();

  return exportFromArtifacts(result, options, sink.takeArtifacts());
}

} // namespace PhantomLedger::exporter::card_fraud
