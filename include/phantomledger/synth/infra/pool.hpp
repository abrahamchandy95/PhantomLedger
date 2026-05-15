#pragma once

#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::synth::infra::pool {

template <class T, class Hash = std::hash<T>, class Eq = std::equal_to<T>>
inline void
swapDelete(std::vector<T> &pool,
           std::unordered_map<T, std::size_t, Hash, Eq> &indexByItem,
           const T &item) {
  const auto it = indexByItem.find(item);
  const std::size_t idx = it->second;
  indexByItem.erase(it);

  const std::size_t lastIdx = pool.size() - 1;
  if (idx != lastIdx) {
    pool[idx] = std::move(pool[lastIdx]);
    indexByItem[pool[idx]] = idx;
  }
  pool.pop_back();
}

template <class T, class Hash = std::hash<T>, class Eq = std::equal_to<T>>
[[nodiscard]] inline bool
contains(const std::unordered_map<T, std::size_t, Hash, Eq> &indexByItem,
         const T &item) {
  return indexByItem.find(item) != indexByItem.end();
}

} // namespace PhantomLedger::synth::infra::pool
