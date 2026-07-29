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
    consumed, so `matched_functions` over-counts by ~4% (measured: 39,743
    reported vs 38,210 honest, all of it anonymous `fn_` symbols). Our fork
    (branch `oversub-disclosure`) populates `Measures.masked_equal_functions`
    so EVERY report states its own honest floor
    (`matched_functions - masked_equal_functions`).

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
            "masked_equal_functions, so matched_functions will over-count by "
            "~4% with nothing flagging it. Do not quote those numbers.",
            file=sys.stderr,
        )
        return
    sys.exit(
        "FATAL: local objdiff fork (../objdiff) not found.\n"
        "  The downloaded objdiff-cli release does NOT populate\n"
        "  Measures.masked_equal_functions, so its reports silently over-count\n"
        "  matched_functions by ~4% (funclet over-subscription) with nothing\n"
        "  flagging it -- a silently-wrong number is worse than no number.\n"
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
config.dtk_tag = "v0.3.0"
config.objdiff_tag = "v4.2.2"  # freeqaz/objdiff fork release (linux-x86_64 asset)
config.sjiswrap_tag = "v1.2.1"
config.wibo_tag = "1.0.1"

# Project
config_dir = Path("config") / config.version
config_json_path = config_dir / "config.json"
objects_path = config_dir / "objects.json"
config.config_path = config_dir / "config.yml"
config.check_sha_path = config_dir / "build.sha1"
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

config.linker_version = "X360/16.00.11886.00"

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
    "post-compile": [
        {
            "outputs": str(stamp_dir / "anon_ns_patched.stamp"),
            "rule": "run_script",
            "order_only": "all_source",
            "variables": {
                "cmd": "python3 scripts/obj_anon_ns_patcher.py --batch --apply",
                "desc": "PATCH anonymous namespace hashes",
            },
        },
        {
            "outputs": str(stamp_dir / "dynamic_init_patched.stamp"),
            "rule": "run_script",
            "order_only": "all_source",
            "implicit": str(stamp_dir / "anon_ns_patched.stamp"),
            "variables": {
                "cmd": "python3 scripts/obj_dynamic_init_patcher.py --batch --apply",
                "desc": "PATCH ??__E dynamic initializers STATIC->EXTERNAL",
            },
        },
        {
            "outputs": str(stamp_dir / "guard_patched.stamp"),
            "rule": "run_script",
            "order_only": "all_source",
            "implicit": str(stamp_dir / "dynamic_init_patched.stamp"),
            "variables": {
                "cmd": "python3 scripts/obj_guard_patcher.py --batch --apply",
                "desc": "PATCH $S guard variables to match ??_B naming",
            },
        },
        {
            "outputs": str(stamp_dir / "bool_mangle_patched.stamp"),
            "rule": "run_script",
            "order_only": "all_source",
            "implicit": str(stamp_dir / "guard_patched.stamp"),
            "variables": {
                "cmd": "python3 scripts/obj_bool_mangle_patcher.py --batch --apply",
                "desc": "PATCH bool parameter back-reference mangling",
            },
        },
        {
            "outputs": str(stamp_dir / "atexit_scope_patched.stamp"),
            "rule": "run_script",
            "order_only": "all_source",
            "implicit": str(stamp_dir / "bool_mangle_patched.stamp"),
            "variables": {
                "cmd": "python3 scripts/obj_atexit_scope_patcher.py --batch --apply",
                "desc": "PATCH ??__F atexit scope counters (fuzzy match)",
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
            lib_objects.append(Object(completed, path))
        else:
            completed = get_object_completed(obj_config["status"])

            if "cflags" in obj_config:
                object_cflags = obj_config["cflags"]
                if isinstance(object_cflags, str):
                    obj_config["cflags"] = get_cflags(object_cflags)

            lib_objects.append(Object(completed, path, **obj_config))

    libs.append({
        "lib": lib,
        "cflags": lib_cflags,
        "host": False,
        "objects": lib_objects,
        **lib_config
    })

config.libs = libs

# Progress tracking categories. We still COMPUTE them (report.json categories are
# consumed by other tooling) but do NOT print the dtk per-category lines: their
# denominators are the *declared/pinned* units only (e.g. "Milo Engine 21%" is
# matched / 0.93 MB declared, not / the true 3.4 MB engine tier), which misreads as
# tier coverage. The authoritative per-tier view comes from the full-binary MAP via
# tools/scope_map.py (printed below in the progress step). dtk's "All" line stays.
config.progress_categories = [ProgressCategory(name, desc) for (name, desc) in progress_categories.items()]
config.print_progress_categories = False
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
