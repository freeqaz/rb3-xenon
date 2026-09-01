#!/usr/bin/env python3
"""S3-ABLATE: per-group alias ablation via the BYPASS path (faster than ninja).

tools/alias_group_ablate.py drives ninja, which today costs ~12 s/group (the
doc's 2.5 s predates the current gate chain) -> 5.3 h for 1,591 groups. This
drives only the two steps the ablation needs -- regenerate icf_aliases.map,
regenerate the report -- at ~8 s/group.

LICENSED: the bypass reproduces the ninja-path FULL-vs-EMPTY figure exactly
(811,492 B / 7.920118 pp / matched_functions +3,154).

SCOPE: only groups with >=1 folded spelling are ablated. A group with
`folded: []` has nothing to remove, so its delta is 0 BY CONSTRUCTION, not by
measurement -- stated rather than spent. (A sample of them is still run as a
null control when --null is passed.)

ORDER: a seeded shuffle, so an INCOMPLETE run is an unbiased random sample of
groups rather than a biased prefix.

Restores scripts/symbol_aliases.json + icf_aliases.map on every exit path
INCLUDING SIGTERM/SIGINT -- tools/alias_group_ablate.py's `finally` does not run
under a kill, which left the file at 1,590 groups in this lane.
"""
import argparse, hashlib, json, os, random, re, signal, subprocess, sys, time
from pathlib import Path

CLI = "/home/free/code/milohax/objdiff/target/release/objdiff-cli"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wt", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--ruler", default="name_check")
    ap.add_argument("--seed", type=int, default=20260901)
    ap.add_argument("--only", default="", help="ablate only groups whose name matches")
    ap.add_argument("--null", type=int, default=0, help="also run N empty-folded groups")
    ap.add_argument("--reverse", action="store_true",
                    help="walk the SAME seeded shuffle from the back, so a second "
                         "sweeper on another worktree meets this one in the middle")
    a = ap.parse_args()

    wt = Path(a.wt).resolve()
    ali = wt / "scripts/symbol_aliases.json"
    amap = wt / "build/45410914/icf_aliases.map"
    rep = wt / "build/45410914/s3_sweep.json"

    ali_b, map_b = ali.read_bytes(), amap.read_bytes()
    ali_s, map_s = hashlib.sha256(ali_b).hexdigest(), hashlib.sha256(map_b).hexdigest()
    doc = json.loads(ali_b)
    groups = doc["groups"]

    def restore(*_):
        ali.write_bytes(ali_b); amap.write_bytes(map_b)
        ok = (hashlib.sha256(ali.read_bytes()).hexdigest() == ali_s
              and hashlib.sha256(amap.read_bytes()).hexdigest() == map_s)
        rep.unlink(missing_ok=True)
        print("restored alias json + map: %s" % ("sha ok" if ok else "FAILED"), flush=True)
        if not ok:
            os._exit(3)

    for s in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        signal.signal(s, lambda *_: (restore(), os._exit(0)))

    def leg(label):
        r = subprocess.run("python3 tools/gen_symbol_alias_map.py --out build/45410914/icf_aliases.map",
                           cwd=wt, shell=True, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit("genmap failed (%s): %s" % (label, r.stderr[-1500:]))
        (wt / "build/45410914/report.cache").unlink(missing_ok=True)
        r = subprocess.run("%s report generate -c functionRelocDiffs=%s -o %s"
                           % (CLI, a.ruler, rep.relative_to(wt)),
                           cwd=wt, shell=True, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit("report failed (%s): %s" % (label, (r.stdout + r.stderr)[-1500:]))
        d = json.loads(rep.read_text())
        rows = {}
        for u in d["units"]:
            for f in u.get("functions", []):
                n = f.get("name")
                if n and float(f.get("fuzzy_match_percent", 0) or 0) == 100.0:
                    rows[(u["name"], n)] = int(f.get("size", 0) or 0)
        m = d["measures"]
        return rows, int(m["matched_code"]), int(m["matched_functions"])

    outp = Path(os.path.expanduser(a.out))
    done = set()
    if outp.exists():
        for line in outp.read_text().splitlines():
            if line.strip():
                done.add(json.loads(line)["i"])
        print("resuming: %d already done" % len(done), flush=True)

    try:
        base_rows, base_code, base_fns = leg("FULL")
        print("FULL baseline: rows@100=%d matched_code=%d matched_functions=%d"
              % (len(base_rows), base_code, base_fns), flush=True)

        live = [i for i, g in enumerate(groups) if g.get("folded")]
        dead = [i for i, g in enumerate(groups) if not g.get("folded")]
        rnd = random.Random(a.seed)
        rnd.shuffle(live)
        todo = live + (dead[:a.null] if a.null else [])
        if a.reverse:
            todo = todo[::-1]
        if a.only:
            todo = [i for i, g in enumerate(groups) if a.only in str(g.get("name"))]
        print("to ablate: %d live (of %d groups, %d have folded:[] -> delta 0 by "
              "construction)" % (len(todo), len(groups), len(dead)), flush=True)

        t0 = time.time(); n = 0
        with outp.open("a") as fh:
            for i in todo:
                if i in done:
                    continue
                g = groups[i]
                doc["groups"] = groups[:i] + groups[i + 1:]
                ali.write_text(json.dumps(doc, indent=1) + "\n")
                rows, code, fns = leg("ablate#%d" % i)
                fell = [["\t".join(k), v] for k, v in base_rows.items() if k not in rows]
                fh.write(json.dumps({
                    "i": i, "name": g.get("name"), "survivor": g.get("survivor"),
                    "address": g.get("address"), "n_folded": len(g.get("folded") or []),
                    "d_code": base_code - code, "d_fns": base_fns - fns,
                    "n_fell": len(fell), "fell_bytes": sum(v for _, v in fell),
                    "fell": fell}) + "\n")
                fh.flush()
                n += 1
                if n % 20 == 0:
                    el = time.time() - t0
                    print("[%d/%d] %.1f s/group, %.0f min left"
                          % (n, len(todo), el / n, (len(todo) - n) * (el / n) / 60), flush=True)
    finally:
        restore()


if __name__ == "__main__":
    main()
