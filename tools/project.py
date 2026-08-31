###
# decomp-toolkit project generator
# Generates build.ninja and objdiff.json.
#
# This generator is intentionally project-agnostic
# and shared between multiple projects. Any configuration
# specific to a project should be added to `configure.py`.
#
# If changes are made, please submit a PR to
# https://github.com/rjkiv/jeff-template
###

import io
import json
import math
import os
import platform
import subprocess
import sys
from pathlib import Path
from typing import (
    Any,
    Callable,
    cast,
    Dict,
    IO,
    Iterable,
    List,
    Optional,
    Set,
    Tuple,
    TypedDict,
    Union,
)

from . import ninja_syntax
from .ninja_syntax import serialize_path

if sys.platform == "cygwin":
    sys.exit(
        f"Cygwin/MSYS2 is not supported."
        f"\nPlease use native Windows Python instead."
        f"\n(Current path: {sys.executable})"
    )

Library = Dict[str, Any]


class Object:
    def __init__(self, completed: bool, name: str, **options: Any) -> None:
        self.name = name
        self.completed = completed
        self.options: Dict[str, Any] = {
            "add_to_all": None,
            "asflags": None,
            "asm_dir": None,
            "cflags": None,
            "extab_padding": None,
            "extra_asflags": [],
            "extra_cflags": [],
            "extra_clang_flags": [],
            "lib": None,
            "mw_version": None,
            "progress_category": None,
            "scratch_preset_id": None,
            "shift_jis": None,
            "source": name,
            "src_dir": None,
        }
        self.options.update(options)

        # Internal
        self.src_path: Optional[Path] = None
        self.asm_path: Optional[Path] = None
        self.src_obj_path: Optional[Path] = None
        self.asm_obj_path: Optional[Path] = None
        self.ctx_path: Optional[Path] = None

    def resolve(self, config: "ProjectConfig", lib: Library) -> "Object":
        # Use object options, then library options
        obj = Object(self.completed, self.name, **lib)
        for key, value in self.options.items():
            if value is not None or key not in obj.options:
                obj.options[key] = value

        # Use default options from config
        def set_default(key: str, value: Any) -> None:
            if obj.options[key] is None:
                obj.options[key] = value

        set_default("add_to_all", True)
        set_default("asflags", config.asflags)
        set_default("asm_dir", config.asm_dir)
        set_default("extab_padding", None)
        set_default("mw_version", config.linker_version)
        set_default("scratch_preset_id", config.scratch_preset_id)
        set_default("shift_jis", config.shift_jis)
        set_default("src_dir", config.src_dir)

        # Validate progress categories
        def check_category(category: str):
            if not any(category == c.id for c in config.progress_categories):
                sys.exit(
                    f"Progress category '{category}' missing from config.progress_categories"
                )

        progress_category = obj.options["progress_category"]
        if isinstance(progress_category, list):
            for category in progress_category:
                check_category(category)
        elif progress_category is not None:
            check_category(progress_category)

        # Resolve paths
        build_dir = config.out_path()
        obj.src_path = Path(obj.options["src_dir"]) / obj.options["source"]
        if obj.options["asm_dir"] is not None:
            obj.asm_path = (
                Path(obj.options["asm_dir"]) / obj.options["source"]
            ).with_suffix(".s")
        base_name = Path(self.name).with_suffix("")
        obj.src_obj_path = build_dir / "src" / f"{base_name}.obj"
        obj.asm_obj_path = build_dir / "mod" / f"{base_name}.obj"
        obj.ctx_path = build_dir / "src" / f"{base_name}.ctx"
        return obj


class ProgressCategory:
    def __init__(self, id: str, name: str) -> None:
        self.id = id
        self.name = name


class ProjectConfig:
    def __init__(self) -> None:
        # Paths
        self.build_dir: Path = Path("build")  # Output build files
        self.src_dir: Path = Path("src")  # C/C++/asm source files
        self.tools_dir: Path = Path("tools")  # Python scripts
        self.asm_dir: Optional[Path] = Path(
            "asm"
        )  # Override incomplete objects (for modding)

        # Tooling
        self.binutils_tag: Optional[str] = None  # Git tag
        self.binutils_path: Optional[Path] = None  # If None, download
        self.dtk_tag: Optional[str] = None  # Git tag
        self.dtk_path: Optional[Path] = None  # If None, download
        self.compilers_tag: Optional[str] = None  # 1
        self.compilers_path: Optional[Path] = None  # If None, download
        self.wibo_tag: Optional[str] = None  # Git tag
        self.wrapper: Optional[Path] = None  # If None, download wibo on Linux
        self.sjiswrap_tag: Optional[str] = None  # Git tag
        self.sjiswrap_path: Optional[Path] = None  # If None, download
        self.ninja_path: Optional[Path] = None  # If None, use system PATH
        self.objdiff_tag: Optional[str] = None  # Git tag
        self.objdiff_path: Optional[Path] = None  # If None, download
        self.objcache_path: Optional[Path] = None  # Prebuilt objcache binary; None disables the cache

        # Project config
        self.non_matching: bool = False
        self.build_rels: bool = True  # Build REL files
        self.check_sha_path: Optional[Path] = None  # Path to version.sha1
        self.config_path: Optional[Path] = None  # Path to config.yml
        self.generate_map: bool = False  # Generate map file(s)
        self.asflags: Optional[List[str]] = None  # Assembler flags
        self.ldflags: Optional[List[str]] = None  # Linker flags
        self.libs: Optional[List[Library]] = None  # List of libraries
        self.linker_version: Optional[str] = None  # mwld version
        self.version: Optional[str] = None  # Version name
        self.warn_missing_config: bool = False  # Warn on missing unit configuration
        self.warn_missing_source: bool = False  # Warn on missing source file
        self.rel_strip_partial: bool = True  # Generate PLFs with -strip_partial
        self.rel_empty_file: Optional[str] = (
            None  # Object name for generating empty RELs
        )
        self.shift_jis = (
            True  # Convert source files from UTF-8 to Shift JIS automatically
        )
        self.reconfig_deps: Optional[List[Path]] = (
            None  # Additional re-configuration dependency files
        )
        self.custom_build_rules: Optional[List[Dict[str, Any]]] = (
            None  # Custom ninja build rules
        )
        self.custom_build_steps: Optional[Dict[str, List[Dict[str, Any]]]] = (
            None  # Custom build steps, types are ["pre-compile", "post-compile", "post-link", "post-build"]
        )
        self.generate_compile_commands: bool = (
            True  # Generate compile_commands.json for clangd
        )
        self.extra_clang_flags: List[str] = []  # Extra flags for clangd
        # Precompiled header (PCH) support (ported from dc3-decomp)
        self.pch_header: Optional[str] = None       # e.g. "decomp_pch.h"
        self.pch_source: Optional[Path] = None      # e.g. Path("src/system/decomp_pch.cpp")
        self.pch_eligible_dirs: Optional[Set[str]] = None  # parent-dir basenames eligible for PCH
        self.scratch_preset_id: Optional[int] = (
            None  # Default decomp.me preset ID for scratches
        )
        self.link_order_callback: Optional[Callable[[int, List[str]], List[str]]] = (
            None  # Callback to add/remove/reorder units within a module
        )

        # Progress output and report.json config
        self.progress = True  # Enable report.json generation and CLI progress output
        self.progress_modules: bool = True  # Include combined "modules" category
        self.progress_each_module: bool = (
            False  # Include individual modules, disable for large numbers of modules
        )
        self.progress_categories: List[ProgressCategory] = []  # Additional categories
        self.print_progress_categories: Union[bool, List[str]] = (
            True  # Print additional progress categories in the CLI progress output
        )
        self.progress_report_args: Optional[List[str]] = (
            None  # Flags to `objdiff-cli report generate`
        )

        # Progress fancy printing
        self.progress_use_fancy: bool = False
        self.progress_code_fancy_frac: int = 0
        self.progress_code_fancy_item: str = ""
        self.progress_data_fancy_frac: int = 0
        self.progress_data_fancy_item: str = ""

    def validate(self) -> None:
        required_attrs = [
            "build_dir",
            "src_dir",
            "tools_dir",
            "check_sha_path",
            "config_path",
            "ldflags",
            "linker_version",
            "libs",
            "version",
        ]
        for attr in required_attrs:
            if getattr(self, attr) is None:
                sys.exit(f"ProjectConfig.{attr} missing")

    # Creates a map of object names to Object instances
    # Options are fully resolved from the library and object
    def objects(self) -> Dict[str, Object]:
        out = {}
        for lib in self.libs or {}:
            objects: List[Object] = lib["objects"]
            for obj in objects:
                if obj.name in out:
                    sys.exit(f"Duplicate object name {obj.name}")
                out[obj.name] = obj.resolve(self, lib)
        return out

    # Gets the output path for build-related files.
    def out_path(self) -> Path:
        return self.build_dir / str(self.version)

    # Gets the path to the compilers directory.
    # Exits the program if neither `compilers_path` nor `compilers_tag` is provided.
    def compilers(self) -> Path:
        if self.compilers_path:
            return self.compilers_path
        elif self.compilers_tag:
            return self.build_dir / "compilers"
        else:
            sys.exit("ProjectConfig.compilers_tag missing")

    # Gets the wrapper to use for compiler commands, if set.
    def compiler_wrapper(self) -> Optional[Path]:
        wrapper = self.wrapper

        if self.use_wibo():
            wrapper = self.build_dir / "tools" / "wibo"
        if not is_windows() and wrapper is None:
            wrapper = Path("wine")

        return wrapper

    # Determines whether or not to use wibo as the compiler wrapper.
    def use_wibo(self) -> bool:
        return (
            self.wibo_tag is not None
            and sys.platform == "linux"
            and platform.machine() in ("i386", "x86_64")
            and self.wrapper is None
        )


def is_windows() -> bool:
    return os.name == "nt"


# On Windows, we need this to use && in commands
CHAIN = "cmd /c " if is_windows() else ""
# Native executable extension
EXE = ".exe" if is_windows() else ""


def file_is_asm(path: Path) -> bool:
    return path.suffix.lower() == ".s"


def file_is_c(path: Path) -> bool:
    return path.suffix.lower() == ".c"


def file_is_cpp(path: Path) -> bool:
    return path.suffix.lower() in (".cc", ".cp", ".cpp", ".cxx")


def file_is_c_cpp(path: Path) -> bool:
    return file_is_c(path) or file_is_cpp(path)


_listdir_cache = {}


def check_path_case(path: Path):
    parts = path.parts
    if path.is_absolute():
        curr = Path(parts[0])
        start = 1
    else:
        curr = Path(".")
        start = 0

    for part in parts[start:]:
        if curr in _listdir_cache:
            entries = _listdir_cache[curr]
        else:
            try:
                entries = os.listdir(curr)
            except (FileNotFoundError, PermissionError):
                sys.exit(f"Cannot access: {curr}")
            _listdir_cache[curr] = entries

        for entry in entries:
            if entry.lower() == part.lower():
                curr = curr / entry
                break
        else:
            sys.exit(f"Cannot resolve: {path}")

    if path != curr:
        print(f"⚠️  Case mismatch: expected={path} actual={curr}")


def make_flags_str(flags: Optional[List[str]]) -> str:
    if flags is None:
        return ""
    return " ".join(flags)


# Unit configuration
class BuildConfigUnit(TypedDict):
    object: Optional[str]
    name: str
    autogenerated: bool


# Module configuration
class BuildConfigModule(TypedDict):
    name: str
    module_id: int
    ldscript: str
    entry: str
    units: List[BuildConfigUnit]


# Module link configuration
class BuildConfigLink(TypedDict):
    modules: List[str]


# Build configuration generated by decomp-toolkit
class BuildConfig(BuildConfigModule):
    version: str
    modules: List[BuildConfigModule]
    links: List[BuildConfigLink]


# Load decomp-toolkit generated config.json
def load_build_config(
    config: ProjectConfig, build_config_path: Path
) -> Optional[BuildConfig]:
    if not build_config_path.is_file():
        return None

    def versiontuple(v: str) -> Tuple[int, ...]:
        return tuple(map(int, (v.split("."))))

    f = open(build_config_path, "r", encoding="utf-8")
    build_config: BuildConfig = json.load(f)
    config_version = build_config.get("version")
    if config_version is None:
        print("Invalid config.json, regenerating...")
        f.close()
        os.remove(build_config_path)
        return None

    dtk_version = str(config.dtk_tag)[1:]  # Strip v
    if versiontuple(config_version) < versiontuple(dtk_version):
        print("Outdated config.json, regenerating...")
        f.close()
        os.remove(build_config_path)
        return None

    f.close()

    # Apply link order callback
    if config.link_order_callback:
        modules: List[BuildConfigModule] = [build_config, *build_config["modules"]]
        for module in modules:
            unit_names = list(map(lambda u: u["name"], module["units"]))
            unit_names = config.link_order_callback(module["module_id"], unit_names)
            units: List[BuildConfigUnit] = []
            for unit_name in unit_names:
                units.append(
                    # Find existing unit or create a new one
                    next(
                        (u for u in module["units"] if u["name"] == unit_name),
                        {"object": None, "name": unit_name, "autogenerated": False},
                    )
                )
            module["units"] = units

    return build_config


# Generate build.ninja, objdiff.json and compile_commands.json
def generate_build(config: ProjectConfig) -> None:
    config.validate()
    objects = config.objects()
    # dtk's splits.txt uses bare basenames (e.g. "MasterAudio.cpp") while our
    # objects.json uses path-prefixed keys (e.g. "system/beatmatch/MasterAudio.cpp").
    # Add basename aliases so objects.get(basename) resolves to the same Object,
    # which lets generate_objdiff_config populate base_path for diffing.
    #
    # ★ This alias is load-bearing and silently fragile: lane BV-2 measured that
    # 715 of the 719 bare headings in splits.txt resolve ONLY via it. Adding a
    # second source file with the same basename makes the alias ambiguous, which
    # used to degrade to a single `print` among thousands of build lines and an
    # objdiff unit with `base_path: None` that can never pair. We now record the
    # colliding owners so the failure below can say *why*, and hard-fail.
    basename_aliases: Dict[str, Object] = {}
    basename_owners: Dict[str, List[str]] = {}
    for path_key, obj in objects.items():
        basename = Path(path_key).name
        if basename == path_key or basename in objects:
            continue
        basename_owners.setdefault(basename, []).append(path_key)
        # Only alias if unambiguous (single object with this basename)
        if basename in basename_aliases:
            basename_aliases[basename] = None  # type: ignore  # mark ambiguous
        else:
            basename_aliases[basename] = obj
    for basename, obj in basename_aliases.items():
        if obj is not None and basename not in objects:
            objects[basename] = obj
    config._basename_owners = {  # type: ignore[attr-defined]
        k: v for k, v in basename_owners.items() if len(v) > 1
    }
    build_config = load_build_config(config, config.out_path() / "config.json")
    generate_build_ninja(config, objects, build_config)
    generate_objdiff_config(config, objects, build_config)
    generate_compile_commands(config, objects, build_config)


# Generate build.ninja
def generate_build_ninja(
    config: ProjectConfig,
    objects: Dict[str, Object],
    build_config: Optional[BuildConfig],
) -> None:
    out = io.StringIO()
    n = ninja_syntax.Writer(out)
    n.variable("ninja_required_version", "1.3")
    n.newline()

    configure_script = Path(os.path.relpath(os.path.abspath(sys.argv[0])))
    python_lib = Path(os.path.relpath(__file__))
    python_lib_dir = python_lib.parent
    n.comment("The arguments passed to configure.py, for rerunning it.")
    n.variable("configure_args", [f'"\"{arg}\""' if ' ' in arg else arg for arg in sys.argv[1:]])
    # for arg in sys.argv[1:] if arg.contains(' ') wrap in quotes else arg
    n.variable("python", f'"{sys.executable}"')
    n.newline()

    ###
    # Variables
    ###
    n.comment("Variables")
    # n.variable("ldflags", make_flags_str(config.ldflags))
    # if config.linker_version is None:
    #     sys.exit("ProjectConfig.linker_version missing")
    n.variable("mw_version", Path(config.linker_version))
    n.variable("objdiff_report_args", make_flags_str(config.progress_report_args))
    n.newline()

    ###
    # Tooling
    ###
    n.comment("Tooling")

    build_path = config.out_path()
    report_path = build_path / "report.json"
    # The synthetic ICF-alias map objdiff.json's `map_file` points at, its
    # generator, and its source of truth. Rendered both at configure time (so a
    # fresh tree's first objdiff.json can reference it) and by the
    # `icf_alias_map` ninja edge; see the design comment on that edge.
    icf_gen_script = Path("tools") / "gen_symbol_alias_map.py"
    icf_aliases_json = Path("scripts") / "symbol_aliases.json"
    # Both always-dirty gate edges below take `--stamp`/`--stamp-input` instead
    # of ending in `touch $out`, so their stamps move only when the thing they
    # attest to moved. Implementation and rationale: tools/stamp_if_changed.py;
    # the cost of the churn is the CHURN note on the map-injectivity edge.
    # The input list is spelled on the ninja command line, not inferred inside
    # each gate, so build.ninja states what the stamp attests to and it cannot
    # drift out of step with the edge's declared implicit inputs.
    icf_map_path = build_path / "icf_aliases.map"
    icf_map_checked = build_path / "icf_aliases_checked.stamp"
    icf_map_purged = build_path / "icf_aliases_cache_purged.stamp"
    # Global NAME-injectivity assertion over scripts/target_symbol_map.json.
    # Same shape and same neighbourhood as icf_alias_map_checked because it is
    # the same class of defect one file over; see its edge below.
    mapinj_script = Path("tools") / "map_name_injectivity.py"
    mapinj_json = Path("scripts") / "target_symbol_map.json"
    mapinj_checked = build_path / "map_name_injectivity_checked.stamp"
    # Assert the SPLIT TARGET OBJS actually carry the mangled names the renamer
    # is supposed to install. objdiff pairs BY NAME, so virgin `fn_<addr>` objs
    # un-pair essentially every named row -- measured on main 2026-08-21 as
    # matched_functions 22962 / 8.633728% against a true 42198 / 36.730980%, on
    # a SETTLED build with zero errors. The renamer is a no-op on already-renamed
    # objs and its stamp can outlive the objs it attests to, so the only sound
    # check is one that reads the OBJS. Same always-dirty + content-addressed
    # stamp shape as the two gates above.
    renamed_script = Path("tools") / "check_target_objs_renamed.py"
    renamed_checked = build_path / "target_objs_renamed_checked.stamp"

    # The split-currency guard. `dtk xex split` writes build/<v>/obj/** -- the
    # TARGET side of every diff -- but declares only config.json, so nothing in
    # the build graph can tell a reader that those objects came from a
    # DIFFERENT config/<v>/symbols.txt than the one on disk, or that a split is
    # rewriting them right now. Both failure modes are mtime-invisible and both
    # read as a plausible LOWER number rather than an error. See
    # scripts/verify_split_current.py for the 341-function reproduction.
    split_guard_script = Path("scripts") / "verify_split_current.py"
    split_stamp = build_path / "split_inputs.stamp"
    split_checked = build_path / "split_current_checked.stamp"
    # The ruler-agreement guard. `objdiff-cli report generate` and
    # `objdiff-cli diff` carry DIFFERENT hardcoded base configs (report.rs:581
    # vs diff.rs:1070/1807) and BOTH layer objdiff.json's `options` on top, so
    # `options` is the only place that can make them agree. When it pinned only
    # `functionRelocDiffs` the two paths disagreed on 102 functions / 55,604 B
    # here, always with the per-function path LOW. See
    # scripts/verify_ruler_agreement.py.
    ruler_guard_script = Path("scripts") / "verify_ruler_agreement.py"
    ruler_checked = build_path / "ruler_agreement_checked.stamp"
    build_tools_path = config.build_dir / "tools"
    download_tool = config.tools_dir / "download_tool.py"
    n.rule(
        name="download_tool",
        command=f"$python {download_tool} $tool $out --tag $tag",
        description="TOOL $out",
        # restat: in worktrees the download edge re-fires once (no .ninja_log
        # entry) but the setup-script-patched download_tool.py no-ops when the
        # output already exists; restat lets ninja see the unchanged mtime and
        # skip dirtying downstream compile edges for that run. Matches dc3.
        restat=True,
    )

    decompctx = config.tools_dir / "decompctx.py"
    n.rule(
        name="decompctx",
        command=f"$python {decompctx} $in -o $out -d $out.d $includes",
        description="CTX $in",
        depfile="$out.d",
        # No deps="gcc" -- binary deps cache is unsafe under concurrent ninja
        # runs (matches rb3-Wii). Ninja reads $out.d directly each build.
    )

    cargo_rule_written = False

    def write_cargo_rule():
        nonlocal cargo_rule_written
        if not cargo_rule_written:
            n.pool("cargo", 1)
            # NO depfile on purpose. Cargo emits a depfile whose TARGET line is
            # an absolute path (e.g. "/home/.../build/tools/release/dtk: ...")
            # while ninja's build edge declares the output with a relative path
            # ("build/tools/release/dtk"). Ninja rejects the mismatched depfile
            # ("expected depfile to mention X, got <abs path>") and treats the
            # target as perpetually dirty, so `cargo` re-fires on EVERY ninja
            # pass (a ~0.15s no-op) and -- if cargo ever bumps the binary mtime
            # -- cascades into re-SPLIT + reconfigure (config.json -> build.ninja),
            # i.e. a manifest-regeneration loop.
            #
            # Dropping the depfile makes ninja track dtk/objdiff-cli via their
            # explicit input (Cargo.toml) + implicit input (Cargo.lock) only, so
            # cargo no longer re-fires when nothing changed. TRADE-OFF: editing
            # the jeff / objdiff *Rust sources* (`.rs`) will NOT be picked up by
            # ninja (Cargo.toml/lock are unchanged). We rarely touch those forks,
            # so this is a documented manual step -- after editing them, force a
            # rebuild with e.g. `touch ../jeff/Cargo.toml && ninja` (dtk) or
            # `touch ../objdiff/Cargo.toml && ninja` (objdiff-cli). See CLAUDE.md
            # "Two build tracks".
            n.rule(
                name="cargo",
                command="cargo build --release --manifest-path $in --bin $bin --target-dir $target",
                description="CARGO $bin",
                pool="cargo",
                # No depfile (see above) and no deps="gcc" -- ninja's binary
                # deps cache is unsafe under concurrent ninja invocations
                # (matches rb3-Wii).
                restat=True,
            )
            cargo_rule_written = True

    if config.dtk_path is not None and config.dtk_path.is_file():
        dtk = config.dtk_path
    elif config.dtk_path is not None:
        dtk = build_tools_path / "release" / f"dtk{EXE}"
        write_cargo_rule()
        n.build(
            outputs=dtk,
            rule="cargo",
            inputs=config.dtk_path / "Cargo.toml",
            implicit=config.dtk_path / "Cargo.lock",
            variables={
                "bin": "dtk",
                "target": build_tools_path,
            },
        )
    elif config.dtk_tag:
        dtk = build_tools_path / f"dtk{EXE}"
        n.build(
            outputs=dtk,
            rule="download_tool",
            implicit=download_tool,
            variables={
                "tool": "dtk",
                "tag": config.dtk_tag,
            },
        )
    else:
        sys.exit("ProjectConfig.dtk_tag missing")

    if config.objdiff_path is not None and config.objdiff_path.is_file():
        objdiff = config.objdiff_path
    elif config.objdiff_path is not None:
        objdiff = build_tools_path / "release" / f"objdiff-cli{EXE}"
        write_cargo_rule()
        n.build(
            outputs=objdiff,
            rule="cargo",
            inputs=config.objdiff_path / "Cargo.toml",
            implicit=config.objdiff_path / "Cargo.lock",
            variables={
                "bin": "objdiff-cli",
                "target": build_tools_path,
            },
        )
    elif config.objdiff_tag:
        objdiff = build_tools_path / f"objdiff-cli{EXE}"
        n.build(
            outputs=objdiff,
            rule="download_tool",
            implicit=download_tool,
            variables={
                "tool": "objdiff-cli",
                "tag": config.objdiff_tag,
            },
        )
    else:
        sys.exit("ProjectConfig.objdiff_tag missing")

    if config.sjiswrap_path:
        sjiswrap = config.sjiswrap_path
    elif config.sjiswrap_tag:
        sjiswrap = build_tools_path / "sjiswrap.exe"
        n.build(
            outputs=sjiswrap,
            rule="download_tool",
            implicit=download_tool,
            variables={
                "tool": "sjiswrap",
                "tag": config.sjiswrap_tag,
            },
        )
    else:
        sys.exit("ProjectConfig.sjiswrap_tag missing")

    wrapper = config.compiler_wrapper()
    # Only add an implicit dependency on wibo if we download it
    wrapper_implicit: Optional[Path] = None
    if wrapper is not None and config.use_wibo():
        wrapper_implicit = wrapper
        n.build(
            outputs=wrapper,
            rule="download_tool",
            implicit=download_tool,
            variables={
                "tool": "wibo",
                "tag": config.wibo_tag,
            },
        )
    wrapper_cmd = f"{wrapper} " if wrapper else ""

    compilers = config.compilers()
    compilers_implicit: Optional[Path] = None
    if config.compilers_path is None and config.compilers_tag is not None:
        compilers_implicit = compilers
        n.build(
            outputs=compilers,
            rule="download_tool",
            implicit=download_tool,
            variables={
                "tool": "compilers",
                "tag": config.compilers_tag,
            },
        )

    binutils_implicit = None
    if config.binutils_path:
        binutils = config.binutils_path
    elif config.binutils_tag:
        binutils = config.build_dir / "binutils"
        binutils_implicit = binutils
        n.build(
            outputs=binutils,
            rule="download_tool",
            implicit=download_tool,
            variables={
                "tool": "binutils",
                "tag": config.binutils_tag,
            },
        )
    else:
        sys.exit("ProjectConfig.binutils_tag missing")

    n.newline()

    ###
    # Helper rule for downloading all tools
    ###
    n.comment("Download all tools")
    n.build(
        outputs="tools",
        rule="phony",
        inputs=[dtk, sjiswrap, wrapper, compilers, binutils, objdiff],
    )
    n.newline()

    ###
    # Build rules
    ###
    compiler_path = compilers / "$mw_version"

    transform_dep: Optional[Path] = None

    # MWCC
    mwcc = compiler_path / "cl.exe"
    mwcc_cmd = f"{wrapper_cmd}{mwcc} $cflags"
    mwcc_implicit: List[Optional[Path]] = [compilers_implicit or mwcc, wrapper_implicit]

    # MWCC with UTF-8 to Shift JIS wrapper
    mwcc_sjis_cmd = f"{wrapper_cmd}{sjiswrap} {mwcc} $cflags -MMD -c $in -o $basedir"
    mwcc_sjis_implicit: List[Optional[Path]] = [*mwcc_implicit, sjiswrap]

    # MWCC with extab post-processing
    mwcc_extab_cmd = f"{CHAIN}{mwcc_cmd} && {dtk} extab clean --padding \"$extab_padding\" $out $out"
    mwcc_extab_implicit: List[Optional[Path]] = [*mwcc_implicit, dtk]
    mwcc_sjis_extab_cmd = f"{CHAIN}{mwcc_sjis_cmd} && {dtk} extab clean --padding \"$extab_padding\" $out $out"
    mwcc_sjis_extab_implicit: List[Optional[Path]] = [*mwcc_sjis_implicit, dtk]

    # MWLD
    mwld = compiler_path / "mwldeppc.exe"
    mwld_cmd = f"{wrapper_cmd}{mwld} $ldflags -o $out @$out.rsp"
    mwld_implicit: List[Optional[Path]] = [compilers_implicit or mwld, wrapper_implicit]

    # GNU as
    gnu_as = binutils / f"powerpc-eabi-as{EXE}"
    gnu_as_cmd = (
        f"{CHAIN}{gnu_as} $asflags -o $out $in" + f" && {dtk} elf fixup $out $out"
    )
    gnu_as_implicit = [binutils_implicit or gnu_as, dtk]
    # As a workaround for https://github.com/encounter/dtk-template/issues/51
    # include macros.inc directly as an implicit dependency
    gnu_as_implicit.append(build_path / "include" / "macros.inc")

    if os.name != "nt":
        transform_dep = config.tools_dir / "transform_dep.py"
        mwcc_implicit.append(transform_dep)
        mwcc_sjis_implicit.append(transform_dep)
        mwcc_extab_implicit.append(transform_dep)
        mwcc_sjis_extab_implicit.append(transform_dep)


    # n.comment("Link ELF file")
    # n.rule(
    #     name="link",
    #     command=mwld_cmd,
    #     description="LINK $out",
    #     rspfile="$out.rsp",
    #     rspfile_content="$in_newline",
    # )
    # n.newline()

    # n.comment("Generate DOL")
    # n.rule(
    #     name="elf2dol",
    #     command=f"{dtk} elf2dol $in $out",
    #     description="DOL $out",
    # )
    # n.newline()

    # MSVC
    msvc = compiler_path / "cl.exe"
    msvc_cmd = f"{wrapper_cmd}{msvc} $cflags /showIncludes /Fo$out $in"
    if transform_dep is not None:
        # WIBO_FS_CACHE=1: cache wibo's case-insensitive path resolution.
        # Without it every header open in cl.exe pays a directory scan --
        # measured 6.8s vs 0.65s per TU (10.5x) on the same compile, with
        # byte-identical output (only the COFF timestamp bytes differ,
        # same as any recompile). dc3 has run with this cache fleet-wide.
        # WIBO_REWRITE_SHOWINCLUDES=1: wibo rewrites the "Note: including file:"
        # lines in-process -- the same z:-strip + case-fix tools/transform_dep.py
        # did out-of-process, minus the pipe (~29ms/TU) and the per-TU python
        # process. dc3 runs this exact path in production. Verified deps-equivalent
        # to the pipe: the recorded dep path SETS are byte-identical (the only
        # per-line difference is a trailing \r the pipe emits, which ninja's
        # deps=msvc parser strips). REQUIRES the freeqaz/wibo fork binary;
        # configure.py hard-gates the wrapper on the WIBO_FS_CACHE /
        # WIBO_REWRITE_SHOWINCLUDES feature bytes. A stock wibo would silently
        # ignore WIBO_REWRITE_SHOWINCLUDES and feed raw Windows paths to ninja's
        # deps=msvc parser (broken dep tracking) -- which is why this pipe removal
        # ships together with the wrapper default. No bash -c / pipe needed: ninja
        # runs the command through sh -c, so the VAR=1 VAR2=1 env prefix applies
        # to the single cl.exe process.
        # objcache: content-addressed obj cache wrapping the compile edge. The
        # prefix goes BETWEEN the WIBO_* env words and the wrapper+cl so (a) ninja
        # runs the whole line via sh -c, setting WIBO_* env for the objcache
        # process, which propagates the entire environment to the child compile
        # (a W1-B guarantee), and (b) the PCH `.replace()` anchor
        # "$cflags /showIncludes /Fo$out $in" lives at the TAIL of msvc_cmd,
        # untouched, so msvc_pch_create/msvc_pch inherit the prefix and their
        # /Yc//Yu asserts still pass. `objcache exec --fo $out` is authoritative
        # for the output path. Absolute binary path from the same
        # _find_local_fork resolver as dtk/objdiff, so main and worktrees emit
        # identical command strings; if the fork/binary is absent the prefix is
        # empty -> a plain (uncached) compile. Runtime kill switch: `objcache off`.
        objcache_prefix = ""
        if config.objcache_path is not None and config.objcache_path.is_file():
            objcache_prefix = f"{config.objcache_path} exec --fo $out -- "
        msvc_cmd = f"WIBO_FS_CACHE=1 WIBO_REWRITE_SHOWINCLUDES=1 {objcache_prefix}{msvc_cmd}"

    n.comment("MSVC build")
    n.variable("msvc_deps_prefix", "Note: including file:")
    n.rule(
        name="msvc",
        command=msvc_cmd,
        description="MSVC $out",
        # depfile="$basefile.d",
        deps="msvc",
    )
    n.newline()

    # MSVC PCH create rule (ported from dc3 tools/project.py:777-789)
    msvc_pch_create_cmd = msvc_cmd.replace(
        "$cflags /showIncludes /Fo$out $in",
        '/Yc"decomp_pch.h" /Fp$pch_out $cflags /showIncludes /Fo$out $in',
    )
    n.comment("MSVC PCH create")
    n.rule(name="msvc_pch_create", command=msvc_pch_create_cmd,
           description="PCH $pch_out", deps="msvc")
    n.newline()

    # MSVC build-with-PCH rule (ported from dc3 tools/project.py:792-803)
    msvc_pch_cmd = msvc_cmd.replace(
        "$cflags /showIncludes /Fo$out $in",
        '/Yu"decomp_pch.h" /FI"decomp_pch.h" /Fp$pch_file $cflags /showIncludes /Fo$out $in',
    )
    n.comment("MSVC build with PCH")
    n.rule(name="msvc_pch", command=msvc_pch_cmd, description="MSVC $out", deps="msvc")
    n.newline()

    assert "/Yc" in msvc_pch_create_cmd and "/Yu" in msvc_pch_cmd, \
        "PCH replace anchor missing from msvc_cmd"

    # n.comment("MWCC build (with UTF-8 to Shift JIS wrapper)")
    # n.rule(
    #     name="mwcc_sjis",
    #     command=mwcc_sjis_cmd,
    #     description="MWCC $out",
    #     depfile="$basefile.d",
    #     deps="gcc",
    # )
    # n.newline()

    # n.comment("MWCC build (with extab post-processing)")
    # n.rule(
    #     name="mwcc_extab",
    #     command=mwcc_extab_cmd,
    #     description="MWCC $out",
    #     depfile="$basefile.d",
    #     deps="gcc",
    # )
    # n.newline()

    # n.comment("MWCC build (with UTF-8 to Shift JIS wrapper and extab post-processing)")
    # n.rule(
    #     name="mwcc_sjis_extab",
    #     command=mwcc_sjis_extab_cmd,
    #     description="MWCC $out",
    #     depfile="$basefile.d",
    #     deps="gcc",
    # )

    # n.comment("Assemble asm")
    # n.rule(
    #     name="as",
    #     command=gnu_as_cmd,
    #     description="AS $out",
    #     # See https://github.com/encounter/dtk-template/issues/51
    #     # depfile="$out.d",
    #     # deps="gcc",
    # )
    # n.newline()

    if len(config.custom_build_rules or {}) > 0:
        n.comment("Custom project build rules (pre/post-processing)")
    for rule in config.custom_build_rules or {}:
        n.rule(
            name=cast(str, rule.get("name")),
            command=cast(str, rule.get("command")),
            description=rule.get("description", None),
            depfile=rule.get("depfile", None),
            generator=rule.get("generator", False),
            pool=rule.get("pool", None),
            restat=rule.get("restat", False),
            rspfile=rule.get("rspfile", None),
            rspfile_content=rule.get("rspfile_content", None),
            deps=rule.get("deps", None),
        )
        n.newline()

    def write_custom_step(step: str, prev_step: Optional[str] = None) -> None:
        implicit: List[str | Path] = []
        if config.custom_build_steps and step in config.custom_build_steps:
            n.comment(f"Custom build steps ({step})")
            for custom_step in config.custom_build_steps[step]:
                outputs = cast(List[str | Path], custom_step.get("outputs"))

                if isinstance(outputs, list):
                    implicit.extend(outputs)
                else:
                    implicit.append(outputs)

                n.build(
                    outputs=outputs,
                    rule=cast(str, custom_step.get("rule")),
                    inputs=custom_step.get("inputs", None),
                    implicit=custom_step.get("implicit", None),
                    order_only=custom_step.get("order_only", None),
                    variables=custom_step.get("variables", None),
                    implicit_outputs=custom_step.get("implicit_outputs", None),
                    pool=custom_step.get("pool", None),
                    dyndep=custom_step.get("dyndep", None),
                )
                n.newline()
        n.build(
            outputs=step,
            rule="phony",
            inputs=implicit,
            order_only=prev_step,
        )

    # Add all build steps needed before we compile (e.g. processing assets)
    write_custom_step("pre-compile")

    ###
    # PCH build edge
    ###
    pch_path: Optional[Path] = None
    if config.pch_source and config.pch_header:
        pch_dir = build_path / "pch"
        pch_path = pch_dir / "system.pch"
        pch_obj = pch_dir / "decomp_pch.obj"
        # The PCH MUST compile with the SAME flags eligible TUs use. All eligible
        # libs share the resolved 'base' cflags (objects.json: every lib uses
        # cflags key 'base'; zero per-object overrides), so take the 'engine'
        # lib's cflags + /TP -- byte-identical to what c_build produces for an
        # eligible .cpp. Do NOT absolutize /I (dc3 does; we must not): consuming
        # compiles use relative /I from the repo-root cwd and MSVC's PCH
        # consistency check requires create/use include sets to match exactly.
        # Likewise keep /Fp (pch_out / pch_file below) REPO-ROOT-RELATIVE, not
        # .resolve()'d: an absolute /Fp bakes the checkout path into the PCH
        # create+use commands, so main and every worktree get different command
        # hashes for the PCH edge + all ~281 dependents — defeating
        # setup_worktree.sh's .ninja_log seeding (the PCH would rebuild and
        # cascade on every fresh worktree's first build). Relative /Fp is
        # byte-neutral to the compiled dependent objs (verified: only the PCH
        # byproduct decomp_pch.obj, which is not a match target, embeds the
        # path string); cl.exe resolves it against the repo-root cwd like /Fo.
        pch_cflags_str = ""
        if config.libs:
            pch_lib = next((l for l in config.libs if l["lib"] == "engine"), config.libs[0])
            pch_cflags_str = make_flags_str([*pch_lib["cflags"], "/TP"])
        n.comment("Precompiled header")
        n.build(
            outputs=[pch_obj],
            rule="msvc_pch_create",
            inputs=config.pch_source,
            implicit=[*mwcc_implicit],
            implicit_outputs=[pch_path],
            variables={"cflags": pch_cflags_str, "pch_out": str(pch_path)},
            order_only="pre-compile",
        )
        n.newline()

    ###
    # Source files
    ###
    n.comment("Source files")

    def map_path(path: Path) -> Path:
        return path.parent / (path.name + ".MAP")

    class LinkStep:
        def __init__(self, config: BuildConfigModule) -> None:
            self.name = config["name"]
            self.module_id = config["module_id"]
            self.ldscript: Optional[Path] = Path(config["ldscript"])
            self.entry = config["entry"]
            self.inputs: List[str] = []

        def add(self, obj: Path) -> None:
            self.inputs.append(serialize_path(obj))

        def output(self) -> Path:
            if self.module_id == 0:
                return build_path / f"{self.name}.dol"
            else:
                return build_path / self.name / f"{self.name}.rel"

        def partial_output(self) -> Path:
            if self.module_id == 0:
                return build_path / f"{self.name}.elf"
            else:
                return build_path / self.name / f"{self.name}.plf"

        def write(self, n: ninja_syntax.Writer) -> None:
            n.comment(f"Link {self.name}")
            if self.module_id == 0:
                elf_path = build_path / f"{self.name}.elf"
                elf_ldflags = f"$ldflags -lcf {serialize_path(self.ldscript)}"
                if config.generate_map:
                    elf_map = map_path(elf_path)
                    elf_ldflags += f" -map {serialize_path(elf_map)}"
                else:
                    elf_map = None
                n.build(
                    outputs=elf_path,
                    rule="link",
                    inputs=self.inputs,
                    implicit=[
                        self.ldscript,
                        *mwld_implicit,
                    ],
                    implicit_outputs=elf_map,
                    variables={"ldflags": elf_ldflags},
                    order_only="post-compile",
                )
            else:
                preplf_path = build_path / self.name / f"{self.name}.preplf"
                plf_path = build_path / self.name / f"{self.name}.plf"
                preplf_ldflags = "$ldflags -sdata 0 -sdata2 0 -r"
                plf_ldflags = f"$ldflags -sdata 0 -sdata2 0 -r1 -lcf {serialize_path(self.ldscript)}"
                if self.entry:
                    plf_ldflags += f" -m {self.entry}"
                    # -strip_partial is only valid with -m
                    if config.rel_strip_partial:
                        plf_ldflags += " -strip_partial"
                if config.generate_map:
                    preplf_map = map_path(preplf_path)
                    preplf_ldflags += f" -map {serialize_path(preplf_map)}"
                    plf_map = map_path(plf_path)
                    plf_ldflags += f" -map {serialize_path(plf_map)}"
                else:
                    preplf_map = None
                    plf_map = None
                n.build(
                    outputs=preplf_path,
                    rule="link",
                    inputs=self.inputs,
                    implicit=mwld_implicit,
                    implicit_outputs=preplf_map,
                    variables={"ldflags": preplf_ldflags},
                    order_only="post-compile",
                )
                n.build(
                    outputs=plf_path,
                    rule="link",
                    inputs=self.inputs,
                    implicit=[self.ldscript, preplf_path, *mwld_implicit],
                    implicit_outputs=plf_map,
                    variables={"ldflags": plf_ldflags},
                    order_only="post-compile",
                )
            n.newline()

    link_outputs: List[Path] = []
    if build_config:
        link_steps: List[LinkStep] = []
        used_compiler_versions: Set[str] = set()
        source_inputs: List[Path] = []
        source_added: Set[Path] = set()

        def c_build(obj: Object, src_path: Path) -> Optional[Path]:
            # Avoid creating duplicate build rules
            if obj.src_obj_path is None or obj.src_obj_path in source_added:
                return obj.src_obj_path
            source_added.add(obj.src_obj_path)

            cflags = obj.options["cflags"]
            extra_cflags = obj.options["extra_cflags"]

            # Add appropriate language flag if it doesn't exist already
            # Added directly to the source so it flows to other generation tasks
            def is_lang_flag(flag):
                return flag.startswith("-lang") or flag in ("/TP", "/TC", "/Tp", "/Tc")

            if not any(is_lang_flag(flag) for flag in cflags) and not any(
                is_lang_flag(flag) for flag in extra_cflags
            ):
                # Ensure extra_cflags is a unique instance,
                # and insert into there to avoid modifying shared sets of flags
                extra_cflags = obj.options["extra_cflags"] = list(extra_cflags)
                if file_is_cpp(src_path):
                    extra_cflags.insert(0, "/TP")
                else:
                    extra_cflags.insert(0, "/TC")

            all_cflags = cflags + extra_cflags
            cflags_str = make_flags_str(all_cflags)
            used_compiler_versions.add(obj.options["mw_version"])

            # Add MSVC build rule
            lib_name = obj.options["lib"]
            build_rule = "msvc"
            build_implcit = mwcc_implicit
            variables = {
                "mw_version": Path(obj.options["mw_version"]),
                "cflags": cflags_str,
                "basedir": os.path.dirname(obj.src_obj_path),
                "basefile": obj.src_obj_path.with_suffix(""),
            }

            if obj.options["shift_jis"] and obj.options["extab_padding"] is not None:
                build_rule = "mwcc_sjis_extab"
                build_implcit = mwcc_sjis_extab_implicit
                variables["extab_padding"] = "".join(f"{i:02x}" for i in obj.options["extab_padding"])
            elif obj.options["shift_jis"]:
                build_rule = "mwcc_sjis"
                build_implcit = mwcc_sjis_implicit
            elif obj.options["extab_padding"] is not None:
                build_rule = "mwcc_extab"
                build_implcit = mwcc_extab_implicit
                variables["extab_padding"] = "".join(f"{i:02x}" for i in obj.options["extab_padding"])

            # Use PCH for eligible files (plain msvc rule, C++ mode, eligible dir)
            pch_implicit: List[Optional[Path]] = []
            if (
                pch_path is not None
                and build_rule == "msvc"
                and file_is_cpp(src_path)
                and "/TC" not in all_cflags
                and config.pch_eligible_dirs
                and src_path.parent.name in config.pch_eligible_dirs
            ):
                build_rule = "msvc_pch"
                variables["pch_file"] = str(pch_path)
                pch_implicit = [pch_path]

            n.comment(f"{obj.name}: {lib_name} (linked {obj.completed})")
            n.build(
                outputs=obj.src_obj_path,
                rule=build_rule,
                inputs=src_path,
                variables=variables,
                implicit=[*build_implcit, *pch_implicit],
                order_only="pre-compile",
            )

            # Add ctx build rule
            if obj.ctx_path is not None:
                include_dirs = []
                for flag in all_cflags:
                    if (
                        flag.startswith("-i ")
                        or flag.startswith("-I ")
                        or flag.startswith("-I+")
                    ):
                        include_dirs.append(flag[3:])
                    elif flag.startswith("/I"):
                        include_dirs.append(flag[2:].lstrip())
                includes = " ".join([f"-I {d}" for d in include_dirs])
                n.build(
                    outputs=obj.ctx_path,
                    rule="decompctx",
                    inputs=src_path,
                    implicit=decompctx,
                    variables={"includes": includes},
                )
            n.newline()

            if obj.options["add_to_all"]:
                source_inputs.append(obj.src_obj_path)

            return obj.src_obj_path

        def asm_build(
            obj: Object, src_path: Path, obj_path: Optional[Path]
        ) -> Optional[Path]:
            if obj.options["asflags"] is None:
                sys.exit("ProjectConfig.asflags missing")
            asflags_str = make_flags_str(obj.options["asflags"])
            if len(obj.options["extra_asflags"]) > 0:
                extra_asflags_str = make_flags_str(obj.options["extra_asflags"])
                asflags_str += " " + extra_asflags_str

            # Avoid creating duplicate build rules
            if obj_path is None or obj_path in source_added:
                return obj_path
            source_added.add(obj_path)

            # Add assembler build rule
            lib_name = obj.options["lib"]
            n.comment(f"{obj.name}: {lib_name} (linked {obj.completed})")
            n.build(
                outputs=obj_path,
                rule="as",
                inputs=src_path,
                variables={"asflags": asflags_str},
                implicit=gnu_as_implicit,
                order_only="pre-compile",
            )
            n.newline()

            if obj.options["add_to_all"]:
                source_inputs.append(obj_path)

            return obj_path

        # Splits headings that resolved to no objects.json Object. Collected so
        # the run can end with one loud summary + non-zero exit instead of a
        # single print lost in thousands of build lines (see check below).
        unresolved_units: List[str] = []

        def add_unit(build_obj: BuildConfigUnit, link_step: LinkStep):
            obj_path, obj_name = build_obj["object"], build_obj["name"]
            obj = objects.get(obj_name)
            if obj is None:
                if config.warn_missing_config and not build_obj["autogenerated"]:
                    print(f"Missing configuration for {obj_name}")
                    unresolved_units.append(obj_name)
                if obj_path is not None:
                    link_step.add(Path(obj_path))
                return

            link_built_obj = obj.completed
            built_obj_path: Optional[Path] = None
            if obj.src_path is not None and obj.src_path.exists():
                check_path_case(obj.src_path)
                if file_is_c_cpp(obj.src_path):
                    # Add C/C++ build rule
                    built_obj_path = c_build(obj, obj.src_path)
                elif file_is_asm(obj.src_path):
                    # Add assembler build rule
                    built_obj_path = asm_build(obj, obj.src_path, obj.src_obj_path)
                else:
                    sys.exit(f"Unknown source file type {obj.src_path}")
            else:
                if config.warn_missing_source or obj.completed:
                    print(f"Missing source file {obj.src_path}")
                link_built_obj = False

            # Assembly overrides
            if (
                not link_built_obj
                and obj.asm_path is not None
                and obj.asm_path.exists()
            ):
                check_path_case(obj.asm_path)
                link_built_obj = True
                built_obj_path = asm_build(obj, obj.asm_path, obj.asm_obj_path)

            if link_built_obj and built_obj_path is not None:
                # Use the source-built object
                link_step.add(built_obj_path)
            elif obj_path is not None:
                # Use the original (extracted) object
                link_step.add(Path(obj_path))

        # Add DOL link step
        link_step = LinkStep(build_config)
        for unit in build_config["units"]:
            add_unit(unit, link_step)
        link_steps.append(link_step)

        if config.build_rels:
            # Add REL link steps
            for module in build_config["modules"]:
                module_link_step = LinkStep(module)
                for unit in module["units"]:
                    add_unit(unit, module_link_step)
                # Add empty object to empty RELs
                if len(module_link_step.inputs) == 0:
                    if config.rel_empty_file is None:
                        sys.exit("ProjectConfig.rel_empty_file missing")
                    add_unit(
                        {
                            "object": None,
                            "name": config.rel_empty_file,
                            "autogenerated": True,
                        },
                        module_link_step,
                    )
                link_steps.append(module_link_step)

        # ── HARD FAIL: unresolvable splits heading ────────────────────────────
        # A splits.txt heading that resolves to no objects.json Object produces
        # an objdiff unit with `base_path: None`, which can NEVER pair — the
        # unit silently reads 0% forever. Historically this was announced by one
        # `print` among thousands of build lines, which is exactly how the
        # orphan `Rnd.cpp` heading hid for months. Since 715 of the 719 bare
        # headings resolve only via the unique-basename alias (lane BV-2), a
        # single new same-basename source file can silently kill a pin. Fail
        # loudly instead.
        if unresolved_units:
            collisions = getattr(config, "_basename_owners", {}) or {}
            lines = [
                "",
                "=" * 72,
                "ERROR: %d splits.txt heading(s) resolve to no objects.json entry."
                % len(unresolved_units),
                "=" * 72,
            ]
            for name in unresolved_units:
                owners = collisions.get(Path(name).name)
                if owners:
                    lines.append(
                        "  %s -> AMBIGUOUS basename, %d objects.json entries claim it:"
                        % (name, len(owners))
                    )
                    lines.extend("       %s" % o for o in sorted(owners))
                else:
                    lines.append("  %s -> no objects.json entry" % name)
            lines += [
                "",
                "Each of these emits `base_path: None` in objdiff.json and can never",
                "pair, so the unit reads 0% no matter how good the source is.",
                "Fix by either:",
                "  * adding the file to config/45410914/objects.json, or",
                "  * removing/renaming the stale heading in config/45410914/splits.txt, or",
                "  * for an AMBIGUOUS basename: path-qualify the splits.txt heading",
                "    so it matches the objects.json key exactly.",
                "",
                "Escape hatch (use only to unblock, then fix):",
                "  RB3_ALLOW_UNRESOLVED_SPLITS=1 python3 configure.py",
                "=" * 72,
            ]
            msg = "\n".join(lines)
            if os.environ.get("RB3_ALLOW_UNRESOLVED_SPLITS") == "1":
                print(msg)
                print("RB3_ALLOW_UNRESOLVED_SPLITS=1 set — continuing anyway.")
            else:
                sys.exit(msg)

        # Emit compile-only build edges for objects declared in objects.json
        # that don't have a corresponding entry in the dtk split units. Used for
        # smoke-testing C/C++ source files before they have real address ranges.
        for obj in objects.values():
            if obj.src_path is None or not obj.src_path.exists():
                continue
            if obj.src_obj_path is not None and obj.src_obj_path in source_added:
                continue
            if file_is_c_cpp(obj.src_path):
                c_build(obj, obj.src_path)
            elif file_is_asm(obj.src_path):
                asm_build(obj, obj.src_path, obj.src_obj_path)
        n.newline()

        # Check if all compiler versions exist
        for mw_version in used_compiler_versions:
            msvc_path = compilers / mw_version / "cl.exe"
            if config.compilers_path and not os.path.exists(msvc_path):
                sys.exit(f"Compiler {msvc_path} does not exist")

        # Check if linker exists
        msvc_path = compilers / str(config.linker_version) / "link.exe"
        if config.compilers_path and not os.path.exists(msvc_path):
            sys.exit(f"Linker {msvc_path} does not exist")

        # Add all build steps needed before we link and after compiling objects
        write_custom_step("post-compile", "pre-compile")

        ###
        # Link
        ###
        # TODO: add this functionality back when you have a few objs together you can work with (X360)
        # for step in link_steps:
        #     step.write(n)
        #     link_outputs.append(step.output())
        # n.newline()

        # Add all build steps needed after linking and before GC/Wii native format generation
        write_custom_step("post-link", "post-compile")

        ###
        # Generate DOL
        ###
        # n.build(
        #     outputs=link_steps[0].output(),
        #     rule="elf2dol",
        #     inputs=link_steps[0].partial_output(),
        #     implicit=dtk,
        #     order_only="post-link",
        # )

        # ###
        # # Generate RELs
        # ###
        # n.comment("Generate REL(s)")
        # flags = "-w"
        # if len(build_config["links"]) > 1:
        #     flags += " -q"
        # n.rule(
        #     name="makerel",
        #     command=f"{dtk} rel make {flags} -c $config $names @$rspfile",
        #     description="REL",
        #     rspfile="$rspfile",
        #     rspfile_content="$in_newline",
        # )
        # generated_rels: List[str] = []
        # for idx, link in enumerate(build_config["links"]):
        #     # Map module names to link steps
        #     link_steps_local = list(
        #         filter(
        #             lambda step: step.name in link["modules"],
        #             link_steps,
        #         )
        #     )
        #     link_steps_local.sort(key=lambda step: step.module_id)
        #     # RELs can be the output of multiple link steps,
        #     # so we need to filter out duplicates
        #     rels_to_generate = list(
        #         filter(
        #             lambda step: step.module_id != 0
        #             and step.name not in generated_rels,
        #             link_steps_local,
        #         )
        #     )
        #     if len(rels_to_generate) == 0:
        #         continue
        #     generated_rels.extend(map(lambda step: step.name, rels_to_generate))
        #     rel_outputs = list(
        #         map(
        #             lambda step: step.output(),
        #             rels_to_generate,
        #         )
        #     )
        #     rel_names = list(
        #         map(
        #             lambda step: step.name,
        #             link_steps_local,
        #         )
        #     )
        #     rel_names_arg = " ".join(map(lambda name: f"-n {name}", rel_names))
        #     n.build(
        #         outputs=rel_outputs,
        #         rule="makerel",
        #         inputs=list(map(lambda step: step.partial_output(), link_steps_local)),
        #         implicit=[dtk, config.config_path],
        #         variables={
        #             "config": config.config_path,
        #             "rspfile": config.out_path() / f"rel{idx}.rsp",
        #             "names": rel_names_arg,
        #         },
        #         order_only="post-link",
        #     )
        #     n.newline()

        # Add all build steps needed post-build (re-building archives and such)
        write_custom_step("post-build", "post-link")

        ###
        # Helper rule for building all source files
        ###
        n.comment("Build all source files")
        n.build(
            outputs="all_source",
            rule="phony",
            inputs=source_inputs,
        )
        n.newline()

        ###
        # Check hash
        ###
        # n.comment("Check hash")
        # ok_path = build_path / "ok"
        # quiet = "-q " if len(link_steps) > 3 else ""
        # n.rule(
        #     name="check",
        #     command=f"{dtk} shasum {quiet} -c $in -o $out",
        #     description="CHECK $in",
        # )
        # n.build(
        #     outputs=ok_path,
        #     rule="check",
        #     inputs=config.check_sha_path,
        #     implicit=[dtk, *link_outputs],
        #     order_only="post-build",
        # )
        # n.newline()

        ###
        # Calculate progress
        ###
        n.comment("Calculate progress")
        n.rule(
            name="progress",
            command=f"$python {configure_script} $configure_args progress",
            description="PROGRESS",
        )
        n.build(
            outputs="progress",
            rule="progress",
            implicit=[
                configure_script,
                python_lib,
                report_path,
                str(icf_map_checked),
                str(mapinj_checked),
                str(renamed_checked),
            ],
            order_only="post-build",
        )

        ###
        # *** THE RENDERED ICF-ALIAS MAP IS AN INPUT OF THE REPORT. ***
        # objdiff.json names `map_file` -> build/<version>/icf_aliases.map, and
        # that map -- not scripts/symbol_aliases.json -- is the file objdiff reads
        # when it decides whether two relocation names denote one address. It is a
        # RENDERED artifact, so it needs an edge; without one, editing the alias
        # JSON and running `ninja` re-renders nothing and the change measures as
        # zero. dc3 was still in that state until 2026-08-12, where a
        # +198-complete-function fold tier landed, `ninja` reported success, and
        # the tree measured +0 of it. A landed change that measures as nothing is
        # indistinguishable from a lane that overstated its result. The two repos
        # are deliberately identical in shape here; dc3's copy of this comment is
        # in its own tools/project.py.
        #
        # THREE edges, because one of them cannot see everything:
        #
        #   icf_alias_map          renders the map when the JSON or the generator
        #                          is newer. This is the edge that makes "edit the
        #                          JSON, run ninja" sufficient.
        #   icf_alias_map_purge    invalidates the report caches when the map's
        #                          CONTENT moved -- see its own comment below,
        #                          which is the same defect one layer down.
        #   icf_alias_map_checked  re-derives the map content and FAILS THE BUILD
        #                          if the file on disk disagrees. It runs on every
        #                          build (`always`) because the three ways this map
        #                          goes wrong here are all mtime-INVISIBLE to the
        #                          render edge: a hand-edited map (the file's own
        #                          header says DO NOT EDIT BY HAND, which is said
        #                          because people do), a map rendered from a
        #                          different --aliases by a lane measuring a
        #                          variant, and a JSON restored with an OLDER mtime
        #                          than the map (`cp -a`, `tar -x`, `rsync -a`).
        #                          The last one was reproduced HERE on 2026-08-12:
        #                          a byte-exact `cp -a` restore of
        #                          symbol_aliases.json left a content-stale map and
        #                          `ninja` said "no work to do". In each case the
        #                          map is NEWER than its input, so no mtime rule
        #                          can fire.
        # The check is a read-only assertion costing one interpreter start (~0.03s)
        # placed beside PROGRESS, which is already an always-dirty step -- it does
        # not break convergence, because "converged" here means no render, no purge
        # and no report, and none of those run twice.
        #
        # The check deliberately does NOT self-heal. Silently re-rendering over a
        # hand-edited map would erase somebody's deliberate experiment and hide
        # that it ever existed; the failure names the one command that fixes it.
        #
        # What this still does NOT protect: `objdiff-cli report generate -p <repo>`
        # invoked directly, which is how most measurement here actually happens and
        # which never touches ninja. Nothing in the build can reach that path. The
        # cheap assertion for it is the same one this edge runs --
        # `python3 tools/gen_symbol_alias_map.py --check` (exit 1 = stale) -- and a
        # measuring script should call it before it believes a number.
        ###
        n.comment("Render the synthetic ICF-alias map objdiff.json's map_file names")
        n.rule(
            name="icf_alias_map",
            command=f"$python {icf_gen_script} --out $out",
            description="GEN ICF-ALIAS MAP",
            # The generator writes only when the rendered content changes, so an
            # untouched map keeps its mtime; restat lets ninja mark the edge clean
            # instead of re-running it forever and dragging the report behind it.
            restat=True,
        )
        n.build(
            outputs=str(icf_map_path),
            rule="icf_alias_map",
            implicit=[str(icf_gen_script), str(icf_aliases_json)],
        )
        n.comment("Assert the rendered map still agrees with the alias JSON")
        n.rule(
            name="icf_alias_map_check",
            # `stamp_if_changed` rather than `touch`: see the CHURN note on the
            # NAME-injectivity edge below, which is the same defect and where the
            # cost was actually paid. This stamp feeds only `progress` (itself
            # always-dirty), so it was never the expensive one -- it is fixed
            # here so the two gates cannot drift apart, and so that "ninja did no
            # work" stays a usable signal on this edge too.
            command=(f"$python {icf_gen_script} --check --out {icf_map_path}"
                     f" --stamp $out --stamp-input {icf_gen_script}"
                     f" --stamp-input {icf_aliases_json}"
                     f" --stamp-input {icf_map_path}"),
            description="CHECK ICF-ALIAS MAP",
            restat=True,
        )
        n.build(
            outputs=str(icf_map_checked),
            rule="icf_alias_map_check",
            implicit=[
                str(icf_gen_script), str(icf_aliases_json), str(icf_map_path),
                "always",
            ],
        )

        ###
        # *** THE TARGET SYMBOL MAP MUST BE INJECTIVE ON NAME. ***
        # scripts/target_symbol_map.json is address-keyed, so its shape offers
        # no defence at all against stamping one mangled name onto several VAs
        # -- which a linked image can never do, because it resolves every
        # COMDAT/extern symbol to exactly ONE definition. The duplicate still
        # SCORES at every VA, because objdiff pairs by name inside a unit and
        # is blind to the relocation targets that separate byte-twin thunks,
        # deleting dtors and template bodies. So a duplicate name is a live
        # path to minting a BYTE-EXACT witness against the wrong target body,
        # and byte-exact is the ADMISSION gate for a crack and for a training
        # label -- not a metric we can revise later.
        #
        # `always`, like the CHECK edge above and for the same reason: the map
        # is edited by hand and by fragment appliers, and neither the renamer
        # stamp nor any output mtime can express "the file gained a duplicate".
        # Cost is one interpreter start on a read-only pass over one JSON.
        #
        # WHY THIS EDGE EXISTS WHEN TWO PER-UNIT CHECKS ALREADY DO:
        # scripts/harvest/icf_class_bijection.py and
        # scripts/harvest/tu5_map_apply_fragment.py enforce injectivity WITHIN
        # one unit / one fragment. Cross-unit duplicates pass both. That gap is
        # how the debt returned twice (738 surplus VAs before e7b8ba85, 533
        # again two days later from new fragments): a fragment applier being
        # locally correct is not evidence that the map is globally correct.
        #
        # The check is SET-based and prints the offending set, never a count --
        # 2eb6307a records a map plan that left the collision COUNT unchanged
        # (8 -> 8) while retiring one duplicate and INTRODUCING another, which
        # a count comparison passes clean. It reads the MAP rather than the
        # objs, because a two-VA collision inside ONE unit
        # (?DataDir@UIPanel@@$4...) never reaches a differ at all. And it scores
        # the APPLIED map via the renamer's own load_address_map, so the gate
        # and the renamer cannot disagree about which rows count -- `_denylist`
        # was declared in that JSON and IGNORED by the loader until f3fe9ab1,
        # and a gate carrying a private copy of that filter would be the second
        # safeguard here that silently did nothing.
        ###
        #
        # ⛔ CHURN, and the reason this edge does NOT end in `touch` (2026-08-13).
        # It used to, and because this stamp is a declared implicit input of the
        # REPORT edge (the ★ lane J3 paragraph below), a `touch` on an
        # always-dirty edge made REPORT unconditionally dirty. rb3-xenon
        # therefore regenerated its 14 MB `report.json` on EVERY ninja
        # invocation, forever, with nothing changed. dc3 never showed this only
        # because its report edge consumes no gate stamp.
        #
        # The wall clock was never the problem -- measured 0.97 s for a whole
        # no-op build, and the regenerated report was byte-identical (sha
        # 5b331399… before and after, three runs). What the churn destroyed is a
        # SIGNAL: on a completely no-op build `report.json`'s mtime advanced 14 s
        # while every object it describes stayed put. So `report.json` is
        # unconditionally newer than its inputs, and the obvious freshness
        # assertion -- "refuse to score against a report older than the objects"
        # -- is a gate that CANNOT FAIL here. That is the silent-success family,
        # and it is worth more than the second of build time.
        #
        # The gate is unchanged in every way that matters: still `always`, still
        # runs on every build, still fails the build. Only the stamp became
        # content-addressed, so `restat` lets ninja clean REPORT when the map and
        # the gate script did not move. A failing gate never reaches the stamp
        # (`&&`), so a red build cannot leave a stamp claiming it passed.
        ###
        n.comment("Assert the target symbol map is globally injective on NAME")
        n.rule(
            name="map_name_injectivity_check",
            command=(f"$python {mapinj_script} --quiet"
                     f" --stamp $out --stamp-input {mapinj_script}"
                     f" --stamp-input {mapinj_json}"),
            description="CHECK MAP NAME-INJECTIVITY",
            restat=True,
        )
        n.build(
            outputs=str(mapinj_checked),
            rule="map_name_injectivity_check",
            implicit=[str(mapinj_script), str(mapinj_json), "always"],
        )

        n.comment("Assert the split target objs carry their mangled names")
        n.rule(
            name="target_objs_renamed_check",
            command=(f"$python {renamed_script} --title {config.version}"
                     f" --stamp $out"),
            description="CHECK TARGET OBJS RENAMED",
            restat=True,
        )
        # `pre-compile` is the renamer's phony, so this can only run after the
        # renamer had its chance; `always` makes it re-check every build, which
        # is the point -- the defect appears when a SPLIT re-emits virgin objs
        # under a stamp that still looks fresh, and nothing else notices.
        n.build(
            outputs=str(renamed_checked),
            rule="target_objs_renamed_check",
            implicit=[str(renamed_script), "always", "pre-compile"],
        )

        n.comment("Assert the target objects came from the current split config")
        # `always`, because BOTH failure modes are mtime-invisible: a split in
        # flight, and a symbols.txt restored with an older mtime than
        # config.json (which ninja does not even plan a SPLIT for).
        # write-if-changed + restat keeps the OUTPUT from moving unless the
        # split record does, so this does not re-fire REPORT on a quiet tree.
        n.rule(
            name="split_current_check",
            command=f"$python {split_guard_script} --check --quiet --stamp-out $out",
            description="CHECK SPLIT CURRENT",
            restat=True,
        )
        # `split_inputs.stamp` is an implicit OUTPUT of the split edge, so
        # naming it here does two things that `always` alone cannot:
        #
        #   ordering -- without it ninja is free to run this check BEFORE the
        #   split in the same invocation, where it would vouch for the OLD
        #   recorded state and pass;
        #   dirtiness -- the stamp carries the split's pid and unix_time, so
        #   its digest moves on every split RUN, not merely on every split
        #   whose config inputs changed. That is the signal downstream edges
        #   need, because what a split rewrites is build/<v>/obj/** -- an
        #   undeclared output nothing else in the graph stats.
        n.build(
            outputs=str(split_checked),
            rule="split_current_check",
            implicit=[str(split_guard_script), str(split_stamp), "always"],
        )

        n.comment("Assert both objdiff-cli entry points resolve the same ruler")
        # `--pins-only`, deliberately. The stronger form of this check compares
        # objdiff.json against report.json's own provenance.diff_config, and
        # that assertion is legitimately FALSE for exactly one build: the first
        # one after a deliberate ruler change, whose whole job is to replace
        # the report it would be checked against. Gating REPORT on it would
        # deadlock. The pins themselves have no legitimate transient, so that
        # is what the build edge asserts; the report cross-check stays a manual
        # / CI step (`--check`, and `--selftest` for the negative control).
        #
        # Input is objdiff.json, not `always`: the check is a pure function of
        # that file, which configure.py rewrites whenever the options block
        # moves. write-if-changed + restat keeps the stamp from dirtying REPORT
        # on a quiet tree.
        n.rule(
            name="ruler_agreement_check",
            command=(f"$python {ruler_guard_script} --check --pins-only --quiet"
                     f" --stamp-out $out"),
            description="CHECK RULER AGREEMENT",
            restat=True,
        )
        n.build(
            outputs=str(ruler_checked),
            rule="ruler_agreement_check",
            implicit=[str(ruler_guard_script), "objdiff.json"],
        )

        ###
        # BELT AND BRACES: purge the report-cache sidecars when the alias map
        # moves. As of 2026-08-13 this edge is REDUNDANT, and it stays anyway.
        #
        # The history it was built for: `report generate -o X.json` writes a
        # sidecar `X.cache` and seeds the next run from it, and its key used to be
        # `ReportCache::hash_unit` (objdiff-cli/src/cmd/report.rs) over the target
        # obj bytes, the base obj bytes, the `-c` args, and the project/unit
        # `options` blocks -- with `map_file`, and the CONTENT of the map it names,
        # in none of them. So making the map an input of the report edge was
        # necessary and NOT sufficient: measured on dc3 2026-08-12, with that input
        # wired, editing the alias JSON re-rendered the map, re-ran REPORT, and
        # report.json still served the pre-change answer out of cache -- the
        # original defect one layer down. This repo paid for it too: see the
        # CORRECTED note on the `name_check` figures below, whose stale read came
        # out of exactly this sidecar.
        #
        # THE UPSTREAM FIX LANDED. The objdiff fork now folds the map file's
        # content hash -- and the resolved diff config, and the objdiff-cli
        # binary's own xxh3 -- into the cache key, and every generated report
        # carries a `provenance` block naming all three plus `cache_hits`. A stale
        # entry can no longer be served under a changed map. So the purge below can
        # no longer be the thing that saves a measurement.
        #
        # Kept regardless, for two reasons: it costs one `rm -f` on a rebuild that
        # only fires when the RENDERED map bytes actually changed (the generator
        # writes only on change, with `restat` above), so a touched-but-identical
        # JSON still costs nobody a re-diff; and it keeps the build correct against
        # an older objdiff-cli, which this repo does not pin. That second reason is
        # weaker than it was: as of 2026-08-13 every objdiff-cli path in this repo
        # (`bin/`, `build/tools/`, `build/tools/release/`) is a symlink onto the one
        # shared build, so reaching an older binary now takes a deliberate act
        # rather than a path choice. Measurement code stays; a redundant guard is
        # cheap.
        ###
        n.comment("Purge report caches on an alias-map change "
                  "(redundant since the upstream map-keyed cache landed)")
        icf_purge_targets = " ".join(
            str(p.with_suffix(".cache"))
            # `baseline.json` by literal name: it is defined further down, and it
            # gets the same treatment as the report's.
            for p in (report_path, build_path / "baseline.json")
        )
        n.rule(
            name="icf_alias_map_purge",
            command=f"rm -f {icf_purge_targets} && touch $out",
            description="PURGE REPORT CACHE (alias map changed)",
        )
        n.build(
            outputs=str(icf_map_purged),
            rule="icf_alias_map_purge",
            implicit=[str(icf_map_path)],
        )
        n.newline()

        # Generate progress report
        ###
        n.comment("Generate progress report")
        n.rule(
            name="report",
            command=f"{objdiff} report generate $objdiff_report_args -o $out",
            description="REPORT",
        )
        # ★ lane CN-1: the report is generated from the PATCHED objs, so it
        # must DEPEND ON the post-compile patch stamps, not merely be ordered
        # after them. `order_only="post-build"` never marks this edge dirty,
        # and now that every obj patcher restores the obj mtime (required so
        # the patcher edges converge -- see configure.py's post-compile block)
        # `all_source` no longer changes either. Without this, a build in which
        # only the patchers ran (e.g. after editing a patcher script) leaves
        # report.json STALE and the change measures as inert.
        #
        # ★ lane J3: the NAME-injectivity stamp is an input HERE, not only on
        # `progress`. laneJ2 wired the gate to `progress` (the default target)
        # and its docs claimed it therefore "runs on every build". It did not:
        # `ninja build/45410914/report.json` names a target that does not reach
        # `progress`, so the gate never ran and ninja exited 0 with a live
        # collision in the map -- and that is exactly the target
        # `scripts/sync_match_percent.py --build` invokes, i.e. the one path
        # that takes objdiff's numbers into decomp.db. A map collision is a
        # false NAME pairing, so the report generated over it is the artifact
        # the collision corrupts; gating `progress` but not the report gated
        # the summary and not the measurement. Cost is one interpreter start on
        # a read-only pass over one JSON.
        report_implicit: List[str | Path] = [
            objdiff, "objdiff.json", "all_source",
            str(icf_map_path), str(icf_map_purged),
            str(mapinj_checked), str(renamed_checked),
            # ... and on the split-currency check, because the TARGET side of
            # every diff in this report is written by an edge that declares
            # none of it. Without this the report is free to measure objects
            # the config on disk did not produce.
            str(split_checked),
            # ... and on the ruler-agreement check, because objdiff.json's
            # `options` block is the ONLY place that makes `report generate`
            # and `diff` score the same way, and a regenerated objdiff.json
            # that lost the pins is silent: the report stays correct while
            # every per-function reading drifts LOW beneath it.
            str(ruler_checked),
        ]
        if config.custom_build_steps and "post-compile" in config.custom_build_steps:
            report_implicit.append("post-compile")
        n.build(
            outputs=report_path,
            rule="report",
            implicit=report_implicit,
            order_only="post-build",
        )

        n.comment("Phony edge that will always be considered dirty by ninja.")
        n.comment(
            "This can be used as an implicit to a target that should always be rerun, ignoring file modified times."
        )
        n.build(
            outputs="always",
            rule="phony",
        )
        n.newline()

        ###
        # Regression test progress reports
        ###
        report_baseline_path = build_path / "baseline.json"
        report_changes_path = build_path / "report_changes.json"
        changes_fmt = config.tools_dir / "changes_fmt.py"
        regressions_md = build_path / "regressions.md"
        n.comment(
            "Create a baseline progress report for later match regression testing"
        )
        n.build(
            outputs=report_baseline_path,
            rule="report",
            implicit=[objdiff, "all_source", "always"],
            order_only="post-build",
        )
        n.build(
            outputs="baseline",
            rule="phony",
            inputs=report_baseline_path,
        )
        n.comment("Check for any match regressions against the baseline")
        n.comment("Will fail if no baseline has been created")
        n.rule(
            name="report_changes",
            command=f"{objdiff} report changes --format json-pretty {report_baseline_path} $in -o $out",
            description="CHANGES",
        )
        n.build(
            outputs=report_changes_path,
            rule="report_changes",
            inputs=report_path,
            implicit=[objdiff, "always"],
        )
        n.rule(
            name="changes_fmt",
            command=f"$python {changes_fmt} $args $in",
            description="CHANGESFMT",
        )
        n.build(
            outputs="changes",
            rule="changes_fmt",
            inputs=report_changes_path,
            implicit=changes_fmt,
        )
        n.build(
            outputs="changes_all",
            rule="changes_fmt",
            inputs=report_changes_path,
            implicit=changes_fmt,
            variables={"args": "--all"},
        )
        n.rule(
            name="changes_md",
            command=f"$python {changes_fmt} $in -o $out",
            description="CHANGESFMT $out",
        )
        n.build(
            outputs=regressions_md,
            rule="changes_md",
            inputs=report_changes_path,
            implicit=changes_fmt,
        )
        n.newline()

        ###
        # Helper tools
        ###
        # TODO: make these rules work for RELs too
        # dol_link_step = link_steps[0]
        # dol_elf_path = dol_link_step.partial_output()
        # n.comment("Check for mismatching symbols")
        # n.rule(
        #     name="dol_diff",
        #     command=f"{dtk} -L error dol diff $in",
        #     description=f"DIFF {dol_elf_path}",
        # )
        # n.build(
        #     inputs=[config.config_path, dol_elf_path],
        #     outputs="dol_diff",
        #     rule="dol_diff",
        # )
        # n.build(
        #     outputs="diff",
        #     rule="phony",
        #     inputs="dol_diff",
        # )
        # n.newline()

        # n.comment("Apply symbols from linked ELF")
        # n.rule(
        #     name="dol_apply",
        #     command=f"{dtk} dol apply $in",
        #     description=f"APPLY {dol_elf_path}",
        # )
        # n.build(
        #     inputs=[config.config_path, dol_elf_path],
        #     outputs="dol_apply",
        #     rule="dol_apply",
        #     implicit=[ok_path],
        # )
        # n.build(
        #     outputs="apply",
        #     rule="phony",
        #     inputs="dol_apply",
        # )
        # n.newline()

    ###
    # Split XEX
    ###
    build_config_path = build_path / "config.json"
    n.comment("Split XEX into relocatable objects")
    n.rule(
        name="split",
        # JEFF_MERGE_PROTECT: the jeff Class-2 leaf-fragment merge pass
        # (fall-through PDATA-less fragments) must never ABSORB a
        # map-identified real function. Relative path resolves from ninja's
        # cwd (repo/worktree root) in main AND every worktree — each protects
        # with its own target_symbol_map.json — and keeps the command string
        # byte-identical everywhere (warm-worktree command-hash parity). The
        # pass fails safe (empty protect set -> over-fires by the 2 explained
        # losses) if the file is unreadable, so the path must be correct.
        # The `&& $python tools/prune_split_outputs.py` tail deletes split
        # outputs the CURRENT split no longer emits. dtk rewrites the whole live
        # set every run but never removes a unit's previous generation, so every
        # re-pathed / renamed / deleted splits.txt heading orphans a `.s`+`.obj`
        # on disk forever -- and an orphan `auto_<addr>` keeps claiming bytes
        # that a real unit has since been pinned to. Relative script path (like
        # JEFF_MERGE_PROTECT above) keeps the command string byte-identical in
        # main and every worktree, preserving warm-worktree command-hash reuse.
        # The --begin/--complete bracket is the split-currency guard. dtk's
        # 3,091 target objects are NOT declared ninja outputs, so nothing else
        # can tell a reader that they are mid-rewrite or that they came from a
        # different symbols.txt. --begin marks the record `running` BEFORE dtk
        # touches anything and --complete marks it `complete` only on success,
        # so a crashed split leaves the tree explicitly unvouchable rather than
        # quietly half-rewritten. Relative script path, like JEFF_MERGE_PROTECT
        # above, to keep the command string byte-identical across worktrees.
        command=(
            f"$python {split_guard_script} --begin --quiet && "
            f"JEFF_MERGE_PROTECT=scripts/target_symbol_map.json {dtk} xex split $in $out_dir"
            f" && $python tools/prune_split_outputs.py $out_dir"
            f" && $python {split_guard_script} --complete --quiet"
        ),
        description="SPLIT $in",
        depfile="$out_dir/dep",
        # restat: dtk split is deterministic, so re-running it with an
        # unchanged config.yml produces an identical config.json. restat lets
        # ninja keep the old mtime and avoid re-triggering the `configure`
        # generator rule -- which otherwise causes an infinite
        # SPLIT->configure manifest-regeneration loop. (Mirrors rb3-Wii.)
        #
        # NOTE: no `deps="gcc"` here. The binary deps cache (.ninja_deps) is
        # unsafe under concurrent ninja invocations; if two builds race the
        # cache can become inconsistent and cause spurious re-SPLITs. Ninja
        # will read $out_dir/dep directly each build -- slightly slower, but
        # killed the rebuild-everything failure mode in rb3-Wii.
        restat=True,
    )
    # target_symbol_map.json is a REAL INPUT to the split (JEFF_MERGE_PROTECT
    # above reads it to decide which symbols the Class-2 merge may not absorb),
    # but jeff does not list it in the depfile it emits -- that names only
    # default.xex, splits.txt and symbols.txt. So it must be declared here or
    # ninja never learns the split depends on it.
    #
    # WHY THIS MATTERS, and why the failure is SILENT: the renamer rewrites the
    # split target obj's anonymous fn_<addr> symbols to mangled names IN PLACE.
    # Once an obj is renamed there are no fn_<addr> symbols left, so re-running
    # the renamer against a NEW map is a guaranteed no-op -- it reports
    # "[APPLIED] 3091 files checked, 0 files patched, 0 total symbol renames"
    # and exits 0. The only way a map edit reaches the objs is for the SPLIT to
    # re-emit virgin ones first. Without this edge a landed map change stays
    # inert on every warm tree until somebody remembers to touch config.yml,
    # and the metric silently UNDER-REPORTS in the meantime.
    #
    # Measured on main 2026-08-21: commit 1cc6896d corrected RndFont's map rows
    # from the virtual spelling (?CharDefined@RndFont@@UBA_NG@Z) to the
    # non-virtual one (...@QBA_NG@Z, which retail bytes at 0x82473A98 support).
    # main's warm tree still carried the stale U objs, so four RndFont rows sat
    # at 0% that score 100% against a fresh split. A forced re-split recovered
    # exactly +5 matched_functions / +748 B (42193 -> 42198), with symbols.txt
    # byte-identical and the trees provably identical. Nothing detected it; it
    # only surfaced because a worktree and main disagreed on the same content.
    #
    # Safe as a dependency: the split READS this file and never writes it, so
    # unlike splits.txt/symbols.txt (which are simultaneously split OUTPUTS,
    # cf. ABSPLIT-1's fixed-point iteration) it cannot make the edge
    # self-dirtying. Implicit deps are not part of the command string, so
    # warm-worktree command-hash parity is unaffected.
    n.build(
        inputs=config.config_path,
        outputs=build_config_path,
        # DECLARE THE RECORD. The 3,091 target objects still cannot be listed
        # here (they are not known until the split has run), but the stamp that
        # VOUCHES for them can be -- so deleting it re-fires the split that can
        # legitimately recreate it, instead of wedging the build until someone
        # touches config.yml by hand.
        implicit_outputs=[str(split_stamp)],
        rule="split",
        implicit=[dtk, "scripts/target_symbol_map.json"],
        variables={"out_dir": build_path},
    )
    n.newline()

    ###
    # Regenerate on change
    ###
    n.comment("Reconfigure on change")
    n.rule(
        name="configure",
        command=f"$python {configure_script} $configure_args",
        generator=True,
        description=f"RUN {configure_script}",
    )
    n.build(
        outputs=["build.ninja", "objdiff.json"],
        rule="configure",
        implicit=[
            build_config_path,
            configure_script,
            python_lib,
            python_lib_dir / "ninja_syntax.py",
            *(config.reconfig_deps or []),
        ],
    )
    n.newline()

    ###
    # Default rule
    ###
    n.comment("Default rule")
    if build_config:
        if config.non_matching:
            n.default(link_outputs)
        elif config.progress:
            n.default("progress")
        else:
            n.default(ok_path)
    else:
        n.default(build_config_path)

    # Write build.ninja
    with open("build.ninja", "w", encoding="utf-8") as f:
        f.write(out.getvalue())
    out.close()


# Generate objdiff.json
def generate_objdiff_config(
    config: ProjectConfig,
    objects: Dict[str, Object],
    build_config: Optional[BuildConfig],
) -> None:
    if build_config is None:
        return

    # Load existing objdiff.json
    existing_units = {}
    if Path("objdiff.json").is_file():
        with open("objdiff.json", "r", encoding="utf-8") as r:
            existing_config = json.load(r)
            existing_units = {unit["name"]: unit for unit in existing_config["units"]}

    if config.ninja_path:
        ninja = str(config.ninja_path.absolute())
    else:
        # Default custom_make: the ninja-locked flock wrapper. Bare `ninja`
        # under multiple agents corrupts .ninja_log/.ninja_deps and can hang
        # in an infinite SPLIT->configure manifest-regeneration loop.
        # Ported from rb3-Wii (see tools/ninja-locked).
        ninja_locked = Path("tools") / "ninja-locked"
        if (Path(__file__).resolve().parent.parent / "tools" / "ninja-locked").is_file():
            ninja = str(ninja_locked)
        else:
            ninja = "ninja"

    objdiff_config: Dict[str, Any] = {
        "min_version": "2.0.0-beta.5",
        "custom_make": ninja,
        "build_target": False,
        "watch_patterns": [
            "*.c",
            "*.cp",
            "*.cpp",
            "*.h",
            "*.hpp",
            "*.inc",
            "*.py",
            "*.yml",
            "*.txt",
            "*.json",
        ],
        "units": [],
        "progress_categories": [],
        # Relocation ruler. objdiff's default, `none`, compares a relocation's
        # POSITION and TYPE but never the NAME of the symbol it points at, so a
        # `bl` to the wrong function and a load of the wrong global both score a
        # COMPLETE match. `name_check` checks the name, with the tolerances a
        # split target needs (unverifiable left relocation, placeholder left
        # name, COFF weak-external alias, template array sizes, data-section
        # placement), and it honours the ICF equivalence classes below.
        #
        # This costs more here than on dc3 or rb3 -- 42.2200% -> 32.4623%
        # matched_code, exposing 7,462 of 44,055 complete functions -- and the
        # reason is the alias map, not the source. Some of those functions are
        # blocked only by trivial destructors and allocator overloads, the two
        # canonical /OPT:ICF fold shapes: retail kept one spelling and we
        # reference another. Those are not defects, but neither are they proven
        # folds, and an unproven fold is exactly what symbol_aliases.json exists
        # to adjudicate. 1,440 groups are admitted today, so the honest reading
        # of the drop is "the alias evidence lane still has thousands of
        # functions of headroom", and it is recovered by
        # tools/icf_alias_build.py, not by editing source.
        #
        # CORRECTED 2026-08-12 (lane P). This read "-> 31.1425%, exposing 9,087"
        # against 511 admitted groups. That came from a report.json served out of
        # the `.cache` sidecar, which had units in it diffed under an older
        # configuration -- at the time the key covered neither `map_file` nor the
        # binary (as of 2026-08-13 it covers both, plus the resolved config; see
        # the icf_alias_map_purge edge above), and the alias set is 1,440 groups
        # today. Re-measured on this tree with the caches purged, on pinned
        # objdiff main 745b7e3, and independently reproduced by the build's own
        # objdiff after the purge edge fired: none 42.220000% (4,357,396 B,
        # 44,055 complete), name_check 32.462280% (3,350,332 B, 36,593 complete).
        # The stale figure was 136,212 matched bytes low.
        #
        # ── ALL FOUR KEYS, not just the ruler (2026-08-31) ──────────────────
        # `objdiff-cli report generate` and `objdiff-cli diff` carry DIFFERENT
        # hardcoded base configs, and neither is the schema default:
        #
        #   report generate (report.rs:581)  functionRelocDiffs=none,
        #                                    combineDataSections=true,
        #                                    combineTextSections=true,
        #                                    ppc.calculatePoolRelocations=false
        #   diff (diff.rs:1070, --batch at diff.rs:1807)
        #                                    functionRelocDiffs=data_value,
        #                                    and the other three at their SCHEMA
        #                                    defaults: false / false / TRUE
        #
        # Both then layer THIS block on top, so pinning only `functionRelocDiffs`
        # fixed the ruler the two paths argue about most visibly and left them
        # disagreeing on the other three.
        #
        # `ppc.calculatePoolRelocations` is the one that bites. It SYNTHESIZES
        # `R_PPC_NONE` "fake" relocations for pooled data loads
        # (objdiff-core/src/arch/ppc/mod.rs:819 make_fake_pool_reloc; the schema
        # calls them "fake relocations" in as many words), reconstructed per
        # object from THAT OBJECT'S OWN symbol table. A dtk-carved target obj --
        # a whole linked data section with anonymous `lbl_*` labels -- and our
        # MSVC per-TU COMDAT obj do not reconstruct the same set. `reloc_eq`
        # (objdiff-core/src/diff/code.rs:1330-1338) forgives a BASE-only
        # synthesized relocation under `name_check` but its `_ => return false`
        # arm charges a TARGET-only one under every ruler except `none`. The
        # visible symptom is a charged mismatch row on two TEXTUALLY IDENTICAL
        # instructions.
        #
        # Measured whole-binary on this repo (rb3-xenon), 2026-08-31, one
        # worktree, one objdiff-cli 4.2.8 (358c715835cc, xxh3 9b2bb6f1f3a21062),
        # full ninja before reading report.json, `diff --batch` over every
        # uniquely-named function in the report:
        #
        #   comparable rows (a real percent on both sides): 47,208
        #     (22,009 further rows are unpaired -- batch null, report 0.0 --
        #      which is AGREEMENT, and 123 carry the batch path's disclosed
        #      cross-unit `base_unit` COMDAT fallback, a different question)
        #   disagreements: 102 functions / 55,604 bytes
        #   direction: report higher on 100, diff higher on 2
        #   magnitude: up to 65.20 pp
        #   1 of them (308 B) reads exactly 100.0 in report.json and <100
        #     through `diff` -- the class where a lane refuses a promotion for a
        #     reason that does not exist
        #
        # Attribution over those 102: `ppc.calculatePoolRelocations` alone
        # explains 102/102. The other two are NOT inert and must be pinned with
        # it: `combineDataSections`+`combineTextSections` applied WITHOUT the
        # pool key ADD two fresh disagreements (`kdTree<Triangle>::Intersect`,
        # `DataVarName`). All four together -> 0 disagreements.
        #
        # This is upstream objdiff, not a fork bug: the three extra report-side
        # values arrive in 0c9e552 "Combine sections when generating report"
        # (Luke Street, 2025-05-07), which touched report.rs only.
        # `bin/objdiff-cli` is a symlink shared with ../rb3 and ../dc3-decomp,
        # so all three repos were exposed; the fix is config-only in each and
        # needs no tool rebuild. Found in dc3-decomp (155 fns / 120,728 B);
        # ../rb3 measures 151 fns / 224,892 B.
        #
        # Pinning these changed NO recorded number here -- see the guard's
        # docstring and scripts/verify_ruler_agreement.py.
        "options": {
            "functionRelocDiffs": "name_check",
            "combineDataSections": True,
            "combineTextSections": True,
            "ppc.calculatePoolRelocations": False,
        },
    }

    # ICF-merged-symbol alias map. Retail /O1 ICF-folds debug-stripped allocator
    # overloads (2-arg PoolAlloc, 1-arg MemOrPoolAlloc) onto their named debug
    # siblings: only the survivor spelling exists at one XEX address, but our
    # objs emit the byte-identical call site under the folded spelling, so
    # objdiff's by-name reloc_eq flags a [sym] mismatch. The objdiff fork already
    # consumes ICF equivalences from an MSVC `map_file` (parse_msvc_map groups
    # symbols sharing an address). tools/gen_symbol_alias_map.py renders the
    # proven folds (scripts/symbol_aliases.json) into this synthetic map; declare
    # it so `report generate` neutralizes the alias noise. Conditional: a fresh
    # tree without the generated map still builds (no map -> no equivalences).
    # Generate it here (best-effort) so objdiff.json references it even on the
    # FIRST configure of a fresh tree.
    #
    # This render is the BOOTSTRAP ONLY. It is NOT what keeps the map fresh:
    # configure runs only when configure.py, this file, or a config/ input
    # changes, and scripts/symbol_aliases.json is none of those. The
    # `icf_alias_map` / `icf_alias_map_purge` / `icf_alias_map_checked` edges
    # above own freshness; the design comment there is the one place per repo
    # this rule is written down.
    icf_map = config.build_dir / config.version / "icf_aliases.map"
    gen_icf = Path("tools") / "gen_symbol_alias_map.py"
    if gen_icf.is_file():
        icf_map.parent.mkdir(parents=True, exist_ok=True)
        try:
            subprocess.run(
                [sys.executable, str(gen_icf), "--out", str(icf_map)],
                check=True, stdout=subprocess.DEVNULL,
            )
        except Exception as e:
            print(f"(icf alias map generation skipped: {e})")
    if icf_map.is_file():
        objdiff_config["map_file"] = str(icf_map)

    # decomp.me compiler name mapping
    COMPILER_MAP = {
        "GC/1.0": "mwcc_233_144",
        "GC/1.1": "mwcc_233_159",
        "GC/1.1p1": "mwcc_233_159p1",
        "GC/1.2.5": "mwcc_233_163",
        "GC/1.2.5e": "mwcc_233_163e",
        "GC/1.2.5n": "mwcc_233_163n",
        "GC/1.3": "mwcc_242_53",
        "GC/1.3.2": "mwcc_242_81",
        "GC/1.3.2r": "mwcc_242_81r",
        "GC/2.0": "mwcc_247_92",
        "GC/2.0p1": "mwcc_247_92p1",
        "GC/2.5": "mwcc_247_105",
        "GC/2.6": "mwcc_247_107",
        "GC/2.7": "mwcc_247_108",
        "GC/3.0a3": "mwcc_41_51213",
        "GC/3.0a3.2": "mwcc_41_60126",
        "GC/3.0a3.3": "mwcc_41_60209",
        "GC/3.0a3.4": "mwcc_42_60308",
        "GC/3.0a5": "mwcc_42_60422",
        "GC/3.0a5.2": "mwcc_41_60831",
        "GC/3.0": "mwcc_41_60831",
        "Wii/1.0RC1": "mwcc_42_140",
        "Wii/0x4201_127": "mwcc_42_142",
        "Wii/1.0a": "mwcc_42_142",
        "Wii/1.0": "mwcc_43_145",
        "Wii/1.1": "mwcc_43_151",
        "Wii/1.3": "mwcc_43_172",
        "Wii/1.5": "mwcc_43_188",
        "Wii/1.6": "mwcc_43_202",
        "Wii/1.7": "mwcc_43_213",
        "X360/14.00.2110": "msvc_ppc_14.00.2110",
        "X360/16.00.11886.00": "msvc_ppc_16.00.11886.00",
        "X360/16.00.10224.00": "msvc_ppc_16.00.10224.00",
    }

    # decomp.me platform mapping (by version prefix)
    PLATFORM_MAP = {
        "GC": "gc_wii",
        "Wii": "gc_wii",
        "X360": "xbox360",
    }

    def add_unit(
        build_obj: BuildConfigUnit, module_name: str, progress_categories: List[str]
    ) -> None:
        obj_path, obj_name = build_obj["object"], build_obj["name"]
        base_object = Path(obj_name).with_suffix("")
        name = str(Path(module_name) / base_object).replace(os.sep, "/")
        unit_config: Dict[str, Any] = {
            "name": name,
            "target_path": obj_path,
            "base_path": None,
            "scratch": None,
            "metadata": {
                "complete": None,
                "reverse_fn_order": None,
                "source_path": None,
                "progress_categories": progress_categories,
                "auto_generated": build_obj["autogenerated"],
            },
            "symbol_mappings": None,
        }

        # Preserve existing symbol mappings
        existing_unit = existing_units.get(name)
        if existing_unit is not None:
            unit_config["symbol_mappings"] = existing_unit.get("symbol_mappings")

        obj = objects.get(obj_name)
        if obj is None:
            objdiff_config["units"].append(unit_config)
            return

        src_exists = obj.src_path is not None and obj.src_path.exists()
        if src_exists:
            unit_config["base_path"] = obj.src_obj_path
            unit_config["metadata"]["source_path"] = obj.src_path

        # Filter out include directories
        def keep_flag(flag):
            return (
                not flag.startswith("-i ")
                and not flag.startswith("-i-")
                and not flag.startswith("-I ")
                and not flag.startswith("-I+")
                and not flag.startswith("-I-")
                and not flag.startswith("/I")
            )

        all_cflags = list(
            filter(keep_flag, obj.options["cflags"] + obj.options["extra_cflags"])
        )
        reverse_fn_order = False
        for flag in all_cflags:
            if not flag.startswith("-inline "):
                continue
            for value in flag.split(" ")[1].split(","):
                if value == "deferred":
                    reverse_fn_order = True
                elif value == "nodeferred":
                    reverse_fn_order = False

        compiler_version = COMPILER_MAP.get(obj.options["mw_version"])
        if compiler_version is None:
            print(f"Missing scratch compiler mapping for {obj.options['mw_version']}")
        else:
            platform_prefix = obj.options["mw_version"].split("/")[0]
            platform = PLATFORM_MAP.get(platform_prefix, "gc_wii")
            cflags_str = make_flags_str(all_cflags)
            unit_config["scratch"] = {
                "platform": platform,
                "compiler": compiler_version,
                "c_flags": cflags_str,
                "preset_id": obj.options["scratch_preset_id"],
            }
            if src_exists:
                unit_config["scratch"].update(
                    {
                        "ctx_path": obj.ctx_path,
                        "build_ctx": True,
                    }
                )
        category_opt: List[str] | str = obj.options["progress_category"]
        if isinstance(category_opt, list):
            progress_categories.extend(category_opt)
        elif category_opt is not None:
            progress_categories.append(category_opt)
        unit_config["metadata"].update(
            {
                "complete": obj.completed if src_exists else None,
                "reverse_fn_order": reverse_fn_order,
                "progress_categories": progress_categories,
            }
        )
        objdiff_config["units"].append(unit_config)

    # Add DOL units
    for unit in build_config["units"]:
        progress_categories = []
        # Only include a "dol" category if there are any modules
        # Otherwise it's redundant with the global report measures
        if len(build_config["modules"]) > 0:
            progress_categories.append("dol")
        add_unit(unit, build_config["name"], progress_categories)

    # Add REL units
    for module in build_config["modules"]:
        for unit in module["units"]:
            progress_categories = []
            if config.progress_modules:
                progress_categories.append("modules")
            if config.progress_each_module:
                progress_categories.append(module["name"])
            add_unit(unit, module["name"], progress_categories)

    # Add progress categories
    def add_category(id: str, name: str):
        objdiff_config["progress_categories"].append(
            {
                "id": id,
                "name": name,
            }
        )

    if len(build_config["modules"]) > 0:
        add_category("dol", "DOL")
        if config.progress_modules:
            add_category("modules", "Modules")
        if config.progress_each_module:
            for module in build_config["modules"]:
                add_category(module["name"], module["name"])
    for category in config.progress_categories:
        add_category(category.id, category.name)

    def cleandict(d):
        if isinstance(d, dict):
            return {k: cleandict(v) for k, v in d.items() if v is not None}
        elif isinstance(d, list):
            return [cleandict(v) for v in d]
        else:
            return d

    # Write objdiff.json
    with open("objdiff.json", "w", encoding="utf-8") as w:

        def unix_path(input: Any) -> str:
            return str(input).replace(os.sep, "/") if input else ""

        json.dump(cleandict(objdiff_config), w, indent=2, default=unix_path)


def generate_compile_commands(
    config: ProjectConfig,
    objects: Dict[str, Object],
    build_config: Optional[BuildConfig],
) -> None:
    if build_config is None or not config.generate_compile_commands:
        return

    # The following code attempts to convert mwcc flags to clang flags
    # for use with clangd.

    # Flags to ignore explicitly
    CFLAG_IGNORE: Set[str] = {
        # Search order modifier
        # Has a different meaning to Clang, and would otherwise
        # be picked up by the include passthrough prefix
        "-I-",
        "-i-",
    }
    CFLAG_IGNORE_PREFIX: Tuple[str, ...] = (
        # Recursive includes are not supported by modern compilers
        "-ir ",
    )

    # Flags to replace
    CFLAG_REPLACE: Dict[str, str] = {}
    CFLAG_REPLACE_PREFIX: Tuple[Tuple[str, str], ...] = (
        # Includes
        ("-i ", "-I"),
        ("-I ", "-I"),
        ("-I+", "-I"),
        # Defines
        ("-d ", "-D"),
        ("-D ", "-D"),
        ("-D+", "-D"),
    )

    # Flags with a finite set of options
    CFLAG_REPLACE_OPTIONS: Tuple[Tuple[str, Dict[str, Tuple[str, ...]]], ...] = (
        # Exceptions
        (
            "-Cpp_exceptions",
            {
                "off": ("-fno-cxx-exceptions",),
                "on": ("-fcxx-exceptions",),
            },
        ),
        # RTTI
        (
            "-RTTI",
            {
                "off": ("-fno-rtti",),
                "on": ("-frtti",),
            },
        ),
        # Language configuration
        (
            "-lang",
            {
                "c": ("--language=c", "--std=c99"),
                "c99": ("--language=c", "--std=c99"),
                "c++": ("--language=c++", "--std=c++98"),
                "cplus": ("--language=c++", "--std=c++98"),
            },
        ),
        # Enum size
        (
            "-enum",
            {
                "min": ("-fshort-enums",),
                "int": ("-fno-short-enums",),
            },
        ),
        # Common BSS
        (
            "-common",
            {
                "off": ("-fno-common",),
                "on": ("-fcommon",),
            },
        ),
    )

    # Flags to pass through
    CFLAG_PASSTHROUGH: Set[str] = set()
    CFLAG_PASSTHROUGH_PREFIX: Tuple[str, ...] = (
        "-I",  # includes
        "-D",  # defines
    )

    clangd_config = []

    def add_unit(build_obj: BuildConfigUnit) -> None:
        obj = objects.get(build_obj["name"])
        if obj is None:
            return

        # Skip unresolved objects
        if (
            obj.src_path is None
            or obj.src_obj_path is None
            or not file_is_c_cpp(obj.src_path)
        ):
            return

        # Gather cflags for source file
        cflags: list[str] = []

        def append_cflags(flags: Iterable[str]) -> None:
            # Match a flag against either a set of concrete flags, or a set of prefixes.
            def flag_match(
                flag: str, concrete: Set[str], prefixes: Tuple[str, ...]
            ) -> bool:
                if flag in concrete:
                    return True

                for prefix in prefixes:
                    if flag.startswith(prefix):
                        return True

                return False

            # Determine whether a flag should be ignored.
            def should_ignore(flag: str) -> bool:
                return flag_match(flag, CFLAG_IGNORE, CFLAG_IGNORE_PREFIX)

            # Determine whether a flag should be passed through.
            def should_passthrough(flag: str) -> bool:
                return flag_match(flag, CFLAG_PASSTHROUGH, CFLAG_PASSTHROUGH_PREFIX)

            # Attempts replacement for the given flag.
            def try_replace(flag: str) -> bool:
                replacement = CFLAG_REPLACE.get(flag)
                if replacement is not None:
                    cflags.append(replacement)
                    return True

                for prefix, replacement in CFLAG_REPLACE_PREFIX:
                    if flag.startswith(prefix):
                        cflags.append(flag.replace(prefix, replacement, 1))
                        return True

                for prefix, options in CFLAG_REPLACE_OPTIONS:
                    if not flag.startswith(prefix):
                        continue

                    # "-lang c99" and "-lang=c99" are both generally valid option forms
                    option = flag.removeprefix(prefix).removeprefix("=").lstrip()
                    replacements = options.get(option)
                    if replacements is not None:
                        cflags.extend(replacements)

                    return True

                return False

            for flag in flags:
                if flag.startswith("/I "):
                    cflags.extend(flag.split(' '))
                else:
                    cflags.append(flag)

                # # Ignore flags first
                # if should_ignore(flag):
                #     continue

                # # Then find replacements
                # if try_replace(flag):
                #     continue

                # # Pass flags through last
                # if should_passthrough(flag):
                #     cflags.append(flag)
                #     continue

        append_cflags(obj.options["cflags"])
        append_cflags(obj.options["extra_cflags"])
        cflags.extend(config.extra_clang_flags)
        cflags.extend(obj.options["extra_clang_flags"])

        unit_config = {
            "directory": Path.cwd(),
            "file": obj.src_path,
            "output": obj.src_obj_path,
            "arguments": [
                "clang-cl.exe",
                "--target=powerpc-eabi",
                *cflags,
                obj.src_path,
                "/Fo",
                obj.src_obj_path,
            ],
        }
        clangd_config.append(unit_config)

    # Add DOL units
    for unit in build_config["units"]:
        add_unit(unit)

    # Add REL units
    for module in build_config["modules"]:
        for unit in module["units"]:
            add_unit(unit)

    # Write compile_commands.json
    with open("compile_commands.json", "w", encoding="utf-8") as w:

        def default_format(o):
            if isinstance(o, Path):
                return o.resolve().as_posix()
            return str(o)

        json.dump(clangd_config, w, indent=2, default=default_format)


# Print progress information from objdiff report
def calculate_progress(config: ProjectConfig) -> None:
    config.validate()
    out_path = config.out_path()
    report_path = out_path / "report.json"
    if not report_path.is_file():
        sys.exit(f"Report file {report_path} does not exist")

    report_data: Dict[str, Any] = {}
    with open(report_path, "r", encoding="utf-8") as f:
        report_data = json.load(f)

    # Convert string numbers (u64) to int
    def convert_numbers(data: Dict[str, Any]) -> None:
        for key, value in data.items():
            if isinstance(value, str) and value.isdigit():
                data[key] = int(value)

    convert_numbers(report_data["measures"])
    for category in report_data.get("categories", []):
        convert_numbers(category["measures"])

    # Output to GitHub Actions job summary, if available
    summary_path = os.getenv("GITHUB_STEP_SUMMARY")
    summary_file: Optional[IO[str]] = None
    if summary_path:
        summary_file = open(summary_path, "a", encoding="utf-8")
        summary_file.write("```\n")

    def progress_print(s: str) -> None:
        print(s)
        if summary_file:
            summary_file.write(s + "\n")

    # Print human-readable progress
    progress_print("Progress:")

    def print_category(name: str, measures: Dict[str, Any]) -> None:
        total_code = measures.get("total_code", 0)
        matched_code = measures.get("matched_code", 0)
        matched_code_percent = measures.get("matched_code_percent", 0)
        fuzzy_match_percent = measures.get("fuzzy_match_percent", 0)
        total_data = measures.get("total_data", 0)
        matched_data = measures.get("matched_data", 0)
        matched_data_percent = measures.get("matched_data_percent", 0)
        total_functions = measures.get("total_functions", 0)
        matched_functions = measures.get("matched_functions", 0)
        complete_code_percent = measures.get("complete_code_percent", 0)
        total_units = measures.get("total_units", 0)
        complete_units = measures.get("complete_units", 0)

        progress_print(
            f"  {name}: {matched_code_percent:.2f}% matched ({fuzzy_match_percent:.2f}% fuzzy), {complete_code_percent:.2f}% linked ({complete_units} / {total_units} files)"
        )
        progress_print(
            f"    Code: {matched_code} / {total_code} bytes ({matched_functions} / {total_functions} functions)"
        )
        progress_print(
            f"    Data: {matched_data} / {total_data} bytes ({matched_data_percent:.2f}%)"
        )

    print_category("All", report_data["measures"])
    for category in report_data.get("categories", []):
        if config.print_progress_categories is True or (
            isinstance(config.print_progress_categories, list)
            and category["id"] in config.print_progress_categories
        ):
            print_category(category["name"], category["measures"])

    if config.progress_use_fancy:
        measures = report_data["measures"]
        total_code = measures.get("total_code", 0)
        total_data = measures.get("total_data", 0)
        if total_code == 0 or total_data == 0:
            return
        code_frac = measures.get("complete_code", 0) / total_code
        data_frac = measures.get("complete_data", 0) / total_data

        progress_print(
            "\nYou have {} out of {} {} and {} out of {} {}.".format(
                math.floor(code_frac * config.progress_code_fancy_frac),
                config.progress_code_fancy_frac,
                config.progress_code_fancy_item,
                math.floor(data_frac * config.progress_data_fancy_frac),
                config.progress_data_fancy_frac,
                config.progress_data_fancy_item,
            )
        )

    # Finalize GitHub Actions job summary
    if summary_file:
        summary_file.write("```\n")
        summary_file.close()
