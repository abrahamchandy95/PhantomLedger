#pragma once

#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::pipeline::sorting {

namespace detail {

[[nodiscard]] inline std::int64_t
ringSortValue(const transactions::Transaction &tx) noexcept {
  return tx.fraud.ringId.has_value()
             ? static_cast<std::int64_t>(*tx.fraud.ringId)
             : -1;
}

[[nodiscard]] inline bool
lessByAuditKey(const transactions::Transaction &a,
               const transactions::Transaction &b) noexcept {
  if (a.timestamp != b.timestamp) {
    return a.timestamp < b.timestamp;
  }
  if (a.source != b.source) {
    return a.source < b.source;
  }
  if (a.target != b.target) {
    return a.target < b.target;
  }
  if (a.amount != b.amount) {
    return a.amount < b.amount;
  }
  if (a.fraud.flag != b.fraud.flag) {
    return a.fraud.flag < b.fraud.flag;
  }

  const auto ra = ringSortValue(a);
  const auto rb = ringSortValue(b);
  if (ra != rb) {
    return ra < rb;
  }

  if (a.session.channel != b.session.channel) {
    return a.session.channel < b.session.channel;
  }
  if (a.session.deviceId != b.session.deviceId) {
    return a.session.deviceId < b.session.deviceId;
  }
  return a.session.ipAddress < b.session.ipAddress;
}

} // namespace detail

inline void sortInPlace(std::vector<transactions::Transaction> &txns) {
  std::stable_sort(txns.begin(), txns.end(), detail::lessByAuditKey);
}

[[nodiscard]] inline std::vector<transactions::Transaction>
sorted(std::span<const transactions::Transaction> txns) {
  std::vector<transactions::Transaction> out{txns.begin(), txns.end()};
  sortInPlace(out);
  return out;
}

} // namespace PhantomLedger::pipeline::sorting
