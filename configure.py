#!/usr/bin/env python3

###
# Generates build files for the project.
# This file also includes the project configuration,
# such as compiler flags and the object matching status.
#
# Usage:
#   python3 configure.py
#   ninja
#
# Append --help to see available options.
###

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from tools.project import (
    Object,
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
)

from tools.defines_common import (
    cflags_includes,
    DEFAULT_VERSION,
    VERSIONS
)

# ---------------------------------------------------------------------------
# X360 MSVC compiler selection (the default is the fleet toolchain)
# ---------------------------------------------------------------------------
# The build compiles every TU with build/compilers/$mw_version/cl.exe.  The
# fleet default is build 10224, extracted from XDK 2.0.11164
# (XDKSetupXenon11164.3.exe).  That is the build retail Rock Band 3 was
# compiled with -- confirmed by the Rich header of retail band.exe (lane CA-1)
# and by the @comp.id stamped into objects it produces (0x00AB27F0 ->
# prodid 0x00AB, build 0x27F0 = 10224; lane CA-2).
#
# FLIPPED 2026-07-30 from build 11886 (DC3's compiler, the original bring-up
# toolchain): a whole-tree cold-cache A/B measured +26 strict / 0 lost /
# 0 breakage under 10224 (~/tmp cc10224 experiment; pre-flip main baseline
# 41,187 matched / floor 39,677).  10224 fixes instruction-selection walls
# (redundant clrlwi vs mr, extsb. vs cmplwi) that no source change could.
# All match% figures from before the flip were calibrated against 11886.
#
# 11886 remains installed for A/B archaeology; select it explicitly:
#     python3 configure.py --x360-compiler-version X360/16.00.11886.00
#     RB3_X360_COMPILER_VERSION=X360/16.00.11886.00 python3 configure.py
DEFAULT_X360_COMPILER_VERSION = "X360/16.00.10224.00"
X360_COMPILER_VERSION_ENV = "RB3_X360_COMPILER_VERSION"

parser = argparse.ArgumentParser()
parser.add_argument(
    "mode",
    choices=["configure", "progress"],
    default="configure",
    help="script mode (default: configure)",
    nargs="?",
)
parser.add_argument(
    "-v",
    "--version",
    choices=VERSIONS,
    type=str.upper,
    default=VERSIONS[DEFAULT_VERSION],
    help="version to build",
)
parser.add_argument(
    "--build-dir",
    metavar="DIR",
    type=Path,
    default=Path("build"),
    help="base build directory (default: build)",
)
parser.add_argument(
    "--binutils",
    metavar="BINARY",
    type=Path,
    help="path to binutils (optional)",
)
parser.add_argument(
    "--compilers",
    metavar="DIR",
    type=Path,
    help="path to compilers (optional)",
)
parser.add_argument(
    "--x360-compiler-version",
    metavar="VERSION",
    type=str,
    default=None,
    help=(
        "X360 MSVC compiler version directory under build/compilers "
        f"(default: {DEFAULT_X360_COMPILER_VERSION}). Opt-in only -- e.g. "
        "'X360/16.00.10224.00' for the XDK 2.0.11164 (retail-RB3) compiler. "
        "May also be set via the RB3_X360_COMPILER_VERSION environment "
        "variable; the command-line flag wins."
    ),
)
parser.add_argument(
    "--map",
    action="store_true",
    help="generate map file(s)",
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="build with debug info (non-matching)",
)
if not is_windows():
    parser.add_argument(
        "--wrapper",
        metavar="BINARY",
        type=Path,
        help="path to wibo or wine (optional)",
    )
parser.add_argument(
    "--dtk",
    metavar="BINARY | DIR",
    type=Path,
    help="path to decomp-toolkit binary or source "
    "(optional; defaults to the local jeff fork at ../jeff)",
)
parser.add_argument(
    "--objdiff",
    metavar="BINARY | DIR",
    type=Path,
    help="path to objdiff-cli binary or source "
    "(optional; defaults to the local objdiff fork at ../objdiff)",
)
parser.add_argument(
    "--sjiswrap",
    metavar="EXE",
    type=Path,
    help="path to sjiswrap.exe (optional)",
)
parser.add_argument(
    "--ninja",
    metavar="BINARY",
    type=Path,
    help="path to ninja binary (optional)"
)
parser.add_argument(
    "--verbose",
    action="store_true",
    help="print verbose output",
)
parser.add_argument(
    "--non-matching",
    dest="non_matching",
    action="store_true",
    help="builds equivalent (but non-matching) or modded objects",
)
parser.add_argument(
    "--warn",
    dest="warn",
    type=str,
    choices=["all", "off", "error"],
    help="how to handle warnings",
)
parser.add_argument(
    "--no-progress",
    dest="progress",
    action="store_false",
    help="disable progress calculation",
)
args = parser.parse_args()

config = ProjectConfig()
config.version = str(args.version)
version_num = VERSIONS.index(config.version)

# Default toolchain sources: ALWAYS prefer the locally-checked-out forks we
# iterate on -- ../jeff for dtk and ../objdiff for objdiff-cli -- so the build
# runs the exact fork state on disk. Building from the sibling source tree (via
# the `cargo` ninja rule) is strictly better than downloading a pinned release:
# even the forks' own tagged releases (rjkiv/jeff, freeqaz/objdiff) can lag
# behind local commits carrying RB3-retail fixes (jeff's overlap-tolerance
# patch, objdiff's --include-data + branch-graph JSON the scripts depend on).
# Override either with --dtk / --objdiff to point at a different path or binary.
#
# Absolute fallback for the sibling fork checkouts: (source_dir, prebuilt_bin).
# The upward CWD-walk below works when configure.py lives next to its siblings
# (the main repo, or a worktree nested under .claude/worktrees/ which still
# walks up to the real ../../../../jeff). But a *detached* worktree (e.g.
# /tmp/wt-foo from scripts/setup_worktree.sh) has no sibling ../jeff to walk up
# to, so the walk fails and a bare `configure.py` would silently fall back to a
# DOWNLOADED dtk release (v0.3.0) that hard-fails the RB3 split on "Overlapping
# functions 3:0x8229D660". To make a bare `python3 configure.py` work from any
# cwd, fall back to these known absolute paths (overridable via env var) after
# the walk fails. We prefer the prebuilt binary (no cargo edge, matching what
# setup_worktree.sh passes via --dtk/--objdiff), else the source dir (cargo
# edge, same as the main-repo default). Explicit --dtk/--objdiff flags still win
# (they short-circuit before _default_*_path), and the upward walk still wins
# when it succeeds -- this only ADDS a fallback.
# repo_name -> (source_dir, release_binary_name). The release binary is looked
# for at <source_dir>/target/release/<binary_name>.
_FORK_FALLBACK = {
    "jeff": ("/home/free/code/milohax/jeff", "dtk"),
    "objdiff": ("/home/free/code/milohax/objdiff", "objdiff-cli"),
    # objcache: the content-addressed MSVC obj cache wrapping the compile edge
    # (sibling repo, rebuilt manually like jeff/objdiff -- see CLAUDE.md). Same
    # resolver so main and every worktree bake the IDENTICAL absolute path into
    # the msvc rule -> byte-identical command strings (warm-worktree parity).
    "objcache": ("/home/free/code/milohax/objcache", "objcache"),
}


def _find_local_fork(repo_name: str) -> Optional[Path]:
    """Return the sibling fork's tool for `<repo_name>`, PREFERRING the prebuilt
    release binary at `<fork>/target/release/<bin>` over the source dir.

    Searches upward from this file's directory for the fork checkout (the dir
    holding its Cargo.toml) -- walking parents, not just the direct sibling, so
    worktrees nested under .claude/worktrees/<name>/ still resolve up to the
    shared ../../../../jeff and ../../../../objdiff checkouts. If the walk fails
    (e.g. a detached ~/tmp worktree with no sibling checkout), falls back to a
    known absolute path (env var RB3_<REPO>_DIR, else a baked-in default).

    In BOTH the walk and the fallback we return the prebuilt release binary when
    it exists (emitting NO cargo build edge), else the source dir (cargo edge).
    Preferring the prebuilt is what makes bare `configure.py` produce a
    byte-identical build.ninja in the main repo (walk succeeds) and in a
    detached worktree (walk fails -> absolute fallback): both resolve to the
    SAME absolute binary path, e.g. /home/free/code/milohax/jeff/target/release/dtk.
    That parity is load-bearing for the fully-warm (0-compile) worktree flow in
    scripts/setup_worktree.sh, and it survives the constant bare-reconfigure
    churn (agents run `python3 configure.py` after every splits.txt/objects.json
    edit). Trade-off: editing the jeff/objdiff *Rust sources* is no longer
    auto-picked-up -- rebuild the release binary manually (`cargo build
    --release` in the fork). See CLAUDE.md "Git & worktrees".
    Returns None only if nothing resolves."""
    # bin_name = the fork's release binary; used to prefer a prebuilt binary in
    # BOTH resolution paths below.
    src_default, bin_name = _FORK_FALLBACK.get(repo_name, (None, None))

    def _prefer_prebuilt(fork_dir: Path) -> Path:
        if bin_name:
            prebuilt = fork_dir / "target" / "release" / bin_name
            if prebuilt.is_file():
                return prebuilt
        return fork_dir

    cur = Path(__file__).resolve().parent
    while True:
        fork_dir = cur.parent / repo_name
        if (fork_dir / "Cargo.toml").is_file():
            return _prefer_prebuilt(fork_dir)
        if cur.parent == cur:
            break
        cur = cur.parent
    # Upward walk failed -- resolve from a known fork dir: env var override
    # (authoritative if set), else the baked-in absolute default.
    src_dir = os.environ.get(f"RB3_{repo_name.upper()}_DIR") or src_default
    if src_dir and bin_name:
        sd = Path(src_dir)
        prebuilt = sd / "target" / "release" / bin_name
        if prebuilt.is_file():
            return prebuilt
        if (sd / "Cargo.toml").is_file():
            return sd
    return None

def _default_dtk_path() -> Optional[Path]:
    return _find_local_fork("jeff")

def _default_objdiff_path() -> Optional[Path]:
    return _find_local_fork("objdiff")

def _default_objcache_path() -> Optional[Path]:
    # Prefer the prebuilt release binary; None if the fork/binary is absent
    # (then the msvc rule simply omits the cache prefix -> uncached but correct).
    return _find_local_fork("objcache")

# Absolute fallback dir for the sibling freeqaz/wibo fork checkout (overridable
# via env RB3_WIBO_DIR). The release binary is looked for at
# <dir>/build/release/wibo -- the SAME layout scripts/setup_worktree.sh passes
# via --wrapper "$TOOL_DIR/wibo/build/release/wibo".
_WIBO_DEFAULT_DIR = "/home/free/code/milohax/wibo"
_WIBO_BIN_SUBPATH = ("build", "release", "wibo")


def _find_local_wibo() -> Optional[Path]:
    """Return the ABSOLUTE path to the freeqaz/wibo fork release binary at
    <wibo>/build/release/wibo, or None if it cannot be found.

    Mirrors _find_local_fork's resolution so behaviour is uniform: upward-walk
    from this file's directory for a sibling `wibo/` checkout (so a worktree
    nested under .claude/worktrees/<name>/ still resolves up to the shared
    ../../../../wibo), then env RB3_WIBO_DIR, then a baked-in absolute default.

    ALWAYS resolves to an absolute path (Path(...).resolve()). This is
    load-bearing: scripts/setup_worktree.sh passes the absolute
    $TOOL_DIR/wibo/build/release/wibo, and main's default must produce the
    IDENTICAL string in the msvc rule command, or command-hash parity (and the
    fully-warm worktree seeding) is dead. dc3 uses a relative Path("..") default
    -- we deliberately do NOT, for exactly this reason."""
    cur = Path(__file__).resolve().parent
    while True:
        cand = cur.parent.joinpath("wibo", *_WIBO_BIN_SUBPATH)
        if cand.is_file():
            return cand.resolve()
        if cur.parent == cur:
            break
        cur = cur.parent
    # Upward walk failed (e.g. a detached ~/tmp worktree) -- env override, else
    # the baked-in absolute default.
    wibo_dir = os.environ.get("RB3_WIBO_DIR") or _WIBO_DEFAULT_DIR
    cand = Path(wibo_dir).joinpath(*_WIBO_BIN_SUBPATH)
    if cand.is_file():
        return cand.resolve()
    return None


def _gate_wibo_wrapper(wrapper_path: Path) -> None:
    """Hard-fail unless wrapper_path is the freeqaz/wibo fork build: it must
    exist AND carry the WIBO_FS_CACHE + WIBO_REWRITE_SHOWINCLUDES feature bytes.

    Rationale: tools/project.py drops the out-of-process transform_dep.py pipe
    and instead passes WIBO_REWRITE_SHOWINCLUDES=1 so wibo rewrites the
    "Note: including file:" lines in-process. A STOCK wibo silently ignores that
    env var and feeds raw backslash/wrong-case Windows paths to ninja's
    deps=msvc parser -> corrupted dependency tracking (a silent, insidious
    failure). A loud configure failure is the only acceptable fallback."""
    if not wrapper_path.is_file():
        sys.exit(
            f"FATAL: wrapper {wrapper_path} does not exist. Build the "
            "freeqaz/wibo fork: cd ../wibo && cmake --preset release && "
            "cmake --build --preset release"
        )
    data = wrapper_path.read_bytes()
    if b"WIBO_REWRITE_SHOWINCLUDES" not in data or b"WIBO_FS_CACHE" not in data:
        sys.exit(
            f"FATAL: {wrapper_path} is not the freeqaz/wibo fork build "
            "(missing WIBO_FS_CACHE/WIBO_REWRITE_SHOWINCLUDES). The msvc rule "
            "relies on the fork's in-process showIncludes rewrite; a stock "
            "binary would corrupt ninja's deps=msvc tracking. Build it: "
            "cd ../wibo && cmake --preset release && cmake --build --preset release"
        )

def _gate_objdiff_missing() -> None:
    """Hard-fail when the local objdiff fork cannot be resolved.

    Rationale (2026-07-29): objdiff's funclet pairing pass credits a target
    funclet at 100% even when every byte-identical base partner is already
    consumed. Our fork populates `Measures.masked_equal_functions` so EVERY
    report states its own honest floor
    (`matched_functions - masked_equal_functions`).

    *** RULER CHANGE 2026-08-02 -- the size of this disclosure grew ~20x. ***
    The fork originally reported pass-2b over-subscription ONLY, which read as
    a ~4% over-count (measured then: 39,743 reported vs 38,210 honest). It now
    discloses ALL funclet byte-signature pairings: masked_equal_functions
    1,096 -> 22,640, honest 42,358 -> 20,814, a 52.10% disclosure share of
    matched_functions. NO SCORE KEY MOVED -- matched_functions,
    matched_code_percent and fuzzy_match_percent were verified identical
    (11/11 keys); this is disclosure, not scoring. So the cost of falling back
    to the downloaded release is NOT "~4% inflated": it is losing the
    disclosure on roughly HALF the reported match count.
    Authoritative record: docs/decomp/RULER_CHANGE_2026-08-02.md.

    The old behaviour here was a WARN plus a silent fallback to the DOWNLOADED
    objdiff release, which does not populate that field. A report generated
    that way looks completely normal and simply omits the disclosure -- i.e. it
    silently restores the inflated headline with nothing flagging it. That is
    the same insidious-failure shape as a stock wibo corrupting deps=msvc
    tracking, and it gets the same treatment: a loud configure failure is the
    only acceptable fallback. See `_gate_wibo_wrapper` above.

    Escape hatch for a genuinely fork-less environment (mirrors
    RB3_OBJCACHE_OPTIONAL): RB3_OBJDIFF_OPTIONAL=1 -> warn and fall back, and
    OWN the fact that your reports will over-count."""
    if os.environ.get("RB3_OBJDIFF_OPTIONAL") == "1":
        print(
            "WARN: RB3_OBJDIFF_OPTIONAL=1 -- falling back to the downloaded "
            "objdiff-cli release. Reports will NOT carry "
            "masked_equal_functions, so ~52% of matched_functions (22,640 of "
            "43,454 at f48bcad7) goes undisclosed with nothing flagging it. "
            "Do not quote those numbers.",
            file=sys.stderr,
        )
        return
    sys.exit(
        "FATAL: local objdiff fork (../objdiff) not found.\n"
        "  The downloaded objdiff-cli release does NOT populate\n"
        "  Measures.masked_equal_functions, so its reports silently omit the\n"
        "  funclet-pairing disclosure that currently covers ~52% of\n"
        "  matched_functions -- with nothing flagging it. A silently-wrong\n"
        "  number is worse than no number.\n"
        "  Fix: clone/build the fork (cd ../objdiff && cargo build --release,\n"
        "  with the oversub-disclosure branch checked out), or pass --objdiff\n"
        "  explicitly. Genuinely fork-less environment: RB3_OBJDIFF_OPTIONAL=1\n"
        "  (accepts inflated reports)."
    )


# Apply arguments
config.build_dir = args.build_dir
config.dtk_path = args.dtk if args.dtk is not None else _default_dtk_path()
config.objdiff_path = args.objdiff if args.objdiff is not None else _default_objdiff_path()
# objcache: content-addressed MSVC obj cache wrapping every compile edge. Resolve
# the prebuilt binary via the SAME _find_local_fork resolver as dtk/objdiff, so
# main and every worktree bake the identical absolute path into the msvc rule
# (byte-identical command strings -> warm-worktree command-hash parity).
config.objcache_path = _default_objcache_path()
# Existence gate (mirrors _gate_wibo_wrapper): the cache prefix is a permanent
# part of the msvc rule and every worktree resolves this same absolute binary, so
# a missing binary is a real misconfiguration to surface LOUDLY, not to silently
# skip (a skip would emit a different, uncached command string -> break warm
# parity and quietly de-optimize the fleet). Runtime kill switch stays `objcache
# off` / OBJCACHE=off (keeps the prefix; the binary itself passes through to a
# real compile). Escape hatch for a genuinely cache-less environment:
# RB3_OBJCACHE_OPTIONAL=1 -> omit the prefix (plain uncached compile, still
# correct). Explicit --wrapper/other overrides are unaffected.
if os.environ.get("RB3_OBJCACHE_OPTIONAL") not in (None, "", "0"):
    config.objcache_path = None  # opt out: msvc rule omits the prefix
elif config.objcache_path is None:
    sys.exit(
        "FATAL: objcache binary not found (looked upward for <ancestor>/objcache/"
        "target/release/objcache, then $RB3_OBJCACHE_DIR, then the baked default "
        "/home/free/code/milohax/objcache/target/release/objcache). The msvc rule "
        "prefixes every compile with the object cache; a missing binary would "
        "either break warm-worktree command parity or silently de-optimize. Build "
        "it: cd ../objcache && cargo build --release  (or set "
        "RB3_OBJCACHE_OPTIONAL=1 to build uncached on a cache-less box)."
    )

# "Always use our forks": warn loudly if a local fork checkout couldn't be found
# and no explicit override was given, so we notice when a build silently falls
# back to a downloaded release instead of the source tree we're iterating on.
if args.dtk is None and config.dtk_path is None:
    print(
        "WARN: local jeff fork (../jeff) not found; falling back to a "
        "downloaded dtk release from rjkiv/jeff. Clone the fork or pass --dtk.",
        file=sys.stderr,
    )
if args.objdiff is None and config.objdiff_path is None:
    _gate_objdiff_missing()
config.binutils_path = args.binutils
config.compilers_path = args.compilers
config.generate_map = args.map
config.non_matching = args.non_matching
config.sjiswrap_path = args.sjiswrap
config.ninja_path = args.ninja
config.progress = args.progress
if not is_windows():
    # Compiler wrapper resolution (Linux). Order: explicit --wrapper -> the
    # freeqaz/wibo fork release binary (upward-walk -> RB3_WIBO_DIR -> baked-in
    # absolute default). Resolving to an absolute path makes main's msvc command
    # string byte-identical to what scripts/setup_worktree.sh bakes into a
    # worktree (--wrapper "$TOOL_DIR/wibo/build/release/wibo"), which is what
    # kills the per-worktree "recompile all ~745 objs once" and enables warm
    # seeding. Setting config.wrapper makes use_wibo() False, so tools/project.py
    # emits NO stock-wibo download edge and NO build/tools/wibo implicit dep --
    # the download-clobber-the-fork mechanism is structurally gone.
    if args.wrapper is not None:
        wrapper_path = Path(args.wrapper).resolve()
        # Only feature-gate wibo wrappers; wine / other wrappers are out of scope.
        if "wibo" in str(wrapper_path):
            _gate_wibo_wrapper(wrapper_path)
        config.wrapper = wrapper_path
    else:
        wrapper_path = _find_local_wibo()
        if wrapper_path is None:
            sys.exit(
                "FATAL: freeqaz/wibo fork binary not found (looked upward for "
                "<ancestor>/wibo/build/release/wibo, then $RB3_WIBO_DIR/build/"
                f"release/wibo, then {_WIBO_DEFAULT_DIR}/build/release/wibo). The "
                "msvc rule requires the fork's in-process showIncludes rewrite; "
                "there is no safe stock-wibo fallback. Build it: cd ../wibo && "
                "cmake --preset release && cmake --build --preset release"
            )
        _gate_wibo_wrapper(wrapper_path)
        config.wrapper = wrapper_path
# Don't build asm unless we're --non-matching
if not config.non_matching:
    config.asm_dir = None

# Tool versions
config.binutils_tag = "2.42-1"
config.compilers_tag = "20250812"
# Nothing DOWNLOADS this tag, and it does not select the binary -- dtk is resolved
# by absolute path (see the _fork_dir docstring ~line 233): the walk and the
# fallback both land on /home/free/code/milohax/jeff/target/release/dtk. But this
# string is NOT inert. tools/project.py:400-401 compares it against the "version"
# recorded in build/<version>/config.json and DELETES that file -- forcing a
# re-split -- when the recorded version is older. A stale pin therefore disarms
# the staleness gate: at "v0.3.0" this project would never have auto-re-split for
# any fork release, which is how it sat on 1.9.4 output silently. Keep it current.
# It also records WHICH jeff build the checked-in split output corresponds to.
# Was "v0.3.0", which was fiction (upstream decomp-toolkit numbering, never our
# fork's). Measured 2026-08-08 on 45410914: re-split 1.9.4 -> 1.10.0 (cdfe173)
# is a STRICT NO-OP. All 2420 split objects content-identical, 782,103
# relocations before and after (delta exactly 0), 72,948 sections, 426,091
# symbol-table entries. 1.9.5's xam.xex ordinal-table fix touches nothing here
# (609 __imp_/Unused* thunk symbols across 7 units, unchanged); 1.10.0's PDB
# function-size harvest is gated on `pdb:`, which config/45410914/config.yml
# does not set. Verified by re-running 1.10.0 with the Aug-6-era
# scripts/target_symbol_map.json (JEFF_MERGE_PROTECT): it reproduces the
# previous on-disk objects exactly. The 4 objects that DID move in the live
# tree (CharBonesSamples, DataNode, VocalPart, meta_band/BandProfile) moved
# because that map grew by ~1000 addresses on Aug 6, which unblocked four
# merge_branch_reached_overcarve_tails runs -- project state, not dtk.
#
# 1.10.0 -> 1.11.0: XDK CRT register save/restore sleds are now named from their
# bodies (__savegprlr_25) instead of split as lbl_<addr>. NOT `pdb:`-gated, so unlike
# the 1.10.0 PDB harvest (measured a strict no-op here, 2420/2420 objects identical)
# this one does reach RB3. build/45410914/config.json already records 1.11.0 -- the
# shared jeff binary was rebuilt and this tree re-split against it -- so this bump is
# documentation catching up, and it cannot force a re-split: load_build_config() drops
# config.json only when the RECORDED version is older than the pin, and
# 1.11.0 < 1.11.0 is false. It re-arms the gate for the next release.
#
# 1.11.0 -> 1.12.0: the intra-function relocation-addend fix. Before it, a
# relocation whose anchor symbol was the ENCLOSING function lost its addend, so
# every self-referential site inside a function pointed at the function's first
# byte instead of its real target; and the splitter minted spurious PpcRel14
# records for branches that never left their own function. Both change SECTION
# CONTENTS. Census on this tree's TARGET objects (jeff
# docs/sessions/2026-08-12-splitter-reloc-addend/findings/T6-selfref-census.md):
# 262 self-referential relocation sites across 110 functions, of which **226
# sites / 101 functions are the defect** and 36 / 9 are legitimate zero-addend
# references. So RB3-XENON IS AFFECTED -- an earlier claim that it was not is
# retracted. Same shape as the 1.11.0 bump above: build/45410914/config.json
# already records 1.12.0, because the shared jeff binary was rebuilt (c0cc506,
# 2026-08-13) and this tree re-split against it, so this line is documentation
# catching up and cannot itself force a re-split. It re-arms the gate for the
# next release.
#
# 1.12.0 -> 1.13.0: two splitter correctness fixes, co-measured before deploy
# (decomp-bench archive/runs/2026-08-13-jeff-combined-deploy-gate/).
#   * DS-form decode, both sides: the analysis side read a DS-form load/store
#     displacement as the full low halfword, when bits [1:0] are an opcode
#     extension -- so it anchored two bytes inside the real datum -- and the
#     writer zeroed those same two bits, rewriting the opcode. Both fixed
#     together; fixing either alone regresses.
#   * The relocation tracker walked past function_end into the NEXT function and
#     judged its branches against the PREVIOUS function's bounds, minting
#     spurious PpcRel14 records. Fixed at the cause, plus an
#     instruction-derived COMDAT keep-back and a COMDAT-NESTING fix in two
#     containment lookups (nested regions were resolved by a nearest-region
#     query, which both held functions back spuriously and dropped needed
#     relocations).
# Measured object movement, exact -- the split is deterministic, six control
# runs 3084/3084 identical: dc3 3 of 2223, rb3-xenon 64 of 3084, cea 2 of 3675.
# Zero interaction between the two fixes on all three projects.
config.dtk_tag = "v1.13.0"  # jeff b4b25bc (fork of decomp-toolkit)
config.objdiff_tag = "v4.2.2"  # freeqaz/objdiff fork release (linux-x86_64 asset)
config.sjiswrap_tag = "v1.2.1"
config.wibo_tag = "1.0.1"

# Project
config_dir = Path("config") / config.version
config_json_path = config_dir / "config.json"
objects_path = config_dir / "objects.json"
config.config_path = config_dir / "config.yml"
config.check_sha_path = config_dir / "build.sha1"

# ── HARD FAIL: duplicate splits.txt unit heading ──────────────────────────────
# A unit heading repeated in splits.txt does NOT error — dtk unions both blocks
# into BOTH headings, so each unit silently claims the other's ranges. That is
# invisible in the build log and was caught previously only by reading the
# post-split diff by hand (it cost lane BS-2 real time). Check it up front.
_splits_path = config_dir / "splits.txt"
if _splits_path.is_file():
    _seen: Dict[str, int] = {}
    for _lineno, _line in enumerate(
        _splits_path.read_text().splitlines(), start=1
    ):
        # Headings are unindented and end with ':'; section rows are indented.
        if not _line or _line[:1].isspace() or _line.lstrip().startswith("#"):
            continue
        _s = _line.strip()
        if not _s.endswith(":"):
            continue
        _name = _s[:-1].strip()
        if _name in _seen:
            sys.exit(
                "\n"
                + "=" * 72
                + "\nERROR: duplicate unit heading in %s\n" % _splits_path
                + "=" * 72
                + "\n  %r appears at line %d and again at line %d.\n"
                % (_name, _seen[_name], _lineno)
                + "\ndtk does NOT reject this: it unions both blocks into BOTH headings,"
                + "\nso each unit silently claims the other's address ranges and the"
                + "\nresulting objs are wrong in a way no build line reports."
                + "\nMerge the two blocks into a single heading.\n"
                + "=" * 72
            )
        _seen[_name] = _lineno
# Use for any additional files that should cause a re-configure when modified
config.reconfig_deps = [
    config_json_path,
    objects_path,
]

# Optional numeric ID for decomp.me preset
# Can be overridden in libraries or objects
config.scratch_preset_id = None

# Build flags
flags = json.load(open(config_json_path, "r", encoding="utf-8"))
progress_categories: dict[str, str] = flags["progress_categories"]
asflags: list[str] = flags["asflags"]
ldflags: list[str] = flags["ldflags"]
cflags: dict[str, dict] = flags["cflags"]

def get_cflags(name: str) -> list[str]:
    return cflags[name]["flags"]
def add_cflags(name: str, flags: list[str]):
    cflags[name]["flags"] = [*flags, *cflags[name]["flags"]]

def get_cflags_base(name: str) -> str:
    return cflags[name].get("base", None)

def are_cflags_inherited(name: str) -> bool:
    return "inherited" in cflags[name]
def set_cflags_inherited(name: str):
    cflags[name]["inherited"] = True

def apply_base_cflags(key: str):
    if are_cflags_inherited(key):
        return

    base = get_cflags_base(key)
    if base is None:
        add_cflags(key, cflags_includes)
    else:
        apply_base_cflags(base)
        add_cflags(key, get_cflags(base))

    set_cflags_inherited(key)

# Set up base flags
base_cflags = get_cflags("base")

# Apply cflag inheritance
for key in cflags.keys():
    apply_base_cflags(key)

config.asflags = [
    *asflags,
    # f"--defsym BUILD_VERSION={version_num}",
    # f"--defsym VERSION_{config.version}",
]
config.ldflags = ldflags

# Resolve the X360 compiler: command line > environment > fleet default.
# Unset on both => the default string, i.e. byte-identical generated output.
_x360_version = (
    args.x360_compiler_version
    or os.environ.get(X360_COMPILER_VERSION_ENV)
    or DEFAULT_X360_COMPILER_VERSION
)
if _x360_version != DEFAULT_X360_COMPILER_VERSION:
    # Only validate on the opt-in path. On the default path the compilers dir
    # may legitimately not exist yet (it is a download edge), and probing it
    # would change side effects relative to pre-switch behaviour.
    _x360_dir = config.compilers() / _x360_version
    if not (_x360_dir / "cl.exe").is_file():
        sys.exit(
            f"--x360-compiler-version: no cl.exe at {_x360_dir}\n"
            f"Available under {config.compilers() / 'X360'}: "
            + ", ".join(
                sorted(p.name for p in (config.compilers() / "X360").glob("*"))
            )
        )
    print(
        f"NOTE: using non-default X360 compiler {_x360_version} "
        f"(fleet default is {DEFAULT_X360_COMPILER_VERSION})",
        file=sys.stderr,
    )
config.linker_version = _x360_version

config.shift_jis = False
config.progress_all = False

# Precompiled header (PCH): engine dirs whose tracked functions are unaffected
# by /FI"decomp_pch.h" force-including Object.h. dc3's full engine set minus the
# four dirs that regress tracked matches on rb3-xenon (char, rndobj, world, ui);
# see W1-C gate evidence. /FI changes inlining of *untracked* helper/template
# code (dc3 saw byte-identical .text; rb3-xenon does not), but no tracked
# function in these nine dirs changes match% (Gate-2/3 per-function equality).
config.pch_header = "decomp_pch.h"
config.pch_source = Path("src/system/decomp_pch.cpp")
config.pch_eligible_dirs = {
    "hamobj", "synth", "flow", "gesture", "meta",
    "obj", "os", "utl", "movie",
}

# Post-compile patchers: run after all .obj files are compiled, before linking.
# These patch decomp .obj files to match original binary patterns (anonymous
# namespace hashes, ??__E dynamic initializers, $S guard variables, bool
# parameter mangling, ??__F atexit scope counters). Mirrors
# dc3-decomp/configure.py:294-357.
stamp_dir = config.build_dir / config.version
config.custom_build_rules = [
    {
        "name": "run_script",
        "command": "$cmd && touch $out",
        "description": "$desc",
    },
]
config.custom_build_steps = {
    "pre-compile": [
        # Rename anonymous fn_<addr> symbols in dtk-split target .obj files to
        # their MSVC mangled equivalents (from scripts/target_symbol_map.json).
        # Runs after SPLIT (target obj is a dep) but before the report step
        # consumes them. Idempotent: a re-SPLIT recreates fn_<addr> symbols,
        # which this stamp depends on via the order_only edge to "split"... but
        # we keep it cheap by scanning all objs every time the stamp is dirty.
        {
            "outputs": str(stamp_dir / "target_symbol_renames.stamp"),
            "rule": "run_script",
            # No explicit input edge to specific objs — ninja will rerun this
            # when the stamp is older than build/.../config.json (SPLIT output)
            # via the implicit dep below.
            "implicit": [
                "scripts/obj_target_symbol_renamer.py",
                "scripts/target_symbol_map.json",
                str(stamp_dir / "config.json"),
            ],
            "variables": {
                "cmd": "python3 scripts/obj_target_symbol_renamer.py --batch --apply",
                "desc": "PATCH target fn_<addr> -> MSVC mangled names",
            },
        },
    ],
    # The five obj patchers each read-modify-write the SAME build/**/*.obj
    # set, so they MUST be serialized: with only `order_only: all_source`
    # ninja runs them concurrently, and a reader can catch a file another
    # patcher is rewriting (transient truncation -> parse crash), or two
    # patchers can lose each other's writes (silently dropped symbol patches,
    # nondeterministic match output). Each stamp is an implicit input of the
    # next (same fix as dc3 3cc46ca1, where serializing recovered a lost
    # matched function).
    #
    # ★★★ `all_source` IS AN IMPLICIT INPUT, NOT AN ORDER-ONLY ONE (lane CN-1).
    # It used to be `order_only`, which in ninja constrains ORDER but NEVER
    # makes the edge dirty. So when a source change recompiled an obj, the
    # freshly compiled obj arrived UNPATCHED, the stamp was still considered
    # current, and the patchers NEVER RE-FIRED -- the patches were silently
    # gone and the whole-binary metric dropped with nothing failing (measured:
    # a fresh worktree read 43,097 instead of 43,147, and a 9-file source patch
    # cost 84 matched functions that a revert did not restore). As an implicit
    # input, `all_source` propagates the objs' mtimes through the phony, so any
    # obj newer than a stamp re-triggers that patcher.
    #
    # ⚠ THIS ONLY CONVERGES BECAUSE EVERY PATCHER RESTORES THE OBJ mtime.
    # ninja's `deps = msvc` stores each obj's mtime next to its dep record in
    # .ninja_deps and RECOMPILES the obj when the file on disk is newer
    # ("stored deps info out of date"). A patcher that bumps the mtime would
    # therefore make ninja recompile the obj it just patched, which -- now that
    # the recompile re-triggers the patcher -- would OSCILLATE forever. Before
    # this lane, obj_anon_ns bumped 27 objs and obj_dynamic_init bumped 188
    # (198 unique) and the next build recompiled exactly those 198. All six
    # patchers now write via _write_preserving_mtime(); do not remove that.
    "post-compile": [
        {
            "outputs": str(stamp_dir / "anon_ns_patched.stamp"),
            "rule": "run_script",
            "implicit": [
                "scripts/obj_anon_ns_patcher.py",
                "all_source",
            ],
            "variables": {
                "cmd": "python3 scripts/obj_anon_ns_patcher.py --batch --apply",
                "desc": "PATCH anonymous namespace hashes",
            },
        },
        {
            "outputs": str(stamp_dir / "dynamic_init_patched.stamp"),
            "rule": "run_script",
            "implicit": [
                "scripts/obj_dynamic_init_patcher.py",
                str(stamp_dir / "anon_ns_patched.stamp"),
                "all_source",
            ],
            "variables": {
                "cmd": "python3 scripts/obj_dynamic_init_patcher.py --batch --apply",
                "desc": "PATCH ??__E dynamic initializers STATIC->EXTERNAL",
            },
        },
        {
            "outputs": str(stamp_dir / "guard_patched.stamp"),
            "rule": "run_script",
            "implicit": [
                "scripts/obj_guard_patcher.py",
                str(stamp_dir / "dynamic_init_patched.stamp"),
                "all_source",
            ],
            "variables": {
                "cmd": "python3 scripts/obj_guard_patcher.py --batch --apply",
                "desc": "PATCH $S guard variables to match ??_B naming",
            },
        },
        {
            "outputs": str(stamp_dir / "bool_mangle_patched.stamp"),
            "rule": "run_script",
            "implicit": [
                "scripts/obj_bool_mangle_patcher.py",
                str(stamp_dir / "guard_patched.stamp"),
                "all_source",
            ],
            "variables": {
                "cmd": "python3 scripts/obj_bool_mangle_patcher.py --batch --apply",
                "desc": "PATCH bool parameter back-reference mangling",
            },
        },
        {
            "outputs": str(stamp_dir / "atexit_scope_patched.stamp"),
            "rule": "run_script",
            "implicit": [
                "scripts/obj_atexit_scope_patcher.py",
                str(stamp_dir / "bool_mangle_patched.stamp"),
                "all_source",
            ],
            "variables": {
                "cmd": "python3 scripts/obj_atexit_scope_patcher.py --batch --apply",
                "desc": "PATCH ??__F atexit scope counters (fuzzy match)",
            },
        },
        # Insert an extent boundary at every EH-FUNCLET prefix. MSVC marks the
        # end of an EH function only with a class-6 `$M#####` label, which
        # objdiff does not treat as a boundary, so our function's extent runs
        # 8 bytes past its true end and swallows the funclet's
        # __CxxFrameHandler/__ehfuncinfo$ word pair -- reported as two trailing
        # `<illegal>` inserts. dtk gives the TARGET side an `except_data_<addr>`
        # symbol there, so only our side is wrong. See the script's docstring
        # for the measured population. LAST in the chain: it only appends to
        # the symbol table and never renames, so it cannot disturb the five
        # name-rewriting patchers above.
        {
            "outputs": str(stamp_dir / "eh_boundary_patched.stamp"),
            "rule": "run_script",
            "implicit": [
                "scripts/obj_eh_boundary_patcher.py",
                str(stamp_dir / "atexit_scope_patched.stamp"),
                "all_source",
            ],
            "variables": {
                "cmd": "python3 scripts/obj_eh_boundary_patcher.py --batch --apply",
                "desc": "PATCH EH funclet extent boundaries",
            },
        },
        # ── the assertion, LAST ────────────────────────────────────────────
        # Everything above REPAIRS the tree; nothing above could say whether
        # the repair was complete, and a build that omits a post-processing
        # pass and announces nothing is the failure class this edge exists
        # for.  Two checks, because there are two ways in:
        #
        #   --check  re-runs every patcher in dry-run and fails the build if
        #            any would still change a file.  Catches a regression of
        #            THIS dependency graph, including "someone added a seventh
        #            patcher and forgot the edge".
        #   --emit   records build/<v>/patch_state.json, a content manifest of
        #            every decomp AND target object at the moment the tree was
        #            verified.  Catches what no build-time check can: a tool
        #            that compiles a single TU outside the graph later and
        #            leaves one raw-compiler object behind.  Measured cost of
        #            that on this repo: unit default/BandUI -2.006 pp of
        #            matched_code_percent, ?InitPanels@BandUI@@QAAXXZ 100.0 ->
        #            99.7, from ONE object.
        #
        # The OUTPUT is patch_state.json itself, not a stamp beside it, and
        # there is deliberately no `always`.
        #
        # `always` was the first draft and it was wrong twice over.  It made
        # `ninja post-compile` incapable of being a no-op -- re-running six
        # patcher dry-runs and re-hashing 4,296 objects every time, measured at
        # 15 s -- which matters because scripts/orchestrator/patch_guard.py
        # calls this on every measurement.  Without it the edge is a true no-op
        # on a consistent tree, because the ordinary dependency logic already
        # covers the case it was there for: the patchers preserve each object's
        # mtime, so a targeted single-object build leaves that object NEWER
        # than every patch stamp, which dirties `all_source` and re-fires this
        # edge along with the rest of the chain.
        #
        # Naming patch_state.json as the output rather than a stamp closes the
        # one gap that reasoning leaves: if the manifest is deleted, a stamp
        # would still look clean and nothing would re-emit it, whereas a
        # missing output is rebuilt.
        {
            "outputs": str(stamp_dir / "patch_state.json"),
            "rule": "run_script",
            "implicit": [
                "scripts/verify_objs_patched.py",
                str(stamp_dir / "eh_boundary_patched.stamp"),
                "all_source",
            ],
            "variables": {
                "cmd": "python3 scripts/verify_objs_patched.py --check --emit",
                "desc": "VERIFY objs are a fixed point of the patch chain",
            },
        },
    ],
}

# Object files
Matching = True
Equivalent = config.non_matching
NonMatching = False

config.warn_missing_config = True
config.warn_missing_source = False

def get_object_completed(status: str) -> bool:
    if status == "MISSING":
        return NonMatching
    elif status == "Matching":
        return Matching
    elif status == "NonMatching":
        return NonMatching
    elif status == "Equivalent":
        return Equivalent
    elif status == "LinkIssues":
        return NonMatching

    assert False, f"Invalid object status {status}"

libs: list[dict] = []
# progress_category is DERIVED from the source path (tools/source_category.py),
# never inherited from the objects.json library group -- see the note at the
# per-object call site below.
sys.path.insert(0, str(Path(__file__).parent / "tools"))
from source_category import category_for_source  # noqa: E402
_category_retagged: list[tuple[str, Optional[str], str]] = []
objects: dict[str, dict] = json.load(open(objects_path, "r", encoding="utf-8"))
for (lib, lib_config) in objects.items():
    # config_cflags: str | list[str]
    config_cflags: list[str] = lib_config.pop("cflags")
    lib_cflags = get_cflags(config_cflags) if isinstance(config_cflags, str) else config_cflags

    lib_objects: list[Object] = []
    # config_objects: dict[str, str | dict]
    config_objects: dict[str, Union[str, dict[str, Union[str, Any]]]] = lib_config.pop("objects")
    if len(config_objects) < 1:
        continue

    for (path, obj_config) in config_objects.items():
        if isinstance(obj_config, str):
            completed = get_object_completed(obj_config)
            obj_config = {}
        else:
            completed = get_object_completed(obj_config["status"])

            if "cflags" in obj_config:
                object_cflags = obj_config["cflags"]
                if isinstance(object_cflags, str):
                    obj_config["cflags"] = get_cflags(object_cflags)

        # Derive progress_category from the SOURCE PATH, not from which
        # library group somebody listed the object in (lane CATTAG-1). The
        # group tag is a hand-maintained value with no tie to the path, so it
        # drifted: 10 objects were tagged on the wrong side, including 13,692 B
        # of src/band3/ game code counted as engine and the whole `xdk` group
        # counted as engine rather than sdk. Deriving makes drift impossible --
        # to change a file's tier you move the file. Resolution mirrors
        # tools/project.py exactly: src_dir / (source or <object key>), with
        # src_dir defaulting object -> library -> "src". NEVER basename().
        _src_dir = obj_config.get("src_dir") or lib_config.get("src_dir") or "src"
        _derived_category = category_for_source(
            f"{_src_dir}/{obj_config.get('source') or path}"
        )
        if _derived_category is not None:
            # An explicit per-object progress_category still wins, so a genuine
            # exception stays expressible -- there are currently zero.
            obj_config.setdefault("progress_category", _derived_category)
            if _derived_category != lib_config.get("progress_category"):
                _category_retagged.append(
                    (f"{_src_dir}/{obj_config.get('source') or path}",
                     lib_config.get("progress_category"), _derived_category)
                )

        lib_objects.append(Object(completed, path, **obj_config))

    libs.append({
        "lib": lib,
        "cflags": lib_cflags,
        "host": False,
        "objects": lib_objects,
        **lib_config
    })

config.libs = libs

# objects.json pins "mw_version" explicitly on each library group, which beats
# tools/project.py's set_default(..., config.linker_version) -- so setting
# config.linker_version alone would flip only the global build.ninja variable
# and leave all ~1094 per-object edges on the old compiler. When (and only
# when) the opt-in switch is active, retarget those explicit pins too. On the
# default path this loop does not run at all, so generated output is untouched.
if _x360_version != DEFAULT_X360_COMPILER_VERSION:
    _retargeted = 0
    for _lib in config.libs:
        if _lib.get("mw_version"):
            _lib["mw_version"] = _x360_version
            _retargeted += 1
        for _obj in _lib.get("objects", []):
            if getattr(_obj, "options", {}).get("mw_version"):
                _obj.options["mw_version"] = _x360_version
                _retargeted += 1
    print(
        f"NOTE: retargeted {_retargeted} explicit mw_version pin(s) to "
        f"{_x360_version}",
        file=sys.stderr,
    )

# Progress tracking categories. We still COMPUTE them (report.json categories are
# consumed by other tooling) but do NOT print the dtk per-category lines: their
# denominators are the *declared/pinned* units only (e.g. "Milo Engine 21%" is
# matched / 0.93 MB declared, not / the true 3.4 MB engine tier), which misreads as
# tier coverage. The authoritative per-tier view comes from the full-binary MAP via
# tools/scope_map.py (printed below in the progress step). dtk's "All" line stays.
config.progress_categories = [ProgressCategory(name, desc) for (name, desc) in progress_categories.items()]
config.print_progress_categories = False

# Announce every object whose path-derived category disagrees with the library
# group it is listed under. Silence here is the healthy state; a line is not an
# error, it is a group tag that has drifted from the tree and should be fixed in
# objects.json (or the file moved). Audit in full: tools/source_category.py audit
#
# ⚠ ONE DISAGREEMENT IS STRUCTURAL AND IS **NOT** DRIFT (lane VENDTIER-1). A
# library group is a CFLAGS grouping; `thirdparty` is a finer tier that cuts
# ACROSS groups, because the vendored libs are compiled with the engine's flags
# and are listed in the `engine` group. That group is now legitimately
# tier-heterogeneous (732 engine + 28 thirdparty), so there is no group tag it
# could carry that would make the comparison agree.
#
# Listing those 28+2 as drift would print 30 permanent lines on every single
# configure -- which destroys the only property that makes this notice worth
# reading, namely that SILENCE IS THE HEALTHY STATE. A warning that always fires
# is a warning nobody reads. So the thirdparty refinement is SUMMARISED in one
# line and genuine drift is still listed in full.
_tp_refine = [r for r in _category_retagged if r[2] == "thirdparty"]
_real_drift = [r for r in _category_retagged if r[2] != "thirdparty"]
if _tp_refine:
    print(
        f"NOTE: {len(_tp_refine)} vendored object(s) refine their library "
        f"group's tier to `thirdparty` (expected: groups are cflags groupings "
        f"and are coarser than tiers). Detail: tools/source_category.py audit",
        file=sys.stderr,
    )
if _real_drift:
    print(
        f"NOTE: progress_category derived from source path overrides the "
        f"library group tag for {len(_real_drift)} object(s):",
        file=sys.stderr,
    )
    for _sp, _was, _now in sorted(_real_drift):
        print(f"        {_sp}: {_was} -> {_now}", file=sys.stderr)
config.progress_each_module = args.verbose

if args.mode == "configure":
    # Write build.ninja and objdiff.json
    generate_build(config)
elif args.mode == "progress":
    # tools/scope_map.py prints the single unified decomp dashboard ninja shows after
    # every build: headline matched/fuzzy/fns (== ninja "All"), gains-today, and the
    # per-tier (oracle-backed vs no-oracle) breakdown. calculate_progress() is the
    # canonical dtk printer, kept ONLY as a fallback if scope_map fails -- a build
    # must never be left with no progress output.
    rc = 1
    try:
        import subprocess

        rc = subprocess.run(
            [sys.executable, "tools/scope_map.py", "priority"],
            cwd=os.path.dirname(os.path.abspath(__file__)),
            check=False,
        ).returncode
    except Exception as e:
        print(f"(scope_map dashboard failed, falling back: {e})")
    if rc != 0:
        calculate_progress(config)
else:
    sys.exit("Unknown mode: " + args.mode)
