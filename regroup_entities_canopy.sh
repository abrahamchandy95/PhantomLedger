#!/usr/bin/env bash
#
# regroup_entities_canopy.sh — canopy round: group the loose
# include/phantomledger/entities root headers by responsibility.
#
#   parties/         people, pii, identity, behaviors
#   holdings/        accounts, cards            (products/ stays a sibling)
#   counterparties/  merchants, landlords, directory (was counterparties.hpp,
#                    renamed so the path does not stutter),
#                    institutional_accounts
#   identifiers.hpp  stays at the entities root (the shared vocabulary)
#   infra/, products/ unchanged
#
# The entities layer is header-only, so this is git mv + include-path
# rewrites; no CMake source lists move and the layer lint's path mapper
# keys on the top-level layer directory only. Verified before writing
# this script: the old paths appear ONLY in .hpp/.cpp files under
# include/, src/ and tests/ — no cmake/, docs/ or README references.
#
# Owner runs ONCE from the repo root, then: make test (zero golden
# movement), graphify update ., git add -A, commit, delete this script.
# Nothing is committed by the script; `git reset --hard HEAD` undoes it.

set -euo pipefail

fail() {
  echo "regroup_entities_canopy: $*" >&2
  exit 1
}

[ -f CMakeLists.txt ] || fail "run from the repository root"
[ -d include/phantomledger/entities ] || fail "entities tree not found"

# old-include-path new-include-path (relative to include/)
PAIRS='
phantomledger/entities/people.hpp phantomledger/entities/parties/people.hpp
phantomledger/entities/pii.hpp phantomledger/entities/parties/pii.hpp
phantomledger/entities/identity.hpp phantomledger/entities/parties/identity.hpp
phantomledger/entities/behaviors.hpp phantomledger/entities/parties/behaviors.hpp
phantomledger/entities/accounts.hpp phantomledger/entities/holdings/accounts.hpp
phantomledger/entities/cards.hpp phantomledger/entities/holdings/cards.hpp
phantomledger/entities/merchants.hpp phantomledger/entities/counterparties/merchants.hpp
phantomledger/entities/landlords.hpp phantomledger/entities/counterparties/landlords.hpp
phantomledger/entities/counterparties.hpp phantomledger/entities/counterparties/directory.hpp
phantomledger/entities/institutional_accounts.hpp phantomledger/entities/counterparties/institutional_accounts.hpp
'

# ---- pre-flight: every source present and tracked, no destination taken
echo "$PAIRS" | while read -r old new; do
  [ -z "$old" ] && continue
  [ -f "include/$old" ] || fail "missing include/$old (already migrated?)"
  git ls-files --error-unmatch "include/$old" >/dev/null 2>&1 ||
    fail "include/$old is not tracked by git"
  [ -e "include/$new" ] && fail "include/$new already exists"
done

mkdir -p include/phantomledger/entities/parties \
         include/phantomledger/entities/holdings \
         include/phantomledger/entities/counterparties

# ---- moves
echo "$PAIRS" | while read -r old new; do
  [ -z "$old" ] && continue
  git mv "include/$old" "include/$new"
  echo "moved   $old -> $new"
done

# ---- include-path rewrites (tracked .hpp/.cpp under include/, src/, tests/)
echo "$PAIRS" | while read -r old new; do
  [ -z "$old" ] && continue
  files=$(git grep -lF "$old" -- include src tests || true)
  if [ -n "$files" ]; then
    count=$(echo "$files" | wc -l | tr -d ' ')
    # shellcheck disable=SC2086
    OLD="$old" NEW="$new" \
      perl -pi -e 's/\Q$ENV{OLD}\E/$ENV{NEW}/g' $files
    echo "rewrote $old in $count file(s)"
  else
    echo "rewrote $old in 0 files"
  fi
done

# ---- verify: no old path survives anywhere
echo "$PAIRS" | while read -r old new; do
  [ -z "$old" ] && continue
  if git grep -nF "$old" -- include src tests; then
    fail "stale reference to $old survives (see above); git reset --hard HEAD to undo"
  fi
done

# ---- verify: every remaining entities include points at a known home
stray=$(git grep -nF 'phantomledger/entities/' -- include src tests |
  grep -vE 'phantomledger/entities/(identifiers\.hpp|infra/|products/|parties/|holdings/|counterparties/)' || true)
if [ -n "$stray" ]; then
  echo "$stray" >&2
  fail "unexpected entities path(s) above; git reset --hard HEAD to undo"
fi

echo
echo "entities canopy in place:"
git ls-files include/phantomledger/entities | sed 's/^/  /'
echo
echo "next: make test            # zero golden movement expected"
echo "      graphify update ."
echo "      git add -A && git commit"
echo "      rm regroup_entities_canopy.sh"
