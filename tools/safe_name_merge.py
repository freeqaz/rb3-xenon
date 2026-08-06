#!/usr/bin/env python3
"""Collision-safe naming GATE for target_symbol_map.json — the load-bearing
invariant every bulk naming/pinning sweep MUST pass through before merge.

WHY (proven 2026-06-03)
-----------------------
`scripts/obj_target_symbol_renamer.py` does NO collision/alias checking: it
blindly applies the addr->name map onto dtk-split target objs. objdiff pairs
symbols by NAME per unit obj, so a *duplicate* mangled name (two addrs, same
name, landing in the same unit) makes objdiff's pairing ambiguous and SUPPRESSES
the real match. A naive bulk-name of ~1821 candidates regressed -138 this way.
Collision-safe + Ham/Band-correct + ICF-canonical-only naming landed a clean +73.

Because the renamer is dumb, ALL safety must be an invariant of the MAP ITSELF.
This tool is that invariant. It takes a candidate `{addr:name}` fragment and the
live map and emits only the SAFE-to-add subset (+ a rejection report).

THE INVARIANT — emit (addr -> name) iff ALL hold:
  1. addr not already mapped in tsm (skip-existing by address; never clobber a
     verified/landed name, incl. ICF aliases the relocators already pinned).
  2. name (after Ham->Band normalization) is not already a VALUE in tsm, AND is
     unique within the candidate batch. If 2+ candidate addrs want the same
     normalized name, drop ALL of them UNLESS exactly one is the ICF-canonical
     for its unit (rule 4).
  3. Ham/Band normalize: RB3 `Band*` == DC3 `Ham*`. If a name references a `Ham<X>`
     class that has a `Band<X>` counterpart RB3 actually defines, emit the
     `Band<X>` form. If `Band<X>` is NOT a known RB3 class AND `Ham<X>` is NOT a
     known RB3 hamobj class either, the name is a PHANTOM our base never defines
     -> reject. (RB3-360 kept some `Ham*` hamobj classes verbatim, e.g.
     HamCamTransform / HamMove — those are NOT phantoms.)
  4. ICF-canonical-only: ICF family names (`??_G@`, `??_E@`, `?StaticClassName@`,
     `??$`-template helpers, `?_M_`...) are byte-identical across many classes at
     many addrs; objdiff can pair only ONE per unit. For an ICF name wanted by
     >1 candidate addr, assign it to AT MOST ONE addr -- the canonical: the addr
     that exact-content-matched (in dc3_content_match.json / game_content_match),
     else the addr whose owning unit's basename matches the class the name
     references; else DROP the whole family group (can't disambiguate). Folded
     twins are left unnamed.

CANDIDATE SCHEMAS accepted (auto-detected per record OR a flat {addr:name} dict):
  - flat            {"0xADDR": "mangled", ...}
  - dc3_content     [{"rb3_addr","dc3_name","dc3_obj",...}]
  - game_content    [{"rb3_addr","mangled_name","unit",...}]
  - fuzzy/global    [{"rb3_addr","dc3_name","dc3_obj"/"unit",...}]

USAGE
-----
  # gate a fragment, write the safe subset + print rejection report:
  tools/safe_name_merge.py --gate cand.json --out safe.json
  tools/safe_name_merge.py --gate cand.json            # stdin/stdout ok (-)
  cat cand.json | tools/safe_name_merge.py --gate -

  # merge the safe subset INTO a tsm (writes tsm in place; default dry):
  tools/safe_name_merge.py --gate cand.json --merge --tsm <worktree>/.../target_symbol_map.json

  # audit the LIVE tsm for existing same-unit duplicate names (informational):
  tools/safe_name_merge.py --audit
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc3_obj_source import DC3_OBJ_DIR as DC3_DIR  # canonical retail-DC3 target tree

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEF_TSM = os.path.join(ROOT, "scripts", "target_symbol_map.json")
DEF_SPLITS = os.path.join(ROOT, "config", "45410914", "splits.txt")
DEF_SYMBOLS = os.path.join(ROOT, "config", "45410914", "symbols.txt")
DEF_OBJECTS = os.path.join(ROOT, "config", "45410914", "objects.json")
DC3_CONTENT = os.path.join(ROOT, "dc3_content_match.json")
GAME_CONTENT = os.path.join(ROOT, "game_content_match.json")

# ICF families — byte-identical across classes, objdiff pairs <=1 per unit.
# Match on the MANGLED prefix / substring.
ICF_PREFIXES = ("??_G", "??_E", "??$", "??_F", "??__E", "??__F")
ICF_SUBSTR = ("StaticClassName", "?_M_", "@_M_")


def is_icf(name):
    if name.startswith(ICF_PREFIXES):
        return True
    return any(s in name for s in ICF_SUBSTR)


# NON-REAL / build-environment symbols — these are NOT functions with a stable
# cross-compile identity. They carry a per-TU sequence counter (MSVC EH/unwind
# funclets, `__unwind$NNNNNN`, `__catch$`, `__sep$`, `__ehhandler$`, GS cookie
# handlers, safeseh thunks) or an address-derived placeholder
# (`fn_8XXXXXXX`, `lbl_8XXXXXXX`, `jumptable_8XXXXXXX`). objdiff already pairs
# unwind funclets in pinned units BY ADDRESS as `fn_<addr>` and they match 100%
# for free. Naming a target funclet with DC3's `__unwind$<DC3-number>` forces a
# NAME pairing against our base obj's `__unwind$<RB3-number>` (a different per-TU
# counter) -> the name matches nothing -> the funclet UN-PAIRS and drops from
# 100. This is the recurring BandDirector -16 (and CharIKFoot/Rnd/ContextChecker)
# regression: a span dense in EH funclets gets each one named with a DC3 counter
# and loses every address-paired funclet. PROVEN 2026-06-03: applying the 22
# `__unwind$` names in safe.json drops BandDirector 154->141. The fix is to NEVER
# emit a name whose IDENTITY is build-environment-specific.
NON_REAL_PREFIXES = (
    "__unwind$",     # EH unwind funclet, per-TU sequence counter
    "__catch$",      # EH catch funclet
    "__ehhandler$",  # EH handler thunk
    "__sep$",        # separated/cold code chunk
    "__GSHandler",   # /GS cookie check handler
    "__safe_se",     # SafeSEH handler thunk
    "__tls",         # TLS init/guard helper
    "fn_",           # dtk address placeholder (no real identity)
    "lbl_",          # dtk address-label placeholder
    "jumptable_",    # dtk jump-table data placeholder
    "$LN",           # MSVC local code label leaked as a symbol
    "__xmm@",        # SIMD literal pool symbol
)

# RATIFIED 2026-08-06 (owner ruling, doc 62 §3): `__real@<hex>` was REMOVED from
# NON_REAL_PREFIXES. It never belonged there. The list's rationale is
# "per-TU sequence counter or address-derived placeholder with no stable
# cross-compile identity"; `__real@<hex>` is CONTENT-derived — MSVC spells it
# deterministically from the constant's bytes, our own base objects emit exactly
# that symbol at exactly that relocation slot, and every instance proposed so far
# was content-VERIFIED against retail `band.exe` (doc 55 §4, doc 58 §3, doc 60
# §3/§4). It was swept in by analogy with `$LN`/`fn_`/`__unwind$`, which are
# genuinely address-derived, and the analogy was wrong.
#
# SCOPE OF THE RULING — read before touching this list again:
#   * ONLY `__real@` was ratified. `$LN`, `fn_`, `lbl_`, `jumptable_`,
#     `__unwind$` and the rest stay refused; nothing else in the gate weakens.
#   * `__xmm@` (SIMD literal pool) is content-derived by the SAME argument and
#     is the obvious next candidate — it was deliberately NOT relaxed here
#     because the ruling names `__real@` and nothing else. It needs its own
#     ruling (reported, not acted on).
#   * The ruling does NOT touch ICF routing: an address proposing more than one
#     tree-wide name still goes to the alias mechanism and never to the name
#     map, `__real@` or not. That is where `0x82000d78` (`__real@00000000`,
#     1,070 witnesses + 2 singletons) still sits.


def is_non_real_symbol(name):
    """True for compiler/linker-generated pseudo-symbols whose name has NO
    stable cross-compile identity (per-TU sequence counters or address-derived
    placeholders). Naming a target obj symbol with one of these forces objdiff
    into a name-pairing it cannot satisfy, UN-PAIRING funclets it already matched
    by address. These must never be emitted by the gate."""
    return name.startswith(NON_REAL_PREFIXES)


# ---------------------------------------------------------------------------
# splits / symbols parsing (reused shape from relocate_engine_splits.py)
# ---------------------------------------------------------------------------
UNIT_RE = re.compile(r"^(\S+\.(?:cpp|c|cc)):\s*$")
TEXT_RE = re.compile(r"^(\s*)\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")


def parse_splits(path):
    """Return list of (lo, hi, basename_lower, cpp) for every .text pin."""
    units = []
    cur = None
    for line in open(path):
        m = UNIT_RE.match(line)
        if m:
            cur = m.group(1)
            continue
        if cur:
            t = TEXT_RE.match(line)
            if t:
                lo = int(t.group(2), 16)
                hi = int(t.group(3), 16)
                base = os.path.splitext(cur.split("/")[-1])[0].lower()
                units.append((lo, hi, base, cur))
                cur = None  # only first .text per unit
    units.sort()
    return units


def unit_of_addr(addr, splits):
    """Return (base_lower, cpp) of the pinned unit containing addr, or None."""
    for lo, hi, base, cpp in splits:
        if lo <= addr < hi:
            return base, cpp
    return None


def load_sizes(path):
    sizes = {}
    rx = re.compile(
        r"fn_([0-9A-Fa-f]+) = \.text:0x[0-9A-Fa-f]+; // type:function size:0x([0-9A-Fa-f]+)"
    )
    for line in open(path):
        m = rx.match(line)
        if m:
            sizes[int(m.group(1), 16)] = int(m.group(2), 16)
    return sizes


# ---------------------------------------------------------------------------
# Ham/Band authority
# ---------------------------------------------------------------------------
def build_band_authority(tsm, objects_path, splits):
    """Set of class basenames `X` for which RB3 defines `Band<X>` (so a DC3
    `Ham<X>` reference must be substituted to `Band<X>`), and the set of `Ham<X>`
    classes RB3 keeps verbatim (so `Ham<X>` is NOT a phantom)."""
    band_classes = set()  # full class names like 'BandSong'
    ham_classes = set()  # hamobj class names RB3 kept, like 'HamCamTransform'

    # (a) Band* / Ham* class names already used as VALUES in the tsm.
    for k, v in tsm.items():
        if not k.lower().startswith("0x"):
            continue
        for m in re.finditer(r"\b(Band[A-Za-z0-9_]+)@", v):
            band_classes.add(m.group(1))
        for m in re.finditer(r"\b(Ham[A-Za-z0-9_]+)@", v):
            ham_classes.add(m.group(1))

    # (b) Band* class declarations in our src tree.
    src_band = os.path.join(ROOT, "src")
    try:
        import subprocess

        out = subprocess.run(
            ["grep", "-rhoE", r"\b(class|struct)\s+Band[A-Za-z0-9_]+", src_band],
            capture_output=True, text=True,
        ).stdout
        for line in out.splitlines():
            m = re.search(r"(Band[A-Za-z0-9_]+)", line)
            if m:
                band_classes.add(m.group(1))
    except Exception:
        pass

    # (c) objects.json hamobj units named HamX.cpp => RB3 keeps Ham<X> verbatim;
    #     units named BandX.cpp => RB3 has Band<X>.
    try:
        o = json.load(open(objects_path))
        for grp, gd in o.items():
            if not isinstance(gd, dict):
                continue
            for fn in gd.get("objects", {}):
                b = os.path.splitext(fn.split("/")[-1])[0]
                if b.startswith("Ham"):
                    ham_classes.add(b)
                elif b.startswith("Band"):
                    band_classes.add(b)
    except Exception:
        pass

    # (d) splits.txt pinned units (authoritative for "this Ham*.cpp is pinned").
    for _, _, base, cpp in splits:
        b = os.path.splitext(cpp.split("/")[-1])[0]
        if b.startswith("Ham"):
            ham_classes.add(b)
        elif b.startswith("Band"):
            band_classes.add(b)

    # (e) DC3 Ham*.obj basenames whose Band* counterpart RB3 defines -> the X
    #     set for substitution. We already capture Band* directly above; this
    #     just records the Ham bases seen in DC3 so we know a class is "known".
    band_x = {c[len("Band"):] for c in band_classes}
    return band_x, band_classes, ham_classes


# A class token `Ham<X>` begins right after a mangle delimiter: '@' (nested
# name), a digit (e.g. ??0Ham..., the ctor op), or an operator letter run like
# ??_G / ??_E (so 'GHamSong' -> the 'G' is the op code, class starts at 'Ham').
# A bare \b fails on '0Ham'/'GHam' (both word chars). Match Ham preceded by one
# of @, a digit, an uppercase op letter, or string start; capture up to next '@'.
HAM_REF_RE = re.compile(r"(?:^|[@0-9A-Z])Ham([A-Z][A-Za-z0-9_]*?)(?=@)")


def normalize_ham_band(name, band_x, ham_classes):
    """Apply Ham->Band substitution where RB3 has the Band counterpart.

    Returns (normalized_name, status) where status in:
      'ok'        - no Ham refs, or all Ham refs are RB3-kept hamobj classes.
      'subst'     - one or more Ham<X> -> Band<X> substitutions applied.
      'phantom'   - references a Ham<X> with NO Band<X> in RB3 AND Ham<X> is not
                    a known RB3 hamobj class -> our base never defines it.
    """
    refs = HAM_REF_RE.findall(name)
    if not refs:
        return name, "ok"

    phantom = False
    did_subst = False
    out = name
    # Substitute the longest class refs first to avoid partial overlaps.
    for x in sorted(set(refs), key=len, reverse=True):
        ham_full = "Ham" + x
        band_full = "Band" + x
        if x in band_x:
            # RB3 renamed this class to Band<X>; substitute the class token.
            # Use a lookbehind on the delimiter so we don't consume it (the
            # token begins after @ / a digit / an op letter / string start).
            new = re.sub(r"(?<=[@0-9A-Z])Ham" + re.escape(x) + r"(?=@)",
                         band_full, out)
            new = re.sub(r"^Ham" + re.escape(x) + r"(?=@)", band_full, new)
            if new != out:
                out = new
                did_subst = True
        elif ham_full in ham_classes:
            # RB3 kept Ham<X> verbatim (e.g. HamCamTransform) -> leave as-is.
            pass
        else:
            # No Band counterpart, not a known kept Ham class -> phantom.
            phantom = True

    if phantom:
        return out, "phantom"
    return out, ("subst" if did_subst else "ok")


# ---------------------------------------------------------------------------
# candidate loading (multi-schema)
# ---------------------------------------------------------------------------
def norm_addr(a):
    if isinstance(a, int):
        v = a
    else:
        v = int(str(a), 16)
    return "0x%08X" % v


def load_candidates(path):
    """Return list of dicts {addr, name, owner} (owner = owning unit basename
    lower or None) from any accepted schema."""
    if path == "-":
        raw = json.load(sys.stdin)
    else:
        raw = json.load(open(path))

    cands = []
    if isinstance(raw, dict):
        for k, v in raw.items():
            if not str(k).lower().startswith("0x"):
                continue
            cands.append({"addr": norm_addr(k), "name": v, "owner": None})
        return cands

    for e in raw:
        if not isinstance(e, dict):
            continue
        addr = e.get("rb3_addr") or e.get("addr")
        name = e.get("dc3_name") or e.get("mangled_name") or e.get("name")
        if not addr or not name:
            continue
        owner = None
        # game_content uses 'unit' = path; dc3/global use 'dc3_obj' = X.obj
        if e.get("unit"):
            owner = os.path.splitext(e["unit"].split("/")[-1])[0].lower()
        elif e.get("dc3_obj"):
            b = os.path.splitext(e["dc3_obj"].split("/")[-1])[0].lower()
            # DC3 Ham*.obj -> RB3 Band*.obj owner alias
            if b.startswith("ham"):
                owner = "band" + b[3:]
            else:
                owner = b
        cands.append({"addr": norm_addr(addr), "name": name, "owner": owner})
    return cands


def load_exact_canonical():
    """addr(int) -> True for addrs that EXACT-content-matched (dc3/game). These
    are the strongest ICF-canonical signal."""
    exact = set()
    for p in (DC3_CONTENT, GAME_CONTENT):
        if os.path.isfile(p):
            for e in json.load(open(p)):
                a = e.get("rb3_addr")
                if a:
                    exact.add(int(a, 16))
    return exact


# ---------------------------------------------------------------------------
# the gate
# ---------------------------------------------------------------------------
def class_of_name(name):
    """Heuristic: the immediate class basename a mangled name refers to.
    `?Foo@BandSong@@...` -> 'bandsong'; `??_GBandSong@@...` -> 'bandsong'."""
    # strip leading ?? / ? and any ??_G / ??_E decoration
    m = re.search(r"@([A-Za-z_][A-Za-z0-9_]*)@@", name)
    if m:
        return m.group(1).lower()
    # ??_G<Class>@@ form
    m = re.match(r"\?\?[_$][A-Za-z]?([A-Za-z_][A-Za-z0-9_]*)@@", name)
    if m:
        return m.group(1).lower()
    return None


def gate(cands, tsm, splits, band_x, ham_classes, exact_canonical):
    tsm_addrs = {k.lower() for k in tsm if k.lower().startswith("0x")}
    tsm_names = {v for k, v in tsm.items() if k.lower().startswith("0x")}

    reasons = defaultdict(int)
    safe = {}

    # Pass 1: per-candidate filters (addr-exists, phantom, normalize).
    staged = []  # (addr, norm_name, owner, is_icf, is_exact)
    for c in cands:
        addr = c["addr"]
        if addr.lower() in tsm_addrs:
            reasons["addr_exists"] += 1
            continue
        # Reject build-environment pseudo-symbols (EH funclets / address
        # placeholders) BEFORE any normalization: their identity is per-TU and
        # un-pairs objdiff's address-matched funclets. See is_non_real_symbol.
        if is_non_real_symbol(c["name"]):
            reasons["non_real_symbol"] += 1
            continue
        norm_name, status = normalize_ham_band(c["name"], band_x, ham_classes)
        if status == "phantom":
            reasons["ham_band_phantom"] += 1
            continue
        if norm_name in tsm_names:
            reasons["name_collision_tsm"] += 1
            continue
        staged.append({
            "addr": addr,
            "name": norm_name,
            "owner": c["owner"],
            "icf": is_icf(norm_name),
            "exact": int(addr, 16) in exact_canonical,
        })

    # Pass 2: within-batch dedupe by NAME.
    by_name = defaultdict(list)
    for s in staged:
        by_name[s["name"]].append(s)

    for name, group in by_name.items():
        if len(group) == 1:
            safe[group[0]["addr"]] = name
            continue
        # multiple candidate addrs want the same name.
        if not is_icf(name):
            # non-ICF duplicate name -> drop ALL (wrong-identity / ambiguous).
            reasons["batch_dup_nonicf"] += len(group)
            continue
        # ICF family: pick at most ONE canonical addr.
        canon = pick_icf_canonical(name, group, splits)
        if canon is None:
            reasons["icf_indeterminate"] += len(group)
            continue
        safe[canon["addr"]] = name
        reasons["icf_folded_twin"] += len(group) - 1

    return safe, reasons


def pick_icf_canonical(name, group, splits):
    """Choose the single canonical addr for an ICF name among contenders.

    Priority:
      1. addr that exact-content-matched (group[i]['exact']) — strongest.
      2. addr whose OWNING PINNED UNIT basename matches the class the name
         references (the unit whose base obj actually defines it).
      3. addr whose declared 'owner' (from candidate metadata) matches that class.
      else: indeterminate -> None (leave the whole family unnamed).
    Also: never name two ICF twins in the SAME unit (the suppressing case).
    """
    # 1. exact-match wins, but only if EXACTLY one is exact (else ambiguous).
    exacts = [g for g in group if g["exact"]]
    if len(exacts) == 1:
        return exacts[0]

    cls = class_of_name(name)

    # annotate each with its owning pinned unit basename
    for g in group:
        ub = unit_of_addr(int(g["addr"], 16), splits)
        g["_unit"] = ub[0] if ub else None

    # 2. owning-pinned-unit basename matches the class.
    if cls:
        unit_matches = [g for g in group if g["_unit"] and cls in g["_unit"]]
        if len(unit_matches) == 1:
            return unit_matches[0]
        if len(unit_matches) > 1:
            # multiple twins in matching units -> can't disambiguate safely.
            return None

    # 3. candidate-declared owner matches the class.
    if cls:
        owner_matches = [g for g in group if g["owner"] and cls in g["owner"]]
        if len(owner_matches) == 1:
            return owner_matches[0]

    return None


# ---------------------------------------------------------------------------
# audit: existing same-unit duplicate names in the LIVE tsm
# ---------------------------------------------------------------------------
def audit(tsm, splits):
    by_name = defaultdict(list)
    for k, v in tsm.items():
        if not k.lower().startswith("0x"):
            continue
        by_name[v].append(int(k, 16))

    dup_groups = {n: addrs for n, addrs in by_name.items() if len(addrs) > 1}
    print(f"[audit] {len(dup_groups)} duplicate names / "
          f"{sum(len(a) for a in dup_groups.values())} addrs in tsm")

    same_unit = []
    for name, addrs in dup_groups.items():
        units = {}
        for a in addrs:
            ub = unit_of_addr(a, splits)
            key = ub[0] if ub else "(unpinned)"
            units.setdefault(key, []).append(a)
        # groups where >=2 addrs land in the SAME pinned unit
        for u, ua in units.items():
            if u != "(unpinned)" and len(ua) >= 2:
                same_unit.append((name, u, ua))

    print(f"[audit] {len(same_unit)} same-unit duplicate groups "
          f"(>=2 addrs of one name pinned in one unit) -- these SUPPRESS:")
    for name, u, ua in sorted(same_unit, key=lambda x: x[1]):
        print(f"  {u:28s} {len(ua)}x  {name[:70]}")
        for a in ua:
            print(f"      0x%08X" % a)
    print("\n[audit] NOTE: removing these measured +0 this session -> "
          "informational, NOT auto-applied. Pass to a manual hygiene pass.")
    return same_unit


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--gate", metavar="CAND",
                    help="candidate fragment (JSON file or '-' for stdin)")
    ap.add_argument("--audit", action="store_true",
                    help="scan LIVE tsm for same-unit duplicate names (info only)")
    ap.add_argument("--out", default="-",
                    help="write safe {addr:name} subset here ('-' = stdout)")
    ap.add_argument("--merge", action="store_true",
                    help="merge the safe subset INTO --tsm (in place)")
    ap.add_argument("--tsm", default=DEF_TSM)
    ap.add_argument("--splits", default=DEF_SPLITS)
    ap.add_argument("--symbols", default=DEF_SYMBOLS)
    ap.add_argument("--objects", default=DEF_OBJECTS)
    args = ap.parse_args()

    tsm = json.load(open(args.tsm))
    splits = parse_splits(args.splits)

    if args.audit:
        audit(tsm, splits)
        if not args.gate:
            return 0

    if not args.gate:
        ap.error("specify --gate CAND and/or --audit")

    band_x, band_classes, ham_classes = build_band_authority(
        tsm, args.objects, splits)
    exact_canonical = load_exact_canonical()
    cands = load_candidates(args.gate)

    safe, reasons = gate(cands, tsm, splits, band_x, ham_classes, exact_canonical)

    print(f"[gate] candidates in : {len(cands)}", file=sys.stderr)
    print(f"[gate] safe out      : {len(safe)}", file=sys.stderr)
    print(f"[gate] rejected      : {len(cands) - len(safe)}", file=sys.stderr)
    order = ["addr_exists", "non_real_symbol", "name_collision_tsm",
             "batch_dup_nonicf", "icf_folded_twin", "icf_indeterminate",
             "ham_band_phantom"]
    for r in order:
        if reasons.get(r):
            print(f"         {r:22s}: {reasons[r]}", file=sys.stderr)
    for r in reasons:
        if r not in order:
            print(f"         {r:22s}: {reasons[r]}", file=sys.stderr)

    if args.merge:
        added = 0
        for a, n in safe.items():
            if a.lower() not in {k.lower() for k in tsm if k.lower().startswith("0x")}:
                tsm[a] = n
                added += 1
        json.dump(tsm, open(args.tsm, "w"), indent=1)
        print(f"[merge] +{added} names -> {args.tsm}", file=sys.stderr)
    else:
        if args.out == "-":
            json.dump(safe, sys.stdout, indent=1)
            sys.stdout.write("\n")
        else:
            json.dump(safe, open(args.out, "w"), indent=1)
            print(f"[gate] wrote {args.out} ({len(safe)} safe names)",
                  file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
