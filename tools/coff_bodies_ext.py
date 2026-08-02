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

Validation, stated so it can fail: retail's .pdata entry VA is the function
ENTRY (not the prefix), so recovered bodies must show the same length-delta-0
dominance against retail .pdata as the already-working offset-0 population. If
the layout story is wrong, deltas scatter.
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
        for v, name in pts:
            if v % 4 or v >= len(raw):
                if stats is not None:
                    stats["skip_bad_off"] += 1
                continue
            end = nxt.get(v, len(raw))
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
