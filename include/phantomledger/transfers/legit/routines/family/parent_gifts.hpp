#pragma once

#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/family/transfer_config.hpp"
#include "phantomledger/transfers/legit/routines/family/runtime.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::parent_gifts {

[[nodiscard]] std::vector<transactions::Transaction>
generate(const Runtime &rt,
         const ::PhantomLedger::transfers::family::ParentGifts &cfg);

} // namespace PhantomLedger::transfers::legit::routines::family::parent_gifts
