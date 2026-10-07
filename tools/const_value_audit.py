#!/usr/bin/env python3
"""Compare the VALUE of every constant our compiled functions load against retail's.

Why this exists
---------------
objdiff's graded ruler (`name_check`) compares a constant-pool load by the NAME of
its relocation target only.  When retail's target is a placeholder (`lbl_820EEF18`)
the name check is forgiven, and the pointed-to bytes are never read.  So a wrong
float literal, a wrong string, or two constants swapped between sites reads as
equal -- even in a row at fuzzy 100.  (W16-PQ: a byte-swapped 9.0e9f in
`VocalPlayer::Poll`; W16-PR: swapped `key_shift_left/right.wid`, inverted
FLT_MAX bounds, 250 vs 1000 in `VocalTrack::UpdateScrolling`.)

What it does
------------
For every paired function row (report.json row whose symbol exists on both sides),
objdiff's own instruction alignment (`diff --batch --include-instructions`, the
graded config from objdiff.json) is read, and every `@l` relocation operand on a
load or `addi` is resolved to bytes:

  retail  -- the target symbol's VA (placeholder `lbl_/data_/rdata_XXXXXXXX`, then
             config symbols.txt, then scripts/target_symbol_map.json), read from
             orig/45410914/band.exe through the PE section table.
  ours    -- the defining COFF symbol in OUR compiled obj (this unit's base obj
             first, then every base obj), read from its section bytes.  Words our
             relocations cover (pointers) are masked on both sides.

Width: lfs 4, lfd 8, lwz 4, lhz/lha 2, lbz 1, ld 8; an `addi` (address-of) compares
a NUL-terminated string when our bytes look like one, else our symbol's extent.
Only READ-ONLY data is compared (our section lacks IMAGE_SCN_MEM_WRITE, or retail's
VA is in .rdata); mutable globals are not constants and are counted as `mutable`.

Three comparisons per function, each blind where another is not:

  POS     the two operands of one aligned instruction row.  Catches wrong values
          and swaps; flags harmless load reordering.
  SET     the set of constant values each side loads anywhere in the function.
          Reorder-proof and reload-proof; blind to swaps.
  USE     each loaded value is followed through its register to the aligned row
          that consumes it (a call consumes r3-r10/f1-f13 by register).  A reorder
          leaves the consumer's value unchanged; a swap does not.

Verdict per function:
  VALUE    a value appears on one side's SET only, and a POS or USE mismatch
           involves it                                   -> candidate wrong literal
  SETONLY  SET differs but no aligned row disagrees     -> unaligned; read by hand
  SWAP     SET equal, a USE mismatch                     -> candidate swap
  REORDER  SET equal, USE equal, POS mismatch            -> benign
  CLEAN    nothing differs

Usage
-----
  tools/const_value_audit.py                         # whole binary, writes JSON
  tools/const_value_audit.py --unit default/VocalPlayer
  tools/const_value_audit.py --rows rows.txt         # "unit symbol" per line
  tools/const_value_audit.py --out ~/tmp/cva.json --jobs 16

Run it on a FULLY BUILT tree (./tools/ninja-locked): an unpatched or unbuilt
tree has stale target names (the renamer) and stale objs.
"""
import argparse
import collections
import concurrent.futures as cf
import json
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------- COFF (ours)

SCN_WRITE = 0x80000000
SCN_CODE = 0x20


class Coff:
    """Minimal PE/COFF obj reader: sections, relocations, symbols (LE headers)."""

    def __init__(self, path):
        self.path = path
        d = open(path, "rb").read()
        self.d = d
        _m, nsec, _t, psym, nsym, opt, _c = struct.unpack_from("<HHIIIHH", d, 0)
        strtab = psym + nsym * 18
        self.secs = []
        for i in range(nsec):
            o = 20 + opt + i * 40
            raw = d[o:o + 8]
            size, ptr, prel, _pl, nrel, _nl, ch = struct.unpack_from("<IIIIHHI", d, o + 16)
            relocs = set()
            for k in range(nrel):
                va, _si, _ty = struct.unpack_from("<IIH", d, prel + k * 10)
                relocs.add(va)
            self.secs.append({"size": size, "ptr": ptr, "ch": ch, "relocs": relocs,
                              "raw": raw})
        self.syms = {}            # name -> (secidx 0-based, value)
        self.by_sec = collections.defaultdict(list)
        i = 0
        while i < nsym:
            o = psym + i * 18
            if d[o:o + 4] == b"\0\0\0\0":
                so = struct.unpack_from("<I", d, o + 4)[0]
                e = d.index(b"\0", strtab + so)
                name = d[strtab + so:e].decode("latin1")
            else:
                name = d[o:o + 8].rstrip(b"\0").decode("latin1")
            value, secn, _typ, scl, naux = struct.unpack_from("<IhHBB", d, o + 8)
            if secn > 0 and scl in (2, 3) and naux == 0:
                self.syms.setdefault(name, (secn - 1, value))
                self.by_sec[secn - 1].append(value)
            i += 1 + naux
        for k in self.by_sec:
            self.by_sec[k] = sorted(set(self.by_sec[k]))

    def code(self, name):
        """(section offset, bytes, reloc offsets relative to start) for a code symbol."""
        if name not in self.syms:
            return None
        si, val = self.syms[name]
        s = self.secs[si]
        if not (s["ch"] & SCN_CODE):
            return None
        nxt = [v for v in self.by_sec[si] if v > val]
        end = nxt[0] if nxt else s["size"]
        rel = {r - val for r in s["relocs"] if val <= r < end}
        return val, self.d[s["ptr"] + val:s["ptr"] + end], rel

    def extent(self, name):
        """(bytes, mask-positions set, readonly) for a defined data symbol, or None."""
        if name not in self.syms:
            return None
        si, val = self.syms[name]
        s = self.secs[si]
        if s["ch"] & SCN_CODE:
            return None
        if s["ptr"] == 0:                      # .bss: uninitialised, mutable
            return b"", set(), False
        nxt = [v for v in self.by_sec[si] if v > val]
        end = nxt[0] if nxt else s["size"]
        data = self.d[s["ptr"] + val:s["ptr"] + end]
        mask = set()
        for r in s["relocs"]:
            if val <= r < end:
                mask.update(range(r - val, r - val + 4))
        return data, mask, not (s["ch"] & SCN_WRITE)


# ------------------------------------------------------------- PE (retail)

class Image:
    def __init__(self, path):
        d = open(path, "rb").read()
        self.d = d
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        assert d[pe:pe + 4] == b"PE\0\0", path
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        szopt = struct.unpack_from("<H", d, pe + 20)[0]
        base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        self.secs = []
        for i in range(nsec):
            o = pe + 24 + szopt + i * 40
            name = d[o:o + 8].rstrip(b"\0").decode("latin1")
            vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
            ch = struct.unpack_from("<I", d, o + 36)[0]
            self.secs.append((name, base + va, vs, rp, rs, ch))

    def section(self, va):
        for s in self.secs:
            if s[1] <= va < s[1] + s[2]:
                return s
        return None

    def cstr(self, va, cap=4096, allow_empty=False):
        """NUL-terminated printable string at va (with its NUL), else None."""
        s = self.section(va)
        if s is None:
            return None
        off = va - s[1]
        b = self.d[s[3] + off:s[3] + min(s[4], off + cap)]
        if allow_empty and b[:1] == b"\0":
            return b"\0"
        return b[:b.index(b"\0") + 1] if looks_like_cstr(b) else None

    def read(self, va, n):
        s = self.section(va)
        if s is None:
            return None
        off = va - s[1]
        if off + n > s[4]:
            return None
        return self.d[s[3] + off:s[3] + off + n]


# ------------------------------------------------------------- resolution

PLACEHOLDER = re.compile(r"^(?:lbl|data|rdata|bss|jumptable)_([0-9A-Fa-f]{8})$")
OPERAND = re.compile(r"(?P<sym>[^\s,()]+?)(?P<add>[+-]0x[0-9a-fA-F]+)?@l\b")

LOAD_W = {"lfs": 4, "lfsu": 4, "lfd": 8, "lfdu": 8, "lwz": 4, "lwzu": 4,
          "lha": 2, "lhz": 2, "lhzu": 2, "lbz": 1, "lbzu": 1, "ld": 8, "lwa": 4}
VALUE_OPS = set(LOAD_W) | {"addi"}

NO_DEST_PREFIX = ("st", "cmp", "fcmp", "b", "mt", "dcb", "tw", "td", "sync",
                  "isync", "eieio", "lwsync", "icbi")


class Resolver:
    def __init__(self, root, img):
        self.root = root
        self.img = img
        self.va_by_name = {}
        sym_txt = os.path.join(root, "config/45410914/symbols.txt")
        rx = re.compile(r"^(\S+) = [^:]+:0x([0-9A-Fa-f]+);")
        self.fn_vas = collections.defaultdict(list)
        for ln in open(sym_txt):
            m = rx.match(ln)
            if m:
                self.va_by_name.setdefault(m.group(1), int(m.group(2), 16))
                self.fn_vas[m.group(1)].append(int(m.group(2), 16))
        tsm = json.load(open(os.path.join(root, "scripts/target_symbol_map.json")))
        for k, v in tsm.items():
            if isinstance(v, str) and re.fullmatch(r"0x[0-9A-Fa-f]+", k):
                self.va_by_name.setdefault(v, int(k, 16))
                self.fn_vas[v].append(int(k, 16))
        self.objs = {}
        self.global_def = None
        self.clean = None          # --alt-image, for the in-place-patch cross-check

    def obj(self, path):
        if path not in self.objs:
            try:
                self.objs[path] = Coff(path)
            except (OSError, struct.error, ValueError):
                self.objs[path] = None
        return self.objs[path]

    def build_global(self, base_paths):
        self.global_def = {}
        for p in base_paths:
            c = self.obj(p)
            if c is None:
                continue
            for n, (si, _v) in c.syms.items():
                s = c.secs[si]
                if not (s["ch"] & SCN_CODE):
                    self.global_def.setdefault(n, p)

    def function_va(self, name, tcode):
        """VA of a target function whose non-relocated words match band.exe, else None."""
        if tcode is None:
            return None
        _off, body, rel = tcode
        for va in self.fn_vas.get(name, []):
            r = self.img.read(va, len(body))
            if r is None:
                continue
            if all(body[k:k + 4] == r[k:k + 4] for k in range(0, len(body) - 3, 4)
                   if k not in rel):
                return va
        return None

    def retail_ea(self, insn_va, label_va):
        """Effective address of retail's D-form @l instruction at insn_va.

        dtk's split relocates to the CONTAINING label and can drop the addend
        (std::exception::what: addi -0x1500 -> 0x8200EB00, reloc lbl_8200EAF8+0),
        so the address is rebuilt from retail's own 16-bit immediate, taking the
        high half that lands nearest the label.
        """
        w = self.img.read(insn_va, 4)
        if w is None:
            return None, None
        word = struct.unpack(">I", w)[0]
        imm = word & 0xFFFF
        if imm >= 0x8000:
            imm -= 0x10000
        hi = label_va & 0xFFFF0000
        cands = [hi + dh + imm for dh in (-0x10000, 0, 0x10000)]
        return min(cands, key=lambda a: abs(a - label_va)), word >> 26

    def retail_va(self, name):
        m = PLACEHOLDER.match(name)
        if m:
            return int(m.group(1), 16)
        return self.va_by_name.get(name)

    def ours(self, name, base_path):
        c = self.obj(base_path)
        ext = c.extent(name) if c else None
        if ext is None and self.global_def is not None and name in self.global_def:
            c2 = self.obj(self.global_def[name])
            ext = c2.extent(name) if c2 else None
        return ext


def looks_like_cstr(b):
    z = b.find(b"\0")
    if z < 1:
        return False
    s = b[:z]
    return all(32 <= ch < 127 or ch in (9, 10, 13) for ch in s)


def compare_operand(res, op, va, bsym, badd, base_path):
    """Return dict(kind, ours, retail, eq) or dict(skip=reason).  va: retail EA."""
    ext = res.ours(bsym, base_path)
    if ext is None:
        return {"skip": "ours_unresolved"}
    data, mask, ro = ext
    if va is None:
        return {"skip": "retail_unresolved"}
    sec = res.img.section(va)
    if sec is None:
        return {"skip": "retail_unmapped"}
    retail_ro = sec[0] == ".rdata"
    if not ro and not retail_ro and not getattr(res, "include_mutable", False):
        return {"skip": "mutable"}
    data = data[badd:]
    mask = {m - badd for m in mask if m >= badd}
    if op == "addi":
        if looks_like_cstr(data) or (bsym.startswith("??_C@") and data[:1] == b"\0"):
            s = data[:data.index(b"\0") + 1]
            r = res.img.cstr(va, allow_empty=True)
            return {"kind": "str", "ours": s, "retail": r, "eq": r == s}
        w = len(data)
        if w == 0 or w > 4096:
            return {"skip": "extent"}
        kind = "blob"
    else:
        w = LOAD_W[op]
        if not data and not ro and getattr(res, "include_mutable", False):
            # Ours is in .bss (Coff.extent returns b"" for a section with no raw
            # data): a zero-initialised static, so its initial value is zeros.
            data = bytes(w)
        if len(data) < w:
            return {"skip": "extent"}
        data = data[:w]
        kind = {"lfs": "f32", "lfsu": "f32", "lfd": "f64", "lfdu": "f64"}.get(op, "int%d" % (8 * w))
    r = res.img.read(va, w)
    if r is None and not retail_ro and getattr(res, "include_mutable", False):
        # Past the section's raw data but inside its virtual size: the loader
        # zero-fills this tail (retail .bss), so the initial value is zeros.
        if sec[1] <= va and va + w <= sec[1] + sec[2]:
            r = bytes(w)
    if r is None:
        return {"skip": "retail_unmapped"}
    if mask:
        data = bytes(0 if i in mask else x for i, x in enumerate(data))
        r = bytes(0 if i in mask else x for i, x in enumerate(r))
    return {"kind": kind, "ours": data, "retail": r, "eq": r == data}


def fmt(kind, b):
    if b is None:
        return None
    if kind == "f32" and len(b) == 4:
        return "%s (%r)" % (b.hex(), struct.unpack(">f", b)[0])
    if kind == "f64" and len(b) == 8:
        return "%s (%r)" % (b.hex(), struct.unpack(">d", b)[0])
    if kind == "str":
        return repr(b.rstrip(b"\0").decode("latin1"))
    return b.hex() if len(b) <= 64 else b[:64].hex() + "..."


# ------------------------------------------------------------- per function

def parse_operand(side):
    if not side:
        return None
    m = OPERAND.search(side.get("args", ""))
    if not m:
        return None
    return m.group("sym"), int(m.group("add"), 16) if m.group("add") else 0


def regs(side):
    return [a["value"] for a in side.get("typed_args", []) if a["type"] == "Register"]


ARG_REGS = ["r%d" % i for i in range(3, 11)] + ["f%d" % i for i in range(1, 14)]
VOLATILE = {"r0"} | {"r%d" % i for i in range(3, 13)} | {"f%d" % i for i in range(0, 14)}


def use_sites(rows, side_key, values):
    """values: {row_index: value_key} for loads on this side.

    Returns (sites, feeds): sites {site: Counter(value)}, feeds {load_row: set(site)}.
    Follows each loaded register forward through the aligned listing until it is
    overwritten; a consuming row is a site.  Calls consume argument registers by
    register and clobber volatiles.  Unconditional b/blr/bctr end tracking.
    """
    live = {}
    sites = collections.defaultdict(collections.Counter)
    feeds = collections.defaultdict(set)
    for i, ins in enumerate(rows):
        x = ins.get(side_key)
        if not x:
            continue
        op = x["opcode"]
        rr = regs(x)
        if op in ("bl", "bctrl", "blrl"):
            for r in ARG_REGS:
                if r in live:
                    site = ("call", i, r)
                    sites[site][live[r][0]] += 1
                    feeds[live[r][1]].add(site)
            for r in VOLATILE:
                live.pop(r, None)
            continue
        if op in ("b", "blr", "bctr"):
            live.clear()
            continue
        has_dest = not op.startswith(NO_DEST_PREFIX)
        srcs = rr[1:] if has_dest else rr
        # A store consumes its value register into a memory slot: key the site by
        # the slot (offset + base register), so two stores emitted in a different
        # order still compare slot-to-slot instead of row-to-row.
        slot = None
        if op.startswith("st") and rr:
            ta = x.get("typed_args", [])
            slot = ("store", op) + tuple(str(a["value"]) for a in ta[1:])
        for k, r in enumerate(srcs):
            if r in live:
                site = slot if (slot and k == 0) else ("row", i)
                sites[site][live[r][0]] += 1
                feeds[live[r][1]].add(site)
        if has_dest and rr:
            live.pop(rr[0], None)
            if op.endswith("u") and len(rr) > 1:
                live.pop(rr[1], None)
        if i in values and has_dest and rr:
            live[rr[0]] = (values[i], i)
    return sites, feeds


PRIMARY = {"addi": 14, "lfs": 48, "lfsu": 49, "lfd": 50, "lfdu": 51, "lwz": 32,
           "lwzu": 33, "lbz": 34, "lbzu": 35, "lhz": 40, "lhzu": 41, "lha": 42,
           "ld": 58, "lwa": 58}


def sext16(x):
    return x - 0x10000 if x >= 0x8000 else x


def analyze_function(res, d, base_path, tobj, bobj):
    rows = d.get("instructions", [])
    out = {"pos": [], "skips": collections.Counter(), "n_pos": 0}
    sym = d["symbol"]
    tcode = tobj.code(sym) if tobj else None
    bcode = bobj.code(sym) if bobj else None
    fva = res.function_va(sym, tcode)
    # objdiff addresses are in its combined-section space: offset from the first row.
    t0 = next((int(r["target"]["address"], 16) for r in rows if r.get("target")), 0)
    b0 = next((int(r["base"]["address"], 16) for r in rows if r.get("base")), 0)
    out["retail_va"] = "0x%08X" % fva if fva else None

    def retail_addr(t, to):
        """Retail EA of a target row's @l operand: from retail's instruction when
        the function VA is verified, else label + printed addend."""
        label = res.retail_va(to[0])
        if label is None:
            return None, "retail_unresolved"
        label += to[1]
        if fva is None or tcode is None:
            return label, "ea_label"
        insn_va = fva + int(t["address"], 16) - t0
        insn_vas[id(t)] = insn_va
        ea, prim = res.retail_ea(insn_va, label)
        if prim != PRIMARY.get(t["opcode"]):
            return label, "ea_label"
        if t["opcode"] in ("ld", "lwa"):
            ea = (ea & ~3)
        return ea, "ea_insn"

    def our_addend(b, bo):
        """Our in-place REFLO addend (the instruction immediate), else the printed one."""
        if bcode is None:
            return bo[1]
        off = int(b["address"], 16) - b0
        if not (0 <= off <= len(bcode[1]) - 4):
            return bo[1]
        word = struct.unpack_from(">I", bcode[1], off)[0]
        if word >> 26 != PRIMARY.get(b["opcode"]):
            return bo[1]
        imm = word & (0xFFFC if b["opcode"] in ("ld", "lwa") else 0xFFFF)
        return sext16(imm)

    insn_vas = {}
    has_bctr = any((r.get("base") or {}).get("opcode") == "bctr" for r in rows)
    tvals, bvals = {}, {}
    for i, ins in enumerate(rows):
        t, b = ins.get("target"), ins.get("base")
        to = parse_operand(t) if t and t["opcode"] in VALUE_OPS else None
        bo = parse_operand(b) if b and b["opcode"] in VALUE_OPS else None
        if bo:
            bo = (bo[0], our_addend(b, bo))
            ext = res.ours(bo[0], base_path)
            if ext is not None and ext[2]:
                data = ext[0][bo[1]:]
                w = LOAD_W.get(b["opcode"])
                key = data[:w] if w else (data[:data.index(b"\0") + 1] if looks_like_cstr(data) else None)
                if key:
                    bvals[i] = key
        va = None
        if to:
            va, how = retail_addr(t, to)
            out["skips"][how] += 1
            sec = res.img.section(va) if va is not None else None
            if sec is not None and sec[0] == ".rdata":
                w = LOAD_W.get(t["opcode"])
                key = res.img.read(va, w) if w else res.img.cstr(va)
                if key:
                    tvals[i] = key
        if to and bo and t["opcode"] == b["opcode"]:
            if bo[0].startswith("$T") and has_bctr:
                # MSVC switch jump table: branch offsets that move with codegen.
                out["skips"]["jumptable"] += 1
                bvals.pop(i, None)
                tvals.pop(i, None)
                continue
            c = compare_operand(res, b["opcode"], va, bo[0], bo[1], base_path)
            if "skip" in c:
                out["skips"][c["skip"]] += 1
                continue
            out["n_pos"] += 1
            if c["retail"] is not None:
                tvals[i] = c["retail"]
            bvals[i] = c["ours"]
            if not c["eq"]:
                dx = None
                if res.clean is not None and c["retail"] is not None:
                    iva = insn_vas.get(id(t))
                    dx = (res.clean.read(va, len(c["retail"])) != res.img.read(va, len(c["retail"]))
                          or (iva is not None and res.clean.read(iva, 4) != res.img.read(iva, 4)))
                out["pos"].append({"row": i, "op": b["opcode"], "kind": c["kind"],
                                   "image_patch": dx,
                                   "target_sym": to[0], "base_sym": bo[0],
                                   "retail_ea": "0x%08X" % va,
                                   "retail": fmt(c["kind"], c["retail"]),
                                   "ours": fmt(c["kind"], c["ours"]),
                                   "retail_raw": c["retail"].hex() if c["retail"] else None,
                                   "ours_raw": c["ours"].hex()})
    # Rebuild the sets from the final per-row keys so masking is applied uniformly.
    tset = collections.Counter(tvals.values())
    bset = collections.Counter(bvals.values())
    tsites, tfeeds = use_sites(rows, "target", tvals)
    bsites, bfeeds = use_sites(rows, "base", bvals)
    use_mm = []

    def same_op(site):
        # A row site is only comparable when both sides run the same instruction
        # there; a "replace" row (fmuls vs fcmpu) is two different consumers.
        if site[0] == "store":
            return True
        r = rows[site[1]]
        return (r.get("target") or {}).get("opcode") == (r.get("base") or {}).get("opcode")

    for site in set(tsites) & set(bsites):
        if not same_op(site):
            continue
        # Compare the VALUES reaching the site, not how often: a stack slot
        # stored on two paths in one build and three in the other is not a swap.
        if set(tsites[site]) != set(bsites[site]):
            use_mm.append({"site": list(site),
                           "retail": sorted(k.hex() for k in tsites[site]),
                           "ours": sorted(k.hex() for k in bsites[site])})
    t_only = set(tset) - set(bset)
    b_only = set(bset) - set(tset)
    out["set_retail_only"] = sorted(k.hex() for k in t_only)
    out["set_ours_only"] = sorted(k.hex() for k in b_only)
    out["use"] = use_mm
    involved = set()
    for p in out["pos"]:
        involved.add(bytes.fromhex(p["ours_raw"]))
        if p["retail_raw"]:
            involved.add(bytes.fromhex(p["retail_raw"]))
    for u in use_mm:
        involved.update(bytes.fromhex(h) for h in u["retail"] + u["ours"])
    # A position mismatch is CLEARED (a reorder) only when each side's load reaches
    # at least one consumer that is aligned and carries the same value on both sides.
    uncleared = []
    for p in out["pos"]:
        i = p["row"]
        ok = True
        for feeds, mine, other in ((tfeeds, tsites, bsites), (bfeeds, bsites, tsites)):
            st = feeds.get(i, set())
            if not any(site in other and other[site] == mine[site] for site in st):
                ok = False
        p["cleared"] = ok
        if not ok:
            uncleared.append(i)
    if out["pos"] and all(p.get("image_patch") for p in out["pos"]):
        verdict = "IMAGE_PATCH"
    elif (t_only | b_only) & involved:
        verdict = "VALUE"
    elif use_mm:
        verdict = "SWAP"
    elif uncleared:
        verdict = "POS_UNRESOLVED"
    elif t_only or b_only:
        verdict = "SETONLY"
    elif out["pos"]:
        verdict = "REORDER"
    else:
        verdict = "CLEAN"
    out["verdict"] = verdict
    out["skips"] = dict(out["skips"])
    return out


# ------------------------------------------------------------- driver

G = {}


def objdiff_cmd(root):
    return [os.path.join(root, "bin/objdiff-cli"), "diff", "-p", root, "-f", "json",
            "--include-instructions", "--batch", "-o", "-"]


def run_unit(args):
    unit, base_path, target_path, syms = args
    res = G["res"]
    tobj = res.obj(target_path)
    bobj = res.obj(base_path)
    p = subprocess.run(objdiff_cmd(G["root"]) + ["-u", unit], input="\n".join(syms),
                       capture_output=True, text=True, cwd=G["root"])
    results = []
    n_not_found = 0
    for ln in p.stdout.splitlines():
        try:
            d = json.loads(ln)
        except ValueError:
            continue
        if "error" in d:
            n_not_found += 1
            continue
        a = analyze_function(res, d, base_path, tobj, bobj)
        a.update({"unit": unit, "symbol": d["symbol"],
                  "fuzzy": d.get("fuzzy_match_percent", 0.0),
                  "size": d.get("target_size")})
        a.pop("pos_keys", None)
        results.append(a)
    return unit, results, n_not_found, p.returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--unit", action="append", help="limit to unit(s)")
    ap.add_argument("--rows", help="file of 'unit symbol' lines")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 4) // 2))
    ap.add_argument("--out", default=os.path.expanduser("~/tmp/const_value_audit.json"))
    ap.add_argument("--image", default=None)
    ap.add_argument("--alt-image", default=None,
                    help="a second PE of the same title (e.g. the RB3DX image, or clean TU5 "
                         "if the target is ever switched back); a mismatch whose retail bytes or "
                         "instruction differ there is labelled image_patch -- an in-place binary "
                         "patch, not source.  Refused if byte-identical to --image.")
    ap.add_argument("--include-mutable", action="store_true",
                    help="also compare INITIALISED writable data (a file-scope or function "
                         "static's initial value, e.g. `static float sFogScale = 0.125f`). "
                         "A symbol of ours in .bss reads as zeros (a zero-initialised "
                         "static); a value a dynamic initialiser writes later is not seen "
                         "on either side.")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    img = Image(args.image or os.path.join(root, "orig/45410914/band.exe"))
    res = Resolver(root, img)
    res.include_mutable = args.include_mutable
    if args.alt_image:
        alt = Image(args.alt_image)
        if alt.d == img.d:
            sys.exit("--alt-image is byte-identical to the target image: the cross-check "
                     "would be vacuous")
        res.clean = alt
        print("alt-image cross-check:", args.alt_image)
    od = json.load(open(os.path.join(root, "objdiff.json")))
    rep = json.load(open(os.path.join(root, "build/45410914/report.json")))
    base_of = {u["name"]: os.path.join(root, u["base_path"]) for u in od["units"]
               if u.get("base_path")}
    tgt_of = {u["name"]: os.path.join(root, u["target_path"]) for u in od["units"]
              if u.get("target_path")}
    src_of = {u["name"]: u.get("metadata", {}).get("source_path") for u in od["units"]}
    res.build_global(sorted(set(p for p in base_of.values() if os.path.exists(p))))
    want = None
    if args.rows:
        want = collections.defaultdict(list)
        for ln in open(args.rows):
            if ln.strip():
                u, s = ln.split()[:2]
                want[u].append(s)
    jobs = []
    for u in rep["units"]:
        n = u["name"]
        if n not in base_of or not os.path.exists(base_of[n]):
            continue
        if args.unit and n not in args.unit:
            continue
        syms = [f["name"] for f in u.get("functions", [])]
        if want is not None:
            if n not in want:
                continue
            syms = [s for s in syms if s in set(want[n])]
        if syms:
            jobs.append((n, base_of[n], tgt_of.get(n), syms))
    G.update(res=res, root=root)
    all_rows = []
    totals = collections.Counter()
    with cf.ProcessPoolExecutor(max_workers=args.jobs) as ex:
        for unit, results, nf, rc in ex.map(run_unit, jobs, chunksize=1):
            totals["units"] += 1
            totals["not_found"] += nf
            if rc != 0:
                totals["objdiff_rc_nonzero"] += 1
            for a in results:
                a["source_path"] = src_of.get(unit)
                totals["functions"] += 1
                totals["pos_pairs"] += a["n_pos"]
                totals["pos_mismatch"] += len(a["pos"])
                totals["verdict_" + a["verdict"]] += 1
                for k, v in a["skips"].items():
                    totals["skip_" + k] += v
                all_rows.append(a)
    flagged = [a for a in all_rows if a["verdict"] != "CLEAN"]
    json.dump({"totals": dict(totals), "flagged": flagged}, open(args.out, "w"), indent=1)
    print("units %d  functions %d  not-found %d" % (totals["units"], totals["functions"],
                                                     totals["not_found"]))
    print("position-paired constant operands compared: %d, unequal: %d" %
          (totals["pos_pairs"], totals["pos_mismatch"]))
    print("verdicts: " + "  ".join("%s=%d" % (k[8:], v) for k, v in sorted(totals.items())
                                   if k.startswith("verdict_")))
    print("skipped operands: " + "  ".join("%s=%d" % (k[5:], v) for k, v in sorted(totals.items())
                                          if k.startswith("skip_")))
    for a in sorted(flagged, key=lambda a: ("VALUE SWAP POS_UNRESOLVED IMAGE_PATCH SETONLY REORDER".split().index(a["verdict"]),
                                            a["unit"], a["symbol"])):
        if a["verdict"] in ("VALUE", "SWAP", "POS_UNRESOLVED", "IMAGE_PATCH"):
            print("%-7s %s %s (fuzzy %.2f)" % (a["verdict"], a["unit"], a["symbol"], a["fuzzy"]))
            for p in a["pos"]:
                print("        row %d %s: retail %s  ours %s" % (p["row"], p["op"], p["retail"], p["ours"]))
    print("wrote", args.out)


if __name__ == "__main__":
    main()
