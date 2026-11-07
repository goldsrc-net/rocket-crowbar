#!/usr/bin/env python3
"""xrefs.py <so> <got_base_hex> <regex>: every function that calls (via PLT) or reads through the GOT
a symbol matching regex. Prints caller symbol, caller address, call address, target."""
import re, subprocess, sys
so, got, pat = sys.argv[1], int(sys.argv[2], 16), re.compile(sys.argv[3])
rel = {}
for line in subprocess.run(['objdump', '-R', so], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) >= 3 and re.fullmatch(r'[0-9a-f]{8}', p[0]): rel[int(p[0], 16)] = p[2].split('@')[0]
proc = subprocess.Popen(['objdump', '-d', '--no-show-raw-insn', so], stdout=subprocess.PIPE, text=True)
cur = None
for line in proc.stdout:
    m = re.match(r'([0-9a-f]+) <(.+)>:', line)
    if m: cur = (m.group(2), m.group(1)); continue
    m = re.match(r'\s*([0-9a-f]+):\s+(.*)', line)
    if not m or cur is None: continue
    ins = m.group(2)
    tgt = None
    mm = re.search(r'call\s+[0-9a-f]+ <([^@>]+)(@plt)?>', ins)
    if mm: tgt = mm.group(1)
    else:
        mm = re.search(r'(-?0x[0-9a-f]+)\(%ebx\)', ins)
        if mm and not ins.startswith('lea'):
            tgt = rel.get((got + int(mm.group(1), 16)) & 0xffffffff)
    if tgt and pat.search(tgt) and not cur[0].startswith(tgt):
        print(cur[0], cur[1], m.group(1), tgt)
