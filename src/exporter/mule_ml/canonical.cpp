#include "phantomledger/exporter/mule_ml/canonical.hpp"

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/infra/format.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/primitives/hashing/combine.hpp"

#include <cassert>
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
namespace dev_ns = ::PhantomLedger::devices;

template <typename K, typename H>
[[nodiscard]] K
pickMostFrequent(const std::unordered_map<K, std::uint32_t, H> &counts) {
  assert(!counts.empty());
  const K *best = nullptr;
  std::uint32_t bestCount = 0;
  for (const auto &[key, count] : counts) {
    if (best == nullptr || count > bestCount ||
        (count == bestCount && key < *best)) {
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
  const auto ip =
      net::Ipv4::pack(static_cast<std::uint8_t>(11U + (x % 212U)),
                      static_cast<std::uint8_t>((x >> 8U) % 256U),
                      static_cast<std::uint8_t>((x >> 16U) % 256U),
                      static_cast<std::uint8_t>(1U + ((x >> 24U) % 254U)));
  return std::string{net::format(ip).view()};
}

[[nodiscard]] std::string fallbackDevice(const ent::Key &accountKey) {
  const auto rendered = enc::format(accountKey);
  std::string out = "DEV_";
  out.append(rendered.view());
  return out;
}

[[nodiscard]] inline std::string
resolveDeviceFromPerson(const CanonicalResolveInputs &inputs,
                        ent::PersonId person) {
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
resolveIpFromPerson(const CanonicalResolveInputs &inputs,
                    ent::PersonId person) {
  if (inputs.ipsByPerson == nullptr) {
    return {};
  }
  const auto ipIt = inputs.ipsByPerson->find(person);
  if (ipIt == inputs.ipsByPerson->end() || ipIt->second.empty()) {
    return {};
  }
  return std::string{net::format(ipIt->second.front()).view()};
}

inline void fillFromPerson(CanonicalPair &pair,
                           const CanonicalResolveInputs &inputs,
                           const ent::Key &accountKey) {
  if (!pair.deviceId.empty() && !pair.ipAddress.empty()) {
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

[[nodiscard]] inline CanonicalPair
resolveCanonicalPair(const ent::Key &accountKey,
                     const detail::AccountHistograms *hist,
                     const CanonicalResolveInputs &inputs) {
  CanonicalPair pair{};
  if (hist != nullptr) {
    if (!hist->deviceCounts.empty()) {
      const auto devKey = pickMostFrequent(hist->deviceCounts);
      pair.deviceId = std::string{common::renderDeviceId(devKey).view()};
    }
    if (!hist->ipCounts.empty()) {
      const auto ipKey = pickMostFrequent(hist->ipCounts);
      pair.ipAddress = std::string{net::format(ipKey).view()};
    }
  }
  fillFromPerson(pair, inputs, accountKey);
  fillFallbacks(pair, accountKey);
  return pair;
}

} // namespace

void CanonicalAccumulator::observe(
    const ::PhantomLedger::transactions::Transaction &tx) {
  auto &hist = perAccount_[tx.source];
  if (tx.session.deviceId.assigned()) {
    ++hist.deviceCounts[tx.session.deviceId];
  }
  ++hist.ipCounts[tx.session.ipAddress];
}

CanonicalMap CanonicalAccumulator::resolve(
    std::span<const ::PhantomLedger::entity::Key> partyIds,
    const CanonicalResolveInputs &inputs) const {
  CanonicalMap out;
  out.reserve(partyIds.size());

  for (const auto &accountKey : partyIds) {
    const detail::AccountHistograms *hist = nullptr;
    if (const auto it = perAccount_.find(accountKey);
        it != perAccount_.end()) {
      hist = &it->second;
    }
    out.emplace(accountKey, resolveCanonicalPair(accountKey, hist, inputs));
  }

  return out;
}

CanonicalMap
buildCanonicalMaps(std::span<const ::PhantomLedger::entity::Key> partyIds,
                   const CanonicalInputs &inputs) {
  CanonicalAccumulator accumulator;
  for (const auto &tx : inputs.finalTxns) {
    accumulator.observe(tx);
  }

  const CanonicalResolveInputs resolveInputs{
      .devicesByPerson = inputs.devicesByPerson,
      .ipsByPerson = inputs.ipsByPerson,
      .accountToOwner = inputs.accountToOwner,
  };
  return accumulator.resolve(partyIds, resolveInputs);
}

} // namespace PhantomLedger::exporter::mule_ml
