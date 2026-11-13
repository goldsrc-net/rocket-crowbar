#!/usr/bin/env bash
# package.sh — build the reRC patch for a stock Rocket Crowbar 1.9 install at the current commit:
# the i386 server through the buildchain image (scripts/build.sh), the Win32 one with clang-cl
# (scripts/build-win.sh), then scripts/assemble-dropin.sh lays out the package. CI builds the
# same binaries with its own jobs and runs assemble-dropin.sh on its artifacts.
#
# Usage: scripts/package.sh [--debug] [outdir]   (default dist/)
#   Release (default): reRC-<rev>, stripped, from build-i386 and build-win32-clangcl.
#   --debug: reRC-<rev>-debug from the Debug profile, not stripped, with PDBs, built apart in
#   build-i386-debug and build-win32-clangcl-debug. The Win32 Debug build needs xwin's debug CRT:
#     xwin --accept-license --arch x86 splat --include-debug-libs --output ~/.cache/xwin/splat-x86
# Refuses a tree with uncommitted changes, so a package is a commit.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEBUG=()
if [ "${1:-}" = "--debug" ]; then DEBUG=(--debug); shift; fi
OUT="${1:-$ROOT/dist}"
cd "$ROOT"

if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
  echo "ERROR: uncommitted changes; commit first so the package matches a commit" >&2
  git status --short --untracked-files=no >&2
  exit 1
fi
# The package name: the v* tag on a tagged commit, <tag>-N-g<hash> after it, the bare hash before
# any tag (the same describe cmake/Version.cmake stamps into the Windows version resource).
REV="$(git describe --tags --match 'v*' 2>/dev/null || git rev-parse --short HEAD)"

if [ ${#DEBUG[@]} -eq 0 ]; then
  LIN=build-i386; WIN=build-win32-clangcl
  scripts/build.sh i386 hl
  scripts/build-win.sh hl
else
  LIN=build-i386-debug; WIN=build-win32-clangcl-debug
  BUILDTYPE=Debug BUILD_DIR=$LIN scripts/build.sh i386 hl
  BUILDTYPE=Debug BUILD_DIR=$WIN scripts/build-win.sh hl
fi

scripts/assemble-dropin.sh "${DEBUG[@]}" "$REV" \
  $LIN/dlls/hl.so $WIN/dlls/hl.dll \
  "$OUT"
