#!/usr/bin/env python3
"""Assert that the objdiff-cli this repo CONSUMES keys its report cache on the
ICF ALIAS MAP's CONTENT -- i.e. that a map change alone cannot serve a stale
number.

WHY (lane CLEANUP, 2026-09-11)
------------------------------
There used to be an `icf_aliases_cache_purged` edge in tools/project.py:

    rm -f build/<v>/report.cache build/<v>/baseline.cache && touch $out

hung off the rendered alias map and made an implicit input of report.json. It
existed because `report generate -o X.json` writes a sidecar `X.cache` and seeds
the next run from it, and `ReportCache::hash_unit` used to key on the target obj
bytes, the base obj bytes and the `options` blocks -- with `map_file`, and the
CONTENT of the map it names, in NONE of them. On dc3 (2026-08-12) that cost a
+198-complete-function change: it re-rendered the map, re-ran REPORT, and
report.json still served the pre-change answer.

objdiff `345778c` ("report: key the unit cache on the binary and the alias map,
and stamp the ruler into the report") folds the map file's content hash into
every unit key, which made that edge dead code. It was removed.

THE REDUNDANCY IS CONDITIONAL ON A HAND-SWAPPED PREBUILT BINARY.
`configure.py` resolves objdiff-cli to ../objdiff/target/release/objdiff-cli --
a prebuilt binary shared by symlink with ../rb3 and ../dc3-decomp, which nothing
in this repo pins or verifies, and which CLAUDE.md documents as routinely
swapped and rolled back. Roll back past 345778c with the purge edge already
deleted and stale report caches return SILENTLY: the score simply reads wrong.
That is the same structure lane W4-E found for the EH boundary pass, and the
same remedy -- retire behind a guard, never bare-retire. See
tools/check_objdiff_eh_prefix.py, which this tool is modelled on.

WHAT IT ASSERTS, AND WHY NOT THE HIT COUNTER
--------------------------------------------
The tempting observable is `Report cache: N hits, M misses` / the report's
`provenance.cache_hits`. Both are REFUSED as the primary assertion, because a
binary old enough to have the defect may not emit either -- and then the guard's
RED leg would read "vacuous" instead of "fail", which is the one outcome a guard
must never confuse. So this asserts the failure directly: can a changed map
serve the PREVIOUS answer out of a warm cache?

Five legs over ONE shared cache, plus a cold reference:

    A  map M1, shared cache (cold)      -> measures_A
    B  map M1, shared cache (warm)      -> must equal measures_A
    R  map M2, OWN FRESH cache          -> measures_R   (the honest M2 answer)
    C  map M2, shared cache (warm)      -> MUST equal measures_R.
                                           If it equals measures_A instead, the
                                           cache served the pre-change answer
                                           under a changed map: FAIL.
    D  map M1, shared cache (warm)      -> must equal measures_A exactly

Leg R is the ANTI-VACUITY CONTROL and it is the whole reason this tool can be
believed: if M1 and M2 produce the SAME answer, then leg C agreeing with
measures_A proves nothing at all, and this exits 5 (VACUOUS) rather than
reporting a pass it did not earn. Same discipline as the `--self-break` proofs
elsewhere in tools/: an instrument that cannot distinguish its own silence from
a pass is worse than none.

M1 is the repo's real rendered map; M2 is that map EMPTIED. Measured here at
3ab3f494, 1,591 groups, driving objdiff-cli by hand over one cache:

    real map    3,834,712 B / 42,505 fns      0 hits / 3086 misses
    real again  3,834,712 B                3086 HITS / 0 misses   <- cache is live
    emptied     3,007,800 B / 39,295 fns      0 hits / 3086 misses
    restored    3,834,712 B, to the byte   3086 HITS / 0 misses

Nothing purged anything in any leg; the key did all of it. Note the restore leg
HITS -- the emptied-map run never displaced the real-map entries, so both
keyings coexist in one cache file, which happens only if the map hash is in the
key.

CONTROL -- the red leg, exercised not asserted
----------------------------------------------
Against an objdiff-cli built from `345778c^` this guard MUST fail (rc=2):

    git clone <objdiff> ~/tmp/odprobe && git -C ~/tmp/odprobe checkout 345778c^
    CARGO_TARGET_DIR=~/tmp/odprobe-t cargo build --release -p objdiff-cli \
        --manifest-path ~/tmp/odprobe/Cargo.toml
    python3 tools/check_objdiff_map_cache.py --objdiff ~/tmp/odprobe-t/release/objdiff-cli

NEVER `cargo build --release` inside ../objdiff itself: its target/release path
IS the deployed fleet binary, shared with two other repos.

EXIT CODES
    0  PASS    -- the consumed binary keys its cache on the map. The purge edge
                  is genuinely redundant; leaving it deleted is safe.
    2  FAIL    -- a changed map served a stale answer. RESTORE the purge edge
                  (tools/project.py, see the TOMBSTONE comment) or roll the
                  objdiff binary forward. Do not measure anything until fixed.
    5  VACUOUS -- the probe proved nothing (no units, or the two maps do not
                  change the answer). NOT a pass.
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

_HERE = Path(__file__).resolve().parent


def consumed_objdiff(root: Path) -> str:
    """The binary the REPORT edge actually runs -- read out of build.ninja, so a
    swapped symlink or an edited rule is reflected rather than assumed. Same
    approach as tools/check_objdiff_eh_prefix.py."""
    ninja = root / "build.ninja"
    if ninja.exists():
        m = re.search(r"^rule report\n\s+command = (\S+)",
                      ninja.read_text(errors="replace"), re.M)
        if m:
            return m.group(1)
    return str(root / "bin" / "objdiff-cli")


def write_probe_project(root: Path, dest: Path, map_path: Path, limit: int) -> int:
    """A private objdiff.json pointing at the SAME objects but at our own map, so
    the live tree is never touched. Paths are absolutised because the project
    dir moves."""
    cfg = json.loads((root / "objdiff.json").read_text())
    units = []
    for u in cfg.get("units", []):
        tp, bp = u.get("target_path"), u.get("base_path")
        if not tp or not bp:
            continue
        v = dict(u)
        v["target_path"] = str((root / tp).resolve())
        v["base_path"] = str((root / bp).resolve())
        v.pop("scratch", None)
        units.append(v)
        if limit and len(units) >= limit:
            break
    out = {
        "units": units,
        # The ruler pins matter: they are what makes `report generate` and `diff`
        # agree, and a probe on a different ruler would answer a different
        # question. Carry the project's own block verbatim.
        "options": cfg.get("options", {}),
        "map_file": str(map_path.resolve()),
    }
    dest.mkdir(parents=True, exist_ok=True)
    (dest / "objdiff.json").write_text(json.dumps(out))
    return len(units)


def run_report(objdiff: str, proj: Path, out: Path):
    """-> (measures tuple, cache_hits or None). Coerces every numeric: several
    report.json values are JSON STRINGS, and an un-coerced compare is a silent
    lexicographic one."""
    r = subprocess.run([objdiff, "report", "generate", "-p", str(proj),
                        "-o", str(out)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("objdiff-cli report generate failed (rc=%d): %s"
                         % (r.returncode, (r.stderr or "")[-600:]))
    d = json.loads(out.read_text())
    m = d.get("measures", {})
    if not m:
        return None, None
    measures = (
        int(m.get("matched_code", 0) or 0),
        int(m.get("matched_functions", 0) or 0),
        round(float(m.get("matched_code_percent", 0) or 0.0), 6),
        round(float(m.get("fuzzy_match_percent", 0) or 0.0), 6),
    )
    hits = d.get("provenance", {}).get("cache_hits")
    if hits is None:
        mm = re.search(r"Report cache: (\d+) hits", (r.stderr or "") + (r.stdout or ""))
        hits = int(mm.group(1)) if mm else None
    return measures, hits


def fmt(t):
    return ("matched_code=%d fns=%d code%%=%.6f fuzzy=%.6f" % t) if t else "<none>"


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=str(_HERE.parent))
    ap.add_argument("--objdiff", help="binary to test (default: the one build.ninja runs)")
    ap.add_argument("--units", type=int, default=0,
                    help="probe only the first N units (0 = all; all is ~10s and "
                         "is what makes leg R discriminate)")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    objdiff = args.objdiff or consumed_objdiff(root)
    live_map = root / "build/45410914/icf_aliases.map"
    if not live_map.exists():
        print("VACUOUS: %s does not exist -- build the tree first. This guard "
              "asserts nothing." % live_map, file=sys.stderr)
        return 5

    with tempfile.TemporaryDirectory(prefix="mapcache-probe-") as td:
        t = Path(td)
        m1, m2 = t / "m1.map", t / "m2.map"
        shutil.copyfile(live_map, m1)
        # M2: the same map EMPTIED. Keeping the header makes it a content change
        # of the same shape the real generator produces, not a truncated file.
        head = [ln for ln in m1.read_text(errors="replace").splitlines()[:3]]
        m2.write_text("\n".join(head) + "\n")

        p1, p2 = t / "proj1", t / "proj2"
        n1 = write_probe_project(root, p1, m1, args.units)
        n2 = write_probe_project(root, p2, m2, args.units)
        if n1 == 0 or n1 != n2:
            print("VACUOUS: probe project has %d units -- nothing to measure."
                  % n1, file=sys.stderr)
            return 5

        warm = t / "warm.json"      # ONE cache, shared by legs A,B,C,D
        ref = t / "ref.json"        # leg R's own fresh cache

        a, ha = run_report(objdiff, p1, warm)
        b, hb = run_report(objdiff, p1, warm)
        r, hr = run_report(objdiff, p2, ref)
        c, hc = run_report(objdiff, p2, warm)
        d, hd = run_report(objdiff, p1, warm)

    def h(x):
        return "?" if x is None else str(x)

    print("objdiff : %s" % objdiff)
    print("units   : %d" % n1)
    print("  A  M1 cold   %s   (cache_hits=%s)" % (fmt(a), h(ha)))
    print("  B  M1 warm   %s   (cache_hits=%s)" % (fmt(b), h(hb)))
    print("  R  M2 fresh  %s   (cache_hits=%s)" % (fmt(r), h(hr)))
    print("  C  M2 warm   %s   (cache_hits=%s)" % (fmt(c), h(hc)))
    print("  D  M1 warm   %s   (cache_hits=%s)" % (fmt(d), h(hd)))

    if a is None or r is None or c is None or d is None:
        print("VACUOUS: a leg produced no measures block.", file=sys.stderr)
        return 5

    # ---- ANTI-VACUITY: do the two maps change the answer at all? -------------
    if a == r:
        print("VACUOUS: the real map and the emptied map produce the SAME answer "
              "(%s), so leg C cannot discriminate a stale cache from a correct "
              "one. This guard asserts nothing. (Are there any alias groups? "
              "%s)" % (fmt(a), live_map), file=sys.stderr)
        return 5

    if b != a:
        print("VACUOUS: re-running the SAME map over the warm cache changed the "
              "answer (%s -> %s). The probe is not stable enough to test."
              % (fmt(a), fmt(b)), file=sys.stderr)
        return 5

    # ---- the property -------------------------------------------------------
    ok = True
    if c != r:
        ok = False
        if c == a:
            print("FAIL: with a CHANGED map the warm cache served the PREVIOUS "
                  "answer (%s) instead of the correct one (%s). This objdiff "
                  "does NOT key its report cache on the alias map -- it predates "
                  "345778c." % (fmt(a), fmt(r)), file=sys.stderr)
        else:
            print("FAIL: warm-cache answer under the changed map (%s) differs "
                  "from the cold reference (%s)." % (fmt(c), fmt(r)),
                  file=sys.stderr)
    if d != a:
        ok = False
        print("FAIL: restoring the original map did not restore the original "
              "answer (%s != %s)." % (fmt(d), fmt(a)), file=sys.stderr)

    if not ok:
        print("=> RESTORE the icf_aliases_cache_purged edge in tools/project.py "
              "(see the TOMBSTONE comment there), or move the objdiff binary "
              "forward past 345778c. Until then every alias-map measurement on "
              "this tree can be STALE, and it fails as a wrong number, not as "
              "an error.", file=sys.stderr)
        return 2

    print("PASS: this objdiff keys its report cache on the alias map's content "
          "(a changed map missed the cache and returned the correct answer; a "
          "restored map returned the original answer exactly). The "
          "icf_aliases_cache_purged edge is redundant here.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
