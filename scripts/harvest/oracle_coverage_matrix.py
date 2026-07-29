#!/usr/bin/env python3
"""Three-way oracle-coverage matrix for RB3-Xbox360: RB3-360 x rb3-Wii x DC3.

Answers "which RB3-360 code has no rb3-Wii counterpart?" at TU granularity.
Findings + methodology: docs/plans/rb3-360-vs-wii-coverage-2026-07-29.md

KEY METHODOLOGICAL CHOICE -------------------------------------------------
The oracles are the shipped BINARIES' link maps, not the decompiled source
trees.  Grepping ../rb3/src conflates "the Wii SKU lacks it" with "the Wii
decomp hasn't got there yet".  band_r_wii.map names all 42,599 .text symbols
in the shipped Wii binary, decompiled or not, so it has no such gap.

  Oracle W : ../rb3/orig/SZBE69_B8/files/band_r_wii.map   (CodeWarrior map)
  Oracle D : ../dc3-decomp/orig/373307D9/ham_xbox_r.map   (MSVC map, leaked PDB)
  Universe : build/45410914/report.json

Join key   : canonical TU identity <module>/<subdir>/<Stem>, case-folded,
             with a stem-only fallback (reported separately as W:stem).

Cells      : (a) DC3 only   (b) Wii only   (c) BOTH   (d) NEITHER
             (u) UNATTRIBUTED - 360 fns not pinned to any source file.

*** READ THIS BEFORE QUOTING CELL (d) ***
Cell (d) is near-empty BY CONSTRUCTION, not because the 360 has no exclusive
code.  We pin a TU in splits.txt *because* an oracle told us where to look, so
oracle-driven attribution structurally cannot discover 360-exclusive code.  The
informative outputs of this tool are:
  --reverse : Wii TUs we have NOT located in the 360 binary (NOT selection-
              biased, because the Wii map is a complete census).  This is where
              the actionable work is.
  --control : two-sided a-priori control measuring this join's own recall and
              specificity (currently 1.0000 / 1.0000, n=673 / n=22).

Read-only.  Touches no tracked file, runs no build.
"""
import argparse
import collections
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SIB = os.path.dirname(REPO)
WII_MAP = f"{SIB}/rb3/orig/SZBE69_B8/files/band_r_wii.map"
WII_SRC = f"{SIB}/rb3/src"
DC3_MAP = f"{SIB}/dc3-decomp/orig/373307D9/ham_xbox_r.map"
DC3_SRC = f"{SIB}/dc3-decomp/src"
REPORT = f"{REPO}/build/45410914/report.json"
OUTDIR = os.path.expanduser("~/tmp/laneBB")

# CodeWarrior .text-layout row: offset size vaddr fileoff [align] SYMBOL \t TU
WII_LINE = re.compile(
    r"^\s+([0-9a-f]{8}) ([0-9a-f]{6}) ([0-9a-f]{8}) ([0-9a-f]{8})\s+(\d+)?\s*(\S+)\s*\t(.*)$")
# lib.a C:\hproj\band3_wii\<module>\src\<subdir...>\wii_release\<Stem>.o
WII_TU = re.compile(
    r"band3_wii[\\/](?P<mod>[^\\/]+)[\\/]src[\\/](?P<sub>.*?)[\\/]?wii_release[\\/](?P<stem>[^\\/]+)\.o$",
    re.I)
DC3_LINE = re.compile(
    r"^\s+[0-9]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s+(?:f\s+)?(?:i\s+)?(.*?)\s*$")
# Wii platform layer: absent from the 360 by construction, not a work item.
WII_PLATFORM = re.compile(
    r"(^|/)(synthwii|usbwii|rndwii)/|wii|revolution|nintendo|wpad|homemenu|memcard", re.I)


def canon(*parts):
    return "/".join(p for p in parts if p).lower()


def parse_wii(path):
    """-> (tus: canon -> {fns,bytes,raw}, stem_idx, unmapped_counter)"""
    txt = open(path, errors="replace").read().split("\n")
    try:
        s = next(i for i, l in enumerate(txt) if l.strip() == ".text section layout")
        e = next(i for i, l in enumerate(txt) if l.strip() == ".ctors section layout")
    except StopIteration:
        sys.exit(f"error: {path} has no .text/.ctors section layout blocks")
    tus, stem_idx, unmapped = {}, collections.defaultdict(set), collections.Counter()
    for l in txt[s:e]:
        m = WII_LINE.match(l)
        if not m:
            continue
        _, size, _va, _fo, _al, name, tu = m.groups()
        if name in (".text", "*fill*"):
            continue
        tu = tu.strip()
        t = WII_TU.search(tu)
        if not t:                      # Wii SDK / runtime libs - excluded by design
            unmapped[tu] += 1
            continue
        key = canon(t.group("mod"), t.group("sub").replace("\\", "/").strip("/"),
                    t.group("stem"))
        d = tus.setdefault(key, {"fns": 0, "bytes": 0, "raw": tu})
        d["fns"] += 1
        d["bytes"] += int(size, 16)
        stem_idx[t.group("stem").lower()].add(key)
    return tus, stem_idx, unmapped


def parse_dc3(path):
    objs = collections.Counter()
    for l in open(path, errors="replace"):
        m = DC3_LINE.match(l)
        if m and m.group(3).endswith(".obj"):
            objs[m.group(3).split(":")[-1]] += 1
    return objs


def index_tree(root):
    """-> (set of canon rel keys, stem -> {canon keys})"""
    keys, stem_idx = set(), collections.defaultdict(set)
    for dp, _, files in os.walk(root):
        for f in files:
            if not f.endswith((".cpp", ".c")):
                continue
            rel = os.path.relpath(os.path.join(dp, f), root)
            k = rel[: rel.rfind(".")].replace("\\", "/").lower()
            keys.add(k)
            stem_idx[os.path.splitext(f)[0].lower()].add(k)
    return keys, stem_idx


def classify(report, wii_tus, wii_stem, wii_src_stem, dc3_keys, dc3_stem, dc3_mapstem):
    per_unit = []
    for u in report["units"]:
        md = u.get("metadata") or {}
        m = u["measures"]
        nf, nb = int(m.get("total_functions", 0)), int(m.get("total_code", 0))
        mf = int(m.get("matched_functions", 0))
        sp = md.get("source_path")
        cat = ",".join(md.get("progress_categories") or [])
        if md.get("auto_generated") or not sp:
            cell, wk, dk, how = "u", None, None, "unattributed"
        else:
            rel = sp[4:] if sp.startswith("src/") else sp
            key = rel[: rel.rfind(".")].lower() if "." in rel else rel.lower()
            stem = os.path.basename(key)
            wk, how_w = (key, "exact") if key in wii_tus else (None, "")
            if not wk and stem in wii_stem:
                wk, how_w = sorted(wii_stem[stem])[0], "stem"
            if not wk and stem in wii_src_stem:
                wk, how_w = sorted(wii_src_stem[stem])[0], "srcstem"
            dk, how_d = (key, "exact") if key in dc3_keys else (None, "")
            if not dk and stem in dc3_stem:
                dk, how_d = sorted(dc3_stem[stem])[0], "stem"
            if not dk and stem in dc3_mapstem:
                dk, how_d = stem, "mapstem"
            cell = {(1, 1): "c", (1, 0): "a", (0, 1): "b", (0, 0): "d"}[
                (1 if dk else 0, 1 if wk else 0)]
            how = f"W:{how_w or '-'} D:{how_d or '-'}"
        per_unit.append({"unit": u["name"], "src": sp, "cat": cat, "cell": cell,
                         "wii": wk, "dc3": dk, "how": how,
                         "fns": nf, "bytes": nb, "matched": mf})
    return per_unit


LBL = {"a": "(a) DC3 only", "b": "(b) Wii only", "c": "(c) BOTH",
       "d": "(d) NEITHER", "u": "(u) UNATTRIBUTED"}


def report_matrix(per_unit, total_fns):
    by = collections.defaultdict(lambda: [0, 0, 0, 0])
    bycat = collections.defaultdict(lambda: collections.Counter())
    for r in per_unit:
        b = by[r["cell"]]
        b[0] += 1; b[1] += r["fns"]; b[2] += r["bytes"]; b[3] += r["matched"]
        bycat[r["cat"] or "<none>"][r["cell"]] += r["fns"]
    print("\n=== 4-cell matrix (pinned 360 units) + unattributed residue ===")
    for c in "cabdu":
        u_, f_, b_, m_ = by[c]
        print(f"{LBL[c]:20s} units={u_:5d} fns={f_:6d} ({100*f_/total_fns:5.1f}%) "
              f"code={b_/1e6:6.3f}MB matched={m_:6d}")
    print(f"{'TOTAL':20s} units={sum(by[c][0] for c in 'cabdu'):5d} "
          f"fns={sum(by[c][1] for c in 'cabdu'):6d} "
          f"code={sum(by[c][2] for c in 'cabdu')/1e6:6.3f}MB")
    print("\n=== by category x cell (fns) ===")
    print(f"{'cat':12s}" + "".join(f"{LBL[c][:10]:>12s}" for c in "cabdu"))
    for cat in sorted(bycat):
        print(f"{cat:12s}" + "".join(f"{bycat[cat][c]:12d}" for c in "cabdu"))
    print("\n  NOTE: cell (d) is near-empty BY CONSTRUCTION (selection bias) --")
    print("        we pin a TU because an oracle located it. See --reverse.")


def report_reverse(wii_tus, per_unit):
    """Wii TU census -> have we located it in the 360 binary? NOT selection-biased."""
    pin, pinstem = set(), collections.defaultdict(set)
    for r in per_unit:
        if not r["src"]:
            continue
        rel = r["src"][4:] if r["src"].startswith("src/") else r["src"]
        k = rel[: rel.rfind(".")].lower()
        pin.add(k)
        pinstem[os.path.basename(k)].add(k)
    tree, treestem = index_tree(f"{REPO}/src")
    dc3stem = {o[:-4].lower() for o in parse_dc3(DC3_MAP)}
    rows, agg = [], collections.defaultdict(collections.Counter)
    for k, v in wii_tus.items():
        mod, stem = k.split("/")[0], os.path.basename(k)
        if k in pin or stem in pinstem:
            st = "PINNED"
        elif k in tree or stem in treestem:
            st = "IN_TREE_UNPINNED"
        elif stem in dc3stem:
            st = "DC3_ONLY"
        else:
            st = "ABSENT"
        agg[mod][st] += 1
        agg[mod][st + "_fns"] += v["fns"]
        agg[mod][st + "_b"] += v["bytes"]
        rows.append((k, mod, stem, st, v["fns"], v["bytes"]))
    print("\n=== REVERSE: Wii Harmonix TU census -> located in RB3-360 work? ===")
    for mod in ("system", "band3", "network"):
        c = agg[mod]
        print(f"\n {mod}:")
        for st in ("PINNED", "IN_TREE_UNPINNED", "DC3_ONLY", "ABSENT"):
            print(f"   {st:18s} tus={c[st]:5d} wii_fns={c[st+'_fns']:6d} "
                  f"wii_bytes={c[st+'_b']/1e6:6.3f}MB")
    ab = [r for r in rows if r[3] == "ABSENT" and r[1] != "network"]
    plat = [r for r in ab if WII_PLATFORM.search(r[0])]
    real = [r for r in ab if not WII_PLATFORM.search(r[0])]
    print(f"\n non-network ABSENT: {len(ab)} TUs / {sum(r[4] for r in ab)} wii fns")
    print(f"   WII_PLATFORM (correctly absent) : {len(plat):4d} TUs "
          f"{sum(r[4] for r in plat):5d} fns {sum(r[5] for r in plat)/1e6:.3f}MB")
    print(f"   REAL_UNLOCATED (Wii oracle EXISTS, 360 location unknown):")
    print(f"                                     {len(real):4d} TUs "
          f"{sum(r[4] for r in real):5d} fns {sum(r[5] for r in real)/1e6:.3f}MB")
    print("\n   *** REAL_UNLOCATED is the actionable worklist. Top 30: ***")
    for f, k in sorted(((r[4], r[0]) for r in real), reverse=True)[:30]:
        print(f"     {f:5d}  {k}")
    return rows


def report_control(per_unit):
    """Two-sided a-priori control. Rules declared from naming convention only."""
    pu = [r for r in per_unit if r["src"]]
    pos = [r for r in pu if os.path.exists(f"{SIB}/rb3/{r['src']}")]
    neg = [r for r in pu if re.search(r"(_Xbox\.cpp$|360\.cpp$|^src/xdk/|_xbox\.cpp$)",
                                      r["src"])]
    tp = sum(1 for r in pos if r["wii"]); fn = len(pos) - tp
    tn = sum(1 for r in neg if not r["wii"]); fp = len(neg) - tn
    print("\n=== A-PRIORI TWO-SIDED CONTROL (unit-level Wii-counterpart join) ===")
    print(f" POSITIVE (same relpath exists in ../rb3/src) n={len(pos):4d} "
          f"TP={tp:4d} FN={fn:3d} recall={tp/max(len(pos),1):.4f} "
          f"FN-rate={fn/max(len(pos),1):.4f}")
    print(f" NEGATIVE (*_Xbox / *360 / src/xdk)           n={len(neg):4d} "
          f"TN={tn:4d} FP={fp:3d} specificity={tn/max(len(neg),1):.4f}")
    live = sum(1 for r in neg if r["dc3"])
    print(f"\n LIVENESS (the control CAN fail): the same machinery matched "
          f"{live}/{len(neg)} of these\n   negative inputs to DC3, so 'no Wii "
          f"counterpart' was a real decision, not a dead matcher.")
    broad = [r for r in pu if re.search(r"synth_xbox|rnddx9|/xdk/|_Xbox", r["src"], re.I)]
    print(f" LIVENESS: on a looser dir-prefix negative set (n={len(broad)}) the join "
          f"returns\n   {sum(1 for r in broad if r['wii'])} Wii matches -> it does "
          f"produce positives on 'xbox' paths.\n   Those are CORRECT matches against "
          f"a bad label (e.g. synth_xbox/FxSend* -> Wii system/synth/*);\n   directory "
          f"prefix is NOT evidence of exclusivity.")


def report_wii_census(wii_tus, unmapped):
    mod = collections.Counter(); modb = collections.Counter()
    modtu = collections.defaultdict(set)
    for k, v in wii_tus.items():
        m = k.split("/")[0]
        mod[m] += v["fns"]; modb[m] += v["bytes"]; modtu[m].add(k)
    print("\n=== WII binary .text census (band_r_wii.map), Harmonix TUs only ===")
    for m, c in mod.most_common():
        print(f"  {m:10s} fns={c:6d} bytes={modb[m]/1e6:6.3f}MB tus={len(modtu[m]):5d}")
    print(f"  {'TOTAL':10s} fns={sum(mod.values()):6d} "
          f"bytes={sum(modb.values())/1e6:6.3f}MB tus={len(wii_tus):5d}")
    print(f"  (excluded: {sum(unmapped.values())} fns in {len(unmapped)} Wii "
          f"SDK/runtime lib TUs)")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--reverse", action="store_true",
                    help="also print the Wii->360 reverse census (the actionable side)")
    ap.add_argument("--control", action="store_true",
                    help="also print the two-sided a-priori control")
    ap.add_argument("--wii-census", action="store_true",
                    help="print only the Wii binary module census and exit")
    ap.add_argument("--outdir", default=OUTDIR)
    a = ap.parse_args()

    for p in (WII_MAP, DC3_MAP, REPORT):
        if not os.path.exists(p):
            sys.exit(f"error: missing required input {p}")

    wii_tus, wii_stem, unmapped = parse_wii(WII_MAP)
    if a.wii_census:
        report_wii_census(wii_tus, unmapped)
        return

    dc3_objs = parse_dc3(DC3_MAP)
    dc3_keys, dc3_stem = index_tree(DC3_SRC)
    _wii_keys, wii_src_stem = index_tree(WII_SRC)
    print(f"[wii ] map TUs {len(wii_tus)} (excluded {sum(unmapped.values())} lib fns)")
    print(f"[dc3 ] map objs {len(dc3_objs)}   src files {len(dc3_keys)}")

    report = json.load(open(REPORT))
    per_unit = classify(report, wii_tus, wii_stem, wii_src_stem,
                        dc3_keys, dc3_stem, {o[:-4].lower() for o in dc3_objs})
    total = int(report["measures"]["total_functions"])
    report_matrix(per_unit, total)

    os.makedirs(a.outdir, exist_ok=True)
    json.dump({"per_unit": per_unit, "wii_tus": wii_tus, "dc3_objs": dict(dc3_objs)},
              open(f"{a.outdir}/coverage_matrix.json", "w"))
    print(f"\nwrote {a.outdir}/coverage_matrix.json")

    if a.reverse or not (a.control):
        rows = report_reverse(wii_tus, per_unit)
        json.dump(rows, open(f"{a.outdir}/wii_reverse.json", "w"))
        print(f"wrote {a.outdir}/wii_reverse.json")
    if a.control:
        report_control(per_unit)


if __name__ == "__main__":
    main()
