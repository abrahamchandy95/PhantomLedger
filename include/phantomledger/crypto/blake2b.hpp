#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace PhantomLedger::crypto::blake2b {

namespace detail {

struct State {
  std::uint64_t chain[8]{};
  std::uint64_t count[2]{};
  std::uint64_t last[2]{};
  std::uint8_t buf[128]{};
  std::size_t bufSize = 0;
  std::size_t outSize = 0;
};

inline constexpr std::uint64_t initialVector[8] = {
    0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL, 0x3C6EF372FE94F82BULL,
    0xA54FF53A5F1D36F1ULL, 0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL,
    0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL,
};

inline constexpr std::uint8_t sigma[12][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
    {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
    {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
    {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
    {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
    {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
    {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
    {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
    {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
};

[[nodiscard]] inline std::uint64_t rotateRight64(std::uint64_t value,
                                                 int count) noexcept {
  return (value >> count) | (value << (64 - count));
}

[[nodiscard]] inline std::uint64_t loadU64Le(const void *source) noexcept {
  std::uint64_t value = 0;
  std::memcpy(&value, source, sizeof(value));
  return value;
}

inline void mixG(std::uint64_t *work, int a, int b, int c, int d,
                 std::uint64_t x, std::uint64_t y) noexcept {
  work[a] += work[b] + x;
  work[d] = rotateRight64(work[d] ^ work[a], 32);
  work[c] += work[d];
  work[b] = rotateRight64(work[b] ^ work[c], 24);
  work[a] += work[b] + y;
  work[d] = rotateRight64(work[d] ^ work[a], 16);
  work[c] += work[d];
  work[b] = rotateRight64(work[b] ^ work[c], 63);
}

inline void compress(State &state, const std::uint8_t *block) noexcept {
  std::uint64_t work[16]{};
  std::uint64_t msg[16]{};

  for (int i = 0; i < 8; ++i) {
    work[i] = state.chain[i];
  }

  work[8] = initialVector[0];
  work[9] = initialVector[1];
  work[10] = initialVector[2];
  work[11] = initialVector[3];
  work[12] = initialVector[4] ^ state.count[0];
  work[13] = initialVector[5] ^ state.count[1];
  work[14] = initialVector[6] ^ state.last[0];
  work[15] = initialVector[7] ^ state.last[1];

  for (int i = 0; i < 16; ++i) {
    msg[i] = loadU64Le(block + i * 8);
  }

  for (int round = 0; round < 12; ++round) {
    const auto *schedule = sigma[round];
    mixG(work, 0, 4, 8, 12, msg[schedule[0]], msg[schedule[1]]);
    mixG(work, 1, 5, 9, 13, msg[schedule[2]], msg[schedule[3]]);
    mixG(work, 2, 6, 10, 14, msg[schedule[4]], msg[schedule[5]]);
    mixG(work, 3, 7, 11, 15, msg[schedule[6]], msg[schedule[7]]);
    mixG(work, 0, 5, 10, 15, msg[schedule[8]], msg[schedule[9]]);
    mixG(work, 1, 6, 11, 12, msg[schedule[10]], msg[schedule[11]]);
    mixG(work, 2, 7, 8, 13, msg[schedule[12]], msg[schedule[13]]);
    mixG(work, 3, 4, 9, 14, msg[schedule[14]], msg[schedule[15]]);
  }

  for (int i = 0; i < 8; ++i) {
    state.chain[i] ^= work[i] ^ work[i + 8];
  }
}

inline void start(State &state, std::size_t outSize, const void *key = nullptr,
                  std::size_t keySize = 0) {
  std::memset(&state, 0, sizeof(state));
  state.outSize = outSize;

  for (int i = 0; i < 8; ++i) {
    state.chain[i] = initialVector[i];
  }

  state.chain[0] ^= 0x01010000ULL ^
                    (static_cast<std::uint64_t>(keySize) << 8U) ^
                    static_cast<std::uint64_t>(outSize);

  if (key != nullptr && keySize > 0) {
    std::memset(state.buf, 0, sizeof(state.buf));
    std::memcpy(state.buf, key, keySize);
    state.bufSize = sizeof(state.buf);
  }
}

inline void absorb(State &state, const void *data, std::size_t size) {
  auto *input = static_cast<const std::uint8_t *>(data);

  while (size > 0) {
    if (state.bufSize == sizeof(state.buf)) {
      state.count[0] += sizeof(state.buf);
      if (state.count[0] < sizeof(state.buf)) {
        ++state.count[1];
      }
      compress(state, state.buf);
      state.bufSize = 0;
    }

    std::size_t writable = sizeof(state.buf) - state.bufSize;
    if (writable > size) {
      writable = size;
    }

    std::memcpy(state.buf + state.bufSize, input, writable);
    state.bufSize += writable;
    input += writable;
    size -= writable;
  }
}

inline void finish(State &state, void *output) {
  state.count[0] += static_cast<std::uint64_t>(state.bufSize);
  if (state.count[0] < state.bufSize) {
    ++state.count[1];
  }

  state.last[0] = ~static_cast<std::uint64_t>(0);
  std::memset(state.buf + state.bufSize, 0, sizeof(state.buf) - state.bufSize);
  compress(state, state.buf);

  auto *bytes = static_cast<std::uint8_t *>(output);
  for (std::size_t i = 0; i < state.outSize; ++i) {
    bytes[i] = static_cast<std::uint8_t>(state.chain[i / 8] >> (8 * (i % 8)));
  }
}

} // namespace detail

inline void digest(const void *data, std::size_t size, void *output,
                   std::size_t outSize, const void *key = nullptr,
                   std::size_t keySize = 0) {
  detail::State state;
  detail::start(state, outSize, key, keySize);
  detail::absorb(state, data, size);
  detail::finish(state, output);
}

} // namespace PhantomLedger::crypto::blake2b
