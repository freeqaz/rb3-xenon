#!/usr/bin/env python3
"""W16-AA: WHICH of our candidate COMDATs is the retail body at address X?

Given a retail address and a list of candidate mangled names, compare retail's
.pdata-authoritative extent at X against each candidate's function-only COMDAT
extent (`fn_raw`/`fn_relocs` from comdat_bytes -- NOT `raw`, which runs to the
section end and bills the trailing __unwind$ funclet into the body, the ONE-SIDED
artifact that produced the phantom "+8 B STLport source bug" in lane STLPORT-1).

Both sides are therefore FUNCTION EXTENTS, measured the same way, so a size
disagreement here is a real disagreement rather than a reader artifact.

The comparison masks relocated words (their values are link-time) but, like
w16s_alias_census.retail_compare, decodes b/bl destinations and reports them --
here WITHOUT requiring a map name, because the caller often wants to see the raw
destination in order to name it.
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
from wrong_callee_triage import Image, load_sizes       # noqa: E402

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


def main():
    va = int(sys.argv[1], 16)
    cands = sys.argv[2:]
    fns, where = our_index()
    img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
    size = load_sizes()
    smap = json.load(open(ROOT / "scripts/target_symbol_map.json"))
    byva = {}
    for k, v in smap.items():
        try:
            byva[int(k, 16)] = v
        except (ValueError, TypeError):
            pass

    n = size.get(va)
    o = img.off(va)
    print("retail 0x%08x : .pdata extent %s B, map name %r"
          % (va, n, byva.get(va)))
    if not n or o is None:
        print("  UNREADABLE")
        return
    rw = list(struct.unpack_from(">%dI" % (n // 4), img.data, o))
    # retail's own outgoing calls
    print("  retail outgoing b/bl:")
    for i, w in enumerate(rw):
        if (w >> 26) == 18 and not (w & 2):
            d = w & 0x03FFFFFC
            if d & 0x02000000:
                d -= 0x04000000
            dest = (va + i * 4 + d) & 0xFFFFFFFF
            print("    +0x%-4x -> 0x%08x  %s" % (i * 4, dest,
                                                 byva.get(dest) or "(UNNAMED)"))
    for c in cands:
        vs = fns.get(c)
        if not vs:
            print("  %-70s : NO COMDAT in our build" % c[:70])
            continue
        best = None
        for raw, rel in sorted(vs, key=lambda kv: (-len(kv[0]), kv[0], kv[1])):
            if len(raw) != n:
                if best is None:
                    best = ("SIZE", "our %d B vs retail %d B" % (len(raw), n))
                continue
            ow = list(struct.unpack(">%dI" % (n // 4), raw))
            relo = {off: s for off, s, _t in rel}
            bad = None
            for i, (x, y) in enumerate(zip(rw, ow)):
                off = i * 4
                if off in relo:
                    if (x >> 26) != (y >> 26):
                        bad = "opcode differs at +0x%x" % off
                        break
                elif x != y:
                    bad = "non-relocated word differs at +0x%x (%08x vs %08x)" % (off, x, y)
                    break
            if bad is None:
                best = ("MASKED-EQ", "%d B, %d relocs" % (len(raw), len(rel)))
                break
            if best is None or best[0] == "SIZE":
                best = ("NE", bad)
        print("  %-70s : %s  %s" % (c[:70], best[0], best[1]))
        print("       (%d variant(s), obj %s)" % (len(vs), sorted(where[c])[0].split("/src/")[-1]))


if __name__ == "__main__":
    main()
