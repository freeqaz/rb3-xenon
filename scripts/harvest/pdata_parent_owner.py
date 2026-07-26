#!/usr/bin/env python3
"""pdata_parent_owner.py — HARD unit attribution for EH funclets via ``.pdata``.

WHY THIS EXISTS
---------------
Every attribution channel this project has built for *unowned* retail code is a
**similarity** score: objdiff's reloc-masked byte signature
(``pair_funclets_by_bytes``) pairs a target ``fn_<VA>`` with any funclet-shaped
base symbol whose masked bytes are equal.  Two independent measurements show how
weak that is as *identity* evidence:

* ``docs/plans/lane-am-diffunit-2026-07-26.md`` — only **128 of 1,531** evidenced
  functions have a byte signature unique to one unit tree-wide (1.8%); the modal
  32-byte shape occurs in **694 of our 1,024 objs**; median ``LEFT_ONLY``
  multiplicity is **122 units**.
* the report driver sets ``function_reloc_diffs: None``
  (``objdiff-cli/src/cmd/report.rs``), so "byte-true" means *identical modulo
  every relocation target* — same instruction bytes, different callees, still a
  true 100%.

So a LEFT_ONLY verdict usually means only "the right neighbour happens not to
carry a stereotyped shape that 122 other units do".  It cannot exclude a **third**
unit owning the body.

**Parentage can.**  MSVC emits each EH funclet while compiling its parent
function, so a funclet's object file *is* its parent's object file.  Find the
parent, find the parent's unit, and you have a **hard exclusion**: every other
unit is impossible.  This is immune to the relocation-masking hazard above,
because it never looks at bytes at all.

THE CHAIN (already implemented — this module reuses it)
-------------------------------------------------------
``.pdata`` exception flag -> the two DWORDs immediately *before* the entry point
are ``{handler, handlerData}`` -> ``handlerData`` is an MSVC ``_s_FuncInfo``
(magic ``0x19930522``) -> its unwind-map actions and try-block catch handlers are
exactly the funclet entry points.  ``scripts/harvest/funclet_cascade_rank.py``
walks this; we import its ``PE`` / ``parse_pdata`` machinery rather than
re-deriving it, and add:

1. **multi-parent detection** — ``funclet_cascade_rank.parse_eh`` uses
   ``setdefault`` and therefore silently drops a second parent.  A funclet with
   two parents is an ICF fold and is *not* a hard exclusion.  Measured on the
   retail image (2026-07-26): **26,312 of 26,321 funclets (99.97%) have exactly
   one parent**; only **9** are multi-parented.  Parentage is a near-bijection.
2. the **unit join** against ``config/45410914/splits.txt`` ``.text`` spans,
   keyed by the **full path header** (never basename — objdiff units are keyed by
   basename and a collision once measured -613).

MEASURED FUNNEL (retail TU5 image, 2026-07-26 — see docs/plans/lane-an-*.md)
---------------------------------------------------------------------------
Over laneAM's 7,265-function different-unit gap pool, using PRE-laneAM pins so
the evidence is independent of laneAM's own fills::

    PROVES_LEFT               1,244
    PROVES_RIGHT                 31
    THIRD_UNIT (excl. both)      54
    parent unpinned           2,625   (no verdict)
    no EH parent at all       3,311   (no verdict)

i.e. **18.3% of the pool gets a hard verdict**, and the 40:1 left/right skew is a
*mechanical* fact (the linker places a TU's funclets after its parent), which
independently explains — and partly rehabilitates — laneAM's observed left bias.

★ The funnel **collapses** on the class that motivated this tool: of the ~4,400
functions unreachable under either neighbour, only **123** get any verdict
(2.8%), because **3,309 of them have no EH parentage at all**.  Parentage does
not rescue that residue; it is a source problem, not an attribution one.

Where it *does* pay is honesty: it re-classifies existing byte-signature fills
from "coin flip" to proven or contradicted.

USAGE
-----
    # census of the parent map + how unique it is
    python3 scripts/harvest/pdata_parent_owner.py census

    # THE PRIMARY QUERY: for an unclaimed span, which units are possible owners?
    python3 scripts/harvest/pdata_parent_owner.py span 0x82271400 0x822717A0

    # audit a fills manifest ({tier: [{va, claimed_by, ...}]}) for mis-attribution
    python3 scripts/harvest/pdata_parent_owner.py audit docs/plans/laneAM/fills-T1.json

    # every currently-unpinned funclet whose parent IS pinned -> micro-pin proposals
    python3 scripts/harvest/pdata_parent_owner.py actionable --json out.json

    # sweep every unclaimed gap in splits.txt
    python3 scripts/harvest/pdata_parent_owner.py gaps --json out.json

Options: ``--splits PATH`` (defaults to config/45410914/splits.txt; pass a
pre-edit backup to make an audit independent of the edit being audited),
``--repo PATH``, ``--exe PATH``.
"""
from __future__ import annotations

import argparse
import bisect
import importlib.util
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# auto_03_* spans in 0x828-0x82C are XDK vendor + Quazal: hard-skipped by the
# project owner, and 58% of the raw unowned pool, so exclude them *inside* the
# funnel rather than after it.
VENDOR_LO, VENDOR_HI = 0x82800000, 0x82C00000


def _load_fcr(repo):
    p = Path(repo) / "scripts/harvest/funclet_cascade_rank.py"
    spec = importlib.util.spec_from_file_location("_fcr", p)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


# --------------------------------------------------------------- parent map
def build_parent_map(repo, exe=None):
    """funclet VA -> (root parent VA, kind).  Multi-parent funclets are EXCLUDED.

    Returns (parent_of, multi, funcs, stats).  ``multi`` is {va: [parents]} for
    the ICF-folded funclets we refuse to attribute.
    """
    fcr = _load_fcr(repo)
    pe = fcr.PE(exe or (Path(repo) / "orig/45410914/band.exe"))
    funcs = fcr.parse_pdata(pe)
    stats = Counter()
    raw = defaultdict(list)
    for va, info in funcs.items():
        if not info["eh"]:
            continue
        stats["eh_functions"] += 1
        hdata = pe.u32(va - 4)
        if not hdata:
            stats["no_handler_data"] += 1
            continue
        magic = pe.u32(hdata)
        if magic is None or (magic >> 8) != 0x199305:
            stats["bad_magic"] += 1
            continue
        stats["funcinfo_ok"] += 1
        max_state = fcr.s32(pe.u32(hdata + 4)) or 0
        p_unwind = pe.u32(hdata + 8)
        n_try = pe.u32(hdata + 12) or 0
        p_try = pe.u32(hdata + 16)
        if p_unwind and max_state > 0:
            for i in range(min(max_state, 4096)):
                act = pe.u32(p_unwind + i * 8 + 4)
                if act:
                    raw[va].append(("unwind", act))
        if p_try and n_try:
            for i in range(min(n_try, 1024)):
                e = p_try + i * 20
                n_catch = pe.u32(e + 12) or 0
                p_ha = pe.u32(e + 16)
                if not p_ha:
                    continue
                for j in range(min(n_catch, 256)):
                    h = pe.u32(p_ha + j * 16 + 12)
                    if h:
                        raw[va].append(("catch", h))

    # Collect ALL parents per funclet (unlike funclet_cascade_rank's setdefault).
    direct: dict[int, set[int]] = defaultdict(set)
    kind_of: dict[int, str] = {}
    for p, kids in raw.items():
        for kind, k in kids:
            if k != p:
                direct[k].add(p)
                kind_of.setdefault(k, kind)
    multi = {k: sorted(v) for k, v in direct.items() if len(v) > 1}

    def root(va, seen=None):
        seen = seen or set()
        while va in direct and va not in seen and len(direct[va]) == 1:
            seen.add(va)
            va = next(iter(direct[va]))
        return va

    parent_of = {}
    for k, ps in direct.items():
        if len(ps) != 1:
            continue  # ICF fold: refuse to attribute
        r = root(next(iter(ps)))
        if r != k:
            parent_of[k] = (r, kind_of[k])
    stats["funclets_total"] = len(direct)
    stats["funclets_unique_parent"] = len(parent_of)
    stats["funclets_multi_parent"] = len(multi)
    return parent_of, multi, funcs, stats


# ------------------------------------------------------------------- splits
def load_text_spans(splits):
    """[(start, end, unit_full_path_header)] sorted.  NEVER keyed by basename."""
    units = defaultdict(lambda: defaultdict(list))
    cur = None
    for ln in open(splits):
        if not ln.strip() or ln.strip().startswith("#"):
            continue
        if not ln[0].isspace():
            m = re.match(r"^(\S+):", ln.strip())
            if m:
                cur = m.group(1)
            continue
        m = re.match(r"\s*(\.\w+)\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)", ln)
        if m and cur:
            units[cur][m.group(1)].append((int(m.group(2), 16), int(m.group(3), 16)))
    spans = []
    for u, secs in units.items():
        for s, e in secs.get(".text", []):
            spans.append((s, e, u))
    spans.sort()
    return spans


class UnitIndex:
    def __init__(self, spans):
        self.spans = spans
        self.starts = [s[0] for s in spans]

    def __call__(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        if i >= 0 and self.spans[i][0] <= va < self.spans[i][1]:
            return self.spans[i][2]
        return None

    def gaps(self, lo=None, hi=None):
        out = []
        prev = None
        for s, e, u in self.spans:
            if prev is not None and s > prev[1]:
                out.append((prev[1], s, prev[2], u))
            if prev is None or e > prev[1]:
                prev = (s, e, u)
        return out


def is_vendor(va):
    return VENDOR_LO <= va < VENDOR_HI


# ------------------------------------------------------------------ queries
class Oracle:
    def __init__(self, repo: Path, splits: Path | None = None, exe: Path | None = None):
        self.repo = repo
        self.parent_of, self.multi, self.funcs, self.stats = build_parent_map(repo, exe)
        self.splits = splits or (repo / "config/45410914/splits.txt")
        self.idx = UnitIndex(load_text_spans(self.splits))
        self.all_units = sorted({u for _, _, u in self.idx.spans})

    def verdict(self, va):
        """Hard owner for one function VA, or a reason it has none."""
        pk = self.parent_of.get(va)
        if pk is None:
            if va in self.multi:
                return {"va": va, "status": "ICF_MULTI_PARENT",
                        "parents": ["0x%08X" % p for p in self.multi[va]]}
            return {"va": va, "status": "NO_EH_PARENT"}
        p, kind = pk
        pu = self.idx(p)
        if pu is None:
            return {"va": va, "status": "PARENT_UNPINNED",
                    "parent": "0x%08X" % p, "kind": kind}
        return {"va": va, "status": "OWNER", "owner": pu,
                "parent": "0x%08X" % p, "kind": kind}

    def span(self, lo, hi):
        """THE primary query: for an unclaimed span, which units can own it?"""
        fns = sorted(va for va in self.funcs if lo <= va < hi)
        owners, rows = Counter(), []
        n_none = Counter()
        for va in fns:
            v = self.verdict(va)
            v["size"] = self.funcs[va]["size"]
            rows.append(v)
            if v["status"] == "OWNER":
                owners[v["owner"]] += 1
            else:
                n_none[v["status"]] += 1
        possible = sorted(owners)
        impossible = [u for u in self.all_units if u not in owners] if owners else []
        return {
            "span": ["0x%08X" % lo, "0x%08X" % hi],
            "n_functions": len(fns),
            "n_with_owner": sum(owners.values()),
            "no_verdict": dict(n_none),
            "parent_units": dict(owners.most_common()),
            "possible_owners": possible,
            "n_impossible_owners": len(impossible),
            "verdict": ("UNIQUE:" + possible[0]) if len(possible) == 1
            else ("SPLIT:" + ",".join(possible)) if possible else "NO_EVIDENCE",
            "functions": rows,
        }


# -------------------------------------------------------------------- CLI
def cmd_census(o: Oracle, args):
    st = o.stats
    print("## parent map")
    for k in ("eh_functions", "funcinfo_ok", "bad_magic", "no_handler_data",
              "funclets_total", "funclets_unique_parent", "funclets_multi_parent"):
        print("  %-26s %7d" % (k, st[k]))
    print("  %-26s %7.2f%%" % ("unique-parent share",
                               100.0 * st["funclets_unique_parent"] / max(1, st["funclets_total"])))
    print("\n## funclet vs parent pinning (splits: %s)" % o.splits)
    c = Counter()
    for f, (p, _k) in o.parent_of.items():
        if is_vendor(f):
            c["vendor_window_skipped"] += 1
            continue
        fu, pu = o.idx(f), o.idx(p)
        if pu is None:
            c["parent_unpinned"] += 1
        elif fu is None:
            c["ACTIONABLE (funclet unpinned, parent pinned)"] += 1
        elif fu == pu:
            c["consistent"] += 1
        else:
            c["CONTRADICTION (funclet unit != parent unit)"] += 1
    for k, v in c.most_common():
        print("  %-46s %6d" % (k, v))


def cmd_span(o: Oracle, args):
    r = o.span(int(args.lo, 0), int(args.hi, 0))
    fns = r.pop("functions")
    print(json.dumps(r, indent=2))
    if args.verbose:
        for f in fns:
            print("   0x%08X %4d %s" % (f["va"], f["size"], f.get("owner", f["status"])))


def cmd_audit(o: Oracle, args):
    man = json.load(open(args.manifest))
    tiers = {k: v for k, v in man.items() if isinstance(v, list)}
    rows, tally = [], defaultdict(Counter)
    for tier, entries in tiers.items():
        for e in entries:
            va = int(e["va"], 16) if isinstance(e["va"], str) else e["va"]
            claimed = e.get("claimed_by")
            v = o.verdict(va)
            if v["status"] != "OWNER":
                tally[tier][v["status"]] += 1
                continue
            ok = v["owner"] == claimed
            tally[tier]["PROVEN" if ok else "CONTRADICTED"] += 1
            rows.append({**e, "parent": v["parent"], "parent_unit": v["owner"],
                         "verdict": "PROVEN" if ok else "CONTRADICTED"})
    for tier, c in tally.items():
        print("%-28s n=%5d  %s" % (tier, len(tiers[tier]), dict(c.most_common())))
    tot = Counter(r["verdict"] for r in rows)
    print("TOTAL decided: %s" % dict(tot))
    if args.json:
        json.dump(rows, open(args.json, "w"), indent=1)
        print("-> %s" % args.json)


def cmd_actionable(o: Oracle, args):
    """Unpinned funclets whose parent IS pinned: an evidence-backed micro-pin."""
    out = []
    for f, (p, kind) in sorted(o.parent_of.items()):
        if is_vendor(f):
            continue
        if o.idx(f) is not None:
            continue
        pu = o.idx(p)
        if pu is None:
            continue
        sz = o.funcs.get(f, {}).get("size", 0)
        out.append({"va": "0x%08X" % f, "size": sz, "kind": kind,
                    "parent": "0x%08X" % p, "owner": pu})
    print("actionable funclets: %d across %d units" %
          (len(out), len({r["owner"] for r in out})))
    print("size histogram:", dict(sorted(Counter(r["size"] for r in out).most_common())))
    print("top units:", Counter(r["owner"] for r in out).most_common(10))
    if args.json:
        json.dump(out, open(args.json, "w"), indent=1)
        print("-> %s" % args.json)


def cmd_gaps(o: Oracle, args):
    res, rows = Counter(), []
    for lo, hi, lu, ru in o.idx.gaps():
        if is_vendor(lo):
            continue
        r = o.span(lo, hi)
        r.pop("functions")
        if r["n_functions"] == 0:
            continue
        owners = set(r["parent_units"])
        if not owners:
            res["no_evidence"] += 1
            k = "NO_EVIDENCE"
        elif owners == {lu}:
            res["proves_left"] += 1
            k = "PROVES_LEFT"
        elif owners == {ru}:
            res["proves_right"] += 1
            k = "PROVES_RIGHT"
        elif owners <= {lu, ru}:
            res["split_lr"] += 1
            k = "SPLIT_LR"
        else:
            res["third_unit"] += 1
            k = "THIRD_UNIT"
        rows.append({**r, "left": lu, "right": ru, "class": k})
    for k, v in res.most_common():
        print("  %-14s %5d" % (k, v))
    if args.json:
        json.dump(rows, open(args.json, "w"), indent=1)
        print("-> %s" % args.json)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--repo", default=str(REPO))
    ap.add_argument("--splits", default=None)
    ap.add_argument("--exe", default=None)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("census")
    p = sub.add_parser("span")
    p.add_argument("lo")
    p.add_argument("hi")
    p.add_argument("-v", "--verbose", action="store_true")
    p = sub.add_parser("audit")
    p.add_argument("manifest")
    p.add_argument("--json")
    p = sub.add_parser("actionable")
    p.add_argument("--json")
    p = sub.add_parser("gaps")
    p.add_argument("--json")
    args = ap.parse_args()
    repo = Path(args.repo)
    o = Oracle(repo, Path(args.splits) if args.splits else None,
               Path(args.exe) if args.exe else None)
    {"census": cmd_census, "span": cmd_span, "audit": cmd_audit,
     "actionable": cmd_actionable, "gaps": cmd_gaps}[args.cmd](o, args)


if __name__ == "__main__":
    main()
