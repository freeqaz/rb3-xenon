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


def bucket_for_source(sp):
    """Map a pinned unit source_path to a scope bucket. Returns None if unknown."""
    if not sp:
        return None
    low = sp.lower()
    # third-party libs (some live UNDER src/system/, so test before engine)
    for m in THIRDPARTY_MARKERS:
        if m in low:
            return "thirdparty"
    # CRT we compile ourselves
    if "/xdk/libcmt/" in low:
        return "crt"
    # XDK middleware glue we ship (nuiapi etc.) -- out of scope
    if low.startswith("src/xdk/") or "/xdk/nuiapi/" in low:
        return "xdk"
    # game layer
    if low.startswith("src/band3/") or low.startswith("src/network/"):
        return "game"
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
        # dtk emits clean per-unit relative offsets, so base + rel is exact.
        # Base from any fn_ anchor (base = abs - rel), else splits.txt .text
        # pin, else a synthetic island (handful of .pdata-only pinned units).
        if sp:
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
                m = FN_ADDR_RE.match(fn["name"])
                addr = int(m.group(1), 16) if m else base + int(fn.get("address", "0"))
                size = int(fn.get("size", "0"))
                matched = float(fn.get("match_percent_normalized", 0.0)) >= 100.0
                fz = float(fn.get("fuzzy_match_percent", 0.0))
                funcs.append((addr, size, matched, sp, unit, fn["name"], fz))
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
        tsm_name2addr = load_target_symbol_name2addr()
        symtxt_name2addr = load_symbols_txt_name2addr()
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
                real_addr = tsm_name2addr.get(name)
                if real_addr is None:
                    real_addr = symtxt_name2addr.get(name)
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
    with open(SCOPE_MAP, "w") as f:
        json.dump(out, f, indent=0, sort_keys=True)
    print("wrote %s (%d functions)" % (SCOPE_MAP, len(out)))
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
    print("%-12s %8s %12s %8s %12s  %-9s" %
          ("bucket", "fns", "bytes", "m_fns", "m_bytes", "tier"))
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


def _by_live(scope_by_addr, funcs):
    """Per-bucket aggregate [fns, bytes, m_fns, m_bytes, fuzzy_bytes] over EVERY
    report fn (funcs must be dedup=False), classified by the cached map (real addr)
    with a name/source fallback for catch-all fns the build's dedup dropped. Because
    no fn is dropped, sum(bytes) == report total_code and sum(fns) == total_functions
    EXACTLY -- the per-tier denominators are honest TRUE tier sizes."""
    # per bucket: [fns, bytes, m_fns(normalized), m_bytes(raw byte-perfect),
    #              fuzzy_bytes(size-weighted fuzzy_match_percent)]
    # m_fns uses normalized (sums to measures.matched_functions); m_bytes uses the
    # raw fuzzy_match_percent==100 predicate (sums to measures.matched_code) -- the
    # two predicates objdiff itself uses, so each axis reconciles with ninja's All.
    # fuzzy_bytes / bytes == the tier's size-weighted fuzzy% (work-in-progress).
    by = defaultdict(lambda: [0, 0, 0, 0, 0.0])
    engine_cls, game_cls, vendor_cls = load_name_class(WN_DATA)
    for addr, size, matched, sp, unit, name, fz in funcs:
        sc = scope_by_addr.get("%08X" % addr)
        if sc is None:                # not in cache (dropped by build dedup / new)
            sc = bucket_for_source(sp)
            if sc is None:            # catch-all named fn -> try class-by-name
                sc = classify_name(name, engine_cls, game_cls, vendor_cls)[0]
            sc = sc or "unknown"
        by[sc][0] += 1
        by[sc][1] += size
        if matched:
            by[sc][2] += 1
        if fz >= 100.0:
            by[sc][3] += size
        by[sc][4] += size * fz / 100.0
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
    return "%+d fns · %+.2f%% matched · %+.2f%% fuzzy" % (d_fns, cur_mp - base_mp, d_fz)


def _progress_delta(off_mc, off_tc, off_mf, fuzzy_pct):
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
        return (r.get("mc"), r.get("tc"), r.get("mf"))

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
    Headline reads report.json's official `measures` (== ninja "All"); the per-tier
    `by` uses objdiff's OWN predicates so each axis sums to its official total:
      * matched%  = match_percent_normalized==100 fns / raw fuzzy==100 bytes
      * fuzzy%    = size-weighted fuzzy_match_percent (work-in-progress indicator)
    Denominator per tier = the TRUE tier size from the map (sums to total_code).

    `cache` is the _cache_status() record. When it is not `ok` the tier
    denominators collapse to pinned-only coverage and every percentage below the
    headline reads HIGH, so we shout about it instead of silently reporting a
    number that is not comparable to main's. The binary/headline line is always
    honest (it comes straight from report.json) and is unaffected."""
    def mb(x):
        return x / 1048576.0

    def pct(n, d):
        return 100.0 * n / d if d else 0.0

    def mi(k):
        v = measures.get(k, 0)
        return int(v) if isinstance(v, str) else v

    off_mc, off_tc = mi("matched_code"), mi("total_code")
    off_mf, off_tf = mi("matched_functions"), mi("total_functions")
    fuzzy_pct = float(measures.get("fuzzy_match_percent", 0.0) or 0.0)

    tot_b = sum(v[1] for v in by.values())           # == off_tc (no-dedup)
    orac_b = sum(by[b][1] for b in ORACLE_BACKED)
    orac_mb = sum(by[b][3] for b in ORACLE_BACKED)   # raw matched bytes
    orac_fzb = sum(by[b][4] for b in ORACLE_BACKED)  # fuzzy-weighted bytes
    noora_b = sum(by[b][1] for b in NO_ORACLE)
    noora_mb = sum(by[b][3] for b in NO_ORACLE)
    noora_fzb = sum(by[b][4] for b in NO_ORACLE)
    unk_b = by["unknown"][1]
    mapped_b = (tot_b - unk_b)
    delta, window = _progress_delta(off_mc, off_tc, off_mf, fuzzy_pct)

    cache = cache or {"state": "ok"}
    cstate = cache.get("state", "ok")
    cbad = cstate in _CACHE_BANNER
    # Stale-but-live cache: boundaries age even though the matched overlay doesn't.
    age = cache.get("age_days")
    cstale = (not cbad) and age is not None and age >= CACHE_STALE_DAYS

    out = []
    if compact:
        out.append("Decomp %.2f%% matched · %.2f%% fuzzy · %d/%d fns%s"
                   "  |  north-star oracle %.2f MB @ %.2f%% matched / %.2f%% fuzzy · %.0f%% tier-classified%s" %
                   (pct(off_mc, off_tc), fuzzy_pct, off_mf, off_tf,
                    "  (moved " + delta + ")" if delta else "",
                    mb(orac_b), pct(orac_mb, orac_b), pct(orac_fzb, orac_b), pct(mapped_b, tot_b),
                    ("  [!! scope cache %s — tier %% INFLATED, run: python3 tools/scope_map.py build]"
                     % cstate) if cbad else ""))
    else:
        # ---- framed dashboard --------------------------------------------------
        IW = 66                                  # inner width between the │ borders
        def edge(l, r):
            return l + "─" * IW + r
        def rule(label):                         # ├── label ───────┤ section break
            seg = "── " + label + " "
            return "├" + seg + "─" * max(0, IW - len(seg)) + "┤"
        def line(content=""):
            return "│" + content.ljust(IW) + "│"

        cols = "   %-11s%8s%8s%14s%15s"          # tier · match% · fuzzy% · fns m/t · MB m/t
        def tier_line(b):
            fns, byt, mf, mbr, fzb = by[b]
            return line(cols % (b, "%.2f%%" % pct(mbr, byt), "%.2f%%" % pct(fzb, byt),
                                "%d / %d" % (mf, fns), "%.2f / %.2f" % (mb(mbr), mb(byt))))

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
        # MB subtotal rather than spending a row). Bars gauge the run to 100% =
        # "cruising". Tier rows break the cluster down (matched / total each axis).
        out.append(rule("CORE GOALS — oracle-backed · %.2f / %.2f MB" %
                        (mb(orac_mb), mb(orac_b))))
        out.append(line())
        out.append(line("   %-8s%s  %6.2f%%" % ("matched", _bar(pct(orac_mb, orac_b)), pct(orac_mb, orac_b))))
        out.append(line("   %-8s%s  %6.2f%%" % ("fuzzy", _bar(pct(orac_fzb, orac_b)), pct(orac_fzb, orac_b))))
        out.append(line())
        out.append(line(cols % ("", "match", "fuzzy", "fns m/t", "MB m/t")))
        for b in TIER_ROWS:
            out.append(tier_line(b))
        out.append(line())

        # Lower priority: no-oracle (matchable too, just deferred), largest first.
        out.append(rule("lower priority — no oracle · %.2f / %.2f MB" %
                        (mb(noora_mb), mb(noora_b))))
        out.append(line())
        for b in sorted(NO_ORACLE, key=lambda x: -by[x][1]):
            out.append(tier_line(b))
        out.append(line())

        # NB: "tier-classified" != the dtk/build box's "mapped". That one counts
        # bytes PINNED to a splits.txt unit; this counts bytes attributed to a
        # scope TIER by any of the 8 classification layers, pinned or not.
        out.append(rule("%.0f%% of binary tier-classified · %.2f MB unclassified" %
                        (pct(mapped_b, tot_b), mb(unk_b))))
        if cstale:
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
# Age (days) past which the cache gets a one-line staleness footer. The matched
# overlay is always live (it comes from report.json); what ages is the TIER
# BOUNDARIES baked into the map.
CACHE_STALE_DAYS = 14


def _cache_status(scope_by_addr, funcs, state):
    """Describe the health of the cached scope map for the dashboard.

    Returns {state, path, age_days, mtime (ISO date or None), coverage (0..1 or
    None)}. `state` is one of:
      ok        -- present, readable, resolves most report addrs
      missing   -- scope_map.json absent (fresh checkout / un-primed worktree)
      unreadable-- present but truncated/corrupt JSON
      dead      -- present but addr-keyed to a DIFFERENT target build
    Every non-`ok` state means the tier denominators are pinned-only -> INFLATED.
    Never raises: dashboard output must not be able to break a build."""
    st = {"state": state, "path": os.path.relpath(SCOPE_MAP, ROOT),
          "age_days": None, "mtime": None, "coverage": None}
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
        if st["coverage"] < CACHE_DEAD_COVERAGE:
            st["state"] = "dead"
    return st


def _progress_by_live():
    """Fresh per-bucket aggregate (every fn, no dedup) over the cached map, plus
    report.json's official `measures` for the authoritative headline, plus a
    health record for the cache itself (see _cache_status)."""
    funcs, rep = load_functions(REPORT, dedup=False)
    state = "ok"
    try:
        sm = _load_scope_map()
        scope_by_addr = {a: e["scope"] for a, e in sm.items()}
    except FileNotFoundError:
        scope_by_addr, state = {}, "missing"
    except (json.JSONDecodeError, OSError, KeyError, TypeError):
        scope_by_addr, state = {}, "unreadable"
    cache = _cache_status(scope_by_addr, funcs, state)
    return _by_live(scope_by_addr, funcs), rep.get("measures", {}), cache


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
    sub.add_parser("build", help="classify all fns, write scope_map.json")
    sub.add_parser("report", help="per-bucket breakdown + progress toward 100%%")
    pr = sub.add_parser("priority", help="progress toward 100%% by priority tier (live matched, cached classification)")
    pr.add_argument("--compact", action="store_true", help="one-line form (for the build PROGRESS step)")
    w = sub.add_parser("worklist", help="size-ranked unmatched oracle-backed clusters (cheapest work)")
    w.add_argument("--limit", type=int, default=50)
    w.add_argument("--bucket", choices=BUCKET_ORDER, help="filter to one bucket")
    c = sub.add_parser("classify", help="classify a single addr")
    c.add_argument("addr")
    args = ap.parse_args()
    {"build": cmd_build, "report": cmd_report, "priority": cmd_priority,
     "worklist": cmd_worklist, "classify": cmd_classify}[args.cmd](args)


if __name__ == "__main__":
    main()
