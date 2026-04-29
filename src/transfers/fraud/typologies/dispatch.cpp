#include "phantomledger/transfers/fraud/typologies/dispatch.hpp"

#include "phantomledger/transfers/fraud/typologies/classic.hpp"
#include "phantomledger/transfers/fraud/typologies/funnel.hpp"
#include "phantomledger/transfers/fraud/typologies/invoice.hpp"
#include "phantomledger/transfers/fraud/typologies/layering.hpp"
#include "phantomledger/transfers/fraud/typologies/mule.hpp"
#include "phantomledger/transfers/fraud/typologies/structuring.hpp"

namespace PhantomLedger::transfers::fraud::typologies {

std::vector<transactions::Transaction> generate(IllicitContext &ctx,
                                                const Plan &plan,
                                                Typology typology,
                                                std::int32_t budget) {
  switch (typology) {
  case Typology::classic:
    return classic::generate(ctx, plan, budget);
  case Typology::layering:
    return layering::generate(ctx, plan, budget);
  case Typology::funnel:
    return funnel::generate(ctx, plan, budget);
  case Typology::structuring:
    return structuring::generate(ctx, plan, budget);
  case Typology::invoice:
    return invoice::generate(ctx, plan, budget);
  case Typology::mule:
    return mule::generate(ctx, plan, budget);
  }
  // Unreachable in practice — every Typology value is handled above —
  // but the silent fallback keeps the function total in the face of
  // future enum extensions during development.
  return classic::generate(ctx, plan, budget);
}

} // namespace PhantomLedger::transfers::fraud::typologies
