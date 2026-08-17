#!/usr/bin/env python3
"""CW-2: function COMDAT reader that does NOT drop EH-bearing functions.

THE BUG IT FIXES
----------------
`icf_fold_evidence.function_bodies()` accepts a code section only when it holds
EXACTLY ONE defining function symbol, AT OFFSET 0:

    defs = [s for s in by_sec[si] if s["value"] == 0 and storage in (2,3) and type == 0x20]
    if len(defs) != 1: continue

Both halves of that gate misfire on every EH-bearing function. MSVC X360 lays
such a COMDAT out as

    +0    8-byte EH prefix (unnamed; memory project_eh_prefix_extent_2026-08-02)
    +8    the function entry            <-- value != 0, so the offset-0 gate fails
    +N    __unwind$NNNNN                <-- ALSO storage 2 / type 0x20,
                                            so len(defs) == 2 and the section is
                                            dropped entirely

Measured on this tree: 31,075 sections with 2 defs and 8,849 with 3+ were being
discarded, and 185 of the 404 callees that lane CW-2's first cross-binary pass
reported as "not compiled by us" are in fact compiled inside them. Lane CV-4's
class-(c) fold-provability test reads the same index, so it shared the blind
spot.

THE FIX
-------
Slice each code section by CONSECUTIVE defining-symbol values: symbol i owns
[value_i, value_{i+1}), the last owns [value_i, end). Relocations are rebased
into the owning slice and anything in the unnamed prefix is dropped.

⛔⛔ THE EH_PREFIX_SUFFIX_ARTIFACT -- fixed 2026-08-16, lane STLPORT-1
---------------------------------------------------------------------
"anything in the unnamed prefix is dropped" was true of the LEADING prefix only.
EVERY EH-bearing region carries that same 8-byte prefix -- the entry AND each
funclet -- so a section laid out

    +0    8-byte EH prefix          <- excluded: the slice starts at value==8
    +8    the function body, 96 B
    +104  8-byte EH prefix          <- BILLED TO THE FUNCTION ABOVE
    +112  __catch$NNNNN

yielded a 104-byte body for a function that is 96 bytes.  dtk's split objs carve
the target side by .pdata extents and carry no prefixes, so a retail-vs-ours size
test read a UNIFORM +8 on every EH-bearing function with a successor.

Measured on this tree: 288 symbols comparable to retail carry the signature and
260 read a false +8; the fix moves 259 of them to delta 0 and leaves all 22,563
control symbols bit-identical.

⛔ ...AND THE FIX WAS GATED ON THE WRONG FACT -- corrected 2026-08-17, lane
EHFIX-BODIES
--------------------------------------------------------------------------
The 2026-08-16 fix asked "is the SUCCESSOR named `__unwind$`/`__catch$`/...?".
That is a proxy for the question that matters ("is there an 8-byte EH prefix at
`end - 8`?") and it is not equivalent: an interior EH prefix can precede an
ORDINARY function packed into the same `.text`, and then the successor carries
no funclet name and the trim never fires.  Measured on build 45410914: 3 of the
6,395 interior prefixes are of that shape, so the name predicate leaves exactly
3 slices 8 bytes long.

The fix is MARKER-FIRST, NAME-NEVER:

  1. PRIMARY -- the `$EH*` boundary symbol `scripts/obj_eh_boundary_patcher.py`
     injects at every interior prefix (class 3, type 0; stamp
     `eh_boundary_patched.stamp`).  A fact the object STATES, applied as a
     bounded `end - 8` trim rather than merged into the boundary set, so a
     marker in the wrong place can only ever cost 8 bytes -- never truncate.
  2. FALLBACK, for an object the patcher never touched (dtk target splits, a
     pre-patcher build) -- the prefix's own signature: eight zero bytes whose
     two words relocate to `__CxxFrameHandler` and `__ehfuncinfo$...`.  Those
     are data pointers and cannot be instructions, so it cannot fire on a real
     body.
  3. The successor's NAME is not consulted at all.

Same semantics as `tools/ident_body_channel.py` and
`scripts/unicorn_runner/extractor.py`, which already landed it.

★ WHAT IT COST, and why this docstring is long: the artifact was read as a SOURCE
defect and written into the record as one.  ALIAS-2 (docs/decomp/
ALIAS_UNPROVEN_REMAINDER_ADJUDICATED_2026-08-16.md) reported "our STLport emits
__uninitialized_copy / _M_allocate_and_copy uniformly +8 B vs retail across ~95
same-name pairs -- ONE shared source bug".  The ~95 is real and reproduces to the
row; the CAUSE was this function.  objdiff on the exact symbol it named reports
target 96 / base 96, 24/24 instructions equal.  A whole lane was commissioned to
fix a bug that does not exist.

⚠ Note the direction of the failure: `Sides.l3_exact` returns "NO -- size %d vs
%d" the moment lengths differ, so the artifact made retail-byte fold proof
IMPOSSIBLE for this entire population, while the within-build test (`l4_ourside`,
our(S) vs our(F)) was immune because the artifact cancels on both sides.  A
one-sided instrument error is invisible to the two-sided control.

Validation, stated so it can fail: retail's .pdata entry VA is the function
ENTRY (not the prefix), so recovered bodies must show the same length-delta-0
dominance against retail .pdata as the already-working offset-0 population. If
the layout story is wrong, deltas scatter.

CROSS-REFERENCE, not a duplicate (lane WS-4, 2026-08-06)
--------------------------------------------------------
decomp-synth ships `tools/revcomp/probes/coff_x360.py`, another X360 PPC COFF
reader (machine 0x01F2, which llvm-nm and objdump both reject). It is NOT a
duplicate of this file and neither should be collapsed into the other:

  * this file slices a section by CONSECUTIVE DEFINING-SYMBOL VALUES because
    MSVC lays an EH-bearing COMDAT out as [+0 8-byte EH prefix][+8 entry]
    [+N __unwind$NNNNN] -- several defining symbols per section, entry not at
    offset 0. That is the grain of OUR objs and of the dtk split objs.
  * `coff_x360.py` reads the whole-section-per-function grain and is paired with
    `pe_va_read.py`, which reads the RETAIL PE IMAGE by virtual address. Its job
    is the cross-BINARY comparison, not the cross-OBJ one.
  * decomp-synth's `tools/il_witness/coff_ppc.py` is a THIRD reader and assumes a
    `/Gy` COMDAT-per-function grain that rb3-xenon's objects do not have. Do not
    reach for it here.

The consumer that matters is `tools/xbin_adjudicate.py`, whose docstring carries
the full instrument comparison and the CV-4 class-(b) residual chain.
"""
import collections, os, sys
from pathlib import Path

import pathlib
ROOT = os.environ.get("CW2_ROOT", str(pathlib.Path(__file__).resolve().parent.parent))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from icf_fold_evidence import parse_coff, IMAGE_SCN_CNT_CODE   # noqa: E402


def is_aux_code_symbol(name):
    """EH/unwind data emitted as type-0x20 symbols. Real code, but never a
    C++ callee, so it delimits slices without becoming a comparison target."""
    return name.startswith("__unwind$") or name.startswith("__ehhandler$")


# Every EH-bearing region MSVC X360 emits -- the function entry itself and each
# funclet -- is preceded by the same unnamed 8-byte prefix.  The LEADING one is
# already excluded because the slice starts at the symbol's `value` (== 8).  An
# INTERIOR one is not: it sits inside [value_i, value_{i+1}) and so was being
# billed to the PRECEDING function.  See EH_PREFIX_SUFFIX_ARTIFACT above.
EH_PREFIX_BYTES = 8
#: Boundary marker the build injects at every interior prefix (class 3, type 0),
#: so a `type == 0x20` filter discards exactly the symbol that states the answer.
EH_MARKER_PREFIX = "$EH"
EH_PERSONALITY = "__CxxFrameHandler"
EH_FUNCINFO_PREFIX = "__ehfuncinfo$"
#: A type-18 PAIR record's "VirtualAddress" is a DISPLACEMENT, not an address, so
#: it must never claim an offset in a reloc-by-offset map.
IMAGE_REL_PPC_PAIR = 0x12


def eh_boundaries(section_symbols):
    """Offsets of the `$EH*` EH-prefix markers among one section's symbols.

    Keyed on the `$EH` name AND class 3 / type 0, not on storage class alone:
    695,516 class-6 `$M#####` debug labels sit INSIDE bodies in this build, and
    the class-2/3 rule that happens to be equivalent here truncates a target
    function on dc3 (lane EHFIX-SYNTH).
    """
    return {s["value"] for s in section_symbols
            if s["storage"] == 3 and s["type"] == 0
            and s["name"].startswith(EH_MARKER_PREFIX)}


def eh_prefix_end(end, v, marks, raw, rel):
    """`end` with the SUCCESSOR's 8-byte EH prefix handed back, or `end` itself.

    Marker first, then the prefix's own byte+relocation signature; the
    successor's NAME is never consulted -- see the corrected docstring above for
    why a name predicate misses 3 of this build's 6,395 interior prefixes.
    Bounded to exactly 8 bytes and never empties a slice, so a marker in the
    wrong place costs 8 bytes and cannot truncate a body.
    """
    lo = end - EH_PREFIX_BYTES
    if lo <= v:
        return end
    if lo in marks:
        return lo
    if end < len(raw) and raw[lo:end] == b"\0" * EH_PREFIX_BYTES \
            and rel.get(lo) == EH_PERSONALITY \
            and str(rel.get(lo + 4, "")).startswith(EH_FUNCINFO_PREFIX):
        return lo
    return end


def function_bodies_ext(path, stats=None):
    """Yield (name, body_bytes, relocs, entry_off) for each function slice."""
    data = Path(path).read_bytes()
    sections, symbols = parse_coff(data)
    if not sections:
        return
    idx_name = {}
    by_sec = collections.defaultdict(list)
    for s in symbols:
        if s is None:
            continue
        idx_name[s["idx"]] = s["name"]
        if s["section"] > 0:
            by_sec[s["section"] - 1].append(s)
    for si, sec in enumerate(sections):
        if not (sec["chars"] & IMAGE_SCN_CNT_CODE):
            continue
        defs = [s for s in by_sec.get(si, [])
                if s["name"] != sec["name"] and s["storage"] in (2, 3)
                and s["type"] == 0x20]
        if not defs:
            continue
        raw = sec["raw"]
        # de-duplicate identical offsets (aliases at the same address), keeping
        # a stable order, then slice by consecutive values.
        pts = sorted({(s["value"], s["name"]) for s in defs})
        bounds = sorted({v for v, _n in pts} | {len(raw)})
        nxt = {v: bounds[i + 1] for i, v in enumerate(bounds[:-1])}
        marks = eh_boundaries(by_sec.get(si, []))
        rel = {}
        for (o, i, t) in sec["relocs"]:
            if t != IMAGE_REL_PPC_PAIR:
                rel.setdefault(o, idx_name.get(i, "?"))
        for v, name in pts:
            if v % 4 or v >= len(raw):
                if stats is not None:
                    stats["skip_bad_off"] += 1
                continue
            end = nxt.get(v, len(raw))
            # Hand the successor's 8-byte EH prefix back to the successor.
            trimmed = eh_prefix_end(end, v, marks, raw, rel)
            if trimmed != end:
                end = trimmed
                if stats is not None:
                    stats["eh_prefix_suffix_stripped"] += 1
            if stats is not None:
                stats["entry_off_%s" % (v if v <= 8 else "gt8")] += 1
                stats["slices"] += 1
            if is_aux_code_symbol(name):
                if stats is not None:
                    stats["aux_skipped"] += 1
                continue
            rl = [(o - v, idx_name.get(i, "?"), t)
                  for (o, i, t) in sec["relocs"] if v <= o < end]
            yield name, raw[v:end], rl, v
