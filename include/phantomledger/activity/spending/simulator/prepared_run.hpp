#pragma once

#include "phantomledger/activity/spending/replay_source.hpp"
#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/activity/spending/routing/emission_result.hpp"
#include "phantomledger/activity/spending/simulator/payday_index.hpp"
#include "phantomledger/activity/spending/spenders/prepared.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace PhantomLedger::activity::spending::simulator {

class PreparedRun {
public:
  struct Population {
    std::vector<spenders::PreparedSpender> spenders;
    std::span<const double> sensitivities;

    [[nodiscard]] std::uint32_t activeCount() const noexcept {
      return static_cast<std::uint32_t>(spenders.size());
    }
  };

  struct Budget {
    double targetTotalTxns = 0.0;
    std::uint64_t totalPersonDays = 0;
    std::optional<std::uint32_t> personLimit;
  };

  // The base-stream replay feed (RAM R2.4b-1, replay_source.hpp). The
  // span adapter over the resident vector is the default; `source`
  // overrides it when the caller supplies a non-resident feed (the
  // R2.4b-2 disk spool). resolved() picks at use so the struct stays
  // copy/move-safe (no self-referential pointer at rest).
  struct LedgerReplay {
    SpanReplaySource span{};
    const BaseReplaySource *source = nullptr;

    [[nodiscard]] const BaseReplaySource &resolved() const noexcept {
      return source != nullptr ? *source : span;
    }
  };

  struct Paydays {
    PaydayIndex index;
  };

  struct Routing {
    routing::ChannelCdf channelCdf{};
    routing::PaymentRoutingRules paymentRules{};

    std::vector<clearing::Ledger::Index> personPrimaryIdx;
    std::vector<clearing::Ledger::Index> merchantCounterpartyIdx;
    clearing::Ledger::Index externalUnknownIdx = clearing::Ledger::invalid;

    [[nodiscard]] routing::ResolvedAccounts resolvedAccounts() const noexcept {
      return routing::ResolvedAccounts{
          .personPrimaryIdx =
              std::span<const clearing::Ledger::Index>(personPrimaryIdx),
          .merchantCounterpartyIdx =
              std::span<const clearing::Ledger::Index>(merchantCounterpartyIdx),
          .externalUnknownIdx = externalUnknownIdx,
      };
    }
  };

  [[nodiscard]] Population &population() noexcept { return population_; }
  [[nodiscard]] const Population &population() const noexcept {
    return population_;
  }

  [[nodiscard]] Budget &budget() noexcept { return budget_; }
  [[nodiscard]] const Budget &budget() const noexcept { return budget_; }

  [[nodiscard]] LedgerReplay &ledgerReplay() noexcept { return ledgerReplay_; }
  [[nodiscard]] const LedgerReplay &ledgerReplay() const noexcept {
    return ledgerReplay_;
  }

  [[nodiscard]] Paydays &paydays() noexcept { return paydays_; }
  [[nodiscard]] const Paydays &paydays() const noexcept { return paydays_; }

  [[nodiscard]] Routing &routing() noexcept { return routing_; }
  [[nodiscard]] const Routing &routing() const noexcept { return routing_; }

private:
  Population population_{};
  Budget budget_{};
  LedgerReplay ledgerReplay_{};
  Paydays paydays_{};
  Routing routing_{};
};

} // namespace PhantomLedger::activity::spending::simulator
