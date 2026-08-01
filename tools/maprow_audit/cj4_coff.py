#!/usr/bin/env python3
"""Minimal COFF symbol-table reader for the lane CJ-4 definedness audit.

WHY THIS AND NOT grep: an undefined external *reference* contains the identical
name bytes as a definition.  A substring scan cannot tell them apart -- it is
structurally incapable of answering "does this obj DEFINE the symbol".  Only the
symbol table's SectionNumber field answers it.

  SectionNumber  > 0  -> DEFINED (in that section)
  SectionNumber == 0  -> IMAGE_SYM_UNDEFINED  (external ref, or COMMON if Value!=0)
  SectionNumber == -1 -> IMAGE_SYM_ABSOLUTE
  SectionNumber == -2 -> IMAGE_SYM_DEBUG
  StorageClass == 105 -> IMAGE_SYM_CLASS_WEAK_EXTERNAL (section 0 + 1 aux record
                         whose TagIndex names the symbol it aliases)
"""
import struct
from typing import Dict, List, NamedTuple

IMAGE_SYM_CLASS_WEAK_EXTERNAL = 105
IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3


class Sym(NamedTuple):
    name: str
    value: int
    section: int      # signed int16
    typ: int
    storage: int
    naux: int
    index: int        # raw symbol-table index (aux records occupy indices too)
    weak_tag: int     # for weak externals: aux TagIndex, else -1


def read_symbols(data: bytes) -> List[Sym]:
    if len(data) < 20:
        return []
    sym_off, nsyms = struct.unpack_from("<II", data, 8)
    if sym_off == 0 or nsyms == 0:
        return []
    strtab = sym_off + nsyms * 18
    out: List[Sym] = []
    i = 0
    while i < nsyms:
        e = sym_off + i * 18
        if e + 18 > len(data):
            break
        nb = data[e:e + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            so = struct.unpack_from("<I", nb, 4)[0]
            a = strtab + so
            if a < len(data):
                end = data.index(b"\x00", a)
                name = data[a:end].decode("ascii", "replace")
            else:
                name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii", "replace")
        value, section, typ = struct.unpack_from("<IhH", data, e + 8)
        storage = data[e + 16]
        naux = data[e + 17]
        weak_tag = -1
        if storage == IMAGE_SYM_CLASS_WEAK_EXTERNAL and naux >= 1:
            ae = e + 18
            if ae + 4 <= len(data):
                weak_tag = struct.unpack_from("<I", data, ae)[0]
        out.append(Sym(name, value, section, typ, storage, naux, i, weak_tag))
        i += 1 + naux
    return out


def classify(data: bytes) -> Dict[str, str]:
    """name -> one of DEFINED / WEAK / COMMON / UNDEF.

    If a name appears more than once (it shouldn't for externals, but static
    COMDAT selection can), DEFINED wins -- we are asking "is it defined at all".
    """
    rank = {"DEFINED": 3, "COMMON": 2, "WEAK": 1, "UNDEF": 0}
    out: Dict[str, str] = {}
    for s in read_symbols(data):
        if s.section > 0:
            k = "DEFINED"
        elif s.section == 0:
            if s.storage == IMAGE_SYM_CLASS_WEAK_EXTERNAL:
                k = "WEAK"
            elif s.value != 0:
                k = "COMMON"
            else:
                k = "UNDEF"
        else:
            continue  # ABSOLUTE / DEBUG -- not a code definition
        if rank[k] > rank.get(out.get(s.name, "UNDEF"), -1) or s.name not in out:
            out[s.name] = k
    return out
