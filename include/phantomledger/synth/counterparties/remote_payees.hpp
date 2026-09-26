#pragma once
//
// phantomledger/synth/counterparties/remote_payees.hpp
//
// WHO a retired external-unknown row pays (unknown-counterparty-2026-09).
// The catch-all took the spending router's external-unknown slot, P2P with no
// usable contact, and every funeral. Each now pays the counterparty a bank
// actually records for it:
//
//   * the slot splits into PAID CHECKS (keyed by the payee's bank, the FDIC
//     Summary of Deposits share table) and IDENTIFIED REMOTE MERCHANTS (the
//     external online and national-service catalogue outlets), at the DCPC
//     check share of consumer payments for the row's year;
//   * P2P with no usable contact goes to a named P2P platform, one per
//     person, weighted by the platforms' monthly actives;
//   * a funeral pays a funeral home in the decedent's city, at the Census CBP
//     density of funeral homes per resident.
//
// EVERY CHOICE HERE IS DRAW-FREE. Each one is a splitmix hash of the person
// id (and, per row, the row's own timestamp), the construction
// market/commerce/affinity.hpp and actors/instruments.hpp already use, so no
// RNG stream spends a uniform and every amount, timestamp and decline is
// byte-identical to the catch-all build. Only the destination moves. The
// authority rows are the unknown-counterparty amendment in
// docs/fraud_model_audit.md.
//

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/counterparties/remote_payees.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/personas/timeline.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace PhantomLedger::synth::counterparties::remote {

namespace keys = ::PhantomLedger::counterparties::remote;

// --- The check route ---------------------------------------------------

// DCPC check share of consumer payments by number: 7% in 2016, 3% in 2024.
// Linear between the two anchors (a declared shape), flat outside them.
inline constexpr int kCheckShareEarlyYear = 2016;
inline constexpr double kCheckShareEarly = 0.07;
inline constexpr int kCheckShareLateYear = 2024;
inline constexpr double kCheckShareLate = 0.03;

[[nodiscard]] double checkShareForYear(int year) noexcept;

// The share of the external-unknown SLOT that is a paid check: the year's
// check share over the slot's probability mass, capped at 1. A slot with no
// mass emits nothing, so the answer there is 1 by convention.
[[nodiscard]] double checkFractionOfSlot(int year, double slotShare) noexcept;

// Uniform deciding check route versus remote-merchant route for one row.
[[nodiscard]] double checkRouteUniform(entity::PersonId person,
                                       std::int64_t timestamp) noexcept;

// Each person writes checks to this many payees (CHOICE), each banking at a
// bank drawn once from the SOD table.
inline constexpr std::uint32_t kCheckPayeesPerPerson = 4;

// The SOD pool: 1,000 institutions, shares normalized over the pool.
[[nodiscard]] std::uint32_t checkBankPoolSize() noexcept;
[[nodiscard]] double checkBankShare(std::uint32_t rank);
[[nodiscard]] double checkBankCumulativeShare(std::uint32_t rank);

[[nodiscard]] std::uint32_t checkBankRankFor(entity::PersonId person,
                                             std::uint32_t payeeSlot) noexcept;
[[nodiscard]] std::uint32_t checkPayeeSlot(entity::PersonId person,
                                           std::int64_t timestamp) noexcept;
[[nodiscard]] entity::Key checkPayeeBank(entity::PersonId person,
                                         std::int64_t timestamp) noexcept;

// Every bank some person's payee slots use, in rank order. The entity stage
// registers exactly these.
[[nodiscard]] std::vector<entity::Key>
checkBanksInUse(std::uint32_t personCount);

// --- Identified remote merchants ---------------------------------------

inline constexpr std::uint32_t kRemoteMerchantAttempts = 8;

// The external catalogue outlets a customer pays remotely: footprint
// `online` or `nationalService`, external bank, weighted by volume weight.
// Internal (on-us) merchants are excluded: crediting one would move a
// customer balance, and only the destination may change.
class RemoteMerchantTable {
public:
  [[nodiscard]] static RemoteMerchantTable
  build(const entity::merchant::Catalog &catalog);

  [[nodiscard]] bool empty() const noexcept { return indices_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return indices_.size(); }

  // The catalogue index this row pays, or nullopt when no candidate is live
  // at `timestamp` within kRemoteMerchantAttempts hash-derived tries (the
  // caller then takes the check route).
  [[nodiscard]] std::optional<std::uint32_t>
  pick(const entity::merchant::Catalog &catalog, entity::PersonId person,
       std::int64_t timestamp) const noexcept;

  [[nodiscard]] static bool
  eligible(const entity::merchant::Record &record) noexcept;

private:
  std::vector<std::uint32_t> indices_;
  std::vector<double> cdf_;
};

// --- P2P platforms -----------------------------------------------------

[[nodiscard]] double p2pPlatformShare(keys::P2pPlatform platform) noexcept;
[[nodiscard]] entity::Key p2pPlatformFor(entity::PersonId person) noexcept;
[[nodiscard]] std::vector<entity::Key> p2pPlatforms();

// --- Funeral homes -----------------------------------------------------

// Census CBP 2022, NAICS 812210 establishments, over the 2022 population.
inline constexpr double kFuneralHomeEstablishments = 15'375.0;
inline constexpr double kFuneralHomePopulation = 334'017'321.0;

// max(1, round(area population x density)); 1 for an area the geo catalogue
// does not hold (area 0 included).
[[nodiscard]] std::uint32_t funeralHomeCount(entity::geography::GeoAreaId area);

// The home area occupied at `timestamp`, resolved the way the cash-point
// rails resolve it: the window-start carrier, overridden by the relocation
// schedule when it has an answer.
[[nodiscard]] entity::geography::GeoAreaId
homeAreaAt(std::span<const entity::geography::GeoAreaId> homeAreas,
           const entity::parties::relocation::Schedule *relocation,
           entity::PersonId person, std::int64_t timestamp) noexcept;

[[nodiscard]] entity::Key funeralHomeFor(entity::geography::GeoAreaId area,
                                         entity::PersonId decedent);

// The funeral home of every person who dies inside `window`, in key order.
// A superset of the homes actually paid: a death in the window's last days
// can post its funeral after the window closes.
[[nodiscard]] std::vector<entity::Key> funeralHomesInUse(
    std::span<const ::PhantomLedger::synth::personas::timeline::Timeline>
        timelines,
    std::span<const entity::geography::GeoAreaId> homeAreas,
    const entity::parties::relocation::Schedule *relocation,
    ::PhantomLedger::time::Window window);

} // namespace PhantomLedger::synth::counterparties::remote
