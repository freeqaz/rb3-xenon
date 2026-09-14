#!/usr/bin/env python3
"""W16-AA: is the DISCRIMINATING TARGET pair itself co-folded?

An UNDECIDED_MASKED membership says our COMDAT for N and our COMDAT for S differ
ONLY in relocation target names at offsets the census could not adjudicate.  The
linker's fold condition is a FIXED POINT: N and S fold iff every differing target
pair is itself co-folded.  So the membership reduces, exactly, to a question about
the targets -- and that question is ENTIRELY OUR-SIDE.  No map, no retail address,
no masking channel.

  targets co-folded   -> the parent fold is STRUCTURALLY POSSIBLE; the alias is
                         forgiving a real fold and naming the retail destination
                         is expected to change nothing.
  targets NOT folded  -> the parent COMDATs CANNOT fold under /OPT:ICF, whatever
                         retail's address holds.  The alias is then forgiving a
                         genuinely different callee -- the bug this lane exists
                         to expose -- UNLESS only one of the two spellings is the
                         body at that address (an identification, not a fold).

⚠ THE "NOT FOLDED" VERDICT IS NOT BY ITSELF A BUG REPORT.  Our build compiles both
spellings; retail may contain only one of them.  The tool therefore reports WHY
the targets fail to fold (masked bytes, reloc shape, or a deeper target pair), so
the reader can tell a genuine divergence from a spelling our tree simply has extra.
"""
import collections
import glob
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from comdat_bytes import comdats                        # noqa: E402
import w16s_alias_census as CEN                         # noqa: E402

BUILD_ID = "45410914"


def our_index():
    fns = collections.defaultdict(set)
    where = collections.defaultdict(set)
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"),
                       recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for n, v in c.items():
            if not v.get("is_code"):
                continue
            rel = tuple(sorted((o, s, t) for o, s, t in (v["fn_relocs"] or [])
                               if s != "@comp.id"))
            fns[n].add((v["fn_raw"], rel))
            where[n].add(p)
    return fns, where


def masked(raw, rel):
    m = bytearray(raw)
    for o, _s, _t in rel:
        m[o:o + 4] = b"\0\0\0\0"
    return bytes(m)


def rep(vs):
    return max(vs, key=lambda kv: (len(kv[0]), kv[0], kv[1]))


def explain(a, b, fns, closure):
    """Why do our COMDATs for a and b fail to be co-folded?"""
    if a is None or b is None:
        return "one side has no relocation at this offset"
    if a == b:
        return "SAME TARGET NAME (not a discriminator)"
    va, vb = fns.get(a), fns.get(b)
    if not va:
        return "our build defines NO code COMDAT for %s" % a
    if not vb:
        return "our build defines NO code COMDAT for %s" % b
    if closure.get(a) is not None and closure.get(a) == closure.get(b):
        return "CO-FOLDED (same /OPT:ICF class)"
    ra, rla = rep(va)
    rb, rlb = rep(vb)
    if len(ra) != len(rb):
        return "NOT FOLDED: different sizes (%d B vs %d B)" % (len(ra), len(rb))
    if masked(ra, rla) != masked(rb, rlb):
        n = sum(1 for x, y in zip(masked(ra, rla), masked(rb, rlb)) if x != y)
        return "NOT FOLDED: masked bodies differ in %d bytes" % n
    sa = tuple((o, t) for o, _s, t in rla)
    sb = tuple((o, t) for o, _s, t in rlb)
    if sa != sb:
        return "NOT FOLDED: relocation shape differs"
    da = {o: s for o, s, _t in rla}
    db = {o: s for o, s, _t in rlb}
    sub = [(o, da.get(o), db.get(o)) for o in sorted(set(da) | set(db))
           if da.get(o) != db.get(o)]
    parts = []
    for o, x, y in sub:
        cx, cy = closure.get(x), closure.get(y)
        ok = cx is not None and cx == cy
        parts.append("+0x%x %s vs %s -> %s" % (o, x, y, "co-folded" if ok else "NOT"))
    return ("NOT FOLDED: bodies+shape equal, %d deeper target pair(s): %s"
            % (len(sub), "; ".join(parts)[:400]))


def main():
    diag = json.load(open(sys.argv[1]))
    fns, where = our_index()
    closure = CEN.icf_closure(fns)
    for r in diag:
        if "discriminators" not in r:
            continue
        print("=" * 100)
        print("gi=%s %s %s B" % (r["gi"], r["addr"], r["bytes"]))
        print("  S=%s" % r["survivor"][:95])
        print("  N=%s" % r["folded"][:95])
        for c in r["discriminators"]:
            print("  +0x%-4x %-28s retail_dest=%s named=%s"
                  % (c["offset"], c["kind"], c.get("retail_dest"),
                     bool(c.get("retail_dest_name"))))
            print("        -> %s" % explain(c.get("N_target"), c.get("S_target"),
                                            fns, closure))


if __name__ == "__main__":
    main()
