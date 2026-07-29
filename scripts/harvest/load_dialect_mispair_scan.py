#!/usr/bin/env python3
"""Discriminate real retail ``Load`` bodies from map mispairs, straight off the
dtk split assembly.

Two checks, cheapest first.

1. UNIT-LEVEL NEGATIVE EXISTENCE PROOF (the strong one).
   RB3-360 retail's ``LOAD_REVS`` splits the packed rev int into a high and a
   low halfword, which always costs one ``srwi rN, rM, 16``.  So if a pinned
   unit's whole ``.text`` span contains ZERO ``srwi ..., 16``,
   then no ``LOAD_REVS`` body lives anywhere in that span -- and every
   ``?Load@X@@...`` the target symbol map points into it is a mispair.  This is
   a proof over the entire span, which is strictly stronger than arguing from
   one body's shape.

   NOTE: do NOT use the ``sth``-to-gRev store as the unit-level marker.  MSVC
   deletes those stores whenever nothing outside the function reads the statics
   back, so their absence proves nothing.  The ``srwi ..., 16`` is not
   removable -- the switch/if chain consumes the split value.

   (Verified on ``default/Group``: zero ``srwi ..., 16`` in the whole span, so
   ``?Load@RndGroup@@UAAXAAVBinStream@@@Z`` at 0x82453d80 cannot be RndGroup's
   Load.  Its retail body is a list walk with a Replace shape.)

2. BODY-LEVEL SHAPE CHECK (the per-function one).
   For a mapped ``Load`` VA, look for the retail rev preamble --
   ``ReadEndian(&rev, 4)`` immediately followed by ``srwi rX, rY, 16`` and a
   ``sth`` pair.  Absent that, report which *other* shape the body has:

   * ``__RTDynamicCast``-with-two-type-name-strings  => a ``Copy`` body
     (``Copy(const Hmx::Object*, CopyType)`` -- 3 args, RTTI down-cast).  This
     is by far the most common mispair in the rndobj/char units.
   * a single ``b <import>``                        => an import thunk.
   * neither                                        => unclassified; read it.

Usage::

    scripts/harvest/load_dialect_mispair_scan.py --all
    scripts/harvest/load_dialect_mispair_scan.py --unit Group
    scripts/harvest/load_dialect_mispair_scan.py --symbol '?Load@RndGroup@@UAAXAAVBinStream@@@Z'

Reads ``build/45410914/asm/*.s`` (dtk split output) and
``scripts/target_symbol_map.json``.  Read-only.
"""

import argparse
import glob
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ASM_DIR = os.path.join(REPO, "build", "45410914", "asm")
MAP = os.path.join(REPO, "scripts", "target_symbol_map.json")

# BinStream::ReadEndian in the retail image.  Split asm names it either by the
# mangled symbol (when symbols.txt has it) or as fn_827C5058.
READ_ENDIAN = ("fn_827C5058", "?ReadEndian@BinStream@@QAAXPAXH@Z")
RT_DYNAMIC_CAST = "fn_8282A0C8"

# The rev split itself: `srwi rN, rM, 16` takes the high half of the packed rev
# word.  This is the load-bearing marker -- it survives even when MSVC deletes
# the stores to gRev/gAltRev as dead (internal-linkage statics whose only reader
# is the same function), which happens in most bodies.
REV_SPLIT = re.compile(r"\bsrwi\s+r\d+,\s*r\d+,\s*16\b")
# The halfword stores, either `sth rN, lbl_X@l(rM)` or `sth rN, 0x4(rM)` off a
# materialised base -- present only when something reads the statics back.
STH_TO_DATA = re.compile(r"\bsth\s+r\d+,")
COMMENT = re.compile(r"/\* [0-9A-F]+ [0-9A-F]+ +[0-9A-F ]*\*/")


def load_map():
    with open(MAP) as fh:
        raw = json.load(fh)
    # 264 legacy keys are uppercase `0X...`; normalise for lookup, keep the
    # original spelling so callers can emit a fragment row against the real key.
    out = {}
    for key, val in raw.items():
        if isinstance(val, str):
            out[key.lower()] = (key, val)
    return out


def iter_units():
    for path in sorted(glob.glob(os.path.join(ASM_DIR, "*.s"))):
        yield os.path.splitext(os.path.basename(path))[0], path


def strip(text):
    return COMMENT.sub("", text)


def unit_has_rev_store(path):
    """Negative existence proof over a whole pinned unit span."""
    text = strip(open(path, errors="replace").read())
    hits = REV_SPLIT.findall(text)
    return len(hits), text


def find_body(text, va):
    fn = "fn_%s" % va[2:].upper()
    i = text.find(".fn %s," % fn)
    if i < 0:
        return None
    j = text.find(".endfn", i)
    return text[i:j]


def classify_body(body):
    if body is None:
        return "NOT_IN_SPAN", "the mapped VA is not inside any pinned .text span"
    lines = [l.strip() for l in body.splitlines() if l.strip()]
    code = [l for l in lines if not l.startswith(".")]
    if len(code) <= 2 and any(l.startswith("b ") for l in code):
        return "IMPORT_THUNK", "single branch to an import -- not a Load"
    has_read = any(sym in body for sym in READ_ENDIAN)
    has_split = bool(REV_SPLIT.search(body))
    stores = "with the gRev/gAltRev stores kept" if len(STH_TO_DATA.findall(body)) >= 2 else "gRev/gAltRev stores dead-code-eliminated"
    if has_read and has_split:
        return "LOAD_REV_PREAMBLE", "retail dialect-B rev preamble present (%s) -- real Load" % stores
    if RT_DYNAMIC_CAST in body:
        return "COPY_SHAPE", "calls __RTDynamicCast with two type-name strings -- a Copy body"
    if has_read:
        return "READS_NO_REV", "reads the stream but stores no rev -- Load whose rev statics were dead, or a mispair"
    return "UNCLASSIFIED", "no stream read, no rev store, no dynamic cast -- read it by hand"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--all", action="store_true", help="scan every ?Load@ in the map")
    ap.add_argument("--unit", help="unit stem, e.g. Group (runs the negative existence proof)")
    ap.add_argument("--symbol", help="one mangled ?Load@ symbol")
    args = ap.parse_args()

    if args.unit:
        path = os.path.join(ASM_DIR, args.unit + ".s")
        if not os.path.exists(path):
            sys.exit("no split asm for unit %r" % args.unit)
        n, _ = unit_has_rev_store(path)
        print("unit %-28s rev-split (srwi ...,16) count = %d" % (args.unit, n))
        if n == 0:
            print("  => NEGATIVE EXISTENCE PROOF: no LOAD_REVS body anywhere in this span.")
            print("     Every ?Load@X@@ the map points into this unit is a MISPAIR.")
        else:
            print("  => at least one rev-storing Load is present; fall back to per-body checks.")
        return

    smap = load_map()
    by_name = {}
    for key, val in smap.values():
        by_name.setdefault(val, []).append(key)

    if args.symbol:
        wanted = [args.symbol]
    elif args.all:
        wanted = sorted(n for n in by_name if n.startswith("?Load@") or n.startswith("?PreLoad@") or n.startswith("?PostLoad@"))
    else:
        sys.exit("pass --all, --unit or --symbol")

    texts = {stem: strip(open(p, errors="replace").read()) for stem, p in iter_units()}

    print("%-56s %-12s %-10s %-18s %s" % ("SYMBOL", "VA", "UNIT", "VERDICT", "WHY"))
    for name in wanted:
        for va in by_name.get(name, []):
            body, unit = None, "-"
            for stem, text in texts.items():
                body = find_body(text, va)
                if body is not None:
                    unit = stem
                    break
            verdict, why = classify_body(body)
            print("%-56s %-12s %-10s %-18s %s" % (name[:56], va, unit[:10], verdict, why))


if __name__ == "__main__":
    main()
