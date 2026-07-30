#!/usr/bin/env python3
"""laneBV3 -- find map rows that can NEVER match, and the ones that are fixable.

A DIFFERENT defect class from the collision channel, and the only one in this
family that can actually pay.

A collision repoint moves a name between two reloc-masked byte twins that BOTH
already score 100%, so dmatched is identically 0 (see repoint_supply.py and the
laneBV3 section of README.md). By contrast a **size-impossible** row -- one whose
mapped target function has a different size from the base COMDAT of the same
name in the same unit -- can never reach 100%. It is dead weight, and worse, it
squats on a VA. Repointing it is strictly non-negative:

    outcome(current) = False  (size differs => cannot be byte-equal)
    outcome(candidate) = True (exact masked-byte equality at the right size)
    => dmatched = +1

A candidate is only proposed when it is UNMAPPED, in the SAME unit (objdiff pairs
target<->base per unit by name), the right size, and masked-byte-identical.

⚠ EXACT MASKED BYTES IS NOT IDENTITY. That is the whole premise of this
directory: masked twins differ only in relocations. `?SetType@PracticeSection@@`
was asserted onto a perfectly-sized, byte-equal 316 B body that turned out to be
`CharTransCopy::SetType`. So every candidate is put through the same
class-consistency test as collision_classcheck.py, and rows with more than one
candidate are reported as AMBIGUOUS rather than guessed.

Measured on the current map: 21,482 rows have a same-unit base COMDAT; **1,867
are size-impossible**; only 19 of those have an exact-byte right-sized unmapped
target available in-unit.

USAGE
    size_impossible_scan.py --worktree WT --lblidx LBL [--out rows.json]
"""
import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R          # noqa: E402
import relocdisc as D         # noqa: E402

STR = re.compile(rb"^([\x20-\x7e]{2,63})\x00")
IDENT = re.compile(r"^[A-Za-z_]\w*$")


def cls_of(m):
    x = re.match(r"\?\?[0-9A-Z_]?([A-Za-z_]\w*)@@", m)
    if x:
        return x.group(1)
    x = re.match(r"\?[A-Za-z_]\w*@([A-Za-z_]\w*)@@", m)
    return x.group(1) if x else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--lblidx", required=True)
    ap.add_argument("--out")
    args = ap.parse_args()

    wt = Path(args.worktree).resolve()
    S = R.load_S(wt)
    lbl = {int(k): bytes.fromhex(v) for k, v in
           json.loads(Path(args.lblidx).read_text()).items()}
    cur = json.loads((wt / "scripts/target_symbol_map.json").read_text())
    va2n = {}
    for k, v in cur.items():
        if isinstance(v, str) and k.startswith("0x"):
            try:
                va2n[int(k, 16)] = v
            except ValueError:
                pass

    units, allt = [], {}
    for u, tobj, tasm, cobj in D.unit_iter(wt):
        if not (tasm.exists() and cobj.exists()):
            continue
        try:
            tf = R.target_funcs(tasm)
            bf, _ = R.base_funcs(cobj)
        except Exception:
            continue
        units.append((u, tf, {S.anon_ns_strip(f["name"]): f for f in bf}))
        for va, ti in tf.items():
            allt.setdefault(va, ti)
    print(f"indexed {len(units)} units / {len(allt)} target fns", file=sys.stderr)

    def sstr(va):
        ti = allt.get(va)
        if not ti:
            return None
        for off, tok in ti["relocs"]:
            m = re.match(r"lbl_([0-9A-Fa-f]{8})$", tok)
            if m:
                b = lbl.get(int(m.group(1), 16))
                if b:
                    mm = STR.match(b)
                    if mm:
                        return mm.group(1).decode("latin1")
        return None

    def classcheck(va, want):
        """CONSISTENT / CONTRADICTED / NO_EVIDENCE / NO_CLASS_TOKEN"""
        ti = allt.get(va)
        if ti is None or not want:
            return "NO_CLASS_TOKEN", []
        toks, evid = set(), []
        for off, tok in ti["relocs"]:
            m = re.match(r"fn_([0-9A-Fa-f]{8})$", tok)
            if not m:
                continue
            cva = int(m.group(1), 16)
            nm, s = va2n.get(cva), sstr(cva)
            evid.append((nm or f"fn_{cva:08x}", s))
            if nm and cls_of(nm):
                toks.add(cls_of(nm))
            if s and IDENT.match(s):
                toks.add(s)
        own = sstr(va)
        if own and IDENT.match(own):
            toks.add(own)
        if not toks:
            return "NO_EVIDENCE", evid
        return ("CONSISTENT" if want in toks else "CONTRADICTED"), evid

    scanned = bad = 0
    out = []
    for u, tf, bmap in units:
        for va, ti in tf.items():
            n = va2n.get(va)
            if not n:
                continue
            b = bmap.get(S.anon_ns_strip(n))
            if b is None:
                continue
            scanned += 1
            if b["size"] == ti["size"]:
                continue
            bad += 1
            cands = [v2 for v2, t2 in tf.items()
                     if v2 not in va2n and t2["size"] == b["size"]
                     and bytes(t2["masked"]) == bytes(b["masked"])]
            if not cands:
                continue
            want = cls_of(n)
            recs = []
            for c in sorted(cands):
                v, evid = classcheck(c, want)
                recs.append(dict(va=f"0x{c:08x}", verdict=v,
                                 evid=[(a, s) for a, s in evid[:4]]))
            good = [r for r in recs if r["verdict"] == "CONSISTENT"]
            status = ("SHIP" if len(good) == 1 and len(recs) == 1 else
                      "AMBIGUOUS" if len(recs) > 1 else
                      "UNCORROBORATED")
            out.append(dict(unit=u, name=n, cur=f"0x{va:08x}", want_cls=want,
                            tsize=ti["size"], bsize=b["size"],
                            status=status, cands=recs))

    print(f"\nmap rows with same-unit base COMDAT : {scanned}")
    print(f"  SIZE-IMPOSSIBLE (never matchable) : {bad}")
    print(f"  with an exact-byte in-unit target : {len(out)}")
    t = {}
    for r in out:
        t[r["status"]] = t.get(r["status"], 0) + 1
    for k, v in sorted(t.items(), key=lambda kv: -kv[1]):
        print(f"    {k:16s} {v:3d}")
    for r in out:
        print(f"\n[{r['status']}] {r['unit'][:44]} :: {r['name'][:66]}")
        print(f"   cur {r['cur']} tsz={r['tsize']} bsz={r['bsize']} want={r['want_cls']}")
        for c in r["cands"]:
            print(f"     -> {c['va']} {c['verdict']}")
            for a, s in c["evid"]:
                print(f"          {a[:64]:66s} str={s!r}")
    if args.out:
        Path(args.out).write_text(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
