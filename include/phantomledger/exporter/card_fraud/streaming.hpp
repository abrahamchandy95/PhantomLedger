#pragma once
//
// phantomledger/exporter/card_fraud/streaming.hpp
//
// Streaming half of the card-fraud exporter, shared by BOTH engines: a
// chunk sink that consumes settled rows, filters THE CARD VIEW
// (schema.hpp: channels cardPurchase + merchant), and
//
//   STREAMS (row-scale, never retained):
//     Payment_Transaction vertices    id T<row_seq>; 8 loaded attrs +
//                                     the 3 chronological split flags
//     Card_Send_Transaction edges     txn -> card, edge_unix_time
//     Merchant_Receive_Transaction    txn -> merchant, edge_unix_time
//
//   ACCUMULATES (bounded — the finisher's inputs):
//     cards       distinct source Keys seen in the view, with the
//                 credit/debit resolution and the ever-fraud flag
//                 (account/card scale; ordered map so the finisher
//                 writes vertices in deterministic Key order)
//     merchants   distinct destination Keys (merchant scale; ordered)
//     stream stats  first timestamp, total/view/fraud row counts
//
// row_seq alignment: the sink counts EVERY settled row (view or not),
// so T<row_seq> ids cross-reference the streamed 'transactions' table
// 1:1 — the same identity test_postgres pins for the corpus.
//
// use_chip / error / splits / category fallback are the content-keyed
// derivations in derive.hpp (doc-anchored card-fraud-2026-07 block);
// catalog destinations resolve their real category through the
// merchant catalog index built at construction.
//
// All three streamed tables go through common::Table: when
// Config::pgMirror is armed the rendered bytes stream into PostgreSQL
// directly — the only production destination (no files) — each on its
// own connection, open across the whole fold.
//

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/exporter/card_fraud/derive.hpp"
#include "phantomledger/exporter/card_fraud/schema.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/merchants/names.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

namespace PhantomLedger::exporter::card_fraud {

struct CardSeen {
  bool credit = false;
  bool fraud = false;
};

// Everything the fold accumulates for the finisher. Per the
// stage-product rule a NEW fold output belongs IN here, never as a new
// finisher parameter.
struct StreamedArtifacts {
  std::map<entity::Key, CardSeen> cards;
  std::set<entity::Key> merchants;

  std::int64_t firstTs = common::kFallbackEpoch;
  std::uint64_t rows = 0;      // every settled row (row_seq domain)
  std::uint64_t viewRows = 0;  // rows in the card view
  std::uint64_t fraudViewRows = 0;
};

class StreamingCardFraudExport {
public:
  struct Config {
    // Credit-card resolution: source Key in byKey => credit card.
    const entity::card::Registry *cards = nullptr;

    // Category resolution for catalog destinations; non-catalog
    // destinations use derive::fallbackCategory.
    const entity::merchant::Catalog *merchants = nullptr;

    // Split boundaries derive from the simulation window.
    ::PhantomLedger::time::Window window{};

    // When set, the streamed tables are written directly into
    // PostgreSQL as the bytes the csv::Writer renders — the only
    // production destination.
    const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;

    // Test infrastructure: rendered bytes per table stem.
    common::TableCapture *capture = nullptr;
  };

  explicit StreamingCardFraudExport(Config config)
      : config_(std::move(config)),
        bounds_(derive::splitBounds(config_.window)) {
    if (config_.merchants != nullptr) {
      categoryByKey_.reserve(config_.merchants->records.size());
      for (const auto &record : config_.merchants->records) {
        categoryByKey_.emplace(record.counterpartyId, record.category);
      }
    }

    const common::TableTarget target{.pg = config_.pgMirror,
                                     .capture = config_.capture};
    namespace sch = ::PhantomLedger::exporter::schema::card_fraud;
    paymentW_.emplace(common::openTable(target, sch::kPaymentTransaction));
    cardSendW_.emplace(common::openTable(target, sch::kCardSend));
    merchantReceiveW_.emplace(
        common::openTable(target, sch::kMerchantReceive));
  }

  void beginSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void append(std::span<const transactions::Transaction> txnsBatch) {
    if (txnsBatch.empty()) {
      return;
    }
    if (artifacts_.rows == 0) {
      // Replay-sorted stream: the first row carries the minimum
      // timestamp, identical to deriveSimStart over the full corpus.
      artifacts_.firstTs = txnsBatch.front().timestamp;
    }

    static constexpr auto kCardTag =
        channels::tag(channels::Legit::cardPurchase);
    static constexpr auto kMerchantTag =
        channels::tag(channels::Legit::merchant);

    for (const auto &tx : txnsBatch) {
      ++artifacts_.rows;

      const auto channel = tx.session.channel;
      if (channel != kCardTag && channel != kMerchantTag) {
        continue;
      }
      ++artifacts_.viewRows;

      const bool fraud = tx.fraud.flag != 0;
      if (fraud) {
        ++artifacts_.fraudViewRows;
      }

      const bool credit =
          config_.cards != nullptr &&
          config_.cards->byKey.find(tx.source) != config_.cards->byKey.end();

      auto &card = artifacts_.cards[tx.source];
      card.credit = credit;
      card.fraud = card.fraud || fraud;
      artifacts_.merchants.insert(tx.target);

      const auto id = derive::txnId(artifacts_.rows);
      const auto cardNumber = derive::cardId(tx.source, credit);
      const auto merchant = derive::merchantId(tx.target);
      const auto unixTime = static_cast<std::uint64_t>(tx.timestamp);

      const auto catIt = categoryByKey_.find(tx.target);
      const auto category = catIt != categoryByKey_.end()
                                ? catIt->second
                                : derive::fallbackCategory(tx.target);

      const auto split = derive::splitFor(tx.timestamp, bounds_);

      paymentW_->writer().writeRow(
          id,
          time::formatTimestamp(time::fromEpochSeconds(tx.timestamp)),
          tx.amount, static_cast<std::int32_t>(fraud ? 1 : 0), unixTime,
          merchants::name(category), derive::useChipFor(tx),
          derive::errorFor(tx), split.train, split.val, split.test);

      cardSendW_->writer().writeRow(id, cardNumber, unixTime);
      merchantReceiveW_->writer().writeRow(id, merchant, unixTime);
    }
  }

  void endSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void finish() {
    paymentW_.reset();
    cardSendW_.reset();
    merchantReceiveW_.reset();
  }

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept {
    return artifacts_.rows;
  }

  // Call after finish(); the writers are done by then.
  [[nodiscard]] StreamedArtifacts takeArtifacts() noexcept {
    return std::move(artifacts_);
  }

private:
  Config config_;

  derive::SplitBounds bounds_;
  std::unordered_map<entity::Key, merchants::Category> categoryByKey_;

  StreamedArtifacts artifacts_;

  std::optional<common::Table> paymentW_;
  std::optional<common::Table> cardSendW_;
  std::optional<common::Table> merchantReceiveW_;
};

} // namespace PhantomLedger::exporter::card_fraud
