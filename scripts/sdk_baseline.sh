#!/usr/bin/env bash
# sdk_baseline.sh - record what the untouched SDK has, for scripts/inventory.py.
#
# inventory.py decides whether an original function is RC's own or stock SDK by asking "does
# the SDK have Class::method?". That has to be the pristine SDK at the fork point, not this
# tree, or every function we reconstruct starts counting as stock. This builds the fork point
# (upstream/64bit-demo) for i386 in a throwaway worktree and writes:
#
#   reference/sdk-stock-methods.txt   every defined Class::method / free function name
#   reference/sdk-stock-files.txt     every SDK source file stem (dlls, pm_shared, game_shared, ...)
#   reference/sdk-stock-entities.txt  every entity factory (unmangled .dynsym function)
#
# Re-run only when the fork point changes. Usage: scripts/sdk_baseline.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REF="${REF:-upstream/64bit-demo}"
WT="$(mktemp -d "${TMPDIR:-/tmp}/sdk-baseline.XXXXXX")"
trap 'git -C "$ROOT" worktree remove --force "$WT" 2>/dev/null || true' EXIT

git -C "$ROOT" worktree add -q --detach "$WT" "$REF"
mkdir -p "$WT/scripts"
sed 's/mod-rockcrowbar/sdk-baseline/g' "$ROOT/scripts/build.sh" > "$WT/scripts/build.sh"
chmod +x "$WT/scripts/build.sh"
"$WT/scripts/build.sh" i386 >/dev/null

export LC_ALL=C
nm -C --defined-only "$WT/build-i386/dlls/hl.so" | awk '{ $1=""; $2=""; sub(/^  /, ""); print }' \
  | sed 's/(.*//' | grep -v -E '^(non-virtual thunk|virtual thunk|typeinfo|vtable|guard variable)' \
  | sort -u > "$ROOT/reference/sdk-stock-methods.txt"
nm -D --defined-only "$WT/build-i386/dlls/hl.so" | awk '$2 ~ /^[TW]$/ && $3 !~ /^_/ && $3 !~ /__/ {print $3}' \
  | sort -u > "$ROOT/reference/sdk-stock-entities.txt"
git -C "$ROOT" ls-tree -r --name-only "$REF" -- dlls pm_shared game_shared common engine \
  | grep -E '\.(cpp|c)$' | sed 's|.*/||; s/\.[^.]*$//' | tr 'A-Z' 'a-z' | sort -u > "$ROOT/reference/sdk-stock-files.txt"
echo "$(wc -l < "$ROOT/reference/sdk-stock-methods.txt") methods, $(wc -l < "$ROOT/reference/sdk-stock-files.txt") files ($REF $(git -C "$ROOT" rev-parse --short "$REF"))"
