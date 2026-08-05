#pragma once
//
// phantomledger/entities/infra/tenure.hpp
//
// The interval during which one session endpoint (a device or an IP)
// belongs to one person. This is the carrier that makes access routing
// POINT-IN-TIME HONEST.
//
// WHY IT LIVES IN `entities` AND NOT `synth`. `infra::Router` is an
// entities-layer type and the include-layer DAG allows entities to see
// only primitives and taxonomies, so the router cannot name
// `synth::infra::devices::Usage`. The generator (synth) owns the
// SAMPLING of these intervals; the router only needs to ASK whether an
// endpoint was live at a timestamp. That question is what this type is.
//
// Tenures are supplied to the router as arrays PARALLEL to the
// per-person endpoint pools: index i of a person's tenure vector
// describes index i of that person's pool. Keeping them parallel rather
// than pairing identity with interval is deliberate — the router's
// sticky state is already a pool INDEX, so the two stay in one
// coordinate system and no lookup is needed on the hot path.
//
// Half-open by construction: `lastEpochExcl` is the first second the
// endpoint is NO LONGER in use. A generator that samples inclusive
// last-seen DAYS converts by adding one day, so consecutive tenures in
// a replacement chain meet exactly and leave no one-second gap for a
// transaction to fall through.

#include <cstdint>

namespace PhantomLedger::infra {

struct Tenure {
  std::int64_t firstEpoch = 0;
  std::int64_t lastEpochExcl = 0;

  [[nodiscard]] constexpr bool contains(std::int64_t ts) const noexcept {
    return ts >= firstEpoch && ts < lastEpochExcl;
  }
};

} // namespace PhantomLedger::infra
