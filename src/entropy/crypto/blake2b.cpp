#include "phantomledger/entropy/crypto/blake2b.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

namespace PhantomLedger::crypto::blake2b {
namespace {

constexpr std::size_t kBlockBytes = 128;
constexpr std::size_t kMaxDigestBytes = 64;
constexpr std::size_t kMaxKeyBytes = 64;
constexpr std::size_t kRounds = 12;

struct State {
  std::array<std::uint64_t, 8> chain{};
  std::array<std::uint64_t, 2> count{};
  std::array<std::uint64_t, 2> last{};
  std::array<std::uint8_t, kBlockBytes> buf{};
  std::size_t bufSize = 0;
  std::size_t outSize = 0;
};

constexpr std::array<std::uint64_t, 8> kInitialVector{
    0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL, 0x3C6EF372FE94F82BULL,
    0xA54FF53A5F1D36F1ULL, 0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL,
    0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL,
};

constexpr std::array<std::array<std::uint8_t, 16>, kRounds> kSigma{{
    {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    {{14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3}},
    {{11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4}},
    {{7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8}},
    {{9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13}},
    {{2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9}},
    {{12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11}},
    {{13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10}},
    {{6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5}},
    {{10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0}},
    {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    {{14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3}},
}};

[[nodiscard]] std::uint64_t rotr64(std::uint64_t value, int count) noexcept {
  return std::rotr(value, count);
}

[[nodiscard]] std::uint64_t loadU64Le(const void *source) noexcept {
  std::uint64_t value = 0;
  std::memcpy(&value, source, sizeof(value));
  if constexpr (std::endian::native == std::endian::big) {
    value = std::byteswap(value);
  }
  return value;
}

void storeU64Le(void *dest, std::uint64_t value) noexcept {
  if constexpr (std::endian::native == std::endian::big) {
    value = std::byteswap(value);
  }
  std::memcpy(dest, &value, sizeof(value));
}

void secureZero(void *ptr, std::size_t size) noexcept {
  auto *bytes = static_cast<volatile std::uint8_t *>(ptr);
  while (size-- > 0) {
    *bytes++ = 0;
  }
}

void addCount(State &state, std::uint64_t amount) noexcept {
  const auto old = state.count[0];
  state.count[0] += amount;
  if (state.count[0] < old) {
    ++state.count[1];
  }
}

void mixG(std::array<std::uint64_t, 16> &work, int a, int b, int c, int d,
          std::uint64_t x, std::uint64_t y) noexcept {
  work[a] += work[b] + x;
  work[d] = rotr64(work[d] ^ work[a], 32);
  work[c] += work[d];
  work[b] = rotr64(work[b] ^ work[c], 24);
  work[a] += work[b] + y;
  work[d] = rotr64(work[d] ^ work[a], 16);
  work[c] += work[d];
  work[b] = rotr64(work[b] ^ work[c], 63);
}

void compress(State &state, const std::uint8_t *block) noexcept {
  std::array<std::uint64_t, 16> work{};
  std::array<std::uint64_t, 16> msg{};

  for (int i = 0; i < 8; ++i) {
    work[i] = state.chain[i];
  }

  work[8] = kInitialVector[0];
  work[9] = kInitialVector[1];
  work[10] = kInitialVector[2];
  work[11] = kInitialVector[3];
  work[12] = kInitialVector[4] ^ state.count[0];
  work[13] = kInitialVector[5] ^ state.count[1];
  work[14] = kInitialVector[6] ^ state.last[0];
  work[15] = kInitialVector[7] ^ state.last[1];

  for (int i = 0; i < 16; ++i) {
    msg[i] = loadU64Le(block + i * 8);
  }

  for (int round = 0; round < static_cast<int>(kRounds); ++round) {
    const auto &schedule = kSigma[round];

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

[[nodiscard]] bool start(State &state, std::size_t outSize, const void *key,
                         std::size_t keySize) noexcept {
  if (outSize == 0 || outSize > kMaxDigestBytes) {
    return false;
  }
  if (keySize > kMaxKeyBytes) {
    return false;
  }
  if (keySize > 0 && key == nullptr) {
    return false;
  }

  state = State{};
  state.outSize = outSize;
  state.chain = kInitialVector;

  state.chain[0] ^= 0x01010000ULL ^
                    (static_cast<std::uint64_t>(keySize) << 8U) ^
                    static_cast<std::uint64_t>(outSize);

  if (keySize > 0) {
    std::fill(state.buf.begin(), state.buf.end(), std::uint8_t{0});
    std::memcpy(state.buf.data(), key, keySize);
    state.bufSize = kBlockBytes;
  }

  return true;
}

[[nodiscard]] bool absorb(State &state, const void *data,
                          std::size_t size) noexcept {
  if (size > 0 && data == nullptr) {
    return false;
  }

  auto *input = static_cast<const std::uint8_t *>(data);

  while (size > 0) {
    if (state.bufSize == kBlockBytes) {
      addCount(state, kBlockBytes);
      compress(state, state.buf.data());
      state.bufSize = 0;
    }

    const auto writable = std::min(kBlockBytes - state.bufSize, size);
    std::memcpy(state.buf.data() + state.bufSize, input, writable);
    state.bufSize += writable;
    input += writable;
    size -= writable;
  }

  return true;
}

[[nodiscard]] bool finish(State &state, void *output) noexcept {
  if (state.outSize > 0 && output == nullptr) {
    return false;
  }

  addCount(state, static_cast<std::uint64_t>(state.bufSize));
  state.last[0] = ~static_cast<std::uint64_t>(0);
  state.last[1] = 0;

  std::fill(state.buf.begin() + static_cast<std::ptrdiff_t>(state.bufSize),
            state.buf.end(), std::uint8_t{0});
  compress(state, state.buf.data());

  auto *bytes = static_cast<std::uint8_t *>(output);

  const auto fullWords = state.outSize / 8;
  const auto tailBytes = state.outSize % 8;

  for (std::size_t i = 0; i < fullWords; ++i) {
    storeU64Le(bytes + i * 8, state.chain[i]);
  }

  if (tailBytes > 0) {
    std::array<std::uint8_t, 8> tail{};
    storeU64Le(tail.data(), state.chain[fullWords]);
    std::memcpy(bytes + fullWords * 8, tail.data(), tailBytes);
  }

  return true;
}

} // namespace

bool digest(const void *data, std::size_t size, void *output,
            std::size_t outSize, const void *key,
            std::size_t keySize) noexcept {
  State state{};

  if (!start(state, outSize, key, keySize)) {
    return false;
  }

  if (!absorb(state, data, size)) {
    secureZero(&state, sizeof(state));
    return false;
  }

  const bool ok = finish(state, output);
  secureZero(&state, sizeof(state));
  return ok;
}

} // namespace PhantomLedger::crypto::blake2b
