#pragma once

#include <cstddef>

namespace PhantomLedger::crypto::blake2b {

[[nodiscard]] bool digest(const void *data, std::size_t size, void *output,
                          std::size_t outSize, const void *key = nullptr,
                          std::size_t keySize = 0) noexcept;

} // namespace PhantomLedger::crypto::blake2b
