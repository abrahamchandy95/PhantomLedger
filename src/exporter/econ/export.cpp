#include "phantomledger/exporter/econ/export.hpp"

#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/synth/econ/catalog.hpp"
#include "phantomledger/synth/econ/era_data.hpp"

#include <span>
#include <string_view>

namespace PhantomLedger::exporter::econ {

namespace {

namespace eradata = ::PhantomLedger::synth::econ::data;

[[nodiscard]] schema::Table
descriptor(std::string_view filename,
           std::span<const std::string_view> header) noexcept {
  return schema::Table{filename, header};
}

void writeMacro(const common::TableTarget &target) {
  auto table =
      common::openTable(target, descriptor("macro_annual.csv",
                                           eradata::kMacroColumns));
  for (const auto &row : eradata::kMacroAnnual) {
    table.writer().writeRow(row.year, row.cpiUE3, row.awiCents,
                            row.pceDollars, row.unempBp, row.recessionMonths,
                            row.populationThousands);
  }
  table.close();
}

void writeMortality(const common::TableTarget &target) {
  auto table = common::openTable(
      target, descriptor("mortality.csv", eradata::kMortalityColumns));
  for (const auto &row : eradata::kMortality) {
    table.writer().writeRow(row.age, row.qxMaleE6, row.qxFemaleE6);
  }
  table.close();
}

void writeProvenance(const common::TableTarget &target) {
  auto table = common::openTable(
      target, descriptor("provenance.csv", eradata::kSourceColumns));
  for (const auto &row : eradata::kSources) {
    table.writer().writeRow(row.seriesKey, row.target, row.provider,
                            row.seriesId, row.access, row.urlPrimary,
                            row.urlFallback, row.transform, row.publishedFrom,
                            row.lastVerified, row.status);
  }
  table.close();
}

} // namespace

void writeEraTables(const common::TableTarget &target) {
  // Validate through the builders FIRST: a malformed rewrite of the
  // embedded era data fails through synth::econ's structural checks
  // before any table lands.
  (void)synth::econ::macroSeries();
  (void)synth::econ::mortality();

  writeMacro(target);
  writeMortality(target);
  writeProvenance(target);
}

} // namespace PhantomLedger::exporter::econ
