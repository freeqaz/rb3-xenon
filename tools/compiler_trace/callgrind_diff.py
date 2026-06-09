"""callgrind-diff: instruction-level profiling of c2.dll.

Compiles two source variants with profiling, then compares per-address
execution patterns within c2.dll's .text section. Addresses where behavior
differs between variants indicate code paths sensitive to source differences
(e.g., register allocation decisions).

Supports two backends:
- callgrind (default): Deterministic per-instruction counts via valgrind.
  Requires 32-bit wibo and valgrind with callgrind support.
- perf (--perf): Sampling-based profiling. Noisier but no valgrind needed.
"""

import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from .invoker import (
    CompilerInvoker,
    PROJECT_ROOT,
    C2_IMAGE_BASE,
    C2_TEXT_START,
    C2_TEXT_END,
)
from .funcmap import C2FuncMap, DEFAULT_FUNCMAP_PATH


# --- Dependency checks ---


def _check_valgrind() -> bool:
    """Check if valgrind with callgrind is available."""
    try:
        result = subprocess.run(
            ["valgrind", "--version"], capture_output=True, text=True
        )
        return result.returncode == 0
    except FileNotFoundError:
        return False


def _check_perf() -> bool:
    """Check if perf is available and usable."""
    try:
        result = subprocess.run(
            ["perf", "version"], capture_output=True, text=True
        )
        return result.returncode == 0
    except FileNotFoundError:
        return False


# --- Callgrind backend ---


def run_callgrind(
    source: Path,
    output_file: Path,
    invoker: CompilerInvoker,
) -> subprocess.CompletedProcess:
    """Compile a source file under valgrind/callgrind.

    Produces a callgrind output file with per-instruction execution counts.
    The --dump-instr=yes flag gives instruction-granularity data, and
    --collect-jumps=yes records branch taken/not-taken counts.
    """
    obj_path = output_file.with_suffix(".obj")
    cmd = invoker.base_command(source, obj_path)

    callgrind_cmd = [
        "valgrind",
        "--tool=callgrind",
        "--dump-instr=yes",
        "--collect-jumps=yes",
        f"--callgrind-out-file={output_file}",
    ] + cmd

    # Set TMPDIR to project tmp/ so valgrind's shared memory files
    # aren't affected by /tmp tmpfs quota issues
    import os
    env = os.environ.copy()
    env["TMPDIR"] = str(PROJECT_ROOT / "tmp")

    return subprocess.run(
        callgrind_cmd,
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        env=env,
    )


def _parse_position(token: str, last_value: int) -> Optional[int]:
    """Parse a callgrind position field (absolute, relative, or repeat)."""
    if token == "*":
        return last_value
    if token.startswith("0x") or token.startswith("0X"):
        try:
            return int(token, 16)
        except ValueError:
            return None
    if token.startswith("+"):
        try:
            return last_value + int(token[1:])
        except ValueError:
            return None
    if token.startswith("-"):
        try:
            return last_value - int(token[1:])
        except ValueError:
            return None
    # Plain decimal number (absolute)
    try:
        return int(token)
    except ValueError:
        return None


def parse_callgrind_output(callgrind_file: Path) -> Dict[int, int]:
    """Parse callgrind output and extract per-address execution counts.

    Callgrind format with ``positions: instr line`` and ``events: Ir``::

        0x10B01000 42 3    # addr=0x10B01000, line=42, Ir=3
        +4 +1 3            # addr+=4, line+=1, Ir=3
        * * 1              # same addr, same line, Ir=1

    Each position field (instr, line) can be absolute (``0x...`` / decimal),
    relative (``+N`` / ``-N``), or repeat (``*``).  The last field is always
    the event count (Ir).  We track both position cursors but only use the
    instruction address for our purposes.

    Returns {address: execution_count} filtered to c2.dll .text range.
    """
    counts: Dict[int, int] = defaultdict(int)
    last_addr = 0
    last_line = 0
    num_positions = 1  # default: just instr

    with open(callgrind_file) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or line.startswith("="):
                continue

            # Detect position format
            if line.startswith("positions:"):
                fields = line.split(":")[1].strip().split()
                num_positions = len(fields)
                continue

            # Skip header/metadata lines
            if line.startswith(("events:", "fl=", "fn=", "fi=", "fe=",
                                "ob=", "cfl=", "cfn=", "cfi=", "cob=",
                                "calls=", "jump=", "jcnd=",
                                "summary:", "totals:", "cmd:",
                                "creator:", "pid:", "desc:",
                                "version:", "part:", "event:")):
                continue
            if "=" in line and not line[0].isdigit() and not line.startswith(("+", "*", "0")):
                continue

            parts = line.split()
            # Need at least num_positions + 1 fields (positions + count)
            if len(parts) < num_positions + 1:
                continue

            # Parse instruction address (first position field)
            addr_part = parts[0]
            addr = _parse_position(addr_part, last_addr)
            if addr is None:
                continue
            last_addr = addr

            # Skip intermediate position fields (e.g. line number)
            # and parse the event count (last field)
            try:
                count = int(parts[num_positions])
            except (ValueError, IndexError):
                continue

            # Update line cursor if present
            if num_positions >= 2:
                line_part = parts[1]
                parsed_line = _parse_position(line_part, last_line)
                if parsed_line is not None:
                    last_line = parsed_line

            # Filter to c2.dll .text range
            if C2_TEXT_START <= addr < C2_TEXT_END:
                counts[addr] += count

    return dict(counts)


# --- Perf backend ---


def run_perf_record(
    source: Path,
    output_file: Path,
    invoker: CompilerInvoker,
) -> subprocess.CompletedProcess:
    """Compile a source file under perf recording."""
    obj_path = output_file.with_suffix(".obj")
    cmd = invoker.base_command(source, obj_path)

    perf_cmd = [
        "perf", "record",
        "-e", "instructions:u",
        "--freq", "max",
        "-o", str(output_file),
        "--",
    ] + cmd

    return subprocess.run(
        perf_cmd,
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
    )


def parse_perf_report(perf_data: Path) -> Dict[int, int]:
    """Parse perf data and extract per-address sample counts.

    Uses `perf script` to get raw samples, then filters to c2.dll range.
    Returns {address: sample_count}.
    """
    counts: Dict[int, int] = defaultdict(int)

    # perf script outputs: comm pid tid time: addr symbol
    result = subprocess.run(
        ["perf", "script", "-i", str(perf_data), "-F", "ip"],
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        return {}

    for line in result.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            addr = int(line, 16)
            if C2_TEXT_START <= addr < C2_TEXT_END:
                counts[addr] += 1
        except ValueError:
            pass

    return dict(counts)


# --- Shared analysis ---


def compute_delta(
    counts_a: Dict[int, int], counts_b: Dict[int, int]
) -> List[Tuple[int, int, int, int]]:
    """Compute per-address deltas between two profiles.

    Returns [(address, count_a, count_b, delta)] sorted by |delta| descending.
    Only includes addresses where counts differ.
    """
    all_addrs = set(counts_a.keys()) | set(counts_b.keys())
    deltas = []

    for addr in all_addrs:
        ca = counts_a.get(addr, 0)
        cb = counts_b.get(addr, 0)
        if ca != cb:
            deltas.append((addr, ca, cb, ca - cb))

    deltas.sort(key=lambda x: abs(x[3]), reverse=True)
    return deltas


def cluster_addresses(
    deltas: List[Tuple[int, int, int, int]], gap: int = 32
) -> List[List[Tuple[int, int, int, int]]]:
    """Group adjacent divergent addresses into likely function boundaries."""
    if not deltas:
        return []

    by_addr = sorted(deltas, key=lambda x: x[0])
    clusters: List[List[Tuple[int, int, int, int]]] = []
    current = [by_addr[0]]

    for entry in by_addr[1:]:
        if entry[0] - current[-1][0] <= gap:
            current.append(entry)
        else:
            clusters.append(current)
            current = [entry]
    clusters.append(current)

    clusters.sort(
        key=lambda c: sum(abs(d[3]) for d in c), reverse=True
    )
    return clusters


# --- Entry point ---


def cmd_callgrind_diff(args) -> None:
    """Entry point for callgrind-diff subcommand."""
    source_a = Path(args.source_a).resolve()
    source_b = Path(args.source_b).resolve()

    if not source_a.exists():
        print(f"Error: {source_a} not found", file=sys.stderr)
        sys.exit(1)
    if not source_b.exists():
        print(f"Error: {source_b} not found", file=sys.stderr)
        sys.exit(1)

    use_perf = getattr(args, "perf", False)

    if use_perf:
        if not _check_perf():
            print("Error: perf not found. Install linux-tools for your kernel.", file=sys.stderr)
            sys.exit(1)
        backend_name = "perf"
    else:
        if not _check_valgrind():
            print("Error: valgrind not found. Install valgrind.", file=sys.stderr)
            print("Tip: use --perf to fall back to perf-based sampling.", file=sys.stderr)
            sys.exit(1)
        backend_name = "callgrind"

    invoker = CompilerInvoker()

    if args.output_dir:
        output_dir = Path(args.output_dir).resolve()
    else:
        output_dir = Path(tempfile.mkdtemp(prefix="callgrind_diff_"))

    output_dir.mkdir(parents=True, exist_ok=True)

    if use_perf:
        data_a = output_dir / f"perf.{source_a.stem}.data"
        data_b = output_dir / f"perf.{source_b.stem}.data"

        print(f"Profiling {source_a.name} under perf...")
        result_a = run_perf_record(source_a, data_a, invoker)
        if result_a.returncode != 0:
            print(f"Warning: perf returned {result_a.returncode}", file=sys.stderr)
            if result_a.stderr:
                for line in result_a.stderr.splitlines()[-5:]:
                    print(f"  {line}", file=sys.stderr)

        print(f"Profiling {source_b.name} under perf...")
        result_b = run_perf_record(source_b, data_b, invoker)
        if result_b.returncode != 0:
            print(f"Warning: perf returned {result_b.returncode}", file=sys.stderr)

        if not data_a.exists() or not data_b.exists():
            print("Error: perf data files not generated", file=sys.stderr)
            sys.exit(1)

        print(f"\nParsing perf profiles...")
        counts_a = parse_perf_report(data_a)
        counts_b = parse_perf_report(data_b)
    else:
        data_a = output_dir / f"callgrind.{source_a.stem}.out"
        data_b = output_dir / f"callgrind.{source_b.stem}.out"

        print(f"Profiling {source_a.name} under callgrind...")
        result_a = run_callgrind(source_a, data_a, invoker)
        if result_a.returncode != 0:
            # Valgrind returns non-zero if the child process fails, but
            # callgrind output may still be generated
            if not data_a.exists():
                print(f"Error: callgrind failed for {source_a.name}", file=sys.stderr)
                if result_a.stderr:
                    for line in result_a.stderr.splitlines()[-5:]:
                        print(f"  {line}", file=sys.stderr)
                sys.exit(1)

        print(f"Profiling {source_b.name} under callgrind...")
        result_b = run_callgrind(source_b, data_b, invoker)
        if result_b.returncode != 0:
            if not data_b.exists():
                print(f"Error: callgrind failed for {source_b.name}", file=sys.stderr)
                if result_b.stderr:
                    for line in result_b.stderr.splitlines()[-5:]:
                        print(f"  {line}", file=sys.stderr)
                sys.exit(1)

        print(f"\nParsing callgrind profiles...")
        counts_a = parse_callgrind_output(data_a)
        counts_b = parse_callgrind_output(data_b)

    print(f"  A: {len(counts_a)} c2.dll addresses with {'counts' if not use_perf else 'samples'}")
    print(f"  B: {len(counts_b)} c2.dll addresses with {'counts' if not use_perf else 'samples'}")

    deltas = compute_delta(counts_a, counts_b)
    print(f"  Divergent addresses: {len(deltas)}")

    if not deltas:
        print(f"\nNo divergence detected in c2.dll — sources may compile identically")
        if use_perf:
            print("(perf sampling may miss short-running differences; try callgrind for complete traces)")
        return

    # Cluster analysis
    clusters = cluster_addresses(deltas)
    print(f"  Clusters: {len(clusters)}")
    print()

    # Show top clusters
    print(f"Top divergent c2.dll regions ({backend_name}):")
    print(f"{'Cluster':<10} {'Addr Range':<30} {'Addrs':<8} {'Total |Δ|':<12}")
    print("-" * 60)
    for i, cluster in enumerate(clusters[:20]):
        addr_lo = cluster[0][0]
        addr_hi = cluster[-1][0]
        total_delta = sum(abs(d[3]) for d in cluster)
        rva_lo = addr_lo - C2_IMAGE_BASE
        rva_hi = addr_hi - C2_IMAGE_BASE
        print(
            f"  #{i:<7} 0x{rva_lo:08x}-0x{rva_hi:08x}  {len(cluster):<8} {total_delta:<12}"
        )

    # Show top individual addresses for callgrind (deterministic counts are meaningful)
    if not use_perf and len(deltas) > 0:
        print()
        print("Top 20 divergent addresses (by |delta|):")
        print(f"  {'Address':<14} {'RVA':<14} {'Count A':<12} {'Count B':<12} {'Delta':<12}")
        print("  " + "-" * 60)
        for addr, ca, cb, delta in deltas[:20]:
            rva = addr - C2_IMAGE_BASE
            print(f"  0x{addr:08x}   0x{rva:06x}     {ca:<12} {cb:<12} {delta:<+12}")

    # Update funcmap
    funcmap_path = Path(args.funcmap) if args.funcmap else DEFAULT_FUNCMAP_PATH
    funcmap = C2FuncMap(funcmap_path)
    evidence_tag = f"{source_a.stem}_vs_{source_b.stem}"

    for addr, ca, cb, delta in deltas:
        rva = addr - C2_IMAGE_BASE
        funcmap.add_observation(rva, evidence_tag, delta)

    funcmap.save()
    print(f"\nUpdated {funcmap_path}: {funcmap.summary()}")
    print(f"Output: {data_a}, {data_b}")
