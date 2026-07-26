#pragma once
//
// phantomledger/synth/econ/era_data.hpp
//
// THE PINNED ERA REFERENCE DATA (macro-history-v1), EMBEDDED as
// constexpr tables per the owner's minimize-repo-data-files directive
// (2026-07-24) — this header replaced data/econ/{us_macro_annual,
// us_mortality,sources}.csv and IS the pinned artifact refresh rounds
// rewrite. Everything is integer-encoded exactly as the retired CSVs
// were; nothing is derived or rounded here.
//
// PROVENANCE (full contract: docs/era_data_provenance.md; per-series
// sources: kSources below — the machine-readable refresh registry):
//   * kMacroAnnual — US era series 1990-2024, one row per year.
//       cpiUE3        CPI-U annual average (1982-84=100) x 1000
//                     [VERIFIED EXACT 2026-07-24: the official annual
//                     average IS the mean of the 12 NSA monthly
//                     indexes; recomputed from FRED CPIAUCNS for every
//                     year and matched to the third decimal. The H1
//                     round corrected 1990-2006 from earlier
//                     tenths-rounded transcriptions to the exact
//                     values.]
//       awiCents      SSA national Average Wage Index in cents
//                     [VERIFIED EXACT vs ssa.gov 2026-07-24. LAG: the
//                     AWI for year N publishes ~October N+1, so this
//                     column is THE binding coverage constraint — in
//                     2026-07 the series ends at 2024.]
//       pceDollars    nominal per-capita PCE, exact dollars
//                     [BEA via FRED A794RC0A052NBEA, vintage
//                     2026-04-09; VERIFIED EXACT]
//       unempBp       U-3 ANNUAL AVERAGE x 100 (560 = 5.60%) — NOT
//                     the monthly peak [BLS LNS14000000 official
//                     annual averages; cross-checked 2026-07-24
//                     against FRED UNRATENSA monthly means, which
//                     reproduce every value within 0.1pp (the official
//                     statistic is a ratio of annual averages, not a
//                     mean of monthly rates)]
//       recessionMonths  NBER months strictly AFTER the peak month
//                     through the trough month (1990-91: 5+3=8;
//                     2001: 8; 2008-09: 12+6=18; 2020: 2) [Certain]
//       populationThousands  US population, BEA NIPA MIDPERIOD (the
//                     per-capita PCE denominator — NOT Census July-1)
//                     [FRED B230RC0A052NBEA, vintage 2026-02-20;
//                     VERIFIED EXACT]
//     COVERAGE ENDS 2024 because no later year is fully MEASURED as
//     of 2026-07: AWI 2025 publishes ~2026-10, and the October 2025
//     CPI release and CPS household survey were cancelled outright
//     (federal shutdown), so no official 2025 CPI annual average or
//     unemployment annual average exists. The measured frontier
//     ALWAYS lags now-time — a permanent structural fact any
//     operator (today or in 2050) inherits; appending a fully
//     published year is a data+authority refresh round, never an
//     engine change.
//   * kCalibrationYear — the H1 nominal-scale anchor (below).
//   * kMortality — EXACT transcription of the SSA PERIOD LIFE TABLE
//       FOR 2023 (2026 Trustees Report, Table 4C6, read 2026-07-24):
//       single ages 0-119, qx x 1,000,000 (six-decimal source = exact
//       integers; male == female from age 109 up, source values).
//       DECLARED CHOICE: one table year era-wide; per-year SSA tables
//       are the registered upgrade path.
//   * kSources — the SOURCE REGISTRY (refresh contract): provider,
//       series id, the exact WORKING URL + access method, fallback
//       with axis warnings, transform, coverage, last-verified,
//       status. A refresh round reads THIS registry, never memory,
//       and updates lastVerified in the same round. Re-emitted
//       verbatim as the econ.provenance direct table (H0.5), so cell
//       text must stay comma-free (use semicolons).
//
// Data changes are NEVER silent: every value change is an authority
// round (docs/fraud_model_audit.md U-4/U-5 lineage) + a rewrite of
// this header + green test_econ_catalog/test_econ_tables.
//

#include <array>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::synth::econ::data {

// --- Macro series ------------------------------------------------

struct MacroYearRow {
  int year;
  std::int64_t cpiUE3;
  std::int64_t awiCents;
  std::int64_t pceDollars;
  std::int64_t unempBp;
  std::int64_t recessionMonths;
  std::int64_t populationThousands;
};

inline constexpr std::array<std::string_view, 7> kMacroColumns{
    "year",
    "cpi_u_e3",
    "awi_cents",
    "pce_per_capita_dollars",
    "unemployment_rate_bp",
    "recession_months",
    "population_thousands"};

inline constexpr std::array<MacroYearRow, 35> kMacroAnnual{{
    {1990, 130658, 2102798, 15225, 560, 5, 250181},
    {1991, 136192, 2181160, 15554, 680, 3, 253530},
    {1992, 140317, 2293542, 16338, 750, 0, 256922},
    {1993, 144458, 2313267, 17104, 690, 0, 260282},
    {1994, 148225, 2375353, 17919, 610, 0, 263455},
    {1995, 152383, 2470566, 18615, 560, 0, 266588},
    {1996, 156850, 2591390, 19445, 540, 0, 269714},
    {1997, 160517, 2742600, 20284, 490, 0, 272958},
    {1998, 163008, 2886144, 21283, 450, 0, 276154},
    {1999, 166575, 3046984, 22496, 420, 0, 279328},
    {2000, 172200, 3215482, 23963, 400, 0, 282398},
    {2001, 177067, 3292192, 24801, 470, 8, 285225},
    {2002, 179875, 3325209, 25521, 580, 0, 287955},
    {2003, 183958, 3406495, 26635, 600, 0, 290626},
    {2004, 188883, 3564855, 28070, 550, 0, 293262},
    {2005, 195292, 3695294, 29626, 510, 0, 295993},
    {2006, 201592, 3865141, 31046, 460, 0, 298818},
    {2007, 207342, 4040548, 32306, 460, 0, 301696},
    {2008, 215303, 4133497, 33001, 580, 12, 304543},
    {2009, 214537, 4071161, 32194, 930, 6, 307240},
    {2010, 218056, 4167383, 33115, 960, 0, 309839},
    {2011, 224939, 4297961, 34259, 890, 0, 312295},
    {2012, 229594, 4432167, 35102, 810, 0, 314725},
    {2013, 232957, 4488816, 35914, 740, 0, 317099},
    {2014, 236736, 4648152, 37154, 620, 0, 319601},
    {2015, 237017, 4809863, 38177, 530, 0, 322113},
    {2016, 240007, 4864215, 39207, 490, 0, 324609},
    {2017, 245120, 5032189, 40662, 440, 0, 326860},
    {2018, 251107, 5214580, 42380, 390, 0, 328794},
    {2019, 255657, 5409999, 43682, 370, 0, 330513},
    {2020, 258811, 5562860, 42886, 810, 2, 331840},
    {2021, 270970, 6057507, 48480, 530, 0, 332503},
    {2022, 292655, 6379513, 52909, 360, 0, 334350},
    {2023, 304702, 6662180, 55870, 360, 0, 337087},
    {2024, 313689, 6984657, 58501, 400, 0, 340095},
}};

// --- Calibration year (H1 nominal-scale anchor) -------------------
// THE YEAR THE AUTHORITY'S CALIBRATED DOLLAR CONSTANTS ARE DENOMINATED
// IN. The conformance program anchored PhantomLedger's dollar
// magnitudes to measurements taken roughly 2015-2024; the owner-
// approved CHOICE (macro-history-2026-07c) declares that calibrated
// economy 2019-denominated: the last full simulated year of the
// canonical card-fraud window and the last pre-COVID year, so the era
// ramps to a scale of exactly 1.0 at its endpoint. This is a
// PROVENANCE FACT OF THE CALIBRATION DATA — not "the present", not
// the last covered year, never the wall clock or the run's start
// date. It changes ONLY together with the constants it denominates,
// in a named model-moving round (per-constant measurement vintages
// are the registered upgrade path). Builder-validated to lie inside
// coverage; H1 consumers scale by index(year)/index(kCalibrationYear).
inline constexpr int kCalibrationYear = 2019;

// --- Mortality (SSA 2023 period life table, 2026 TR) -------------

struct MortalityRow {
  int age;
  std::int64_t qxMaleE6;
  std::int64_t qxFemaleE6;
};

inline constexpr std::array<std::string_view, 3> kMortalityColumns{
    "age", "qx_male_e6", "qx_female_e6"};

inline constexpr std::array<MortalityRow, 120> kMortality{{
    {0, 6015, 5125},       {1, 479, 392},         {2, 320, 229},
    {3, 249, 188},         {4, 194, 155},         {5, 159, 133},
    {6, 137, 115},         {7, 125, 105},         {8, 120, 100},
    {9, 120, 98},          {10, 125, 101},        {11, 140, 111},
    {12, 173, 126},        {13, 233, 152},        {14, 327, 188},
    {15, 463, 229},        {16, 634, 273},        {17, 819, 323},
    {18, 999, 372},        {19, 1138, 410},       {20, 1235, 441},
    {21, 1315, 476},       {22, 1378, 513},       {23, 1439, 546},
    {24, 1509, 582},       {25, 1595, 609},       {26, 1685, 641},
    {27, 1783, 683},       {28, 1876, 740},       {29, 1970, 808},
    {30, 2085, 878},       {31, 2202, 947},       {32, 2308, 1018},
    {33, 2407, 1089},      {34, 2490, 1154},      {35, 2577, 1209},
    {36, 2665, 1263},      {37, 2764, 1347},      {38, 2864, 1438},
    {39, 2987, 1533},      {40, 3115, 1643},      {41, 3253, 1742},
    {42, 3419, 1845},      {43, 3600, 1954},      {44, 3777, 2075},
    {45, 3931, 2187},      {46, 4073, 2306},      {47, 4245, 2438},
    {48, 4477, 2595},      {49, 4795, 2791},      {50, 5126, 3030},
    {51, 5496, 3288},      {52, 5917, 3554},      {53, 6404, 3847},
    {54, 6923, 4172},      {55, 7491, 4532},      {56, 8173, 4923},
    {57, 8938, 5365},      {58, 9714, 5815},      {59, 10494, 6333},
    {60, 11337, 6923},     {61, 12232, 7555},     {62, 13196, 8220},
    {63, 14229, 8881},     {64, 15316, 9514},     {65, 16455, 10188},
    {66, 17574, 10880},    {67, 18735, 11659},    {68, 19981, 12543},
    {69, 21366, 13581},    {70, 22903, 14769},    {71, 24615, 16153},
    {72, 26504, 17705},    {73, 28648, 19495},    {74, 31071, 21533},
    {75, 33802, 23846},    {76, 37010, 26458},    {77, 41158, 29700},
    {78, 45461, 33135},    {79, 50346, 36982},    {80, 55633, 41183},
    {81, 61757, 45959},    {82, 68358, 51282},    {83, 75420, 57262},
    {84, 83364, 64107},    {85, 92680, 71752},    {86, 103459, 80490},
    {87, 115502, 90566},   {88, 129018, 102204},  {89, 143810, 115178},
    {90, 159458, 129176},  {91, 176551, 144229},  {92, 195360, 160353},
    {93, 216286, 177635},  {94, 238799, 196502},  {95, 262268, 216846},
    {96, 286291, 238750},  {97, 310944, 261359},  {98, 332325, 283899},
    {99, 349036, 306491},  {100, 366568, 329680}, {101, 384960, 353333},
    {102, 404252, 377300}, {103, 424488, 401416}, {104, 445712, 425501},
    {105, 467998, 451031}, {106, 491398, 478092}, {107, 515968, 506778},
    {108, 541766, 537185}, {109, 568854, 568854}, {110, 597297, 597297},
    {111, 627162, 627162}, {112, 658520, 658520}, {113, 691446, 691446},
    {114, 726018, 726018}, {115, 762319, 762319}, {116, 800435, 800435},
    {117, 840457, 840457}, {118, 882480, 882480}, {119, 926604, 926604},
}};

// --- Source registry (the refresh contract) ----------------------
// Cell text must stay comma-free (semicolons instead): these rows are
// re-emitted verbatim as the econ.provenance direct table.

struct SourceRow {
  std::string_view seriesKey;
  std::string_view target;
  std::string_view provider;
  std::string_view seriesId;
  std::string_view access;
  std::string_view urlPrimary;
  std::string_view urlFallback;
  std::string_view transform;
  std::string_view publishedFrom;
  std::string_view lastVerified;
  std::string_view status;
};

inline constexpr std::array<std::string_view, 11> kSourceColumns{
    "series_key",     "target",       "provider", "series_id",
    "access",         "url_primary",  "url_fallback", "transform",
    "published_from", "last_verified", "status"};

inline constexpr std::array<SourceRow, 10> kSources{{
    {"cpi_u", "econ.macro_annual:cpi_u_e3", "BLS (via FRED mirror)",
     "CUUR0000SA0 == FRED CPIAUCNS (monthly NSA; official annual avg = "
     "mean of the 12 monthly indexes)",
     "html_data_view_monthly_mean",
     "https://fred.stlouisfed.org/data/CPIAUCNS",
     "https://data.bls.gov/timeseries/CUUR0000SA0 (bls.gov timed out "
     "2026-07-24; FRED mirror verified exact instead)",
     "index x 1000", "1913", "2026-07-24", "verified_exact"},
    {"awi", "econ.macro_annual:awi_cents", "SSA",
     "national Average Wage Index (year N publishes ~Oct N+1 - THE "
     "binding coverage constraint)",
     "html_table",
     "https://www.ssa.gov/oact/cola/awiseries.html",
     "https://www.ssa.gov/oact/COLA/AWI.html (same series; narrative page)",
     "dollars x 100", "1951", "2026-07-24", "verified_exact"},
    {"pce_per_capita", "econ.macro_annual:pce_per_capita_dollars",
     "BEA (GDP release)", "A794RC0A052NBEA", "html_data_view",
     "https://fred.stlouisfed.org/data/A794RC0A052NBEA",
     "https://www.bea.gov/data/consumer-spending/main", "dollars (exact)",
     "1929", "2026-07-24", "verified_exact"},
    {"unemployment", "econ.macro_annual:unemployment_rate_bp", "BLS",
     "LNS14000000 (U-3 annual avg; official statistic is a ratio of "
     "annual averages)",
     "html_table",
     "https://data.bls.gov/timeseries/LNS14000000",
     "https://fred.stlouisfed.org/data/UNRATENSA (monthly NSA; yearly "
     "means reproduce every official value within 0.1pp - cross-checked "
     "2026-07-24)",
     "percent x 100", "1948", "2026-07-24", "transcribed_crosschecked_0p1pp"},
    {"recession_months", "econ.macro_annual:recession_months", "NBER",
     "business-cycle dating committee chronology", "derived",
     "https://www.nber.org/research/business-cycle-dating", "",
     "months strictly AFTER peak month through trough month per calendar "
     "year",
     "1857", "2026-07-24", "derived_certain_dates"},
    {"population", "econ.macro_annual:population_thousands",
     "BEA (GDP release)",
     "B230RC0A052NBEA (NIPA midperiod; the per-capita PCE denominator)",
     "html_data_view", "https://fred.stlouisfed.org/data/B230RC0A052NBEA",
     "https://www.census.gov/programs-surveys/popest.html (DIFFERENT AXIS "
     "- Census July-1 resident; <0.3% apart; do not mix)",
     "thousands (exact)", "1929", "2026-07-24", "verified_exact"},
    {"mortality_qx", "econ.mortality:qx_male_e6+qx_female_e6", "SSA OACT",
     "Period Life Table 2023 (2026 Trustees Report; Table 4C6)",
     "html_table", "https://www.ssa.gov/oact/STATS/table4c6.html",
     "https://www.ssa.gov/oact/HistEst/PerLifeTables/PerLifeTables.html "
     "(per-year historical period tables; the 4C6 page dropdown lists "
     "2004-2023)",
     "qx x 1000000 (6-decimal source = exact)",
     "2004 (per-year tables; older in TR archives)", "2026-07-24",
     "verified_exact"},
    {"remote_share", "H5 (planned)", "Federal Reserve",
     "triennial payments study + NPIPS", "html_table",
     "https://www.federalreserve.gov/paymentsystems/fr-payments-study.htm",
     "https://www.federalreserve.gov/paymentsystems/frps_npips24ddr.htm",
     "share x 10000 (proposed)", "2001", "", "planned"},
    {"debit_credit_mix", "H5 (planned)", "Federal Reserve",
     "triennial payments study", "html_table",
     "https://www.federalreserve.gov/paymentsystems/fr-payments-study.htm",
     "", "share x 10000 (proposed)", "2001", "", "planned"},
    {"ecommerce_share", "H5 (planned)", "Census",
     "quarterly/annual e-commerce retail share", "html_table",
     "https://www.census.gov/retail/ecommerce.html", "",
     "share x 10000 (proposed)", "1999", "", "planned"},
}};

} // namespace PhantomLedger::synth::econ::data
