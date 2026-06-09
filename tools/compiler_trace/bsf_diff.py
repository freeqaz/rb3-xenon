"""BSF trace comparison — find divergent register allocation decisions.

Compares two BSF traces (from different source variants) to identify
exactly which register color assignments differ. Divergent calls indicate
allocation decisions sensitive to declaration order.

Usage:
    from tools.compiler_trace.bsf_diff import diff_bsf_traces
    divs = diff_bsf_traces(trace_a, trace_b)
    for d in divs:
        print(f"Call #{d.call_index}: bit {d.bit_a} vs {d.bit_b}")
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from .bsf_trace import BSFTrace, BSFCall, trace_bsf

# Known c2.dll caller RVAs and their phases
CALLER_PHASES = {
    0x027242: "initial_coloring",  # First pass: initial register assignment
    0x026B5E: "coalescing",  # Register coalescing phase
    0x0272E8: "recoloring",  # Recoloring / spill resolution
}


@dataclass
class BSFDivergence:
    """A divergent BSF call between two compilation traces."""

    call_index: int  # 1-based call index
    caller_rva: int  # c2.dll caller RVA
    bit_a: int  # Color chosen in trace A
    bit_b: int  # Color chosen in trace B
    lo_a: int  # Availability mask lo (trace A)
    lo_b: int  # Availability mask lo (trace B)
    hi_a: int  # Availability mask hi (trace A)
    hi_b: int  # Availability mask hi (trace B)
    base_a: int  # Register class base (trace A)
    base_b: int  # Register class base (trace B)

    @property
    def phase(self) -> str:
        return CALLER_PHASES.get(self.caller_rva, f"unknown_0x{self.caller_rva:06x}")

    @property
    def mask_differs(self) -> bool:
        """Whether the availability masks differ (not just the result)."""
        return self.lo_a != self.lo_b or self.hi_a != self.hi_b


@dataclass
class BSFDiffResult:
    """Result of comparing two BSF traces."""

    trace_a: BSFTrace
    trace_b: BSFTrace
    divergences: list[BSFDivergence]
    total_calls_a: int
    total_calls_b: int

    @property
    def call_count_matches(self) -> bool:
        return self.total_calls_a == self.total_calls_b

    @property
    def first_divergence(self) -> Optional[BSFDivergence]:
        return self.divergences[0] if self.divergences else None

    def divergences_by_phase(self) -> dict[str, list[BSFDivergence]]:
        """Group divergences by compiler phase."""
        groups: dict[str, list[BSFDivergence]] = {}
        for d in self.divergences:
            groups.setdefault(d.phase, []).append(d)
        return groups

    def color_map_a(self) -> dict[int, int]:
        """Map call_index -> color for trace A (divergent calls only)."""
        return {d.call_index: d.bit_a for d in self.divergences}

    def color_map_b(self) -> dict[int, int]:
        """Map call_index -> color for trace B (divergent calls only)."""
        return {d.call_index: d.bit_b for d in self.divergences}


def diff_bsf_traces(trace_a: BSFTrace, trace_b: BSFTrace) -> BSFDiffResult:
    """Find divergent BSF calls between two compilation traces.

    Aligns traces by call index and compares the BSF result (bit).
    Both traces should have the same number of calls if they come
    from source variants with the same structure.
    """
    divergences: list[BSFDivergence] = []

    # Compare up to the shorter trace length
    min_len = min(len(trace_a.calls), len(trace_b.calls))

    for i in range(min_len):
        a = trace_a.calls[i]
        b = trace_b.calls[i]

        if a.bit != b.bit or a.lo != b.lo or a.hi != b.hi:
            divergences.append(
                BSFDivergence(
                    call_index=a.index,
                    caller_rva=a.caller_rva,
                    bit_a=a.bit,
                    bit_b=b.bit,
                    lo_a=a.lo,
                    lo_b=b.lo,
                    hi_a=a.hi,
                    hi_b=b.hi,
                    base_a=a.base,
                    base_b=b.base,
                )
            )

    return BSFDiffResult(
        trace_a=trace_a,
        trace_b=trace_b,
        divergences=divergences,
        total_calls_a=len(trace_a.calls),
        total_calls_b=len(trace_b.calls),
    )


def cmd_bsf_diff(args) -> None:
    """Entry point for bsf-diff subcommand."""
    import sys
    from pathlib import Path

    source_a = Path(args.source_a).resolve()
    source_b = Path(args.source_b).resolve()

    print(f"Tracing BSF calls for {source_a.name}...", file=sys.stderr)
    trace_a = trace_bsf(source_a)
    print(f"  {trace_a.total_calls} calls", file=sys.stderr)

    print(f"Tracing BSF calls for {source_b.name}...", file=sys.stderr)
    trace_b = trace_bsf(source_b)
    print(f"  {trace_b.total_calls} calls", file=sys.stderr)

    result = diff_bsf_traces(trace_a, trace_b)

    if not result.call_count_matches:
        print(
            f"WARNING: Call count mismatch: {result.total_calls_a} vs {result.total_calls_b}",
            file=sys.stderr,
        )

    print(f"\nDivergent BSF calls: {len(result.divergences)}")

    if result.divergences:
        by_phase = result.divergences_by_phase()
        for phase, divs in sorted(by_phase.items()):
            print(f"\n  Phase: {phase} ({len(divs)} divergences)")
            for d in divs:
                mask_note = " [mask differs]" if d.mask_differs else ""
                print(
                    f"    #{d.call_index}: bit {d.bit_a} -> {d.bit_b}{mask_note}"
                )

        # Show the first divergence in detail
        first = result.first_divergence
        if first:
            print(f"\nFirst divergence at call #{first.call_index}:")
            print(f"  Caller RVA: 0x{first.caller_rva:06x} ({first.phase})")
            print(f"  Trace A: lo=0x{first.lo_a:08x} hi=0x{first.hi_a:08x} -> bit={first.bit_a}")
            print(f"  Trace B: lo=0x{first.lo_b:08x} hi=0x{first.hi_b:08x} -> bit={first.bit_b}")
    else:
        print("  Traces are identical — no register allocation differences.")
