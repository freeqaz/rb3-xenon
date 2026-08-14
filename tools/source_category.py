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

  category (here, 5 values)      scope tier (`tools/scope_map.py`, 7 values)
  ---------------------------    ------------------------------------------
  game       src/band3/          game        band3 + network (one priority tier)
  network    src/network/        engine      src/system/ Milo
  engine     src/system/ + root  thirdparty  vendored libs (some under src/system/)
  thirdparty vendored upstream   crt, xdk, vendor, unknown
  sdk        src/xdk/

`CATEGORY_ALLOWED_TIERS` below records the mapping, and `--audit` asserts every
declared object satisfies it. A disagreement OUTSIDE that table is real drift
and fails the audit; a disagreement INSIDE it is granularity and is expected.

✅ CLOSED 2026-08-14 (lane VENDTIER-1) -- the gap CATTAG-1 sized and escalated.
`thirdparty` is now a fifth category: vendored upstream under `src/system/` no
longer counts as "Milo Engine Code". ⚠⚠ ANY `engine` TIER % QUOTED ACROSS THIS
CHANGE MUST SAY WHICH SIDE IT IS ON -- the engine denominator moved by 105,740 B
and the tier's own percentages moved with it.

The claim was ADJUDICATED PER FILE against actual upstream, not accepted from the
directory name, because the directory name is precisely what went wrong last
time (ZLibCompression.cpp, above). Of 34 declared objects that `bucket_for_source`
called `thirdparty`:

  30 GENUINELY VENDORED -- 28 still carry their upstream copyright banner
     (Xiph, Mark Adler/Jean-loup Gailly, Metaparadigm, Tom St Denis, Daniel
     Stenberg); tomcrypt crypt.c + ctr.c lost the banner in reconstruction but
     are unmistakable LibTomCrypt 1.x by API (`mycrypt.h`,
     `_cipher_descriptor[32]`, `ctr_start`/`symmetric_CTR`/`CRYPT_OK`).
     None includes a Milo header. -> `thirdparty`.

  4  FALSE MEMBERS, Harmonix code that merely LIVES in a vendored directory ->
     stay `engine`: oggvorbis/VorbisMem.cpp (OggMalloc/OggFree over
     utl/MemMgr.h), jpeg/Jpeg.cpp (LoadBitmapIntoJpeg, MILO_ASSERT),
     zlib/ZlibLicense.cpp and synth/tomcrypt/TomCryptLicense.cpp (utl/Licenses.h
     registration objects). The rule that catches them lives in
     `scope_map.bucket_for_source`: these six vendored libs are pure C upstream,
     so a C++ TU inside one is Harmonix glue by construction. Tree-wide there are
     exactly five .cpp files under a vendor-marker directory and the fifth is
     ZLibCompression.cpp, already handled -- so the rule is 4/4 with no false
     positives. `/stlport/` is excluded from it because STLport is C++.

⚠ CATTAG-1's own breakdown of this set ("oggvorbis 18, zlib 4, net/json-c 5,
synth/tomcrypt 3, net/curl 2, jpeg 1") sums to 33 against its own headline of 31
-- it interleaved DECLARED and PINNED counts. Measured: 34 declared, of which 31
appear in report.json (Jpeg.cpp, ZlibLicense.cpp and TomCryptLicense.cpp are
declared but unpinned, 0 B). Pinned, after the false-member fix: 30 thirdparty /
105,740 B, and VorbisMem.cpp's 12 B stays engine. The headline 31 / 105,752 B was
right; the per-library row was not.
"""

import argparse
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# scope_map owns the single copy of the vendored-library markers (and the
# Harmonix-glue exception to them); `category_for_source` delegates to it rather
# than keeping a second list that could drift. Imported at module scope, and
# deliberately NOT wrapped in try/except: configure.py calls this for all 1,434
# declared objects, and a silent fallback would mis-tag 31 units exactly as
# quietly as the library-group tags this module replaced. Fail loudly instead.
# (Import is ~18 ms and pulls in no build state.)
sys.path.insert(0, os.path.join(ROOT, "tools"))
from scope_map import bucket_for_source as _bucket_for_source  # noqa: E402

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
    # ★ TIGHTENED by lane VENDTIER-1: this used to allow {"engine",
    # "thirdparty"}, which is what let 31 units / 105,752 B of vendored source
    # sit in the engine tier without the audit saying a word. Now that the
    # category derives the split, an engine-category file reading `thirdparty`
    # is real drift again -- and the audit can fail on it, which is the whole
    # point of the table. Do NOT re-widen this to silence a failure.
    "engine": {"engine"},
    # Vendored upstream (zlib, libvorbis/libogg, libtomcrypt, json-c, curl,
    # libjpeg). One tier, one category -- the public tarball is the oracle.
    "thirdparty": {"thirdparty"},
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
    # Vendored third-party libraries physically live under src/system/ but their
    # oracle is the PUBLIC UPSTREAM TARBALL, not DC3 -- so the work is mechanical
    # (diff against real zlib/libvorbis/libtomcrypt/json-c/curl) rather than
    # DC3 label-transfer. Since the tier means "which oracle exists", that is a
    # different tier. Tested BEFORE the engine rule because these paths start
    # with src/system/ too. (lane VENDTIER-1)
    #
    # ⚠ Delegated to scope_map rather than re-listing the markers here: a second
    # copy of the list is exactly the hand-maintained drift this module exists to
    # abolish. bucket_for_source is also where the "C++ TU inside a C library is
    # Harmonix glue" exception lives, so VorbisMem.cpp / Jpeg.cpp / *License.cpp
    # correctly stay `engine` through this call.
    if low.startswith("src/system/"):
        if _bucket_for_source(low) == "thirdparty":
            return "thirdparty"
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
        # vendored upstream under src/system -> thirdparty (lane VENDTIER-1).
        # Public-tarball oracle, mechanical work, NOT DC3 label-transfer.
        ("src/system/zlib/inflate.c", "thirdparty"),
        ("src/system/oggvorbis/floor1.c", "thirdparty"),
        ("src/system/synth/tomcrypt/aes.c", "thirdparty"),
        ("src/system/net/json-c/json_tokener.c", "thirdparty"),
        ("src/system/net/curl/lib/sslgen.c", "thirdparty"),
        # ⛔ NEGATIVES -- the four C++ TUs filed INSIDE those C libraries are
        # Harmonix glue, not upstream, and must stay `engine`. Adjudicated
        # per-file: OggMalloc/OggFree over utl/MemMgr.h; LoadBitmapIntoJpeg with
        # MILO_ASSERT; two utl/Licenses.h registration objects. Without these
        # five cases the rule below them is indistinguishable from "everything
        # under a vendor dir is thirdparty", which is the bug it fixes.
        ("src/system/oggvorbis/VorbisMem.cpp", "engine"),
        ("src/system/jpeg/Jpeg.cpp", "engine"),
        ("src/system/zlib/ZlibLicense.cpp", "engine"),
        ("src/system/synth/tomcrypt/TomCryptLicense.cpp", "engine"),
        # must still be `engine` -- ordinary Milo with no vendor dir in sight
        ("src/system/utl/Str.cpp", "engine"),
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
