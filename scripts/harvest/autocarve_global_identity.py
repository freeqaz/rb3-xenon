#!/usr/bin/env python3
"""autocarve_global_identity — identity funnel for the `auto_*` carve arm.

★ STATUS (laneBT5, 2026-07-30): recovered from orphaned branch `laneAS-D`. It
  RUNS (rc=0, non-empty) on the current tree in ~1 min:
      GLOBAL_EXACT_UNIQUE 59 · LOCAL_UNIQUE 116 · GLOBAL_EXACT_AMBIG 572
      NOMATCH 1807 · TOTAL 2554
  = 175 attribution candidates. Its PRECISION IS UNVALIDATED — laneBT5 did not
  run the `--holdout` mode; budget went to the reloc_disc set. Metric-neutral by
  construction (see the docstring below: auto units carry no `base_path`, so
  nothing pairs until the VA is pinned into a real unit), which is consistent
  with the recorded auto-carve ceiling of roughly +25..85. Treat the output as
  input to a splits/attribution lane, not as a match product.
  Usage: autocarve_global_identity.py --worktree WT --out OUT.json [--holdout]


The `auto_03_<VA>_text` units are dtk's synthetic leftover carves. They have a
target `.s`/`.obj` but **no compiled base obj at all** (measured: 0 of 681 auto
units carry a `base_path` in objdiff.json). So the per-unit funnel that works on
pinned units has nothing to compare against, and objdiff scores every function
in them 0 % regardless of what name the map assigns.

That makes this arm an **attribution** product, not a strict-match product: a
name recovered here says *which TU owns this VA*, which is input to the
splits/attribution lane, and only becomes a match once that VA is pinned into a
real unit.

Method
  1. GLOBAL index: reloc-masked bytes -> distinct symbol names, over the code
     symbols of ALL compiled base objs. Supply excludes internal/funclet-like
     names and names already claimed (present in some target obj / in the map).
  2. Classify each arm VA: GLOBAL_EXACT_UNIQUE / GLOBAL_EXACT_AMBIG / NOMATCH.
  3. LOCALITY: an auto carve span is bounded by pinned neighbours in address
     order. Restrict the index to the objs of the N nearest pinned units and
     re-classify -- does "globally ambiguous" become "locally decisive"?

Read-only.  Writes JSON only under --out.
"""
import argparse
import json
import pickle
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import size_order_automap as soa  # noqa: E402

ANON = re.compile(r"^fn_([0-9A-Fa-f]{8})$")
FN_RX = re.compile(r"^\.fn (\S+),", re.M)
VENDOR = (0x82800000, 0x82D00000)
FUNCLET_LIKE = re.compile(r"^(__unwind\$|__catch\$|\?\?__E|\?\?__F|fn_)")


def masked_target_funcs(asm: Path):
    """VA -> (size, masked bytes), address order."""
    out = {}
    cur, in_fn, words = None, False, []
    for line in asm.read_text(errors="replace").splitlines():
        m = FN_RX.match(line)
        if m:
            cur, in_fn, words = m.group(1), True, []
            continue
        if line.startswith(".endfn"):
            if in_fn:
                mm = ANON.match(cur or "")
                if mm:
                    b = b"".join(words)
                    out[int(mm.group(1), 16)] = (len(b), b)
            in_fn = False
            continue
        if not in_fn:
            continue
        im = soa._INSTR_RX.match(line)
        if im:
            w = bytes(int(x, 16) for x in im.groups()[:4])
            if soa._operand_relocated(im.group(6)):
                w = b"\0\0\0\0"
            words.append(w)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--cache", default=None)
    ap.add_argument("--neighbours", type=int, default=2,
                    help="pinned units on EACH side to form the local index")
    ap.add_argument("--holdout", action="store_true",
                    help="leave-one-out precision of GLOBAL byte-uniqueness, "
                         "measured on already-named PINNED target VAs")
    args = ap.parse_args()

    wt = Path(args.worktree).resolve()
    build = wt / "build" / "45410914"
    od = json.loads((wt / "objdiff.json").read_text())
    rep = json.loads((build / "report.json").read_text())
    vamap = json.loads((wt / "scripts" / "target_symbol_map.json").read_text())
    mapped_names = {v for v in vamap.values() if isinstance(v, str)}

    # ---- unit table -------------------------------------------------------
    units = {}
    for u in od["units"]:
        tp, bp = u.get("target_path"), u.get("base_path")
        if not tp:
            continue
        units[u["name"]] = dict(
            asm=wt / (tp.replace("/obj/", "/asm/")[:-4] + ".s"),
            tobj=wt / tp,
            cobj=(wt / bp) if bp else None)

    # ---- the arm ----------------------------------------------------------
    arm = []
    for u in rep["units"]:
        un = u["name"]
        if not un.rsplit("/", 1)[-1].startswith("auto_"):
            continue
        for f in u.get("functions", []):
            m = ANON.match(f["name"])
            if not m:
                continue
            va = int(m.group(1), 16)
            if VENDOR[0] <= va < VENDOR[1]:
                continue
            if (f.get("match_percent_normalized") or 0.0) >= 100.0:
                continue
            arm.append(dict(unit=un, va=va, size=int(f["size"])))
    print(f"arm: {len(arm)} VAs in {len({a['unit'] for a in arm})} auto units",
          file=sys.stderr)

    cache = Path(args.cache) if args.cache else None
    if cache and cache.exists():
        blob = pickle.loads(cache.read_bytes())
        supply, spans, arm_bytes = blob["supply"], blob["spans"], blob["arm_bytes"]
        claimed_idx = blob.get("claimed_idx", {})
        pinned_bytes = blob.get("pinned_bytes", {})
    else:
        # ---- VA spans for EVERY unit (from the target asm) -----------------
        spans = {}
        for un, e in units.items():
            if not e["asm"].exists():
                continue
            vas = [int(x, 16) for x in
                   re.findall(r"^\.fn fn_([0-9A-Fa-f]{8}),",
                              e["asm"].read_text(errors="replace"), re.M)]
            if vas:
                spans[un] = (min(vas), max(vas))

        # ---- global supply: masked bytes -> {name: [units]} ----------------
        supply = defaultdict(lambda: defaultdict(list))
        claimed_idx = defaultdict(lambda: defaultdict(list))
        n = 0
        for un, e in units.items():
            if e["cobj"] is None or not e["cobj"].exists():
                continue
            # names already present in this unit's own target obj are claimed
            claimed = set()
            if e["tobj"].exists():
                try:
                    _, _, ts = soa._parse_coff(e["tobj"])
                    claimed = {soa.anon_ns_strip(s["name"]) for s in ts}
                except Exception:
                    pass
            try:
                bf = soa._ordered_funcs(e["cobj"])
            except Exception:
                continue
            n += 1
            if n % 100 == 0:
                print(f"  indexed {n} objs", file=sys.stderr)
            for f in bf:
                nm = soa.anon_ns_strip(f["name"])
                if soa.is_internal(f["name"]) or FUNCLET_LIKE.match(f["name"]):
                    continue
                if nm in claimed or nm in mapped_names:
                    # keep it out of live supply, but record it so the
                    # leave-one-out harness can add exactly one back.
                    claimed_idx[f["masked"]][nm].append(un)
                    continue
                supply[f["masked"]][nm].append(un)
        supply = {k: dict(v) for k, v in supply.items()}
        claimed_idx = {k: dict(v) for k, v in claimed_idx.items()}
        print(f"global supply: {len(supply)} distinct masked bodies "
              f"over {n} objs", file=sys.stderr)

        # held-out truth set: pinned-unit target VAs that already carry a
        # map entry, with their masked target bytes.
        pinned_bytes = {}
        vamap_int = {}
        for k, v in vamap.items():
            if isinstance(k, str) and k.lower().startswith("0x") and isinstance(v, str):
                try:
                    vamap_int[int(k, 16)] = v
                except ValueError:
                    pass
        for un, e in units.items():
            if un.rsplit("/", 1)[-1].startswith("auto_"):
                continue
            if e["cobj"] is None or not e["asm"].exists():
                continue
            for va, (sz, mb) in masked_target_funcs(e["asm"]).items():
                t = vamap_int.get(va)
                if t and not FUNCLET_LIKE.match(t) and not soa.is_internal(t):
                    pinned_bytes[va] = (un, sz, mb, soa.anon_ns_strip(t))

        arm_bytes = {}
        for un in sorted({a["unit"] for a in arm}):
            e = units.get(un)
            if e is None or not e["asm"].exists():
                continue
            arm_bytes.update(masked_target_funcs(e["asm"]))
        if cache:
            cache.write_bytes(pickle.dumps(
                dict(supply=supply, spans=spans, arm_bytes=arm_bytes,
                     claimed_idx=claimed_idx, pinned_bytes=pinned_bytes)))

    # ---- pinned-unit address ladder for locality --------------------------
    pinned = sorted(((lo, hi, un) for un, (lo, hi) in spans.items()
                     if not un.rsplit("/", 1)[-1].startswith("auto_")
                     and units.get(un, {}).get("cobj") is not None),
                    key=lambda x: x[0])
    pin_lo = [p[0] for p in pinned]
    import bisect

    def neighbours(va, k):
        i = bisect.bisect_left(pin_lo, va)
        return {pinned[j][2] for j in range(max(0, i - k), min(len(pinned), i + k))}

    if args.holdout:
        res = Counter()
        bands = defaultdict(lambda: [0, 0])
        wrong = []
        for va, (un, sz, mb, truth) in pinned_bytes.items():
            live = dict(supply.get(mb, {}))
            back = claimed_idx.get(mb, {})
            if truth not in back and truth not in live:
                res["truth_body_not_byte_identical"] += 1
                continue
            if truth in back:
                live[truth] = back[truth]
            res["testable"] += 1
            band = "<=68B" if sz <= 68 else ">68B"
            if len(live) == 1:
                got = next(iter(live))
                ok = got == truth
                res["GLOBAL_EXACT_UNIQUE"] += 1
                for key in ("GLOBAL_EXACT_UNIQUE", "GEU " + band):
                    bands[key][1] += 1
                    bands[key][0] += ok
                if not ok and len(wrong) < 15:
                    wrong.append((un, "0x%08X" % va, sz, got[:44], truth[:44]))
                continue
            nb = neighbours(va, args.neighbours)
            loc = {nm: us for nm, us in live.items() if nb & set(us)}
            if len(loc) == 1:
                got = next(iter(loc))
                ok = got == truth
                res["LOCAL_UNIQUE"] += 1
                for key in ("LOCAL_UNIQUE", "LOC " + band):
                    bands[key][1] += 1
                    bands[key][0] += ok
                if not ok and len(wrong) < 15:
                    wrong.append((un, "0x%08X" % va, sz, got[:44], truth[:44]))
            else:
                res["AMBIG"] += 1
                bands["AMBIG(argmax-free)"][1] += 1
        print("=" * 74)
        print("CHANNEL-2 HELD-OUT: global masked-byte uniqueness, leave-one-out")
        print("=" * 74)
        for k in ("testable", "truth_body_not_byte_identical",
                  "GLOBAL_EXACT_UNIQUE", "LOCAL_UNIQUE", "AMBIG"):
            print(f"  {k:32s} {res.get(k, 0):6d}")
        print()
        for k in sorted(bands):
            c, n = bands[k]
            if n:
                print(f"  precision[{k:<22}] {c}/{n} = {100.0*c/n:.1f}%")
        if wrong:
            print("\n  sample WRONG:")
            for w in wrong:
                print(f"    {w[0]:<30} {w[1]} sz={w[2]} got={w[3]} want={w[4]}")
        return

    rows = []
    cls = Counter()
    for a in arm:
        t = arm_bytes.get(a["va"])
        r = dict(a)
        if t is None:
            r["cls"] = "NO_TARGET_ASM"
            cls["NO_TARGET_ASM"] += 1
            rows.append(r)
            continue
        tsz, tmb = t
        hit = supply.get(tmb)
        if not hit:
            r["cls"] = "NOMATCH"
            cls["NOMATCH"] += 1
            rows.append(r)
            continue
        r["names"] = sorted(hit)[:12]
        r["nnames"] = len(hit)
        r["owners"] = sorted({u for us in hit.values() for u in us})[:12]
        if len(hit) == 1:
            r["cls"] = "GLOBAL_EXACT_UNIQUE"
            r["cand"] = r["names"][0]
        else:
            r["cls"] = "GLOBAL_EXACT_AMBIG"
            # locality: restrict to objs of the nearest pinned units
            nb = neighbours(a["va"], args.neighbours)
            local = {nm: us for nm, us in hit.items() if nb & set(us)}
            r["nlocal"] = len(local)
            if len(local) == 1:
                r["cls"] = "LOCAL_UNIQUE"
                r["cand"] = next(iter(local))
        cls[r["cls"]] += 1
        rows.append(r)

    json.dump(rows, open(args.out, "w"))
    print("\n=== auto_* GLOBAL IDENTITY FUNNEL ===")
    for k in ("GLOBAL_EXACT_UNIQUE", "LOCAL_UNIQUE", "GLOBAL_EXACT_AMBIG",
              "NOMATCH", "NO_TARGET_ASM"):
        print(f"  {k:22s} {cls.get(k, 0):6d}")
    print(f"  {'TOTAL':22s} {sum(cls.values()):6d}")
    big = [r for r in rows if r["size"] > 68]
    cb = Counter(r["cls"] for r in big)
    print(f"\n  of which > 68 B ({len(big)}):")
    for k in ("GLOBAL_EXACT_UNIQUE", "LOCAL_UNIQUE", "GLOBAL_EXACT_AMBIG",
              "NOMATCH", "NO_TARGET_ASM"):
        print(f"    {k:20s} {cb.get(k, 0):6d}")


if __name__ == "__main__":
    main()
