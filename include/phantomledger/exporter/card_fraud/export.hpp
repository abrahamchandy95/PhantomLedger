#pragma once
/*
  Finisher half of the card-fraud exporter: writes every table the
  streaming leg does NOT stream (Card, Merchant + geo chain, Party,
  Party_Has_Card, Merchant_Category, and the PII investigative layer) from
  the world plus the fold's StreamedArtifacts. Shared by both engines:
  main.cpp's windowed path hands over its sink's artifacts; exportAll()
  runs the SAME sink over the retained corpus and calls the same finisher
  — one code path, two engines.

  Is_Merchant carries the merchant -> proprietor register. Owners come from
  the business-owner cohort, resolved DRAW-FREE FROM THE MERCHANT KEY ALONE
  so owner-edge presence cannot correlate with footprint, size or
  geography, and the table stays RESTRICTED TO VIEW-OBSERVED MERCHANTS
  because the Merchant vertex set is stream-derived. An empty table
  hard-aborts the downstream tf_gnn_loader_v2 push.

  Has_Device and Has_IP come from the world's usage records filtered by
  registry coverage (`infra::enrollment`) — NEVER from the stream, because
  test_card_point_in_time classifies both as world-derived and requires
  full-vs-prefix byte identity. See tests/test_card_endpoint_graph.cpp for
  the gate that keeps the residual signal weak.
 */

#include "phantomledger/exporter/card_fraud/streaming.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::exporter::card_fraud {

struct Options {
  /* Required: party identity rendering (names / dob / street) reads the PII
   * pools. Merchant and home geography are world-modelled and resolved
   * through synth::geo::geography(), not from the pools' zip table. */
  const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

  /* The simulation window: Party.created_at derives from the same
   * Membership model the standard exporter uses. */
  ::PhantomLedger::time::Window window{};

  /* When set, every table is written directly into PostgreSQL as the bytes
   * the csv::Writer renders — the only production destination. */
  const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;

  /* Test infrastructure: rendered bytes per table stem. */
  common::TableCapture *capture = nullptr;
};

struct Summary {
  std::uint64_t totalRows = 0;
  std::uint64_t viewRows = 0;
  std::uint64_t fraudViewRows = 0;

  std::size_t cardCount = 0;
  std::size_t merchantCount = 0;
  std::size_t partyCount = 0;
  std::size_t cityCount = 0;
  std::size_t stateCount = 0;
  std::size_t zipcodeCount = 0;
};

/* Writes every table EXCEPT the five the sink streamed: the
 * Payment_Transaction vertex and its four timestamped relationships
 * (Card_Send_Transaction, Merchant_Receive_Transaction,
 * Transaction_Uses_Device, Transaction_Uses_IP). Together the sink and this
 * finisher produce schema::card_fraud::kTableCount tables. Shared by both
 * engines. */
[[nodiscard]] Summary
exportFromArtifacts(const ::PhantomLedger::pipeline::SimulationResult &world,
                    const Options &options, StreamedArtifacts artifacts);

/* One code path, two engines: runs the streaming sink over the retained
 * corpus as one batch, then calls the same finisher. */
[[nodiscard]] Summary
exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
          const Options &options);

} // namespace PhantomLedger::exporter::card_fraud
