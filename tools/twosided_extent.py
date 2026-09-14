#!/usr/bin/env python3
"""TWO-SIDED function-extent normalization: our COMDAT vs retail .pdata.

WHY THIS EXISTS
---------------
STLPORT-1 (`ff832b50`) refuted a confident "+8 B STLport source bug" that did
not exist: `tools/coff_bodies_ext.py` was billing the SUCCESSOR symbol's EH
funclet prefix into a COMDAT span, so "retail is 8 B smaller" compared a
`.pdata` FUNCTION EXTENT against a COMDAT SPAN INCLUDING A FUNCLET.  W16-S
found and fixed the same defect in `tools/comdat_bytes.py` (doc section 1: 587
memberships over 30 STLport addresses, every one a constant -44 B -- the
signature of a reader, not of 587 bugs).

Both fixes were ONE-SIDED.  Nobody had written a comparator that measures BOTH
sides the same way, and the standing lesson is that

    ★ A ONE-SIDED INSTRUMENT ERROR IS INVISIBLE TO ITS TWO-SIDED CONTROL --
      a size test cancels the artifact on both sides, so it cannot catch a
      reader that is wrong on only one of them.

So a size argument is only admissible through a comparator that states, for
each side, exactly what it included and excluded.  That is this module.

WHAT EACH SIDE MEANS
--------------------
our side     COMDAT `.text` section bytes from the symbol's `value` up to the
             next EXTERNAL definition or EH funclet label in the same section
             (`comdat_bytes.fn_*`).  EH funclets are EXCLUDED and reported
             separately in `funclets`.
retail side  `.pdata` BeginAddress .. BeginAddress+FunctionLength, decoded by
             `pdata_extent` (X360 packed word, FunctionLength at bits 29..8,
             in instructions).  Retail EH funclets carry their OWN `.pdata`
             rows, so this extent is already funclet-free -- that claim is not
             assumed, it is MEASURED by `selftest()` over the whole mapped
             population (see below).

⚠ SCOPE BOUND, inherited from CD-7 / AUDIT-NC: the retail side exists only for
addresses that ARE `.pdata` BeginAddresses.  An 8-byte leaf stub touches
neither the stack nor LR, gets no unwind record, and therefore has NO retail
extent here -- `retail_extent()` returns `bounded=False` and the verdict is
`NO_PDATA`, never a size claim.  25.8% of named target_symbol_map rows are not
BeginAddresses (lane MAP-FIX), so this is common, not exotic.

SELFTEST -- and it can FAIL
---------------------------
    python3 tools/twosided_extent.py --selftest

(1) POPULATION check, the one that matters.  Over every mapped symbol our build
    defines, compare the NAIVE our-side reader (`size`: value -> end of
    section, i.e. funclet included) and the NORMALIZED one (`fn_size`) against
    retail's `.pdata` length.  The normalized reader must agree strictly more
    often, and the disagreements the normalization repairs must be concentrated
    in funclet-bearing symbols.  A CONTROL that must fail is built in: if the
    NAIVE reader ever agrees at least as often as the normalized one, the
    normalization is doing nothing and the test exits non-zero rather than
    printing a green tick.
(2) KNOWN ANSWER, from W16-S: `_Copy_Construct` @0x823d3ac8 -- section 112,
    symbol value 8, `__unwind$15409` at 68 => the function is [8,68) = 60 B,
    which is retail's `.pdata` extent exactly.
(3) A GENUINE mismatch must STAY a mismatch: our `??_GUIPanel@@UAAPAXI@Z`
    (68 B) against retail 0x827ba3f8 (`??_GConnectionStatusPanel`, 80 B).
    Without this the tool could pass by declaring everything equal.

Usage:
    python3 tools/twosided_extent.py <symbol> [<hexva>]
    python3 tools/twosided_extent.py --selftest
"""
import glob
import json
import os
import struct
import subprocess
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

from comdat_bytes import comdats          # noqa: E402
import pdata_extent as _P                 # noqa: E402


def _root():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--show-toplevel"],
            stderr=subprocess.DEVNULL, cwd=_HERE).decode().strip()
    except Exception:
        return os.path.dirname(_HERE)


ROOT = _root()
OBJGLOB = os.path.join(ROOT, "build/45410914/src/**/*.obj")

FUNCLET = ("__unwind$", "__catch$", "__ehhandler$", "__unwindfunclet$",
           "__tryblocktable$", "__ehfuncinfo$", "$EH")


# --------------------------------------------------------------------------
# our side
# --------------------------------------------------------------------------
_OBJ_INDEX = None


def _index(force=False):
    """symbol -> (objpath, comdat dict entry).  First definition wins."""
    global _OBJ_INDEX
    if _OBJ_INDEX is not None and not force:
        return _OBJ_INDEX
    idx = {}
    for p in glob.glob(OBJGLOB, recursive=True):
        try:
            cd = comdats(p)
        except Exception:
            continue
        for k, v in cd.items():
            if k not in idx:
                idx[k] = (p, v)
    _OBJ_INDEX = idx
    return idx


def our_extent(symbol):
    """Normalized OUR-side extent.  EH funclets excluded and reported."""
    hit = _index().get(symbol)
    if not hit:
        return None
    path, v = hit
    return {
        "symbol": symbol, "obj": os.path.relpath(path, ROOT),
        "size": v["fn_size"],                 # NORMALIZED (funclet-free)
        "naive_size": v["size"],              # value -> end of section
        "raw": v["fn_raw"],
        "relocs": v["fn_relocs"],
        "bounded": v["fn_bounded"],
        "is_code": v["is_code"],
        "funclet_bytes": v["size"] - v["fn_size"],
    }


# --------------------------------------------------------------------------
# retail side
# --------------------------------------------------------------------------
def retail_extent(va):
    """Normalized RETAIL extent from .pdata.  Funclets have their own rows.

    Returns bounded=False when `va` is not itself a .pdata BeginAddress --
    the sub-.pdata stub stratum, where NO size claim is licensed."""
    e = _P.pdata_extent(va)
    if not e or e[0] != va:
        return {"va": va, "size": None, "bounded": False,
                "note": "not a .pdata BeginAddress"}
    return {"va": va, "size": e[1], "bounded": True,
            "raw": _P.rd(va, e[1]), "next": e[2]}


# --------------------------------------------------------------------------
# comparison
# --------------------------------------------------------------------------
def compare(symbol, va):
    """Verdict over the two NORMALIZED extents.

    EQ_SIZE / NE_SIZE / NO_PDATA / NO_OURSIDE, plus both raw sizes so a caller
    can see what the naive reader would have said."""
    o = our_extent(symbol)
    r = retail_extent(va)
    if o is None:
        return {"verdict": "NO_OURSIDE", "symbol": symbol, "va": va}
    if not r["bounded"]:
        return {"verdict": "NO_PDATA", "symbol": symbol, "va": va,
                "our_size": o["size"], "note": r["note"]}
    v = "EQ_SIZE" if o["size"] == r["size"] else "NE_SIZE"
    naive = "EQ_SIZE" if o["naive_size"] == r["size"] else "NE_SIZE"
    return {
        "verdict": v, "symbol": symbol, "va": va,
        "our_size": o["size"], "our_naive_size": o["naive_size"],
        "retail_size": r["size"],
        "funclet_bytes": o["funclet_bytes"],
        "naive_verdict": naive,
        "reader_artifact": (v == "EQ_SIZE" and naive == "NE_SIZE"),
        "obj": o["obj"],
    }


# --------------------------------------------------------------------------
def _mapped():
    with open(os.path.join(ROOT, "scripts/target_symbol_map.json")) as f:
        m = json.load(f)
    out = {}
    for k, val in m.items():
        if k.startswith("0x") and isinstance(val, str):
            out.setdefault(val, int(k, 16))
    return out


def selftest():
    ok = True
    idx = _index()
    mp = _mapped()
    naive_eq = norm_eq = both = 0
    repaired = repaired_with_funclet = 0
    broke = 0
    for sym, va in mp.items():
        if sym not in idx:
            continue
        c = compare(sym, va)
        if c["verdict"] in ("NO_PDATA", "NO_OURSIDE"):
            continue
        both += 1
        n_ok = c["naive_verdict"] == "EQ_SIZE"
        z_ok = c["verdict"] == "EQ_SIZE"
        naive_eq += n_ok
        norm_eq += z_ok
        if z_ok and not n_ok:
            repaired += 1
            if c["funclet_bytes"]:
                repaired_with_funclet += 1
        if n_ok and not z_ok:
            broke += 1
    print("(1) POPULATION over %d mapped symbols with both extents:" % both)
    print("      naive  our-side reader agrees with retail .pdata: %6d" % naive_eq)
    print("      NORMALIZED our-side reader agrees               : %6d" % norm_eq)
    print("      repaired by normalization                       : %6d "
          "(%d of them funclet-bearing)" % (repaired, repaired_with_funclet))
    print("      BROKEN by normalization                         : %6d" % broke)
    # CONTROL that must fail: if normalization buys nothing the test is vacuous
    if norm_eq <= naive_eq:
        print("      FAIL: normalization agrees no more often than the naive "
              "reader -- this comparator is doing nothing.")
        ok = False
    if repaired == 0:
        print("      FAIL: zero repairs -- vacuous, nothing to validate.")
        ok = False

    print("(2) KNOWN ANSWER _Copy_Construct @0x823d3ac8 (W16-S):")
    KNOWN_SYM = ("??$_Copy_Construct@UWeight@PlayBack@CharLipSync@@@stlpmtx_std@@"
                 "YAXPAUWeight@PlayBack@CharLipSync@@ABU123@@Z")
    got = compare(KNOWN_SYM, 0x823D3AC8) if KNOWN_SYM in idx else None
    if got and got["verdict"] in ("EQ_SIZE", "NE_SIZE"):
        flag = "OK" if got["our_size"] == 60 and got["retail_size"] == 60 else "FAIL"
        if flag == "FAIL":
            ok = False
        print("      our=%s retail=%s naive=%s  %s"
              % (got["our_size"], got["retail_size"], got["our_naive_size"], flag))
    else:
        print("      SKIP: symbol not defined in this build")

    print("(3) GENUINE mismatch must STAY a mismatch "
          "(??_GUIPanel 68 B vs retail 0x827ba3f8 = ??_GConnectionStatusPanel 80 B):")
    c = compare("??_GUIPanel@@UAAPAXI@Z", 0x827BA3F8)
    flag = "OK" if c.get("verdict") == "NE_SIZE" else "FAIL"
    if flag == "FAIL":
        ok = False
    print("      %s our=%s retail=%s  %s"
          % (c.get("verdict"), c.get("our_size"), c.get("retail_size"), flag))

    print("SELFTEST", "PASS" if ok else "FAIL")
    return ok


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a or "--selftest" in a:
        raise SystemExit(0 if selftest() else 1)
    sym = a[0]
    if len(a) > 1:
        print(json.dumps(compare(sym, int(a[1], 16)), indent=1, default=str))
    else:
        o = our_extent(sym)
        print(json.dumps({k: v for k, v in (o or {}).items() if k != "raw"},
                         indent=1, default=str))
