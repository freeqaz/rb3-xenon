#!/usr/bin/env python3
"""Deep-classify the UNPAIRED-ANON blocker rows that hold units OUT of the
reachable ceiling.

WHY THIS EXISTS
---------------
tools/reachable_ceiling.py's `subclassify` runs ONLY on units where
`bucket == ANON_BLOCKED and sub100_total == 1`.  On the tree measured by lane
DT-1 that is 56 units.  But the CEILING is governed by a different predicate:
a unit sits outside AT_100+COMPLETABLE iff it holds >=1 UNPAIRED-ANON row
(verified: 768/770, the 2 exceptions being /Od units).  The population that is
ONE unpaired-anon row from entering the ceiling is 142 units, not 56 -- the
other 86 are MIXED (they also carry named or paired-anon blockers, which are
SOURCE work and do not affect ceiling membership).

So this reuses the SAME classification logic, keyed on the ceiling predicate
instead of on the bucket.  Nothing here is a new instrument: cls_of, the
anti-vacuity guard and the free/same-class/same-size test are imported from the
shipped tools so the two cannot drift.

Every label is named after WHAT WAS MEASURED, never after the inferred remedy.
"""
import argparse
import collections
import importlib.util
import json
import sys
from pathlib import Path


def load_mod(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--census", required=True)
    ap.add_argument("--max-anon", type=int, default=2,
                    help="classify units with anon_unpaired <= this")
    ap.add_argument("--json", dest="jsonout", default=None)
    args = ap.parse_args()

    root = Path(args.root).resolve()
    rc = load_mod(root / "tools/reachable_ceiling.py", "rc_dt1")
    soa, err = rc._load_soa(root)
    if soa is None:
        print(f"REFUSING: sub-classifier could not be loaded: {err}", file=sys.stderr)
        return 4

    census = json.loads(Path(args.census).read_text())
    addr2name, _mapvals, _n = rc.load_map(root / "scripts/target_symbol_map.json")

    # ------------------------------------------------------------------
    # ⚠ DO NOT SOURCE BLOCKER VAs FROM THE CENSUS.  `blocker_rows` is
    # truncated to 60 per unit AND sorted by DESCENDING mpn, so the mpn==0
    # unpaired-anon rows -- the only rows that govern ceiling membership --
    # sort LAST and are the FIRST to be dropped.  Reading the census gave
    # 8,099 of 16,906 rows (47.9%), biased against heavily-blocked units.
    # Read them from report.json instead, where nothing is truncated.
    # ------------------------------------------------------------------
    rep = json.loads((root / "build/45410914/report.json").read_text())
    unit_vas = {}
    for u in rep.get("units", []):
        vs = []
        for f in u.get("functions") or []:
            m = rc.ANON_RX.match(f["name"])
            if m and f["match_percent_normalized"] == 0.0:
                vs.append(f"0x{int(m.group(1), 16):08x}")
        unit_vas[u["name"]] = vs

    n_census = sum(len([b for b in r["blocker_rows"] if b["kind"] == "anon_unpaired"])
                   for r in census["units"])
    n_report = sum(len(v) for v in unit_vas.values())
    print(f"[coverage] unpaired-anon rows: {n_report} from report.json vs "
          f"{n_census} visible in the census (truncation loses "
          f"{n_report - n_census})\n")

    targets = [r for r in census["units"]
               if r["bucket"] in ("ANON_BLOCKED", "MIXED", "OD_REGION")
               and 1 <= r["rows"]["anon_unpaired"] <= args.max_anon]
    if not targets:
        print("REFUSING: zero units in the requested band -- that is a collapsed "
              "selection, not a finding.", file=sys.stderr)
        return 4

    out = []
    for r in targets:
        rec = {"unit": r["unit"], "bucket": r["bucket"],
               "source_path": r["source_path"],
               "auto_generated": r["auto_generated"],
               "anon_unpaired": r["rows"]["anon_unpaired"],
               "named_sub100": r["rows"]["named_sub100"],
               "anon_paired": r["rows"]["anon_paired_sub100"],
               "sub100_total": r["sub100_total"],
               "vanishes": r["unit_vanishes_if_drained"],
               "text_lo": r["text_lo"], "text_hi": r["text_hi"],
               "blockers": []}
        vas = unit_vas.get(r["unit"], [])
        if len(vas) != r["rows"]["anon_unpaired"]:
            rec["va_count_mismatch"] = [len(vas), r["rows"]["anon_unpaired"]]
        try:
            if r["auto_generated"]:
                for v in vas:
                    rec["blockers"].append({"va": v, "label": "AUTO_NO_SOURCE",
                                            "detail": "auto_generated split: no source, no compiled obj"})
                out.append(rec)
                continue
            _n2, tobj, tasm, cobj = soa.resolve_unit(r["unit"])
            if not cobj.exists():
                for v in vas:
                    rec["blockers"].append({"va": v, "label": "NO_COMPILED_OBJ",
                                            "detail": "no compiled obj -- no map row can ever fix it"})
                out.append(rec)
                continue
            T = soa.load_target(tobj, tasm)
            mapped = {addr2name[t.va] for t in T if t.va in addr2name}
            mapped_strip = {soa.anon_ns_strip(m) for m in mapped}
            unit_classes = {c for c in (rc.cls_of(m) for m in mapped) if c}
            C = soa.load_compiled(cobj)
            free = [c for c in C
                    if soa.anon_ns_strip(c.name) not in mapped_strip
                    and not soa.is_internal(c.name)]
            rec["n_unit_classes"] = len(unit_classes)
            rec["n_free"] = len(free)
            for v in vas:
                va = int(v, 16)
                d = {"va": v}
                tf = next((t for t in T if t.va == va), None)
                if tf is None:
                    d.update(label="TARGET_FN_NOT_IN_ASM",
                             detail="blocker VA absent from the dtk target asm (pin/asm drift)")
                    rec["blockers"].append(d)
                    continue
                d["size"] = tf.size
                g = rc.anti_vacuity(tf.masked)
                d["guard_ok"] = g["guard_ok"]
                d["n_real_words"] = g["n_real_words"]
                same_cls = [c for c in free if rc.cls_of(c.name) in unit_classes]
                same_sz = [c for c in same_cls if c.size == tf.size]
                d["n_free_sameclass"] = len(same_cls)
                d["n_free_sameclass_samesize"] = len(same_sz)
                d["samesize_names"] = sorted(c.name for c in same_sz)[:6]
                d["exact_masked_match"] = sorted(
                    c.name for c in same_sz if c.masked == tf.masked)[:5]
                d["sameclass_nearest"] = sorted(
                    ([c.size, c.name] for c in same_cls),
                    key=lambda z: abs(z[0] - tf.size))[:4]
                if not unit_classes:
                    d["label"] = "NO_CLASS_ANCHOR"
                elif same_sz:
                    d["label"] = ("MAP_FIXABLE_CANDIDATE" if g["guard_ok"]
                                  else "MAP_FIXABLE_UNADJUDICABLE")
                elif same_cls:
                    d["label"] = "SAMECLASS_DIFFSIZE"
                else:
                    d["label"] = "NO_SAMECLASS"
                rec["blockers"].append(d)
        except Exception as e:
            rec["error"] = f"{type(e).__name__}: {e}"
            for v in vas:
                rec["blockers"].append({"va": v, "label": "TOOLING_ERROR",
                                        "detail": rec["error"]})
        out.append(rec)

    errs = sum(1 for r in out if r.get("error"))
    if errs == len(out):
        print("REFUSING: every unit errored -- collapsed pipeline, not a finding.",
              file=sys.stderr)
        return 4

    print(f"classified {len(out)} units ({errs} tooling errors)\n")
    for band in (1, 2):
        sel = [r for r in out if r["anon_unpaired"] == band]
        if not sel:
            continue
        print(f"=== anon_unpaired == {band}  ({len(sel)} units) ===")
        c = collections.Counter(b["label"] for r in sel for b in r["blockers"])
        for k, v in c.most_common():
            print(f"   {k:30s} {v:5d}")
        print()
    print("=== units whose EVERY unpaired-anon blocker is MAP_FIXABLE_CANDIDATE ===")
    ready = [r for r in out if r["blockers"]
             and all(b["label"] == "MAP_FIXABLE_CANDIDATE" for b in r["blockers"])]
    print(f"   {len(ready)} units")
    for r in sorted(ready, key=lambda r: (r["anon_unpaired"], r["unit"])):
        b = r["blockers"][0]
        print(f"   {r['unit'][:52]:52s} n={r['anon_unpaired']} sub100={r['sub100_total']:3d} "
              f"{b['size']:5d}B free_ss={b['n_free_sameclass_samesize']} "
              f"exact={len(b['exact_masked_match'])} {b['samesize_names'][:1]}")
    if args.jsonout:
        Path(args.jsonout).write_text(json.dumps(out, indent=1))
        print(f"\nwrote {args.jsonout}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
