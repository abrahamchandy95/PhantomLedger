#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/mule_ml/canonical.hpp"
#include "phantomledger/exporter/mule_ml/identity.hpp"

#include <string>
#include <unordered_map>

namespace PhantomLedger::exporter::mule_ml {

struct PartyInputs {
  const ::PhantomLedger::entities::synth::pii::PoolSet *piiPools = nullptr;

  const CanonicalMap *canonical = nullptr;
};

namespace detail {

[[nodiscard]] inline bool
isPartyFraud(const ::PhantomLedger::entity::account::Record &record,
             const ::PhantomLedger::entity::person::Roster &peopleRoster) {
  using ::PhantomLedger::entity::account::bit;
  using ::PhantomLedger::entity::account::Flag;

  if ((record.flags & bit(Flag::mule)) != 0) {
    return true;
  }
  if ((record.flags & bit(Flag::fraud)) != 0) {
    return true;
  }
  if (record.owner != ::PhantomLedger::entity::invalidPerson) {
    using ::PhantomLedger::entity::person::Flag;
    if (peopleRoster.has(record.owner, Flag::soloFraud)) {
      return true;
    }
  }
  return false;
}

} // namespace detail

inline void
writePartyRows(::PhantomLedger::exporter::csv::Writer &w,
               const ::PhantomLedger::entity::account::Registry &accounts,
               const ::PhantomLedger::entity::person::Roster &peopleRoster,
               const ::PhantomLedger::entity::pii::Roster &piiRoster,
               const PartyInputs &inputs) {
  for (const auto &record : accounts.records) {
    const bool isInternal =
        (record.owner != ::PhantomLedger::entity::invalidPerson);

    std::string phoneStr;
    std::string emailStr;
    if (isInternal && record.owner <= piiRoster.records.size()) {
      const auto &piiRec = piiRoster.records[record.owner - 1];
      phoneStr = std::string{piiRec.phone.view()};
      emailStr = std::string{piiRec.email.view()};
    }

    PartyIdentity identity{};
    if (isInternal && inputs.piiPools != nullptr &&
        record.owner <= piiRoster.records.size()) {
      const auto &piiRec = piiRoster.records[record.owner - 1];
      identity = renderIdentity(piiRec, *inputs.piiPools);
    }

    std::string deviceStr;
    std::string ipStr;
    if (inputs.canonical != nullptr) {
      if (const auto it = inputs.canonical->find(record.id);
          it != inputs.canonical->end()) {
        deviceStr = it->second.deviceId;
        ipStr = it->second.ipAddress;
      }
    }

    w.writeRow(::PhantomLedger::encoding::format(record.id),
               detail::isPartyFraud(record, peopleRoster), phoneStr, emailStr,
               identity.name, identity.ssn, identity.dob, identity.address,
               identity.state, identity.city, identity.zipcode,
               identity.country, ipStr, deviceStr);
  }
}

} // namespace PhantomLedger::exporter::mule_ml
