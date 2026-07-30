#!/usr/bin/env python3
"""Build `bodyidx.pkl`: stripped symbol name -> {masked-body hash}.

MISSING PRODUCER, reconstructed by laneBT5. The laneAS-B branch shipped
heldout_reloc.py / emit_reloc_frag.py, which both load a `bodyidx.pkl`, but no
script on that branch ever produced one -- so the pipeline could not be run as
shipped. The contract is inferred from relocdisc.decide()'s R2 rule:

    nms = {candidate base symbol names at a discriminating reloc offset}
    hs[h].add(n) for h in bodyidx[strip(n)]
    if any(len(v) > 1 for v in hs.values()): drop this offset

i.e. R2 drops an offset when two *competing callee* symbols are themselves
reloc-masked byte twins (ICF-degenerate): they fold to one address, so the
offset cannot discriminate between the candidates naming them.

Hence: map each stripped symbol name to the set of hashes of its masked body,
over every compiled base obj in objdiff.json. Two names sharing a hash are
byte-twins modulo relocations.

Usage:  bodyidx.py <worktree> <out.pkl>
"""
import hashlib
import json
import pickle
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R  # noqa: E402


def build(worktree: Path, out_path: Path):
    S = R.load_S(worktree)
    od = json.loads((worktree / "objdiff.json").read_text())
    idx = defaultdict(set)
    nobj = nfn = 0
    for u in od["units"]:
        bp = u.get("base_path")
        if not bp:
            continue
        p = worktree / bp
        if not p.exists():
            continue
        try:
            bf, _ = R.base_funcs(p)
        except Exception:
            continue
        nobj += 1
        for f in bf:
            idx[S.anon_ns_strip(f["name"])].add(
                hashlib.sha1(f["masked"]).hexdigest())
            nfn += 1
    idx = {k: frozenset(v) for k, v in idx.items()}
    with open(out_path, "wb") as fh:
        pickle.dump(idx, fh)
    shared = sum(1 for v in idx.values() if len(v) > 1)
    print(f"bodyidx: {nobj} objs, {nfn} fns, {len(idx)} distinct names "
          f"({shared} with >1 body variant) -> {out_path}", file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    build(Path(sys.argv[1]).resolve(), Path(sys.argv[2]))
