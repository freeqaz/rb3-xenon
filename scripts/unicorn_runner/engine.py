"""Unicorn PPC32 execution engine for function comparison."""

import ctypes
import struct
import sys
import os
from pathlib import Path

try:
    from . import _trampoline_hook
    _HAS_C_HOOK = True
except ImportError:
    _HAS_C_HOOK = False

# Locate the local Unicorn checkout and put its bindings AHEAD of this repo's
# own scripts/unicorn package on sys.path. See unicorn_dep.py for why counting
# parent directories here was wrong (it broke in every git worktree, and the
# unconditional LIBUNICORN_PATH assignment overwrote a correct value).
from .unicorn_dep import ensure_unicorn_on_path, require as _require_unicorn

_UNICORN_DIR = ensure_unicorn_on_path()
UNICORN_PATH = str(_UNICORN_DIR / "bindings" / "python") if _UNICORN_DIR else ""
_require_unicorn()

from unicorn import Uc, UC_ARCH_PPC, UC_MODE_PPC32, UC_MODE_BIG_ENDIAN
from unicorn import UC_HOOK_BLOCK, UC_HOOK_CODE, UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE_UNMAPPED
from unicorn import UC_MEM_READ_UNMAPPED, UC_MEM_WRITE_UNMAPPED, UC_MEM_FETCH_UNMAPPED
from unicorn import UcError, UC_ERR_FETCH_UNMAPPED
from unicorn.ppc_const import *
from unicorn.unicorn_py3.unicorn import uclib

from .memory_map import (
    STACK_BASE, OBJECT_BASE, GLOBAL_BASE, TRAMPOLINE_BASE, CODE_BASE,
    RDATA_BASE, VTABLE_BASE, SENTINEL_ADDR, REGION_SIZE, STACK_INIT,
    MSR_FP_BIT, TRAMPOLINE_STUB, VTABLE_SLOTS, VTABLE_TRAMP_OFFSET,
)


class ExecutionResult:
    """Result of executing a function in Unicorn.

    terminated_normally: True iff the function returned via blr to the
        SENTINEL_ADDR (the controlled return path). False if it ran into
        any other exit condition (cap exhaustion, wild jump, error).
    cap_exhausted: True iff emu_start returned because the max_insns
        count was hit (PC neither at SENTINEL_ADDR nor at the post-func
        terminator and no UcError was raised).
    final_pc: PC value at emu_start return. SENTINEL_ADDR (0xDEAD0000)
        for normal returns; the in-function address for cap exhaustion;
        the faulting address for unmapped fetches.
    unmapped_log: list of (kind, page_base, addr) tuples for unmapped
        accesses in suspicious regions (page 0, kernel range). Used by
        the comparator to fingerprint null-deref-style divergences that
        would otherwise be hidden by on-demand page mapping.
    """

    def __init__(self, call_log, r3, f1, object_memory, globals_memory,
                 error=None, terminated_normally=False, cap_exhausted=False,
                 final_pc=0, unmapped_log=None):
        self.call_log = call_log
        self.r3 = r3
        self.f1 = f1
        self.object_memory = object_memory
        self.globals_memory = globals_memory
        self.error = error
        self.terminated_normally = terminated_normally
        self.cap_exhausted = cap_exhausted
        self.final_pc = final_pc
        self.unmapped_log = unmapped_log or []


# Call log tuple indices (flat tuple instead of nested dicts for speed).
# Defined in call_log.py so that pure-logic consumers (comparator.py) can read a
# call log without importing the emulator; re-exported here for existing callers.
from .call_log import (  # noqa: E402,F401
    CL_INDEX, CL_TRAMP_ADDR, CL_SRC_OFFSET, CL_R3, CL_R4, CL_R5, CL_R6,
)


# Pre-computed static vtable data (same for every execution)
def _build_vtable_data():
    data = bytearray(VTABLE_SLOTS * 4)
    for slot in range(VTABLE_SLOTS):
        tramp_addr = TRAMPOLINE_BASE + VTABLE_TRAMP_OFFSET + (slot * 8)
        struct.pack_into(">I", data, slot * 4, tramp_addr)
    return bytes(data)

def _build_vtable_tramp_data():
    data = bytearray(VTABLE_SLOTS * 8)
    for slot in range(VTABLE_SLOTS):
        data[slot * 8 : slot * 8 + 8] = TRAMPOLINE_STUB
    return bytes(data)

_VTABLE_DATA = _build_vtable_data()
_VTABLE_TRAMP_DATA = _build_vtable_tramp_data()
_VTABLE_PTR = struct.pack(">I", VTABLE_BASE)
_ZERO_REGION = bytes(REGION_SIZE)
_ZERO_PAGE = bytes(0x1000)

_DATA_REGIONS = (STACK_BASE, OBJECT_BASE, GLOBAL_BASE, VTABLE_BASE)
_ALL_REGIONS = (STACK_BASE, OBJECT_BASE, GLOBAL_BASE,
                TRAMPOLINE_BASE, CODE_BASE, VTABLE_BASE)


class UnicornEngine:
    """Reusable Unicorn PPC32 BE engine for batch function execution.

    Creates the Uc() instance and memory mappings once, then resets state
    between executions. Avoids the ~50ms Uc() teardown cost per function.
    """

    def __init__(self):
        self._mu = Uc(UC_ARCH_PPC, UC_MODE_PPC32 + UC_MODE_BIG_ENDIAN)
        for base in _ALL_REGIONS:
            self._mu.mem_map(base, REGION_SIZE)

        self._rdata_mapped = False
        self._ondemand_pages = set()
        # Phase 3.1: pages in suspicious ranges (null/kernel) that we
        # had to map on demand. We unmap them between executions so
        # _on_unmapped fires every time, not just on the first encounter.
        self._suspicious_pages = set()

        # Fill buffer cache: fill_pattern -> bytes(REGION_SIZE)
        self._fill_cache = {}
        self._page_fill_cache = {}

        # Pre-computed constant for hot path
        self._code_base_plus4 = CODE_BASE + 4

        # Pre-allocated ctypes buffers for direct uc_reg_read_batch calls.
        # Bypasses the Python reg_read() wrapper entirely — one FFI call
        # reads all 5 registers (LR, r3-r6) with zero per-call allocation.
        self._batch_regs = (ctypes.c_int * 5)(
            UC_PPC_REG_LR, UC_PPC_REG_3, UC_PPC_REG_4,
            UC_PPC_REG_5, UC_PPC_REG_6)
        self._batch_vals = [ctypes.c_int() for _ in range(5)]
        self._batch_ptrs = (ctypes.c_void_p * 5)(
            *(ctypes.c_void_p(ctypes.addressof(v))
              for v in self._batch_vals))
        self._batch_count = ctypes.c_int(5)
        self._uch = self._mu._uch  # C engine handle for direct FFI

        # Mutable execution state referenced by hooks
        self._call_log = []
        self._verbose = False
        self._fill_page = None
        # Phase 3.1: suspicious unmapped accesses. Stack-page accesses
        # legitimately differ across decomp/orig due to register
        # allocation, so we scope to unambiguous-bug ranges only.
        self._unmapped_log = []

        # Set up hooks once.
        # UC_HOOK_BLOCK fires once per translation block (at the block start)
        # instead of per instruction. Trampoline stubs are 2-insn blocks
        # (li r3,0; blr), so this fires once at the aligned start address —
        # halving callback count and eliminating all non-aligned fast-returns.
        self._use_c_hook = _HAS_C_HOOK
        if self._use_c_hook:
            _trampoline_hook.install_hook(
                self._mu._uch.value,
                TRAMPOLINE_BASE,
                TRAMPOLINE_BASE + REGION_SIZE - 1,
                CODE_BASE)
        else:
            self._mu.hook_add(
                UC_HOOK_BLOCK, self._on_trampoline,
                begin=TRAMPOLINE_BASE, end=TRAMPOLINE_BASE + REGION_SIZE - 1)
        self._mu.hook_add(
            UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED,
            self._on_unmapped)

    def _on_trampoline(self, uc, address, size, user_data):
        if address & 7:
            return

        # Direct C call: read LR, r3-r6 in one FFI round-trip
        # using pre-allocated ctypes buffers (no per-call allocation)
        uclib.uc_reg_read_batch(
            self._uch, self._batch_regs, self._batch_ptrs, self._batch_count)
        vals = self._batch_vals
        entry = (
            len(self._call_log),
            address,
            vals[0].value - self._code_base_plus4,
            vals[1].value,
            vals[2].value,
            vals[3].value,
            vals[4].value,
        )
        self._call_log.append(entry)

        if self._verbose:
            print(f"  Call #{entry[CL_INDEX]}: "
                  f"tramp=0x{entry[CL_TRAMP_ADDR]:08X} "
                  f"r3=0x{entry[CL_R3]:08X} "
                  f"r4=0x{entry[CL_R4]:08X} "
                  f"r5=0x{entry[CL_R5]:08X} "
                  f"r6=0x{entry[CL_R6]:08X} "
                  f"src_off=0x{entry[CL_SRC_OFFSET]:X}")

    def _on_unmapped(self, uc, access, address, size, value, user_data):
        page_base = address & ~0xFFF

        # Phase 3.1: log suspicious unmapped accesses for the comparator
        # to fingerprint. Scope to clear-bug pages only:
        #   - low addresses (page 0, near-null deref): address < 0x1000
        #   - kernel/sentinel range: address >= 0xF0000000
        # Stack-page (~0x10000000) and tramp-region accesses legitimately
        # differ between decomp and orig; we ignore them.
        #
        # Note: we fire EVERY time the suspicious access happens, even
        # for pages that are already on-demand mapped (so we don't lose
        # signal due to engine reuse warming up page 0). The fingerprint
        # in the comparator dedupes by (kind, page_base) anyway.
        if address < 0x1000 or address >= 0xF0000000:
            if access == UC_MEM_READ_UNMAPPED:
                kind = "rd"
            elif access == UC_MEM_WRITE_UNMAPPED:
                kind = "wr"
            elif access == UC_MEM_FETCH_UNMAPPED:
                kind = "fetch"
            else:
                kind = f"k{access}"
            self._unmapped_log.append((kind, page_base, address & 0xFFFFFFFC))

        if page_base not in self._ondemand_pages:
            try:
                uc.mem_map(page_base, 0x1000)
                if self._fill_page is not None:
                    uc.mem_write(page_base, self._fill_page)
                self._ondemand_pages.add(page_base)
                if address < 0x1000 or address >= 0xF0000000:
                    self._suspicious_pages.add(page_base)
            except Exception:
                # Surface this — silently swallowing harness errors is bad.
                # We log the fact of the failure so the operator notices.
                self._unmapped_log.append(("map_failed", page_base, address))
                return False
        return True

    def _get_fill_region(self, fill_pattern):
        if fill_pattern is None:
            return _ZERO_REGION
        key = fill_pattern & 0xFF
        if key not in self._fill_cache:
            self._fill_cache[key] = bytes([key]) * REGION_SIZE
        return self._fill_cache[key]

    def _get_fill_page(self, fill_pattern):
        if fill_pattern is None:
            return _ZERO_PAGE
        key = fill_pattern & 0xFF
        if key not in self._page_fill_cache:
            self._page_fill_cache[key] = bytes([key]) * 0x1000
        return self._page_fill_cache[key]

    def execute(self, patched_code, trampolines, func_size, timeout=5_000_000,
                verbose=False, rdata_bytes=None, fill_pattern=None,
                max_insns=50_000, object_memory=None, arg_registers=None):
        """Execute a patched function, resetting state from any previous run.

        Same interface as the standalone execute_function().
        max_insns caps instruction count to prevent runaway loops from
        dominating batch time via expensive Python hook callbacks.
        arg_registers: optional dict mapping register IDs to values (e.g., {UC_PPC_REG_4: 1})
        """
        mu = self._mu

        # Reset execution context
        if self._use_c_hook:
            _trampoline_hook.clear_log()
        else:
            self._call_log = []
        self._verbose = verbose
        self._fill_page = self._get_fill_page(fill_pattern) if fill_pattern is not None else None
        self._unmapped_log = []

        # Phase 3.1: unmap any suspicious-range pages from previous runs
        # so the unmapped hook fires this run too. Non-suspicious
        # on-demand pages (e.g. extended data sections) stay mapped to
        # keep on-demand mapping cheap for normal cases.
        for page in self._suspicious_pages:
            try:
                mu.mem_unmap(page, 0x1000)
            except Exception:
                # Race with concurrent harness use; leave it mapped and
                # accept the miss for this one run.
                pass
            self._ondemand_pages.discard(page)
        self._suspicious_pages = set()

        # Reset memory regions
        fill_buf = self._get_fill_region(fill_pattern)
        for base in _DATA_REGIONS:
            mu.mem_write(base, fill_buf)
        mu.mem_write(CODE_BASE, _ZERO_REGION)
        mu.mem_write(TRAMPOLINE_BASE, _ZERO_REGION)

        # Reset on-demand mapped pages
        page_fill = self._get_fill_page(fill_pattern)
        for page in self._ondemand_pages:
            mu.mem_write(page, page_fill)

        # RDATA region
        if rdata_bytes is not None:
            if not self._rdata_mapped:
                mu.mem_map(RDATA_BASE, REGION_SIZE)
                self._rdata_mapped = True
            else:
                mu.mem_write(RDATA_BASE, _ZERO_REGION)
            mu.mem_write(RDATA_BASE, rdata_bytes)
        elif self._rdata_mapped:
            mu.mem_write(RDATA_BASE, _ZERO_REGION)

        # Load function code
        mu.mem_write(CODE_BASE, bytes(patched_code))

        # Write trampoline stubs
        for addr in trampolines.values():
            mu.mem_write(addr, TRAMPOLINE_STUB)

        # Override object region with typed memory if provided
        if object_memory is not None:
            mu.mem_write(OBJECT_BASE, bytes(object_memory[:REGION_SIZE]))

        # Write vtable data
        mu.mem_write(VTABLE_BASE, _VTABLE_DATA)
        mu.mem_write(TRAMPOLINE_BASE + VTABLE_TRAMP_OFFSET, _VTABLE_TRAMP_DATA)
        mu.mem_write(OBJECT_BASE, _VTABLE_PTR)

        # Reset all GPRs and FPRs to zero
        for i in range(32):
            mu.reg_write(UC_PPC_REG_0 + i, 0)
            mu.reg_write(UC_PPC_REG_FPR0 + i, 0)
        mu.reg_write(UC_PPC_REG_CR, 0)
        mu.reg_write(UC_PPC_REG_XER, 0)
        mu.reg_write(UC_PPC_REG_CTR, 0)

        # Set up registers
        mu.reg_write(UC_PPC_REG_1, STACK_INIT)
        mu.reg_write(UC_PPC_REG_2, 0)
        mu.reg_write(UC_PPC_REG_3, OBJECT_BASE)
        mu.reg_write(UC_PPC_REG_LR, SENTINEL_ADDR)
        msr = mu.reg_read(UC_PPC_REG_MSR)
        mu.reg_write(UC_PPC_REG_MSR, msr | MSR_FP_BIT)

        # Set argument registers if provided
        if arg_registers:
            for reg_id, val in arg_registers.items():
                mu.reg_write(reg_id, val)

        # Execute (count= caps instructions to prevent runaway loops)
        error = None
        terminated_normally = False
        cap_exhausted = False
        try:
            mu.emu_start(CODE_BASE, CODE_BASE + func_size,
                         timeout=timeout, count=max_insns)
            # emu_start returned without raising. Two ways this happens:
            # 1. We reached the function's end address (CODE_BASE+func_size).
            #    The function ran off the end without a blr — unusual but
            #    occasionally happens with tail calls; treat as normal.
            # 2. We hit the instruction count cap. PC will be somewhere
            #    inside the function. This is the cap_exhausted case —
            #    the function did not actually finish, so we can't trust
            #    the captured state.
            pc_after = mu.reg_read(UC_PPC_REG_PC)
            if CODE_BASE <= pc_after < CODE_BASE + func_size:
                cap_exhausted = True
            else:
                # PC at or past the function-end terminator.
                terminated_normally = True
        except UcError as e:
            if e.errno == UC_ERR_FETCH_UNMAPPED:
                pc = mu.reg_read(UC_PPC_REG_PC)
                if pc == SENTINEL_ADDR:
                    terminated_normally = True
                else:
                    error = f"Unexpected fetch from unmapped 0x{pc:08X}"
            else:
                error = str(e)

        # Capture output state
        r3 = mu.reg_read(UC_PPC_REG_3)
        f1 = mu.reg_read(UC_PPC_REG_FPR0 + 1)
        final_pc = mu.reg_read(UC_PPC_REG_PC)
        object_memory = bytes(mu.mem_read(OBJECT_BASE, REGION_SIZE))
        globals_memory = bytes(mu.mem_read(GLOBAL_BASE, REGION_SIZE))

        if self._use_c_hook:
            call_log = _trampoline_hook.get_log()
            if verbose and call_log:
                for entry in call_log:
                    print(f"  Call #{entry[CL_INDEX]}: "
                          f"tramp=0x{entry[CL_TRAMP_ADDR]:08X} "
                          f"r3=0x{entry[CL_R3]:08X} "
                          f"r4=0x{entry[CL_R4]:08X} "
                          f"r5=0x{entry[CL_R5]:08X} "
                          f"r6=0x{entry[CL_R6]:08X} "
                          f"src_off=0x{entry[CL_SRC_OFFSET]:X}")
        else:
            call_log = list(self._call_log)

        return ExecutionResult(
            call_log=call_log,
            r3=r3,
            f1=f1,
            object_memory=object_memory,
            globals_memory=globals_memory,
            error=error,
            terminated_normally=terminated_normally,
            cap_exhausted=cap_exhausted,
            final_pc=final_pc,
            unmapped_log=list(self._unmapped_log),
        )


def execute_function(patched_code, trampolines, func_size, timeout=5_000_000,
                     verbose=False, rdata_bytes=None, fill_pattern=None,
                     max_insns=50_000, object_memory=None, arg_registers=None):
    """Execute a patched function in Unicorn and return the result.

    Standalone version — creates a fresh engine each call.
    For batch use, prefer UnicornEngine for reuse across functions.
    """
    engine = UnicornEngine()
    return engine.execute(patched_code, trampolines, func_size,
                          timeout=timeout, verbose=verbose,
                          rdata_bytes=rdata_bytes, fill_pattern=fill_pattern,
                          max_insns=max_insns, object_memory=object_memory,
                          arg_registers=arg_registers)
