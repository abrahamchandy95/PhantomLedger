#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace PhantomLedger::primitives::concurrent {

/// Array of per-element spinlocks, one `std::atomic_flag` per index.

class AccountLockArray {
public:
  AccountLockArray() = default;

  explicit AccountLockArray(std::size_t count) : flags_(count) {}

  void resize(std::size_t count) {
    flags_ = std::vector<std::atomic_flag>(count);
  }

  [[nodiscard]] std::size_t size() const noexcept { return flags_.size(); }

  void lock(std::size_t idx) noexcept {
    while (flags_[idx].test_and_set(std::memory_order_acquire)) {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
      flags_[idx].wait(true, std::memory_order_relaxed);
#endif
    }
  }

  void unlock(std::size_t idx) noexcept {
    flags_[idx].clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
    flags_[idx].notify_one();
#endif
  }

  void lockPair(std::size_t a, std::size_t b,
                std::size_t invalidSentinel) noexcept {
    const bool aValid = (a != invalidSentinel);
    const bool bValid = (b != invalidSentinel);

    if (!aValid && !bValid) {
      return;
    }
    if (aValid && !bValid) {
      lock(a);
      return;
    }
    if (!aValid && bValid) {
      lock(b);
      return;
    }
    if (a == b) {
      lock(a);
      return;
    }

    if (a < b) {
      lock(a);
      lock(b);
    } else {
      lock(b);
      lock(a);
    }
  }

  void unlockPair(std::size_t a, std::size_t b,
                  std::size_t invalidSentinel) noexcept {
    if (a != invalidSentinel) {
      unlock(a);
    }
    if (b != invalidSentinel && b != a) {
      unlock(b);
    }
  }

private:
  std::vector<std::atomic_flag> flags_;
};

} // namespace PhantomLedger::primitives::concurrent
