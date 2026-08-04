#!/usr/bin/env python3
"""Delete dtk split outputs that the CURRENT split no longer emits.

`dtk xex split` writes one `.s` (under `<build>/asm/`) and one target `.obj`
(under `<build>/obj/`) per unit, rewriting the whole live set on every run. It
never *removes* a file whose unit disappeared -- so every time a heading in
`config/45410914/splits.txt` is re-pathed, renamed, deleted, or has its address
range narrowed, the previous generation is orphaned on disk forever.

Those orphans are silently misleading rather than merely untidy:

  * A stale `.s` sits next to (or shadows) the live one for the same basename,
    so anyone opening `asm/Anim.s` may be reading a generation from weeks ago.
  * An `auto_*` unit is named after the address range it covers, so when a range
    is pinned to a real unit the old `auto_<addr>` file keeps claiming those
    bytes. Any asm-wide scan keyed on address then sees two contradictory
    opinions for the same code.

Ground truth is dtk's own `<build>/config.json`: its `units[].object` list is
exactly what the split just produced. Anything else under `asm/`+`obj/` is a
previous generation.

Run as the second half of the `split` ninja rule, so it fires exactly when (and
only when) a split has just succeeded.
"""

import json
import os
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: prune_split_outputs.py <build_dir>", file=sys.stderr)
        return 2
    build_dir = Path(sys.argv[1])
    config_path = build_dir / "config.json"
    if not config_path.exists():
        # Split has not produced a config yet; nothing authoritative to prune
        # against. Fail open -- never guess.
        return 0

    with config_path.open() as fh:
        config = json.load(fh)
    units = config.get("units") or []
    if not units:
        # A config with no units would make EVERY file on disk look stale and
        # delete the entire split output. Refuse: an empty unit list means the
        # split went wrong, not that the tree is garbage.
        print(
            "PRUNE: refusing -- %s lists 0 units" % config_path,
            file=sys.stderr,
        )
        return 1

    # `units[].object` is repo-root-relative (e.g. build/45410914/obj/Foo.obj),
    # matching ninja's cwd. The paired listing is the same path under asm/ with
    # a .s suffix.
    live_obj = set()
    live_asm = set()
    obj_root = os.path.normpath(build_dir / "obj")
    asm_root = os.path.normpath(build_dir / "asm")
    for unit in units:
        obj = os.path.normpath(unit["object"])
        live_obj.add(obj)
        rel = os.path.relpath(obj, obj_root)
        live_asm.add(os.path.normpath(os.path.join(asm_root, rel[:-4] + ".s")))

    removed = 0
    for root, want_suffix, live in (
        (asm_root, ".s", live_asm),
        (obj_root, ".obj", live_obj),
    ):
        if not os.path.isdir(root):
            continue
        for dirpath, _dirnames, filenames in os.walk(root):
            for name in filenames:
                if not name.endswith(want_suffix):
                    # asm/ and obj/ contain nothing but .s and .obj today; if
                    # that ever changes, leave the stranger alone.
                    continue
                path = os.path.normpath(os.path.join(dirpath, name))
                if path not in live:
                    os.remove(path)
                    removed += 1
        # Directories emptied by a re-path (e.g. asm/band3/meta_band/) would
        # otherwise linger as empty husks suggesting a unit still lives there.
        for dirpath, dirnames, filenames in os.walk(root, topdown=False):
            if dirpath == root:
                continue
            if not dirnames and not filenames:
                os.rmdir(dirpath)

    if removed:
        print("PRUNE: removed %d stale split output(s)" % removed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
