#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/taxonomies/merchants/names.hpp"

#include <string_view>
#include <unordered_map>

namespace PhantomLedger::exporter::standard {

namespace detail {

struct ExternalLabel {
  std::string_view kind;
  std::string_view category;
};

[[nodiscard]] inline ExternalLabel
defaultLabelFor(::PhantomLedger::entity::Role role,
                ::PhantomLedger::entity::Bank bank) noexcept {
  using ::PhantomLedger::entity::Bank;
  using ::PhantomLedger::entity::Role;

  if (bank != Bank::external) {
    return {"external_account", "unknown"};
  }

  switch (role) {
  case Role::employer:
    return {"employer_external", "employer"};
  case Role::client:
    return {"client_external", "client"};
  case Role::platform:
    return {"platform_external", "platform"};
  case Role::processor:
    return {"processor_external", "processor"};
  case Role::family:
    return {"family_external", "family"};
  case Role::business:
    return {"business_external", "business"};
  case Role::brokerage:
    return {"brokerage_external", "brokerage"};
  case Role::merchant:
  case Role::landlord:
    return {"external_account", "unknown"};
  case Role::customer:
  case Role::account:
  case Role::card:
    break;
  }
  return {"external_account", "unknown"};
}

[[nodiscard]] inline ExternalLabel
labelForLandlordClass(::PhantomLedger::entity::landlord::Class kind) noexcept {
  using ::PhantomLedger::entity::landlord::Class;
  switch (kind) {
  case Class::individual:
    return {"landlord_individual_external", "landlord_individual"};
  case Class::llcSmall:
    return {"landlord_small_llc_external", "landlord_small_llc"};
  case Class::corporate:
    return {"landlord_corporate_external", "landlord_corporate"};
  }
  return {"landlord_external", "landlord"};
}

/// Build a map keyed by counterparty Key for fast merchant lookup.
[[nodiscard]] inline std::unordered_map<::PhantomLedger::entity::Key,
                                        ::PhantomLedger::merchants::Category>
buildMerchantCategoryIndex(
    const ::PhantomLedger::entity::merchant::Catalog &catalog) {
  std::unordered_map<::PhantomLedger::entity::Key,
                     ::PhantomLedger::merchants::Category>
      out;
  out.reserve(catalog.records.size());
  for (const auto &rec : catalog.records) {
    if (rec.counterpartyId.bank == ::PhantomLedger::entity::Bank::external) {
      out.emplace(rec.counterpartyId, rec.category);
    }
  }
  return out;
}

/// Build a map keyed by external landlord Key for fast typology lookup.
[[nodiscard]] inline std::unordered_map<
    ::PhantomLedger::entity::Key, ::PhantomLedger::entity::landlord::Class>
buildLandlordClassIndex(
    const ::PhantomLedger::entity::landlord::Roster &roster) {
  std::unordered_map<::PhantomLedger::entity::Key,
                     ::PhantomLedger::entity::landlord::Class>
      out;
  out.reserve(roster.records.size());
  for (const auto &rec : roster.records) {
    if (rec.accountId.bank == ::PhantomLedger::entity::Bank::external) {
      out.emplace(rec.accountId, rec.type);
    }
  }
  return out;
}

} // namespace detail

inline void writeExternalAccountRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::entity::account::Registry &registry,
    const ::PhantomLedger::entity::merchant::Catalog &merchants,
    const ::PhantomLedger::entity::landlord::Roster &landlords) {
  const auto merchantIndex = detail::buildMerchantCategoryIndex(merchants);
  const auto landlordIndex = detail::buildLandlordClassIndex(landlords);

  using ::PhantomLedger::entity::Role;
  using ::PhantomLedger::entity::account::bit;
  using ::PhantomLedger::entity::account::Flag;

  for (const auto &record : registry.records) {
    if ((record.flags & bit(Flag::external)) == 0) {
      continue;
    }

    const auto &id = record.id;

    // Merchant externals: read category from the catalog index.
    if (id.role == Role::merchant) {
      const auto it = merchantIndex.find(id);
      if (it != merchantIndex.end()) {
        const auto categoryName = ::PhantomLedger::merchants::name(it->second);
        w.writeRow(::PhantomLedger::encoding::format(id),
                   std::string_view{"merchant_external"}, categoryName);
        continue;
      }
    }

    // Landlord externals: read typology from the landlord index.
    if (id.role == Role::landlord) {
      const auto it = landlordIndex.find(id);
      if (it != landlordIndex.end()) {
        const auto label = detail::labelForLandlordClass(it->second);
        w.writeRow(::PhantomLedger::encoding::format(id), label.kind,
                   label.category);
        continue;
      }
      w.writeRow(::PhantomLedger::encoding::format(id),
                 std::string_view{"landlord_external"},
                 std::string_view{"landlord"});
      continue;
    }

    const auto label = detail::defaultLabelFor(id.role, id.bank);
    w.writeRow(::PhantomLedger::encoding::format(id), label.kind,
               label.category);
  }
}

} // namespace PhantomLedger::exporter::standard
