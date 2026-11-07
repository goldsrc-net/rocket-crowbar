#!/usr/bin/env python3
"""vtables.py - every vtable in the original, slot by slot, with the method in each slot.

gcc 2.95 (vtable thunks, the Linux default) lays a vtable out as: slot 0 = offset word,
slot 1 = typeinfo function, then one entry per virtual. A PIC library stores each entry as
a relocation (R_386_32 against a symbol, or R_386_RELATIVE with the address in place), so
the slot's target is resolved through .rel.dyn and the symbol table.

Writes reference/vtables.tsv: class, slot (0 = first virtual), method, implementing class,
plus a line per class in the summary. Also prints, for one class, the virtuals it introduces
beyond its base's slots:  scripts/vtables.py --new CBaseEntity
Usage: scripts/vtables.py [--new <Class> ...]
"""
import argparse, bisect, csv, os, re, struct, sys
from elftools.elf.elffile import ELFFile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SO = os.path.join(ROOT, "reference", "rc_i386.so")
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import gnv2demangle  # noqa: E402

VT = re.compile(r"^__vt_(\d+)(\w+)$")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--new", nargs="*", default=[])
    args = ap.parse_args()
    elf = ELFFile(open(SO, "rb"))
    data = open(SO, "rb").read()
    segs = [s for s in elf.iter_segments() if s["p_type"] == "PT_LOAD"]

    def word(a):
        for s in segs:
            if s["p_vaddr"] <= a < s["p_vaddr"] + s["p_filesz"]:
                o = s["p_offset"] + a - s["p_vaddr"]
                return struct.unpack_from("<I", data, o)[0]
        return 0

    dynsym = elf.get_section_by_name(".dynsym")
    rel = {}
    for sec in elf.iter_sections():
        if sec.header["sh_type"] == "SHT_REL":
            for r in sec.iter_relocations():
                sym = dynsym.get_symbol(r["r_info_sym"]).name if r["r_info_sym"] else None
                rel[r["r_offset"]] = sym

    funcs, vts = {}, []
    for s in elf.get_section_by_name(".symtab").iter_symbols():
        t = s["st_info"]["type"]
        if t == "STT_FUNC" and s["st_value"]:
            funcs.setdefault(s["st_value"], s.name)
        m = VT.match(s.name)
        if m and t == "STT_OBJECT" and s["st_size"]:
            n, rest = int(m.group(1)), m.group(2)
            if len(rest) == n:   # primary vtable only
                vts.append((rest, s["st_value"], s["st_size"]))

    def target(addr):
        name = rel.get(addr)
        if name:
            return name
        w = word(addr)
        return funcs.get(w, f"0x{w:x}" if w else "(null)")

    def demangle(n):
        try:
            return gnv2demangle.demangle(n)
        except Exception:
            return None

    table = {}
    for cls, a, size in sorted(set(vts)):
        slots = []
        for i in range(2, size // 4):
            sym = target(a + 4 * i)
            sig = demangle(sym) if sym else None
            if sym == "__pure_virtual":
                slots.append(("(pure virtual)", ""))
            elif sig and "::" in sig.split("(")[0]:
                owner, meth = sig.split("(")[0].rsplit("::", 1)
                slots.append((meth, owner))
            else:
                slots.append((sym, ""))
        table[cls] = slots

    out = os.path.join(ROOT, "reference", "vtables.tsv")
    with open(out, "w") as f:
        f.write("class\tslot\tmethod\timplemented_by\n")
        for cls in sorted(table):
            for i, (m, o) in enumerate(table[cls]):
                f.write(f"{cls}\t{i}\t{m}\t{o}\n")
    print(f"{len(table)} vtables -> {os.path.relpath(out, ROOT)}")

    bases = {}
    h = os.path.join(ROOT, "reference", "hierarchy.tsv")
    if os.path.exists(h):
        for r in csv.DictReader(open(h), delimiter="\t"):
            bases[r["class"]] = r["bases"].split(",")[0] if r["bases"] else ""
    for cls in args.new:
        b = bases.get(cls, "")
        start = len(table.get(b, [])) if b else 0
        print(f"\n{cls} (base {b or '-'}, {len(table.get(cls, []))} slots; new from slot {start}):")
        for i, (m, o) in enumerate(table.get(cls, [])[start:], start):
            print(f"  {i:3d} {m}  [{o}]")


if __name__ == "__main__":
    main()
