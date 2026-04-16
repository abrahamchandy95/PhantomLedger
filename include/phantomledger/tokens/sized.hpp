#pragma once
/*
 Fixed capacity inline storage for short tokens
 keeps token bytes inside the object itself, without a separate allocation
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PhantomLedger::tokens {

template <std::size_t Capacity> class Bounded {
  static_assert(Capacity > 0, "Capacity must be greater than zero");
  static_assert(Capacity <= static_cast<std::size_t>(
                                std::numeric_limits<std::uint8_t>::max()),
                "Capacity must fit in std::uint8_t");

public:
  static constexpr std::size_t max_size = Capacity;

  constexpr Bounded() noexcept = default;

  explicit Bounded(std::string_view value) { assign(value); }

  void assign(std::string_view value) {
    if (value.size() > Capacity) {
      throw std::length_error("Fixed token value exceeds capacity");
    }

    size_ = static_cast<std::uint8_t>(value.size());
    if (!value.empty()) {
      std::memcpy(storage_.data(), value.data(), value.size());
    }
  }

  [[nodiscard]] constexpr const char *data() const noexcept {
    return storage_.data();
  }

  [[nodiscard]] constexpr std::size_t size() const noexcept {
    return static_cast<std::size_t>(size_);
  }

  [[nodiscard]] constexpr bool empty() const noexcept { return 0 == size_; }

  [[nodiscard]] std::string_view view() const noexcept {
    return std::string_view{storage_.data(), size()};
  }

  [[nodiscard]] std::string str() const {
    return std::string{storage_.data(), size()};
  }

  [[nodiscard]] bool startsWith(char prefix) const noexcept {
    return size_ > 0 && storage_[0] == prefix;
  }

  [[nodiscard]] bool startsWith(std::string_view prefix) const noexcept {
    return size() >= prefix.size() &&
           std::memcmp(storage_.data(), prefix.data(), prefix.size()) == 0;
  }

  friend bool operator==(const Bounded &lhs, const Bounded &rhs) noexcept {
    return lhs.size_ == rhs.size_ &&
           std::memcmp(lhs.storage_.data(), rhs.storage_.data(), lhs.size()) ==
               0;
  }

  friend bool operator!=(const Bounded &lhs, const Bounded &rhs) noexcept {
    return !(lhs == rhs);
  }

  friend bool operator<(const Bounded &lhs, const Bounded &rhs) noexcept {
    const std::size_t common = std::min(lhs.size(), rhs.size());
    const int compare =
        std::memcmp(lhs.storage_.data(), rhs.storage_.data(), common);
    return compare < 0 || (compare == 0 && lhs.size() < rhs.size());
  }

private:
  std::array<char, Capacity> storage_{};
  std::uint8_t size_ = 0;
};

} // namespace PhantomLedger::tokens
