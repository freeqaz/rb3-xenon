#!/usr/bin/env python3
"""Sweep dc3's leaked linker map for HOMONYMS -- one mangled name, two functions.

Why anyone cares
----------------
`tools/fold_thunk_gate.py`'s FT3 tier rests on one observation.  `??3@YAXPAX@Z`
is not one function: dc3's leaked `ham_xbox_r.map` names it at THREE distinct
addresses, under `utl:MemMgr.obj`, `nuispeech:ctransducer.obj` and
`xaudio2:baseswfilter.obj`, because each module compiles its own file-scope
`operator delete`.  So a retail map entry naming OUR spelling at a "wrong"
address can be another module's own function that legitimately carries the same
mangled name -- not evidence against a fold at all.

That fired on exactly one pair.  The open question was whether "two modules, one
mangled name" is a CLASS with an oracle nobody had swept: dc3's map names 117,960
symbols together with their owning `.obj`, and both games are Xbox 360 MSVC-PPC
builds of the same Milo engine lineage.

This tool sweeps it.  The answer is that the class is real, structural, and tiny.

What the sweep finds
--------------------
    map rows parsed                             119,504
    distinct names                              117,960
    names in a CODE section (0005/0006/0007)     77,927
      of which PUBLIC                            53,906
      of which STATIC (internal linkage)         24,023

    names at MORE THAN ONE code address           1,393
      from the Publics table                          0     <- structural, see below
      from the Static symbols table               1,392
      both public and static                          1     (`??3@YAXPAX@Z`)

    ... after dropping compiler-generated labels
        (`__unwind$`, `__catch$`, `__ehhandler$`,
         `__tryblocktable$`), which are not functions      25

The zero is the finding.  A linker CANNOT emit two publics with the same name --
that is a duplicate-symbol error, and COMDAT selection collapses the rest onto
one address.  So a homonym can only ever arise between definitions with INTERNAL
linkage, and the class is bounded by the number of file-scope functions two TUs
happen to name identically.  In a 77,927-function game that is 25 names, and 20
of those are `??__E<var>@@YAXXZ` / `??__F<var>@@YAXXZ` dynamic initialisers for
file-scope statics that two TUs happened to call `sLicense`, `gFile`, `sRand`,
`vecNegate0` ...

Why that closes the generalisation rather than opening it
---------------------------------------------------------
Cross-checking the 25 against RB3:

    present in rb3's scripts/target_symbol_map.json at all            5
    present in the rb3 `name_check` wrong-callee charge set           1   (`??3@YAXPAX@Z`)

`??3@YAXPAX@Z` is the pair FT3 already fired on.  There is no second one, and the
reason is not sampling -- it is the linkage rule above.  The dc3 map is a fine
oracle; the population it can speak to is one pair.

Usage
-----
    python3 tools/homonym_index.py                       # census + rb3 cross-check
    python3 tools/homonym_index.py --json out.json       # the index itself
"""

import argparse
import collections
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MAP = "dc3-decomp/orig/373307D9/ham_xbox_r.map"
CODE_SECTIONS = {"0005", "0006", "0007"}          # .text, BINK, RADCODE
# Compiler-generated per-function labels. They live in the static table, they are
# not functions, and they collide constantly -- 1,367 of the 1,392 multi-address
# static names are these.
LABELS = ("__unwind$", "__catch$", "__ehhandler$", "__tryblocktable$")
ROW = re.compile(r"^\s*(\d{4}):([0-9a-fA-F]+)\s+(\S+)\s+([0-9a-fA-F]{8})\s+(.*)$")


def portable(p):
    """Contract $HOME to `~` for anything that lands in a committed artifact.

    The map lives outside this repo, so its provenance is an absolute path at
    runtime -- and writing that verbatim into the JSON bakes one machine's
    layout into git history.
    """
    try:
        return "~/" + str(Path(p).relative_to(Path.home()))
    except ValueError:
        return str(p)


def sweep(path):
    """({name: {(va, obj)}} per table, rows_parsed)."""
    lines = Path(path).read_text(errors="replace").splitlines()
    i_pub = next(n for n, l in enumerate(lines) if "Publics by Value" in l)
    i_st = next((n for n, l in enumerate(lines) if l.strip() == "Static symbols"), len(lines))

    def table(rows):
        d = collections.defaultdict(set)
        seen = 0
        for l in rows:
            m = ROW.match(l)
            if not m:
                continue
            seen += 1
            if m.group(1) not in CODE_SECTIONS:
                continue
            parts = m.group(5).split()
            d[m.group(3)].add((int(m.group(4), 16), parts[-1] if parts else "?"))
        return d, seen

    pub, n1 = table(lines[i_pub + 1:i_st])
    st, n2 = table(lines[i_st + 1:])
    return pub, st, n1 + n2


def multi(d):
    return {k: sorted(v) for k, v in d.items() if len({a for a, _ in v}) > 1}


def is_label(n):
    return n.startswith(LABELS)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", default=DEFAULT_MAP,
                    help="dc3's leaked ham_xbox_r.map (default: %(default)s, resolved "
                         "against this repo or any ancestor directory)")
    ap.add_argument("--worklist", default="docs/plans/wrong-callee-triage-2026-08-12.json")
    ap.add_argument("--json", help="write the homonym index here")
    args = ap.parse_args()

    p = Path(args.map)
    if not p.is_absolute():
        for anc in [ROOT] + list(ROOT.parents):
            if (anc / args.map).is_file():
                p = anc / args.map
                break
    if not p.is_file():
        raise SystemExit("no leaked map at %s" % args.map)

    pub, st, rows = sweep(p)
    comb = collections.defaultdict(set)
    for d in (pub, st):
        for k, v in d.items():
            comb[k] |= v
    mp, ms, mc = multi(pub), multi(st), multi(comb)
    real = {k: v for k, v in mc.items() if not is_label(k)}

    print("dc3 leaked map: %s" % p)
    print("  rows parsed                       %7d" % rows)
    print("  code-section names                %7d   (public %d, static %d)"
          % (len(comb), len(pub), len(st)))
    print("  names at >1 code address          %7d" % len(mc))
    print("    from the Publics table          %7d   <- a duplicate public is a link error"
          % len(mp))
    print("    from the Static symbols table   %7d" % len(ms))
    print("    both public and static          %7d   %s"
          % (len(set(pub) & set(st) & set(mc)), sorted(set(pub) & set(st) & set(mc))))
    print("  after dropping %s labels" % "/".join(x.rstrip("$") for x in LABELS))
    print("    HOMONYMS (real functions)       %7d" % len(real))

    kinds = collections.Counter("dynamic-init/atexit ??__E ??__F" if k.startswith(("??__E", "??__F"))
                                else "mangled C++ free function" if k.startswith("?")
                                else "C / CRT" for k in real)
    for k, v in kinds.most_common():
        print("      %-34s %5d" % (k, v))

    smap = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
    names = {n for a, n in smap.items() if a.startswith("0x") and isinstance(n, str)}
    wl = json.loads((ROOT / args.worklist).read_text())
    charged = {r["base"] for r in wl["pairs"]} | {r["target"] for r in wl["pairs"]}

    print("\nrb3 cross-check")
    print("  homonyms present in scripts/target_symbol_map.json  %5d / %d"
          % (len(set(real) & names), len(real)))
    for k in sorted(set(real) & names):
        print("      %-70s %s" % (k[:70], ["%08x %s" % (a, m) for a, m in real[k]]))
    print("  homonyms present in the wrong-callee charge set     %5d / %d"
          % (len(set(real) & charged), len(real)))
    for k in sorted(set(real) & charged):
        print("      %s" % k)

    print("\nfull homonym list")
    for k in sorted(real):
        print("  %d  %-72s %s" % (len(real[k]), k[:72], ",".join(m for _, m in real[k])))

    if args.json:
        Path(args.json).write_text(json.dumps(
            {"map": portable(p), "rows": rows,
             "counts": {"code_names": len(comb), "public": len(pub), "static": len(st),
                        "multi_address": len(mc), "multi_address_public": len(mp),
                        "multi_address_static": len(ms), "homonyms": len(real)},
             "homonyms": {k: [["0x%08x" % a, m] for a, m in v] for k, v in sorted(real.items())}},
            indent=1) + "\n")
        print("-> %s" % args.json)


if __name__ == "__main__":
    main()
