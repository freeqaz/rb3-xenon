#!/usr/bin/env python3
"""Locate every retail `ObjPtr<T>::Replace(ObjRef*, Hmx::Object*)` body and
propose a target_symbol_map entry for each one our build can actually pair.

Why this exists
---------------
laneAU-2 found that the retail body of ObjPtr<T>::Replace

    if (mObject == from) SetObjConcrete(dynamic_cast<T *>(to));

is a fixed 0x60-byte shape that repeats once per instantiated T.  Two of them
(0x82314B70 / 0x823BDAB0) had been *mis*-mapped onto
`?SetObj@?$ObjRefConcrete@V<T>@@VObjectDir@@@@…` by laneAK's byte-class
bijection, which reads a plausible-looking 95.2 % because the two bodies differ
by only three instructions.  The rest of the family is simply unmapped `fn_*`.

The shape is machine-recognisable, so this scanner enumerates it rather than
hand-decoding:

    mflr r12 / stw r12,-0x8(r1) / std r31,-0x10(r1) / stwu r1,-0x60(r1)
    mr   r31, r3
    mr   r3,  r5                      <- `to` hoisted into the dyncast arg slot
    lwz  r11, 0x8(r31)                <- mObject @ 8  (ObjDirPtr would be @ 4)
    cmplw cr6, r11, r4                <- vs `from`
    bne  cr6, <end>
    lis/addi r6 = <T type descriptor>
    lis/addi r5 = <Hmx::Object type descriptor>
    li r7,0 / li r4,0
    bl __RTDynamicCast
    mr r4,r3 / mr r3,r31
    bl ?SetObjConcrete@…

Then, for each hit, it resolves the r6 RTTI type-descriptor label to a class
name out of the `auto_06_*_data.s` dumps (`.?AV<Name>@@`) and checks whether our
compiled object for the *same pinned unit* actually defines
`?Replace@?$ObjPtr@V<Name>@@@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z`.  Only those are
proposable — an unemitted name cannot pair, and a name emitted in a *different*
unit is a splits problem, not a map problem.

Usage:
  objptr_replace_family_scan.py [--repo DIR] [--json OUT]
"""
import argparse
import json
import os
import re
import sys

FN_RE = re.compile(r"^\.fn (fn_[0-9A-Fa-f]{8}), ")
ENDFN_RE = re.compile(r"^\.endfn ")
INSN_RE = re.compile(r"^/\* ([0-9A-F]{8}) [0-9A-F]{8}  (?:[0-9A-F]{2} ){4}\*/\t(.*)$")
LBL_RE = re.compile(r"lbl_([0-9A-F]{8})")


def parse_functions(path):
    """Yield (va, [insn strings]) for every .fn block in a dtk .s file."""
    cur = None
    insns = []
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            m = FN_RE.match(line)
            if m:
                cur = m.group(1)
                insns = []
                continue
            if cur is None:
                continue
            if ENDFN_RE.match(line):
                yield cur, insns
                cur = None
                continue
            mi = INSN_RE.match(line)
            if mi:
                insns.append(mi.group(2).strip())


def load_type_descriptors(asm_dir):
    """label VA (upper hex) -> class name, from the `.?AV<Name>@@` payload of
    an MSVC ??_R0 type descriptor sitting in the auto_06 .data dumps."""
    out = {}
    for name in sorted(os.listdir(asm_dir)):
        if not (name.startswith("auto_06_") and name.endswith("_data.s")):
            continue
        cur = None
        words = []
        with open(os.path.join(asm_dir, name), "r", errors="replace") as fh:
            for line in fh:
                line = line.strip()
                if line.startswith(".obj lbl_"):
                    cur = line.split()[1].rstrip(",")[4:]
                    words = []
                elif line.startswith(".endobj"):
                    if cur and len(words) >= 3:
                        raw = b"".join(w.to_bytes(4, "big") for w in words[2:])
                        s = raw.split(b"\x00")[0].decode("ascii", "replace")
                        if s.startswith(".?AV") and s.endswith("@@"):
                            out.setdefault(cur, s[4:-2])
                    cur = None
                elif cur is not None and line.startswith(".4byte"):
                    tok = line.split()[1]
                    words.append(int(tok, 16) if tok.startswith("0x") else 0)
    return out


def is_replace_body(insns):
    """Structural recogniser for the retail ObjPtr<T>::Replace body."""
    if len(insns) != 24:
        return None
    joined = insns
    if not joined[0].startswith("mflr"):
        return None
    if "stwu" not in joined[3] or "-0x60" not in joined[3]:
        return None
    if joined[4] != "mr r31, r3" or joined[5] != "mr r3, r5":
        return None
    # mObject @ 0x8 -> ObjPtr.  ObjDirPtr keeps its pointer at 0x4.
    if not re.match(r"^lwz r11, 0x8\(r31\)$", joined[6]):
        return None
    if joined[7] != "cmplw cr6, r11, r4":
        return None
    if not joined[8].startswith("bne cr6"):
        return None
    m6 = LBL_RE.search(joined[11])  # addi r6, r11, lbl_<T>
    m5 = LBL_RE.search(joined[12])  # addi r5, r10, lbl_<Hmx::Object>
    if not (m6 and m5 and joined[11].startswith("addi r6") and joined[12].startswith("addi r5")):
        return None
    if joined[13] != "li r7, 0x0" or joined[14] != "li r4, 0x0":
        return None
    if not joined[15].startswith("bl "):
        return None
    if joined[16] != "mr r4, r3" or joined[17] != "mr r3, r31":
        return None
    if not joined[18].startswith("bl "):
        return None
    return m6.group(1), m5.group(1)


def obj_defines(objpath, symbol):
    """Cheap containment test against the COFF string table."""
    try:
        with open(objpath, "rb") as fh:
            return symbol.encode() in fh.read()
    except OSError:
        return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=os.getcwd())
    ap.add_argument("--json", default=None)
    args = ap.parse_args()

    repo = os.path.abspath(args.repo)
    asm_dir = os.path.join(repo, "build/45410914/asm")
    obj_root = os.path.join(repo, "build/45410914/src")
    map_path = os.path.join(repo, "scripts/target_symbol_map.json")

    with open(map_path) as fh:
        raw = json.load(fh)
    # the map also carries list-valued bookkeeping keys (_bijection_arbitrary,
    # _icf_arbitrary, _denylist) -- keep only real VA -> mangled-name rows
    symmap = {k: v for k, v in raw.items() if isinstance(v, str)}
    taken = set(symmap.values())

    tdesc = load_type_descriptors(asm_dir)
    sys.stderr.write("type descriptors: %d\n" % len(tdesc))

    # unit .s files only (skip the auto_* whole-section dumps: unpinned space)
    unit_asm = [
        n for n in sorted(os.listdir(asm_dir))
        if n.endswith(".s") and not n.startswith("auto_")
    ]

    # unit stem -> compiled obj path
    objs = {}
    for dirpath, _, files in os.walk(obj_root):
        for f in files:
            if f.endswith(".obj"):
                objs.setdefault(f[:-4], os.path.join(dirpath, f))

    rows = []
    for name in unit_asm:
        stem = name[:-2]
        for fn, insns in parse_functions(os.path.join(asm_dir, name)):
            hit = is_replace_body(insns)
            if not hit:
                continue
            tlbl, _srclbl = hit
            cls = tdesc.get(tlbl)
            va = "0x" + fn[3:].lower()
            sym = (
                "?Replace@?$ObjPtr@V%s@@@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z" % cls
                if cls else None
            )
            objpath = objs.get(stem)
            rows.append({
                "va": va,
                "unit": stem,
                "type_label": tlbl,
                "class": cls,
                "symbol": sym,
                "already_mapped_as": symmap.get(va) or symmap.get(va.upper()),
                "emitted_in_unit": bool(sym and objpath and obj_defines(objpath, sym)),
                "obj": objpath,
            })

    rows.sort(key=lambda r: r["va"])
    proposable = [
        r for r in rows
        if r["emitted_in_unit"] and r["symbol"]
        and r["symbol"] not in taken
    ]
    print("candidate Replace bodies in pinned units: %d" % len(rows))
    print("  already mapped (any name):              %d"
          % sum(1 for r in rows if r["already_mapped_as"]))
    print("  emitted in the same unit:               %d"
          % sum(1 for r in rows if r["emitted_in_unit"]))
    print("  PROPOSABLE (emitted + name free):       %d" % len(proposable))
    for r in proposable:
        print("    %s  %-28s %s%s" % (
            r["va"], r["unit"], r["symbol"],
            "   [displaces %s]" % r["already_mapped_as"] if r["already_mapped_as"] else "",
        ))
    if args.json:
        with open(args.json, "w") as fh:
            json.dump({"all": rows, "proposable": proposable}, fh, indent=1)
        print("wrote %s" % args.json)


if __name__ == "__main__":
    main()
