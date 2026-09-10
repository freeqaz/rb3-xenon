"""Address space constants for the Unicorn Function Runner."""

# Region bases and sizes
STACK_BASE      = 0x10000000
OBJECT_BASE     = 0x20000000
GLOBAL_BASE     = 0x30000000
CODE_BASE       = 0x80000000
TRAMPOLINE_BASE = 0x80010000  # Must be within ±32MB of CODE_BASE for REL24
RDATA_BASE      = 0x80020000  # Switch table / jump table data
# Real bodies for MSVC's __savegprlr_N / __restgprlr_N / __savefpr_N /
# __restfpr_N / __savevmx_N / __restvmx_N (see save_helpers.py). Deliberately
# OUTSIDE the trampoline region: the call-log hook covers that range, and a
# prologue helper is not a call the function made.
#
# 0x80040000, not 0x80030000: that would abut RDATA's 64KB window, so an
# oversized rdata buffer would land in executable helper bodies instead of
# faulting on unmapped memory. One unmapped 64KB guard region now sits between
# them, and the engine refuses an oversized buffer outright. Belt and braces:
# a corrupted helper body would present as a decomp bug, not a harness bug.
HELPER_BASE     = 0x80040000  # Within ±32MB of CODE_BASE for REL24
HELPER_SLOT_SIZE = 0x60       # Bytes per entry point; longest body is 0x54
VTABLE_BASE     = 0x40000000  # Mock vtable data region
SENTINEL_ADDR   = 0xDEAD0000
REGION_SIZE     = 0x10000  # 64KB each
FILL_BYTE       = 0xCD    # MSVC debug uninitialized heap fill

# Vtable mock parameters
VTABLE_SLOTS        = 256   # Max virtual methods per class
VTABLE_TRAMP_OFFSET = 0x8000  # Offset in TRAMPOLINE region for vtable stubs

# Initial register values
STACK_INIT = STACK_BASE + 0x8000
MSR_FP_BIT = 0x2000

# Trampoline stub: li r3, 0; blr
TRAMPOLINE_STUB = bytes([0x38, 0x60, 0x00, 0x00, 0x4E, 0x80, 0x00, 0x20])
