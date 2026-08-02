#!/usr/bin/env python3
"""funclet_credit_audit.py -- is a 100% credit BENIGN or OVER-COUNT?  (lane BO-8)

THE QUESTION
    ~53% of `matched_functions` are EH-funclet-shaped target symbols
    (`fn_<addr>`, `__unwind$N`, `__catch$N`, `??__E*`, `??__F*`) that objdiff
    paired NOT by name but by masked-byte signature, and scored under
    `functionRelocDiffs = None`, which normalizes relocation targets away.  So
    for that half, callee identity is unverified in both the pairing and the
    scoring.  Is that credit real?

    It decomposes into TWO INDEPENDENT DEFECTS.  Conflating them is the mistake
    this tool exists to prevent.

    AXIS 1 -- SUPPLY.  `pair_funclets_by_bytes` pass 2b (objdiff
        `objdiff-core/src/diff/mod.rs:1554`) pairs overflow target funclets
        MANY-TO-ONE onto a base funclet another target already owns, WITHOUT
        marking it used.  N target symbols each score 100% against 1 body we
        compiled.  The surplus is machine code we never generated.
        Already disclosed per item as `masked_equal` and summed into
        `measures.masked_equal_functions` -- read it, do not re-derive it.
        NOTE (2026-08-02, lane CZ-4): `masked_equal_functions` is no longer
        JUST this over-subscription surplus.  It now discloses EVERY funclet
        byte-signature pairing (`SymbolDiff::masked_equal_symbol`), of which
        over-subscription is a small subset -- 1,130 of 22,632 on rb3-xenon.
        The wider class is SUPPLY-BACKED (we did emit those bodies); what it
        discloses is that the per-row ATTRIBUTION is unverified, because the
        pairing compared bodies with relocation targets masked.  So this axis
        is now "attribution unverified", not "code we never generated".

    AXIS 2 -- IDENTITY.  Even a supply-backed pair can point somewhere else:
        our `bl` goes to `??1UIListProvider@@` where retail's goes to
        `??1Callback@ContentMgr@@`.  Decided by
        `scripts/harvest/reloc_correspondence.py --census`, whose content oracle
        reads the DECOMPRESSED RETAIL PE (`orig/45410914/band.exe`) and so
        dissolves the ICF confound (folded bytes ARE our bytes).

★ WHAT DOES *NOT* DECIDE THIS: the ICF alias map.
    `build/45410914/icf_aliases.map` feeds objdiff's `symbol_equivalences`, which
    is consumed ONLY by `reloc_eq`.  Under `functionRelocDiffs = None` -- which
    `objdiff-cli report generate` HARDCODES (`objdiff-cli/src/cmd/report.rs:392`)
    -- `reloc_eq` returns true on flag equality BEFORE it ever looks at a name
    (`objdiff-core/src/diff/code.rs:874-897`).  The map is dead code on the
    report path.  Measured: growing it from 3 groups to 1,408 moved
    `matched_functions` by exactly 0.  Populate it for diff-time noise and for
    analysis oracles -- never as a prerequisite for this decision.

INPUT
    build/45410914/report.json                (per-item `masked_equal`)
    a census NDJSON from reloc_correspondence.py --census --out <f>

USAGE
    python3 scripts/harvest/funclet_credit_audit.py \
        --census ~/tmp/census_perm.ndjson [--census-strict ~/tmp/census_cons.ndjson]
"""

import argparse
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent


def is_funclet_like(n):
    """objdiff's own predicate (objdiff-core/src/diff/mod.rs:815)."""
    if n.startswith("__unwind$"):
        return n[9:].isdigit()
    if n.startswith("__catch$"):
        return n[8:].isdigit()
    if n.startswith("__unwind__merged_"):
        return True
    if n.startswith("fn_"):
        r = n[3:]
        return len(r) == 8 and all(c in "0123456789abcdefABCDEF" for c in r)
    return n.startswith("??__E") or n.startswith("??__F")


def load_report(path):
    r = json.load(open(path))
    at100 = {}
    for u in r["units"]:
        for f in u.get("functions") or []:
            if f.get("match_percent_normalized") == 100.0:
                at100[(u["name"], f["name"])] = dict(
                    size=int(f["size"]),
                    masked_equal=bool(f.get("masked_equal")),
                    funclet=is_funclet_like(f["name"]))
    return r["measures"], at100


def load_census(path):
    """reloc_correspondence.py --out writes a JSON ARRAY; accept NDJSON too."""
    txt = Path(path).read_text()
    rows = []
    try:
        rows = json.loads(txt)
    except Exception:
        for line in txt.splitlines():
            line = line.strip()
            if line:
                try:
                    rows.append(json.loads(line))
                except Exception:
                    pass
    return {(d["unit"], d["name"]): d.get("verdict", "?")
            for d in rows if "unit" in d and "name" in d}


EVID = ("CORRESPONDING", "NO_RELOCS")
UNDEC = ("UNRESOLVABLE", "NO_BASE_PAIR", "SHAPE_MISMATCH", "NO_TARGET_SYM",
         "OBJ_ERROR", "?")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", default=str(ROOT / "build/45410914/report.json"))
    ap.add_argument("--census", required=True)
    ap.add_argument("--census-strict")
    ap.add_argument("--json-out")
    args = ap.parse_args()

    measures, at100 = load_report(args.report)
    cen = load_census(args.census)
    cen_s = load_census(args.census_strict) if args.census_strict else None

    M = measures["matched_functions"]
    ME = measures.get("masked_equal_functions", 0)
    print("=" * 74)
    print("matched_functions          %6d" % M)
    print("  funclet-like at 100%%      %6d  (%.1f%%)"
          % (sum(1 for v in at100.values() if v["funclet"]),
             100.0 * sum(1 for v in at100.values() if v["funclet"]) / M))
    print("  named at 100%%             %6d" % sum(1 for v in at100.values() if not v["funclet"]))
    print()
    print("AXIS 1 -- SUPPLY (objdiff's own disclosure)")
    print("  masked_equal_functions   %6d   funclet byte-signature pairings "
          "(attribution unverified; pass-2b surplus is a subset)" % ME)
    nf = sum(1 for v in at100.values() if v["masked_equal"] and not v["funclet"])
    print("  ... of which NAMED       %6d   (structurally must be 0)" % nf)
    print()

    def tab(label, cenmap):
        print("AXIS 2 -- IDENTITY (%s)" % label)
        rows = defaultdict(Counter)
        for k, v in at100.items():
            vd = cenmap.get(k, "?")
            pop = ("funclet/surplus" if v["masked_equal"] else
                   ("funclet/backed" if v["funclet"] else "named"))
            rows[pop][vd] += 1
        allv = sorted({v for c in rows.values() for v in c})
        hdr = "  %-16s %6s | " % ("population", "n") + " ".join("%14s" % v[:14] for v in allv)
        print(hdr)
        tot = Counter()
        for pop in ("named", "funclet/backed", "funclet/surplus"):
            c = rows[pop]
            n = sum(c.values())
            tot.update(c)
            print("  %-16s %6d | " % (pop, n) + " ".join("%14d" % c[v] for v in allv))
        n = sum(tot.values())
        print("  %-16s %6d | " % ("ALL", n) + " ".join("%14d" % tot[v] for v in allv))
        div = tot.get("DIVERGENT", 0)
        ev = sum(tot[v] for v in EVID if v in tot)
        print("  -> evidenced %d (%.1f%%)   DIVERGENT %d (%.1f%%)   undecidable %d (%.1f%%)"
              % (ev, 100.0 * ev / n, div, 100.0 * div / n, n - ev - div,
                 100.0 * (n - ev - div) / n))
        print()
        return rows, tot

    rows_p, tot_p = tab("permissive", cen)
    if cen_s:
        rows_s, tot_s = tab("conservative / --strict-consistency", cen_s)

    # ---- the floor -------------------------------------------------------
    def floor(tot, rows):
        # surplus is unsupported regardless of identity; divergent among the
        # SUPPORTED population is a second, disjoint deduction.
        surplus = sum(rows["funclet/surplus"].values())
        div_backed = rows["funclet/backed"].get("DIVERGENT", 0) + rows["named"].get("DIVERGENT", 0)
        return surplus, div_backed

    print("HONEST FLOOR")
    s, d = floor(tot_p, rows_p)
    print("  matched_functions                       %6d" % M)
    print("  - axis 1 surplus (unsupported)          %6d" % s)
    print("  - axis 2 DIVERGENT among supported      %6d   (permissive)" % d)
    print("  = floor                                 %6d" % (M - s - d))
    if cen_s:
        s2, d2 = floor(tot_s, rows_s)
        print("  - axis 2 DIVERGENT among supported      %6d   (conservative)" % d2)
        print("  = ceiling on the corrected count        %6d" % (M - s2 - d2))

    if args.json_out:
        Path(args.json_out).write_text(json.dumps(dict(
            matched=M, masked_equal=ME,
            permissive={k: dict(v) for k, v in rows_p.items()},
            conservative=({k: dict(v) for k, v in rows_s.items()} if cen_s else None),
        ), indent=1) + "\n")


if __name__ == "__main__":
    main()
