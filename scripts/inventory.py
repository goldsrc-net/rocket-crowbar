#!/usr/bin/env python3
"""inventory.py - the reconstruction work list for Rocket Crowbar 1.9.

Reads the original reference/rc_i386.so (not stripped, gcc 2.95) and our i386 build of this
repo, and writes:

  reference/funcfile.tsv     every defined function in the original: address, size, source file,
                             class, method, demangled signature, and whether our build has a
                             method of that name (stock = in our build, rc = not)
  reference/RECONSTRUCTION.md  the work list: RC's own source files, RC-only classes, methods RC
                             added to SDK classes, entities, and SDK classes RC does not have

Source files come from the symbol table: gcc 2.x emits a local `gcc2_compiled.` label at the
start of every object file's .text, right after that file's FILE symbol, so each function
belongs to the file whose start is the nearest one below it.

Usage: scripts/inventory.py [--ours build-i386/dlls/hl.so]
Needs readelf/nm (binutils) and scripts/gnv2demangle.py next to this file.
"""
import argparse, bisect, collections, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import gnv2demangle  # noqa: E402

ORIG = os.path.join(ROOT, "reference", "rc_i386.so")
TEXT_NDX = "17"   # .text's section index in rc_i386.so
RUNTIME = {"crtstuff.c", "initfini.c", "libgcc2.c", "frame.c", "exception.cc",
           "new.cc", "new1.cc", "new2.cc", "tinfo.cc", "tinfo2.cc"}
GENERATED = re.compile(r"^(__tf|__ti|_GLOBAL_\$|__thunk_|__\w+__F|_init$|_fini$)")


def run(*cmd):
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def symtab(path):
    """Yield (num, value, size, type, bind, ndx, name) for .symtab only, in table order."""
    out = run("readelf", "-W", "-s", path)
    sect = None
    for line in out.splitlines():
        m = re.match(r"Symbol table '(\S+)'", line)
        if m:
            sect = m.group(1)
            continue
        if sect != ".symtab":
            continue
        p = line.split()
        if len(p) < 8 or not p[0].endswith(":") or p[1] == "Value":
            continue
        yield int(p[1], 16), int(p[2]), p[3], p[4], p[6], p[7]


def demangle(name):
    try:
        return gnv2demangle.demangle(name)
    except Exception:
        return None


def split_sig(sig):
    """'CFoo::Bar(int)' -> ('CFoo', 'Bar'); free function -> ('', name)."""
    head = sig.split("(", 1)[0]
    if "::" in head:
        cls, meth = head.rsplit("::", 1)
        return cls, meth
    return "", head


def our_methods(so):
    """Class -> set(method) and the set of free functions, from our (Itanium) build."""
    out = run("nm", "-C", "--defined-only", so)
    methods = collections.defaultdict(set)
    for line in out.splitlines():
        p = line.split(" ", 2)
        if len(p) < 3 or p[1] not in "TtWw":
            continue
        name = p[2]
        if name.startswith(("non-virtual thunk", "virtual thunk")):
            continue
        cls, meth = split_sig(name)
        methods[cls].add(meth)
    return methods


def dyn_entities(so):
    """Unmangled defined functions in .dynsym (LINK_ENTITY_TO_CLASS factories + the engine API)."""
    out = run("nm", "-D", "--defined-only", so)
    names = set()
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "TW" and not p[2].startswith("_") and "__" not in p[2]:
            names.add(p[2])
    return names


def stem(f):
    return f.rsplit(".", 1)[0].lower()


def sdk_source_files():
    """Stems of the SDK's source files (halflife-updated renamed pm_shared's .c files to .cpp).
    globals.cpp is Valve's; halflife-updated folded it into other files."""
    stems = {"globals"}
    for d in ("dlls", "pm_shared", "game_shared", "common", "engine"):
        for dp, _, fs in os.walk(os.path.join(ROOT, d)):
            stems.update(stem(f) for f in fs if f.endswith((".cpp", ".c")))
    return stems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ours", default=os.path.join(ROOT, "build-i386", "dlls", "hl.so"))
    args = ap.parse_args()
    if not os.path.exists(args.ours):
        sys.exit(f"missing {args.ours}: run scripts/build.sh i386 first")

    # --- original: file starts + functions ---
    # A file starts at its gcc2_compiled. label. The statically linked C++ runtime objects
    # (tinfo.cc, new.cc, ...) have no such label, so for them use their first local code symbol.
    starts, funcs, cur, labelled, first_local = [], [], None, set(), {}
    for val, size, typ, bind, ndx, name in symtab(ORIG):
        if typ == "FILE":
            cur = name
        elif name == "gcc2_compiled." and cur:
            starts.append((val, cur))
            labelled.add(cur)
        else:
            if cur and bind == "LOCAL" and typ in ("FUNC", "NOTYPE") and ndx == TEXT_NDX and val:
                first_local.setdefault(cur, val)
            if typ == "FUNC" and ndx != "UND" and size > 0:
                funcs.append((val, size, name))
    starts += [(v, f) for f, v in first_local.items() if f not in labelled]
    starts.sort()
    start_addrs = [s[0] for s in starts]
    funcs = sorted(set(funcs))

    ours = our_methods(args.ours)
    sdk_files = sdk_source_files()

    rows = []
    for val, size, name in funcs:
        i = bisect.bisect_right(start_addrs, val) - 1
        src = starts[i][1] if i >= 0 else "?"
        sig = demangle(name)
        cls, meth = split_sig(sig) if sig else ("", name)
        if GENERATED.match(name) or (cls and meth in (cls.split("::")[-1], "~" + cls.split("::")[-1])):
            kind = "generated"   # typeinfo fns, static init, thunks, ctors/dtors
        elif meth in ours.get(cls, ()):
            kind = "stock"
        else:
            kind = "rc"
        rows.append(dict(addr=val, size=size, file=src, cls=cls, meth=meth,
                         sig=sig or name, mangled=name, kind=kind))

    with open(os.path.join(ROOT, "reference", "funcfile.tsv"), "w") as f:
        f.write("addr\tsize\tfile\tclass\tmethod\tkind\tsignature\tmangled\n")
        for r in rows:
            f.write(f"{r['addr']:08x}\t{r['size']}\t{r['file']}\t{r['cls']}\t{r['meth']}\t"
                    f"{r['kind']}\t{r['sig']}\t{r['mangled']}\n")

    # --- summaries: drop the C++ runtime and compiler-generated functions ---
    rows = [r for r in rows if r["file"] not in RUNTIME and r["kind"] != "generated"]
    for r in rows:
        r["stock"] = r["kind"] == "stock"
    by_file = collections.defaultdict(list)
    for r in rows:
        by_file[r["file"]].append(r)
    rc_files = sorted(f for f in by_file if stem(f) not in sdk_files)
    sdk_in_rc = sorted(f for f in by_file if stem(f) in sdk_files)

    our_classes = {c for c in ours if c}
    orig_classes = collections.defaultdict(list)
    for r in rows:
        if r["cls"]:
            orig_classes[r["cls"]].append(r)
    rc_classes = sorted(c for c in orig_classes if c not in our_classes)
    sdk_classes = sorted(c for c in orig_classes if c in our_classes)
    added = {c: [r for r in orig_classes[c] if not r["stock"]] for c in sdk_classes}
    added = {c: v for c, v in added.items() if v}
    missing_classes = sorted(c for c in our_classes
                             if c not in orig_classes and c.startswith("C") and "::" not in c)

    ent_orig, ent_ours = dyn_entities(ORIG), dyn_entities(args.ours)

    def fmt_bytes(rs):
        return sum(r["size"] for r in rs)

    L = []
    L.append("# Rocket Crowbar 1.9: reconstruction work list\n")
    L.append("Generated by `scripts/inventory.py` from `reference/rc_i386.so` and an i386 build of this "
             "repo. Per-function detail (address, size, file, signature) is in `funcfile.tsv`. "
             "\"rc\" means our SDK has no method of that name in that class; a stock-named method "
             "can still have RC changes in its body, which this list can't see.\n")
    L.append("## Totals\n")
    L.append("```")
    L.append(f"functions (excluding gcc runtime)   {len(rows):5d}   {fmt_bytes(rows):8d} bytes")
    rcf = [r for f in rc_files for r in by_file[f]]
    L.append(f"  in RC's own source files ({len(rc_files):2d})    {len(rcf):5d}   {fmt_bytes(rcf):8d} bytes")
    rest = [r for r in rows if stem(r["file"]) in sdk_files]
    rest_rc = [r for r in rest if not r["stock"]]
    L.append(f"  in SDK-named files ({len(sdk_in_rc):3d})          {len(rest):5d}   {fmt_bytes(rest):8d} bytes")
    L.append(f"    of which not in our SDK          {len(rest_rc):5d}   {fmt_bytes(rest_rc):8d} bytes")
    L.append(f"classes                             {len(orig_classes):5d}")
    L.append(f"  RC-only                           {len(rc_classes):5d}")
    L.append(f"  SDK classes with RC-added methods {len(added):5d}")
    L.append(f"entity factories   original {len(ent_orig)}  ours {len(ent_ours)}  "
             f"RC-only {len(ent_orig - ent_ours)}  ours-only {len(ent_ours - ent_orig)}")
    L.append("```\n")

    L.append("## RC's own source files\n")
    L.append("```")
    for f in sorted(rc_files, key=lambda f: -fmt_bytes(by_file[f])):
        cl = sorted({r["cls"] for r in by_file[f] if r["cls"]})
        L.append(f"{f:22s} {len(by_file[f]):4d} fns {fmt_bytes(by_file[f]):7d} B  {' '.join(cl)[:150]}")
    L.append("```\n")

    L.append("## SDK files with RC additions\n")
    L.append("Functions in SDK-named files that our SDK does not have, by file.\n")
    L.append("```")
    tally = []
    for f in sdk_in_rc:
        x = [r for r in by_file[f] if not r["stock"]]
        if x:
            tally.append((fmt_bytes(x), f, x))
    for b, f, x in sorted(tally, reverse=True):
        L.append(f"{f:26s} {len(x):4d} fns {b:7d} B  of {len(by_file[f])} fns")
    L.append("```\n")

    L.append("## RC-only classes\n")
    L.append("```")
    for c in sorted(rc_classes, key=lambda c: -fmt_bytes(orig_classes[c])):
        files = sorted({r["file"] for r in orig_classes[c]})
        L.append(f"{c:28s} {len(orig_classes[c]):4d} fns {fmt_bytes(orig_classes[c]):7d} B  {' '.join(files)}")
    L.append("```\n")

    L.append("## Methods RC added to SDK classes\n")
    L.append("```")
    for c in sorted(added, key=lambda c: -fmt_bytes(added[c])):
        ms = sorted({r["meth"] for r in added[c]})
        L.append(f"{c:24s} {len(added[c]):3d}  {' '.join(ms)[:170]}")
    L.append("```\n")

    L.append("## Entities\n")
    L.append("RC-only (in the original, not in our build):\n")
    L.append("```")
    L.append(" ".join(sorted(ent_orig - ent_ours)))
    L.append("```\n")
    L.append("Ours only (in our build, not in the original): remove only after checking the original "
             "really lacks them.\n")
    L.append("```")
    L.append(" ".join(sorted(ent_ours - ent_orig)))
    L.append("```\n")

    L.append("## SDK classes the original does not have\n")
    L.append("```")
    L.append(" ".join(missing_classes))
    L.append("```")

    with open(os.path.join(ROOT, "reference", "RECONSTRUCTION.md"), "w") as f:
        f.write("\n".join(L) + "\n")
    print(f"{len(rows)} functions, {len(rc_files)} RC files, {len(rc_classes)} RC classes, "
          f"{len(added)} SDK classes with additions -> reference/RECONSTRUCTION.md, funcfile.tsv")


if __name__ == "__main__":
    main()
