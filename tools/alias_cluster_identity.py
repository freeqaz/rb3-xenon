#!/usr/bin/env python3
"""Are the SURVIVORS of a multi-group alias cluster one retail function or many?

WHY THE QUESTION EXISTS
-----------------------
``scripts/symbol_aliases.json`` renders to a synthetic MSVC map whose only
structure is "these names share an address".  842 of its 6,450 names sit at more
than one address, the worst at 67 -- and objdiff's ``parse_msvc_map`` gives such
a name an arbitrary one of its groups (see ``tools/alias_coinflip_audit.py``).
Before deciding how to repair that, one has to know what the duplication MEANS.
Three candidates:

  (i)   the alias file should have taken the transitive closure -- the data is
        un-closed and the duplication is an encoding artefact;
  (ii)  a name genuinely sits at two addresses -- the data is fine and objdiff
        must define a tie-break;
  (iii) the renderer invents the duplication when it flattens groups.

WHAT THIS TOOL MEASURES
-----------------------
A cluster is a set of alias groups sharing one folded-spelling set: N groups, N
distinct survivor addresses, one shared list of the spellings our compiler emits.
For each cluster it reads the RETAIL bytes at every survivor address (extent from
``.pdata``) and asks how far apart those N bodies are:

  * identical raw;
  * identical once BRANCH displacements are masked (``bl`` is PC-relative, so two
    copies of one function at two addresses differ here by construction);
  * identical once branch + every relocatable D-form/``addi``/``addis``/``ori``
    immediate is masked -- i.e. modulo relocated fields, the tier-T1 standard the
    alias evidence was gathered under;
  * how many words still differ, and at which opcodes.

MEASURED, 2026-08-12, on the 1,440-group file
---------------------------------------------
The 67-group ``??$Find@V<T>@ObjectDir@@`` cluster: all 67 survivors are 164 bytes,
all 67 RAW bodies distinct, all 67 still distinct with branch and D-form masking,
and all 67 collapse to ONE body once ``addi``/``addis`` immediates are masked too.
The words that vary are five ``bl`` displacements (relative, so they differ purely
because the functions sit at different addresses) and ONE ``addis``/``addi`` hi/lo
pair -- the type-name pointer that is the entire difference between one template
instantiation and the next.

So the answer is none of (i)-(iii) as stated.  The 67 survivors are 67 GENUINELY
DIFFERENT retail functions, each with its own name at its own address (all 67
confirmed in ``scripts/target_symbol_map.json``); no name sits at two addresses in
retail, and the renderer invents nothing.  What is many-to-many is the EVIDENCE:
tier T1 compares our compiled body against retail bytes modulo relocated fields,
and that mask erases the single word that tells the 67 apart, so each of the 48
folded spellings matches all 67 survivors.  A relation, rendered into a file
format that can only express a partition, necessarily repeats a name.

The consequence for the repair: taking the transitive closure would declare the
67 survivors equivalent TO EACH OTHER, which they demonstrably are not -- they
differ in a live relocated pointer.  dc3's closure argument does not transfer,
because dc3's classes come from folds the retail linker actually performed (the
bodies there really are bit-identical); these 67 the linker declined to fold, and
that refusal is the evidence that they differ.

    python3 tools/alias_cluster_identity.py            # every multi-group cluster
    python3 tools/alias_cluster_identity.py --top 1    # just the worst
"""

import argparse
import collections
import importlib.util
import json
import os
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

_spec = importlib.util.spec_from_file_location(
    "fs", str(ROOT / "tools" / "maprow_audit" / "ck4_foldscan.py"))
fs = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fs)
from thunk_identity import Image  # noqa: E402

BRANCH_OPS = (16, 18)                       # bc, b/bl -- PC-relative
LOW_HALF_OPS = {14, 15, 24, 25} | set(range(32, 48))   # addi/addis/ori/oris + D-form


def mask(body, low_ops):
    out = bytearray(body)
    for off in range(0, len(body) - 3, 4):
        word = struct.unpack_from(">I", body, off)[0]
        op = word >> 26
        if op in BRANCH_OPS:
            word &= 0xFC000003
        elif op in low_ops:
            word &= 0xFFFF0000
        struct.pack_into(">I", out, off, word)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--aliases", default=str(ROOT / "scripts" / "symbol_aliases.json"))
    ap.add_argument("--top", type=int, default=0, help="only the N largest clusters")
    args = ap.parse_args()

    groups = json.loads(Path(args.aliases).read_text())["groups"]
    clusters = collections.defaultdict(list)
    for g in groups:
        clusters[frozenset(g.get("folded", []))].append(g)
    multi = sorted((c for c in clusters.values() if len(c) > 1),
                   key=len, reverse=True)
    if args.top:
        multi = multi[:args.top]
    print(f"{len(multi)} clusters with more than one survivor address")

    img = Image(fs.BAND)
    extents = {va: length for va, length, _p, _e in fs.pdata(img)}

    for cluster in multi:
        bodies = {}
        for g in cluster:
            va = int(g["address"], 16)
            length = extents.get(va)
            if length is None:
                continue
            off = img.offset(va)
            bodies[va] = bytes(img.data[off:off + length])
        if len(bodies) < 2:
            print(f"  cluster of {len(cluster)}: no .pdata extent, skipped")
            continue
        sizes = collections.Counter(len(b) for b in bodies.values())
        raw_distinct = len(set(bodies.values()))
        branch_distinct = len({mask(b, set()) for b in bodies.values()})
        full_distinct = len({mask(b, LOW_HALF_OPS) for b in bodies.values()})
        ref = next(iter(bodies.values()))
        varying = set()
        for body in bodies.values():
            if len(body) != len(ref):
                continue
            for off in range(0, len(ref) - 3, 4):
                if body[off:off + 4] != ref[off:off + 4]:
                    varying.add(off)
        ops = sorted({struct.unpack_from(">I", ref, o)[0] >> 26 for o in varying})
        print(f"  cluster: {len(cluster)} survivors, {len(cluster[0].get('folded', []))} folded "
              f"spellings, sizes {dict(sizes)}")
        print(f"    distinct bodies  raw {raw_distinct}  branch-masked {branch_distinct}  "
              f"relocation-masked {full_distinct}   varying words {len(varying)} "
              f"at opcodes {ops}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
