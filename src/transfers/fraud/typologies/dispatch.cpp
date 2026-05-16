#include "phantomledger/transfers/fraud/typologies/dispatch.hpp"

#include "phantomledger/transfers/fraud/typologies/bipartite.hpp"
#include "phantomledger/transfers/fraud/typologies/classic.hpp"
#include "phantomledger/transfers/fraud/typologies/cycle.hpp"
#include "phantomledger/transfers/fraud/typologies/funnel.hpp"
#include "phantomledger/transfers/fraud/typologies/invoice.hpp"
#include "phantomledger/transfers/fraud/typologies/layering.hpp"
#include "phantomledger/transfers/fraud/typologies/mule.hpp"
#include "phantomledger/transfers/fraud/typologies/scatter_gather.hpp"
#include "phantomledger/transfers/fraud/typologies/structuring.hpp"

namespace PhantomLedger::transfers::fraud::typologies {

Dispatcher::Dispatcher(IllicitContext &ctx, const Behavior &behavior) noexcept
    : ctx_(ctx), behavior_(behavior) {}

std::vector<transactions::Transaction>
Dispatcher::run(const Plan &plan, Typology typology,
                std::int32_t budget) const {
  switch (typology) {
  case Typology::classic:
    return classic::generate(ctx_, plan, budget);
  case Typology::layering:
    return layering::generate(ctx_, plan, budget, behavior_.layering);
  case Typology::funnel:
    return funnel::generate(ctx_, plan, budget);
  case Typology::structuring:
    return structuring::generate(ctx_, plan, budget, behavior_.structuring);
  case Typology::invoice:
    return invoice::generate(ctx_, plan, budget);
  case Typology::mule:
    return mule::generate(ctx_, plan, budget);
  case Typology::cycle:
    return cycle::generate(ctx_, plan, budget);
  case Typology::scatterGather:
    return scatterGather::generate(ctx_, plan, budget);
  case Typology::bipartite:
    return bipartite::generate(ctx_, plan, budget);
  }

  return classic::generate(ctx_, plan, budget);
}

} // namespace PhantomLedger::transfers::fraud::typologies
