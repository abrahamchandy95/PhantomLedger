#pragma once
/*
  Streaming half of the card-fraud exporter, shared by BOTH engines: a
  chunk sink that consumes settled rows, filters THE CARD VIEW (schema.hpp:
  channels cardPurchase + merchant), and

    STREAMS (row-scale, never retained) — one vertex, four relationships:
      Payment_Transaction vertices    id T<row_seq>; the 8 loaded attrs
      Card_Send_Transaction edges     txn -> card, edge_unix_time
      Merchant_Receive_Transaction    txn -> merchant, edge_unix_time
      Transaction_Uses_Device edges   txn -> event-time device
      Transaction_Uses_IP edges       txn -> event-time IP

    ACCUMULATES (bounded — the finisher's inputs):
      cards       distinct source Keys seen in the view, with the
                  credit/debit resolution and the ever-fraud flag
                  (account/card scale; ordered map so the finisher writes
                  vertices in deterministic Key order)
      merchants   distinct destination Keys (merchant scale; ordered)
      devices/IPs distinct session endpoints observed in the card view
      stream stats  first timestamp, total/view/fraud row counts

  ROW_SEQ ALIGNMENT: the sink counts EVERY settled row, view or not, so
  T<row_seq> ids cross-reference the streamed 'transactions' table 1:1 —
  the same identity test_postgres pins for the corpus.

  use_chip is CAUSAL: the catalog index built at construction resolves each
  destination's category AND footprint, and derive::useChipFor reads the
  footprint (the acceptance environment) plus the row's own year — the same
  axis generation partitioned on. `error` and the category fallback stay
  content-keyed in derive.hpp.

  All five streamed tables go through common::Table: when Config::pgMirror
  is armed the rendered bytes stream straight into PostgreSQL — the only
  production destination, no files — each on its own connection, open
  across the whole fold.
 */

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/holdings/card_reissue.hpp"
#include "phantomledger/entities/holdings/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/enumeration.hpp"
#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/exporter/card_fraud/derive.hpp"
#include "phantomledger/exporter/card_fraud/schema.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/merchants/names.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::card_fraud {

struct CardSeen {
  bool credit = false;
  bool fraud = false;

  /* Which card GENERATIONS this account was observed transacting on. The
   * finisher emits one `cf_Card` row per observed generation, so a card
   * replaced mid-window contributes two vertices — which is the point.
   *
   * OBSERVED, NEVER SCHEDULED: the schedule may say a card turned over five
   * times, but a generation with no transaction in the view must not become
   * a vertex, for the same reason `cf_Merchant` is restricted to
   * view-observed merchants — an unreferenced vertex is a dangling one. */
  std::set<std::uint32_t> generations;
};

/* A DECLINED ROW'S LABEL IS WITHHELD, exactly as `cf_Card.is_fraud`,
 * `cf_Party.is_fraud`, `cf_Device.is_blocked` and `cf_IP.is_blocked` are.
 *
 * Declined authorizations are not GNN transaction nodes — they are
 * authorization traffic, separated by a non-empty `error` — so their label
 * has no consumer that should train on it. Exporting it anyway would be a
 * liability rather than information, and for two reasons beyond tidiness.
 *
 * The funding declines are 37.9-45.7% fraud by construction (the unauthorized
 * rail drains a victim, so its own later charges cannot fund), which makes the
 * column the single strongest correlate anywhere in the export. And even with
 * the rows themselves excluded as nodes, any velocity feature computed OVER
 * them — "declines on this card in the last 24h" is the canonical one — would
 * carry that correlation straight back into the graph through the aggregate.
 * Withholding the label removes the whole class of path rather than managing
 * its magnitude.
 *
 * The truth is not lost: it stays on the SETTLED rows, which is where the
 * per-row target lives, and entity-level verdicts stay quarantined in
 * `cf_Ground_Truth_Label`. */
inline constexpr std::int32_t kDeclinedLabelWithheld = 0;

struct StreamedArtifacts {
  std::map<entity::Key, CardSeen> cards;
  std::set<entity::Key> merchants;
  std::set<devices::Identity> devices;
  std::set<network::Ipv4> ips;

  std::int64_t firstTs = common::kFallbackEpoch;
  std::uint64_t rows = 0;     // every settled row (row_seq domain)
  std::uint64_t viewRows = 0; // rows in the card view
  std::uint64_t fraudViewRows = 0;
  /* Declined authorizations written alongside the settled rows. NOT counted
   * in `rows` or `viewRows`: those two are the SETTLED population, and every
   * consumer of them — the acceptance script's payment/edge parity, the
   * manifest, the gates — means settled. A declined row is identified by a
   * non-empty `error`. */
  std::uint64_t declinedRows = 0;

  /* Card-testing probes, counted apart from `declinedRows` because they are a
   * different population with a different cause: those are this corpus's own
   * cardholders being refused, these are an operator sweeping a stolen list.
   * A consumer separating authorization traffic from purchases wants their
   * sum; anyone asking why fan-out is high wants this one alone. */
  std::uint64_t enumerationRows = 0;
};

class StreamingCardFraudExport {
public:
  struct Config {
    /* Membership resolution for the visible card graph. The shared raw
     * ledger retains generation rows, but a Party and its card may enter
     * this feature graph only during the [join, close) interval. */
    const entity::account::Registry *registry = nullptr;
    const entity::account::Lookup *lookup = nullptr;
    ::PhantomLedger::synth::pii::Membership membership;

    /* Credit-card resolution: source Key in byKey => credit card. */
    const entity::card::Registry *cards = nullptr;

    /* Category and footprint resolution for catalog destinations;
     * non-catalog destinations use derive::fallbackCategory and derive as
     * remote (Online) acceptance endpoints. */
    const entity::merchant::Catalog *merchants = nullptr;

    /* When set, the streamed tables are written directly into PostgreSQL as
     * the bytes the csv::Writer renders — the only production
     * destination. */
    const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;

    /* Test infrastructure: rendered bytes per table stem. */
    common::TableCapture *capture = nullptr;

    /* The run window, which anchors the card reissue schedule. Generation 0
     * begins at `window.start`.
     *
     * KEEP IT WINDOW-ANCHORED, NOT EPOCH-ANCHORED. A fixed epoch would open
     * a 2000-start run at generation ~7 and rewrite EVERY card id in every
     * table. The point-in-time property is unaffected because a prefix
     * export and a full export of the same run share the same window start,
     * which is exactly what `test_card_point_in_time` compares. */
    ::PhantomLedger::time::Window window{};

    /* THE FUNDING DECLINES, BY POINTER RATHER THAN BY VALUE, and the
     * indirection is what makes both engines feed them.
     *
     * A drive site constructs this sink BEFORE the run, while the attempts
     * are only decided during it; and `finish()` closes the writers at
     * end-of-run through the chunk-sink protocol. So there is no moment at
     * which a caller could hand the attempts over with the tables still open.
     * A pointer captured at construction is valid the whole time and is
     * populated by the time `finish()` reads it — the monolithic path fills
     * it from `PostedLedgerReplay::declined`, the windowed one from
     * `WindowedConfig::declinedOut`.
     *
     * BOTH SITES MUST FILL IT. Shipping it on one only is what 6de9c95 backed
     * out: the batch path wrote declines, the binary read an empty vector and
     * wrote none, and nothing failed. `test_production_windowed` now compares
     * the two populations for exactly that reason.
     *
     * Null means the caller has no declined stream (a direct unit
     * construction), and the pass is simply skipped. */
    const std::vector<transfers::legit::ledger::DeclinedAttempt> *declined =
        nullptr;

    /* Attacker infrastructure, for the card-testing probes. Null disables
     * them entirely, which is what a direct unit construction wants and what
     * the enumeration gate's disarm leg uses. */
    const ::PhantomLedger::infra::AttackerInfra *attackers = nullptr;
  };

  explicit StreamingCardFraudExport(Config config)
      : config_(std::move(config)) {
    if (config_.merchants != nullptr) {
      catalogByKey_.reserve(config_.merchants->records.size());
      for (const auto &record : config_.merchants->records) {
        catalogByKey_.emplace(record.counterpartyId,
                              CatalogEntry{record.category, record.footprint});
      }
    }

    const common::TableTarget target{.pg = config_.pgMirror,
                                     .capture = config_.capture};
    namespace sch = ::PhantomLedger::exporter::schema::card_fraud;
    paymentW_.emplace(common::openTable(target, sch::kPaymentTransaction));
    cardSendW_.emplace(common::openTable(target, sch::kCardSend));
    merchantReceiveW_.emplace(common::openTable(target, sch::kMerchantReceive));
    transactionDeviceW_.emplace(
        common::openTable(target, sch::kTransactionUsesDevice));
    transactionIpW_.emplace(common::openTable(target, sch::kTransactionUsesIp));
  }

  void beginSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void append(std::span<const transactions::Transaction> txnsBatch) {
    if (txnsBatch.empty()) {
      return;
    }
    if (artifacts_.rows == 0) {
      /* Replay-sorted stream: the first row carries the minimum timestamp,
       * identical to deriveSimStart over the full corpus. */
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

      const auto sourceOwner = ownerOf(tx.source);
      const auto targetOwner = ownerOf(tx.target);
      if (!config_.membership.activeAt(sourceOwner, tx.timestamp) ||
          !config_.membership.activeAt(targetOwner, tx.timestamp)) {
        continue;
      }
      ++artifacts_.viewRows;

      /* THE SESSION IS PART OF THE CARD VIEW'S CONTRACT, NOT AN OPTION.
       * test_pipeline_e2e and docs/card_fraud_postgres_acceptance.sql both
       * require exactly ONE device edge and ONE IP edge per
       * Payment_Transaction row (the acceptance script FULL JOINs and
       * counts any unmatched side as a failure). The two edge writes below
       * are therefore UNCONDITIONAL, and can only stay that way if every
       * row reaching them carries a session.
       *
       * A row CAN reach here without one, and the asymmetry that lets it is
       * worth naming. `Membership::activeAt` returns TRUE for out-of-range
       * ids — "counterparties, hubs, invalid owners are always active"
       * (synth/pii/membership.hpp) — which is right for the TARGET side,
       * where merchants are legitimately ownerless. But the access router's
       * owner map SKIPS every account whose owner is invalidPerson
       * (pipeline/stages/infra.cpp), and ownerless accounts do exist
       * (synth/accounts/assign.hpp registers them). So a card-view row
       * sourced from an ownerless account passes the filter above while
       * never having been routable, and transactions::Factory left its
       * session empty.
       *
       * That is a GENERATION defect and it is REFUSED here, not absorbed.
       * Both alternatives are worse: dropping the row hides the defect and
       * makes the payment count silently depend on session availability,
       * and writing a blank endpoint emits a dangling edge that surfaces
       * only as a PostgreSQL acceptance exception hours later. Fail at the
       * point of truth, naming the row. No production row trips this — the
       * e2e 1:1 check would already be red if one did. The throw closes the
       * gap between "happens to hold" and "cannot be violated silently". */
      if (!tx.session.deviceId.assigned() || tx.session.ipAddress.value == 0) {
        throw std::runtime_error(
            "card_fraud: card-view row carries no session endpoint (source "
            "owner " +
            std::to_string(sourceOwner) + ", timestamp " +
            std::to_string(tx.timestamp) +
            "). Every Payment_Transaction row must carry one device and one "
            "IP. The source account is unroutable by infra::Router — most "
            "likely an account whose owner is invalidPerson, which the "
            "router's owner map skips.");
      }

      const bool fraud = tx.fraud.flag != 0;
      if (fraud) {
        ++artifacts_.fraudViewRows;
      }

      const bool credit =
          config_.cards != nullptr &&
          config_.cards->byKey.find(tx.source) != config_.cards->byKey.end();

      /* Every funding decline this row does not precede, emitted first. This
       * is the merge that keeps the table causally ordered and the score-time
       * export a byte prefix of the full one. */
      writeFundingDeclinesUpTo(tx.timestamp);

      /* Read BEFORE the map insert below creates the entry. A card's first row
       * in the view is what anchors its enumeration probe, and it is the same
       * row in a score-time export as in a full-window one. */
      const bool firstRowForCard = !artifacts_.cards.contains(tx.source);

      auto &card = artifacts_.cards[tx.source];
      card.credit = credit;
      card.fraud = card.fraud || fraud;
      artifacts_.merchants.insert(tx.target);
      artifacts_.devices.insert(tx.session.deviceId);
      artifacts_.ips.insert(tx.session.ipAddress);

      const auto id = derive::txnId(artifacts_.rows);
      /* The card number for THIS row is the generation live at THIS row's
       * timestamp, so a transaction before a reissue and one after it carry
       * different numbers on the same account. Schedules are cached per
       * account: `generationsFor` is draw-free and pure, so caching cannot
       * change the answer, only the cost. */
      const auto generation = generationFor(tx.source, tx.timestamp);
      card.generations.insert(generation);
      const auto cardNumber = derive::cardId(tx.source, credit, generation);
      const auto merchant = derive::merchantId(tx.target);
      const auto unixTime = static_cast<std::uint64_t>(tx.timestamp);

      const auto catIt = catalogByKey_.find(tx.target);
      const auto category = catIt != catalogByKey_.end()
                                ? catIt->second.category
                                : derive::fallbackCategory(tx.target);
      const auto footprint =
          catIt != catalogByKey_.end()
              ? std::optional<entity::merchant::Footprint>{catIt->second
                                                               .footprint}
              : std::nullopt;

      /* THE DECLINED ATTEMPT COMES FIRST, IF THERE IS ONE.
       *
       * A non-empty `error` marks a DECLINED AUTHORIZATION; the settled row
       * below always carries the empty string. That one bit is what lets a
       * consumer keep declines out of the graph — they are authorization
       * traffic, not settled purchases, and the GNN's transaction nodes are
       * the latter.
       *
       * It is ADDITIVE: the settled row is byte-unchanged, no balance moves,
       * and the ledger never sees it. That is the ordinary shape of a
       * mistyped CVV followed by a successful retry, and it is why this can
       * be generated at export time from a draw-free hash rather than
       * modelled in the replay.
       *
       * THE DECLINED ROW CARRIES THE SAME SESSION, CARD AND MERCHANT, which
       * is the point: a declined attempt happened on a real device, at a real
       * address, against a real card. Withholding its edges would make
       * "row with no device edge" mean "declined" and hand back a shortcut on
       * the way out.
       *
       * Its id is suffixed rather than drawn from the row counter, so adding
       * or removing declines cannot renumber a single settled row. */
      if (firstRowForCard) {
        writeEnumerationProbe(tx, id, cardNumber, generation);
      }

      if (derive::declinedBefore(tx)) {
        const auto declinedId = id + "D";
        /* One minute earlier: inside the same authorization episode, and
         * strictly ordered before the settlement it precedes. Fixed rather
         * than hashed because the ordering is the only thing that carries
         * meaning here. */
        const auto declinedTs = tx.timestamp - 60;
        const auto declinedUnix = static_cast<std::uint64_t>(declinedTs);
        paymentW_->writer().writeRow(
            declinedId,
            time::formatTimestamp(time::fromEpochSeconds(declinedTs)),
            tx.amount, kDeclinedLabelWithheld, declinedUnix,
            merchants::name(category), derive::useChipFor(tx, footprint),
            derive::nonFundingErrorFor(tx));
        cardSendW_->writer().writeRow(declinedId, cardNumber, declinedUnix);
        merchantReceiveW_->writer().writeRow(declinedId, merchant,
                                             declinedUnix);
        transactionDeviceW_->writer().writeRow(
            declinedId, common::renderDeviceId(tx.session.deviceId).view(),
            declinedUnix);
        transactionIpW_->writer().writeRow(
            declinedId, network::format(tx.session.ipAddress).view(),
            declinedUnix);
        ++artifacts_.declinedRows;
      }

      paymentW_->writer().writeRow(
          id, time::formatTimestamp(time::fromEpochSeconds(tx.timestamp)),
          tx.amount, static_cast<std::int32_t>(fraud ? 1 : 0), unixTime,
          merchants::name(category), derive::useChipFor(tx, footprint),
          derive::emptyIfSettled());

      cardSendW_->writer().writeRow(id, cardNumber, unixTime);
      merchantReceiveW_->writer().writeRow(id, merchant, unixTime);
      transactionDeviceW_->writer().writeRow(
          id, common::renderDeviceId(tx.session.deviceId).view(), unixTime);
      transactionIpW_->writer().writeRow(
          id, network::format(tx.session.ipAddress).view(), unixTime);
    }
  }

  void endSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

private:
  /* ONE CARD-TESTING PROBE, on the card's first appearance in the view.
   *
   * The row is a DECLINED authorization carrying the OPERATOR's device and
   * address rather than the cardholder's, which is the whole point: one
   * endpoint touches every card on that operator's list, so device-to-card
   * fan-out stops being a fraud tell. The label is withheld, so those edges
   * carry no supervision — a 500-card enumeration device is uninformative,
   * not diagnostic.
   *
   * THE MERCHANT IS THE OPERATOR'S, NOT THE ROW'S. Card testing runs against
   * one acquirer the operator has picked, and reusing the settled row's
   * merchant would instead smear probes across the catalogue and correlate
   * them with wherever the victim happened to shop first. Chosen by hashing
   * the operator index, so it is draw-free and stable, and REGISTERED below
   * so no edge dangles.
   *
   * `card.fraud` is deliberately untouched, for the same reason a funding
   * decline does not set it: that flag means fraud SETTLED on the card. */
  void writeEnumerationProbe(const transactions::Transaction &tx,
                             const std::string &id,
                             const std::string &cardNumber,
                             std::uint32_t generation) {
    if (config_.attackers == nullptr || paymentW_ == std::nullopt ||
        config_.merchants == nullptr ||
        config_.merchants->records.empty()) {
      return;
    }
    const auto probe = ::PhantomLedger::infra::enumeration::probeFor(
        *config_.attackers, tx.source, tx.timestamp);
    if (!probe.has_value()) {
      return;
    }

    const auto &records = config_.merchants->records;
    const auto slot = ::PhantomLedger::infra::derived::splitmix(
                          ::PhantomLedger::infra::enumeration::kOperatorDomain ^
                          probe->operatorIndex) %
                      records.size();
    const auto merchantKey = records[slot].counterpartyId;

    artifacts_.merchants.insert(merchantKey);
    artifacts_.devices.insert(probe->device);
    artifacts_.ips.insert(probe->ip);
    /* The probe's own generation is the one live at the settled row it is
     * anchored to; a 900-second lead cannot cross a reissue boundary. */
    (void)generation;

    const auto probeId = id + "E";
    const auto probeUnix = static_cast<std::uint64_t>(probe->timestamp);

    paymentW_->writer().writeRow(
        probeId,
        time::formatTimestamp(time::fromEpochSeconds(probe->timestamp)),
        /* A probe is a live-or-not question, not a purchase. The amount is
         * the small fixed ticket operators use so the authorization is cheap
         * and unremarkable; it is a DECLARED value, CLASS S UNCITED. */
        ::PhantomLedger::infra::enumeration::kProbeAmount,
        kDeclinedLabelWithheld, probeUnix,
        merchants::name(records[slot].category),
        /* Card-not-present: the operator is not holding the card. */
        0, derive::kEnumerationError);
    cardSendW_->writer().writeRow(probeId, cardNumber, probeUnix);
    merchantReceiveW_->writer().writeRow(
        probeId, derive::merchantId(merchantKey), probeUnix);
    transactionDeviceW_->writer().writeRow(
        probeId, common::renderDeviceId(probe->device).view(), probeUnix);
    transactionIpW_->writer().writeRow(
        probeId, network::format(probe->ip).view(), probeUnix);
    ++artifacts_.enumerationRows;
  }

  /* THE FUNDING DECLINES — the ones the replay actually decided, as opposed
   * to the non-funding ones synthesised beside each settled row.
   *
   * MERGED INTO THE SETTLED STREAM BY TIMESTAMP, not appended at the end, and
   * that ordering is a correctness requirement rather than a tidiness one.
   * `test_card_point_in_time` demands a score-time export be a byte PREFIX of
   * the full-window one. Rows written from `finish()` land after every settled
   * row, so a short export's declines sit where a long export still has
   * purchases — measured as all five streamed tables diverging at the first
   * declined line. Emitting each attempt just before the first settled row it
   * does not precede makes the prefix exact.
   *
   * NO TAIL FLUSH, deliberately. An attempt later than the last settled row is
   * dropped rather than emitted at close, because emitting it is precisely
   * what breaks the prefix: the short export would have to show a decline the
   * long export places after rows the short one never saw. The loss is the
   * handful of attempts in the gap between the final settled row and the
   * window end.
   *
   * Called from `append()`, so both drive sites feed them identically. An
   * asymmetry here would make the binary and the tests export different
   * tables from the same world, which is what `test_card_point_in_time`
   * forbids.
   *
   * SAME VIEW FILTER AS THE SETTLED STREAM, deliberately: an attempt on a
   * channel the card view does not carry, or by a party outside its
   * membership interval, is not a card-view authorization just because it
   * failed.
   *
   * THEIR ENTITIES MUST BE REGISTERED, and this is the part that is easy to
   * get wrong. A settled row's card, merchant, device and address are already
   * vertices; a declined attempt's need not be, because the attempt may be
   * the only time that combination appears. Skipping registration would emit
   * edges pointing at vertices no writer produced — the dangling-edge failure
   * the acceptance script's orphan check exists to catch.
   *
   * `card.fraud` is deliberately NOT set from a declined attempt. That flag
   * drives the `cf_Ground_Truth_Label` card overlay, and a card is
   * "ever_fraud" because fraud SETTLED on it, not because an attempt was
   * refused — the same reason the row's own label is withheld.
   *
   * Ids carry an 'F' suffix, disjoint from the settled 'T<n>' space and the
   * non-funding 'T<n>D' space, so nothing can collide and no settled row is
   * renumbered by a change here. */
  void writeFundingDeclinesUpTo(std::int64_t boundInclusive) {
    if (config_.declined == nullptr || paymentW_ == std::nullopt) {
      return;
    }
    static constexpr auto kCardTag =
        channels::tag(channels::Legit::cardPurchase);
    static constexpr auto kMerchantTag =
        channels::tag(channels::Legit::merchant);

    const auto &all = *config_.declined;
    while (declinedCursor_ < all.size() &&
           all[declinedCursor_].txn.timestamp <= boundInclusive) {
      const auto &attempt = all[declinedCursor_];
      /* One-based, and advanced for EVERY attempt including the ones the view
       * filter drops below, so an id names a fixed entry of the run's decline
       * list. Keying it on emitted rows instead would renumber every later
       * decline whenever an earlier one fell outside the view. */
      const auto seq = ++declinedCursor_;
      const auto &tx = attempt.txn;

      const auto channel = tx.session.channel;
      if (channel != kCardTag && channel != kMerchantTag) {
        continue;
      }
      if (!config_.membership.activeAt(ownerOf(tx.source), tx.timestamp) ||
          !config_.membership.activeAt(ownerOf(tx.target), tx.timestamp)) {
        continue;
      }
      /* A declined attempt with no session cannot carry the two endpoint
       * edges the card view requires. The settled path THROWS on this,
       * because a settled row reaching it is a generation defect; here it is
       * skipped, because the funding test can refuse a row the router never
       * reached and that is not a defect in this pass. */
      if (!tx.session.deviceId.assigned() || tx.session.ipAddress.value == 0) {
        continue;
      }

      const bool credit =
          config_.cards != nullptr &&
          config_.cards->byKey.find(tx.source) != config_.cards->byKey.end();
      const auto generation = generationFor(tx.source, tx.timestamp);

      auto &card = artifacts_.cards[tx.source];
      card.credit = credit;
      card.generations.insert(generation);
      artifacts_.merchants.insert(tx.target);
      artifacts_.devices.insert(tx.session.deviceId);
      artifacts_.ips.insert(tx.session.ipAddress);

      const auto declinedId = "T" + std::to_string(seq) + "F";
      const auto cardNumber = derive::cardId(tx.source, credit, generation);
      const auto merchant = derive::merchantId(tx.target);
      const auto unixTime = static_cast<std::uint64_t>(tx.timestamp);

      const auto catIt = catalogByKey_.find(tx.target);
      const auto category = catIt != catalogByKey_.end()
                                ? catIt->second.category
                                : derive::fallbackCategory(tx.target);
      const auto footprint =
          catIt != catalogByKey_.end()
              ? std::optional<entity::merchant::Footprint>{catIt->second
                                                               .footprint}
              : std::nullopt;

      paymentW_->writer().writeRow(
          declinedId,
          time::formatTimestamp(time::fromEpochSeconds(tx.timestamp)),
          tx.amount, kDeclinedLabelWithheld, unixTime,
          merchants::name(category), derive::useChipFor(tx, footprint),
          derive::kInsufficientBalanceError);
      cardSendW_->writer().writeRow(declinedId, cardNumber, unixTime);
      merchantReceiveW_->writer().writeRow(declinedId, merchant, unixTime);
      transactionDeviceW_->writer().writeRow(
          declinedId, common::renderDeviceId(tx.session.deviceId).view(),
          unixTime);
      transactionIpW_->writer().writeRow(
          declinedId, network::format(tx.session.ipAddress).view(), unixTime);
      ++artifacts_.declinedRows;
    }
  }

public:
  void finish() {
    /* CLOSE EXPLICITLY, do not rely on reset(). Each long-lived COPY must
     * close while an exception can still reach runWindowedStream. Table's
     * destructor is only an unwind safety net and downgrades close failures
     * to stderr, so reset() alone could let the run ledger be marked
     * complete after PostgreSQL rejected a card table's final COPY. */
    paymentW_->close();
    cardSendW_->close();
    merchantReceiveW_->close();
    transactionDeviceW_->close();
    transactionIpW_->close();
    paymentW_.reset();
    cardSendW_.reset();
    merchantReceiveW_.reset();
    transactionDeviceW_.reset();
    transactionIpW_.reset();
  }

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept {
    return artifacts_.rows;
  }

  /* Call after finish(); the writers are done by then. */
  [[nodiscard]] StreamedArtifacts takeArtifacts() noexcept {
    return std::move(artifacts_);
  }

private:
  /* The reissue schedule for one account, memoised. Bounded by the number
   * of distinct card accounts in the view. */
  [[nodiscard]] std::uint32_t generationFor(const entity::Key &key,
                                            std::int64_t ts) {
    auto it = generationCache_.find(key);
    if (it == generationCache_.end()) {
      const auto start =
          ::PhantomLedger::time::toEpochSeconds(config_.window.start);
      const auto endExcl =
          ::PhantomLedger::time::toEpochSeconds(::PhantomLedger::time::addDays(
              config_.window.start, config_.window.days));
      it = generationCache_
               .emplace(key, entity::card::reissue::generationsFor(key, start,
                                                                   endExcl))
               .first;
    }
    return entity::card::reissue::generationAt(it->second, ts);
  }

  std::map<entity::Key, std::vector<entity::card::reissue::Generation>>
      generationCache_;
  struct CatalogEntry {
    merchants::Category category;
    entity::merchant::Footprint footprint;
  };

  [[nodiscard]] entity::PersonId ownerOf(entity::Key key) const noexcept {
    if (config_.cards != nullptr) {
      if (const auto *card = config_.cards->forKey(key)) {
        return card->owner;
      }
    }
    if (config_.registry == nullptr || config_.lookup == nullptr) {
      return entity::invalidPerson;
    }
    const auto it = config_.lookup->byId.find(key);
    if (it == config_.lookup->byId.end() ||
        it->second >= config_.registry->records.size()) {
      return entity::invalidPerson;
    }
    return config_.registry->records[it->second].owner;
  }

  /* How far into `config_.declined` the merge has consumed. Monotonic, and
   * indexes the WHOLE list rather than the emitted rows, so a declined row's
   * id is a fixed name for a fixed attempt. */
  std::size_t declinedCursor_ = 0;

  Config config_;

  std::unordered_map<entity::Key, CatalogEntry> catalogByKey_;

  StreamedArtifacts artifacts_;

  std::optional<common::Table> paymentW_;
  std::optional<common::Table> cardSendW_;
  std::optional<common::Table> merchantReceiveW_;
  std::optional<common::Table> transactionDeviceW_;
  std::optional<common::Table> transactionIpW_;
};

} // namespace PhantomLedger::exporter::card_fraud
