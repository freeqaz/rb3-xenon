#!/usr/bin/env python3
"""Find symbols in OUR compiled objs, so a replacement map name is lifted
VERBATIM from the destination unit's own symbol table rather than hand-typed.

Hard limit #1 (W9, -180 B / -3 fns when skipped): if the pinned unit's base obj
cannot define the replacement name, the row goes PERMANENTLY 0%. Typing the
name by hand cannot satisfy that limit; reading it out of the obj satisfies it
by construction.

Usage: python3 tools/objsym_find.py <substr> [<substr>...] [--unit NAME]
"""
import glob
import os
import sys

ROOT = os.environ.get("RB3_ROOT", ".")
OBJDIR = os.path.join(ROOT, "build/45410914/src")
sys.path.insert(0, os.path.join(ROOT, "tools"))
from coff_bodies_ext import function_bodies_ext  # noqa: E402


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    unit = None
    if "--unit" in sys.argv:
        unit = sys.argv[sys.argv.index("--unit") + 1]
        args = [a for a in args if a != unit]
    pats = args
    for path in sorted(glob.glob(os.path.join(OBJDIR, "**", "*.obj"),
                                 recursive=True)):
        if unit and unit.lower() not in path.lower():
            continue
        try:
            names = {n for n, _b, _r, _e in function_bodies_ext(path)}
        except Exception:
            continue
        rel = os.path.relpath(path, OBJDIR)
        for n in sorted(names):
            if all(p in n for p in pats):
                print(f"{rel}\n   {n}")


if __name__ == "__main__":
    main()
