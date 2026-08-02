#!/usr/bin/env python3
"""Regenerate the reachable-ceiling census — WHY each pinned unit cannot reach 100%.

WHAT THIS REPLACES AND WHY
--------------------------
Lane DF-4 (`5f7bf5b4`) produced a static `reachable_ceiling_census.json` over
1,024 pairable units.  It was immediately load-bearing for targeting, and it was
ALSO wrong within one wave, in two independent ways:

  * IT AGED INSTANTLY.  Two of DF-4's own 23 boundary-move candidates had already
    been landed by DF-4's own commit; three of its 65 "one map row away" rows
    were already at 100%.  A census is a photograph of a tree that keeps moving,
    and THAT PERISHABILITY IS THE WHOLE REASON THIS IS A TOOL WITH A STALENESS
    REFUSAL RATHER THAN A CHECKED-IN JSON ARTIFACT.
  * ITS HEADLINE BUCKET WAS MISLABELLED.  Lane DG-1 (`bce10a25`) worked the 65
    and shipped 3 (4.6% — 21x optimistic).  Lane DG-2 (`1cbcabc8`) worked the 23
    and shipped 8 (~35%, ~3x optimistic).  Critically, DG-1 found that **31 of
    the 65 have the method IN OUR SOURCE with a diverging body** — so the bucket
    DF-4 named MAP_ONLY, glossed "no source lane can EVER finish these", was
    telling source lanes to skip their most valuable follow-on.

So this tool RECOMPUTES rather than caches, REFUSES on a stale or collapsed
input rather than emitting a confident-looking zero, and ships the audit
corrections baked in rather than re-shipping the refuted semantics.

RULERS — SAY WHICH ONE YOU MEAN (this project has three percent fields)
-----------------------------------------------------------------------
  * Every FUNCTION COUNT and every BUCKET here uses `match_percent_normalized`
    (mpn).  "At 100" == every row mpn == 100.0.  This is the ruler
    `matched_functions` is computed on.
  * `fuzzy_match_percent` gates BYTES (`matched_code`), not function counts.
    The two disagree at unit scale: on the tree this docstring was written
    against, 176 units are at-100 by mpn and 162 by all-rows-fuzzy.  Both
    numbers are reported (`at_100_mpn_ruler` / `at_100_all_fuzzy_ruler`) and
    they are NEVER conflated.  mpn excludes arg-only penalties, so it is
    structurally blind to wrong callees; do not read at-100 as "correct".
  * A per-item `fuzzy_match_percent` is omitted from the JSON when it is 0.0
    (proto3 float), so `.get('fuzzy_match_percent', 0.0)` is the CORRECT read.

THE ANON GATE (ground truth, re-derived by DF-4 and re-verified every run)
--------------------------------------------------------------------------
A target row still named `fn_<8HEX>` is precisely "a retail address absent from
`scripts/target_symbol_map.json`".  objdiff's fork pairs a target `fn_<8hex>`
only with a base symbol satisfying `is_funclet_like`; an ordinary retail method
compiles on our side to a MANGLED name, which is not funclet-like, so no pair
can ever form.  This tool re-checks the identity every run and prints the
result; when it holds with zero exceptions the map check is a CONSISTENCY
VERIFICATION, not an independent classifier, and this tool says so rather than
implying the map lookup added discriminating power it did not add.

Anon rows classify THREE ways, not two (a DF-4 prediction that failed):
    mpn == 0        UNPAIRED  -> needs a map row (but see the mutability note)
    0 < mpn < 100   PAIRED    -> byte-signature pairing formed; SOURCE-reachable
    mpn == 100      MASKED    -> counts as matched; == `masked_equal_functions`

** THE ANON SPLIT IS MUTABLE, SO THE CEILING IS A FLOOR OF KNOWLEDGE. **  Pass 3
pairs on identical size AND >=50% masked byte equality, so a source change that
alters FUNCLET BYTES can move a row across that threshold -- in either
direction.  Lane DG-3 (`362217af`) is the worked case: an RndFont layout port
took default/Font 64/109 -> 72/109 against a briefed reachable ceiling of 70,
with anon@100 going 32 -> 37, while BitmapLocker::ctor went the other way,
32.6 -> 0.  Scope this honestly: the briefed 70 counted ONLY named sub-100 rows
as reachable, and THIS tool already counts the PAIRED anon rows as
source-reachable too (Font: 64 matched + 6 named + 11 paired-anon => 81), so
DG-3's 72 lands inside this tool's ceiling, not outside it.  The residual
un-modelled mutability is the UNPAIRED pool crossing into pairing.  No attempt
is made to predict which rows will cross: that would need a masked-identity
sweep over all unpaired anon rows plus a validated null, and this project has
twice been burned by a weak enrichment used as a deterministic classifier
(pin-tail 1.25x, name_check fold-alias 1.95x).  An unvalidated proximity flag
would be a third.

EXIT CODES (a refusal is a distinct code, never a zero-filled report)
----------------------------------------------------------------------
    0   census produced
    2   CONSISTENCY CONTROL FAILED (independent recount disagrees)
    3   STALE INPUT refused (report.json older than the tree it describes)
    4   COLLAPSED JOIN / unusable input refused (the CY-1 lesson: a tool that
        reports an empty tree as done is shaped exactly like a decisive negative)
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import importlib.util
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

TOOL_VERSION = "1.0.0"

# Documented /Od region (lane CF-4).  NOTE: /Od is OBJECT-granular, not
# address-range — a pin outside this window can still be /Od (keygen_xbox is an
# island inside HMX code).  Overlap here is a POSITIVE signal only.
OD_LO, OD_HI = 0x82A6D168, 0x82B54190

ANON_RX = re.compile(r"^fn_([0-9A-Fa-f]{8})$")

# Paths whose modification invalidates a report.json.
STALENESS_PATHS = ["src", "config", "scripts/target_symbol_map.json"]

# Collapsed-join floor.  On a healthy tree 99.2% of NAMED target rows carry a
# name that appears in the map's values (the renamer put it there).  A
# truncated / empty / wrong-schema map collapses this to ~0.  The floor sits far
# below the healthy value and far above a collapsed one.
MIN_NAMED_ROWS_EXPLAINED_BY_MAP = 0.50

BUCKET_ORDER = ["AT_100", "COMPLETABLE", "ANON_BLOCKED", "MIXED", "OD_REGION"]

LEGEND = {
    "AT_100": "every row mpn == 100 (mpn ruler; NOT a claim of byte-correctness)",
    "COMPLETABLE": (
        "sub-100 rows exist but NONE is an unpaired-anon row => pure source work "
        "reaches 100%.  ** Reachable IN PRINCIPLE ONLY: this says nothing about "
        "whether the pin or the map row is CORRECT. **"
    ),
    "ANON_BLOCKED": (
        "leftover rows are ANON (unpaired fn_<8hex>) -- NOT the same as "
        "unreachable-by-source.  DG-1 proved 31 of 65 such units DO have the "
        "method in our source with a diverging body.  Renamed from DF-4's "
        "MAP_ONLY, whose gloss 'no source lane can EVER finish these' is FALSE. "
        "The anon count is also NOT conserved: a source fix that alters funclet "
        "bytes can move rows across the pairing threshold either way (DG-3)."
    ),
    "MIXED": "needs BOTH source work and map rows",
    "OD_REGION": f"pin overlaps {OD_LO:#x}-{OD_HI:#x} (compiled /Od)",
}

SUB_BUCKET_LEGEND = {
    "MAP_FIXABLE_CANDIDATE": (
        "a free same-class symbol of the SAME SIZE exists in our compiled obj and "
        "the target body passes the anti-vacuity guard => a map row could plausibly "
        "complete this unit.  Still needs adjudication (RTTI/xref), never the metric."
    ),
    "MAP_FIXABLE_UNADJUDICABLE": (
        "same-class same-size candidate exists but the target body FAILS the "
        "anti-vacuity guard (<4 real non-relocated words or <50% of the body). "
        "DG-1 retired all 6 of these: no future evidence at that body size can "
        "adjudicate them.  DO NOT RE-FUND."
    ),
    "SAMECLASS_DIFFSIZE": (
        "** SOURCE WORK, NOT A MAP ROW. **  Our obj HAS a symbol of the unit's own "
        "class but at a different size => the body diverges.  This is the 31/65 "
        "class DF-4 mislabelled as unreachable-by-source."
    ),
    "NO_SAMECLASS": "no free symbol of the unit's own class => genuine source gap",
    "NO_CLASS_ANCHOR": "no mapped symbol yields a class name; nothing to anchor on",
    "AUTO_03_NO_OBJ": (
        "auto-generated split with no source and no compiled obj -- unfixable by "
        "any map row, ever"
    ),
    "TARGET_FN_NOT_IN_ASM": "blocker VA absent from the dtk target asm (pin/asm drift)",
    "UNCLASSIFIED_TOOLING_ERROR": "sub-classifier raised; see .error",
}

HEADER_CAVEATS = [
    "THIS OUTPUT IS STALE THE MOMENT ANYTHING LANDS -- REGENERATE, DO NOT CACHE. "
    "DF-4's static JSON predated its own commit and 2 of its 23 candidates were "
    "already fixed by the lane that produced the list.",
    "COMPLETABLE DOES NOT MEAN THE COMPLETION PAYS HONEST.  Driving an "
    "anon-PAIRED blocker row to mpn 100 makes it anon@100 -- and anon@100 IS "
    "masked_equal exactly (22,686 == 22,686 measured on this tree).  So for the "
    "11 COMPLETABLE units whose blockers are anon_paired (26 rows), finishing "
    "the unit pays matched_functions into the MASKED stratum, not into honest "
    "(matched - masked_equal).  The bucket assignment is correct; what is easy "
    "to over-read is the VALUE of finishing such a unit.  Check blocker_rows' "
    "kind= before pricing a unit-completion lane.",
    "THE CEILING IS A FLOOR OF KNOWLEDGE, NOT A HARD CAP -- A SOURCE FIX CAN BEAT "
    "IT.  Lane DG-3 (362217af) ported RndFont to the rb3-Wii retail layout and took "
    "default/Font to 72/109 against a briefed reachable ceiling of 70, because the "
    "layout fix changed FUNCLET BYTES enough that the fork's byte-signature pairing "
    "picked up 5 more anon rows (anon@100 32 -> 37).  Anon residue is NOT static, "
    "and it moves BOTH WAYS: the same commit drove BitmapLocker::ctor 32.6 -> 0. "
    "So treat ANON_BLOCKED/MIXED anon counts as a snapshot of what is currently "
    "paired, never as a conserved quantity.",
    "THE CENSUS CANNOT DETECT A BOGUS PIN.  HamDriver / FilterQueue / SkeletonDir "
    "sit in COMPLETABLE and were already rejected on evidence as bogus-pin / "
    "metric-fitting cases.",
    "COMPLETABLE means reachable IN PRINCIPLE.  It does not mean the pin is right, "
    "the map row is right, or that finishing the unit is worth the budget.",
    "ANON_BLOCKED means 'the leftover rows are anon'.  It does NOT mean "
    "'unreachable by source' -- see the sub_bucket, where SAMECLASS_DIFFSIZE is "
    "source work.",
    "Bucket counts use the mpn ruler.  mpn excludes arg-only penalties and is "
    "structurally blind to wrong callees; 'at 100' is not 'correct'.",
    "A unit flagged unit_vanishes_if_drained CANNOT be completed by a boundary "
    "move: the move drains its only function and the unit must be DELETED.",
]


# ---------------------------------------------------------------------------
# refusals
# ---------------------------------------------------------------------------
class Refusal(Exception):
    def __init__(self, code: int, title: str, lines):
        super().__init__(title)
        self.code = code
        self.title = title
        self.lines = lines if isinstance(lines, (list, tuple)) else [lines]


def _banner(title, lines, char="!"):
    w = 78
    out = [char * w, f"{char*2} {title}", char * w]
    for ln in lines:
        out.append(f"{char*2} {ln}")
    out.append(char * w)
    return "\n".join(out)


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def git(root: Path, *args) -> str:
    return subprocess.run(["git", "-C", str(root), *args],
                          capture_output=True, text=True).stdout.strip()


# ---------------------------------------------------------------------------
# staleness
# ---------------------------------------------------------------------------
def staleness_check(root: Path, report: Path, allow_stale: bool):
    """report.json must be NEWER than the tree it purports to describe."""
    rep_mtime = report.stat().st_mtime
    facts = {
        "report_mtime_epoch": int(rep_mtime),
        "report_mtime_iso": time.strftime("%Y-%m-%dT%H:%M:%S%z",
                                          time.localtime(rep_mtime)),
    }
    reasons = []

    # (0) no git => no provenance => the whole staleness gate is VACUOUS.  Say so
    # rather than passing a check that never ran.
    if not git(root, "rev-parse", "HEAD"):
        if not allow_stale:
            raise Refusal(3, "REFUSING: CANNOT ESTABLISH PROVENANCE", [
                f"{root} is not a git repository, so the staleness gate cannot run.",
                "A gate that cannot fail is not a gate.  Use --allow-stale to "
                "proceed with an explicitly unverifiable report.",
            ])
        facts["provenance_unavailable"] = True

    # (a) newest COMMIT touching the inputs that change the metric
    out = git(root, "log", "-1", "--format=%H %ct %ci %s", "--", *STALENESS_PATHS)
    if out:
        h, ct, rest = out.split(" ", 2)
        facts["newest_relevant_commit"] = h
        facts["newest_relevant_commit_epoch"] = int(ct)
        facts["newest_relevant_commit_desc"] = rest
        if int(ct) > rep_mtime:
            reasons.append(
                f"report.json ({facts['report_mtime_iso']}) predates commit "
                f"{h[:8]} ({rest.split(' ',2)[0]} {rest.split(' ',2)[1]}) which "
                f"touched {'/'.join(STALENESS_PATHS)}")

    # (b) UNCOMMITTED edits are exactly as stale-making as a commit
    dirty = []
    porc = git(root, "status", "--porcelain", "--", *STALENESS_PATHS)
    for line in porc.splitlines():
        p = line[3:].strip().split(" -> ")[-1].strip('"')
        fp = root / p
        try:
            if fp.is_file() and fp.stat().st_mtime > rep_mtime:
                dirty.append(p)
        except OSError:
            pass
    facts["dirty_inputs_newer_than_report"] = sorted(dirty)[:20]
    facts["dirty_inputs_newer_count"] = len(dirty)
    if dirty:
        reasons.append(
            f"{len(dirty)} uncommitted file(s) under {'/'.join(STALENESS_PATHS)} "
            f"are newer than report.json (e.g. {', '.join(sorted(dirty)[:3])})")

    facts["stale"] = bool(reasons)
    facts["stale_reasons"] = reasons
    facts["stale_override_used"] = bool(reasons) and allow_stale

    if reasons and not allow_stale:
        raise Refusal(3, "REFUSING: report.json IS STALE", reasons + [
            "",
            "Staleness was DF-4's failure mode: its static census was already wrong "
            "about its own tree on the day it shipped.",
            "Fix:  rm build/45410914/report.json build/45410914/report.cache && "
            "./tools/ninja-locked",
            "Override (you will get a banner on every artifact):  --allow-stale",
        ])
    if reasons:
        print(_banner("STALE INPUT ACCEPTED VIA --allow-stale -- NUMBERS MAY BE WRONG",
                      reasons), file=sys.stderr)
    return facts


# ---------------------------------------------------------------------------
# inputs
# ---------------------------------------------------------------------------
def load_map(path: Path):
    """address->mangled-name map, with a schema gate."""
    try:
        raw = json.loads(path.read_text())
    except Exception as e:
        raise Refusal(4, "REFUSING: target_symbol_map.json IS NOT PARSEABLE",
                      [f"{path}: {type(e).__name__}: {e}"])
    if not isinstance(raw, dict):
        raise Refusal(4, "REFUSING: target_symbol_map.json IS NOT AN OBJECT",
                      [f"{path}: top level is {type(raw).__name__}"])
    addr2name = {}
    for k, v in raw.items():
        if isinstance(k, str) and k.startswith("0x") and isinstance(v, str):
            try:
                addr2name[int(k, 16)] = v
            except ValueError:
                pass
    if not addr2name:
        raise Refusal(4, "REFUSING: target_symbol_map.json HAS ZERO ADDRESS ROWS", [
            f"{path}: parsed {len(raw)} top-level keys, 0 of them '0x...' -> str.",
            "An empty map would silently reclassify EVERY named row as anon and "
            "report a tree with no reachable work as a finished census.",
        ])
    return addr2name, set(addr2name.values()), len(raw)


def parse_splits(path: Path):
    """heading -> ordered .text blocks.  Headings are a MIX of bare basenames
    and full relative paths; callers must key on the stem, not the raw heading."""
    headings, cur = {}, None
    for line in path.read_text().splitlines():
        if not line.strip():
            continue
        if not line[0].isspace():
            h = line.strip().rstrip(":")
            cur = None if h == "Sections" else h
            if cur is not None:
                headings.setdefault(cur, [])
            continue
        if cur is None:
            continue
        parts = line.split()
        if parts and parts[0] == ".text":
            kv = dict(p.split(":", 1) for p in parts[1:] if ":" in p)
            if "start" in kv and "end" in kv:
                headings[cur].append((int(kv["start"], 16), int(kv["end"], 16)))
    if not headings:
        raise Refusal(4, "REFUSING: splits.txt YIELDED ZERO HEADINGS", [
            f"{path}: no unit headings parsed -- /Od and pin-extent fields would "
            "all be silently null.",
        ])
    return headings


# ---------------------------------------------------------------------------
# census (PATH A -- the bucketer)
# ---------------------------------------------------------------------------
def build_census(report_doc, addr2name, mapvals, splits):
    units = [u for u in report_doc["units"] if u.get("functions")]
    if not units:
        raise Refusal(4, "REFUSING: report.json HAS ZERO PAIRABLE UNITS",
                      ["No unit carries a 'functions' array -- nothing to census."])

    stem2heading = collections.defaultdict(list)
    for h in splits:
        stem2heading[os.path.splitext(os.path.basename(h))[0]].append(h)

    map_addrs = set(addr2name)
    anon_total = anon_in_map = named_total = named_explained = 0
    records = []

    for u in units:
        heads = stem2heading.get(os.path.basename(u["name"]), [])
        blocks = sorted(b for h in heads for b in splits[h])

        n_anon0 = n_anonmid = n_anon100 = n_named_sub = n_named100 = 0
        n_anon_in_map = 0
        blockers = []

        for f in u["functions"]:
            nm, p = f["name"], f["match_percent_normalized"]
            m = ANON_RX.match(nm)
            if m:
                anon_total += 1
                va = int(m.group(1), 16)          # VA is embedded in the NAME.
                in_map = va in map_addrs          # (unit-relative 'address' is
                if in_map:                        #  NOT pin_start+offset -- one
                    anon_in_map += 1              #  of DF-4's self-caught bugs.)
                    n_anon_in_map += 1
                if p == 0.0:
                    n_anon0 += 1
                    blockers.append(_row(f, va, "anon_unpaired", in_map))
                elif p == 100.0:
                    n_anon100 += 1
                else:
                    n_anonmid += 1
                    blockers.append(_row(f, va, "anon_paired", in_map))
            else:
                named_total += 1
                if nm in mapvals:
                    named_explained += 1
                if p != 100.0:
                    n_named_sub += 1
                    # NOTE: no VA is emitted for named rows.  The unit-relative
                    # 'address' is not pin_start+offset, and DF-4's va_of() walk
                    # was one of its three self-caught instrument bugs.  Emitting
                    # nothing beats emitting a wrong address.
                    blockers.append(_row(f, None, "named", None))
                else:
                    n_named100 += 1

        sub = n_anon0 + n_anonmid + n_named_sub
        reachable = n_anonmid + n_named_sub
        od = bool(blocks) and any(not (e <= OD_LO or s >= OD_HI) for s, e in blocks)

        if sub == 0:
            bucket = "AT_100"
        elif od:
            bucket = "OD_REGION"
        elif n_anon0 == 0:
            bucket = "COMPLETABLE"
        elif reachable == 0:
            bucket = "ANON_BLOCKED"
        else:
            bucket = "MIXED"

        tf = int(u["measures"].get("total_functions", 0))
        mf = int(u["measures"].get("matched_functions", 0))

        records.append({
            "unit": u["name"],
            "source_path": u["metadata"].get("source_path"),
            "auto_generated": bool(u["metadata"].get("auto_generated")),
            "bucket": bucket,
            "headings": heads,
            "n_text_blocks": len(blocks),
            "text_lo": hex(blocks[0][0]) if blocks else None,
            "text_hi": hex(blocks[-1][1]) if blocks else None,
            "in_od_region": od,
            "total_functions": tf,
            "matched_functions": mf,
            "total_code": int(u["measures"].get("total_code", 0)),
            "masked_equal": int(u["measures"].get("masked_equal_functions", 0)),
            # ---- blocker detail: named vs anon, and the anon map-absence check
            "rows": {
                "named_at_100": n_named100,
                "named_sub100": n_named_sub,
                "anon_at_100_masked": n_anon100,
                "anon_paired_sub100": n_anonmid,
                "anon_unpaired": n_anon0,
                "anon_rows_present_in_map": n_anon_in_map,
            },
            "sub100_total": sub,
            "sub100_named": n_named_sub,
            "sub100_anon": n_anon0 + n_anonmid,
            "reachable_blockers": reachable,
            # DG-2's structurally-impossible class: the boundary move DRAINS the
            # unit, so it must be deleted -- it VANISHES, it does not complete.
            "unit_vanishes_if_drained": (tf == 1 and mf == 0),
            "blocker_rows": sorted(blockers, key=lambda r: (-r["mpn"], r["name"]))[:60],
            "blocker_rows_truncated": max(0, len(blockers) - 60),
            "sub_bucket": None,
            "sub_bucket_detail": None,
        })

    join = {
        "anon_rows_total": anon_total,
        "anon_rows_present_in_map": anon_in_map,
        "anon_gate_holds": anon_in_map == 0,
        "named_rows_total": named_total,
        "named_rows_explained_by_map": named_explained,
        "named_rows_explained_pct": (100.0 * named_explained / named_total) if named_total else 0.0,
    }
    return records, join


def _row(f, va, kind, in_map):
    return {
        "name": f["name"],
        "kind": kind,
        "size": int(f["size"]),
        "mpn": f["match_percent_normalized"],
        "fuzzy": f.get("fuzzy_match_percent", 0.0),   # omitted iff 0.0 (proto3)
        "va": (f"0x{va:08x}" if va is not None else None),
        "va_absent_from_map": (None if in_map is None else (not in_map)),
    }


def collapsed_join_gate(join):
    """CY-1: a tool that reports an empty tree as done is shaped exactly like a
    decisive negative.  Refuse rather than emit a confident zero."""
    if join["named_rows_total"] == 0:
        raise Refusal(4, "REFUSING: NOT ONE NAMED TARGET ROW IN THE WHOLE REPORT", [
            "Every target row is anon.  Either the obj_target_symbol_renamer never "
            "ran (worktree renamer race -> silently -4,731 matched) or the map is "
            "empty.  Every unit would read ANON_BLOCKED and the census would be "
            "a confident lie.",
        ])
    pct = join["named_rows_explained_pct"]
    if pct < MIN_NAMED_ROWS_EXPLAINED_BY_MAP * 100.0:
        raise Refusal(4, "REFUSING: COLLAPSED JOIN report.json <-> target_symbol_map.json", [
            f"Only {join['named_rows_explained_by_map']}/{join['named_rows_total']} "
            f"({pct:.2f}%) of NAMED target rows carry a name present in the map's "
            f"values; the floor is {MIN_NAMED_ROWS_EXPLAINED_BY_MAP*100:.0f}% and a "
            f"healthy tree reads ~99%.",
            "The map and the report describe different trees (truncated map, wrong "
            "path, or a report generated before the map landed).",
        ])


# ---------------------------------------------------------------------------
# CONSISTENCY CONTROL (PATH B -- deliberately shares NO code with path A)
# ---------------------------------------------------------------------------
def independent_recount(report_path: Path):
    """Re-read the report FROM DISK and recount, sharing nothing with the
    bucketer: no ANON_RX, no unit records, no helpers.  If this disagrees, the
    bucketer is wrong and the run must fail."""
    doc = json.loads(Path(report_path).read_text())
    at100_mpn = 0
    at100_fuzzy = 0
    pairable = 0
    rows = 0
    anon_at_100 = 0
    for unit in doc.get("units", []):
        fns = unit.get("functions")
        if not fns:
            continue
        pairable += 1
        rows += len(fns)
        every_mpn = True
        every_fuzzy = True
        for fn in fns:
            if fn["match_percent_normalized"] != 100.0:
                every_mpn = False
            if fn.get("fuzzy_match_percent", 0.0) != 100.0:
                every_fuzzy = False
            nm = fn["name"]
            if (len(nm) == 11 and nm.startswith("fn_")
                    and all(c in "0123456789abcdefABCDEF" for c in nm[3:])
                    and fn["match_percent_normalized"] == 100.0):
                anon_at_100 += 1
        if every_mpn:
            at100_mpn += 1
        if every_fuzzy:
            at100_fuzzy += 1
    return {"pairable_units": pairable, "total_rows": rows,
            "at_100_mpn_ruler": at100_mpn, "at_100_all_fuzzy_ruler": at100_fuzzy,
            "anon_at_100": anon_at_100}


def run_consistency_control(records, join, report_path, report_doc, strict=True):
    ind = independent_recount(report_path)
    a_at100 = sum(1 for r in records if r["bucket"] == "AT_100")
    a_pairable = len(records)
    a_rows = sum(r["total_functions"] for r in records)
    a_anon100 = sum(r["rows"]["anon_at_100_masked"] for r in records)

    checks = []

    def chk(name, mine, theirs, fatal, note=""):
        ok = (mine == theirs)
        checks.append({"check": name, "bucketer": mine, "independent": theirs,
                       "ok": ok, "fatal": fatal, "note": note})
        return ok

    chk("AT_100 unit count (mpn ruler)", a_at100, ind["at_100_mpn_ruler"], True)
    chk("pairable unit count", a_pairable, ind["pairable_units"], True)
    chk("total function rows", a_rows, ind["total_rows"], True,
        "unit measures.total_functions vs len(functions)")
    chk("anon rows at mpn==100", a_anon100, ind["anon_at_100"], True)

    # NOT fatal: an identity that has held exactly twice but whose right-hand
    # side is an objdiff disclosure key -- the 2026-08-02 ruler flip moved it
    # 1,096 -> 22,640 once already.  Report loudly; do not gate the tool on it.
    me = int(report_doc["measures"]["masked_equal_functions"])
    chk("anon@100 == masked_equal_functions", a_anon100, me, False,
        "disclosure identity; ruler-flip sensitive, WARN only")

    # The two rulers must NOT be conflated -- record the gap explicitly.
    two_rulers = {
        "at_100_mpn_ruler": ind["at_100_mpn_ruler"],
        "at_100_all_fuzzy_ruler": ind["at_100_all_fuzzy_ruler"],
        "units_counted_by_mpn_but_withholding_bytes":
            ind["at_100_mpn_ruler"] - ind["at_100_all_fuzzy_ruler"],
    }

    fatal_fail = [c for c in checks if c["fatal"] and not c["ok"]]
    warn_fail = [c for c in checks if not c["fatal"] and not c["ok"]]
    for c in warn_fail:
        print(_banner("CONSISTENCY WARNING (non-fatal)",
                      [f"{c['check']}: bucketer={c['bucketer']} "
                       f"independent={c['independent']}", c["note"]], "~"),
              file=sys.stderr)
    if fatal_fail and strict:
        raise Refusal(2, "CONSISTENCY CONTROL FAILED -- THE CENSUS IS NOT TRUSTWORTHY",
                      [f"{c['check']}: bucketer={c['bucketer']} != "
                       f"independent={c['independent']}" for c in fatal_fail])
    return {"checks": checks, "two_rulers": two_rulers, "independent": ind}


# ---------------------------------------------------------------------------
# SUB-CLASSIFICATION of one-away ANON_BLOCKED units  (DG-1's partition)
# ---------------------------------------------------------------------------
_CLS_CTOR = re.compile(r"\?\?[0-9_][A-Z]?([A-Za-z_]\w*)@@")
_CLS_METH = re.compile(r"\?[A-Za-z_]\w*@([A-Za-z_]\w*)@@")


def cls_of(name):
    m = _CLS_CTOR.match(name) or _CLS_METH.match(name)
    return m.group(1) if m else None


def _load_soa(root: Path):
    p = root / "scripts/harvest/size_order_automap.py"
    if not p.exists():
        return None, f"missing {p}"
    try:
        spec = importlib.util.spec_from_file_location("soa_dh4", p)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        mod.PROJECT_ROOT = root
        mod.BUILD = root / "build/45410914"
        return mod, None
    except Exception as e:
        return None, f"{type(e).__name__}: {e}"


def anti_vacuity(masked: bytes):
    """>=4 real (non-relocated) words AND >=50% of the body.  Any masked
    comparator without this guard confirms whatever you point it at."""
    n = len(masked) // 4
    zero = sum(1 for i in range(n) if masked[i * 4:(i + 1) * 4] == b"\0\0\0\0")
    real = n - zero
    return {"n_words": n, "n_masked_words": zero, "n_real_words": real,
            "guard_ok": bool(n and real >= 4 and real / n >= 0.5)}


def subclassify(records, root: Path, addr2name, limit=None):
    """One-away ANON_BLOCKED units only -- the DG-1 population."""
    targets = [r for r in records
               if r["bucket"] == "ANON_BLOCKED" and r["sub100_total"] == 1]
    if limit:
        targets = targets[:limit]
    if not targets:
        return {"attempted": 0, "note": "no one-away ANON_BLOCKED units"}

    soa, err = _load_soa(root)
    if soa is None:
        # Refuse rather than emit a census whose ANON_BLOCKED units all read
        # "UNCLASSIFIED".  Silence there is indistinguishable from "nothing to
        # sub-classify", which is exactly the shape of a decisive negative.
        raise Refusal(4, "REFUSING: SUB-CLASSIFIER COULD NOT BE LOADED", [
            err,
            f"{len(targets)} one-away ANON_BLOCKED units would be emitted "
            "UNCLASSIFIED, which reads identically to 'no source work here'.",
            "Pass --no-subclass if you genuinely want the bucket-only census.",
        ])

    for r in targets:
        det = {}
        try:
            va = int(r["blocker_rows"][0]["va"], 16)
            det["va"] = f"0x{va:08x}"
            if r["auto_generated"]:
                r["sub_bucket"] = "AUTO_03_NO_OBJ"
                det["reason"] = "metadata.auto_generated -- no source, no compiled obj"
                r["sub_bucket_detail"] = det
                continue
            _n, tobj, tasm, cobj = soa.resolve_unit(r["unit"])
            det["compiled_obj"] = str(cobj) if cobj.exists() else None
            if not cobj.exists():
                r["sub_bucket"] = "AUTO_03_NO_OBJ"
                det["reason"] = "no compiled obj -- no map row can ever fix it"
                r["sub_bucket_detail"] = det
                continue
            T = soa.load_target(tobj, tasm)
            tf = next((t for t in T if t.va == va), None)
            if tf is None:
                r["sub_bucket"] = "TARGET_FN_NOT_IN_ASM"
                r["sub_bucket_detail"] = det
                continue
            det["target_size"] = tf.size
            det.update(anti_vacuity(tf.masked))

            mapped = {addr2name[t.va] for t in T if t.va in addr2name}
            mapped_strip = {soa.anon_ns_strip(m) for m in mapped}
            unit_classes = {c for c in (cls_of(m) for m in mapped) if c}
            det["n_unit_classes"] = len(unit_classes)

            C = soa.load_compiled(cobj)
            free = [c for c in C
                    if soa.anon_ns_strip(c.name) not in mapped_strip
                    and not soa.is_internal(c.name)]
            det["n_compiled"] = len(C)
            det["n_free"] = len(free)
            same_cls = [c for c in free if cls_of(c.name) in unit_classes]
            same_sz = [c for c in same_cls if c.size == tf.size]
            det["n_free_sameclass"] = len(same_cls)
            det["n_free_sameclass_samesize"] = len(same_sz)
            det["sameclass_nearest"] = sorted(
                ([c.size, c.name] for c in same_cls),
                key=lambda z: abs(z[0] - tf.size))[:5]
            det["exact_masked_match"] = sorted(
                c.name for c in same_sz if c.masked == tf.masked)[:5]

            if not unit_classes:
                r["sub_bucket"] = "NO_CLASS_ANCHOR"
            elif same_sz:
                r["sub_bucket"] = ("MAP_FIXABLE_CANDIDATE" if det["guard_ok"]
                                   else "MAP_FIXABLE_UNADJUDICABLE")
            elif same_cls:
                r["sub_bucket"] = "SAMECLASS_DIFFSIZE"      # SOURCE work
            else:
                r["sub_bucket"] = "NO_SAMECLASS"
        except Exception as e:
            r["sub_bucket"] = "UNCLASSIFIED_TOOLING_ERROR"
            det["error"] = f"{type(e).__name__}: {e}"
        r["sub_bucket_detail"] = det

    errs = sum(1 for r in targets if r["sub_bucket"] == "UNCLASSIFIED_TOOLING_ERROR")
    if targets and errs == len(targets):
        raise Refusal(4, "REFUSING: SUB-CLASSIFIER FAILED ON EVERY UNIT", [
            f"all {len(targets)} one-away ANON_BLOCKED units errored -- a collapsed "
            "pipeline, not a finding.",
            "sample: " + str(targets[0]["sub_bucket_detail"].get("error"))[:200],
        ])
    return {"attempted": len(targets), "loaded": True, "errors": errs,
            "buckets": dict(collections.Counter(r["sub_bucket"] for r in targets))}


# ---------------------------------------------------------------------------
# reporting
# ---------------------------------------------------------------------------
def emit_text(payload, out=sys.stdout):
    p = payload
    s = p["summary"]
    w = out.write
    w("\n" + "=" * 78 + "\n")
    w("REACHABLE-CEILING CENSUS  (regenerated, not cached)\n")
    w("=" * 78 + "\n")
    st = p["provenance"]
    w(f"HEAD            {st['head']}{'  [DIRTY TREE]' if st['head_dirty'] else ''}\n")
    w(f"report.json     {st['report_path']}\n")
    w(f"                sha256 {st['report_sha256'][:16]}...  mtime {st['report_mtime_iso']}\n")
    w(f"map / splits    {st['map_sha256'][:16]}... / {st['splits_sha256'][:16]}...\n")
    if st["staleness"].get("stale"):
        w("STALENESS       *** STALE, ACCEPTED VIA --allow-stale ***\n")
    w("\n-- CAVEATS (carried into every artifact) " + "-" * 37 + "\n")
    for c in p["caveats"]:
        w(f"  ! {c}\n")

    w("\n-- BUCKETS (mpn ruler: 'at 100' == every row match_percent_normalized==100)\n")
    tot = s["pairable_units"]
    for b in BUCKET_ORDER:
        n = s["buckets"].get(b, 0)
        w(f"  {b:14s} {n:5d}  ({100.0*n/tot:5.1f}%)  {LEGEND[b][:74]}\n")
    w(f"  {'TOTAL':14s} {tot:5d}\n")
    w(f"\n  SOURCE-ONLY CEILING on units-at-100 = AT_100 + COMPLETABLE = "
      f"{s['source_only_unit_ceiling']} / {tot} ({100.0*s['source_only_unit_ceiling']/tot:.1f}%)\n")

    tr = p["consistency"]["two_rulers"]
    w(f"\n-- TWO RULERS (never conflate) --\n")
    w(f"  units at 100 by mpn (function ruler) : {tr['at_100_mpn_ruler']}\n")
    w(f"  units at 100 by all-rows fuzzy (byte): {tr['at_100_all_fuzzy_ruler']}\n")
    w(f"  counted complete but withholding bytes: "
      f"{tr['units_counted_by_mpn_but_withholding_bytes']}\n")

    j = p["anon_gate"]
    w(f"\n-- ANON GATE VERIFICATION --\n")
    w(f"  anon rows: {j['anon_rows_total']}, of which present in target_symbol_map: "
      f"{j['anon_rows_present_in_map']}\n")
    if j["anon_gate_holds"]:
        w("  => identity HOLDS with zero exceptions: 'anon row' == 'retail address\n"
          "     absent from the map'.  Therefore the map lookup is a CONSISTENCY\n"
          "     CHECK here, NOT an independent classifier -- the discriminating\n"
          "     split is the three-way on anon mpn below.\n")
    else:
        w("  => *** identity BROKEN *** some anon rows ARE in the map: the renamer\n"
          "     did not patch them.  Treat the anon buckets with suspicion.\n")
    w(f"  named rows explained by map: {j['named_rows_explained_by_map']}/"
      f"{j['named_rows_total']} ({j['named_rows_explained_pct']:.2f}%)\n")
    a = s["anon_rows"]
    w(f"  anon three-way: unpaired(mpn=0) {a['unpaired']} | paired(0<mpn<100, "
      f"SOURCE-REACHABLE) {a['paired']} | masked(mpn=100) {a['masked']}\n")

    o = s["one_away"]
    w(f"\n-- ONE-AWAY UNITS ({o['total']}) -- broken down BY BUCKET --\n")
    for b, n in o["by_bucket"].items():
        w(f"  {b:14s} {n:4d}\n")
    w(f"  (COMPLETABLE == the single blocker is source-reachable; "
      f"ANON_BLOCKED == it is an\n   unpaired anon row.  /Od is assigned first, so a "
      f"one-away /Od unit lands there\n   even when its blocker is anon.)\n")
    if s["sub_buckets"]:
        w("\n  sub-classification of the one-away ANON_BLOCKED units\n")
        w("  (DG-1's partition -- 'anon' is NOT 'unreachable by source'):\n")
        for k, n in sorted(s["sub_buckets"].items(), key=lambda kv: -kv[1]):
            w(f"    {k:28s} {n:4d}   {SUB_BUCKET_LEGEND.get(k,'')[:60]}\n")
        srcwork = s["sub_buckets"].get("SAMECLASS_DIFFSIZE", 0)
        if srcwork:
            w(f"\n    ** {srcwork} of these are SOURCE WORK, not map rows. **\n")

    uv = s["unit_vanishes_if_drained"]
    w(f"\n-- STRUCTURAL: single-function units with matched=0 ({uv['total']}) --\n")
    for b, n in sorted(uv["by_bucket"].items(), key=lambda kv: -kv[1]):
        w(f"    {b:14s} {n:4d}\n")
    w(f"  ** {uv['boundary_move_impossible_anon_blocked']} of these are ANON_BLOCKED "
      f"= DG-2's STRUCTURALLY-IMPOSSIBLE class: **\n")
    w("     a boundary move drains the unit's only function, so the unit must be\n"
      "     DELETED -- it VANISHES rather than reaching 100%.  DG-2 found 3 such\n"
      "     among 23 hand-picked candidates and its filter never tested for it.\n")
    w("  The COMPLETABLE ones are NOT that class: their blocker is a NAMED sub-100\n"
      "  row, so source work is the lever and no move is contemplated.\n")

    if p.get("top_completable"):
        w("\n-- CHEAPEST COMPLETABLE UNITS (source-reachable, 1 blocker) --\n")
        for r in p["top_completable"][:15]:
            b = r["blockers"][0]
            w(f"  {r['unit'][:48]:48s} {b['mpn']:6.2f}% {b['size']:6d}B  "
              f"{b['name'][:44]}\n")
    w("\n")


def build_summary(records, sub_stats):
    cnt = collections.Counter(r["bucket"] for r in records)
    oneaway = [r for r in records if r["sub100_total"] == 1]
    return {
        "pairable_units": len(records),
        "buckets": {b: cnt.get(b, 0) for b in BUCKET_ORDER},
        "source_only_unit_ceiling": cnt.get("AT_100", 0) + cnt.get("COMPLETABLE", 0),
        "anon_rows": {
            "unpaired": sum(r["rows"]["anon_unpaired"] for r in records),
            "paired": sum(r["rows"]["anon_paired_sub100"] for r in records),
            "masked": sum(r["rows"]["anon_at_100_masked"] for r in records),
        },
        "named_rows_sub100": sum(r["rows"]["named_sub100"] for r in records),
        # Keyed on the BUCKET, not on reachable_blockers.  Those two disagree:
        # OD_REGION is assigned before the anon test, so a one-away /Od unit with
        # an anon blocker (e.g. `ssluse`) has reachable_blockers==0 while its
        # bucket is OD_REGION -- and the sub-classifier, which keys on the
        # bucket, then never sees it.  Reporting by bucket makes the one-away
        # breakdown and the sub-classified population agree by construction.
        "one_away": {
            "total": len(oneaway),
            "by_bucket": {b: sum(1 for r in oneaway if r["bucket"] == b)
                          for b in BUCKET_ORDER if any(r["bucket"] == b for r in oneaway)},
            "source_reachable": sum(1 for r in oneaway if r["bucket"] == "COMPLETABLE"),
            "anon_blocked": sum(1 for r in oneaway if r["bucket"] == "ANON_BLOCKED"),
            "od_region": sum(1 for r in oneaway if r["bucket"] == "OD_REGION"),
        },
        "sub_buckets": (sub_stats or {}).get("buckets", {}),
        # Report the cross-tab, never the bare total: the flag means two
        # different things in different buckets.  In ANON_BLOCKED it IS DG-2's
        # structurally-impossible class (a boundary move is the candidate lever
        # and it would drain the unit).  In COMPLETABLE the unit has a NAMED
        # sub-100 blocker and is finishable by SOURCE work -- a move is not the
        # lever there and the flag is merely descriptive.
        "unit_vanishes_if_drained": {
            "total": sum(1 for r in records if r["unit_vanishes_if_drained"]),
            "by_bucket": dict(collections.Counter(
                r["bucket"] for r in records if r["unit_vanishes_if_drained"])),
            "boundary_move_impossible_anon_blocked": sum(
                1 for r in records
                if r["unit_vanishes_if_drained"] and r["bucket"] == "ANON_BLOCKED"),
        },
    }


# ---------------------------------------------------------------------------
def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Regenerate the reachable-ceiling census (never cache it).")
    ap.add_argument("--root", default=None, help="repo/worktree root (default: this file's repo)")
    ap.add_argument("--report", default=None, help="path to report.json")
    ap.add_argument("--map", dest="mapfile", default=None, help="path to target_symbol_map.json")
    ap.add_argument("--splits", default=None, help="path to splits.txt")
    ap.add_argument("--json", dest="jsonout", default=None, help="write full census JSON here")
    ap.add_argument("--allow-stale", action="store_true",
                    help="proceed on a stale report.json (banner on every artifact)")
    ap.add_argument("--no-subclass", action="store_true",
                    help="skip the per-unit COFF sub-classification (faster)")
    ap.add_argument("--subclass-limit", type=int, default=None)
    ap.add_argument("--no-strict", action="store_true",
                    help="downgrade the consistency control to a warning (NOT recommended)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parents[1]
    report = Path(args.report) if args.report else root / "build/45410914/report.json"
    mapfile = Path(args.mapfile) if args.mapfile else root / "scripts/target_symbol_map.json"
    splitsf = Path(args.splits) if args.splits else root / "config/45410914/splits.txt"

    try:
        for label, p in (("report.json", report), ("target_symbol_map.json", mapfile),
                         ("splits.txt", splitsf)):
            if not p.exists():
                raise Refusal(4, f"REFUSING: MISSING INPUT {label}", [str(p)])

        stale = staleness_check(root, report, args.allow_stale)

        try:
            doc = json.loads(report.read_text())
        except Exception as e:
            raise Refusal(4, "REFUSING: report.json IS NOT PARSEABLE",
                          [f"{report}: {type(e).__name__}: {e}"])
        if "units" not in doc or "measures" not in doc:
            raise Refusal(4, "REFUSING: report.json HAS THE WRONG SCHEMA",
                          [f"top-level keys: {sorted(doc)[:10]}"])

        addr2name, mapvals, map_rawkeys = load_map(mapfile)
        splits = parse_splits(splitsf)

        records, join = build_census(doc, addr2name, mapvals, splits)
        collapsed_join_gate(join)
        consistency = run_consistency_control(records, join, report, doc,
                                              strict=not args.no_strict)
        sub_stats = None
        if not args.no_subclass:
            sub_stats = subclassify(records, root, addr2name, args.subclass_limit)

        summary = build_summary(records, sub_stats)
        top = [{"unit": r["unit"], "source_path": r["source_path"],
                "blockers": r["blocker_rows"]}
               for r in sorted((x for x in records
                                if x["bucket"] == "COMPLETABLE" and x["reachable_blockers"] == 1),
                               key=lambda x: -x["blocker_rows"][0]["mpn"])]

        payload = {
            "tool": "tools/reachable_ceiling.py",
            "tool_version": TOOL_VERSION,
            "generated_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
            "caveats": HEADER_CAVEATS,
            "rulers": {
                "buckets_and_function_counts": "match_percent_normalized (mpn)",
                "byte_credit": "fuzzy_match_percent (reported, never used for buckets)",
                "note": "mpn excludes arg-only penalties => blind to wrong callees",
            },
            "provenance": {
                "head": git(root, "rev-parse", "HEAD"),
                "head_dirty": bool(git(root, "status", "--porcelain")),
                "root": str(root),
                "report_path": str(report),
                "report_sha256": sha256_of(report),
                "report_mtime_epoch": stale["report_mtime_epoch"],
                "report_mtime_iso": stale["report_mtime_iso"],
                "map_path": str(mapfile), "map_sha256": sha256_of(mapfile),
                "map_address_rows": len(addr2name), "map_raw_keys": map_rawkeys,
                "splits_path": str(splitsf), "splits_sha256": sha256_of(splitsf),
                "staleness": stale,
            },
            "baseline_measures": doc["measures"],
            "bucket_legend": LEGEND,
            "sub_bucket_legend": SUB_BUCKET_LEGEND,
            "anon_gate": join,
            "consistency": consistency,
            "subclassification": sub_stats,
            "summary": summary,
            "top_completable": top,
            "units": records,
        }
        if not args.quiet:
            emit_text(payload)
        if args.jsonout:
            Path(args.jsonout).write_text(json.dumps(payload, indent=1))
            print(f"wrote {args.jsonout}  ({len(records)} units)")
        return 0

    except Refusal as r:
        print(_banner(r.title, r.lines), file=sys.stderr)
        print(f"\n[reachable_ceiling] REFUSED, exit {r.code} -- NO CENSUS WAS "
              f"PRODUCED (this is not a zero-filled report).", file=sys.stderr)
        return r.code


if __name__ == "__main__":
    sys.exit(main())
