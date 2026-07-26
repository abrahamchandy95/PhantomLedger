#pragma once
//
// phantomledger/exporter/econ/export.hpp
//
// Era reference tables (macro-history-v1 H0.5, owner directive #2).
// Writes the pinned economic-era reference data into PostgreSQL for
// EVERY use case, so downstream in-database modeling can JOIN
// transactions against era context (CPI, wages, unemployment,
// recession months, mortality) without touching repo artifacts:
//
//   econ.macro_annual   <- synth::econ::data::kMacroAnnual
//   econ.mortality      <- synth::econ::data::kMortality
//   econ.provenance     <- synth::econ::data::kSources (the registry)
//
// The tables are a REPORT of the EMBEDDED pinned data (era_data.hpp —
// the constexpr tables that replaced the retired data/econ CSVs),
// rendered cell-for-cell — the exporter derives nothing and hashes
// nothing, and generation itself keeps reading the same embedded data
// through synth::econ (the database stays disposable; it is never a
// source). They live in their OWN schema so the public-schema table
// golden does not move; content is pinned serverlessly by
// test_econ_tables through the TableCapture seam.
//
// The embedded data is validated through synth::econ's builders
// before any table lands, so a malformed data rewrite fails loudly
// first.
//

#include "phantomledger/exporter/common/table.hpp"

namespace PhantomLedger::exporter::econ {

void writeEraTables(const common::TableTarget &target);

} // namespace PhantomLedger::exporter::econ
