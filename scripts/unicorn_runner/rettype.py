"""Return-type awareness for the unicorn comparator.

Why this exists
---------------
`compare()` used to diff `r3` unconditionally and call any difference a
`return_value_mismatch`, which `classify_divergence()` then labelled
`return_value` -- documented as "integer return value mismatch (real bug)".

`r3` is only the return value if the function returns something that lives in
`r3`. On the Xenon PPC ABI:

  * a **void** function's `r3` is a scratch register. Whatever is in it is
    leftover from the last thing that touched it, and it differs between two
    correct compilations for free.
  * a **float/double** function returns in **`f1`**. Its `r3` is scratch too --
    and worse, the `r3` check ran *before* the `f1` check, so a genuine
    floating-point return divergence was reported as `return_value` and the
    `fpr_return_mismatch` branch below it never ran.

Measured 2026-08-19 over the 10 rows carrying `unicorn_class='return_value'`:
**4 return void and 1 returns float** -- and those 5 include 4 of the 6 rows
below 100%, i.e. most of the population where the class could have mattered.

What we do about it
-------------------
Consult the return type, which the MSVC mangled name already encodes, and
compare the register the ABI actually uses. A difference in the *other*
register is still surfaced -- as `scratch_return_reg`, an artifact class -- so
the oracle never gets quieter, only more accurate. Verdicts stay DIVERGENT.

`llvm-undname` is the demangler (it ships with LLVM and is already on this box;
`bin/` also carries objdiff's). If it is missing we return None, which restores
exactly the old behaviour rather than guessing -- with one warning to stderr, so
"unknown" is never silent.
"""

import shutil
import subprocess
import sys

__all__ = ["return_type_class", "returns_in_r3", "returns_in_f1"]


_CALLING_CONVENTIONS = (
    "__cdecl", "__thiscall", "__stdcall", "__fastcall",
    "__pascal", "__clrcall", "__vectorcall",
)

_ACCESS_PREFIXES = (
    "public:", "private:", "protected:", "virtual", "static",
    "[thunk]:", "extern", '"C"',
)

_FLOAT_TYPES = {"float", "double", "long double"}

_cache: dict[str, str | None] = {}
_warned = False


def _undname(symbol: str) -> str | None:
    """Demangle one MSVC symbol, or None if it is not a mangled name."""
    global _warned
    exe = shutil.which("llvm-undname")
    if exe is None:
        if not _warned:
            _warned = True
            print("[rettype] llvm-undname not found; return-type awareness is "
                  "OFF and r3 will be compared for every function, including "
                  "void and float returns. Install LLVM to re-enable.",
                  file=sys.stderr)
        return None
    try:
        proc = subprocess.run([exe, symbol], capture_output=True, text=True,
                              timeout=20)
    except (OSError, subprocess.SubprocessError):
        return None
    # llvm-undname echoes the input, then the demangling (or an error line).
    lines = [ln for ln in proc.stdout.splitlines() if ln.strip()]
    if len(lines) < 2:
        return None
    demangled = lines[1].strip()
    if demangled.startswith("error:") or demangled == symbol:
        return None
    return demangled


def _classify_demangled(demangled: str) -> str | None:
    """Map a demangled MSVC signature to 'void' / 'float' / 'int' / None.

    None means "no opinion" -- constructors, destructors and anything whose
    shape we do not recognise. Callers must treat None as "behave as before".
    """
    head = None
    for cc in _CALLING_CONVENTIONS:
        idx = demangled.find(cc + " ")
        if idx != -1:
            head = demangled[:idx]
            break
    if head is None:
        return None                      # no calling convention -> not a function

    for token in _ACCESS_PREFIXES:
        head = head.replace(token, " ")
    ret = " ".join(head.split())

    if not ret:
        # Constructors and destructors carry no return type in the mangling.
        # MSVC still returns `this` in r3 from a constructor, so r3 IS
        # meaningful there -- say nothing and let the old comparison stand.
        return None
    if ret == "void":
        return "void"
    if ret in _FLOAT_TYPES:
        return "float"
    return "int"                         # integers, pointers, refs, sret classes


def return_type_class(symbol: str, demangled: str | None = None) -> str | None:
    """'void' | 'float' | 'int', or None when the return type is unknown.

    Pass `demangled` if you already have it (e.g. from decomp.db) to skip the
    subprocess. Results are memoised per process.
    """
    if demangled is not None:
        return _classify_demangled(demangled)
    if symbol in _cache:
        return _cache[symbol]
    text = _undname(symbol)
    result = _classify_demangled(text) if text else None
    _cache[symbol] = result
    return result


def returns_in_r3(symbol: str, demangled: str | None = None) -> bool:
    """False only when we KNOW r3 is not the ABI return register."""
    return return_type_class(symbol, demangled) not in ("void", "float")


def returns_in_f1(symbol: str, demangled: str | None = None) -> bool:
    """False only when we KNOW f1 is not the ABI return register."""
    return return_type_class(symbol, demangled) in ("float", None)
