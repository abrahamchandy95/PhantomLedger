#include "phantomledger/synth/geo/catalog.hpp"

#include "phantomledger/taxonomies/locale/names.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef PL_GEO_DATA
#error "PL_GEO_DATA must be defined (build-time path to data/geo/us_cities.csv)"
#endif

namespace PhantomLedger::synth::geo {

namespace {

using entity::geography::GeoArea;
using entity::geography::GeoAreaId;
using entity::geography::GeoCatalog;

// Column names; row order in the file defines the 1-based GeoAreaId.
enum Column : std::size_t {
  kCountry = 0,
  kPostalAreaCode,
  kCity,
  kStateCode,
  kStateName,
  kLatitudeE6,
  kLongitudeE6,
  kPopulation,
  kColumnCount,
};

inline constexpr std::array<std::string_view, kColumnCount> kColumnNames{
    "country",      "postal_area_code", "city",
    "state_code",   "state_name",       "latitude_e6",
    "longitude_e6", "population"};

[[nodiscard]] std::vector<std::string_view> splitCsv(std::string_view line) {
  std::vector<std::string_view> cells;
  std::size_t start = 0;
  while (true) {
    const std::size_t comma = line.find(',', start);
    if (comma == std::string_view::npos) {
      cells.push_back(line.substr(start));
      break;
    }
    cells.push_back(line.substr(start, comma - start));
    start = comma + 1;
  }
  return cells;
}

[[nodiscard]] std::string_view trim(std::string_view s) noexcept {
  const auto space = [](unsigned char c) { return std::isspace(c) != 0; };
  std::size_t b = 0;
  while (b < s.size() && space(static_cast<unsigned char>(s[b]))) {
    ++b;
  }
  std::size_t e = s.size();
  while (e > b && space(static_cast<unsigned char>(s[e - 1]))) {
    --e;
  }
  return s.substr(b, e - b);
}

[[nodiscard]] bool skippable(std::string_view line) noexcept {
  const auto t = trim(line);
  return t.empty() || t.front() == '#';
}

[[nodiscard]] std::optional<locale::Country>
parseCountry(std::string_view token) {
  std::string upper{trim(token)};
  for (char &c : upper) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  for (const auto country : locale::kCountries) {
    if (locale::code(country) == upper) {
      return country;
    }
  }
  return std::nullopt;
}

template <typename Int>
[[nodiscard]] bool parseInt(std::string_view token, Int &out) {
  const auto t = trim(token);
  if (t.empty()) {
    return false;
  }
  const auto [ptr, ec] = std::from_chars(t.data(), t.data() + t.size(), out);
  return ec == std::errc{} && ptr == t.data() + t.size();
}

[[noreturn]] void fail(std::size_t lineNo, const std::string &why) {
  throw std::runtime_error(std::string{"geo catalogue "} + PL_GEO_DATA + ":" +
                           std::to_string(lineNo) + ": " + why);
}

[[nodiscard]] GeoCatalog load() {
  std::ifstream in(PL_GEO_DATA);
  if (!in) {
    throw std::runtime_error(
        std::string{"geo catalogue: cannot open "} + PL_GEO_DATA +
        " (built-in path; the file ships with the repo)");
  }

  std::string line;
  std::size_t lineNo = 0;

  std::array<std::size_t, kColumnCount> col{};
  bool haveHeader = false;
  while (std::getline(in, line)) {
    ++lineNo;
    if (skippable(line)) {
      continue;
    }
    const auto header = splitCsv(line);
    for (std::size_t want = 0; want < kColumnCount; ++want) {
      bool found = false;
      for (std::size_t i = 0; i < header.size(); ++i) {
        if (trim(header[i]) == kColumnNames[want]) {
          col[want] = i;
          found = true;
          break;
        }
      }
      if (!found) {
        fail(lineNo, "header missing required column '" +
                         std::string{kColumnNames[want]} + "'");
      }
    }
    haveHeader = true;
    break;
  }
  if (!haveHeader) {
    throw std::runtime_error(std::string{"geo catalogue "} + PL_GEO_DATA +
                             ": no header row");
  }

  std::vector<GeoArea> areas;
  while (std::getline(in, line)) {
    ++lineNo;
    if (skippable(line)) {
      continue;
    }
    const auto cells = splitCsv(line);
    std::size_t maxCol = 0;
    for (const auto c : col) {
      maxCol = c > maxCol ? c : maxCol;
    }
    if (cells.size() <= maxCol) {
      fail(lineNo, "row has too few fields");
    }

    GeoArea a;
    a.id = static_cast<GeoAreaId>(areas.size() + 1);
    const auto country = parseCountry(cells[col[kCountry]]);
    if (!country) {
      fail(lineNo, "unknown country '" +
                       std::string{trim(cells[col[kCountry]])} + "'");
    }
    a.country = *country;
    a.postalAreaCode = std::string{trim(cells[col[kPostalAreaCode]])};
    a.city = std::string{trim(cells[col[kCity]])};
    a.stateCode = std::string{trim(cells[col[kStateCode]])};
    a.stateName = std::string{trim(cells[col[kStateName]])};
    if (!parseInt(cells[col[kLatitudeE6]], a.latitudeE6)) {
      fail(lineNo, "latitude_e6 is not an integer");
    }
    if (!parseInt(cells[col[kLongitudeE6]], a.longitudeE6)) {
      fail(lineNo, "longitude_e6 is not an integer");
    }
    if (!parseInt(cells[col[kPopulation]], a.population)) {
      fail(lineNo, "population is not an unsigned integer");
    }
    areas.push_back(std::move(a));
  }

  if (areas.empty()) {
    throw std::runtime_error(std::string{"geo catalogue "} + PL_GEO_DATA +
                             ": no data rows");
  }
  return GeoCatalog{std::move(areas)};
}

} // namespace

const GeoCatalog &geography() {
  static const GeoCatalog kCatalog = load();
  return kCatalog;
}

} // namespace PhantomLedger::synth::geo
