#!/usr/bin/env python3
"""Read OUR compiled objs' factory-registration lists.  Lane REGORDER-1.

Companion to ``reglist_diff``.  Our side is read from the COMPILED OBJS rather
than from source text because ``REGISTER_OBJ_FACTORY`` reaches the emitted list
through several spellings -- a direct macro use, an inlined ``X::Init()``, an
inlined ``X::Register()`` -- and only the compiler knows which ones survived
``/O1 /Ob2`` inlining into the caller.  A source-text scan of ``Rnd::PreInit``
sees 6 registrations; the obj has 43.

⚠ ONE SYMBOL, MANY OBJS.  ``?PreInit@Rnd@@`` is defined in NINE of our objs
(duplicate COMDATs of the same inline).  ``icf_alias_build.collect()`` over a
whole-tree glob is LAST-WINS across those, which on a partially-built tree
returned a stale copy and made a real source edit read as INERT.  This reader
keeps EVERY definition and asserts they agree, reporting disagreement rather
than picking one.
"""
from __future__ import annotations

import collections
import re
import sys
from pathlib import Path

SCN = re.compile(r"^\?StaticClassName@(.+?)@@")
RF = "?RegisterFactory@Object@Hmx@@"


def our_reglists(worktree, minn=2):
    """-> {symbol: [class names]} for every function with >= `minn` registrations."""
    wt = Path(worktree)
    sys.path.insert(0, str(wt / "tools"))
    from coff_bodies_ext import function_bodies_ext

    seen = collections.defaultdict(set)
    for op in sorted((wt / "build/45410914/src").rglob("*.obj")):
        try:
            fns = list(function_bodies_ext(op))
        except Exception:
            continue
        for name, _b, rl, _o in fns:
            br = [(off, n) for off, n, t in sorted(rl) if t == 0x6]
            if sum(1 for _o2, n in br if n.startswith(RF)) < minn:
                continue
            seq = []
            for i, (_off, n) in enumerate(br):
                if not n.startswith(RF):
                    continue
                prev = next((br[j][1] for j in range(i - 1, -1, -1)
                             if not br[j][1].startswith(RF)), None)
                m = SCN.match(prev or "")
                seq.append(m.group(1) if m else "<UNRESOLVED>")
            seen[name].add(tuple(seq))
    out = {}
    for name, variants in seen.items():
        if len(variants) > 1:
            print("!! %s: %d duplicate COMDATs DISAGREE -- not resolving silently"
                  % (name, len(variants)), file=sys.stderr)
            continue
        out[name] = list(next(iter(variants)))
    return out
