#include "phantomledger/exporter/aml/identity.hpp"

#include "phantomledger/exporter/aml/shared.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"
#include "phantomledger/taxonomies/locale/us_banks.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace PhantomLedger::exporter::aml::identity {

namespace {

namespace pii_ns = ::PhantomLedger::entities::synth::pii;

template <std::size_t N>
void appendBytes(StackString<N> &out, std::string_view bytes) noexcept {
  const auto cap = N - out.len;
  const auto take = bytes.size() < cap ? bytes.size() : cap;
  std::memcpy(out.data.data() + out.len, bytes.data(), take);
  out.len = static_cast<std::uint8_t>(out.len + take);
}

template <std::size_t N>
void appendUnsignedZeroPadded(StackString<N> &out, unsigned value,
                              std::size_t width) noexcept {
  char tmp[12];
  std::size_t pos = sizeof(tmp);
  do {
    tmp[--pos] = static_cast<char>('0' + (value % 10U));
    value /= 10U;
  } while (value != 0U && pos > 0);
  const auto digits = sizeof(tmp) - pos;
  for (std::size_t i = digits; i < width; ++i) {
    if (out.len < N) {
      out.data[out.len++] = '0';
    }
  }
  appendBytes(out, std::string_view{tmp + pos, digits});
}

template <std::size_t N>
StackString<N> renderPersonId(std::string_view prefix,
                              ::PhantomLedger::entity::PersonId p) noexcept {
  StackString<N> out;
  appendBytes(out, prefix);
  // "C%010u"
  if (out.len < N) {
    out.data[out.len++] = 'C';
  }
  appendUnsignedZeroPadded(out, static_cast<unsigned>(p), 10);
  return out;
}

template <std::size_t N>
StackString<N> renderRawId(std::string_view prefix,
                           std::string_view rawId) noexcept {
  StackString<N> out;
  appendBytes(out, prefix);
  appendBytes(out, rawId);
  return out;
}

struct PersonSeed {
  std::array<char, 12> bytes{};
  std::uint8_t len = 0;
  [[nodiscard]] std::string_view view() const noexcept {
    return {bytes.data(), len};
  }
};

[[nodiscard]] PersonSeed
personSeed(::PhantomLedger::entity::PersonId p) noexcept {
  PersonSeed s;
  s.bytes[s.len++] = 'C';
  // 10-digit zero-padded.
  char tmp[10];
  auto value = static_cast<unsigned>(p);
  for (int i = 9; i >= 0; --i) {
    tmp[i] = static_cast<char>('0' + (value % 10U));
    value /= 10U;
  }
  std::memcpy(s.bytes.data() + s.len, tmp, sizeof(tmp));
  s.len = static_cast<std::uint8_t>(s.len + sizeof(tmp));
  return s;
}

// ────────── Pool helpers ──────────

template <std::size_t N>
[[nodiscard]] std::string_view
pick(const std::array<std::string_view, N> &items,
     std::uint64_t seed) noexcept {
  return items[seed % N];
}

inline constexpr std::array<std::string_view, 3> kOccStudent{
    "student",
    "intern",
    "part_time",
};
inline constexpr std::array<std::string_view, 1> kOccRetired{"retired"};
inline constexpr std::array<std::string_view, 5> kOccSalaried{
    "office_worker", "engineer", "manager", "teacher", "healthcare",
};
inline constexpr std::array<std::string_view, 4> kOccFreelancer{
    "freelancer",
    "consultant",
    "designer",
    "writer",
};
inline constexpr std::array<std::string_view, 3> kOccSmallBiz{
    "business_owner",
    "restaurateur",
    "contractor",
};
inline constexpr std::array<std::string_view, 4> kOccHnw{
    "executive",
    "investor",
    "attorney",
    "physician",
};

inline constexpr std::array<std::string_view, 5> kSingleStatuses{
    "single", "single", "single", "divorced", "widowed",
};

// ────────── Marriage probability by persona (× 100, integer math) ──────────

[[nodiscard]] unsigned
marriedPctFor(::PhantomLedger::personas::Type p) noexcept {
  using ::PhantomLedger::personas::Type;
  switch (p) {
  case Type::student:
    return 5;
  case Type::retiree:
    return 55;
  case Type::salaried:
    return 50;
  case Type::freelancer:
    return 35;
  case Type::smallBusiness:
    return 55;
  case Type::highNetWorth:
    return 60;
  }
  return 50;
}

template <typename Vec>
[[nodiscard]] std::string_view poolPick(const Vec &v,
                                        std::uint64_t seed) noexcept {
  if (v.empty()) {
    return {};
  }
  return v[seed % v.size()];
}

void fillStreetLine2(StackString<24> &out, std::uint64_t seedHash) noexcept {
  if ((seedHash % 5U) != 0U) {
    return;
  }
  const auto apt = 1U + static_cast<unsigned>((seedHash >> 4U) % 500U);
  appendBytes(out, std::string_view{"Apt "});
  // Up to 3 digits, no padding.
  char tmp[6];
  auto r = std::to_chars(tmp, tmp + sizeof(tmp), apt);
  appendBytes(out,
              std::string_view{tmp, static_cast<std::size_t>(r.ptr - tmp)});
}

[[nodiscard]] AddressRecord
buildAddressFromHash(std::string_view seedKey, std::string_view nameSpace,
                     std::string_view addressType,
                     const pii_ns::LocalePool &usPool) {
  const auto x = stableU64({seedKey, nameSpace});

  AddressRecord out;
  out.id = renderRawId<32>(std::string_view{"ADDR_"}, seedKey);

  out.streetLine1 = poolPick(usPool.streets, x);
  fillStreetLine2(out.streetLine2, x >> 24U);

  if (!usPool.zipTable.empty()) {
    const auto &zip = usPool.zipTable[static_cast<std::size_t>(x >> 16U) %
                                      usPool.zipTable.size()];
    out.city = zip.city;
    out.state = zip.adminCode;
    out.postalCode = zip.postalCode;
  }

  out.country =
      ::PhantomLedger::locale::code(::PhantomLedger::locale::Country::us);
  out.addressType = addressType;
  out.isHighRiskGeo = false;
  return out;
}

} // namespace

// ────────────────────────────────────────────────────────────────────
// Pure code/rating lookups
// ────────────────────────────────────────────────────────────────────

std::string_view
customerType(::PhantomLedger::personas::Type persona) noexcept {
  return persona == ::PhantomLedger::personas::Type::smallBusiness
             ? std::string_view{"business"}
             : std::string_view{"individual"};
}

std::string_view
maritalStatus(::PhantomLedger::entity::PersonId personId,
              ::PhantomLedger::personas::Type persona) noexcept {
  const auto seed = personSeed(personId);
  const auto x = stableU64({seed.view(), "marital"});
  const auto pct = marriedPctFor(persona);
  if ((x % 100U) < pct) {
    return "married";
  }
  return pick(kSingleStatuses, x >> 8U);
}

std::string_view
networthCode(::PhantomLedger::personas::Type persona) noexcept {
  using ::PhantomLedger::personas::Type;
  switch (persona) {
  case Type::student:
    return "A";
  case Type::retiree:
    return "C";
  case Type::salaried:
    return "B";
  case Type::freelancer:
    return "B";
  case Type::smallBusiness:
    return "C";
  case Type::highNetWorth:
    return "E";
  }
  return "B";
}

std::string_view incomeCode(::PhantomLedger::personas::Type persona) noexcept {
  using ::PhantomLedger::personas::Type;
  switch (persona) {
  case Type::student:
    return "1";
  case Type::retiree:
    return "2";
  case Type::salaried:
    return "3";
  case Type::freelancer:
    return "3";
  case Type::smallBusiness:
    return "4";
  case Type::highNetWorth:
    return "5";
  }
  return "3";
}

std::string_view occupation(::PhantomLedger::entity::PersonId personId,
                            ::PhantomLedger::personas::Type persona) noexcept {
  const auto seed = personSeed(personId);
  const auto x = stableU64({seed.view(), "occupation"});
  using ::PhantomLedger::personas::Type;
  switch (persona) {
  case Type::student:
    return pick(kOccStudent, x);
  case Type::retiree:
    return pick(kOccRetired, x);
  case Type::salaried:
    return pick(kOccSalaried, x);
  case Type::freelancer:
    return pick(kOccFreelancer, x);
  case Type::smallBusiness:
    return pick(kOccSmallBiz, x);
  case Type::highNetWorth:
    return pick(kOccHnw, x);
  }
  return pick(kOccSalaried, x);
}

std::string_view riskRating(bool isFraud, bool isMule, bool isVictim) noexcept {
  if (isFraud || isMule) {
    return "very_high";
  }
  if (isVictim) {
    return "high";
  }
  return "low";
}

::PhantomLedger::time::TimePoint
onboardingDate(::PhantomLedger::entity::PersonId personId,
               ::PhantomLedger::time::TimePoint simStart) {
  const auto seed = personSeed(personId);
  const auto x = stableU64({seed.view(), "onboarding"});
  const auto daysBefore = 90 + static_cast<int>(x % (365U * 5U));
  return simStart - ::PhantomLedger::time::Days{daysBefore};
}

// ────────────────────────────────────────────────────────────────────
// Name producers
//
// `nameForPerson` reads the indices already assigned to the person's
// `pii::Record` and views into the matching pool slots. No hashing,
// no allocation, no duplicate data.
// ────────────────────────────────────────────────────────────────────

NameRecord nameForPerson(::PhantomLedger::entity::PersonId personId,
                         const ::PhantomLedger::entity::pii::Roster &pii,
                         const pii_ns::PoolSet &pools) {
  NameRecord out;
  out.id = renderPersonId<24>(std::string_view{"NM_"}, personId);

  const auto &rec = pii.at(personId);
  const auto &pool = pools.forCountry(rec.country);

  if (rec.name.firstIdx < pool.firstNames.size()) {
    out.firstName = pool.firstNames[rec.name.firstIdx];
  }
  if (rec.name.middleIdx != ::PhantomLedger::entity::pii::kNoMiddleIdx &&
      rec.name.middleIdx < pool.middleNames.size()) {
    out.middleName = pool.middleNames[rec.name.middleIdx];
  }
  if (rec.name.lastIdx < pool.lastNames.size()) {
    out.lastName = pool.lastNames[rec.name.lastIdx];
  }
  return out;
}

NameRecord nameForCounterparty(std::string_view counterpartyId,
                               const pii_ns::LocalePool &usPool) {
  const auto x = stableU64({counterpartyId, "cp_name"});

  NameRecord out;
  out.id = renderRawId<24>(std::string_view{"NM_"}, counterpartyId);
  out.firstName = poolPick(usPool.businessNames, x);
  return out;
}

NameRecord nameForBank(std::string_view bankId) {
  const auto x = stableU64({bankId, "bank_name"});

  NameRecord out;
  out.id = renderRawId<24>(std::string_view{"NM_"}, bankId);
  out.firstName = ::PhantomLedger::locale::us::bankNameBySeed(x);
  return out;
}

RoutingNumber routingNumberForId(std::string_view bankId) noexcept {
  const auto x = stableU64({bankId, "routing"});
  // 9-digit ABA-style: in [100_000_000, 999_999_999].
  const auto routing = (x % 900'000'000ULL) + 100'000'000ULL;
  RoutingNumber out;
  const auto r = std::to_chars(out.bytes.data(),
                               out.bytes.data() + out.bytes.size(), routing);
  out.length = static_cast<std::uint8_t>(r.ptr - out.bytes.data());
  return out;
}

// ────────────────────────────────────────────────────────────────────
// Address producers
// ────────────────────────────────────────────────────────────────────

AddressRecord addressForPerson(::PhantomLedger::entity::PersonId personId,
                               const ::PhantomLedger::entity::pii::Roster &pii,
                               const pii_ns::PoolSet &pools) {
  AddressRecord out;
  out.id = renderPersonId<32>(std::string_view{"ADDR_"}, personId);

  const auto &rec = pii.at(personId);
  const auto &pool = pools.forCountry(rec.country);

  if (rec.address.streetIdx < pool.streets.size()) {
    out.streetLine1 = pool.streets[rec.address.streetIdx];
  }
  if (rec.address.zipTableIdx < pool.zipTable.size()) {
    const auto &zip = pool.zipTable[rec.address.zipTableIdx];
    out.city = zip.city;
    out.state = zip.adminCode;
    out.postalCode = zip.postalCode;
  }

  const auto seed = personSeed(personId);
  const auto hash = stableU64({seed.view(), "address_apt"});
  fillStreetLine2(out.streetLine2, hash);

  out.country = ::PhantomLedger::locale::code(rec.country);
  out.addressType = "residential";
  out.isHighRiskGeo = false;
  return out;
}

AddressRecord addressForCounterparty(std::string_view counterpartyId,
                                     const pii_ns::LocalePool &usPool) {
  return buildAddressFromHash(counterpartyId, "cp_address", "commercial",
                              usPool);
}

AddressRecord addressForBank(std::string_view bankId,
                             const pii_ns::LocalePool &usPool) {
  return buildAddressFromHash(bankId, "bank_address", "commercial", usPool);
}

StackString<24>
nameIdForPerson(::PhantomLedger::entity::PersonId personId) noexcept {
  return renderPersonId<24>(std::string_view{"NM_"}, personId);
}

StackString<32>
addressIdForPerson(::PhantomLedger::entity::PersonId personId) noexcept {
  return renderPersonId<32>(std::string_view{"ADDR_"}, personId);
}

StackString<24>
nameIdForCounterparty(std::string_view counterpartyId) noexcept {
  return renderRawId<24>(std::string_view{"NM_"}, counterpartyId);
}

StackString<32>
addressIdForCounterparty(std::string_view counterpartyId) noexcept {
  return renderRawId<32>(std::string_view{"ADDR_"}, counterpartyId);
}

StackString<24> nameIdForBank(std::string_view bankId) noexcept {
  return renderRawId<24>(std::string_view{"NM_"}, bankId);
}

StackString<32> addressIdForBank(std::string_view bankId) noexcept {
  return renderRawId<32>(std::string_view{"ADDR_"}, bankId);
}

} // namespace PhantomLedger::exporter::aml::identity
