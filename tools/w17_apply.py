#!/usr/bin/env python3
"""Apply lane W17-FAMILYSWEEP's four adjudicated _Rb_tree family repairs.

Every replacement name is LIFTED VERBATIM from the destination unit's own obj
symbol table, matched to its retail address BY BODY LENGTH -- so hard limit #1
("the pinned unit's base obj must be able to define the name, or the row goes
permanently 0%") is satisfied by construction rather than by hope, and a typo
cannot silently produce a dead row.

Refuses unless:
  * every source address currently carries the expected (wrong) name;
  * every replacement name is collision-free as a DELTA against the pre-edit
    duplicate set (the map has 3 pre-existing duplicates; asserting purity
    would fire on those -- W15);
  * every splits block being re-homed exists exactly as expected;
  * no donor unit is drained of its last .text block (an empty unit still emits
    a 42-byte obj and report.json then hard-fails on it).

Only `.text` lines are touched: `.pdata` is DERIVED and re-derived on every
split run, so hand-editing it is at best inert and at worst breaks the split.
"""
import json
import os
import re
import sys

ROOT = os.environ.get("RB3_ROOT", ".")
MAP = os.path.join(ROOT, "scripts/target_symbol_map.json")
SPLITS = os.path.join(ROOT, "config/45410914/splits.txt")
OBJDIR = os.path.join(ROOT, "build/45410914/src")

sys.path.insert(0, os.path.join(ROOT, "tools"))
from coff_bodies_ext import function_bodies_ext  # noqa: E402


def obj_names(relpath):
    return {n: len(b) for n, b, _r, _e in
            function_bodies_ext(os.path.join(OBJDIR, relpath))}


def pick(names, length, *frags):
    hits = [n for n, ln in names.items()
            if ln == length and all(f in n for f in frags)]
    if len(hits) != 1:
        raise SystemExit(f"REFUSE: {len(hits)} symbols of length {length} "
                         f"matching {frags} (need exactly 1)")
    return hits[0]


def main():
    font = obj_names("system/rndobj/Font.obj")
    text = obj_names("system/rndobj/Text.obj")
    rock = obj_names("band3/net_band/RockCentral.obj")
    ssm = obj_names("band3/meta_band/SongSortMgr.obj")

    F = ("UCharInfo@RndFont@@", "?$_Rb_tree@G")
    T = ("VMeshInfo@RndText@@", "?$_Rb_tree@I")

    # (addr, expected-current-substring, new name, destination unit or None)
    renames = [
        # -- D: RndFont::mCharMap, map<unsigned short, RndFont::CharInfo> BY VALUE
        (0x82472df0, None,
         pick(font, 112, "?_M_create_node@", *F), "Font.cpp"),
        (0x824730e8, "PAUCharInfo@RndFont3d@@",
         pick(font, 204, "?_M_insert@", *F), "Font.cpp"),
        (0x824731c0, "PAVCharClip@@",
         pick(font, 200, "?_M_copy@", *F), "Font.cpp"),
        (0x824732b8, "PAUCharInfo@RndFont3d@@",
         pick(font, 224, "?insert_unique@", *F), "Font.cpp"),
        (0x82473630, "PAUCharInfo@RndFont3d@@",
         pick(font, 472, "?insert_unique@", *F), "Font.cpp"),
        (0x824742e8, "PAUCharInfo@RndFont3d@@",
         pick(font, 208, "??A?$map@G", "UCharInfo@RndFont@@"), "Font.cpp"),
        # -- C: RndText::mMeshMap, map<unsigned int, RndText::MeshInfo>
        (0x82456190, "PAVFaderGroup@@",
         pick(text, 204, "?_M_insert@", *T), "Text.cpp"),
        (0x824563d8, "PAVFaderGroup@@",
         pick(text, 224, "?insert_unique@", *T), "Text.cpp"),
        (0x824566e8, "PAVAward@@",
         pick(text, 472, "?insert_unique@", *T), "Text.cpp"),
        # -- B: map<String,DataNode>::_M_copy
        (0x824f9288, "_Identity@VSymbol@@",
         pick(rock, 200, "?_M_copy@", "?$_Rb_tree@VString@@", "VDataNode@@"),
         "RockCentral.cpp"),
        # -- A: map<Symbol,SetlistRecord>::clear  (in-place rename, no re-home)
        (0x82597098, "U?$pair@$$CBVSymbol@@H@",
         pick(ssm, 80, "?clear@", "?$_Rb_tree@VSymbol@@", "VSetlistRecord@@"),
         None),
    ]

    # splits: (lo, hi, from-heading-suffix, to-heading-suffix, carve_lo_or_None)
    moves = [
        (0x82472df0, 0x82472e60, "Font3d.cpp", "Font.cpp", None),
        (0x824730e8, 0x824731c0, "Font3d.cpp", "Font.cpp", None),
        (0x824731c0, 0x824732b8, "HamCharacter.cpp", "Font.cpp", None),
        (0x824732b8, 0x82473398, "Font3d.cpp", "Font.cpp", None),
        (0x82473630, 0x82473808, "Font3d.cpp", "Font.cpp", None),
        (0x824742e8, 0x824743b8, "Font3d.cpp", "Font.cpp", None),
        (0x82456190, 0x8245625c, "Faders.cpp", "Text.cpp", None),
        (0x824563d8, 0x824564b8, "Faders.cpp", "Text.cpp", None),
        # SkeletonClip's block is 0x824564b8-0x824568c0; carve the 472 B tail.
        (0x824564b8, 0x824568c0, "SkeletonClip.cpp", "Text.cpp", 0x824566e8),
        (0x824f9288, 0x824f9358, "AccomplishmentDiscSongConditional.cpp",
         "RockCentral.cpp", None),
    ]

    # ---- map ----
    raw = json.load(open(MAP))
    byname = {}
    for k, v in raw.items():
        if k.startswith("0x"):
            byname.setdefault(v, []).append(k)
    predup = {n for n, ks in byname.items() if len(ks) > 1}

    def key_of(ad):
        for cand in (f"0x{ad:08x}", f"0x{ad:08X}", hex(ad)):
            if cand in raw:
                return cand
        return None

    for ad, expect, new, _dst in renames:
        k = key_of(ad)
        cur = raw.get(k) if k else None
        if expect is None:
            if cur is not None:
                raise SystemExit(f"REFUSE 0x{ad:08x}: expected UNMAPPED, got {cur[:60]}")
        else:
            if not cur or expect not in cur:
                raise SystemExit(f"REFUSE 0x{ad:08x}: expected {expect!r} in "
                                 f"current name, got {(cur or '<none>')[:80]}")
        if new in byname and byname[new] != [k]:
            raise SystemExit(f"REFUSE 0x{ad:08x}: replacement already mapped at "
                             f"{byname[new]}")
        raw[k or f"0x{ad:08x}"] = new

    post = {}
    for k, v in raw.items():
        if k.startswith("0x"):
            post.setdefault(v, []).append(k)
    newdup = {n for n, ks in post.items() if len(ks) > 1} - predup
    if newdup:
        raise SystemExit(f"REFUSE: {len(newdup)} NEW duplicate names introduced")

    # ---- splits (.text only) ----
    lines = open(SPLITS).read().split("\n")
    heads = {}
    cur = None
    for i, ln in enumerate(lines):
        if ln and not ln[0].isspace() and ln.rstrip().endswith(":"):
            cur = ln.strip()[:-1]
            heads.setdefault(cur, []).append(i)
    rx = re.compile(r"^(\s*)\.text(\s+)start:0x([0-9A-Fa-f]+)(\s+)end:0x([0-9A-Fa-f]+)")

    # index every .text line with its owning heading
    owned = []
    cur = None
    for i, ln in enumerate(lines):
        if ln and not ln[0].isspace() and ln.rstrip().endswith(":"):
            cur = ln.strip()[:-1]
            continue
        m = rx.match(ln)
        if m and cur:
            owned.append((i, cur, int(m.group(3), 16), int(m.group(5), 16)))

    def heading_for(suffix):
        cands = [h for h in heads if h == suffix or h.endswith("/" + suffix)]
        if len(cands) != 1:
            raise SystemExit(f"REFUSE: heading {suffix!r} -> {cands} (need 1). "
                             f"BARE vs NESTED trap: key on FULL PATH.")
        return cands[0]

    add = {}
    drop = set()
    for lo, hi, src, dst, carve in moves:
        hit = [(i, h) for i, h, a, b in owned if a == lo and b == hi
               and (h == src or h.endswith("/" + src))]
        if len(hit) != 1:
            raise SystemExit(f"REFUSE: block {lo:#x}-{hi:#x} in {src}: {len(hit)} matches")
        i, h = hit[0]
        drop.add(i)
        dsth = heading_for(dst)
        if carve is None:
            add.setdefault(dsth, []).append((lo, hi))
        else:
            add.setdefault(h, []).append((lo, carve))     # remainder stays
            add.setdefault(dsth, []).append((carve, hi))

    # donor-drain guard
    remaining = {}
    for i, h, a, b in owned:
        if i not in drop:
            remaining.setdefault(h, []).append((a, b))
    for h, extra in add.items():
        remaining.setdefault(h, []).extend(extra)
    for _lo, _hi, src, _dst, _c in moves:
        h = heading_for(src)
        if not remaining.get(h):
            raise SystemExit(f"REFUSE: {h} would be drained of its last .text "
                             f"block; an empty unit hard-fails report.json")

    out = []
    for i, ln in enumerate(lines):
        if i in drop:
            continue
        out.append(ln)
        if ln and not ln[0].isspace() and ln.rstrip().endswith(":"):
            h = ln.strip()[:-1]
            for lo, hi in sorted(add.pop(h, [])):
                out.append(f"\t.text       start:0x{lo:08X} end:0x{hi:08X}")
    if add:
        raise SystemExit(f"REFUSE: headings never emitted: {list(add)}")

    json.dump(raw, open(MAP, "w"), indent=1)
    open(SPLITS, "w").write("\n".join(out))
    print(f"applied {len(renames)} renames, {len(moves)} splits moves")


if __name__ == "__main__":
    main()
