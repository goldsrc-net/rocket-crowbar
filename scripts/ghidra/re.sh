#!/usr/bin/env bash
# re.sh - Ghidra front-end for the original rc_i386.so.
#
# Keeps one analyzed project at recovered/ghidra/rc (gitignored) so decompiles don't
# re-analyze the binary. A slimmed-down version of mod-ts's scripts/ghidra/re.sh: no
# ApplyInventory yet, so decompiles carry the gcc2 symbol names and, as a comment, the
# demangled prototype from reference/signatures-gcc2.txt.
#
#   scripts/ghidra/re.sh setup              import + analyze (noreturn discovery off); ~minutes
#   scripts/ghidra/re.sh export <filter>    decompile every function whose name contains
#                                           <filter> to recovered/decomp/<Class>.c
#   scripts/ghidra/re.sh export             everything
#   scripts/ghidra/re.sh at 0x<addr> ...    decompile by address (Ghidra addr = nm addr + 0x10000)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA="${GHIDRA_HOME:-$HOME/Tools/ghidra}/support/analyzeHeadless"
SCRIPTS="$ROOT/scripts/ghidra"
PROJDIR="$ROOT/recovered/ghidra"
PROJ=rc
BIN="$ROOT/reference/rc_i386.so"
SIGMAP="$ROOT/reference/signatures-gcc2.txt"
OUT="$ROOT/recovered/decomp"

cmd="${1:-}"; shift || true
case "$cmd" in
  setup)
    mkdir -p "$PROJDIR"
    rm -rf "${PROJDIR:?}/$PROJ.gpr" "${PROJDIR:?}/$PROJ.rep"
    "$GHIDRA" "$PROJDIR" "$PROJ" -import "$BIN" -scriptPath "$SCRIPTS" \
      -preScript DisableNoReturn.java
    ;;
  export)
    mkdir -p "$OUT"
    "$GHIDRA" "$PROJDIR" "$PROJ" -process "$(basename "$BIN")" -noanalysis -readOnly \
      -scriptPath "$SCRIPTS" -postScript DecompExport.java "$OUT" "${1:-}" "$SIGMAP" 2>&1 \
      | grep -E "^(INFO|ERROR|WARN).*(DecompExport|wrote|loaded|error)" || true
    ls "$OUT" | wc -l | sed 's/^/files in recovered\/decomp: /'
    ;;
  at)
    [ $# -gt 0 ] || { echo "usage: re.sh at 0x<addr> ..." >&2; exit 2; }
    "$GHIDRA" "$PROJDIR" "$PROJ" -process "$(basename "$BIN")" -noanalysis -readOnly \
      -scriptPath "$SCRIPTS" -postScript DecompAt.java "$@" 2>&1 \
      | sed -n '/DecompAt.java (HeadlessAnalyzer)/,/REPORT/p' | sed '1d;$d'
    ;;
  *)
    sed -n '2,16p' "$0"; exit 2 ;;
esac
