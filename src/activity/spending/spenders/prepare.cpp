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
  s.depositAccount = pop.primary(person);
  s.personaType = pop.kind(person);
  s.persona = &persona;

  s.rateMultiplier = persona.cash.rateMultiplier;
  s.amountMultiplier = persona.cash.amountMultiplier;
  s.cardShare = persona.card.share;
  s.timing = persona.archetype.timing;

  if (market.cards().hasCard(person)) {
    s.hasCard = true;
    s.card = market.cards().card(person);
  }

  if (ledger != nullptr) {
    s.depositAccountIdx = ledger->findAccount(s.depositAccount);
    if (s.hasCard) {
      s.cardIdx = ledger->findAccount(s.card);
    }
  }

  // Per-person merchant pool counts come from CSR row sizes.
  const auto favRow = commerce.favorites().rowOf(personIndex);
  const auto billRow = commerce.billers().rowOf(personIndex);
  s.favCount = static_cast<std::uint16_t>(favRow.size());
  s.billCount = static_cast<std::uint16_t>(billRow.size());

  s.exploreProp = commerce.exploreProp(personIndex);
  s.burstStart = commerce.burstStartDay(personIndex);
  s.burstLen = commerce.burstLen(personIndex);

  // geo-causal-v1 (G2a): the customer's home area for card-present
  // distance-decay selection. invalidGeoArea when no carrier is bound
  // (the monolith reference oracle) — step-2 selection then treats it as
  // "no local anchor". UNREAD until step-2, so this moves no golden.
  s.homeArea = pop.homeArea(person);

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
