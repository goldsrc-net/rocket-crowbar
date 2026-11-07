#!/usr/bin/env python3
"""hierarchy.py - each class's declared base(s), read from the original's typeinfo functions.

gcc 2.x emits __tf<Class>() to build a class's type_info. It calls __tf<Base>() for each
base before registering itself (__rtti_si for single inheritance, __rtti_class for multiple),
so the __tf calls inside it are the declared bases, in order. Same method as mod-ts's
scripts/coverage/hierarchy.py.

Writes reference/hierarchy.tsv: class, base(s) comma-separated (empty = root), source file.
Usage: scripts/hierarchy.py
"""
import os, re, subprocess, sys, csv

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SO = os.path.join(ROOT, "reference", "rc_i386.so")
TF = re.compile(r"^__tf(\d+)(\w+)$")


def cls_of(sym):
    m = TF.match(sym)
    if not m:
        return None
    n, rest = int(m.group(1)), m.group(2)
    return rest[:n] if len(rest) >= n else None


def main():
    dis = subprocess.run(["objdump", "-d", "--no-show-raw-insn", SO], check=True,
                         capture_output=True, text=True).stdout
    bases, cur = {}, None
    for line in dis.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            cur = cls_of(m.group(1))
            if cur:
                bases.setdefault(cur, [])
            continue
        if cur:
            c = re.search(r"call\s+[0-9a-f]+ <(__tf\w+?)(@plt)?>", line)
            if c:
                b = cls_of(c.group(1))
                if b and b != cur and b not in bases[cur]:
                    bases[cur].append(b)

    files = {}
    ff = os.path.join(ROOT, "reference", "funcfile.tsv")
    if os.path.exists(ff):
        for r in csv.DictReader(open(ff), delimiter="\t"):
            if r["class"] and r["kind"] != "generated":
                files.setdefault(r["class"], r["file"])

    out = os.path.join(ROOT, "reference", "hierarchy.tsv")
    with open(out, "w") as f:
        f.write("class\tbases\tfile\n")
        for c in sorted(bases):
            f.write(f"{c}\t{','.join(bases[c])}\t{files.get(c, '')}\n")
    multi = [c for c in bases if len(bases[c]) > 1]
    print(f"{len(bases)} classes, {len(multi)} with multiple bases -> {os.path.relpath(out, ROOT)}")


if __name__ == "__main__":
    main()
