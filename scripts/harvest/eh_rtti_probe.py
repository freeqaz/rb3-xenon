#!/usr/bin/env python3
"""Decode the retail MSVC exception-handling chain and measure whether it can
act as an *identification* discriminator.

Lane M, 2026-07-26.  **Verdict: it cannot.  This tool exists to record that
measurement so nobody rebuilds the channel.**  See
``docs/plans/identification-discriminators-2026-07-26.md``.

Background
----------
The identification stack (``homing_scan.py``) homes one of our compiled
functions on a retail VA by reloc-masked byte identity.  When several retail
VAs are byte-identical it emits ``cls=MULTI`` and refuses.  Lane K measured two
would-be discriminators for that residue and killed both:

* ``.pdata`` prolog shape -- varies in 18 of 33,714 hit sets (0.053 %).
* every probe computed from the function's *own bytes* -- dead by construction,
  because a hit set is built out of functions whose masked bytes are equal.

Lane K's closing note proposed one channel that escapes that argument: 29 % of
the residue is exception-flagged, so the word before the entry point reaches
MSVC ``_s_FuncInfo`` and from there a try-block map, handler arrays and finally
**RTTI type descriptors whose name strings are referenced content, not a
property of the function's own bytes**.  That is the channel this tool decodes.

The chain
---------
1. ``.pdata`` ``RUNTIME_FUNCTION`` (8 bytes, big-endian payload):
   ``BeginAddress``; bitfield with PrologLen bits 0-7, FuncLen bits 8-29,
   ThirtyTwoBit bit 30, **ExceptionFlag bit 31**.  ``BeginAddress`` is already a
   full VA on this image, not an RVA.
2. For an exception-flagged function the two big-endian words at
   ``BeginAddress-8`` are ``{handler, handlerData}``.
3. ``handlerData`` -> ``_s_FuncInfo``: ``magic`` (0x199305xx), ``maxState``,
   ``pUnwindMap``, ``nTryBlocks``, ``pTryBlockMap``, ``nIPMapEntries``,
   ``pIPToStateMap``, ``pESTypeList``, ``EHFlags``.
4. ``_s_TryBlockMapEntry`` (20 bytes): ``tryLow, tryHigh, catchHigh, nCatches,
   pHandlerArray``.
5. ``_s_HandlerType`` (16 bytes): ``adjectives, pType, dispCatchObj,
   addressOfHandler``.
6. ``pType`` -> ``TypeDescriptor``: ``pVFTable, spare, name[]`` -- the name is an
   inline NUL-terminated mangled string such as ``.?AVFoo@@``.

What it measures
----------------
``--census``   decode every exception-flagged function; report how far the chain
               gets and what content it yields.
``--variation`` the decisive test, mirroring ``pdata_shape_probe.py``: given a
               ``homing_scan`` results file, ask whether the EH signature
               *varies within a byte-identical hit set*.  A discriminator that
               is constant across the candidates cannot separate them.
``--dump VA``  fully worked example for eyeballing correctness.

Usage
-----
    python3 scripts/harvest/eh_rtti_probe.py --census
    python3 scripts/harvest/eh_rtti_probe.py --variation --results merged.json
    python3 scripts/harvest/eh_rtti_probe.py --dump 0x8227f1a0
"""

from __future__ import annotations

import argparse
import json
import os
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from funclet_cascade_rank import PE, parse_pdata  # noqa: E402  (shared PE reader)

FUNCINFO_MAGIC_HI = 0x199305  # magic is 0x199305xx; the low byte is a version
TRYBLOCK_SZ = 20
HANDLER_SZ = 16
TYPEDESC_NAME_OFF = 8  # {pVFTable, spare, name[]}


def default_exe(root: Path) -> Path:
    return root / "orig" / "45410914" / "band.exe"


# --------------------------------------------------------------------- decode
def cstr(pe: PE, va: int, maxlen: int = 256) -> str | None:
    o = pe.off(va)
    if o is None:
        return None
    e = pe.d.find(b"\0", o, o + maxlen)
    if e < 0:
        return None
    try:
        return pe.d[o:e].decode("latin1")
    except Exception:
        return None


def decode_eh(pe: PE, va: int) -> dict:
    """Walk the whole chain for one function entry point.

    Always returns a dict with a ``status`` field so callers can count the
    failure modes instead of silently dropping them.
    """
    out: dict = {
        "va": va,
        "handler": None,
        "handler_data": None,
        "magic": None,
        "max_state": None,
        "n_try": 0,
        "eh_flags": None,
        "unwind_actions": [],
        "catches": [],       # list of dicts: adjectives, p_type, type_name, handler
        "type_names": set(),
        "status": "ok",
    }
    handler = pe.u32(va - 8)
    hdata = pe.u32(va - 4)
    out["handler"], out["handler_data"] = handler, hdata
    if not hdata:
        out["status"] = "no-handler-data"
        return out
    magic = pe.u32(hdata)
    out["magic"] = magic
    if magic is None or (magic >> 8) != FUNCINFO_MAGIC_HI:
        out["status"] = "bad-magic"
        return out

    max_state = pe.u32(hdata + 4)
    if max_state is not None and max_state >= (1 << 31):
        max_state -= 1 << 32
    p_unwind = pe.u32(hdata + 8)
    n_try = pe.u32(hdata + 12) or 0
    p_try = pe.u32(hdata + 16)
    out["max_state"] = max_state
    out["n_try"] = n_try
    out["eh_flags"] = pe.u32(hdata + 32)

    if p_unwind and max_state and max_state > 0:
        for i in range(min(max_state, 4096)):
            act = pe.u32(p_unwind + i * 8 + 4)
            if act:
                out["unwind_actions"].append(act)

    if p_try and n_try:
        for i in range(min(n_try, 1024)):
            e = p_try + i * TRYBLOCK_SZ
            n_catch = pe.u32(e + 12) or 0
            p_ha = pe.u32(e + 16)
            if not p_ha:
                continue
            for j in range(min(n_catch, 256)):
                h = p_ha + j * HANDLER_SZ
                adj = pe.u32(h)
                p_type = pe.u32(h + 4)
                name = cstr(pe, p_type + TYPEDESC_NAME_OFF) if p_type else None
                out["catches"].append(
                    {
                        "adjectives": adj,
                        "p_type": p_type,
                        "type_name": name,
                        "handler": pe.u32(h + 12),
                    }
                )
                if name:
                    out["type_names"].add(name)
    return out


def eh_signature(pe: PE, va: int, funcs: dict) -> tuple:
    """Everything the EH chain can say about ``va``, as a comparable tuple.

    This is deliberately *maximal* -- handler routine, state count, try-block
    count, EH flags and every catch clause's (adjectives, type name).  If even
    this maximal signature is constant across a hit set, no weaker EH-derived
    probe can separate it either.
    """
    info = funcs.get(va)
    if info is None:
        return ("no-pdata",)
    if not info["eh"]:
        return ("no-eh",)
    d = decode_eh(pe, va)
    if d["status"] != "ok":
        return ("eh", d["status"])
    return (
        "eh",
        d["handler"],
        d["max_state"],
        d["n_try"],
        d["eh_flags"],
        tuple(sorted((c["adjectives"], c["type_name"]) for c in d["catches"])),
    )


# -------------------------------------------------------------------- reports
def cmd_census(pe: PE, funcs: dict) -> None:
    st = Counter()
    setsize = Counter()
    names = Counter()
    adjectives = Counter()
    nameset_groups: dict[frozenset, int] = Counter()

    st["pdata_entries"] = len(funcs)
    for va, info in funcs.items():
        if not info["eh"]:
            continue
        st["eh_flagged"] += 1
        d = decode_eh(pe, va)
        st["status/" + d["status"]] += 1
        if d["status"] != "ok":
            continue
        st["handler/0x%08x" % (d["handler"] or 0)] += 1
        st["ntry_%d" % min(d["n_try"], 3)] += 1
        for c in d["catches"]:
            adjectives["adj=0x%x p_type=%s" % (c["adjectives"], "0" if not c["p_type"] else "nonzero")] += 1
        for n in d["type_names"]:
            names[n] += 1
        setsize[len(d["type_names"])] += 1
        nameset_groups[frozenset(d["type_names"])] += 1

    print("== EH census ==")
    for k in sorted(st):
        print("  %-34s %d" % (k, st[k]))
    print("\n== catch-handler shapes ==")
    for k, v in adjectives.most_common():
        print("  %-40s %d" % (k, v))
    print("\n== type-name set size distribution ==")
    for k in sorted(setsize):
        print("  %d names: %d functions" % (k, setsize[k]))
    print("\n== most common type-descriptor names ==")
    if not names:
        print("  (none -- the binary contains no typed catch clauses)")
    for n, c in names.most_common(30):
        print("  %-50s %d" % (n, c))
    print("\n== how discriminating would the name-set be? ==")
    print("  distinct name-sets: %d" % len(nameset_groups))
    for s, c in nameset_groups.most_common(5):
        print("  %-50s shared by %d functions" % (sorted(s) or "(empty set)", c))


def cmd_variation(pe: PE, funcs: dict, results: Path) -> None:
    """The decisive test: does the EH signature vary WITHIN a hit set?

    Mirrors ``pdata_shape_probe.py``.  Candidates in a hit set are byte-identical
    by construction, so any probe that is constant across them is not evidence.
    """
    merged = json.load(open(results))
    sig_cache: dict[int, tuple] = {}

    def sig(v: int) -> tuple:
        if v not in sig_cache:
            sig_cache[v] = eh_signature(pe, v, funcs)
        return sig_cache[v]

    per_set: dict[tuple, str] = {}
    per_record = Counter()
    cls_count = Counter()
    isolating = 0

    for _tu, recs in merged.items():
        for r in recs:
            cls_count[r["cls"]] += 1
            hits = tuple(sorted(int(h, 16) for h in r["hits"]))
            if len(hits) < 2:
                continue
            per_set.setdefault(hits, r["cls"])
            if r["cls"] != "MULTI":
                continue
            sigs = {sig(h) for h in hits}
            any_eh = any(funcs.get(h, {}).get("eh") for h in hits)
            per_record["%s/%s" % ("EH" if any_eh else "noEH",
                                  "VARIES" if len(sigs) > 1 else "constant")] += 1

    by_set = Counter()
    for hits, cls in per_set.items():
        sigs = [sig(h) for h in hits]
        any_eh = any(funcs.get(h, {}).get("eh") for h in hits)
        varies = len(set(sigs)) > 1
        by_set["%s/%s/%s" % (cls, "EH" if any_eh else "noEH",
                             "VARIES" if varies else "constant")] += 1
        if varies and any(n == 1 for n in Counter(sigs).values()):
            isolating += 1

    print("== homing_scan record census ==")
    for k, v in cls_count.most_common():
        print("  %-12s %d" % (k, v))
    print("\n== distinct hit tuples with >=2 candidates: %d ==" % len(per_set))
    for k in sorted(by_set):
        print("  %-34s %d" % (k, by_set[k]))
    print("  hit sets where variation isolates a unique candidate: %d" % isolating)
    print("\n== per-record view (MULTI only) ==")
    for k in sorted(per_record):
        print("  %-20s %d" % (k, per_record[k]))
    tot = sum(per_record.values())
    var = sum(v for k, v in per_record.items() if k.endswith("VARIES"))
    if tot:
        print("\n  EH signature varies for %d of %d MULTI records (%.3f %%)"
              % (var, tot, 100.0 * var / tot))


def cmd_dump(pe: PE, funcs: dict, va: int) -> None:
    info = funcs.get(va)
    print("VA 0x%08x  pdata=%s" % (va, info))
    d = decode_eh(pe, va)
    print("  handler      = %s" % (None if d["handler"] is None else "0x%08x" % d["handler"]))
    print("  handler_data = %s" % (None if d["handler_data"] is None else "0x%08x" % d["handler_data"]))
    print("  magic        = %s" % (None if d["magic"] is None else "0x%08x" % d["magic"]))
    print("  status       = %s" % d["status"])
    print("  max_state=%s n_try=%s eh_flags=%s" % (d["max_state"], d["n_try"], d["eh_flags"]))
    print("  unwind actions: %s" % ["0x%08x" % a for a in d["unwind_actions"]])
    for c in d["catches"]:
        print("  catch: adjectives=0x%x p_type=%s type_name=%r handler=%s"
              % (c["adjectives"],
                 "0x%08x" % c["p_type"] if c["p_type"] else "0 (catch-all)",
                 c["type_name"],
                 "0x%08x" % c["handler"] if c["handler"] else None))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", type=Path, default=None)
    ap.add_argument("--worktree", type=Path, default=Path(os.environ.get("HOMING_WT", ".")))
    ap.add_argument("--census", action="store_true", help="decode every EH-flagged function")
    ap.add_argument("--variation", action="store_true",
                    help="does the EH signature vary within a byte-identical hit set?")
    ap.add_argument("--results", type=Path, help="homing_scan merged.json (for --variation)")
    ap.add_argument("--dump", help="fully decode one VA (hex)")
    a = ap.parse_args()

    exe = a.exe or default_exe(a.worktree)
    pe = PE(exe)
    funcs = parse_pdata(pe)

    if a.dump:
        cmd_dump(pe, funcs, int(a.dump, 16))
        return
    if a.variation:
        if not a.results:
            ap.error("--variation needs --results merged.json")
        cmd_variation(pe, funcs, a.results)
        return
    cmd_census(pe, funcs)


if __name__ == "__main__":
    main()
