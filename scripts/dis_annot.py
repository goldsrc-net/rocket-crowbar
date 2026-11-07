#!/usr/bin/env python3
"""dis.py <so> <start> <stop>: objdump a range of a gcc2 PIC i386 .so and annotate
# Written by the houndeye-agrunt worker; scripts/annot.py does much the same.
-0xNN(%ebx) GOT-relative operands with the string / float / double / pointer they reach."""
import re, struct, subprocess, sys
so, start, stop = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
data = open(so, 'rb').read()
secs = []
for l in subprocess.run(['readelf', '-SW', so], capture_output=True, text=True).stdout.splitlines():
    m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', l)
    if m:
        secs.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)))
def rd(va, n):
    for name, a, off, sz in secs:
        if a <= va < a + sz and name != '.bss':
            return data[off + va - a: off + va - a + n]
    return None
def desc(va):
    b = rd(va, 64)
    if b is None:
        return 'bss/unmapped %#x' % va
    s = b.split(b'\0')[0]
    out = []
    if len(s) >= 2 and all(32 <= c < 127 or c in (9, 10) for c in s):
        out.append(repr(s.decode()))
    f = struct.unpack('<f', b[:4])[0]; d = struct.unpack('<d', b[:8])[0]; i = struct.unpack('<I', b[:4])[0]
    out.append('f=%g d=%g i=%#x' % (f, d, i))
    return ' '.join(out)
lines = subprocess.run(['objdump', '-d', '--no-show-raw-insn', '-C', '--start-address=%#x' % start,
                        '--stop-address=%#x' % stop, so], capture_output=True, text=True).stdout.splitlines()
got = None; pend = None
for l in lines:
    m = re.match(r'\s*([0-9a-f]+):\s+(.*)', l)
    if not m:
        print(l); continue
    addr, ins = int(m.group(1), 16), m.group(2)
    mc = re.match(r'call\s+([0-9a-f]+)\s', ins)
    if mc and int(mc.group(1), 16) == addr + 5:
        pend = addr + 5
    ma = re.match(r'add\s+\$0x([0-9a-f]+),%ebx', ins)
    if ma and pend is not None:
        got = pend + int(ma.group(1), 16); pend = None
    note = ''
    if got is not None:
        for mo in re.finditer(r'(-?)0x([0-9a-f]+)\(%ebx\)', ins):
            off = int(mo.group(2), 16) * (-1 if mo.group(1) else 1)
            va = got + off
            if off < 0:
                note += '  ; [%#x] %s' % (va, desc(va))
            else:
                p = rd(va, 4)
                note += '  ; GOT[%#x]' % va + (' -> %#x' % struct.unpack('<I', p)[0] if p else '')
    print('%x: %s%s' % (addr, ins, note))
