"""CompilerInvoker: wraps wibo + cl.exe with project flags."""

import json
import subprocess
import tempfile
from pathlib import Path
from typing import List, Optional


# Project root (tools/compiler_trace/invoker.py -> project root)
PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

WIBO = PROJECT_ROOT / "build" / "tools" / "wibo"
COMPILER_DIR = PROJECT_ROOT / "build" / "compilers" / "X360" / "16.00.11886.00"
CL_EXE = COMPILER_DIR / "cl.exe"

# c2.dll address space info (for callgrind/gdb filtering)
C2_DLL = COMPILER_DIR / "c2.dll"
C2_IMAGE_BASE = 0x10B00000
C2_TEXT_RVA = 0x1000
C2_TEXT_SIZE = 0x12CC7C
C2_TEXT_START = C2_IMAGE_BASE + C2_TEXT_RVA  # 0x10B01000
C2_TEXT_END = C2_TEXT_START + C2_TEXT_SIZE


def _to_wibo_path(path: Path) -> str:
    """Convert an absolute path to a wibo-compatible path.

    Wibo maps Z:\\ to /. Absolute Linux paths starting with / get
    misinterpreted as MSVC flags, so we convert them.
    Relative paths are left as-is (the build system uses relative paths from
    the project root for source/include paths).
    """
    s = str(path)
    if s.startswith("/"):
        return "Z:" + s.replace("/", "\\")
    return s


def _make_cl_path(path: Path) -> str:
    """Convert a path for use as a cl.exe argument.

    Tries to make the path relative to PROJECT_ROOT (the build system uses
    relative paths). If the path is outside the project, converts to wibo
    Z:\\ notation.
    """
    try:
        rel = path.resolve().relative_to(PROJECT_ROOT)
        return str(rel)
    except ValueError:
        # Path is outside project root — use wibo Z: mapping
        return _to_wibo_path(path.resolve())


def _load_base_cflags() -> List[str]:
    config_path = PROJECT_ROOT / "config" / "45410914" / "config.json"
    with open(config_path) as f:
        config = json.load(f)
    return list(config["cflags"]["base"]["flags"])


def _load_include_flags() -> List[str]:
    # Parse include paths from defines_common.py
    defines_path = PROJECT_ROOT / "tools" / "defines_common.py"
    ns: dict = {}
    exec(defines_path.read_text(), ns)
    # Split "/I path" into ["/I", "path"] for subprocess argument passing
    result = []
    for flag in ns["cflags_includes"]:
        path_str = ""
        if flag.startswith("/I "):
            path_str = flag[3:]
        else:
            path_str = flag

        # Map original build paths to local source tree
        if path_str.startswith("e:/lazer_build_gmc1/system/src"):
            local_path = PROJECT_ROOT / "src" / "system" / path_str[len("e:/lazer_build_gmc1/system/src") :].lstrip("/")
            result.extend(["/I", str(local_path)])
        elif path_str.startswith("e:/lazer_build_gmc1/lazer/src"):
            local_path = PROJECT_ROOT / "src" / "lazer" / path_str[len("e:/lazer_build_gmc1/lazer/src") :].lstrip("/")
            result.extend(["/I", str(local_path)])
        elif flag.startswith("/I "):
            result.extend(["/I", path_str])
        else:
            result.append(flag)
    return result


class CompilerInvoker:
    """Wraps wibo + cl.exe invocation with correct project flags."""

    def __init__(self, extra_cflags: Optional[List[str]] = None):
        self.wibo = WIBO
        self.cl_exe = CL_EXE
        self.base_cflags = _load_base_cflags()
        self.include_flags = _load_include_flags()
        self.extra_cflags = extra_cflags or []

    def base_command(
        self,
        source: Path,
        output: Path,
        extra_flags: Optional[List[str]] = None,
    ) -> List[str]:
        """Build the full command line for a compilation.

        Paths are relativized to PROJECT_ROOT when possible, or converted
        to wibo Z:\\ paths for absolute paths outside the project.
        """
        cmd = [str(self.wibo), str(self.cl_exe)]
        cmd.extend(self.base_cflags)
        cmd.extend(self.include_flags)
        cmd.extend(self.extra_cflags)
        if extra_flags:
            cmd.extend(extra_flags)

        # Convert paths: try relative to PROJECT_ROOT first, fall back to Z:\ paths
        out_str = _make_cl_path(output)
        src_str = _make_cl_path(source)
        cmd.extend([f"/Fo{out_str}", src_str])
        return cmd

    @staticmethod
    def make_cl_path(path: Path) -> str:
        """Public path conversion for use by submodules."""
        return _make_cl_path(path)

    def compile(
        self,
        source: Path,
        output: Path,
        extra_flags: Optional[List[str]] = None,
        cwd: Optional[Path] = None,
        env: Optional[dict] = None,
    ) -> subprocess.CompletedProcess:
        """Compile a source file to an object file."""
        cmd = self.base_command(source, output, extra_flags)
        return subprocess.run(
            cmd,
            cwd=cwd or PROJECT_ROOT,
            capture_output=True,
            text=True,
            env=env,
        )

    def compile_with_asm(
        self,
        source: Path,
        output_dir: Path,
        extra_flags: Optional[List[str]] = None,
        listing_type: str = "/FAs",
    ) -> subprocess.CompletedProcess:
        """Compile with assembly listing output.

        listing_type: /FA (asm only), /FAs (asm+source), /FAcs (code+source)
        """
        output_dir.mkdir(parents=True, exist_ok=True)
        obj_path = output_dir / (source.stem + ".obj")
        fa_path = _make_cl_path(output_dir) + "\\"
        asm_flags = [listing_type, f"/Fa{fa_path}"]
        all_flags = (extra_flags or []) + asm_flags
        return self.compile(source, obj_path, extra_flags=all_flags)

    def wrap_command(
        self,
        source: Path,
        output: Path,
        extra_flags: Optional[List[str]] = None,
    ) -> List[str]:
        """Return the command list without wibo prefix (for wrapping with
        strace, valgrind, rr, etc.)."""
        cmd = self.base_command(source, output, extra_flags)
        # cmd[0] is wibo, rest is cl.exe + flags
        return cmd
