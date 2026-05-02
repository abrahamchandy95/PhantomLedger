#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/obligation_stream.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::products {

void appendObligation(
    ::PhantomLedger::entity::product::ObligationStream &stream,
    ::PhantomLedger::entity::PersonId person,
    ::PhantomLedger::entity::product::Direction direction,
    ::PhantomLedger::entity::Key counterparty, double amount,
    ::PhantomLedger::time::TimePoint timestamp,
    ::PhantomLedger::channels::Tag channel,
    ::PhantomLedger::entity::product::ProductType productType,
    std::uint32_t productId = 0);

} // namespace PhantomLedger::entities::synth::products
