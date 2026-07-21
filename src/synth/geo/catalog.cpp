#include "phantomledger/synth/geo/catalog.hpp"

#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/taxonomies/locale/us_state.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PhantomLedger::synth::geo {

namespace {

using entity::geography::GeoArea;
using entity::geography::GeoAreaId;
using entity::geography::GeoCatalog;
namespace usloc = ::PhantomLedger::locale::us;

// Coordinates are integer MICRODEGREES (deg x 1e6); US longitudes are
// negative. Populations are approximate municipal figures [Likely —
// owner verifies at citation time]; they only need to be
// order-of-magnitude correct for population-weighted home placement.
// One representative real postal code per city. Coverage: every US
// state + DC (largest metros carry extra rows so population weight
// concentrates realistically).
struct UsPlace {
  std::string_view city;
  usloc::State state;
  std::string_view zip;
  std::int32_t latE6;
  std::int32_t lonE6;
  std::uint32_t population;
};

inline constexpr auto kUsPlaces = std::to_array<UsPlace>({
    {"New York", usloc::State::ny, "10001", 40712800, -74006000, 8478000},
    {"Los Angeles", usloc::State::ca, "90012", 34052200, -118243700, 3820000},
    {"Chicago", usloc::State::il, "60602", 41878100, -87629800, 2665000},
    {"Houston", usloc::State::tx, "77002", 29760400, -95369800, 2314000},
    {"Phoenix", usloc::State::az, "85004", 33448400, -112074000, 1650000},
    {"Philadelphia", usloc::State::pa, "19107", 39952600, -75165200, 1550000},
    {"San Antonio", usloc::State::tx, "78205", 29424100, -98493600, 1495000},
    {"San Diego", usloc::State::ca, "92101", 32715700, -117161100, 1388000},
    {"Dallas", usloc::State::tx, "75201", 32776700, -96797000, 1300000},
    {"Austin", usloc::State::tx, "78701", 30267200, -97743100, 979000},
    {"Fort Worth", usloc::State::tx, "76102", 32755500, -97330800, 978000},
    {"Jacksonville", usloc::State::fl, "32202", 30332200, -81655700, 971000},
    {"San Jose", usloc::State::ca, "95113", 37338200, -121886300, 969000},
    {"Columbus", usloc::State::oh, "43215", 39961200, -82998800, 907000},
    {"Charlotte", usloc::State::nc, "28202", 35227100, -80843100, 897000},
    {"Indianapolis", usloc::State::in, "46204", 39768400, -86158100, 887000},
    {"San Francisco", usloc::State::ca, "94103", 37774900, -122419400, 808000},
    {"Seattle", usloc::State::wa, "98104", 47606200, -122332100, 755000},
    {"Denver", usloc::State::co, "80202", 39739200, -104990300, 716000},
    {"Nashville", usloc::State::tn, "37203", 36162700, -86781600, 687000},
    {"Oklahoma City", usloc::State::ok, "73102", 35467600, -97516400, 694000},
    {"Washington", usloc::State::dc, "20001", 38907200, -77036900, 671000},
    {"Las Vegas", usloc::State::nv, "89101", 36169900, -115139800, 656000},
    {"Boston", usloc::State::ma, "02108", 42360100, -71058900, 654000},
    {"Portland", usloc::State::or_, "97204", 45515200, -122678400, 635000},
    {"Detroit", usloc::State::mi, "48226", 42331400, -83045800, 633000},
    {"Louisville", usloc::State::ky, "40202", 38252700, -85758500, 624000},
    {"Memphis", usloc::State::tn, "38103", 35149500, -90049000, 618000},
    {"Baltimore", usloc::State::md, "21202", 39290400, -76612200, 569000},
    {"Milwaukee", usloc::State::wi, "53202", 43038900, -87906500, 561000},
    {"Albuquerque", usloc::State::nm, "87102", 35084400, -106650400, 562000},
    {"Tucson", usloc::State::az, "85701", 32222600, -110974700, 543000},
    {"Fresno", usloc::State::ca, "93721", 36737800, -119787100, 545000},
    {"Sacramento", usloc::State::ca, "95814", 38581600, -121494400, 526000},
    {"Kansas City", usloc::State::mo, "64106", 39099700, -94578600, 510000},
    {"Atlanta", usloc::State::ga, "30303", 33749000, -84388000, 499000},
    {"Omaha", usloc::State::ne, "68102", 41256500, -95934500, 487000},
    {"Colorado Springs", usloc::State::co, "80903", 38833900, -104821400, 483000},
    {"Raleigh", usloc::State::nc, "27601", 35779600, -78638200, 469000},
    {"Virginia Beach", usloc::State::va, "23451", 36852900, -75978000, 455000},
    {"Miami", usloc::State::fl, "33130", 25761700, -80191800, 442000},
    {"Oakland", usloc::State::ca, "94607", 37804400, -122271200, 440000},
    {"Minneapolis", usloc::State::mn, "55401", 44977800, -93265000, 429000},
    {"Tulsa", usloc::State::ok, "74103", 36154000, -95992800, 411000},
    {"Tampa", usloc::State::fl, "33602", 27950600, -82457200, 398000},
    {"Wichita", usloc::State::ks, "67202", 37687200, -97330100, 397000},
    {"New Orleans", usloc::State::la, "70112", 29951100, -90071500, 383000},
    {"Cleveland", usloc::State::oh, "44113", 41499300, -81694400, 372000},
    {"Honolulu", usloc::State::hi, "96813", 21306900, -157858300, 350000},
    {"Newark", usloc::State::nj, "07102", 40735700, -74172400, 305000},
    {"Anchorage", usloc::State::ak, "99501", 61218100, -149900300, 291000},
    {"Buffalo", usloc::State::ny, "14202", 42886400, -78878400, 278000},
    {"Boise", usloc::State::id, "83702", 43615000, -116202300, 237000},
    {"Richmond", usloc::State::va, "23219", 37540700, -77436000, 226000},
    {"Des Moines", usloc::State::ia, "50309", 41586800, -93625000, 214000},
    {"Little Rock", usloc::State::ar, "72201", 34746500, -92289600, 202000},
    {"Salt Lake City", usloc::State::ut, "84101", 40760800, -111891000, 200000},
    {"Birmingham", usloc::State::al, "35203", 33518600, -86810400, 197000},
    {"Sioux Falls", usloc::State::sd, "57104", 43546000, -96731300, 197000},
    {"Providence", usloc::State::ri, "02903", 41824000, -71412800, 190000},
    {"Jackson", usloc::State::ms, "39201", 32298800, -90184800, 149000},
    {"Bridgeport", usloc::State::ct, "06604", 41179200, -73189400, 148000},
    {"Columbia", usloc::State::sc, "29201", 34000700, -81034800, 137000},
    {"Fargo", usloc::State::nd, "58102", 46877200, -96789800, 126000},
    {"Billings", usloc::State::mt, "59101", 45783300, -108500700, 118000},
    {"Manchester", usloc::State::nh, "03101", 42995600, -71454800, 115000},
    {"Wilmington", usloc::State::de, "19801", 39739100, -75539800, 71000},
    {"Portland", usloc::State::me, "04101", 43659100, -70256800, 68000},
    {"Cheyenne", usloc::State::wy, "82001", 41140000, -104820200, 65000},
    {"Charleston", usloc::State::wv, "25301", 38349800, -81632600, 46000},
    {"Burlington", usloc::State::vt, "05401", 44475900, -73212100, 44000},
});

// Major international cities: the destinations of realistic cross-
// border events — customer travel and card-not-present / stolen-card
// fraud abroad (the transaction model reaches these in G2). Also the
// home areas for the small non-US share of the locale mix, so no
// person is left without a residence. One per non-US locale-mix
// country. stateCode/stateName are the country's first-level region
// [Likely — owner verifies].
struct IntlPlace {
  std::string_view city;
  locale::Country country;
  std::string_view stateCode;
  std::string_view stateName;
  std::string_view postal;
  std::int32_t latE6;
  std::int32_t lonE6;
  std::uint32_t population;
};

inline constexpr auto kIntlPlaces = std::to_array<IntlPlace>({
    {"London", locale::Country::gb, "LND", "Greater London", "EC1A", 51507400,
     -127800, 8982000},
    {"Toronto", locale::Country::ca, "ON", "Ontario", "M5H", 43653200,
     -79383200, 2794000},
    {"Mexico City", locale::Country::mx, "CMX", "Mexico City", "06000",
     19432600, -99133200, 9209000},
    {"Mumbai", locale::Country::in, "MH", "Maharashtra", "400001", 19076000,
     72877700, 12442000},
    {"Shanghai", locale::Country::cn, "SH", "Shanghai", "200000", 31230400,
     121473700, 24870000},
    {"Seoul", locale::Country::kr, "SEO", "Seoul", "04524", 37566500, 126978000,
     9776000},
    {"Sao Paulo", locale::Country::br, "SP", "Sao Paulo", "01000", -23550500,
     -46633300, 12330000},
    {"Berlin", locale::Country::de, "BE", "Berlin", "10115", 52520000, 13405000,
     3645000},
    {"Tokyo", locale::Country::jp, "TK", "Tokyo", "100-0001", 35676200,
     139650300, 13960000},
    {"Paris", locale::Country::fr, "IDF", "Ile-de-France", "75001", 48856600,
     2352200, 2161000},
    {"Madrid", locale::Country::es, "MD", "Madrid", "28001", 40416800, -3703800,
     3223000},
    {"Rome", locale::Country::it, "RM", "Lazio", "00118", 41902800, 12496400,
     2761000},
    {"Amsterdam", locale::Country::nl, "NH", "North Holland", "1011", 52367600,
     4904100, 872000},
    {"Sydney", locale::Country::au, "NSW", "New South Wales", "2000", -33868800,
     151209300, 5312000},
    {"Moscow", locale::Country::ru, "MSK", "Moscow", "101000", 55755800,
     37617300, 12615000},
});

[[nodiscard]] GeoCatalog build() {
  std::vector<GeoArea> areas;
  areas.reserve(kUsPlaces.size() + kIntlPlaces.size());

  GeoAreaId id = 0;

  for (const auto &p : kUsPlaces) {
    GeoArea a;
    a.id = ++id;
    a.country = locale::Country::us;
    a.postalAreaCode = std::string{p.zip};
    a.city = std::string{p.city};
    a.stateCode = std::string{usloc::abbrev(p.state)};
    a.stateName = std::string{usloc::fullName(p.state)};
    a.latitudeE6 = p.latE6;
    a.longitudeE6 = p.lonE6;
    a.population = p.population;
    areas.push_back(std::move(a));
  }

  for (const auto &p : kIntlPlaces) {
    GeoArea a;
    a.id = ++id;
    a.country = p.country;
    a.postalAreaCode = std::string{p.postal};
    a.city = std::string{p.city};
    a.stateCode = std::string{p.stateCode};
    a.stateName = std::string{p.stateName};
    a.latitudeE6 = p.latE6;
    a.longitudeE6 = p.lonE6;
    a.population = p.population;
    areas.push_back(std::move(a));
  }

  return GeoCatalog{std::move(areas)};
}

} // namespace

const GeoCatalog &geography() {
  static const GeoCatalog kCatalog = build();
  return kCatalog;
}

} // namespace PhantomLedger::synth::geo
