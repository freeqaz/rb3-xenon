#!/usr/bin/env python3
"""W25-UI: decide FOLD vs WRONG-CALLEE for a set of same-address candidates.

WHY THIS EXISTS. Retail's Handle@BandUI dispatcher calls ONE address from five
arms that each construct a provably different message class (RTTI read from
retail bytes). That proves the five handler ROLES resolve to one address. It
does NOT by itself decide between:

  (a) FOLD      -- five distinct handler functions with identical code, which
                   /OPT:ICF merged; our source is right and the surviving map
                   name is arbitrary among the class; or
  (b) OUR BUG   -- retail really has ONE handler, and our source wrongly carries
                   five overloads.

The discriminator is our OWN build: if our COMDATs for the five spellings are
byte- AND relocation-identical (same reloc offsets, same reloc types, same
target symbol NAMES), then /OPT:ICF *must* place them at one address -- that is
the linker's own condition, evaluated on our own objects. Under (b) our
overloads would have to differ.

WHAT THIS CANNOT RULE OUT, STATED PLAINLY (same bound ourside_fold_sweep.py
declares): identity of OUR bodies is a fact about OUR build. If retail's source
for F differed from retail's source for S, retail did not fold them and our F is
simply wrong in a way that coincidentally equals S. That is cheapest on SHORT,
relocation-free bodies -- so this tool reports body size and relocation count
and refuses to call a zero-relocation stub proven.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from scripts.analysis.coffx import read_coff  # noqa: E402


def sym_index_names(syms):
    return {s.index: s.name for s in syms}


def body_of(secs, syms, name):
    """Return (bytes, [(off, type, target_name)]) for a symbol's COMDAT."""
    idx = sym_index_names(syms)
    for s in syms:
        if s.name != name or s.sec <= 0:
            continue
        sec = secs[s.sec - 1]
        if not sec.is_code:
            continue
        data = sec.data
        rels = []
        for va, symidx, typ in sec.relocs:
            rels.append((va, typ, idx.get(symidx, f"?{symidx}")))
        rels.sort()
        return data, rels
    return None, None


def main():
    obj = Path(sys.argv[1])
    survivor = sys.argv[2]
    folded = sys.argv[3:]
    data = obj.read_bytes()
    secs, syms = read_coff(data)
    if secs is None:
        print("REFUSE: not a COFF object")
        return 2

    sd, sr = body_of(secs, syms, survivor)
    if sd is None:
        print(f"REFUSE (VACUITY GUARD): survivor not found in {obj}")
        print(f"  {survivor}")
        return 2

    print(f"object   : {obj}")
    print(f"survivor : {survivor}")
    print(f"  body   : {len(sd)} bytes, {len(sr)} relocations")
    for off, typ, tn in sr:
        print(f"     +0x{off:04x} type=0x{typ:04x} -> {tn[:72]}")
    print()

    allok = True
    for f in folded:
        fd, fr = body_of(secs, syms, f)
        if fd is None:
            print(f"NOT FOUND (cannot prove): {f}")
            allok = False
            continue
        same_bytes = (fd == sd)
        same_rels = (fr == sr)
        verdict = "IDENTICAL" if (same_bytes and same_rels) else "DIFFERS"
        print(f"[{verdict}] {f}")
        print(f"    bytes {len(fd)} (equal={same_bytes})  "
              f"relocs {len(fr)} (equal={same_rels})")
        if not same_bytes:
            n = min(len(fd), len(sd))
            diffs = [i for i in range(0, n, 4) if fd[i:i+4] != sd[i:i+4]]
            print(f"    first differing words at: {diffs[:8]}")
        if not same_rels:
            print(f"    survivor relocs: {sr}")
            print(f"    folded   relocs: {fr}")
        if not (same_bytes and same_rels):
            allok = False
    print()
    if allok:
        print("RESULT: every folded spelling is byte- AND relocation-identical")
        print("        to the survivor in OUR build => /OPT:ICF must merge them.")
        if len(sr) == 0:
            print("⚠ ZERO-RELOCATION BODY: an unimplemented stub in our tree also")
            print("  compiles to this, so identity here is CHEAP. NOT proven.")
            return 1
    else:
        print("RESULT: at least one spelling is NOT identical in our build.")
        print("        The fold is NOT established -- withdraw the alias.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
