#pragma once
//
// phantomledger/exporter/card_fraud/export.hpp
//
// Finisher half of the card-fraud exporter: writes every table the
// streaming leg does NOT stream (Card, Merchant + geo chain, Party,
// Party_Has_Card, Merchant_Category, and the PII investigative layer)
// from the world plus the fold's StreamedArtifacts. Shared by both
// engines: main.cpp's windowed path hands over its sink's artifacts;
// exportAll() runs the SAME sink over the retained corpus and calls
// the same finisher — one code path, two engines.
//
// Is_Merchant is emitted header-only: the world has no modeled
// merchant-owning-party link (business owners own ACCOUNTS, not
// catalog merchants). Documented in the card-fraud-2026-07 block;
// populating it is a model round of its own.
//

#include "phantomledger/exporter/card_fraud/streaming.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::exporter::card_fraud {

struct Options {
  // Required: names/dob/address rendering and the merchant-geo zip
  // table both read the PII pools.
  const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

  // The simulation window: Party.created_at derives from the same
  // Membership model the standard exporter uses.
  ::PhantomLedger::time::Window window{};

  // When set, every table is written directly into PostgreSQL as the
  // bytes the csv::Writer renders — the only production destination.
  const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;

  // Test infrastructure: rendered bytes per table stem.
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

// Writes every table EXCEPT the three the sink streamed
// (Payment_Transaction, Card_Send_Transaction,
// Merchant_Receive_Transaction), from the world + the assembled
// artifacts. Shared by both engines.
[[nodiscard]] Summary
exportFromArtifacts(const ::PhantomLedger::pipeline::SimulationResult &world,
                    const Options &options, StreamedArtifacts artifacts);

// One code path, two engines: runs the streaming sink over the
// retained corpus as one batch, then calls the same finisher.
[[nodiscard]] Summary
exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
          const Options &options);

} // namespace PhantomLedger::exporter::card_fraud
