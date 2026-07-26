#!/usr/bin/env python3
"""Unit-scoped reloc-masked byte-identity resolution for UNNAMED EH parents.

Background
----------
``scripts/harvest/homing_scan.py`` homes one of our compiled functions on a
retail VA by reloc-masked byte identity, searching the *whole binary*.  When a
function's masked bytes are identical at several retail VAs it reports ``MULTI``
and refuses -- correctly, because a wrong pick is invisible in the score.

This tool attacks the same identity question from the other end.  Instead of
"where in the binary could this symbol of ours live?", it asks

    "this specific retail VA is an unnamed exception-handling parent inside a
     pinned unit; which of the symbols that unit compiles is byte-identical to
     it?"

That is a much smaller, unit-scoped candidate space, so a global ``MULTI`` can
still resolve here: the extra evidence is the pin (VA -> unit, from splits.txt's
``.pdata`` ranges) plus the EH-parent constraint, neither of which is derived
from the function's own bytes.

Honesty model
-------------
* A proposal is emitted ONLY when exactly one of the unit's candidate symbols is
  reloc-masked byte-identical at the parent VA.  Ties are REFUSED and counted.
* Byte identity is checked at the retail VA with the function length taken from
  ``.pdata``, masking OUR relocation offsets on BOTH sides -- the same rule
  ``homing_scan.masked_eq`` uses.
* ``--validate`` measures precision HELD-OUT: it runs over parents that are
  ALREADY named, hides the truth from the resolver, and scores the pick.

Measured caveat (lane L, 2026-07-26)
------------------------------------
Naming a parent does NOT cascade its EH funclets.  Two whole-binary A/B
experiments (removing 116 frame-mismatch parent names: 0 change; removing 60
frame-OK parent names with 1,446 matched funclets between them: -47, and all 47
losses were the parent symbols themselves, zero funclets) show funclet pairing
is independent of the parent's map entry.  So the payoff of a naming wave is
exactly "parents whose body is already byte-identical" -- which is precisely
what this tool finds, and why it gates on byte identity rather than on softer
evidence.

Usage
-----
    python3 scripts/harvest/unnamed_parent_verify.py --worktree $PWD --census
    python3 scripts/harvest/unnamed_parent_verify.py --worktree $PWD --validate
    python3 scripts/harvest/unnamed_parent_verify.py --worktree $PWD \
        --propose --out /home/free/tmp/laneL_prop.json --frag /home/free/tmp/laneL_frag.json
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import homing_scan as HS  # noqa: E402  (parse_band / band_bytes / extract_funcs / masked_eq)
import funclet_cascade_rank as FCR  # noqa: E402

FUNCLET_PREFIXES = ("__unwind", "__ehhandler", "__catch", "__tls", "__GSHandler")


# --------------------------------------------------------------------------- io
def load_map(path: Path):
    """-> (va:int -> name, set of names already used as a map value)."""
    raw = json.load(open(path))
    by_va, values = {}, set()
    for k, v in raw.items():
        if k.startswith("_"):
            continue
        try:
            by_va[int(k, 16)] = v
        except ValueError:
            continue
        values.add(v)
    return by_va, values


def unit_objs(repo: Path):
    """report.json unit name -> compiled .obj path (via the unit's source_path)."""
    rep = json.load(open(repo / "build/45410914/report.json"))
    objdir = repo / "build/45410914/src"
    out = {}
    for u in rep["units"]:
        sp = (u.get("metadata") or {}).get("source_path")
        if not sp:
            continue
        p = objdir / (sp[4:] if sp.startswith("src/") else sp)
        out[u["name"]] = p.with_suffix(".obj")
    return out


def parent_rows(repo: Path, exe: Path | None):
    """Ranked funclet-parent rows from funclet_cascade_rank.build()."""
    rows, census, ehstats = FCR.build(repo, exe)
    return rows


# ---------------------------------------------------------------------- resolve
class Resolver:
    def __init__(self, repo: Path, exe: Path | None = None, strict_unique: bool = False):
        self.repo = repo
        self.strict_unique = strict_unique
        self.max_cands = 0
        HS.BAND = str(exe or repo / "orig/45410914/band.exe")
        HS.TMAP = str(repo / "scripts/target_symbol_map.json")
        self.data, self.secs, self.ents = HS.parse_band()
        self.size_at = {va: sz for va, sz in self.ents}
        self.map_by_va, self.map_values = load_map(repo / "scripts/target_symbol_map.json")
        self.objs = unit_objs(repo)
        self._fn_cache: dict[Path, dict] = {}
        # size -> [(va, raw retail bytes)] for the global-uniqueness guard
        self.by_size: dict[int, list] = defaultdict(list)
        for va, sz in self.ents:
            b = HS.band_bytes(self.data, self.secs, va, sz)
            if b is not None:
                self.by_size[sz].append((va, b))
        # VA -> owning unit, from splits.txt's pinned .pdata ranges (survives
        # COMDAT scatter: each 8-byte entry names exactly one function VA)
        pe = FCR.PE(exe or repo / "orig/45410914/band.exe")
        units = FCR.parse_splits(repo / "config/45410914/splits.txt")
        raw_va2unit, _ = FCR.unit_function_vas(pe, units)
        # splits.txt keys are 'Foo.cpp' / a path; report keys are 'default/Foo'
        report_units = set(self.objs)
        xlat = {u: FCR.unit_report_key(u, report_units) for u in units}
        self.va2unit = {va: xlat.get(u) for va, u in raw_va2unit.items()}
        self._unit_syms: dict[str, set] = {}

    # ---- global uniqueness guard -----------------------------------------
    @staticmethod
    def _segments(size: int, offs):
        """Byte ranges of a function body NOT covered by a 4-byte reloc slot."""
        masked = set()
        for o in offs:
            for b in range(4):
                if o + b < size:
                    masked.add(o + b)
        segs, start = [], None
        for i in range(size):
            if i in masked:
                if start is not None:
                    segs.append((start, i))
                    start = None
            elif start is None:
                start = i
        if start is not None:
            segs.append((start, size))
        return segs

    def global_hits(self, body: bytes, offs) -> list:
        """Every retail VA whose bytes are reloc-masked-equal to `body`."""
        size = len(body)
        segs = self._segments(size, offs)
        if not segs:
            return []
        s0, e0 = segs[0]
        probe = body[s0:e0]
        out = []
        for va, raw in self.by_size.get(size, ()):
            if raw[s0:e0] != probe:
                continue
            if all(raw[s:e] == body[s:e] for s, e in segs[1:]):
                out.append(va)
        return out

    def unit_compiles(self, unit_key: str, sym: str) -> bool:
        if unit_key not in self._unit_syms:
            obj = self.objs.get(unit_key)
            self._unit_syms[unit_key] = set(self.funcs(obj)) if obj else set()
        return sym in self._unit_syms[unit_key]

    def home_is_unique(self, sym: str, body: bytes, offs, va: int):
        """The guard.

        Among every retail VA byte-identical to `sym`, count those that are
        (a) not already claimed by another name in the map and (b) pinned to a
        unit that actually compiles `sym`.  Exactly one such VA -- and it must
        be `va` -- or we refuse.  This is what kills the observed failure mode:
        the true owner's body has diverged, and some *other* function of the
        same unit happens to be byte-identical to these retail bytes.  That
        impostor is byte-identical at its own home too, so it shows two
        compiling-unit homes and is refused.
        """
        hits = self.global_hits(body, offs)
        live = []
        for h in hits:
            if h != va and h in self.map_by_va:
                continue
            u = self.va2unit.get(h)
            if u is None:
                continue
            if self.unit_compiles(u, sym):
                live.append(h)
        return (len(live) == 1 and live[0] == va), hits, live

    def funcs(self, obj: Path):
        if obj not in self._fn_cache:
            self._fn_cache[obj] = HS.extract_funcs(obj) if obj.exists() else {}
        return self._fn_cache[obj]

    def candidates(self, unit: str, exclude_values: bool = True):
        """Our compiled symbols in `unit` that are legal naming candidates."""
        obj = self.objs.get(unit)
        if obj is None:
            return {}
        out = {}
        for name, (body, offs) in self.funcs(obj).items():
            if any(p in name for p in FUNCLET_PREFIXES):
                continue
            if exclude_values and name in self.map_values:
                continue
            out[name] = (body, offs)
        return out

    def resolve_va(self, va: int, unit: str, extra_allow: str | None = None):
        """-> (verdict, winner, competitors, n_candidates_of_right_size)."""
        size = self.size_at.get(va)
        if size is None:
            return "NO-PDATA", None, [], 0
        band = HS.band_bytes(self.data, self.secs, va, size)
        if band is None:
            return "NO-BYTES", None, [], 0
        cands = self.candidates(unit)
        if extra_allow is not None:
            obj = self.objs.get(unit)
            if obj is not None:
                f = self.funcs(obj).get(extra_allow)
                if f is not None:
                    cands[extra_allow] = f
        same_size = {n: v for n, v in cands.items() if len(v[0]) == size}
        hits = [n for n, (body, offs) in same_size.items() if HS.masked_eq(body, band, offs)]
        if not hits:
            return ("NO-BYTE-MATCH" if same_size else "NO-CANDIDATE"), None, [], len(same_size)
        if len(hits) > 1:
            return "TIE", None, sorted(hits), len(same_size)
        win = hits[0]
        body, offs = same_size[win]
        ok, ghits, live = self.home_is_unique(win, body, offs, va)
        if not ok:
            return "AMBIGUOUS-HOME", None, ["0x%08x" % h for h in live], len(same_size)
        if self.strict_unique and len(ghits) != 1:
            return "ICF-TWIN", None, ["0x%08x" % h for h in ghits], len(same_size)
        if self.max_cands and len(same_size) > self.max_cands:
            return "OVER-CAP", None, [], len(same_size)
        return "RESOLVED", win, [], len(same_size)


# ------------------------------------------------------------------------- main
def unnamed_pinned(rows):
    return [r for r in rows
            if "UNNAMED" in (r.get("flags") or [])
            and "PARENT_UNPINNED" not in (r.get("flags") or [])]


def named_pinned(rows):
    return [r for r in rows
            if "UNNAMED" not in (r.get("flags") or [])
            and "PARENT_UNPINNED" not in (r.get("flags") or [])]


def all_pinned(R, named: bool):
    """Every pinned function VA, EH parent or not, as ranked-row-shaped dicts.

    The unit-scoped byte-identity argument has nothing to do with exception
    handling -- it applies to any VA whose owning unit is pinned.  `--all`
    widens the pool from the 1,809 unnamed EH parents to every unnamed pinned
    function, and `--validate --all` measures precision on the same wider
    population instead of extrapolating from the EH slice.
    """
    out = []
    for va, unit in R.va2unit.items():
        if unit is None:
            continue
        is_named = va in R.map_by_va
        if is_named != named:
            continue
        if R.size_at.get(va, 0) < 32:
            continue
        out.append(dict(parent_va="0x%08X" % va, parent_unit=unit,
                        funclets=0, funclets_unmatched=0))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--worktree", required=True, type=Path)
    ap.add_argument("--exe", type=Path, default=None)
    ap.add_argument("--census", action="store_true")
    ap.add_argument("--validate", action="store_true",
                    help="held-out precision over already-named parents")
    ap.add_argument("--propose", action="store_true")
    ap.add_argument("--out", type=Path, default=None,
                    help="span_predictor-shaped {tu: [records]} proposals")
    ap.add_argument("--frag", type=Path, default=None,
                    help="flat {\"0xVA\": name} fragment for tu5_map_apply_fragment.py")
    ap.add_argument("--max-cands", type=int, default=0,
                    help="refuse when the unit offers more than N same-size candidates "
                         "(1 is the measured 98.67%% operating point over all pinned VAs)")
    ap.add_argument("--all", action="store_true",
                    help="widen the pool from unnamed EH parents to ALL unnamed pinned VAs")
    ap.add_argument("--strict-unique", action="store_true",
                    help="refuse any winner that has an ICF twin anywhere in the image")
    ap.add_argument("--limit", type=int, default=0)
    a = ap.parse_args()

    repo = a.worktree.resolve()
    rows = parent_rows(repo, a.exe)
    R = Resolver(repo, a.exe, strict_unique=a.strict_unique)
    R.max_cands = a.max_cands

    if a.census:
        un = unnamed_pinned(rows)
        print(f"* ranked rows                 = {len(rows)}")
        print(f"* unnamed pinned parents      = {len(un)}")
        print(f"* funclets behind them        = {sum(r['funclets'] for r in un)}")
        print(f"* unmatched funclets          = {sum(r['funclets_unmatched'] for r in un)}")
        units = {r["parent_unit"] for r in un}
        print(f"* distinct units              = {len(units)}")
        have = sum(1 for u in units if R.objs.get(u) and R.objs[u].exists())
        print(f"* units with a compiled obj   = {have}")

    if a.validate:
        tgt = all_pinned(R, named=True) if a.all else named_pinned(rows)
        if a.limit:
            tgt = tgt[:a.limit]
        stat = defaultdict(int)
        buckets = defaultdict(lambda: [0, 0])
        cbuckets = defaultdict(lambda: [0, 0])
        hit = miss = 0
        misses = []
        for r in tgt:
            va = int(r["parent_va"], 16)
            truth = R.map_by_va.get(va)
            if truth is None:
                stat["no-truth"] += 1
                continue
            # Hide the truth: `truth` is a map VALUE so candidates() drops it;
            # re-admit it explicitly so the resolver sees it as one competitor
            # among the unit's unmapped symbols, exactly as in production.
            v, win, comp, n = R.resolve_va(va, r["parent_unit"], extra_allow=truth)
            stat[v] += 1
            if v == "RESOLVED":
                sz = R.size_at.get(va, 0)
                b = ("<64" if sz < 64 else "64-127" if sz < 128 else
                     "128-255" if sz < 256 else "256-511" if sz < 512 else ">=512")
                buckets[b][1] += 1
                nc = n
                cb = ("cand1" if nc <= 1 else "cand2-4" if nc <= 4 else
                      "cand5-16" if nc <= 16 else "cand17+")
                cbuckets[cb][1] += 1
                if win == truth:
                    hit += 1
                    buckets[b][0] += 1
                    cbuckets[cb][0] += 1
                else:
                    miss += 1
                    misses.append((r["parent_va"], truth, win))
        tot = hit + miss
        print("\n## held-out precision (already-named pinned parents)")
        for k in sorted(stat):
            print(f"  {k:<16} {stat[k]}")
        if tot:
            print(f"  PRECISION        {hit}/{tot} = {100.0*hit/tot:.2f}%")
        else:
            print("  PRECISION        n/a (no resolvable held-out cases)")
        print("  -- by retail function size --")
        for k in ("<64", "64-127", "128-255", "256-511", ">=512"):
            h, t = buckets[k]
            if t:
                print(f"    {k:<10} {h}/{t} = {100.0*h/t:.2f}%")
        print("  -- by in-unit same-size candidate count --")
        for k in ("cand1", "cand2-4", "cand5-16", "cand17+"):
            h, t = cbuckets[k]
            if t:
                print(f"    {k:<10} {h}/{t} = {100.0*h/t:.2f}%")
        for m in misses[:8]:
            print(f"    MISS {m[0]} truth={m[1]} pick={m[2]}")

    if a.propose:
        un = all_pinned(R, named=False) if a.all else unnamed_pinned(rows)
        if a.limit:
            un = un[:a.limit]
        stat = defaultdict(int)
        props = defaultdict(list)
        frag = {}
        taken = set()
        for r in un:
            va = int(r["parent_va"], 16)
            v, win, comp, n = R.resolve_va(va, r["parent_unit"])
            stat[v] += 1
            if v != "RESOLVED":
                continue
            if win in taken or win in R.map_values or va in R.map_by_va:
                stat["COLLISION"] += 1
                continue
            taken.add(win)
            frag["0x%08x" % va] = win
            props[r["parent_unit"]].append(dict(
                name=win, va="0x%08x" % va, size=R.size_at.get(va, 0),
                cls="UNIT-SCOPED-BYTE", n_hits=1, hits=["0x%08x" % va],
                funclets=r["funclets"], funclets_unmatched=r["funclets_unmatched"]))
        print("\n## proposals over unnamed pinned parents")
        for k in sorted(stat):
            print(f"  {k:<16} {stat[k]}")
        print(f"  emitted          {len(frag)}")
        if a.out:
            json.dump(props, open(a.out, "w"), indent=1)
            print(f"  wrote {a.out}")
        if a.frag:
            json.dump(frag, open(a.frag, "w"), indent=1)
            print(f"  wrote {a.frag}")


if __name__ == "__main__":
    main()
