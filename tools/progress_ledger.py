#!/usr/bin/env python3
"""Longitudinal progress ledger: is this delta PROGRESS, or did the RULER move?

★ THE MOTIVATING INCIDENT (measured 2026-09-01)
───────────────────────────────────────────────
README said **44,444 matched functions** (2026-08-17).  `report.json` said
**42,276**.  A 2,188-function hole that NO ARTIFACT IN THE TREE COULD EXPLAIN.
It cost a dedicated survey lane to establish the answer: objdiff 4.2.3 -> 4.2.8
changed `mpn` semantics (2,196 of 2,214 lost rows carry BIT-IDENTICAL `fuzzy`,
which source work cannot produce).  Real progress over that window was
**+13,964 B**, and it was hiding underneath a scoreboard that appeared to have
collapsed.

House doctrine says a metric DROP can be a TRUER DENOMINATOR rather than a
regression (feedback_accuracy_beats_headline_percent).  But nothing in this tree
could tell those two apart, so every such event cost a lane.  That is the gap
this closes.

WHAT IT RECORDS, per snapshot, keyed by MERGE COMMIT
───────────────────────────────────────────────────
  1. the headline measures -- BOTH rulers, never conflated (see below);
  2. the gap-partition strata, from tools/reachability_census.py (imported, not
     reimplemented -- one partition engine, or it becomes two rulers);
  3. **the tool provenance** -- objdiff version, tool_commit, tool_binary_hash,
     the resolved `functionRelocDiffs` ruler, a hash of the full diff_config,
     AND the ICF alias map hash/entry-count.  report.json self-declares all of
     it.  This is the field that would have answered the incident in one
     command instead of one lane.

⚠ THE ALIAS MAP IS A RULER INPUT AND THE BRIEF FOR THIS LANE DID NOT NAME IT.
`provenance.map_file_hash` / `map_file_entries` describe
build/45410914/icf_aliases.map.  Per CLAUDE.md that mechanism is worth
**818,416 B / 7.93 pp** of `matched_code` (lane ALIAS-2) -- i.e. ~22% of
everything we count as matched.  A snapshot pair with identical objdiff hashes
but a different map hash is a RULER CHANGE, and reading only the tool fields
would have called it source work.

⛔ TWO RULERS, NEVER CONFLATED
──────────────────────────────
`matched_functions` counts rows at `mpn == 100`.
`matched_code`      sums the size of rows at `fuzzy == 100`.
They are DIFFERENT MEASURES and a change can move one with the other flat --
that is the definition, not an anomaly (CLAUDE.md, lane DB-4).  Every delta this
tool prints is labelled with which ruler produced it, and the classifier treats
the two moving in OPPOSITE directions as positive evidence of a tool change,
because ordinary source work cannot produce it at scale.

HAZARDS HANDLED (each learned expensively, all recorded in CLAUDE.md)
─────────────────────────────────────────────────────────────────────
  * report.json is protobuf-JSON: numerics are sometimes JSON STRINGS and
    DEFAULTS ARE OMITTED.  Every read goes through I()/F() with a default; a
    naive d['matched_code'] raises KeyError and a naive `+` CONCATENATES.
  * `total_code` is NOT a constant.  This lane's brief said it took THREE
    values in four weeks; the recovered scope_map trace shows **~24 distinct
    values** between 2026-07-29 and 2026-09-01, wobbling by ~100 B constantly
    with a handful of large steps (-95,100 on 08-04; -325,804 on 08-09;
    -74,708 on 08-18).  It is read from the key, NEVER hardcoded, and a change
    in it is a first-class verdict class rather than noise to smooth over.
  * A RECONSTRUCTED figure is not a MEASURED one.  Rows carry kind=measured vs
    kind=reconstructed and the classifier REFUSES to reason past missing
    evidence -- it returns INDETERMINATE and says which evidence it lacked.
    Conflating the two is the exact error this tool exists to prevent.

WHERE THE DATA LIVES (and why not decomp.db)
────────────────────────────────────────────
`decomp.db` is GITIGNORED (.gitignore:128).  A ledger living only there is an
INVISIBLE INSTITUTIONAL MEMORY -- this project has already funded two lanes to
rebuild a pipeline that had already run three times, because its output was
gitignored.  So the ledger is COMMITTED TEXT:

    docs/decomp/progress_ledger.jsonl            one line per snapshot
    docs/decomp/progress_ledger_units/<id>.tsv   per-unit sidecar (diffusion)

Usage
─────
    python3 tools/progress_ledger.py record --commit HEAD
    python3 tools/progress_ledger.py record --report <old.json> --id v423-20260820 \\
            --reconstructed --source "report.pre-v424 snapshot" --note "..."
    python3 tools/progress_ledger.py list
    python3 tools/progress_ledger.py classify --from <id> --to <id>
    python3 tools/progress_ledger.py classify --last
    python3 tools/progress_ledger.py classify --from A --to B --deep \\
            --report-a <a.json> --report-b <b.json>
    python3 tools/progress_ledger.py selftest [--prove-can-fail]

KNOWN-ANSWER TESTS (all three reproduced on delivery; see the commit log)
────────────────────────────────────────────────────────────────────────
  1. `25dd39b41192 -> 897b9763ef4a` (objdiff 4.2.3 -> 4.2.8, 12 days apart)
     => RULER_OR_TOOL_CHANGE on four independent lines, reproducing the survey
     lane's forensics unprompted: 2,214 rows left `mpn==100` and 2,193 (99.05%)
     kept a BIT-IDENTICAL `fuzzy`.
  2. `ctl-ruler-none -> ctl-ruler-namecheck` -- a PURE ruler A/B, same tree,
     same tool binary, same alias map.  -674,936 B / -6.539656 pp with
     `matched_functions` BIT-IDENTICAL.  => RULER_OR_TOOL_CHANGE.
  3. `ctl-objdiff-423 -> ctl-objdiff-425` -- the tool boundary in a 99-MINUTE
     window: -2,321 functions, +3,812 bytes, denominator and `masked_equal`
     both bit-identical.  => RULER_OR_TOOL_CHANGE.  This pins the collapse to
     **4.2.3 -> 4.2.5**, not to 4.2.8; the chain was 4.2.3/4.2.5/4.2.6/4.2.7/
     4.2.8.  It is also LARGER than the 12-day figure, i.e. real source work
     recovered ~100 functions in between -- progress that the headline hid.

⚠ TWO TRAPS FOR ANYONE EXTENDING THE BACKFILL
  * `functionRelocDiffs` was `name_check` on BOTH sides of test 1, and all 22
    `diff_config` keys were identical.  Only tool_version/commit/binary_hash and
    the alias map moved.  **A tracker watching only the ruler string would have
    called that SOURCE WORK.**
  * Reports written before 2026-08-12 carry **no `provenance` block at all**, so
    every pair spanning that date is INDETERMINATE by construction unless
    unit-level evidence corroborates.  That is the honest answer, not a gap to
    paper over by guessing the ruler from the percentage level.

Exit codes: 0 ok / 1 error or selftest failure / 2 refusal (precondition).
"""
import argparse
import collections
import datetime
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
sys.path.insert(0, str(REPO))

LEDGER = REPO / "docs" / "decomp" / "progress_ledger.jsonl"
UNITDIR = REPO / "docs" / "decomp" / "progress_ledger_units"
SCHEMA = 1

# ── thresholds, named and justified rather than tuned ──────────────────────
# "diffuses across hundreds of unrelated units" is the measured shape of a tool
# change; source progress concentrates in the units a lane touched.
DIFFUSE_UNITS = 100
CONCENTRATED_TOP10 = 0.90     # >=90% of |delta| in <=10 units => concentrated
# Below these, a movement is too small to carry a verdict of its own.
SIG_FNS = 50
SIG_BYTES = 2000


def I(x, d=0):
    """int-coerce a protobuf-JSON numeric that may be a STRING or ABSENT."""
    if x is None:
        return d
    return int(str(x))


def F(x, d=0.0):
    if x is None:
        return d
    return float(x)


# ═══════════════════════════════════════════════════════════════════════════
# Extraction
# ═══════════════════════════════════════════════════════════════════════════

MEASURE_KEYS_INT = ["total_code", "total_functions", "matched_code",
                    "matched_functions", "masked_equal_functions",
                    "complete_code", "total_units", "complete_units",
                    "total_data"]
MEASURE_KEYS_FLT = ["matched_code_percent", "matched_functions_percent",
                    "fuzzy_match_percent", "complete_code_percent"]


def extract_measures(rep):
    m = rep.get("measures") or {}
    out = {k: I(m.get(k)) for k in MEASURE_KEYS_INT}
    out.update({k: F(m.get(k)) for k in MEASURE_KEYS_FLT})
    # The honest floor: matched minus the funclet byte-signature pairings that
    # objdiff discloses as masked_equal.  (CLAUDE.md: honest = matched - masked.)
    out["honest_matched_functions"] = (out["matched_functions"]
                                       - out["masked_equal_functions"])
    return out


def extract_provenance(rep):
    """Pull the ruler identity out of report.json's self-declared provenance.

    Returns a dict whose values are None when the report predates the
    provenance block -- an ABSENCE, which the classifier must treat as missing
    evidence rather than as 'unchanged'.
    """
    p = rep.get("provenance") or {}
    cfg = p.get("diff_config")
    ruler = None
    if cfg:
        for entry in cfg:
            if str(entry).startswith("functionRelocDiffs="):
                ruler = str(entry).split("=", 1)[1]
    cfg_hash = None
    if cfg:
        cfg_hash = hashlib.sha256(
            "\n".join(sorted(str(c) for c in cfg)).encode()).hexdigest()[:16]
    return {
        "tool_version": p.get("tool_version"),
        "tool_commit": p.get("tool_commit"),
        "tool_binary_hash": p.get("tool_binary_hash"),
        "ruler": ruler,
        "diff_config_hash": cfg_hash,
        "diff_config": list(cfg) if cfg else None,
        # The ICF alias map is a ruler input worth ~7.9pp of matched_code.
        "map_file": p.get("map_file"),
        "map_file_hash": p.get("map_file_hash"),
        "map_file_entries": p.get("map_file_entries"),
        "present": bool(p),
    }


# Provenance fields whose change means "the measuring instrument changed".
PROV_FIELDS = ["tool_version", "tool_commit", "tool_binary_hash", "ruler",
               "diff_config_hash", "map_file_hash", "map_file_entries"]


def extract_strata(rep):
    """Gap partition, delegated to tools/reachability_census.py.

    Imported rather than reimplemented on purpose.  Returns None if the census
    cannot reconcile the report against its own four keys -- that census exits 1
    rather than report a partition it cannot validate, and this inherits that.
    """
    import tools.reachability_census as rc
    rows = rc.rows_from_report(rep)
    m = rep.get("measures") or {}
    # Replicate the census's self-validation WITHOUT its printing.
    if (len(rows) != I(m.get("total_functions"))
            or sum(r["size"] for r in rows) != I(m.get("total_code"))
            or sum(r["size"] for r in rows if r["fuzzy"] == 100.0)
                != I(m.get("matched_code"))
            or sum(1 for r in rows if r["mpn"] == 100.0)
                != I(m.get("matched_functions"))):
        return None
    strata = rc.partition_rows(rows)
    out = collections.OrderedDict()
    out["MATCHED (fuzzy==100)"] = {
        "rows": sum(1 for r in rows if r["fuzzy"] == 100.0),
        "bytes": sum(r["size"] for r in rows if r["fuzzy"] == 100.0)}
    for k, v in strata.items():
        out[k] = {"rows": len(v), "bytes": sum(r["size"] for r in v)}
    return out


def extract_units(rep):
    """Per-unit (matched_code, matched_functions, total_code, total_functions).

    This is the DIFFUSION evidence: a tool/ruler change moves hundreds of
    unrelated units at once, real progress concentrates in the touched ones.
    """
    out = {}
    for u in rep.get("units") or []:
        um = u.get("measures") or {}
        tc, tf = I(um.get("total_code")), I(um.get("total_functions"))
        mc, mf = I(um.get("matched_code")), I(um.get("matched_functions"))
        if tc or tf or mc or mf:
            out[u["name"]] = (mc, mf, tc, tf)
    return out


def fn_map(rep):
    """(unit, name, occurrence) -> (mpn, fuzzy), for the deep test.

    Occurrence bookkeeping is IDENTICAL to scripts/harvest/snapshot_landing.py
    ::pct_rows and measure_delta.py::pct_map, so the three tools agree on
    function identity (~11 binary-wide duplicate names exist within a unit).
    """
    out, seen = {}, {}
    for u in rep.get("units") or []:
        un = u.get("name")
        for f in (u.get("functions") or []):
            k = (un, f.get("name", ""))
            i = seen.get(k, 0)
            seen[k] = i + 1
            out[(un, f.get("name", ""), i)] = (
                F(f.get("match_percent_normalized")),
                F(f.get("fuzzy_match_percent")))
    return out


# ═══════════════════════════════════════════════════════════════════════════
# Ledger I/O
# ═══════════════════════════════════════════════════════════════════════════

def load_ledger(path=None):
    p = Path(path) if path else LEDGER
    if not p.exists():
        return []
    rows = []
    for n, line in enumerate(p.read_text().splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError as e:
            raise SystemExit(f"error: {p}:{n} is not valid JSON: {e}")
    return rows


def append_ledger(row, path=None):
    p = Path(path) if path else LEDGER
    p.parent.mkdir(parents=True, exist_ok=True)
    with open(p, "a") as fh:
        fh.write(json.dumps(row, sort_keys=False) + "\n")


def git(*args, cwd=None):
    try:
        r = subprocess.run(["git"] + list(args), cwd=str(cwd or REPO),
                           capture_output=True, text=True, timeout=60)
        return r.stdout.strip() if r.returncode == 0 else None
    except Exception:
        return None


# ═══════════════════════════════════════════════════════════════════════════
# record
# ═══════════════════════════════════════════════════════════════════════════

def cmd_record(a):
    report_path = Path(a.report) if a.report else (
        Path(a.project or REPO) / "build/45410914/report.json")
    if not report_path.exists():
        print(f"REFUSED: no report.json at {report_path}", file=sys.stderr)
        return 2
    rep = json.loads(report_path.read_text())

    commit = full = None
    if a.commit:
        full = git("rev-parse", a.commit) or a.commit
        commit = full
    snap_id = a.id or (commit[:12] if commit else None)
    if not snap_id:
        print("REFUSED: need --commit or --id to key the snapshot",
              file=sys.stderr)
        return 2

    existing = {r["snapshot_id"] for r in load_ledger(a.ledger)}
    if snap_id in existing and not a.replace:
        print(f"REFUSED: snapshot_id {snap_id} already in the ledger "
              f"(pass --replace to supersede)", file=sys.stderr)
        return 2

    measures = extract_measures(rep)
    prov = extract_provenance(rep)
    strata = extract_strata(rep)
    if strata is None:
        # Inherited from reachability_census: never publish a census that does
        # not reconcile against the report's own four keys.
        msg = ("REFUSED: report does not self-reconcile (rows vs "
               "total_functions / bytes vs total_code / matched_code / "
               "matched_functions) -- the partition would be untrustworthy")
        if not a.allow_unreconciled:
            print(msg, file=sys.stderr)
            return 2
        print("WARNING: " + msg.replace("REFUSED", "unreconciled"),
              file=sys.stderr)

    kind = "reconstructed" if a.reconstructed else "measured"
    row = collections.OrderedDict()
    row["schema"] = SCHEMA
    row["snapshot_id"] = snap_id
    row["kind"] = kind
    row["commit"] = commit
    row["commit_date"] = git("show", "-s", "--format=%cI", commit) if commit else None
    row["commit_subject"] = git("show", "-s", "--format=%s", commit) if commit else None
    row["recorded_at"] = datetime.datetime.now().astimezone().isoformat(
        timespec="seconds")
    row["source"] = a.source or str(report_path)
    row["report_sha256"] = hashlib.sha256(report_path.read_bytes()).hexdigest()[:16]
    row["measures"] = measures
    row["strata"] = strata
    row["provenance"] = prov
    row["note"] = a.note or ""

    units = extract_units(rep)
    unit_file = None
    if units and not a.no_units:
        UNITDIR.mkdir(parents=True, exist_ok=True)
        unit_file = UNITDIR / f"{snap_id}.tsv"
        lines = ["# unit\tmatched_code\tmatched_functions\ttotal_code\ttotal_functions"]
        for name in sorted(units):
            mc, mf, tc, tf = units[name]
            lines.append(f"{name}\t{mc}\t{mf}\t{tc}\t{tf}")
        if not a.dry_run:
            unit_file.write_text("\n".join(lines) + "\n")
    row["units_file"] = (str(unit_file.relative_to(REPO)) if unit_file else None)

    if a.dry_run:
        print(json.dumps(row, indent=1))
        return 0

    if snap_id in existing and a.replace:
        p = Path(a.ledger) if a.ledger else LEDGER
        keep = [r for r in load_ledger(a.ledger) if r["snapshot_id"] != snap_id]
        p.write_text("".join(json.dumps(r, sort_keys=False) + "\n" for r in keep))
    append_ledger(row, a.ledger)
    m = measures
    print(f"recorded {snap_id} [{kind}]  matched_fns {m['matched_functions']:,} "
          f"(mpn ruler) / matched_code {m['matched_code']:,} B (fuzzy ruler) / "
          f"total_code {m['total_code']:,} / ruler={prov['ruler']} "
          f"objdiff={prov['tool_version']}")
    return 0


# ═══════════════════════════════════════════════════════════════════════════
# backfill -- headline figures with NO report.json behind them
# ═══════════════════════════════════════════════════════════════════════════

def cmd_backfill(a):
    """Record a RECONSTRUCTED row from a dated figure in the docs/README record.

    These rows exist so the series starts populated rather than in six weeks.
    They are NOT measurements and the ledger says so in three places: kind,
    source, and derived_fields.  A reconstructed row carries NO provenance, so
    classify() will return INDETERMINATE across it unless unit-level evidence
    corroborates -- which is the correct answer, not a limitation.
    """
    existing = {r["snapshot_id"] for r in load_ledger(a.ledger)}
    if a.id in existing and not a.replace:
        print(f"REFUSED: snapshot_id {a.id} already in the ledger",
              file=sys.stderr)
        return 2
    if not a.source:
        print("REFUSED: a reconstructed row MUST cite where the figure came "
              "from (--source 'file:line' or 'commit README table')",
              file=sys.stderr)
        return 2

    tc, tf = a.total_code, a.total_functions
    mc, derived = a.matched_code, []
    if mc is None and a.matched_code_percent is not None and tc:
        mc = int(round(tc * a.matched_code_percent / 100.0))
        derived.append("matched_code (from matched_code_percent x total_code -- "
                       "a DERIVED byte count, not a read one)")
    measures = {k: 0 for k in MEASURE_KEYS_INT}
    measures.update({k: 0.0 for k in MEASURE_KEYS_FLT})
    measures["total_code"] = tc or 0
    measures["total_functions"] = tf or 0
    measures["matched_code"] = mc or 0
    measures["matched_functions"] = a.matched_functions or 0
    measures["masked_equal_functions"] = a.masked_equal or 0
    measures["matched_code_percent"] = (
        a.matched_code_percent if a.matched_code_percent is not None
        else (100.0 * mc / tc if (mc and tc) else 0.0))
    measures["matched_functions_percent"] = (
        100.0 * a.matched_functions / tf
        if (a.matched_functions and tf) else 0.0)
    measures["honest_matched_functions"] = (measures["matched_functions"]
                                            - measures["masked_equal_functions"])
    unknown = [k for k in ("total_code", "total_functions", "matched_code",
                           "matched_functions")
               if not measures[k]]
    if a.masked_equal is None:
        # Without masked_equal the honest floor (matched - masked) cannot be
        # formed.  Leaving it equal to matched_functions would silently invent
        # a ~22,900-function improvement out of an absence.
        unknown.append("masked_equal_functions")
        measures["honest_matched_functions"] = 0

    prov = {f: None for f in PROV_FIELDS}
    prov.update({"diff_config": None, "map_file": None, "present": False})
    if a.ruler or a.tool_version:
        # A ruler asserted from the docs record is still a RECONSTRUCTION.
        prov["ruler"], prov["tool_version"] = a.ruler, a.tool_version
        prov["present"] = bool(a.ruler and a.tool_version)
        derived.append("provenance asserted from prose, not read from a "
                       "report.json provenance block")

    row = collections.OrderedDict()
    row["schema"] = SCHEMA
    row["snapshot_id"] = a.id
    row["kind"] = "reconstructed"
    row["commit"] = git("rev-parse", a.commit) if a.commit else None
    row["commit_date"] = a.date
    row["commit_subject"] = None
    row["recorded_at"] = datetime.datetime.now().astimezone().isoformat(
        timespec="seconds")
    row["source"] = a.source
    row["report_sha256"] = None
    row["measures"] = measures
    row["strata"] = None
    row["provenance"] = prov
    row["note"] = a.note or ""
    row["derived_fields"] = derived
    row["unknown_fields"] = unknown
    row["units_file"] = None

    if a.dry_run:
        print(json.dumps(row, indent=1))
        return 0
    if a.id in existing and a.replace:
        p = Path(a.ledger) if a.ledger else LEDGER
        keep = [r for r in load_ledger(a.ledger) if r["snapshot_id"] != a.id]
        p.write_text("".join(json.dumps(r, sort_keys=False) + "\n" for r in keep))
    append_ledger(row, a.ledger)
    print(f"backfilled {a.id} [reconstructed] from {a.source}"
          + (f"\n  DERIVED: {'; '.join(derived)}" if derived else "")
          + (f"\n  UNKNOWN (recorded as 0): {', '.join(unknown)}" if unknown else ""))
    return 0


# ═══════════════════════════════════════════════════════════════════════════
# list
# ═══════════════════════════════════════════════════════════════════════════

def cmd_list(a):
    rows = load_ledger(a.ledger)
    if not rows:
        print("ledger is empty")
        return 0
    rows.sort(key=lambda r: (r.get("commit_date") or r.get("recorded_at") or ""))
    w = max(16, max(len(r["snapshot_id"]) for r in rows) + 2)
    print(f"{'ID':<{w}}{'KIND':<15}{'DATE':<12}{'matched_fns':>12}"
          f"{'matched_code':>14}{'total_code':>13}{'code%':>9}"
          f"{'objdiff':>9}  {'ruler':<12}")
    print("-" * (w + 106))
    for r in rows:
        m = r.get("measures") or {}
        pv = r.get("provenance") or {}
        unk = set(r.get("unknown_fields") or [])
        d = (r.get("commit_date") or r.get("recorded_at") or "")[:10]

        def cell(key, width, fmt=",", unknown_mark="?"):
            # An UNKNOWN measure must never render as a confident 0.
            if key in unk:
                return f"{unknown_mark:>{width}}"
            v = m.get(key, 0)
            return f"{v:>{width}{fmt}}"

        print(f"{r['snapshot_id']:<{w}}{r.get('kind','?'):<15}{d:<12}"
              f"{cell('matched_functions', 12)}"
              f"{cell('matched_code', 14)}"
              f"{cell('total_code', 13)}"
              f"{cell('matched_code_percent', 9, '.4f')}"
              f"{str(pv.get('tool_version') or '-'):>9}  "
              f"{str(pv.get('ruler') or '-'):<12}")
    print("\nmatched_fns is the mpn ruler; matched_code is the fuzzy ruler. "
          "They are NOT the same measure.")
    print("kind=reconstructed rows are NOT measurements -- see each row's "
          "`source` and `note`.")
    return 0


# ═══════════════════════════════════════════════════════════════════════════
# classify
# ═══════════════════════════════════════════════════════════════════════════

def load_units_file(row):
    f = row.get("units_file")
    if not f:
        return None
    p = REPO / f
    if not p.exists():
        return None
    out = {}
    for line in p.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) != 5:
            continue
        out[parts[0]] = tuple(int(x) for x in parts[1:])
    return out or None


def classify(a_row, b_row, deep=None, units_a=None, units_b=None):
    """Return a verdict dict.  EVERY verdict names the evidence it used.

    Deliberately a rule cascade with recorded reasons, not a scored classifier:
    a defensible heuristic that says which evidence it used beats a confident
    black box (and this repo's ledger of confidently-wrong tools is long).
    """
    ma, mb = a_row.get("measures") or {}, b_row.get("measures") or {}
    pa, pb = a_row.get("provenance") or {}, b_row.get("provenance") or {}
    ev, used, missing = collections.OrderedDict(), [], []

    # ⛔ A backfilled row records an UNKNOWN measure as 0.  Differencing against
    # that 0 fabricates a colossal delta out of an absence -- exactly the
    # measured-vs-reconstructed conflation this tool exists to prevent.  Refuse
    # instead.
    unk = set(a_row.get("unknown_fields") or []) | set(
        b_row.get("unknown_fields") or [])
    # Per-field, not all-or-nothing: a row that knows matched_functions but not
    # total_functions is still worth something, provided the tool says which
    # comparisons it could not make.
    if {"matched_code", "matched_functions"} <= unk:
        return dict(
            verdict="INDETERMINATE",
            reasons=["neither score is known on one endpoint; there is nothing "
                     "to compare. A 0 here means UNKNOWN, and differencing "
                     "against an absence fabricates a delta."],
            evidence={"deltas": {"status": f"REFUSED (unknown: {sorted(unk)})"}},
            evidence_used=[], evidence_missing=["measures"] + sorted(unk))

    d_mf = mb.get("matched_functions", 0) - ma.get("matched_functions", 0)
    d_mc = mb.get("matched_code", 0) - ma.get("matched_code", 0)
    d_tc = mb.get("total_code", 0) - ma.get("total_code", 0)
    d_tf = mb.get("total_functions", 0) - ma.get("total_functions", 0)
    d_pct = mb.get("matched_code_percent", 0) - ma.get("matched_code_percent", 0)
    d_honest = (None if "masked_equal_functions" in (
                    set(a_row.get("unknown_fields") or [])
                    | set(b_row.get("unknown_fields") or []))
                else mb.get("honest_matched_functions", 0)
                     - ma.get("honest_matched_functions", 0))

    # A field UNKNOWN on either endpoint yields no delta at all -- never a 0.
    d_mf = None if "matched_functions" in unk else d_mf
    d_mc = None if "matched_code" in unk else d_mc
    d_tc = None if "total_code" in unk else d_tc
    d_tf = None if "total_functions" in unk else d_tf
    ev["deltas"] = {"matched_functions (mpn ruler)": d_mf,
                    "matched_code (fuzzy ruler)": d_mc,
                    "matched_code_percent": round(d_pct, 6),
                    "honest_matched_functions": d_honest,
                    "total_code": d_tc, "total_functions": d_tf}
    if unk:
        ev["deltas"]["UNKNOWN_ON_AN_ENDPOINT"] = sorted(unk)
        missing.append("measures:" + ",".join(sorted(unk)))
    # Downstream arithmetic uses 0 for an absent delta but the SIGNAL flags
    # below are gated on the field actually being known.
    kn_mf, kn_mc = d_mf is not None, d_mc is not None
    kn_tc, kn_tf = d_tc is not None, d_tf is not None
    z_mf, z_mc = (d_mf or 0), (d_mc or 0)

    # ── E1 provenance: DIRECT evidence of an instrument change ──
    if pa.get("present") and pb.get("present"):
        changed = [f for f in PROV_FIELDS if pa.get(f) != pb.get(f)]
        ev["provenance"] = {"status": "COMPARED", "changed": changed,
                            "detail": {f: [pa.get(f), pb.get(f)] for f in changed}}
        used.append("provenance")
        prov_changed = bool(changed)
        prov_known = True
    else:
        which = [n for n, p in (("A", pa), ("B", pb)) if not p.get("present")]
        ev["provenance"] = {"status": f"UNAVAILABLE (no provenance block in {'/'.join(which)})",
                            "changed": None}
        missing.append("provenance")
        prov_changed, prov_known = False, False

    # ── E2 denominator ──
    ev["denominator"] = {"status": ("COMPARED" if (kn_tc and kn_tf)
                                    else "PARTIAL (a denominator field is "
                                         "unknown on an endpoint)"),
                         "d_total_code": d_tc, "d_total_functions": d_tf,
                         "note": ("total_code has taken three values in four "
                                  "weeks; a move here is a DENOMINATOR event, "
                                  "not necessarily a regression")}
    used.append("denominator")
    # MATERIALITY: the recovered trace shows total_code wobbling by ~100 B
    # between ordinary landings.  A sub-threshold wobble is REPORTED but is not
    # allowed to become a verdict component, or a -16 B jitter outranks a
    # -785,380 B ruler flip (measured: it did, on the 2026-08-13 pair).
    denom_moved = bool(d_tc or d_tf)
    denom_changed = bool(abs(d_tc or 0) >= SIG_BYTES
                         or abs(d_tf or 0) >= SIG_FNS)
    denom_known = kn_tc and kn_tf
    ev["denominator"]["material"] = denom_changed
    ev["denominator"]["materiality_threshold_bytes"] = SIG_BYTES

    # ── E3 ruler decoupling: the two rulers moving apart ──
    both_scores_known = kn_mf and kn_mc
    opposite = both_scores_known and (
        (z_mf > SIG_FNS and z_mc < -SIG_BYTES) or
        (z_mf < -SIG_FNS and z_mc > SIG_BYTES))
    decoupled = both_scores_known and (abs(z_mf) > SIG_FNS
                                       and abs(z_mc) < SIG_BYTES)
    # ── the MIRROR IMAGE, and the shape of the 2026-08-13 name_check flip:
    # matched_code moves by ~1% of the binary while matched_functions is flat.
    # CLAUDE.md measured that flip at -817,184 B with matched_functions
    # BIT-IDENTICAL.  Source work cannot do this: only the arg-only stratum
    # (mpn==100, fuzzy<100) can move bytes without moving functions, and that
    # stratum is documented ~91% irreducible.  Threshold is 1% of total_code,
    # READ from the row -- never a hardcoded byte count.
    big = max(1, int(0.01 * (mb.get("total_code") or ma.get("total_code") or 0)))
    bytes_moved_fns_flat = both_scores_known and (abs(z_mc) >= big
                                                  and abs(z_mf) < SIG_FNS)
    ev["ruler_decoupling"] = {
        "status": ("COMPARED" if both_scores_known
                   else "UNAVAILABLE (needs BOTH rulers known on both "
                        "endpoints)"),
        "opposite_signs": opposite,
        "fns_moved_bytes_flat": decoupled,
        "bytes_moved_fns_flat": bytes_moved_fns_flat,
        "big_byte_threshold_1pct_total_code": big,
        "note": ("matched_functions counts mpn==100; matched_code sums "
                 "fuzzy==100. Opposite signs at scale is not a shape ordinary "
                 "source work produces.")}
    (used if both_scores_known else missing).append("ruler_decoupling")

    # ── E4 unit diffusion ──
    if units_a and units_b:
        moved = {}
        for u in set(units_a) | set(units_b):
            va = units_a.get(u, (0, 0, 0, 0))
            vb = units_b.get(u, (0, 0, 0, 0))
            dd = vb[0] - va[0]
            if dd:
                moved[u] = dd
        tot = sum(abs(v) for v in moved.values())
        top = sorted(moved.items(), key=lambda kv: -abs(kv[1]))
        top10 = sum(abs(v) for _, v in top[:10]) / tot if tot else 0.0
        ev["unit_diffusion"] = {
            "status": "COMPARED", "units_changed": len(moved),
            "top10_share_of_abs_delta": round(top10, 4),
            "top_movers": [[u, v] for u, v in top[:10]],
            "note": ("a tool/ruler change diffuses across hundreds of "
                     "unrelated units; real progress concentrates")}
        used.append("unit_diffusion")
        diffuse = len(moved) >= DIFFUSE_UNITS and top10 < CONCENTRATED_TOP10
        concentrated = (tot > 0 and top10 >= CONCENTRATED_TOP10)
    else:
        ev["unit_diffusion"] = {"status": "UNAVAILABLE (no per-unit sidecar on "
                                          "one or both snapshots)"}
        missing.append("unit_diffusion")
        diffuse = concentrated = False

    # ── E5 deep: per-function fuzzy identity (the decisive test) ──
    fuzzy_identical_share = None
    if deep:
        ev["fuzzy_identity"] = deep
        used.append("fuzzy_identity")
        if deep.get("rows_lost_mpn100"):
            fuzzy_identical_share = deep.get("share_bit_identical_fuzzy")
    else:
        ev["fuzzy_identity"] = {"status": "NOT RUN (pass --deep with both "
                                          "report.json files)"}
        missing.append("fuzzy_identity")

    # ── verdict cascade ────────────────────────────────────────────────
    reasons, components = [], []
    scores_moved = bool(z_mf or z_mc)

    if denom_moved and not denom_changed:
        reasons.append(f"denominator moved (total_code {(d_tc or 0):+,} B, "
                       f"total_functions {(d_tf or 0):+,}) but BELOW the "
                       f"materiality thresholds ({SIG_BYTES:,} B / {SIG_FNS} "
                       f"fns) -- reported, not treated as a denominator event; "
                       f"both wobble routinely between landings")
    # A denominator move that is far too small to account for the score move is
    # not an explanation, and must not be allowed to masquerade as one.
    if denom_changed and kn_mf and abs(z_mf) > 10 * max(1, abs(d_tf or 0)):
        reasons.append(f"NOTE: matched_functions moved {z_mf:+,} but "
                       f"total_functions only {(d_tf or 0):+,} -- the "
                       f"denominator move is far too small to account for it, "
                       f"so something else is also in play")
    if not scores_moved and not denom_moved and not prov_changed:
        return dict(verdict="NO_CHANGE",
                    reasons=["no measure and no provenance field moved"],
                    evidence=ev, evidence_used=used, evidence_missing=missing)

    if prov_changed:
        components.append("RULER_OR_TOOL_CHANGE")
        reasons.append("provenance fields differ: "
                       + ", ".join(ev["provenance"]["changed"])
                       + " -- this is DIRECT evidence the instrument changed")
    if denom_changed:
        components.append("DENOMINATOR_CHANGE")
        reasons.append(f"denominator moved: total_code {(d_tc or 0):+,}, "
                       f"total_functions {(d_tf or 0):+,} -- scores are not comparable "
                       f"as absolutes across this boundary")
    if opposite:
        reasons.append(f"the two rulers moved in OPPOSITE directions "
                       f"(matched_functions {z_mf:+,} vs matched_code "
                       f"{z_mc:+,} B) -- source work does not do this at scale")
        if "RULER_OR_TOOL_CHANGE" not in components:
            components.append("RULER_OR_TOOL_CHANGE")
    elif decoupled:
        reasons.append(f"matched_functions moved {z_mf:+,} while matched_code "
                       f"stayed flat ({z_mc:+,} B) -- an mpn-only movement")
    if bytes_moved_fns_flat:
        reasons.append(f"matched_code moved {z_mc:+,} B (>= 1% of total_code) "
                       f"while matched_functions stayed flat ({z_mf:+,}) -- "
                       f"only the arg-only stratum can move bytes without "
                       f"moving functions, and it is ~91% irreducible; this is "
                       f"the shape of the 2026-08-13 name_check flip")
        if "RULER_OR_TOOL_CHANGE" not in components:
            components.append("RULER_OR_TOOL_CHANGE")
    if fuzzy_identical_share is not None and fuzzy_identical_share >= 0.9:
        reasons.append(f"{fuzzy_identical_share:.1%} of rows that fell out of "
                       f"mpn==100 have BIT-IDENTICAL fuzzy -- source work "
                       f"cannot produce that; the scoring semantics changed")
        if "RULER_OR_TOOL_CHANGE" not in components:
            components.append("RULER_OR_TOOL_CHANGE")
    if diffuse:
        reasons.append(f"{ev['unit_diffusion']['units_changed']} units moved "
                       f"with no concentration (top10 = "
                       f"{ev['unit_diffusion']['top10_share_of_abs_delta']:.1%}"
                       f" of |delta|) -- diffuse, not lane-shaped")
    if concentrated:
        reasons.append(f"movement concentrates: top10 units carry "
                       f"{ev['unit_diffusion']['top10_share_of_abs_delta']:.1%}"
                       f" of |delta| -- lane-shaped")

    if components:
        if len(components) > 1:
            verdict = "MIXED"
            reasons.append("more than one mechanism is present; the score "
                           "delta CANNOT be attributed to source work without "
                           "re-measuring on a single ruler")
        else:
            verdict = components[0]
        if concentrated and not diffuse and scores_moved:
            verdict = "MIXED"
            reasons.append("an instrument/denominator change is present AND "
                           "the movement is lane-shaped -- real work may be "
                           "superimposed; separate them before crediting")
        return dict(verdict=verdict, reasons=reasons, evidence=ev,
                    evidence_used=used, evidence_missing=missing)

    # No instrument change and no denominator change detected.
    if not prov_known:
        # We could not RULE OUT a tool change.  Only strong corroboration
        # licenses a source verdict; otherwise say so rather than guess.
        if concentrated and not diffuse:
            v = "SOURCE_PROGRESS" if (z_mc >= 0 and z_mf >= 0) else "REGRESSION"
            reasons.append("provenance unavailable, but the movement is "
                           "concentrated in few units, which a ruler change "
                           "does not produce -- verdict rests on diffusion "
                           "alone and is weaker than a provenance-backed one")
            return dict(verdict=v, reasons=reasons, evidence=ev,
                        evidence_used=used, evidence_missing=missing,
                        confidence="LOW (no provenance)")
        reasons.append("no provenance on one or both snapshots, and no "
                       "corroborating unit-level evidence -- a tool/ruler "
                       "change CANNOT BE EXCLUDED, so no verdict is earned")
        return dict(verdict="INDETERMINATE", reasons=reasons, evidence=ev,
                    evidence_used=used, evidence_missing=missing)

    if not both_scores_known:
        reasons.append("only one of the two rulers is known on this pair; a "
                       "SOURCE vs REGRESSION call needs both, because "
                       "matched_functions (mpn) and matched_code (fuzzy) move "
                       "independently by construction")
        return dict(verdict="INDETERMINATE", reasons=reasons, evidence=ev,
                    evidence_used=used, evidence_missing=missing)
    if z_mc >= 0 and z_mf >= 0:
        reasons.append(f"instrument identical on all {len(PROV_FIELDS)} "
                       f"provenance fields, denominator identical, both rulers "
                       f"non-negative ({z_mf:+,} fns / {z_mc:+,} B)")
        return dict(verdict="SOURCE_PROGRESS", reasons=reasons, evidence=ev,
                    evidence_used=used, evidence_missing=missing)
    if z_mc <= 0 and z_mf <= 0:
        reasons.append(f"instrument identical on all {len(PROV_FIELDS)} "
                       f"provenance fields, denominator identical, and both "
                       f"rulers fell ({z_mf:+,} fns / {z_mc:+,} B) -- nothing "
                       f"external explains it")
        return dict(verdict="REGRESSION", reasons=reasons, evidence=ev,
                    evidence_used=used, evidence_missing=missing)
    reasons.append(f"instrument and denominator identical, but the two rulers "
                   f"disagree in sign ({z_mf:+,} fns / {z_mc:+,} B) -- "
                   f"expected when a change moves mpn without moving fuzzy or "
                   f"vice versa; adjudicate per row")
    return dict(verdict="MIXED", reasons=reasons, evidence=ev,
                evidence_used=used, evidence_missing=missing)


def deep_compare(path_a, path_b):
    """Per-function test: of the rows that LEFT mpn==100, how many kept a
    BIT-IDENTICAL fuzzy?  A body edit changes bytes, so it moves fuzzy too;
    a scoring-semantics change moves mpn while fuzzy is untouched."""
    a = fn_map(json.loads(Path(path_a).read_text()))
    b = fn_map(json.loads(Path(path_b).read_text()))
    common = set(a) & set(b)
    lost = [k for k in common if a[k][0] == 100.0 and b[k][0] < 100.0]
    gained = [k for k in common if a[k][0] < 100.0 and b[k][0] == 100.0]
    same_fuzzy = sum(1 for k in lost if a[k][1] == b[k][1])
    return {"status": "COMPARED",
            "rows_common": len(common),
            "rows_only_in_a": len(set(a) - set(b)),
            "rows_only_in_b": len(set(b) - set(a)),
            "rows_lost_mpn100": len(lost),
            "rows_gained_mpn100": len(gained),
            "lost_with_bit_identical_fuzzy": same_fuzzy,
            "share_bit_identical_fuzzy": (same_fuzzy / len(lost)) if lost else None,
            "note": ("source work that un-matches a function must also change "
                     "its bytes, hence its fuzzy; identical fuzzy on a lost "
                     "row means the RULER moved, not the code")}


def print_verdict(a_row, b_row, v):
    print("=" * 78)
    print(f"  {a_row['snapshot_id']} [{a_row.get('kind')}]  ->  "
          f"{b_row['snapshot_id']} [{b_row.get('kind')}]")
    print("=" * 78)
    d = v["evidence"]["deltas"]
    print("\nDELTAS (each labelled with its ruler):")
    for k, val in d.items():
        if val is None:
            # UNKNOWN on an endpoint -- never render an absence as a 0 delta.
            print(f"  {k:<38} (unknown on an endpoint -- not differenced)")
        elif isinstance(val, bool) or not isinstance(val, (int, float)):
            print(f"  {k:<38} {val}")
        else:
            print(f"  {k:<38} {val:+,}" if isinstance(val, int)
                  else f"  {k:<38} {val:+}")
    print(f"\nVERDICT: {v['verdict']}"
          + (f"   confidence={v['confidence']}" if v.get("confidence") else ""))
    print("\nWHY:")
    for r in v["reasons"]:
        print(f"  - {r}")
    print(f"\nEVIDENCE USED:    {', '.join(v['evidence_used']) or '(none)'}")
    print(f"EVIDENCE MISSING: {', '.join(v['evidence_missing']) or '(none)'}")
    for name in ("provenance", "unit_diffusion", "fuzzy_identity"):
        e = v["evidence"].get(name) or {}
        if e.get("status", "").startswith("COMPARED"):
            print(f"\n[{name}] " + json.dumps(
                {k: val for k, val in e.items() if k != "note"}, indent=1)[:1400])
    if any(r.get("kind") == "reconstructed" for r in (a_row, b_row)):
        print("\n⚠ At least one endpoint is RECONSTRUCTED, not measured. A "
              "reconstructed figure carries no provenance and no strata; treat "
              "this verdict as an orientation, not a measurement.")


def cmd_classify(a):
    rows = load_ledger(a.ledger)
    if len(rows) < 2:
        print("REFUSED: need at least 2 snapshots in the ledger",
              file=sys.stderr)
        return 2
    rows.sort(key=lambda r: (r.get("commit_date") or r.get("recorded_at") or ""))
    if a.last:
        ra, rb = rows[-2], rows[-1]
    else:
        by = {r["snapshot_id"]: r for r in rows}
        if a.frm not in by or a.to not in by:
            print(f"REFUSED: unknown snapshot id(s). known: "
                  f"{', '.join(sorted(by))}", file=sys.stderr)
            return 2
        ra, rb = by[a.frm], by[a.to]
    deep = None
    if a.deep:
        pa = a.report_a or ra.get("source")
        pb = a.report_b or rb.get("source")
        if not (pa and pb and Path(pa).exists() and Path(pb).exists()):
            print("REFUSED: --deep needs both report.json files "
                  "(--report-a / --report-b)", file=sys.stderr)
            return 2
        deep = deep_compare(pa, pb)
    v = classify(ra, rb, deep=deep,
                 units_a=load_units_file(ra), units_b=load_units_file(rb))
    if a.json:
        print(json.dumps({"from": ra["snapshot_id"], "to": rb["snapshot_id"],
                          **v}, indent=1))
    else:
        print_verdict(ra, rb, v)
    return 0


# ═══════════════════════════════════════════════════════════════════════════
# selftest -- it must be able to FAIL
# ═══════════════════════════════════════════════════════════════════════════

def _snap(sid, *, mf, mc, tc=10_245_956, tf=69_219, ruler="name_check",
          ver="4.2.8", bhash="9b2bb6f1f3a21062", maph="2336c7061a35c2bb",
          prov=True, kind="measured"):
    p = {"tool_version": ver, "tool_commit": "358c715835cc",
         "tool_binary_hash": bhash, "ruler": ruler,
         "diff_config_hash": "cfg" + ruler, "diff_config": None,
         "map_file": "m", "map_file_hash": maph, "map_file_entries": 5449,
         "present": prov}
    if not prov:
        p = {k: None for k in p}
        p["present"] = False
    return {"schema": SCHEMA, "snapshot_id": sid, "kind": kind,
            "commit": None, "commit_date": None, "recorded_at": "2026-01-01",
            "measures": {"matched_functions": mf, "matched_code": mc,
                         "total_code": tc, "total_functions": tf,
                         "matched_code_percent": 100.0 * mc / tc,
                         "masked_equal_functions": 0,
                         "honest_matched_functions": mf},
            "provenance": p, "strata": None, "units_file": None}


def _cases():
    """Each case: (name, A, B, required verdict, kwargs).

    Includes NEGATIVE controls -- cases that must NOT be called progress.
    A classifier that labels everything SOURCE_PROGRESS must fail here.
    """
    base = dict(mf=42_000, mc=3_700_000)
    return [
        ("regression: same instrument, same denominator, scores fell",
         _snap("A", **base), _snap("B", mf=41_500, mc=3_650_000),
         "REGRESSION", {}),
        ("progress: same instrument, same denominator, scores rose",
         _snap("A", **base), _snap("B", mf=42_400, mc=3_740_000),
         "SOURCE_PROGRESS", {}),
        ("ruler change: functionRelocDiffs flipped, denominator identical",
         _snap("A", **base), _snap("B", mf=42_000, mc=4_500_000,
                                   ruler="none"),
         "RULER_OR_TOOL_CHANGE", {}),
        ("tool change: objdiff version bump, denominator identical",
         _snap("A", **base), _snap("B", mf=39_800, mc=3_714_000,
                                   ver="4.2.9", bhash="deadbeefdeadbeef"),
         "RULER_OR_TOOL_CHANGE", {}),
        ("alias-map change: only map_file_hash moved (a ruler input worth ~7.9pp)",
         _snap("A", **base), _snap("B", mf=42_000, mc=3_500_000,
                                   maph="ffffffffffffffff"),
         "RULER_OR_TOOL_CHANGE", {}),
        ("denominator change: total_code moved, instrument identical",
         _snap("A", **base), _snap("B", mf=42_010, mc=3_702_000,
                                   tc=10_320_664),
         "DENOMINATOR_CHANGE", {}),
        ("mixed: instrument AND denominator both moved",
         _snap("A", **base), _snap("B", mf=40_000, mc=3_900_000,
                                   ver="4.2.9", tc=10_320_664),
         "MIXED", {}),
        ("no change: nothing moved",
         _snap("A", **base), _snap("B", **base), "NO_CHANGE", {}),
        ("NEGATIVE CONTROL: scores rose but A has no provenance -> not earned",
         _snap("A", prov=False, kind="reconstructed", **base),
         _snap("B", mf=42_400, mc=3_740_000),
         "INDETERMINATE", {}),
        ("opposite rulers: fns fell while bytes rose (the 44,514 event shape)",
         _snap("A", **base), _snap("B", mf=39_800, mc=3_714_000),
         "RULER_OR_TOOL_CHANGE", {}),
        ("name_check flip shape: matched_code -785,380 B, matched_functions "
         "+21 (real 2026-08-13 event, NO provenance recorded at the time)",
         _snap("A", mf=44_248, mc=4_340_756, tc=10_320_692, prov=False),
         _snap("B", mf=44_269, mc=3_555_376, tc=10_320_692, prov=False),
         "RULER_OR_TOOL_CHANGE", {}),
        ("NEGATIVE CONTROL: endpoint's matched_code is UNKNOWN (recorded 0) -- "
         "differencing against an absence must be refused, not scored",
         dict(_snap("A", mf=42_000, mc=0, prov=False, kind="reconstructed"),
              unknown_fields=["matched_code"]),
         _snap("B", mf=42_400, mc=3_740_000),
         "INDETERMINATE", {}),
    ]


def cmd_selftest(a):
    """Run the discrimination suite.  --prove-can-fail shows it is not vacuous
    by re-running against a deliberately broken classifier and REQUIRING that
    the suite goes red."""
    cases = _cases()
    print(f"=== SELFTEST: {len(cases)} cases, each REQUIRING a specific "
          f"verdict ===\n")
    fails = 0
    for name, A, B, want, kw in cases:
        got = classify(A, B, **kw)["verdict"]
        ok = got == want
        fails += (not ok)
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}\n"
              f"          want={want:<24} got={got}")
    print(f"\n{len(cases)-fails}/{len(cases)} cases classified correctly")

    if a.prove_can_fail:
        print("\n=== ANTI-VACUITY: re-running against a BROKEN classifier "
              "that always says SOURCE_PROGRESS ===")
        real = globals()["classify"]
        globals()["classify"] = lambda A, B, **k: {"verdict": "SOURCE_PROGRESS"}
        try:
            broken = sum(1 for n, A, B, want, kw in cases
                         if classify(A, B, **kw)["verdict"] != want)
        finally:
            globals()["classify"] = real
        print(f"  broken classifier fails {broken}/{len(cases)} cases")
        if broken == 0:
            print("  !! THE SUITE CANNOT FAIL -- it is vacuous. Exiting 1.")
            return 1
        print(f"  OK: the suite discriminates (a 'everything is progress' "
              f"classifier is rejected on {broken} cases)")

    if fails:
        print(f"\nSELFTEST FAILED: {fails} case(s) misclassified")
        return 1
    print("\nSELFTEST PASSED")
    return 0


# ═══════════════════════════════════════════════════════════════════════════

def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ledger", help="override ledger path (testing)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    r = sub.add_parser("record", help="append a snapshot")
    r.add_argument("--report", help="report.json (default: <project>/build/45410914/report.json)")
    r.add_argument("--project", help="project dir")
    r.add_argument("--commit", help="merge commit (rev-parsed); keys the row")
    r.add_argument("--id", help="explicit snapshot id (required if no --commit)")
    r.add_argument("--note", help="free text: what landed, what to know")
    r.add_argument("--source", help="provenance of the FIGURES (for backfill)")
    r.add_argument("--reconstructed", action="store_true",
                   help="mark as NOT a measurement (backfill from docs/README)")
    r.add_argument("--replace", action="store_true")
    r.add_argument("--no-units", action="store_true", help="skip the per-unit sidecar")
    r.add_argument("--dry-run", action="store_true")
    r.add_argument("--allow-unreconciled", action="store_true",
                   help="record even if the report fails self-validation (loud)")
    r.set_defaults(fn=cmd_record)

    b = sub.add_parser("backfill", help="record a RECONSTRUCTED row from a "
                                        "dated figure (no report.json)")
    b.add_argument("--id", required=True)
    b.add_argument("--date", help="ISO date of the figure")
    b.add_argument("--commit")
    b.add_argument("--source", help="REQUIRED: where the figure came from")
    b.add_argument("--note")
    b.add_argument("--matched-functions", type=int)
    b.add_argument("--matched-code", type=int)
    b.add_argument("--matched-code-percent", type=float)
    b.add_argument("--total-code", type=int)
    b.add_argument("--total-functions", type=int)
    b.add_argument("--masked-equal", type=int)
    b.add_argument("--ruler", help="asserted from prose; still reconstructed")
    b.add_argument("--tool-version")
    b.add_argument("--replace", action="store_true")
    b.add_argument("--dry-run", action="store_true")
    b.set_defaults(fn=cmd_backfill)

    l = sub.add_parser("list", help="print the series")
    l.set_defaults(fn=cmd_list)

    c = sub.add_parser("classify", help="classify the delta between two snapshots")
    c.add_argument("--from", dest="frm")
    c.add_argument("--to")
    c.add_argument("--last", action="store_true", help="the two most recent")
    c.add_argument("--deep", action="store_true",
                   help="add the per-function fuzzy-identity test (decisive)")
    c.add_argument("--report-a")
    c.add_argument("--report-b")
    c.add_argument("--json", action="store_true")
    c.set_defaults(fn=cmd_classify)

    s = sub.add_parser("selftest", help="require correct classification of "
                                        "synthetic events")
    s.add_argument("--prove-can-fail", action="store_true",
                   help="also require the suite to REJECT a broken classifier")
    s.set_defaults(fn=cmd_selftest)

    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
