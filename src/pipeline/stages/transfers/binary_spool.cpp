#include "phantomledger/pipeline/stages/transfers/binary_spool.hpp"

#include <array>
#include <bit>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

using Txn = ::PhantomLedger::transactions::Transaction;

// Bounded staging on both sides of the file. 256 KiB holds ~4000 records;
// the flush granularity has no output effect.
constexpr std::size_t kFlushThresholdBytes = std::size_t{1} << 18;
constexpr std::size_t kReadBufferBytes = std::size_t{1} << 18;

constexpr std::array<unsigned char, 8> kMagic = {'P', 'L', 'S', 'P',
                                                 'O', 'O', 'L', '1'};
constexpr std::uint32_t kFormatVersion = 1;
constexpr std::size_t kHeaderBytes = kMagic.size() + 4 + 4;

// --------------------------------------------- little-endian primitives

void putU8(unsigned char *&p, std::uint8_t value) noexcept { *p++ = value; }

void putU32(unsigned char *&p, std::uint32_t value) noexcept {
  for (int i = 0; i < 4; ++i) {
    *p++ = static_cast<unsigned char>((value >> (8 * i)) & 0xFFU);
  }
}

void putU64(unsigned char *&p, std::uint64_t value) noexcept {
  for (int i = 0; i < 8; ++i) {
    *p++ = static_cast<unsigned char>((value >> (8 * i)) & 0xFFU);
  }
}

[[nodiscard]] std::uint8_t readU8(const unsigned char *&p) noexcept {
  return *p++;
}

[[nodiscard]] std::uint32_t readU32(const unsigned char *&p) noexcept {
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i) {
    value |= static_cast<std::uint32_t>(*p++) << (8 * i);
  }
  return value;
}

[[nodiscard]] std::uint64_t readU64(const unsigned char *&p) noexcept {
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) {
    value |= static_cast<std::uint64_t>(*p++) << (8 * i);
  }
  return value;
}

// ---------------------------------------------------------- record codec

void putKey(unsigned char *&p, const ::PhantomLedger::entity::Key &key) noexcept {
  putU8(p, static_cast<std::uint8_t>(key.role));
  putU8(p, static_cast<std::uint8_t>(key.bank));
  putU64(p, key.number);
}

[[nodiscard]] ::PhantomLedger::entity::Key
readKey(const unsigned char *&p) noexcept {
  ::PhantomLedger::entity::Key key;
  key.role = static_cast<::PhantomLedger::entity::Role>(readU8(p));
  key.bank = static_cast<::PhantomLedger::entity::Bank>(readU8(p));
  key.number = readU64(p);
  return key;
}

void putOptU32(unsigned char *&p,
               const std::optional<std::uint32_t> &value) noexcept {
  putU8(p, value.has_value() ? 1 : 0);
  putU32(p, value.value_or(0));
}

[[nodiscard]] std::optional<std::uint32_t>
readOptU32(const unsigned char *&p) noexcept {
  const auto has = readU8(p);
  const auto value = readU32(p);
  if (has == 0) {
    return std::nullopt;
  }
  return value;
}

using RecordBytes =
    std::array<unsigned char, BinaryCandidateSpool::kRecordBytes>;

[[nodiscard]] RecordBytes encodeRecord(const Txn &txn) noexcept {
  RecordBytes out{};
  unsigned char *p = out.data();

  putKey(p, txn.source);
  putKey(p, txn.target);
  putU64(p, std::bit_cast<std::uint64_t>(txn.amount));
  putU64(p, static_cast<std::uint64_t>(txn.timestamp));

  putU8(p, txn.fraud.flag);
  putOptU32(p, txn.fraud.ringId);
  putOptU32(p, txn.fraud.chainId);
  putU8(p, static_cast<std::uint8_t>(txn.fraud.type));

  putU8(p, static_cast<std::uint8_t>(txn.session.deviceId.ownerType));
  putU64(p, txn.session.deviceId.ownerId);
  putU32(p, txn.session.deviceId.slot);
  putU32(p, txn.session.ipAddress.value);
  putU8(p, txn.session.channel.value);

  return out;
}

[[nodiscard]] Txn decodeRecord(const unsigned char *record) noexcept {
  const unsigned char *p = record;

  Txn txn;
  txn.source = readKey(p);
  txn.target = readKey(p);
  txn.amount = std::bit_cast<double>(readU64(p));
  txn.timestamp = static_cast<std::int64_t>(readU64(p));

  txn.fraud.flag = readU8(p);
  txn.fraud.ringId = readOptU32(p);
  txn.fraud.chainId = readOptU32(p);
  txn.fraud.type = static_cast<::PhantomLedger::fraud::FraudType>(readU8(p));

  txn.session.deviceId.ownerType =
      static_cast<::PhantomLedger::devices::OwnerType>(readU8(p));
  txn.session.deviceId.ownerId = readU64(p);
  txn.session.deviceId.slot = readU32(p);
  txn.session.ipAddress.value = readU32(p);
  txn.session.channel.value = readU8(p);

  return txn;
}

} // namespace

// ------------------------------------------------- BinaryCandidateSpool

BinaryCandidateSpool::BinaryCandidateSpool() : file_(std::tmpfile()) {
  if (file_ == nullptr) {
    throw std::runtime_error(
        "BinaryCandidateSpool: std::tmpfile() failed (no temp directory?)");
  }
  startHeader();
}

BinaryCandidateSpool::BinaryCandidateSpool(std::filesystem::path path)
    : ownedPath_(std::move(path)) {
  file_ = std::fopen(ownedPath_.string().c_str(), "wb+");
  if (file_ == nullptr) {
    throw std::runtime_error("BinaryCandidateSpool: cannot create spool file " +
                             ownedPath_.string());
  }
  startHeader();
}

BinaryCandidateSpool::~BinaryCandidateSpool() {
  if (file_ != nullptr) {
    std::fclose(file_);
  }
  if (!ownedPath_.empty()) {
    std::error_code ec;
    std::filesystem::remove(ownedPath_, ec);
  }
}

void BinaryCandidateSpool::startHeader() {
  writeBuffer_.reserve(kFlushThresholdBytes + kRecordBytes);

  std::array<unsigned char, kHeaderBytes> header{};
  unsigned char *p = header.data();
  std::memcpy(p, kMagic.data(), kMagic.size());
  p += kMagic.size();
  putU32(p, kFormatVersion);
  putU32(p, static_cast<std::uint32_t>(kRecordBytes));

  writeBuffer_.insert(writeBuffer_.end(), header.begin(), header.end());
}

void BinaryCandidateSpool::append(
    std::span<const transactions::Transaction> txns) {
  if (sealed_) {
    throw std::logic_error("BinaryCandidateSpool: append after finish()");
  }

  for (const auto &txn : txns) {
    const auto record = encodeRecord(txn);
    writeBuffer_.insert(writeBuffer_.end(), record.begin(), record.end());
  }

  rows_ += txns.size();

  if (writeBuffer_.size() >= kFlushThresholdBytes) {
    flushBuffer();
  }
}

void BinaryCandidateSpool::flushBuffer() {
  if (writeBuffer_.empty()) {
    return;
  }

  const auto written =
      std::fwrite(writeBuffer_.data(), 1, writeBuffer_.size(), file_);
  if (written != writeBuffer_.size()) {
    throw std::runtime_error(
        "BinaryCandidateSpool: short write (spool volume full?)");
  }

  writeBuffer_.clear();
}

void BinaryCandidateSpool::finish() {
  if (sealed_) {
    return;
  }

  flushBuffer();
  writeBuffer_.shrink_to_fit();

  if (std::fflush(file_) != 0) {
    throw std::runtime_error("BinaryCandidateSpool: fflush failed");
  }

  sealed_ = true;
}

std::unique_ptr<BinarySpoolCursor> BinaryCandidateSpool::openCursor() {
  if (!sealed_) {
    throw std::logic_error(
        "BinaryCandidateSpool::openCursor requires finish() first");
  }
  if (cursorOpened_) {
    throw std::logic_error("BinaryCandidateSpool: single-cursor spool");
  }

  if (std::fseek(file_, 0L, SEEK_SET) != 0) {
    throw std::runtime_error("BinaryCandidateSpool: rewind failed");
  }

  cursorOpened_ = true;

  return std::unique_ptr<BinarySpoolCursor>(
      new BinarySpoolCursor(file_, rows_));
}

// ----------------------------------------------------- BinarySpoolCursor

BinarySpoolCursor::BinarySpoolCursor(std::FILE *file, std::uint64_t rowCount)
    : file_(file), rowCount_(rowCount) {
  readBuffer_.resize(kReadBufferBytes);

  std::array<unsigned char, kHeaderBytes> header{};
  if (std::fread(header.data(), 1, header.size(), file_) != header.size()) {
    throw std::runtime_error("BinarySpoolCursor: truncated spool header");
  }
  if (std::memcmp(header.data(), kMagic.data(), kMagic.size()) != 0) {
    throw std::runtime_error("BinarySpoolCursor: bad spool magic");
  }

  const unsigned char *p = header.data() + kMagic.size();
  const auto version = readU32(p);
  const auto recordBytes = readU32(p);
  if (version != kFormatVersion ||
      recordBytes != BinaryCandidateSpool::kRecordBytes) {
    throw std::runtime_error("BinarySpoolCursor: spool format mismatch");
  }
}

bool BinarySpoolCursor::advance() {
  if (decoded_ == rowCount_) {
    return false;
  }

  constexpr auto kRecord = BinaryCandidateSpool::kRecordBytes;

  if (bufferEnd_ - bufferPos_ < kRecord) {
    // Slide the partial tail to the front and refill from the file, so
    // records that straddle a buffer boundary decode contiguously.
    const auto tail = bufferEnd_ - bufferPos_;
    std::memmove(readBuffer_.data(), readBuffer_.data() + bufferPos_, tail);
    bufferPos_ = 0;
    bufferEnd_ = tail;

    const auto got = std::fread(readBuffer_.data() + bufferEnd_, 1,
                                readBuffer_.size() - bufferEnd_, file_);
    bufferEnd_ += got;

    if (bufferEnd_ - bufferPos_ < kRecord) {
      throw std::runtime_error("BinarySpoolCursor: spool truncated mid-record");
    }
  }

  pending_ = decodeRecord(readBuffer_.data() + bufferPos_);
  bufferPos_ += kRecord;
  ++decoded_;
  pendingValid_ = true;

  constexpr transactions::Comparator replayLess{
      transactions::Comparator::Scope::fundsTransfer};
  if (haveLast_ && replayLess(pending_, last_)) {
    throw std::invalid_argument(
        "BinarySpoolCursor requires replay-sorted rows");
  }
  last_ = pending_;
  haveLast_ = true;

  return true;
}

void BinarySpoolCursor::emitUntil(std::int64_t endExclusiveEpochSeconds,
                                  std::vector<transactions::Transaction> &out) {
  if (boundSeen_ && endExclusiveEpochSeconds < lastBoundExcl_) {
    throw std::logic_error(
        "BinarySpoolCursor::emitUntil bounds must be monotone");
  }

  lastBoundExcl_ = endExclusiveEpochSeconds;
  boundSeen_ = true;

  for (;;) {
    if (!pendingValid_ && !advance()) {
      return;
    }
    if (pending_.timestamp >= endExclusiveEpochSeconds) {
      return;
    }
    out.push_back(pending_);
    pendingValid_ = false;
    ++emittedTotal_;
  }
}

} // namespace PhantomLedger::pipeline::stages::transfers
