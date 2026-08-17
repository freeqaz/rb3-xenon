#!/usr/bin/env python3
"""The PAIRING channel, modelled -- W20's bound turned into a prediction.

WHY THIS EXISTS
---------------
Lane W20-CASCADE ran a round trip on a map edit and split the measured delta
into two mechanisms that share no cause:

    CASCADE  4 rows  -580 B   call-site relocation-name charges (priced exactly
                              by tools/cascade_price.py, fixture-validated)
    PAIRING 10 rows -2396 B   the row UN-PAIRS -- it stops being compared at all

=> the cascade is 19.5% of the delta; PAIRING is 80.5%.  W20 could only BOUND
the pairing term (2,520 B of candidates against 2,396 B recovered) because it
priced every edited row at its full size.  This module makes the term exact
where it is exact, and labels the rest as bounded.

THE MECHANISM (one sentence)
----------------------------
objdiff pairs target<->base BY NAME within a unit, so if the base obj behind the
pin cannot DEFINE the name the target obj carries, the row reads 0% however
correct our source is.  That is W9's measured -180 B failure mode; W20 showed it
is the dominant term, not an edge case.

THE MODEL
---------
For an edited address A (old name O -> new name N), pinned in unit U whose
compiled base obj is B, with the current report row (size S, fuzzy F, mpn M):

  R1  B cannot define N  (or N is a placeholder)
        => the row UN-PAIRS.  DETERMINED, no body diff needed:
             d_bytes -= S  if F == 100      (an uncredited row costs no bytes)
             d_fns   -= 1  if M == 100
  R2  B can define N
        => the row RE-PAIRS against a different COMDAT.  Its new score depends
           on the BODY, which cannot be diffed until the tree re-splits.
           BOUNDED, never a point estimate.
  R3  callers -> tools/cascade_price.py (a separate mechanism; do not merge the
      two, or you credit the right bytes to the wrong cause).

★ THE REFINEMENT THAT MATTERS, and the one W20's raw bound missed: un-pairing a
row that is NOT currently at fuzzy 100 costs ZERO BYTES -- it was never
credited.  It still costs a matched_function if mpn == 100.  Because `mpn`
excludes relocation-name penalties, those two conditions come apart routinely,
so bytes and functions MUST be modelled on their own rulers.

KNOWN-ANSWER TEST  (`validate`, no build required)
--------------------------------------------------
W20's round trip is the inverse of commit 7e9c2d01's map edit, whose 11 rows are
frozen in docs/decomp/w17-cascade-fixture.json.  The tree ships in the post-W17
state, so the inverse can be priced statically and scored against W20's measured
answer:

    DETERMINED un-pairing   9 rows / -2,316 B
    BOUNDED    (re-pairs)   1 row  /    80 B   (0x82597098, SongSortMgr)
    ---------------------------------------------------------------
    bracket             [-2,316, -2,396]   W20 MEASURED  -2,396   (contained)
    d_fns  DETERMINED          -10         W20 MEASURED     -10   (exact)

The bounded row is explained, not fitted: SongSortMgr.obj defines BOTH `clear`
spellings and carries a distinct `_M_erase` per value type, so reverting the
name re-pairs the row against a COMDAT whose callee NAME differs -- a diff_arg
reloc-name charge, which drops `fuzzy` while leaving `mpn` at 100.  Hence
-80 B / -0 fns, and hence the bracket closing exactly on the measured total.

`validate --self-break` drops the fuzzy==100 test from R1 to prove the fixture
can FAIL.  A regression test that passes under a broken model proves nothing.

THE INVERSE  (`rehome-census`)
------------------------------
If rows un-pair when the obj cannot define a name, are there rows ALREADY at 0%
that WOULD pair if re-homed to an obj that can define them?  The asymmetry is
what makes this attractive: such a row is already scoring zero, so re-homing it
cannot LOSE bytes on that row -- the downside is bounded at 0 and the upside is
its full size.  That is the opposite risk profile to renaming.

⚠ Re-homing is NOT metric-neutral (CLAUDE.md / PINHOME-1: +3 fns / +428 B
measured) precisely BECAUSE it changes which base obj is consulted.  Adding a
pin over unpinned auto_* code is neutral; this is not that.

Usage:
    python3 tools/pairing_model.py validate [--self-break] [-C <root>]
    python3 tools/pairing_model.py price --fixture <json> [--inverse]
    python3 tools/pairing_model.py rehome-census [--json out.json]
"""
import argparse
import json
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cascade_price as cp          # noqa: E402  loaders + can_define + splits
import coff_owned                   # noqa: E402

VERSION = "45410914"

# Anonymous-namespace hashes are normalized away before objdiff pairs symbols.
NORM_ANON = re.compile(r"\?A0x[0-9a-f]+")

# W20's measured answer for the round trip. Frozen; see docs/decomp/w20-cascade.md.
W20_MEASURED_PAIRING_BYTES = -2396      # graded (name_check) -- the shipped ruler
W20_MEASURED_NONE_BYTES = -2520         # functionRelocDiffs=none control
W20_MEASURED_DFNS = -10                 # matched_functions is RULER-INVARIANT


def credit_at_ruler(root, names_units, selector):
    """fuzzy per row ON A GIVEN RULER.

    ★ THE SEPARATION THIS LANE IS BUILT ON.  Two things decide an un-pairing
    row's byte cost and only ONE of them is ruler-dependent:

      * the UN-PAIRING VERDICT (does the pinned obj define the name?) is a COFF
        fact -- RULER-INVARIANT.  `none` forgives relocation NAMES; it does NOT
        forgive an ABSENT BASE SYMBOL, so an un-paired row reads 0 on every
        ruler.
      * the CREDIT TEST (is this row credited TODAY?) is ruler-dependent: a row
        charged only by relocation names sits below 100 graded and at exactly
        100 under `none`.  Measured here on 0x82456190: graded 99.70588,
        none 100.0.

    Conflating the two is how a pairing change gets mistaken for name
    forgiveness.  A FABRICATED ALIAS leaves `none` FLAT (it only ever moves
    relocation-name arguments); a PAIRING change MOVES `none`.  Same direction,
    different mechanism -- so the two rulers are predicted separately, never
    assumed equal.
    """
    if selector == "graded":
        return None                     # report.json already carries it
    rargs, _lbl = cp.ruler_args(root, selector)
    out = {}
    for nm, unit in names_units:
        d, _err = cp.run_diff(root, nm, unit, rargs,
                              cache_dir=f"/tmp/claude/w24_{selector}")
        if d is not None:
            out[nm] = float(d.get("fuzzy_match_percent", 0.0) or 0.0)
    return out



# ---------------------------------------------------------------------------
# R2a -- ADJUDICATE a re-pairing row, so the bracket collapses to a prediction
#
# When the obj CAN define the new name the row re-pairs against a DIFFERENT
# COMDAT, and W20 could only bound the outcome because "the body decides".  But
# the body is available statically: compare the two COMDATs in our own obj with
# every relocation-patched word MASKED.  (Raw memcmp is vacuous -- PC-relative
# displacements differ at different addresses; CLAUDE.md's ICF section makes the
# same point.)
#
#   bodies differ                -> genuinely different code: stays BOUNDED.
#   bodies equal, reloc TARGETS equal
#                                -> indistinguishable on every ruler: cost 0.
#   bodies equal, reloc TARGETS differ
#                                -> the ONLY difference is a relocation NAME:
#                                   `none` ignores it  => cost 0
#                                   `name_check` charges it => the row drops off
#                                   fuzzy 100 (cost = size) but KEEPS mpn 100,
#                                   because mpn excludes argument penalties.
#
# That last cell is why the two rulers must be predicted separately, and it is
# exactly the 80 B by which the graded and `none` answers differ on the W20
# fixture (SongSortMgr's two `clear` COMDATs: 80 B, 1 reloc, reloc-normalized
# IDENTICAL, differing only in _M_erase<...int> vs _M_erase<...SetlistRecord>).
# ---------------------------------------------------------------------------

def _comdat_bodies(path, names):
    """{name: (masked_body, [reloc target names])} for COMDATs in one obj."""
    d = open(path, "rb").read()
    _m, nsec, _t, psym, nsym, opt, _c = struct.unpack_from("<HHIIIHH", d, 0)
    stro = psym + nsym * 18
    strt = d[stro:]

    def nm(off):
        raw = d[off:off + 8]
        if raw[:4] == b"\x00\x00\x00\x00":
            o = struct.unpack_from("<I", raw, 4)[0]
            e = strt.find(b"\x00", o)
            return strt[o:e].decode("latin-1") if e >= 0 else ""
        return raw.rstrip(b"\x00").decode("latin-1")

    secs = []
    for i in range(nsec):
        o = 20 + opt + i * 40
        secs.append((struct.unpack_from("<I", d, o + 16)[0],
                     struct.unpack_from("<I", d, o + 20)[0],
                     struct.unpack_from("<I", d, o + 24)[0],
                     struct.unpack_from("<H", d, o + 32)[0]))
    symname, want = {}, {}
    i = 0
    while i < nsym:
        o = psym + i * 18
        n = nm(o)
        symname[i] = n
        if n in names:
            want[n] = struct.unpack_from("<h", d, o + 12)[0]
        i += 1 + d[o + 17]

    out = {}
    for n, sec in want.items():
        if sec <= 0 or sec > len(secs):
            continue
        sz, pdata, preloc, nrel = secs[sec - 1]
        body = bytearray(d[pdata:pdata + sz])
        tgts = []
        for r in range(nrel):
            ro = preloc + r * 10
            va = struct.unpack_from("<I", d, ro)[0]
            si = struct.unpack_from("<I", d, ro + 4)[0]
            tgts.append(symname.get(si, "?"))
            body[va:va + 4] = b"\xAA\xAA\xAA\xAA"
        out[n] = (bytes(body), tgts)
    return out


def adjudicate_repair(root, unit_src, old, new, unit_name=None):
    """(verdict, none_cost_factor, graded_cost_factor) for a RE-PAIRS row.

    Factors multiply the row's size; None means UNDETERMINED (stay bounded).
    """
    bp = cp.base_obj_for_unit(root, unit_src, unit_name)
    if not bp or not (Path(root) / bp).exists():
        return "UNDETERMINED (no base obj)", None, None
    try:
        b = _comdat_bodies(Path(root) / bp, {old, new})
    except Exception as e:
        return f"UNDETERMINED ({e})", None, None
    if old not in b or new not in b:
        return "UNDETERMINED (a COMDAT is not a distinct section)", None, None
    (ba, ta), (bb, tb) = b[old], b[new]
    if ba != bb:
        return "BODIES DIFFER -> bounded", None, None
    if ta == tb:
        return "IDENTICAL incl. reloc targets -> free on every ruler", 0, 0
    return ("RELOC-NAME ONLY -> free under `none`, charged under name_check",
            0, 1)


# ---------------------------------------------------------------------------
# The model
# ---------------------------------------------------------------------------

class RowFate:
    """What happens to ONE edited row's own score."""

    def __init__(self, addr, old, new, unit, pin_unit, size, fuzzy, mpn):
        self.addr, self.old, self.new = addr, old, new
        self.unit, self.pin_unit = unit, pin_unit
        self.size, self.fuzzy, self.mpn = size, fuzzy, mpn
        self.verdict = "?"
        self.detail = ""
        self.adj = None          # R2a verdict string
        self.adj_factor = None   # 0/1 * size, or None = still bounded

    # -- R1: the row un-pairs. Both effects are DETERMINED. ------------------
    @property
    def unpairs(self):
        return self.verdict in ("BLOCKED", "DE-NAMED", "UNPINNED")

    @property
    def det_bytes(self):
        """Bytes lost with certainty.  Zero unless the row is CREDITED today."""
        return -self.size if (self.unpairs and self.fuzzy >= 100.0) else 0

    @property
    def det_fns(self):
        """matched_functions lost with certainty -- a SEPARATE ruler (mpn)."""
        return -1 if (self.unpairs and self.mpn >= 100.0) else 0

    # -- R2: the row re-pairs; the BODY decides, so this is a bracket. -------
    @property
    def adjudicated_bytes(self):
        """R2a resolved this re-pairing row -> a DETERMINED cost, not a bound."""
        if self.unpairs or self.adj_factor is None:
            return 0
        return -self.size * self.adj_factor if self.fuzzy >= 100.0 else 0

    @property
    def bounded_bytes(self):
        """Worst case for a re-pairing row R2a could NOT resolve."""
        if self.unpairs or self.adj_factor is not None:
            return 0
        return -self.size if self.fuzzy >= 100.0 else 0

    @property
    def bounded_gain(self):
        """Best case for a row that re-pairs from below 100."""
        if self.unpairs or self.adj_factor is not None:
            return 0
        return self.size if self.fuzzy < 100.0 else 0


def fate_of(root, addr, old, new, smap, rows, splits, fuzzy_override=None,
            ruler="graded"):
    """Classify one edited address under R1/R2.  No build, no body diff."""
    r = rows.get(old)
    unit, size, fuzzy, mpn = (r if r else (None, 0, 0.0, 0.0))
    pin_unit, _s, _e = cp.pinning_unit(splits, addr)
    if fuzzy_override is not None and old in fuzzy_override:
        fuzzy = fuzzy_override[old]
    f = RowFate(addr, old, new, unit, pin_unit, size, fuzzy, mpn)

    if r is None:
        f.verdict, f.detail = "NO_ROW", ("the CURRENT name has no report row -- "
                                         "unpinned/unpairable already; it cannot "
                                         "lose what it is not credited")
        return f
    if pin_unit is None:
        f.verdict, f.detail = "UNPINNED", ("address is in no splits.txt .text block "
                                           "(auto_*) -- cannot pair at all")
        return f
    if cp.is_placeholder(new):
        f.verdict, f.detail = "DE-NAMED", ("replacement is a placeholder: the base obj "
                                           "cannot define it, so the row un-pairs")
        return f
    v, detail = cp.can_define(root, pin_unit, new, unit)
    f.verdict = {"OK": "RE-PAIRS", "BLOCKED": "BLOCKED", "UNKNOWN": "UNKNOWN"}[v]
    f.detail = detail
    if f.verdict == "RE-PAIRS":
        f.adj, none_fac, graded_fac = adjudicate_repair(
            root, pin_unit, old, new, unit)
        fac = none_fac if ruler == "none" else graded_fac
        f.adj_factor = fac
    return f


def price_pairing(root, edits, smap, rows, splits, fuzzy_override=None,
                  ruler="graded"):
    """edits: {addr:int -> new_name}.  Returns [RowFate] for the edited rows."""
    out = []
    for addr, new in sorted(edits.items()):
        old = smap.get(addr) or f"fn_{addr:08X}"
        if old == new:
            continue
        out.append(fate_of(root, addr, old, new, smap, rows, splits,
                           fuzzy_override, ruler))
    return out


def render(fates, title, self_break=False):
    print(f"\n=== {title} ===")
    print(f"{'addr':<12}{'size':>6}{'fuzzy':>10}{'mpn':>7}  {'verdict':<10} unit")
    det_b = det_f = 0
    lo = hi = 0
    for f in fates:
        db = f.det_bytes
        if self_break:                      # sabotage: ignore the credit test
            db = -f.size if f.unpairs else 0
        det_b += db + f.adjudicated_bytes
        det_f += f.det_fns
        lo += f.bounded_bytes
        hi += f.bounded_gain
        print(f"0x{f.addr:08x}{f.size:>6}{f.fuzzy:>10.4f}{f.mpn:>7.1f}  "
              f"{f.verdict:<10} {str(f.unit)[:30]}")
        if f.adj:
            print(f"{'':>12}R2a: {f.adj}  => determined {f.adjudicated_bytes:+d} B")
    print(f"\n  DETERMINED  bytes {det_b:+d}   fns {det_f:+d}")
    print(f"  BOUNDED     re-pairing rows may additionally move "
          f"[{lo:+d}, {hi:+d}] B  (body-dependent, NOT a point estimate)")
    print(f"  BRACKET     bytes [{det_b + lo:+d}, {det_b + hi:+d}]")
    return det_b, det_f, lo, hi


# ---------------------------------------------------------------------------
# validate -- the W20 round-trip known-answer test
# ---------------------------------------------------------------------------

def cmd_validate(args):
    root = args.root
    fix = json.load(open(Path(root) / "docs/decomp/w17-cascade-fixture.json"))
    smap = cp.load_map(root)
    _measures, rows = cp.load_report(root)
    splits = cp.load_splits(root)

    at_new = sum(1 for a, e in fix["edits"].items()
                 if smap.get(int(a, 16)) == e["new"])
    if at_new != len(fix["edits"]):
        print(f"REFUSING: tree is not in the post-W17 map state "
              f"({at_new}/{len(fix['edits'])} at the NEW spelling). The known "
              f"answer only applies to the round-trip INVERSE from that state.")
        return 2

    # The round trip = revert every edited address to its OLD spelling.
    edits = {int(a, 16): e["old"] for a, e in fix["edits"].items()}

    results = {}
    for sel, measured in (("graded", W20_MEASURED_PAIRING_BYTES),
                          ("none", W20_MEASURED_NONE_BYTES)):
        nu = [(smap[addr], rows[smap[addr]][0])
              for addr in edits if smap.get(addr) in rows]
        ov = credit_at_ruler(root, nu, sel)
        fates = price_pairing(root, edits, smap, rows, splits, ov, sel)
        det_b, det_f, lo, hi = render(
            fates, f"W20 ROUND TRIP (inverse of 7e9c2d01) -- PAIRING, ruler={sel}",
            self_break=args.self_break)
        inside = (det_b + lo) <= measured <= (det_b + hi)
        print(f"\n  W20 MEASURED ({sel}) bytes {measured:+d}")
        print(f"  MEASURED inside the bracket : {'PASS' if inside else 'FAIL'} "
              f"(bracket width {hi - lo} B)")
        results[sel] = (det_b, det_f, lo, hi, inside, measured)

    det_f = results["graded"][1]
    ok_f = (det_f == W20_MEASURED_DFNS)
    print(f"\n  d_fns DETERMINED {det_f:+d} vs W20 MEASURED {W20_MEASURED_DFNS:+d} "
          f"(matched_functions is ruler-invariant): {'PASS' if ok_f else 'FAIL'}")

    ok = ok_f and all(r[4] for r in results.values())
    if args.self_break:
        if ok:
            print("\n  VACUOUS: the sabotaged model still passes -- the fixture "
                  "cannot discriminate. Exit 2.")
            return 2
        print("\n  Sabotage correctly REJECTED -> the fixture can fail.")
        return 0
    print(f"\n  OVERALL: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


# ---------------------------------------------------------------------------
# rehome-census -- the inverse question
# ---------------------------------------------------------------------------

def build_definition_index(root, units):
    """symbol -> {unit_name: 'owned'|'shared'} over every compiled base obj."""
    idx = defaultdict(dict)
    for u in units:
        bp = u.get("base_path")
        name = u.get("name", "")
        if not bp:
            continue
        p = Path(root) / bp
        if not p.exists():
            continue
        try:
            owned, shared = coff_owned.analyze(p)
        except Exception:
            continue
        for s in owned:
            idx[s][name] = "owned"
        for s in shared:
            idx[s].setdefault(name, "shared")
    return idx


def cmd_rehome(args):
    root = args.root
    objdiff = json.load(open(Path(root) / "objdiff.json"))
    report = json.load(open(Path(root) / f"build/{VERSION}/report.json"))
    units = objdiff.get("units", [])

    rows = {}
    for u in report["units"]:
        un = u.get("name", "")
        for f in u.get("functions", []):
            rows[(un, f.get("name", ""))] = (
                int(f.get("size", 0) or 0),
                float(f.get("fuzzy_match_percent", 0.0) or 0.0),
                float(f.get("match_percent_normalized", 0.0) or 0.0))

    print("indexing every compiled base obj ...", flush=True)
    idx = build_definition_index(root, units)
    print(f"  {len(idx):,} distinct symbols defined across the base objs")

    orphans, anon_over = [], []
    for u in units:
        name, tp, bp = u.get("name", ""), u.get("target_path"), u.get("base_path")
        if not tp or not bp:
            continue
        tpa, bpa = Path(root) / tp, Path(root) / bp
        if not tpa.exists() or not bpa.exists():
            continue
        try:
            towned, tshared = coff_owned.analyze(tpa)
            bowned, bshared = coff_owned.analyze(bpa)
        except Exception:
            continue
        bdef = bowned | bshared
        bnorm = {NORM_ANON.sub("?A", x) for x in bdef}
        for sym in (towned | tshared):
            key = (name, sym)
            # Anonymous rows are a DIFFERENT, already-sized class (they never
            # pair by name at all); excluding them keeps this census about
            # re-homing rather than about identification.
            if key not in rows or cp.is_placeholder(sym):
                continue
            size, fuzzy, mpn = rows[key]
            if sym in bdef:
                continue
            # ⚠ PAIRING IS NOT STRICTLY EXACT-NAME. An anonymous-namespace hash
            # (?A0x<hash>, which MSVC derives from machine name + source path)
            # is normalized away before pairing, so a symbol the obj does not
            # define VERBATIM can still pair. Measured: 21 of 282 orphans carry
            # such a hash, 2 normalize onto a base definition, and exactly 1 of
            # those 2 pairs -- 2 target symbols against 1 base definition, so
            # the other loses an OVER-SUBSCRIPTION. Booking these as orphans is
            # what made the self-validation fail; they are a different class.
            if NORM_ANON.sub("?A", sym) in bnorm:
                anon_over.append(dict(unit=name, symbol=sym, size=size,
                                      fuzzy=fuzzy, mpn=mpn))
                continue
            elsewhere = {k: v for k, v in idx.get(sym, {}).items() if k != name}
            orphans.append(dict(unit=name, symbol=sym, size=size, fuzzy=fuzzy,
                                mpn=mpn, elsewhere=elsewhere))

    tot = sum(o["size"] for o in orphans)
    bad = [o for o in orphans if o["fuzzy"] != 0.0]
    print(f"\nORPHAN PINS (base obj cannot define the pinned name):")
    print(f"  {len(orphans)} rows / {tot:,} B")
    print(f"  SELF-VALIDATION -- every orphan must read fuzzy 0.0: "
          f"{len(bad)} violations ({'PASS' if not bad else 'FAIL'})")
    ao_paired = sum(1 for o in anon_over if o["fuzzy"] > 0)
    print(f"  ANON-NORMALIZED (pair despite no verbatim definition): "
          f"{len(anon_over)} rows / {sum(o['size'] for o in anon_over):,} B, "
          f"of which {ao_paired} actually pair "
          f"({len(anon_over) - ao_paired} lose an over-subscription)")
    for o in bad[:5]:
        print("    ", o["unit"], o["symbol"][:60], o["fuzzy"])

    reh = [o for o in orphans if o["elsewhere"]]
    uniq = [o for o in reh if len(o["elsewhere"]) == 1]
    owned_only = [o for o in reh
                  if any(v == "owned" for v in o["elsewhere"].values())]
    uniq_owned = [o for o in uniq if "owned" in o["elsewhere"].values()]
    print(f"\nRE-HOMABLE (some OTHER unit's obj defines the same name):")
    print(f"  any destination      : {len(reh):4} rows / "
          f"{sum(o['size'] for o in reh):,} B")
    print(f"  UNIQUE destination   : {len(uniq):4} rows / "
          f"{sum(o['size'] for o in uniq):,} B")
    print(f"  >=1 OWNED definition : {len(owned_only):4} rows / "
          f"{sum(o['size'] for o in owned_only):,} B")
    print(f"  UNIQUE and OWNED     : {len(uniq_owned):4} rows / "
          f"{sum(o['size'] for o in uniq_owned):,} B   <-- cleanest lever")
    print(f"  NO destination       : {len(orphans) - len(reh):4} rows / "
          f"{sum(o['size'] for o in orphans if not o['elsewhere']):,} B "
          f"(no obj anywhere defines it -- needs SOURCE, not a pin)")

    if uniq_owned:
        print(f"\nTop UNIQUE+OWNED candidates by size:")
        for o in sorted(uniq_owned, key=lambda x: -x["size"])[:25]:
            dest = next(iter(o["elsewhere"]))
            print(f"  {o['size']:6,} B  {o['unit'][:26]:26} -> {dest[:26]:26} "
                  f"{o['symbol'][:54]}")

    if args.json:
        with open(args.json, "w") as fh:
            json.dump(orphans, fh, indent=1)
        print(f"\nwrote {args.json}")
    return 0 if not bad else 1


def cmd_whereis(args):
    """Which compiled objs can DEFINE this name?  The shipping gate for a
    rename: if the answer is 'only unit V' and the row is pinned to U, then an
    in-place rename sends the row to 0% and the remedy is a RE-HOME to V."""
    root = args.root
    units = json.load(open(Path(root) / "objdiff.json")).get("units", [])
    idx = build_definition_index(root, units)
    for pat in args.pattern:
        hits = {s: d for s, d in idx.items() if pat in s}
        print(f"\n=== {pat!r}: {len(hits)} matching symbol(s) ===")
        for s, d in sorted(hits.items())[:40]:
            own = [k for k, v in d.items() if v == "owned"]
            print(f"  {s[:88]}")
            print(f"     defined in {len(d)} obj(s); owned in {len(own)}: "
                  f"{', '.join(sorted(d)[:6])}{' ...' if len(d) > 6 else ''}")
        if not hits:
            print("  NO compiled obj defines any symbol containing this. A rename "
                  "to such a name un-pairs the row WHEREVER it is pinned; the "
                  "remedy is SOURCE, not a pin move.")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-C", "--root", default=".")
    sub = ap.add_subparsers(dest="cmd", required=True)
    v = sub.add_parser("validate", help="W20 round-trip known-answer test")
    v.add_argument("--self-break", action="store_true",
                   help="sabotage R1 to prove the fixture can FAIL")
    v.set_defaults(fn=cmd_validate)
    r = sub.add_parser("rehome-census", help="the inverse: rows that would PAIR if re-homed")
    r.add_argument("--json", default=None)
    r.set_defaults(fn=cmd_rehome)
    w = sub.add_parser("whereis", help="which objs can DEFINE a name (rename gate)")
    w.add_argument("pattern", nargs="+")
    w.set_defaults(fn=cmd_whereis)
    args = ap.parse_args()
    args.root = os.path.abspath(args.root)
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
