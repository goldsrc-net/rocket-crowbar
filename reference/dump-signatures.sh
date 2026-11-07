#!/usr/bin/env bash
# dump-signatures.sh - extract recoverable C++ signatures from the original
# Rocket Crowbar 1.9 server libraries, to seed reconstruction of the source.
# Same method as mod-ts/reference/dump-signatures.sh.
#
# Symbol sources next to this script:
#   rc.dll        Windows MSVC server dll. Its exports are the SDK's save/restore
#                 callbacks (think/touch/use) and the LINK_ENTITY_TO_CLASS
#                 factories; the mangled ones demangle to fully typed signatures.
#   rc_i386.so    Linux server dll, egcs 1.1.2 / gcc 2.95, NOT stripped: the
#                 .symtab is the complete function inventory with addresses.
#                 gcc 2.x names are demangled by ../scripts/gnv2demangle.py.
#
# Output (written next to this script):
#   exports-rc.raw        raw mangled rc.dll exports
#   signatures-rc.txt     demangled rc.dll signatures
#   entities-rc.txt       unmangled rc.dll exports (entity classnames + engine API)
#   symbols-gcc2.txt      addr  name, every defined function in rc_i386.so
#   signatures-gcc2.txt   name  =>  demangled, for the same functions
#   classes.txt           distinct class names with method counts (both libs)
set -euo pipefail
cd "$(dirname "$0")"

DLL=rc.dll
SO=rc_i386.so

UNDNAME=""
for c in llvm-undname llvm-undname-21 llvm-undname-20 llvm-undname-19 llvm-undname-18; do
  if command -v "$c" >/dev/null 2>&1; then UNDNAME="$c"; break; fi
done
[ -n "$UNDNAME" ] || { echo "ERROR: no llvm-undname found (apt install llvm)"; exit 1; }

export LC_ALL=C

echo ">> $DLL exports"
objdump -p "$DLL" | sed -n '/\[Ordinal\/Name Pointer\] Table/,/^$/p' \
  | awk 'NR>1 && NF {print $NF}' > exports-all.tmp
grep '^?' exports-all.tmp | sort -u > exports-rc.raw
grep -v '^?' exports-all.tmp | sort -u > entities-rc.txt
rm -f exports-all.tmp
"$UNDNAME" < exports-rc.raw > signatures-rc.txt 2>/dev/null || true
echo "   $(wc -l < exports-rc.raw) mangled, $(wc -l < entities-rc.txt) unmangled"

echo ">> $SO symtab"
nm "$SO" | awk '$2 ~ /^[tT]$/ {print $1"  "$3}' | sort > symbols-gcc2.txt
python3 -I ../scripts/gnv2demangle.py < symbols-gcc2.txt > signatures-gcc2.txt
echo "   $(wc -l < symbols-gcc2.txt) defined functions"

echo ">> class inventory"
cat signatures-rc.txt signatures-gcc2.txt \
  | grep -oE '[A-Za-z_][A-Za-z0-9_]*::' | sed 's/:://' \
  | sort | uniq -c | sort -rn > classes.txt
echo "   $(wc -l < classes.txt) distinct classes"
