"""rr-record: record compilation under rr for deterministic replay debugging.

Records the wibo + cl.exe compilation process so it can be replayed with
rr replay, allowing reverse debugging through c2.dll's register allocator.

Requirements:
- wibo must be built as 32-bit (i386). The 64-bit build uses far jumps
  for 64/32-bit mode switching which valgrind/rr cannot handle.
  Build with: cmake --preset debug && cmake --build build/debug
- AMD Zen CPUs: rr needs -S -F flags to suppress SpecLockMap warnings.
  For reliable recording, disable SpecLockMap (see rr wiki).
- rr must have 32-bit support (librrpage_32.so). On Arch Linux, the
  default rr package lacks this; rebuild from source or AUR with multilib.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional

from .invoker import CompilerInvoker, PROJECT_ROOT


# Custom rr build with 32-bit support (librrpage_32.so).
# The system rr package on Arch Linux lacks 32-bit libraries.
_MILOHAX_DIR = Path(__file__).resolve().parent.parent.parent.parent
CUSTOM_RR = _MILOHAX_DIR / "rr" / "build" / "bin" / "rr"
CUSTOM_RR_LIB = _MILOHAX_DIR / "rr" / "build" / "lib" / "rr"


def _find_rr() -> str:
    """Find rr binary, preferring custom build with 32-bit support."""
    if CUSTOM_RR.exists():
        # Verify 32-bit support
        if (CUSTOM_RR_LIB / "librrpage_32.so").exists():
            return str(CUSTOM_RR)
        print("Warning: custom rr lacks librrpage_32.so, falling back to system rr",
              file=sys.stderr)
    rr = shutil.which("rr")
    if rr:
        return rr
    return "rr"  # let it fail with a clear error


def record_compilation(
    source: Path,
    trace_dir: Path,
    invoker: Optional[CompilerInvoker] = None,
) -> subprocess.CompletedProcess:
    """Record a compilation under rr.

    Args:
        source: Source file to compile
        trace_dir: Directory for the rr trace output
        invoker: CompilerInvoker instance (created if not provided)
    """
    if invoker is None:
        invoker = CompilerInvoker()

    obj_path = trace_dir / (source.stem + ".obj")
    cmd = invoker.base_command(source, obj_path)

    trace_dir.mkdir(parents=True, exist_ok=True)

    rr_bin = _find_rr()
    rr_cmd = [
        rr_bin, "record",
        "-S",  # Suppress environment warnings (AMD Zen SpecLockMap)
        "-F",  # Force recording despite non-fatal warnings
        "--output-trace-dir", str(trace_dir / "trace"),
    ] + cmd

    # rr needs a writable home for its trace data.
    # _RR_TRACE_DIR overrides the default ~/.local/share/rr
    env = os.environ.copy()
    env["_RR_TRACE_DIR"] = str(trace_dir)

    # Custom rr needs its lib dir for 32-bit support libraries
    if rr_bin == str(CUSTOM_RR) and CUSTOM_RR_LIB.exists():
        existing = env.get("LD_LIBRARY_PATH", "")
        env["LD_LIBRARY_PATH"] = f"{CUSTOM_RR_LIB}:{existing}" if existing else str(CUSTOM_RR_LIB)

    return subprocess.run(
        rr_cmd,
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        env=env,
    )


def cmd_rr_record(args) -> None:
    """Entry point for rr-record subcommand."""
    source = Path(args.source).resolve()
    trace_dir = Path(args.trace_dir).resolve()

    if not source.exists():
        print(f"Error: {source} not found", file=sys.stderr)
        sys.exit(1)

    invoker = CompilerInvoker()

    if args.source_b:
        # Dual recording mode
        source_b = Path(args.source_b).resolve()
        if not source_b.exists():
            print(f"Error: {source_b} not found", file=sys.stderr)
            sys.exit(1)

        dir_a = trace_dir / "a"
        dir_b = trace_dir / "b"

        print(f"Recording {source.name}...")
        result_a = record_compilation(source, dir_a, invoker)
        if result_a.returncode != 0:
            print(f"Warning: recording returned {result_a.returncode}", file=sys.stderr)
            if result_a.stderr:
                print(result_a.stderr[:500], file=sys.stderr)
        else:
            print(f"  Trace: {dir_a / 'trace'}")

        print(f"Recording {source_b.name}...")
        result_b = record_compilation(source_b, dir_b, invoker)
        if result_b.returncode != 0:
            print(f"Warning: recording returned {result_b.returncode}", file=sys.stderr)
            if result_b.stderr:
                print(result_b.stderr[:500], file=sys.stderr)
        else:
            print(f"  Trace: {dir_b / 'trace'}")

        rr_bin = _find_rr()
        print(f"\nReplay with:")
        print(f"  {rr_bin} replay {dir_a / 'trace'}")
        print(f"  {rr_bin} replay {dir_b / 'trace'}")

    else:
        # Single recording
        print(f"Recording {source.name}...")
        result = record_compilation(source, trace_dir, invoker)

        if result.returncode != 0:
            print(f"Error: recording failed (rc={result.returncode})", file=sys.stderr)
            if result.stderr:
                print(result.stderr[:1000], file=sys.stderr)
            sys.exit(1)

        trace_path = trace_dir / "trace"
        print(f"Trace recorded: {trace_path}")
        rr_bin = _find_rr()
        print(f"\nReplay with:")
        print(f"  {rr_bin} replay {trace_path}")
