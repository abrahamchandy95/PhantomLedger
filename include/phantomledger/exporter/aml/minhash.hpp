#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PhantomLedger::exporter::aml::minhash {

inline constexpr std::uint32_t kMh2M = 0x5BD1E995U;
inline constexpr std::uint32_t kMh2R = 24U;

// ────────── Universal-hash-family parameters ──────────

inline constexpr std::uint64_t kUhP = 4'294'967'311ULL;

inline constexpr std::size_t kShingleK = 3;
inline constexpr std::size_t kBandB = 10; // 10 bands × r=1 → 10 sigs
inline constexpr std::size_t kBandR = 1;
inline constexpr std::size_t kSignatureLen = kBandB * kBandR;

static_assert(kSignatureLen <= 100, "b*r must be <= 100");

extern const std::array<std::uint32_t, 101> kUhC1;
extern const std::array<std::uint32_t, 101> kUhC2;

// ────────── Public API ──────────

[[nodiscard]] std::uint32_t murmurhash2(std::span<const std::uint8_t> data,
                                        std::uint32_t seed = 0) noexcept;

[[nodiscard]] std::vector<std::uint32_t>
shingleHashes(std::span<const std::uint8_t> data, std::size_t k);

[[nodiscard]] std::array<std::uint32_t, kSignatureLen>
signature(std::string_view text);

[[nodiscard]] std::vector<std::string>
bucketIds(std::string_view prefix,
          std::array<std::uint32_t, kSignatureLen> signature);

[[nodiscard]] std::vector<std::string>
nameMinhashIds(std::string_view firstName, std::string_view lastName);

[[nodiscard]] std::vector<std::string>
addressMinhashIds(std::string_view fullAddress);

[[nodiscard]] std::vector<std::string>
streetMinhashIds(std::string_view streetLine1);

[[nodiscard]] std::string cityMinhashId(std::string_view city);

[[nodiscard]] std::string stateMinhashId(std::string_view state);

} // namespace PhantomLedger::exporter::aml::minhash
