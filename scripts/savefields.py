#!/usr/bin/env python3
"""savefields.py - every class's save/restore table in the original: member names, types, offsets.

DEFINE_FIELD entries live in static `m_SaveData` arrays (gcc 2.x symbol _<n><Class>.m_SaveData)
of TYPEDESCRIPTION { FIELDTYPE fieldType; char *fieldName; int fieldOffset; short fieldSize;
short flags; } (16 bytes on i386). The name pointer is an R_386_RELATIVE relocation with the
address in place. Offsets are the ORIGINAL's (gcc 2.95 layout, vtable pointer after the base
data), so they are provenance, not our layout.

Writes reference/savefields.tsv: class, field, offset (hex), type, count, flags.
Usage: scripts/savefields.py
"""
import os, re, struct
from elftools.elf.elffile import ELFFile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SO = os.path.join(ROOT, "reference", "rc_i386.so")
SD = re.compile(r"^_(\d+)(\w+)\.m_\w*[Ss]aveData$")
# FIELDTYPE order from the SDK's dlls/util.h
TYPES = ["FIELD_FLOAT", "FIELD_STRING", "FIELD_ENTITY", "FIELD_CLASSPTR", "FIELD_EHANDLE",
         "FIELD_EVARS", "FIELD_EDICT", "FIELD_VECTOR", "FIELD_POSITION_VECTOR", "FIELD_POINTER",
         "FIELD_INTEGER", "FIELD_FUNCTION", "FIELD_BOOLEAN", "FIELD_SHORT", "FIELD_CHARACTER",
         "FIELD_TIME", "FIELD_MODELNAME", "FIELD_SOUNDNAME"]


def main():
    elf = ELFFile(open(SO, "rb"))
    data = open(SO, "rb").read()
    segs = [s for s in elf.iter_segments() if s["p_type"] == "PT_LOAD"]

    def read(a, n):
        for s in segs:
            if s["p_vaddr"] <= a < s["p_vaddr"] + s["p_filesz"]:
                o = s["p_offset"] + a - s["p_vaddr"]
                return data[o:o + n]

    def cstr(a):
        b = read(a, 128)
        return b.split(b"\0", 1)[0].decode("latin-1") if b else "?"

    rows = []
    for s in elf.get_section_by_name(".symtab").iter_symbols():
        m = SD.match(s.name)
        if not m or not s["st_size"]:
            continue
        n, rest = int(m.group(1)), m.group(2)
        cls = rest[:n]
        blob = read(s["st_value"], s["st_size"])
        for i in range(0, len(blob) - 15, 16):
            ft, pn, off, size, flags = struct.unpack_from("<iIihh", blob, i)
            rows.append((cls, cstr(pn), off, TYPES[ft] if 0 <= ft < len(TYPES) else str(ft), size, flags))
    out = os.path.join(ROOT, "reference", "savefields.tsv")
    with open(out, "w") as f:
        f.write("class\tfield\toffset\ttype\tcount\tflags\n")
        for r in sorted(rows, key=lambda r: (r[0], r[2])):
            f.write(f"{r[0]}\t{r[1]}\t0x{r[2]:x}\t{r[3]}\t{r[4]}\t{r[5]}\n")
    print(f"{len({r[0] for r in rows})} classes, {len(rows)} fields -> {os.path.relpath(out, ROOT)}")

    # members.tsv: the same fields with a C++ type, in the column layout scripts/ghidra/ApplyRC.java
    # reads (class, offset, fieldtype, cpptype, name, count). Booleans are 1 byte here: RC's bool
    # members sit at adjacent byte offsets (CBasePlayer m_blSetView 0x628, m_blCamInited 0x629).
    out = os.path.join(ROOT, "reference", "members.tsv")
    with open(out, "w") as f:
        f.write("class\toffset\tfieldtype\tcpptype\tname\tcount\n")
        for r in sorted(rows, key=lambda r: (r[0], r[2])):
            f.write(f"{r[0]}\t0x{r[2]:x}\t{r[3]}\t{CPPTYPE.get(r[3], 'int')}\t{r[1]}\t{r[4]}\n")
    print(f"-> {os.path.relpath(out, ROOT)}")


CPPTYPE = {"FIELD_FLOAT": "float", "FIELD_TIME": "float", "FIELD_STRING": "string_t",
           "FIELD_MODELNAME": "string_t", "FIELD_SOUNDNAME": "string_t", "FIELD_INTEGER": "int",
           "FIELD_BOOLEAN": "bool", "FIELD_SHORT": "short", "FIELD_CHARACTER": "char",
           "FIELD_VECTOR": "Vector", "FIELD_POSITION_VECTOR": "Vector",
           "FIELD_CLASSPTR": "CBaseEntity *", "FIELD_EHANDLE": "EHANDLE",
           "FIELD_EVARS": "entvars_t *", "FIELD_EDICT": "edict_t *", "FIELD_ENTITY": "EOFFSET",
           "FIELD_POINTER": "void *", "FIELD_FUNCTION": "void *"}


if __name__ == "__main__":
    main()
