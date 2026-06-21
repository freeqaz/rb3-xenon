#!/usr/bin/env python3
"""field_offset_gate.py - partial-port POISONED-TAIL static analyzer (B1).

THE PROBLEM (PIPELINE-DESIGN.md S6 / research/04-sourceport-bottleneck.md S2A)
-----------------------------------------------------------------------------
Identity-transfer's transport is solved (RockCentral.cpp +17). The wall is the
SOURCE PORT: a *faithful* whole-TU port can still byte-match 0/64 (wave-16
BandProfile) because of **struct-layout drift amplified by array/embedded
members**. The Wii oracle layout (e.g. ``Stats``) is fatter than retail; an
embedded/array member at offset ``D`` shifts every subsequent field. Any method
that touches a field AT OR ABOVE ``D`` loads the wrong offset -> instant
divergence ("POISONED-TAIL"). Methods that touch only the stable HEAD
(``off < D``) can still byte-match before the layout is fixed.

THE GATE (the highest-leverage B1 build)
----------------------------------------
``field_offset_gate(TU, D)`` statically scans each of the TU's Wii method bodies
in ``../rb3/build/SZBE69_B8/asm`` for any ``this``-relative load/store/lea
(``lwz/lwzx/lfs/lfd/stw/stfs/...`` AND ``addi this+off``) with ``off >= D`` and
tags that method POISONED-TAIL. It emits the **clean pin-set** for a partial port:

    pin-set = real(>44B body)
              AND NOT MISATTRIBUTED   (locator: oracle mapped a wrong VA)
              AND NOT WALL / ::Handle  (locator + name: macro/STL divergence)
              AND NOT POISONED-TAIL    (this gate: touches a tail field >= D)

That set is fed to ``identity_transfer.py --pin-only <list>`` so a partial port
pins only the methods that can byte-match, converting the wave-16 0/64 whole-TU
loss into a small-but-positive head-only win (the RockCentral shape).

``D`` (the divergence point)
----------------------------
``D`` = the FIRST member offset whose retail layout DIVERGES from Wii (default:
the first embedded/array-heavy member). A flat-struct TU with no layout drift =>
``D = infinity`` = nothing is poisoned (the gate excludes nothing on layout
grounds). ``D`` is best-effort to infer (``--infer-d`` heuristic scans the TU's
primary header for the first embedded-class/array member) with a SAFE default
(``D = infinity``, retain everything) and a manual ``--D 0xNNN`` override that is
ALWAYS authoritative when a struct-lever lane has pinned the real divergence.

NOTE ON ``this`` (MWCC PPC ABI): ``this`` arrives in ``r3``; bodies frequently
``mr rN, r3`` it into a callee-save register before the field accesses. We track
``this`` through ``mr`` copies (and the ``addr_of`` ``addi rN, this, K`` form,
which itself counts as a tail-touch when ``K >= D`` because it materializes a
pointer into the divergent tail).

PER-CLASS SCOPING (``--class``): a TU can host several classes (RockCentral.cpp
defines RockCentral + the embedded Stats/PerformanceData accessors). The
divergence is a property of ONE class's layout (e.g. ``Stats`` is ~65B fatter on
Wii). ``D`` is therefore only meaningful for methods of the DIVERGING class:
a ``RockCentral::RockCentral`` ``addi this,0x114`` is a RockCentral field, NOT a
Stats field, and must NOT be poisoned by the Stats ``D``. Pass
``--class Stats,PerformanceData`` to apply ``D`` ONLY to those classes' methods;
methods of every other class are never POISONED-TAIL (only STUB/WALL can drop
them). Omit ``--class`` to apply ``D`` to every method (the flat single-class TU
the base spec describes).

CONSERVATIVE BY DESIGN (it must never drop a proven win)
--------------------------------------------------------
A method the gate cannot affirmatively analyze (no joinable Wii body; size
unknown) is RETAINED, never excluded -- exclusion would risk dropping a win, and
objdiff byte-equality is the final gate anyway (PIPELINE-DESIGN.md S10 gate 8).
The gate only EXCLUDES on a positive proof: a stub (<=44B), a locator
MISATTRIBUTED/WALL class, a ``::Handle`` method, or a proven tail-touch.

USAGE
-----
  tools/field_offset_gate.py --tu RockCentral.cpp                      # report
  tools/field_offset_gate.py --tu BandProfile.cpp --D 0x788            # set D
  tools/field_offset_gate.py --tu Stats.cpp --infer-d                  # best-effort D
  tools/field_offset_gate.py --tu RockCentral.cpp --emit-pin-only s.json
        # write the clean VA pin-set for: identity_transfer --pin-only s.json
  tools/field_offset_gate.py --tu RockCentral.cpp --locator-gate g.json
        # fuse in a locator --emit-gate sidecar (MISATTRIBUTED/WALL skip)

It REUSES locator.py's asm-walk + name primitives; it does not duplicate them.
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

# REUSED primitives (do NOT reimplement): retail size load, oracle->name parse,
# the Wii-asm filename resolution + comment/fn parsing conventions.
from pin_identified import load_sizes  # noqa: E402
from gen_game_target_map import parse_wii_name  # noqa: E402
from locator import (  # noqa: E402
    DEF_WII_ASM, DEF_ORACLE, DEF_SYMBOLS, tu_base, _norm_demangled,
)

DEF_SIZE_GATE = 44          # <=44B body is honesty-disqualified (ICF stub fold)
INF = float("inf")

# --- asm matchers ----------------------------------------------------------
# A this-relative MEMORY ACCESS: load OR store, integer OR float, of the form
#   <mnem> rX, OFF(rBASE)
# We deliberately enumerate the PPC forms the Wii build emits for field access.
# (lXzx/stXx indexed forms have no literal offset operand -> handled separately.)
MEM_RE = re.compile(
    r"^(lbz|lhz|lha|lwz|lwa|ld|lfs|lfd|"          # loads
    r"stb|sth|stw|std|stfs|stfd)"                  # stores
    r"\s+\w+,\s*-?(?:0x)?([0-9A-Fa-f]+)\(r(\d+)\)")
# addi rDst, rSrc, OFF  -- materializes &(this->member); a tail member's address
# (e.g. ``addi r3, r3, 0x48`` = return embedded sub-object) is itself a tail-touch.
ADDI_RE = re.compile(r"^addi\s+r(\d+),\s*r(\d+),\s*-?(?:0x)?([0-9A-Fa-f]+)\b")
# mr rDst, rSrc -- register copy; propagates this-ness.
MR_RE = re.compile(r"^mr\s+r(\d+),\s*r(\d+)\b")
# a subroutine call clobbers all volatile GPRs (r3-r12 are caller-saved on the
# PPC EABI). After a call, r3 holds a RETURN value, not `this` -- so we must drop
# r3..r12 from the tracked set, else a post-call ``lwz off(r3)`` would be a FALSE
# tail-touch that could wrongly exclude a proven win (we err toward RETAIN).
CALL_RE = re.compile(r"^(bl|bla|bctrl|bctrll|blrl)\b")
_VOLATILE_GPRS = set(range(3, 13))
# any instruction that DEFINES a GPR we'd otherwise trust as this (a fresh value
# overwrites this-ness). We only need the destination register of common defining
# forms; anything we don't recognize that writes rN is handled by INSN_DEF_RE.
INSN_DEF_RE = re.compile(
    r"^(?:li|lis|addi|addis|add|sub|subf|or|ori|and|andi|xor|mr|"
    r"lbz|lhz|lha|lwz|lwa|ld|lfs|lfd|slwi|srwi|rlwinm|mulli|neg|"
    r"lwzx|lhzx|lbzx|cntlzw|extsb|extsh|mfspr|mflr|mfctr|cmpw)"
    r"[a-z.]*\s+r(\d+)\b")

# ::Handle methods are the BEGIN_HANDLERS macro WALL (axis C) -- always deferred.
HANDLE_RE = re.compile(r"(?:^|::)~?Handle\b|@Handle@|Handle__")


# ===========================================================================
# Wii body loader (keyed for a robust oracle join)
# ===========================================================================
def _find_tu_asm(wii_asm_dir, tu):
    """Locate ``<stem>.s`` recursively under the Wii asm tree (the layout has
    per-TU files in subdirs, e.g. band3/net_band/RockCentral.s)."""
    stem = os.path.splitext(tu)[0]
    if not os.path.isdir(wii_asm_dir):
        return None
    for root, _d, files in os.walk(wii_asm_dir):
        if (stem + ".s") in files:
            return os.path.join(root, stem + ".s")
    return None


_CMT_RE = re.compile(r"^#\s+(.*\S)\s*$")
_FN_RE = re.compile(r"^\.fn\s")
_ENDFN_RE = re.compile(r"^\.endfn")
# INSN_RE (asm line w/ VA prefix) is fpm's; we re-derive the ops the same way.
_INSN_RE = re.compile(
    r"^/\*\s*[0-9A-Fa-f]{8}\s+[0-9A-Fa-f]{8}\s+[0-9A-Fa-f ]+\*/\t(.*)$")


def load_wii_method_bodies(wii_asm_dir, tu):
    """Parse the Wii TU asm into per-method bodies, keyed BOTH ways for a robust
    oracle join:
      * by (class, method, arity)  -- via parse_wii_name on the # comment;
      * by normalized-demangled string (exact, disambiguates overloads).
    Returns (by_key, by_dem); each body = {dem, ops:[str]}. Empty dicts if the
    TU asm is absent (the gate then degrades to "retain everything analysable",
    i.e. no POISON exclusions -- conservative)."""
    path = _find_tu_asm(wii_asm_dir, tu)
    by_key, by_dem = {}, {}
    if path is None:
        return by_key, by_dem, None
    pending, cur = None, None
    for line in open(path, errors="replace"):
        cm = _CMT_RE.match(line)
        if cm and not line.startswith("# .text") and not line.startswith("# 0x"):
            pending = cm.group(1)
            continue
        if _FN_RE.match(line):
            cur = {"dem": pending, "ops": []}
            pending = None
            continue
        if _ENDFN_RE.match(line):
            if cur and cur.get("dem"):
                p = parse_wii_name(cur["dem"])
                if p:
                    # first writer wins on the (class,method,arity) key so an
                    # overload collision keeps a deterministic body; the exact
                    # by_dem key still disambiguates when the oracle name is exact.
                    by_key.setdefault((p[0], p[1], p[2]), cur)
                by_dem.setdefault(_norm_demangled(cur["dem"]), cur)
            cur = None
            continue
        if cur is None:
            continue
        im = _INSN_RE.match(line)
        if im:
            cur["ops"].append(im.group(1).rstrip())
    return by_key, by_dem, path


def lookup_body(by_key, by_dem, wii_name):
    """Join an oracle ``wii_name`` to a parsed Wii body. Exact normalized-demangled
    match first (overload-safe), then the (class,method,arity) key."""
    b = by_dem.get(_norm_demangled(wii_name))
    if b is not None:
        return b
    p = parse_wii_name(wii_name)
    if p:
        return by_key.get((p[0], p[1], p[2]))
    return None


# ===========================================================================
# The core static check: does a method touch a this-relative field >= D?
# ===========================================================================
def tail_touch(ops, D):
    """Scan a method body's ops for any this-relative field access with
    offset >= D. ``this`` arrives in r3 and is tracked through ``mr`` copies; an
    instruction that redefines a tracked register clears its this-ness. Returns
    (touched: bool, evidence: [(op, off)]) -- evidence lists the offending
    accesses (capped) for the report. ``D == inf`` => never touched."""
    if D == INF:
        return False, []
    this_regs = {3}             # MWCC PPC: this in r3 on entry
    evidence = []
    for op in ops:
        # --- a call clobbers volatile GPRs r3..r12 (this no longer in r3) -----
        if CALL_RE.match(op):
            this_regs -= _VOLATILE_GPRS
            continue
        # --- propagate this-ness through mr copies (handle BEFORE def-clear) --
        mm = MR_RE.match(op)
        if mm:
            dst, src = int(mm.group(1)), int(mm.group(2))
            if src in this_regs:
                this_regs.add(dst)
            else:
                this_regs.discard(dst)   # mr from a non-this reg overwrites
            continue
        # --- this-relative MEMORY access -------------------------------------
        me = MEM_RE.match(op)
        if me:
            off = int(me.group(2), 16)
            base = int(me.group(3))
            if base in this_regs and off >= D:
                evidence.append((op, off))
            # a load INTO a tracked reg destroys its this-ness (handled by the
            # generic def-clear below); fallthrough.
        # --- addi rDst, this, K  (materialize &this->member) -----------------
        ae = ADDI_RE.match(op)
        if ae:
            dst, src, k = int(ae.group(1)), int(ae.group(2)), int(ae.group(3), 16)
            if src in this_regs:
                if k >= D:
                    evidence.append((op, k))
                # addi this+0 keeps this-ness; addi this+K (K>0) yields an
                # interior pointer, NOT this -- drop dst from this_regs (it now
                # points mid-object). If dst==src and k==0, no change.
                if k == 0:
                    this_regs.add(dst)
                else:
                    this_regs.discard(dst)
            else:
                this_regs.discard(dst)
            continue
        # --- generic register redefinition clears this-ness ------------------
        de = INSN_DEF_RE.match(op)
        if de:
            dst = int(de.group(1))
            # the load forms already matched above did not `continue`; clear here
            this_regs.discard(dst)
    return (len(evidence) > 0), evidence


# ===========================================================================
# Best-effort D inference from the TU's primary header (clearly heuristic)
# ===========================================================================
# A header member line annotated `// 0xHEX` (the struct_db convention) whose type
# is an embedded class/array (NOT a pointer, NOT a primitive) is the first
# layout-amplifying member. We return its offset as a CANDIDATE D.
_MEMBER_RE = re.compile(
    r"^\s*([A-Za-z_][\w:<>,\s\*&]*?)\s+(\w+)\s*(\[[^\]]*\])?\s*;.*//\s*0x([0-9A-Fa-f]+)")
_PRIMITIVE = {
    "char", "short", "int", "long", "bool", "float", "double", "unsigned",
    "signed", "void", "u8", "u16", "u32", "s8", "s16", "s32", "uchar", "ushort",
    "uint", "ulong", "byte", "word", "dword", "Symbol", "String",  # Symbol/String are 4-8B flat
}


def infer_d_from_header(tu):
    """Best-effort: scan ``src/.../<stem>.h`` for the first embedded-class/array
    member annotated with a ``// 0xHEX`` offset. Returns (D, member, header_path)
    or (None, None, header_path-or-None). HEURISTIC -- a real divergence point
    must be confirmed by a struct-lever lane; prefer ``--D``."""
    stem = os.path.splitext(tu)[0]
    header = None
    src_root = os.path.join(ROOT, "src")
    if os.path.isdir(src_root):
        for root, _d, files in os.walk(src_root):
            if (stem + ".h") in files:
                header = os.path.join(root, stem + ".h")
                break
    if header is None:
        return None, None, None
    for line in open(header, errors="replace"):
        m = _MEMBER_RE.match(line)
        if not m:
            continue
        typ, name, arr, offhex = m.group(1).strip(), m.group(2), m.group(3), m.group(4)
        is_ptr = "*" in typ or "&" in typ
        base_type = typ.replace("const", "").strip().split()[-1] if typ else ""
        is_primitive = base_type in _PRIMITIVE
        if is_ptr:
            continue
        # an array of anything, OR an embedded non-primitive class, amplifies drift
        if arr or not is_primitive:
            return int(offhex, 16), f"{typ} {name}{arr or ''}", header
    return None, None, header


# ===========================================================================
# Locator-sidecar fusion (MISATTRIBUTED / WALL skip)
# ===========================================================================
def load_locator_skip(path):
    """Read a locator --emit-gate sidecar ({VA:{class,confidence}}); return a
    dict int(VA)->class for MISATTRIBUTED/WALL/UNPLACEABLE skip decisions. None
    if absent/unreadable (the gate then relies on size + name + tail only)."""
    if not path:
        return None
    try:
        sc = json.load(open(path))
    except (OSError, ValueError):
        return None
    out = {}
    for va_str, info in sc.items():
        try:
            out[int(va_str, 16)] = info.get("class")
        except ValueError:
            continue
    return out


# ===========================================================================
# Main gate
# ===========================================================================
SKIP_LOCATOR_CLASSES = {"MISATTRIBUTED", "WALL", "UNPLACEABLE"}


def run(args):
    tu = args.tu if args.tu.endswith((".cpp", ".c", ".cc")) else args.tu + ".cpp"

    # --- resolve D -----------------------------------------------------------
    d_source = "default(inf)"
    if args.D is not None:
        D = int(args.D, 16) if isinstance(args.D, str) else int(args.D)
        d_source = f"--D 0x{D:X}"
    elif args.infer_d:
        cand, member, hdr = infer_d_from_header(tu)
        if cand is not None:
            D = cand
            d_source = f"inferred 0x{D:X} (first embedded/array member '{member}' in {os.path.relpath(hdr, ROOT)})"
        else:
            D = INF
            d_source = ("inferred=NONE (flat struct / no // 0xHEX header found) "
                        "-> D=inf, nothing poisoned")
    else:
        D = INF

    # --- inputs --------------------------------------------------------------
    oracle = json.load(open(args.oracle))
    recs = [e for e in oracle if tu_base(e.get("bindiff_src") or "") == tu]
    if not recs:
        print(f"ERROR: no oracle records for {tu}", file=sys.stderr)
        return 2
    sizes = load_sizes(args.symbols)              # int VA -> retail body size
    by_key, by_dem, wii_path = load_wii_method_bodies(args.wii_asm, tu)
    loc_skip = load_locator_skip(args.locator_gate)
    diverging_classes = {c.strip() for c in (args.diverging_class or "").split(",")
                         if c.strip()}

    if wii_path is None:
        print(f"[field_offset_gate] WARN: Wii asm for {tu} not found under "
              f"{args.wii_asm}; POISONED-TAIL detection DISABLED (no body to "
              f"scan) -- the gate degrades to size+name+locator only "
              f"(conservative: retains tail-touchers).", file=sys.stderr)

    pin_set = []          # the clean partial-port pin-set
    excluded = []         # (va, name, reason, detail)
    for e in sorted(recs, key=lambda x: int(x["rb3_addr"], 16)):
        va = int(e["rb3_addr"], 16)
        wii_name = e.get("wii_name", "") or ""
        sz = sizes.get(va, e.get("size", 0) or 0)
        rec = {"va": f"0x{va:08X}", "name": wii_name, "size": sz}

        # 1. honesty floor: stub (<=44B) cannot carry a win (ICF stub-fold).
        if sz <= args.size_gate:
            excluded.append((va, wii_name, "STUB",
                             f"{sz}B <= {args.size_gate}B (ICF stub-fold, honesty-DQ)"))
            continue
        # 2. ::Handle macro WALL (axis C) -- always defer (lowest-sim divergence).
        if HANDLE_RE.search(wii_name):
            excluded.append((va, wii_name, "WALL/Handle",
                             "BEGIN_HANDLERS macro divergence (axis C)"))
            continue
        # 3. locator MISATTRIBUTED / WALL / UNPLACEABLE skip (re-tasked as SKIP
        #    list per PIPELINE-DESIGN.md S4 -- we do NOT gate IN on CONFIRMED).
        if loc_skip is not None:
            cls = loc_skip.get(va)
            if cls in SKIP_LOCATOR_CLASSES:
                excluded.append((va, wii_name, f"LOCATOR:{cls}",
                                 "locator sidecar demoted this VA"))
                continue
        # 4. POISONED-TAIL: this-relative field access at/above the divergence D.
        #    D is a property of the DIVERGING class's layout; when --class is set
        #    we apply it ONLY to methods of those classes (a RockCentral method's
        #    this-offset is a RockCentral field, NOT the Stats tail -- see the
        #    PER-CLASS SCOPING note). With no --class set, D applies to all.
        m_class = None
        p = parse_wii_name(wii_name)
        if p:
            m_class = p[0]
        apply_d = D
        if diverging_classes and m_class not in diverging_classes:
            apply_d = INF          # not a diverging-class method -> never poison
        body = lookup_body(by_key, by_dem, wii_name)
        if body is not None:
            touched, evid = tail_touch(body["ops"], apply_d)
            if touched:
                detail = "; ".join(f"{op} (off=0x{off:X})" for op, off in evid[:3])
                if len(evid) > 3:
                    detail += f" (+{len(evid)-3} more)"
                excluded.append((va, wii_name, "POISONED-TAIL",
                                 f"touches field >= 0x{int(apply_d):X}: {detail}"))
                continue
            rec["wii_body"] = True
            rec["tail_scanned"] = True
        else:
            # no joinable Wii body -> cannot prove tail-touch -> RETAIN (safe).
            rec["wii_body"] = False
            rec["tail_scanned"] = False
        pin_set.append(rec)

    # --- report --------------------------------------------------------------
    _print_report(tu, D, d_source, recs, pin_set, excluded, wii_path,
                  by_key, by_dem, loc_skip, diverging_classes)

    # --- emit the --pin-only list for identity_transfer ----------------------
    if args.emit_pin_only:
        vas = [r["va"] for r in pin_set]
        json.dump(vas, open(args.emit_pin_only, "w"), indent=1)
        print(f"\n[field_offset_gate] wrote pin-only list {args.emit_pin_only} "
              f"({len(vas)} VAs) -> identity_transfer.py --pin-only "
              f"{args.emit_pin_only}", file=sys.stderr)
    if args.out:
        json.dump({
            "tu": tu,
            "D": (None if D == INF else f"0x{int(D):X}"),
            "d_source": d_source,
            "diverging_classes": sorted(diverging_classes),
            "pin_set": pin_set,
            "excluded": [{"va": f"0x{va:08X}", "name": n, "reason": r,
                          "detail": d} for va, n, r, d in excluded],
        }, open(args.out, "w"), indent=1)
        print(f"[field_offset_gate] wrote {args.out}", file=sys.stderr)
    return 0


def _print_report(tu, D, d_source, recs, pin_set, excluded, wii_path,
                  by_key, by_dem, loc_skip, diverging_classes):
    from collections import Counter
    reasons = Counter(r for _va, _n, r, _d in excluded)
    print("=" * 74)
    print(f"field_offset_gate  TU={tu}")
    print("=" * 74)
    print(f"D (divergence point)       : {d_source}")
    print(f"D applies to class(es)     : "
          f"{', '.join(sorted(diverging_classes)) if diverging_classes else 'ALL (no --class scope)'}")
    print(f"oracle methods             : {len(recs)}")
    print(f"Wii asm                    : "
          f"{os.path.relpath(wii_path, ROOT) if wii_path else 'NOT FOUND (poison detection off)'}")
    print(f"locator sidecar            : "
          f"{'loaded (' + str(len(loc_skip)) + ' VAs)' if loc_skip is not None else 'none'}")
    print("-" * 74)
    print(f"CLEAN PIN-SET (port these) : {len(pin_set)}")
    scanned = sum(1 for r in pin_set if r.get("tail_scanned"))
    print(f"  of which tail-scanned    : {scanned} "
          f"(head-only, proven below D)")
    print(f"  retained un-analysable   : {len(pin_set) - scanned} "
          f"(no joinable Wii body -> retained, objdiff is final gate)")
    print(f"EXCLUDED                   : {len(excluded)}")
    for reason, n in reasons.most_common():
        print(f"    {reason:<16} : {n}")
    print()
    print("PIN-SET (clean -- feed to identity_transfer --pin-only):")
    for r in sorted(pin_set, key=lambda x: int(x["va"], 16)):
        tag = "scan" if r.get("tail_scanned") else "n/a "
        print(f"  {r['va']}  {r['size']:5d}B  [{tag}]  {r['name'][:62]}")
    print()
    print("EXCLUDED (with reason):")
    for va, n, reason, detail in sorted(excluded):
        print(f"  0x{va:08X}  {reason:<14}  {n[:48]}")
        print(f"             -> {detail[:96]}")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tu", required=True,
                    help="target TU basename, e.g. RockCentral.cpp")
    ap.add_argument("--D", default=None,
                    help="divergence-point offset (hex, e.g. 0x788). The first "
                         "member whose retail layout != Wii. Omit => D=inf "
                         "(nothing poisoned) unless --infer-d.")
    ap.add_argument("--infer-d", action="store_true",
                    help="best-effort: infer D from the first embedded/array "
                         "member in the TU's header (HEURISTIC; prefer --D).")
    ap.add_argument("--class", dest="diverging_class", default=None,
                    help="comma list of the DIVERGING class(es) whose layout D "
                         "describes (e.g. Stats,PerformanceData). D is applied "
                         "ONLY to those classes' methods; other classes are "
                         "never POISONED-TAIL. Omit => apply D to every method.")
    ap.add_argument("--oracle", default=DEF_ORACLE)
    ap.add_argument("--symbols", default=DEF_SYMBOLS)
    ap.add_argument("--wii-asm", default=DEF_WII_ASM)
    ap.add_argument("--size-gate", type=int, default=DEF_SIZE_GATE,
                    help="bodies <= this many bytes are stub-folds (honesty-DQ)")
    ap.add_argument("--locator-gate", default=None,
                    help="locator --emit-gate sidecar to fuse (MISATTRIB/WALL skip)")
    ap.add_argument("--emit-pin-only", default=None,
                    help="write the clean pin-set as a JSON VA list for "
                         "identity_transfer.py --pin-only")
    ap.add_argument("--out", default=None, help="write the full gate result JSON")
    args = ap.parse_args()
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
