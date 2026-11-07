#!/usr/bin/env python3
"""schedcmp.py <orig.so> <ours.so> [name-regex]: compare every sl* Schedule_t table (and the Task_t
list each points to) by symbol between the original i386 library and our i386 build. Prints the
ones that differ: task ids/data, interrupt mask, sound mask, name. i386 builds only."""
import subprocess, struct, sys, re
def syms(so):
    out = subprocess.run(["nm", "-n", so], capture_output=True, text=True).stdout.split("\n")
    lst = [(int(p[0], 16), re.sub(r"^_ZL\d+", "", p[2])) for p in (l.split() for l in out) if len(p) == 3 and p[1] in "dD"]
    return {n: (a, (lst[i + 1][0] if i + 1 < len(lst) else a + 64) - a) for i, (a, n) in enumerate(lst)}
def reader(so):
    d = open(so, "rb").read()
    out = subprocess.run(["readelf", "-SW", so], capture_output=True, text=True).stdout
    secs = [(m.group(1), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16))
            for m in re.finditer(r"\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", out)]
    rel = {}
    for l in subprocess.run(["readelf", "-rW", so], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) >= 4 and p[2] == "R_386_32":
            rel[int(p[0], 16)] = int(p[3], 16)
    def rd(addr, n):
        for name, va, off, size in secs:
            if va <= addr < va + size and name != ".bss" and va:
                b = bytearray(d[off + addr - va: off + addr - va + n])
                for k in range(0, n - 3):
                    if addr + k in rel:
                        v = struct.unpack("<I", b[k:k + 4])[0] + rel[addr + k]
                        b[k:k + 4] = struct.pack("<I", v & 0xffffffff)
                return bytes(b)
        return None
    def cstr(addr):
        b = rd(addr, 80) or b""
        return b.split(b"\0")[0].decode("latin1")
    return rd, cstr
def scheds(so, n, size):
    rd, cstr = reader(so)
    out = []
    a = n
    for i in range(size // 20):
        tp, cnt, intr, snd, nm = struct.unpack("<IiiiI", rd(a + i * 20, 20))
        if cnt <= 0 or cnt > 64: break
        tb = rd(tp, cnt * 8)
        if tb is None: out.append(("?ptr", hex(tp))); break
        tasks = [struct.unpack("<if", tb[j:j + 8]) for j in range(0, cnt * 8, 8)]
        out.append((cstr(nm), [(t, round(f, 3)) for t, f in tasks], hex(intr & 0xffffffff), hex(snd & 0xffffffff)))
    return out
o, u = sys.argv[1], sys.argv[2]
pat = re.compile(sys.argv[3] if len(sys.argv) > 3 else ".")
so, su = syms(o), syms(u)
for n in sorted(so):
    if not re.match(r"^sl[A-Z0-9]", n) or not pat.search(n): continue
    if n not in su: print("ONLY-ORIG", n); continue
    a = scheds(o, so[n][0], so[n][1]); b = scheds(u, su[n][0], su[n][1])
    if a != b:
        print(n)
        for x, y in zip(a, b):
            if x != y: print("  orig", x, "\n  ours", y)
        if len(a) != len(b): print("  count", len(a), len(b))
for n in sorted(su):
    if re.match(r"^sl[A-Z0-9]", n) and n not in so and pat.search(n): print("ONLY-OURS", n)
