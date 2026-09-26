#include "phantomledger/synth/counterparties/remote_payees.hpp"

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/geo/catalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace PhantomLedger::synth::counterparties::remote {

namespace {

namespace geo = ::PhantomLedger::entity::geography;
namespace merchant = ::PhantomLedger::entity::merchant;

// --- Draw-free uniforms --------------------------------------------------
//
// A splitmix64 finalizer over a per-purpose domain word, the person id and
// the row's salt, so no two choices here share bits with each other or with
// the affinity and instrument hashes.

[[nodiscard]] constexpr std::uint64_t splitmix(std::uint64_t value) noexcept {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

inline constexpr std::uint64_t kCheckRouteDomain = 0x5245'4D43'4852'5401ULL;
inline constexpr std::uint64_t kCheckSlotDomain = 0x5245'4D43'534C'5401ULL;
inline constexpr std::uint64_t kCheckBankDomain = 0x5245'4D43'424E'4B01ULL;
inline constexpr std::uint64_t kRemoteMerchantDomain = 0x5245'4D4D'4552'4301ULL;
inline constexpr std::uint64_t kP2pPlatformDomain = 0x5245'4D50'3250'5001ULL;
inline constexpr std::uint64_t kFuneralHomeDomain = 0x5245'4D46'554E'5201ULL;

[[nodiscard]] std::uint64_t mix(std::uint64_t domain, entity::PersonId person,
                                std::uint64_t salt) noexcept {
  return splitmix(splitmix(domain ^ splitmix(person)) ^
                  splitmix(salt + 0x632B'E59B'D9B4'E019ULL));
}

[[nodiscard]] double toUnit(std::uint64_t bits) noexcept {
  return static_cast<double>(bits >> 11U) * 0x1.0p-53;
}

// --- The FDIC Summary of Deposits table ----------------------------------
//
// June 30, 2024 deposits summed by institution: 4,548 institutions, $17.405T
// across 76,727 offices (banks.data.fdic.gov/api/sod, YEAR:2024, agg_by=CERT,
// agg_sum_fields=DEPSUMBR, read 25 September 2026) [P]. Ranks 1-25 are the
// exact shares of total deposits; the anchors are the table's own cumulative
// shares, with a 1/rank shape between them. The pool stops at 1,000 (CHOICE;
// the 3,548 smaller institutions hold 5.49%) and is renormalized by the
// top-1,000 mass.
constexpr std::array kSodNamed{
    0.11541, // JPMorgan Chase Bank
    0.10926, // Bank of America
    0.08062, // Wells Fargo Bank
    0.04271, // Citibank
    0.03029, // U.S. Bank
    0.02429, // PNC Bank
    0.02283, // Truist Bank
    0.02140, // Capital One
    0.02050, // Goldman Sachs Bank USA
    0.01667, // TD Bank
    0.01287, // Charles Schwab Bank
    0.01188, // BMO Bank
    0.01161, // The Bank of New York Mellon
    0.01083, // Morgan Stanley Private Bank
    0.01035, // Citizens Bank
    0.01031, // State Street Bank and Trust
    0.00989, // Morgan Stanley Bank
    0.00988, // Fifth Third Bank
    0.00940, // Manufacturers and Traders Trust
    0.00915, // The Huntington National Bank
    0.00894, // Ally Bank
    0.00870, // First-Citizens Bank & Trust
    0.00858, // KeyBank
    0.00854, // American Express National Bank
    0.00741, // Regions Bank
};

struct SodAnchor {
  std::uint32_t rank;
  double cumulative;
};

constexpr std::array kSodAnchors{
    SodAnchor{50, 0.7251},  SodAnchor{100, 0.7931},  SodAnchor{250, 0.8651},
    SodAnchor{500, 0.9079}, SodAnchor{1000, 0.9451},
};

// Built once: the normalized cumulative share per rank.
[[nodiscard]] std::vector<double> buildSodCdf() {
  std::vector<double> shares(kSodNamed.begin(), kSodNamed.end());
  double cumulative = 0.0;
  for (const double share : kSodNamed) {
    cumulative += share;
  }

  auto rank = static_cast<std::uint32_t>(kSodNamed.size());
  for (const auto &anchor : kSodAnchors) {
    const double mass = anchor.cumulative - cumulative;
    double harmonic = 0.0;
    for (std::uint32_t r = rank + 1; r <= anchor.rank; ++r) {
      harmonic += 1.0 / static_cast<double>(r);
    }
    for (std::uint32_t r = rank + 1; r <= anchor.rank; ++r) {
      const double share = mass / (static_cast<double>(r) * harmonic);
      if (!(share > 0.0) || share > shares.back()) {
        throw std::logic_error(
            "remote payees: the SOD 1/rank tail must stay positive and "
            "nonincreasing");
      }
      shares.push_back(share);
    }
    rank = anchor.rank;
    cumulative = anchor.cumulative;
  }

  std::vector<double> cdf;
  cdf.reserve(shares.size());
  double running = 0.0;
  for (const double share : shares) {
    running += share / cumulative;
    cdf.push_back(running);
  }
  cdf.back() = 1.0;
  return cdf;
}

[[nodiscard]] const std::vector<double> &sodCdf() {
  static const std::vector<double> cdf = buildSodCdf();
  return cdf;
}

[[nodiscard]] std::uint32_t rankFor(const std::vector<double> &cdf,
                                    double u) noexcept {
  const auto it = std::upper_bound(cdf.begin(), cdf.end(), u);
  const auto index = std::min<std::size_t>(
      static_cast<std::size_t>(it - cdf.begin()), cdf.size() - 1U);
  return static_cast<std::uint32_t>(index) + 1U;
}

// --- P2P platform table --------------------------------------------------
//
// Venmo: more than 64 million monthly active accounts, Q4 2024 (PayPal
// Holdings Q4 2024 earnings call) [P]. Cash App: 57 million monthly
// transacting actives, December 2024 (Block, Inc. Form 10-K FY2024) [P].
inline constexpr double kVenmoMonthlyActives = 64.0;
inline constexpr double kCashAppMonthlyActives = 57.0;
inline constexpr double kVenmoShare =
    kVenmoMonthlyActives / (kVenmoMonthlyActives + kCashAppMonthlyActives);

} // namespace

// --- The check route -----------------------------------------------------

double checkShareForYear(int year) noexcept {
  if (year <= kCheckShareEarlyYear) {
    return kCheckShareEarly;
  }
  if (year >= kCheckShareLateYear) {
    return kCheckShareLate;
  }
  const double t =
      static_cast<double>(year - kCheckShareEarlyYear) /
      static_cast<double>(kCheckShareLateYear - kCheckShareEarlyYear);
  return kCheckShareEarly + t * (kCheckShareLate - kCheckShareEarly);
}

double checkFractionOfSlot(int year, double slotShare) noexcept {
  if (!(slotShare > 0.0)) {
    return 1.0;
  }
  return std::min(1.0, checkShareForYear(year) / slotShare);
}

double checkRouteUniform(entity::PersonId person,
                         std::int64_t timestamp) noexcept {
  return toUnit(
      mix(kCheckRouteDomain, person, static_cast<std::uint64_t>(timestamp)));
}

std::uint32_t checkBankPoolSize() noexcept {
  return static_cast<std::uint32_t>(sodCdf().size());
}

double checkBankShare(std::uint32_t rank) {
  const auto &cdf = sodCdf();
  if (rank == 0 || rank > cdf.size()) {
    throw std::out_of_range("remote payees: check-bank rank outside the pool");
  }
  return rank == 1 ? cdf[0] : cdf[rank - 1U] - cdf[rank - 2U];
}

double checkBankCumulativeShare(std::uint32_t rank) {
  const auto &cdf = sodCdf();
  if (rank == 0) {
    return 0.0;
  }
  return cdf[std::min<std::size_t>(rank, cdf.size()) - 1U];
}

std::uint32_t checkBankRankFor(entity::PersonId person,
                               std::uint32_t payeeSlot) noexcept {
  return rankFor(sodCdf(), toUnit(mix(kCheckBankDomain, person, payeeSlot)));
}

std::uint32_t checkPayeeSlot(entity::PersonId person,
                             std::int64_t timestamp) noexcept {
  return static_cast<std::uint32_t>(
      mix(kCheckSlotDomain, person, static_cast<std::uint64_t>(timestamp)) %
      kCheckPayeesPerPerson);
}

entity::Key checkPayeeBank(entity::PersonId person,
                           std::int64_t timestamp) noexcept {
  return keys::checkBank(
      checkBankRankFor(person, checkPayeeSlot(person, timestamp)));
}

std::vector<entity::Key> checkBanksInUse(std::uint32_t personCount) {
  std::vector<bool> used(static_cast<std::size_t>(checkBankPoolSize()) + 1U,
                         false);
  for (entity::PersonId person = 1; person <= personCount; ++person) {
    for (std::uint32_t slot = 0; slot < kCheckPayeesPerPerson; ++slot) {
      used[checkBankRankFor(person, slot)] = true;
    }
  }
  std::vector<entity::Key> out;
  for (std::uint32_t rank = 1; rank < used.size(); ++rank) {
    if (used[rank]) {
      out.push_back(keys::checkBank(rank));
    }
  }
  return out;
}

// --- Identified remote merchants ---------------------------------------

bool RemoteMerchantTable::eligible(const merchant::Record &record) noexcept {
  return record.counterpartyId.bank == entity::Bank::external &&
         record.weight > 0.0 &&
         (record.footprint == merchant::Footprint::online ||
          record.footprint == merchant::Footprint::nationalService);
}

RemoteMerchantTable
RemoteMerchantTable::build(const merchant::Catalog &catalog) {
  RemoteMerchantTable out;
  double running = 0.0;
  for (std::size_t i = 0; i < catalog.records.size(); ++i) {
    const auto &record = catalog.records[i];
    if (!eligible(record)) {
      continue;
    }
    running += record.weight;
    out.indices_.push_back(static_cast<std::uint32_t>(i));
    out.cdf_.push_back(running);
  }
  for (auto &value : out.cdf_) {
    value /= running;
  }
  if (!out.cdf_.empty()) {
    out.cdf_.back() = 1.0;
  }
  return out;
}

std::optional<std::uint32_t>
RemoteMerchantTable::pick(const merchant::Catalog &catalog,
                          entity::PersonId person,
                          std::int64_t timestamp) const noexcept {
  if (indices_.empty()) {
    return std::nullopt;
  }
  for (std::uint32_t attempt = 0; attempt < kRemoteMerchantAttempts;
       ++attempt) {
    const auto salt = splitmix(static_cast<std::uint64_t>(timestamp)) ^ attempt;
    const double u = toUnit(mix(kRemoteMerchantDomain, person, salt));
    const auto it = std::upper_bound(cdf_.begin(), cdf_.end(), u);
    const auto slot = std::min<std::size_t>(
        static_cast<std::size_t>(it - cdf_.begin()), cdf_.size() - 1U);
    const auto index = indices_[slot];
    if (catalog.records[index].liveAt(timestamp)) {
      return index;
    }
  }
  return std::nullopt;
}

// --- P2P platforms -----------------------------------------------------

double p2pPlatformShare(keys::P2pPlatform platform) noexcept {
  return platform == keys::P2pPlatform::venmo ? kVenmoShare : 1.0 - kVenmoShare;
}

entity::Key p2pPlatformFor(entity::PersonId person) noexcept {
  const double u = toUnit(mix(kP2pPlatformDomain, person, 0));
  return keys::p2pPlatform(u < kVenmoShare ? keys::P2pPlatform::venmo
                                           : keys::P2pPlatform::cashApp);
}

std::vector<entity::Key> p2pPlatforms() {
  return {keys::p2pPlatform(keys::P2pPlatform::venmo),
          keys::p2pPlatform(keys::P2pPlatform::cashApp)};
}

// --- Funeral homes -----------------------------------------------------

std::uint32_t funeralHomeCount(geo::GeoAreaId area) {
  const auto &catalog = ::PhantomLedger::synth::geo::geography();
  if (!catalog.contains(area)) {
    return 1U;
  }
  const double expected = static_cast<double>(catalog.at(area).population) *
                          kFuneralHomeEstablishments / kFuneralHomePopulation;
  const auto rounded = static_cast<std::uint32_t>(std::llround(expected));
  return std::clamp<std::uint32_t>(rounded, 1U, keys::kMaxFuneralHomesPerArea);
}

geo::GeoAreaId
homeAreaAt(std::span<const geo::GeoAreaId> homeAreas,
           const entity::parties::relocation::Schedule *relocation,
           entity::PersonId person, std::int64_t timestamp) noexcept {
  auto area = geo::invalidGeoArea;
  if (person != entity::invalidPerson && person <= homeAreas.size()) {
    area = homeAreas[person - 1U];
  }
  if (relocation != nullptr) {
    const auto atDate = relocation->areaAt(person, timestamp);
    if (geo::validArea(atDate)) {
      area = atDate;
    }
  }
  return area;
}

entity::Key funeralHomeFor(geo::GeoAreaId area, entity::PersonId decedent) {
  const auto count = funeralHomeCount(area);
  const auto ordinal = static_cast<std::uint32_t>(
                           mix(kFuneralHomeDomain, decedent, area) % count) +
                       1U;
  return keys::funeralHome(area, ordinal);
}

std::vector<entity::Key> funeralHomesInUse(
    std::span<const ::PhantomLedger::synth::personas::timeline::Timeline>
        timelines,
    std::span<const geo::GeoAreaId> homeAreas,
    const entity::parties::relocation::Schedule *relocation,
    ::PhantomLedger::time::Window window) {
  const auto start = ::PhantomLedger::time::toEpochSeconds(window.start);
  const auto end = ::PhantomLedger::time::toEpochSeconds(window.endExcl());

  std::vector<entity::Key> out;
  for (std::size_t i = 0; i < timelines.size(); ++i) {
    const auto death =
        ::PhantomLedger::time::toEpochSeconds(timelines[i].death);
    if (death < start || death >= end) {
      continue;
    }
    const auto person = static_cast<entity::PersonId>(i + 1U);
    out.push_back(funeralHomeFor(
        homeAreaAt(homeAreas, relocation, person, death), person));
  }
  std::ranges::sort(out);
  out.erase(std::ranges::unique(out).begin(), out.end());
  return out;
}

} // namespace PhantomLedger::synth::counterparties::remote
