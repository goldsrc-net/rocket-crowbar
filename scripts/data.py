#!/usr/bin/env python3
"""data.py <so> <addr> <fmt>...: dump data at a vaddr. fmt chars: s=char* (resolved string), f=float, i=int.
e.g. data.py rc_i386.so 28a6c0 sss"""
import re, struct, subprocess, sys
so = sys.argv[1]; va = int(sys.argv[2], 16); fmt = ''.join(sys.argv[3:])
d = open(so, 'rb').read(); secs = []
for l in subprocess.run(['readelf', '-SW', so], capture_output=True, text=True).stdout.splitlines():
    m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', l)
    if m: secs.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)))
def rd(a, n):
    for nm, s, o, z in secs:
        if s <= a < s + z and nm != '.bss': return d[o + a - s:o + a - s + n]
    return b'\0' * n
out = []
for c in fmt:
    b = rd(va, 4); va += 4
    if c == 's':
        p = struct.unpack('<I', b)[0]; out.append(repr(rd(p, 128).split(b'\0')[0].decode(errors='replace')) if p else 'NULL')
    elif c == 'f': out.append('%g' % struct.unpack('<f', b)[0])
    else: out.append('%#x' % struct.unpack('<I', b)[0])
print(', '.join(out))
