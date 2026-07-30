#!/usr/bin/env python3
"""laneAS-B — relocation-content extraction for byte-twin discrimination.

Both sides of a reloc-masked byte-equal pair have, by construction, identical
bytes ONLY because the relocated words were zeroed. This module recovers what
was zeroed:

  base side (compiled COFF obj)  -> per function, [(offset, symbol_name, type)]
  target side (dtk asm listing)  -> per function, [(offset, operand_token)]

Target tokens are `fn_<VA>` / `lbl_<VA>` / `jmp_<VA>` (resolved VAs) or helper
names (`__savegprlr_27`). Base names resolve to VAs through
scripts/target_symbol_map.json (inverted) + config/45410914/symbols.txt.

Read-only.
"""
import importlib.util
import json
import re
import struct
from collections import defaultdict
from pathlib import Path

IMAGE_SCN_CNT_CODE = 0x20


def load_S(worktree: Path):
    p = worktree / "scripts" / "harvest" / "size_order_automap.py"
    spec = importlib.util.spec_from_file_location("soa", p)
    S = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(S)
    S.PROJECT_ROOT = worktree
    S.BUILD = worktree / "build" / "45410914"
    return S


# ---------------------------------------------------------------------------
# COFF with relocation SYMBOL NAMES (size_order_automap._parse_coff drops them)
# ---------------------------------------------------------------------------
def parse_coff_full(path: Path):
    data = path.read_bytes()
    nsec = struct.unpack_from("<H", data, 2)[0]
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    optsz = struct.unpack_from("<H", data, 16)[0]
    str_start = symoff + nsym * 18

    # symbol table indexed by RAW index (aux records occupy indices too)
    idx2name = {}
    syms = []
    i = 0
    while i < nsym:
        o = symoff + i * 18
        nm = data[o:o + 8]
        if nm[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", nm, 4)[0]
            e = data.index(b"\0", str_start + so)
            name = data[str_start + so:e].decode("latin1")
        else:
            name = nm.split(b"\0")[0].decode("latin1")
        val = struct.unpack_from("<I", data, o + 8)[0]
        secn = struct.unpack_from("<h", data, o + 12)[0]
        typ = struct.unpack_from("<H", data, o + 14)[0]
        sc = data[o + 16]
        naux = data[o + 17]
        idx2name[i] = name
        syms.append(dict(name=name, val=val, secn=secn, typ=typ, sc=sc, idx=i))
        i += 1 + naux

    secs = {}
    base = 20 + optsz
    for i in range(nsec):
        o = base + i * 40
        raw_size = struct.unpack_from("<I", data, o + 16)[0]
        praw = struct.unpack_from("<I", data, o + 20)[0]
        preloc = struct.unpack_from("<I", data, o + 24)[0]
        nreloc = struct.unpack_from("<H", data, o + 32)[0]
        chars = struct.unpack_from("<I", data, o + 36)[0]
        rl = []
        for j in range(nreloc):
            rva = struct.unpack_from("<I", data, preloc + j * 10)[0]
            sidx = struct.unpack_from("<I", data, preloc + j * 10 + 4)[0]
            rtyp = struct.unpack_from("<H", data, preloc + j * 10 + 8)[0]
            rl.append((rva, idx2name.get(sidx, f"?idx{sidx}"), rtyp))
        secs[i + 1] = dict(raw_size=raw_size, praw=praw, relocs=rl,
                           chars=chars, idx=i + 1)
    return data, secs, syms


def base_funcs(path: Path):
    """Ordered code function symbols with masked bytes AND reloc names."""
    data, secs, syms = parse_coff_full(path)
    by_sec = defaultdict(list)
    for sy in syms:
        if (sy["secn"] > 0 and sy["secn"] in secs
                and sy["typ"] == 0x20 and sy["sc"] in (2, 3)):
            s = secs[sy["secn"]]
            if s["chars"] & IMAGE_SCN_CNT_CODE:
                by_sec[sy["secn"]].append(sy)
    # names of every symbol DEFINED in this obj (for intra-TU detection)
    defined = {sy["name"] for sy in syms if sy["secn"] > 0}
    out = []
    for secn in sorted(by_sec):
        s = secs[secn]
        members = sorted(by_sec[secn], key=lambda x: x["val"])
        for k, sy in enumerate(members):
            start = sy["val"]
            end = members[k + 1]["val"] if k + 1 < len(members) else s["raw_size"]
            if s["praw"] == 0 or end <= start:
                continue
            body = bytearray(data[s["praw"] + start:s["praw"] + end])
            rl = []
            for rva, rnm, rtyp in s["relocs"]:
                if start <= rva < end:
                    off = rva - start
                    for b in range(4):
                        if off + b < len(body):
                            body[off + b] = 0
                    rl.append((off, rnm, rtyp))
            rl.sort()
            out.append(dict(name=sy["name"], secn=secn, val=start,
                            size=end - start, masked=bytes(body), relocs=rl))
    return out, defined


# ---------------------------------------------------------------------------
# Target asm with operand tokens
# ---------------------------------------------------------------------------
_FN_RX = re.compile(r"^\.fn (fn_[0-9A-Fa-f]{8}|\S+),", re.M)
_INSTR_RX = re.compile(
    r"^/\* [0-9A-Fa-f]{8} [0-9A-Fa-f]{8}  "
    r"([0-9A-Fa-f]{2}) ([0-9A-Fa-f]{2}) ([0-9A-Fa-f]{2}) ([0-9A-Fa-f]{2}) \*/"
    r"\t(\S+)(.*)$")
_RELOC_AT = ("@ha", "@l", "@sda21")
_RELOC_SYM = re.compile(r"\b(fn_|lbl_|jmp_|__save|__rest|__fpr|__gpr|__mem|__vec)")

_TOK_RX = re.compile(
    r"(fn_[0-9A-Fa-f]{8}|lbl_[0-9A-Fa-f]{8}|jmp_[0-9A-Fa-f]{8}"
    r"|__[A-Za-z_][A-Za-z_0-9]*|\?[^\s,()]+|@[A-Za-z_][A-Za-z_0-9]*)")


def _operand_relocated(operand: str) -> bool:
    if any(t in operand for t in _RELOC_AT):
        return True
    for tok in operand.replace(",", " ").split():
        if tok.startswith(".L_"):
            continue
        if tok.startswith("?") or tok.startswith("@"):
            return True
        if _RELOC_SYM.search(tok):
            return True
    return False


def _token(operand: str):
    m = _TOK_RX.search(operand)
    if not m:
        return None
    t = m.group(1)
    for suf in ("@ha", "@l", "@sda21"):
        if t.endswith(suf):
            t = t[:-len(suf)]
    return t


def target_funcs(asm_path: Path):
    """{VA: dict(size, masked, relocs=[(off, token)])}"""
    out = {}
    cur_va = None
    in_fn = False
    words = []
    rl = []
    for line in asm_path.read_text().splitlines():
        fm = _FN_RX.match(line)
        if fm:
            tok = fm.group(1)
            vm = re.match(r"fn_([0-9A-Fa-f]{8})$", tok)
            cur_va = int(vm.group(1), 16) if vm else None
            in_fn = True
            words = []
            rl = []
            continue
        if line.startswith(".endfn"):
            if in_fn and cur_va is not None:
                body = b"".join(words)
                out[cur_va] = dict(size=len(body), masked=body, relocs=rl)
            in_fn = False
            continue
        if not in_fn:
            continue
        im = _INSTR_RX.match(line)
        if im:
            b = bytes(int(x, 16) for x in im.groups()[:4])
            if _operand_relocated(im.group(6)):
                off = len(words) * 4
                t = _token(im.group(6))
                # strip a trailing @l / @ha already handled; drop '@ha(' forms
                rl.append((off, t))
                b = b"\0\0\0\0"
            words.append(b)
    return out


# ---------------------------------------------------------------------------
# Name -> VA resolution
# ---------------------------------------------------------------------------
VA_RX = re.compile(r"^([A-Za-z_$?@][^\s=]*) = \.[a-z]+:0x([0-9A-Fa-f]{8})")


class Resolver:
    def __init__(self, worktree: Path, extra_map=None, hide=frozenset()):
        self.name2vas = defaultdict(set)
        self.va2names = defaultdict(set)
        m = json.loads((worktree / "scripts/target_symbol_map.json").read_text())
        # ★ names the map itself declares ARBITRARY must not resolve: they are
        # byte-true but identity-false, and inverting them poisons name->VA.
        arb = set()
        for key in ("_bijection_arbitrary", "_icf_arbitrary", "_denylist"):
            for a in m.get(key, []) or []:
                try:
                    arb.add(int(a, 16))
                except (ValueError, TypeError):
                    pass
        self.arbitrary = arb
        if extra_map:
            m = dict(m)
            m.update(extra_map)
        for k, v in m.items():
            if not isinstance(v, str) or not k.startswith("0x"):
                continue
            try:
                va = int(k, 16)
            except ValueError:
                continue
            if va in hide or va in arb:
                continue
            self.name2vas[v].add(va)
            self.va2names[va].add(v)
        # helpers / label symbols from symbols.txt (names dtk itself knows)
        for line in (worktree / "config/45410914/symbols.txt").read_text().splitlines():
            mm = VA_RX.match(line)
            if not mm:
                continue
            nm, va = mm.group(1), int(mm.group(2), 16)
            if nm.startswith(("fn_", "lbl_", "jmp_")):
                continue
            self.name2vas[nm].add(va)
            self.va2names[va].add(nm)

    def vas(self, name):
        return self.name2vas.get(name)


HELPER_RX = re.compile(r"^__(save|rest)(gpr|fpr|vmx|gprlr|fprlr)")


def token_va(tok):
    if tok is None:
        return None
    m = re.match(r"^(?:fn|lbl|jmp)_([0-9A-Fa-f]{8})$", tok)
    return int(m.group(1), 16) if m else None
