#!/usr/bin/env python3
"""scope_map.py -- classify every function in the RB3-360 XEX into a decomp scope
bucket and compute a *meaningful* progress denominator.

The GOAL is 100% of the WHOLE 11.53 MB binary. EVERYTHING in it is matchable:
DC3 and rb3-Wii both started from ZERO source and matched functions by
reconstructing C++ from the asm. A "source twin" (DC3 engine / rb3-Wii game /
public 3rd-party) is an ACCELERANT, not a prerequisite -- its absence makes a
function harder and lower-priority, never impossible. So the buckets below are
PRIORITY + difficulty tiers, NOT in/out of scope. This tool partitions every
function into a tier so we can (a) report honest progress toward 100% and
(b) sequence effort by ROI (oracle-backed first).

Buckets (tier = priority + whether a source-oracle exists)
-------
  game        src/band3/, src/network/         oracle: rb3-Wii  · HIGHEST priority
  engine      src/system/ (Milo engine)        oracle: DC3 (byte-faithful, cheapest)
  thirdparty  zlib/ogg/vorbis/tomcrypt/curl/   oracle: public source · mechanical
              json-c/expat/speex/STLport
  crt         src/xdk/LIBCMT/ (CRT we compile)  oracle: we compile it
  xdk         XEX-imported XDK glue + BINK       NO oracle · lower prio (reconstruct)
  vendor      statically-linked MS              NO oracle · lower prio (reconstruct)
              D3DX/D3D9/XGRAPHICS/XAUDIO/XMA,    -- still matchable + worth SPLITTING
              RAD BINK, Quazal/Rendez-vous net
  unknown     residual (mapping TODO)           not yet attributed to a TU

Classification is layered, highest-confidence first:
  1. xdk        -- exact addr in xdk.json (import thunks + BINK)            [conf=1.0]
  2. pinned src -- unit has a source_path -> bucket by path                 [conf=1.0]
  3. thirdparty -- addr inside a thirdparty.json library range              [conf=0.9]
  4. engine/game-- addr in engine.json / game.json provenance labels        [conf=label]
  5. uid merge  -- addr in uid_merge.json (DC3+rb3-Wii BinDiff cross-ID)    [conf<=0.95]
  6. name class -- mangled-symbol CLASS -> bucket (catch-all named fns)     [conf=0.9]
  7. spatial    -- propagate the bucket of confident neighbors across an    [conf<=0.5]
                   unlabeled .text run (no LTCG => TUs stay contiguous)
  8. unknown    -- nothing said anything                                    [conf=0.0]

Outputs config/45410914/scope_map.json: {addr_hex: {size, scope, provenance,
confidence, matched}}.

TERMINOLOGY -- "mapped" means two different things in this repo, do not conflate:
  * the main build/dtk progress box's "mapped" = PINNED coverage: bytes that live
    inside a unit with a `splits.txt` .text range (the prerequisite to matching).
  * this tool's coverage footer = TIER-CLASSIFIED coverage: bytes this map could
    attribute to a scope tier (game/engine/thirdparty/crt/xdk/vendor) by ANY of
    the 8 layers above -- pinned or not. It is therefore always >= the pinned
    number. The dashboard labels it "tier-classified" for exactly this reason.

★★★ AND "the game fuzzy number" MEANT AT LEAST SIX THINGS ON ONE TREE (lane
SCOPEDEN-1, 2026-08-14, measured at b574f653). Two independent axes multiply:

  WHICH FIELD          aon%   Sigma size of rows at fuzzy==100 / denominator. What
                              `matched_code` keys on; a 99.9% row contributes 0.
                       mean%  size-weighted mean of fuzzy_match_percent. Always
                              >= aon%. Both are legitimate; they are NOT two
                              views of one measure.
  WHICH DENOMINATOR    pinned units we compile (base object exists => rows CAN
                              pair). "How is our SOURCE doing."
                       all-in pinned + mass the map ATTRIBUTES to the tier that
                              has no base object and cannot pair today.

  ... times the fact that "game" itself has more than one extension:

    band3 only, pinned          2,107,200 B   aon 59.85%   mean 82.07%  <- ⚠ SEE
                                                                            BELOW
    band3+network, pinned       2,376,840 B   aon 54.36%   mean 74.68%   <- the
                                              remembered "we were at 75%"
    scope tier, pinned          2,387,580 B   aon 54.60%   mean 74.82%   <- printed
    scope tier, all-in          3,039,424 B   aon 42.89%   mean 58.78%   <- printed

`src/network/` alone is 269,640 B at mean 16.96%, so INCLUDING it costs 7.4 pp
before any inferred mass is folded in at all. Nothing regressed to produce any of
these; they are four denominators and two fields.

✅ FIXED 2026-08-14 (lane CATTAG-1). The paragraph here used to say the dtk
`progress_categories` join and `bucket_for_source` disagree on 8 units and that
"the category tags are the side that is wrong". A census over ALL 1,434 declared
objects -- not the 8 that fell out of one reconciliation -- corrects that three
ways:

  * It is 10, not 8. The extra class is the WHOLE `xdk` library group (nuidetroit,
    osfinfo, rtti) tagged `engine` while xdk_ucode/xdk_vendor already used `sdk`.
    A game-tier-only reconciliation cannot see it.
  * ⛔ ONE OF THE 8 WAS NOT A MIS-TAG -- THIS TOOL WAS WRONG.
    `src/network/quazal/Compression/ZLib/ZLibCompression.cpp` (316 B, of the
    6,520 B "swept in") read `thirdparty` only because the "/zlib/" marker matched
    the DIRECTORY NAME `ZLib/`. It is a 7-line `namespace Quazal {}` map scaffold
    whose oracle is ../rb3/src/network/Plugins/ZLibCompression.cpp -- Quazal NetZ
    middleware, not zlib. Its `network` tag was right. `bucket_for_source` now
    tests the in-scope game layer ABOVE the marker substring.
  * ⚠ The `band3 only, pinned` row in the table above is NOT band3-only: 2,107,200 B
    is numerically the `game` CATEGORY block, which then held 254 band3 units PLUS
    the three root-level files and was MISSING four band3 units. The two errors
    partly cancel in the byte total, which is why it looked plausible. True
    band3-only, same ruler, after the fix: 258 units / 2,114,688 B / aon 60.240%.

The cause was mechanism, not bookkeeping: the tag was library-group membership in
objects.json with nothing tying it to the path. It is now DERIVED from the source
path (tools/source_category.py, wired in configure.py), so drift is impossible.
`source_category.py audit` fails on any category<->tier pair outside the
documented `CATEGORY_ALLOWED_TIERS` granularity table -- and it FAILED on
ZLibCompression before the fix above, so the guard is known able to fail.

⚠ STILL a granularity difference BY DESIGN, do not "fix" it: the 114 `src/network/`
units read tier `game` here (one priority tier) and category `network` there.

✅ THE VENDORED-SOURCE HALF OF THAT PARAGRAPH IS CLOSED (lane VENDTIER-1,
2026-08-14). `thirdparty` is a progress category now, so vendored upstream under
`src/system/` no longer counts as "Milo Engine Code". ⚠⚠ THE ENGINE TIER MOVED --
any `engine` % quoted across this change must say which side it is on:

    engine   BEFORE  670 units / 4,125,408 B / aon 55.522%
             AFTER   640 units / 4,019,668 B / aon 54.635%   (-30 units,
                                                              -105,740 B,
                                                              -0.887 pp)
    thirdparty (new)  30 units /   105,740 B / aon 89.230%

  Conservation checked exactly: engine+thirdparty after == engine before, +0
  units, +0 bytes, +0 matched bytes and +0 functions. game / network / sdk are
  bit-identical. Nothing left the total -- this is reattribution, not a
  shrunken denominator.

  ⚠ MEASURED AT ae7b8b9d. An earlier run of this same A/B at e629a7f8 read
  engine 55.569% -> 54.683% (-0.885 pp): the SPLIT is stable, the ABSOLUTES are
  not. Between the two runs lane GROUNDED-1 withdrew 8 alias memberships, and
  engine's matched bytes fell 2,292,428 -> 2,290,508 = -1,920 B, which is
  GROUNDED-1's reported figure to the byte. So a tier absolute here is only true
  at the commit it was measured on -- read `total_code`/`matched_code` out of
  report.json rather than quoting these, exactly as CLAUDE.md says for
  `total_code`. The -30 units / -105,740 B reattribution is the part that does
  not move.

★ THE SIGN WAS NOT PREDICTABLE FROM THE BYTE DIRECTION, and the intuition
"removing 105 kB from a tier" says nothing about which way its % goes. It is set
by the MATCH RATE OF WHAT MOVES: the departing units are 89.230% matched against
an engine tier averaging 55.569%, so exporting them LOWERS the remainder. The
engine number had been flattered by 0.885 pp of vendored code. (CATTAG-1 made
the mirror-image observation with the opposite sign -- game% ROSE when it gained
units matched 76.48% and shed units matched 12.06%.)

⇒ Third-party is comfortably our best-matched tier (89.2% vs game 60.2%, engine
54.7%), which is what a public-source oracle and mechanical work should look
like, and is an argument for the split rather than against it.

⚠ 30, not 31: the set is 34 declared / 31 pinned, and 4 of the 34 are Harmonix
code that merely LIVES in a vendored directory (VorbisMem.cpp, Jpeg.cpp, and two
*License.cpp) -- adjudicated per file against actual upstream and kept in
`engine`. Only VorbisMem.cpp of those is pinned, at 12 B. See the module
docstring of tools/source_category.py for the full per-file adjudication and
`bucket_for_source` below for the rule that catches them.

This tool remains reporting-only and metric-neutral by construction; the CATTAG-1
fix landed in objects.json/configure.py and the VENDTIER-1 fix in
config.json/source_category.py/scope_map.py, each measured Δmatched=0 /
Δcode%=0.000000pp (VENDTIER-1: Δfuzzy=0.000000pp too, 0 recompiles, and
`build.ninja` byte-identical).

CACHE HYGIENE -- scope_map.json is gitignored and addr-keyed to ONE target build.
If it is absent (fresh checkout / worktree) or keyed to a different revision of
the XEX, `priority` falls back to source-path/name-class only, ~65k anonymous
fn_8XXXXXXX functions collapse into `unknown`, the per-tier DENOMINATORS shrink,
and every tier % reads INFLATED. The dashboard now detects this and prints a
banner; the fix is always `python3 tools/scope_map.py build` (~1 s).

Subcommands: build | report | worklist | classify <addr>
"""
import argparse
import datetime
import json
import os
import re
import sys
from collections import defaultdict

# --- dead-index guard (lane BX-4) -------------------------------------------
# The guard helpers below were CALLED but never imported when the guards first
# landed, so every call site raised NameError instead of failing/skipping
# loudly. Audit: python3 tools/dead_index_guard.py --audit
import os as _dig_os, sys as _dig_sys
_dig_d = _dig_os.path.dirname(_dig_os.path.abspath(__file__))
while _dig_d != "/" and not _dig_os.path.exists(
        _dig_os.path.join(_dig_d, "tools", "dead_index_guard.py")):
    _dig_d = _dig_os.path.dirname(_dig_d)
_dig_sys.path.insert(0, _dig_os.path.join(_dig_d, "tools"))
from dead_index_guard import skip_if_dead as _dead_skip  # noqa: E402
# ----------------------------------------------------------------------------


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, "build", "45410914", "report.json")
SCOPE_MAP = os.path.join(ROOT, "config", "45410914", "scope_map.json")

# ID-sweep artifacts (provenance/xdk/thirdparty classification inputs). Committed
# under tools/scope_data so `build` is reproducible from a fresh checkout; the
# generated scope_map.json is gitignored (rebuild it with `scope_map.py build`).
# Override with WN_DATA= to point at a fresh identification-sweep output dir.
WN_DATA = os.environ.get("WN_DATA", os.path.join(ROOT, "tools", "scope_data"))

FN_ADDR_RE = re.compile(r"fn_([0-9A-Fa-f]{8})")
AUTO_ADDR_RE = re.compile(r"auto_\d+_([0-9A-Fa-f]{8})_")

# Tiers, NOT in/out of scope. Everything is matchable; this only sequences ROI.
#   ORACLE_BACKED  a source twin exists -> CHEAP -> near-term priority
#   NO_ORACLE      no source twin -> HARDER (reconstruct from asm), LOWER priority,
#                  but still matchable AND worth splitting
#   (unknown is neither: it's just not-yet-mapped)
ORACLE_BACKED = {"game", "engine", "thirdparty", "crt"}
NO_ORACLE = {"xdk", "vendor"}

# --- bucket ordering for stable reporting ---
BUCKET_ORDER = ["game", "engine", "thirdparty", "crt", "xdk", "vendor", "unknown"]


# ---------------------------------------------------------------------------
# source_path -> bucket
# ---------------------------------------------------------------------------
THIRDPARTY_MARKERS = (
    "/zlib/", "/oggvorbis/", "/json-c/", "/curl/", "/tomcrypt/",
    "/speex/", "/expat/", "/stlport/", "/libpng/", "/jpeg/",
)

# Vendored libraries that are pure C upstream. Every one of these ships C only,
# so a C++ TU sitting inside one is NOT upstream -- it is Harmonix glue that was
# filed next to the library it wraps (an allocator shim, a Milo-side API wrapper,
# a `Licenses` registration object). Its oracle is DC3/rb3-Wii, not the public
# tarball, so it belongs in `engine`.
#
# ⚠ `/stlport/` is deliberately ABSENT: STLport is a C++ library, so ".cpp inside
# it" would be upstream and the rule must not fire. (Moot today -- 0 stlport
# objects are declared and the tree carries 34 .c / 0 .cpp under it -- but the
# rule has to stay true if that changes.)
_C_ONLY_VENDOR_MARKERS = (
    "/zlib/", "/oggvorbis/", "/json-c/", "/curl/", "/tomcrypt/",
    "/speex/", "/expat/", "/libpng/", "/jpeg/",
)
_CXX_SUFFIXES = (".cpp", ".cxx", ".cc")


def bucket_for_source(sp):
    """Map a pinned unit source_path to a scope bucket. Returns None if unknown."""
    if not sp:
        return None
    low = sp.lower()
    # ⚠ Our own game/network trees vendor NOTHING, but they do contain
    # DIRECTORIES NAMED AFTER the library they bind, so a substring marker
    # false-positives there. Measured (lane CATTAG-1): exactly one row,
    # src/network/quazal/Compression/ZLib/ZLibCompression.cpp (316 B) -- a
    # 7-line `namespace Quazal {}` map scaffold whose oracle is
    # ../rb3/src/network/Plugins/ZLibCompression.cpp, i.e. Quazal NetZ
    # middleware and not zlib at all. It read `thirdparty` and was briefed as
    # a mis-tagged unit when the TAG was right and this function was wrong.
    # So: the in-scope game layer wins over a marker substring.
    if low.startswith("src/band3/") or low.startswith("src/network/"):
        return "game"
    # third-party libs (some live UNDER src/system/, so test before engine)
    #
    # ⚠ SECOND INSTANCE OF THE SAME DISEASE AS ZLibCompression.cpp ABOVE (lane
    # VENDTIER-1): a marker says "this path is near library X", which is not the
    # same claim as "this FILE came from X". Adjudicated per-file against actual
    # upstream, the four C++ TUs inside our C vendor dirs are all Harmonix:
    #   src/system/oggvorbis/VorbisMem.cpp     OggMalloc/OggFree over utl/MemMgr.h
    #   src/system/jpeg/Jpeg.cpp               LoadBitmapIntoJpeg, MILO_ASSERT
    #   src/system/zlib/ZlibLicense.cpp        utl/Licenses.h registration object
    #   src/system/synth/tomcrypt/TomCryptLicense.cpp    ditto
    # while all 30 .c files carry their upstream banner or unmistakable upstream
    # API (LibTomCrypt's mycrypt.h / cipher_descriptor / symmetric_CTR). Only
    # VorbisMem.cpp is pinned (12 B); the other three contribute 0 B today, so
    # this rule is about being RIGHT, not about the bytes.
    for m in THIRDPARTY_MARKERS:
        if m in low:
            if m in _C_ONLY_VENDOR_MARKERS and low.endswith(_CXX_SUFFIXES):
                break  # Harmonix glue in a C library -- fall through to engine
            return "thirdparty"
    # CRT we compile ourselves
    if "/xdk/libcmt/" in low:
        return "crt"
    # XDK middleware glue we ship (nuiapi etc.) -- out of scope
    if low.startswith("src/xdk/") or "/xdk/nuiapi/" in low:
        return "xdk"
    # (game layer is tested ABOVE the thirdparty markers -- see the note there)
    # Milo engine
    if low.startswith("src/system/"):
        return "engine"
    # root-level platform glue (keygen_xbox.cpp, Memory_Xbox.cpp) -> engine
    if low.startswith("src/") and low.count("/") == 1:
        return "engine"
    return None


def bucket_for_engine_label(src_file):
    """engine.json src_file -> bucket. Mostly src/system/* (engine), but the
    sweep flagged hamobj/ dc3-false-friends and jpeg/ FP utility; those still
    land in engine/thirdparty respectively -- the per-entry confidence/note in
    engine.json already caveats them, and provenance records the file."""
    return bucket_for_source(src_file) or "engine"


# ---------------------------------------------------------------------------
# uid-merge / cross-binary BinDiff src_path -> bucket
# ---------------------------------------------------------------------------
# The DC3 / rb3-Wii BinDiff cross-ID artifacts carry a `bindiff_src` path that
# names the *source-twin's* TU. Map it to our scope buckets. Distinct from
# bucket_for_source (which classifies OUR pinned units) because these paths use
# the twin trees' layout: DC3's `../dc3-decomp/src/{system,xdk,lazer,...}` and
# rb3-Wii's `band3/`, `network/`. Crucially this is also where the OUT-OF-SCOPE
# `vendor` bucket gets populated: DC3 already attributed the statically-linked
# Microsoft D3DX/XGRAPHICS/XAUDIO/XMA + RAD BINK code to `xdk/<lib>/` TUs with
# no source -- those are vendor, not the small XDK glue we ship.
_VENDOR_XDK_DIRS = (
    "/d3dx9/", "/xgraphics/", "/xaudio2/", "/d3d9i/", "/xhv2/", "/xapilibi/",
    "/xonline/", "/xmic/", "/xinput2/", "/xmcore/", "/xnet/", "/xlrc/", "/xmp/",
    "/xparty/", "/xjson/", "/xbdm/", "/nuiaudio/", "/nuispeech/", "/ST/",
    "/xact3/", "/xmedia/",
)


def bucket_for_uid_src(src):
    """A BinDiff `bindiff_src` path -> scope bucket. None if unattributable."""
    if not src:
        return None
    low = "/" + src.lower().replace("../dc3-decomp/src/", "").replace("../rb3/src/", "")
    for m in THIRDPARTY_MARKERS:
        if m in low:
            return "thirdparty"
    if "binkxenon" in low or "/binkxenon/" in low:
        return "vendor"          # RAD BINK middleware (DC3 ships it under src/)
    if "/xdk/libcmt/" in low:
        return "crt"
    for v in _VENDOR_XDK_DIRS:
        if v in low and "/xdk/" in low:
            return "vendor"
    if "/xdk/nuiapi/" in low:
        return "xdk"             # NUI api glue we actually ship (OUT, but not vendor)
    if "/xdk/" in low:
        return "vendor"          # any other XDK TU is a no-oracle MS lib (no source twin)
    if "/lazer/" in low:
        return "game"            # DC3's game/meta layer == RB3 game-layer twin
    if low.startswith("/band3/") or low.startswith("/network/"):
        return "game"            # rb3-Wii game code
    if "/system/" in low:
        return "engine"
    base = low.lstrip("/")
    if base.count("/") == 0 and base.endswith(".cpp"):
        return "engine"          # root-level DC3 platform glue (Memory_Xbox.cpp ...)
    return None


# ---------------------------------------------------------------------------
# mangled-symbol CLASS -> bucket (catch-all NAMED functions)
# ---------------------------------------------------------------------------
# ~8 K functions in the catch-all `auto_*` units carry full MSVC-mangled names
# whose CLASS/namespace token is ground-truth identity (e.g. ?DrawRegular@App@@
# -> class App). The loader retains these names (the catch-all loader path no
# longer drops them); classify_name() routes them by class. Resolution order:
#   STL token (stlpmtx_std/std)         -> thirdparty   (authoritative)
#   class present in an in-scope tree    -> engine/game  (NEVER overridden by
#                                          vendor -- protects matchable work)
#   class in empirical vendor set / prefix -> vendor
#   else                                 -> unknown (don't guess)
# The class sets live in tools/scope_data/name_class.json (regenerate with
# tools/scope_data/gen_name_class.py); the vendor set is derived empirically
# from DC3's BinDiff xdk/* attributions, so it's data-backed not guessed.
_VENDOR_NAME_PREFIXES = (
    "D3D", "ID3D", "XG", "XAudio", "IXAudio", "XACT", "XMA", "XAPO", "XHV",
    "Bink", "RAD", "Quazal", "NetZ", "ENet",
)
_VENDOR_NAME_EXACT = {
    "XGRAPHICS", "xWMA", "OAPIPELINE", "COAPReverbMono", "COAPReverbStereo",
    "CMixMatrix",
}


def _parse_msvc_scope(n):
    """Extract the primary class/namespace token from an MSVC-mangled name.
    Handles ctors (??0Class@@), dtors (??1/??_E), template classes (?$Tmpl@..),
    template functions (??$fn@..), anon namespaces (?A0x..@), and plain
    ?meth@Class@@. Returns the class token, or None for free functions /
    non-mangled names."""
    if not n.startswith("?"):
        return None
    s = n[1:]
    if s.startswith("?"):
        s = s[1:]
        if s.startswith("$"):
            # template FUNCTION ??$fn@<template-args>@<CLASS>@@<sig>. The class
            # is NOT the first token after the fn-name -- the template ARGS come
            # first (e.g. ??$Find@VCharClip@@@ObjectDir@@ -> class ObjectDir, not
            # the arg CharClip). Parsing the exact class out of the interleaved
            # arg/scope chain is brittle, so the template-fn case is resolved by
            # _candidate_scopes() + the class maps in classify_name(); here we
            # just return a sentinel so classify_name knows to take that path.
            return "\x00TMPLFN"
        else:
            # operator (??0 ctor, ??1 dtor, ??_E vector-dtor, ...): drop op code.
            if s and s[0] == "_":
                s = s[2:]
            elif s:
                s = s[1:]
    else:
        # ?name@... : skip the simple function-name token.
        at = s.find("@")
        if at < 0:
            return None
        s = s[at + 1:]
    # s now begins with the innermost scope token; skip anon-ns wrappers.
    for _ in range(4):
        if s.startswith("?$"):
            m = re.match(r"\?\$([A-Za-z0-9_]+)@", s)
            return m.group(1) if m else None
        if s.startswith("?A0x"):
            m = re.match(r"\?A0x[0-9a-f]+@(.*)", s, re.S)
            if m:
                s = m.group(1)
                continue
        break
    m = re.match(r"([A-Za-z0-9_]+)@", s)
    return m.group(1) if m else None


_NAME_CLASS_CACHE = {}


def load_name_class(wn_data):
    """Load + cache tools/scope_data/name_class.json -> (engine, game, vendor)
    class-name sets."""
    key = wn_data
    if key in _NAME_CLASS_CACHE:
        return _NAME_CLASS_CACHE[key]
    p = os.path.join(wn_data, "name_class.json")
    if not os.path.exists(p):
        res = (set(), set(), set())
    else:
        d = json.load(open(p))
        res = (set(d.get("engine", [])), set(d.get("game", [])),
               set(d.get("vendor", [])))
    _NAME_CLASS_CACHE[key] = res
    return res


def _vendor_name_hit(cls):
    if cls in _VENDOR_NAME_EXACT:
        return True
    for p in _VENDOR_NAME_PREFIXES:
        if cls.startswith(p) and len(cls) > len(p):
            return True
    return False


_CAND_RE = re.compile(r"[@VU]\??\$?([A-Za-z_][A-Za-z0-9_]*)@")


def _candidate_scopes(name):
    """All plausible class/type identifier tokens in a mangled name, e.g. every
    V<Class>@@ / U<Struct>@@ / @<Scope>@ token. Used to resolve template-FUNCTION
    owners (where the exact owning class is interleaved with template args)."""
    return [m.group(1) for m in _CAND_RE.finditer(name)]


def _resolve_class(cls, engine_cls, game_cls, vendor_cls):
    """A single class token -> (bucket, tag) or (None, None). In-scope source
    presence wins so `vendor` never steals matchable work."""
    if cls in engine_cls:
        return "engine", "name:" + cls
    if cls in game_cls:
        return "game", "name:" + cls
    if cls in vendor_cls:
        return "vendor", "name:" + cls
    if _vendor_name_hit(cls):
        return "vendor", "name-prefix:" + cls
    return None, None


def classify_name(name, engine_cls, game_cls, vendor_cls):
    """Mangled symbol name -> (bucket, provenance_tag) or (None, None).
    HIGH confidence when matched (the symbol is authoritative)."""
    if not name or not name.startswith("?"):
        return None, None
    # STLport / std templates are thirdparty regardless of the outer token.
    if "stlpmtx_std" in name or "@std@@" in name or "@stlp" in name or "stlp_std" in name:
        return "thirdparty", "name:stl"
    cls = _parse_msvc_scope(name)
    if cls == "\x00TMPLFN":
        # template FUNCTION: owning class is interleaved with template args. Scan
        # every candidate type/scope token; prefer an in-scope (engine/game)
        # match, else vendor, over the whole token set. (`Find<CharClip>` in
        # ObjectDir scans {Find?, CharClip, ObjectDir} -> ObjectDir=engine.)
        best = None
        for c in _candidate_scopes(name):
            b, tag = _resolve_class(c, engine_cls, game_cls, vendor_cls)
            if b in ("engine", "game"):
                return b, tag              # in-scope wins immediately
            if b and best is None:
                best = (b, tag)            # remember a vendor hit as fallback
        return best if best else (None, None)
    if not cls:
        return None, None              # free function / unparsable -> leave unknown
    return _resolve_class(cls, engine_cls, game_cls, vendor_cls)


SPLITS = os.path.join(ROOT, "config", "45410914", "splits.txt")
SPLIT_HDR_RE = re.compile(r"^(\S.*?):\s*$")
SPLIT_TEXT_RE = re.compile(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")
# Synthetic address space for fns we cannot place on real .text (a handful of
# pinned units with only a .pdata pin). Keyed high so they never collide with
# real .text (max real fn ~0x82C23xxx) and never participate in spatial runs.
SYNTH_BASE = 0xF0000000

# Pinned-unit named functions no ground-truth map could place, recorded rather
# than given a fabricated real-looking address.  Populated by load_functions();
# reported by `validate-addrs`.  Measured on the current tree: 0 rows.
UNRESOLVED_NAMED = []


def load_splits_text_bases(splits_path):
    """basename(no ext) -> .text start addr, from pinned splits.txt entries."""
    bases = {}
    if not os.path.exists(splits_path):
        return bases
    cur = None
    for line in open(splits_path):
        h = SPLIT_HDR_RE.match(line)
        if h and not line.startswith((" ", "\t")):
            cur = h.group(1)
        elif cur:
            t = SPLIT_TEXT_RE.search(line)
            if t:
                stem = cur.rsplit(".", 1)[0]
                bases[stem] = int(t.group(1), 16)
    return bases


_SPLIT_TEXT_BLOCKS = None


def load_splits_text_blocks(splits_path=None):
    """stem(basename, no ext) -> [ [(start,end), ...], ... ] -- one block LIST
    per splits heading sharing that stem.

    ⚠ Deliberately a list-of-lists, not a flat list: `Movie`, `Game`, `UIStats`,
    `Utl` and 23 other stems have BOTH a bare `Foo.cpp:` heading and a nested
    `band3/.../Foo.cpp:` one, and they are DIFFERENT units at different
    addresses.  Collapsing them by basename is the trap CLAUDE.md records as
    having broken four consecutive lanes; callers must disambiguate with real
    addresses (see `blocks_for_report_unit`), never by string matching.
    """
    global _SPLIT_TEXT_BLOCKS
    if splits_path is None:
        splits_path = SPLITS
        if _SPLIT_TEXT_BLOCKS is not None:
            return _SPLIT_TEXT_BLOCKS
    out = {}
    if os.path.exists(splits_path):
        per_head = {}
        order = []
        cur = None
        for line in open(splits_path):
            h = SPLIT_HDR_RE.match(line)
            if h and not line.startswith((" ", "\t")):
                cur = h.group(1)
            elif cur:
                t = SPLIT_TEXT_RE.search(line)
                if t:
                    if cur not in per_head:
                        per_head[cur] = []
                        order.append(cur)
                    per_head[cur].append((int(t.group(1), 16), int(t.group(2), 16)))
        for head in order:
            stem = os.path.basename(head).rsplit(".", 1)[0]
            out.setdefault(stem, []).append(per_head[head])
    if splits_path == SPLITS:
        _SPLIT_TEXT_BLOCKS = out
    return out


def _addr_in_blocks(addr, blks):
    return any(s <= addr < e for s, e in blks)


def blocks_for_report_unit(unit, anchors, blocks_by_stem=None):
    """Resolve a report unit name to its splits `.text` blocks.

    `anchors` are the unit's own `fn_<addr>` VAs, which are GROUND TRUTH (the
    address is in the symbol name).  When a basename is shared by several
    headings we pick the candidate containing the most anchors -- never a
    string-suffix match, which picks the wrong one for `Game`/`UIStats`/
    `AccomplishmentProgress` (measured: 79 phantom verdicts).  Returns None
    when the unit has no `.text` pin or cannot be disambiguated.
    """
    if blocks_by_stem is None:
        blocks_by_stem = load_splits_text_blocks()
    cands = blocks_by_stem.get(unit.split("/")[-1])
    if not cands:
        return None
    if len(cands) == 1:
        return cands[0]
    best, best_hits = None, 0
    for blks in cands:
        hits = sum(1 for a in anchors if _addr_in_blocks(a, blks))
        if hits > best_hits:
            best, best_hits = blks, hits
    return best


# ---------------------------------------------------------------------------
# NAMED catch-all fn -> true VA lookups (fixes the dropped/mis-addressed defect:
# every auto-carve function that obj_target_symbol_renamer renamed from
# fn_<addr> to a real MSVC mangled name used to get a SYNTHETIC address instead
# of its true VA -- see load_functions() catch-all branch below).
# ---------------------------------------------------------------------------
TARGET_SYMBOL_MAP = os.path.join(ROOT, "scripts", "target_symbol_map.json")
SYMBOLS_TXT = os.path.join(ROOT, "config", "45410914", "symbols.txt")

_TARGET_SYMBOL_NAME2ADDR = None


def load_target_symbol_name2addr():
    """scripts/target_symbol_map.json is addr(0x-hex str) -> mangled name, plus
    a handful of leading-'_' metadata keys (comments, denylists, ICF/bijection-
    arbitrary lists) that are NOT addr entries. Invert to name -> addr so a
    catch-all named function (renamed fn_<addr> -> mangled name by
    obj_target_symbol_renamer) can recover its true VA. A tiny number of names
    map to >1 addr (genuine ICF-folded duplicates); we cannot pick a VA for
    those without more evidence, so they are dropped here and fall through to
    the next lookup (load_symbols_txt_name2addr) or the synthetic fallback."""
    global _TARGET_SYMBOL_NAME2ADDR
    if _TARGET_SYMBOL_NAME2ADDR is not None:
        return _TARGET_SYMBOL_NAME2ADDR
    name2addr = {}
    if os.path.exists(TARGET_SYMBOL_MAP):
        d = json.load(open(TARGET_SYMBOL_MAP))
        dup = set()
        for k, v in d.items():
            if not (k.startswith("0x") and isinstance(v, str)):
                continue      # skip metadata keys + ICF-arbitrary addr LISTS
            if v in name2addr:
                dup.add(v)
                continue
            name2addr[v] = int(k, 16)
        for v in dup:
            name2addr.pop(v, None)
    _TARGET_SYMBOL_NAME2ADDR = name2addr
    return name2addr


_SYMBOLS_TXT_NAME2ADDR = None
_SYMBOLS_TXT_LINE_RE = re.compile(r"^(\S+) = \S+:0x([0-9A-Fa-f]+);")


def load_symbols_txt_name2addr():
    """config/45410914/symbols.txt: 'NAME = section:0xADDR; ...', one line per
    real VA. Covers names target_symbol_map.json doesn't carry -- e.g. compiler
    runtime-helper stubs (__savegprlr_N / __restgprlr_N / __save-/restfprlr_N)
    and dtk-assigned lbl_<addr> labels whose embedded hex is stale (post-ICF/
    relocation) so it must NOT be parsed as an address -- the line's actual
    0xADDR is authoritative. Second fallback after target_symbol_map.json."""
    global _SYMBOLS_TXT_NAME2ADDR
    if _SYMBOLS_TXT_NAME2ADDR is not None:
        return _SYMBOLS_TXT_NAME2ADDR
    out = {}
    if os.path.exists(SYMBOLS_TXT):
        for line in open(SYMBOLS_TXT):
            m = _SYMBOLS_TXT_LINE_RE.match(line)
            if m:
                out[m.group(1)] = int(m.group(2), 16)
    _SYMBOLS_TXT_NAME2ADDR = out
    return out


_TARGET_SYMBOL_NAME2ADDRS = None


def load_target_symbol_name2addrs():
    """name -> [addr, ...] INCLUDING the duplicates `load_target_symbol_name2addr`
    drops.  A name mapping to >1 address cannot be resolved by name alone, but a
    caller who knows the row's unit CAN pick the candidate lying inside that
    unit's own `.text` blocks.  That recovers the residual (measured: 2 rows,
    both `?NodeCmp@@YAHPBX0@Z`, a file-static name present in several TUs)."""
    global _TARGET_SYMBOL_NAME2ADDRS
    if _TARGET_SYMBOL_NAME2ADDRS is not None:
        return _TARGET_SYMBOL_NAME2ADDRS
    out = {}
    if os.path.exists(TARGET_SYMBOL_MAP):
        for k, v in json.load(open(TARGET_SYMBOL_MAP)).items():
            if k.startswith("0x") and isinstance(v, str):
                out.setdefault(v, []).append(int(k, 16))
    _TARGET_SYMBOL_NAME2ADDRS = out
    return out


def resolve_named_va(name, unit_blocks=None):
    """Mangled/named function -> its TRUE virtual address, or None.

    THE single address-resolution path for named functions, shared by the
    pinned-unit and catch-all branches of load_functions().  Order:

      (1) scripts/target_symbol_map.json  -- ground truth, the same map
          obj_target_symbol_renamer used, inverted;
      (2) config/45410914/symbols.txt     -- covers compiler runtime helpers
          (__savegprlr_N etc.) and dtk lbl_ labels the map does not carry;
      (3) ambiguous map names (>1 address) disambiguated by `unit_blocks`.

    ⛔ There is deliberately NO arithmetic fallback here.  report.json's per-fn
    `address` is a per-unit CUMULATIVE offset, so `block_start + address` is a
    SYNTHETIC address -- the identical formula CLAUDE.md documents for dtk's
    `.s` address columns, and the defect this function exists to remove.  A
    block-list walk over the unit's `.text` spans looks like it should invert
    that cumulative offset and DOES NOT: measured, it reproduces the true VA
    for only 5.20% of rows (errors of 4-12 B that accumulate across blocks,
    because the pinned spans include alignment/EH padding the cumulative
    offsets do not).  It was implemented, measured and rejected -- do not
    reintroduce it.  Callers must handle None rather than fabricate.
    """
    addr = load_target_symbol_name2addr().get(name)
    if addr is not None:
        return addr
    addr = load_symbols_txt_name2addr().get(name)
    if addr is not None:
        return addr
    if unit_blocks:
        cands = [a for a in load_target_symbol_name2addrs().get(name, ())
                 if _addr_in_blocks(a, unit_blocks)]
        if len(cands) == 1:
            return cands[0]
    return None


# ---------------------------------------------------------------------------
# load report.json -> list of (addr, size, matched, source_path, unit)
# ---------------------------------------------------------------------------
def load_functions(report_path, dedup=True):
    """Return [(addr, size, matched, source_path, unit, name)], rep.

    dedup=True (for `build`, where the output scope_map.json is addr-keyed and
    must be unique) collapses same-addr records (ICF folds / catch-all named-fn
    anchoring). dedup=False (for PROGRESS accounting) keeps EVERY report fn so the
    totals reconcile EXACTLY with report.json's official measures: summing all fn
    sizes == total_code, and the count == total_functions. Dedup drops ~28 fns /
    ~4 KB, so progress must NOT dedup."""
    with open(report_path) as f:
        rep = json.load(f)
    split_bases = load_splits_text_bases(SPLITS)
    funcs = []  # (addr:int, size:int, matched:bool, source_path, unit_name, name)
    synth = SYNTH_BASE
    for u in rep["units"]:
        unit = u["name"]
        sp = (u.get("metadata") or {}).get("source_path")
        fns = u.get("functions") or []
        if not fns:
            continue

        # -- PINNED SOURCE UNITS (have source_path) --
        # `fn_<addr>` rows carry their VA in the name.  NAMED rows are resolved
        # by real VA through resolve_named_va(), exactly as the catch-all
        # branch below does -- ONE shared implementation.
        #
        # ⛔ This branch used to read `base + int(fn["address"])`, with the
        # comment "dtk emits clean per-unit relative offsets, so base + rel is
        # exact".  That premise is FALSE: report.json's per-fn `address` is a
        # per-unit CUMULATIVE offset, so for a MULTI-BLOCK unit it computed
        # `first_block_start + cumulative offset` -- a synthetic address that
        # is well-formed, confident, and points at another TU's code.  Measured
        # on the unfixed tool: 11,023 / 20,190 named rows in multi-block units
        # (54.60%) landed outside every real `.text` block of their own unit,
        # vs 217 / 2,846 (7.62%) in single-block units where the defect is
        # structurally impossible -- a 7.16x enrichment, 11,240 rows total.
        # The unit ATTRIBUTIONS were always right: of the bad rows with a known
        # true address, 11,239 / 11,239 (100%) lie inside their own unit.
        if sp:
            anchors = [int(m.group(1), 16)
                       for m in (FN_ADDR_RE.match(f["name"]) for f in fns) if m]
            unit_blocks = blocks_for_report_unit(unit, anchors)
            base = None
            for fn in fns:
                m = FN_ADDR_RE.match(fn["name"])
                if m:
                    cand = int(m.group(1), 16) - int(fn.get("address", "0"))
                    if base is None or cand < base:
                        base = cand
            if base is None:
                base = split_bases.get(unit.split("/")[-1])
            if base is None:
                base = synth
                synth += 0x10000
            for fn in fns:
                name = fn["name"]
                m = FN_ADDR_RE.match(name)
                if m:
                    addr = int(m.group(1), 16)
                else:
                    addr = resolve_named_va(name, unit_blocks)
                    if addr is None:
                        # Unresolvable by any ground-truth map.  Park it in the
                        # synthetic island rather than on a plausible-looking
                        # real address: a wrong-but-well-formed VA is the defect
                        # this branch exists to remove, and UNRESOLVED_NAMED
                        # lets `validate-addrs` count it instead of hiding it.
                        addr = SYNTH_BASE + (len(UNRESOLVED_NAMED) + 1)
                        UNRESOLVED_NAMED.append((unit, name))
                size = int(fn.get("size", "0"))
                matched = float(fn.get("match_percent_normalized", 0.0)) >= 100.0
                fz = float(fn.get("fuzzy_match_percent", 0.0))
                funcs.append((addr, size, matched, sp, unit, name, fz))
            continue

        # -- CATCH-ALL / auto UNITS (no source_path) --
        # The report's per-fn `address` here is an internal monotonic index, NOT
        # a recoverable VA (catch-all units interleave many non-contiguous
        # regions and the offset drifts vs the true layout). So fn_ functions
        # are placed by their absolute name.
        #
        # NAMED functions (every one obj_target_symbol_renamer renamed from
        # fn_<addr> to its real MSVC mangled name -- template insts, thunks,
        # ordinary named methods, all still unmatched here) DO have a real VA:
        # look it up, in order, via
        #   (1) scripts/target_symbol_map.json (name -> VA, ground truth --
        #       this is exactly the map the renamer used, inverted), else
        #   (2) config/45410914/symbols.txt (exact name -> VA -- covers a
        #       residual class target_symbol_map.json doesn't carry: compiler
        #       runtime-helper stubs like __savegprlr_N/__restgprlr_N and
        #       dtk lbl_<addr> labels).
        # Only when BOTH miss (measured: 1 of 5,377 catch-all named fns, an
        # ICF-merged __MERGED_fn_ symbol with no map entry) do we fall back to
        # the old synthetic placement: anchor to the nearest *preceding* fn_ in
        # listing order (good enough to inherit that neighbor's bucket via
        # spatial locality) with a tiny distinct delta to avoid collisions.
        # Functions are listed in `address`-monotonic order.
        last_anchor = None
        named_off = 0
        for fn in fns:
            name = fn["name"]
            m = FN_ADDR_RE.match(name)
            size = int(fn.get("size", "0"))
            matched = float(fn.get("match_percent_normalized", 0.0)) >= 100.0
            fz = float(fn.get("fuzzy_match_percent", 0.0))
            if m:
                addr = int(m.group(1), 16)
                last_anchor = addr
                named_off = 0
            else:
                real_addr = resolve_named_va(name)
                if real_addr is not None:
                    # Real VA recovered -- do NOT touch last_anchor/named_off,
                    # those only govern the synthetic-placement fallback for
                    # fns that still need it.
                    addr = real_addr
                else:
                    if last_anchor is None:
                        # named fns before the first anchor: use unit-name base.
                        am = AUTO_ADDR_RE.search(unit)
                        last_anchor = int(am.group(1), 16) if am else SYNTH_BASE
                    named_off += 1
                    # small odd delta keeps these distinct from the anchor +
                    # each other without crossing the next real fn (sizes >=8).
                    addr = last_anchor + named_off  # 1..N within the anchor's slot
            funcs.append((addr, size, matched, sp, unit, name, fz))
    if not dedup:
        return funcs, rep
    # de-dup on addr (ICF folds / catch-all named-fn anchoring can land two
    # records on one addr). Precedence, best first:
    #   (1) pinned source_path beats catch-all   (we compile it; ground truth)
    #   (2) matched beats unmatched
    #   (3) larger size
    # Tuple ordering on (has_sp, matched, size) does exactly this.
    by_addr = {}
    for addr, size, matched, sp, unit, name, fz in funcs:
        rank = (1 if sp else 0, 1 if matched else 0, size)
        cur = by_addr.get(addr)
        if cur is None or rank > cur[0]:
            by_addr[addr] = (rank, size, matched, sp, unit, name, fz)
    out = [(addr, r[1], r[2], r[3], r[4], r[5], r[6]) for addr, r in by_addr.items()]
    out.sort()
    return out, rep


# ---------------------------------------------------------------------------
# ID-sweep artifact loaders
# ---------------------------------------------------------------------------
def load_xdk(wn_data):
    p = os.path.join(wn_data, "xdk.json")
    if not os.path.exists(p):
        return set()
    d = json.load(open(p))
    addrs = set()
    for a in d.get("xdk_fn_addrs", []):
        addrs.add(int(a, 16))
    # also fold in any BINK range so we catch gap fns between named exports
    return addrs


def load_xdk_bink_range(wn_data):
    p = os.path.join(wn_data, "xdk.json")
    if not os.path.exists(p):
        return None
    d = json.load(open(p))
    bs = d.get("bink_section") or {}
    if "code_start" in bs and "code_end" in bs:
        return (int(bs["code_start"], 16), int(bs["code_end"], 16))
    return None


def load_thirdparty_ranges(wn_data):
    p = os.path.join(wn_data, "thirdparty.json")
    if not os.path.exists(p):
        return []
    d = json.load(open(p))
    ranges = []  # (start, end, libname)
    for lib, entries in d.items():
        if not isinstance(entries, list):
            continue
        for e in entries:
            if "start" in e and "end" in e:
                ranges.append((int(e["start"], 16), int(e["end"], 16), lib))
    ranges.sort()
    return ranges


def load_provenance(wn_data, fname):
    """engine.json / game.json: {ADDR_HEX_no0x: {src_file, confidence[, note]}}"""
    p = os.path.join(wn_data, fname)
    if not os.path.exists(p):
        return {}
    d = json.load(open(p))
    out = {}
    for k, v in d.items():
        try:
            addr = int(k, 16)
        except ValueError:
            continue
        out[addr] = v
    return out


def load_uid_merge(wn_data):
    """uid_merge.json: {ADDR_HEX: {bucket, src, sim, source, conf}} addr-keyed
    cross-binary BinDiff IDs (DC3 sim>=0.9, rb3-Wii sim>=0.7). Pre-bucketed by
    the generator (tools/scope_data/gen_uid_merge.py); we re-derive the bucket
    here too as a guard so a stale file with raw `src` still classifies."""
    p = os.path.join(wn_data, "uid_merge.json")
    if not os.path.exists(p):
        # DELETED ON PURPOSE by lane BX-4 (2026-07-30) -- do not "restore" it.
        # uid_merge.json was a committed 1.48 MB TU0-era artifact: only 3.81% of
        # its 10,671 addresses were real .text function starts (chance ~2-3%),
        # and an exhaustive search over every 4-byte shift in +/-0x20000 topped
        # out at 4.70%, so no rebase could recover it. It was also
        # IRRECOVERABLE in principle: each entry is {bucket, conf, sim, source,
        # src} keyed ONLY by the dead address -- strip the address and nothing
        # identifiable remains to re-attach. Its generator
        # (tools/scope_data/gen_uid_merge.py) reads unified_id.json +
        # unified_id_rb3wii.json, which are themselves dead, so it cannot be
        # regenerated either. This layer is simply gone; the other provenance
        # layers carry the map. See tools/dead_index_guard.py.
        return {}
    # dead-index guard (lane BX-4): uid_merge.json is TU0-era (3.81% of its
    # 10,671 addresses are real .text function starts; chance ~2-3%). It is
    # ONE provenance layer of several, so drop it loudly rather than failing
    # the whole scope map -- but it must never silently colour a tier again.
    if _dead_skip(p, what="scope_map uid_merge layer"):
        return {}
    d = json.load(open(p))
    entries = d.get("entries", d)
    out = {}
    for k, v in entries.items():
        if k.startswith("_"):
            continue
        try:
            addr = int(k, 16)
        except ValueError:
            continue
        out[addr] = v
    return out


# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------
def normalize_conf(v):
    """confidence may be a string tier (engine.json) or a float (game.json)."""
    if isinstance(v, (int, float)):
        return float(v)
    tier = {
        "high": 0.85, "medium": 0.65, "low": 0.45, "low-single": 0.35,
        "low-generic": 0.2, "fp-jpeg-util": 0.15,
    }
    return tier.get(str(v), 0.4)


def cmd_build(args):
    funcs, rep = load_functions(REPORT)
    xdk_addrs = load_xdk(WN_DATA)
    bink_range = load_xdk_bink_range(WN_DATA)
    tp_ranges = load_thirdparty_ranges(WN_DATA)
    engine_prov = load_provenance(WN_DATA, "engine.json")
    game_prov = load_provenance(WN_DATA, "game.json")
    uid_merge = load_uid_merge(WN_DATA)
    engine_cls, game_cls, vendor_cls = load_name_class(WN_DATA)

    def in_tp(addr):
        for s, e, lib in tp_ranges:
            if s <= addr < e:
                return lib
        return None

    n = len(funcs)
    scope = [None] * n          # bucket string
    prov = [None] * n           # provenance string
    conf = [0.0] * n            # confidence float

    for i, (addr, size, matched, sp, unit, name, fz) in enumerate(funcs):
        # Layer 1: XDK exact addr / BINK range -> OUT-OF-SCOPE.
        if addr in xdk_addrs or (bink_range and bink_range[0] <= addr < bink_range[1]):
            scope[i] = "xdk"
            prov[i] = "xdk.json:import-thunk-or-bink"
            conf[i] = 1.0
            continue
        # Layer 2: pinned-unit source_path (strongest -- we compile it).
        b = bucket_for_source(sp)
        if b:
            scope[i] = b
            prov[i] = "pinned:" + sp
            conf[i] = 1.0
            continue
        # Layer 3: thirdparty library address range.
        lib = in_tp(addr)
        if lib:
            scope[i] = "thirdparty"
            prov[i] = "thirdparty.json:" + lib
            conf[i] = 0.9
            continue
        # Layer 4: engine/game provenance labels (address-keyed).
        if addr in game_prov:
            v = game_prov[addr]
            scope[i] = bucket_for_source(v.get("src_file")) or "game"
            prov[i] = "game.json:" + str(v.get("src_file"))
            conf[i] = normalize_conf(v.get("confidence"))
            continue
        if addr in engine_prov:
            v = engine_prov[addr]
            scope[i] = bucket_for_engine_label(v.get("src_file"))
            note = v.get("note")
            prov[i] = "engine.json:" + str(v.get("src_file")) + (("|" + note) if note else "")
            conf[i] = normalize_conf(v.get("confidence"))
            continue
        # Layer 5: uid_merge cross-binary BinDiff ID (addr-keyed). DC3 (sim>=0.9,
        # conf 0.95) and rb3-Wii (sim>=0.7, conf 0.7) twins. Pre-bucketed by the
        # generator; re-derive from `src` if a stale file lacks `bucket`.
        if addr in uid_merge:
            v = uid_merge[addr]
            ub = v.get("bucket") or bucket_for_uid_src(v.get("src"))
            if ub:
                scope[i] = ub
                prov[i] = "%s:%s" % (v.get("source", "uid"), v.get("src"))
                conf[i] = float(v.get("conf", 0.9))
                continue
        # Layer 6: mangled-symbol CLASS -> bucket (catch-all NAMED fns). The
        # symbol name is authoritative for the owning class; classify_name never
        # lets `vendor` override a class that exists in an in-scope source tree.
        nb, ntag = classify_name(name, engine_cls, game_cls, vendor_cls)
        if nb:
            scope[i] = nb
            prov[i] = ntag
            conf[i] = 0.9
            continue
        # else: leave for spatial propagation.

    # ---- Layer 5: REACH-CAPPED SPATIAL CLUSTER PROPAGATION ----
    # No LTCG => functions of one TU sit in one contiguous .text run, so an
    # unlabeled function immediately adjacent to a confident label is very
    # likely the SAME TU (same bucket). But labels are sparse islands: ~12% of
    # bytes are confidently labeled (layers 1-4), and naive "fill the whole gap
    # from its endpoints" would absorb 5+ MB of genuinely-unidentified code
    # (the largest single gap spans 1.2 MB / 768 fns) into a bucket on no
    # evidence -- destroying the meaning of the denominator.
    #
    # So we propagate a confident edge's label only a bounded REACH into the
    # adjacent gap (REACH_BYTES per side, ~a couple of large TUs worth), and
    # only across functions that stay spatially contiguous (no jump larger than
    # GAP_JUMP between consecutive fns -- a big address jump implies a section
    # boundary / unrelated TU). Whatever the two edges cannot reach in a large
    # gap stays UNKNOWN -- the honest classification-TODO residual the metric
    # reports separately. XDK is an exact-addr island and is never propagated.
    REACH_BYTES = 6144      # ~one big TU of reach from each confident edge
    GAP_JUMP = 256          # consecutive-fn address jump that breaks a TU run

    # xdk is an exact-addr import-thunk island and never propagates. `vendor` is
    # a real contiguous-TU bucket, BUT a single BinDiff-misplaced vendor fn (e.g.
    # one nuispeech leaf dropped into a CharClip/PropKeys engine run) must not
    # seed a 6 KB vendor sweep that steals matchable engine code from the
    # denominator. So vendor is allowed to propagate ONLY when it is the agreeing
    # SANDWICH bucket (both edges vendor) -- never as a lone single edge. (Name-
    # classified vendor fns carry synthetic addrs >= SYNTH_BASE and sort past all
    # real fns, so they never act as edges inside a real-fn run anyway.)
    NO_PROP = {"xdk"}
    SANDWICH_ONLY = {"vendor"}

    def is_conf(i):
        return scope[i] is not None and scope[i] not in NO_PROP

    def fn_end(i):
        return funcs[i][0] + funcs[i][1]

    i = 0
    while i < n:
        if scope[i] is not None:
            i += 1
            continue
        j = i
        while j < n and scope[j] is None:
            j += 1
        left = i - 1
        right = j
        lb = scope[left] if left >= 0 and is_conf(left) else None
        rb = scope[right] if right < n and is_conf(right) else None
        sandwiched = (lb is not None and lb == rb)
        # a SANDWICH_ONLY bucket may only fill when it is the agreeing sandwich.
        lb_prop = lb if (lb not in SANDWICH_ONLY or sandwiched) else None
        rb_prop = rb if (rb not in SANDWICH_ONLY or sandwiched) else None

        # propagate from the LEFT edge rightward, within reach + contiguity
        if lb_prop is not None:
            anchor_end = fn_end(left)
            prev_end = anchor_end
            for k in range(i, j):
                a = funcs[k][0]
                if a - prev_end > GAP_JUMP:
                    break                      # section/TU break -> stop reach
                if a - anchor_end > REACH_BYTES:
                    break                      # out of reach -> stop
                scope[k] = lb_prop
                prov[k] = "spatial:after-" + lb_prop
                conf[k] = 0.4
                prev_end = fn_end(k)
        # propagate from the RIGHT edge leftward into still-unlabeled fns
        if rb_prop is not None:
            anchor_start = funcs[right][0]
            next_start = anchor_start
            for k in range(j - 1, i - 1, -1):
                if scope[k] is not None:
                    break                      # met the left-side fill
                e = fn_end(k)
                if next_start - e > GAP_JUMP:
                    break
                if anchor_start - funcs[k][0] > REACH_BYTES:
                    break
                # if both edges agree, bump confidence slightly (sandwiched)
                if scope[k] is None:
                    scope[k] = rb_prop
                    prov[k] = ("spatial:between-" + rb_prop) if sandwiched else ("spatial:before-" + rb_prop)
                    conf[k] = 0.5 if sandwiched else 0.4
                next_start = funcs[k][0]

        # anything still unlabeled in this gap = genuine residual
        for k in range(i, j):
            if scope[k] is None:
                scope[k] = "unknown"
                prov[k] = "residual"
                conf[k] = 0.0
        i = j

    # ---- emit ----
    out = {}
    for i, (addr, size, matched, sp, unit, name, fz) in enumerate(funcs):
        out["%08X" % addr] = {
            "size": size,
            "scope": scope[i],
            "provenance": prov[i],
            "confidence": round(conf[i], 3),
            "matched": matched,
        }
    os.makedirs(os.path.dirname(SCOPE_MAP), exist_ok=True)

    # Back up the outgoing cache BEFORE clobbering it, and report what moved.
    # `build` silently replaced the only copy of the classification, so a tier %
    # could shift by ~15 points with nothing on screen saying why -- on
    # 2026-08-13 the game tier moved 20,688 -> 24,985 fns (denominator +24%,
    # reading 75% -> 59%) and the change was only reconstructable because
    # unrelated worktrees happened to hold reflinked copies from 07-31.
    # --quiet is for the automated PROGRESS path, where this rebuild is a
    # cache refresh nobody asked for and the tier dashboard prints immediately
    # after: the RECLASSIFIED diff and the summary table are signal when a human
    # types `build` and pure noise in front of the thing they actually ran.
    # ONE line still prints -- a rebuild that silently did nothing must not look
    # identical to one that ran.
    quiet = getattr(args, "quiet", False)

    prev = None
    if os.path.exists(SCOPE_MAP):
        try:
            with open(SCOPE_MAP) as f:
                prev = json.load(f)
            bak = SCOPE_MAP + ".bak"
            os.replace(SCOPE_MAP, bak)
            if not quiet:
                print("backed up previous cache -> %s" % bak)
        except (OSError, ValueError) as e:
            print("(could not back up previous cache: %s)" % e)
            prev = None

    with open(SCOPE_MAP, "w") as f:
        json.dump(out, f, indent=0, sort_keys=True)
    if quiet:
        print("scope cache rebuilt from report.json (%d functions)" % len(out))
        return
    print("wrote %s (%d functions)" % (SCOPE_MAP, len(out)))

    if prev:
        import collections as _c
        oldc, newc = _c.Counter(), _c.Counter()
        for v in prev.values():
            oldc[v.get("scope", "?")] += 1
        for v in out.values():
            newc[v.get("scope", "?")] += 1
        moves = [(s, newc[s] - oldc[s]) for s in sorted(set(oldc) | set(newc))
                 if newc[s] != oldc[s]]
        if moves:
            print("RECLASSIFIED vs previous cache (tier denominators moved — "
                  "a tier % can shift with NO change in matched code):")
            for s, d in sorted(moves, key=lambda x: -abs(x[1])):
                print("    %-12s %+6d fns   (%d -> %d)" % (s, d, oldc[s], newc[s]))
    # quick summary
    _print_report(out)


# ---------------------------------------------------------------------------
# report
# ---------------------------------------------------------------------------
def _load_scope_map():
    with open(SCOPE_MAP) as f:
        return json.load(f)


def _print_report(sm):
    by = defaultdict(lambda: [0, 0, 0, 0])  # bucket -> [fns, bytes, matched_fns, matched_bytes]
    for addr, e in sm.items():
        b = e["scope"]
        by[b][0] += 1
        by[b][1] += e["size"]
        if e["matched"]:
            by[b][2] += 1
            by[b][3] += e["size"]
    total_bytes = sum(v[1] for v in by.values())
    total_fns = sum(v[0] for v in by.values())
    def tier(b):
        return ("oracle" if b in ORACLE_BACKED
                else "no-oracle" if b in NO_ORACLE else "todo")

    print()
    print("=== BUCKET BREAKDOWN (tier = priority + oracle, NOT in/out) ===")
    # ⚠ COLUMN NAMES MATTER HERE -- this is the SAME disease the dashboard's
    # aon%/mean% split fixes, one field further out. `matched` in scope_map.json
    # is the match_percent_normalized==100 predicate -- the ARG-BLIND ruler --
    # while matched_code (and every number in the dashboard below) keys on
    # fuzzy_match_percent==100. Printed as a bare "m_bytes" it read as an
    # optimistic duplicate of the dashboard's figure and was quoted as one.
    # Measured at b574f653 on the game tier: mpn 1,653,676 B vs fuzzy
    # 1,303,552 B. Of the 342,456 B gap vs the cached value, 350,124 B is the
    # PREDICATE and only 7,668 B is cache staleness -- so "it's just a stale
    # snapshot" is the wrong explanation by ~44x. Whole-binary the mpn-vs-fuzzy
    # gap is 922,112 B, i.e. MPNGAP-1's documented 930,204 B mpn==100/fuzzy<100
    # stratum, reached here independently.
    print("(mpn_* = match_percent_normalized==100, the ARG-BLIND ruler. They do")
    print(" NOT reconcile with matched_code, which keys on fuzzy==100; the")
    print(" dashboard below is the fuzzy-keyed view. Cached + dedup'd.)")
    print("%-12s %8s %12s %8s %12s  %-9s" %
          ("bucket", "fns", "bytes", "mpn_fns", "mpn_bytes", "tier"))
    for b in BUCKET_ORDER:
        fns, byt, mf, mb = by[b]
        print("%-12s %8d %12d %8d %12d  %-9s" % (b, fns, byt, mf, mb, tier(b)))
    print("-" * 64)
    print("%-12s %8d %12d %8d %12d  %s" %
          ("TOTAL", total_fns, total_bytes, sum(v[2] for v in by.values()),
           sum(v[3] for v in by.values()), "(cached snapshot; dedup'd)"))
    # The progress block below is AUTHORITATIVE: official measures for the headline
    # + a no-dedup live classification for per-tier (sums to total_code exactly).
    by_live, measures, cache = _progress_by_live()
    print_progress(by_live, measures, cache=cache)


# Tolerated |report size - symbols.txt size| gap; see cmd_validate_addrs.
SIZE_TOLERANCE = 4

_SYMBOLS_TXT_ADDR2SIZE = None
_SYMBOLS_TXT_SIZE_RE = re.compile(
    r"^\S+ = \.text:0x([0-9A-Fa-f]+);.*\bsize:0x([0-9A-Fa-f]+)")


def load_symbols_txt_addr2size():
    """.text VA -> size, from config/45410914/symbols.txt."""
    global _SYMBOLS_TXT_ADDR2SIZE
    if _SYMBOLS_TXT_ADDR2SIZE is not None:
        return _SYMBOLS_TXT_ADDR2SIZE
    out = {}
    if os.path.exists(SYMBOLS_TXT):
        for line in open(SYMBOLS_TXT):
            m = _SYMBOLS_TXT_SIZE_RE.match(line)
            if m:
                out[int(m.group(1), 16)] = int(m.group(2), 16)
    _SYMBOLS_TXT_ADDR2SIZE = out
    return out


# Exit code for "the artifact this command is believed to validate is not on
# disk". Deliberately NOT 0 and NOT 1: absent is neither a clean bill of health
# nor a defect, and the two must be distinguishable by a caller. See
# _artifact_check's docstring for why 0 was the dangerous answer.
EXIT_ARTIFACT_ABSENT = 3


def _artifact_check(scope_map_path, funcs_nodedup, total_functions):
    """Assert the ON-DISK scope_map.json agrees with THIS report.json.

    ⛔⛔ THIS COMMAND USED TO VALIDATE A FILE IT NEVER OPENED.  Everything below
    `validate-addrs`' own banner is RECOMPUTED from report.json + symbols.txt +
    splits.txt, so it graded the DERIVATION LOGIC and said nothing whatever
    about `config/45410914/scope_map.json`.  Measured, both on the same tree:
    with the stale fabricating artifact on disk it reported `VERDICT: PASS,
    exit 0`, and with the artifact **DELETED ENTIRELY** it still reported
    `VERDICT: PASS, exit 0`.  That is the hole that let one stale cache produce
    two green signals -- a gate that cannot fail is worse than no gate, because
    it is also a gate nobody re-checks.

    The invariant was already WRITTEN in this command's docstring and never
    wired up: the artifact must hold exactly `total_functions` keys with ZERO
    address collisions.  Three checks, chosen so each can fail alone:

      C1 COUNT      keys on disk == report `total_functions`.  The fabricating
                    version lost 650 functions to colliding synthetic keys, so
                    a short count is that defect's direct signature.
      C2 COLLISION  the derivation itself must not collapse two functions onto
                    one address (unique report addrs == report row count).
                    Fails on the derivation, not the file -- kept separate so a
                    regression in load_functions cannot be misread as staleness.
      C3 IDENTITY   the key SET on disk == the set of addresses in this report.
                    Strictly stronger than C1: it catches a cache with the right
                    number of keys pointing at the WRONG addresses, which is
                    exactly the shape of the incident file (68,576 keys, 0.6881
                    coverage -- a count within 1% of correct, and a third of the
                    addresses fabricated).

    ⚠ `expected` is derived from the no-dedup rows the caller already loaded,
    NOT from a second `load_functions(dedup=True)` call: `load_functions`
    APPENDS to the module-global UNRESOLVED_NAMED, so calling it twice
    double-counts the unresolved list this command reports and gates on.
    Dedup is a pure group-by on `addr` (see load_functions), so the dedup'd key
    set IS the set of unique addresses -- same answer, no side effect.

    Returns a dict with `status` in {ok, fail, absent, unreadable} plus the
    counts, and never raises: a reporting bug must not be able to mask the
    finding it is reporting."""
    rel = os.path.relpath(scope_map_path, ROOT)
    res = {"status": "ok", "path": rel, "keys": None, "expected": None,
           "total_functions": total_functions, "missing": None, "extra": None,
           "collisions": None, "coverage": None, "fails": []}

    expected = {"%08X" % f[0] for f in funcs_nodedup}
    res["expected"] = len(expected)
    # C2 is a property of the derivation, so it is computable even with no file.
    res["collisions"] = len(funcs_nodedup) - len(expected)

    if not os.path.exists(scope_map_path):
        res["status"] = "absent"
        return res
    try:
        with open(scope_map_path) as f:
            sm = json.load(f)
        if not isinstance(sm, dict):
            raise ValueError("top level is %s, not an object" % type(sm).__name__)
    except (OSError, ValueError) as e:
        res["status"] = "unreadable"
        res["error"] = str(e)
        res["fails"].append("artifact unreadable: %s" % e)
        return res

    keys = set(sm)
    res["keys"] = len(keys)
    res["missing"] = len(expected - keys)
    res["extra"] = len(keys - expected)
    res["coverage"] = (len(expected & keys) / float(len(expected))) if expected else None

    if total_functions and len(keys) != total_functions:
        res["fails"].append(
            "C1 COUNT: %d keys on disk != report total_functions %d (%+d)"
            % (len(keys), total_functions, len(keys) - total_functions))
    if res["collisions"]:
        res["fails"].append(
            "C2 COLLISION: %d report rows collapse onto a shared address "
            "(%d rows -> %d unique addrs)"
            % (res["collisions"], len(funcs_nodedup), len(expected)))
    if res["missing"] or res["extra"]:
        res["fails"].append(
            "C3 IDENTITY: key set disagrees with this report -- %d expected "
            "addrs absent, %d keys not in the report"
            % (res["missing"], res["extra"]))
    if res["fails"]:
        res["status"] = "fail"
    return res


def _print_artifact_check(res):
    print("=" * 70)
    print("scope_map ON-DISK ARTIFACT -- %s" % res["path"])
    print("=" * 70)
    if res["status"] == "absent":
        print("  NOT PRESENT -- nothing was validated.")
        print("  The address invariants above are recomputed from report.json and")
        print("  hold whether or not this file exists, so they do NOT vouch for it.")
        print("  build it:  python3 tools/scope_map.py build      (~1 s)")
        return
    print("  keys on disk                           %s"
          % ("-" if res["keys"] is None else res["keys"]))
    print("  expected (unique report addrs)         %d" % res["expected"])
    print("  report total_functions                 %d" % res["total_functions"])
    if res["coverage"] is not None:
        print("  coverage of report addrs               %.4f" % res["coverage"])
    if res["missing"] is not None:
        print("  expected addrs absent / extra keys     %d / %d"
              % (res["missing"], res["extra"]))
    print("  address collisions in derivation       %s"
          % ("-" if res["collisions"] is None else res["collisions"]))
    for f in res["fails"]:
        print("    FAIL: %s" % f)
    if not res["fails"]:
        print("  all artifact invariants hold.")


def cmd_validate_addrs(args):
    """Assert every PINNED-unit function address is real, not fabricated.

    Two independent invariants, because the defect this guards against produced
    a confident, well-formed, WRONG answer that no consistency check inside the
    tool could see:

      A. CONTAINMENT -- a row's address must lie inside one of its OWN unit's
         `.text` blocks in splits.txt.  The fabricated key
         `first_block_start + cumulative_offset` violates this for multi-block
         units (11,240 rows before the fix).
      B. SIZE AGREEMENT -- the row's size must equal symbols.txt's size for the
         symbol AT that address.  This is the check that originally exposed the
         bug by byte geometry: scope_map claimed 116 B at 0x822734E0 where
         symbols.txt says 0x3C = 60 B, because 116 B is the size of
         `?SetTypeDef@Object@Hmx@@UAAXPAVDataArray@@@Z`, whose true home is
         0x8275AB18.

    Rows in units with no `.text` pin are counted and skipped (they have no
    blocks to be inside of).

    C. ARTIFACT -- the on-disk scope_map.json must agree with this report.  See
       _artifact_check: A and B are recomputed from report.json and hold
       whether or not that file exists, so they vouch for the DERIVATION and
       not for the CACHE.

    Exit codes, so this can gate:
      0  everything above holds
      1  a row failed, or the on-disk artifact contradicts this report
      3  the artifact is ABSENT -- addresses pass, cache NOT validated.
         Deliberately not 0: "there was nothing to check" and "I checked and it
         is fine" must not be the same signal.  --allow-absent demotes it.

    ⚠⚠ CONTAINMENT UNDERCOUNTS BY ~2x AND ITS "CONTROL" IS NOT ONE.  Measured
    against the true pre-fix addresses (lane SCOPEMAP-VA): of 23,036 pinned
    named rows, **22,090 (95.89%) carried a fabricated address**, but only
    11,240 (48.79%) were outside every block -- the other 10,850 (47.10%) landed
    inside ANOTHER BLOCK OF THE SAME UNIT, where invariant A is structurally
    blind.  Consequently the single-block-vs-multi-block split that sized the
    defect measures **detectability, not defect rate**: single-block units were
    87.53% fabricated while reading only 7.62% "bad", so the apparent ~6-7x
    multi/single enrichment is an ARTIFACT of containment's blind spot, not a
    real difference between the populations.  The instrument that has no blind
    spot is the arithmetic one: after the fix `scope_map.json` holds exactly
    `total_functions` keys (69,226) with ZERO address collisions, where the
    fabricating version lost **650 functions** to colliding synthetic keys.
    Check that identity before trusting this command's counts.
    """
    report_path = args.report
    with open(report_path) as f:
        rep = json.load(f)
    funcs, _ = load_functions(report_path, dedup=False)
    addr_of = {}
    for addr, size, matched, sp, unit, name, fz in funcs:
        addr_of[(unit, name)] = (addr, size)

    a2s = load_symbols_txt_addr2size()
    blocks_by_stem = load_splits_text_blocks()

    n_named = n_contain = n_size = n_size_tol = 0
    no_blocks = no_symrow = 0
    bad_contain, bad_size = [], []
    for u in rep["units"]:
        unit = u["name"]
        sp = (u.get("metadata") or {}).get("source_path")
        if not sp:
            continue
        fns = u.get("functions") or []
        if not fns:
            continue
        anchors = [int(m.group(1), 16)
                   for m in (FN_ADDR_RE.match(f["name"]) for f in fns) if m]
        blks = blocks_for_report_unit(unit, anchors, blocks_by_stem)
        if not blks:
            no_blocks += 1
            continue
        for fn in fns:
            name = fn["name"]
            if FN_ADDR_RE.match(name):
                continue
            got = addr_of.get((unit, name))
            if got is None:
                continue
            addr, size = got
            n_named += 1
            if _addr_in_blocks(addr, blks):
                n_contain += 1
            elif len(bad_contain) < 100000:
                bad_contain.append((unit, name, addr, size, blks[0][0], len(blks)))
            want = a2s.get(addr)
            if want is None:
                no_symrow += 1            # no symbols.txt row at that VA
            elif want == size:
                n_size += 1
            elif abs(want - size) <= SIZE_TOLERANCE:
                # Known systematic one-word accounting gap between report.json's
                # function size and dtk's symbol extent.  MEASURED across all
                # 23,034 checkable rows: delta 0 x22,779, +4 x253, -4 x2, and
                # NOTHING else -- so it is a convention, not corruption, and it
                # predates this lane (which is tooling-only and must not touch
                # symbols.txt).  Tolerated, but COUNTED, never hidden.
                n_size_tol += 1
            elif len(bad_size) < 100000:
                bad_size.append((unit, name, addr, size, want))

    nb_c, nb_s = len(bad_contain), len(bad_size)
    print("=" * 70)
    print("scope_map validate-addrs -- PINNED-unit named functions")
    print("=" * 70)
    print(f"  named rows checked                     {n_named}")
    print(f"  A. inside own unit's .text blocks      {n_contain}   FAIL {nb_c}")
    print(f"  B. size agrees with symbols.txt        {n_size}   FAIL {nb_s}")
    print(f"     (+/-{SIZE_TOLERANCE}B known accounting gap, tolerated) {n_size_tol}")
    print(f"  unresolvable by any ground-truth map   {len(UNRESOLVED_NAMED)}")
    if no_symrow:
        print(f"  no symbols.txt row at resolved VA      {no_symrow}")
    if no_blocks:
        print(f"  units skipped (no .text pin)           {no_blocks}")
    for label, rows in (("CONTAINMENT", bad_contain), ("SIZE", bad_size)):
        for r in rows[: args.samples]:
            print(f"    {label}: {r[2]:08X} {r[0]} {r[1][:60]} {r[3:]}")
    for unit, name in UNRESOLVED_NAMED[: args.samples]:
        print(f"    UNRESOLVED: {unit} {name[:70]}")
    addr_ok = (nb_c == 0 and nb_s == 0 and not UNRESOLVED_NAMED)
    print("\n  address invariants: " + ("PASS" if addr_ok else "FAIL"))

    # ---- the file this command is BELIEVED to validate --------------------
    # int()-coerced: report.json is protobuf-JSON and several numerics arrive as
    # STRINGS, where `!=` against an int is silently true for every value.
    print()
    tf = int((rep.get("measures") or {}).get("total_functions", 0) or 0)
    art = _artifact_check(args.scope_map, funcs, tf)
    _print_artifact_check(art)

    if art["status"] == "absent" and args.allow_absent:
        print("  (--allow-absent: not a failure here, but still NOT validated.)")
        art_absent_rc = 0
    else:
        art_absent_rc = EXIT_ARTIFACT_ABSENT

    if not addr_ok or art["status"] in ("fail", "unreadable"):
        print("\n  VERDICT: FAIL")
        return 1
    if art["status"] == "absent":
        # NOT a PASS: the old code returned 0 here, which is how a DELETED
        # artifact used to produce a confident green.
        print("\n  VERDICT: ADDRESSES PASS / ARTIFACT NOT VALIDATED (absent)")
        return art_absent_rc
    print("\n  VERDICT: PASS")
    return 0


def _synth_fixtures(funcs_nodedup, out_dir):
    """Deterministically synthesise {correct, stale, dead} scope_map fixtures.

    Reproducible on ANY machine from report.json alone -- no preserved incident
    file required.  (One was preserved at ~/tmp/scopemap-policy/bak-stale-68576
    .json, but a fixture that lives in ~/tmp is a fixture that gets swept, and a
    test nobody can re-run is a test that silently stops running.)

    The `stale` fixture reproduces the INCIDENT'S SHAPE, not merely a small
    file: it drops every 3rd address AND injects the same number of fabricated
    ones, so its key COUNT lands within ~1% of correct while a third of its
    addresses are wrong.  That matters -- a fixture that only truncates would be
    caught by C1 alone and would never exercise C3, which is the check that
    actually catches a same-size, wrong-addresses cache (the real file scored
    68,576 keys @ 0.6881 coverage).
    """
    addrs = sorted({f[0] for f in funcs_nodedup})
    size_of = {}
    for a, size, *_ in funcs_nodedup:
        size_of.setdefault(a, size)

    def ent(sz):
        return {"size": sz, "scope": "unknown", "provenance": "selftest",
                "confidence": 0.0, "matched": False}

    correct = {"%08X" % a: ent(size_of[a]) for a in addrs}

    stale = {"%08X" % a: ent(size_of[a])
             for i, a in enumerate(addrs) if i % 3 != 0}
    for j in range(len(addrs) - len(stale)):          # fabricated, not in report
        stale["%08X" % (0xF0000000 + j)] = ent(16)

    dead = {"%08X" % a: ent(size_of[a])
            for i, a in enumerate(addrs) if i % 5 == 0}   # 20% coverage

    paths = {}
    for name, obj in (("correct", correct), ("stale", stale), ("dead", dead)):
        p = os.path.join(out_dir, "scope_map.%s.json" % name)
        with open(p, "w") as f:
            json.dump(obj, f, indent=0, sort_keys=True)
        paths[name] = p
    return paths, {"correct": correct, "stale": stale, "dead": dead}


def cmd_selftest(args):
    """Prove the artifact gate DISCRIMINATES -- it must PASS the correct
    artifact and FAIL the stale one.

    A gate that fails on everything proves nothing, so the correct-artifact
    control is not optional here: it is half the evidence.  Exits 1 if any cell
    disagrees, so this can run in CI as a known-answer test the way
    tools/source_category.py selftest does."""
    import tempfile
    funcs, rep = load_functions(REPORT, dedup=False)
    tf = int((rep.get("measures") or {}).get("total_functions", 0) or 0)
    out_dir = args.keep or tempfile.mkdtemp(prefix="scope_map_selftest_")
    os.makedirs(out_dir, exist_ok=True)
    paths, objs = _synth_fixtures(funcs, out_dir)

    cells, fails = [], 0

    def check(label, got, want):
        nonlocal fails
        ok = got == want
        if not ok:
            fails += 1
        cells.append((label, got, want, ok))

    # --- _cache_status: graded coverage -----------------------------------
    for name, want in (("correct", "ok"), ("stale", "incomplete"), ("dead", "dead")):
        st = _cache_status({a: e["scope"] for a, e in objs[name].items()},
                           funcs, "ok")
        check("_cache_status(%s)" % name, st["state"], want)
    check("_cache_status(missing)",
          _cache_status({}, funcs, "missing")["state"], "missing")

    # --- _artifact_check: the on-disk assertion ----------------------------
    for name, want in (("correct", "ok"), ("stale", "fail"), ("dead", "fail")):
        check("_artifact_check(%s)" % name,
              _artifact_check(paths[name], funcs, tf)["status"], want)
    check("_artifact_check(absent)",
          _artifact_check(os.path.join(out_dir, "nope.json"), funcs, tf)["status"],
          "absent")

    # The control that makes the rest mean something: a correct artifact must
    # resolve EXACTLY every report address, so coverage is 1.0 by construction.
    cov = _artifact_check(paths["correct"], funcs, tf)["coverage"]
    check("correct coverage == 1.0", round(cov, 6), 1.0)

    print("=" * 70)
    print("scope_map selftest -- artifact gate discrimination")
    print("=" * 70)
    print("  fixtures: %s" % out_dir)
    print("  report total_functions %d · unique addrs %d"
          % (tf, len({f[0] for f in funcs})))
    print("  stale fixture: %d keys (%.4f coverage)"
          % (len(objs["stale"]),
             _artifact_check(paths["stale"], funcs, tf)["coverage"]))
    print()
    for label, got, want, ok in cells:
        print("  %-32s %-12s want %-12s %s"
              % (label, got, want, "ok" if ok else "MISMATCH"))
    print("\n  VERDICT: %s (%d/%d)"
          % ("PASS" if not fails else "FAIL", len(cells) - fails, len(cells)))
    if not args.keep:
        import shutil
        shutil.rmtree(out_dir, ignore_errors=True)
    return 1 if fails else 0


def cmd_report(args):
    _print_report(_load_scope_map())


# ---------------------------------------------------------------------------
# progress -- honest progress toward 100% of the WHOLE binary, by priority tier
# ---------------------------------------------------------------------------
# The goal is 100% of the whole binary; ALL of it is matchable (DC3/RB3 started
# from zero source). So there is no "ceiling" -- only sequencing. We report:
#   * matched / whole-binary           the true progress toward 100%
#   * mapped %                         how much is attributed to a TU yet (the
#                                      mapping axis -- prerequisite to matching)
#   * matched% per priority tier       so effort goes oracle-backed-first (cheap)
#   * a near-term focus number         matched / oracle-backed -- explicitly NOT
#                                      a ceiling, just the cheapest slice to do now
# Per-row notes describe the tier's CONTENT (the oracle status now lives in the
# cluster header, so it's no longer repeated per row).
LABELS = {
    "game": "rb3-Wii (HIGH)",
    "engine": "DC3",
    "thirdparty": "public src",
    "crt": "LIBCMT",
    "xdk": "XDK glue",
    "vendor": "MS/RAD libs",
    "unknown": "mapping TODO",
}


def _acc():
    """[fns, bytes, m_fns(normalized), m_bytes(fuzzy==100), fuzzy_bytes]."""
    return [0, 0, 0, 0, 0.0]


# Index names for an _acc() row, so call sites read as prose.
A_FNS, A_BYTES, A_MFNS, A_MBYTES, A_FZBYTES = 0, 1, 2, 3, 4

# AUTOID-1 (2026-08-13) controlled the two-sided spatial inference -- "enclosed by
# the same heading on both sides => membership" -- at 66.24% precision, i.e. a
# 33.76% FALSE-POSITIVE rate. The one-sided after-/before- form is weaker still,
# so this is a floor on the error, not an estimate of it.
GUESS_FP_RATE = 0.3376


def _is_guess(prov):
    """Provenance that is an ADJACENCY or NAME-CLASS inference about the function
    rather than evidence about it. `spatial:*` inherits a neighbour's tier across
    an unlabeled .text run; `name:`/`name-prefix:` route a mangled class token.
    Both attribute a tier without any per-function proof, and the spatial form's
    measured precision is 66.24% (GUESS_FP_RATE). Everything else -- pinned
    source, an address-keyed provenance label, a thirdparty range, an xdk import
    address -- rests on evidence about that specific address."""
    return prov.startswith("spatial:") or prov.startswith("name:") \
        or prov.startswith("name-prefix:")


def _by_live(scope_by_addr, prov_by_addr, funcs):
    """Per-bucket aggregate over EVERY report fn (funcs must be dedup=False),
    classified by the cached map (real addr) with a name/source fallback for
    catch-all fns the build's dedup dropped. Because no fn is dropped,
    sum(bytes) == report total_code and sum(fns) == total_functions EXACTLY.

    Returns {bucket: {"pin": _acc(), "unp": _acc(), "guess": [fns, bytes]}}.

    ★ THE SPLIT IS THE POINT (lane SCOPEDEN-1). A tier's denominator is TWO
    populations that must never be summed into one unqualified percentage:

      pin  PINNED -- the report unit declares a `source_path`, so we compile it,
           so it has a base object, so its rows CAN pair. This is the "how is our
           SOURCE doing" denominator, and it is the number people remember.
      unp  UNPINNED -- attributed to this tier by the map (spatial adjacency,
           name class, provenance label, thirdparty range) but with NO compiled
           base object, so every row is structurally unmatchable TODAY. It is
           real binary that someone must eventually match, so it stays in the
           whole-tier denominator -- deleting it would be the ForceEmit_* disease
           of shrinking the denominator until the number looks good.

    Measured on b574f653: `sp` truthy reproduces NOOBJ-1's PAIRABLE census
    EXACTLY -- 1,045 units / 6,512,524 B pairable vs 3,808,140 B not -- so this
    split is the 63.10% reachable ceiling, localized per tier. It is also
    CACHE-INDEPENDENT (it reads report.json, not scope_map.json), so it stays
    correct even under the stale-cache banner.

    `guess` counts the UNPINNED subset whose tier came from an adjacency/name
    inference (see _is_guess) -- the slice carrying AUTOID-1's 33.76% FP rate.
    """
    # m_fns uses normalized (sums to measures.matched_functions); m_bytes uses the
    # raw fuzzy_match_percent==100 predicate (sums to measures.matched_code) -- the
    # two predicates objdiff itself uses, so each axis reconciles with ninja's All.
    # fuzzy_bytes / bytes == the tier's size-weighted mean fuzzy%.
    by = defaultdict(lambda: {"pin": _acc(), "unp": _acc(), "guess": [0, 0]})
    engine_cls, game_cls, vendor_cls = load_name_class(WN_DATA)
    for addr, size, matched, sp, unit, name, fz in funcs:
        key = "%08X" % addr
        sc = scope_by_addr.get(key)
        if sc is None:                # not in cache (dropped by build dedup / new)
            sc = bucket_for_source(sp)
            if sc is None:            # catch-all named fn -> try class-by-name
                sc = classify_name(name, engine_cls, game_cls, vendor_cls)[0]
            sc = sc or "unknown"
        rec = by[sc]
        pinned = bool(sp)
        a = rec["pin"] if pinned else rec["unp"]
        a[A_FNS] += 1
        a[A_BYTES] += size
        if matched:
            a[A_MFNS] += 1
        if fz >= 100.0:
            a[A_MBYTES] += size
        a[A_FZBYTES] += size * fz / 100.0
        if not pinned:
            # A fn absent from the cache reached its tier via classify_name above,
            # which IS the name-class guess -- count it as one.
            prov = prov_by_addr.get(key)
            if prov is None or _is_guess(prov):
                rec["guess"][0] += 1
                rec["guess"][1] += size
    for b in BUCKET_ORDER:            # ensure every bucket key exists
        by[b]
    return by


_HISTORY_CAP = 4000            # entries kept; ~1 per landing, so years of headroom
_WINDOW_DAYS = 7               # trailing window for the second delta line


def _fmt_ago(seconds):
    """Compact human age: 45s / 12m / 3h / 5d."""
    s = int(max(0, seconds))
    if s < 90:
        return "%ds" % s
    if s < 5400:
        return "%dm" % (s // 60)
    if s < 172800:
        return "%dh" % (s // 3600)
    return "%dd" % (s // 86400)


def _delta_str(cur, base):
    d_fns = cur["mf"] - base.get("mf", cur["mf"])
    cur_mp = 100.0 * cur["mc"] / cur["tc"] if cur["tc"] else 0.0
    base_mp = 100.0 * base.get("mc", 0) / base["tc"] if base.get("tc") else 0.0
    d_fz = cur["fz"] - base.get("fz", cur["fz"])

    # ---- cross-ruler guard -------------------------------------------------
    # matched_code is RULER-DEPENDENT: the 2026-08-12 functionRelocDiffs
    # none -> name_check flip (d04c83df) moved matched_code by -1,144,956 B
    # (-11.09 pp) with ZERO source change, while matched_functions moved +0.
    # Subtracting a `none` absolute from a `name_check` one therefore fabricates
    # a large regression that never happened -- this dashboard reported
    # "-6.14% matched · 7d" on a tree whose matched_functions had risen +104.
    # matched_functions is ruler-INVARIANT and fuzzy% was measured unchanged
    # across the flip, so those two stay comparable; only matched% is withheld.
    # (ab_measure.py already refuses cross-ruler comparisons; this is the same
    # guard for the history path, which had none.)
    a, b = cur.get("rk"), base.get("rk")
    if a and b and a != b:
        return ("%+d fns · matched%% n/a (ruler %s→%s) · %+.2f%% fuzzy"
                % (d_fns, b, a, d_fz))
    if not b and a:
        # Pre-guard history carries no ruler tag. It may or may not straddle the
        # flip, and we cannot tell -- so say so rather than assert a number.
        return "%+d fns · matched%% n/a (untagged) · %+.2f%% fuzzy" % (d_fns, d_fz)
    return "%+d fns · %+.2f%% matched · %+.2f%% fuzzy" % (d_fns, cur_mp - base_mp, d_fz)


def _ruler_key(rep):
    """Short tag for the diff ruler a report.json was scored on.

    report.json SELF-DECLARES this in its provenance block -- read it, never
    infer it from a date. Absolutes are only comparable within one ruler."""
    try:
        cfg = rep.get("provenance", {}).get("diff_config") or []
        for item in cfg:
            if str(item).startswith("functionRelocDiffs="):
                return str(item).split("=", 1)[1]
    except Exception:
        pass
    return None


def _progress_delta(off_mc, off_tc, off_mf, fuzzy_pct, ruler=None):
    """Append-only rolling progress history; return (moved_line, window_line).

    Replaces the old per-calendar-day baseline, which reset at midnight and so
    read '+0' for every build of a session that began the previous day — the
    common case here, since landings routinely straddle midnight.

    A record is appended ONLY when the totals actually change, so 'moved' is the
    delta of the last real movement (with its age), not the delta since some
    arbitrary clock boundary. The second line is the trailing-window total.

    History lives in the private, per-worktree build dir, so it never collides
    across worktrees and is never committed.
    Robust by design: any error -> ('', '') (this must never break a build)."""
    path = os.path.join(ROOT, "build", "45410914", "progress_history.jsonl")
    try:
        now = datetime.datetime.now()
        cur = {"t": now.isoformat(timespec="seconds"), "mc": off_mc, "tc": off_tc,
               "mf": off_mf, "fz": fuzzy_pct}
        if ruler:
            cur["rk"] = ruler          # tag every entry; see _delta_str's guard
    except Exception:
        return "", ""

    hist = []
    try:
        with open(path) as f:
            for ln in f:
                ln = ln.strip()
                if not ln:
                    continue
                try:
                    hist.append(json.loads(ln))
                except json.JSONDecodeError:
                    continue          # tolerate a torn line from a killed build
    except (FileNotFoundError, OSError):
        pass

    def totals(r):
        # rk is part of the identity: a ruler change with unchanged totals is
        # still a new measurement basis and must be recorded, or the next delta
        # silently compares across it.
        return (r.get("mc"), r.get("tc"), r.get("mf"), r.get("rk"))

    changed = (not hist) or totals(hist[-1]) != totals(cur)
    if changed:
        try:
            with open(path, "a") as f:
                f.write(json.dumps(cur) + "\n")
        except OSError:
            pass
        if len(hist) + 1 > _HISTORY_CAP:                 # trim oldest, keep it bounded
            try:
                keep = (hist + [cur])[-_HISTORY_CAP:]
                with open(path, "w") as f:
                    for r in keep:
                        f.write(json.dumps(r) + "\n")
            except OSError:
                pass
        hist = hist + [cur]

    # 'moved' = the two most recent DISTINCT states. When this build changed
    # nothing, that is still the last real landing rather than a zero.
    moved = ""
    if len(hist) >= 2:
        prev, last = hist[-2], hist[-1]
        try:
            age = (now - datetime.datetime.fromisoformat(last["t"])).total_seconds()
            ago = " (%s ago)" % _fmt_ago(age)
        except Exception:
            ago = ""
        moved = _delta_str(last, prev) + ago

    # trailing window vs the oldest record still inside it
    window = ""
    try:
        cutoff = now - datetime.timedelta(days=_WINDOW_DAYS)
        older = [r for r in hist
                 if datetime.datetime.fromisoformat(r["t"]) <= cutoff]
        base = older[-1] if older else (hist[0] if hist else None)
        if base is not None and totals(base) != totals(cur):
            window = _delta_str(cur, base) + "  · %dd" % _WINDOW_DAYS
    except Exception:
        window = ""

    return moved, window


# Tiers shown as their own row -- the oracle-backed movers. crt (oracle, ~0.01 MB)
# folds into the oracle-backed footer total; xdk/vendor into no-oracle; unknown into
# the mapped footer. So the footer reconciles every byte while the table stays short.
TIER_ROWS = ["game", "engine", "thirdparty"]


def _bar(p, width=42):
    """A fixed-width unicode progress bar, 0..100% -> full=100%. Uses eighth-block
    partials so even single-digit percentages render a visible sliver; the unfilled
    track is drawn with light shade so the bar reads as a gauge."""
    p = max(0.0, min(100.0, p))
    units = p / 100.0 * width
    full = int(units)
    s = "█" * full
    used = full
    if used < width:
        idx = int(round((units - full) * 8))
        if idx > 0:
            s += "▏▎▍▌▋▊▉█"[idx - 1]
            used += 1
    return s + "░" * (width - used)


# Banner copy for each unhealthy cache state: (headline, why-line).
# All of them mean the same thing operationally -- the map could not classify the
# anonymous fn_8XXXXXXX bulk, so tier denominators are PINNED-ONLY and inflated.
_CACHE_BANNER = {
    "missing": ("SCOPE CACHE MISSING — EVERY % BELOW IS INFLATED",
                "scope_map.json is absent (fresh checkout / worktree)."),
    "unreadable": ("SCOPE CACHE UNREADABLE — EVERY % BELOW IS INFLATED",
                   "scope_map.json is present but corrupt / truncated."),
    "dead": ("SCOPE CACHE STALE — EVERY % BELOW IS INFLATED",
             "scope_map.json is addr-keyed to a DIFFERENT target build."),
}


def print_progress(by, measures, cache=None, compact=False):
    """The single at-a-glance decomp dashboard ninja prints after every build.

    Headline reads report.json's official `measures` (== ninja "All"). Everything
    below it is per-tier, and after lane SCOPEDEN-1 every per-tier percentage
    names BOTH of the two things that were previously left implicit:

    WHICH FIELD (they are different measures, not two views of one):
      aon%   ALL-OR-NOTHING byte share -- Sigma size of rows at fuzzy==100 over the
             denominator. This is what `matched_code` keys on; a row at 99.9%
             contributes ZERO.
      mean%  SIZE-WEIGHTED MEAN of fuzzy_match_percent over the same rows. A
             work-in-progress indicator; always >= aon%.

    WHICH DENOMINATOR (see _by_live):
      pinned  units we compile (have a base object, so their rows can pair) --
              "how is our SOURCE doing", and the number people remember.
      all-in  pinned + unpinned mass the map attributes to this tier but which has
              no base object and therefore cannot pair today.

    Both rows are printed for every tier, deliberately: three defensible "game
    fuzzy" numbers coexisted on one tree (82.07 band3-pinned / 74.68
    band3+network-pinned / 58.78 all-in) and a single unqualified column called
    "fuzzy" is what let a denominator change read as "scope_map build totally
    broke our matches" when the numerator had not moved at all.

    `cache` is the _cache_status() record. When it is not `ok` the tier
    denominators collapse to pinned-only coverage and every all-in percentage
    reads HIGH, so we shout about it instead of silently reporting a number that
    is not comparable to main's. The binary/headline line is always honest (it
    comes straight from report.json) and is unaffected."""
    def mb(x):
        return x / 1048576.0

    def pct(n, d):
        return 100.0 * n / d if d else 0.0

    def mi(k):
        v = measures.get(k, 0)
        return int(v) if isinstance(v, str) else v

    def tot(b, i):
        return by[b]["pin"][i] + by[b]["unp"][i]

    def s_pin(bs, i):
        return sum(by[b]["pin"][i] for b in bs)

    def s_tot(bs, i):
        return sum(tot(b, i) for b in bs)

    off_mc, off_tc = mi("matched_code"), mi("total_code")
    off_mf, off_tf = mi("matched_functions"), mi("total_functions")
    fuzzy_pct = float(measures.get("fuzzy_match_percent", 0.0) or 0.0)

    tot_b = s_tot(by.keys(), A_BYTES)                # == off_tc (no-dedup)
    orac_b = s_tot(ORACLE_BACKED, A_BYTES)
    orac_mb = s_tot(ORACLE_BACKED, A_MBYTES)         # raw matched bytes
    orac_fzb = s_tot(ORACLE_BACKED, A_FZBYTES)       # fuzzy-weighted bytes
    orac_pb = s_pin(ORACLE_BACKED, A_BYTES)          # PINNED denominator
    orac_pmb = s_pin(ORACLE_BACKED, A_MBYTES)
    orac_pfzb = s_pin(ORACLE_BACKED, A_FZBYTES)
    noora_b = s_tot(NO_ORACLE, A_BYTES)
    noora_mb = s_tot(NO_ORACLE, A_MBYTES)
    unk_b = tot("unknown", A_BYTES)
    mapped_b = (tot_b - unk_b)
    # Unpinned mass folded into the oracle-backed tiers, and the guessed subset
    # of it -- the two numbers the reader needs to discount an all-in row.
    orac_ub = sum(by[b]["unp"][A_BYTES] for b in ORACLE_BACKED)
    orac_umb = sum(by[b]["unp"][A_MBYTES] for b in ORACLE_BACKED)
    orac_gb = sum(by[b]["guess"][1] for b in ORACLE_BACKED)
    delta, window = _progress_delta(off_mc, off_tc, off_mf, fuzzy_pct,
                                    ruler=(cache or {}).get("ruler"))

    cache = cache or {"state": "ok"}
    cstate = cache.get("state", "ok")
    cbad = cstate in _CACHE_BANNER
    # Stale-but-live cache: boundaries age even though the matched overlay doesn't.
    # TWO independent reasons to advise a rebuild, and the COVERAGE one is the
    # load-bearing half: mtime is a proxy that a `setup_worktree.sh` reflink or a
    # `touch` resets, whereas an unresolved report address is direct evidence
    # that this cache was not built from this report.
    age = cache.get("age_days")
    cincomplete = (not cbad) and cstate == "incomplete"
    cstale = cincomplete or ((not cbad) and age is not None
                             and age >= CACHE_STALE_DAYS)

    out = []
    if compact:
        # Even the one-liner names both denominators: quoting the oracle number
        # without saying which one is exactly the failure this tool exists to
        # prevent.
        out.append("Decomp %.2f%% matched · %.2f%% fuzzy · %d/%d fns%s"
                   "  |  oracle-backed pinned %.2f MB @ aon %.2f%% / mean %.2f%%"
                   " · all-in %.2f MB @ aon %.2f%% / mean %.2f%%"
                   " · %.0f%% tier-classified%s" %
                   (pct(off_mc, off_tc), fuzzy_pct, off_mf, off_tf,
                    "  (moved " + delta + ")" if delta else "",
                    mb(orac_pb), pct(orac_pmb, orac_pb), pct(orac_pfzb, orac_pb),
                    mb(orac_b), pct(orac_mb, orac_b), pct(orac_fzb, orac_b),
                    pct(mapped_b, tot_b),
                    ("  [!! scope cache %s — tier %% INFLATED, run: python3 tools/scope_map.py build]"
                     % cstate) if cbad else
                    ("  [scope cache INCOMPLETE: %d fns unclassified — run: "
                     "python3 tools/scope_map.py build]"
                     % (cache.get("unresolved") or 0)) if cincomplete else ""))
    else:
        # ---- framed dashboard --------------------------------------------------
        IW = 66                                  # inner width between the │ borders
        def edge(l, r):
            return l + "─" * IW + r
        def rule(label):                         # ├── label ───────┤ section break
            seg = "── " + label + " "
            return "├" + seg + "─" * max(0, IW - len(seg)) + "┤"
        def line(content=""):
            # Clamp, don't just pad. Any over-long content (a delta string with
            # a wide age suffix, a long ruler name) used to push the right border
            # out and visibly break the box -- silently, and only for some
            # values, which is the worst kind of layout bug to chase.
            if len(content) > IW:
                content = content[:IW - 1] + "…"
            return "│" + content.ljust(IW) + "│"

        # tier · WHICH DENOMINATOR · aon% · mean% · fns m/t · MB m/t
        # 3 + 11 + 8 + 8 + 8 + 15 + 13 == 66 == IW exactly. If you widen a column,
        # take the width from another one: line() CLAMPS to IW with an ellipsis, so
        # an over-wide row silently loses its right-hand number instead of breaking
        # the box visibly.
        cols = "   %-11s%-8s%8s%8s%15s%13s"

        def _row(nm, which, a, byt, fns):
            return line(cols % (
                nm, which,
                ("%.2f%%" % pct(a[A_MBYTES], byt)) if byt else "—",
                ("%.2f%%" % pct(a[A_FZBYTES], byt)) if byt else "—",
                "%d / %d" % (a[A_MFNS], fns),
                "%.2f / %.2f" % (mb(a[A_MBYTES]), mb(byt))))

        def tier_lines(b):
            """TWO rows per tier, one per denominator -- see print_progress's
            docstring for why a single unqualified row is the defect.

            Deliberately RIGID: a tier with zero pinned bytes still prints its
            em-dash `pinned` row rather than collapsing to one line. "This entire
            tier has no source at all" is precisely what a reader must not be able
            to miss, and a layout whose shape changes with the data cannot be
            compared build-to-build.

            The two rows share a numerator BY CONSTRUCTION (an unpinned row has no
            base object, so it cannot pair), which makes the pair self-evidencing:
            the reader watches the denominator grow while `MB m` stands still. If
            an unpinned row ever does match, the numerators separate on screen and
            the claim visibly fails instead of being silently wrong."""
            p, u = by[b]["pin"], by[b]["unp"]
            allin = [p[i] + u[i] for i in range(len(p))]
            return [_row(b, "pinned", p, p[A_BYTES], p[A_FNS]),
                    _row("", "all-in", allin, allin[A_BYTES], allin[A_FNS])]

        out.append(edge("╭", "╮"))
        out.append(line("  RB3-XENON · decomp dashboard"))
        out.append(line())
        out.append(line("  binary   %.2f%% matched · %.2f%% fuzzy" %
                        (pct(off_mc, off_tc), fuzzy_pct)))
        out.append(line("           %.2f / %.2f MB · %d / %d fns" %
                        (mb(off_mc), mb(off_tc), off_mf, off_tf)))
        if delta:
            out.append(line("  moved    %s" % delta))
        if window:
            out.append(line("  trend    %s" % window))
        out.append(line())

        # ---- scope-cache health banner ------------------------------------
        # Sits ABOVE the tier block it invalidates, so nobody can read an
        # inflated CORE GOALS number without first reading why it is wrong.
        if cbad:
            head, why = _CACHE_BANNER[cstate]
            out.append(rule("!!  " + head))
            out.append(line())
            out.append(line("   %s" % why))
            out.append(line("   Anonymous fn_8XXXXXXX functions cannot be classified, so"))
            out.append(line("   tier DENOMINATORS are pinned-only and every % below reads"))
            out.append(line("   HIGH.  NOT comparable to main / to any other worktree."))
            out.append(line())
            out.append(line("   fix:  python3 tools/scope_map.py build      (~1 s)"))
            if cache.get("coverage") is not None:
                out.append(line("   (cache dated %s resolves only %.1f%% of report addrs)"
                                % (cache.get("mtime") or "?", 100.0 * cache["coverage"])))
            out.append(line())

        # Core goals: oracle-backed (the cheap near-term work; crt folds into the
        # MB subtotal rather than spending a row). The BARS are drawn on the
        # PINNED denominator -- the source-progress question -- and the rule names
        # that denominator so the bar can never be read against the other one.
        out.append(rule("CORE GOALS — oracle-backed · pinned %.2f / %.2f MB" %
                        (mb(orac_pmb), mb(orac_pb))))
        out.append(line())
        out.append(line("   %-8s%s  %6.2f%%" % ("aon", _bar(pct(orac_pmb, orac_pb)), pct(orac_pmb, orac_pb))))
        out.append(line("   %-8s%s  %6.2f%%" % ("mean", _bar(pct(orac_pfzb, orac_pb)), pct(orac_pfzb, orac_pb))))
        out.append(line())
        out.append(line("   aon% = bytes in rows at fuzzy==100 (what matched_code counts)"))
        out.append(line("   mean% = size-weighted mean fuzzy.  Never quote one without"))
        out.append(line("   saying WHICH FIELD and against WHICH DENOMINATOR:"))
        out.append(line(cols % ("", "", "aon%", "mean%", "fns m/t", "MB m/t")))
        for b in TIER_ROWS:
            out.extend(tier_lines(b))
        out.append(line())

        # The all-in rows above are only honest if the reader is told what was
        # added to reach them, in the same currency (bytes) as the table.
        out.append(line("   all-in adds %.2f MB UNPINNED (no base obj ⇒ cannot pair"
                        % mb(orac_ub)))
        out.append(line("   today); it moved the numerator by %.2f MB." % mb(orac_umb)))
        out.append(line("   %.2f MB of that is adjacency/name GUESS at 66.24%%"
                        % mb(orac_gb)))
        out.append(line("   precision (AUTOID-1) ⇒ ~%.2f MB sits in the WRONG tier."
                        % mb(orac_gb * GUESS_FP_RATE)))
        out.append(line())

        # Lower priority: no-oracle (matchable too, just deferred), largest first.
        out.append(rule("lower priority — no oracle · %.2f / %.2f MB all-in" %
                        (mb(noora_mb), mb(noora_b))))
        out.append(line())
        for b in sorted(NO_ORACLE, key=lambda x: -tot(x, A_BYTES)):
            out.extend(tier_lines(b))
        out.append(line())

        # NB: "tier-classified" != the dtk/build box's "mapped". That one counts
        # bytes PINNED to a splits.txt unit; this counts bytes attributed to a
        # scope TIER by any of the 8 classification layers, pinned or not.
        out.append(rule("%.0f%% of binary tier-classified · %.2f MB unclassified" %
                        (pct(mapped_b, tot_b), mb(unk_b))))
        if cstale:
            if cincomplete:
                out.append(line("   scope cache INCOMPLETE — %d of %d report fns "
                                "unclassified" % (cache.get("unresolved") or 0,
                                                  off_tf)))
                out.append(line("   (%.2f%% coverage — a correct cache resolves "
                                "100.00%%)"
                                % (100.0 * (cache.get("coverage") or 0.0))))
            else:
                out.append(line("   scope cache %s (%dd old) — tier bounds may drift;"
                                % (cache.get("mtime") or "?", age)))
            out.append(line("   refresh: python3 tools/scope_map.py build"))
            out.append(edge("╰", "╯"))
        else:
            out.append(edge("╰", "╯"))

    text = "\n".join(out)
    print(text)
    # Mirror to the GitHub Actions job summary when running in CI (best-effort).
    sp = os.getenv("GITHUB_STEP_SUMMARY")
    if sp:
        try:
            with open(sp, "a", encoding="utf-8") as sf:
                sf.write("```\n" + text + "\n```\n")
        except OSError:
            pass


# A cache whose addresses resolve for fewer than this share of report functions
# is treated as DEAD (keyed to a different target build, e.g. a pre-TU5 map):
# behaviourally identical to no cache at all, so it gets the same loud banner.
CACHE_DEAD_COVERAGE = 0.50
# ⛔ 0.50 IS THE *FLOOR*, NOT THE HEALTH BAR, AND THE 31 POINTS BETWEEN THEM WERE
# A BLIND SPOT.  A correct cache is built from the SAME report.json the dashboard
# reads, so every report address resolves BY CONSTRUCTION: coverage is EXACTLY
# 1.0000, never 0.99.  The dead-only test therefore bought nothing on the way
# down -- the fabricating cache that caused this lane scored **0.6881 and
# reported `ok`, with no banner at all**, because 0.6881 > 0.50.  Graded now:
# anything short of full resolution says REBUILD.  Kept as a separate constant
# (not inlined as `< 1.0`) so the exactness is a stated invariant rather than a
# float literal someone later "fixes" into a tolerance.
CACHE_FRESH_COVERAGE = 1.0
# Age (days) past which the cache gets a one-line staleness footer. The matched
# overlay is always live (it comes from report.json); what ages is the TIER
# BOUNDARIES baked into the map.
CACHE_STALE_DAYS = 14


def _cache_status(scope_by_addr, funcs, state):
    """Describe the health of the cached scope map for the dashboard.

    Returns {state, path, age_days, mtime (ISO date or None), coverage (0..1 or
    None), unresolved (int or None)}. `state` is one of:
      ok        -- present, readable, resolves EVERY report addr (coverage 1.0)
      incomplete-- present and live, but some report addrs are unclassified;
                   tier denominators are short by that much -> REBUILD ADVISED
      missing   -- scope_map.json absent (fresh checkout / un-primed worktree)
      unreadable-- present but truncated/corrupt JSON
      dead      -- present but addr-keyed to a DIFFERENT target build
    `missing`/`unreadable`/`dead` mean the tier denominators are pinned-only ->
    INFLATED, and get the loud banner.  `incomplete` is deliberately the quieter
    advisory channel (same footer as age-staleness): it is a partial cache, not
    an absent one, and a legitimate upstream change that adds functions should
    print "rebuild", not shout that every number is wrong.
    Never raises: dashboard output must not be able to break a build."""
    st = {"state": state, "path": os.path.relpath(SCOPE_MAP, ROOT),
          "age_days": None, "mtime": None, "coverage": None, "unresolved": None}
    try:
        mt = os.path.getmtime(SCOPE_MAP)
        d = datetime.date.fromtimestamp(mt)
        st["mtime"] = d.isoformat()
        st["age_days"] = (datetime.date.today() - d).days
    except (OSError, OverflowError, ValueError):
        pass
    if state == "ok" and funcs:
        hit = sum(1 for f in funcs if ("%08X" % f[0]) in scope_by_addr)
        st["coverage"] = hit / float(len(funcs))
        st["unresolved"] = len(funcs) - hit
        if st["coverage"] < CACHE_DEAD_COVERAGE:
            st["state"] = "dead"
        elif st["coverage"] < CACHE_FRESH_COVERAGE:
            st["state"] = "incomplete"
    return st


def _progress_by_live():
    """Fresh per-bucket aggregate (every fn, no dedup) over the cached map, plus
    report.json's official `measures` for the authoritative headline, plus a
    health record for the cache itself (see _cache_status).

    The old per-tier `inf` column (share of a tier's FUNCTION COUNT carrying
    `spatial:*` provenance, computed separately over the dedup'd cache) is gone.
    It reported the right hazard on the wrong axis and from a second arithmetic:
    bytes are the currency every other number here is in, and the reader still
    could not recover the pinned-only percentage from a share. `_by_live` now
    carries the pinned/unpinned byte split and the guess subset in ONE pass over
    the same rows, so the disclosure and the percentages cannot disagree."""
    funcs, rep = load_functions(REPORT, dedup=False)
    state = "ok"
    try:
        sm = _load_scope_map()
        scope_by_addr = {a: e["scope"] for a, e in sm.items()}
        prov_by_addr = {a: (e.get("provenance") or "") for a, e in sm.items()}
    except FileNotFoundError:
        scope_by_addr, prov_by_addr, state = {}, {}, "missing"
    except (json.JSONDecodeError, OSError, KeyError, TypeError):
        scope_by_addr, prov_by_addr, state = {}, {}, "unreadable"
    cache = _cache_status(scope_by_addr, funcs, state)
    cache["ruler"] = _ruler_key(rep)      # carried through to the delta guard
    return _by_live(scope_by_addr, prov_by_addr, funcs), rep.get("measures", {}), cache


def cmd_priority(args):
    by, measures, cache = _progress_by_live()
    print_progress(by, measures, cache=cache, compact=args.compact)


# ---------------------------------------------------------------------------
# worklist -- size-ranked largest UNMATCHED in-scope clusters
# ---------------------------------------------------------------------------
def cmd_worklist(args):
    sm = _load_scope_map()
    items = sorted((int(a, 16), e) for a, e in sm.items())
    # Build contiguous clusters of same-provenance unmatched in-scope functions.
    # A cluster groups adjacent functions that share a normalized provenance key
    # (pinned source file, thirdparty lib, engine/game src_file, or spatial tag)
    # and are NOT matched and ARE in-scope.
    def prov_key(e):
        p = e["provenance"] or ""
        # collapse to the meaningful identity
        if p.startswith("pinned:"):
            return p
        if p.startswith("thirdparty.json:"):
            return p
        if p.startswith("engine.json:") or p.startswith("game.json:"):
            return p.split("|")[0]
        return "spatial:" + e["scope"]

    clusters = []  # (start, end, scope, prov, n_unmatched, unmatched_bytes, total_bytes)
    cur = None
    for addr, e in items:
        scope = e["scope"]
        in_scope = scope in ORACLE_BACKED   # oracle-backed = the cheap near-term work
        unmatched = not e["matched"]
        key = (scope, prov_key(e)) if in_scope else None
        if in_scope and unmatched and cur and cur["key"] == key:
            cur["end"] = addr + e["size"]
            cur["n"] += 1
            cur["umb"] += e["size"]
        else:
            if cur:
                clusters.append(cur)
            if in_scope and unmatched:
                cur = {"key": key, "scope": scope, "prov": prov_key(e),
                       "start": addr, "end": addr + e["size"], "n": 1,
                       "umb": e["size"]}
            else:
                cur = None
    if cur:
        clusters.append(cur)

    clusters.sort(key=lambda c: c["umb"], reverse=True)
    limit = args.limit or 50
    bucket_filter = args.bucket
    print("=== PRIORITY WORKLIST: unmatched oracle-backed clusters (size-ranked) ===")
    print("(the cheapest near-term work; no-oracle xdk/vendor is matchable too, just lower prio)")
    print("%-10s %-10s %6s %9s  %-9s %s" %
          ("start", "end", "fns", "bytes", "bucket", "provenance"))
    shown = 0
    for c in clusters:
        if bucket_filter and c["scope"] != bucket_filter:
            continue
        prov = c["prov"]
        if prov.startswith("pinned:"):
            prov = prov[len("pinned:"):]
        print("0x%08X 0x%08X %6d %9d  %-9s %s" %
              (c["start"], c["end"], c["n"], c["umb"], c["scope"], prov))
        shown += 1
        if shown >= limit:
            break


# ---------------------------------------------------------------------------
# classify <addr>
# ---------------------------------------------------------------------------
def cmd_classify(args):
    sm = _load_scope_map()
    a = args.addr
    if a.lower().startswith("0x"):
        a = a[2:]
    key = ("%08X" % int(a, 16))
    e = sm.get(key)
    if not e:
        print("addr %s not in scope_map" % key)
        # nearest neighbor
        addrs = sorted(int(x, 16) for x in sm)
        target = int(a, 16)
        import bisect
        i = bisect.bisect_left(addrs, target)
        for j in (i - 1, i):
            if 0 <= j < len(addrs):
                nk = "%08X" % addrs[j]
                print("  nearest 0x%s -> %s" % (nk, json.dumps(sm[nk])))
        return
    print("0x%s -> %s" % (key, json.dumps(e, indent=2)))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build", help="classify all fns, write scope_map.json")
    b.add_argument("--quiet", action="store_true",
                   help="one line of output; no RECLASSIFIED diff, no summary "
                        "table (for the automated PROGRESS refresh)")
    sub.add_parser("report", help="per-bucket breakdown + progress toward 100%%")
    pr = sub.add_parser("priority", help="progress toward 100%% by priority tier (live matched, cached classification)")
    pr.add_argument("--compact", action="store_true", help="one-line form (for the build PROGRESS step)")
    w = sub.add_parser("worklist", help="size-ranked unmatched oracle-backed clusters (cheapest work)")
    w.add_argument("--limit", type=int, default=50)
    w.add_argument("--bucket", choices=BUCKET_ORDER, help="filter to one bucket")
    c = sub.add_parser("classify", help="classify a single addr")
    c.add_argument("addr")
    v = sub.add_parser("validate-addrs",
                       help="assert pinned-unit fn addresses are real, not fabricated")
    v.add_argument("--report",
                   default=os.path.join(ROOT, "build", "45410914", "report.json"))
    v.add_argument("--samples", type=int, default=10)
    v.add_argument("--scope-map", default=SCOPE_MAP,
                   help="artifact to validate (default: %s). Exists so the "
                        "gate can be pointed at a fixture and PROVED to fail."
                        % os.path.relpath(SCOPE_MAP, ROOT))
    v.add_argument("--allow-absent", action="store_true",
                   help="a missing artifact exits 0 instead of %d (for hosts "
                        "that legitimately never build it)" % EXIT_ARTIFACT_ABSENT)
    stp = sub.add_parser("selftest",
                         help="prove the artifact gate DISCRIMINATES (synthetic "
                              "stale fixtures + a correct-artifact control)")
    stp.add_argument("--keep", metavar="DIR",
                     help="write the fixtures here and leave them in place")
    args = ap.parse_args()
    rc = {"build": cmd_build, "report": cmd_report, "priority": cmd_priority,
          "worklist": cmd_worklist, "classify": cmd_classify,
          "validate-addrs": cmd_validate_addrs,
          "selftest": cmd_selftest}[args.cmd](args)
    sys.exit(rc or 0)


if __name__ == "__main__":
    main()
