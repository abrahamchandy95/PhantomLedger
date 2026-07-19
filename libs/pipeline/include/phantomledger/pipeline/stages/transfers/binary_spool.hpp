#pragma once
//
// phantomledger/pipeline/stages/transfers/binary_spool.hpp
//
// File-backed candidate spool: the bounded-memory replacement for
// VectorCandidateSpool at the Phase A / Phase B boundary.
//
// WRITE SIDE (Phase A sink, SinkRef-compatible)
// append() encodes rows into fixed-width binary records and streams them
// through a bounded buffer to a temporary file. Span markers carry no
// data: Phase B re-partitions by its own settlement schedule, exactly as
// PrecomputedCursorSource flattens the vector spool.
//
// READ SIDE (Phase B ScheduleCursorSource)
// openCursor() — legal only after finish() — rewinds the file and returns
// a streaming cursor. emitUntil() decodes forward until the first row at
// or beyond the bound and holds that row back for a later call, so read
// memory is one fixed buffer regardless of the candidate count L. Bounds
// may repeat but must never move backward (the driver's settlement
// schedule guarantees this).
//
// ENCODING
// Fixed-width little-endian records, one per transaction, written field
// by field (never a struct memcpy: padding bytes and std::optional layout
// are not stable serialization surfaces). The amount travels as its
// IEEE-754 bit pattern, so a decoded row is bit-identical to the encoded
// one and the Golden digest cannot tell the spools apart. The layout is
// an internal spool format, not an export format; the file never
// outlives the run (the destructor removes it).
//

#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/stages/transfers/windowed_driver.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

class BinarySpoolCursor;

class BinaryCandidateSpool {
public:
  // One fixed-width record per transaction, field by field, little-endian:
  //   source key   role u8 + bank u8 + number u64          10
  //   target key                                           10
  //   amount       IEEE-754 bit pattern as u64              8
  //   timestamp    i64                                      8
  //   fraud        flag u8, ringId u8+u32,
  //                chainId u8+u32, type u8                 12
  //   session      device u8+u64+u32, ip u32, channel u8   18
  static constexpr std::size_t kRecordBytes = 66;

  // Spools to an unnamed temporary file (std::tmpfile), removed by the
  // OS when the stream closes.
  BinaryCandidateSpool();

  // Spools to `path` (created or truncated); the destructor removes the
  // file. Use when the spool must live on a specific volume.
  explicit BinaryCandidateSpool(std::filesystem::path path);

  BinaryCandidateSpool(const BinaryCandidateSpool &) = delete;
  BinaryCandidateSpool &operator=(const BinaryCandidateSpool &) = delete;

  ~BinaryCandidateSpool();

  // ----------------------------------------------- chunk sink interface

  void beginSpan(const chunk::Span &) noexcept {}

  void append(std::span<const transactions::Transaction> txns);

  void endSpan(const chunk::Span &) noexcept {}

  // Flushes and seals the spool. append() is illegal afterwards.
  void finish();

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept { return rows_; }

  [[nodiscard]] std::uint64_t bytesSpooled() const noexcept {
    return rows_ * kRecordBytes;
  }

  // ---------------------------------------------------------- read side

  // Rewinds the sealed spool and returns a streaming cursor over it. The
  // cursor borrows this spool's file handle: the spool must outlive the
  // cursor, and only one cursor may ever be opened.
  [[nodiscard]] std::unique_ptr<BinarySpoolCursor> openCursor();

private:
  void startHeader();

  void flushBuffer();

  std::FILE *file_ = nullptr;

  // Empty for tmpfile spools; otherwise removed by the destructor.
  std::filesystem::path ownedPath_;

  std::vector<unsigned char> writeBuffer_;
  std::uint64_t rows_ = 0;

  bool sealed_ = false;
  bool cursorOpened_ = false;
};

// Streaming ScheduleCursorSource over a sealed BinaryCandidateSpool. File
// order is settlement-span order, hence replay order; the cursor verifies
// this row by row, mirroring PrecomputedCursorSource's replay-sorted
// precondition.
class BinarySpoolCursor final : public ScheduleCursorSource {
public:
  void emitUntil(std::int64_t endExclusiveEpochSeconds,
                 std::vector<transactions::Transaction> &out) override;

  [[nodiscard]] std::uint64_t emittedTotal() const noexcept {
    return emittedTotal_;
  }

  [[nodiscard]] std::uint64_t remaining() const noexcept {
    return rowCount_ - emittedTotal_;
  }

private:
  friend class BinaryCandidateSpool;

  BinarySpoolCursor(std::FILE *file, std::uint64_t rowCount);

  // Decodes the next record into pending_. False at end of spool.
  [[nodiscard]] bool advance();

  std::FILE *file_ = nullptr;
  std::uint64_t rowCount_ = 0;

  std::vector<unsigned char> readBuffer_;
  std::size_t bufferPos_ = 0;
  std::size_t bufferEnd_ = 0;

  transactions::Transaction pending_{};
  bool pendingValid_ = false;
  std::uint64_t decoded_ = 0;
  std::uint64_t emittedTotal_ = 0;

  // Replay-order verification across the streamed sequence.
  transactions::Transaction last_{};
  bool haveLast_ = false;

  std::int64_t lastBoundExcl_ = 0;
  bool boundSeen_ = false;
};

} // namespace PhantomLedger::pipeline::stages::transfers
