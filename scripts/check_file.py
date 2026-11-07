#!/usr/bin/env python3
"""check_file.py - is every function of one original source file in our build?

For each function reference/funcfile.tsv puts in <file> (the original's own source file), look
for the same Class::method (or free function) in our i386 build's symbols, and for each entity
factory (LINK_ENTITY_TO_CLASS) in the original's .dynsym, the same export in ours.

  scripts/check_file.py drivegarg.cpp            # needs build-i386/dlls/hl.so (scripts/build.sh i386)
  scripts/check_file.py drivegarg.cpp --missing  # only what's missing

Compiler-generated functions (typeinfo, static init, ctors/dtors) are skipped. A method with no
symbol but an inline body in our headers (or in the file itself) prints "inl" (by name only: confirm it's the right class).
Exit status 1 when anything is missing.
"""
import argparse, csv, glob, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("--ours", default=os.path.join(ROOT, "build-i386", "dlls", "hl.so"))
    ap.add_argument("--missing", action="store_true")
    a = ap.parse_args()
    if not os.path.exists(a.ours):
        sys.exit(f"missing {a.ours}: run scripts/build.sh i386")

    out = subprocess.run(["nm", "-C", "--defined-only", a.ours], capture_output=True, text=True).stdout
    ours = set()
    for line in out.splitlines():
        p = line.split(" ", 2)
        if len(p) == 3:
            ours.add(p[2].split("(")[0])
    dyn = set(subprocess.run(["nm", "-D", "--defined-only", a.ours], capture_output=True,
                             text=True).stdout.split())

    # methods with an inline body in our headers are present even when no symbol is emitted
    headers = ""
    for h in glob.glob(os.path.join(ROOT, "dlls", "*.h")) + glob.glob(os.path.join(ROOT, "game_shared", "*.h")):
        headers += open(h, errors="replace").read()
    # and classes declared in the file itself (the SDK declares many in their .cpp)
    for src in (os.path.join(ROOT, "dlls", a.file), os.path.join(ROOT, "pm_shared", a.file)):
        if os.path.exists(src):
            headers += open(src, errors="replace").read()

    def inline(meth):
        return re.search(r"\b" + re.escape(meth) + r"\s*\([^;{]*\)\s*(const\s*)?(override\s*)?\{", headers) is not None

    rows = [r for r in csv.DictReader(open(os.path.join(ROOT, "reference", "funcfile.tsv")),
                                      delimiter="\t")
            if r["file"] == a.file and r["kind"] != "generated"]
    if not rows:
        sys.exit(f"no functions for {a.file} in reference/funcfile.tsv")
    have = miss = 0
    for r in sorted(rows, key=lambda r: (r["class"], r["method"])):
        name = f"{r['class']}::{r['method']}" if r["class"] else r["method"]
        # an unmangled free function with no class is either a C function or an entity factory
        ok = name in ours or (not r["class"] and r["method"] in dyn)
        tag = "ok  " if ok else ("inl " if r["class"] and inline(r["method"]) else "MISS")
        ok = tag != "MISS"
        have += ok
        miss += not ok
        if not ok or not a.missing:
            print(f"{tag} {r['addr']} {r['size']:>6} {r['signature']}")
    print(f"\n{a.file}: {have} of {have + miss} present, {miss} missing")
    sys.exit(1 if miss else 0)


if __name__ == "__main__":
    main()
