#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"
#include "phantomledger/taxonomies/landlords/names.hpp"
#include "phantomledger/taxonomies/merchants/names.hpp"

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

namespace PhantomLedger::exporter::standard {

namespace detail {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

namespace ent = ::PhantomLedger::entity;
namespace merch = ::PhantomLedger::entity::merchant;
namespace ll = ::PhantomLedger::entity::landlord;
namespace ids = ::PhantomLedger::identifiers;
namespace land = ::PhantomLedger::landlords;
namespace enc = ::PhantomLedger::encoding;

struct RowLabel {
  std::string_view kind;
  std::string_view category;
};

inline constexpr RowLabel kUnknown{"external_account", "unknown"};
inline constexpr RowLabel kLandlord{"landlord_external", "landlord"};

[[nodiscard]] consteval std::array<RowLabel, ids::kRoleCount> roleLabels() {
  std::array<RowLabel, ids::kRoleCount> out{};

  out.fill(kUnknown);

  out[enumTax::toIndex(ids::Role::employer)] = {"employer_external",
                                                "employer"};
  out[enumTax::toIndex(ids::Role::client)] = {"client_external", "client"};
  out[enumTax::toIndex(ids::Role::platform)] = {"platform_external",
                                                "platform"};
  out[enumTax::toIndex(ids::Role::processor)] = {"processor_external",
                                                 "processor"};
  out[enumTax::toIndex(ids::Role::family)] = {"family_external", "family"};
  out[enumTax::toIndex(ids::Role::business)] = {"business_external",
                                                "business"};
  out[enumTax::toIndex(ids::Role::brokerage)] = {"brokerage_external",
                                                 "brokerage"};

  return out;
}

inline constexpr auto kRoleLabels = roleLabels();

[[nodiscard]] constexpr RowLabel labelFor(ids::Role role,
                                          ids::Bank bank) noexcept {
  if (bank != ids::Bank::external) {
    return kUnknown;
  }

  return kRoleLabels[enumTax::toIndex(role)];
}

[[nodiscard]] inline std::string landlordCategory(ll::Type type) {
  const auto name = land::name(type);

  std::string out;
  out.reserve(std::string_view{"landlord_"}.size() + name.size());
  out.append("landlord_");
  out.append(name);

  return out;
}

[[nodiscard]] inline std::string landlordKind(std::string_view category) {
  std::string out;
  out.reserve(category.size() + std::string_view{"_external"}.size());
  out.append(category);
  out.append("_external");

  return out;
}

inline void writeLandlordRow(::PhantomLedger::exporter::csv::Writer &w,
                             std::string_view rendered, ll::Type type) {
  const auto category = landlordCategory(type);
  const auto kind = landlordKind(category);

  w.writeRow(rendered, kind, category);
}

inline void writeFallbackLandlordRow(::PhantomLedger::exporter::csv::Writer &w,
                                     std::string_view rendered) {
  w.writeRow(rendered, kLandlord.kind, kLandlord.category);
}

/// Build a map keyed by counterparty Key for fast merchant lookup.
[[nodiscard]] inline std::unordered_map<ent::Key,
                                        ::PhantomLedger::merchants::Category>
merchantIndex(const merch::Catalog &catalog) {
  std::unordered_map<ent::Key, ::PhantomLedger::merchants::Category> out;

  out.reserve(catalog.records.size());

  for (const auto &rec : catalog.records) {
    if (rec.counterpartyId.bank == ids::Bank::external) {
      out.emplace(rec.counterpartyId, rec.category);
    }
  }

  return out;
}

/// Build a map keyed by external landlord Key for fast type lookup.
[[nodiscard]] inline std::unordered_map<ent::Key, ll::Type>
landlordIndex(const ll::Roster &roster) {
  std::unordered_map<ent::Key, ll::Type> out;

  out.reserve(roster.records.size());

  for (const auto &rec : roster.records) {
    if (rec.accountId.bank == ids::Bank::external) {
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
  const auto merchantsById = detail::merchantIndex(merchants);
  const auto landlordsById = detail::landlordIndex(landlords);

  using ::PhantomLedger::entity::Role;
  using ::PhantomLedger::entity::account::bit;
  using ::PhantomLedger::entity::account::Flag;

  for (const auto &record : registry.records) {
    if ((record.flags & bit(Flag::external)) == 0) {
      continue;
    }

    const auto &id = record.id;
    const auto rendered = detail::enc::format(id);
    const auto view = rendered.view();

    if (id.role == Role::merchant) {
      const auto it = merchantsById.find(id);

      if (it != merchantsById.end()) {
        const auto category = ::PhantomLedger::merchants::name(it->second);
        w.writeRow(view, std::string_view{"merchant_external"}, category);
        continue;
      }
    }

    if (id.role == Role::landlord) {
      const auto it = landlordsById.find(id);

      if (it != landlordsById.end()) {
        detail::writeLandlordRow(w, view, it->second);
        continue;
      }

      detail::writeFallbackLandlordRow(w, view);
      continue;
    }

    const auto label = detail::labelFor(id.role, id.bank);
    w.writeRow(view, label.kind, label.category);
  }
}

} // namespace PhantomLedger::exporter::standard
