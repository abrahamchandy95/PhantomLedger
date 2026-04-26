#include "phantomledger/entities/synth/pii/geonames.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"

#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace PhantomLedger::entities::synth::pii {
namespace {

[[nodiscard]] bool parseRow(std::string_view line, ZipEntry &out,
                            std::string &countryOut) {
  const std::size_t t1 = line.find('\t');
  if (t1 == std::string::npos)
    return false;
  const std::size_t t2 = line.find('\t', t1 + 1);
  if (t2 == std::string::npos)
    return false;
  const std::size_t t3 = line.find('\t', t2 + 1);
  if (t3 == std::string::npos)
    return false;
  const std::size_t t4 = line.find('\t', t3 + 1);
  if (t4 == std::string::npos)
    return false;
  const std::size_t t5 = line.find('\t', t4 + 1);
  if (t5 == std::string::npos)
    return false;

  countryOut.assign(line.data(), t1);
  out.postalCode.assign(line.data() + t1 + 1, t2 - t1 - 1);
  out.city.assign(line.data() + t2 + 1, t3 - t2 - 1);
  out.adminName.assign(line.data() + t3 + 1, t4 - t3 - 1);
  out.adminCode.assign(line.data() + t4 + 1, t5 - t4 - 1);
  return true;
}

} // namespace

std::vector<ZipEntry> loadGeoNames(const std::filesystem::path &tsvFile) {
  std::ifstream in(tsvFile);
  if (!in) {
    throw std::runtime_error("loadGeoNames: cannot open " + tsvFile.string());
  }

  std::vector<ZipEntry> out;
  out.reserve(64'000); // rough upper bound for a single-country file

  std::string line;
  std::string country;
  while (std::getline(in, line)) {
    ZipEntry row;
    if (parseRow(line, row, country)) {
      out.push_back(std::move(row));
    }
  }
  return out;
}

std::vector<ZipEntry>
loadGeoNamesForCountry(const std::filesystem::path &tsvFile,
                       locale::Country country) {
  std::ifstream in(tsvFile);
  if (!in) {
    throw std::runtime_error("loadGeoNamesForCountry: cannot open " +
                             tsvFile.string());
  }

  const std::string_view target = locale::code(country);

  std::vector<ZipEntry> out;
  out.reserve(64'000);

  std::string line;
  std::string rowCountry;
  while (std::getline(in, line)) {
    ZipEntry row;
    if (!parseRow(line, row, rowCountry)) {
      continue;
    }
    if (rowCountry == target) {
      out.push_back(std::move(row));
    }
  }
  return out;
}

} // namespace PhantomLedger::entities::synth::pii
