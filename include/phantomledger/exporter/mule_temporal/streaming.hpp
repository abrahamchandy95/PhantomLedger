#pragma once

#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/parties/pii.hpp"
#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <memory>
#include <span>

namespace PhantomLedger::exporter::mule_temporal {

[[nodiscard]] constexpr bool visible(std::uint64_t from, std::uint64_t to,
                                     std::uint64_t seed) noexcept {
  return from > 0 && seed > 0 && from <= seed && (to == 0 || seed < to);
}

class StreamingMuleTemporalExport {
public:
  struct Config {
    const entity::account::Registry *registry = nullptr;
    const entity::pii::Roster *pii = nullptr;
    const entity::parties::relocation::Schedule *relocation = nullptr;
    const synth::infra::devices::Output *devices = nullptr;
    const synth::infra::ips::Output *ips = nullptr;
    synth::pii::Membership membership;
    time::Window window;
    std::uint64_t seed = 42;
    const sinks::PgMirror *pgMirror = nullptr;
    common::TableCapture *capture = nullptr;
  };

  explicit StreamingMuleTemporalExport(Config config);
  ~StreamingMuleTemporalExport();
  StreamingMuleTemporalExport(const StreamingMuleTemporalExport &) = delete;
  StreamingMuleTemporalExport &
  operator=(const StreamingMuleTemporalExport &) = delete;
  StreamingMuleTemporalExport(StreamingMuleTemporalExport &&) noexcept;
  StreamingMuleTemporalExport &
  operator=(StreamingMuleTemporalExport &&) noexcept;

  void beginSpan(const pipeline::chunk::Span &) noexcept {}
  void append(std::span<const transactions::Transaction> batch);
  void endSpan(const pipeline::chunk::Span &) noexcept {}
  void finish();
  [[nodiscard]] std::uint64_t rowsWritten() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace PhantomLedger::exporter::mule_temporal
