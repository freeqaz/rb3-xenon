"""Address space constants for the Unicorn Function Runner."""

# Region bases and sizes
STACK_BASE      = 0x10000000
OBJECT_BASE     = 0x20000000
GLOBAL_BASE     = 0x30000000
CODE_BASE       = 0x80000000
TRAMPOLINE_BASE = 0x80010000  # Must be within ±32MB of CODE_BASE for REL24
RDATA_BASE      = 0x80020000  # Switch table / jump table data
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
