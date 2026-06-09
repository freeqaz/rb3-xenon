"""BSF trace capture — compile under GDB and capture all BSF (Bit Scan Forward) calls.

The MSVC x86-to-PPC cross-compiler (c2.dll) uses BSF to pick the lowest
available register color during graph coloring. By tracing every BSF call
we capture the full register allocation sequence, which is deterministic
for a given source ordering.

Usage:
    from tools.compiler_trace.bsf_trace import trace_bsf
    trace = trace_bsf(Path("src/system/obj/Foo.cpp"))
    for call in trace.calls:
        print(f"BSF #{call.index}: bit={call.bit} caller=0x{call.caller_rva:x}")
"""

from __future__ import annotations

import os
import re
import subprocess
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional

from .invoker import (
    CompilerInvoker,
    PROJECT_ROOT,
    C2_IMAGE_BASE,
    _make_cl_path,
    _load_base_cflags,
    _load_include_flags,
)

# Regex patterns for parsing MSVC assembly listings
_PROC_RE = re.compile(r"^(\S+)\s+PROC\s+NEAR")
_ENDP_RE = re.compile(r"^(\S+)\s+ENDP")
_SAVEGPRLR_RE = re.compile(r"__savegprlr_(\d+)")
_STMW_RE = re.compile(r"\bstmw\s+r(\d+)")
# Individual callee-saved register saves: std rN,-X(r1) or stw rN,-X(r1)
# These appear in debug builds instead of __savegprlr_N or stmw
_INDIVIDUAL_SAVE_RE = re.compile(r"\bst[dw]\s+r(\d+)\s*,\s*-")

# 32-bit wibo build (required for GDB — the 64-bit one crashes).
_MILOHAX_DIR = Path(__file__).resolve().parent.parent.parent.parent


def _resolve_wibo_32() -> Path:
    """Locate the 32-bit debug wibo robustly.

    The repo-adjacent path (``_MILOHAX_DIR``) is wrong inside a git worktree:
    ``__file__`` then resolves next to the worktree (e.g. ``/tmp/claude/wt-*``)
    rather than the canonical milohax checkout, so BSF tracing silently falls
    back to unguided mode. Try, in order: an explicit env override, the
    repo-adjacent path, the worktree's *real* repo parent (via git-common-dir),
    and the conventional ``~/code/milohax`` layout. First existing path wins.
    """
    rel = ("wibo", "build", "debug", "wibo")
    env = os.environ.get("BSF_WIBO_32") or os.environ.get("PERMUTER_WIBO_32")
    if env:
        return Path(env)
    candidates = [_MILOHAX_DIR.joinpath(*rel)]
    try:
        here = Path(__file__).resolve().parent
        common = subprocess.run(
            ["git", "rev-parse", "--git-common-dir"],
            cwd=str(here), capture_output=True, text=True, timeout=5,
        )
        if common.returncode == 0 and common.stdout.strip():
            gcd = Path(common.stdout.strip())
            if not gcd.is_absolute():
                gcd = (here / gcd).resolve()
            real_repo = gcd.parent if gcd.name == ".git" else gcd
            candidates.append(real_repo.parent.joinpath(*rel))
    except Exception:
        pass
    candidates.append(Path.home() / "code" / "milohax" / Path(*rel))
    for c in candidates:
        if c.exists():
            return c
    return candidates[0]


WIBO_32 = _resolve_wibo_32()

# BSF function address in c2.dll
BSF_RVA = 0x026780
BSF_VA = C2_IMAGE_BASE + BSF_RVA  # 0x10b26780

# c2.dll loads at callDllMain hit #13
C2_LOAD_HIT = 13

# Regex to parse BSF output lines
_BSF_RE = re.compile(
    r"BSF #(\d+): caller=0x([0-9a-f]+) lo=0x([0-9a-f]+) "
    r"hi=0x([0-9a-f]+) base=(\d+) bit=(-?\d+)"
)


@dataclass
class BSFCall:
    """A single BSF (Bit Scan Forward) call captured during compilation."""

    index: int  # Sequential call number (1-based)
    caller_rva: int  # Return address RVA in c2.dll
    lo: int  # Low 32 bits of availability mask
    hi: int  # High 32 bits of availability mask
    base: int  # Register class base offset
    bit: int  # BSF result (lowest available color)

    @property
    def caller_va(self) -> int:
        return self.caller_rva + C2_IMAGE_BASE


@dataclass
class BSFTrace:
    """Complete BSF trace from a single compilation."""

    source: Path
    calls: list[BSFCall] = field(default_factory=list)

    @property
    def total_calls(self) -> int:
        return len(self.calls)

    def calls_by_caller(self) -> dict[int, list[BSFCall]]:
        """Group BSF calls by caller RVA."""
        groups: dict[int, list[BSFCall]] = {}
        for call in self.calls:
            groups.setdefault(call.caller_rva, []).append(call)
        return groups

    def phase_calls(self, caller_rva: int) -> list[BSFCall]:
        """Get BSF calls from a specific compiler phase (by caller RVA)."""
        return [c for c in self.calls if c.caller_rva == caller_rva]

    def partition_by_function(
        self, asm_lines: list[str]
    ) -> dict[str, "BSFTrace"]:
        """Partition BSF trace by function using assembly listing.

        Parses PROC NEAR/ENDP markers from the assembly listing to get
        function names in source order. Counts callee-saved registers per
        function from __savegprlr_N / stmw patterns. Then partitions the
        initial coloring BSF calls by consuming the expected number of
        distinct colors for each function in order.

        Args:
            asm_lines: Lines from MSVC assembly listing (/FAs output).

        Returns:
            Dict mapping function name to a BSFTrace containing only that
            function's initial coloring BSF calls. Falls back to returning
            {'__all__': self} if partitioning fails.
        """
        from .regmap_solver import INITIAL_COLORING_RVA

        # Step 1: Parse function order and callee-saved register counts
        func_info = _parse_function_info(asm_lines)
        if not func_info:
            return {"__all__": self}

        # Step 2: Get initial coloring calls (the ones sensitive to decl order)
        initial_calls = self.phase_calls(INITIAL_COLORING_RVA)
        if not initial_calls:
            return {"__all__": self}

        # Step 3: Walk initial coloring calls, consuming N distinct colors
        # per function. Each function's initial coloring is a contiguous block
        # of BSF calls where we see N new (previously unseen within this
        # function) colors.
        result: dict[str, BSFTrace] = {}
        call_idx = 0

        for func_name, n_callee_saved in func_info:
            if n_callee_saved == 0:
                # Function uses no callee-saved registers — skip it
                result[func_name] = BSFTrace(source=self.source, calls=[])
                continue

            func_calls: list[BSFCall] = []
            seen_colors: set[int] = set()
            distinct_count = 0

            while call_idx < len(initial_calls) and distinct_count < n_callee_saved:
                call = initial_calls[call_idx]
                func_calls.append(call)
                if call.bit >= 0 and call.bit not in seen_colors:
                    seen_colors.add(call.bit)
                    distinct_count += 1
                call_idx += 1

            result[func_name] = BSFTrace(source=self.source, calls=func_calls)

        # Any remaining calls go into __remainder__
        if call_idx < len(initial_calls):
            result["__remainder__"] = BSFTrace(
                source=self.source, calls=initial_calls[call_idx:]
            )

        return result


def _parse_function_info(asm_lines: list[str]) -> list[tuple[str, int]]:
    """Parse function names and callee-saved register counts from assembly listing.

    Returns a list of (function_name, n_callee_saved) in source order.
    Callee-saved count is determined from:
    - __savegprlr_N (saves r_N through r31) — optimized builds
    - stmw r_N (store multiple, saves r_N through r31) — optimized builds
    - Individual std/stw r_N,-offset(r1) in prologue — debug builds
    """
    functions: list[tuple[str, int]] = []
    current_func: str | None = None
    current_callee_saved = 0
    callee_saved_regs: set[int] = set()  # Track individual saves
    in_prologue = True  # Only count saves before first bl or .endprolog

    for line in asm_lines:
        stripped = line.strip()

        # Detect function start
        m = _PROC_RE.match(stripped)
        if m:
            if current_func is not None:
                count = max(current_callee_saved, len(callee_saved_regs))
                functions.append((current_func, count))
            current_func = m.group(1)
            current_callee_saved = 0
            callee_saved_regs = set()
            in_prologue = True
            continue

        # Detect function end
        m = _ENDP_RE.match(stripped)
        if m:
            if current_func is not None:
                count = max(current_callee_saved, len(callee_saved_regs))
                functions.append((current_func, count))
                current_func = None
                current_callee_saved = 0
                callee_saved_regs = set()
                in_prologue = True
            continue

        if current_func is None:
            continue

        # End of prologue markers
        if ".endprolog" in stripped or (stripped.startswith("bl ") and "__savegprlr" not in stripped):
            in_prologue = False

        # Count callee-saved registers from __savegprlr_N pattern
        m = _SAVEGPRLR_RE.search(stripped)
        if m:
            first_saved = int(m.group(1))
            count = 32 - first_saved  # saves r_N through r31
            current_callee_saved = max(current_callee_saved, count)
            continue

        # Count from stmw r_N (store multiple word from r_N to r31)
        m = _STMW_RE.search(stripped)
        if m:
            first_saved = int(m.group(1))
            if 13 <= first_saved <= 31:
                count = 32 - first_saved
                current_callee_saved = max(current_callee_saved, count)
            continue

        # Count individual callee-saved saves in prologue: std/stw rN,-offset(r1)
        # Debug builds save registers individually instead of using __savegprlr_N
        if in_prologue:
            m = _INDIVIDUAL_SAVE_RE.search(stripped)
            if m:
                reg_num = int(m.group(1))
                if 13 <= reg_num <= 31:
                    callee_saved_regs.add(reg_num)

    # Flush last function
    if current_func is not None:
        count = max(current_callee_saved, len(callee_saved_regs))
        functions.append((current_func, count))

    return functions


def _generate_gdb_script(
    source: Path,
    obj_output: Path,
    extra_flags: list[str] | None = None,
) -> str:
    """Generate a GDB batch script for BSF tracing.

    Based on the working template at /tmp/claude/bsf_trace_a.gdb.
    """
    # Build the cl.exe command line (without wibo prefix — we'll use WIBO_32)
    invoker = CompilerInvoker()
    # Get the full command and strip the wibo prefix
    cmd = invoker.base_command(source, obj_output, extra_flags)
    # cmd[0] is wibo path, cmd[1] is cl.exe, rest are flags
    cl_args = " ".join(cmd[1:])

    lines = [
        "# Auto-generated BSF trace script",
        "set confirm off",
        "set pagination off",
        "set debuginfod enabled off",
        'set libthread-db-search-path ""',
        "set print elements 0",
        "",
        f"file {WIBO_32}",
        f"set args {cl_args}",
        "",
        "# Run until c2.dll is loaded (callDllMain hit #13)",
        "break callDllMain",
        "run",
        "",
        f"# Skip through {C2_LOAD_HIT - 1} callDllMain hits",
        "set $i = 0",
        f"while $i < {C2_LOAD_HIT - 1}",
        "  set $i = $i + 1",
        "  continue",
        "end",
        "",
        "# Verify c2.dll is loaded",
        f"set $val = *(unsigned char*)0x{BSF_VA:08x}",
        f'printf "### At callDllMain hit #{C2_LOAD_HIT}: BSF byte = 0x%02x\\n", $val',
        "",
        "# Delete callDllMain breakpoint, set BSF breakpoint",
        "delete 1",
        f"break *0x{BSF_VA:08x}",
        "",
        "# Trace all BSF calls",
        "set $n = 0",
        "set $done = 0",
        "while $done == 0",
        "  continue",
        "  if $_isvoid($eip)",
        "    set $done = 1",
        "  else",
        f"    if $eip == 0x{BSF_VA:08x}",
        "      set $n = $n + 1",
        "      set $caller = *(unsigned int*)$esp",
        "      set $lo = *(unsigned int*)($esp + 4)",
        "      set $hi = *(unsigned int*)($esp + 8)",
        "      set $node_ptr = *(unsigned int*)$edx",
        "      set $base = 0",
        "      if $node_ptr != 0",
        "        set $base = *(unsigned int*)$node_ptr",
        "      end",
        "      set $bit = -1",
        "      if $lo != 0",
        "        set $tmp = $lo",
        "        set $bit = 0",
        "        while ($tmp & 1) == 0",
        "          set $tmp = $tmp >> 1",
        "          set $bit = $bit + 1",
        "        end",
        "      end",
        "      if $lo == 0 && $hi != 0",
        "        set $tmp = $hi",
        "        set $bit = 32",
        "        while ($tmp & 1) == 0",
        "          set $tmp = $tmp >> 1",
        "          set $bit = $bit + 1",
        "        end",
        "      end",
        '      printf "BSF #%d: caller=0x%08x lo=0x%08x hi=0x%08x base=%d bit=%d\\n", $n, $caller, $lo, $hi, $base, $bit',
        "    else",
        "      set $done = 1",
        "    end",
        "  end",
        "end",
        "",
        'printf "### Total BSF calls: %d\\n", $n',
        "quit",
    ]
    return "\n".join(lines)


def _parse_bsf_output(output: str, source: Path) -> BSFTrace:
    """Parse GDB output into a BSFTrace."""
    trace = BSFTrace(source=source)
    for match in _BSF_RE.finditer(output):
        index = int(match.group(1))
        caller_va = int(match.group(2), 16)
        lo = int(match.group(3), 16)
        hi = int(match.group(4), 16)
        base = int(match.group(5))
        bit = int(match.group(6))

        caller_rva = caller_va - C2_IMAGE_BASE

        trace.calls.append(
            BSFCall(
                index=index,
                caller_rva=caller_rva,
                lo=lo,
                hi=hi,
                base=base,
                bit=bit,
            )
        )
    return trace


def trace_bsf(
    source: Path,
    extra_flags: list[str] | None = None,
    timeout: int = 300,
    verbose: bool = False,
) -> BSFTrace:
    """Compile source under GDB and capture all BSF calls.

    Args:
        source: Path to C++ source file
        extra_flags: Additional cl.exe flags
        timeout: GDB timeout in seconds (default: 5 minutes)
        verbose: Print GDB output to stderr

    Returns:
        BSFTrace with all captured BSF calls
    """
    if not WIBO_32.exists():
        raise FileNotFoundError(
            f"32-bit wibo not found at {WIBO_32}. "
            f"Build with: cd {_MILOHAX_DIR / 'wibo'} && mkdir -p build/debug && "
            "cd build/debug && cmake -DCMAKE_BUILD_TYPE=Debug ../.. && make"
        )

    # Create temp files for GDB script and object output
    with tempfile.NamedTemporaryFile(
        suffix=".gdb", prefix="bsf_trace_", dir="/tmp/claude", delete=False, mode="w"
    ) as gdb_f:
        obj_path = Path(tempfile.mktemp(suffix=".obj", prefix="bsf_", dir="/tmp/claude"))
        script = _generate_gdb_script(source, obj_path, extra_flags)
        gdb_f.write(script)
        gdb_path = Path(gdb_f.name)

    # WIBO_PATH_MAP for mapping e:\ paths to local src/
    wibo_path_map = (
        f"e:/lazer_build_gmc1/system/src/={PROJECT_ROOT}/src/system;"
        f"e:/lazer_build_gmc1/lazer/src/={PROJECT_ROOT}/src/lazer"
    )
    env = os.environ.copy()
    env["WIBO_PATH_MAP"] = wibo_path_map

    try:
        result = subprocess.run(
            ["gdb", "-batch", "-x", str(gdb_path)],
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            timeout=timeout,
            env=env,
        )

        output = result.stdout + result.stderr
        if verbose:
            import sys
            print(output, file=sys.stderr)

        trace = _parse_bsf_output(output, source)

        if trace.total_calls == 0:
            # Check for common errors
            if "No such file" in output:
                raise RuntimeError(f"GDB could not find wibo or source: {output[:500]}")
            if "not in executable format" in output:
                raise RuntimeError(f"Wrong wibo binary format: {output[:500]}")
            raise RuntimeError(
                f"No BSF calls captured. GDB return code: {result.returncode}\n"
                f"Last 500 chars of output: {output[-500:]}"
            )

        return trace

    finally:
        # Cleanup temp files
        gdb_path.unlink(missing_ok=True)
        obj_path.unlink(missing_ok=True)


def cmd_bsf_trace(args) -> None:
    """Entry point for bsf-trace subcommand."""
    import sys

    source = Path(args.source).resolve()
    extra_flags = args.extra_flags.split() if hasattr(args, "extra_flags") and args.extra_flags else None
    verbose = getattr(args, "verbose", False)

    print(f"Tracing BSF calls for {source.name}...", file=sys.stderr)
    trace = trace_bsf(source, extra_flags=extra_flags, verbose=verbose)
    print(f"Captured {trace.total_calls} BSF calls", file=sys.stderr)

    # Group by caller
    by_caller = trace.calls_by_caller()
    print(f"Caller phases: {len(by_caller)}", file=sys.stderr)
    for rva, calls in sorted(by_caller.items()):
        print(f"  RVA 0x{rva:06x}: {len(calls)} calls", file=sys.stderr)

    # Print full trace to stdout
    for call in trace.calls:
        print(
            f"BSF #{call.index}: caller=0x{call.caller_va:08x} "
            f"lo=0x{call.lo:08x} hi=0x{call.hi:08x} "
            f"base={call.base} bit={call.bit}"
        )
