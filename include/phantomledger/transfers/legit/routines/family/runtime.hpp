#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/relationships/family/network.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/factory.hpp"

#include <span>

namespace PhantomLedger::transfers::legit::routines::family {

struct CounterpartyRouting {
  double externalP = 0.18;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("externalP", externalP, 0.0, 1.0); });
  }
};

inline constexpr CounterpartyRouting kDefaultCounterpartyRouting{};

struct Runtime {

  const ::PhantomLedger::relationships::family::Graph *graph = nullptr;

  std::span<const ::PhantomLedger::personas::Type> personas;

  std::span<const double> amountMultipliers;

  const entity::account::Registry *accounts = nullptr;

  const entity::account::Ownership *ownership = nullptr;

  const entity::merchant::Catalog *merchants = nullptr;

  ::PhantomLedger::time::Window window{};

  /// Pre-computed month anchors covering `window`.
  std::span<const ::PhantomLedger::time::TimePoint> monthStarts;

  const random::RngFactory *rngFactory = nullptr;

  const transactions::Factory *txf = nullptr;

  CounterpartyRouting routing{};
};

} // namespace PhantomLedger::transfers::legit::routines::family
