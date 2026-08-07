#pragma once

/*
  THE PER-PERSON CARD-VIEW INSTRUMENT SET, and the DRAW-FREE choice among it.

  `Spender` used to carry exactly one `depositAccount` Key and exactly one
  `card` Key, and `selectPaymentRoute` returned one of those two on all four
  of its return paths. A person could therefore present AT MOST TWO distinct
  card-view source accounts, and since every legitimate device belongs to
  exactly one person, NO legitimate device could reach three distinct cards.
  That ceiling — not any property of the device generator — is the whole of
  the exported device fan-out cliff.

  TWO CAPACITIES, WITH DIFFERENT PROVENANCE, AND THEY MUST NOT BE CONFLATED:

    * `kMaxDepositInstruments` is STRUCTURAL. It equals
      `synth::accounts::makePack`'s `maxPerPerson = 3`, which is the support
      of the existing `Binomial(2, 0.25) + 1` account-count draw. It is not a
      choice; if that draw's support ever changes, this must move with it.
    * `kMaxCreditInstruments` is a DECLARED CHOICE and it is a SPEND-ACTIVE
      cap, not a holdings cap. The Fed SDCPC card-count law governs how many
      cards a person HOLDS (`entity::card::kMaxCardsPerPerson`); this bounds
      how many of them are wired into the hot loop, so the CardCycleDriver's
      statement/payment/interest blast radius stays bounded while the held
      count still reaches the cited law. The direction is anchored on the
      bureau-side active-versus-held gap (Experian reports ~3.84 ACTIVE cards
      against a materially larger held count, accessed 2026-08-07); the exact
      integer is declared, not measured.

  THE PICK IS DRAW-FREE, AND THAT IS THE LOAD-BEARING PROPERTY.
  `selectPaymentRoute` spends exactly one uniform (`rng.coin(cardShare)`) and
  only when the person holds a credit card. Adding a uniform to choose WHICH
  instrument would re-roll every amount, timestamp and burst schedule in the
  corpus for a decision that carries no randomness of its own. The
  construction mirrors `market/commerce/affinity.hpp` — a splitmix hash giving
  a stable rank — with two differences: `sampleFavoriteSlot` consumes a uniform
  its caller already drew and this DERIVES its own, so it costs zero; and the
  SELECTING uniform is keyed on the ROW rather than on the merchant, for the
  reason set out at `kRowDomain`.

  Three properties carry over verbatim from `affinity.hpp` and each is
  load-bearing here too:

  1. THE RANK IS KEYED ON THE INSTRUMENT'S OWN `Key.number`, NEVER ON ITS
     SLOT. Instrument slots are populated in issuance order today, but a
     slot-keyed rank would make a person's per-card volume split jump the
     moment an account closed or a card was added, and nothing downstream
     would attribute the jump.
  2. DRAW-FREENESS, as above.
  3. PREFIX-IDENTITY. A pure function of world state, so a short run is a
     byte-identical prefix of a longer one — the standing
     `test_card_point_in_time` requirement. This is also why the row salt is
     the row's TIMESTAMP and never an emission ordinal.

  THE EXPONENT IS CLASS S UNCITED. A uniform pick over K instruments is an
  exponent of ZERO, which flattens per-card volume by 1/K and destroys the
  per-card activity distribution; a Zipf rank law is the right SHAPE, and the
  active-versus-held gap says the distribution is very skewed. No public
  source gives the within-cardholder split of spend across cards, so the
  magnitude below is a declared choice and must be labelled one wherever it
  is quoted.
 */

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace PhantomLedger::activity::spending::actors {

/* "This instrument has no resolved Ledger slot." */
inline constexpr std::uint32_t kInvalidLedgerIndex =
    std::numeric_limits<std::uint32_t>::max();

/* STRUCTURAL: the support of `synth::accounts::counts`' per-person draw. */
inline constexpr std::uint8_t kMaxDepositInstruments = 3;

/* DECLARED CHOICE: the spend-active credit subset. See the header note. */
inline constexpr std::uint8_t kMaxCreditInstruments = 4;

inline constexpr std::uint8_t kMaxInstruments =
    kMaxDepositInstruments + kMaxCreditInstruments;

/* CLASS S UNCITED: the rank-frequency exponent of a cardholder's spend across
 * their OWN instruments. Not `commerce::kVisitZipfAlpha` — that one is CITED
 * (Krumme 2013) and describes merchants, not instruments. Do not reuse it
 * here, and do not let this value acquire that one's authority. */
inline constexpr double kInstrumentZipfAlpha = 0.80;

/* A fixed-capacity, parallel-array instrument set. Parallel rather than an
 * array of structs because `Spender` lives in a vector one entry per person
 * and an interleaved `{Key, uint32}` pays eight bytes of padding per slot. */
struct InstrumentSet {
  std::array<entity::Key, kMaxInstruments> keys{};
  std::array<std::uint32_t, kMaxInstruments> ledgerIndex{};

  /* Deposits occupy `[0, depositCount)`; credits occupy
   * `[kMaxDepositInstruments, kMaxDepositInstruments + creditCount)`. The two
   * regions are FIXED, not packed, so adding a card can never renumber a
   * deposit slot. */
  std::uint8_t depositCount = 0;
  std::uint8_t creditCount = 0;

  [[nodiscard]] std::span<const entity::Key> deposits() const noexcept {
    return {keys.data(), depositCount};
  }

  [[nodiscard]] std::span<const entity::Key> credits() const noexcept {
    return {keys.data() + kMaxDepositInstruments, creditCount};
  }

  [[nodiscard]] std::span<const std::uint32_t> depositIndices() const noexcept {
    return {ledgerIndex.data(), depositCount};
  }

  [[nodiscard]] std::span<const std::uint32_t> creditIndices() const noexcept {
    return {ledgerIndex.data() + kMaxDepositInstruments, creditCount};
  }

  /* THE PRIMARY DEPOSIT ACCOUNT IS ALWAYS SLOT 0. Every non-merchant emitter
   * (`emitBill`, `emitExternal`, `emitP2p`) sources from it unconditionally,
   * which is what keeps those three channels byte-identical to the pre-round
   * corpus. Only the merchant/card channel spreads across the set. */
  [[nodiscard]] entity::Key primaryDeposit() const noexcept { return keys[0]; }

  [[nodiscard]] std::uint32_t primaryDepositIndex() const noexcept {
    return ledgerIndex[0];
  }

  [[nodiscard]] bool hasCard() const noexcept { return creditCount > 0; }

  /* Append; returns false when the region is full so the caller decides
   * whether that is a no-op or an error, exactly as `Csr::pushBack` does. */
  [[nodiscard]] bool addDeposit(entity::Key key, std::uint32_t idx) noexcept {
    if (depositCount >= kMaxDepositInstruments) {
      return false;
    }
    keys[depositCount] = key;
    ledgerIndex[depositCount] = idx;
    ++depositCount;
    return true;
  }

  [[nodiscard]] bool addCredit(entity::Key key, std::uint32_t idx) noexcept {
    if (creditCount >= kMaxCreditInstruments) {
      return false;
    }
    const auto at = static_cast<std::size_t>(kMaxDepositInstruments) +
                    static_cast<std::size_t>(creditCount);
    keys[at] = key;
    ledgerIndex[at] = idx;
    ++creditCount;
    return true;
  }
};

namespace instruments {

/* THE SELECTING UNIFORM IS KEYED ON THE ROW, NOT ON THE MERCHANT, AND THAT IS
 * A CORRECTION — the first version of this keyed it on (person, merchant).
 *
 * That looked like the right habit model: one card, one merchant, always. What
 * it actually does is PARTITION the person's favourite set across their cards,
 * because the merchant fixes the instrument outright. Each card then sees only
 * its own block of merchants — measured 4.4 distinct merchants per card against
 * a favourite set of ~19 — and the within-card top-1 visit share, which is a
 * CITED quantity (Krumme et al. 2013: 13% North American issuer, 22% European),
 * went to 0.438. The concentration ratio collapsed to 1.92 against a 2.30
 * floor, i.e. to within noise of a uniform pick, because a Zipf law measured
 * over four merchants has almost no room left to express itself.
 *
 * `merchant-selection-2026-08` rule 1 is the governing precedent: an earlier
 * round pushed this same statistic to 31% chasing an unreachable national
 * volume share and had to withdraw it. Do not re-key this on the merchant to
 * recover a habit model — a card-per-merchant habit is UNCITED and the band it
 * costs is CITED.
 *
 * Keying on the row instead makes each instrument an independent thinning of
 * the person's merchant stream, so every card carries the person's full
 * merchant mix and the within-card law is the within-person law. Per-card
 * VOLUME still differs, because `weightFor` ranks the instruments themselves.
 */
inline constexpr std::uint64_t kRowDomain = 0x49'4E53'5400'0003ULL;

/* A second domain for the per-instrument RANK, for the same reason
 * `affinity.hpp` mixes both indices before combining them. */
inline constexpr std::uint64_t kRankDomain = 0x49'4E53'5400'0002ULL;

[[nodiscard]] constexpr std::uint64_t splitmix(std::uint64_t value) noexcept {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

/* A stable uniform in [0,1) for a (person, word, domain) triple. `word` is
 * 64-bit because one caller passes a row timestamp. */
[[nodiscard]] constexpr double unitFor(std::uint32_t personIndex,
                                       std::uint64_t word,
                                       std::uint64_t domain) noexcept {
  const auto mixed =
      splitmix(domain ^ splitmix(personIndex)) ^
      (splitmix(word + ::PhantomLedger::hashing::constants::fnv64_prime) *
       ::PhantomLedger::hashing::constants::golden64);
  return static_cast<double>(splitmix(mixed) >> 11U) * 0x1.0p-53;
}

/* This person's affinity weight for this instrument. Keyed on the account
 * NUMBER, never on the slot — see property 1 in the header note. */
[[nodiscard]] inline double weightFor(std::uint32_t personIndex,
                                      const entity::Key &key, std::size_t count,
                                      double alpha) noexcept {
  if (count <= 1) {
    return 1.0;
  }
  const auto u =
      unitFor(personIndex, static_cast<std::uint32_t>(key.number), kRankDomain);
  const double rank = 1.0 + std::floor(static_cast<double>(count) * u);
  return std::pow(rank, -alpha);
}

} // namespace instruments

/* THE DRAW-FREE INSTRUMENT PICK. Returns a slot in `[0, region.size())`,
 * relative to the region handed in — pass `deposits()` to choose a deposit
 * instrument and `credits()` to choose a credit one, and index the matching
 * `depositIndices()` / `creditIndices()` with the result.
 *
 * SPENDS NO UNIFORM. The selecting uniform is DERIVED from (person, rowSalt),
 * which is what keeps `selectPaymentRoute`'s per-row draw count byte-identical
 * to the pre-round tree.
 *
 * `rowSalt` must be world-derived and row-stable — the row's own timestamp is
 * what the caller passes, for the same reason `Router::publicDeviceFor` takes
 * one. It must NOT be an emission ordinal, or a short run stops being a
 * byte-identical prefix of a long one and the batch and windowed engines drift.
 *
 * Returns 0 for an empty or single-entry region; the caller guards emptiness
 * (a person with no credit instrument never reaches the card branch). */
[[nodiscard]] inline std::size_t
pickInstrumentSlot(std::span<const entity::Key> region,
                   std::uint32_t personIndex, std::uint64_t rowSalt,
                   double alpha = kInstrumentZipfAlpha) noexcept {
  const auto count = region.size();
  if (count <= 1) {
    return 0;
  }

  double total = 0.0;
  for (const auto &key : region) {
    total += instruments::weightFor(personIndex, key, count, alpha);
  }

  /* Unreachable from rank^-alpha with alpha > 0 and rank >= 1, but a
   * caller-supplied alpha makes it reachable; fall back to the head rather
   * than reading past the region. Same guard, same reason, as
   * `sampleFavoriteSlot`. */
  if (!(total > 0.0) || !std::isfinite(total)) {
    return 0;
  }

  double target =
      instruments::unitFor(personIndex, rowSalt, instruments::kRowDomain) *
      total;
  for (std::size_t slot = 0; slot + 1 < count; ++slot) {
    target -= instruments::weightFor(personIndex, region[slot], count, alpha);
    if (target < 0.0) {
      return slot;
    }
  }
  return count - 1;
}

} // namespace PhantomLedger::activity::spending::actors
