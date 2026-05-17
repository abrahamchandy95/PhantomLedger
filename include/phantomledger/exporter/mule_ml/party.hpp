#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/mule_ml/canonical.hpp"
#include "phantomledger/exporter/mule_ml/identity.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

namespace PhantomLedger::exporter::mule_ml {

struct PartyInputs {
  const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

  const CanonicalMap *canonical = nullptr;
};

namespace detail {

namespace ent = ::PhantomLedger::entity;
namespace enc = ::PhantomLedger::encoding;

[[nodiscard]] inline bool
isPartyFraud(const ent::account::Record &record,
             const ent::person::Roster &peopleRoster) {
  using ent::account::bit;
  using ent::account::Flag;

  if ((record.flags & bit(Flag::mule)) != 0) {
    return true;
  }
  if ((record.flags & bit(Flag::fraud)) != 0) {
    return true;
  }
  if (record.owner != ent::invalidPerson) {
    using ent::person::Flag;
    if (peopleRoster.has(record.owner, Flag::soloFraud)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline bool isInternal(const ent::account::Record &record) {
  return record.owner != ent::invalidPerson;
}

inline void writeIdentityCells(::PhantomLedger::exporter::csv::Writer &w,
                               const ent::account::Record &record,
                               bool isFraud) {
  w.cell(enc::format(record.id).view()).cell(isFraud);
}

inline void writeContactCells(::PhantomLedger::exporter::csv::Writer &w,
                              std::string_view phone, std::string_view email) {
  w.cell(phone).cell(email);
}

inline void writeNameCells(::PhantomLedger::exporter::csv::Writer &w,
                           const PartyIdentity &identity) {
  w.cell(identity.name).cell(identity.ssn).cell(identity.dob);
}

inline void writeAddressCells(::PhantomLedger::exporter::csv::Writer &w,
                              const PartyIdentity &identity) {
  w.cell(identity.address)
      .cell(identity.state)
      .cell(identity.city)
      .cell(identity.zipcode)
      .cell(identity.country);
}

inline void writeSessionCells(::PhantomLedger::exporter::csv::Writer &w,
                              std::string_view ip, std::string_view device) {
  w.cell(ip).cell(device);
}

struct ContactInfo {
  std::string phone;
  std::string email;
};

[[nodiscard]] inline ContactInfo
resolveContact(const ent::account::Record &record,
               const ent::pii::Roster &piiRoster) {
  ContactInfo out;
  if (isInternal(record) && record.owner <= piiRoster.records.size()) {
    const auto &piiRec = piiRoster.records[record.owner - 1];
    out.phone = std::string{piiRec.phone.view()};
    out.email = std::string{piiRec.email.view()};
  }
  return out;
}

[[nodiscard]] inline PartyIdentity
resolvePartyIdentity(const ent::account::Record &record,
                     const ent::pii::Roster &piiRoster,
                     const PartyInputs &inputs) {
  if (!isInternal(record) || inputs.piiPools == nullptr ||
      record.owner > piiRoster.records.size()) {
    return {};
  }
  const auto &piiRec = piiRoster.records[record.owner - 1];
  return renderIdentity(piiRec, *inputs.piiPools);
}

struct SessionInfo {
  std::string ip;
  std::string device;
};

[[nodiscard]] inline SessionInfo
resolveSession(const ent::account::Record &record, const PartyInputs &inputs) {
  SessionInfo out;
  if (inputs.canonical == nullptr) {
    return out;
  }
  if (const auto it = inputs.canonical->find(record.id);
      it != inputs.canonical->end()) {
    out.ip = it->second.ipAddress;
    out.device = it->second.deviceId;
  }
  return out;
}

} // namespace detail

inline void
writePartyRows(::PhantomLedger::exporter::csv::Writer &w,
               const ::PhantomLedger::entity::account::Registry &accounts,
               const ::PhantomLedger::entity::person::Roster &peopleRoster,
               const ::PhantomLedger::entity::pii::Roster &piiRoster,
               const PartyInputs &inputs) {
  for (const auto &record : accounts.records) {
    const auto contact = detail::resolveContact(record, piiRoster);
    const auto identity =
        detail::resolvePartyIdentity(record, piiRoster, inputs);
    const auto session = detail::resolveSession(record, inputs);

    detail::writeIdentityCells(w, record,
                               detail::isPartyFraud(record, peopleRoster));
    detail::writeContactCells(w, contact.phone, contact.email);
    detail::writeNameCells(w, identity);
    detail::writeAddressCells(w, identity);
    detail::writeSessionCells(w, session.ip, session.device);
    w.endRow();
  }
}

} // namespace PhantomLedger::exporter::mule_ml
