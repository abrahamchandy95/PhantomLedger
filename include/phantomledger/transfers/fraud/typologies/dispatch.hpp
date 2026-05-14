#pragma once

#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/behavior.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies {

class Dispatcher {
public:
  Dispatcher(IllicitContext &ctx, const Behavior &behavior) noexcept;

  [[nodiscard]] std::vector<transactions::Transaction>
  run(const Plan &plan, Typology typology, std::int32_t budget) const;

private:
  IllicitContext &ctx_;
  const Behavior &behavior_;
};

} // namespace PhantomLedger::transfers::fraud::typologies
