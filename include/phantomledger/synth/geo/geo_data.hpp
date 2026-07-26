#pragma once
//
// phantomledger/synth/geo/geo_data.hpp
//
// THE PINNED GEOGRAPHIC CATALOGUE (geo-causal-v1), EMBEDDED as a
// constexpr table per the owner's minimize-repo-data-files directive
// (2026-07-24) — this header replaced data/geo/us_cities.csv (and its
// README) and IS the pinned artifact a geography-data round rewrites.
// Values are transcribed VERBATIM from the retired CSV: the embed is
// byte-neutral by construction (identical rows ⇒ identical GeoCatalog
// ⇒ identical corpus; zero golden movement is the acceptance gate).
//
// SEMANTICS: real US places (homes + domestic commerce, population-
// weighted) plus major international cities (travel / cross-border
// destinations, and homes for the small non-US locale share). Homes,
// merchant outlets, and every exporter read the SAME rows, so a
// person's home, the merchant they visit, and the geography reported
// downstream are one coherent (city, state, postal, coordinates)
// tuple. ROW ORDER DEFINES THE 1-BASED GeoAreaId — never reorder
// rows outside a named corpus-moving round; append only.
//
// FIELDS: country = locale::Country; postal code is a representative
// postal-area string (NOT a USPS route; leading zeros preserved);
// coordinates are integer microdegrees (deg x 1e6; US longitudes are
// negative); population is approximate municipal population [Likely —
// order-of-magnitude correct for population-weighted home placement].
// Land area is NOT carried here — the loader never populated
// GeoArea.landAreaKm2 (a recorded code/data gap), and preserving that
// 0 is part of the byte-neutral contract.
//
// STATUS: this 86-row set (71 US city cores covering all 50 states +
// DC, 15 international) is the RUNNABLE PLACEHOLDER recorded in the
// active arc — NOT a Census-complete place/ZCTA catalogue. The target
// shape (pinned Census Gazetteer + ACS with stable source keys) and
// its storage format are decided in the merchant-data round (owner
// sign-off under the minimize-data-files law). Extending the set =
// append rows here in a named model-moving round (home placement is
// population-weighted ⇒ new rows move the corpus).
//

#include "phantomledger/taxonomies/locale/types.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::synth::geo::data {

struct GeoAreaRow {
  locale::Country country;
  std::string_view postalAreaCode;
  std::string_view city;
  std::string_view stateCode;
  std::string_view stateName;
  std::int32_t latitudeE6;
  std::int32_t longitudeE6;
  std::uint32_t population;
};

using locale::Country;

inline constexpr std::array<GeoAreaRow, 86> kAreas{{
    {Country::us, "10001", "New York", "NY", "New York", 40712800, -74006000, 8478000},
    {Country::us, "90012", "Los Angeles", "CA", "California", 34052200, -118243700, 3820000},
    {Country::us, "60602", "Chicago", "IL", "Illinois", 41878100, -87629800, 2665000},
    {Country::us, "77002", "Houston", "TX", "Texas", 29760400, -95369800, 2314000},
    {Country::us, "85004", "Phoenix", "AZ", "Arizona", 33448400, -112074000, 1650000},
    {Country::us, "19107", "Philadelphia", "PA", "Pennsylvania", 39952600, -75165200, 1550000},
    {Country::us, "78205", "San Antonio", "TX", "Texas", 29424100, -98493600, 1495000},
    {Country::us, "92101", "San Diego", "CA", "California", 32715700, -117161100, 1388000},
    {Country::us, "75201", "Dallas", "TX", "Texas", 32776700, -96797000, 1300000},
    {Country::us, "78701", "Austin", "TX", "Texas", 30267200, -97743100, 979000},
    {Country::us, "76102", "Fort Worth", "TX", "Texas", 32755500, -97330800, 978000},
    {Country::us, "32202", "Jacksonville", "FL", "Florida", 30332200, -81655700, 971000},
    {Country::us, "95113", "San Jose", "CA", "California", 37338200, -121886300, 969000},
    {Country::us, "43215", "Columbus", "OH", "Ohio", 39961200, -82998800, 907000},
    {Country::us, "28202", "Charlotte", "NC", "North Carolina", 35227100, -80843100, 897000},
    {Country::us, "46204", "Indianapolis", "IN", "Indiana", 39768400, -86158100, 887000},
    {Country::us, "94103", "San Francisco", "CA", "California", 37774900, -122419400, 808000},
    {Country::us, "98104", "Seattle", "WA", "Washington", 47606200, -122332100, 755000},
    {Country::us, "80202", "Denver", "CO", "Colorado", 39739200, -104990300, 716000},
    {Country::us, "37203", "Nashville", "TN", "Tennessee", 36162700, -86781600, 687000},
    {Country::us, "73102", "Oklahoma City", "OK", "Oklahoma", 35467600, -97516400, 694000},
    {Country::us, "20001", "Washington", "DC", "District of Columbia", 38907200, -77036900, 671000},
    {Country::us, "89101", "Las Vegas", "NV", "Nevada", 36169900, -115139800, 656000},
    {Country::us, "02108", "Boston", "MA", "Massachusetts", 42360100, -71058900, 654000},
    {Country::us, "97204", "Portland", "OR", "Oregon", 45515200, -122678400, 635000},
    {Country::us, "48226", "Detroit", "MI", "Michigan", 42331400, -83045800, 633000},
    {Country::us, "40202", "Louisville", "KY", "Kentucky", 38252700, -85758500, 624000},
    {Country::us, "38103", "Memphis", "TN", "Tennessee", 35149500, -90049000, 618000},
    {Country::us, "21202", "Baltimore", "MD", "Maryland", 39290400, -76612200, 569000},
    {Country::us, "53202", "Milwaukee", "WI", "Wisconsin", 43038900, -87906500, 561000},
    {Country::us, "87102", "Albuquerque", "NM", "New Mexico", 35084400, -106650400, 562000},
    {Country::us, "85701", "Tucson", "AZ", "Arizona", 32222600, -110974700, 543000},
    {Country::us, "93721", "Fresno", "CA", "California", 36737800, -119787100, 545000},
    {Country::us, "95814", "Sacramento", "CA", "California", 38581600, -121494400, 526000},
    {Country::us, "64106", "Kansas City", "MO", "Missouri", 39099700, -94578600, 510000},
    {Country::us, "30303", "Atlanta", "GA", "Georgia", 33749000, -84388000, 499000},
    {Country::us, "68102", "Omaha", "NE", "Nebraska", 41256500, -95934500, 487000},
    {Country::us, "80903", "Colorado Springs", "CO", "Colorado", 38833900, -104821400, 483000},
    {Country::us, "27601", "Raleigh", "NC", "North Carolina", 35779600, -78638200, 469000},
    {Country::us, "23451", "Virginia Beach", "VA", "Virginia", 36852900, -75978000, 455000},
    {Country::us, "33130", "Miami", "FL", "Florida", 25761700, -80191800, 442000},
    {Country::us, "94607", "Oakland", "CA", "California", 37804400, -122271200, 440000},
    {Country::us, "55401", "Minneapolis", "MN", "Minnesota", 44977800, -93265000, 429000},
    {Country::us, "74103", "Tulsa", "OK", "Oklahoma", 36154000, -95992800, 411000},
    {Country::us, "33602", "Tampa", "FL", "Florida", 27950600, -82457200, 398000},
    {Country::us, "67202", "Wichita", "KS", "Kansas", 37687200, -97330100, 397000},
    {Country::us, "70112", "New Orleans", "LA", "Louisiana", 29951100, -90071500, 383000},
    {Country::us, "44113", "Cleveland", "OH", "Ohio", 41499300, -81694400, 372000},
    {Country::us, "96813", "Honolulu", "HI", "Hawaii", 21306900, -157858300, 350000},
    {Country::us, "07102", "Newark", "NJ", "New Jersey", 40735700, -74172400, 305000},
    {Country::us, "99501", "Anchorage", "AK", "Alaska", 61218100, -149900300, 291000},
    {Country::us, "14202", "Buffalo", "NY", "New York", 42886400, -78878400, 278000},
    {Country::us, "83702", "Boise", "ID", "Idaho", 43615000, -116202300, 237000},
    {Country::us, "23219", "Richmond", "VA", "Virginia", 37540700, -77436000, 226000},
    {Country::us, "50309", "Des Moines", "IA", "Iowa", 41586800, -93625000, 214000},
    {Country::us, "72201", "Little Rock", "AR", "Arkansas", 34746500, -92289600, 202000},
    {Country::us, "84101", "Salt Lake City", "UT", "Utah", 40760800, -111891000, 200000},
    {Country::us, "35203", "Birmingham", "AL", "Alabama", 33518600, -86810400, 197000},
    {Country::us, "57104", "Sioux Falls", "SD", "South Dakota", 43546000, -96731300, 197000},
    {Country::us, "02903", "Providence", "RI", "Rhode Island", 41824000, -71412800, 190000},
    {Country::us, "39201", "Jackson", "MS", "Mississippi", 32298800, -90184800, 149000},
    {Country::us, "06604", "Bridgeport", "CT", "Connecticut", 41179200, -73189400, 148000},
    {Country::us, "29201", "Columbia", "SC", "South Carolina", 34000700, -81034800, 137000},
    {Country::us, "58102", "Fargo", "ND", "North Dakota", 46877200, -96789800, 126000},
    {Country::us, "59101", "Billings", "MT", "Montana", 45783300, -108500700, 118000},
    {Country::us, "03101", "Manchester", "NH", "New Hampshire", 42995600, -71454800, 115000},
    {Country::us, "19801", "Wilmington", "DE", "Delaware", 39739100, -75539800, 71000},
    {Country::us, "04101", "Portland", "ME", "Maine", 43659100, -70256800, 68000},
    {Country::us, "82001", "Cheyenne", "WY", "Wyoming", 41140000, -104820200, 65000},
    {Country::us, "25301", "Charleston", "WV", "West Virginia", 38349800, -81632600, 46000},
    {Country::us, "05401", "Burlington", "VT", "Vermont", 44475900, -73212100, 44000},
    {Country::gb, "EC1A", "London", "LND", "Greater London", 51507400, -127800, 8982000},
    {Country::ca, "M5H", "Toronto", "ON", "Ontario", 43653200, -79383200, 2794000},
    {Country::mx, "06000", "Mexico City", "CMX", "Mexico City", 19432600, -99133200, 9209000},
    {Country::in, "400001", "Mumbai", "MH", "Maharashtra", 19076000, 72877700, 12442000},
    {Country::cn, "200000", "Shanghai", "SH", "Shanghai", 31230400, 121473700, 24870000},
    {Country::kr, "04524", "Seoul", "SEO", "Seoul", 37566500, 126978000, 9776000},
    {Country::br, "01000", "Sao Paulo", "SP", "Sao Paulo", -23550500, -46633300, 12330000},
    {Country::de, "10115", "Berlin", "BE", "Berlin", 52520000, 13405000, 3645000},
    {Country::jp, "100-0001", "Tokyo", "TK", "Tokyo", 35676200, 139650300, 13960000},
    {Country::fr, "75001", "Paris", "IDF", "Ile-de-France", 48856600, 2352200, 2161000},
    {Country::es, "28001", "Madrid", "MD", "Madrid", 40416800, -3703800, 3223000},
    {Country::it, "00118", "Rome", "RM", "Lazio", 41902800, 12496400, 2761000},
    {Country::nl, "1011", "Amsterdam", "NH", "North Holland", 52367600, 4904100, 872000},
    {Country::au, "2000", "Sydney", "NSW", "New South Wales", -33868800, 151209300, 5312000},
    {Country::ru, "101000", "Moscow", "MSK", "Moscow", 55755800, 37617300, 12615000},
}};

} // namespace PhantomLedger::synth::geo::data
