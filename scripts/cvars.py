#!/usr/bin/env python3
"""cvars.py - every cvar_t the original defines, with its name, default string and flags.

A cvar_t in the original's .data is { char *name; char *string; int flags; float value;
cvar_t *next; }. In a PIC library the two string pointers are R_386_RELATIVE relocations
whose addend sits in place, so the stored word is the string's address in the file.
Lists them in the order GameDLLInit registers them (from the decompile if present, else
symbol order), and marks which ones our build also defines.

Writes reference/cvars.tsv: symbol, name, default, flags, in_ours.
Usage: scripts/cvars.py [--ours build-i386/dlls/hl.so]
"""
import argparse, os, re, struct, subprocess
from elftools.elf.elffile import ELFFile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SO = os.path.join(ROOT, "reference", "rc_i386.so")
DECOMP = os.path.join(ROOT, "recovered", "decomp", "_free.c")


def vaddr_reader(elf):
    segs = [s for s in elf.iter_segments() if s["p_type"] == "PT_LOAD"]
    data = open(SO, "rb").read()

    def read(addr, n):
        for s in segs:
            if s["p_vaddr"] <= addr < s["p_vaddr"] + s["p_filesz"]:
                off = s["p_offset"] + addr - s["p_vaddr"]
                return data[off:off + n]
        return None

    def cstr(addr):
        b = read(addr, 256)
        return b.split(b"\0", 1)[0].decode("latin-1") if b else None
    return read, cstr


def ours_cvars(so):
    """Cvar names our build defines: read the same way from our .so (Itanium, any arch is fine)."""
    names = set()
    if not os.path.exists(so):
        return names
    out = subprocess.run(["strings", "-n", "2", so], capture_output=True, text=True).stdout
    return set(out.split())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ours", default=os.path.join(ROOT, "build-i386", "dlls", "hl.so"))
    args = ap.parse_args()
    elf = ELFFile(open(SO, "rb"))
    read, cstr = vaddr_reader(elf)
    symtab = elf.get_section_by_name(".symtab")
    cvars = {}
    for s in symtab.iter_symbols():
        if s["st_info"]["type"] != "STT_OBJECT" or s["st_size"] != 20 or s["st_shndx"] == "SHN_UNDEF":
            continue
        b = read(s["st_value"], 20)
        if not b:
            continue
        pname, pstr, flags = struct.unpack_from("<III", b)
        name, default = cstr(pname), cstr(pstr)
        if name and default is not None and re.fullmatch(r"[A-Za-z_][\w.]*", name):
            cvars[s.name] = (name, default, flags)

    order = []
    if os.path.exists(DECOMP):
        src = open(DECOMP).read()
        m = re.search(r"void GameDLLInit__Fv\(void\)\s*\{(.*?)\n\}", src, re.S)
        if m:
            order = [x for x in re.findall(r"\(\*DAT_\w+\)\((\w+)\);", m.group(1)) if x in cvars]
    order += [k for k in sorted(cvars) if k not in order]

    ours = ours_cvars(args.ours)
    out = os.path.join(ROOT, "reference", "cvars.tsv")
    with open(out, "w") as f:
        f.write("symbol\tname\tdefault\tflags\tin_ours\n")
        for k in order:
            n, d, fl = cvars[k]
            f.write(f"{k}\t{n}\t{d}\t{fl}\t{'yes' if n in ours else 'no'}\n")
    miss = sum(1 for k in order if cvars[k][0] not in ours)
    print(f"{len(order)} cvars ({miss} not in our build) -> {os.path.relpath(out, ROOT)}")


if __name__ == "__main__":
    main()
