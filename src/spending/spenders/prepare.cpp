#include "phantomledger/spending/spenders/prepare.hpp"

#include "phantomledger/spending/market/commerce/view.hpp"
#include "phantomledger/spending/market/population/view.hpp"

#include <algorithm>

namespace PhantomLedger::spending::spenders {

namespace {

constexpr double kBaselineCashFloor = 150.0;

actors::Spender buildSpender(const market::Market &market,
                             entity::PersonId person,
                             std::uint32_t personIndex) {
  const auto &pop = market.population();
  const auto &commerce = market.commerce();

  actors::Spender s{};
  s.person = person;
  s.personIndex = personIndex;
  s.depositAccount = pop.primary(person);
  s.personaType = pop.kind(person);
  s.persona = &pop.object(person);

  if (market.cards().hasCard(person)) {
    s.hasCard = true;
    s.card = market.cards().card(person);
  }

  // Per-person merchant pool counts come from CSR row sizes.
  const auto favRow = commerce.favorites().rowOf(personIndex);
  const auto billRow = commerce.billers().rowOf(personIndex);
  s.favCount = static_cast<std::uint16_t>(favRow.size());
  s.billCount = static_cast<std::uint16_t>(billRow.size());

  s.exploreProp = commerce.exploreProp(personIndex);
  s.burstStart = commerce.burstStartDay(personIndex);
  s.burstLen = commerce.burstLen(personIndex);

  return s;
}

} // namespace

std::vector<PreparedSpender>
prepareSpenders(const market::Market &market,
                const obligations::Snapshot &obligations) {
  const auto &pop = market.population();
  const auto count = pop.count();

  std::vector<PreparedSpender> out;
  out.reserve(count);

  for (std::uint32_t i = 0; i < count; ++i) {
    const auto person = static_cast<entity::PersonId>(i + 1);

    // Skip persons without a valid primary account — matches Python's
    // `primary_accounts.get(...)` returning None.
    if (!entity::valid(pop.primary(person))) {
      continue;
    }

    PreparedSpender ps{};
    ps.spender = buildSpender(market, person, i);
    ps.paydays = std::span<const std::uint32_t>(
        pop.paydays().personView(i).first, pop.paydays().personView(i).size());

    // `entity::behavior::Persona` is nested: cash fields live under
    // `cash`, payday fields under `payday`. The previous flat-field
    // accessors don't exist on the real type.
    const double initialCash = ps.spender.persona->cash.initialBalance;
    ps.initialCash = initialCash;
    ps.baselineCash = std::max(kBaselineCashFloor, initialCash);
    ps.fixedBurden =
        i < obligations.burden.size() ? obligations.burden.monthlyAt(i) : 0.0;
    ps.paycheckSensitivity = ps.spender.persona->payday.sensitivity;

    out.push_back(ps);
  }

  return out;
}

} // namespace PhantomLedger::spending::spenders
