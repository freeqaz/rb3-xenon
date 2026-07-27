#!/usr/bin/env python3
"""dupname_rebijection — make `scripts/target_symbol_map.json` injective on NAME.

The defect
----------
The map assigns the *same* MSVC mangled name to more than one target VA.  A
linked image resolves every external / COMDAT symbol to exactly **one**
definition, so `?Copy@UIComponent@@$4...` exists at exactly one VA in the retail
image.  Any additional VA carrying that name is a false identity claim.

It still *scores*, because objdiff pairs by name inside a unit and then compares
bytes with `function_reloc_diffs = None`.  Adjustor thunks / deleting dtors /
template bodies are byte-identical modulo their relocations, so the wrong name
on a byte-twin VA reads 100%.  `icf_class_bijection.py` enforces injectivity
*per unit* but not globally, and its wave 2 deliberately emitted cross-unit
duplicates ("the same mangled name legitimately sits at two VAs in two different
units" — true only for internal-linkage statics, not for the COMDAT/extern
symbols that make up ~all of the pool).

The repair
----------
Exactly the bijection argument the project already accepts, applied globally:
inside one reloc-masked byte-equivalence class **any** assignment scores 100%.
So for every surplus VA we look for a *different*, globally unused, byte-class
identical name in that VA's own unit obj.  Result: the map becomes injective,
every repaired VA keeps its 100%, and no claim survives that the image can
disprove.  Where no spare name exists the entry is dropped (that VA reverts to
anonymous `fn_<VA>` and costs one strict match) — reported separately so the
costly part can be landed independently.

Read-only by default; `--emit` writes a fragment, `--apply` rewrites the map
in place touching only the changed keys (the map is HOT — other lanes edit it).
"""
import argparse
import bisect
import collections
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts" / "harvest"))
from size_order_automap import _ordered_funcs, _asm_target_funcs  # noqa: E402

BUILD = ROOT / "build" / "45410914"
MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"
SPLITS = ROOT / "config" / "45410914" / "splits.txt"
SKIP_RX = re.compile(r"^__unwind\$|^\$|^\?\?_9")


def load_map():
    raw = json.load(open(MAP_PATH))
    m = {}
    for k, v in raw.items():
        if k.lower().startswith("0x") and isinstance(v, str):
            m[int(k, 16)] = v
    return raw, m


def load_units():
    """unit -> [(start, end)] for .text, plus an address index."""
    units = collections.defaultdict(list)
    cur = None
    for line in open(SPLITS):
        if not line.strip() or line.startswith("Sections:"):
            continue
        if not line[0].isspace():
            cur = line.strip().rstrip(":")
            continue
        p = line.split()
        if len(p) >= 3 and p[0] == ".text" and p[1].startswith("start:"):
            units[cur].append((int(p[1].split(":")[1], 16),
                               int(p[2].split(":")[1], 16)))
    ranges = sorted((s, e, u) for u, rs in units.items() for s, e in rs)
    starts = [r[0] for r in ranges]

    def unit_of(va):
        i = bisect.bisect_right(starts, va) - 1
        if i >= 0 and ranges[i][0] <= va < ranges[i][1]:
            return ranges[i][2]
        return None
    return units, unit_of


def unit_paths(unit):
    rel = unit[:-4] if unit.endswith(".cpp") else unit
    asm = BUILD / "asm" / (rel + ".s")
    bobj = BUILD / "src" / (rel + ".obj")
    if not bobj.exists():
        cands = list((BUILD / "src").rglob(Path(rel).name + ".obj"))
        bobj = cands[0] if len(cands) == 1 else bobj
    return asm, bobj


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--emit", default="/home/free/tmp/dupname/frag.json")
    ap.add_argument("--report", default="/home/free/tmp/dupname/rebijection.json")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--only-drop", action="store_true",
                    help="emit only the drops (the costly commit)")
    ap.add_argument("--only-rename", action="store_true",
                    help="emit only the score-neutral renames")
    args = ap.parse_args()

    raw, m = load_map()
    _units, unit_of = load_units()
    byname = collections.defaultdict(list)
    for va, n in m.items():
        byname[n].append(va)
    dups = {n: sorted(v) for n, v in byname.items() if len(v) > 1}
    used = set(m.values())
    stat = collections.Counter()

    # ------------------------------------------------------------------
    # Every VA of a duplicated name is a free agent: it needs SOME name that
    # (a) lives in its own unit's compiled obj, (b) is reloc-masked identical
    # to its body, (c) is not used anywhere else in the map.  The duplicated
    # name itself is one such candidate — available exactly once.  Maximising
    # the number of assigned VAs is a maximum bipartite matching, not a greedy
    # walk (a name free in two units must go to the unit that has no other
    # option).  Greedy left 303 VAs unassignable; matching is run instead.
    # ------------------------------------------------------------------
    by_unit = collections.defaultdict(list)
    homeless = []
    dupvas = []
    for n, vas in dups.items():
        for va in vas:
            u = unit_of(va)
            if not u:
                homeless.append((n, va))
                continue
            by_unit[u].append((va, n))
            dupvas.append((va, n, u))

    reserved = set(m.values()) - set(dups)      # names held by non-duplicate VAs
    cand = {}                                   # va -> [names]
    info = {}
    ownsym = {}                                 # (unit, name) -> masked body
    for unit in sorted(by_unit):
        asm, bobj = unit_paths(unit)
        if not (asm.exists() and bobj.exists()):
            stat["skip_missing_artifacts"] += len(by_unit[unit])
            continue
        try:
            tf = {va: (sz, mask) for va, sz, mask in _asm_target_funcs(asm) if va}
            bf = _ordered_funcs(bobj)
        except Exception:
            stat["skip_parse"] += len(by_unit[unit])
            continue
        bclass = collections.defaultdict(list)
        for f in bf:
            if SKIP_RX.match(f["name"]):
                continue
            bclass[f["masked"]].append(f["name"])
        bybase = {}
        for f in bf:
            bybase.setdefault(f["name"], f["masked"])
        for va, n in by_unit[unit]:
            t = tf.get(va)
            if t is None:
                stat["target_va_not_in_asm"] += 1
                continue
            names = [c for c in bclass.get(t[1], []) if c not in reserved]
            cand[va] = sorted(set(names))
            info[va] = dict(unit=unit, old=n, size=t[0], nclass=len(names))
            if bybase.get(n) == t[1]:
                ownsym[(unit, n)] = t[1]

    # ------------------------------------------------------------------
    # EXCEPTION: internal linkage.  A `static` free function is not a COMDAT
    # and is not deduped by the linker, so the same mangled name legitimately
    # sits at one VA per defining TU.  Signature: >=2 of the duplicate VAs each
    # reloc-masked match THEIR OWN unit's compiled symbol of that name, and
    # those compiled bodies DIFFER between units (a single COMDAT would be
    # identical everywhere).  `?NodeCmp@@YAHPBX0@Z` (a qsort comparator that is
    # file-static in both DataArray.cpp 0x14c and BandWardrobe.cpp 0x94) is the
    # measured instance.  These are exempted from injectivity entirely.
    # ------------------------------------------------------------------
    multidef = set()
    for n, vas in dups.items():
        sig = {}
        for va in vas:
            u = info.get(va, {}).get("unit")
            if u and n in cand.get(va, []):
                sig[u] = ownsym.get((u, n))
        bodies = {v for v in sig.values() if v is not None}
        if len(sig) >= 2 and len(bodies) >= 2:
            multidef.add(n)
            stat["internal_linkage_exempt"] += len(sig)
    exempt = {va for n in multidef for va in dups[n]
              if n in cand.get(va, [])}

    # Seed the matching with the evidence-based HOME assignment: the VA whose
    # unit file stem equals the class in the mangled name keeps its name.  Kuhn
    # still reaches a maximum matching from any seed, so seeding costs nothing
    # in size and preserves the one assignment we have positive evidence for.
    match_name = {}
    for n, vas in dups.items():
        mo = re.match(r"\?\??_?[A-Z0-9]*\??([A-Za-z_][A-Za-z0-9_]*)@([A-Za-z_][A-Za-z0-9_]*)@", n)
        cls = mo.group(2) if mo else None
        for va in vas:
            u = info.get(va, {}).get("unit")
            if u and cls and Path(u).stem == cls and n in cand.get(va, []):
                match_name[n] = va
                stat["seed_home_by_class"] += 1
                break
        else:
            for va in vas:
                if n in cand.get(va, []):
                    match_name[n] = va
                    break

    order = sorted((v for v in cand
                    if v not in set(match_name.values()) and v not in exempt),
                   key=lambda v: (len(cand[v]), v))

    def try_assign(va, seen):
        for nm in cand[va]:
            if nm in seen:
                continue
            seen.add(nm)
            if nm not in match_name or try_assign(match_name[nm], seen):
                match_name[nm] = va
                return True
        return False

    sys.setrecursionlimit(20000)
    for va in order:
        try_assign(va, set())
    assign = {v: k for k, v in match_name.items()}

    renames, drops, ev = {}, {}, []
    for va, n, unit in sorted(dupvas):
        if va in exempt:
            stat["KEEP_internal_linkage"] += 1
            ev.append(dict(va="0x%08x" % va, old=n, unit=unit,
                           verdict="KEEP_internal_linkage"))
            continue
        new_name = assign.get(va)
        if new_name is None:
            drops["0x%08x" % va] = n
            stat["DROP_no_spare"] += 1
            ev.append(dict(va="0x%08x" % va, old=n, unit=unit,
                           verdict="DROP_no_spare",
                           ncand=len(cand.get(va, []))))
        elif new_name == n:
            stat["KEEP_home"] += 1
            ev.append(dict(va="0x%08x" % va, old=n, unit=unit, verdict="KEEP"))
        else:
            renames["0x%08x" % va] = new_name
            stat["RENAME"] += 1
            ev.append(dict(va="0x%08x" % va, old=n, new=new_name, unit=unit,
                           verdict="RENAME", ncand=len(cand.get(va, []))))
    for n, va in homeless:
        drops["0x%08x" % va] = n
        stat["DROP_unpinned_va"] += 1

    for k in sorted(stat):
        print(f"{k:26s} {stat[k]}")
    print(f"\ndup names {len(dups)}  surplus VAs {sum(len(v)-1 for v in dups.values())}")
    print(f"renames {len(renames)}  drops {len(drops)}")

    Path(args.report).write_text(json.dumps(ev, indent=1))
    frag = {}
    if not args.only_drop:
        frag.update(renames)
    out = {"rename": renames, "drop": sorted(drops)}
    Path(args.emit).write_text(json.dumps(out, indent=1))

    if args.apply:
        txt = MAP_PATH.read_text()
        nrep = 0
        for va, new in renames.items():
            if args.only_drop:
                break
            old = m[int(va, 16)]
            for key in (va, va.upper().replace("0X", "0x"),
                        "0x%08X" % int(va, 16)):
                pat = f'"{key}": "{old}"'
                if pat in txt:
                    txt = txt.replace(pat, f'"{key}": "{new}"', 1)
                    nrep += 1
                    break
        ndrop = 0
        if not args.only_rename:
            for va in drops:
                old = m[int(va, 16)]
                for key in (va, "0x%08X" % int(va, 16)):
                    pat = re.compile(r'^\s*"%s": "%s",?\n' % (re.escape(key), re.escape(old)),
                                     re.M)
                    new_txt, k = pat.subn("", txt, count=1)
                    if k:
                        txt = new_txt
                        ndrop += 1
                        break
        # a removal at the tail leaves a dangling comma before the final
        # brace -- repair it without reformatting the rest of the file
        txt = re.sub(r",(\s*)\}\s*$", r"\1}", txt)
        MAP_PATH.write_text(txt)
        print(f"applied: {nrep} renames, {ndrop} drops")


if __name__ == "__main__":
    main()
