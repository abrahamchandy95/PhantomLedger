#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/relationships/family/network.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/family/transfer_config.hpp"

#include <span>

namespace PhantomLedger::transfers::legit::routines::family {

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

  ::PhantomLedger::transfers::family::Routing routing{};
};

} // namespace PhantomLedger::transfers::legit::routines::family
