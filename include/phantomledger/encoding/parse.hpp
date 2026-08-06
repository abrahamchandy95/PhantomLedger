#pragma once

/*
  Reverse of `encoding::format` for ledger-rendered entity keys: prefix
  lookup over the SAME identity-layout table `render.hpp` writes from, so the
  two directions cannot drift.

  Unambiguous by construction: every layout prefix is purely alphabetic and
  the rendered remainder is purely numeric, so if a longer prefix matches,
  any shorter prefix of it leaves a letter at the front of the digit run and
  is rejected. At most one table entry can accept a given rendered id.

  This is the decode half of the PostgreSQL read-back contract: the streamed
  `transactions` table stores src_acct/dst_acct exactly as `format()`
  rendered them, and the derived-analytics pass needs the structured key back
  (bank internal-ness, role, account number).
 */

#include "phantomledger/encoding/lookup.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <charconv>
#include <optional>
#include <string_view>

namespace PhantomLedger::encoding {

[[nodiscard]] inline std::optional<entity::Key>
parseKey(std::string_view rendered) {
  const detail::IdentityLayoutEntry *match = nullptr;

  for (const auto &entry : detail::kIdentityLayoutEntries) {
    const auto prefix = entry.layout.prefix;
    if (rendered.size() <= prefix.size() ||
        rendered.substr(0, prefix.size()) != prefix) {
      continue;
    }

    const auto digits = rendered.substr(prefix.size());
    bool allDigits = true;
    for (const char c : digits) {
      if (c < '0' || c > '9') {
        allDigits = false;
        break;
      }
    }
    if (!allDigits) {
      continue;
    }

    /* Provably unique (see the header comment); the longest-prefix rule is
     * belt-and-braces against future layout-table edits. */
    if (match == nullptr || prefix.size() > match->layout.prefix.size()) {
      match = &entry;
    }
  }

  if (match == nullptr) {
    return std::nullopt;
  }

  const auto digits = rendered.substr(match->layout.prefix.size());
  std::uint64_t number = 0;
  const auto [end, ec] =
      std::from_chars(digits.data(), digits.data() + digits.size(), number);
  if (ec != std::errc{} || end != digits.data() + digits.size()) {
    return std::nullopt;
  }
  if (number == 0 && !match->layout.allowZero) {
    return std::nullopt;
  }

  return entity::makeKey(match->role, match->bank, number);
}

} // namespace PhantomLedger::encoding
