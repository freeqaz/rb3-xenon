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

ATTRIBUTION: IS THE BLOCKER EVEN OURS?  (lane DL-2, 2026-08-03)
---------------------------------------------------------------
Lane DK-3 reported that some units this census ranks as source-reachable are
blocked by symbols our source does not own -- map/splits ATTRIBUTION defects,
not source work -- and named `HamRibbon` and `FlowDistance` as the worst cases,
each occurring **0 times in band.exe** against a control of `ObjectDir` 168.

DL-2 tried to turn that into a classifier.  **Two of the three claims did not
survive, and the refutations are more useful than the fix would have been.**

1. ** `FlowDistance` IS AT_100 AT HEAD ** with zero sub-100 rows -- it is not a
   blocker at all any more.  The staleness axis (shape 7) applies to lane
   findings, not just to JSON artifacts.
2. ** `HamRibbon`'s blockers are STL template bodies **
   (`?_M_erase@?$vector@V?$Key@VTransform@@@@...`), which carry NO class anchor.
   So DK-3's evidence is UNIT-level, not blocker-level.
3. ** THE UNIT-LEVEL TEST FAILS ITS NULL, DECISIVELY. **  "unit stem occurs 0
   times in band.exe" fires on:

       AT_100 (untreated)        72/204 = 35.3%   <-- the null
       all sub-100 buckets      221/745 = 29.7%
       ENRICHMENT                          0.84x  <-- BELOW ONE

   An already-complete unit is *more* likely to trip it than a blocked one.
   DK-3's quoted null -- a fabricated name scoring 0 -- measured whether the
   SCANNER works, not whether the PREDICATE discriminates.  That is exactly
   shape 4 (base-rate error), and the same failure as pin-tail 1.25x and the
   name_check fold-alias 1.95x.  ** DO NOT RE-FUND THE UNIT-STEM TEST. **

What DOES survive is the BLOCKER-CLASS form, and only as a calibrated
suspicion::

    named rows whose CLASS is absent from band.exe
      AT_100  (untreated null) 1152/15237 =  7.56%
      SUB_100 (charged)         565/ 2250 = 25.11%
      ENRICHMENT                             3.32x

3.32x is real and it is NOT a classifier: roughly one flagged row in three is
expected to be a false positive, because a non-polymorphic helper class carries
no RTTI and no factory-registration string (`CSHA1` reads 0 and is perfectly
real).  So this tool:

  * ATTACHES the label to each named blocker (`CLASS_ABSENT_FROM_RETAIL` /
    `CLASS_PRESENT_IN_RETAIL` / `NO_CLASS_ANCHOR` / `UNKNOWN_NO_RETAIL_BINARY`),
    named after WHAT WAS MEASURED, never after the inferred remedy (rule 14);
  * RECOMPUTES and PRINTS the null + enrichment on every run, so the number
    travels with the flag and cannot be laundered into a verdict; and
  * ** DOES NOT REMOVE FLAGGED UNITS FROM COMPLETABLE OR FROM THE CEILING. **
    At a 7.56% base rate that would silently delete real source work -- the
    over-correction the brief warned against, in the opposite direction.

The three-way anon model (unpaired / paired / at-100) is untouched: it was
independently verified (`anon@100 == masked_equal_functions` exactly) and is
still checked every run.

The retail scan is pure-Python `bytes.count`, immune to the binary-blind `grep`
shim -- and `--sabotage retail-blind` models that shim exactly, to prove the
known-positive control can fail.

EXIT CODES (a refusal is a distinct code, never a zero-filled report)
----------------------------------------------------------------------
    0   census produced
    2   CONSISTENCY CONTROL FAILED (independent recount disagrees)
    3   STALE INPUT refused (report.json older than the tree it describes)
    4   COLLAPSED JOIN / unusable input refused (the CY-1 lesson: a tool that
        reports an empty tree as done is shaped exactly like a decisive negative)
    5   ATTRIBUTION SCANNER VACUOUS refused (a known-present class read absent,
        so every blocker would be flagged foreign)
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
    "THE ATTRIBUTION FLAG IS A SUSPICION, NOT A VERDICT, AND ITS UNIT-LEVEL "
    "SIBLING IS REFUTED.  'blocker class absent from band.exe' now POOLS to "
    "~5.3x, but THAT POOLED FIGURE IS CONFOUNDED (Simpson's paradox): it EXCEEDS "
    "the enrichment in EITHER stratum -- 2.02x plain / 1.45x namespaced -- because "
    "the v2 anchor admits ~3,047 extra vendor-middleware rows.  On the comparable "
    "stratum (rows BOTH anchors can judge) v1 and v2 BOTH measure 2.03x, so the "
    "anchor swap itself moves NOTHING.  QUOTE THE STRATIFIED RATES, NEVER THE "
    "POOLED ONE -- and note 1.45x sits in the band already refuted for classifier "
    "use (pin-tail 1.25x, fold-alias 1.95x).  ~1 flag in 3 is a false positive and "
    "flagged units are deliberately LEFT IN COMPLETABLE. "
    "The unit-STEM form measured 0.84x (ANTI-enriched: 35.3% of AT_100 units trip "
    "it) and must not be re-funded.  Adjudicate on retail bytes, never on a flag.",
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
#
# ---------------------------------------------------------------------------
# CLASS ANCHORING  (lane DN-2, 2026-08-03 -- the cls_of migration)
# ---------------------------------------------------------------------------
# The shipped anchor was two anchored regexes.  Lane DM-4 diagnosed three
# distinct defects in them, all of which produce a WRONG-OR-MISSING class name
# rather than an error:
#
#   1. GREEDY INITIAL.  In `\?\?[0-9_][A-Z]?([A-Za-z_]\w*)@@` the optional
#      `[A-Z]?` is greedy and EATS THE CLASS'S OWN FIRST LETTER:
#          ??0RndText@@   -> 'ndText'      (1,702 rows tree-wide)
#          ??1ObjectDir@@ -> 'bjectDir'
#   2. A NAMESPACE RETURNED AS A CLASS.  `?fn@ns@@YAXXZ` is a free function in
#      namespace `ns`; the shipped regex hands back 'ns' as if it were a class
#      (998 rows).
#   3. NESTED SCOPES UNPARSEABLE.  Both regexes demand `@@` immediately after
#      the first scope identifier, so `?Meth@Object@Hmx@@` matches neither and
#      returns None => EVERY Hmx::Object method in the tree -- 3,593 rows -- was
#      filed NO_CLASS_ANCHOR and never judged.
#
# ⚠ THIS IS A COVERAGE DEFECT, NOT A CORRECTNESS DEFECT, and the distinction is
# what makes the swap safe.  DM-4 proved defect 1 is SELF-MASKING for the
# attribution flag: `RetailNames.count()` is a SUBSTRING search over band.exe and
# the truncated name is a substring of the full one, so 'ndText' and 'RndText'
# score present/absent alike.  Measured tree-wide label churn is TEN ROWS.  So do
# NOT expect the published ~3.3x enrichment to move; if it moves a lot, something
# ELSE is wrong and the run should be investigated, not accepted.
#
# `cls_of_v2` is ported verbatim from tools/no_anchor_partition.py (DM-4,
# f5016a76), which is also where its own validation lives.  The v1 implementation
# is retained ONLY so `--sabotage cls-truncate` can restore the defect and prove
# the regression pin can fail.
_CLS_CTOR = re.compile(r"\?\?[0-9_][A-Z]?([A-Za-z_]\w*)@@")
_CLS_METH = re.compile(r"\?[A-Za-z_]\w*@([A-Za-z_]\w*)@@")

_SPECIAL = re.compile(r"^\?\?(__[0-9A-Za-z]|_[0-9A-Za-z]|[0-9A-Za-z])")
_PLAIN = re.compile(r"^\?([?$@]?[A-Za-z_0-9]*)@")
_ENC = re.compile(r"@@([A-Z_$])")


def cls_of_v1(name):
    """THE SHIPPED (DEFECTIVE) ANCHOR.  Kept only as the sabotage leg."""
    m = _CLS_CTOR.match(name) or _CLS_METH.match(name)
    return m.group(1) if m else None


def basic_name_extent(name: str):
    """Index just past an MSVC decorated name's basic name, or None."""
    if not name.startswith("?") or name.startswith("??$"):
        return None
    m = _SPECIAL.match(name) or _PLAIN.match(name)
    return m.end() if m else None


def cls_of_v2(name: str):
    """`cls_of` with the forms v1 cannot parse repaired.  -> (class|None, why).

    An MSVC decorated name is `?` <basic-name> <scope...> `@@` <encoding>, so the
    basic name's extent is decidable without parsing the type grammar; the
    immediate scope is then the first identifier after it.  The encoding letter
    after `@@` separates a CLASS scope from a NAMESPACE scope: `Y` == free
    function (so the scope is a namespace, NOT a class), anything else == member.
    """
    e = basic_name_extent(name)
    if e is None:
        return None, "no_basic_name"
    rest = name[e:]
    m = re.match(r"([A-Za-z_]\w*)@", rest)
    if not m:
        if rest.startswith("?$"):
            return None, "template_scope"
        if rest.startswith("@"):
            return None, "global_scope"
        if rest.startswith("?A"):
            return None, "anonymous_namespace"
        return None, "unparsed_scope"
    enc = _ENC.search(name)
    if enc and enc.group(1) == "Y":
        return None, "namespace_scope_free_function"
    return m.group(1), "ok"


#: "v2" (default) or "v1" (restored defect, via --sabotage cls-truncate).
_CLS_IMPL = "v2"


def set_cls_impl(which):
    global _CLS_IMPL
    _CLS_IMPL = which


def cls_of(name):
    if _CLS_IMPL == "v1":
        return cls_of_v1(name)
    return cls_of_v2(name)[0]


# ---------------------------------------------------------------------------
# ATTRIBUTION (DL-2) -- a CALIBRATED SUSPICION, never a classifier
# ---------------------------------------------------------------------------
CLASS_PRESENT = "CLASS_PRESENT_IN_RETAIL"
CLASS_ABSENT = "CLASS_ABSENT_FROM_RETAIL"
NO_ANCHOR = "NO_CLASS_ANCHOR"          # free fn / STL template -> not judged
NO_BINARY = "UNKNOWN_NO_RETAIL_BINARY"  # cannot judge; NOT 'present'

ATTRIBUTION_LEGEND = {
    CLASS_PRESENT: "the blocker's class name occurs in band.exe",
    CLASS_ABSENT: (
        "the blocker's class name occurs ZERO times in band.exe.  A SUSPICION "
        "ONLY -- measured enrichment ~3.3x over the untreated population, whose "
        "own rate is ~7.6%, so ~1 in 3 flags is expected to be a false positive "
        "(a non-polymorphic helper carries no RTTI and no registration string; "
        "CSHA1 reads 0 and is real).  Adjudicate on retail bytes, never on this."
    ),
    NO_ANCHOR: (
        "no class parsed from the mangled name -- a free function or an STL "
        "template instantiation.  NOT JUDGED.  This is the majority of the "
        "highest-penalty blockers, and it is where HamRibbon's actually sit."
    ),
    NO_BINARY: "band.exe unavailable -- the question was not asked, not answered",
}


class RetailNames:
    """Does a class name occur anywhere in the retail image?

    Pure-Python `bytes.count`, so it is immune to the binary-blind `grep` shim
    (a shell `grep` here returns only FALSE NEGATIVES on band.exe, which would
    make every class read absent -- the exact vacuity `--sabotage retail-blind`
    reproduces).
    """

    def __init__(self, path: Path, sabotage=None):
        self.path = Path(path)
        self.sabotage = sabotage
        self.data = self.path.read_bytes() if self.path.exists() else None
        self._memo = {}

    @property
    def available(self):
        return self.data is not None

    def count(self, cls):
        if self.data is None:
            return None
        if self.sabotage == "retail-blind":
            return 0                      # models the binary-blind grep shim
        if cls not in self._memo:
            self._memo[cls] = self.data.count(cls.encode())
        return self._memo[cls]

    def label(self, mangled):
        cls = cls_of(mangled)
        if cls is None:
            return NO_ANCHOR, None, None
        if self.data is None:
            return NO_BINARY, cls, None
        n = self.count(cls)
        return (CLASS_PRESENT if n else CLASS_ABSENT), cls, n


def attribution_controls(RN: RetailNames):
    """Rule 1 + rule 4: assert a KNOWN POSITIVE before any absence is believed,
    and show the instrument producing the OTHER label on a known-opposite case."""
    rows = []
    if not RN.available:
        rows.append(("retail binary present", False, f"missing {RN.path}"))
        return rows
    n = RN.count("ObjectDir")
    rows.append(("known-positive: 'ObjectDir' occurs in band.exe", n == 168,
                 f"count={n} (want 168; a vacuous scanner reads 0 and would flag "
                 f"EVERY blocker foreign)"))
    n2 = RN.count("RndText")
    rows.append(("second known-positive: 'RndText' occurs", bool(n2),
                 f"count={n2} (want >0; note it is only 2 -- a LOW count is not "
                 f"absence, which is why the threshold is ==0 and not a margin)"))
    n3 = RN.count("ZzQqDefinitelyNotAClassDL2")
    rows.append(("known-negative: a fabricated class reads 0", n3 == 0,
                 f"count={n3} (want 0; proves the scanner is not matching "
                 f"everything -- rule 4, the second label)"))
    return rows



# ---------------------------------------------------------------------------
# ★★ THE POOLED ENRICHMENT IS CONFOUNDED BY SCOPE DEPTH (lane DN-2, 2026-08-03)
# ---------------------------------------------------------------------------
# Swapping the class anchor v1 -> v2 moved the headline enrichment 3.31x -> 5.33x.
# That is NOT the anchor disagreeing with itself.  On the INTERSECTION -- the rows
# BOTH anchors can judge -- v1 and v2 measure 2.03x and 2.03x, IDENTICAL to two
# decimals, exactly as DM-4's self-masking argument predicted (RetailNames.count
# is a substring search, so a truncated class name scores like the full one).
#
# The whole movement is a POPULATION change: v2 can parse nested scopes, which
# admits 3,047 additional CHARGED rows (1,276 -> 4,323), almost all namespaced
# vendor/middleware classes (DSP::, Synapse::, soundtouch::, D3DX::, TrueColor::).
# Those genuinely do not appear as strings in band.exe -- they are precisely the
# non-polymorphic-helper FALSE POSITIVE the flag's own legend warns about.
#
# ⇒ AND THE POOLED FIGURE IS LARGER THAN THE ENRICHMENT IN *EITHER* STRATUM:
#       depth 1  (plain global class)   null  7.57%  charged 15.27%  = 2.02x
#       depth >=2 (namespaced/nested)   null 40.73%  charged 59.19%  = 1.45x
#       POOLED                          null  8.67%  charged 46.22%  = 5.33x
#   (measured by this tool at HEAD 2026-08-03; regenerate rather than quote.)
#   That is Simpson's paradox: pooling strata with very different BASE RATES
#   manufactures enrichment out of composition.  So the published 3.32x was
#   ALREADY a confounded statistic (2.03x on its own comparable stratum), and
#   the corrected anchor makes the confounding bigger, not the suspicion stronger.
#   1.45x sits in the same band as the predicates this project has already
#   REFUTED for being used as classifiers (pin-tail 1.25x, fold-alias 1.95x).
#
# The tool therefore reports the STRATIFIED numbers next to the pooled one and
# flags `pooled_is_confounded` whenever pooling exceeds every stratum.  A number
# that cannot be quoted bare is the point.
def _scope_depth(name):
    """How many scope identifiers precede the `@@` (1 == plain global class)."""
    e = basic_name_extent(name)
    if e is None:
        return 0
    m = re.match(r"((?:[A-Za-z_]\w*@)+)@", name[e:])
    return m.group(1).count("@") if m else 0


def _enrichment_by_scope_depth(report_doc, RN):
    acc = collections.defaultdict(lambda: [0, 0, 0, 0])   # na, nn, ca, cn
    for u in report_doc.get("units", []):
        for f in u.get("functions") or []:
            nm = f["name"]
            if ANON_RX.match(nm):
                continue
            lab, _c, _n = RN.label(nm)
            if lab in (NO_ANCHOR, NO_BINARY):
                continue
            k = "depth_1_plain_class" if _scope_depth(nm) == 1 else "depth_2plus_nested_or_namespaced"
            v = acc[k]
            if f["match_percent_normalized"] == 100.0:
                v[1] += 1
                v[0] += (lab == CLASS_ABSENT)
            else:
                v[3] += 1
                v[2] += (lab == CLASS_ABSENT)
    out = {}
    for k, (na, nn, ca, cn) in acc.items():
        nr = na / nn if nn else 0.0
        cr = ca / cn if cn else 0.0
        out[k] = {"null_absent": na, "null_anchored": nn, "null_rate_pct": 100 * nr,
                  "charged_absent": ca, "charged_anchored": cn,
                  "charged_rate_pct": 100 * cr,
                  "enrichment": (cr / nr) if nr else None}
    return out


def _pooled_exceeds_all_strata(strata, pooled):
    es = [v["enrichment"] for v in strata.values() if v["enrichment"] is not None]
    if pooled is None or not es:
        return None
    return bool(pooled > max(es) + 1e-9)


def annotate_attribution(records, report_doc, RN: RetailNames):
    """Label every NAMED blocker, and RECOMPUTE the null + enrichment so the
    calibration always travels with the flag (rule 3, rule 15)."""
    for r in records:
        for b in r["blocker_rows"]:
            if b["kind"] != "named":
                continue
            lab, cls, n = RN.label(b["name"])
            b["attribution"] = {"label": lab, "class": cls, "retail_occurrences": n}

    # --- THE NULL, recomputed from report.json every run.  The untreated
    # population is NAMED rows already at mpn==100: whatever their true
    # correctness, they are the rows nobody is proposing to reclassify.
    null_abs = null_anch = chg_abs = chg_anch = 0
    for u in report_doc.get("units", []):
        for f in u.get("functions") or []:
            nm = f["name"]
            if ANON_RX.match(nm):
                continue
            lab, _c, _n = RN.label(nm)
            if lab in (NO_ANCHOR, NO_BINARY):
                continue
            at100 = (f["match_percent_normalized"] == 100.0)
            if at100:
                null_anch += 1
                null_abs += (lab == CLASS_ABSENT)
            else:
                chg_anch += 1
                chg_abs += (lab == CLASS_ABSENT)
    nr = (null_abs / null_anch) if null_anch else 0.0
    cr = (chg_abs / chg_anch) if chg_anch else 0.0
    strata = _enrichment_by_scope_depth(report_doc, RN)
    return {
        "strata_by_scope_depth": strata,
        "pooled_is_confounded": _pooled_exceeds_all_strata(strata, (cr / nr) if nr else None),
        "available": RN.available,
        "retail_exe": str(RN.path),
        "sabotage": RN.sabotage,
        "null_untreated_at100": {"absent": null_abs, "class_anchored": null_anch,
                                 "rate_pct": 100.0 * nr},
        "charged_sub100": {"absent": chg_abs, "class_anchored": chg_anch,
                           "rate_pct": 100.0 * cr},
        "enrichment": (cr / nr) if nr else None,
        "discriminating": bool(nr and cr / nr > 1.0),
        "legend": ATTRIBUTION_LEGEND,
        "verdict": (
            "SUSPICION ONLY.  Enrichment is reported above; the untreated "
            "population already trips this flag at the stated rate, so a flagged "
            "row is a TRIAGE CANDIDATE, not an attribution defect.  Flagged units "
            "are deliberately NOT removed from COMPLETABLE or from the ceiling."
        ),
        "refuted_sibling_test": (
            "THE UNIT-STEM FORM OF THIS TEST IS REFUTED, DO NOT RE-FUND IT: "
            "'unit stem occurs 0 times in band.exe' measured 35.3% on AT_100 vs "
            "29.7% on sub-100 units = 0.84x, i.e. ANTI-enriched.  It fires on one "
            "healthy unit in three."
        ),
    }


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

    at = p.get("attribution")
    if at:
        w("\n-- BLOCKER ATTRIBUTION (a CALIBRATED SUSPICION, never a classifier) --\n")
        for c in at["controls"]:
            w(f"  [{'PASS' if c['ok'] else 'FAIL'}] {c['check']}\n         {c['detail']}\n")
        n, c = at["null_untreated_at100"], at["charged_sub100"]
        w(f"  class ABSENT from band.exe:\n")
        w(f"    NULL   (named rows already at 100) {n['absent']:6d}/{n['class_anchored']:6d}"
          f" = {n['rate_pct']:5.2f}%\n")
        w(f"    CHARGED(named rows sub-100)        {c['absent']:6d}/{c['class_anchored']:6d}"
          f" = {c['rate_pct']:5.2f}%\n")
        e = at["enrichment"]
        w(f"    ENRICHMENT (POOLED)                {'n/a' if e is None else f'{e:.2f}x'}"
          f"   discriminating={at['discriminating']}\n")
        st = at.get("strata_by_scope_depth") or {}
        if st:
            w("  ** STRATIFIED BY SCOPE DEPTH -- quote THESE, never the pooled figure:\n")
            for k, v in sorted(st.items()):
                ee = v["enrichment"]
                w(f"    {k:34s} null {v['null_rate_pct']:5.2f}%  "
                  f"charged {v['charged_rate_pct']:5.2f}%  "
                  f"= {'n/a' if ee is None else f'{ee:.2f}x'}\n")
            if at.get("pooled_is_confounded"):
                w("  ** POOLED > EVERY STRATUM (Simpson's paradox): the headline\n"
                  "     enrichment is manufactured by COMPOSITION, not by the flag\n"
                  "     discriminating.  The strata differ hugely in BASE RATE, so the\n"
                  "     pooled number is not interpretable.  DO NOT QUOTE IT.\n")
        w(f"  ** {at['verdict']}\n")
        w(f"  ** {at['refuted_sibling_test']}\n")
        cnt = collections.Counter(
            b.get("attribution", {}).get("label")
            for r in p["units"] for b in r["blocker_rows"]
            if b["kind"] == "named" and b.get("attribution"))
        w("  labels over NAMED blocker rows:\n")
        for k, v in cnt.most_common():
            w(f"    {k:26s} {v:5d}\n")

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
# REGRESSION PIN for the class anchor (INSTRUMENT_DESIGN rules 1, 2, 4, 6)
# ---------------------------------------------------------------------------
#: Each row: (mangled, v2-expected-class, v1-defective-answer, what it pins).
#: The third column is what makes this table DISCRIMINATING rather than
#: decorative -- a pin both implementations satisfy proves nothing (rule 2:
#: "a differ hardcoded to return nothing passes an empty-check").
CLS_PINS = [
    ("??0RndText@@QAA@XZ", "RndText", "ndText",
     "greedy [A-Z]? eats the class initial (1,702 rows)"),
    ("??1ObjectDir@@UAA@XZ", "ObjectDir", "bjectDir",
     "same defect on a dtor"),
    ("??_GBandCharacter@@UAAPAXI@Z", "BandCharacter", "andCharacter",
     "same defect on a deleting dtor (??_G)"),
    ("?Poll@Object@Hmx@@UAAXXZ", "Object", None,
     "NESTED SCOPE: every Hmx::Object method -- 3,593 rows -- was unparseable"),
    ("?Save@FaderGroup@@QAAXAAVBinStream@@@Z", "FaderGroup", "FaderGroup",
     "the plain form v1 got right: v2 must not regress it"),
    ("?PropSync@@YA_NAAVFaderGroup@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z",
     None, None, "global free function: no class, and v1 agrees"),
]

#: A namespace-scope free function must NOT be reported as a class.  Held apart
#: from CLS_PINS because v1's wrong answer here is a *fabricated* class, which is
#: a different failure from a truncated one.
NS_PIN = ("?fn@ns@@YAXXZ", None, "ns",
          "namespace returned AS A CLASS (998 rows)")


def run_cls_selftest(sabotaged=False):
    """`--selftest` must PASS on v2 and FAIL under `--sabotage cls-truncate`."""
    print("reachable_ceiling class-anchor selftest  impl=" + _CLS_IMPL
          + ("   SABOTAGE=cls-truncate" if sabotaged else ""))
    if sabotaged:
        print("  (sabotage leg: this run is EXPECTED to FAIL -- it proves the "
              "pin is not vacuous)")
    rows = []
    bad = []
    for mangled, want, _v1, why in CLS_PINS + [NS_PIN]:
        got = cls_of(mangled)
        if got != want:
            bad.append(f"{mangled[:46]}: got {got!r} want {want!r}  [{why}]")
    rows.append(("PIN. every anchor row resolves to its true class",
                 not bad, "; ".join(bad[:3]) if bad
                 else f"{len(CLS_PINS)+1}/{len(CLS_PINS)+1} rows correct"))

    # ★ RULE 2 / RULE 4: show the table DISCRIMINATES.  If v1 and v2 agreed on
    #   every pinned row the table would pass under the sabotage too, and the
    #   whole control would be decorative.  Note row 5 and 6 are deliberately
    #   rows where they AGREE -- a pin set of only-disagreements would not catch
    #   a v2 that broke the cases v1 handled.
    disagree = [m for m, w, v1a, _ in CLS_PINS + [NS_PIN]
                if cls_of_v1(m) != w]
    agree = [m for m, w, v1a, _ in CLS_PINS + [NS_PIN] if cls_of_v1(m) == w]
    rows.append(("DISCRIMINATION. the defective v1 anchor FAILS a majority of "
                 "these rows (so the pin can fail) while some rows are ones v1 "
                 "got right (so v2 cannot regress them unnoticed)",
                 len(disagree) >= 4 and len(agree) >= 2,
                 f"v1 wrong on {len(disagree)}/{len(CLS_PINS)+1}, v1 right on "
                 f"{len(agree)} (want >=4 wrong and >=2 right)"))

    # RULE 4, the second label: v2 must return None for the two scope kinds that
    # are NOT classes, and a name for the ones that are.  A one-label anchor
    # ("everything has a class" or "nothing does") is not believed.
    labels = {cls_of_v2(m)[1] for m, *_ in CLS_PINS + [NS_PIN]}
    rows.append(("SECOND LABEL. v2 emits >1 distinct reason code across the pins",
                 len(labels) >= 3, f"reasons={sorted(labels)}"))

    nfail = 0
    for name, ok, detail in rows:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}\n         {detail}")
        nfail += (not ok)
    print(f"  {len(rows) - nfail}/{len(rows)} controls passed")
    return 6 if nfail else 0


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
    ap.add_argument("--retail-exe", default=None,
                    help="retail PE for the attribution flag (default orig/45410914/band.exe)")
    ap.add_argument("--no-attribution", action="store_true",
                    help="skip the blocker-attribution labelling entirely")
    ap.add_argument("--selftest", action="store_true",
                    help="run the class-anchor regression pin (offline, no build); "
                         "exit 6 on failure")
    ap.add_argument("--sabotage", choices=["retail-blind", "cls-truncate"],
                    default=None,
                    help="retail-blind: make the retail scanner always read 0 "
                         "(models the binary-blind grep shim) -- the run MUST then "
                         "REFUSE with exit 5.  cls-truncate: restore the shipped "
                         "greedy-regex cls_of defect -- --selftest MUST then FAIL "
                         "with exit 6.")
    args = ap.parse_args(argv)

    # The class anchor is process-global (cls_of is called from the attribution
    # labeller AND the sub-classifier), so the sabotage is set once, here, and
    # stamped into provenance so a sabotaged census can never be mistaken for a
    # real one.
    set_cls_impl("v1" if args.sabotage == "cls-truncate" else "v2")
    if args.selftest:
        return run_cls_selftest(args.sabotage == "cls-truncate")

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
        # -- ATTRIBUTION (DL-2).  Controls FIRST: if a known-present class reads
        # absent, every blocker would be flagged foreign and the census would be
        # a confident lie in the opposite direction from CY-1's.  Refuse.
        attribution = None
        if not args.no_attribution:
            rexe = Path(args.retail_exe) if args.retail_exe \
                else root / "orig/45410914/band.exe"
            RN = RetailNames(rexe, sabotage=(args.sabotage
                                             if args.sabotage == "retail-blind"
                                             else None))
            ctl = attribution_controls(RN)
            if any(not ok for _n, ok, _d in ctl):
                raise Refusal(5, "REFUSING: ATTRIBUTION SCANNER IS VACUOUS", [
                    f"{n}: {d}" for n, ok, d in ctl if not ok
                ] + [
                    "",
                    "A known-present class read as absent.  Every blocker would "
                    "then be flagged foreign -- a decisive-looking negative "
                    "produced by a dead instrument.",
                    "This is the binary-blind `grep` shim's failure shape; the "
                    "real scan is pure-Python bytes.count for exactly that reason.",
                    "Re-run with --no-attribution for the bucket-only census.",
                ])
            # ★ NON-INTERFERENCE PIN.  The brief's explicit warning was "do not
            # over-correct: the three-way anon model is RIGHT".  Snapshot every
            # scoring field BEFORE labelling and require it unchanged after, so
            # a future edit that lets the attribution flag feed back into
            # bucketing, the ceiling or the anon split fails HERE rather than
            # silently deleting real source work at a 7.6% false-positive rate.
            before = [(r["unit"], r["bucket"], r["reachable_blockers"],
                       tuple(sorted(r["rows"].items()))) for r in records]
            attribution = annotate_attribution(records, doc, RN)
            after = [(r["unit"], r["bucket"], r["reachable_blockers"],
                      tuple(sorted(r["rows"].items()))) for r in records]
            if before != after:
                bad = [b[0] for b, a in zip(before, after) if b != a][:10]
                raise Refusal(2, "CONSISTENCY CONTROL FAILED: ATTRIBUTION IS NOT "
                                 "PURELY ADDITIVE", [
                    "Labelling blockers changed bucket / reachable_blockers / row "
                    "counts, which it must never do.",
                    f"first divergent units: {bad}",
                    "At a measured ~7.6% false-positive rate on the untreated "
                    "population, letting this flag drive bucketing would delete "
                    "real source work.",
                ])
            attribution["controls"] = [{"check": n, "ok": ok, "detail": d}
                                       for n, ok, d in ctl]
            attribution["controls"].append({
                "check": "attribution is purely additive (buckets/ceiling/anon "
                         "split unchanged)",
                "ok": True,
                "detail": f"{len(records)} unit records identical in bucket, "
                          f"reachable_blockers and row counts before/after labelling"})

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
                # DN-2: which class anchor produced every class-scoped field in
                # this artifact.  "v1" means --sabotage cls-truncate was used and
                # the census is DELIBERATELY DEFECTIVE -- never quote it.
                "cls_anchor_impl": _CLS_IMPL,
            },
            "baseline_measures": doc["measures"],
            "bucket_legend": LEGEND,
            "sub_bucket_legend": SUB_BUCKET_LEGEND,
            "anon_gate": join,
            "attribution": attribution,
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
