#!/usr/bin/env python3
"""The withdrawal ledger of `scripts/symbol_aliases.json`, as a DENYLIST.

WHY THIS EXISTS
---------------
`tools/icf_alias_build.py` had **zero references to `withdrawn`** (lane W8-A
measured `grep -c withdrawn tools/icf_alias_build.py` -> 0 at `2fc2552a`).  The
withdrawal ledger was therefore not an input to the generator, and `--merge` is
additive: it carries hand-verified groups FORWARD and has no suppression list.
So every withdrawal ever adjudicated was durable only for as long as nobody
re-ran the generator.

That is not a hypothetical.  `scripts/symbol_aliases.json`'s own `_comment`
states it as a known property of the artifact:

    2026-08-19, lane ALIAS-CONSOLIDATION: 357 groups in 43 clusters had their
    folded lists withdrawn (9,395 memberships). ... The generator emitted a
    closure where it owed a partition; UNTIL THAT IS FIXED A REGENERATION GROWS
    THEM BACK.

An alias is PURE FORGIVENESS under the shipped `name_check` ruler, so a
re-fabricated one lifts `matched_code` BY CONSTRUCTION with no byte evidence
behind it.  Re-growing a withdrawn membership is therefore not a metric
regression that someone would notice -- it is a silent metric *gain*, which is
the direction this project's standing directive calls worse than a lower score.

WHAT A WITHDRAWAL IS KEYED ON
-----------------------------
Withdrawals are recorded PER MEMBERSHIP, in each group's `withdrawn[]` list, and
name a `spelling` (the folded side).  The survivor side is the group carrying
the record.  This module keys a denial on BOTH

    (survivor, spelling)   and   (address, spelling)

and denies on EITHER.  Both are needed, and neither alone is sufficient:

  * `address` alone is what lane W7-D's handoff H1 prescribed, but **51 of the
    1,595 groups have `address: null`** -- they would collapse into one bucket
    keyed on `None`.  (None of those 51 carries a withdrawal today; that is
    luck, not an invariant.)
  * `survivor` alone misses a withdrawal whose stated remedy was a MAP REPAIR
    that renamed the survivor at that address -- exactly the
    `SURVIVOR_SPELLING_CORRECTED_BY_MAP_REPAIR` and `SURVIVOR_MISNAMED` classes
    in the ledger.  The address outlives the name.

Measured on the shipped file: `survivor` is unique across all 1,595 groups
(1,595 distinct), and non-null `address` is unique across the 1,544 that have
one.  So neither key over-blocks by aliasing two groups together.

ANTI-VACUITY
------------
A denylist that silently loads nothing is worse than no denylist, because it
reports "0 suppressed" and reads as a clean run.  `load_ledger` therefore
REFUSES (raises `VacuousLedger`) when the ledger file is missing, unparseable,
or carries fewer than `MIN_RECORDS` withdrawal records.  Same reasoning as
`scripts/obj_pairing.py`'s `DEFAULT_MIN_DECLARED`: printing a denominator nobody
checks is how a coverage gap survives being written down.
"""

import json
from pathlib import Path

# The shipped ledger carries 10,058 records (10,056 dicts + 2 bare strings) at
# 2fc2552a.  This floor exists to catch an EMPTY or TRUNCATED load, not to pin
# the exact count -- withdrawals only ever accumulate, and a legitimate prune
# would be a deliberate act that has to move this constant on purpose.
MIN_RECORDS = 5000


class VacuousLedger(Exception):
    """The ledger loaded, but not enough of it to be protecting anything."""


class Withdrawal:
    __slots__ = ("survivor", "address", "spelling", "cls", "lane", "raw")

    def __init__(self, survivor, address, spelling, raw):
        self.survivor = survivor
        self.address = address
        self.spelling = spelling
        self.raw = raw
        if isinstance(raw, dict):
            self.cls = raw.get("class") or "<none>"
            self.lane = raw.get("lane") or "<none>"
        else:
            # Two records in the shipped file are a BARE STRING naming the
            # spelling, with no class and no lane.  They are still withdrawals.
            self.cls = "<BARE_STRING>"
            self.lane = "<none>"

    def describe(self):
        return ("spelling %s\n       withdrawn from survivor %s @ %s\n"
                "       class %s (lane %s)"
                % (self.spelling, self.survivor, self.address, self.cls, self.lane))


class Ledger:
    def __init__(self, records, path):
        self.records = records
        self.path = path
        self.by_survivor = {}
        self.by_address = {}
        for w in records:
            self.by_survivor.setdefault((w.survivor, w.spelling), w)
            if w.address:
                self.by_address.setdefault((w.address, w.spelling), w)

    def __len__(self):
        return len(self.records)

    def lookup(self, survivor, address, spelling):
        """Return the Withdrawal denying (survivor|address, spelling), or None."""
        w = self.by_survivor.get((survivor, spelling))
        if w is not None:
            return w
        if address:
            return self.by_address.get((address, spelling))
        return None


def spelling_of(rec):
    """A withdrawal record is a dict with a `spelling`, or a bare spelling str."""
    if isinstance(rec, str):
        return rec
    if isinstance(rec, dict):
        return rec.get("spelling")
    return None


def load_ledger(path, min_records=MIN_RECORDS):
    p = Path(path)
    if not p.exists():
        raise VacuousLedger("withdrawal ledger %s does not exist" % p)
    try:
        groups = json.loads(p.read_text())["groups"]
    except Exception as e:
        raise VacuousLedger("withdrawal ledger %s is unreadable: %s" % (p, e))
    out = []
    for g in groups:
        surv, addr = g.get("survivor"), g.get("address")
        for rec in g.get("withdrawn", []) or []:
            sp = spelling_of(rec)
            if sp:
                out.append(Withdrawal(surv, addr, sp, rec))
    if len(out) < min_records:
        raise VacuousLedger(
            "withdrawal ledger %s carries only %d record(s), below the %d floor. "
            "A denylist that loads nothing reports '0 suppressed' and reads as a "
            "clean run -- refusing rather than running unprotected."
            % (p, len(out), min_records))
    return Ledger(out, str(p))


def load_overrides(path, ledger):
    """Read an override file.  An override must NAME the record it overrides.

    Schema: a JSON list of objects, each with

        survivor / address   -- at least one, identifying the group
        spelling             -- the folded spelling being re-admitted
        overrides_class      -- MUST equal the ledger record's `class`
        reason               -- non-empty prose

    `overrides_class` is the load-bearing field: it makes a blind override
    impossible.  You cannot re-admit a membership without having read the record
    that withdrew it, because naming the wrong class is refused.  A blanket
    "allow everything" flag would defeat the entire mechanism, so there isn't one.
    """
    if not path:
        return {}
    entries = json.loads(Path(path).read_text())
    if not isinstance(entries, list):
        raise SystemExit("REFUSING: override file %s is not a JSON list" % path)
    allowed = {}
    for i, e in enumerate(entries):
        sp = e.get("spelling")
        surv, addr = e.get("survivor"), e.get("address")
        if not sp or not (surv or addr):
            raise SystemExit(
                "REFUSING: override[%d] needs `spelling` and one of "
                "`survivor`/`address`" % i)
        if not (e.get("reason") or "").strip():
            raise SystemExit(
                "REFUSING: override[%d] (%s) has no `reason`. An override that "
                "does not say why is indistinguishable from the defect this "
                "guard exists to stop." % (i, sp))
        w = ledger.lookup(surv, addr, sp)
        if w is None:
            raise SystemExit(
                "REFUSING: override[%d] names %s @ %s / %s, which carries NO "
                "withdrawal record. An override must override something -- "
                "otherwise it is a silent no-op that will be read as protection."
                % (i, surv, addr, sp))
        if e.get("overrides_class") != w.cls:
            raise SystemExit(
                "REFUSING: override[%d] claims class %r but the ledger record "
                "says %r.\n       %s\nNaming the class is what makes an override "
                "deliberate; a mismatch means the record was not read."
                % (i, e.get("overrides_class"), w.cls, w.describe()))
        if surv:
            allowed[(surv, sp)] = e
        if addr:
            allowed[(addr, sp)] = e
    return allowed


def overridden(allowed, survivor, address, spelling):
    return allowed.get((survivor, spelling)) or (
        allowed.get((address, spelling)) if address else None)
