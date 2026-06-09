#!/usr/bin/env python3
"""Single source of truth for *which* DC3 object tree the cross-binary
function-identification tools read.

Why this module exists
----------------------
RB3 anonymous `fn_<addr>` functions are identified by matching their
(reloc-masked) instruction stream against DC3, the byte-faithful twin. There are
**two** candidate DC3 object trees on disk, and historically the id tools picked
them inconsistently:

  CANONICAL  .dc3_text_scratch/named/obj
      dtk-split of DC3's *retail* XEX (orig/373307D9/default.xex), named via the
      leaked PDB/.map. These are TARGET objs == the byte-faithful retail-DC3
      truth. Symbols are clean per-function (no ICF `merged_*` artifacts).

  PORT       ../dc3-decomp/build/373307D9/obj
      dc3-decomp's *compiled* port. Byte-identical to retail only where
      dc3-decomp is already matched; where it is unmatched (e.g. gesture/
      nuispeech units at <70%) its bytes DIVERGE from retail, so it is the WRONG
      oracle there. It also carries ICF `merged_<addr>` / `__unwind__merged_*`
      symbol names the split tree does not.

For *identifying RB3 retail functions*, the retail-DC3 TARGET tree is canonical:
it represents what the retail binaries actually contain, and its clean symbol
names avoid the `merged_*` tie-break noise. dc3_content_match.py used the PORT
tree by default and thus could disagree with global_fuzzy_index.py /
fuzzy_content_match.py (which used the TARGET tree) on ICF-ambiguous bodies (the
masked content is byte-identical across N functions, so the arbitrary
first-seen-name pick differs by tree enumeration). Unifying on the TARGET tree
makes their answers consistent.

Tools that legitimately need the PORT tree (they ask a *port-state* question,
not a retail-identity one — e.g. dc3_residual_rank.py, dc3_map.py) keep their own
path and do not use this module.

CLI override: every tool that imports this still exposes a `--dc3-dir` flag, so
either tree (or a third one) can be pointed at explicitly.
"""
import glob
import os

# Canonical retail-DC3 TARGET object tree (dtk-split of the retail XEX, named).
DC3_TARGET_OBJ_DIR = "/home/free/code/milohax/.dc3_text_scratch/named/obj"

# dc3-decomp's compiled PORT object tree (diverges from retail where unmatched).
_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DC3_PORT_OBJ_DIR = os.path.join(_ROOT, "..", "dc3-decomp", "build", "373307D9", "obj")

# Default for identification tools.
DC3_OBJ_DIR = DC3_TARGET_OBJ_DIR


def iter_dc3_objs(dc3_dir=None, recursive=True):
    """Yield every `*.obj` path under the DC3 object tree.

    `recursive=True` (the default) walks subdirectories (system/, xdk/, lib/,
    lazer/, …); the DC3 trees are nested, so a non-recursive glob misses almost
    everything. Sorted for deterministic enumeration order (so ICF tie-breaks
    are stable run-to-run).
    """
    root = dc3_dir or DC3_OBJ_DIR
    pat = os.path.join(root, "**", "*.obj") if recursive else os.path.join(root, "*.obj")
    return sorted(glob.glob(pat, recursive=recursive))


if __name__ == "__main__":
    import sys
    d = sys.argv[1] if len(sys.argv) > 1 else None
    objs = iter_dc3_objs(d)
    print(f"DC3 obj dir: {d or DC3_OBJ_DIR}")
    print(f"  {len(objs)} objs (recursive)")
