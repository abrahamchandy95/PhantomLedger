#pragma once

#include "us_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace PhantomLedger::locale::us {

struct CityRange {
  std::uint16_t offset;
  std::uint16_t count;
};

namespace detail {

inline constexpr auto kAllUsCities = std::to_array<std::string_view>({
    // AL  [0, 10)
    "Birmingham",
    "Montgomery",
    "Huntsville",
    "Mobile",
    "Tuscaloosa",
    "Hoover",
    "Dothan",
    "Decatur",
    "Auburn",
    "Madison",
    // AK  [10, 18)
    "Anchorage",
    "Fairbanks",
    "Juneau",
    "Wasilla",
    "Sitka",
    "Ketchikan",
    "Kenai",
    "Kodiak",
    // AZ  [18, 28)
    "Phoenix",
    "Tucson",
    "Mesa",
    "Chandler",
    "Scottsdale",
    "Glendale",
    "Gilbert",
    "Tempe",
    "Peoria",
    "Surprise",
    // AR  [28, 38)
    "Little Rock",
    "Fort Smith",
    "Fayetteville",
    "Springdale",
    "Jonesboro",
    "Rogers",
    "Conway",
    "North Little Rock",
    "Bentonville",
    "Pine Bluff",
    // CA  [38, 48)
    "Los Angeles",
    "San Diego",
    "San Jose",
    "San Francisco",
    "Fresno",
    "Sacramento",
    "Long Beach",
    "Oakland",
    "Bakersfield",
    "Anaheim",
    // CO  [48, 58)
    "Denver",
    "Colorado Springs",
    "Aurora",
    "Fort Collins",
    "Lakewood",
    "Thornton",
    "Arvada",
    "Westminster",
    "Pueblo",
    "Boulder",
    // CT  [58, 66)
    "Bridgeport",
    "New Haven",
    "Stamford",
    "Hartford",
    "Waterbury",
    "Norwalk",
    "Danbury",
    "New Britain",
    // DE  [66, 74)
    "Wilmington",
    "Dover",
    "Newark",
    "Middletown",
    "Smyrna",
    "Milford",
    "Seaford",
    "Georgetown",
    // FL  [74, 84)
    "Jacksonville",
    "Miami",
    "Tampa",
    "Orlando",
    "St. Petersburg",
    "Hialeah",
    "Tallahassee",
    "Fort Lauderdale",
    "Port St. Lucie",
    "Cape Coral",
    // GA  [84, 94)
    "Atlanta",
    "Augusta",
    "Columbus",
    "Macon",
    "Savannah",
    "Athens",
    "Sandy Springs",
    "Roswell",
    "Albany",
    "Marietta",
    // HI  [94, 102)
    "Honolulu",
    "Hilo",
    "Kailua",
    "Kaneohe",
    "Waipahu",
    "Pearl City",
    "Mililani",
    "Kahului",
    // ID  [102, 110)
    "Boise",
    "Meridian",
    "Nampa",
    "Idaho Falls",
    "Pocatello",
    "Caldwell",
    "Coeur d'Alene",
    "Twin Falls",
    // IL  [110, 120)
    "Chicago",
    "Aurora",
    "Naperville",
    "Joliet",
    "Rockford",
    "Springfield",
    "Elgin",
    "Peoria",
    "Champaign",
    "Waukegan",
    // IN  [120, 130)
    "Indianapolis",
    "Fort Wayne",
    "Evansville",
    "South Bend",
    "Carmel",
    "Fishers",
    "Bloomington",
    "Hammond",
    "Gary",
    "Lafayette",
    // IA  [130, 140)
    "Des Moines",
    "Cedar Rapids",
    "Davenport",
    "Sioux City",
    "Iowa City",
    "Waterloo",
    "Council Bluffs",
    "Ames",
    "West Des Moines",
    "Dubuque",
    // KS  [140, 150)
    "Wichita",
    "Overland Park",
    "Kansas City",
    "Olathe",
    "Topeka",
    "Lawrence",
    "Shawnee",
    "Manhattan",
    "Lenexa",
    "Salina",
    // KY  [150, 160)
    "Louisville",
    "Lexington",
    "Bowling Green",
    "Owensboro",
    "Covington",
    "Hopkinsville",
    "Richmond",
    "Florence",
    "Georgetown",
    "Henderson",
    // LA  [160, 170)
    "New Orleans",
    "Baton Rouge",
    "Shreveport",
    "Lafayette",
    "Lake Charles",
    "Kenner",
    "Bossier City",
    "Monroe",
    "Alexandria",
    "Houma",
    // ME  [170, 180)
    "Portland",
    "Lewiston",
    "Bangor",
    "South Portland",
    "Auburn",
    "Biddeford",
    "Sanford",
    "Saco",
    "Augusta",
    "Westbrook",
    // MD  [180, 190)
    "Baltimore",
    "Columbia",
    "Germantown",
    "Silver Spring",
    "Waldorf",
    "Glen Burnie",
    "Frederick",
    "Rockville",
    "Gaithersburg",
    "Annapolis",
    // MA  [190, 200)
    "Boston",
    "Worcester",
    "Springfield",
    "Cambridge",
    "Lowell",
    "Brockton",
    "New Bedford",
    "Quincy",
    "Lynn",
    "Newton",
    // MI  [200, 210)
    "Detroit",
    "Grand Rapids",
    "Warren",
    "Sterling Heights",
    "Ann Arbor",
    "Lansing",
    "Flint",
    "Dearborn",
    "Livonia",
    "Westland",
    // MN  [210, 220)
    "Minneapolis",
    "Saint Paul",
    "Rochester",
    "Duluth",
    "Bloomington",
    "Brooklyn Park",
    "Plymouth",
    "Maple Grove",
    "Woodbury",
    "St. Cloud",
    // MS  [220, 230)
    "Jackson",
    "Gulfport",
    "Southaven",
    "Hattiesburg",
    "Biloxi",
    "Meridian",
    "Tupelo",
    "Olive Branch",
    "Greenville",
    "Horn Lake",
    // MO  [230, 240)
    "Kansas City",
    "St. Louis",
    "Springfield",
    "Independence",
    "Columbia",
    "Lee's Summit",
    "O'Fallon",
    "St. Joseph",
    "St. Charles",
    "St. Peters",
    // MT  [240, 248)
    "Billings",
    "Missoula",
    "Great Falls",
    "Bozeman",
    "Butte",
    "Helena",
    "Kalispell",
    "Havre",
    // NE  [248, 256)
    "Omaha",
    "Lincoln",
    "Bellevue",
    "Grand Island",
    "Kearney",
    "Fremont",
    "Hastings",
    "Norfolk",
    // NV  [256, 264)
    "Las Vegas",
    "Henderson",
    "Reno",
    "North Las Vegas",
    "Sparks",
    "Carson City",
    "Fernley",
    "Elko",
    // NH  [264, 272)
    "Manchester",
    "Nashua",
    "Concord",
    "Derry",
    "Dover",
    "Rochester",
    "Salem",
    "Merrimack",
    // NJ  [272, 282)
    "Newark",
    "Jersey City",
    "Paterson",
    "Elizabeth",
    "Lakewood",
    "Edison",
    "Woodbridge",
    "Toms River",
    "Hamilton",
    "Trenton",
    // NM  [282, 290)
    "Albuquerque",
    "Las Cruces",
    "Rio Rancho",
    "Santa Fe",
    "Roswell",
    "Farmington",
    "Clovis",
    "Hobbs",
    // NY  [290, 300)
    "New York",
    "Buffalo",
    "Yonkers",
    "Rochester",
    "Syracuse",
    "Albany",
    "New Rochelle",
    "Mount Vernon",
    "Schenectady",
    "Utica",
    // NC  [300, 310)
    "Charlotte",
    "Raleigh",
    "Greensboro",
    "Durham",
    "Winston-Salem",
    "Fayetteville",
    "Cary",
    "Wilmington",
    "High Point",
    "Greenville",
    // ND  [310, 318)
    "Fargo",
    "Bismarck",
    "Grand Forks",
    "Minot",
    "West Fargo",
    "Mandan",
    "Dickinson",
    "Jamestown",
    // OH  [318, 328)
    "Columbus",
    "Cleveland",
    "Cincinnati",
    "Toledo",
    "Akron",
    "Dayton",
    "Parma",
    "Canton",
    "Youngstown",
    "Lorain",
    // OK  [328, 338)
    "Oklahoma City",
    "Tulsa",
    "Norman",
    "Broken Arrow",
    "Edmond",
    "Lawton",
    "Moore",
    "Midwest City",
    "Stillwater",
    "Enid",
    // OR  [338, 348)
    "Portland",
    "Salem",
    "Eugene",
    "Gresham",
    "Hillsboro",
    "Beaverton",
    "Bend",
    "Medford",
    "Springfield",
    "Corvallis",
    // PA  [348, 358)
    "Philadelphia",
    "Pittsburgh",
    "Allentown",
    "Erie",
    "Reading",
    "Scranton",
    "Bethlehem",
    "Lancaster",
    "Harrisburg",
    "York",
    // RI  [358, 366)
    "Providence",
    "Warwick",
    "Cranston",
    "Pawtucket",
    "East Providence",
    "Woonsocket",
    "Coventry",
    "North Providence",
    // SC  [366, 374)
    "Charleston",
    "Columbia",
    "North Charleston",
    "Mount Pleasant",
    "Rock Hill",
    "Greenville",
    "Summerville",
    "Sumter",
    // SD  [374, 382)
    "Sioux Falls",
    "Rapid City",
    "Aberdeen",
    "Brookings",
    "Watertown",
    "Mitchell",
    "Yankton",
    "Pierre",
    // TN  [382, 392)
    "Nashville",
    "Memphis",
    "Knoxville",
    "Chattanooga",
    "Clarksville",
    "Murfreesboro",
    "Franklin",
    "Jackson",
    "Johnson City",
    "Hendersonville",
    // TX  [392, 402)
    "Houston",
    "San Antonio",
    "Dallas",
    "Austin",
    "Fort Worth",
    "El Paso",
    "Arlington",
    "Corpus Christi",
    "Plano",
    "Lubbock",
    // UT  [402, 410)
    "Salt Lake City",
    "West Valley City",
    "West Jordan",
    "Provo",
    "Orem",
    "Sandy",
    "Ogden",
    "St. George",
    // VT  [410, 418)
    "Burlington",
    "South Burlington",
    "Rutland",
    "Essex Junction",
    "Colchester",
    "Bennington",
    "Brattleboro",
    "Hartford",
    // VA  [418, 428)
    "Virginia Beach",
    "Norfolk",
    "Chesapeake",
    "Richmond",
    "Newport News",
    "Alexandria",
    "Hampton",
    "Roanoke",
    "Portsmouth",
    "Suffolk",
    // WA  [428, 438)
    "Seattle",
    "Spokane",
    "Tacoma",
    "Vancouver",
    "Bellevue",
    "Kent",
    "Everett",
    "Renton",
    "Spokane Valley",
    "Federal Way",
    // WV  [438, 446)
    "Charleston",
    "Huntington",
    "Morgantown",
    "Parkersburg",
    "Wheeling",
    "Weirton",
    "Fairmont",
    "Beckley",
    // WI  [446, 456)
    "Milwaukee",
    "Madison",
    "Green Bay",
    "Kenosha",
    "Racine",
    "Appleton",
    "Waukesha",
    "Eau Claire",
    "Oshkosh",
    "Janesville",
    // WY  [456, 464)
    "Cheyenne",
    "Casper",
    "Laramie",
    "Gillette",
    "Rock Springs",
    "Sheridan",
    "Green River",
    "Evanston",
    // DC  [464, 470) — single city; we pad with neighborhood-style
    // place names that locals recognize as part of the District,
    // since DC has no second-largest city to fall back on.
    "Washington",
    "Georgetown",
    "Capitol Hill",
    "Anacostia",
    "Foggy Bottom",
    "Adams Morgan",
});

/// Per-state slice into `kAllUsCities`, indexed by `slot(state)`.
inline constexpr std::array<CityRange, kStateCount> kStateCityRanges{{
    {0, 10},   // AL
    {10, 8},   // AK
    {18, 10},  // AZ
    {28, 10},  // AR
    {38, 10},  // CA
    {48, 10},  // CO
    {58, 8},   // CT
    {66, 8},   // DE
    {74, 10},  // FL
    {84, 10},  // GA
    {94, 8},   // HI
    {102, 8},  // ID
    {110, 10}, // IL
    {120, 10}, // IN
    {130, 10}, // IA
    {140, 10}, // KS
    {150, 10}, // KY
    {160, 10}, // LA
    {170, 10}, // ME
    {180, 10}, // MD
    {190, 10}, // MA
    {200, 10}, // MI
    {210, 10}, // MN
    {220, 10}, // MS
    {230, 10}, // MO
    {240, 8},  // MT
    {248, 8},  // NE
    {256, 8},  // NV
    {264, 8},  // NH
    {272, 10}, // NJ
    {282, 8},  // NM
    {290, 10}, // NY
    {300, 10}, // NC
    {310, 8},  // ND
    {318, 10}, // OH
    {328, 10}, // OK
    {338, 10}, // OR
    {348, 10}, // PA
    {358, 8},  // RI
    {366, 8},  // SC
    {374, 8},  // SD
    {382, 10}, // TN
    {392, 10}, // TX
    {402, 8},  // UT
    {410, 8},  // VT
    {418, 10}, // VA
    {428, 10}, // WA
    {438, 8},  // WV
    {446, 10}, // WI
    {456, 8},  // WY
    {464, 6},  // DC
}};
static_assert(kStateCityRanges.size() == kStateCount);

consteval bool cityRangesAreValid() {
  std::size_t offset = 0;

  for (const auto range : kStateCityRanges) {
    if (range.offset != offset) {
      return false;
    }

    offset += range.count;

    if (offset > kAllUsCities.size()) {
      return false;
    }
  }

  return offset == kAllUsCities.size();
}

static_assert(cityRangesAreValid());
} // namespace detail

[[nodiscard]] constexpr std::span<const std::string_view>
citiesFor(State state) noexcept {
  const auto range = detail::kStateCityRanges[toIndex(state)];
  return std::span<const std::string_view>{
      detail::kAllUsCities.data() + range.offset, range.count};
}

} // namespace PhantomLedger::locale::us
