#!/usr/bin/env python3
"""source path -> `progress_category` (the dtk/objdiff progress tier tag).

WHY THIS EXISTS (lane CATTAG-1, 2026-08-14)
-------------------------------------------
Before this module the tag was **library-group membership** in
`config/45410914/objects.json`: nine hand-maintained groups, each carrying one
`progress_category`, every object inheriting its group's value. Nothing tied a
file's tag to where the file actually lives, so the tag could disagree with the
path indefinitely and silently -- and it did, on ten declared objects:

    src/band3/game/HitTracker.cpp          tagged 'engine'  (game code)
    src/band3/meta_band/Asset.cpp          tagged 'engine'
    src/band3/meta_band/LockStepMgr.cpp    tagged 'engine'
    src/band3/meta_band/UGCPurchasePanel.cpp tagged 'engine'   = 13,692 B
    src/Main.cpp / Memory_Xbox.cpp / keygen_xbox.cpp  tagged 'game'  = 6,204 B
    src/xdk/nuiapi/nuidetroit.cpp          tagged 'engine'  (whole `xdk`
    src/xdk/LIBCMT/osfinfo.cpp             tagged 'engine'   group was tagged
    src/xdk/LIBCMT/rtti.cpp                tagged 'engine'   'engine', not 'sdk')

That mattered beyond cosmetics: `tools/fingerprint_pipeline.py` selects the
game worklist on the tag ALONE, so those four band3 units were absent from it
and the three root-level files were in it.

The tag is now a **pure function of the source path**, computed in
`configure.py` for every declared object. Drift is impossible by construction:
to change a file's tier you move the file. The `progress_category` still
present on library groups in objects.json is a fallback only (it is used iff
this function returns None, which it currently does for zero declared objects).

⚠ THIS IS NOT `scope_map.bucket_for_source`, AND THEY ARE NOT SUPPOSED TO BE
EQUAL. Two different granularities, deliberately:

  category (here, 4 values)      scope tier (`tools/scope_map.py`, 7 values)
  ---------------------------    ------------------------------------------
  game     src/band3/            game        band3 + network (one priority tier)
  network  src/network/          engine      src/system/ Milo
  engine   src/system/ + root    thirdparty  vendored libs (some under src/system/)
  sdk      src/xdk/              crt, xdk, vendor, unknown

`CATEGORY_ALLOWED_TIERS` below records the mapping, and `--audit` asserts every
declared object satisfies it. A disagreement OUTSIDE that table is real drift
and fails the audit; a disagreement INSIDE it is granularity and is expected.

⛔ Known granularity gap, SIZED AND DELIBERATELY NOT CLOSED by CATTAG-1:
31 pinned units / 105,752 B of vendored third-party source live under
`src/system/` (oggvorbis 18, zlib 4, net/json-c 5, synth/tomcrypt 3, net/curl 2,
jpeg 1) and are therefore tagged `engine` -- i.e. counted as "Milo Engine Code".
`scope_map` already separates them as `thirdparty` (public-source oracle,
mechanical) and that separation is the truer one. Closing it needs a
`"thirdparty"` entry in config.json's `progress_categories` plus the one commented
line in `category_for_source` below; it is a one-line change. CATTAG-1 did not
make it unilaterally because it moves the widely-quoted `engine` tier denominator
by ~105 kB, which is a coordinator-level call, not a side effect of a game-tier
correction.
"""

import argparse
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# category id -> scope_map buckets a file in that category may legitimately be.
# A pair outside this table is DRIFT, not granularity. Keep it minimal: every
# entry here is a documented granularity difference, never an excuse.
CATEGORY_ALLOWED_TIERS = {
    # band3 game code. One tier, one category.
    "game": {"game"},
    # scope_map folds src/network/ into the `game` PRIORITY tier (rb3-Wii
    # oracle, RB3-specific); the category keeps it separate because Quazal
    # NetZ behaves nothing like band3 (269,640 B at mean 16.96%).
    "network": {"game"},
    # vendored libs under src/system/ read as `thirdparty` -- see the sized gap
    # in the module docstring.
    "engine": {"engine", "thirdparty"},
    # src/xdk/LIBCMT is Microsoft's CRT shipped with the XDK; scope_map splits
    # it out as `crt` because its oracle differs, but at category granularity
    # "XDK Code" describes it exactly.
    "sdk": {"xdk", "crt"},
}


def category_for_source(src_path):
    """src_path (repo-relative, e.g. 'src/band3/game/HitTracker.cpp') -> category
    id, or None if the path is outside the declared source tree.

    Total over every path currently declared in objects.json (1,434/1,434)."""
    if not src_path:
        return None
    low = str(src_path).replace("\\", "/").lower()
    if not low.startswith("src/"):
        return None
    # XDK / CRT vendor code we compile ourselves. First, because src/xdk/ would
    # otherwise fall through to the root-glue rule for nothing.
    if low.startswith("src/xdk/"):
        return "sdk"
    # RB3 game layer -- the highest-priority tier, rb3-Wii oracle.
    if low.startswith("src/band3/"):
        return "game"
    # Quazal NetZ middleware.
    if low.startswith("src/network/"):
        return "network"
    # Milo engine, DC3 oracle.
    if low.startswith("src/system/"):
        # To split vendored libs out of the engine tier, add "thirdparty" to
        # config.json's progress_categories and uncomment:
        #   from scope_map import bucket_for_source
        #   if bucket_for_source(low) == "thirdparty":
        #       return "thirdparty"
        return "engine"
    # Root-level platform glue: src/Main.cpp, src/Memory_Xbox.cpp,
    # src/keygen_xbox.cpp. ADJUDICATED as engine, not game (lane CATTAG-1):
    # all three exist in dc3-decomp at the identical root path and ours are
    # ports of DC3's (Main.cpp differs only by a comment block, Memory_Xbox.cpp
    # only by include-slash direction, keygen_xbox.cpp only by a local-name
    # binding). rb3-Wii has keygen_WII.cpp, not the Xbox variant -- so the
    # oracle for these is DC3, and "which oracle" is what the tier means.
    if low.count("/") == 1:
        return "engine"
    return None


# ---------------------------------------------------------------------------
# audit: every declared object's category must agree with its scope tier,
# modulo CATEGORY_ALLOWED_TIERS.
# ---------------------------------------------------------------------------
def _declared_objects():
    """Replicate tools/project.py's own resolution: src_path is
    src_dir / (options['source'] or <object key>), with src_dir defaulting
    object -> library -> config.src_dir ('src'). NEVER key on basename()."""
    path = os.path.join(ROOT, "config", "45410914", "objects.json")
    objs = json.load(open(path, encoding="utf-8"))
    out = []
    for lib, cfg in objs.items():
        lib_cat = cfg.get("progress_category")
        lib_src_dir = cfg.get("src_dir")
        for name, oc in (cfg.get("objects") or {}).items():
            oc = {} if isinstance(oc, str) else oc
            src_dir = oc.get("src_dir") or lib_src_dir or "src"
            source = oc.get("source") or name
            out.append({
                "lib": lib,
                "name": name,
                "src_path": f"{src_dir}/{source}".replace("//", "/"),
                "declared_category": oc.get("progress_category") or lib_cat,
            })
    return out


def cmd_audit(args):
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    from scope_map import bucket_for_source

    rows = _declared_objects()
    violations, retagged = [], []
    for r in rows:
        cat = category_for_source(r["src_path"])
        if cat is None:
            violations.append((r, None, "UNCLASSIFIABLE path"))
            continue
        if cat != r["declared_category"]:
            retagged.append((r, cat))
        tier = bucket_for_source(r["src_path"])
        allowed = CATEGORY_ALLOWED_TIERS.get(cat, set())
        if tier not in allowed:
            violations.append((r, cat,
                               f"tier {tier!r} not in allowed {sorted(allowed)}"))

    print(f"declared objects: {len(rows)}")
    print(f"derived category differs from objects.json group tag: {len(retagged)}")
    for r, cat in sorted(retagged, key=lambda x: x[0]["src_path"]):
        print(f"    {r['src_path']:55s} {r['declared_category']} -> {cat}")
    print(f"category<->scope_map tier violations: {len(violations)}")
    for r, cat, why in violations:
        print(f"    {r['src_path']:55s} cat={cat}: {why}")
    return 1 if violations else 0


def cmd_selftest(args):
    """Anti-vacuity: the classifier must be able to return the OTHER answer.
    Every case below has a known answer, and the negatives are the point."""
    cases = [
        ("src/band3/game/HitTracker.cpp", "game"),
        ("src/band3/meta_band/UGCPurchasePanel.cpp", "game"),
        # must NOT be game -- engine code sits next to nothing game-shaped
        ("src/system/beatmatch/MasterAudio.cpp", "engine"),
        # must NOT be thirdparty/game -- Quazal's own ZLib binding, 7-line
        # scaffold, oracle is ../rb3/src/network/Plugins/ZLibCompression.cpp
        ("src/network/quazal/Compression/ZLib/ZLibCompression.cpp", "network"),
        ("src/network/net/NetSession.cpp", "network"),
        # must NOT be engine -- whole `xdk` group was mis-tagged engine
        ("src/xdk/LIBCMT/rtti.cpp", "sdk"),
        ("src/xdk/nuiapi/nuidetroit.cpp", "sdk"),
        # must NOT be game -- DC3-oracle platform glue
        ("src/Main.cpp", "engine"),
        ("src/Memory_Xbox.cpp", "engine"),
        ("src/keygen_xbox.cpp", "engine"),
        # documented granularity: vendored lib under src/system stays engine
        ("src/system/zlib/inflate.c", "engine"),
        # outside the source tree
        ("native/src/dta_link_stubs.s", None),
        ("", None),
        (None, None),
    ]
    bad = 0
    for sp, want in cases:
        got = category_for_source(sp)
        ok = got == want
        bad += not ok
        print(f"  [{'ok ' if ok else 'FAIL'}] {str(sp):58s} -> {got!r} (want {want!r})")
    print(f"{len(cases) - bad}/{len(cases)} passed")
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd")
    sub.add_parser("audit", help="check every declared object (exit 1 on drift)")
    sub.add_parser("selftest", help="classifier known-answer cases")
    args = ap.parse_args()
    if args.cmd == "selftest":
        return cmd_selftest(args)
    return cmd_audit(args)


if __name__ == "__main__":
    sys.exit(main())
