#pragma once
//
// phantomledger/pipeline/stages/transfers/replay_spool.hpp
//
// RAM R2.4b-2: the disk-backed implementation of the base-stream replay
// seam (activity/spending/replay_source.hpp). The windowed run writes
// the screened base stream (timestamp order) to a sequential temporary
// file after the spending prep has consumed it, frees the resident
// vector, and the day driver's ledger replay decodes rows back one
// bounded buffer at a time.
//
// Each record carries exactly the fields advanceBookThrough uses —
// source key, target key, amount (IEEE-754 bit pattern), channel,
// timestamp — written field by field (never a struct memcpy). The file
// is process-private scratch (std::tmpfile: unlinked immediately, gone
// with the stream), so host byte order is the format.
//
// postThrough mirrors clearing::advanceBookThrough call for call: a
// null book returns without consuming; the first row at or beyond the
// bound is held back for the next call; every posted row makes the
// identical ledger->transfer call. The caller keeps the index
// (RunState::baseIdx) and presents monotone bounds, which the fromIdx
// handshake asserts.
//

#include "phantomledger/activity/spending/replay_source.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

class BaseReplaySpool final
    : public ::PhantomLedger::activity::spending::BaseReplaySource {
public:
  //   source key   role u8 + bank u8 + number u64          10
  //   target key                                           10
  //   amount       IEEE-754 bit pattern as u64              8
  //   channel      u8                                       1
  //   timestamp    i64                                      8
  static constexpr std::size_t kRecordBytes = 37;

  BaseReplaySpool() : file_(std::tmpfile()) {
    if (file_ == nullptr) {
      throw std::runtime_error(
          "BaseReplaySpool: cannot create temporary spool file");
    }
    writeBuffer_.reserve(kBufferBytes);
  }

  BaseReplaySpool(const BaseReplaySpool &) = delete;
  BaseReplaySpool &operator=(const BaseReplaySpool &) = delete;

  ~BaseReplaySpool() override {
    if (file_ != nullptr) {
      std::fclose(file_);
    }
  }

  // ------------------------------------------------------------ write

  void spool(std::span<const transactions::Transaction> rows) {
    if (sealed_) {
      throw std::logic_error("BaseReplaySpool: spool() after seal()");
    }
    for (const auto &txn : rows) {
      encode(txn);
      ++rowCount_;
      if (writeBuffer_.size() >= kBufferBytes) {
        flushWrites();
      }
    }
  }

  // Flushes, rewinds, and switches the file to the read side.
  void seal() {
    if (sealed_) {
      return;
    }
    flushWrites();
    if (std::fflush(file_) != 0 || std::fseek(file_, 0, SEEK_SET) != 0) {
      throw std::runtime_error("BaseReplaySpool: seal flush/rewind failed");
    }
    sealed_ = true;
  }

  [[nodiscard]] std::uint64_t rowsSpooled() const noexcept {
    return rowCount_;
  }

  [[nodiscard]] std::uint64_t bytesSpooled() const noexcept {
    return rowCount_ * kRecordBytes;
  }

  // ---------------------------------------------- BaseReplaySource

  [[nodiscard]] std::size_t
  postThrough(clearing::Ledger *book, std::size_t fromIdx,
              clearing::TimeBound bound) const override {
    if (book == nullptr) {
      return fromIdx; // mirror advanceBookThrough exactly
    }
    if (!sealed_) {
      throw std::logic_error("BaseReplaySpool: postThrough before seal()");
    }
    if (fromIdx != consumed_) {
      throw std::logic_error(
          "BaseReplaySpool: replay index diverged from spool position");
    }

    while (true) {
      if (!pendingValid_) {
        if (!decodeNext()) {
          break; // end of spool
        }
      }
      if (!bound.includes(pending_.timestamp)) {
        break; // held back for the next (monotone) bound
      }
      (void)book->transfer(pending_.source, pending_.target, pending_.amount,
                           pending_.session.channel);
      pendingValid_ = false;
      ++consumed_;
    }

    return consumed_;
  }

private:
  static constexpr std::size_t kBufferBytes = 1U << 16U;

  template <class T> void put(const T &value) {
    const auto *bytes = reinterpret_cast<const unsigned char *>(&value);
    writeBuffer_.insert(writeBuffer_.end(), bytes, bytes + sizeof(T));
  }

  void encode(const transactions::Transaction &txn) {
    put(static_cast<std::uint8_t>(txn.source.role));
    put(static_cast<std::uint8_t>(txn.source.bank));
    put(txn.source.number);
    put(static_cast<std::uint8_t>(txn.target.role));
    put(static_cast<std::uint8_t>(txn.target.bank));
    put(txn.target.number);
    std::uint64_t amountBits = 0;
    std::memcpy(&amountBits, &txn.amount, sizeof(amountBits));
    put(amountBits);
    put(static_cast<std::uint8_t>(txn.session.channel.value));
    put(txn.timestamp);
  }

  void flushWrites() {
    if (writeBuffer_.empty()) {
      return;
    }
    const auto written =
        std::fwrite(writeBuffer_.data(), 1, writeBuffer_.size(), file_);
    if (written != writeBuffer_.size()) {
      throw std::runtime_error("BaseReplaySpool: short write to spool file");
    }
    writeBuffer_.clear();
  }

  template <class T> [[nodiscard]] T take() const {
    T value{};
    std::memcpy(&value, readBuffer_.data() + readPos_, sizeof(T));
    readPos_ += sizeof(T);
    return value;
  }

  [[nodiscard]] bool decodeNext() const {
    if (decoded_ == rowCount_) {
      return false;
    }
    if (readEnd_ - readPos_ < kRecordBytes) {
      refill();
    }

    pending_ = transactions::Transaction{};
    pending_.source.role = static_cast<entity::Role>(take<std::uint8_t>());
    pending_.source.bank = static_cast<entity::Bank>(take<std::uint8_t>());
    pending_.source.number = take<std::uint64_t>();
    pending_.target.role = static_cast<entity::Role>(take<std::uint8_t>());
    pending_.target.bank = static_cast<entity::Bank>(take<std::uint8_t>());
    pending_.target.number = take<std::uint64_t>();
    const auto amountBits = take<std::uint64_t>();
    std::memcpy(&pending_.amount, &amountBits, sizeof(pending_.amount));
    pending_.session.channel.value = take<std::uint8_t>();
    pending_.timestamp = take<std::int64_t>();

    ++decoded_;
    pendingValid_ = true;
    return true;
  }

  void refill() const {
    const auto held = readEnd_ - readPos_;
    if (held != 0) {
      std::memmove(readBuffer_.data(), readBuffer_.data() + readPos_, held);
    }
    readPos_ = 0;
    readEnd_ = held;
    if (readBuffer_.size() < kBufferBytes) {
      readBuffer_.resize(kBufferBytes);
    }
    const auto got = std::fread(readBuffer_.data() + readEnd_, 1,
                                readBuffer_.size() - readEnd_, file_);
    readEnd_ += got;
    if (readEnd_ - readPos_ < kRecordBytes) {
      throw std::runtime_error("BaseReplaySpool: truncated spool record");
    }
  }

  std::FILE *file_ = nullptr;

  std::vector<unsigned char> writeBuffer_;
  std::uint64_t rowCount_ = 0;
  bool sealed_ = false;

  // Read-side state is mutable because BaseReplaySource::postThrough is
  // const (the SPAN adapter is stateless); the spool's cursor position
  // advances under the same caller-owned-index contract, verified by
  // the fromIdx handshake above.
  mutable std::vector<unsigned char> readBuffer_;
  mutable std::size_t readPos_ = 0;
  mutable std::size_t readEnd_ = 0;
  mutable transactions::Transaction pending_{};
  mutable bool pendingValid_ = false;
  mutable std::uint64_t decoded_ = 0;
  mutable std::size_t consumed_ = 0;
};

} // namespace PhantomLedger::pipeline::stages::transfers
