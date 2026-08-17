#!/usr/bin/env python3
"""W25-UI: dump every charged relocation-NAME pair in the UI cluster, with the
row size it gates and whether closing it alone would cross the row.

A row only pays when EVERY charge on it is closed (matched_code is
all-or-nothing per row), so a pair is only worth acting on when the rest of
its row is already clean.
"""
import collections
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from scripts.analysis import ruler as ruler_mod  # noqa: E402


def I(x, d=0):
    return d if x is None else int(x)


def F(x, d=0.0):
    return d if x is None else float(x)


def main():
    proj = Path(sys.argv[1]).resolve()
    pattern = sys.argv[2] if len(sys.argv) > 2 else "UI"
    rk = ruler_mod.resolve_ruler(proj)
    rep = json.load(open(proj / "build/45410914/report.json"))
    units = [u for u in rep["units"] if pattern in u.get("name", "")]

    want = {}
    for u in units:
        for f in (u.get("functions") or []):
            fz = F(f.get("fuzzy_match_percent"))
            nm = f.get("name", "")
            if fz >= 100.0 or fz <= 0.0 or nm.startswith(("fn_", "lbl_")):
                continue
            want[(u["name"], nm)] = I(f.get("size"))

    cli = str(proj / "bin/objdiff-cli")
    out_rows = []
    for u in units:
        uname = u["name"]
        syms = [k[1] for k in want if k[0] == uname]
        if not syms:
            continue
        cmd = [cli, "diff", "-p", str(proj), "-u", uname, "--batch",
               "-f", "json", "-o", "-", "--include-instructions"] + rk.args
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=900,
                           input="\n".join(syms) + "\n")
        txt = r.stdout.strip()
        if not txt:
            continue
        try:
            j = json.loads(txt)
            recs = j if isinstance(j, list) else [j]
        except json.JSONDecodeError:
            recs = [json.loads(x) for x in txt.splitlines() if x.strip()]
        for rec in recs:
            sym = rec.get("symbol") or rec.get("name") or ""
            if (uname, sym) not in want or rec.get("error"):
                continue
            namepairs = []
            other = 0
            for i in rec.get("instructions", []) or []:
                mt = i.get("match_type")
                if mt == "equal":
                    continue
                if mt != "diff_arg":
                    other += 1
                    continue
                t = i.get("target") or {}
                b = i.get("base") or {}
                kinds, sp = set(), None
                for x, y in zip(t.get("typed_args", []) or [],
                                b.get("typed_args", []) or []):
                    if x.get("value") != y.get("value"):
                        kinds.add(x.get("type"))
                        if x.get("type") == "Symbol":
                            sp = (x.get("value"), y.get("value"))
                if kinds == {"Symbol"}:
                    namepairs.append(sp)
                else:
                    other += 1
            if namepairs:
                out_rows.append({
                    "unit": uname, "symbol": sym, "size": want[(uname, sym)],
                    "other_charges": other,
                    "pairs": [list(p) for p in namepairs],
                })

    out_rows.sort(key=lambda r: -r["size"])
    Path("/home/free/tmp/w25_pairs.json").write_text(json.dumps(out_rows,
                                                                indent=1))

    print("== rows whose ONLY charges are relocation NAMES (other_charges==0)")
    print("   -> closing the name pairs alone crosses the row and pays its "
          "full size")
    clean = [r for r in out_rows if r["other_charges"] == 0]
    dirty = [r for r in out_rows if r["other_charges"] > 0]
    print(f"   clean rows: {len(clean)}  bytes: {sum(r['size'] for r in clean)}")
    print(f"   rows with other charges too: {len(dirty)}  "
          f"bytes: {sum(r['size'] for r in dirty)}")
    print()
    for r in clean[:40]:
        print(f"{r['size']:>6}  {r['unit']:<22} {r['symbol'][:52]}")
        for tgt, ours in r["pairs"]:
            print(f"          target: {str(tgt)[:70]}")
            print(f"          ours  : {str(ours)[:70]}")
    print()
    print("== pair frequency across CLEAN rows (bytes gated) ==")
    agg = collections.Counter()
    byb = collections.Counter()
    for r in clean:
        for tgt, ours in r["pairs"]:
            agg[(tgt, ours)] += 1
            byb[(tgt, ours)] += r["size"]
    for (tgt, ours), c in agg.most_common(30):
        print(f"  x{c:<3} gates<= {byb[(tgt, ours)]:>6} B")
        print(f"       target {str(tgt)[:66]}")
        print(f"       ours   {str(ours)[:66]}")


if __name__ == "__main__":
    main()
