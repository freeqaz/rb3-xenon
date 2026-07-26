#!/usr/bin/env python3
"""order_anchored_bijection — recover fn_<VA> -> MSVC-mangled identities for
anonymous target functions by NAME-ANCHORED monotone interval bijection.

Relationship to scripts/harvest/size_order_automap.py
-----------------------------------------------------
`size_order_automap.py` ALREADY implements anchor-based interval alignment, but
its anchors come from ONE source: reloc-masked BYTE IDENTITY (a target fn whose
masked body equals exactly one compiled fn).  That anchor source is, by
construction, blind in exactly the region we care about: if our compiled body
diverges from retail's, no byte-identity anchor can exist, and the whole unit
degenerates to a single unbounded segment where the size-DP is weak.

This tool adds the anchor source that survives body divergence: **functions that
are already paired by NAME**.  A target `.fn` whose identity is already known —
either because `scripts/target_symbol_map.json` names its VA, or because dtk's
own `symbols.txt` gave it a real mangled name — and whose name is emitted by our
compiled obj, is an order anchor that required no byte agreement whatsoever.
Between two such anchors the monotone-order premise (/O1, no LTCG, TU source
order preserved in .text) forces the assignment when the interval holds exactly
k unnamed target functions and exactly k unclaimed base symbols.

So: EXTENDS size_order_automap's idea with a different, body-divergence-immune
anchor source, plus a hard k==s forcing rule (instead of a soft size DP) and a
leave-one-out precision calibration.

Sequences
    target : build/45410914/asm/<stem>.s        address order (rename-immune)
    base   : build/45410914/src/<rel>.obj       (section index, value) = emission order

Modes
    --funnel                  measure the whole funnel over every pinned unit
    --holdout                 leave-one-out precision on already-named anchors
    --emit FRAG.json          write an apply-ready target_symbol_map fragment
    --unit NAME               restrict to one unit (debug table)

Read-only.  Never writes the map; only EMITS a fragment for
scripts/harvest/tu5_map_apply_fragment.py.

MEASURED (laneAQ, 2026-07-26, main HEAD 9b2d2737, baseline 36,661 strict)
------------------------------------------------------------------------
Funnel over all 910 analysable pinned units (49,711 target .fn / 455,533 base
code symbols / 11,990 monotone name anchors):

    5,060  demand   (anon fn_, >68 B, non-vendor, not strict-100, pinned unit)
    3,962  (78.3%)  fall inside a BOUNDED anchor gap
      549           fall in a FORCED gap (k demand == k supply)
                      k=1 274 | k=2 125 | k>=3 150
    3,664           fall in an unforced (k != s) gap

Corroboration on the 274 k=1 forced:  38 size-exact, 38 within 8 B,
**198 size-CONTRADICTORY (>8 B)**, 32 size-exact AND reloc-exact.
That 72% contradiction rate is the headline: raw order-forcing on the
never-named residue is mostly wrong, and the corroborators say so.

Held-out precision (leave-one-out over 6,956 already-named interior anchors;
2,252 recovered into a forced gap):

    all forced          2133/2252 = 94.7%
    k=1                 1860/1917 = 97.0%
    k=1 + size          1754/1762 = 99.5%
    k=1 + size + reloc  1688/1690 = 99.9%     <- the gate
    any k + size+reloc  1931/1934 = 99.8%     <- tier T4, what --emit ships
    k>=2                 273/335  = 81.5%

Measured A/B of tier T4 (62 entries), two builds per leg:
    36,661 -> 36,698 = **+37 strict, 0 losses**
    band of the 62 newly-paired: 100% x37 | 99-100 x13 | 90-99 x7 | 50-90 x5
                                 | 0-50 x0 | unpaired x0
Control, same protocol, the 320 candidates the gate REJECTS:
    +5 strict, 0 losses, band: 100 x5 | 99-100 x2 | 90-99 x47 | 50-90 x151
                               | **0-50 x115**
So the size+reloc corroboration gate is load-bearing: 60% strict hit rate and
zero junk-band pairings inside it, 1.6% and 36% junk-band outside it.
"""
import argparse
import json
import re
import sys
from collections import defaultdict, Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
import size_order_automap as soa  # noqa: E402  (COFF + dtk-asm parsers)

PROJECT_ROOT = Path(__file__).resolve().parents[2]
BUILD = PROJECT_ROOT / "build" / "45410914"
MAP_PATH = PROJECT_ROOT / "scripts" / "target_symbol_map.json"
REPORT = BUILD / "report.json"

VENDOR_LO, VENDOR_HI = 0x82800000, 0x82D00000
MIN_SIZE = 68            # > 68 bytes: above the EH-funclet/thunk crumb band
                         # (docs/plans/lane-al-autocarve-2026-07-26.md)

_FN_RX = re.compile(r"^\.fn (\S+),", re.M)
_VA_RX = re.compile(r"^fn_([0-9A-Fa-f]{8})$")


# ---------------------------------------------------------------------------
# Target sequence, keeping BOTH anonymous fn_<VA> and already-named entries.
# (size_order_automap.load_target drops the named ones; we need them - they are
#  the anchors AND they consume base-side supply.)
# ---------------------------------------------------------------------------
@dataclass
class TFn:
    idx: int
    va: Optional[int]
    tokname: str            # asm .fn token
    size: int
    masked: bytes
    nmask: int              # count of relocated (zeroed) instruction words


@dataclass
class BFn:
    idx: int
    name: str
    size: int
    masked: bytes
    nmask: int


def parse_target_asm(asm_path: Path) -> List[TFn]:
    out: List[TFn] = []
    cur = None
    in_fn = False
    words: List[bytes] = []
    nmask = 0
    for line in asm_path.read_text(errors="replace").splitlines():
        fm = _FN_RX.match(line)
        if fm:
            cur = fm.group(1)
            in_fn, words, nmask = True, [], 0
            continue
        if line.startswith(".endfn"):
            if in_fn:
                body = b"".join(words)
                vm = _VA_RX.match(cur or "")
                out.append(TFn(idx=len(out),
                               va=int(vm.group(1), 16) if vm else None,
                               tokname=cur or "", size=len(body),
                               masked=body, nmask=nmask))
            in_fn = False
            continue
        if not in_fn:
            continue
        im = soa._INSTR_RX.match(line)
        if im:
            b = bytes(int(x, 16) for x in im.groups()[:4])
            if soa._operand_relocated(im.group(6)):
                b = b"\0\0\0\0"
                nmask += 1
            words.append(b)
    # address order == emission order in the dtk asm; renumber defensively by VA
    # where available (non-VA entries keep their emission slot).
    for i, t in enumerate(out):
        t.idx = i
    return out


def parse_base_obj(obj_path: Path) -> List[BFn]:
    funcs = soa._ordered_funcs(obj_path)
    out: List[BFn] = []
    for f in funcs:
        nmask = sum(1 for i in range(0, len(f["masked"]), 4)
                    if f["masked"][i:i + 4] == b"\0\0\0\0")
        out.append(BFn(idx=len(out), name=f["name"], size=f["size"],
                       masked=f["masked"], nmask=nmask))
    return out


# ---------------------------------------------------------------------------
# Unit enumeration
# ---------------------------------------------------------------------------
@dataclass
class Unit:
    name: str
    stem: str
    asm: Path
    obj: Path
    T: List[TFn] = field(default_factory=list)
    B: List[BFn] = field(default_factory=list)


def enumerate_units() -> List[Unit]:
    rep = json.loads(REPORT.read_text())
    out = []
    for u in rep.get("units", []):
        md = u.get("metadata") or {}
        if md.get("auto_generated"):
            continue
        sp = md.get("source_path")
        if not sp:
            continue
        # Unit names are EITHER a bare basename (`default/MasterAudio`, flat
        # target asm) OR a nested rel path (`default/band3/meta_band/ModifierMgr`,
        # nested target asm). Taking only the basename silently dropped 165 of
        # 912 units / 8,244 target functions — always try the full rel path
        # first, then the flat basename.
        unit_rel = u["name"][len("default/"):] if u["name"].startswith("default/") \
            else u["name"]
        stem = unit_rel.split("/")[-1]
        asm = None
        for cand in (BUILD / "asm" / (unit_rel + ".s"), BUILD / "asm" / (stem + ".s")):
            if cand.exists():
                asm = cand
                break
        rel = sp[4:] if sp.startswith("src/") else sp
        obj = BUILD / "src" / (rel.rsplit(".", 1)[0] + ".obj")
        if asm is None or not obj.exists():
            continue
        out.append(Unit(name=u["name"], stem=stem, asm=asm, obj=obj))
    return out


def strict100_vas() -> set:
    """VAs of anonymous fn_ targets objdiff already credits at strict 100.

    Renaming one of these destroys the funclet-byte-signature credit it already
    earns (measured net -25, correlator r7 2026-07-20). Hard exclusion.
    """
    vas = set()
    rep = json.loads(REPORT.read_text())
    for u in rep.get("units", []):
        for f in u.get("functions", []):
            nm = f.get("name", "")
            if nm.startswith("fn_") and (f.get("match_percent_normalized") or 0) == 100.0:
                try:
                    vas.add(int(nm[3:], 16))
                except ValueError:
                    pass
    return vas


# ---------------------------------------------------------------------------
# Anchoring + interval forcing
# ---------------------------------------------------------------------------
def _norm(n: str) -> str:
    return soa.anon_ns_strip(n)


def is_supply_eligible(name: str) -> bool:
    if soa.is_internal(name):          # __unwind$ / $-prefixed
        return False
    if name.startswith("__unwind__merged"):
        return False
    return True


@dataclass
class Interval:
    t_lo: int
    t_hi: int          # exclusive target index bounds (strictly between anchors)
    b_lo: int
    b_hi: int
    demand: List[TFn]
    supply: List[BFn]


def build_anchors(u: Unit, vamap: Dict[int, str],
                  suppress_va: Optional[int] = None
                  ) -> Tuple[List[Tuple[int, int]], Dict[str, int], set]:
    """Return (monotone anchor chain [(t_idx,b_idx)], name->b_idx, anchored base idx).

    An anchor is a target .fn whose EFFECTIVE name (map entry for its VA, or its
    own non-fn_ asm token) occurs exactly once in the base obj.
    `suppress_va` pretends that VA is unnamed (leave-one-out).
    """
    bname_idx: Dict[str, List[int]] = defaultdict(list)
    for b in u.B:
        bname_idx[_norm(b.name)].append(b.idx)

    raw: List[Tuple[int, int]] = []
    for t in u.T:
        eff = effective_name(t, vamap, suppress_va)
        if eff is None:
            continue
        cands = bname_idx.get(_norm(eff), [])
        if len(cands) == 1:
            raw.append((t.idx, cands[0]))
    # longest increasing subsequence on b_idx (patience, O(n log n))
    chain = _lis(raw)
    return chain, {k: v[0] for k, v in bname_idx.items() if len(v) == 1}, \
        set(b for _, b in raw)


def effective_name(t: TFn, vamap: Dict[int, str],
                   suppress_va: Optional[int] = None) -> Optional[str]:
    if t.va is not None:
        if suppress_va is not None and t.va == suppress_va:
            return None
        return vamap.get(t.va)
    return t.tokname or None


def _lis(pairs: List[Tuple[int, int]]) -> List[Tuple[int, int]]:
    import bisect
    if not pairs:
        return []
    pairs = sorted(pairs)
    tails: List[int] = []
    tails_idx: List[int] = []
    prev = [-1] * len(pairs)
    for i, (_, b) in enumerate(pairs):
        j = bisect.bisect_left(tails, b)
        if j == len(tails):
            tails.append(b)
            tails_idx.append(i)
        else:
            tails[j] = b
            tails_idx[j] = i
        prev[i] = tails_idx[j - 1] if j > 0 else -1
    out = []
    k = tails_idx[-1]
    while k != -1:
        out.append(pairs[k])
        k = prev[k]
    out.reverse()
    return out


def intervals(u: Unit, chain: List[Tuple[int, int]], vamap: Dict[int, str],
              anchored_b: set, suppress_va: Optional[int] = None
              ) -> List[Interval]:
    """Bounded intervals only — strictly BETWEEN two consecutive chain anchors.

    Head/tail regions are deliberately excluded: they are bounded on one side
    only, so monotone order does not force anything there.
    """
    # base names claimed by ANY named target in this unit (whether or not that
    # target anchored) may not be offered as supply — offering one would create
    # a duplicate name inside the unit.
    claimed_names = set()
    for t in u.T:
        eff = effective_name(t, vamap, suppress_va)
        if eff:
            claimed_names.add(_norm(eff))

    out = []
    for (t1, b1), (t2, b2) in zip(chain, chain[1:]):
        # SYMMETRIC size gate. laneAN measured every base-side `__unwind$` at
        # <=68 B (max exactly 68) and every `??__F` likewise; laneAL measured
        # ~86-90% of the <=68 B anonymous target pool to be the same EH/guard/
        # thunk boilerplate. Projecting BOTH ordered sequences to size > 68
        # therefore removes the funclet population from both sides at once,
        # which is what makes k==s counting meaningful. An order-preserving
        # projection of two monotonically-aligned sequences is still
        # monotonically aligned, so the forcing argument survives intact.
        dem = [t for t in u.T[t1 + 1:t2]
               if t.va is not None and t.size > MIN_SIZE
               and effective_name(t, vamap, suppress_va) is None]
        sup = [b for b in u.B[b1 + 1:b2]
               if b.size > MIN_SIZE and is_supply_eligible(b.name)
               and _norm(b.name) not in claimed_names]
        if not dem and not sup:
            continue
        out.append(Interval(t1 + 1, t2, b1 + 1, b2, dem, sup))
    return out


# ---------------------------------------------------------------------------
# Corroborators
# ---------------------------------------------------------------------------
def masked_identity(a: bytes, b: bytes) -> float:
    n = max(len(a), len(b))
    if n == 0:
        return 100.0
    same = sum(1 for i in range(min(len(a), len(b))) if a[i] == b[i])
    return 100.0 * same / n


@dataclass
class Cand:
    unit: str
    va: int
    name: str
    k: int
    tsize: int
    bsize: int
    tnmask: int
    bnmask: int
    ident: float

    @property
    def dsize(self) -> int:
        return abs(self.tsize - self.bsize)

    @property
    def size_exact(self) -> bool:
        return self.tsize == self.bsize

    @property
    def reloc_exact(self) -> bool:
        return self.tnmask == self.bnmask


def force(iv: Interval, unit: str) -> List[Cand]:
    """k==s forcing: monotone order makes the assignment unique."""
    if len(iv.demand) != len(iv.supply) or not iv.demand:
        return []
    out = []
    k = len(iv.demand)
    for t, b in zip(iv.demand, iv.supply):
        out.append(Cand(unit=unit, va=t.va, name=b.name, k=k,
                        tsize=t.size, bsize=b.size,
                        tnmask=t.nmask, bnmask=b.nmask,
                        ident=round(masked_identity(t.masked, b.masked), 1)))
    return out


# ---------------------------------------------------------------------------
# Funnel
# ---------------------------------------------------------------------------
def load_map() -> Dict[int, str]:
    raw = json.loads(MAP_PATH.read_text())
    out = {}
    for k, v in raw.items():
        if k.lower().startswith("0x") and isinstance(v, str):
            try:
                out[int(k, 16)] = v
            except ValueError:
                pass
    return out


def in_scope(va: int) -> bool:
    return not (VENDOR_LO <= va < VENDOR_HI)


def run_funnel(args):
    vamap = load_map()
    s100 = strict100_vas()
    units = enumerate_units()
    if args.unit:
        units = [u for u in units if u.stem == args.unit or u.name == args.unit]

    tot = Counter()
    all_cands: List[Cand] = []
    per_k = Counter()
    demand_in_interval = set()
    demand_all = set()

    for u in units:
        try:
            u.T = parse_target_asm(u.asm)
            u.B = parse_base_obj(u.obj)
        except Exception as e:
            tot["unit_parse_error"] += 1
            continue
        tot["units"] += 1
        tot["target_fns"] += len(u.T)
        tot["base_fns"] += len(u.B)

        # demand universe for this unit
        for t in u.T:
            if t.va is None or t.va in vamap:
                continue
            if t.size <= MIN_SIZE or not in_scope(t.va) or t.va in s100:
                continue
            demand_all.add(t.va)

        chain, _, anchored_b = build_anchors(u, vamap)
        tot["anchors"] += len(chain)
        if len(chain) >= 2:
            tot["units_with_2plus_anchors"] += 1
        for iv in intervals(u, chain, vamap, anchored_b):
            k, s = len(iv.demand), len(iv.supply)
            for t in iv.demand:
                if t.va in demand_all:
                    demand_in_interval.add(t.va)
            if k == 0:
                continue
            tot["intervals_with_demand"] += 1
            if k == s:
                per_k[min(k, 3)] += k
                all_cands.extend(force(iv, u.name))
            else:
                tot["unforced_demand"] += k

    # restrict candidates to the in-scope demand pool
    pool = [c for c in all_cands
            if c.va in demand_all]

    print("=" * 78)
    print("ORDER-ANCHORED INTERVAL BIJECTION — funnel")
    print("=" * 78)
    print(f"pinned units analysed            {tot['units']}")
    print(f"  units with >=2 name anchors    {tot['units_with_2plus_anchors']}")
    print(f"target .fn entries               {tot['target_fns']}")
    print(f"base code symbols                {tot['base_fns']}")
    print(f"name anchors (monotone chain)    {tot['anchors']}")
    print()
    print(f"DEMAND pool (anon, >{MIN_SIZE}B, non-vendor, not strict-100, pinned unit)")
    print(f"  total                          {len(demand_all)}")
    print(f"  inside a bounded anchor gap    {len(demand_in_interval)}"
          f"  ({100.0*len(demand_in_interval)/max(1,len(demand_all)):.1f}%)")
    print()
    kk = Counter(c.k for c in pool)
    print("FORCED (k demand == k supply), pool members only")
    print(f"  k=1                            {kk[1]}")
    print(f"  k=2                            {sum(v for k,v in kk.items() if k==2)}")
    print(f"  k>=3                           {sum(v for k,v in kk.items() if k>=3)}")
    print(f"  total forced                   {len(pool)}")
    print(f"  demand in unforced (k!=s) gaps {tot['unforced_demand']}")
    print()
    k1 = [c for c in pool if c.k == 1]
    print(f"CORROBORATION on the {len(k1)} k=1 forced candidates")
    print(f"  size exact                     {sum(1 for c in k1 if c.size_exact)}")
    print(f"  size within 8B                 {sum(1 for c in k1 if 0 < c.dsize <= 8)}")
    print(f"  size contradictory (>8B)       {sum(1 for c in k1 if c.dsize > 8)}")
    print(f"  reloc-count exact              {sum(1 for c in k1 if c.reloc_exact)}")
    print(f"  size exact AND reloc exact     "
          f"{sum(1 for c in k1 if c.size_exact and c.reloc_exact)}")
    print(f"  masked identity >= 50%         {sum(1 for c in k1 if c.ident >= 50)}")
    print()
    for label, sel in (("T1 k=1 + size exact + reloc exact",
                        [c for c in k1 if c.size_exact and c.reloc_exact]),
                       ("T2 k=1 + size exact",
                        [c for c in k1 if c.size_exact]),
                       ("T3 k<=2 + size exact + reloc exact",
                        [c for c in pool if c.k <= 2 and c.size_exact and c.reloc_exact]),
                       ("T4 any k + size exact + reloc exact",
                        [c for c in pool if c.size_exact and c.reloc_exact])):
        print(f"  {label:<42} {len(sel)}")

    if args.dump:
        Path(args.dump).write_text(json.dumps(
            [c.__dict__ for c in pool], indent=1) + "\n")
        print(f"\n[dump] {len(pool)} candidates -> {args.dump}")
    return pool


# ---------------------------------------------------------------------------
# Leave-one-out held-out precision
# ---------------------------------------------------------------------------
def run_holdout(args):
    vamap = load_map()
    units = enumerate_units()
    if args.unit:
        units = [u for u in units if u.stem == args.unit or u.name == args.unit]

    res = Counter()
    tiers = defaultdict(lambda: [0, 0])   # tier -> [correct, total]
    wrong_examples = []

    for u in units:
        try:
            u.T = parse_target_asm(u.asm)
            u.B = parse_base_obj(u.obj)
        except Exception:
            continue
        chain, _, anchored_b = build_anchors(u, vamap)
        if len(chain) < 3:
            continue
        chain_t = {t: b for t, b in chain}
        tset = [t for t, _ in chain]
        # hold out interior anchors one at a time
        for pos in range(1, len(chain) - 1):
            t_idx, b_idx = chain[pos]
            tf = u.T[t_idx]
            if tf.va is None:
                continue
            truth = u.B[b_idx].name
            if tf.size <= MIN_SIZE or not in_scope(tf.va):
                continue
            res["held_out"] += 1
            chain2, _, ab2 = build_anchors(u, vamap, suppress_va=tf.va)
            got = None
            for iv in intervals(u, chain2, vamap, ab2, suppress_va=tf.va):
                if not (iv.t_lo <= t_idx < iv.t_hi):
                    continue
                cands = force(iv, u.name)
                for c in cands:
                    if c.va == tf.va:
                        got = c
                break
            if got is None:
                res["not_forced"] += 1
                continue
            res["forced"] += 1
            ok = _norm(got.name) == _norm(truth)
            res["correct" if ok else "wrong"] += 1
            for tier, cond in (("k=1", got.k == 1),
                               ("k=1+size", got.k == 1 and got.size_exact),
                               ("k=1+size+reloc",
                                got.k == 1 and got.size_exact and got.reloc_exact),
                               ("k>=2", got.k >= 2),
                               ("any+size+reloc",
                                got.size_exact and got.reloc_exact),
                               ("all", True)):
                if cond:
                    tiers[tier][1] += 1
                    if ok:
                        tiers[tier][0] += 1
            if not ok and len(wrong_examples) < 25:
                wrong_examples.append(
                    (u.name, f"0x{tf.va:08X}", got.k, got.tsize, got.bsize,
                     got.name[:44], truth[:44]))

    print("=" * 78)
    print("HELD-OUT PRECISION (leave-one-out over already-named interior anchors)")
    print("=" * 78)
    print(f"anchors held out (>{MIN_SIZE}B, in-scope)   {res['held_out']}")
    print(f"  recovered into a forced interval    {res['forced']}")
    print(f"  fell in an unforced interval        {res['not_forced']}")
    print()
    for tier in ("all", "k=1", "k=1+size", "k=1+size+reloc", "k>=2",
                 "any+size+reloc"):
        c, n = tiers[tier]
        if n:
            print(f"  precision[{tier:<16}] {c}/{n} = {100.0*c/n:.1f}%")
    if wrong_examples:
        print("\n  sample MISPAIRS:")
        for e in wrong_examples:
            print(f"    {e[0]:<28} {e[1]} k={e[2]} tsz={e[3]} bsz={e[4]} "
                  f"got={e[5]} want={e[6]}")
    return tiers


# ---------------------------------------------------------------------------
# Emit
# ---------------------------------------------------------------------------
def run_emit(args):
    pool = run_funnel(args)
    raw = json.loads(MAP_PATH.read_text())
    keys = set(k.lower() for k in raw if k.lower().startswith("0x"))
    vals = set(v for v in raw.values() if isinstance(v, str))

    def tier_ok(c: Cand) -> bool:
        if args.tier == "T1":
            return c.k == 1 and c.size_exact and c.reloc_exact
        if args.tier == "T2":
            return c.k == 1 and c.size_exact
        if args.tier == "T3":
            return c.k <= 2 and c.size_exact and c.reloc_exact
        return c.size_exact and c.reloc_exact

    frag: Dict[str, str] = {}
    used = set()
    skipped = Counter()
    for c in sorted(pool, key=lambda x: x.va):
        if not tier_ok(c):
            continue
        key = f"0x{c.va:08x}"
        if key in keys:
            skipped["addr_already_mapped"] += 1
            continue
        if c.name in vals or c.name in used:
            skipped["name_collision"] += 1
            continue
        frag[key] = c.name
        used.add(c.name)
    Path(args.emit).write_text(json.dumps(frag, indent=1) + "\n")
    print(f"\n[emit] tier={args.tier}  {len(frag)} entries -> {args.emit}")
    for k, v in skipped.items():
        print(f"  skip {k}: {v}")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--funnel", action="store_true")
    ap.add_argument("--holdout", action="store_true")
    ap.add_argument("--emit", help="write map fragment here")
    ap.add_argument("--tier", default="T1", choices=["T1", "T2", "T3", "T4"])
    ap.add_argument("--unit", help="restrict to one unit stem")
    ap.add_argument("--dump", help="dump candidate JSON")
    args = ap.parse_args()
    if args.emit:
        run_emit(args)
    elif args.holdout:
        run_holdout(args)
    else:
        run_funnel(args)


if __name__ == "__main__":
    main()
