#!/bin/sh
# dis.sh <Class::Method or free name>: disassemble one original function (file addresses)
R="$(cd "$(dirname "$0")/.." && pwd)/reference"
cls=${1%%::*}; m=${1##*::}; [ "$cls" = "$1" ] && cls=""
awk -F'\t' -v c="$cls" -v m="$m" '$4==c && $5==m {print $1, $2}' $R/funcfile.tsv | while read a s; do
  e=$(printf '%x' $((0x$a + s)))
  objdump -d --no-show-raw-insn -C --start-address=0x$a --stop-address=0x$e $R/rc_i386.so | tail -n +7
done
