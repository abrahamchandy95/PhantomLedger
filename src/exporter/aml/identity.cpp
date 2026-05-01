#include "phantomledger/exporter/aml/identity.hpp"

#include "phantomledger/exporter/aml/shared.hpp"

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace PhantomLedger::exporter::aml::identity {

namespace {

[[nodiscard]] std::string personIdKey(::PhantomLedger::entity::PersonId p) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "C%010u", static_cast<unsigned>(p));
  return std::string{buf};
}

// ────────── Pool helpers ──────────

template <std::size_t N>
[[nodiscard]] std::string_view
pick(const std::array<std::string_view, N> &items,
     std::uint64_t seed) noexcept {
  return items[seed % N];
}

template <std::size_t N>
[[nodiscard]] std::string_view pick(const char *const (&items)[N],
                                    std::uint64_t seed) noexcept {
  return std::string_view{items[seed % N]};
}

// ────────── Occupation pools ──────────

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

constexpr const char *kFirstNames[] = {
    "James",   "Mary",   "Robert",  "Patricia",  "John",    "Jennifer",
    "Michael", "Linda",  "David",   "Elizabeth", "William", "Barbara",
    "Richard", "Susan",  "Joseph",  "Jessica",   "Thomas",  "Sarah",
    "Charles", "Karen",  "Daniel",  "Lisa",      "Matthew", "Nancy",
    "Anthony", "Betty",  "Mark",    "Margaret",  "Donald",  "Sandra",
    "Steven",  "Ashley", "Paul",    "Kimberly",  "Andrew",  "Emily",
    "Joshua",  "Donna",  "Kenneth", "Michelle",  "Kevin",   "Carol",
    "Brian",   "Amanda", "George",  "Dorothy",   "Timothy", "Melissa",
    "Wei",     "Aisha",  "Liam",    "Sofia",     "Mateo",   "Olga",
};

constexpr const char *kLastNames[] = {
    "Smith",    "Johnson", "Williams",  "Brown",    "Jones",     "Garcia",
    "Miller",   "Davis",   "Rodriguez", "Martinez", "Hernandez", "Lopez",
    "Gonzalez", "Wilson",  "Anderson",  "Thomas",   "Taylor",    "Moore",
    "Jackson",  "Martin",  "Lee",       "Perez",    "Thompson",  "White",
    "Harris",   "Sanchez", "Clark",     "Ramirez",  "Lewis",     "Robinson",
    "Walker",   "Young",   "Allen",     "King",     "Wright",    "Scott",
    "Torres",   "Nguyen",  "Hill",      "Flores",   "Green",     "Adams",
    "Nelson",   "Baker",   "Hall",      "Rivera",   "Campbell",  "Chen",
    "Patel",    "Kim",     "Muller",    "Santos",   "Yamamoto",  "Khan",
    "Petrov",   "O'Brien", "Johansson", "Rossi",    "Dubois",
};

constexpr const char *kMiddleInitials[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "J",
    "K", "L", "M", "N", "P", "R", "S", "T", "W",
};

// ────────── Counterparty business-name pools ──────────

constexpr const char *kCpPrefixes[] = {
    "National", "Pacific", "Eagle",    "Summit",   "Atlas",    "Prime",
    "Metro",    "Global",  "Sterling", "Heritage", "Apex",     "Liberty",
    "Coastal",  "Pioneer", "Horizon",  "Diamond",  "Pinnacle", "Titan",
};

constexpr const char *kCpSuffixes[] = {
    "LLC", "Inc", "Corp", "Ltd", "Co", "Group", "Services",
};

// ────────── Bank name pool ──────────

constexpr const char *kBankNames[] = {
    "First National Bank", "Pacific Trust",   "Heritage Savings",
    "Metro Credit Union",  "Atlas Financial", "Liberty Bank",
    "Summit Bank",         "Coastal Federal", "Pioneer Trust",
    "Sterling Savings",
};

// ────────── Address pools ──────────

constexpr const char *kStreets[] = {
    "Main St",   "Oak Ave",         "Maple Dr", "Cedar Ln",  "Elm St",
    "Pine Rd",   "Washington Blvd", "Park Ave", "Lake Dr",   "River Rd",
    "Hill St",   "Sunset Blvd",     "Broadway", "Market St", "Highland Ave",
    "Valley Rd", "Forest Dr",
};

constexpr const char *kCities[] = {
    "New York",      "Los Angeles", "Chicago",   "Houston",   "Phoenix",
    "Philadelphia",  "San Antonio", "San Diego", "Dallas",    "Austin",
    "Jacksonville",  "San Jose",    "Columbus",  "Charlotte", "Indianapolis",
    "San Francisco", "Seattle",     "Denver",    "Nashville", "Portland",
    "Miami",         "Atlanta",     "Boston",    "Tampa",     "Minneapolis",
};

constexpr const char *kStates[] = {
    "NY", "CA", "IL", "TX", "AZ", "PA", "FL", "OH", "GA", "NC",
    "WA", "CO", "TN", "OR", "MN", "MA", "IN", "MO", "NV", "VA",
};

constexpr const char *kZips[] = {
    "10001", "90012", "60601", "77002", "85004", "19107", "33130",
    "43215", "30303", "28202", "98101", "80202", "37203", "97201",
    "55401", "02108", "46204", "63101", "89101", "22201",
};

constexpr std::size_t kCitiesCount = sizeof(kCities) / sizeof(kCities[0]);
constexpr std::size_t kStatesCount = sizeof(kStates) / sizeof(kStates[0]);
constexpr std::size_t kZipsCount = sizeof(kZips) / sizeof(kZips[0]);
constexpr std::size_t kStreetsCount = sizeof(kStreets) / sizeof(kStreets[0]);

// ────────── Address builder ──────────

[[nodiscard]] AddressRecord buildAddress(std::string_view seedKey,
                                         std::string_view nameSpace,
                                         std::string_view addressType) {
  const auto x = stableU64({seedKey, nameSpace});

  const unsigned streetNum = 100U + static_cast<unsigned>(x % 9900U);
  const auto street = pick(kStreets, x >> 8U);
  const auto cityIdx = static_cast<std::size_t>((x >> 16U) % kCitiesCount);
  const auto stateIdx = cityIdx % kStatesCount;
  const auto zipIdx = cityIdx % kZipsCount;

  const bool hasUnit = ((x >> 24U) % 5U) == 0U;
  std::string line2;
  if (hasUnit) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "Apt %u",
                  1U + static_cast<unsigned>((x >> 28U) % 500U));
    line2 = buf;
  }

  AddressRecord out;
  out.id.reserve(5 + seedKey.size());
  out.id.append("ADDR_");
  out.id.append(seedKey);

  out.streetLine1.reserve(8 + street.size());
  out.streetLine1.append(std::to_string(streetNum));
  out.streetLine1.push_back(' ');
  out.streetLine1.append(street);

  out.streetLine2 = std::move(line2);
  out.city = kCities[cityIdx];
  out.state = kStates[stateIdx];
  out.postalCode = kZips[zipIdx];
  out.country = "US";
  out.addressType = addressType;
  out.isHighRiskGeo = false;
  return out;
}

} // namespace

std::string customerType(::PhantomLedger::personas::Type persona) {
  return persona == ::PhantomLedger::personas::Type::smallBusiness
             ? "business"
             : "individual";
}

std::string maritalStatus(::PhantomLedger::entity::PersonId personId,
                          ::PhantomLedger::personas::Type persona) {
  const auto seedKey = personIdKey(personId);
  const auto x = stableU64({seedKey, "marital"});
  const auto pct = marriedPctFor(persona);
  if ((x % 100U) < pct) {
    return "married";
  }
  static constexpr std::array<std::string_view, 5> kSingleStatuses{
      "single", "single", "single", "divorced", "widowed",
  };
  return std::string{pick(kSingleStatuses, x >> 8U)};
}

std::string networthCode(::PhantomLedger::personas::Type persona) {
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

std::string incomeCode(::PhantomLedger::personas::Type persona) {
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

std::string occupation(::PhantomLedger::entity::PersonId personId,
                       ::PhantomLedger::personas::Type persona) {
  const auto seedKey = personIdKey(personId);
  const auto x = stableU64({seedKey, "occupation"});
  using ::PhantomLedger::personas::Type;
  switch (persona) {
  case Type::student:
    return std::string{pick(kOccStudent, x)};
  case Type::retiree:
    return std::string{pick(kOccRetired, x)};
  case Type::salaried:
    return std::string{pick(kOccSalaried, x)};
  case Type::freelancer:
    return std::string{pick(kOccFreelancer, x)};
  case Type::smallBusiness:
    return std::string{pick(kOccSmallBiz, x)};
  case Type::highNetWorth:
    return std::string{pick(kOccHnw, x)};
  }
  return std::string{pick(kOccSalaried, x)};
}

std::string riskRating(bool isFraud, bool isMule, bool isVictim) {
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
  const auto seedKey = personIdKey(personId);
  const auto x = stableU64({seedKey, "onboarding"});
  const auto daysBefore = 90 + static_cast<int>(x % (365U * 5U));
  return simStart - ::PhantomLedger::time::Days{daysBefore};
}

NameRecord nameForPerson(::PhantomLedger::entity::PersonId personId) {
  const auto seedKey = personIdKey(personId);
  const auto x = stableU64({seedKey, "name"});

  NameRecord out;
  out.id.reserve(3 + seedKey.size());
  out.id.append("NM_");
  out.id.append(seedKey);
  out.firstName = pick(kFirstNames, x);
  out.lastName = pick(kLastNames, x >> 8U);
  out.middleName = pick(kMiddleInitials, x >> 16U);
  return out;
}

NameRecord nameForCounterparty(std::string_view counterpartyId) {
  const auto x = stableU64({counterpartyId, "cp_name"});

  NameRecord out;
  out.id.reserve(3 + counterpartyId.size());
  out.id.append("NM_");
  out.id.append(counterpartyId);

  std::string full;
  const auto prefix = pick(kCpPrefixes, x);
  const auto suffix = pick(kCpSuffixes, x >> 8U);
  full.reserve(prefix.size() + 1 + suffix.size());
  full.append(prefix);
  full.push_back(' ');
  full.append(suffix);

  out.firstName = std::move(full);
  return out;
}

NameRecord nameForBank(std::string_view bankId) {
  const auto x = stableU64({bankId, "bank_name"});

  NameRecord out;
  out.id.reserve(3 + bankId.size());
  out.id.append("NM_");
  out.id.append(bankId);
  out.firstName = pick(kBankNames, x);
  return out;
}

std::string routingNumberForId(std::string_view bankId) {
  const auto x = stableU64({bankId, "routing"});
  // 9-digit ABA-style: in [100_000_000, 999_999_999].
  const auto routing = (x % 900'000'000ULL) + 100'000'000ULL;
  return std::to_string(routing);
}

AddressRecord addressForPerson(::PhantomLedger::entity::PersonId personId) {
  return buildAddress(personIdKey(personId), "address", "residential");
}

AddressRecord addressForCounterparty(std::string_view counterpartyId) {
  return buildAddress(counterpartyId, "cp_address", "commercial");
}

AddressRecord addressForBank(std::string_view bankId) {
  return buildAddress(bankId, "bank_address", "commercial");
}

} // namespace PhantomLedger::exporter::aml::identity
