#!/usr/bin/env python3
"""W16-AA: WHERE does an UNDECIDED_MASKED membership's discriminator live?

W16-S's census leaves a membership `UNDECIDED_MASKED` when retail@X compares EQ
to BOTH our COMDAT for the folded spelling N and our COMDAT for the survivor S,
under `retail_compare`, whose masking has TWO distinct channels:

  (a) a `b`/`bl` (AA=0) whose retail destination is UNNAMED in
      target_symbol_map.json -- the tool decodes the displacement, finds no name,
      and SKIPS the check.  This channel is CLOSABLE: name the destination.
  (b) ANY relocated word that is not a `b`/`bl` -- a HA16/LO16 data reference, a
      pointer slot -- for which `retail_compare` verifies the 6-bit OPCODE ONLY
      and never looks at the destination at all.  This channel is NOT closable
      by naming anything: the tool structurally does not consult a name there.

⛔ THE DISTINCTION IS THE WHOLE POINT OF THIS TOOL.  The lane brief's lever
("identify the unnamed destination on retail bytes") only reaches channel (a).
A membership whose our-N-vs-our-S discriminator sits in channel (b) will read
UNDECIDED_MASKED no matter how many map names are added, so budgeting map work
against it is spending on a row that cannot move.

The discriminator itself is computed ENTIRELY OUR-SIDE (our N's relocations vs
our S's relocations at equal offsets).  No map lookup, no retail address, hence
not subject to either masking channel -- the same argument
`tools/ourside_fold_sweep.py` makes.  Retail is consulted only afterwards, to
say what sits at the offsets already known to discriminate.
"""
import collections
import glob
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from comdat_bytes import comdats                        # noqa: E402
from wrong_callee_triage import Image, load_sizes       # noqa: E402
import w16s_alias_census as CEN                          # noqa: E402

BUILD_ID = "45410914"


def our_index():
    fns = collections.defaultdict(set)
    where = collections.defaultdict(set)
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"),
                       recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for n, v in c.items():
            if not v.get("is_code"):
                continue
            rel = tuple(sorted((o, s, t) for o, s, t in (v["fn_relocs"] or [])
                               if s != "@comp.id"))
            fns[n].add((v["fn_raw"], rel))
            where[n].add(p)
    return fns, where


def rep(vs):
    """Same deterministic tie-break the census uses."""
    return max(vs, key=lambda kv: (len(kv[0]), kv[0], kv[1]))


def masked(raw, rel):
    m = bytearray(raw)
    for o, _s, _t in rel:
        m[o:o + 4] = b"\0\0\0\0"
    return bytes(m)


def classify(off, our_word, img, byva, va, size):
    """What sits at this offset, and is the masking channel closable?"""
    op = our_word >> 26
    info = {"offset": off, "our_opcode": op}
    if op == 18:
        info["kind"] = "branch(b/bl)"
    elif op == 16:
        info["kind"] = "branch-cond"
    elif op == 15:
        info["kind"] = "lis/addis (HA16 data ref)"
    elif op in (14, 24, 25):
        info["kind"] = "addi/ori (LO16 data ref)"
    elif op in (32, 36, 48, 52, 34, 38):
        info["kind"] = "load/store displacement"
    else:
        info["kind"] = "opcode %d" % op
    n = size.get(va)
    o = img.off(va)
    if n and o is not None and off + 4 <= n:
        rw = struct.unpack_from(">I", img.data, o + off)[0]
        info["retail_word"] = "%08x" % rw
        if (rw >> 26) == 18 and not (rw & 2):
            d = rw & 0x03FFFFFC
            if d & 0x02000000:
                d -= 0x04000000
            dest = (va + off + d) & 0xFFFFFFFF
            info["retail_dest"] = "0x%08x" % dest
            info["retail_dest_name"] = byva.get(dest)
            info["channel"] = "a:CLOSABLE-BY-NAMING" if not byva.get(dest) \
                else "a:already-named"
        else:
            info["channel"] = "b:NOT-CLOSABLE-BY-NAMING (non-branch reloc)"
    return info


def main():
    census = json.load(open(ROOT / "docs/decomp/W16S_alias_census_2026-09-14.json"))
    um = [r for r in census if r["verdict"] == "UNDECIDED_MASKED"]
    um.sort(key=lambda r: -int(r["bytes"]))
    topn = int(sys.argv[1]) if len(sys.argv) > 1 else 10

    fns, where = our_index()
    img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
    size = load_sizes()
    smap = json.load(open(ROOT / "scripts/target_symbol_map.json"))
    byva = {}
    for k, v in smap.items():
        try:
            byva[int(k, 16)] = v
        except (ValueError, TypeError):
            pass

    closure = CEN.icf_closure(fns)

    out = []
    seen = set()
    for r in um:
        key = (r["addr"], r["survivor"], r["folded"])
        if key in seen:
            continue
        seen.add(key)
        if len(out) >= topn:
            break
        N, S = r["folded"], r["survivor"]
        va = int(r["addr"], 16)
        fv, sv = fns.get(N), fns.get(S)
        rec = {"gi": r["gi"], "addr": r["addr"], "bytes": r["bytes"],
               "survivor": S, "folded": N}
        if not fv or not sv:
            rec["status"] = "MISSING_COMDAT"
            out.append(rec)
            continue
        # ⛔ REPRESENTATIVE SELECTION IS LOAD-BEARING.  The census does not diff
        # the LONGEST variant; it walks each side's variants (longest first) and
        # stops at the FIRST that compares EQ to retail@X.  Diffing a different
        # variant than the one the verdict rests on describes a pair the census
        # never compared -- it produced `masked_bytes_equal=False` on gi=109,
        # which is impossible for a genuine UNDECIDED and is how the slip was
        # caught.  Select exactly as the census does, and SAY when no variant
        # of a side reaches EQ.
        def pick(vs):
            best = None
            for raw, rel in sorted(vs, key=lambda kv: (-len(kv[0]), kv[0], kv[1])):
                v, _d, _mo = CEN.retail_compare(img, size, byva, va, raw, rel, closure)
                if v == "EQ":
                    return (raw, rel), True
                if best is None:
                    best = (raw, rel)
            return best, False
        (nraw, nrel), nEQ = pick(fv)
        (sraw, srel), sEQ = pick(sv)
        rec["N_reaches_EQ_vs_retail"] = nEQ
        rec["S_reaches_EQ_vs_retail"] = sEQ
        rec["N_variants"], rec["S_variants"] = len(fv), len(sv)
        rec["our_N_obj"] = sorted(where[N])[0].split("/src/")[-1]
        rec["our_S_obj"] = sorted(where[S])[0].split("/src/")[-1]
        rec["len_N"], rec["len_S"] = len(nraw), len(sraw)
        if masked(nraw, nrel) != masked(sraw, srel):
            rec["masked_bytes_equal"] = False
        else:
            rec["masked_bytes_equal"] = True
        shapeN = tuple((o, t) for o, _s, t in nrel)
        shapeS = tuple((o, t) for o, _s, t in srel)
        rec["reloc_shape_equal"] = (shapeN == shapeS)
        dN = {o: s for o, s, _t in nrel}
        dS = {o: s for o, s, _t in srel}
        diffs = []
        for off in sorted(set(dN) | set(dS)):
            a, b = dN.get(off), dS.get(off)
            if a != b:
                w = struct.unpack_from(">I", nraw, off)[0] if off + 4 <= len(nraw) else 0
                c = classify(off, w, img, byva, va, size)
                c["N_target"], c["S_target"] = a, b
                diffs.append(c)
        rec["discriminators"] = diffs
        rec["n_discriminators"] = len(diffs)
        chans = collections.Counter(d.get("channel", "?") for d in diffs)
        rec["channels"] = dict(chans)
        out.append(rec)
    print(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
