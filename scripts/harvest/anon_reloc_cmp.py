#!/usr/bin/env python3
"""laneBW1 — RELOC-RESOLVING comparator for the anonymous-target naming gate.

WHY THIS EXISTS
---------------
`objdiff-core/src/diff/mod.rs:1410 pair_funclets_by_bytes` is the only path by
which an *anonymous* target symbol can pair, and it needs `is_funclet_like` on
BOTH sides. So an anonymous target **body** whose counterpart in our object
carries a normal mangled name can never pair at any similarity — it scores
exactly 0.0 and is invisible to every near-miss scanner. Naming the target
symbol in `scripts/target_symbol_map.json` is the entire fix (lane BV-4).

BV-4 selected those names by **masked-byte equality**: mask every relocated word
on both sides and compare. That selector is *reloc-masked by construction* — two
bodies that differ only in which vtable they store, which string they load, or
which function they call are byte-identical once the relocated words are zeroed.
Of BV-4's 177 proposals, 25 were dropped at landing for exactly that reason.
Taking them would have been relocated FALSE CREDIT: a byte-perfect anon body
scores 100% purely on whether its unit supplies *the mapped name*, so a wrong
name manufactures a match out of nothing.

This tool recovers what the masking destroyed, and it does so from the
**decompressed retail image** (`orig/45410914/band.exe`) rather than from the
`lbl_` index, so any VA in the binary can be dereferenced — not just the ones
dtk happened to emit a label for.

    base COMDAT reloc symbol NAME --resolve--> VA     ==? target operand VA
                                 --decode---> bytes   ==? bytes at that VA
                                 --RTTI-----> class   ==? class at that vtable

CHANNELS (per shared relocation offset)
  map      base name -> VA via target_symbol_map.json + symbols.txt
  name     target operand is itself a symbolic name (helpers) -> string compare
  str      ??_C@_0.. literal  -> demangled bytes vs retail bytes at the VA
  rtti0    ??_R0<type>@8      -> ".<type>\0" at +8 vs retail bytes
  const    __real@/__xmm@     -> constant bytes vs retail bytes
  vtable   ??_7<Class>@@6B..  -> follow retail (VA-4) COL -> +12 typedesc -> +8
                                 ASCII ".?AV<Class>@@" and compare the class  [NEW]
  data     any other base symbol DEFINED in the base obj with initialised
           content -> compare its non-relocated bytes against retail          [NEW]

  tt       target-vs-target: for a CONTESTED name (already mapped at another
           VA) compare the two retail bodies with every operand resolved to an
           absolute. Map-independent; the only channel that needs no oracle.  [NEW]

  branch   an external branch dtk emitted NO usable relocation token for,
           decoded from the ENCODING. Adjudicated as CODE: body identity of the
           callee (tree-wide code index) against retail at the decoded VA, then
           the bindings oracle. Never the map — see laneBY-2 below.        [BY2]

INHERITED TRAPS THIS TOOL OBEYS
  * A raw byte compare of two VAs **inverts** the verdict — PC-relative `bl`
    displacements must be resolved to absolutes first
    (`project_map_defect_channels_2026-07-29`). 24 of the 25 dropped rows have
    *identical size*, which is the signature of displacement difference, not of
    sameness.
  * "Same mangled name at two VAs ⇒ one is false" is REFUTED: a normalized scan
    of all 77,986 .text functions found 2,129 identical-body groups spanning
    11,932 VAs. Duplicate bodies at distinct addresses are ordinary here, so
    `tt == TWIN` is an ACCEPT, not a rejection.
  * `agree` counts shared boilerplate (reloc_disc/README.md §laneBV3.4). An
    OBJ_SET_TYPE body agrees with *any* SetType body on the "types" literal, the
    Symbol ctor and its own guard. Evidence is therefore counted per *channel*
    and the class-bearing channels (vtable/str/rtti0) are reported separately.

MEASURED (laneBW1, 2026-07-30, worktree baseline 41116/1510/39606/34.685703)
  Regression fixture A -- BV-4's 25 dropped rows, all must be refused:
      SHIP 0 / 25.  ★ But note WHY: all 25 are CONTESTED by construction, and
      the contested clause does the work. Base-vs-target evidence alone refuses
      only 11 of 25, so fixture A does NOT demonstrate power on uncontested rows.
  Regression fixture B -- the 152 rows that landed clean on main:
      130 SHIP, 20 UNDECIDED (no resolvable relocation at all: nrel == 0),
      1 REJECT, 1 NO_UNIT.
  Truth-ablation control (`control`, population = 5,570 established pairings
  that have >= 2 byte-perfect candidates, i.e. where a discriminator is
  actually required):
      POS (truth present)  3,688 picks, 0 wrong  -> precision 100.00%
      NEG (truth ablated)    591 plants /5,570   -> 9.59%
        of which 364 are BENIGN twins (same code AND same referents: the name
        is wrong but the emitted bytes still match retail exactly, so it is a
        naming error, not metric inflation) and 227 are HARMFUL.
      Harmful rate among accepted rows:  agree>=1 5.80% | >=3 3.30% | >=4 2.41%.
      ⇒ ship gate `--evidence 4`, chosen on the CONTROL, not on the metric, to
        sit at the ~2.66% level reloc_disc/laneBU4 accepted for its own channel.
  Sweep of the 667 remaining byte-perfect rows: 42 shipped, and a whole-binary
  same-split A/B measured exactly +42 matched / +0 masked_equal / +42 honest /
  +0.054745 pp (= 5,792 B, a 1:1 byte attribution). All 42 read fuzzy == 100.

laneBY-2 (2026-07-30) — THE UNLABELLED-BRANCH FIX. `subcommand selftest`.
  Defect: a one-sided relocation offset was scored a SHAPE contra (=> REJECT)
  regardless of WHICH side was missing. When the missing side is the TARGET
  that is unsound — absence of a dtk label is not absence of a branch.

  Sized on the 19,048 established masked-equal pairings (all known-correct, so
  every REJECT over this population is false by construction):
    * 25 pairings (0.13%), 29 offsets, carried an unlabelled external branch.
      All 25 read REJECT before; all 25 read ACCEPT after.
    * whole-population REJECTs 83 -> 58 (-30%).
    * channel census after:  bind 124,852 | shape_t 68 | shape 38 | branch 28.
      ➡ `shape_t` (target-missing, no decodable branch) is now the single
        largest residual false-reject cause and is the obvious follow-on.
  Precision gates (truth-ablation `control`, same worktree, before -> after):
    * POS precision 100.00% -> 100.00% (3,733 correct, 0 wrong, UNCHANGED).
    * NEG harmful plants 221 -> 222 (+1 / 6,200 = +0.016pp). The single new
      plant is 0x82341190 SyncProperty BandLabel-vs-HamLabel — the documented
      sibling-twin ambiguity, not a new error class; with truth PRESENT the
      same row refuses as AMBIGUOUS (POS no_survivor 237 -> 236, ambiguous
      1641 -> 1642), i.e. it never picks wrong.
    * `pool` output is BYTE-IDENTICAL before/after: `xbranch` is deliberately
      kept out of `relocs` so `masked_equal` — and therefore candidate
      selection and every calibrated number above — is untouched.
  Yield: NONE, measured not assumed. The 628-row byte-perfect sweep is
  SHIP=0 before AND after (identical breakdown). This lane fixes the REJECT
  ACCOUNTING; it unlocked no new map rows.
  ⚠ Not fixed, deliberately: `tt_compare` still byte-compares `masked`, and an
  unlabelled branch word is NOT masked, so two ICF twins at different VAs
  carry different PC-relative displacements and read DIFF. That feeds the
  CONTESTED clause, which is what refuses all 25 of fixture A — loosening it
  is the false-credit direction and needs its own control run.

Read-only with respect to the repo: emits JSON, never edits the map.
"""
import argparse
import importlib.util
import json
import os
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent


def _load(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


# ---------------------------------------------------------------------------
# Retail image: VA -> bytes, via the PE section table.
# ---------------------------------------------------------------------------
class Retail:
    """Decompressed retail PE (`orig/45410914/band.exe`).

    ⚠ `va - 0x82000000` is NOT a valid file offset in general — `.text` sits at
    RVA 0x270000 but raw 0x264E00, a 0xB200 skew
    (`project_bandexe_read_traps_2026-07-29`). Always go through the section
    table. Self-test anchor: off(0x824DAAD0) == 0x004CF8D0.
    """

    def __init__(self, path: Path):
        d = self.d = path.read_bytes()
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe:pe + 4] != b"PE\0\0":
            raise ValueError(f"{path}: not a PE")
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        self.imagebase = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        self.secs = []
        for i in range(nsec):
            o = pe + 24 + optsz + i * 40
            self.secs.append((
                d[o:o + 8].rstrip(b"\0").decode("latin1"),
                struct.unpack_from("<I", d, o + 12)[0],   # rva
                struct.unpack_from("<I", d, o + 8)[0],    # vsize
                struct.unpack_from("<I", d, o + 20)[0],   # praw
                struct.unpack_from("<I", d, o + 16)[0]))  # rawsize
        if self.off(0x824DAAD0) != 0x004CF8D0:
            raise ValueError("band.exe anchor self-test failed — wrong image?")

    def off(self, va):
        r = va - self.imagebase
        for _nm, rva, vsz, praw, rsz in self.secs:
            if rva <= r < rva + max(vsz, rsz):
                f = praw + (r - rva)
                # .bss-like (praw==0) and virtual tail have no file content
                return f if praw and f < praw + rsz else None
        return None

    def read(self, va, n):
        o = self.off(va)
        if o is None:
            return None
        b = self.d[o:o + n]
        return b if len(b) == n else None

    def u32(self, va):
        b = self.read(va, 4)
        return struct.unpack(">I", b)[0] if b else None   # PowerPC: big endian

    def cstr(self, va, cap=192):
        o = self.off(va)
        if o is None:
            return None
        e = self.d.find(b"\0", o, o + cap)
        return self.d[o:e if e >= 0 else o + cap].decode("latin1", "replace")

    def rtti_class(self, vtable_va):
        """vtable VA -> mangled class name from the MSVC Complete Object Locator.

        Layout: the COL pointer sits at vtable-4; COL+12 is the type descriptor;
        type descriptor +8 holds '.?AV<Class>@@' inline. Validated on
        0x8203231c -> '.?AVUIListCustomTemplate@@'.
        """
        col = self.u32(vtable_va - 4)
        if not col or not (self.imagebase <= col < self.imagebase + 0x1000000):
            return None
        td = self.u32(col + 12)
        if not td or not (self.imagebase <= td < self.imagebase + 0x1000000):
            return None
        s = self.cstr(td + 8)
        return s if s and s.startswith(".?A") else None


# ---------------------------------------------------------------------------
# Name helpers
# ---------------------------------------------------------------------------
_VT_RX = re.compile(r"^\?\?_7(.+?)@@6B")
_TD_RX = re.compile(r"^\.\?A[VU](.+)$")
HELPER_RX = re.compile(r"^__(save|rest)(gpr|fpr|vmx|gprlr|fprlr)")
PPC_PAIR = 0x12          # IMAGE_REL_PPC_PAIR: "symbol index" is really an addend


def decode_branch(word, pc):
    """PC-relative branch target from the ENCODING, or None if not a branch.

    opcode 18 = b/bl/ba/bla (26-bit LI), opcode 16 = bc/bcl (14-bit BD).
    Absolute forms (AA=1) and non-branches return None.
    """
    w = struct.unpack(">I", word)[0]
    op = w >> 26
    if op == 18:
        d = w & 0x03FFFFFC
        if d & 0x02000000:
            d -= 0x04000000
    elif op == 16:
        d = w & 0x0000FFFC
        if d & 0x8000:
            d -= 0x10000
    else:
        return None
    if w & 2:            # AA=1: absolute address, not PC-relative
        return d & 0xFFFFFFFF
    return (pc + d) & 0xFFFFFFFF


def vtable_class(name):
    m = _VT_RX.match(name)
    return m.group(1) if m else None


def typedesc_class(s):
    m = _TD_RX.match(s or "")
    if not m:
        return None
    c = m.group(1)
    return c[:-2] if c.endswith("@@") else c


# ---------------------------------------------------------------------------
# Object parsing (code COMDATs with relocs + initialised data symbols)
# ---------------------------------------------------------------------------
IMAGE_SCN_CNT_CODE = 0x20


def parse_obj(R, path):
    """-> (code, data) where
       code: [ {name,size,masked,relocs:[(off,name,type)],relocoffs:set} ]
       data: {name: {'bytes':b, 'relocoffs':set}}   (initialised sections only)
    """
    raw, secs, syms = R.parse_coff_full(path)
    bysec = defaultdict(list)
    for sy in syms:
        if sy["secn"] > 0 and sy["secn"] in secs and sy["sc"] in (2, 3):
            bysec[sy["secn"]].append(sy)
    code, data = [], {}
    for secn, members in bysec.items():
        s = secs[secn]
        if s["praw"] == 0:
            continue
        members = sorted(members, key=lambda x: x["val"])
        for k, sy in enumerate(members):
            start = sy["val"]
            end = members[k + 1]["val"] if k + 1 < len(members) else s["raw_size"]
            if end <= start:
                continue
            body = bytearray(raw[s["praw"] + start:s["praw"] + end])
            rl, roff = [], set()
            for rva, rnm, rtyp in s["relocs"]:
                if start <= rva < end:
                    off = rva - start
                    roff.add(off)
                    for b in range(4):
                        if off + b < len(body):
                            body[off + b] = 0
                    rl.append((off, rnm, rtyp))
            rl.sort()
            if s["chars"] & IMAGE_SCN_CNT_CODE:
                if sy["typ"] == 0x20:
                    code.append(dict(name=sy["name"], size=end - start,
                                     masked=bytes(body), relocs=rl,
                                     relocoffs=roff))
            elif sy["name"] not in data:
                data[sy["name"]] = dict(
                    bytes=raw[s["praw"] + start:s["praw"] + end], relocoffs=roff)
    return code, data


# ---------------------------------------------------------------------------
class Cmp:
    """Resolve-then-compare adjudicator."""

    MIN_DATA_INFO = 4     # informative (non-zero) bytes before DATA can AGREE
    MIN_BIND_SUPPORT = 2  # independent observations before a binding can CONTRA
    CLASS_CHANS = ("vtable", "str", "rtti0")   # channels that carry identity

    def __init__(self, wt: Path, retail: Retail, resolver, S, RL, RD):
        self.wt, self.retail, self.res, self.S = wt, retail, resolver, S
        self.RL, self.RD = RL, RD
        self._tcache, self._ocache = {}, {}
        self.bind = None      # name -> Counter(target VA), see Ctx.build_bindings
        self.code_index = {}  # name -> code COMDAT body, tree-wide (build_bindings)

    # -- per-relocation-slot comparison -------------------------------------
    def cmp_one(self, bname, ttok, bdata):
        """-> ('AGREE'|'CONTRA'|'UNK', channel)"""
        if ttok is None:
            return ("UNK", "none")
        bn = self.S.anon_ns_strip(bname)
        tva = self.RL.token_va(ttok)

        if tva is None:                                     # symbolic operand
            if ttok == bn:
                return ("AGREE", "name")
            if HELPER_RX.match(ttok) and HELPER_RX.match(bn):
                return ("CONTRA", "name")
            return ("UNK", "name")

        # -- implied-binding oracle (preferred over the raw map)
        #
        # ★ A raw `map` CONTRA is ambiguous: it implicates EITHER the row under
        # test OR the map row of the *callee*. Measured on the 152 already-landed
        # rows, the callee was at fault in most cases — e.g. two independent
        # bodies imply `??3@YAXPAX@Z` (operator delete) lives at 0x8240DDB0 while
        # the map says 0x82BC6B70, and `?StaticClassName@RndLight@@` is mapped at
        # 0x8273FD30 which reloc_disc/README.md §7 already records as
        # "knowingly mislabelled". Blaming the row under test there is a FALSE
        # CONTRA. So resolve names through bindings observed on *established*
        # pairings instead, and only fall back to the map when unobserved.
        bd = self.bind.get(bn) if self.bind else None
        if bd:
            if tva in bd:
                return ("UNK" if HELPER_RX.match(bn) else "AGREE", "bind")
            if sum(bd.values()) >= self.MIN_BIND_SUPPORT:
                return ("CONTRA", "bind")
            return ("UNK", "bind_weak")

        vas = self.res.vas(bn)                              # -- map (fallback)
        if vas:
            if tva in vas:
                return ("UNK" if HELPER_RX.match(bn) else "AGREE", "map")
            return ("CONTRA", "map")

        cls = vtable_class(bn)                              # -- vtable / RTTI
        if cls is not None:
            got = typedesc_class(self.retail.rtti_class(tva))
            if got is None:
                return ("UNK", "vtable")
            return ("AGREE" if got == cls else "CONTRA", "vtable")

        sb = self.RD.string_bytes(bn)                       # -- string literal
        if sb is not None:
            _wide, want = sb
            have = self.retail.read(tva, min(len(want), 48))
            if not have or not want:
                return ("UNK", "str")
            n = min(len(want), len(have))
            return ("AGREE" if have[:n] == want[:n] else "CONTRA", "str")

        rt = self.RD.rtti_name_bytes(bn)                    # -- ??_R0 descriptor
        if rt is not None:
            o, want = rt
            have = self.retail.read(tva + o, len(want))
            if not have:
                return ("UNK", "rtti0")
            return ("AGREE" if have == want else "CONTRA", "rtti0")

        cb = self.RD.const_bytes(bn)                        # -- fp constant
        if cb is not None:
            have = self.retail.read(tva, len(cb))
            if not have:
                return ("UNK", "const")
            return ("AGREE" if have == cb else "CONTRA", "const")

        ent = bdata.get(bname) or bdata.get(bn)             # -- generic data
        if ent is not None:
            want = ent["bytes"][:256]
            have = self.retail.read(tva, len(want))
            if not have:
                return ("UNK", "data")
            skip = ent["relocoffs"]
            info = diff = 0
            for i, (a, b) in enumerate(zip(want, have)):
                if any(i - k in (0, 1, 2, 3) and k in skip for k in (i, i - 1, i - 2, i - 3)):
                    continue
                if a != b:
                    diff += 1
                elif a:
                    info += 1
            if diff:
                return ("CONTRA", "data")
            return ("AGREE" if info >= self.MIN_DATA_INFO else "UNK", "data")

        return ("UNK", "none")

    # -- branch operand recovered from the ENCODING --------------------------
    def cmp_branch(self, bname, tva, cbyname):
        """Adjudicate an UNLABELLED branch operand. -> (verdict, channel)

        A branch target is CODE, so it is adjudicated as code:
          1. body identity against retail at the decoded VA. ICF folds only
             byte-identical bodies, so body identity is exactly the right test
             and is immune to what the callee is *called*;
          2. failing that, the observed-bindings oracle.

        It deliberately does NOT consult the map. A branch callee's map VA is
        precisely what ICF corrupts, and both observed instances prove it:
        0x822E4460 (?GetMaxSlots@TrackConfig@@) is filed as
        `RndAnimatable::GetRate`, and 0x82593F90 -- byte-identical to our
        ?GetName@Accomplishment@@ -- is filed nowhere while GetName is mapped at
        0x8244C094. Both are CORRECT branches that the map channel reads as
        CONTRA, which is how 4 already-landed rows kept rejecting.
        """
        bn = self.S.anon_ns_strip(bname)
        cc = (cbyname or {}).get(bn) or self.code_index.get(bn)
        if cc is not None:
            want = cc["masked"]
            have = self.retail.read(tva, len(want))
            if have and len(have) == len(want):
                skip = set()
                for k in cc["relocoffs"]:
                    skip.update(range(k, k + 4))
                info = diff = 0
                for i in range(len(want)):
                    if i in skip:
                        continue
                    if want[i] != have[i]:
                        diff += 1
                    elif want[i]:
                        info += 1
                if diff:
                    return ("CONTRA", "branch")
                if info >= self.MIN_DATA_INFO:
                    return ("AGREE", "branch")
        bd = self.bind.get(bn) if self.bind else None
        if bd and tva in bd:
            return ("UNK" if HELPER_RX.match(bn) else "AGREE", "branch")
        return ("UNK", "branch_unk")

    # -- whole-body adjudication --------------------------------------------
    def adjudicate(self, tinfo, cand, bdata, bcode=None):
        """target body vs one candidate base COMDAT -> verdict dict.

        `bcode` is the unit's list of code COMDATs; it feeds the `code` channel
        for synthesized-branch offsets only.
        """
        cbyname = {}
        for c in (bcode or ()):
            cbyname.setdefault(self.S.anon_ns_strip(c["name"]), c)
        tb, bb = {}, {}
        for off, tok in tinfo["relocs"]:
            tb.setdefault(off, tok)
        for off, nm, ty in cand["relocs"]:
            if ty == PPC_PAIR:
                continue
            bb.setdefault(off, nm)
        xb = tinfo.get("xbranch") or {}
        agree = contra = unk = 0
        chans, det = Counter(), []
        for off in sorted(set(tb) | set(bb)):
            t, b = tb.get(off), bb.get(off)
            synth = False
            if t is None and b is not None and off in xb:
                # ★★ dtk emitted no usable label, but the ENCODING says this
                # word IS an external branch. Absence of a label is not absence
                # of a branch -- recover the operand and adjudicate it normally
                # (bindings oracle first, which is what makes this robust to the
                # callee being an ICF-fold representative carrying a different
                # map name, e.g. 0x822E4460 -> `RndAnimatable::GetRate`).
                t = "lbl_%08X" % xb[off]
                synth = True
            if t is None or b is None:
                # Direction matters. `b is None` (target relocates, base does
                # not) is reliable: base relocations come from the COFF table.
                # `t is None` with no decodable branch is the unreliable
                # direction the fleet was warned about -- keep it visible under
                # its own channel rather than silently trusting it.
                contra += 1
                ch = "shape" if b is None else "shape_t"
                det.append((off, b, t, "SHAPE", ch))
                chans[ch] += 1
                continue
            if synth:
                v, ch = self.cmp_branch(b, xb[off], cbyname)
            else:
                v, ch = self.cmp_one(b, t, bdata)
            det.append((off, b, t, v, ch))
            if v == "AGREE":
                agree += 1
                chans[ch] += 1
            elif v == "CONTRA":
                contra += 1
                chans[ch] += 1
            else:
                unk += 1
        if contra:
            verdict = "REJECT"
        elif agree:
            verdict = "ACCEPT"
        else:
            verdict = "UNDECIDED"
        return dict(verdict=verdict, agree=agree, contra=contra, unk=unk,
                    nrel=len(set(tb) | set(bb)), chans=dict(chans),
                    class_evidence=sum(chans[c] for c in self.CLASS_CHANS),
                    detail=det)

    # -- target-vs-target (map-independent) ---------------------------------
    def tt_compare(self, a, b):
        """Two retail bodies, every operand resolved to an absolute.
        -> ('TWIN'|'DIFF'|'SIZE', n_differing)"""
        if a["size"] != b["size"]:
            return ("SIZE", abs(a["size"] - b["size"]))
        ta = {o: t for o, t in a["relocs"]}
        tbb = {o: t for o, t in b["relocs"]}
        n = 0
        if a["masked"] != b["masked"]:
            n += sum(1 for x, y in zip(a["masked"], b["masked"]) if x != y)
        for o in set(ta) | set(tbb):
            if ta.get(o) != tbb.get(o):
                n += 1
        return ("TWIN" if n == 0 else "DIFF", n)

    # -- caches --------------------------------------------------------------
    def target(self, asm_path):
        """dtk target asm -> {VA: {size, masked, relocs, xbranch}} with INTERNAL
        branch labels demoted back to ordinary instructions and UNLABELLED
        external branches recovered from the encoding.

        ★ `reloclib._operand_relocated` treats every `lbl_`/`jmp_` token as a
        relocation, but dtk also names *intra-function* branch targets that way
        (`beq cr6, lbl_8246EED0` inside the function that contains 0x8246EED0).
        Those are PC-relative and carry no relocation on the base side, so
        counting them produced (a) a bogus one-sided "shape" contradiction and
        (b) — worse — it EXCLUDED real control-flow words from the byte
        comparison. Demoting them makes the comparison strictly stronger.
        reloc_disc's own copy is left untouched: its 99.41% gate is calibrated
        against that exact behaviour.

        ★★ The CONVERSE defect (laneBY-2). dtk's label is not only sometimes
        wrong, it is sometimes ABSENT, and `_operand_relocated` then records no
        relocation at all for that offset. `?GetMaxSlots@GemManager@@` @
        0x82b99058 is `lwz r3,4(r3); b 0x822E4460`, but dtk prints the tail call
        as `b except_data_822D27F8` -- a token the classifier does not treat as
        a relocation (and whose name does not even denote 0x822E4460). The base
        COMDAT *does* carry a relocation at offset 4, so the offset went
        one-sided and `adjudicate` scored it a SHAPE contra => REJECT on a row
        that is correct. **Absence of a dtk label is not absence of a branch**,
        so decode every unlabelled word from the ENCODING.

        `xbranch` is deliberately kept OUT of `relocs`: `masked_equal` skips the
        UNION of both sides' relocated words, so folding these offsets into
        `relocs` would newly exclude real control-flow bytes from the byte
        comparison and silently widen candidate selection. Keeping them separate
        leaves candidate selection bit-for-bit identical to before, so the
        tool's calibrated control numbers stay valid; only `adjudicate` consults
        them.
        """
        k = str(asm_path)
        if k not in self._tcache:
            raw = self.RL.target_funcs(asm_path)
            fixed = {}
            for va, t in raw.items():
                end = va + t["size"]
                body = bytearray(t["masked"])
                keep = []
                for off, tok in t["relocs"]:
                    w = self.retail.read(va + off, 4)
                    dec = decode_branch(w, va + off) if w else None
                    if dec is not None:
                        # ★ Trust the ENCODING, not dtk's printed label. Live
                        # asm contains e.g. `beq cr6, lbl_8246EED0` for bytes
                        # 41 9A 00 54, which actually branch +0x54 to
                        # 0x82481910 -- inside the same function. Taking the
                        # label at face value invented a one-sided "shape"
                        # contradiction on a body with 21 other AGREEs.
                        tva = dec
                        tok = f"lbl_{dec:08X}"
                    else:
                        tva = self.RL.token_va(tok)
                    # LK=1 (`bl`) is a CALL and carries a relocation on the
                    # base side even when it targets its own COMDAT (direct
                    # recursion, e.g. ?Advance@ObjDirItr<BandList>@@). Only
                    # non-linking branches are internal control flow.
                    is_call = bool(w and (struct.unpack(">I", w)[0] & 1))
                    if (tva is not None and va <= tva < end and w is not None
                            and not is_call):
                        body[off:off + 4] = w      # internal: compare the bytes
                        continue
                    keep.append((off, tok))
                # ★★ recover branches dtk gave no usable relocation token for
                dtk_offs = {o for o, _ in t["relocs"]}
                xbr = {}
                for off in range(0, t["size"], 4):
                    if off in dtk_offs:
                        continue           # dtk already stated an opinion here
                    w = self.retail.read(va + off, 4)
                    if not w or len(w) < 4:
                        continue
                    dec = decode_branch(w, va + off)
                    if dec is None:
                        continue           # not a branch instruction at all
                    is_call = bool(struct.unpack(">I", w)[0] & 1)
                    if va <= dec < end and not is_call:
                        continue           # internal control flow: base has no reloc
                    xbr[off] = dec
                fixed[va] = dict(size=t["size"], masked=bytes(body), relocs=keep,
                                 xbranch=xbr)
            self._tcache[k] = fixed
        return self._tcache[k]



    def obj(self, path):
        k = str(path)
        if k not in self._ocache:
            self._ocache[k] = parse_obj(self.RL, Path(path))
        return self._ocache[k]


# ---------------------------------------------------------------------------
# Context / plumbing
# ---------------------------------------------------------------------------
class Ctx:
    def __init__(self, wt: Path, extra_map=None):
        self.wt = wt
        rd = wt / "scripts" / "harvest" / "reloc_disc"
        self.RL = _load(rd / "reloclib.py", "arc_reloclib")
        self.RD = _load(rd / "relocdisc.py", "arc_relocdisc")
        self.S = self.RL.load_S(wt)
        self.res = self.RL.Resolver(wt, extra_map=extra_map)
        self.retail = Retail(wt / "orig" / "45410914" / "band.exe")
        self.cmp = Cmp(wt, self.retail, self.res, self.S, self.RL, self.RD)
        # VA -> name from the live map (for contested detection)
        m = json.loads((wt / "scripts/target_symbol_map.json").read_text())
        self.map_va2name = {int(k, 16): v for k, v in m.items()
                            if k.startswith("0x") and isinstance(v, str)}
        self.map_name2va = defaultdict(set)
        for va, nm in self.map_va2name.items():
            self.map_name2va[nm].add(va)
        self.units = list(self.RD.unit_iter(wt))

    def unit_index(self):
        """unit name -> (asm path, base obj path); plus VA -> unit."""
        if not hasattr(self, "_ui"):
            self._ui, self._va2unit = {}, {}
            for name, _tobj, asm, bobj in self.units:
                if not asm.exists() or not bobj.exists():
                    continue
                self._ui[name] = (asm, bobj)
                for va in self.cmp.target(asm):
                    self._va2unit[va] = name
        return self._ui, self._va2unit


def masked_equal(tinfo, cand):
    """Byte equality outside the UNION of both sides' relocated words.

    Using the union (rather than each side's own mask, as BV-4 did) makes a
    reloc-position mismatch visible instead of silently breaking equality.
    """
    if tinfo["size"] != cand["size"]:
        return False, 0
    skip = set()
    for off, _tok in tinfo["relocs"]:
        skip.update(range(off, off + 4))
    for off in cand["relocoffs"]:
        skip.update(range(off, off + 4))
    a, b = tinfo["masked"], cand["masked"]
    n = 0
    for i in range(len(a)):
        if i in skip:
            continue
        if a[i] != b[i]:
            return False, 0
        n += 1
    return True, n


# ---------------------------------------------------------------------------
def cmd_pool(ctx, args):
    """Regenerate the anon-zero byte-perfect candidate pool, with ALL candidates."""
    rep = json.loads((ctx.wt / "build/45410914/report.json").read_text())
    ANON = re.compile(r"^fn_([0-9A-Fa-f]{8})$")
    zero = defaultdict(list)
    for u in rep["units"]:
        md = u.get("metadata") or {}
        if md.get("auto_generated") or not md.get("source_path"):
            continue
        for f in u.get("functions") or []:
            if (f.get("match_percent_normalized") or 0.0) != 0.0:
                continue
            m = ANON.match(f["name"])
            if m:
                zero[u["name"]].append((int(m.group(1), 16), int(f["size"])))
    ui, _ = ctx.unit_index()
    rows, G = [], Counter()
    for unit, fns in sorted(zero.items()):
        if unit not in ui:
            G["unit_no_objs"] += len(fns)
            continue
        asm, bobj = ui[unit]
        T = ctx.cmp.target(asm)
        code, _data = ctx.cmp.obj(bobj)
        bysize = defaultdict(list)
        for c in code:
            bysize[c["size"]].append(c)
        for va, sz in fns:
            t = T.get(va)
            if t is None:
                G["tgt_missing"] += 1
                continue
            cands = [c["name"] for c in bysize.get(t["size"], [])
                     if masked_equal(t, c)[0]]
            if not cands:
                G["no_byte_perfect"] += 1
                continue
            G["byte_perfect"] += 1
            names = sorted(set(ctx.S.anon_ns_strip(c) for c in cands))
            contested = {n: sorted(ctx.map_name2va.get(n, ()))
                         for n in names if ctx.map_name2va.get(n)}
            rows.append(dict(unit=unit, va=f"0x{va:08x}", size=t["size"],
                             cands=names, contested=contested,
                             ambiguous=len(names) > 1))
            G["multi_cand"] += len(names) > 1
            G["contested"] += bool(contested)
    print(f"pool rows {len(rows)}   " + "  ".join(f"{k}={v}" for k, v in sorted(G.items())))
    Path(args.out).write_text(json.dumps(rows, indent=1))
    return rows


def _adjudicate_va_name(ctx, va, name, ui, va2unit, suppress_rival=True):
    """Full adjudication of one (VA, name) proposal."""
    unit = va2unit.get(va)
    out = dict(va=f"0x{va:08x}", name=name, unit=unit)
    if unit is None:
        out.update(verdict="NO_UNIT")
        return out
    asm, bobj = ui[unit]
    t = ctx.cmp.target(asm).get(va)
    code, data = ctx.cmp.obj(bobj)
    cand = next((c for c in code if ctx.S.anon_ns_strip(c["name"]) == name), None)
    if t is None or cand is None:
        out.update(verdict="NO_SUPPLY", have_target=t is not None,
                   have_base=cand is not None)
        return out
    ok, ninfo = masked_equal(t, cand)
    out.update(size=t["size"], masked_equal=ok, info_bytes=ninfo)
    if not ok:
        out.update(verdict="NOT_BYTE_PERFECT")
        return out
    v = ctx.cmp.adjudicate(t, cand, data, code)
    out.update({k: v[k] for k in
                ("verdict", "agree", "contra", "unk", "nrel", "chans",
                 "class_evidence")})
    out["detail"] = [d for d in v["detail"] if d[3] != "AGREE"][:8]
    # target-vs-target against any rival VA already holding this name
    rivals = sorted(x for x in ctx.map_name2va.get(name, ()) if x != va)
    if rivals:
        tt = []
        for rv in rivals:
            ru = va2unit.get(rv)
            rt = ctx.cmp.target(ui[ru][0]).get(rv) if ru in ui else None
            if rt is None:
                tt.append((f"0x{rv:08x}", "NO_ASM", 0))
            else:
                k, n = ctx.cmp.tt_compare(t, rt)
                tt.append((f"0x{rv:08x}", k, n))
        out["tt"] = tt
        out["contested"] = True
    return out


def ship_gate(row, evidence=1):
    """The shipping decision (see cmd_sweep for the rationale of each clause)."""
    if row["verdict"] != "ACCEPT" or row.get("agree", 0) < evidence:
        return False
    rivals = row.get("tt") or []
    return all(t[1] == "TWIN" for t in rivals)


def cmd_verify(ctx, args):
    frag = json.loads(Path(args.fragment).read_text())
    ui, va2unit = ctx.unit_index()
    res = []
    for k, name in frag.items():
        if not isinstance(name, str) or not k.startswith("0x"):
            continue
        r = _adjudicate_va_name(ctx, int(k, 16), name, ui, va2unit)
        r["ship"] = ship_gate(r)
        res.append(r)
    G = Counter(r["verdict"] for r in res)
    print(f"verify {len(res)} rows: " + "  ".join(f"{k}={v}" for k, v in G.most_common()))
    print(f"  SHIP={sum(1 for r in res if r['ship'])}  "
          f"NO-SHIP={sum(1 for r in res if not r['ship'])}")
    if args.out:
        Path(args.out).write_text(json.dumps(res, indent=1))
    return res


# ---------------------------------------------------------------------------
# Regression fixtures for the unlabelled-branch defect (laneBY-2).
#
# Both rows are ESTABLISHED, byte-perfect, already-landed map pairings, so
# ACCEPT is the only correct verdict. Before the fix BOTH read REJECT, each via
# a different half of the defect. If either ever reverts to REJECT, the
# comparator has regressed to trusting dtk's labels / the callee's map row.
# ---------------------------------------------------------------------------
BRANCH_FIXTURES = [
    ("0x82b99058", "?GetMaxSlots@GemManager@@QBAHXZ", "ACCEPT",
     "unlabelled cross-unit tail call: retail is `lwz r3,4(r3); b 0x822E4460` "
     "but dtk prints `b except_data_822D27F8`, which the reloc classifier "
     "ignores -> offset 4 went one-sided -> SHAPE contra. The callee "
     "(?GetMaxSlots@TrackConfig@@) is not even defined in GemManager.obj, so "
     "it needs the tree-wide code index; 0x822E4460 is an ICF-fold "
     "representative filed under `RndAnimatable::GetRate`."),
    ("0x825f72a8", "?IsAccomplishmentSecret@@YA_NPAVAccomplishment@@PBVBandProfile@@@Z",
     "ACCEPT",
     "same unlabelled branch, but here the callee IS mapped -- elsewhere. "
     "?GetName@Accomplishment@@ is byte-identical to retail at the branch "
     "target 0x82593F90 yet the map files it at 0x8244C094, so consulting the "
     "map for a branch operand yields a false CONTRA. Body identity must win."),
]


def cmd_selftest(ctx, args):
    ui, va2unit = ctx.unit_index()
    bad = 0
    for va, name, want, why in BRANCH_FIXTURES:
        r = _adjudicate_va_name(ctx, int(va, 16), name, ui, va2unit)
        got = r.get("verdict")
        ok = got == want
        bad += not ok
        print(f"[{'PASS' if ok else 'FAIL'}] {va} {name}")
        print(f"        want={want} got={got} agree={r.get('agree')} "
              f"contra={r.get('contra')} chans={r.get('chans')}")
        if not ok:
            print(f"        WHY THIS FIXTURE EXISTS: {why}")
            for d in r.get("detail", [])[:6]:
                print(f"          {d}")
    print(f"selftest: {len(BRANCH_FIXTURES) - bad}/{len(BRANCH_FIXTURES)} passed")
    if bad:
        sys.exit(1)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--worktree", required=True, type=Path)
    ap.add_argument("--no-bindings", action="store_true",
                    help="disable the implied-binding oracle (raw map channel)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("pool"); p.add_argument("--out", required=True)
    p = sub.add_parser("verify")
    p.add_argument("--fragment", required=True); p.add_argument("--out")
    p = sub.add_parser("control")
    p.add_argument("--out"); p.add_argument("--limit", type=int, default=0)
    sub.add_parser("selftest")
    p = sub.add_parser("sweep")
    p.add_argument("--pool", required=True); p.add_argument("--out", required=True)
    p.add_argument("--min-size", type=int, default=0)
    # default 4: the truth-ablation control (cmd_control) measures 2.41%
    # harmful plants at agree>=4 versus 5.80% at agree>=1, matching the ~2.66%
    # level reloc_disc/laneBU4 accepted for its own shipped channel.
    p.add_argument("--evidence", type=int, default=4)
    a = ap.parse_args()
    ctx = Ctx(a.worktree)
    if not a.no_bindings:
        build_bindings(ctx)
    {"pool": cmd_pool, "verify": cmd_verify, "sweep": cmd_sweep,
     "control": cmd_control, "selftest": cmd_selftest}[a.cmd](ctx, a)



# ---------------------------------------------------------------------------
def build_bindings(ctx, verbose=True):
    """Observe (base symbol -> target VA) bindings on ESTABLISHED pairings.

    An established pairing is a target VA that the map already names N where the
    unit's base obj supplies COMDAT N and the two are masked-equal. If our
    COMDAT really is that body, then the base symbol at relocation offset o
    denotes whatever the retail body references at o — independent of whether
    that callee has a (correct) map row of its own. Accumulating this over the
    whole tree yields a name->VA oracle that is immune to a single bad callee
    row, which the raw map channel is not.
    """
    ui, _ = ctx.unit_index()
    bind = defaultdict(Counter)
    # ★★ tree-wide code-body index (laneBY-2). A branch callee is very often an
    # inline that the *calling* unit only references (undefined in its obj) but
    # some other unit defines as a COMDAT -- e.g.
    # ?GetMaxSlots@TrackConfig@@QBAHXZ is external in GemManager.obj. Every obj
    # is parsed and cached here anyway, so indexing bodies costs nothing.
    # Definitions that DISAGREE across units are dropped rather than guessed.
    cidx, cbad = {}, set()
    npair = 0
    for unit, (asm, bobj) in ui.items():
        T = ctx.cmp.target(asm)
        code, _d = ctx.cmp.obj(bobj)
        byname = {}
        for c in code:
            byname.setdefault(ctx.S.anon_ns_strip(c["name"]), c)
        for c in code:
            n = ctx.S.anon_ns_strip(c["name"])
            prev = cidx.get(n)
            if prev is None:
                cidx[n] = c
            elif (prev["size"] != c["size"] or prev["masked"] != c["masked"]
                  or prev["relocoffs"] != c["relocoffs"]):
                cbad.add(n)
        for va, t in T.items():
            nm = ctx.map_va2name.get(va)
            if not nm:
                continue
            c = byname.get(ctx.S.anon_ns_strip(nm))
            if c is None or not masked_equal(t, c)[0]:
                continue
            npair += 1
            tb = {}
            for off, tok in t["relocs"]:
                tb.setdefault(off, tok)
            seen = set()
            for off, n, ty in c["relocs"]:
                if ty == PPC_PAIR or off in seen:
                    continue
                seen.add(off)
                v = ctx.RL.token_va(tb.get(off))
                if v is not None:
                    bind[ctx.S.anon_ns_strip(n)][v] += 1
    for n in cbad:
        cidx.pop(n, None)
    if verbose:
        multi = sum(1 for v in bind.values() if len(v) > 1)
        print(f"bindings: {npair} established pairings -> {len(bind)} names "
              f"({multi} with >1 implied VA)")
        print(f"code index: {len(cidx)} unambiguous bodies "
              f"({len(cbad)} names dropped as inconsistent across units)")
    ctx.cmp.bind = bind
    ctx.cmp.code_index = cidx
    return bind


def cmd_sweep(ctx, args):
    """Adjudicate the whole byte-perfect pool and emit a shippable fragment.

    Ship gate, in order:
      1. exactly ONE candidate name survives adjudication (contra == 0) — a
         second survivor means the byte-twins are indistinguishable and picking
         either would be a coin flip dressed as evidence;
      2. that survivor carries >= `--evidence` resolved AGREEs, so we are never
         shipping on "nothing contradicted it";
      3. if the name is CONTESTED (already mapped at another VA), every rival
         must be a resolved TWIN. A rival that differs once operands are
         resolved means one of the two claims is false, and this lane refuses
         to guess which — the rival's own adjudication is recorded in the row
         so a later lane can adjudicate without re-deriving it.
    """
    pool = json.loads(Path(args.pool).read_text())
    ui, va2unit = ctx.unit_index()
    out, G = [], Counter()
    for row in pool:
        va = int(row["va"], 16)
        if row["size"] < args.min_size:
            G["below_min_size"] += 1
            continue
        adj = []
        for nm in row["cands"]:
            adj.append(_adjudicate_va_name(ctx, va, nm, ui, va2unit))
        # ★ Uniqueness is decided on ANY evidence; the strength threshold is a
        # POST-filter on the survivor. Pre-filtering by --evidence would let a
        # high-evidence decoy win a row whose true owner merely had weak
        # evidence — the exact inversion the ablation control is there to catch.
        surv = [a for a in adj if a["verdict"] == "ACCEPT"
                and a.get("agree", 0) >= 1]
        if not surv:
            G["no_survivor"] += 1
            continue
        if len(surv) > 1:
            G["ambiguous_survivors"] += 1
            continue
        w = surv[0]
        rivals = w.get("tt") or []
        if rivals:
            nontwin = [t for t in rivals if t[1] != "TWIN"]
            if nontwin:
                # record the rival's own adjudication as evidence, then drop
                for rv, kind, n in nontwin:
                    w.setdefault("rival_adj", []).append(
                        _adjudicate_va_name(ctx, int(rv, 16), w["name"],
                                            ui, va2unit))
                G["contested_nontwin"] += 1
                w["ship"] = False
                out.append(w)
                continue
            G["contested_twin"] += 1
        if w.get("agree", 0) < args.evidence:
            G["below_evidence"] += 1
            w["ship"] = False
            w["drop_reason"] = f"agree<{args.evidence}"
            out.append(w)
            continue
        G["SHIP"] += 1
        w["ship"] = True
        out.append(w)
    # ★ Intra-fragment collision gate. Each row is checked against the EXISTING
    # map, which cannot see two rows of this same fragment proposing one name at
    # two VAs. Measured: 3 such names (?SetType@DxEnviron@@, ?SetType@DxCubeTex@@,
    # ?Highlight@UIComponent@@$4...) = 6 rows. They are byte-twins of each other,
    # so shipping both is guaranteed to be half wrong; shipping either is a coin
    # flip. Drop the whole group.
    claim = Counter(r["name"] for r in out if r.get("ship"))
    ncoll = 0
    for r in out:
        if r.get("ship") and claim[r["name"]] > 1:
            r["ship"] = False
            r["drop_reason"] = "intra_fragment_name_collision"
            ncoll += 1
    G["intra_fragment_collision"] = ncoll
    G["SHIP"] -= ncoll
    frag = {r["va"]: r["name"] for r in out if r.get("ship")}
    print("sweep: " + "  ".join(f"{k}={v}" for k, v in sorted(G.items())))
    print(f"shippable rows: {len(frag)}")
    Path(args.out).write_text(json.dumps(out, indent=1))
    Path(args.out.replace(".json", "_frag.json")).write_text(
        json.dumps(frag, indent=1))
    return out



def benign_twin(ctx, a, b):
    """Are two base COMDATs the SAME CODE with the SAME referents?

    If so, mislabelling one as the other still emits byte-identical machine code
    against retail, so the credit is genuine even though the *name* is wrong.
    An ablation "false plant" of this kind is a naming error, not metric
    inflation, and must not be priced as if it were.
    """
    if a is None or b is None or a["masked"] != b["masked"]:
        return False
    def rmap(c):
        out = {}
        for off, nm, ty in c["relocs"]:
            if ty != PPC_PAIR:
                out.setdefault(off, ctx.S.anon_ns_strip(nm))
        return out
    ra, rb = rmap(a), rmap(b)
    if set(ra) != set(rb):
        return False
    for off in ra:
        if ra[off] == rb[off]:
            continue
        va_a = ctx.cmp.bind.get(ra[off]) if ctx.cmp.bind else None
        va_b = ctx.cmp.bind.get(rb[off]) if ctx.cmp.bind else None
        if not va_a or not va_b or set(va_a) != set(va_b):
            return False
    return True


def cmd_control(ctx, args):
    """Truth-ablation control — the only honest calibration of this gate.

    reloc_disc/README.md §laneBU4.3 is the load-bearing lesson: a discriminator
    calibrated on a TRUTH-PRESENT population says nothing about how it behaves
    when the right answer is not among the candidates. That is not a hypothetical
    here — an anonymous target body is unnamed precisely because earlier passes
    could not name it, so its true owner may not be in our supply at all. When
    truth is absent the only correct behaviour is to REFUSE.

    POS arm: established pairings (VA already mapped to N, N supplied in-unit and
             masked-equal) that have >= 2 byte-perfect candidates. The gate should
             pick N.
    NEG arm: identical rows with N deleted from the candidate set. Truth is now
             absent by construction, so ANY acceptance is a FALSE PLANT.
    """
    ui, va2unit = ctx.unit_index()
    pos = Counter()
    neg = Counter()
    plants = []
    n = 0
    for unit, (asm, bobj) in sorted(ui.items()):
        T = ctx.cmp.target(asm)
        code, data = ctx.cmp.obj(bobj)
        bysize = defaultdict(list)
        byname = {}
        for c in code:
            bysize[c["size"]].append(c)
            byname.setdefault(ctx.S.anon_ns_strip(c["name"]), c)
        for va, t in T.items():
            nm = ctx.map_va2name.get(va)
            if not nm:
                continue
            truth = ctx.S.anon_ns_strip(nm)
            c = byname.get(truth)
            if c is None or not masked_equal(t, c)[0]:
                continue
            cands = sorted({ctx.S.anon_ns_strip(x["name"])
                            for x in bysize.get(t["size"], [])
                            if masked_equal(t, x)[0]})
            if truth not in cands or len(cands) < 2:
                continue
            n += 1
            if args.limit and n > args.limit:
                break
            adj = {c2: _adjudicate_va_name(ctx, va, c2, ui, va2unit)
                   for c2 in cands}
            for arm, pool_names, book in (("pos", cands, pos),
                                          ("neg", [x for x in cands if x != truth],
                                           neg)):
                surv = [adj[x] for x in pool_names
                        if adj[x]["verdict"] == "ACCEPT" and adj[x].get("agree", 0)]
                if not surv:
                    book["refuse_no_survivor"] += 1
                elif len(surv) > 1:
                    book["refuse_ambiguous"] += 1
                else:
                    w = surv[0]
                    rivals = [x for x in (w.get("tt") or []) if x[1] != "TWIN"]
                    # in the NEG arm the row's own map entry is the truth we
                    # ablated, so ignore self when judging contest
                    if rivals and not all(r[0] == f"0x{va:08x}" for r in rivals):
                        book["refuse_contested"] += 1
                    elif arm == "pos":
                        book["PICK_correct" if w["name"] == truth
                             else "PICK_WRONG"] += 1
                        plants.append(dict(arm="pos", va=f"0x{va:08x}",
                                           unit=unit, truth=truth,
                                           planted=w["name"], agree=w["agree"],
                                           size=w["size"], nrel=w["nrel"],
                                           chans=w["chans"], ncand=len(cands),
                                           cls=w.get("class_evidence", 0)))
                    else:
                        book["FALSE_PLANT"] += 1
                        book["plant_BENIGN_TWIN" if benign_twin(
                            ctx, byname.get(truth), byname.get(w["name"]))
                            else "plant_HARMFUL"] += 1
                        plants.append(dict(arm="neg", va=f"0x{va:08x}",
                                           benign=benign_twin(
                                               ctx, byname.get(truth),
                                               byname.get(w["name"])),
                                           unit=unit, truth=truth,
                                           planted=w["name"], agree=w["agree"],
                                           size=w["size"], nrel=w["nrel"],
                                           chans=w["chans"], ncand=len(cands),
                                           cls=w.get("class_evidence", 0)))
        if args.limit and n > args.limit:
            break
    def rate(b, good, bad):
        tot = b[good] + b[bad]
        return f"{100.0*b[good]/tot:.2f}%" if tot else "n/a"
    print(f"ablation population: {n} established pairings with >=2 byte-twins")
    print("POS (truth present):", dict(pos),
          " precision =", rate(pos, "PICK_correct", "PICK_WRONG"))
    print("NEG (truth ablated):", dict(neg))
    dec = neg["FALSE_PLANT"]
    tot = sum(neg.values())
    print(f"  FALSE PLANT RATE = {dec}/{tot} = {100.0*dec/tot:.2f}%" if tot else "")
    if args.out:
        Path(args.out).write_text(json.dumps(
            dict(pos=dict(pos), neg=dict(neg), plants=plants), indent=1))


# NB: keep this guard LAST — main() dispatches to functions defined below it.
if __name__ == "__main__":
    main()
