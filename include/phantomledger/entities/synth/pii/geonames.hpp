#pragma once

#include "phantomledger/taxonomies/locale/types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace PhantomLedger::entities::synth::pii {

struct ZipEntry {
  std::string postalCode;
  std::string city;
  std::string
      adminName; ///< full region name: "New York", "Bavaria", "Île-de-France"
  std::string adminCode; ///< short region code: "NY", "BY", "11"
};

[[nodiscard]] std::vector<ZipEntry>
loadGeoNames(const std::filesystem::path &tsvFile);

[[nodiscard]] std::vector<ZipEntry>
loadGeoNamesForCountry(const std::filesystem::path &tsvFile,
                       locale::Country country);

} // namespace PhantomLedger::entities::synth::pii
