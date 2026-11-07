#!/usr/bin/env python3
"""skim.py <worktree> <file.cpp>... : flag decompiled functions whose strings, float constants or
called names don't appear in our body of the same function. A lead list for spotting RC changes
in stock functions; noisy by design."""
import csv, re, sys, os
WT = sys.argv[1]
D = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "recovered", "decomp") + "/"
cache = {}
def decomp(cls, addr):
    f = D + (cls if cls else "_free") + ".c"
    if f not in cache:
        t = open(f, errors="replace").read()
        cache[f] = {}
        for p in re.split(r"(?=^/\* \S+  @ )", t, flags=re.M):
            m = re.match(r"/\* \S+  @ ([0-9a-f]+)", p)
            if m: cache[f][int(m.group(1), 16)] = p
    return cache[f].get(addr)
def body(src, cls, meth):
    b = body1(src, (re.escape(cls) + r"::" if cls else r"\b") + re.escape(meth))
    if not b and cls: b = body1(src, r"(?<![:\w])" + re.escape(meth))
    return b
def body1(src, head):
    pat = head + r"\s*\([^;{]*\)\s*(const\s*)?(override\s*)?\{"
    out = []
    for m in re.finditer(pat, src):
        i = m.end(); d = 1
        while d and i < len(src):
            if src[i] == '{': d += 1
            elif src[i] == '}': d -= 1
            i += 1
        out.append(src[m.start():i])
    return "\n".join(out)
IGN = set("""SQRT sqrt fabs floor ceil abs atan2 cos sin tan pow __opP11CBaseEntity__7EHANDLE __rf__7EHANDLE __as__7EHANDLEP11CBaseEntity __eq__7EHANDLEP11CBaseEntity
operator Vector Length Normalize Make2D DotProduct CrossProduct ENT VARS OFFSET STRING ALLOC_STRING""".split())
def names(txt):
    s = set()
    for m in re.finditer(r"vtbl->(\w+)", txt): s.add(m.group(1))
    for m in re.finditer(r"\b(?:\w+::)?(\w+?)(?:__\w*)?\s*\(", txt): s.add(m.group(1))
    return s
def floats(txt):
    v = set()
    for m in re.finditer(r"(?<![\w.])(\d+\.\d*(?:e[+-]?\d+)?|\d+e[+-]?\d+)", txt):
        try: v.add(round(float(m.group(1)), 4))
        except: pass
    return v
def allnums(txt):
    v = set()
    for m in re.finditer(r"(?<![\w.])(\d+\.?\d*(?:e[+-]?\d+)?)f?", txt):
        try: v.add(round(float(m.group(1)), 4))
        except: pass
    for m in re.finditer(r"0x[0-9a-fA-F]+", txt): v.add(float(int(m.group(0), 16)))
    return v
import glob
MAC = {}
for h in glob.glob(WT + "/dlls/*.h") + glob.glob(WT + "/common/*.h") + glob.glob(WT + "/engine/*.h") + glob.glob(WT + "/game_shared/*.h") + glob.glob(WT + "/pm_shared/*.h") + glob.glob(WT + "/dlls/*.cpp"):
    for m in re.finditer(r"^\s*#define\s+(\w+)\s+(.*)$", open(h, errors="replace").read(), re.M):
        MAC.setdefault(m.group(1), m.group(2))
    for m in re.finditer(r"^\s*(?:static\s+)?(?:constexpr|const)\s+[\w:]+\s+(\w+)\s*(?:=\s*)?([^;]*);", open(h, errors="replace").read(), re.M):
        MAC.setdefault(m.group(1), m.group(2))
def expand(txt, depth=3):
    out = txt
    for _ in range(depth):
        add = " ".join(MAC[w] for w in set(re.findall(r"\b[A-Z_][A-Z0-9_]+\b", out)) if w in MAC)
        out = txt + " " + add
        txt = out
    return out
rows = list(csv.DictReader(open(WT + "/reference/funcfile.tsv"), delimiter="\t"))
for fn in sys.argv[2:]:
    src = open(WT + "/dlls/" + fn, errors="replace").read()
    for r in rows:
        if r["file"] != fn or r["kind"] == "generated": continue
        cls, meth = r["class"], r["method"]
        d = decomp(cls, int(r["addr"], 16) + 0x10000)
        if not d: print(f"{fn} {cls}::{meth}: NO DECOMP"); continue
        # skip header/decl lines
        dbody = d.split("{", 1)[1] if "{" in d else d
        ours = body(src, cls, meth)
        if not ours:
            print(f"{fn} {r['addr']} {cls}::{meth}: NOT IN OUR FILE"); continue
        strs = set(x.replace("\\'", "'") for x in re.findall(r'"((?:[^"\\]|\\.)*)"', dbody))
        ostr = set(re.findall(r'"((?:[^"\\]|\\.)*)"', ours)) | set(re.findall(r'"((?:[^"\\]|\\.)*)"', src))
        ms = sorted(x for x in strs - ostr if x)
        on = allnums(expand(ours))
        mf = sorted(x for x in floats(dbody) if x not in on and x not in (0.0, 1.0) and not any(abs(x - y) < 1e-3 * max(1, abs(y)) for y in on))
        onames = names(ours)
        mn = sorted(x for x in names(dbody) - onames if x not in IGN and not x.startswith(("FUN_", "CONCAT", "SUB", "NAN", "ROUND", "_", "if", "while", "switch", "return", "sizeof")) and x != meth)
        dmem = set(re.findall(r"(?:->|\.)([A-Za-z_]\w*)", dbody)) - {"vtbl", "pContainingEntity", "x", "y", "z", "pStringBase", "time", "frametime"}
        dmem = {m for m in dmem if not m.startswith(("field_", "_")) and m not in names(dbody)}
        omem = set(re.findall(r"\b([A-Za-z_]\w*)\b", expand(ours)))
        mm = sorted(dmem - omem)
        ostr_f = set(x for x in re.findall(r'"((?:[^"\\]|\\.)*)"', ours) if x)
        rs = sorted(x for x in ostr_f if x not in strs and x.replace("'", "\\'") not in strs)
        if os.environ.get("REV"):
            if rs: print(f"{fn} {r['addr']} {r['size']:>5} {cls}::{meth}: ours-only str={rs}")
            continue
        if ms or mf or mn or mm:
            print(f"{fn} {r['addr']} {r['size']:>5} {cls}::{meth}: str={ms} flt={mf} names={mn} mem={mm}")
