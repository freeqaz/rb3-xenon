#!/usr/bin/env python3
"""Cross-reference the objdiff equal-length-fast-path EXPOSED population against
the campaign's own lane record (lane DS-2, 2026-08-03).

Answers: "did the eqlen bug (objdiff 3d511814, 2026-03-12 .. 2026-08-03) ever
mislead a lane into abandoning correct work?"

USAGE
    python3 scripts/harvest/eqlen_lane_xref.py <report_oldbin.json> <report_newbin.json>

Generate the two reports on ONE FIXED TREE with the two binaries (common-mode):
    objdiff-cli.pre-laneDR1-ff7fcf52  report generate -o rep_old.json
    objdiff-cli                       report generate -o rep_new.json
⚠ Wipe BOTH <name>.json and <name>.cache before each run -- the cache path is
  set_extension("cache"), NOT ".json.cache".

THE DIRECTION RULE (the whole point of this script)
    fix > base  => the bug DEFLATED the score. A lane saw a number LOWER than
                   reality => "unexplained collapse" => abandonment-capable.
                   THIS IS THE ONLY HARM-CAPABLE STRATUM.
    fix < base  => the bug INFLATED the score. Cannot cause an abandonment.
DR-1 reported "257 changed / 97 by >=10pp" without splitting by direction; the
split is 63 up / 194 down, and 82 of the 97 large movers are DOWN. So the
harm-capable population is 63 rows, 15 of them >=10pp -- not 257/97.

⚠ THE INSTRUMENT INVERTS ON REVERTED WORK -- read this before trusting it.
Exposure requires TODAY's source to produce an equal instruction count. A lane
that REVERTED its fix left the source back in the unequal-length state, so
reverted work is by construction LARGELY ABSENT from the exposed set. This script
finds the OTHER population: rows filed as walls AT THE LANDED STATE. To hunt
reverted work you must re-apply the reverted patch and re-score under BOTH
binaries (a 2x2); see the doc for the two adjudicated cases.

⚠ "touched" == has a row in decomp.db.attempts, or a terminal functions.verdict.
Map/splits/body-port lanes often never write the DB, so this UNDERCOUNTS lane
attention. It undercounts numerator and denominator alike, so the ENRICHMENT
RATIO is still meaningful; the absolute touch rates are floors, not estimates.
"""
import sqlite3, sys, json, collections, os

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# decomp.db is gitignored, so it exists in the MAIN repo root and NOT in a
# worktree. Resolve explicitly instead of assuming co-location.
_CANDIDATES = [os.environ.get("DECOMP_DB"), os.path.join(REPO, "decomp.db"),
               "/home/free/code/milohax/rb3-xenon/decomp.db"]
DB = next((p for p in _CANDIDATES if p and os.path.exists(p)), None)
if DB is None:
    sys.exit("REFUSED: decomp.db not found; set DECOMP_DB=<path to the 65 MB db>")
# scripts/orchestrator/decomp.db is a 0-byte DECOY; sqlite opens it happily and
# reports zero rows -- i.e. it would report "nothing was ever touched", which is
# exactly this script's headline conclusion. Refuse rather than confirm itself.
if os.path.getsize(DB) < 1_000_000:
    sys.exit(f"REFUSED: {DB} is {os.path.getsize(DB)} B -- not the real database")


def rows(path):
    rep = json.load(open(path))
    out = {}
    for u in rep["units"]:
        for fn in u.get("functions") or []:
            out[(u["name"], fn["name"])] = (
                float(fn["match_percent_normalized"]), int(fn["size"]))
    return out


def main(old_p, new_p):
    ra, rb = rows(old_p), rows(new_p)
    common = set(ra) & set(rb)
    ups, downs = [], []
    for k in common:
        a, sz = ra[k]
        b, _ = rb[k]
        if b > a:
            ups.append((b - a, a, b, sz, k))
        elif b < a:
            downs.append((b - a, a, b, sz, k))
    ups.sort(key=lambda t: -t[0])

    c = sqlite3.connect(DB)
    c.row_factory = sqlite3.Row
    att = collections.defaultdict(list)
    for r in c.execute("""select f.symbol, a.exit_status, a.notes, a.start_percent,
                                 a.end_percent from attempts a
                          join functions f on f.id = a.function_id"""):
        att[r["symbol"]].append(dict(r))
    verd = {r["symbol"]: dict(r) for r in c.execute(
        "select symbol, verdict, verdict_reason from functions "
        "where verdict is not null and verdict != ''")}

    def touch_rate(names):
        n = len(names)
        t = sum(1 for s in names if s in att or s in verd)
        return t, n, (t / n if n else 0.0)

    sub100 = {k[1] for k, v in rb.items() if v[0] < 100.0}
    up_names = {k[1] for _, _, _, _, k in ups}
    up10 = {k[1] for d, _, _, _, k in ups if d >= 10}

    print(f"changed rows: {len(ups) + len(downs)}  "
          f"({len(ups)} UNDER-reported / {len(downs)} over-reported)")
    print(f"  >=10pp: {len(up10)} up / {sum(1 for d,_,_,_,_ in downs if d <= -10)} down\n")
    print("=== TOUCH RATE (control FIRST -- an enrichment without it is unreadable) ===")
    base = None
    for label, names in [("CONTROL: all sub-100 rows", sub100),
                         ("UNDER-reported (harm-capable)", up_names),
                         ("UNDER-reported >=10pp", up10)]:
        t, n, r = touch_rate(names)
        if base is None:
            base = r
        print(f"  {label:32s} n={n:6d}  touched={t:5d} ({100*r:5.1f}%)"
              f"   enrichment {r/base if base else 0:5.2f}x")

    print("\n=== UNDER-REPORTED ROWS + LANE RECORD ===")
    for d, a, b, sz, k in ups:
        rec = []
        if k[1] in verd:
            rec.append(f"verdict={verd[k[1]]['verdict']}")
        for x in att.get(k[1], []):
            rec.append(f"exit={x['exit_status']} {x['start_percent']}->{x['end_percent']}")
        print(f"{d:+8.3f}pp {a:7.3f}->{b:7.3f} sz={sz:5d} {k[0]} :: {k[1][:80]}")
        print(f"          {' | '.join(rec) if rec else '(never worked by any lane)'}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
