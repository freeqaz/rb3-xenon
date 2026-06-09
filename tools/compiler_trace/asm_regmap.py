"""ASM register mapping — parse /FAs listings for variable→register assignments.

Extracts which callee-saved register each local variable gets assigned to
by analyzing the interleaved source+assembly output from cl.exe /FAs.

Unlike BSF tracing (which requires GDB and only works for ~17% of functions
with BSF calls), this works for ALL functions with callee-saved registers.

The /FAs listing format interleaves source comments with assembly:

    ; 42   :     int a = GetValue();
        bl      ?GetValue@@YAHXZ
        mr      r31, r3
    ; 43   :     int b = GetOther();
        bl      ?GetOther@@YAHXZ
        mr      r30, r3

From this we can infer: a→r31, b→r30.

Usage:
    from tools.compiler_trace.asm_regmap import parse_asm_listing
    regmap = parse_asm_listing(asm_lines, "MyFunc")
    if regmap:
        print(regmap.var_to_reg)  # {"a": "r31", "b": "r30"}
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

from .asm_diff import extract_function
from .bsf_trace import (
    _PROC_RE,
    _ENDP_RE,
    _SAVEGPRLR_RE,
    _STMW_RE,
    _INDIVIDUAL_SAVE_RE,
    _parse_function_info,
)


@dataclass
class AsmRegMap:
    """Register mapping extracted from an /FAs assembly listing."""

    var_to_reg: dict[str, str] = field(default_factory=dict)  # {"a": "r31"}
    reg_to_var: dict[str, str] = field(default_factory=dict)  # {"r31": "a"}
    callee_saved_count: int = 0  # Number of callee-saved GPRs used
    # FPR mappings (f14-f31 callee-saved)
    fpr_var_to_reg: dict[str, str] = field(default_factory=dict)  # {"x": "f31"}
    fpr_reg_to_var: dict[str, str] = field(default_factory=dict)  # {"f31": "x"}
    fpr_callee_saved_count: int = 0  # Number of callee-saved FPRs used


# Source comment line: "; 42   :     int a = GetValue();"
_SOURCE_LINE_RE = re.compile(r"^;\s*(\d+)\s*:\s*(.*)$")

# Register move: mr rN, rM (callee-saved = r13-r31)
_MR_RE = re.compile(r"\bmr\s+(r\d+)\s*,\s*(r\d+)")

# Store to callee-saved: addi rN, rM, imm
_ADDI_RE = re.compile(r"\baddi\s+(r\d+)\s*,")

# li rN, imm (loading immediate into callee-saved)
_LI_RE = re.compile(r"\bli\s+(r\d+)\s*,")

# Any instruction writing to a callee-saved GPR (first operand is dest)
# Matches: mr rN, lwz rN, addi rN, li rN, etc.
_DEST_REG_RE = re.compile(
    r"\b(?:mr|addi|li|lwz|lbz|lhz|lha|lfs|lfd|add|subf|mullw|ori|lis|rlwinm|srawi)\s+(r\d+)"
)

# Any instruction writing to a callee-saved FPR (first operand is dest)
# Matches: fmr fN, lfs fN, lfd fN, fadds fN, fsubs fN, fmuls fN, fdivs fN, etc.
_DEST_FPR_RE = re.compile(
    r"\b(?:fmr|lfs|lfd|fadds|fadd|fsubs|fsub|fmuls|fmul|fdivs|fdiv|"
    r"fmadds|fmadd|fmsubs|fmsub|fnmsubs|fnmsub|fneg|fabs|frsp|fctiw|fctiwz|"
    r"fsel|fres|frsqrte|ps_merge00|ps_merge01|ps_merge10|ps_merge11)\s+(f\d+)"
)

# Declaration patterns in source comments
# Matches: "Type varname = ...", "Type varname;", "Type* varname = ...", etc.
# Handles both primitive types and C++ class types (including namespaced/templated).
_DECL_RE = re.compile(
    r"^\s*(?:(?:const|static|volatile|register)\s+)*"
    r"(?:(?:unsigned|signed)\s+)?"
    r"(?:int|float|double|bool|char|short|long|void|auto|size_t|"
    r"[A-Za-z_]\w*(?:::\w+)*(?:\s*<[^>]*>)?(?:\s*[*&])*)\s+"
    r"([a-zA-Z_]\w*)\s*[=;(]"
)

# Also match "Type* varname" or "Type& varname"
_DECL_PTR_RE = re.compile(
    r"^\s*(?:const\s+)?(?:\w+(?:::\w+)*(?:\s*<[^>]*>)?)\s*[*&]+\s*"
    r"([a-zA-Z_]\w*)\s*[=;(]"
)

# Simple variable assignment: "varname = expr;"
_ASSIGN_RE = re.compile(r"^\s*([a-zA-Z_]\w*)\s*=\s*")


def _is_callee_saved_gpr(reg: str) -> bool:
    """Check if a register is a callee-saved GPR (r13-r31)."""
    if not reg.startswith("r"):
        return False
    try:
        num = int(reg[1:])
        return 13 <= num <= 31
    except ValueError:
        return False


def _is_callee_saved_fpr(reg: str) -> bool:
    """Check if a register is a callee-saved FPR (f14-f31)."""
    if not reg.startswith("f"):
        return False
    try:
        num = int(reg[1:])
        return 14 <= num <= 31
    except ValueError:
        return False


# __savefpr_N saves fN through f31
_SAVEFPR_RE = re.compile(r"__savefpr_(\d+)")

# Individual FPR save: stfd fN, -offset(r1)
_INDIVIDUAL_FPR_SAVE_RE = re.compile(r"\bstfd\s+f(\d+)\s*,\s*-?\d+\(r1\)")

# FPR move: fmr fN, fM (volatile→callee-saved, like parameter save for floats)
_FMR_RE = re.compile(r"\bfmr\s+(f\d+)\s*,\s*f(\d+)")


def _count_fpr_saves(func_lines: list[str]) -> int:
    """Count callee-saved FPR registers from function prologue."""
    count = 0
    fpr_regs: set[int] = set()
    in_prologue = True

    for line in func_lines:
        stripped = line.strip()
        if not stripped:
            continue
        # End of prologue
        if ".endprolog" in stripped or (
            stripped.startswith("bl ") and "__savefpr" not in stripped
            and "__savegprlr" not in stripped
        ):
            in_prologue = False
        if not in_prologue:
            break

        # __savefpr_N
        m = _SAVEFPR_RE.search(stripped)
        if m:
            first_saved = int(m.group(1))
            if 14 <= first_saved <= 31:
                count = max(count, 32 - first_saved)
            continue
        # Individual stfd fN, -offset(r1)
        m = _INDIVIDUAL_FPR_SAVE_RE.search(stripped)
        if m:
            reg_num = int(m.group(1))
            if 14 <= reg_num <= 31:
                fpr_regs.add(reg_num)

    return max(count, len(fpr_regs))


def _extract_var_from_source(source_text: str) -> str | None:
    """Extract a variable name from a source comment line.

    Tries declaration patterns first, then assignment patterns.
    Filters out function definitions (lines with ') {' or ') const {').
    """
    # Skip function definitions: "RetType FuncName(params) {"
    stripped = source_text.rstrip()
    if re.search(r"\)\s*(const\s*)?\{?\s*$", stripped) and "(" in stripped:
        # Check if it looks like a function def (has closing paren near end)
        # vs a constructor call "Type var(args);"
        if not stripped.rstrip().endswith(";"):
            return None

    m = _DECL_RE.match(source_text)
    if m:
        return m.group(1)
    m = _DECL_PTR_RE.match(source_text)
    if m:
        return m.group(1)
    return None


def parse_asm_listing(
    asm_lines: list[str], function_name: str
) -> AsmRegMap | None:
    """Parse /FAs listing for a specific function, extracting var→register mapping.

    Strategy:
    1. Extract the function's assembly using PROC/ENDP markers
    2. Get callee-saved count from prologue (savegprlr/stmw/individual saves)
    3. Walk through interleaved source+asm, tracking:
       - Source comments that declare variables
       - First write to each callee-saved register after a source comment
    4. Build mapping from variable names to registers

    Args:
        asm_lines: Lines from a .cod/.asm file produced by cl.exe /FAs.
        function_name: Function name (mangled or substring) to isolate.

    Returns:
        AsmRegMap with var→reg mappings, or None if function not found.
    """
    # Extract function lines
    func_lines = extract_function(asm_lines, function_name)
    if not func_lines:
        return None

    # Get callee-saved count from function info
    func_info = _parse_function_info(func_lines)
    callee_saved_count = 0
    if func_info:
        callee_saved_count = func_info[0][1]

    # Count FPR callee-saved registers
    fpr_callee_saved_count = _count_fpr_saves(func_lines)

    if callee_saved_count == 0 and fpr_callee_saved_count == 0:
        return AsmRegMap(callee_saved_count=0, fpr_callee_saved_count=0)

    # Determine which callee-saved regs are actually used
    callee_saved_regs = _find_callee_saved_regs(func_lines)
    if not callee_saved_regs:
        # Fall back to count-based: assume r31, r30, ..., r(32-count)
        callee_saved_regs = set()
        for i in range(callee_saved_count):
            callee_saved_regs.add(31 - i)

    # Walk source+asm interleaving to build var→reg mapping
    var_to_reg: dict[str, str] = {}
    reg_to_var: dict[str, str] = {}
    assigned_regs: set[str] = set()
    # FPR mappings
    fpr_var_to_reg: dict[str, str] = {}
    fpr_reg_to_var: dict[str, str] = {}
    assigned_fprs: set[str] = set()

    # Track current source context
    current_var: str | None = None
    in_prologue = True
    past_first_endprolog = False
    seen_bl_since_source = False  # Track if a bl (call) preceded current mr

    # Regex for parameter saves: mr rN, r3/r4/r5/... (volatile→callee-saved)
    _PARAM_SAVE_RE = re.compile(r"\bmr\s+(r\d+)\s*,\s*r([3-9]|1[0-2])\b")

    for line in func_lines:
        stripped = line.strip()

        # Skip empty lines and labels
        if not stripped or stripped.startswith("$") or stripped.endswith(":"):
            continue

        # Detect end of prologue
        if in_prologue:
            if "__savegprlr" in stripped or "stmw" in stripped:
                continue
            if _INDIVIDUAL_SAVE_RE.search(stripped):
                continue
            if ".endprolog" in stripped:
                in_prologue = False
                past_first_endprolog = True
                continue
            # First source comment or bl (non-save) ends prologue
            if _SOURCE_LINE_RE.match(stripped):
                in_prologue = False
                past_first_endprolog = True
            elif stripped.startswith("bl ") and "__savegprlr" not in stripped:
                in_prologue = False
                past_first_endprolog = True

        if in_prologue:
            continue

        # Detect unwind/exception handler blocks (second .endprolog or
        # __unwind labels) — stop parsing to avoid false mappings
        if past_first_endprolog and ".endprolog" in stripped:
            break
        if stripped.startswith("__unwind"):
            break

        # Source comment — extract variable name and reset bl tracker
        m = _SOURCE_LINE_RE.match(stripped)
        if m:
            source_text = m.group(2)
            var = _extract_var_from_source(source_text)
            if var:
                current_var = var
            else:
                # Non-declaration source line resets context
                current_var = None
            seen_bl_since_source = False
            continue

        # Track function calls (bl = branch and link)
        if stripped.startswith("bl") and not stripped.startswith("blr"):
            seen_bl_since_source = True

        # Mark parameter saves (mr rN, r3/r4/...) as occupied — but only
        # when NOT preceded by a bl call (which would mean r3 is a return
        # value, not a parameter being saved)
        m = _PARAM_SAVE_RE.search(stripped)
        if m and not seen_bl_since_source:
            dest_reg = m.group(1)
            if _is_callee_saved_gpr(dest_reg) and dest_reg not in assigned_regs:
                assigned_regs.add(dest_reg)
                reg_to_var[dest_reg] = f"__param_{m.group(2)}"

        # Assembly instruction — look for first write to callee-saved GPR
        if current_var and current_var not in var_to_reg:
            m = _DEST_REG_RE.search(stripped)
            if m:
                dest_reg = m.group(1)
                if _is_callee_saved_gpr(dest_reg) and dest_reg not in assigned_regs:
                    var_to_reg[current_var] = dest_reg
                    reg_to_var[dest_reg] = current_var
                    assigned_regs.add(dest_reg)

        # Also look for first write to callee-saved FPR
        if current_var and current_var not in fpr_var_to_reg:
            m = _DEST_FPR_RE.search(stripped)
            if m:
                dest_fpr = m.group(1)
                if _is_callee_saved_fpr(dest_fpr) and dest_fpr not in assigned_fprs:
                    fpr_var_to_reg[current_var] = dest_fpr
                    fpr_reg_to_var[dest_fpr] = current_var
                    assigned_fprs.add(dest_fpr)

        # FPR parameter saves: fmr fN, f1/f2/... (volatile→callee-saved)
        m = _FMR_RE.search(stripped)
        if m and not seen_bl_since_source:
            dest_fpr = m.group(1)
            src_fpr = m.group(2)
            # Volatile FPR params: f1-f13
            try:
                src_num = int(src_fpr[1:])
            except ValueError:
                src_num = 0
            if 1 <= src_num <= 13 and _is_callee_saved_fpr(dest_fpr) and dest_fpr not in assigned_fprs:
                assigned_fprs.add(dest_fpr)
                fpr_reg_to_var[dest_fpr] = f"__fparam_{src_fpr}"

    return AsmRegMap(
        var_to_reg=var_to_reg,
        reg_to_var=reg_to_var,
        callee_saved_count=callee_saved_count,
        fpr_var_to_reg=fpr_var_to_reg,
        fpr_reg_to_var=fpr_reg_to_var,
        fpr_callee_saved_count=fpr_callee_saved_count,
    )


def _find_callee_saved_regs(func_lines: list[str]) -> set[int]:
    """Find which callee-saved registers are used by scanning prologue."""
    regs: set[int] = set()

    for line in func_lines:
        stripped = line.strip()

        # __savegprlr_N saves rN through r31
        m = _SAVEGPRLR_RE.search(stripped)
        if m:
            first = int(m.group(1))
            for r in range(first, 32):
                regs.add(r)
            return regs

        # stmw rN saves rN through r31
        m = _STMW_RE.search(stripped)
        if m:
            first = int(m.group(1))
            if 13 <= first <= 31:
                for r in range(first, 32):
                    regs.add(r)
                return regs

        # Individual saves: stw rN, -offset(r1)
        m = _INDIVIDUAL_SAVE_RE.search(stripped)
        if m:
            reg_num = int(m.group(1))
            if 13 <= reg_num <= 31:
                regs.add(reg_num)

    return regs
