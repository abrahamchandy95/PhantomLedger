#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

/*
  When a card number is replaced.

  SOURCE: Auriemma Consulting Group (via American Banker) — ~50% of US
  cardholders had at least one reissuance within a year, against a traditional
  ~5-year expiry cycle, and cardholders hit by repeat fraud reported ~5 cards
  in a single year. The decomposition by cause:

      expiration                     33%
      fraudulent charges             27%
      magstripe -> EMV conversion    26%
      lost / stolen / damaged       ~14%

  Mercator's 2019 U.S. PaymentsInsights adds the physical side: 9% of
  consumers reported card theft, 29% reported a loss, theft or fraudulent
  charge, 71% reported nothing.

  THREE of the four causes are modelled: EXPIRY (scheduled, the metronome the
  others ride on), LOST/STOLEN/DAMAGED (an over-dispersed hazard), and the EMV
  CONVERSION wave (an era-specific one-off).

  FRAUD-DRIVEN REISSUE IS DELIBERATELY NOT MODELLED — a leak decision, not an
  oversight. Reissue after compromise is causally downstream of fraud, so
  "this party holds more than one card number" would become an entity-level
  fraud label. The only fraud signal available at export time is `seen.fraud`,
  a FULL-WINDOW verdict, which is the exact column `cf_Card.is_fraud` is
  withheld as 0 for. Doing it correctly means reissuing strictly AFTER an
  observed fraud row using only fraud seen up to that point, and needs its own
  lift gate. Until then the 27% share is a REGISTERED LIMITATION: exported
  churn is lower than reality and carries NO fraud correlation, which is the
  safe direction to be wrong in.

  THE DISTRIBUTION IS OVER-DISPERSED, NOT POISSON. "~50% of cardholders get
  one reissue a year" and "repeat-fraud victims get ~5" cannot come from one
  Poisson rate — the variance far exceeds the mean. The unscheduled component
  is a GAMMA-MIXED POISSON: a per-card latent proneness with mean EXACTLY 1,
  so it rescales the base rate without moving the population average, then
  events at that scaled rate.

  PRONENESS IS KEYED ON THE CARD, NOT THE PERSON, because Auriemma reports
  that among repeat-fraud cases 89% had all the fraudulent charges on ONE
  card. It is a property of how a specific card is used, not of its holder.

  DRAW-FREE: every quantity is a hash of the card key and the generation
  index, so the corpus stream cannot move. It need not — the corpus carries
  account keys and no card identity at all (`transactions::Transaction` has no
  card field, and no standard or AML table has a card number). Card identity
  exists only in the card-fraud exporter.

  CLASS: MEASUREMENT-derived shares, levels [Likely]. The cause decomposition
  and the ~50%/year headline are published Auriemma figures; the validity term
  is the documented 3-5 year range. The dispersion shape is CLASS S UNCITED —
  its DIRECTION is pinned by the 50%-vs-5-cards contrast, its magnitude is a
  declared choice.
 */

namespace PhantomLedger::entity::card::reissue {

/* Validity term in months. Sources put the range at 3-5 years, most
 * commonly 3; Auriemma describes the traditional cycle as ~5. The mix below
 * spans both readings. */
inline constexpr int kValidityMonthsMin = 36;
inline constexpr int kValidityMonthsMax = 60;

/* Unscheduled (lost / stolen / damaged) replacements per card-year, BEFORE
 * the Gamma mixing. Derived from Auriemma's ~14% lost/stolen/damaged share
 * of reissues against a ~50%/year total reissue rate: 0.50 * 0.14 = 0.07. */
inline constexpr double kUnscheduledPerYear = 0.07;

/* Lognormal sigma for the per-card proneness. CLASS S UNCITED — the
 * DIRECTION is pinned by Auriemma's 50%-vs-5-cards contrast, the magnitude
 * is declared. At 0.8 the 99th percentile sits near 5x the base rate. */
inline constexpr double kDispersionSigma = 0.80;

/* The EMV conversion wave. 26% of Auriemma's reissues, concentrated in the
 * US migration around the October 2015 network liability shift. Modelled as
 * a ONE-OFF replacement for cards live across the window, not a rate. */
inline constexpr int kEmvWaveFirstYear = 2015;
inline constexpr int kEmvWaveLastYear = 2017;
inline constexpr double kEmvWaveShare = 0.26 / 0.50; // share of live cards

struct Rules {
  int validityMonthsMin = kValidityMonthsMin;
  int validityMonthsMax = kValidityMonthsMax;
  double unscheduledPerYear = kUnscheduledPerYear;
  double dispersionSigma = kDispersionSigma;
  double emvWaveShare = kEmvWaveShare;
  int emvWaveFirstYear = kEmvWaveFirstYear;
  int emvWaveLastYear = kEmvWaveLastYear;
};

namespace detail {

[[nodiscard]] constexpr std::uint64_t mix(std::uint64_t hash,
                                          std::uint64_t value) noexcept {
  for (int i = 0; i < 8; ++i) {
    hash ^= (value >> (8 * i)) & 0xFFU;
    hash *= ::PhantomLedger::hashing::constants::fnv64_prime;
  }
  return hash;
}

[[nodiscard]] constexpr std::uint64_t avalanche(std::uint64_t hash) noexcept {
  hash += 0x9E3779B97F4A7C15ULL;
  hash = (hash ^ (hash >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  hash = (hash ^ (hash >> 27U)) * 0x94D049BB133111EBULL;
  return hash ^ (hash >> 31U);
}

/* Independent domains so the validity term, the proneness draw, the
 * unscheduled event times and the EMV membership cannot correlate with one
 * another through a shared hash. */
inline constexpr std::uint64_t kValidityDomain = 0x43'4152'4400'0001ULL;
inline constexpr std::uint64_t kPronenessDomain = 0x43'4152'4400'0002ULL;
inline constexpr std::uint64_t kEventDomain = 0x43'4152'4400'0003ULL;
inline constexpr std::uint64_t kEmvDomain = 0x43'4152'4400'0004ULL;

[[nodiscard]] constexpr std::uint64_t keyHash(std::uint64_t domain,
                                              const entity::Key &key,
                                              std::uint64_t salt) noexcept {
  auto hash = ::PhantomLedger::hashing::constants::fnv64_offset;
  hash = mix(hash, domain);
  hash = mix(hash, static_cast<std::uint8_t>(key.role));
  hash = mix(hash, static_cast<std::uint8_t>(key.bank));
  hash = mix(hash, key.number);
  hash = mix(hash, salt);
  return avalanche(hash);
}

[[nodiscard]] constexpr double unitOf(std::uint64_t hash) noexcept {
  return static_cast<double>(hash >> 11U) * 0x1.0p-53;
}

/* MEAN-1 LOGNORMAL PRONENESS.
 *
 * Scaled as `exp(sigma*z - sigma^2/2)` the mean is EXACTLY 1, so it rescales
 * the base rate without moving the population average — which is what lets
 * the base rate stay the value derived from Auriemma's shares. This is
 * already the repo's idiom for over-dispersion (`lognormalByMedian` for
 * attacker campaigns and merchant weights).
 *
 * DO NOT SUBSTITUTE A HAND-ROLLED GAMMA APPROXIMATION. One that repeatedly
 * multiplied by 1/shape measured mean 10.2 generations per card over 20 years
 * against an expiry-only expectation of ~5, variance 99, with tail cards
 * reaching a 64x rate multiplier — 4.5 unscheduled replacements a YEAR. The
 * right qualitative direction does not make an unprincipled approximation
 * correct.
 *
 * The normal comes from Box-Muller over two independent hash uniforms, so
 * this stays draw-free. `sigma = 0.8` puts the 99th percentile near 5x the
 * base rate, matching the "repeat-fraud cardholders reported ~5 cards"
 * observation; the hard cap is a runaway guard, not a modelling statement. */
[[nodiscard]] inline double pronenessOf(const entity::Key &key,
                                        double sigma) noexcept {
  const double u1 =
      std::max(1.0e-12, unitOf(keyHash(kPronenessDomain, key, 0)));
  const double u2 = unitOf(keyHash(kPronenessDomain, key, 1));
  constexpr double kTwoPi = 6.283185307179586;
  const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(kTwoPi * u2);
  const double safeSigma = sigma > 0.0 ? sigma : 0.0;
  const double scale = std::exp(safeSigma * z - 0.5 * safeSigma * safeSigma);
  return std::clamp(scale, 0.0, 8.0);
}

} // namespace detail

/* One issued card number's operating interval, half-open, matching the
 * convention `infra::Tenure` and `merchant::Record` use. */
struct Generation {
  std::uint32_t index = 0; // 0 = the originally issued card
  std::int64_t firstEpoch = 0;
  std::int64_t lastEpochExcl = 0;

  [[nodiscard]] constexpr bool liveAt(std::int64_t ts) const noexcept {
    return ts >= firstEpoch && ts < lastEpochExcl;
  }
};

/* This card's validity term in months, drawn draw-free from the key. */
[[nodiscard]] constexpr int validityMonths(const entity::Key &key,
                                           const Rules &rules = {}) noexcept {
  const auto span = rules.validityMonthsMax - rules.validityMonthsMin;
  if (span <= 0) {
    return rules.validityMonthsMin;
  }
  const auto u =
      detail::unitOf(detail::keyHash(detail::kValidityDomain, key, 0));
  return rules.validityMonthsMin +
         static_cast<int>(u * static_cast<double>(span + 1));
}

/* This card's unscheduled-replacement proneness. Mean ~1 across the
 * population, heavy-tailed: most cards near 1, a minority several times
 * higher. See the header on why this is keyed on the CARD not the person. */
[[nodiscard]] constexpr double proneness(const entity::Key &key,
                                         const Rules &rules = {}) noexcept {
  return detail::pronenessOf(key, rules.dispersionSigma);
}

/* Does this card fall in the EMV conversion wave? */
[[nodiscard]] constexpr bool inEmvWave(const entity::Key &key,
                                       const Rules &rules = {}) noexcept {
  return detail::unitOf(detail::keyHash(detail::kEmvDomain, key, 0)) <
         std::min(1.0, rules.emvWaveShare);
}

/* Every card number this account carries across `[windowStart,
 * windowEndExcl)`, in order, with contiguous half-open intervals that tile the
 * window exactly.
 *
 * Generation 0 starts at `windowStart`. Cards issued before the window are
 * not modelled as such — a card's real issue date precedes the corpus and
 * nothing reads it — so generation 0 simply runs until its first
 * replacement, which is the same treatment `merchant::Record` gives an
 * incumbent's open date.
 *
 * THE THREE CAUSES COMPETE, and whichever comes first wins:
 *   * expiry at each `validityMonths` boundary from the window start;
 *   * unscheduled loss/theft/damage at a Gamma-mixed rate;
 *   * the one-off EMV conversion, for cards in that wave, placed inside
 *     the migration years.
 *
 * DRAW-FREE: every date is a hash of the key and the generation index. */
[[nodiscard]] inline std::vector<Generation>
generationsFor(const entity::Key &key, std::int64_t windowStart,
               std::int64_t windowEndExcl, const Rules &rules = {}) {
  std::vector<Generation> out;
  if (windowEndExcl <= windowStart) {
    return out;
  }

  constexpr std::int64_t kSecondsPerDay = 86'400;
  constexpr double kDaysPerMonth = 30.4375; // 365.25 / 12
  constexpr double kDaysPerYear = 365.25;

  const auto validity = validityMonths(key, rules);
  const auto validitySeconds = static_cast<std::int64_t>(
      static_cast<double>(validity) * kDaysPerMonth * kSecondsPerDay);

  /* The unscheduled rate for THIS card, after Gamma mixing. */
  const double rate = rules.unscheduledPerYear * proneness(key, rules);

  /* EMV wave date, if this card is in it: placed inside the migration
   * window by a hash so the wave has realistic spread rather than a spike. */
  std::int64_t emvAt = 0;
  if (inEmvWave(key, rules)) {
    const auto u = detail::unitOf(detail::keyHash(detail::kEmvDomain, key, 1));
    /* Years since 1970 to the wave start, approximated in days; the exact
     * day inside the wave does not matter, only that it lands in it. */
    const auto waveStartDays = static_cast<std::int64_t>(
        (static_cast<double>(rules.emvWaveFirstYear) - 1970.0) * kDaysPerYear);
    const auto waveSpanDays = static_cast<std::int64_t>(
        (static_cast<double>(rules.emvWaveLastYear - rules.emvWaveFirstYear +
                             1)) *
        kDaysPerYear);
    emvAt = (waveStartDays +
             static_cast<std::int64_t>(u * static_cast<double>(waveSpanDays))) *
            kSecondsPerDay;
  }

  std::uint32_t index = 0;
  std::int64_t cursor = windowStart;

  /* Bounded: a card cannot plausibly turn over more than this in one run,
   * and the cap is a runaway guard rather than a modelling statement. When
   * it binds, the final generation simply runs to the window end. */
  constexpr std::uint32_t kMaxGenerations = 64;

  while (cursor < windowEndExcl && index < kMaxGenerations) {
    std::int64_t next = cursor + validitySeconds;

    /* Unscheduled event for THIS generation: an exponential waiting time at
     * the card's mixed rate, hashed per generation so successive intervals
     * are independent. */
    if (rate > 0.0) {
      const auto u = std::max(1.0e-12, detail::unitOf(detail::keyHash(
                                           detail::kEventDomain, key, index)));
      /* -ln(u)/rate years until the unscheduled replacement. */
      const double years = -std::log(u) / rate;
      const auto waitSeconds = static_cast<std::int64_t>(
          years * kDaysPerYear * static_cast<double>(kSecondsPerDay));
      next = std::min(next, cursor + std::max<std::int64_t>(1, waitSeconds));
    }

    /* The EMV conversion, if it falls inside this generation. */
    if (emvAt > cursor && emvAt < next) {
      next = emvAt;
    }

    const auto end = std::min(next, windowEndExcl);
    out.push_back(Generation{index, cursor, end});
    cursor = end;
    ++index;
  }

  /* The cap bound, or a rounding tie: extend the last generation so the
   * sequence TILES the window with no gap. A gap would leave transactions in
   * it with no resolvable card number, which the exporter cannot represent. */
  if (!out.empty() && out.back().lastEpochExcl < windowEndExcl) {
    out.back().lastEpochExcl = windowEndExcl;
  }
  return out;
}

/* The generation live at `ts`, or index 0 when the sequence is empty. */
[[nodiscard]] inline std::uint32_t
generationAt(const std::vector<Generation> &generations,
             std::int64_t ts) noexcept {
  for (const auto &generation : generations) {
    if (generation.liveAt(ts)) {
      return generation.index;
    }
  }
  return generations.empty() ? 0 : generations.back().index;
}

} // namespace PhantomLedger::entity::card::reissue
