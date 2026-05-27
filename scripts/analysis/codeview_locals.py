#!/usr/bin/env python3
"""Extract base-side local-variable → stack-offset mappings from MSVC CodeView.

Recompiles a source file with `/Z7` (CodeView embedded in `.debug$S` COFF
sections), parses the records, and produces a `{r1_offset: LocalInfo}` table
for a named function. Used by `stack_layout.py` to label slot rows with their
source variable names.

Standalone usage:
    python3 scripts/analysis/codeview_locals.py "?Poll@HamCharacter@@UAAXXZ"

Toolchain: MSVC PPC (Xbox 360 / Xenon, cl.exe 16.00.x via wibo).
Register encoding: CodeView CV_PPCREG_R1 = 2 (we keep only reg==2 entries).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shlex
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


# ── Configuration ────────────────────────────────────────────────────────────

CACHE_DIR = Path("/tmp/claude/stack_codeview")
DEBUG_FLAG = "/Z7"

# CodeView record kinds (cvinfo.h)
S_END         = 0x0006
S_FRAMEPROC   = 0x1012
S_BLOCK32     = 0x1103
S_LABEL32     = 0x1105
S_GPROC32     = 0x1110
S_LPROC32     = 0x110F
S_REGREL32    = 0x1111
S_BPREL32     = 0x110B
S_PROC_ID_END = 0x114F

# DEBUG_S_xxx subsection types
DEBUG_S_SYMBOLS = 0xF1

# PPC register encoding (cvconst.h CV_PPCREG_*): CV_PPCREG_R{n} = n + 1.
# MSVC X360 sets r31 as a frame-pointer alias for r1 in many functions
# (`subi r31, r1, FRAMESIZE` before stwu → r31 == new r1 after frame allocation),
# so locals can appear under either reg.
CV_PPCREG_R1 = 2   # stack pointer
CV_PPCREG_R31 = 32  # MSVC X360 frame-pointer alias
FRAME_REGS = {CV_PPCREG_R1, CV_PPCREG_R31}


# ── Symbol → source resolution ──────────────────────────────────────────────

def _project_root() -> Path:
    return Path(__file__).resolve().parent.parent.parent


def _report_path(root: Path) -> Optional[Path]:
    """Locate report.json under build/ (handles per-title subdirs)."""
    build = root / "build"
    if not build.exists():
        return None
    for sub in sorted(build.iterdir()):
        candidate = sub / "report.json"
        if candidate.exists():
            return candidate
    return None


def _find_unit_for_symbol(symbol: str, project_dir: Optional[str] = None
                           ) -> Optional[tuple[str, Path, str]]:
    """Return (unit_name, source_path, demangled_name) or None."""
    root = Path(project_dir) if project_dir else _project_root()
    report = _report_path(root)
    if report is None:
        return None
    with report.open() as f:
        data = json.load(f)
    for unit in data.get("units", []):
        for fn in unit.get("functions", []):
            if fn.get("name") == symbol:
                unit_name = unit.get("name", "")
                demangled = fn.get("metadata", {}).get("demangled_name", "")
                # Prefer authoritative source_path from unit metadata.
                src_rel = unit.get("metadata", {}).get("source_path")
                if src_rel:
                    src = root / src_rel
                    if src.exists():
                        return unit_name, src, demangled
                # Fallback: strip prefix and search src/.
                stripped = unit_name
                for prefix in ("default/", "main/"):
                    if stripped.startswith(prefix):
                        stripped = stripped[len(prefix):]
                for ext in (".cpp", ".c"):
                    src = root / "src" / (stripped + ext)
                    if src.exists():
                        return unit_name, src, demangled
                return unit_name, root / "src" / (stripped + ".cpp"), demangled
    return None


# ── build.ninja parsing for MSVC rule ───────────────────────────────────────

def _ninja_rule_for(src_path: Path, ninja_path: Path) -> Optional[dict]:
    """Find the `build ... : msvc[_pch] <src>` line for this source and return
    the rule's variable bindings (cflags, in_dir, mw_version, etc.)."""
    rel = str(src_path.resolve().relative_to(ninja_path.parent.resolve()))
    flat = re.sub(r"\$\n\s*", " ", ninja_path.read_text())
    pat = re.compile(
        rf"^build\s+(\S+)\s*:\s*(msvc(?:_pch)?)\s+{re.escape(rel)}\b",
        re.MULTILINE,
    )
    m = pat.search(flat)
    if not m:
        return None
    start = m.end()
    block_end = re.search(r"\nbuild\s|\nrule\s|\n[a-zA-Z]", flat[start:])
    block = flat[start:start + (block_end.start() if block_end else len(flat) - start)]
    bindings: dict[str, str] = {"_rule": m.group(2)}
    for line in block.split("\n"):
        ms = re.match(r"^\s+(\w+)\s*=\s*(.*)$", line)
        if ms:
            bindings[ms.group(1)] = ms.group(2).strip()
    return bindings


def _global_wibo_path_map(ninja_path: Path) -> str:
    """Read the project-global `wibo_path_map` variable from build.ninja."""
    flat = re.sub(r"\$\n\s*", " ", ninja_path.read_text())
    m = re.search(r"^wibo_path_map\s*=\s*(.+?)$", flat, re.MULTILINE)
    return m.group(1).strip() if m else ""


def _wibo_from_ninja(ninja_path: Path) -> Optional[Path]:
    """Scan build.ninja for the wibo binary path used by the msvc rule."""
    text = ninja_path.read_text()
    # Look for the first occurrence of `/wibo` in a rule command.
    m = re.search(r"(\S*/wibo)\b", text)
    if not m:
        return None
    p = Path(m.group(1))
    if not p.is_absolute():
        p = ninja_path.parent / p
    return p if p.exists() else None


# ── Recompile with /Z7 ──────────────────────────────────────────────────────

def compile_with_debug(src_path: Path, project_dir: Optional[str] = None) -> Path:
    """Recompile a single source file with CodeView debug info (/Z7).

    Cached by source mtime + cflags hash at CACHE_DIR/<base>.cv.obj.
    """
    root = Path(project_dir) if project_dir else _project_root()
    ninja_path = root / "build.ninja"
    if not ninja_path.exists():
        raise RuntimeError(f"build.ninja not found at {ninja_path}")

    bindings = _ninja_rule_for(src_path, ninja_path)
    if bindings is None:
        raise RuntimeError(f"no msvc rule found in build.ninja for {src_path}")

    cflags_str = bindings.get("cflags", "")
    if not cflags_str:
        raise RuntimeError(f"no cflags found for {src_path}")
    cflags = shlex.split(cflags_str)

    # Strip PCH-related flags (/Yu /Fp ...) — we compile standalone here.
    cleaned: list[str] = []
    i = 0
    while i < len(cflags):
        f = cflags[i]
        if f.startswith("/Yu") or f.startswith("/Yc") or f.startswith("/Fp"):
            i += 1
            continue
        if f in ("/Yu", "/Yc", "/Fp") and i + 1 < len(cflags):
            i += 2
            continue
        cleaned.append(f)
        i += 1
    cflags = cleaned

    mw_version = bindings.get("mw_version", "X360/16.00.11886.00")
    wibo_path_map = _global_wibo_path_map(ninja_path)

    compiler = root / "build" / "compilers" / mw_version / "cl.exe"
    # Resolve wibo by reading whichever path the msvc rule already uses.
    # dc3-decomp hardcodes /home/free/code/milohax/wibo/build/release/wibo;
    # rb3-xenon uses project-local build/tools/wibo.
    wibo = _wibo_from_ninja(ninja_path)
    if wibo is None:
        for c in (root / "build" / "tools" / "wibo",
                  Path("/home/free/code/milohax/wibo/build/release/wibo")):
            if c.exists():
                wibo = c
                break
    if not compiler.exists():
        raise RuntimeError(f"compiler not found: {compiler}")
    if wibo is None:
        raise RuntimeError(f"wibo not found in any of: {wibo_candidates}")

    key = hashlib.md5(
        (str(cflags) + str(src_path.stat().st_mtime)).encode()
    ).hexdigest()[:10]
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    out_path = CACHE_DIR / f"{src_path.stem}.{key}.cv.obj"
    if out_path.exists():
        return out_path

    # Two msvc-rule styles in the wild:
    #   • dc3-decomp: cwd=$in_dir, cflags use Windows e:/-paths, needs WIBO_PATH_MAP
    #   • rb3-xenon: cwd=$root, cflags use Linux relative paths, no path map needed
    in_dir = bindings.get("in_dir")
    if in_dir:
        cwd = Path(in_dir).resolve()
        try:
            in_arg = str(src_path.resolve().relative_to(cwd)).replace(os.sep, "/")
        except ValueError:
            in_arg = str(src_path)
        env = os.environ.copy()
        env.update({
            "WIBO_COMPUTER_NAME": "9QVZU3",
            "WIBO_FS_CACHE": "1",
            "WIBO_REWRITE_SHOWINCLUDES": "1",
            "WIBO_PATH_MAP": wibo_path_map,
        })
    else:
        cwd = root
        try:
            in_arg = str(src_path.resolve().relative_to(root.resolve()))
        except ValueError:
            in_arg = str(src_path)
        env = os.environ.copy()

    cmd = [str(wibo), str(compiler)] + cflags + [DEBUG_FLAG, "/c",
                                                  f"/Fo{out_path}", in_arg]
    result = subprocess.run(cmd, cwd=str(cwd), env=env,
                            capture_output=True, text=True)
    if result.returncode != 0 or not out_path.exists():
        # Surface a tight excerpt of cl.exe stderr.
        tail = (result.stderr or result.stdout or "").splitlines()[-10:]
        raise RuntimeError("cl.exe /Z7 compile failed:\n  " + "\n  ".join(tail))
    return out_path


# ── COFF / CodeView parsing ─────────────────────────────────────────────────

def _iter_debug_s_sections(data: bytes):
    """Yield (section_index, raw_bytes) for every .debug$S section in the .obj."""
    machine, num_sect, _ts, _sym_ptr, _num_syms, opt_sz, _ch = \
        struct.unpack_from("<HHIIIHH", data, 0)
    sec_off = 20 + opt_sz
    for i in range(num_sect):
        name = data[sec_off + i * 40:sec_off + i * 40 + 8].rstrip(b"\x00") \
            .decode("ascii", errors="replace")
        if name != ".debug$S":
            continue
        _vsize, _vaddr, rsize, rptr = struct.unpack_from(
            "<IIII", data, sec_off + i * 40 + 8)
        yield i, data[rptr:rptr + rsize]


@dataclass
class LocalInfo:
    name: str
    offset: int
    depth: int = 0
    is_param: bool = False


def _parse_section_for_function(sec: bytes, target_name: str
                                  ) -> Optional[dict[int, LocalInfo]]:
    """If this .debug$S section contains S_GPROC32 for target_name, return its
    {frame_offset: LocalInfo}; otherwise None."""
    if len(sec) < 4:
        return None
    if struct.unpack_from("<I", sec, 0)[0] != 4:  # CV_SIGNATURE_C13
        return None

    pos = 4
    while pos + 8 <= len(sec):
        subkind, sublen = struct.unpack_from("<II", sec, pos)
        pos += 8
        if subkind != DEBUG_S_SYMBOLS:
            pos += sublen
            pos = (pos + 3) & ~3
            continue

        end = pos + sublen
        sp = pos
        in_target = False
        depth = 0
        results: dict[int, LocalInfo] = {}

        while sp + 4 <= end:
            slen, skind = struct.unpack_from("<HH", sec, sp)
            if slen == 0:
                break
            payload = sec[sp + 4:sp + 2 + slen]

            if skind in (S_GPROC32, S_LPROC32):
                # Procedure header — fixed 35 bytes, then null-terminated name.
                ne = payload.find(b"\x00", 35)
                proc_name = payload[35:ne].decode("ascii", errors="replace") \
                    if ne >= 0 else ""
                if proc_name == target_name:
                    in_target = True
                    depth = 0
                    results = {}
                else:
                    in_target = False
            elif skind == S_BLOCK32 and in_target:
                depth += 1
            elif skind in (S_END, S_PROC_ID_END):
                if in_target:
                    if depth == 0:
                        return results
                    depth -= 1
            elif skind == S_REGREL32 and in_target:
                if len(payload) >= 10:
                    off, _typind, reg = struct.unpack_from("<IIH", payload, 0)
                    if reg in FRAME_REGS:
                        ne = payload.find(b"\x00", 10)
                        name = payload[10:ne].decode("ascii", errors="replace") \
                            if ne >= 0 else f"reg{reg}_{off:x}"
                        if off not in results or depth < results[off].depth:
                            results[off] = LocalInfo(name, off, depth, False)
            elif skind == S_BPREL32 and in_target:
                # Older BP-relative form: payload is u32 off, i32 typind, name
                if len(payload) >= 8:
                    off, _typind = struct.unpack_from("<Ii", payload, 0)
                    ne = payload.find(b"\x00", 8)
                    name = payload[8:ne].decode("ascii", errors="replace") \
                        if ne >= 0 else f"bp_{off:x}"
                    if off not in results or depth < results[off].depth:
                        results[off] = LocalInfo(name, off, depth, False)

            sp += slen + 2

        if in_target and results:
            # Reached end of section while still inside the proc — return what we have.
            return results

        pos += sublen
        pos = (pos + 3) & ~3
    return None


# ── Mangled-symbol → CodeView name ──────────────────────────────────────────

def msvc_codeview_name(mangled: str, demangled: str) -> str:
    """Build the un-decorated `Class::Method` name that CodeView emits.

    CodeView S_GPROC32 stores names as `Namespace::Class::Method` without
    parameter lists or return types. We can extract this from the demangled
    form (e.g. `public: virtual void __cdecl HamCharacter::Poll(void)` →
    `HamCharacter::Poll`).
    """
    if demangled:
        # Strip leading access/qualifier modifiers.
        s = demangled
        for prefix in ("public: ", "private: ", "protected: ",
                       "virtual ", "static "):
            while s.startswith(prefix):
                s = s[len(prefix):]
        # Strip return type by finding `__cdecl ` / `__stdcall ` / `__fastcall `
        # then walking back. Simpler: split on the first `(` and walk back.
        paren = s.find("(")
        head = s[:paren] if paren >= 0 else s
        # Strip leading return type (everything before the last space before name)
        for cc in (" __cdecl ", " __stdcall ", " __fastcall ", " __thiscall "):
            ci = head.find(cc)
            if ci >= 0:
                head = head[ci + len(cc):]
                break
        else:
            # No calling-convention marker — assume last token is the name
            sp = head.rfind(" ")
            if sp >= 0:
                head = head[sp + 1:]
        return head.strip()

    # Fallback: parse mangled `?Method@Class@@...` form
    m = re.match(r"^\?([^@]+)@([^@]+(?:@[^@]+)*)@@", mangled)
    if m:
        method, class_chain = m.group(1), m.group(2)
        parts = class_chain.split("@")[::-1]  # MSVC: rightmost is outer namespace
        return "::".join(parts + [method])
    return mangled


# ── Public API ──────────────────────────────────────────────────────────────

def extract_locals(symbol: str, project_dir: Optional[str] = None
                    ) -> dict[int, LocalInfo]:
    """Recompile + parse CodeView; return {r1_offset: LocalInfo} for the function."""
    found = _find_unit_for_symbol(symbol, project_dir)
    if not found:
        return {}
    _unit_name, src_path, demangled = found
    if not src_path.exists():
        return {}

    try:
        obj_path = compile_with_debug(src_path, project_dir)
    except RuntimeError as exc:
        print(f"  [codeview_locals] {exc}", file=sys.stderr)
        return {}

    cv_name = msvc_codeview_name(symbol, demangled)
    data = obj_path.read_bytes()
    for _idx, sec in _iter_debug_s_sections(data):
        result = _parse_section_for_function(sec, cv_name)
        if result is not None:
            return result
    return {}


# ── CLI ─────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("symbol", help="Mangled symbol (?Name@Class@@...)")
    parser.add_argument("--project-dir", default=None)
    args = parser.parse_args()

    locals_map = extract_locals(args.symbol, args.project_dir)
    if not locals_map:
        print("No locals extracted.", file=sys.stderr)
        sys.exit(1)
    print(f"Found {len(locals_map)} local(s) on the stack:")
    print(f"  {'offset':>8s}  {'name':24s}  depth")
    for off, info in sorted(locals_map.items()):
        print(f"  {off:>5d}  0x{off:03x}  {info.name:24s}  {info.depth:5d}")


if __name__ == "__main__":
    main()
