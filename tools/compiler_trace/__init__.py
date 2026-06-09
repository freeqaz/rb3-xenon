"""Compiler instrumentation tooling for analyzing c2.dll register allocation.

Usage:
    python -m tools.compiler_trace diff-asm test_a.cpp test_b.cpp
    python -m tools.compiler_trace capture-il test_a.cpp --output-dir /tmp/claude/il_a
    python -m tools.compiler_trace callgrind-diff test_a.cpp test_b.cpp
    python -m tools.compiler_trace callgrind-diff test_a.cpp test_b.cpp --perf
    python -m tools.compiler_trace annotate --top 10
    python -m tools.compiler_trace annotate --address 0x1234 --callgrind path/to/callgrind.out
    python -m tools.compiler_trace rr-record test_a.cpp --trace-dir /tmp/claude/rr_a
    python -m tools.compiler_trace gdb-attach test_a.cpp --print-only
    python -m tools.compiler_trace bsf-trace source.cpp
    python -m tools.compiler_trace bsf-diff source_a.cpp source_b.cpp
    python -m tools.compiler_trace bsf-solve --symbol <mangled> --source source.cpp
"""
