#!/bin/sh
# str.sh <ghidra addr hex>...: print the C string at each Ghidra address (file addr + 0x10000)
# Written by the curses-misc worker. For a whole function with its strings and constants, scripts/annot.py.
RC_SO="$(cd "$(dirname "$0")/.." && pwd)/reference/rc_i386.so"; export RC_SO
for g in "$@"; do
python3 - "$g" <<'P'
import sys
import os; R=os.environ['RC_SO']
a=int(sys.argv[1],16)-0x10000
d=open(R,'rb').read()
# rodata/data: file offset = vaddr for rodata; data vaddr-0x1000
off = a if a < 0x27c0c0 else a-0x1000
print(sys.argv[1], repr(d[off:d.index(b'\0',off)].decode('latin1')))
P
done
