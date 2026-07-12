#!/usr/bin/env python3
"""Recarve Stage B: empirical boundary hill-climb for one EXTEND candidate.

Policy: extend-all, then trim-to-last-good, confirm (2-3 builds total, not
one build per function — objcache makes each build cheap but the re-split +
report regen dominate).

  1. In a WORKTREE (never main: splits.txt edits re-split under concurrent
     agents), find the TU's pin end that abuts a contiguous UNOWNED run of
     real functions (same walk as scan.py).
  2. Extend that .text end to the full run end. touch config.yml (re-split)
     + tools/fresh_report.sh.
  3. Evaluate every newly captured retail fn from the unit's report entry:
     GOOD = normalized 100 or fuzzy >= --hi. Unmapped fn_ names are neutral
     evidence (renamer can't pair them -> false 0%; they are Stage C work),
     counted separately.
  4. Trim the boundary back to the end of the last GOOD fn (unless
     --no-trim), rebuild, confirm.
  5. Gate with scripts/harvest/measure_delta.py (baseline vs final report):
     a keep REQUIRES net > 0 and zero real regressions. Emits evidence JSON
     + the splits.txt diff for the coordinator to land (never lands itself).

See docs/plans/recarve-pipeline.md. Candidates come from scan.py
(~/tmp/recarve/scan.json), e.g.:

  python3 scripts/recarve/climb.py --tu Waypoint.cpp \\
      --worktree ~/tmp/recarve-wt-waypoint
"""
import argparse, bisect, json, re, shutil, subprocess, sys, time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from funclets import load_funclet_set

MAIN = Path(__file__).resolve().parent.parent.parent
STUB = 0x2C
STRICT = 99.999


def sh(cmd, cwd, log=None, check=True):
    print(f"  $ {' '.join(str(c) for c in cmd)}")
    t0 = time.time()
    r = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True)
    if log:
        Path(log).write_text(r.stdout)
    if check and r.returncode != 0:
        tail = "\n".join(r.stdout.splitlines()[-25:])
        sys.exit(f"FATAL: {' '.join(str(c) for c in cmd)} rc={r.returncode}\n{tail}")
    print(f"    ({time.time()-t0:.0f}s)")
    return r


def load_functions(repo):
    funcs = []
    rx = re.compile(
        r"^(\S+) = \.text:0x([0-9A-Fa-f]+);.*?\btype:function\b.*?size:0x([0-9A-Fa-f]+)")
    for l in (repo / "config/45410914/symbols.txt").read_text().splitlines():
        m = rx.match(l)
        if m:
            funcs.append((int(m.group(2), 16), int(m.group(3), 16), m.group(1)))
    funcs.sort()
    return funcs


def load_ranges(repo):
    ranges, cur = [], None
    for l in (repo / "config/45410914/splits.txt").read_text().splitlines():
        h = re.match(r"^(\S+\.(?:cpp|c|cc)):\s*$", l)
        if h:
            cur = h.group(1)
            continue
        t = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", l)
        if t and cur:
            ranges.append((int(t.group(1), 16), int(t.group(2), 16), cur))
    ranges.sort()
    return ranges


def edit_split_end(repo, tu, old_end, new_end):
    """Rewrite ONE .text end:0x{old_end} under the tu's header to new_end."""
    p = repo / "config/45410914/splits.txt"
    lines = p.read_text().splitlines(keepends=True)
    cur, done = None, False
    for i, l in enumerate(lines):
        h = re.match(r"^(\S+\.(?:cpp|c|cc)):\s*$", l)
        if h:
            cur = h.group(1)
            continue
        if cur == tu and re.search(rf"\.text\s+start:0x[0-9A-Fa-f]+\s+end:0x{old_end:08X}", l):
            lines[i] = re.sub(rf"end:0x{old_end:08X}", f"end:0x{new_end:08X}", l)
            done = True
            break
    if not done:
        sys.exit(f"FATAL: could not find {tu} .text end:0x{old_end:08X} in splits.txt")
    p.write_text("".join(lines))
    print(f"  splits.txt: {tu} .text end 0x{old_end:08X} -> 0x{new_end:08X}")


def rebuild(repo):
    (repo / "config/45410914/config.yml").touch()   # force re-split
    sh([str(repo / "tools/fresh_report.sh"), "--no-verify"], repo,
       log=Path.home() / "tmp/recarve/climb_build.log")


def unit_eval(repo, tu, window_fns, tmap, funclet_vas, hi_fuzzy):
    """Classify the retail fns in the captured window by NAME lookup in the
    unit report. (Never reconstruct VAs from report offsets — dtk compacts
    excluded symbols, so offsets drift; Waypoint E2E lesson 2026-07-12.)
    window_fns: [(va, size, symbols_txt_name)] from the symbols.txt walk."""
    report = json.load(open(repo / "build/45410914/report.json"))
    uname = "default/" + (tu[:-4] if tu.endswith(".cpp") else tu)
    u = next((x for x in report["units"] if x["name"] == uname), None)
    if not u:
        sys.exit(f"FATAL: unit {uname} not in report.json after rebuild")
    by_name = {fn["name"]: fn for fn in u.get("functions") or []}
    rows = []
    for va, sz, sym in window_fns:
        if sym.startswith("except_data_"):
            continue
        mapped_name = tmap.get(f"0x{va:08x}") or tmap.get(f"0x{va:08X}")
        expect = mapped_name or f"fn_{va:08X}"
        fn = by_name.get(expect)
        fz = (fn.get("fuzzy_match_percent") or 0.0) if fn else 0.0
        nm = (fn.get("match_percent_normalized") or 0.0) if fn else 0.0
        is_funclet = va in funclet_vas
        rows.append({"va": va, "size": sz, "name": expect,
                     "fuzzy": round(fz, 2), "norm": round(nm, 2),
                     "in_report": fn is not None,
                     "funclet": is_funclet,
                     "unmapped": mapped_name is None,
                     "good": (not is_funclet) and (nm >= STRICT or fz >= hi_fuzzy)})
    rows.sort(key=lambda r: r["va"])
    return rows, report["measures"].get("matched_functions")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--tu", required=True, help="splits.txt TU name, e.g. Waypoint.cpp")
    ap.add_argument("--worktree", required=True,
                    help="worktree path (created via setup_worktree.sh if missing)")
    ap.add_argument("--branch", help="branch for a new worktree (default recarve/<tu>)")
    ap.add_argument("--hi", type=float, default=90.0, help="GOOD fuzzy threshold")
    ap.add_argument("--max-bytes", type=lambda s: int(s, 0), default=0x8000,
                    help="cap on extension size")
    ap.add_argument("--no-trim", action="store_true",
                    help="keep full-run boundary even past the last GOOD fn")
    ap.add_argument("--json", help="evidence JSON out (default ~/tmp/recarve/climb-<tu>.json)")
    args = ap.parse_args()

    wt = Path(args.worktree).expanduser()
    slug = re.sub(r"[^A-Za-z0-9]+", "-", args.tu).strip("-").lower()
    if not wt.exists():
        branch = args.branch or f"recarve/{slug}"
        sh([str(MAIN / "scripts/setup_worktree.sh"), str(wt), branch], MAIN)
    if wt.resolve() == MAIN.resolve():
        sys.exit("FATAL: refusing to run against the main repo (concurrent agents); use a worktree")

    funcs = load_functions(wt)
    faddr = [f[0] for f in funcs]
    ranges = load_ranges(wt)
    rstart = [r[0] for r in ranges]

    def owned(addr):
        i = bisect.bisect_right(rstart, addr) - 1
        while i >= 0 and ranges[i][0] > addr - 0x20000:
            s, e, t = ranges[i]
            if s <= addr < e:
                return t
            i -= 1
        return None

    def fn_at(addr):
        i = bisect.bisect_left(faddr, addr)
        return funcs[i] if i < len(faddr) and funcs[i][0] == addr else None

    funclet_vas = load_funclet_set(wt)
    tmap = json.load(open(wt / "scripts/target_symbol_map.json"))

    # find ALL pin ends abutting an unowned REAL run (same walk as scan.py:
    # real = >44B, not except_data, NOT an EH funclet); process each.
    cands = []
    for s, e, t in ranges:
        if t != args.tu:
            continue
        a, real, fclts, fns = e, 0, 0, []
        while a - e < args.max_bytes:
            f = fn_at(a)
            if not f or owned(a):
                break
            va, sz, name = f
            if sz == 0:
                break
            if not name.startswith("except_data_"):
                if va in funclet_vas:
                    fclts += 1
                elif sz > STUB:
                    real += 1
            fns.append(f)
            a += sz
        if fns and real > 0:
            cands.append({"range_start": s, "old_end": e, "run_end": a,
                          "real": real, "funclets": fclts, "fns": fns})
    if not cands:
        sys.exit(f"no EXTEND candidate for {args.tu}: no pin end abuts an unowned "
                 f"real-fn run (funclet-only runs are excluded)")
    cands.sort(key=lambda c: -c["real"])

    # baseline
    base_json = Path.home() / f"tmp/recarve/BASE-{slug}.json"
    base_json.parent.mkdir(parents=True, exist_ok=True)
    if not (wt / "build/45410914/report.json").exists():
        rebuild(wt)
    shutil.copy(wt / "build/45410914/report.json", base_json)

    results = []
    for cand in cands:
        print(f"[{args.tu}] extending 0x{cand['old_end']:08X} -> 0x{cand['run_end']:08X} "
              f"({len(cand['fns'])} fns: {cand['real']} real, {cand['funclets']} funclets, "
              f"{cand['run_end']-cand['old_end']:#x} bytes)")
        edit_split_end(wt, args.tu, cand["old_end"], cand["run_end"])
        rebuild(wt)
        rows, _ = unit_eval(wt, args.tu, cand["fns"], tmap, funclet_vas, args.hi)
        good = [r for r in rows if r["good"]]
        print(f"  captured {len(rows)} fns: {sum(r['norm']>=STRICT for r in rows)} matched, "
              f"{sum(r['good'] and r['norm']<STRICT for r in rows)} hi-fuzzy, "
              f"{sum(r['funclet'] for r in rows)} funclets, "
              f"{sum(r['unmapped'] and not r['funclet'] for r in rows)} unmapped (Stage C)")

        final_end = cand["run_end"]
        if not good:
            print("  NO GOOD non-funclet fns captured -> reverting this extension")
            edit_split_end(wt, args.tu, cand["run_end"], cand["old_end"])
            rebuild(wt)
            end_verdict = "DEFER"
        elif args.no_trim or good[-1] is rows[-1]:
            end_verdict = "KEEP-FULL"
        else:
            last_good = good[-1]
            trim_to = last_good["va"] + last_good["size"]
            # absorb trailing glue: sub-STUB thunks, except_data blobs, and
            # the last good fn's own EH funclets (they follow their parent)
            while True:
                f = fn_at(trim_to)
                if f and f[0] < cand["run_end"] and (
                        f[1] <= STUB or f[2].startswith("except_data_")
                        or f[0] in funclet_vas):
                    trim_to += f[1]
                else:
                    break
            if trim_to < cand["run_end"]:
                print(f"  trimming boundary: 0x{cand['run_end']:08X} -> 0x{trim_to:08X} "
                      f"(last GOOD fn {last_good['name'][:40]})")
                edit_split_end(wt, args.tu, cand["run_end"], trim_to)
                rebuild(wt)
                final_end = trim_to
                end_verdict = "KEEP-TRIMMED"
            else:
                end_verdict = "KEEP-FULL"
        results.append({"old_end": f"0x{cand['old_end']:08X}",
                        "run_end": f"0x{cand['run_end']:08X}",
                        "final_end": f"0x{final_end:08X}",
                        "verdict": end_verdict, "captured": rows})

    # gate: whole-binary A/B vs baseline (once, over all kept boundaries)
    r = sh(["python3", str(wt / "scripts/harvest/measure_delta.py"),
            str(base_json), str(wt / "build/45410914/report.json")], wt, check=False)
    print(r.stdout[-2000:])

    # overall verdict grading: KEEP requires strict net > 0. Kept boundaries
    # with net <= 0 are EVIDENCE — membership proven (good fns captured) but
    # strict yield pends Stage C identity work / near-miss closing.
    m = re.search(r"NET:\s*\+?(-?\d+)", r.stdout)
    net = int(m.group(1)) if m else 0
    any_kept = any(x["verdict"].startswith("KEEP") for x in results)
    verdict = ("KEEP" if any_kept and net > 0 else
               "EVIDENCE" if any_kept else "DEFER")

    out = Path(args.json) if args.json else Path.home() / f"tmp/recarve/climb-{slug}.json"
    json.dump({"tu": args.tu, "verdict": verdict, "net": net,
               "ends": results, "worktree": str(wt),
               "measure_delta_tail": r.stdout.splitlines()[-12:]},
              open(out, "w"), indent=1)
    print(f"\nverdict: {verdict} (net {net:+d})  evidence: {out}")
    if verdict != "DEFER":
        print(f"splits diff (kept in worktree {wt}):")
        print(sh(["git", "diff", "--", "config/45410914/splits.txt"], wt, check=False).stdout)
        if verdict == "EVIDENCE":
            print("EVIDENCE: boundary kept in the worktree; strict yield pends Stage C "
                  "(identify the unmapped fns, append map entries, re-measure).")
        else:
            print("NOT landed — coordinator lands via scripts/harvest/land.sh after regression lock.")


if __name__ == "__main__":
    main()
