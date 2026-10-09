#!/usr/bin/env bash
# Frozen-migration gate that needs no build: hashes every R"sql( ... )sql" literal of the migrator and compares the
# result, in order, with kShippedMigrationHashes in the migrator test (the CTest case SchemaHistoryTest repeats this
# with SecureStorage::sha256Hex once the tree builds). A shipped migration is never edited; fix mistakes with a NEW
# migration and append its hash.
#
#   scripts/check-migration-hashes.sh [MIGRATOR_CPP [MIGRATOR_TEST_CPP]]     (defaults: the real files under Persistence)
#
# Line endings are normalized to LF before hashing (a CRLF checkout must not change the hash).
set -uo pipefail
cd "$(dirname "$0")/.."
SRC="${1:-Modules/Core/Persistence/src/migrator.cpp}"
TEST="${2:-Modules/Core/Persistence/tests/migrator_test.cpp}"
[ -f "$SRC" ] && [ -f "$TEST" ] || { echo "::error::[migration-hash] missing $SRC or $TEST"; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# Literals, exactly as the compiler sees the raw string: everything between `R"sql(` and `)sql"`, newlines included.
tr -d '\r' < "$SRC" | awk -v dir="$tmp" '
  !open && index($0, "R\"sql(") { n++; open = 1; rest = substr($0, index($0, "R\"sql(") + 6); buf = rest "\n"
    if (index(rest, ")sql\"")) { buf = substr(rest, 1, index(rest, ")sql\"") - 1); printf "%s", buf > (dir "/m" n); close(dir "/m" n); open = 0 }
    next }
  open { if (index($0, ")sql\"")) { printf "%s%s", buf, substr($0, 1, index($0, ")sql\"") - 1) > (dir "/m" n); close(dir "/m" n); open = 0 }
         else buf = buf $0 "\n"; next }
  END { print n + 0 > (dir "/count") }'
count=$(cat "$tmp/count")
[ "$count" -gt 0 ] || { echo "::error::[migration-hash] no R\"sql( literal found in $SRC"; exit 1; }

actual=(); for i in $(seq 1 "$count"); do actual+=("$(sha256sum "$tmp/m$i" | cut -d' ' -f1)"); done

block=$(tr -d '\r' < "$TEST" | awk '/kShippedMigrationHashes\{/ {on=1} on {print} on && /^\};/ {exit}')
[ -n "$block" ] || { echo "::error::[migration-hash] kShippedMigrationHashes not found in $TEST"; exit 1; }
mapfile -t recorded < <(printf '%s\n' "$block" | grep -oE '"[0-9a-f]{64}"' | tr -d '"')
declared=$(printf '%s\n' "$block" | grep -oE 'std::array<std::string_view, *[0-9]+>' | grep -oE '[0-9]+')

status=0
if [ "${declared:-}" != "${#recorded[@]}" ]; then
  echo "::error::[migration-hash] kShippedMigrationHashes declares size ${declared:-?} but lists ${#recorded[@]} hashes"; status=1
fi
if [ "${#recorded[@]}" -lt "$count" ]; then
  echo "::error::[migration-hash] $count migrations in $SRC but ${#recorded[@]} hashes recorded: append the SHA-256 of the new migration to kShippedMigrationHashes"
  for i in $(seq "${#recorded[@]}" $((count - 1))); do echo "  migration $((i + 1)): ${actual[$i]}"; done
  status=1
fi
if [ "${#recorded[@]}" -gt "$count" ]; then
  echo "::error::[migration-hash] ${#recorded[@]} hashes recorded but only $count migrations remain: a shipped migration was removed"; status=1
fi
limit=$(( count < ${#recorded[@]} ? count : ${#recorded[@]} ))
for i in $(seq 0 $((limit - 1))); do
  if [ "${actual[$i]}" != "${recorded[$i]}" ]; then
    echo "::error::[migration-hash] migration $((i + 1)) was edited after it shipped (add a new migration instead)"
    echo "  recorded ${recorded[$i]}"; echo "  actual   ${actual[$i]}"; status=1
  fi
done
[ "$status" -eq 0 ] && echo "OK: $count migration hashes match kShippedMigrationHashes."
exit "$status"
