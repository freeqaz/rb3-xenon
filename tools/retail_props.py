#!/usr/bin/env python3
"""Retail property-list extractor for RB3 (rb3-xenon).

WHAT IT DOES
------------
Every `SYNC_PROP*` in a `BEGIN_PROPSYNCS` block compiles to a guarded
function-static `Symbol` built from the property-name string literal:

        lis  r10, <static Symbol>@ha
        lis  r11, <string>@ha            (REFHI)
        addi r4,  r11, <string>@l        (REFLO)   <-- the NAME
        addi r3,  r10, <static Symbol>@l (REFLO)   <-- the `this`, in .data
        bl   ??0Symbol@@QAA@PBD@Z

So the ordered list of property names a RETAIL function enumerates can be read
straight out of the dtk-split TARGET obj, with no source and no map: for each
REL24 to `??0Symbol@@QAA@PBD@Z`, find what defines **r4** and decode the string
it points at in band.exe.  This is the only instrument we have that can see the
at-100 defect class, because property-name operands are *relocation args* and
objdiff's default ruler masks those (`report.rs` hard-sets them to None).

HISTORY / WHY THIS FILE EXISTS  (read before "simplifying" it)
--------------------------------------------------------------
This is the third build of this instrument (lane CQ-3 -> CR-4 -> CS-4).  The
first two versions took *the reloc at offset B-4* as the name and decoded its VA
as `va - 0x82000000`.  Both halves of that are wrong, and both fail SILENTLY:

  1. `va - 0x82000000` is the file offset for `.rdata`/`.pdata`/`BINKCONS` ONLY.
     `.text` is off by 0xb200 and `.data` by 0x12400.  Worse, a bad offset
     usually still lands *inside* the file, so you get `''` or garbage rather
     than an error.  Use the PE section table -- `va_to_off()` below.
     (CONTROL: over the .data operand VAs, the section-table mapper lands on 371
     well-formed MSVC TypeDescriptors and 66% coherent pointer words; the
     `va-0x82000000` rule lands on 1 and 6.3%.)

  2. "the reloc at B-4" is NOT the name.  Measured over all 1,995 target objs,
     11,999 REL24-to-Symbol-ctor sites:
        -  9,984 had a `.rdata` operand at B-4   (the old tool got these right)
        -    451 had a `.data`  operand at B-4   -> that is `r3`, the *`this`*
                 static Symbol object, or a `lwz r4,glob@l(rX)` dynamic load.
                 There is NO string at those addresses; the old tool decoded
                 `''` and dropped the property.
        -  1,565 had NO reloc at B-4 at all      -> the `addi r4,..@l` was
                 scheduled at -8/-12/-24.  The old tool emitted a literal "?".
     So 2,016 of 11,999 sites (16.8%) were wrong or invisible.

  3. `"??0Symbol@@" in name` also matches `??0Symbol@@QAA@XZ`, the *default*
     ctor, which has no string argument at all.

The fix is a small backward dataflow scan for the last definition of r4 before
the call, plus a real section mapper.  Do not replace it with a fixed offset.

CONTROLS (`--control`) -- and what each one CANNOT prove
--------------------------------------------------------
* `retail-vs-ours` on at-100 rows.  Those bodies are byte-identical to retail,
  so the extracted lists must agree.  ** This validates the VA->string decode
  ONLY.  It CANNOT validate the r4 tracker: identical bytes means the tracker
  sees identical instructions on both sides and agrees with itself. **
* `retail-vs-source` on at-100 rows -- the only control that can actually fail
  the tracker, because the source list is produced by a completely different
  mechanism (text).  Caveat: a regex cannot see `#ifdef HX_NATIVE`, so a handful
  of disagreements are expected and are listed rather than counted as failures.
* `misname` -- compare each row's retail list against a DIFFERENT symbol's
  compiled list (hard version: another SyncProperty in the same unit).  If a
  wrong name agrees as often as the right one the channel is vacuous.
"""
import argparse
import json
import os
import re
import struct
import sys

# ---------------------------------------------------------------- COFF/PE ---

REL24, REFHI, REFLO, PAIR = 0x0006, 0x0010, 0x0011, 0x0012
SYMBOL_CTOR_STR = "??0Symbol@@QAA@PBD@Z"   # Symbol::Symbol(const char*)


def _repo_root(start=None):
    p = os.path.abspath(start or os.path.dirname(os.path.abspath(__file__)))
    while p != "/":
        if os.path.isdir(os.path.join(p, "config", "45410914")):
            return p
        p = os.path.dirname(p)
    raise SystemExit("cannot locate repo root")


class Image:
    """band.exe with a REAL section table.  `va - 0x82000000` is .rdata-only."""

    def __init__(self, path):
        self.data = open(path, "rb").read()
        d = self.data
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe:pe + 4] != b"PE\0\0":
            raise SystemExit(f"{path}: not a PE")
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        szopt = struct.unpack_from("<H", d, pe + 20)[0]
        self.imagebase = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        off = pe + 24 + szopt
        self.sections = []
        for i in range(nsec):
            b = d[off + i * 40: off + (i + 1) * 40]
            self.sections.append({
                "name": b[:8].rstrip(b"\0").decode("ascii", "replace"),
                "vsize": struct.unpack_from("<I", b, 8)[0],
                "va": self.imagebase + struct.unpack_from("<I", b, 12)[0],
                "rawsize": struct.unpack_from("<I", b, 16)[0],
                "rawptr": struct.unpack_from("<I", b, 20)[0],
            })

    def section_of(self, va):
        for s in self.sections:
            if s["va"] <= va < s["va"] + s["vsize"]:
                return s
        return None

    def va_to_off(self, va):
        """File offset for `va`, or None if unmapped or in a zero-init tail."""
        s = self.section_of(va)
        if s is None:
            return None
        d = va - s["va"]
        return s["rawptr"] + d if d < s["rawsize"] else None

    def cstr(self, va, maxlen=512):
        """C string at `va`, or None if unmapped / not printable ASCII.

        ⚠ A ZERO-LENGTH string is a REAL RESULT, not a failure.  This returned
        None for `raw == b""` until lane CV-3, and that one line manufactured a
        whole artifact class: retail's `Symbol("")` (the NUL at .rdata 0x82000C55)
        decoded to None -> dropped from the list, while OUR side went through
        demangle_literal() which correctly returns "".  The lists then differed by
        a phantom extra entry and 5 rows read as at-100 defects.  CS-4 saw the
        5 rows and attributed them to "literal ours vs global char* retail";
        CT-4 proved that mechanism false for all 5 but left the bug in place.
        Fixed at the root here -- both sides now yield "".
        """
        off = self.va_to_off(va)
        if off is None:
            return None
        end = self.data.find(b"\0", off, off + maxlen)
        if end < 0:
            return None
        raw = self.data[off:end]
        if not raw:
            return ""
        try:
            s = raw.decode("ascii")
        except UnicodeDecodeError:
            return None
        return s if all(32 <= ord(c) < 127 for c in s) else None


# --------------------------------------------------- tiny PPC decode bits ---

def _op(w):
    return w >> 26


def _rt(w):
    return (w >> 21) & 31


def _ra(w):
    return (w >> 16) & 31


def _xo(w):
    return (w >> 1) & 0x3FF


def defines(w):
    """GPRs written by `w`, or None when the instruction is not modelled.

    Conservative on purpose: an unmodelled instruction returns None and the
    backward scan treats it as a BARRIER, so we under-resolve rather than
    silently attribute the wrong string.  (Count of barriers is reported.)
    """
    op = _op(w)
    if op in (7, 8, 12, 13, 14, 15):                 # mulli subfic addic. addi addis
        return {_rt(w)}
    if op in (24, 25, 26, 27, 28, 29):               # ori oris xori xoris andi. andis.
        return {_ra(w)}
    if op in (20, 21, 23):                           # rlwimi rlwinm rlwnm
        return {_ra(w)}
    if op in (32, 34, 40, 42, 58):                   # lwz lbz lhz lha ld -- RT
        return {_rt(w)}
    if op in (33, 35, 41, 43):                       # lwzu lbzu lhzu lhau
        return {_rt(w), _ra(w)}                      # ⚠ update form writes RA TOO
    if op in (48, 50):                               # lfs lfd -- FPR only
        return set()
    if op in (49, 51):                               # lfsu lfdu -- FPR + RA
        return {_ra(w)}
    if op in (36, 38, 44, 47, 52, 54, 62):           # stw stb sth stmw stfs stfd std
        return set()
    if op in (37, 39, 45, 53, 55):                   # stwu stbu sthu stfsu stfdu
        return {_ra(w)}                              # ⚠ update form writes RA
    if op in (59, 63):
        # ★ FP arithmetic (fadds/fmr/frsp/fcfid/...).  These write FPRs and can
        # NEVER touch a GPR, but returning None made them BARRIERS -- which is
        # how lane CV-3's image scanner lost 7 property lists: an `fmr f30,f1`
        # scheduled between `lis r11,hi` and `addi r4,r11,lo` truncated the
        # backward scan.  Conservatism is only safe when it costs nothing; here
        # it cost real coverage, silently, in the "list looks SHORTER" direction
        # -- i.e. it manufactures fake "our source has SURPLUS properties".
        return set()
    if op in (16, 18):                               # bc, b/bl
        return None if (w & 1) else set()            # lk=1 clobbers r3..r12
    if op == 11 or op == 10:                         # cmpi cmpli
        return set()
    if op == 31:
        xo = _xo(w)
        if xo in (444, 28, 60, 124, 316, 412, 476, 792, 824, 536, 24):  # logical/shift -> RA
            return {_ra(w)}
        if xo in (266, 40, 10, 8, 138, 234, 202, 235, 75, 11, 459, 491, 339, 371):
            return {_rt(w)}                          # add subf addc adde mullw divw mfspr
        if xo in (23, 87, 279, 343, 55, 119, 311, 375, 21, 53, 341):
            return {_rt(w)}                          # indexed loads
        if xo in (151, 215, 407, 439, 183, 247, 149, 181):
            return set()                             # indexed stores
        if xo in (0, 32, 4):                         # cmp cmpl tw
            return set()
        return None
    if op == 19:                                     # bclr/bcctr etc
        return None if (w & 1) else set()
    return None


def _simm(w):
    d = w & 0xFFFF
    return d - 0x10000 if d & 0x8000 else d


# ---------------------------------------------------------------- decoding --

_CSTR_MANGLE = re.compile(r"^\?\?_C@_[0-9A-Z]")


_MSVC_PUNCT = ",/\\:. \n\t'-"


def demangle_literal(name):
    """MSVC `??_C@_0<len>@<hash>@<text>?$AA@` -> <text>.

    ⚠ The escapes MUST be decoded.  MSVC encodes `/` as `?1`, `.` as `?4`,
    `:` as `?3` &c inside string-literal names.  Leaving them raw made 113
    RockCentral rows read as at-100 defects -- `entities?1linkcode?1get` vs
    retail's `entities/linkcode/get` -- when the two are the same string.
    A demangler that "mostly works" manufactures defects in exactly the rows
    whose property names are paths or URLs.
    """
    t = name[:-5] if name.endswith("?$AA@") else name.rstrip("@")
    t = t.rsplit("@", 1)[-1]
    out, i = [], 0
    while i < len(t):
        c = t[i]
        if c != "?":
            out.append(c)
            i += 1
            continue
        if i + 1 >= len(t):
            break
        n = t[i + 1]
        if n.isdigit():
            out.append(_MSVC_PUNCT[int(n)])
            i += 2
        elif n == "$" and i + 3 < len(t):
            hi, lo = t[i + 2], t[i + 3]
            if "A" <= hi <= "P" and "A" <= lo <= "P":
                out.append(chr((ord(hi) - 65) * 16 + (ord(lo) - 65)))
                i += 4
            else:
                out.append(n)
                i += 2
        elif "A" <= n <= "Z":
            out.append(chr(0xC1 + ord(n) - 65))
            i += 2
        elif "a" <= n <= "z":
            out.append(chr(0xE1 + ord(n) - 97))
            i += 2
        else:
            out.append(n)
            i += 2
    return "".join(out)


class Extractor:
    def __init__(self, root, image=None, bound_to_symbol=True, window=64):
        self.root = root
        self.img = image or Image(os.path.join(root, "orig/45410914/band.exe"))
        self.bound = bound_to_symbol
        self.window = window
        self._objs = {}
        self._boundcache = {}
        self._seccache = {}
        sys.path.insert(0, os.path.join(root, "scripts"))
        from unicorn_runner.coff import COFFParser  # noqa
        self._COFF = COFFParser
        self.stats = {"sites": 0, "resolved": 0, "dyn_global": 0, "dyn_computed": 0,
                      "barrier": 0, "no_def": 0, "bad_string": 0, "default_ctor": 0}

    def _section(self, p, path, si):
        """(relocs_at, blob, sites) for one section, cached.

        `sites` is the sorted list of (offset, callee) for every REL24 to a
        Symbol ctor.  Without this cache, scanning every symbol in a big
        multi-function `.text` re-walks all of its relocations once per symbol
        -- O(symbols x relocs), which made a whole-corpus pass unusable.
        """
        key = (path, si)
        v = self._seccache.get(key)
        if v is None:
            try:
                relocs = p.get_section_relocations(si)
                blob = p.get_section_data(si)
            except Exception:
                self._seccache[key] = False
                return None
            if not blob:
                self._seccache[key] = False
                return None
            rat, sites = {}, []
            for r in relocs:
                rat.setdefault(r["offset"], []).append((r["type"], r["symbol_name"]))
                if r["type"] == REL24 and "??0Symbol@@" in r["symbol_name"]:
                    sites.append((r["offset"], r["symbol_name"]))
            sites.sort()
            v = (rat, blob, sites)
            self._seccache[key] = v
        return v or None

    def _bounds(self, p, path):
        """section -> sorted body-end candidates.  Cached: recomputing this per
        symbol makes a whole-corpus scan O(symbols^2) per obj."""
        b = self._boundcache.get(path)
        if b is None:
            b = {}
            for n, o in p.symbol_map.items():
                if o.get("section", 0) > 0 and (n.startswith("__unwind$") or o.get("type") == 0x20):
                    b.setdefault(o["section"], []).append(o.get("value", 0))
            for k in b:
                b[k] = sorted(b[k])
            self._boundcache[path] = b
        return b

    def obj(self, path):
        if path not in self._objs:
            try:
                self._objs[path] = self._COFF(path) if os.path.exists(path) else None
            except Exception:
                self._objs[path] = None
        return self._objs[path]

    # ---- resolve the `const char*` argument of one Symbol-ctor call --------
    def _resolve_arg(self, blob, relocs_at, call_off, start, reg=4, depth=0):
        """Backward-scan for the last definition of `reg` before `call_off`.

        Returns (kind, payload) where kind is one of
          'literal'  payload = reloc symbol name that supplies the address
          'global'   payload = reloc symbol name of the loaded pointer variable
          'computed' payload = None    (runtime-built string)
          'barrier'  payload = None    (unmodelled instruction; gave up)
          'nodef'    payload = None    (no definition found in the window)
        """
        lo = max(start, call_off - self.window * 4)
        o = call_off - 4
        while o >= lo:
            w = struct.unpack_from(">I", blob, o)[0]
            dset = defines(w)
            if dset is None:
                return ("barrier", None)
            if reg in dset:
                op = _op(w)
                rl = relocs_at.get(o, [])
                lo_rel = next((n for t, n in rl if t == REFLO), None)
                if op in (14, 15) and lo_rel:          # addi/addis rD, rA, sym@l
                    return ("literal", lo_rel)
                if op in (32, 33) and lo_rel:          # lwz rD, glob@l(rA)
                    return ("global", lo_rel)
                if op == 31 and _xo(w) == 444 and _rt(w) == _ra(w) is not None:
                    src = (w >> 11) & 31               # mr rD, rS  (or rA,rS,rS)
                    if src != reg and depth < 4:
                        return self._resolve_arg(blob, relocs_at, o, start, src, depth + 1)
                    return ("computed", None)
                if op in (14, 15) and not lo_rel:
                    return ("computed", None)
                return ("computed", None)
            o -= 4
        return ("nodef", None)

    def _name_of(self, relsym):
        """Reloc symbol name -> property string, or None."""
        m = re.fullmatch(r"lbl_([0-9A-Fa-f]{8})", relsym)
        if m:                                   # dtk target obj: retail VA
            return self.img.cstr(int(m.group(1), 16))
        if _CSTR_MANGLE.match(relsym):          # our compiled obj: MSVC mangling
            return demangle_literal(relsym)
        return None

    # ---- ordered property slots for one symbol ---------------------------
    def slots(self, obj_path, symbol):
        """-> list of (kind, name) in call order, or None if symbol not present.

        kind == 'literal' with a decoded name is a real property.  Everything
        else is a slot we can SEE but cannot NAME -- reported, never silently
        dropped, because dropping one turns a correct list into a wrong one.
        """
        p = self.obj(obj_path)
        if p is None:
            return None
        s = p.symbol_map.get(symbol)
        if s is None or s.get("section", 0) <= 0:
            return None
        si = s["section"] - 1
        sec = self._section(p, obj_path, si)
        if sec is None:
            return None
        relocs_at, blob, sites = sec

        start, end = 0, len(blob)
        if self.bound:
            # ⚠ "the next symbol in this section" is a WRONG bound.  MSVC emits
            # internal EH state labels ($M89622 &c) INSIDE the function body, and
            # dtk emits `lbl_<VA>` branch targets inside it too; either truncates
            # the scan to a few bytes and the function reads as having NO
            # properties at all (measured: UIListMesh::SyncProperty, start=8,
            # $M89622 at 24 -> 0 slots on a 376 B function).
            # The real end of the body is the first /EHsc `__unwind$` funclet --
            # the same fact behind the "COMDAT raw_size is ~1.35x the body" trap.
            start = s.get("value", 0)
            bounds = self._bounds(p, obj_path)
            cands = bounds.get(s["section"], ())
            end = next((v for v in cands if v > start), len(blob))

        out = []
        import bisect
        lo = bisect.bisect_left(sites, (start, ""))
        hi = bisect.bisect_left(sites, (end, ""))
        for off, nm in sites[lo:hi]:
            if True:
                if nm != SYMBOL_CTOR_STR:
                    self.stats["default_ctor"] += 1     # e.g. ??0Symbol@@QAA@XZ
                    continue
                self.stats["sites"] += 1
                kind, payload = self._resolve_arg(blob, relocs_at, off, start)
                if kind == "literal":
                    nmstr = self._name_of(payload)
                    if nmstr is None:
                        self.stats["bad_string"] += 1
                        out.append(("unnamed", payload))
                    else:
                        self.stats["resolved"] += 1
                        out.append(("literal", nmstr))
                elif kind == "global":
                    self.stats["dyn_global"] += 1
                    out.append(("global", payload))
                else:
                    self.stats[{"computed": "dyn_computed", "barrier": "barrier",
                                "nodef": "no_def"}[kind]] += 1
                    out.append((kind, None))
        return out

    def props(self, obj_path, symbol):
        """Resolved property names only (legacy CQ-3 shape)."""
        sl = self.slots(obj_path, symbol)
        return None if sl is None else [n for k, n in sl if k == "literal"]


# ------------------------------------------------- whole-image scanner ------

SYMBOL_CTOR_VA = 0x827C0728        # ??0Symbol@@QAA@PBD@Z in retail band.exe


class ImageScanner:
    """Pin-independent, map-independent retail property census (lane CV-3).

    `Extractor` can only see a function that lives in a unit with a `.text` pin,
    because it reads dtk-split TARGET objs.  This class reads band.exe directly:
    every `bl 0x827C0728` in `.text`, backward-resolved for r4, grouped into
    functions by `.pdata`.  It answers "what is the TRUE denominator" without
    trusting splits.txt, the symbol map, or objdiff pairing.

    ⚠ It is NOT a replacement for `Extractor`.  It has no notion of OUR side, so
    it cannot find a defect on its own -- it sizes the population and supplies
    retail lists for functions that have no target obj at all.

    MEASURED 2026-08-02 on band.exe: 11,990 sites / 3,556 owning functions, of
    which 11,844 sites (98.78%) fall inside a pinned `.text` range.  So the
    "pinned" filter -- the third of the finder's three filters -- is NOT a
    blind spot; the blind spots are `named` and `at-100`.
    """

    def __init__(self, image, ctor_va=SYMBOL_CTOR_VA):
        self.img = image
        self.ctor = ctor_va
        self.text = next(s for s in image.sections if s["name"] == ".text")
        self.blob = image.data[self.text["rawptr"]:
                               self.text["rawptr"] + self.text["rawsize"]]
        self.base = self.text["va"]
        self.starts = self._pdata_starts()
        self.stats = {"sites": 0, "literal": 0, "dynamic": 0, "unresolved": 0,
                      "bad_string": 0}

    def _pdata_starts(self):
        pd = next((s for s in self.img.sections if s["name"] == ".pdata"), None)
        if pd is None:
            return []
        out = []
        for i in range(pd["rawsize"] // 8):
            b = struct.unpack_from(">I", self.img.data, pd["rawptr"] + i * 8)[0]
            if b:
                out.append(b)
        out.sort()
        return out

    def owner(self, va):
        """`.pdata` BeginAddress covering `va`.

        ⚠ `.pdata`-absence is NOT a 'not a function' test on this binary (see
        CLAUDE.md band.exe read traps) -- this is a grouping key, nothing more.
        """
        i = _bisect.bisect_right(self.starts, va) - 1
        return self.starts[i] if i >= 0 else None

    def call_sites(self):
        """Every `bl <ctor>` VA in `.text`, ascending."""
        out, blob, base = [], self.blob, self.base
        for off in range(0, len(blob) - 3, 4):
            w = struct.unpack_from(">I", blob, off)[0]
            if (w >> 26) == 18 and (w & 3) == 1:      # b, LK=1, AA=0
                d = w & 0x03FFFFFC
                if d & 0x02000000:
                    d -= 0x04000000
                if base + off + d == self.ctor:
                    out.append(base + off)
        return out

    def _word(self, va):
        return struct.unpack_from(">I", self.blob, va - self.base)[0]

    def _resolve_r4(self, call_va, window=64):
        """Backward-scan the LINKED image for r4's `lis`/`addi` pair.

        Unlike the obj-level tracker there are no relocations here; the address
        is materialised as `addis rX,0,hi` + `addi r4,rX,lo`.  Returns a VA, or
        None when r4 comes from a load/computation (genuinely dynamic).
        """
        lo_imm = None
        need = 4
        va = call_va - 4
        stop = max(self.base, call_va - window * 4)
        while va >= stop:
            w = self._word(va)
            dset = defines(w)
            if dset is None:
                return None
            if need in dset:
                op, ra = _op(w), _ra(w)
                if op == 14:                                  # addi rD,rA,simm
                    if ra == 0:
                        return _simm(w) & 0xFFFFFFFF
                    if lo_imm is not None:
                        return None                           # two addis, give up
                    lo_imm, need = _simm(w), ra
                    va -= 4
                    continue
                if op == 15:                                  # addis rD,rA,simm
                    if ra != 0:
                        return None
                    hi = (_simm(w) << 16) & 0xFFFFFFFF
                    return (hi + (lo_imm or 0)) & 0xFFFFFFFF
                return None
            va -= 4
        return None

    def census(self):
        """-> {owner_va: [(site_va, kind, name_or_None), ...]} in call order."""
        out = {}
        for site in self.call_sites():
            self.stats["sites"] += 1
            va = self._resolve_r4(site)
            if va is None:
                self.stats["dynamic"] += 1
                rec = ("dynamic", None)
            else:
                s = self.img.cstr(va)
                if s is None:
                    self.stats["bad_string"] += 1
                    rec = ("unnamed", va)
                else:
                    self.stats["literal"] += 1
                    rec = ("literal", s)
            o = self.owner(site)
            if o is None:
                self.stats["unresolved"] += 1
                continue
            out.setdefault(o, []).append((site,) + rec)
        return out

    def lists(self):
        """-> {owner_va: [literal names in call order]} (owners with >=1 name)."""
        out = {}
        for o, sl in self.census().items():
            names = [n for _, k, n in sl if k == "literal"]
            if names:
                out[o] = names
        return out


import bisect as _bisect  # noqa: E402  (used by ImageScanner.owner)


# ------------------------------------------------------------------ CLI -----

def load_units(root):
    od = json.load(open(os.path.join(root, "objdiff.json")))
    return od["units"]


def cmd_dump(a, ex, root):
    units = {u["name"]: u for u in load_units(root)}
    u = units.get(a.unit)
    if u is None:
        raise SystemExit(f"no unit {a.unit!r}")
    for which, key in (("TARGET", "target_path"), ("OURS", "base_path")):
        p = u.get(key)
        if not p:
            continue
        sl = ex.slots(os.path.join(root, p), a.symbol)
        print(f"{which:7s} {p}")
        if sl is None:
            print("        (symbol absent)")
            continue
        for i, (k, n) in enumerate(sl):
            print(f"   {i:3d} {k:9s} {n}")


def cmd_verify(a, ex, root):
    """Compare retail vs ours for one symbol on BOTH channels.

    ★ WHY TWO CHANNELS.  Counting `Symbol` ctor calls alone has a KNOWN
    contamination direction (lane CQ-3): with /Ob2, MSVC sometimes INLINES
    `Symbol::Symbol(const char*)` where retail called it out of line, so a
    property present in both can look "missing from retail" and you delete
    correct code.  The `PropSync` call is NOT inlined away -- every SYNC_PROP
    emits one.  So:
        fewer Symbol ctors in retail  AND  fewer PropSync calls in retail
            => the property really is absent from retail  (safe to remove)
        fewer Symbol ctors but the SAME PropSync count
            => retail merely inlined the ctor         (DO NOT remove)
    """
    units = {u["name"]: u for u in load_units(root)}
    u = units.get(a.unit)
    if u is None:
        raise SystemExit(f"no unit {a.unit!r}")
    out = {}
    for which, key in (("RETAIL", "target_path"), ("OURS", "base_path")):
        p = os.path.join(root, u.get(key) or "")
        sl = ex.slots(p, a.symbol)
        c = ex.obj(p)
        calls = []
        if c is not None and sl is not None:
            s = c.symbol_map[a.symbol]
            si = s["section"] - 1
            blob = c.get_section_data(si)
            start = s.get("value", 0)
            end = next((v for v in ex._bounds(c, p).get(s["section"], ()) if v > start),
                       len(blob))
            for r in c.get_section_relocations(si):
                if r["type"] == REL24 and start <= r["offset"] < end \
                        and "PropSync" in r["symbol_name"]:
                    calls.append(r["symbol_name"])
        out[which] = (sl, calls)
        print(f"{which}")
        print(f"   Symbol ctor slots ({len(sl or [])}): {sl}")
        print(f"   PropSync calls    ({len(calls)}):")
        for c2 in calls:
            print(f"        {c2[:100]}")
    (ts, tc), (bs, bc) = out["RETAIL"], out["OURS"]
    print("\nVERDICT")
    if ts is None or bs is None:
        print("   symbol missing on one side -- attribution issue, not properties")
    elif [n for k, n in ts if k == "literal"] == [n for k, n in bs if k == "literal"] \
            and len(tc) == len(bc):
        print("   lists AGREE on both channels")
    elif len(tc) < len(bc):
        print(f"   retail has FEWER PropSync calls ({len(tc)} vs {len(bc)}) => our source"
              f" carries SURPLUS properties.  Not an inlining artifact.")
    elif len(tc) == len(bc) and len(ts or []) < len(bs or []):
        print("   same PropSync count but fewer retail Symbol ctors => RETAIL INLINED"
              " the ctor.  DO NOT remove the property.")
    else:
        print(f"   retail has MORE PropSync calls ({len(tc)} vs {len(bc)}) => our source"
              f" is MISSING properties.")


def cmd_index(a, ex, root):
    """Map-independent index: prop-list -> [(unit, symbol)] over all target objs."""
    out, nobj, nsym = {}, 0, 0
    for u in load_units(root):
        tp = u.get("target_path")
        if not tp:
            continue
        p = os.path.join(root, tp)
        if not os.path.exists(p):
            continue
        c = ex.obj(p)
        if c is None:
            continue
        nobj += 1
        for name, s in list(c.symbol_map.items()):
            if s.get("section", 0) <= 0 or name.startswith((".", "except_data", "__")):
                continue
            try:
                sl = ex.slots(p, name)
            except Exception:
                continue
            if not sl:
                continue
            names = [n for k, n in sl if k == "literal"]
            if not names:
                continue
            nsym += 1
            rec = out.setdefault("|".join(names), [])
            rec.append([u["name"], name, sum(1 for k, _ in sl if k != "literal")])
        ex._objs.pop(p, None)          # keep memory bounded
        ex._boundcache.pop(p, None)
        for k in [k for k in ex._seccache if k[0] == p]:
            ex._seccache.pop(k, None)
    json.dump(out, open(a.out, "w"), indent=0)
    print(f"objs={nobj} prop-bearing-symbols={nsym} distinct-lists={len(out)} -> {a.out}")
    print("extractor stats:", json.dumps(ex.stats))


SRC_RE = re.compile(r"^\s*SYNC_PROP\w*\(\s*([A-Za-z_]\w*)", re.M)


def source_props(root, src_path, cls):
    try:
        txt = open(os.path.join(root, src_path)).read()
    except OSError:
        return None
    i = txt.find("BEGIN_PROPSYNCS(%s)" % cls)
    if i < 0:
        return None
    j = txt.find("END_PROPSYNCS", i)
    return SRC_RE.findall(txt[i:j])


def at100_rows(root):
    rep = json.load(open(os.path.join(root, "build/45410914/report.json")))
    rows = []
    for u in rep["units"]:
        for f in u.get("functions", []):
            # NB: fuzzy_match_percent / match_percent_normalized are TOP-LEVEL
            # keys on the function record, NOT under `measures`.
            pct = f.get("match_percent_normalized")
            if pct is None:
                pct = f.get("fuzzy_match_percent", 0.0)
            if "SyncProperty" in f["name"] and int(f["size"]) > 16:
                rows.append((u["name"], f["name"], int(f["size"]), float(pct)))
    return rows


def cmd_control(a, ex, root):
    units = {u["name"]: u for u in load_units(root)}
    rows = at100_rows(root)
    hundred = [r for r in rows if r[3] >= 100.0]
    print(f"SyncProperty rows: {len(rows)}   at-100: {len(hundred)}")

    if a.control in ("all", "retail-vs-ours"):
        ok = bad = skip = 0
        ex_bad = []
        for unit, sym, size, pct in hundred:
            u = units.get(unit) or {}
            t = ex.props(os.path.join(root, u.get("target_path", "")), sym)
            b = ex.props(os.path.join(root, u.get("base_path", "")), sym)
            if t is None or b is None:
                skip += 1
                continue
            if t == b:
                ok += 1
            else:
                bad += 1
                if len(ex_bad) < 6:
                    ex_bad.append((sym, t, b))
        print(f"\n[retail-vs-ours]  agree={ok} DISAGREE={bad} skipped={skip}")
        print("  NOTE: validates the VA->string decode ONLY.  At-100 bodies are")
        print("  byte-identical, so the r4 tracker sees the same instructions on")
        print("  both sides and CANNOT be falsified by this control.")
        for s, t, b in ex_bad:
            print(f"   {s}\n     retail={t}\n     ours  ={b}")

    if a.control in ("all", "retail-vs-source"):
        ok = bad = skip = 0
        diffs = []
        for unit, sym, size, pct in hundred:
            u = units.get(unit) or {}
            sp = (u.get("metadata") or {}).get("source_path")
            m = re.match(r"\?SyncProperty@(\w+)@@", sym)
            if not sp or not m:
                skip += 1
                continue
            src = source_props(root, sp, m.group(1))
            t = ex.props(os.path.join(root, u.get("target_path", "")), sym)
            if src is None or t is None:
                skip += 1
                continue
            if src == t:
                ok += 1
            else:
                bad += 1
                diffs.append((m.group(1), t, src))
        print(f"\n[retail-vs-source]  agree={ok} DISAGREE={bad} skipped={skip}")
        print("  This is the ONLY control that can falsify the r4 tracker (the")
        print("  source list comes from text, an independent mechanism).")
        print("  Expect a few disagreements: a regex cannot see #ifdef HX_NATIVE.")
        for cls, t, s in diffs[:12]:
            print(f"   {cls}\n     retail={t}\n     source={s}")

    if a.control in ("all", "misname"):
        import random
        recs = []
        for unit, sym, size, pct in hundred:
            u = units.get(unit) or {}
            t = ex.props(os.path.join(root, u.get("target_path", "")), sym)
            b = ex.props(os.path.join(root, u.get("base_path", "")), sym)
            if t and b:
                recs.append((unit, sym, t, b))
        random.seed(4)
        byunit = {}
        for i, r in enumerate(recs):
            byunit.setdefault(r[0], []).append(i)
        truth = sum(1 for _, _, t, b in recs if t == b)
        mis = 0
        for i, (unit, sym, t, b) in enumerate(recs):
            pool = [j for j in byunit[unit] if j != i] or [j for j in range(len(recs)) if j != i]
            if t == recs[random.choice(pool)][3]:
                mis += 1
        print(f"\n[misname]  rows={len(recs)}  TRUTH agree={truth}  MISNAME agree={mis}")
        if recs:
            print("  separation: " + ("INFINITE (a wrong name pays nothing)" if mis == 0
                                       else f"{(truth/len(recs))/(mis/len(recs)):.1f}x"))
    print("\nextractor stats:", json.dumps(ex.stats))


def cmd_stats(a, ex, root):
    """Site-class census over every target obj -- the coverage number."""
    for u in load_units(root):
        tp = u.get("target_path")
        if not tp:
            continue
        p = os.path.join(root, tp)
        if not os.path.exists(p):
            continue
        c = ex.obj(p)
        if c is None:
            continue
        for name, s in list(c.symbol_map.items()):
            if s.get("section", 0) <= 0 or name.startswith((".", "except_data", "__")):
                continue
            try:
                ex.slots(p, name)
            except Exception:
                pass
        ex._objs.pop(p, None)
        ex._boundcache.pop(p, None)
        for k in [k for k in ex._seccache if k[0] == p]:
            ex._seccache.pop(k, None)
    t = ex.stats
    tot = t["sites"] or 1
    print(json.dumps(t, indent=2))
    print(f"resolved {t['resolved']}/{t['sites']} = {100.0*t['resolved']/tot:.2f}%")


def cmd_scan(a, ex, root):
    """Whole-binary census + the control that can FALSIFY the image scanner.

    The scanner (linked-image lis/addi) and the Extractor (obj relocations) are
    INDEPENDENT mechanisms reading independent artifacts.  On a pinned, named
    retail symbol they must produce the same list; a disagreement falsifies one
    of them.  This is deliberately not the `retail-vs-ours` shape, which cannot
    fail (identical bytes -> the tracker agrees with itself).
    """
    sc = ImageScanner(ex.img)
    lists = sc.lists()
    print("[image scan] " + json.dumps(sc.stats))
    print(f"  owning functions with >=1 named property: {len(lists)}")

    # pin coverage
    rng, cur = [], None
    for ln in open(os.path.join(root, "config/45410914/splits.txt")):
        m = re.match(r"^(\S.*):\s*$", ln)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r"\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", ln)
        if m and cur:
            rng.append((int(m.group(1), 16), int(m.group(2), 16)))
    rng.sort()
    import bisect
    st = [r[0] for r in rng]
    inpin = 0
    for o in lists:
        i = bisect.bisect_right(st, o) - 1
        if i >= 0 and o < rng[i][1]:
            inpin += 1
    print(f"  of those, inside a pinned .text range: {inpin} "
          f"({100.0 * inpin / max(1, len(lists)):.2f}%)  UNPINNED: {len(lists) - inpin}")

    if a.control:
        units = {u["name"]: u for u in load_units(root)}
        addr = {}
        try:
            sm = json.load(open(os.path.join(root, "scripts/target_symbol_map.json")))
            for k, v in sm.items():
                # ⚠ the map carries non-address bookkeeping keys
                # (e.g. `_splits_fill_unresolved_comment`) -- int(k,16) throws.
                if re.fullmatch(r"0x[0-9A-Fa-f]{8}", k) and isinstance(v, str):
                    addr[v] = int(k, 16)
        except OSError:
            pass
        ok = bad = skip = 0
        bads = []
        for u in load_units(root):
            tp = u.get("target_path")
            if not tp or not os.path.exists(os.path.join(root, tp)):
                continue
            p = os.path.join(root, tp)
            c = ex.obj(p)
            if c is None:
                continue
            for name in list(c.symbol_map):
                if name.startswith((".", "except_data", "__", "$")) or name not in addr:
                    continue
                try:
                    t = ex.props(p, name)
                except Exception:
                    continue
                if not t:
                    continue
                s = lists.get(addr[name])
                if s is None:
                    skip += 1
                elif s == t:
                    ok += 1
                else:
                    bad += 1
                    if len(bads) < 8:
                        bads.append((name, addr[name], t, s))
            ex._objs.pop(p, None)
            ex._boundcache.pop(p, None)
            for k in [k for k in ex._seccache if k[0] == p]:
                ex._seccache.pop(k, None)
        print(f"\n[control image-scan vs obj-extractor]  agree={ok} DISAGREE={bad} "
              f"no-pdata-owner={skip}")
        print("  Two independent mechanisms (linked-image lis/addi vs COFF relocs).")
        print("  Unlike retail-vs-ours, this control CAN fail.")
        for n, va, t, s in bads:
            print(f"   {n[:70]} @ {va:#x}\n     obj  ={t}\n     image={s}")
    if a.out:
        json.dump({f"{k:#010x}": v for k, v in lists.items()},
                  open(a.out, "w"), indent=0)
        print(f"\nwrote {a.out}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=None, help="repo/worktree root")
    ap.add_argument("--no-bound", action="store_true",
                    help="scan the whole COMDAT section instead of the symbol extent "
                         "(this is what CQ-3/CR-4 did)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("dump", help="show retail vs our slots for one symbol")
    d.add_argument("unit"); d.add_argument("symbol")
    i = sub.add_parser("index", help="build the map-independent prop-list index")
    i.add_argument("--out", default="propindex.json")
    v = sub.add_parser("verify", help="retail vs ours on BOTH channels for one symbol")
    v.add_argument("unit"); v.add_argument("symbol")
    c = sub.add_parser("control", help="run the falsification controls")
    c.add_argument("--control", default="all",
                   choices=["all", "retail-vs-ours", "retail-vs-source", "misname"])
    sub.add_parser("stats", help="site-class census over all target objs")
    sc = sub.add_parser("scan", help="pin-independent whole-binary census (CV-3)")
    sc.add_argument("--control", action="store_true",
                    help="cross-check the image scanner against the obj extractor")
    sc.add_argument("--out", default=None, help="write {owner_va: [names]} JSON")
    a = ap.parse_args()
    root = a.root or _repo_root()
    ex = Extractor(root, bound_to_symbol=not a.no_bound)
    {"dump": cmd_dump, "index": cmd_index, "control": cmd_control,
     "verify": cmd_verify, "stats": cmd_stats, "scan": cmd_scan}[a.cmd](a, ex, root)


if __name__ == "__main__":
    main()
