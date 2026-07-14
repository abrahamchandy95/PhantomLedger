#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::crypto::blake2b {

inline constexpr std::size_t kBlockBytes = 128;
inline constexpr std::size_t kMaxDigestBytes = 64;
inline constexpr std::size_t kMaxKeyBytes = 64;

namespace detail {

struct State {
  std::array<std::uint64_t, 8> chain{};
  std::array<std::uint64_t, 2> count{};
  std::array<std::uint64_t, 2> last{};
  std::array<std::uint8_t, kBlockBytes> buf{};
  std::size_t bufSize = 0;
  std::size_t outSize = 0;
};

[[nodiscard]] bool start(State &state, std::size_t outSize, const void *key,
                         std::size_t keySize) noexcept;
[[nodiscard]] bool absorb(State &state, const void *data,
                          std::size_t size) noexcept;
[[nodiscard]] bool finish(State &state, void *output) noexcept;
void wipe(State &state) noexcept;

} // namespace detail

[[nodiscard]] bool digest(const void *data, std::size_t size, void *output,
                          std::size_t outSize, const void *key = nullptr,
                          std::size_t keySize = 0) noexcept;

class Stream {
public:
  explicit Stream(std::size_t outSize, const void *key = nullptr,
                  std::size_t keySize = 0) noexcept {
    valid_ = detail::start(state_, outSize, key, keySize);
  }

  Stream(const Stream &) = default;
  Stream &operator=(const Stream &) = default;
  ~Stream() { detail::wipe(state_); }

  // False when the constructor was given invalid sizes.
  [[nodiscard]] bool valid() const noexcept { return valid_ && !finished_; }

  [[nodiscard]] bool update(const void *data, std::size_t size) noexcept {
    if (!valid()) {
      return false;
    }
    return detail::absorb(state_, data, size);
  }

  // outSize must equal the size given at construction. Consumes the
  // stream: further update()/finalize() calls return false.
  [[nodiscard]] bool finalize(void *output, std::size_t outSize) noexcept {
    if (!valid() || outSize != state_.outSize) {
      return false;
    }
    finished_ = true;
    const bool ok = detail::finish(state_, output);
    detail::wipe(state_);
    return ok;
  }

private:
  detail::State state_{};
  bool valid_ = false;
  bool finished_ = false;
};

} // namespace PhantomLedger::crypto::blake2b
