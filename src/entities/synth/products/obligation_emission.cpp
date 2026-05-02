#include "phantomledger/entities/synth/products/obligation_emission.hpp"

#include "phantomledger/entities/products/event.hpp"

namespace PhantomLedger::entities::synth::products {

void appendObligation(
    ::PhantomLedger::entity::product::ObligationStream &stream,
    ::PhantomLedger::entity::PersonId person,
    ::PhantomLedger::entity::product::Direction direction,
    ::PhantomLedger::entity::Key counterparty, double amount,
    ::PhantomLedger::time::TimePoint timestamp,
    ::PhantomLedger::channels::Tag channel,
    ::PhantomLedger::entity::product::ProductType productType,
    std::uint32_t productId) {
  ::PhantomLedger::entity::product::ObligationEvent ev{};
  ev.personId = person;
  ev.direction = direction;
  ev.counterpartyAcct = counterparty;
  ev.amount = amount;
  ev.timestamp = timestamp;
  ev.channel = channel;
  ev.productType = productType;
  ev.productId = productId;

  stream.append(ev);
}

} // namespace PhantomLedger::entities::synth::products
