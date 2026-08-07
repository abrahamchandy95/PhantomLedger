#include "phantomledger/activity/spending/spenders/prepare.hpp"

#include "phantomledger/activity/spending/market/commerce/view.hpp"
#include "phantomledger/activity/spending/market/population/view.hpp"
#include "phantomledger/synth/econ/nominal.hpp"

#include <algorithm>

namespace PhantomLedger::activity::spending::spenders {

namespace {

constexpr double kBaselineCashFloor = 150.0;

actors::Spender buildSpender(const market::Market &market,
                             const clearing::Ledger *ledger,
                             entity::PersonId person,
                             std::uint32_t personIndex) {
  const auto &pop = market.population();
  const auto &commerce = market.commerce();
  const auto &persona = pop.object(person);

  actors::Spender s{};
  s.person = person;
  s.personIndex = personIndex;
  s.personaType = pop.kind(person);
  s.persona = &persona;

  s.rateMultiplier = persona.cash.rateMultiplier;
  s.amountMultiplier = persona.cash.amountMultiplier;
  s.cardShare = persona.card.share;
  s.timing = persona.archetype.timing;

  /* THE CARD-VIEW INSTRUMENT SET.
   *
   * `View::deposits` reports the PRIMARY first and falls back to a one-entry
   * view of it when no ownership carrier is bound, so slot 0 is the primary
   * on every path — which is what `emitBill`, `emitExternal` and `emitP2p`
   * rely on to stay byte-identical to the pre-round corpus.
   *
   * The credit side takes the person's HELD cards in issuance order and stops
   * at `kMaxCreditInstruments`. Holdings follow the cited Fed count law;
   * this cap is the smaller, declared, SPEND-ACTIVE subset, and it is what
   * bounds the CardCycleDriver's statement and payment blast radius — the
   * driver only ever opens a session for a card that receives a purchase.
   *
   * The ledger index is resolved here, once per person for the whole fold,
   * exactly as the two scalar carriers were. An unresolvable account keeps
   * `kInvalidLedgerIndex` and the liquidity readers fall back, same as
   * before. */
  const auto deposits = pop.deposits(person);
  for (const auto &key : deposits) {
    if (!entity::valid(key)) {
      continue;
    }
    const auto idx = ledger != nullptr ? ledger->findAccount(key)
                                       : actors::kInvalidLedgerIndex;
    if (!s.instruments.addDeposit(key, idx)) {
      break;
    }
  }

  for (const auto &key : market.cards().cards(person)) {
    if (!entity::valid(key)) {
      continue;
    }
    const auto idx = ledger != nullptr ? ledger->findAccount(key)
                                       : actors::kInvalidLedgerIndex;
    if (!s.instruments.addCredit(key, idx)) {
      break;
    }
  }

  // Per-person merchant pool counts come from CSR row sizes.
  const auto favRow = commerce.favorites().rowOf(personIndex);
  const auto billRow = commerce.billers().rowOf(personIndex);
  s.favCount = static_cast<std::uint16_t>(favRow.size());
  s.billCount = static_cast<std::uint16_t>(billRow.size());

  s.exploreProp = commerce.exploreProp(personIndex);
  s.bursts = commerce.bursts(personIndex);

  // relocation-2026-07: the home area is NO LONGER CACHED here. This function
  // runs once for the whole fold, so a cached home would freeze at window
  // start while the relocation schedule moved on. `payments.cpp` reads
  // `market.population().homeArea(person)` at selection time instead, which
  // the monthly evolver re-points.

  // H2 step 2c: the retirement day-index (kNoRetirementDay ==
  // Spender::kNoRetireDay — both are the uint32 max sentinel). H3: the
  // death day-index likewise. Both engines carry them through the
  // blueprint's timeline lane.
  s.retireDay = pop.retirementDay(person);
  s.deathDay = pop.deathDay(person);

  return s;
}

} // namespace

std::vector<PreparedSpender>
prepareSpenders(const market::Market &market,
                const obligations::Snapshot &obligations,
                const clearing::Ledger *ledger) {
  const auto &pop = market.population();
  const auto count = pop.count();

  // H1 step 2b (class P stock): the persona's calibration-year cash
  // reference (and its $150 floor) anchors ONCE at the window-start
  // price level, matching the opening-book seeding, so the liquidity
  // cash-ratio compares era dollars to era dollars (authority U-6).
  const double stockScale = synth::econ::priceScale(
      time::toCalendarDate(market.bounds().startDate).year);

  std::vector<PreparedSpender> out;
  out.reserve(count);

  for (std::uint32_t i = 0; i < count; ++i) {
    const auto person = static_cast<entity::PersonId>(i + 1);

    if (!entity::valid(pop.primary(person))) {
      continue;
    }

    PreparedSpender ps{};
    ps.spender = buildSpender(market, ledger, person, i);
    ps.paydays = std::span<const std::uint32_t>(
        pop.paydays().personView(i).first, pop.paydays().personView(i).size());

    const double initialCash =
        ps.spender.persona->cash.initialBalance * stockScale;
    ps.initialCash = initialCash;
    ps.baselineCash = std::max(kBaselineCashFloor * stockScale, initialCash);
    ps.fixedBurden =
        i < obligations.burden.size() ? obligations.burden.monthlyAt(i) : 0.0;
    ps.paycheckSensitivity = ps.spender.persona->payday.sensitivity;

    out.push_back(ps);
  }

  return out;
}

} // namespace PhantomLedger::activity::spending::spenders
