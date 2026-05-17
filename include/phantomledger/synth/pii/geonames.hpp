#pragma once

#include "phantomledger/taxonomies/locale/types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace PhantomLedger::synth::pii {

struct ZipEntry {
  std::string postalCode;
  std::string city;
  std::string adminName;
  std::string adminCode;
};

[[nodiscard]] std::vector<ZipEntry>
loadGeoNames(const std::filesystem::path &tsvFile);

[[nodiscard]] std::vector<ZipEntry>
loadGeoNamesForCountry(const std::filesystem::path &tsvFile,
                       locale::Country country);

} // namespace PhantomLedger::synth::pii
