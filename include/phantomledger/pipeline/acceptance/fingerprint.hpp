#pragma once
//
// phantomledger/pipeline/acceptance/fingerprint.hpp
//
// Exact acceptance fingerprint for window-invariance and equivalence legs.
//
// A leg produces a RunFingerprint; two legs compare with firstDifference(),
// which returns an empty string on exact equality or a human-readable
// description of the first differing field. All comparisons are exact:
// no approximate equality is used anywhere in a deterministic gate.
//
// The digest field is the canonical stream digest (sinks::Golden). Row
// content and output ordering are covered by it; the remaining fields make
// diagnosis cheap when a digest mismatch occurs, and independently pin the
// invariants the digest cannot see (drops, final book, fraud budget, card
// events).
//
// Header-only and side-effect free. The book hash reads balances through
// Ledger's non-const accessors, so capture takes Ledger by non-const
// reference; it does not modify the book.
//

#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace PhantomLedger::pipeline::acceptance {

namespace detail {

inline constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325ULL;
inline constexpr std::uint64_t kFnvPrime = 0x00000100000001b3ULL;

[[nodiscard]] inline std::uint64_t fnvMix(std::uint64_t hash,
                                          std::uint64_t value) noexcept {
  for (int i = 0; i < 8; ++i) {
    hash ^= (value >> (i * 8)) & 0xFFU;
    hash *= kFnvPrime;
  }
  return hash;
}

[[nodiscard]] inline std::uint64_t bitsOf(double value) noexcept {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

} // namespace detail

// Bit-exact hash of every account's cash, overdraft, linked and courtesy
// balances, in index order.
[[nodiscard]] inline std::uint64_t hashBook(clearing::Ledger &book) {
  auto hash = detail::kFnvOffset;
  const auto count = book.size();
  for (clearing::Ledger::Index idx = 0; idx < count; ++idx) {
    hash = detail::fnvMix(hash, idx);
    hash = detail::fnvMix(hash, detail::bitsOf(book.cash(idx)));
    hash = detail::fnvMix(hash, detail::bitsOf(book.overdraft(idx)));
    hash = detail::fnvMix(hash, detail::bitsOf(book.linked(idx)));
    hash = detail::fnvMix(hash, detail::bitsOf(book.courtesy(idx)));
  }
  return hash;
}

struct RunFingerprint {
  // Ordered maps so iteration, comparison and first-difference reporting
  // are deterministic regardless of the source unordered_map layout.
  using ReasonCounts = std::map<std::string, std::uint32_t>;
  using ChannelCounts =
      std::map<std::pair<std::string, std::uint32_t>, std::uint32_t>;

  std::uint64_t rows = 0;
  std::string digest;

  std::uint64_t candidateRows = 0; // exact realized L
  std::uint64_t fraudRows = 0;
  std::uint64_t cardEvents = 0;

  ReasonCounts preDropsByReason;
  ChannelCounts preDropsByChannel;
  ReasonCounts postDropsByReason;
  ChannelCounts postDropsByChannel;

  std::uint64_t bookHash = 0;

  [[nodiscard]] static ReasonCounts normalize(
      const ::PhantomLedger::transfers::legit::ledger::ReplayDropLedger::Counts
          &counts) {
    ReasonCounts out;
    for (const auto &[reason, count] : counts) {
      out.emplace(reason, count);
    }
    return out;
  }

  [[nodiscard]] static ChannelCounts
  normalize(const ::PhantomLedger::transfers::legit::ledger::ReplayDropLedger::
                CountsByChannel &counts) {
    ChannelCounts out;
    for (const auto &[key, count] : counts) {
      out.emplace(
          std::pair{key.first, static_cast<std::uint32_t>(key.second.value)},
          count);
    }
    return out;
  }
};

namespace detail {

[[nodiscard]] inline std::string
diffReasonCounts(const char *label, const RunFingerprint::ReasonCounts &a,
                 const RunFingerprint::ReasonCounts &b) {
  auto ia = a.begin();
  auto ib = b.begin();
  while (ia != a.end() || ib != b.end()) {
    if (ia == a.end() || (ib != b.end() && ib->first < ia->first)) {
      return std::string(label) + ": '" + ib->first +
             "' only present in leg B (count " + std::to_string(ib->second) +
             ")";
    }
    if (ib == b.end() || ia->first < ib->first) {
      return std::string(label) + ": '" + ia->first +
             "' only present in leg A (count " + std::to_string(ia->second) +
             ")";
    }
    if (ia->second != ib->second) {
      return std::string(label) + ": '" + ia->first + "' " +
             std::to_string(ia->second) + " != " + std::to_string(ib->second);
    }
    ++ia;
    ++ib;
  }
  return {};
}

[[nodiscard]] inline std::string
diffChannelCounts(const char *label, const RunFingerprint::ChannelCounts &a,
                  const RunFingerprint::ChannelCounts &b) {
  const auto describe = [](const RunFingerprint::ChannelCounts::key_type &k) {
    return "'" + k.first + "'/channel " + std::to_string(k.second);
  };
  auto ia = a.begin();
  auto ib = b.begin();
  while (ia != a.end() || ib != b.end()) {
    if (ia == a.end() || (ib != b.end() && ib->first < ia->first)) {
      return std::string(label) + ": " + describe(ib->first) +
             " only present in leg B (count " + std::to_string(ib->second) +
             ")";
    }
    if (ib == b.end() || ia->first < ib->first) {
      return std::string(label) + ": " + describe(ia->first) +
             " only present in leg A (count " + std::to_string(ia->second) +
             ")";
    }
    if (ia->second != ib->second) {
      return std::string(label) + ": " + describe(ia->first) + " " +
             std::to_string(ia->second) + " != " + std::to_string(ib->second);
    }
    ++ia;
    ++ib;
  }
  return {};
}

} // namespace detail

// Empty string on exact equality; otherwise the first differing field.
[[nodiscard]] inline std::string firstDifference(const RunFingerprint &a,
                                                 const RunFingerprint &b) {
  const auto diffCount = [](const char *label, std::uint64_t x,
                            std::uint64_t y) -> std::string {
    if (x == y) {
      return {};
    }
    return std::string(label) + ": " + std::to_string(x) +
           " != " + std::to_string(y);
  };

  if (auto d = diffCount("rows", a.rows, b.rows); !d.empty()) {
    return d;
  }
  if (a.digest != b.digest) {
    return "digest: " + a.digest + " != " + b.digest;
  }
  if (auto d = diffCount("candidateRows (L)", a.candidateRows, b.candidateRows);
      !d.empty()) {
    return d;
  }
  if (auto d = diffCount("fraudRows", a.fraudRows, b.fraudRows); !d.empty()) {
    return d;
  }
  if (auto d = diffCount("cardEvents", a.cardEvents, b.cardEvents);
      !d.empty()) {
    return d;
  }
  if (auto d = detail::diffReasonCounts("preDropsByReason", a.preDropsByReason,
                                        b.preDropsByReason);
      !d.empty()) {
    return d;
  }
  if (auto d = detail::diffChannelCounts(
          "preDropsByChannel", a.preDropsByChannel, b.preDropsByChannel);
      !d.empty()) {
    return d;
  }
  if (auto d = detail::diffReasonCounts(
          "postDropsByReason", a.postDropsByReason, b.postDropsByReason);
      !d.empty()) {
    return d;
  }
  if (auto d = detail::diffChannelCounts(
          "postDropsByChannel", a.postDropsByChannel, b.postDropsByChannel);
      !d.empty()) {
    return d;
  }
  if (a.bookHash != b.bookHash) {
    std::ostringstream out;
    out << "bookHash: 0x" << std::hex << a.bookHash << " != 0x" << b.bookHash;
    return out.str();
  }
  return {};
}

} // namespace PhantomLedger::pipeline::acceptance
