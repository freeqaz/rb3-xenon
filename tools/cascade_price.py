#!/usr/bin/env python3
"""cascade_price.py -- price a map-name repair INCLUDING ITS CALLER CASCADE.

WHY THIS EXISTS
===============
A wrong map name is not a local defect. Under the shipped `name_check` ruler a
relocation site is charged when RETAIL's callee name differs from OURS, so one
wrong name at address A is financed by a `diff_arg` charge on EVERY row that
relocates against A. Repairing it therefore pays TWICE: once locally, and again
at every call site whose only remaining blocker was that charge.

Three lanes priced such a repair and all three UNDERSHOT, in the same direction:

    W8-TWINPORT    predicted   +24    measured   +184
    W9-FALSECREDIT predicted    ~0    measured   +268
    W17-FAMILYSWEEP predicted +1,072  measured +1,652

W17 makes the mechanism unmistakable because it ran the `none` control
alongside: `none` predicted +792 and measured +792 EXACTLY, which localises the
whole +580 graded overshoot to FOUR ROWS THAT WERE NOT IN THE PATCH --
`_Rb_tree<String,DataNode>` ctor +240, `SupportChar@RndText` +212,
`CharAdvance@RndFont` +76, `CharWidth@RndFont` +52.

"Remember to include the call sites" has now failed as a rule three times, so
this tool makes it MECHANICAL: given a proposed edit, enumerate the affected
rows and price each as its own line item BEFORE the edit is made.

WHAT IT PREDICTS, AND THE FOUR MECHANISMS IT MODELS
===================================================
`matched_code` keys on `fuzzy == 100` and is ALL-OR-NOTHING per row, so the
question for every affected row is only ever "does its charged-site count reach
zero (or leave zero)". For each site where retail's symbol is renamed O -> N,
with our side spelling B:

  CLEARED    charged now (O != B), N == B after      -> row may CROSS   (+size)
  NEW_CHARGE equal now  (O == B, or O forgiven),
             N != B after                            -> row may FALL    (-size)
  PERSISTS   charged now and still charged after     -> no movement
  UNAFFECTED site does not name an edited address    -> no movement

⇒ **THE CASCADE CAN BE NEGATIVE.** No prior lane priced that direction. A rename
that fixes address A silently breaks every row whose source genuinely calls the
OLD spelling, and those rows are at 100% today so the loss is invisible until
the A/B lands.

★ NAMING AN ANONYMOUS ADDRESS IS HANDLED BY THE SAME MACHINERY. `reloc_eq`
forgives a PLACEHOLDER TARGET name (`fn_`/`lbl_`/`jumptable_`/`code_`/`data_`/
`bss_`/`rdata_`, one optional leading underscore; objdiff-core
`diff/code.rs:915`), so an unnamed callee is already uncharged. Naming it
converts a FORGIVEN site into a CHECKED one -- right name = still free, wrong
name = a brand-new charge. That is CLAUDE.md's "naming is a bet, not a freebie",
and here it falls out of the site table rather than being a thing to remember.

THE INSTRUMENT CORRECTION THAT MAKES THIS NOT LIE (lane W19)
============================================================
⛔ An `arg:{Register,Symbol}` diff is charged BY THE REGISTER; only a BARE
`arg:{Symbol}` is a real relocation-name charge. Counting Symbol-bearing arg
diffs naively reads 138 name charges on `?Handle@VocalPlayer@@` when the true
count is ZERO (all 138 are one uniform r29<->r28 displacement against forgiven
`lbl_` slots). A pricer that gets this wrong invents charges objdiff never
levies, and would then predict crossings that cannot happen. See
`tools/w19_charge_census.py`.

WHAT IT DOES NOT MODEL, DELIBERATELY
=====================================
`reloc_eq` has SIX further forgiveness paths beyond name equality (pool anchors,
counter-named data, interior self-reference, weak-external `??_E`/`??_G`
defaults, compiler-local `$` labels, and the ICF `SymbolEquivalences` map).
Re-implementing them would be a second, drifting copy of objdiff's scoring.

So this tool NEVER models the CURRENT state -- it READS it from objdiff's own
`match_type`. It models only the DELTA, using name equality + placeholder-ness +
the ICF alias map. Any affected site whose post-edit verdict depends on a path
it does not model is reported as UNCERTAIN and excluded from the point estimate
rather than guessed at.

⚠ COVERAGE BOUND: callers are found by scanning retail `.text` for `bl`
instructions (`tools/retail_callers.py`). A reference taken as DATA -- a vtable
slot or a function-pointer table -- is NOT a `bl` and is NOT enumerated here.
Those live in data symbols, which `matched_code` scores separately. A clean run
is a clearance for call sites, never for the whole reference graph.

Usage
=====
    # price a proposed edit, with its cascade broken out as its own line item
    python3 tools/cascade_price.py price --edit 0x82456190=?_M_insert@... \\
                                         --edit 0x824563d8=?insert_unique@...

    # price every edit in a JSON file {"0xaddr": "NewMangledName", ...}
    python3 tools/cascade_price.py price --edit-file proposed.json

    # frozen known-answer test against W17's four cascade rows
    python3 tools/cascade_price.py validate
"""
import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "scripts" / "analysis"))

import retail_callers  # noqa: E402  (reuse the PE bl scan, never re-implement it)
import coff_owned      # noqa: E402  (reuse the COMDAT-aware obj symbol reader)

try:
    from ruler import resolve_ruler
except Exception:                                            # pragma: no cover
    resolve_ruler = None

VERSION = "45410914"
FIXTURE = "docs/decomp/w17-cascade-fixture.json"
SWAP_FIXTURE = "docs/decomp/w36-bodyswap-fixture.json"

PLACEHOLDER_PREFIXES = ("fn_", "lbl_", "jumptable_", "code_",
                        "data_", "bss_", "rdata_")


# ---------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------

def is_placeholder(name):
    """Mirror of objdiff-core `is_placeholder_symbol_name` (diff/code.rs:915)."""
    if not name:
        return False
    n = name[1:] if name.startswith("_") else name
    for p in PLACEHOLDER_PREFIXES:
        if n.startswith(p):
            rest = n[len(p):]
            if rest and all(c in "0123456789abcdefABCDEF_" for c in rest):
                return True
    return False


def load_map(root):
    p = Path(root) / "scripts" / "target_symbol_map.json"
    raw = json.load(open(p))
    out = {}
    for k, v in raw.items():
        if not isinstance(k, str) or not k.startswith("0x") or not isinstance(v, str):
            continue
        try:
            out[int(k, 16)] = v
        except ValueError:
            continue
    return out


SYM_RE = re.compile(r"^(\S+)\s*=\s*\.text:0x([0-9a-fA-F]+);.*?type:function.*?size:0x([0-9a-fA-F]+)")


def load_symbols(root):
    """Sorted [(addr, size, name)] for every .text function in symbols.txt.

    ⚠ Keyed on the SYMBOL, never on any address column elsewhere in the tree:
    CLAUDE.md records that dtk's `.s` address columns are SYNTHETIC for
    multi-block units. symbols.txt itself is the authoritative extent table.
    """
    p = Path(root) / "config" / VERSION / "symbols.txt"
    out = []
    for line in open(p):
        m = SYM_RE.match(line.strip())
        if m:
            out.append((int(m.group(2), 16), int(m.group(3), 16), m.group(1)))
    out.sort()
    return out


def containing_fn(symbols, pc):
    """Function whose [addr, addr+size) contains pc, else None."""
    lo, hi = 0, len(symbols) - 1
    best = None
    while lo <= hi:
        mid = (lo + hi) // 2
        a, sz, nm = symbols[mid]
        if a <= pc:
            best = symbols[mid]
            lo = mid + 1
        else:
            hi = mid - 1
    if best and best[0] <= pc < best[0] + best[1]:
        return best
    return None


def load_report(root):
    """{row_name: (unit, size, fuzzy, mpn)} -- every numeric int()/float() coerced.

    ⚠ report.json is protobuf-JSON: defaults are OMITTED and several numerics are
    JSON STRINGS (CLAUDE.md). An un-coerced `+` concatenates and a `>` compares
    lexicographically.
    """
    p = Path(root) / "build" / VERSION / "report.json"
    d = json.load(open(p))
    rows = {}
    for unit in d.get("units", []):
        u = unit.get("name", "?")
        for fn in unit.get("functions", []):
            rows[fn.get("name", "")] = (
                u,
                int(fn.get("size", 0) or 0),
                float(fn.get("fuzzy_match_percent", 0.0) or 0.0),
                float(fn.get("match_percent_normalized", 0.0) or 0.0))
    return d.get("measures", {}), rows


def load_aliases(root):
    """name -> group-id, from the synthetic ICF alias map objdiff consumes.

    Two spellings sharing an address are equivalent to `reloc_eq` via
    SymbolEquivalences, so a name difference between them is NOT a charge.
    """
    p = Path(root) / "build" / VERSION / "icf_aliases.map"
    groups = {}
    if not p.exists():
        return groups
    for line in open(p):
        if line.startswith(";") or not line.strip():
            continue
        parts = line.split()
        # " 0001:00000000   <name> <addr>  f i icf_aliases.synthetic"
        if len(parts) >= 3 and parts[0].count(":") == 1:
            groups.setdefault(parts[1], set()).add(parts[2].lower())
    return groups


# ---------------------------------------------------------------------------
# HARD LIMIT #1 -- can the pinned unit's base obj even DEFINE the new name?
#
# ⛔ PROVING A NAME WRONG DOES NOT MAKE RENAMING IT SAFE. objdiff pairs
# target<->base BY NAME, so if the obj behind the pin cannot define the
# replacement, the row reads 0% however correct the new name is -- permanently.
# That is W9's measured -180 B failure mode, and it is the single most expensive
# mistake available in this lane, so it is checked mechanically rather than
# remembered.
# ---------------------------------------------------------------------------

SPLIT_UNIT_RE = re.compile(r"^(\S.*):\s*$")
SPLIT_TEXT_RE = re.compile(r"\.text\s+start:0x([0-9a-fA-F]+)\s+end:0x([0-9a-fA-F]+)")


def load_splits(root):
    """Sorted [(start, end, unit)] over pinned .text blocks."""
    out, cur = [], None
    for line in open(Path(root) / "config" / VERSION / "splits.txt"):
        m = SPLIT_UNIT_RE.match(line)
        if m and not line.startswith((" ", "\t")):
            cur = m.group(1).strip()
            continue
        t = SPLIT_TEXT_RE.search(line)
        if t and cur:
            out.append((int(t.group(1), 16), int(t.group(2), 16), cur))
    out.sort()
    return out


def pinning_unit(splits, addr):
    for s, e, u in splits:
        if s <= addr < e:
            return u, s, e
    return None, None, None


_OBJDIFF_CACHE = {}


def _objdiff_units(root):
    if root not in _OBJDIFF_CACHE:
        _OBJDIFF_CACHE[root] = json.load(open(Path(root) / "objdiff.json")).get("units", [])
    return _OBJDIFF_CACHE[root]


def base_obj_for_unit(root, unit_src, unit_name=None):
    """Resolve a unit to its compiled base .obj.

    ⛔ SUFFIX MATCHING IS A TRAP AND THIS TOOL FELL INTO IT ONCE. A first cut
    used `base_path.endswith(stem + ".obj")`, which resolved the splits heading
    `Song.cpp` to `SongSortBySong.obj` -- a WRONG obj, and therefore a wrong
    BLOCKED/OK verdict on the single most expensive decision this tool makes.
    That is CLAUDE.md's basename hazard (`Movie.obj` exists in both `rnddx9/`
    and `rndobj/`) wearing a slightly different costume.

    So prefer report.json's OWN unit name, which objdiff.json also keys on --
    an identity, not a string guess. Fall back to a PATH-COMPONENT match, and
    return None on ambiguity rather than picking one.
    """
    if unit_name:
        for u in _objdiff_units(root):
            if u.get("name") == unit_name:
                return u.get("base_path")
    stem = unit_src[:-4] if unit_src.endswith(".cpp") else unit_src
    want = stem + ".obj"
    cands = []
    for u in _objdiff_units(root):
        bp = u.get("base_path") or ""
        if not bp:
            continue
        rel = bp.split("/src/", 1)[1] if "/src/" in bp else bp
        if rel == want:
            return bp                                  # exact relative path
        if "/" not in want and rel.rsplit("/", 1)[-1] == want:
            cands.append(bp)
    return cands[0] if len(cands) == 1 else None


# ---------------------------------------------------------------------------
# HARD LIMIT #1b -- DEFINING THE NAME IS NOT THE SAME AS DEFINING THE RIGHT BODY
#
# ⛔ THE DEFECT THIS FIXES (found by lane W26, landed unfixed with a warning;
#    repaired by W29). `can_define` used to answer only "is this name present in
#    our obj's symbol table?" and print a confident
#    `PAIRS (obj defines the new name)`. That is the RIGHT question exactly when
#    our obj defines ONE of the two spellings. It is the WRONG question in the
#    SCATTER-INCLUDE case -- where one .cpp `#include`s another and the single
#    obj therefore defines BOTH spellings of a whole family.
#
#    There a rename does not create a pairing, it SWAPS WHICH BODY objdiff
#    COMPARES. The name resolves either way, so the old verdict was green either
#    way, and the tool modelled the row as `no movement` while it was in fact
#    about to be paired against a DIFFERENT function.
#
#    Measured on W26's own case, `?PropSync@@...Target@HamCamShot@@...` at
#    0x822b4298: retail's extent is 1692 B, our `HamCamShot`-spelled COMDAT is
#    1692 B / 194 relocations, and our `BandCamShot`-spelled COMDAT is
#    1604 B / 180 relocations. The rename would have swapped a body that matches
#    retail's size exactly for one 88 B short, and the tool said `no movement`.
#
#    Same disease as W20's "right bytes, wrong mechanism": the verdict was never
#    wrong about DEFINITION, it was SILENT about IDENTITY.
#
# THE TOLERANCE IS CALIBRATED ON THE UNTREATED POPULATION, NOT CHOSEN
# -------------------------------------------------------------------
# A row at `fuzzy == 100` is instruction-for-instruction equal, so its base
# COMDAT size MUST equal retail's extent -- which makes every such row a FREE
# NULL for this check. Measured on build 45410914 (19,074 scored rows), the
# delta `retail_extent - our_COMDAT_size` at `fuzzy == 100` takes exactly three
# values:
#
#       -4 :     8 rows
#       +0 : 18844 rows
#       +4 :   222 rows        (mostly `$4PPPPPPPM@A@` vbase adjustor thunks)
#
# NOT ONE provably-matching row disagrees by 8 or more. The +/-4 band is carve
# granularity (retail extents are 8-byte aligned; a 12 B thunk is carved 16),
# not divergence. So firing at |delta| >= 8 has a MEASURED false-positive rate
# of 0 / 19,074, while still firing on ~495 of the 3,601 sub-100 rows -- i.e.
# it discriminates rather than confirming whatever it is pointed at.
#
# ⚠ The size must come from `coff_bodies_ext`, NEVER from a hand-rolled COMDAT
# span. Lane STLPORT-1 measured a whole fabricated "+8 B STLport source bug"
# that was `coff_bodies_ext`'s predecessor billing the SUCCESSOR symbol's
# 8-byte EH prefix into the function above it. That reader has the artifact
# fixed (twice: marker-first, name-never) and is the only one that should be
# used here. Re-implementing it would resurrect the bug at exactly the
# tolerance that matters.
#
# ⚠ AND A SIZE TEST IS A NECESSARY, NOT SUFFICIENT, CONDITION. Two bodies of
# equal size can still be different functions. This check can therefore only
# ever REFUTE a swap, never bless one -- which is why an equal size prints
# nothing rather than an endorsement.
# ---------------------------------------------------------------------------

# Max |retail_extent - our_COMDAT_size| observed over rows that PROVABLY match.
# Anything above this is not carve granularity.
SIZE_TOLERANCE_B = 4

_BODY_SIZE_CACHE = {}


def defined_body_sizes(root, bp):
    """{symbol -> body size} for one obj, via the EH-artifact-fixed reader."""
    key = str(Path(root) / bp)
    if key not in _BODY_SIZE_CACHE:
        sizes = {}
        try:
            import coff_bodies_ext
            for n, body, _rel, _eo in coff_bodies_ext.function_bodies_ext(key):
                sizes.setdefault(n, len(body))
        except Exception as exc:                              # pragma: no cover
            sizes = {"__error__": str(exc)}
        _BODY_SIZE_CACHE[key] = sizes
    return _BODY_SIZE_CACHE[key]


def can_define(root, unit_src, name, unit_name=None, retail_size=None):
    """(verdict, detail) -- does the unit's obj define `name`, AS THE RIGHT BODY?

    `retail_size` is the target extent at the address being renamed. When it is
    known, a definition whose body diverges beyond SIZE_TOLERANCE_B is reported
    as SIZE_MISMATCH rather than a green PAIRS -- see HARD LIMIT #1b above.
    """
    bp = base_obj_for_unit(root, unit_src, unit_name)
    if not bp:
        return "UNKNOWN", (f"could not resolve a UNIQUE base obj for {unit_src} "
                           f"(ambiguous basename, or unit not wired) -- refusing "
                           f"to guess, because a wrong obj here gives a "
                           f"confidently wrong BLOCKED/OK verdict")
    p = Path(root) / bp
    if not p.exists():
        return "UNKNOWN", f"{bp} not built"
    owned, shared = coff_owned.analyze(p)
    if name in owned:
        kind = "COMDAT NO_DUPLICATES / owned"
    elif name in shared:
        kind = "COMDAT ANY / template-shared"
    else:
        return "BLOCKED", (f"{bp} does NOT define this name -- an in-place rename "
                           f"sends the row to 0% PERMANENTLY (W9's -180 B failure). "
                           f"Re-home to a unit whose obj defines it, and lift the "
                           f"spelling verbatim from that obj's symbol table.")

    # The name resolves. Now ask the question the old verdict skipped: is the
    # body it resolves TO the same function retail has at this address?
    ours = defined_body_sizes(root, bp).get(name)
    if retail_size is None or ours is None:
        why = ("no retail extent for this address" if retail_size is None
               else "no readable COMDAT body for this name")
        return "OK", (f"{bp} defines it ({kind}) -- SIZE UNVERIFIED ({why}); "
                      f"this verdict is about DEFINITION only")
    delta = retail_size - ours
    if abs(delta) > SIZE_TOLERANCE_B:
        return "SIZE_MISMATCH", (
            f"{bp} defines the name ({kind}) BUT ITS BODY IS THE WRONG SIZE: "
            f"ours {ours} B vs retail's {retail_size} B ({delta:+d}). "
            f"The obj defines BOTH spellings, so this rename does not create a "
            f"pairing -- it SWAPS WHICH BODY objdiff COMPARES, onto a function "
            f"that cannot reach fuzzy==100 at any register allocation. "
            f"Reconcile the SOURCE first (the two definitions genuinely differ), "
            f"then re-price. Tolerance is {SIZE_TOLERANCE_B} B, calibrated so "
            f"that 0 of 19,074 provably-matching rows fire.")
    return "OK", (f"{bp} defines it ({kind}); body {ours} B vs retail "
                  f"{retail_size} B ({delta:+d}, within the {SIZE_TOLERANCE_B} B "
                  f"carve tolerance) -- size is CONSISTENT, which refutes a swap "
                  f"but does not prove identity")


# ---------------------------------------------------------------------------
# HARD LIMIT #1c -- THE BASE BODY SWAPS UNDER A LOCAL RENAME
#
# ⛔ W29 found this and deliberately left it unfixed, because the one cell it
#    hand-adjudicated came out in the TOOL's favour. Lane W36 built the
#    known-answer fixture W29 asked for and the defect is REAL and LARGE:
#    on the 11-row BandCamShot rename the local channel is worth +1,552 B and
#    the old model priced it at -396 B -- an error of 1,948 B, while its
#    CASCADE estimate (+812) was exactly right. 100% of the error was here.
#
# THE MECHANISM. objdiff pairs target<->base BY NAME. In the scatter-include
# case (`BandCamShot.cpp` `#include`s `hamobj/HamCamShot.cpp`) ONE obj defines
# BOTH spellings, so a rename does not create a pairing -- it changes WHICH OF
# OUR BODIES objdiff compares. The target side keeps the same retail bytes; the
# BASE side is swapped wholesale. Measured on those 11 rows: all 11 base bodies
# are BYTE-IDENTICAL between the two spellings, but 8 of 11 carry DIFFERENT
# RELOCATION NAMES -- and `name_check` charges on exactly those names.
#
# So a verdict computed from the pre-swap diff is computed against a body
# objdiff will never compare. Concretely, `?GetNumShots@HamCamShot` is charged
# T=`?ListNextShots@BandCamShot` vs B=`?ListNextShots@HamCamShot`; the target
# spelling is ALREADY Band, so it is not in the rename dict, so the old model
# carried the charge forward as PERSISTS. Post-rename the base body is the
# Band-spelled COMDAT, which spells that callee Band, and the charge CLEARS.
#
# THE FIX. Because the two bodies are byte-identical, their relocation lists
# are positionally aligned, which yields a BASE-SIDE rename dict
# {old_body_reloc_name -> new_body_reloc_name}. Applying it to `bsym` before
# every comparison prices the post-swap body instead of the pre-swap one.
#
# ⚠ THIS IS NOT A LICENCE TO GUESS. Every precondition is asserted and a
#   failure returns a REASON, never a silently-empty map -- an empty map is
#   indistinguishable from "no swap", which is precisely the vacuity this lane
#   exists to remove. When the swap cannot be modelled the row is marked
#   UNRELIABLE and says so in the report.
# ---------------------------------------------------------------------------

def base_body_swap(root, unit_src, unit_name, old, new):
    """{old_body_reloc_name -> new_body_reloc_name} for a LOCAL rename.

    Returns (mapping, status). `mapping` is None when the swap cannot be
    modelled; `status` always explains which case fired:

      NO_SWAP      -- our obj does not define `old`, so nothing is swapped away
                      and the existing (pre-swap == post-swap) model is exact.
      SWAP         -- both spellings defined; mapping is the base-side rename.
      SWAP_NOMAP   -- both defined but the bodies are not positionally
                      comparable (different bytes / reloc geometry). The local
                      verdicts for this row are NOT trustworthy.
      UNKNOWN      -- obj unresolvable or unbuilt.
    """
    bp = base_obj_for_unit(root, unit_src, unit_name)
    if not bp or not (Path(root) / bp).exists():
        return None, "UNKNOWN (no resolvable/built base obj)"
    rel = _body_relocs(root, bp)
    o, n = rel.get(old), rel.get(new)
    if n is None:
        # can_define() already reports this as BLOCKED; nothing to swap.
        return {}, "NO_SWAP (obj does not define the new name)"
    if o is None:
        return {}, ("NO_SWAP (obj does not define the OLD name -- this rename "
                    "creates a pairing rather than swapping a body)")
    ob, orl = o
    nb, nrl = n
    if len(ob) != len(nb) or ob != nb:
        return None, (f"SWAP_NOMAP (obj defines BOTH spellings but the bodies "
                      f"differ: {len(ob)} B vs {len(nb)} B, bytes_equal="
                      f"{ob == nb}) -- the post-swap body cannot be derived "
                      f"positionally, so the local charge verdicts for this row "
                      f"are UNRELIABLE")
    if len(orl) != len(nrl) or [x[0] for x in orl] != [x[0] for x in nrl]:
        return None, ("SWAP_NOMAP (bodies are byte-identical but their "
                      "relocation geometry differs) -- local charge verdicts "
                      "for this row are UNRELIABLE")
    mapping = {}
    for (_off, on_, _t1), (_off2, nn, _t2) in zip(orl, nrl):
        if on_ == nn:
            continue
        if mapping.get(on_, nn) != nn:
            return None, (f"SWAP_NOMAP (base spelling {on_[:40]!r} maps to two "
                          f"different post-swap names) -- UNRELIABLE")
        mapping[on_] = nn
    return mapping, (f"SWAP ({len(mapping)} base-side relocation name(s) change "
                     f"when objdiff switches to the {new[:34]!r} body)")


_BODY_RELOC_CACHE = {}


def _body_relocs(root, bp):
    """{symbol -> (body_bytes, [(offset, name, type), ...])} for one obj."""
    key = str(Path(root) / bp)
    if key not in _BODY_RELOC_CACHE:
        out = {}
        try:
            import coff_bodies_ext
            for n, body, rel, _eo in coff_bodies_ext.function_bodies_ext(key):
                out.setdefault(n, (body, list(rel)))
        except Exception:                                     # pragma: no cover
            out = {}
        _BODY_RELOC_CACHE[key] = out
    return _BODY_RELOC_CACHE[key]


# ---------------------------------------------------------------------------
# objdiff
# ---------------------------------------------------------------------------

def ruler_args(root, selector="graded"):
    if resolve_ruler is None:
        return [], "UNLABELLED (analysis.ruler not importable)"
    r = resolve_ruler(root, selector)
    args = r.args() if callable(getattr(r, "args", None)) else r.args
    return list(args), r.label() if callable(getattr(r, "label", None)) else str(r.label)


def run_diff(root, symbol, unit, rargs, cache_dir="/tmp/claude/w20cascade"):
    os.makedirs(cache_dir, exist_ok=True)
    safe = re.sub(r"[^A-Za-z0-9]+", "_", symbol)[:60]
    out = os.path.join(cache_dir, f"{abs(hash((symbol, unit))) % 10**10}_{safe}.json")
    cmd = [str(Path(root) / "bin" / "objdiff-cli"), "diff", "-p", str(root),
           symbol, "--include-instructions", *rargs, "-f", "json", "-o", out]
    if unit:
        cmd += ["-u", unit]
    r = subprocess.run(cmd, cwd=str(root), capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out):
        return None, ((r.stdout or "") + (r.stderr or "")).strip().splitlines()[-1:] or ["?"]
    return json.load(open(out)), None


def sym_arg(side):
    for a in (side or {}).get("typed_args") or []:
        if (a or {}).get("type") == "Symbol":
            return a.get("value")
    return None


def differing_kinds(ins):
    """Types of the ARGUMENTS that actually differ.  The W19 rule lives here."""
    t = (ins.get("target") or {}).get("typed_args") or []
    b = (ins.get("base") or {}).get("typed_args") or []
    kinds = set()
    for x, y in zip(t, b):
        if (x or {}).get("value") != (y or {}).get("value"):
            kinds.add((x or {}).get("type"))
    return kinds


# ---------------------------------------------------------------------------
# The pricing model
# ---------------------------------------------------------------------------

class Verdict:
    def __init__(self, row, unit, size, fuzzy, mpn):
        self.row, self.unit, self.size = row, unit, size
        self.fuzzy, self.mpn = fuzzy, mpn
        self.cur = 0
        self.post = 0
        self.events = []
        self.uncertain = []

    @property
    def crosses(self):
        return self.fuzzy < 100.0 and self.cur > 0 and self.post == 0

    @property
    def falls(self):
        return self.fuzzy >= 100.0 and self.post > 0

    @property
    def delta(self):
        if self.crosses:
            return self.size
        if self.falls:
            return -self.size
        return 0


def equiv(a, b, aliases):
    if a == b:
        return True
    if a is None or b is None:
        return False
    ga, gb = aliases.get(a), aliases.get(b)
    return bool(ga and gb and ga & gb)


def price_row(diff, rename, aliases, unit, size, fuzzy, mpn, name,
              base_rename=None):
    """Charged sites now vs after the edit.  `rename` is {old_name: new_name}.

    `base_rename` is the BASE-side rename induced by a body swap (HARD LIMIT
    #1c).  It is empty for every ordinary rename, and when it is empty this
    function is behaviourally identical to its pre-W36 form -- which the frozen
    W17 fixture exists to keep true.
    """
    v = Verdict(name, unit, size, fuzzy, mpn)
    base_rename = base_rename or {}
    for ins in diff.get("instructions", []):
        mt = ins.get("match_type")
        tsym = sym_arg(ins.get("target"))
        bsym = sym_arg(ins.get("base"))
        charged_now = (mt != "equal")
        if charged_now:
            v.cur += 1

        # ⇣ HARD LIMIT #1c: which of OUR bodies objdiff compares can change.
        bsym_after = base_rename.get(bsym, bsym) if bsym else bsym
        base_moved = (bsym_after != bsym)

        touched = tsym in rename if tsym else False
        if not touched and not base_moved:
            v.post += 1 if charged_now else 0
            continue

        new_t = rename.get(tsym, tsym)
        tag = "/swap" if base_moved else ""
        if not charged_now:
            # Site is EQUAL today. Either the names agree, or objdiff forgave a
            # difference. A rename -- of EITHER side -- can add a charge here.
            if equiv(new_t, bsym_after, aliases):
                continue                       # still agrees -> still free
            if is_placeholder(new_t):
                continue                       # still forgiven (rename to placeholder)
            if is_placeholder(tsym) or ins.get("masked_equal"):
                # Was forgiven by placeholder-ness or an alias; the new REAL name
                # will be checked against ours and disagrees.
                v.post += 1
                v.events.append(("NEW_CHARGE(was forgiven)" + tag,
                                 tsym, bsym_after, new_t))
            else:
                v.post += 1
                v.events.append(("NEW_CHARGE" + tag, tsym, bsym_after, new_t))
            continue

        # Site is CHARGED today.
        kinds = differing_kinds(ins)
        if mt == "diff_arg" and kinds == {"Symbol"}:
            # A genuine relocation-NAME charge (the W19 rule).
            if equiv(new_t, bsym_after, aliases):
                v.events.append(("CLEARED" + tag, tsym, bsym_after, new_t))
                continue
            v.post += 1
            v.events.append(("PERSISTS" + tag, tsym, bsym_after, new_t))
        else:
            # Charged by a register / immediate / branch-dest / whole-instruction
            # difference. The symbol is incidental; a rename cannot clear it.
            v.post += 1
            if mt == "diff_arg":
                v.events.append(("PERSISTS(reg-charged)", tsym, bsym, new_t))
    return v


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def gather(root, edits, selector="graded", verbose=True, model_swap=True):
    """edits: {addr:int -> new_name:str}.  Returns (verdicts, notes, ruler_label)."""
    smap = load_map(root)
    symbols = load_symbols(root)
    measures, rows = load_report(root)
    aliases = load_aliases(root)
    rargs, rlabel = ruler_args(root, selector)
    notes = []

    rename = {}
    for addr, new in edits.items():
        old = smap.get(addr) or f"fn_{addr:08X}"
        if old == new:
            notes.append(f"NO-OP 0x{addr:08x}: already named {new[:40]}")
            continue
        rename[old] = new

    # ⚠ A site shows NAMES, not addresses. If the old name is not UNIQUE in the
    # map, a rename keyed on the name would also rewrite sites that reach a
    # DIFFERENT address with the same spelling. Refuse to price those silently.
    byname = {}
    for a, n in smap.items():
        byname.setdefault(n, []).append(a)
    ambiguous = set()
    for addr, new in edits.items():
        old = smap.get(addr)
        if old and len(byname.get(old, [])) > 1:
            ambiguous.add(old)
            notes.append(
                f"AMBIGUOUS: old name at 0x{addr:08x} is shared by "
                f"{len(byname[old])} addresses -- caller sites naming it cannot "
                f"be attributed to this address. Rows below are UPPER BOUNDS.")

    # Callers: retail bl sites into every edited address.
    hits = retail_callers.bl_sites(set(edits), pe=str(Path(root) / "orig" / VERSION / "band.exe"))
    callers = {}
    for tgt, pcs in hits.items():
        for pc in pcs:
            fn = containing_fn(symbols, pc)
            if fn is None:
                notes.append(f"call site 0x{pc:08x} -> 0x{tgt:08x} is in NO known function extent")
                continue
            callers.setdefault(fn[0], set()).add(tgt)

    # Candidate rows = callers, plus the edited addresses themselves.
    # ⚠ An EDITED address is classified `local` even when it is also a caller
    # (family members call each other). Its dominant effect is the PAIRING
    # channel, and folding it into the cascade estimate would credit the right
    # bytes to the wrong mechanism -- the "count right, cause wrong" trap.
    cand = {a: "caller" for a in callers}
    for a in edits:
        cand[a] = "local"

    # HARD LIMIT #1c: for a LOCAL row, work out whether the rename swaps which
    # of OUR bodies objdiff compares, and if so how the BASE side is respelled.
    # ⚠ Computed per LOCAL address only -- a caller row keeps its own body, so
    # only its TARGET-side spelling moves, which the old model already handled.
    swap_by_addr, swap_status = {}, {}
    splits_pre = load_splits(root)
    for addr, new in (edits.items() if model_swap else []):
        old = smap.get(addr) or f"fn_{addr:08X}"
        if old == new:
            continue
        u, _s, _e = pinning_unit(splits_pre, addr)
        ru = rows.get(old, (None,))[0] if old in rows else None
        if u is None:
            swap_status[addr] = "UNKNOWN (address is not in any splits.txt block)"
            continue
        mp, st = base_body_swap(root, u, new, old, new)
        swap_status[addr] = st
        if mp:
            swap_by_addr[addr] = mp
        elif mp is None:
            swap_by_addr[addr] = None            # UNRELIABLE, not "no swap"

    verdicts = []
    for addr, kind in sorted(cand.items()):
        # Row name AFTER the edit for a local row, BEFORE for a caller row: the
        # report is keyed on the pre-edit name in both cases.
        nm = smap.get(addr) or f"fn_{addr:08X}"
        if nm not in rows:
            notes.append(f"0x{addr:08x} ({kind}) has no paired row in report.json "
                         f"-- unpinned or unpairable; contributes 0")
            continue
        unit, size, fuzzy, mpn = rows[nm]
        diff, err = run_diff(root, nm, unit, rargs)
        if diff is None:
            notes.append(f"objdiff could not diff {nm[:50]} in {unit}: {err}")
            continue
        br = swap_by_addr.get(addr) if kind == "local" else None
        v = price_row(diff, rename, aliases, unit, size, fuzzy, mpn, nm,
                      base_rename=(br if br else None))
        v.kind = kind
        v.addr = addr
        if kind == "local":
            v.swap_status = swap_status.get(addr, "")
            if addr in swap_by_addr and swap_by_addr[addr] is None:
                v.uncertain.append("body-swap-UNMODELLABLE")
        if any(t in ambiguous for t, *_ in [(e[1],) for e in v.events]):
            v.uncertain.append("ambiguous-old-name")
        verdicts.append(v)
        if verbose:
            print(".", end="", flush=True)
    if verbose:
        print()

    # LOCAL / PAIRING term: can the pin behind each edited address define the
    # replacement at all?  (HARD LIMIT #1 -- see can_define.)
    splits = load_splits(root)
    # ⚠ Retail extent comes from symbols.txt keyed on ADDRESS. CLAUDE.md: dtk's
    # `.s` address COLUMNS are synthetic for multi-block units, but symbols.txt
    # is the authoritative extent table and is what load_symbols() already read.
    ext_by_addr = {a: sz for a, sz, _n in symbols}
    locals_ = []
    for addr, new in sorted(edits.items()):
        old = smap.get(addr) or f"fn_{addr:08X}"
        unit, s, e = pinning_unit(splits, addr)
        size, fuzzy = 0, 0.0
        report_unit = None
        if old in rows:
            report_unit, size, fuzzy, _m = rows[old]
        if unit is None:
            verdict, detail = "UNPINNED", ("address is in no splits.txt .text block "
                                           "(auto_* / unattributed) -- it cannot pair "
                                           "at all until it is pinned")
        elif is_placeholder(new):
            verdict, detail = "DE-NAMED", ("replacement is a placeholder: the row "
                                           "un-pairs and its bytes are withdrawn")
        else:
            v, detail = can_define(root, unit, new, report_unit,
                                   retail_size=ext_by_addr.get(addr))
            verdict = {"OK": "PAIRS (obj defines the new name)",
                       "BLOCKED": "BLOCKED (obj cannot define the new name)",
                       "SIZE_MISMATCH": "⛔ SIZE_MISMATCH (defines the name, "
                                        "WRONG BODY -- see detail)",
                       "UNKNOWN": "UNKNOWN"}[v]
        locals_.append(dict(addr=addr, unit=(unit or "-")[:26], size=size,
                            fuzzy=fuzzy, verdict=verdict, detail=detail,
                            pin=(f"{unit} [0x{s:08x}-0x{e:08x}]" if unit else "none"),
                            old=old, new=new))
    return verdicts, notes, rlabel, measures, locals_


def report(verdicts, notes, rlabel, measures, locals_, title):
    print(f"\n{'='*100}\n{title}\n{'='*100}")
    print(f"ruler: {rlabel}")
    if measures:
        print(f"baseline: matched_functions={measures.get('matched_functions')} "
              f"matched_code={measures.get('matched_code')} "
              f"code%={measures.get('matched_code_percent')}")
    print()
    hdr = f"{'ROW':<52} {'UNIT':<18} {'SIZE':>7} {'FUZZY':>9} {'now':>4} {'post':>4}  VERDICT"
    print(hdr)
    print("-" * len(hdr))
    gain = loss = 0
    for v in sorted(verdicts, key=lambda x: -abs(x.delta)):
        is_local = getattr(v, "kind", "caller") == "local"
        if v.crosses:
            verdict = f"CROSSES  +{v.size}" + ("  [local — see PAIRING term]" if is_local else "")
            if not is_local:
                gain += v.size
        elif v.falls:
            verdict = f"FALLS    -{v.size}" + ("  [local — see PAIRING term]" if is_local else "")
            if not is_local:
                loss += v.size
        elif v.cur > 0 and v.post < v.cur:
            verdict = f"improves (still {v.post} charged)"
        else:
            verdict = "no movement"
        print(f"{v.row[:52]:<52} {v.unit[:18]:<18} {v.size:>7} {v.fuzzy:>9.4f} "
              f"{v.cur:>4} {v.post:>4}  {verdict}")
        for e in v.events:
            print(f"      {e[0]:<26} T={str(e[1])[:34]:<34} B={str(e[2])[:34]:<34} -> {str(e[3])[:34]}")

    print("-" * len(hdr))
    print(f"  cascade GAIN  +{gain} B")
    print(f"  cascade LOSS  -{loss} B")
    print(f"  CASCADE POINT ESTIMATE (call-site name charges): {gain - loss:+d} B")

    # ---- LOCAL CHARGE term (HARD LIMIT #1c) ------------------------------
    # ★ This number did not exist before W36. The old tool printed local rows
    #   with a verdict but no byte total, and those verdicts were computed
    #   against the PRE-SWAP body. On the BandCamShot fixture the local channel
    #   is +1,552 B and the old model implied -396 B.
    locs = [v for v in verdicts if getattr(v, "kind", "caller") == "local"]
    if locs:
        lg = sum(v.size for v in locs if v.crosses)
        ll = sum(v.size for v in locs if v.falls)
        unrel = [v for v in locs if "body-swap-UNMODELLABLE" in v.uncertain]
        swaps = [v for v in locs
                 if str(getattr(v, "swap_status", "")).startswith("SWAP (")]
        print(f"  LOCAL CHARGE ESTIMATE (edited rows, post-swap body): "
              f"{lg - ll:+d} B   (+{lg} / -{ll})")
        if swaps:
            print(f"     {len(swaps)} of {len(locs)} edited rows are BODY SWAPS "
                  f"(our obj defines both spellings); their charges are priced "
                  f"against\n     the body objdiff will compare AFTER the "
                  f"rename, not the one it compares now.")
        if unrel:
            print(f"  ⛔ {len(unrel)} edited row(s) have an UNMODELLABLE body "
                  f"swap -- their local verdicts above are NOT trustworthy:")
            for v in unrel:
                print(f"       0x{getattr(v,'addr',0):08x}  "
                      f"{getattr(v,'swap_status','')}")

    # ---- LOCAL / PAIRING term -------------------------------------------
    # ⛔ MEASURED, NOT ASSUMED (lane W20, this tool's own round-trip): renaming
    # 11 addresses moved -2,976 B, of which only -580 was the call-site cascade.
    # The other -2,396 B was TEN ROWS VANISHING FROM THE REPORT ENTIRELY
    # (`fuzzy` absent, not merely below 100) because the pinned unit's obj could
    # not define the new spelling. That is a DIFFERENT CHANNEL from the one
    # priced above, it is bigger than it, and it is exactly W9's -180 B failure
    # mode at scale. It is reported separately because its byte value is only
    # bounded, never a point estimate: whether a newly-pairable row reaches
    # fuzzy==100 depends on the BODY, which cannot be diffed until the map edit
    # lands and the tree is re-split.
    if locals_:
        print()
        print("  LOCAL / PAIRING term -- the edited rows themselves "
              "(SEPARATE CHANNEL, not in the estimate above):")
        blocked = 0
        swapped = 0
        for l in locals_:
            print(f"    0x{l['addr']:08x}  {l['unit']:<26} {l['size']:>6} B  "
                  f"fuzzy={l['fuzzy']:>8.4f}  {l['verdict']}")
            print(f"        pin: {l['pin']}")
            print(f"        {l['detail']}")
            if l["verdict"].startswith("BLOCKED"):
                blocked += l["size"]
            if "SIZE_MISMATCH" in l["verdict"]:
                swapped += l["size"]
        if swapped:
            print(f"\n  ⛔ BODY-SWAP EXPOSURE: {swapped} B of rows whose obj DOES "
                  f"define the replacement name but\n     defines it as a "
                  f"DIFFERENT-SIZED body. These are the SCATTER-INCLUDE rows: one "
                  f".cpp\n     `#include`s another, so a single obj carries both "
                  f"spellings and the rename SWAPS\n     which body objdiff "
                  f"compares rather than creating a pairing. A family carrying "
                  f"any\n     of these CANNOT be renamed coherently -- reconcile "
                  f"the SOURCE first, then re-price.")
        if blocked:
            print(f"\n  ⛔ BLOCKED EXPOSURE: {blocked} B of rows whose pinned unit "
                  f"CANNOT define the replacement name.\n"
                  f"     Renaming these in place sends them to 0% PERMANENTLY. "
                  f"Re-home the pin to a unit whose obj\n"
                  f"     defines the name, and lift the spelling VERBATIM from "
                  f"that obj's symbol table.")

    # ---- CALLER-SPELLING CONSENSUS --------------------------------------
    # ★ THE SHAPE OF THE CALLER SET DISCRIMINATES A WRONG NAME FROM AN
    #   ARBITRARY ICF SURVIVOR NAME, AND IT IS THE ONLY MAP-INDEPENDENT TEST
    #   HERE THAT DOES.
    #
    # An ICF survivor's name is arbitrary (CLAUDE.md / W7's fixed-point trap):
    # every tree whose `clear` folded into one body reaches it, so OUR side
    # spells a DIFFERENT per-tree name at each site. Only one spelling can ever
    # match, so the cascade is structurally ZERO and no rename can collect it --
    # the remedy, if any, is a PROVEN alias, never a rename.
    #
    # A genuinely WRONG map name looks the opposite: the callers agree with each
    # other and disagree only with the map, so one repair clears every site.
    #
    # Measured on this tool's two real candidates: `map<int,bool>`'s builder had
    # 1 caller, and `clear@map<Symbol,CharLipSync*>`'s 62 call sites spelled at
    # least four different trees (`<H,..>`, `<PAVFaderGroup,..>`,
    # `<PAVTrackWidget,..>`, ...) -- dispersed, so +0, so W17's refusal to ship
    # it was right and is now quantified rather than argued.
    spell = {}
    for v in verdicts:
        for kind, tsym, bsym, newt in v.events:
            if kind.startswith(("PERSISTS", "CLEARED")):
                spell.setdefault(tsym, {}).setdefault(bsym, 0)
                spell[tsym][bsym] += 1
    if spell:
        print("\n  CALLER-SPELLING CONSENSUS (what OUR side calls at the charged sites):")
        for tsym, hist in spell.items():
            tot = sum(hist.values())
            top, topn = max(hist.items(), key=lambda kv: kv[1])
            frac = topn / tot
            if len(hist) == 1:
                tag = ("CONCENTRATED — every caller agrees on one spelling ⇒ "
                       "consistent with a WRONG MAP NAME; the cascade is collectable")
            elif frac >= 0.8:
                tag = (f"MOSTLY CONCENTRATED ({frac:.0%}) — a dominant spelling with "
                       f"{len(hist)-1} outlier spelling(s)")
            else:
                tag = ("DISPERSED ⇒ ARBITRARY ICF SURVIVOR. Callers spell "
                       f"{len(hist)} different names, so at most one can ever match "
                       "and the cascade is STRUCTURALLY ZERO. Do NOT rename to "
                       "capture these sites — that is picking the higher-scoring "
                       "arbitrary name, i.e. metric fitting. A PROVEN alias is the "
                       "only legitimate remedy.")
            print(f"    target {tsym[:56]}")
            print(f"      {tot} charged site(s), {len(hist)} distinct our-side spelling(s) -> {tag}")
            for b, n in sorted(hist.items(), key=lambda kv: -kv[1])[:5]:
                print(f"        {n:>3}x  {str(b)[:80]}")

    # A SIZE_MISMATCH row also moves `none`: swapping which body is compared is
    # visible to a ruler that ignores relocation NAMES but not bodies.
    pairing_change = any(l["verdict"].startswith(("BLOCKED", "PAIRS", "⛔ SIZE_MISMATCH"))
                         for l in locals_)
    if pairing_change:
        print(f"\n  PREDICTED `none` delta: NON-ZERO. This edit changes PAIRING "
              f"(a name added, removed, or\n     moved to an obj that defines it), "
              f"and `none` sees pairing even though it ignores\n     relocation "
              f"names. Do NOT pre-register `none` = 0 here.")
    else:
        print(f"\n  PREDICTED `none` delta: +0 B — this edit is a PURE RENAME "
              f"between two names both\n     already defined by the pinned objs, "
              f"so only relocation-name comparison moves.")
    if notes:
        print("\nNOTES / REFUSALS:")
        for n in dict.fromkeys(notes):
            print(f"  - {n}")
    return gain - loss


def parse_edits(args, root):
    edits = {}
    for e in args.edit or []:
        a, _, n = e.partition("=")
        if not n:
            raise SystemExit(f"--edit needs ADDR=NAME, got {e!r}")
        edits[int(a, 16)] = n
    if args.edit_file:
        for k, v in json.load(open(args.edit_file)).items():
            edits[int(k, 16)] = v
    if not edits:
        raise SystemExit("no edits given")
    return edits


# ---------------------------------------------------------------------------
# Frozen known-answer fixture -- W17's four cascade rows
# ---------------------------------------------------------------------------

def cmd_validate(args):
    root = args.project_dir
    fx = json.load(open(Path(root) / FIXTURE))
    smap = load_map(root)

    fwd = {int(k, 16): v["new"] for k, v in fx["edits"].items()}
    rev = {int(k, 16): v["old"] for k, v in fx["edits"].items()}

    # Which direction does this tree support?  Post-W17 trees carry the NEW
    # names, so the reproducible known-answer test is the INVERSE edit: the same
    # four rows must FALL by the same byte values they GAINED.
    # ⚠ An ANONYMOUS address is an ABSENT map key, not a key holding the literal
    # `fn_<addr>` spelling. Comparing against the spelling reads 10/11 and
    # refuses a tree that is genuinely in the fixture's old state.
    def at(addr):
        return smap.get(addr) or f"fn_{addr:08X}"

    at_new = sum(1 for k, v in fx["edits"].items() if at(int(k, 16)) == v["new"])
    at_old = sum(1 for k, v in fx["edits"].items() if at(int(k, 16)) == v["old"])
    n = len(fx["edits"])
    if at_new == n:
        direction, edits, sign = "INVERSE (post-W17 tree: undo the repair)", rev, -1
    elif at_old == n:
        direction, edits, sign = "FORWARD (pre-W17 tree: apply the repair)", fwd, +1
    else:
        raise SystemExit(
            f"REFUSED: tree is in neither fixture state ({at_new}/{n} at new "
            f"names, {at_old}/{n} at old). The fixture cannot be scored here.")

    print(f"W17 cascade fixture -- direction: {direction}")
    print(f"expected cascade rows ({len(fx['cascade'])}), from commit "
          f"{fx['provenance']['commit']}:")
    for c in fx["cascade"]:
        print(f"   {c['row'][:60]:<60} {sign*c['bytes']:+d} B")
    print()

    verdicts, notes, rlabel, measures, locals_ = gather(root, edits, args.ruler)
    got = report(verdicts, notes, rlabel, measures, locals_,
                 "W17 KNOWN-ANSWER FIXTURE")

    # Score ONLY the cascade rows: the local (in-patch) rows are re-homes as well
    # as renames, and a re-home's pairing term is explicitly out of this model.
    byrow = {v.row: v for v in verdicts}
    print(f"\n{'='*100}\nSCORING vs the frozen fixture\n{'='*100}")
    ok = True
    for c in fx["cascade"]:
        want = sign * c["bytes"]
        v = byrow.get(c["row"])
        have = v.delta if v else None
        good = (have == want)
        ok &= good
        print(f"  {'ok  ' if good else 'FAIL'}  {c['row'][:58]:<58} "
              f"want {want:+6d}  got {('%+d' % have) if have is not None else 'ROW NOT REACHED'}")
    total_want = sign * sum(c["bytes"] for c in fx["cascade"])
    total_have = sum(byrow[c["row"]].delta for c in fx["cascade"] if c["row"] in byrow)
    print(f"\n  cascade total  want {total_want:+d}   got {total_have:+d}")

    if not ok:
        print("\nFAIL: the pricer does not reproduce W17's measured cascade.")
        return 1
    print("\nPASS: every W17 cascade row reproduced exactly, by enumeration "
          "rather than by hand.")
    return 0


# ---------------------------------------------------------------------------
# NEGATIVE CONTROL for the size check (HARD LIMIT #1b)
#
# ★ A REPAIR WHOSE TEST CANNOT GO RED IS WORTH NOTHING. This project has been
#   burned repeatedly by instruments that confirm whatever they are pointed at:
#   a `/GS` cookie detector that scored 0 on a known-`/GS` object; a `grep` that
#   cannot match inside binaries; a `fuzzy == mpn` certificate that is trivially
#   true on the unpaired rows it was applied to. So this selftest is built to
#   FAIL, and `--self-break` demonstrates that it does.
#
# It derives BOTH populations from the live tree rather than hardcoding a
# fixture, so it cannot rot into agreement with a changed tree:
#
#   GREEN population -- every row at `fuzzy == 100` that has both a retail
#       extent and a readable base COMDAT. These PROVABLY match, so the check
#       must fire ZERO times. (Tighten SIZE_TOLERANCE_B to 0 and this goes red.)
#   RED population -- rows whose |delta| >= 8. The check must fire on all of
#       them. (Remove the size check and this goes red -- that is --self-break.)
#
# ⛔ VACUITY GUARD, and it is the point. `all([])` is True, so an empty
#   population would make both assertions pass and print a confident green.
#   Both populations are floor-checked and the selftest REFUSES (exit 2) rather
#   than passing on no data -- the failure mode that produced this project's
#   vacuous `/GS` detector.
# ---------------------------------------------------------------------------

def _size_population(root, verbose=True):
    """(green, red, skipped) row lists of (name, unit, retail_ext, our_size, fuzzy)."""
    import os
    smap = load_map(root)
    byname = {}
    for a, n in smap.items():
        byname.setdefault(n, []).append(a)
    ext_by_addr = {a: sz for a, sz, _n in load_symbols(root)}
    # ⚠ Refuse names that are not UNIQUE in the map: a duplicate spelling cannot
    # be attributed to one extent, and guessing would inject the very
    # false-confidence this check exists to remove.
    ext_by_name = {n: ext_by_addr[a[0]] for n, a in byname.items()
                   if len(a) == 1 and a[0] in ext_by_addr}
    rep = json.load(open(Path(root) / "build" / VERSION / "report.json"))
    units = {u["name"]: u for u in _objdiff_units(root)}
    green, red, skipped = [], [], 0
    for u in rep.get("units", []):
        meta = units.get(u.get("name"))
        fns = u.get("functions") or []
        if not meta or not fns:
            continue
        bp = meta.get("base_path")
        if not bp or not os.path.exists(Path(root) / bp):
            continue
        sizes = defined_body_sizes(root, bp)
        for f in fns:
            nm = f.get("name", "")
            fz = float(f.get("fuzzy_match_percent", 0.0) or 0.0)
            e, s = ext_by_name.get(nm), sizes.get(nm)
            if e is None or s is None:
                skipped += 1
                continue
            row = (nm, u["name"], e, s, fz)
            if fz >= 100.0:
                green.append(row)
            elif abs(e - s) > CALIBRATED_TOLERANCE_B:
                red.append(row)
        if verbose:
            print(".", end="", flush=True)
    if verbose:
        print()
    return green, red, skipped


# ⚠ The POPULATION is selected with the frozen calibrated value, never with the
# mutable SIZE_TOLERANCE_B under test. Otherwise `--self-break` would empty the
# red population and trip the vacuity refusal instead of producing the RED it
# exists to demonstrate -- a self-break that cannot break.
CALIBRATED_TOLERANCE_B = 4

MIN_GREEN, MIN_RED = 1000, 1


def cmd_selftest(args):
    global SIZE_TOLERANCE_B
    root = args.project_dir
    print("cascade_price size-check SELFTEST (HARD LIMIT #1b negative control)")
    print(f"  tolerance: |retail_extent - our_COMDAT_size| > {SIZE_TOLERANCE_B} B fires")
    if args.self_break:
        print("  --self-break: DISABLING the size check; the RED assertion MUST fail.")
        SIZE_TOLERANCE_B = 1 << 30                 # size-blind, i.e. the OLD tool
    green, red, skipped = _size_population(root)
    print(f"\npopulations: green(fuzzy==100)={len(green)}  "
          f"red(|delta|>{4})={len(red)}  unreachable={skipped}")

    if len(green) < MIN_GREEN or len(red) < MIN_RED:
        print(f"\nREFUSED (exit 2): population too small to test "
              f"(need >= {MIN_GREEN} green and >= {MIN_RED} red).\n"
              f"  An empty population makes BOTH assertions vacuously true, so "
              f"this refuses rather than\n  printing a green that means nothing. "
              f"Did the tree build? Did the target-symbol renamer run?")
        return 2

    # --- GREEN: the check must never fire on a provably-matching row ---------
    fp = [r for r in green if abs(r[2] - r[3]) > SIZE_TOLERANCE_B]
    print(f"\nGREEN assertion -- 0 fires among {len(green)} rows at fuzzy==100")
    print(f"  fires: {len(fp)}   {'ok' if not fp else 'FAIL'}")
    for r in fp[:8]:
        print(f"    FP {r[0][:58]:<58} retail={r[2]} ours={r[3]}")

    # --- RED: the check must fire on every row it is supposed to ------------
    hit = [r for r in red if abs(r[2] - r[3]) > SIZE_TOLERANCE_B]
    print(f"\nRED assertion -- fires on all {len(red)} rows whose |delta| > 4")
    print(f"  fires: {len(hit)}/{len(red)}   "
          f"{'ok' if len(hit) == len(red) else 'FAIL'}")
    for r in sorted(red, key=lambda r: -abs(r[2] - r[3]))[:5]:
        print(f"    RED {r[0][:58]:<58} retail={r[2]} ours={r[3]} "
              f"({r[2]-r[3]:+d}) fuzzy={r[4]:.4f}")

    ok = (not fp) and len(hit) == len(red)
    if args.self_break:
        if ok:
            print("\nSELF-BREAK FAILED: the size check was disabled and the "
                  "selftest STILL PASSED.\n  That means the test cannot go red "
                  "and proves nothing. Treat it as broken.")
            return 1
        print("\nSELF-BREAK OK: with the size check disabled the selftest goes "
              "RED, so it discriminates\n  rather than confirming whatever it is "
              "pointed at.")
        return 0
    if not ok:
        print("\nFAIL: the size check does not discriminate on this tree.")
        return 1
    print(f"\nPASS: 0 false positives over {len(green)} provably-matching rows, "
          f"and the check fires on\n  all {len(red)} divergent rows. Run with "
          f"--self-break to see it go red.")
    return 0


# ---------------------------------------------------------------------------
# NEGATIVE CONTROL for the body-swap model (HARD LIMIT #1c)
#
# ★ Same discipline as the size selftest above: the fixture's POPULATION is a
#   FROZEN list of 11 addresses in the fixture file, NEVER derived from the
#   model under test. That is deliberate -- W29's note records that a
#   self-break spelled the obvious way (deriving its rows from the thing it is
#   disabling) empties its own red set and trips the vacuity refusal INSTEAD of
#   producing a red. A self-break that cannot break is the disease one level up.
#
# The expectations are the MEASURED PRE/POST report closure of the real
# BandCamShot rename (+2,364 B matched_code, row-level sum +3,684/-1,320),
# not the tool's own output. So the fixture can convict the tool.
# ---------------------------------------------------------------------------

def cmd_validate_swap(args):
    root = args.project_dir
    fx = json.load(open(Path(root) / SWAP_FIXTURE))
    smap = load_map(root)

    def at(addr):
        return smap.get(addr) or f"fn_{addr:08X}"

    n = len(fx["edits"])
    at_new = sum(1 for k, v in fx["edits"].items() if at(int(k, 16)) == v["new"])
    at_old = sum(1 for k, v in fx["edits"].items() if at(int(k, 16)) == v["old"])
    if at_new == n:
        direction, sign = "INVERSE (tree carries the Band names: undo it)", -1
        edits = {int(k, 16): v["old"] for k, v in fx["edits"].items()}
    elif at_old == n:
        direction, sign = "FORWARD (tree carries the Ham names: apply it)", +1
        edits = {int(k, 16): v["new"] for k, v in fx["edits"].items()}
    else:
        raise SystemExit(
            f"REFUSED: tree is in neither fixture state ({at_new}/{n} at new "
            f"names, {at_old}/{n} at old). The fixture cannot be scored here.")

    print(f"W36 BODY-SWAP fixture -- direction: {direction}")
    print(f"provenance: {fx['provenance']['closure']}")
    if args.self_break:
        print("\n⚠ --self-break: the base-body swap model is DISABLED. The "
              "fixture MUST now fail.")

    verdicts, notes, rlabel, measures, locals_ = gather(
        root, edits, args.ruler, model_swap=not args.self_break)
    report(verdicts, notes, rlabel, measures, locals_,
           "W36 BODY-SWAP KNOWN-ANSWER FIXTURE")

    by_addr = {getattr(v, "addr", None): v for v in verdicts
               if getattr(v, "kind", "") == "local"}
    by_row = {v.row: v for v in verdicts}

    print(f"\n{'='*100}\nSCORING vs the frozen fixture (measured, not modelled)"
          f"\n{'='*100}")
    ok, reached = True, 0
    print("  LOCAL channel -- the rows being renamed (this is what W36 fixed):")
    for l in fx["local"]:
        addr = int(l["addr"], 16)
        want = sign * l["bytes"]
        v = by_addr.get(addr)
        have = v.delta if v else None
        if v is not None:
            reached += 1
        good = (have == want)
        ok &= good
        print(f"    {'ok  ' if good else 'FAIL'}  0x{addr:08x}  want {want:+6d}  "
              f"got {('%+d' % have) if have is not None else 'ROW NOT REACHED'}")
    print("  CASCADE channel -- unedited callers (unchanged by W36):")
    creached = 0
    for c in fx["cascade"]:
        want = sign * c["bytes"]
        v = by_row.get(c["row"])
        have = v.delta if v else None
        if v is not None:
            creached += 1
        good = (have == want)
        ok &= good
        print(f"    {'ok  ' if good else 'FAIL'}  {c['row'][:52]:<52} "
              f"want {want:+6d}  got "
              f"{('%+d' % have) if have is not None else 'ROW NOT REACHED'}")

    # ⛔ VACUITY GUARD. `all([])` is True, so a fixture whose rows all failed to
    #    join would score a confident PASS over nothing. W29's first calibration
    #    did exactly that ("0 disagreements" over ZERO rows) and was caught only
    #    by a not-reached counter. Count reach explicitly and REFUSE.
    print(f"\n  rows reached: local {reached}/{len(fx['local'])}, "
          f"cascade {creached}/{len(fx['cascade'])}")
    if reached < len(fx["local"]) or creached < len(fx["cascade"]):
        print("\nREFUSED (exit 2): not every fixture row was reached, so a PASS "
              "would be scored over\n  a partly-empty population. This is the "
              "`all([])` failure mode, not a result.")
        return 2

    want_local = sign * sum(l["bytes"] for l in fx["local"])
    got_local = sum(by_addr[int(l["addr"], 16)].delta for l in fx["local"])
    want_casc = sign * sum(c["bytes"] for c in fx["cascade"])
    got_casc = sum(by_row[c["row"]].delta for c in fx["cascade"])
    print(f"  LOCAL   want {want_local:+d}  got {got_local:+d}")
    print(f"  CASCADE want {want_casc:+d}  got {got_casc:+d}")
    print(f"  TOTAL   want {want_local+want_casc:+d}  got {got_local+got_casc:+d}"
          f"   (measured whole-binary matched_code delta: "
          f"{sign*fx['provenance']['measured_delta']:+d})")

    if args.self_break:
        if ok:
            print("\nSELF-BREAK FAILED: the body-swap model was disabled and the "
                  "fixture STILL PASSED.\n  The fixture cannot go red and proves "
                  "nothing. Treat it as broken.")
            return 1
        print("\nSELF-BREAK OK: with the body-swap model disabled the fixture "
              "goes RED, so it\n  discriminates rather than confirming whatever "
              "it is pointed at.")
        return 0
    if not ok:
        print("\nFAIL: the pricer does not reproduce the measured BandCamShot "
              "rename.")
        return 1
    print("\nPASS: every local AND cascade row reproduced the MEASURED A/B "
          "exactly, by\n  enumeration. Run with --self-break to see it go red.")
    return 0


def cmd_price(args):
    root = args.project_dir
    edits = parse_edits(args, root)
    verdicts, notes, rlabel, measures, locals_ = gather(root, edits, args.ruler)
    report(verdicts, notes, rlabel, measures, locals_,
           "PROPOSED MAP EDIT -- CASCADE PRICE")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["price", "validate", "selftest",
                                    "validate-swap"])
    ap.add_argument("--self-break", action="store_true",
                    help="selftest/validate-swap: disable the check under test "
                         "and PROVE it goes red (a test that cannot fail is "
                         "worth nothing)")
    ap.add_argument("--edit", action="append", help="0xADDR=NewMangledName")
    ap.add_argument("--edit-file", help='JSON {"0xaddr": "NewName", ...}')
    ap.add_argument("--project-dir", default=str(ROOT))
    ap.add_argument("--ruler", default="graded",
                    choices=["graded", "none", "data_value"])
    a = ap.parse_args()
    sys.exit({"validate": cmd_validate,
              "validate-swap": cmd_validate_swap,
              "selftest": cmd_selftest,
              "price": cmd_price}[a.cmd](a))


if __name__ == "__main__":
    main()
