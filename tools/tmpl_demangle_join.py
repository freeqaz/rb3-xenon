#!/usr/bin/env python3
"""Join TEMPLATE-1's DEMANGLED queue columns back to MANGLED symbol names.

    python3 tools/tmpl_demangle_join.py --wt <built-repo> [--out join.json]

``docs/decomp/template-args-queue-TEMPLATE1.tsv`` records its ``target_name`` /
``our_name`` columns **demangled**, because that is what `tmplscan` had in hand.
Every downstream consumer -- ``scripts/symbol_aliases.json``,
``scripts/target_symbol_map.json``, the COFF symbol tables -- is keyed on the
**mangled** name, so the queue cannot be joined to the alias ledger at all
without inverting the demangling. Lane MAPID-1 named this the reason the queue
was left untouched ("its columns are demangled and need a demangled->mangled
join that was not built"). This is that join.

Method: demangling is a FUNCTION, not a bijection -- there is no un-demangler.
So the join is built by *forward* demangling a universe of known mangled names
with the same `llvm-undname` `tmplscan` used, and inverting the resulting map.
A demangled string therefore joins only if some name in the universe produces
it, and coverage is bounded by the universe, not by the algorithm.

⚠ The inverse is genuinely MULTI-VALUED and that is not an implementation
detail: MSVC mangles anonymous-namespace scopes with a per-TU hash
(`?A0xd29bb4b7`) that survives into the demangling, while distinct COMDATs from
different objs can share one mangled name. Both directions of ambiguity are
reported rather than silently resolved -- ALIASAUDIT-2 lost a group to exactly
this (`sort<UIListWidget*,WidgetDrawSort@?A0xd29bb4b7>` vs `@?A0x530db9db` are
the same instantiation and its rival-exclusion compared mangled names exactly).

⚠ Run against a repo whose objs have been BUILT. The dtk target objs are
pre-renamer in a fresh worktree, so the retail half of the universe is empty
until then -- a silent vacuity that makes every queue row read "unjoinable".
This script asserts the retail universe is non-trivial before reporting.
"""
import argparse
import csv
import glob
import json
import os
import struct
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MIN_RETAIL_NAMES = 20000  # measured 27,923 post-renamer; guards the vacuity above


def obj_symbols(path):
    """Mangled ('?'-prefixed) symbol names defined in one COFF obj."""
    try:
        raw = open(path, "rb").read()
    except OSError:
        return
    if len(raw) < 20:
        return
    off = struct.unpack("<I", raw[8:12])[0]
    nsym = struct.unpack("<I", raw[12:16])[0]
    if not off or not nsym:
        return
    strtab = raw[off + nsym * 18:]
    i = 0
    while i < nsym:
        e = raw[off + i * 18: off + i * 18 + 18]
        if len(e) < 18:
            break
        # ⚠ byte 17 is NumberOfAuxSymbols. Auxiliary records are 18-byte blobs of
        # arbitrary binary that are NOT symbol entries -- reading them as such
        # yields names like '?\x00\x00\x00\x01', whose embedded NULs desync
        # llvm-undname's line protocol and silently truncate the whole batch.
        naux = e[17]
        if e[:4] == b"\0\0\0\0":
            o = struct.unpack("<I", e[4:8])[0]
            end = strtab.find(b"\0", o)
            nm = strtab[o:end].decode("latin1", "replace")
        else:
            nm = e[:8].rstrip(b"\0").decode("latin1", "replace")
        i += 1 + naux
        if nm.startswith("?") and nm.isprintable() and " " not in nm:
            yield nm


def collect_universe(wt):
    """All mangled names we could possibly join to, tagged by provenance."""
    prov = defaultdict(set)
    for sub, tag in (("obj", "retail"), ("src", "ours")):
        for f in glob.glob(str(Path(wt) / "build/45410914" / sub / "**/*.obj"),
                           recursive=True):
            for nm in obj_symbols(f):
                prov[nm].add(tag)

    amap = json.load(open(ROOT / "scripts/symbol_aliases.json"))
    for g in amap["groups"]:
        if g.get("survivor"):
            prov[g["survivor"]].add("alias_survivor")
        for f in g.get("folded", []):
            prov[f].add("alias_folded")

    tmap_p = ROOT / "scripts/target_symbol_map.json"
    if tmap_p.exists():
        tmap = json.load(open(tmap_p))
        entries = tmap.get("symbols", tmap) if isinstance(tmap, dict) else tmap
        if isinstance(entries, dict):
            for k, v in entries.items():
                for cand in (k, v):
                    if isinstance(cand, str) and cand.startswith("?"):
                        prov[cand].add("map")
    return prov


def demangle_chunk(names):
    """llvm-undname emits exactly [echo, result, blank] per input line."""
    p = subprocess.run(["llvm-undname"], input="\n".join(names) + "\n",
                       capture_output=True, text=True)
    lines = p.stdout.split("\n")
    out = {}
    for k, nm in enumerate(names):
        e, r = 3 * k, 3 * k + 1
        if e < len(lines) and lines[e] == nm and r < len(lines):
            res = lines[r]
            if res and not res.startswith("error:"):
                out[nm] = res.strip()
    return out


def demangle_all(names, chunk=20000):
    """{mangled: demangled}, chunked, with an ANTI-VACUITY guard.

    ⚠ The unchunked re-syncing parser this replaces returned 23 of 138,913 on
    its first real run and reported it as a clean, unanimous negative. A silent
    truncation here is indistinguishable from "nothing joins", so a implausibly
    low yield REFUSES instead of returning.
    """
    names = [n for n in names if n.isprintable() and " " not in n]
    out = {}
    for i in range(0, len(names), chunk):
        out.update(demangle_chunk(names[i:i + chunk]))
    if names and len(out) < 0.5 * len(names):
        sys.exit("REFUSING: demangled only %d of %d names (<50%%). This is the "
                 "silent-truncation failure mode, not a real negative."
                 % (len(out), len(names)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wt", required=True, help="BUILT repo/worktree")
    ap.add_argument("--queue",
                    default=str(ROOT / "docs/decomp/template-args-queue-TEMPLATE1.tsv"))
    ap.add_argument("--out")
    a = ap.parse_args()

    prov = collect_universe(a.wt)
    retail = {n for n, t in prov.items() if "retail" in t}
    if len(retail) < MIN_RETAIL_NAMES:
        sys.exit("REFUSING: only %d retail mangled names (<%d). The worktree's "
                 "target objs are PRE-RENAMER -- build it first, or every queue "
                 "row will read 'unjoinable' for instrument reasons."
                 % (len(retail), MIN_RETAIL_NAMES))

    dem = demangle_all(sorted(prov))
    inv = defaultdict(set)
    for m, d in dem.items():
        if d:
            inv[d].add(m)

    rows = list(csv.DictReader(open(a.queue), delimiter="\t"))
    pairs = {}
    for r in rows:
        pairs.setdefault((r["target_name"], r["our_name"]), []).append(r)

    stats = defaultdict(int)
    joined = []
    for (tn, on), rs in pairs.items():
        tm, om = sorted(inv.get(tn, ())), sorted(inv.get(on, ()))
        cls = rs[0]["class"]
        key = ("both" if tm and om else
               "target_only" if tm else "ours_only" if om else "neither")
        stats[key] += 1
        stats[key + ":" + cls] += 1
        if tm and om:
            stats["both_bytes"] += sum(int(r["size"]) for r in rs)
        if len(tm) > 1 or len(om) > 1:
            stats["ambiguous"] += 1
        joined.append({
            "class": cls, "target_dem": tn, "our_dem": on,
            "target_mangled": tm, "our_mangled": om,
            "rows": [{"unit": r["unit"], "symbol": r["symbol"],
                      "size": int(r["size"]), "fuzzy": r["fuzzy"],
                      "fold_test": r["fold_test"], "mapname": r["mapname"]}
                     for r in rs],
            "target_prov": sorted({p for m in tm for p in prov[m]}),
            "our_prov": sorted({p for m in om for p in prov[m]}),
        })

    print("universe: %d mangled names (%d retail, %d demangled ok)"
          % (len(prov), len(retail), len(dem)))
    print("queue: %d rows / %d distinct pairs" % (len(rows), len(pairs)))
    for k in ("both", "target_only", "ours_only", "neither", "ambiguous"):
        print("  %-12s %4d" % (k, stats[k]))
    print("  both-side joinable bytes: %d" % stats["both_bytes"])
    print("\nby class (both-side joinable / total):")
    classes = sorted({r["class"] for r in rows})
    for c in classes:
        tot = sum(1 for p, rs in pairs.items() if rs[0]["class"] == c)
        print("  %-26s %3d / %3d" % (c, stats["both:" + c], tot))

    if a.out:
        json.dump({"stats": dict(stats), "pairs": joined}, open(a.out, "w"), indent=1)
        print("\nwrote %s" % a.out)


if __name__ == "__main__":
    main()
