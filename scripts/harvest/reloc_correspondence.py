#!/usr/bin/env python3
"""reloc_correspondence.py -- is a 100%-matched function a REPRODUCTION or a SHAPE?

THE QUESTION
    objdiff scores this project with `functionRelocDiffs = none` (the
    "normalized" diff).  That mode deliberately ignores the *operand* bytes that
    a relocation patches, because in a partial decomp our link layout is not
    retail's, so relocation ADDRESSES legitimately differ everywhere.  The
    consequence nobody had measured: two functions with the same instruction
    shape but pointing at DIFFERENT OBJECTS score 100.0%.

    laneBE found a concrete instance -- a 32-byte static-init guard-clear
    cleanup where our symbol clears `RndAnimatable`'s guard and the retail
    function clears an unrelated one.  The `StaticClassName`/`Type()` family
    (453 members, one identical 22-instruction body) is the same class.

    This tool answers, per function and in census: does each relocation point at
    the SEMANTICALLY CORRESPONDING symbol?

METHOD -- symbolic correspondence, NOT raw byte/address equality
    ★ Raw-mode diff is the WRONG test and this tool does not use it.  Raw diff
    rejects correct code, because our addresses are not retail's.  The right
    test compares the *identity of the pointed-at object*.

    For each function scored 100.0% `match_percent_normalized` in report.json:

      1. TARGET side.  Open the dtk-split target obj named by objdiff.json.
         Each function is its own COMDAT section, so the section's relocation
         list IS the function's.  Every relocation names a retail symbol, which
         after `obj_target_symbol_renamer` is either a mangled MSVC name or an
         anonymous dtk label (`fn_<VA>`, `lbl_<VA>`, `jumptable_<VA>`).  Both
         forms reduce to a retail VIRTUAL ADDRESS:
             fn_/lbl_/... -> parse the VA out of the label
             mangled      -> reverse `scripts/target_symbol_map.json`,
                             else `config/45410914/symbols.txt`
         So every target relocation slot becomes a retail VA.

      2. BASE side.  Open our compiled obj.  Find the paired symbol:
             (a) same name present  -> objdiff's name pairing
             (b) otherwise, if the target name is funclet-like, reproduce
                 objdiff's `pair_funclets_by_bytes` signature (mask the whole
                 4-byte instruction word at every relocation offset) and take
                 EVERY base symbol with an equal signature as a candidate.
                 Being generous here is deliberate: we ask "could ANY admissible
                 base partner have corresponding relocations", so a DIVERGENT
                 verdict is not an artifact of objdiff's arbitrary tie-break.
         Each base relocation names an MSVC symbol.

      3. CORRESPONDENCE.  Align the two relocation lists by (offset, type).
         For each slot we must decide "is base symbol N the thing that lives at
         retail VA V".  Two oracles, in order:

         SUPERVISED (code).  `target_symbol_map.json` / `symbols.txt` give
         name -> retail VA for ~26k named retail functions.  If N is in there,
         compare VAs directly.  Decisive.

         CONSISTENCY (data).  There is NO retail-side name oracle for data:
         every data symbol in `symbols.txt` is an anonymous `lbl_<VA>` (checked:
         0 occurrences of `??_7`/`??_B`/`??_C`).  So data correspondence is
         decided by INJECTIVITY over the whole census instead.  A base data
         symbol `??_B...@51` names exactly ONE object in the linked retail
         binary (identical COMDAT names fold to one VA), therefore it can
         legitimately be paired with exactly ONE target VA.  Build the observed
         relation base_name <-> target_VA across every matched function; a base
         name observed against k > 1 distinct target VAs proves at least k-1 of
         those pairings wrong.  This is the instrument that catches the
         guard-clear class.

FOUR-WAY VERDICT (per function)
    NO_RELOCS      no relocations at all -- nothing to check, trivially fine.
    CORRESPONDING  every relocation slot resolved AND agreed.
    DIVERGENT      >= 1 slot resolved and DISAGREED (or the two sides do not
                   even have the same relocation shape).
    UNRESOLVABLE   >= 1 slot could not be decided by either oracle, and none
                   disagreed.  ★ THIS IS NOT "FAKE".  Never fold it into a
                   headline divergence number.

LIMITS -- read before quoting a row
    * UNRESOLVABLE is the honest bucket for "the binary does not tell us".  It
      is large and that is a property of the evidence, not of the code.
    * ICF.  A relocation may legitimately target a folded symbol whose retained
      retail name differs from ours.  `target_symbol_map.json` keeps ONE name per
      folded VA and it is often the wrong one.  The supervised code oracle
      therefore treats a VA that is a known ICF fold (multiple names -> one VA)
      as UNDECIDED rather than divergent, and `--merged-tolerant` (default)
      extends that to any VA whose name is ambiguous in the map.
    * Consistency inference needs >= 2 observations of a base symbol to say
      anything; singletons are UNRESOLVABLE by construction.
    * Trivial sizes.  Under reloc masking a 12-byte adjustor thunk is shared by
      >1,600 symbols, so correspondence there is near-vacuous either way.  The
      census reports a size-band stratum so those can be read separately.
    * STALENESS.  `build/45410914/{obj,asm}` are append-only; ~70% of a warm
      worktree's files are orphans of prior split layouts.  This tool sources
      its unit list from `objdiff.json` via `live_units.py` and never globs.
    * Run a full `./tools/ninja-locked` in the worktree FIRST.  A fresh worktree
      reflinks main's possibly-dirty objs, and dirty objs manufacture evidence.

USAGE
    python3 scripts/harvest/reloc_correspondence.py --worktree $PWD --symbol fn_82802080
    python3 scripts/harvest/reloc_correspondence.py --worktree $PWD --census --out census.json
    python3 scripts/harvest/reloc_correspondence.py --worktree $PWD --census \
            --va-ranges ranges.json      # only functions whose retail VA is in a range
"""
import argparse
import json
import os
import re
import struct
import sys
from collections import Counter, defaultdict

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
sys.path.insert(0, os.path.join(os.path.dirname(_HERE), "unicorn_runner"))

from live_units import live_target_paths  # noqa: E402
from coff import COFFParser  # noqa: E402

ANON_RE = re.compile(r"^(?:fn|lbl|sub|jumptable|loc|dbl|flt|off|byte|word|dword|unk)_([0-9A-Fa-f]{8})$")
FN_RE = re.compile(r"^fn_([0-9A-Fa-f]{8})$")
SYM_LINE = re.compile(r"^(\S+) = \.(\w+):0x([0-9A-Fa-f]+)")

# Relocation types that carry a *pointed-at object* claim. PAIR (0x12) is the
# second half of a REFHI/REFLO pair and always names @comp.id -- pure noise.
PAIR_TYPE = 0x12
ABSOLUTE_TYPE = 0x0
REL14_TYPE = 0x7   # conditional-branch displacement


class RetailImage:
    """Flat read access to the retail image at any virtual address.

    ★ `orig/45410914/band.exe` is the DECOMPRESSED PE of the retail XEX
    (imagebase 0x82000000, 12 sections).  It is the ground truth this whole
    tool rests on: it turns "does base symbol N correspond to retail VA V" from
    an inference about maps into a BYTE COMPARISON."""

    def __init__(self, path):
        d = open(path, "rb").read()
        self.d = d
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        _, nsec, _, _, _, opt, _ = struct.unpack_from("<HHIIIHH", d, pe + 4)
        self.base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        off = pe + 24 + opt
        self.secs = []
        for _ in range(nsec):
            nm = d[off:off + 8].rstrip(b"\0").decode("ascii", "replace")
            vs, va, rs, ro = struct.unpack_from("<IIII", d, off + 8)
            self.secs.append((nm, self.base + va, vs, ro, rs))
            off += 40

    def read(self, va, n):
        for nm, a, vs, ro, rs in self.secs:
            if a <= va < a + vs:
                o = ro + (va - a)
                if ro == 0 and rs == 0:        # .bss-style, no raw bytes
                    return None
                if o + n > ro + rs:
                    return None
                return self.d[o:o + n]
        return None

    def section_of(self, va):
        for nm, a, vs, ro, rs in self.secs:
            if a <= va < a + vs:
                return nm
        return None


def is_funclet_like(name):
    """Mirror of objdiff-core `is_funclet_like` (diff/mod.rs)."""
    for pfx in ("__unwind$", "__catch$"):
        if name.startswith(pfx):
            return name[len(pfx):].isdigit()
    if name.startswith("__unwind__merged_"):
        return True
    m = FN_RE.match(name)
    if m:
        return True
    return name.startswith("??__E") or name.startswith("??__F")


class Oracles:
    """name -> retail VA, from the two available maps."""

    def __init__(self, root):
        self.name2va = {}
        self.va2names = defaultdict(set)
        self.ambiguous = set()

        tsm_path = os.path.join(root, "scripts", "target_symbol_map.json")
        with open(tsm_path) as f:
            tsm = json.load(f)
        for va_s, name in tsm.items():
            try:
                va = int(va_s, 16)
            except ValueError:
                continue
            self.va2names[va].add(name)
            if name in self.name2va and self.name2va[name] != va:
                self.ambiguous.add(name)
            self.name2va.setdefault(name, va)

        sym_path = os.path.join(root, "config", "45410914", "symbols.txt")
        with open(sym_path) as f:
            for line in f:
                m = SYM_LINE.match(line)
                if not m:
                    continue
                name, va = m.group(1), int(m.group(3), 16)
                if ANON_RE.match(name):
                    continue
                self.va2names[va].add(name)
                if name in self.name2va and self.name2va[name] != va:
                    self.ambiguous.add(name)
                self.name2va.setdefault(name, va)

        # VAs that carry more than one retail name are ICF folds; the map keeps
        # one name and it is routinely the wrong one, so they must not produce a
        # DIVERGENT verdict.
        self.icf_vas = {va for va, ns in self.va2names.items() if len(ns) > 1}

    def va_of_label(self, name):
        """Retail VA for a TARGET-side relocation symbol name, or None."""
        m = ANON_RE.match(name)
        if m:
            return int(m.group(1), 16)
        if name in self.ambiguous:
            return None
        return self.name2va.get(name)

    def va_of_base_name(self, name):
        """Retail VA claimed for a BASE-side (our) symbol name, or None."""
        if name in self.ambiguous:
            return None
        return self.name2va.get(name)


def symbol_extent(parser, sym_name):
    """(section_index0, offset, size) for a symbol.

    ★ Load-bearing: base objs do NOT give every funclet its own COMDAT --
    MSVC packs many `__unwind$NNN` into one `.text` section.  Taking
    "rest of the section" as the size (the naive reading) silently makes a
    40-byte funclet look like a 600-byte one and destroys every signature
    comparison.  Size is therefore derived as the gap to the next symbol in
    the same section."""
    sym = parser.symbol_map.get(sym_name)
    if sym is None or sym["section"] <= 0:
        return None
    sec_i = sym["section"] - 1
    if sec_i >= len(parser.sections):
        return None
    sec = parser.sections[sec_i]
    cache = getattr(parser, "_extent_cache", None)
    if cache is None:
        cache = parser._extent_cache = {}
    bounds = cache.get(sec_i)
    if bounds is None:
        offs = sorted({s["value"] for s in parser.symbols
                       if s["section"] - 1 == sec_i and s["storage_class"] in (2, 3)
                       and not s["name"].startswith("$")})
        bounds = cache[sec_i] = offs
    lo = sym["value"]
    hi = sec["raw_size"]
    for o in bounds:
        if o > lo:
            hi = o
            break
    return sec_i, lo, max(hi - lo, 0)


def obj_relocs_for_symbol(parser, sym_name, size_hint=None):
    """[(offset, type, target_name)] for a function symbol, or None if absent."""
    ext = symbol_extent(parser, sym_name)
    if ext is None:
        return None
    sec_i, lo, size = ext
    hi = lo + (size_hint if size_hint else size)
    out = []
    for r in parser.get_section_relocations(sec_i):
        if r["type"] in (PAIR_TYPE, ABSOLUTE_TYPE):
            continue
        if r["type"] == REL14_TYPE and r["symbol_name"] == sym_name:
            # dtk emits a REL14 relocation for an INTRA-function conditional
            # branch that MSVC resolves internally with no relocation at all.
            # Verified on ?ListAnimChildren@RndAnimFilter@@ and
            # ??4?$ObjList@UTarget@HamCamShot@@@@: pure emission artifact, and
            # the dominant cause of the SHAPE_MISMATCH bucket.
            continue
        if lo <= r["offset"] < hi:
            out.append((r["offset"] - lo, r["type"], r["symbol_name"]))
    out.sort()
    return out


def masked_signature(parser, sym_name, size_hint=None):
    """objdiff's funclet_signature: bytes with the 4-byte word at every reloc
    offset zeroed."""
    ext = symbol_extent(parser, sym_name)
    if ext is None:
        return None
    sec_i, lo, size = ext
    if size_hint:
        size = size_hint
    if size <= 0:
        return None
    data = bytearray(parser.get_section_data(sec_i)[lo:lo + size])
    if len(data) != size:
        return None
    for r in parser.get_section_relocations(sec_i):
        if lo <= r["offset"] < lo + size:
            o = r["offset"] - lo
            for k in range(o, min(o + 4, len(data))):
                data[k] = 0
    return bytes(data)


class UnitPair:
    """Lazy COFF parsers + candidate index for one objdiff unit."""

    def __init__(self, root, unit):
        self.name = unit["name"]
        self.tpath = os.path.join(root, unit["target_path"])
        self.bpath = os.path.join(root, unit["base_path"])
        self._t = self._b = None
        self._sig_index = None

    @property
    def target(self):
        if self._t is None:
            self._t = COFFParser(self.tpath)
        return self._t

    @property
    def base(self):
        if self._b is None:
            self._b = COFFParser(self.bpath)
        return self._b

    def base_sig_index(self):
        """(masked signature -> [names], size -> [names]) for funclet-like Code
        symbols in the base obj."""
        if self._sig_index is not None:
            return self._sig_index
        idx = defaultdict(list)
        by_size = defaultdict(list)
        p = self.base
        seen = set()
        for sym in p.symbols:
            n = sym["name"]
            if n in seen or sym["section"] <= 0 or not is_funclet_like(n):
                continue
            seen.add(n)
            sec = p.sections[sym["section"] - 1]
            if not sec["name"].startswith(".text"):
                continue
            sig = masked_signature(p, n)
            if sig:
                idx[sig].append(n)
                by_size[len(sig)].append(n)
        self._sig_index = (idx, by_size)
        return self._sig_index

    def candidates_for(self, tname, tsize):
        """Base symbols admissible as objdiff's partner for target `tname`.

        Exact masked-signature equality first (objdiff pass 1/2).  Then the
        looser rule that normalized diff actually enforces: bytes equal once the
        4-byte word at EVERY relocation offset on EITHER side is masked --
        which is what lets objdiff's pass-3 scored fallback land on 100.0
        normalized."""
        sig = masked_signature(self.target, tname, tsize)
        if sig is None:
            return [], "none"
        idx, by_size = self.base_sig_index()
        exact = list(idx.get(sig, []))
        if exact:
            return exact, "exact-signature"
        t_off = {o for o, _, _ in (obj_relocs_for_symbol(self.target, tname, tsize) or [])}
        near = []
        for n in by_size.get(len(sig), []):
            bsig = masked_signature(self.base, n)
            if bsig is None or len(bsig) != len(sig):
                continue
            b_off = {o for o, _, _ in (obj_relocs_for_symbol(self.base, n) or [])}
            mask = t_off | b_off
            a = bytearray(sig)
            c = bytearray(bsig)
            for o in mask:
                for k in range(o, min(o + 4, len(a))):
                    a[k] = c[k] = 0
            if bytes(a) == bytes(c):
                near.append(n)
        return near, "union-masked"


def classify_function(up, fname, fsize, oracles, consistency=None,
                      merged_tolerant=True, icf=None, image=None,
                      base_index=None, matched_names=None,
                      strict_consistency=False):
    """Return a dict verdict for one 100%-matched target function."""
    t_rel = obj_relocs_for_symbol(up.target, fname, fsize)
    if t_rel is None:
        return {"verdict": "NO_TARGET_SYM", "detail": "target symbol absent"}

    # --- find admissible base partners -------------------------------------
    cands = []
    if fname in up.base.symbol_map:
        cands = [fname]
        pairing = "name"
    elif is_funclet_like(fname):
        cands, pairing = up.candidates_for(fname, fsize)
    else:
        pairing = "unknown"

    if not cands:
        return {"verdict": "NO_BASE_PAIR", "pairing": pairing,
                "n_target_relocs": len(t_rel),
                "detail": "no admissible base partner found"}

    if not t_rel:
        return {"verdict": "NO_RELOCS", "pairing": pairing, "n_target_relocs": 0,
                "n_cands": len(cands)}

    best = None
    for cand in cands:
        b_rel = obj_relocs_for_symbol(up.base, cand)
        if b_rel is None:
            continue
        res = _compare(t_rel, b_rel, oracles, consistency, merged_tolerant,
                       icf, image, base_index, matched_names, strict_consistency)
        res["base_symbol"] = cand
        rank = {"CORRESPONDING": 0, "UNRESOLVABLE": 1, "DIVERGENT": 2,
                "SHAPE_MISMATCH": 3}[res["verdict"]]
        if best is None or rank < best[0]:
            best = (rank, res)
        if rank == 0:
            break
    if best is None:
        return {"verdict": "NO_BASE_PAIR", "pairing": pairing,
                "n_target_relocs": len(t_rel)}
    res = best[1]
    res["pairing"] = pairing
    res["n_cands"] = len(cands)
    res["n_target_relocs"] = len(t_rel)
    return res


def _compare(t_rel, b_rel, oracles, consistency, merged_tolerant, icf=None,
             image=None, base_index=None, matched_names=None,
             strict_consistency=False):
    if len(t_rel) != len(b_rel) or [(o, t) for o, t, _ in t_rel] != [(o, t) for o, t, _ in b_rel]:
        return {"verdict": "SHAPE_MISMATCH",
                "detail": f"{len(t_rel)} target relocs vs {len(b_rel)} base relocs"}
    slots = []
    n_corr = n_div = n_unk = 0
    for (off, ty, tname), (_, _, bname) in zip(t_rel, b_rel):
        tva = oracles.va_of_label(tname)
        st = _slot(tva, tname, bname, oracles, consistency, merged_tolerant,
                   icf, image, base_index, matched_names, strict_consistency)
        slots.append({"offset": off, "type": ty, "target": tname,
                      "base": bname, "target_va": tva, "state": st[0],
                      "why": st[1], "oracle": st[2]})
        if st[0] == "CORRESPOND":
            n_corr += 1
        elif st[0] == "DIVERGE":
            n_div += 1
        else:
            n_unk += 1
    # Divergence confidence == which oracle spoke.  content > consistency > map.
    order = {"content": 3, "consistency": 2, "map": 1, "name": 0}
    lvl = 0
    for sl in slots:
        if sl["state"] == "DIVERGE":
            lvl = max(lvl, order.get(sl.get("oracle"), 0))
    div_conf = {3: "content", 2: "consistency", 1: "map", 0: "none"}[lvl]
    if n_div:
        verdict = "DIVERGENT"
    elif n_unk:
        verdict = "UNRESOLVABLE"
    else:
        verdict = "CORRESPONDING"
    return {"verdict": verdict, "n_correspond": n_corr, "n_diverge": n_div,
            "n_undecided": n_unk, "div_conf": div_conf, "slots": slots}


def _content_check(bname, tva, image, base_index, matched_names=None):
    """★ THE DECISIVE TEST.  Compare the bytes of OUR symbol `bname` with the
    retail bytes at the address the target relocation actually points to.

    Returns ("CORRESPOND"|"DIVERGE"|None, why).

    Both sides are masked at OUR symbol's own relocation offsets, because those
    operand fields hold link-time addresses that legitimately differ.  What
    survives the mask is the instruction encoding / literal content -- exactly
    the thing that must be equal if the pointer really points at the same
    object.

    This test dissolves the ICF confounder rather than tolerating it: if retail
    folded our callee with another COMDAT, the bytes at the folded address ARE
    our callee's bytes, so the slot correctly reads CORRESPOND no matter which
    name `target_symbol_map.json` kept.

    It abstains (None) when we cannot make the comparison honest:
      - the symbol is not defined in our tree (extern / XDK import)
      - the VA is outside the image or in a .bss-style section with no raw bytes
      - the symbol carries relocations of its own AND is a DATA symbol (vtables,
        RTTI, pointer tables): retail holds resolved absolute addresses there
        while our obj holds zeros, so equality is unachievable in principle.
    """
    import hashlib
    d = base_index.get(bname)
    if d is None or image is None:
        return None, None
    kind, size, roff, sha = d[0], d[1], d[2], d[3]
    if kind != ".text" and roff:
        return None, "data symbol with own relocations; content not comparable"
    if kind == ".text":
        # ★ A CODE symbol's bytes only certify a POINTER if our version of that
        # callee already reproduces retail exactly.  Otherwise a byte
        # difference at the target VA means "we have not ported the callee",
        # which says nothing about whether the pointer is aimed correctly.
        # Applying the content test to unmatched callees is what turned a 3.4%
        # divergence reading into a false 20.4% one -- it was measuring the
        # callees, not the relocations.
        if matched_names is None or bname not in matched_names:
            return None, "callee not itself a 100% match; content not probative"
    if len(d) > 6 and d[6]:
        # ★ ANTI-VACUITY.  An all-zero data object (uninitialised static, guard
        # word, zeroed table) matches the retail image at almost any .data/.bss
        # address, so "the bytes agree" carries no information about WHICH
        # object is pointed at.  Measured: 1,977 slots across 936 functions were
        # being certified by this vacuous comparison before the guard existed.
        return None, "our symbol is all zeroes; content comparison is vacuous"
    raw = image.read(tva, size)
    if raw is None or len(raw) != size:
        return None, "retail VA not readable (outside image or uninitialised)"
    if kind == ".text" and not image.section_of(tva).startswith(".text"):
        return "DIVERGE", "code symbol but target VA is not in .text"
    buf = bytearray(raw)
    for o in roff:
        for k in range(o, min(o + 4, size)):
            buf[k] = 0
    if hashlib.sha1(bytes(buf)).digest() == sha:
        return "CORRESPOND", "retail bytes at target VA equal our symbol's bytes"
    if kind != ".text":
        # ★ EXTENT GUARD.  A data symbol's size is derived as the gap to the
        # next symbol in the same section, which OVER-READS when MSVC leaves
        # padding or an unnamed neighbour.  Measured on the positive control:
        # `gSaveRev_RndCubeTex` reads 8 bytes {2,0} where only the first word is
        # really ours.  If the head agrees and every difference lies inside our
        # own all-zero tail, the comparison is inconclusive, not divergent.
        zt = d[4] if len(d) > 4 else 0
        head_sha = d[5] if len(d) > 5 else sha
        if zt and hashlib.sha1(bytes(buf[:size - zt])).digest() == head_sha:
            return None, ("differs only inside our zero padding tail; symbol "
                          "extent not trustworthy")
    return "DIVERGE", "retail bytes at target VA differ from our symbol's bytes"


def _slot(tva, tname, bname, oracles, consistency, merged_tolerant, icf=None,
          image=None, base_index=None, matched_names=None,
          strict_consistency=False):
    """Decide one relocation slot.  Oracles in order of strength."""
    if tva is None:
        return ("UNDECIDED", "target reloc symbol has no resolvable VA", "-")
    # 0. NAME IDENTITY.  The renamer already resolved this retail VA to a
    #    mangled name (independent evidence from the identification pipeline);
    #    if it is the same name our call site uses, the two oracles corroborate
    #    and nothing weaker should be allowed to overturn that.
    if tname == bname:
        return ("CORRESPOND", "target VA is named identically to our symbol",
                "name")
    # 1. CONTENT -- byte comparison against the retail image. Decisive.
    if base_index is not None:
        st, why = _content_check(bname, tva, image, base_index, matched_names)
        if st is not None:
            return (st, why, "content")
    # 2. CONSISTENCY -- the same base symbol seen against the same target VA
    #    from many independent functions. Overrides the map, which is a pairing
    #    aid and is known to be wrong at ICF-folded and mis-anchored VAs.
    if consistency is not None:
        bound = consistency.get(bname)
        if bound is not None:
            va, n_obs, n_distinct = bound[0], bound[1], bound[2]
            n_named = bound[3] if len(bound) > 3 else n_obs
            if strict_consistency:
                n_obs = n_named
            if va == tva and n_obs >= 2:
                return ("CORRESPOND", f"{n_obs} distinct functions bind "
                        f"{bname} to this VA", "consistency")
            if strict_consistency and n_named < 2:
                return ("UNDECIDED", "consistency support is entirely "
                        "over-subscribed funclets", "consistency")
            if n_distinct > 1 and va != tva:
                return ("DIVERGE", f"base {bname} is bound to 0x{va:08X} "
                        f"({n_obs} obs) but this slot points 0x{tva:08X}",
                        "consistency")
    # 3. MAP -- weakest; only used when nothing else spoke.
    bva = oracles.va_of_base_name(bname)
    if bva is not None:
        if bva == tva:
            return ("CORRESPOND", "target_symbol_map name->VA agrees", "map")
        if merged_tolerant:
            if tva in oracles.icf_vas or bva in oracles.icf_vas:
                return ("UNDECIDED", "ICF-folded VA; map name is unreliable", "map")
            if icf is not None and icf[0].get(bname):
                return ("UNDECIDED",
                        "base symbol is byte-shared with another symbol "
                        "(ICF-capable); map name unreliable", "map")
        return ("DIVERGE", f"map places {bname} at 0x{bva:08X}, "
                f"target points 0x{tva:08X}", "map")
    return ("UNDECIDED", "no oracle for base symbol", "-")


def build_base_index(units, verbose=False):
    """name -> BaseDef(kind, size, reloc_offsets, masked_sha1), plus ICF
    capability (is this body byte-shared with another NAME in our tree).

    Names defined more than once with DIFFERENT bodies are dropped (`conflict`)
    rather than guessed at."""
    """name -> True if this base symbol's bytes are shared with >=1 OTHER base
    symbol name anywhere in our tree, i.e. the linker COULD have folded them.

    ★ This is the single largest false-positive suppressor in the tool.  Retail
    ICF folds byte-identical COMDATs to one address; `target_symbol_map.json`
    then keeps ONE of the folded names for that VA, and it is routinely not the
    one our call site uses.  Without this filter, every `lwz r3,N(r3); blr`
    accessor and every same-shape template instantiation reads as DIVERGENT.
    Measured on the full census: it removes ~64% of raw divergence claims.

    Suppression is deliberately asymmetric: being ICF-capable makes a
    disagreement UNDECIDED, never CORRESPONDING."""
    import hashlib
    sig2names = defaultdict(set)
    defs = {}
    conflict = set()
    for uname, up in units.items():
        try:
            p = up.base
        except Exception:
            continue
        seen = set()
        for sym in p.symbols:
            n = sym["name"]
            if n in seen or sym["section"] <= 0 or sym["storage_class"] not in (2, 3):
                continue
            seen.add(n)
            sec = p.sections[sym["section"] - 1]
            kind = sec["name"]
            ext = symbol_extent(p, n)
            if ext is None:
                continue
            si, lo, size = ext
            if size <= 0 or size > 65536:
                continue
            if kind.startswith(".text"):
                sig = masked_signature(p, n)
            elif kind.startswith((".rdata", ".data")):
                sig = p.get_section_data(si)[lo:lo + size]
            else:
                continue
            if not sig or len(sig) != size:
                continue
            if n.startswith("??_C@") and b"\x00" in sig:
                # String literal COMDATs are padded to alignment; retail's
                # padding is whatever the neighbouring pool entry is, so compare
                # through the NUL terminator only.
                size = sig.index(b"\x00") + 1
                sig = sig[:size]
            roff = tuple(sorted({r["offset"] - lo
                                 for r in p.get_section_relocations(si)
                                 if lo <= r["offset"] < lo + size}))
            h = (kind[:6], size, hashlib.sha1(sig).digest())
            sig2names[h].add(n)
            zt = 0
            if not kind.startswith(".text"):
                while zt < size and sig[size - 1 - zt] == 0:
                    zt += 1
            head_sha = hashlib.sha1(sig[:size - zt]).digest() if zt else h[2]
            all_zero = not kind.startswith(".text") and not any(sig)
            prev = defs.get(n)
            if prev is None:
                defs[n] = (kind[:6], size, roff, h[2], zt, head_sha, all_zero)
            elif prev[3] != h[2] or prev[1] != size:
                conflict.add(n)
    for n in conflict:
        defs.pop(n, None)
    cap = {n: len(sig2names[(v[0], v[1], v[3])]) > 1 for n, v in defs.items()}
    if verbose:
        print(f"[base-index] {len(defs)} base symbols with unambiguous bodies "
              f"({len(conflict)} dropped as multi-body); "
              f"{sum(1 for v in cap.values() if v)} are byte-shared (ICF-capable)",
              file=sys.stderr)
    return cap, defs


def _is_named_observer(name):
    """A relocation observation is INDEPENDENT evidence only if the observing
    function is itself a named body. An EH funclet / `fn_<VA>` crumb reaches
    100% through objdiff's many-to-one masked pairing, so a crowd of them all
    pointing at one base symbol is ONE over-subscribed pairing wearing many
    hats, not N independent corroborations. Measured on `DirLoader`: 33
    `fn_<VA>` funclets "supported" one local guard binding."""
    return not (FN_RE.match(name) or name.startswith(
        ("__unwind$", "__catch$", "??__E", "??__F")))


def build_consistency(root, units, fn_index, oracles, verbose=False):
    """Observe every (base symbol name -> target VA) pairing across the census
    and return name -> (modal VA, n_distinct_FUNCTIONS supporting it,
    n_distinct_VAs).

    ★ The support count is over DISTINCT FUNCTIONS, not slots.  A guard word
    referenced three times inside one 88-byte accessor is ONE observation, not
    three: counting slots would make a function corroborate itself, which is a
    tautology and was measured to inflate CORRESPONDING by ~7 points before it
    was fixed."""
    obs = defaultdict(lambda: defaultdict(set))
    for uname, up in units.items():
        fns = fn_index.get(uname)
        if not fns:
            continue
        try:
            _ = up.target, up.base
        except Exception:
            continue
        for fname, fsize in fns:
            t_rel = obj_relocs_for_symbol(up.target, fname, fsize)
            if not t_rel:
                continue
            if fname in up.base.symbol_map:
                cands = [fname]
            elif is_funclet_like(fname):
                cands = up.candidates_for(fname, fsize)[0]
            else:
                cands = []
            if len(cands) != 1:
                # ambiguous partner -> do not let it pollute the binding
                continue
            b_rel = obj_relocs_for_symbol(up.base, cands[0])
            if not b_rel or len(b_rel) != len(t_rel):
                continue
            for (off, ty, tname), (_, _, bname) in zip(t_rel, b_rel):
                tva = oracles.va_of_label(tname)
                if tva is None:
                    continue
                obs[bname][tva].add((uname, fname))
    out = {}
    for bname, c in obs.items():
        va, fns = max(c.items(), key=lambda kv: (len(kv[1]), -kv[0]))
        n_named = sum(1 for _, fn in fns if _is_named_observer(fn))
        out[bname] = (va, len(fns), len(c), n_named)
    if verbose:
        multi = sum(1 for v in out.values() if v[2] > 1)
        print(f"[consistency] {len(out)} base symbols observed, {multi} bound to "
              f">1 distinct target VA", file=sys.stderr)
    return out


def load_units(root):
    with open(os.path.join(root, "objdiff.json")) as f:
        d = json.load(f)
    live = live_target_paths(root)
    units = {}
    for u in d.get("units", []):
        tp = u.get("target_path")
        bp = u.get("base_path")
        if not tp or not bp:
            continue
        if os.path.normpath(os.path.join(root, tp)) not in live:
            continue
        if not (os.path.exists(os.path.join(root, tp)) and
                os.path.exists(os.path.join(root, bp))):
            continue
        units[u["name"]] = UnitPair(root, u)
    return units


def load_matched(root, va_ranges=None, oracles=None):
    """unit -> [(fn name, size)] for every function at 100.0 normalized."""
    with open(os.path.join(root, "build", "45410914", "report.json")) as f:
        rep = json.load(f)
    idx = defaultdict(list)
    meta = {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            if f.get("match_percent_normalized") != 100.0:
                continue
            va = oracles.va_of_label(f["name"]) if oracles else None
            if va_ranges is not None:
                if va is None or not any(a <= va < b for a, b in va_ranges):
                    continue
            idx[u["name"]].append((f["name"], int(f["size"])))
            meta[(u["name"], f["name"])] = {"size": int(f["size"]), "va": va,
                                            "categories": u.get("metadata", {}).get(
                                                "progress_categories", [])}
    return idx, meta


SIZE_BANDS = [(0, 16, "<=16B trivial"), (16, 48, "17-48B"), (48, 128, "49-128B"),
              (128, 512, "129-512B"), (512, 1 << 30, ">512B")]


def band(sz):
    for lo, hi, nm in SIZE_BANDS:
        if lo < sz <= hi or (lo == 0 and sz <= hi):
            return nm
    return "?"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--worktree", default=os.getcwd())
    ap.add_argument("--symbol")
    ap.add_argument("--unit")
    ap.add_argument("--census", action="store_true")
    ap.add_argument("--va-ranges", help="JSON [[lo,hi],...] retail VA filter")
    ap.add_argument("--out")
    ap.add_argument("--no-consistency", action="store_true",
                    help="disable the data-symbol injectivity oracle")
    ap.add_argument("--no-merged-tolerant", action="store_true")
    ap.add_argument("--strict-consistency", action="store_true",
                    help="require >=2 NAMED (non-funclet) supporting functions "
                         "for a consistency binding -- the conservative bound")
    ap.add_argument("--no-content", action="store_true",
                    help="disable the retail-image byte oracle")
    ap.add_argument("--no-icf", action="store_true",
                    help="disable ICF-capability suppression (raw, over-rejecting)")
    ap.add_argument("--limit", type=int)
    args = ap.parse_args()

    root = os.path.abspath(args.worktree)
    oracles = Oracles(root)
    units = load_units(root)

    ranges = None
    if args.va_ranges:
        ranges = [tuple(x) for x in json.load(open(args.va_ranges))]

    fn_index, meta = load_matched(root, ranges, oracles)
    _all_idx, _ = load_matched(root, None, oracles)
    matched_names = {n for v in _all_idx.values() for n, _ in v}
    if args.unit:
        fn_index = {k: v for k, v in fn_index.items() if k == args.unit}
    if args.symbol:
        fn_index = {k: [(n, s) for n, s in v if n == args.symbol]
                    for k, v in fn_index.items()}
        fn_index = {k: v for k, v in fn_index.items() if v}
    fn_index = {k: v for k, v in fn_index.items() if k in units}

    n_total = sum(len(v) for v in fn_index.values())
    print(f"[scope] {len(fn_index)} live units, {n_total} functions at 100.0% "
          f"normalized", file=sys.stderr)

    image = None
    img_path = os.path.join(root, "orig", "45410914", "band.exe")
    if os.path.exists(img_path) and not args.no_content:
        image = RetailImage(img_path)
        print(f"[image] {img_path} imagebase 0x{image.base:08X}", file=sys.stderr)
    else:
        print("[image] band.exe absent or disabled -- content oracle OFF; "
              "verdicts fall back to consistency+map only", file=sys.stderr)

    icf = base_index = None
    if not args.no_icf:
        icf = build_base_index(units, verbose=True)
        base_index = icf[1]

    consistency = None
    if not args.no_consistency:
        # The binding must be learned from the WHOLE census, not the filtered
        # subset, or a VA range filter would silently blind the oracle.
        full_index, _ = load_matched(root, None, oracles)
        full_index = {k: v for k, v in full_index.items() if k in units}
        if args.limit:
            full_index = dict(list(full_index.items())[:args.limit])
        consistency = build_consistency(root, units, full_index, oracles, verbose=True)

    rows = []
    for uname, fns in sorted(fn_index.items()):
        up = units[uname]
        try:
            _ = up.target, up.base
        except Exception as e:
            for fname, fsize in fns:
                rows.append({"unit": uname, "name": fname, "size": fsize,
                             "verdict": "OBJ_ERROR", "detail": str(e)})
            continue
        for fname, fsize in fns:
            r = classify_function(up, fname, fsize, oracles, consistency,
                                  not args.no_merged_tolerant, icf, image,
                                  base_index, matched_names,
                                  args.strict_consistency)
            m = meta.get((uname, fname), {})
            r.update({"unit": uname, "name": fname, "size": fsize,
                      "va": m.get("va"), "categories": m.get("categories", [])})
            rows.append(r)

    if args.symbol or (args.unit and not args.census):
        for r in rows:
            print(json.dumps(r, indent=1, default=str))
    else:
        _report(rows)

    if args.out:
        with open(args.out, "w") as f:
            json.dump(rows, f, default=str)
        print(f"[out] {args.out}", file=sys.stderr)


def _report(rows):
    c = Counter(r["verdict"] for r in rows)
    n = len(rows)
    print(f"\n=== reloc correspondence census: {n} functions at 100.0% ===")
    for k, v in c.most_common():
        print(f"  {k:<16} {v:>7}  {100.0*v/max(n,1):5.1f}%")

    def strat(key, label):
        print(f"\n-- by {label} --")
        groups = defaultdict(Counter)
        for r in rows:
            groups[key(r)][r["verdict"]] += 1
        keys = sorted(groups, key=lambda k: -sum(groups[k].values()))
        hdr = ["CORRESPONDING", "DIVERGENT", "UNRESOLVABLE", "NO_RELOCS",
               "SHAPE_MISMATCH", "NO_BASE_PAIR", "NO_TARGET_SYM"]
        print(f"  {'':<22}" + "".join(f"{h[:12]:>13}" for h in hdr) + f"{'total':>8}")
        for k in keys:
            g = groups[k]
            print(f"  {str(k)[:22]:<22}" + "".join(f"{g.get(h,0):>13}" for h in hdr)
                  + f"{sum(g.values()):>8}")

    strat(lambda r: band(r["size"]), "size band")
    strat(lambda r: (r.get("categories") or ["?"])[0], "unit tier")
    strat(lambda r: "funclet-like" if is_funclet_like(r["name"]) else "named body",
          "funclet vs body")


if __name__ == "__main__":
    main()
