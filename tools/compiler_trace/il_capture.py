"""capture-il: intercept compiler IL temp files via strace inject.

The MSVC X360 compiler (cl.exe -> c1xx.dll -> c2.dll) creates temporary
intermediate language files (_CL_*) during compilation and deletes them
when done. By using strace's inject feature to make unlink() return 0
without actually deleting, we can capture these files for analysis.

IL file extensions:
    sy - symbol table
    ex - expression tree
    gl - globals
    in - includes
    db - debug info
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List, Optional, Tuple

from .invoker import CompilerInvoker, PROJECT_ROOT


IL_EXTENSIONS = ["sy", "ex", "gl", "in", "db"]


def capture_il_files(
    source: Path,
    output_dir: Path,
    invoker: Optional[CompilerInvoker] = None,
) -> List[Path]:
    """Compile a source file and capture the IL temp files.

    Uses strace to intercept unlink() calls and prevent deletion of _CL_* files.
    Returns list of captured IL file paths.
    """
    if invoker is None:
        invoker = CompilerInvoker()

    output_dir.mkdir(parents=True, exist_ok=True)

    # We need to compile in a temp directory so IL files land somewhere known.
    # The compiler creates _CL_* files in the current working directory or
    # the TEMP directory.
    with tempfile.TemporaryDirectory(prefix="il_capture_") as tmpdir:
        tmpdir = Path(tmpdir)
        obj_path = tmpdir / (source.stem + ".obj")

        # Build the compilation command
        cmd = invoker.base_command(source, obj_path)

        # Wrap with strace: intercept unlink and inject retval=0
        # This makes the compiler think it deleted the files, but they survive
        strace_cmd = [
            "strace",
            "-e", "trace=unlink,unlinkat",
            "-e", "inject=unlink,unlinkat:retval=0",
            "-o", "/dev/null",  # discard strace output
        ] + cmd

        env = os.environ.copy()
        # Set TMP/TEMP to our tmpdir so _CL_* files are created there
        env["TMP"] = str(tmpdir)
        env["TEMP"] = str(tmpdir)

        result = subprocess.run(
            strace_cmd,
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            env=env,
        )

        if result.returncode != 0:
            print(f"Compilation failed (rc={result.returncode}):", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            # Don't exit - IL files may still have been created before failure

        # Collect _CL_* files
        captured = []
        for f in sorted(tmpdir.glob("_CL_*")):
            dest = output_dir / f.name
            shutil.copy2(f, dest)
            captured.append(dest)

    return captured


def group_il_files(files: List[Path]) -> dict:
    """Group IL files by their hash prefix.

    _CL_<hash><ext> -> {hash: {ext: path, ...}, ...}
    """
    groups = {}
    pattern = re.compile(r"_CL_([0-9a-f]+)(db|ex|gl|in|sy)$")
    for f in files:
        m = pattern.match(f.name)
        if m:
            hash_id, ext = m.groups()
            groups.setdefault(hash_id, {})[ext] = f
    return groups


def hex_diff_files(file_a: Path, file_b: Path, max_bytes: int = 4096) -> str:
    """Produce a hex dump diff of two binary files."""
    import difflib

    def hex_dump(data: bytes, width: int = 16) -> List[str]:
        lines = []
        for i in range(0, len(data), width):
            chunk = data[i : i + width]
            hex_part = " ".join(f"{b:02x}" for b in chunk)
            ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
            lines.append(f"{i:08x}  {hex_part:<{width*3}}  {ascii_part}")
        return lines

    data_a = file_a.read_bytes()[:max_bytes]
    data_b = file_b.read_bytes()[:max_bytes]

    dump_a = hex_dump(data_a)
    dump_b = hex_dump(data_b)

    diff = difflib.unified_diff(
        dump_a, dump_b,
        fromfile=file_a.name, tofile=file_b.name,
        lineterm="",
    )
    return "\n".join(diff)


def cmd_capture_il(args) -> None:
    """Entry point for capture-il subcommand."""
    source = Path(args.source).resolve()
    output_dir = Path(args.output_dir).resolve()

    if not source.exists():
        print(f"Error: {source} not found", file=sys.stderr)
        sys.exit(1)

    invoker = CompilerInvoker()

    if args.source_b:
        # Diff mode: capture IL from both and compare
        source_b = Path(args.source_b).resolve()
        if not source_b.exists():
            print(f"Error: {source_b} not found", file=sys.stderr)
            sys.exit(1)

        dir_a = output_dir / "a"
        dir_b = output_dir / "b"

        print(f"Capturing IL for {source.name}...")
        files_a = capture_il_files(source, dir_a, invoker)
        print(f"  Captured {len(files_a)} files")

        print(f"Capturing IL for {source_b.name}...")
        files_b = capture_il_files(source_b, dir_b, invoker)
        print(f"  Captured {len(files_b)} files")

        # Group and diff
        groups_a = group_il_files(files_a)
        groups_b = group_il_files(files_b)

        # Match groups by extension set (they have different hashes)
        for ext in IL_EXTENSIONS:
            fa = None
            fb = None
            for g in groups_a.values():
                if ext in g:
                    fa = g[ext]
                    break
            for g in groups_b.values():
                if ext in g:
                    fb = g[ext]
                    break

            if fa and fb:
                size_a = fa.stat().st_size
                size_b = fb.stat().st_size
                print(f"\n--- IL .{ext} files (A: {size_a}B, B: {size_b}B) ---")
                if size_a != size_b:
                    print(f"  Size differs: {size_a} vs {size_b}")
                diff = hex_diff_files(fa, fb)
                if diff:
                    print(diff)
                else:
                    print("  Identical")
            elif fa:
                print(f"\n.{ext}: only in A ({fa.stat().st_size}B)")
            elif fb:
                print(f"\n.{ext}: only in B ({fb.stat().st_size}B)")
    else:
        # Single capture mode
        print(f"Capturing IL for {source.name}...")
        files = capture_il_files(source, output_dir, invoker)

        if not files:
            print("No IL files captured.", file=sys.stderr)
            sys.exit(1)

        print(f"Captured {len(files)} files:")
        groups = group_il_files(files)
        for hash_id, exts in groups.items():
            print(f"  _CL_{hash_id}:")
            for ext, path in sorted(exts.items()):
                size = path.stat().st_size
                print(f"    .{ext}: {size:,} bytes")
