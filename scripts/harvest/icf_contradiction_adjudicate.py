#!/usr/bin/env python3
"""Adjudicate the ICF-alias map contradictions worklist (lane BP-4).

INPUT   docs/plans/lane-bo8-icf-map-contradictions.json -- one row per ICF
        equivalence class that carries >= 2 names which `target_symbol_map.json`
        places at DIFFERENT retail VAs.  Produced by
        scripts/harvest/icf_alias_map_build.py (see its docstring for the
        evidence sources: dc3 linker map, laneAB tie groups, hand proofs, and
        `derived` = our own byte-identical compiled bodies).

THE QUESTION EACH ROW POSES
        Our fold evidence says names A,B share ONE body.  The map says A is at
        VA_a and B is at VA_b, VA_a != VA_b.  Exactly one of these is true:
          (1) the fold is REAL -> at most one of those VAs can hold the shared
              body, so >= N-1 of the map entries name a VA that actually holds
              something else.  Map defect.
          (2) the fold is FALSE FOR RB3 -> retail kept the bodies distinct and
              OUR build collapsed them (dc3 is a NEWER engine; `derived` folds
              are conditional on our codegen being right).  Source/codegen
              divergence, NOT a map defect -- repointing would make things
              worse.

THE DISCRIMINATOR (this script)
        Read the RETAIL bytes at each mapped VA out of band.exe and compare them
        pairwise.  If retail's bodies at those VAs are the same length and equal
        (exactly, or modulo relocation-bearing immediate fields), the fold claim
        is CORROBORATED at retail and we are in case (1).  If they differ in
        length or in masked body, retail demonstrably kept them apart and we are
        in case (2) -- the fold is refuted and the map is left alone.

        Note what this buys and what it does NOT: corroborating a fold proves
        the map is wrong for >= N-1 entries, but it does NOT say WHICH entry is
        right.  Byte-identical twins are, by construction, not distinguishable
        by bytes.  Naming them requires external evidence (vtable slot, caller
        context, .rdata neighbour, cluster attribution) -- that is the
        hand-adjudication step, not this script.

BAND.EXE ADDRESSING (memory: project_bandexe_read_traps)
        `va - 0x82000000` is NOT a file offset for `.text` (delta 0xB200).  This
        script parses the real PE section table and asserts the known anchor
        off(0x824DAAD0) == 0x004CF8D0 before reading anything.

USAGE   python3 scripts/harvest/icf_contradiction_adjudicate.py \
            --out ~/tmp/bp4_evidence.json [--stats]
"""

import argparse
import json
import re
import struct
import sys
from collections import defaultdict, Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))

BANDEXE = ROOT / "orig" / "45410914" / "band.exe"
SYMBOLS = ROOT / "config" / "45410914" / "symbols.txt"
REPORT = ROOT / "build" / "45410914" / "report.json"
WORKLIST = ROOT / "docs" / "plans" / "lane-bo8-icf-map-contradictions.json"
OBJ_ROOT = ROOT / "build" / "45410914" / "src"

ANCHOR_VA, ANCHOR_OFF = 0x824DAAD0, 0x004CF8D0


# ---------------------------------------------------------------------------
# PE section table -> va2off
# ---------------------------------------------------------------------------
class PE:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        f = self.data
        pe = struct.unpack_from("<I", f, 0x3C)[0]
        if f[pe:pe + 4] != b"PE\0\0":
            raise SystemExit("not a PE: %s" % path)
        nsec = struct.unpack_from("<H", f, pe + 6)[0]
        optsz = struct.unpack_from("<H", f, pe + 20)[0]
        self.imgbase = struct.unpack_from("<I", f, pe + 24 + 28)[0]
        self.secs = []
        off = pe + 24 + optsz
        for _ in range(nsec):
            name = f[off:off + 8].rstrip(b"\0").decode("latin1")
            vsz, va, rsz, praw = struct.unpack_from("<IIII", f, off + 8)
            self.secs.append((name, va, vsz, praw, rsz))
            off += 40
        o, _ = self.va2off(ANCHOR_VA)
        if o != ANCHOR_OFF:
            raise SystemExit("band.exe anchor FAILED: off(%#x)=%s want %#x"
                             % (ANCHOR_VA, o, ANCHOR_OFF))

    def va2off(self, va):
        r = va - self.imgbase
        for name, rva, vsz, praw, rsz in self.secs:
            if rsz and rva <= r < rva + max(vsz, rsz):
                return praw + (r - rva), name
        return None, None

    def read(self, va, size):
        o, sec = self.va2off(va)
        if o is None:
            return None, None
        return self.data[o:o + size], sec


# ---------------------------------------------------------------------------
# PPC masking: neutralise fields the LINKER patches, keep real opcodes/regs
# ---------------------------------------------------------------------------
# Opcodes whose 16-bit immediate commonly carries a relocated address half
# (lis/addis, addi, ori, and the load/store displacement forms).
IMM16_OPS = {14, 15, 24, 25, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
             44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55}


def mask_body(b):
    """Zero out link-time-patched fields so two twins compare equal."""
    out = bytearray(b)
    for i in range(0, len(out) - 3, 4):
        w = struct.unpack_from(">I", out, i)[0]
        op = (w >> 26) & 0x3F
        if op == 18:                      # b / bl / ba / bla : 24-bit LI
            w &= 0xFC000003
        elif op == 16:                    # bc : 14-bit BD
            w &= 0xFFFF0003
        elif op in IMM16_OPS:             # 16-bit D / SI field
            w &= 0xFFFF0000
        struct.pack_into(">I", out, i, w)
    return bytes(out)


def norm_body(b, va):
    """RESOLVE (not mask) PC-relative branch fields to their ABSOLUTE target,
    leaving every other field byte-exact.

    Why this and not mask_body: two copies of the *same* function at two
    addresses have DIFFERENT `bl` displacement bytes purely because the field is
    PC-relative -- masking hides that, but so does it hide a genuinely
    DIFFERENT callee.  Masked-equality therefore cannot tell "identical code the
    linker failed to fold" from "same instruction skeleton, different callee",
    and those two lead to OPPOSITE adjudications.  Resolving to the absolute
    target keeps callee identity load-bearing: normalized-equal means the bodies
    call the same things and hold the same immediates, i.e. TRUE twins.
    """
    toks = []
    for i in range(0, len(b) - 3, 4):
        w = struct.unpack_from(">I", b, i)[0]
        op = (w >> 26) & 0x3F
        if op == 18:                            # b / bl
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000                # sign-extend 26-bit
            tgt = (li if (w & 2) else va + i + li) & 0xFFFFFFFF
            toks += [w & 0xFC000003, tgt]
        elif op == 16:                          # bc
            bd = w & 0x0000FFFC
            if bd & 0x8000:
                bd -= 0x10000                   # sign-extend 16-bit
            tgt = (bd if (w & 2) else va + i + bd) & 0xFFFFFFFF
            toks += [w & 0xFFFF0003, tgt]
        else:
            toks.append(w)
    return struct.pack(">%dI" % len(toks), *toks)


# ---------------------------------------------------------------------------
# symbols.txt -> VA -> (name, size, section)
# ---------------------------------------------------------------------------
SYM_RE = re.compile(
    r"^\s*(?P<name>\S+)\s*=\s*(?P<sec>\.[\w]+):(?P<va>0x[0-9A-Fa-f]+);"
    r".*?type:(?P<typ>\w+)(?:.*?size:(?P<size>0x[0-9A-Fa-f]+))?")


def load_symbols():
    by_va = {}
    with open(SYMBOLS) as fh:
        for line in fh:
            m = SYM_RE.match(line)
            if not m:
                continue
            if m.group("typ") != "function":
                continue
            va = int(m.group("va"), 16)
            size = int(m.group("size"), 16) if m.group("size") else 0
            by_va.setdefault(va, (m.group("name"), size, m.group("sec")))
    return by_va


def load_report():
    """name -> (match_pct, size, unit).  Presence == the pair EXISTS in the
    built report, which is the COMDAT-existence gate a repoint must pass."""
    out = {}
    r = json.loads(Path(REPORT).read_text())
    for u in r["units"]:
        for f in u.get("functions") or []:
            out.setdefault(f["name"], (f.get("match_percent_normalized"),
                                       int(f.get("size") or 0), u["name"]))
    return out, r["measures"]


def load_emitted():
    """Names with a real COMDAT in OUR CURRENT build (fresh, not BO-8's)."""
    import icf_alias_map_build as IAM
    emitted = {}
    for p in sorted(Path(OBJ_ROOT).rglob("*.obj")):
        try:
            for name, body, shape, sel in IAM._obj_bodies(p):
                emitted.setdefault(name, (len(body), p.name))
        except Exception:
            pass
    return emitted


# ---------------------------------------------------------------------------
def adjudicate(rows, pe, syms, report, emitted):
    out = []
    for idx, r in enumerate(rows):
        ev = []
        for name, vastr in sorted(r["vas"].items(), key=lambda kv: kv[1]):
            va = int(vastr, 16)
            sname, size, sec = syms.get(va, (None, 0, None))
            body, rsec = (pe.read(va, size) if size else (None, None))
            rp = report.get(name)
            ev.append(dict(
                name=name, va=vastr, retail_size=size, retail_sec=rsec or sec,
                symbols_name=sname,
                exact=body.hex() if body else None,
                masked=mask_body(body).hex() if body else None,
                norm=norm_body(body, va).hex() if body else None,
                in_our_build=name in emitted,
                our_size=emitted.get(name, (None, None))[0],
                our_obj=emitted.get(name, (None, None))[1],
                in_report=rp is not None,
                match_pct=rp[0] if rp else None,
                report_unit=rp[2] if rp else None))

        sizes = {e["retail_size"] for e in ev}
        readable = [e for e in ev if e["masked"]]
        verdict, why = None, None
        if len(readable) < 2:
            verdict, why = "UNREADABLE", "fewer than 2 mapped VAs have a size in symbols.txt"
        elif len(sizes) > 1:
            verdict, why = "FOLD_REFUTED_SIZE", "retail bodies differ in LENGTH: %s" % sorted(sizes)
        else:
            ex = {e["exact"] for e in readable}
            nm = {e["norm"] for e in readable}
            mk = {e["masked"] for e in readable}
            if len(ex) == 1:
                verdict, why = "TWIN_EXACT", "retail bodies byte-identical at every mapped VA"
            elif len(nm) == 1:
                verdict, why = ("TWIN_TRUE", "retail bodies identical once PC-relative branch "
                                             "fields are resolved to absolute targets: same "
                                             "callees, same immediates")
            elif len(mk) == 1:
                verdict, why = ("SKELETON_ONLY", "same instruction skeleton but %d distinct "
                                                 "resolved callee/immediate sets -> retail did "
                                                 "NOT fold these" % len(nm))
            else:
                verdict, why = "FOLD_REFUTED_BODY", ("same length, %d distinct masked bodies"
                                                     % len(mk))
        out.append(dict(row=idx, verdict=verdict, why=why,
                        n_members=r["n_members"], n_mapped=len(r["vas"]),
                        sources=r["sources"], hand_anchor=r.get("hand_anchor"),
                        evidence=ev))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--stats", action="store_true")
    a = ap.parse_args()

    pe = PE(BANDEXE)
    print("band.exe anchor OK: off(%#x) == %#x" % (ANCHOR_VA, ANCHOR_OFF),
          file=sys.stderr)
    syms = load_symbols()
    report, measures = load_report()
    emitted = load_emitted()
    print("symbols.txt functions=%d  report names=%d  emitted COMDATs=%d"
          % (len(syms), len(report), len(emitted)), file=sys.stderr)
    print("baseline: matched=%s masked_equal=%s honest=%s"
          % (measures["matched_functions"], measures.get("masked_equal_functions"),
             measures["matched_functions"] - measures.get("masked_equal_functions", 0)),
          file=sys.stderr)

    rows = json.loads(Path(WORKLIST).read_text())
    res = adjudicate(rows, pe, syms, report, emitted)
    Path(a.out).expanduser().write_text(json.dumps(res, indent=1))

    c = Counter(r["verdict"] for r in res)
    print("\n=== verdicts ===", file=sys.stderr)
    for k, v in c.most_common():
        print("  %-18s %4d" % (k, v), file=sys.stderr)
    if a.stats:
        miss = [(r["row"], e["name"]) for r in res for e in r["evidence"]
                if not e["in_our_build"]]
        print("\nmapped names with NO COMDAT in our build: %d" % len(miss),
              file=sys.stderr)
    print("\nwrote %s" % a.out, file=sys.stderr)


if __name__ == "__main__":
    main()
