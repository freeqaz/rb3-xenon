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
def _find_local_fork(repo_name: str) -> Optional[Path]:
    """Return the sibling fork checkout `<repo_name>/` (the directory holding
    its Cargo.toml), searching upward from this file's directory. Walks parents
    -- not just the direct sibling -- so worktrees nested under
    .claude/worktrees/<name>/ still resolve up to the shared ../../../../jeff
    and ../../../../objdiff checkouts. Returns None if no checkout is found, in
    which case the build falls back to a downloaded fork release."""
    cur = Path(__file__).resolve().parent
    while True:
        candidate = cur.parent / repo_name / "Cargo.toml"
        if candidate.is_file():
            return candidate.parent
        if cur.parent == cur:
            return None
        cur = cur.parent

def _default_dtk_path() -> Optional[Path]:
    return _find_local_fork("jeff")

def _default_objdiff_path() -> Optional[Path]:
    return _find_local_fork("objdiff")

# Apply arguments
config.build_dir = args.build_dir
config.dtk_path = args.dtk if args.dtk is not None else _default_dtk_path()
config.objdiff_path = args.objdiff if args.objdiff is not None else _default_objdiff_path()

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
    print(
        "WARN: local objdiff fork (../objdiff) not found; falling back to a "
        "downloaded objdiff-cli release from freeqaz/objdiff. Clone the fork or "
        "pass --objdiff.",
        file=sys.stderr,
    )
config.binutils_path = args.binutils
config.compilers_path = args.compilers
config.generate_map = args.map
config.non_matching = args.non_matching
config.sjiswrap_path = args.sjiswrap
config.ninja_path = args.ninja
config.progress = args.progress
if not is_windows():
    config.wrapper = args.wrapper
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
            "variables": {
                "cmd": "python3 scripts/obj_dynamic_init_patcher.py --batch --apply",
                "desc": "PATCH ??__E dynamic initializers STATIC->EXTERNAL",
            },
        },
        {
            "outputs": str(stamp_dir / "guard_patched.stamp"),
            "rule": "run_script",
            "order_only": "all_source",
            "variables": {
                "cmd": "python3 scripts/obj_guard_patcher.py --batch --apply",
                "desc": "PATCH $S guard variables to match ??_B naming",
            },
        },
        {
            "outputs": str(stamp_dir / "bool_mangle_patched.stamp"),
            "rule": "run_script",
            "order_only": "all_source",
            "variables": {
                "cmd": "python3 scripts/obj_bool_mangle_patcher.py --batch --apply",
                "desc": "PATCH bool parameter back-reference mangling",
            },
        },
        {
            "outputs": str(stamp_dir / "atexit_scope_patched.stamp"),
            "rule": "run_script",
            "order_only": "all_source",
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

# Progress tracking categories
config.progress_categories = [ProgressCategory(name, desc) for (name, desc) in progress_categories.items()]
config.progress_each_module = args.verbose

if args.mode == "configure":
    # Write build.ninja and objdiff.json
    generate_build(config)
elif args.mode == "progress":
    # Print progress and write progress.json
    calculate_progress(config)
else:
    sys.exit("Unknown mode: " + args.mode)
