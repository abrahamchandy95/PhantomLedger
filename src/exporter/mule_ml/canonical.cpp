#include "phantomledger/exporter/mule_ml/canonical.hpp"

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/primitives/hashing/combine.hpp"
#include "phantomledger/transactions/network/format.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

namespace PhantomLedger::exporter::mule_ml {

namespace {

namespace common = ::PhantomLedger::exporter::common;
namespace enc = ::PhantomLedger::encoding;
namespace net = ::PhantomLedger::network;
namespace ent = ::PhantomLedger::entity;

template <typename ValueT> struct ObservedCount {
  ValueT value{};
  std::uint32_t count = 0;
};

struct AccountHistograms {
  std::unordered_map<std::string, std::uint32_t> deviceCounts;
  std::unordered_map<std::string, std::uint32_t> ipCounts;
};

[[nodiscard]] std::string
pickMostFrequent(const std::unordered_map<std::string, std::uint32_t> &counts) {
  if (counts.empty()) {
    return {};
  }

  const std::string *best = nullptr;
  std::uint32_t bestCount = 0;
  for (const auto &[key, count] : counts) {
    if (best == nullptr) {
      best = &key;
      bestCount = count;
      continue;
    }
    if (count > bestCount || (count == bestCount && key < *best)) {
      best = &key;
      bestCount = count;
    }
  }
  return *best;
}

[[nodiscard]] std::uint64_t stableU64ForKey(const ent::Key &key,
                                            std::uint64_t salt) noexcept {
  return ::PhantomLedger::hashing::make(static_cast<std::uint16_t>(key.role),
                                        static_cast<std::uint8_t>(key.bank),
                                        key.number, salt);
}

[[nodiscard]] std::string fallbackIp(const ent::Key &accountKey) {
  constexpr std::uint64_t kSalt = 0x49500A6E1B0CDEFEULL;
  const auto x = stableU64ForKey(accountKey, kSalt);
  const auto o1 = 11U + static_cast<std::uint32_t>(x % 212U);
  const auto o2 = static_cast<std::uint32_t>((x >> 8U) % 256U);
  const auto o3 = static_cast<std::uint32_t>((x >> 16U) % 256U);
  const auto o4 = 1U + static_cast<std::uint32_t>((x >> 24U) % 254U);
  return std::to_string(o1) + "." + std::to_string(o2) + "." +
         std::to_string(o3) + "." + std::to_string(o4);
}

[[nodiscard]] std::string fallbackDevice(const ent::Key &accountKey) {
  // "DEV_" + rendered account key. encoding::format returns a stack buffer;
  // we materialize it explicitly because we want an owned std::string for
  // the concatenation.
  return "DEV_" + std::string(enc::format(accountKey).view());
}

// ---------------------------------------------------------------------------
// Per-account histogram accumulators — one concern each.
// ---------------------------------------------------------------------------

inline void recordDevice(AccountHistograms &hist,
                         const ::PhantomLedger::devices::Identity &id) {
  const auto buf = common::renderDeviceId(id);
  if (!buf.empty()) {
    // Map key needs to own the string; explicit allocation at the storage
    // boundary, not at every render.
    ++hist.deviceCounts[std::string{buf.view()}];
  }
}

inline void recordIp(AccountHistograms &hist,
                     const ::PhantomLedger::network::Ipv4 &ip) {
  const auto ipStr = net::format(ip);
  if (!ipStr.empty()) {
    ++hist.ipCounts[ipStr];
  }
}

// ---------------------------------------------------------------------------
// Fallback resolution — one concern each.
// ---------------------------------------------------------------------------

[[nodiscard]] inline std::string
resolveDeviceFromPerson(const CanonicalInputs &inputs, ent::PersonId person) {
  if (inputs.devicesByPerson == nullptr) {
    return {};
  }
  const auto devIt = inputs.devicesByPerson->find(person);
  if (devIt == inputs.devicesByPerson->end() || devIt->second.empty()) {
    return {};
  }
  return std::string{common::renderDeviceId(devIt->second.front()).view()};
}

[[nodiscard]] inline std::string
resolveIpFromPerson(const CanonicalInputs &inputs, ent::PersonId person) {
  if (inputs.ipsByPerson == nullptr) {
    return {};
  }
  const auto ipIt = inputs.ipsByPerson->find(person);
  if (ipIt == inputs.ipsByPerson->end() || ipIt->second.empty()) {
    return {};
  }
  return net::format(ipIt->second.front());
}

inline void fillFromPerson(CanonicalPair &pair, const CanonicalInputs &inputs,
                           const ent::Key &accountKey) {
  if (pair.deviceId.size() > 0 && pair.ipAddress.size() > 0) {
    return;
  }
  if (inputs.accountToOwner == nullptr) {
    return;
  }
  const auto ownerIt = inputs.accountToOwner->find(accountKey);
  if (ownerIt == inputs.accountToOwner->end()) {
    return;
  }
  const auto person = ownerIt->second;

  if (pair.deviceId.empty()) {
    pair.deviceId = resolveDeviceFromPerson(inputs, person);
  }
  if (pair.ipAddress.empty()) {
    pair.ipAddress = resolveIpFromPerson(inputs, person);
  }
}

inline void fillFallbacks(CanonicalPair &pair, const ent::Key &accountKey) {
  if (pair.deviceId.empty()) {
    pair.deviceId = fallbackDevice(accountKey);
  }
  if (pair.ipAddress.empty()) {
    pair.ipAddress = fallbackIp(accountKey);
  }
}

// ---------------------------------------------------------------------------
// Per-account assembly.
// ---------------------------------------------------------------------------

[[nodiscard]] inline CanonicalPair
resolveCanonicalPair(const ent::Key &accountKey, const AccountHistograms *hist,
                     const CanonicalInputs &inputs) {
  CanonicalPair pair{};
  if (hist != nullptr) {
    pair.deviceId = pickMostFrequent(hist->deviceCounts);
    pair.ipAddress = pickMostFrequent(hist->ipCounts);
  }
  fillFromPerson(pair, inputs, accountKey);
  fillFallbacks(pair, accountKey);
  return pair;
}

} // namespace

CanonicalMap
buildCanonicalMaps(std::span<const ::PhantomLedger::entity::Key> partyIds,
                   const CanonicalInputs &inputs) {
  CanonicalMap out;
  out.reserve(partyIds.size());

  std::unordered_map<ent::Key, AccountHistograms> perAccount;
  perAccount.reserve(partyIds.size());

  // Phase 1 — observe per-account device/IP usage from the transaction log.
  for (const auto &tx : inputs.finalTxns) {
    auto &hist = perAccount[tx.source];
    recordDevice(hist, tx.session.deviceId);
    recordIp(hist, tx.session.ipAddress);
  }

  // Phase 2 — pick canonical values per account, with fallbacks.
  for (const auto &accountKey : partyIds) {
    const AccountHistograms *hist = nullptr;
    if (const auto it = perAccount.find(accountKey); it != perAccount.end()) {
      hist = &it->second;
    }
    out.emplace(accountKey, resolveCanonicalPair(accountKey, hist, inputs));
  }

  return out;
}

} // namespace PhantomLedger::exporter::mule_ml
