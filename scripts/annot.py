#!/usr/bin/env python3
"""annot.py <so> <start> <stop>: objdump a PIC i386 function and annotate %ebx-relative
strings, GOT symbols and float/double constants."""
import re, struct, subprocess, sys
so, a, b = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
data = open(so, 'rb').read()
phoff = struct.unpack_from('<I', data, 0x1c)[0]; n = struct.unpack_from('<H', data, 0x2c)[0]
segs = []
for i in range(n):
    p = struct.unpack_from('<8I', data, phoff + i * 32)
    if p[0] == 1: segs.append((p[2], p[1], p[4]))
def off(va):
    for v, o, s in segs:
        if v <= va < v + s: return va - v + o
def cstr(va):
    o = off(va)
    if o is None: return None
    e = data.find(b'\0', o)
    s = data[o:e]
    if len(s) < 1 or any(c < 9 or c > 126 for c in s[:40]): return None
    return s.decode('latin1')
rel = {}
for line in subprocess.run(['objdump', '-R', so], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) >= 3 and re.fullmatch(r'[0-9a-f]{8}', p[0]): rel[int(p[0], 16)] = p[2]
syms = {}
for line in subprocess.run(['nm', so], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3: syms[int(p[0], 16)] = p[2]
out = subprocess.run(['objdump', '-d', '--no-show-raw-insn', '-C', f'--start-address={a:#x}', f'--stop-address={b:#x}', so], capture_output=True, text=True).stdout.splitlines()
ebx = None
prev = None
for line in out:
    m = re.match(r'\s*([0-9a-f]+):\s+(.*)', line)
    if not m:
        print(line); continue
    addr, ins = int(m.group(1), 16), m.group(2)
    if ins.strip() == 'nop': continue
    mm = re.match(r'add\s+\$0x([0-9a-f]+),%ebx', ins)
    if mm and prev and prev[1].startswith('pop') and '%ebx' in prev[1]:
        ebx = prev[0] + int(mm.group(1), 16)
    note = ''
    if ebx is not None:
        for mm in re.finditer(r'(-?0x[0-9a-f]+)\(%ebx\)', ins):
            o = int(mm.group(1), 16); va = (ebx + o) & 0xffffffff
            if va in rel: note += f' [GOT {rel[va]}]'
            else:
                s = cstr(va)
                if s is not None and ins.startswith('lea'): note += ' "' + s[:200].encode('unicode_escape').decode().replace('"', '\\"') + '"'
                else:
                    fo = off(va)
                    if fo is not None and fo + 8 <= len(data):
                        f = struct.unpack_from('<f', data, fo)[0]; d = struct.unpack_from('<d', data, fo)[0]
                        nm = syms.get(va, '')
                        note += f' [{nm} f={f:g} d={d:g}]'
    print(f'{addr:x}: {ins}{("   ;" + note) if note else ""}')
    prev = (addr, ins)
