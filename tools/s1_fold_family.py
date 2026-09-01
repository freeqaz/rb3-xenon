#!/usr/bin/env python3
"""S1-FOLDTOOL -- adjudicate a METHOD FAMILY of relocation-name charges on RETAIL
BYTES to PROVEN_FOLD / REFUTED / UNDECIDED.

THE PROBLEM
===========
Under the shipped `name_check` ruler objdiff charges a site where retail's object
relocates a call to symbol A and ours relocates it to symbol B.  Class E (rows
whose ONLY charges are such names) is the largest addressable structural slice.
Each charge is one of:

  FOLD    /OPT:ICF merged two identical COMDATs; the surviving map name is
          ARBITRARY.  Our source is right; an alias would be legitimate.
  MAP     addr(A) is misidentified in target_symbol_map.json.
  CALLEE  our source genuinely calls the wrong function -- a real bug, and the
          most valuable thing this tool can find.

The proof is necessarily PER PAIR.  The work is mechanizable PER FAMILY, because
58% of charged instances are same-method template siblings (`push_back`,
`insert`, `~`, `_M_fill_insert`, `resize`, `clear`).  Hence: family in, per-pair
adjudication out.

⛔⛔ WHY A `REFUSAL` IS A FIRST-CLASS OUTCOME AND NOT A FAILURE
==============================================================
A FABRICATED alias lifts `name_check` BY CONSTRUCTION, and the `none`-ruler
control CANNOT catch it (`none` ignores relocation names, so it reads +0 by
construction -- that flatness is the SIGNATURE of the hazard, not a clearance).
So the only thing standing between this tool and a silently-inflated score is its
willingness to say UNDECIDED.  Every ambiguity resolves to UNDECIDED, never to a
fold.

⛔⛔ NEVER MASK A RELOCATION -- AND THAT RULE IS ENFORCED BY A TYPE, NOT BY
DISCIPLINE
==========================================================================
Lane W33 produced a false `IDENTICAL` (`??1ObjRefConcrete<FlowLabel>` vs our
`<EventTrigger>`) because it FORGAVE a placeholder relocation -- and the datum
distinguishing two template instantiations is frequently ANONYMOUS (a vtable or
RTTI pointer).  Lane W34 found `symbol_aliases.json` carrying a T1-"proven"
group at 0x826936f8 whose two members BRANCH TO DIFFERENT FUNCTIONS: for a
thunk, the destination IS the whole information content.  Measured on this
binary, 80 groups of 12-24 byte forwarders share identical MASKED bytes while
differing in destination -- the largest with 661 members.

Both are one bug: masking or forgiving the one word that carries the
discriminator.  The record's rule is "never mask a relocation; if you cannot
resolve it, return UNDECIDED" -- but a rule you must REMEMBER to apply is not a
guard.  So an unresolvable relocation target is represented by `Unresolved`,
whose `__eq__`/`__ne__`/`__hash__` RAISE.  Any comparison path that touches one
cannot silently answer; it throws, and the adjudicator converts the throw into
UNDECIDED.  The unsafe operation is not merely discouraged, it is unavailable.

THE SIZE TRAP (lane STLPORT-1 -- it cost six wrongly-withdrawn folds)
=====================================================================
"Different-size COMDATs cannot fold" is TRUE but a size test CANNOT catch a
one-sided reader artifact, because the artifact cancels on both legs.  The two
sides are not natively the same measurement:

  * OUR obj is MSVC /Gy: one COMDAT per function -- body plus EH funclets.
  * The TARGET obj is a dtk split: ONE .text per unit, so an extent must be
    derived as [sym, next boundary sym).  That extent can absorb the NEXT
    function's 8-byte EH prefix -- exactly the artifact that billed a
    successor's funclet into a COMDAT span and manufactured a phantom "+8 B".

⚠ AND THE CARVE ITSELF IS KNOWN-DEFECTIVE: dtk truncates a carve at an
unconditional `bctr`, dropping a trailing `blr`, which made
`w33_fold_adjudicate.py` return a confident false DIFFERENT.  So a size
disagreement that looks like that defect is UNDECIDED_EXTENT, never REFUTED.
Both extents are derived by the SAME rule and the normalization is REPORTED.

WHAT COUNTS AS PROOF HERE (strongest first)
===========================================
T-OURICF  Our COMDAT for A is byte- AND relocation-target-identical to our
          COMDAT for B.  That IS /OPT:ICF's folding condition, applied to two of
          our own COMDATs, so the linker MUST fold them.  Both sides carry the
          same relocations to the same symbols, so retail's anonymous labels
          never enter the argument (lane ALLOCGATE-1's move).
T-INCONS  Retail's body at addr(A) calls functions whose names imply TWO OR MORE
          distinct template instantiations.  Three `T` in one function is
          impossible without folding -- this needs no fold model at all.
T-RETAIL  Retail's body at addr(A) equals our B, relocation target names
          COMPARED (never masked), with zero Unresolved operands.

WHAT COUNTS AS REFUTATION
=========================
R-TWOADDR The map places A and B at DIFFERENT retail addresses => retail holds
          both functions separately => they did not fold.  Corroborated by
          retail-vs-retail body comparison, which is the SOUND test (one naming
          scheme, one extent rule, so the one-sided hazard cannot apply).
R-BODY    Retail's body at addr(A) differs from our B in a non-relocated word.
R-NAME    Bodies equal but a resolved (non-placeholder) relocation target
          differs => a wrong callee or a genuinely different function.

⛔ THIS TOOL INSTALLS NOTHING.  Existing alias coverage is REPORTED, never
applied -- those groups are the thing under audit, and applying them would let a
fabricated alias validate itself.
"""
import argparse
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts" / "analysis"))
from scripts.analysis.coffx import read_coff  # noqa: E402
from cascade_price import is_placeholder  # noqa: E402


# --------------------------------------------------------------------------
# The guard that is a TYPE, not a habit.
# --------------------------------------------------------------------------
class UnresolvedComparison(Exception):
    """Raised when code tries to compare a relocation target it cannot resolve."""


class Unresolved:
    """A relocation target whose identity is NOT established.

    Comparing it raises.  This is the point: the record's rule is "never mask a
    relocation -- if you cannot resolve it, return UNDECIDED", and a rule that
    depends on a caller remembering to check is not a guard.  Here the unsafe
    operation is simply unavailable: any `==` against an Unresolved throws, the
    adjudicator catches it, and the pair becomes UNDECIDED.
    """

    __slots__ = ("why", "shown")

    def __init__(self, why, shown=""):
        self.why = why
        self.shown = shown

    def __eq__(self, other):
        raise UnresolvedComparison(
            "refusing to compare an unresolved relocation target (%s: %r)"
            % (self.why, self.shown))

    __ne__ = __eq__

    def __hash__(self):
        raise UnresolvedComparison(
            "refusing to hash an unresolved relocation target (%s)" % self.why)

    def __repr__(self):
        return "<Unresolved %s %r>" % (self.why, self.shown)


def reloc_identity(name_a, name_b):
    """Identity values for one relocation slot, as a comparable pair.

    A placeholder name (`lbl_`/`fn_`/`data_`/...) carries NO identity: retail's
    anonymous data label and our `__real@3f8020c5` float COMDAT can be the same
    datum, or a per-type vtable pointer that is the ONLY discriminator between
    two template instantiations.  objdiff forgives placeholders because for
    SCORING they cannot be spelled wrong.  For FOLD PROOF that forgiveness is
    unsound -- it is what made W33's tool report a false IDENTICAL.

    So: placeholder-vs-named yields `Unresolved` on that slot, and comparing it
    raises.  Placeholder-vs-placeholder is ALSO Unresolved -- two anonymous
    labels are not evidence of sameness.
    """
    pa, pb = is_placeholder(name_a), is_placeholder(name_b)
    if pa or pb:
        u = Unresolved("placeholder relocation target", (name_a, name_b))
        return u, u
    return name_a, name_b


# --------------------------------------------------------------------------
# COFF extent derivation -- both legs by the SAME rule.
# --------------------------------------------------------------------------
def is_boundary_symbol(name):
    """Does `name` start a NEW body, bounding the previous one?

    ⛔ Wrong in EITHER direction silently makes the two legs different
    measurements.  TOO PERMISSIVE: MSVC emits `$M<n>` line/scope LABELS inside a
    function; treating one as a boundary truncates mid-function.  TOO STRICT:
    our COMDAT holds the parent body AND its EH funclets while the dtk extent
    stops at the next symbol, and the resulting size deltas come out as
    multiples of 40 -- the funclet size.
    """
    if not name or name.startswith("$M"):
        return False
    if name.startswith(".") or name.startswith("@"):
        return False
    return True


def strip_tail_padding(body, relocs):
    n = len(body)
    relmax = max([r[0] for r in relocs], default=-1)
    while n >= 4:
        if (n - 4) <= relmax:
            break
        if body[n - 4:n] in (b"\x00\x00\x00\x00", b"\x60\x00\x00\x00"):
            n -= 4
        else:
            break
    return body[:n], (len(body) - n)


def strip_eh_prefix_tail(body, relocs):
    """Drop a trailing 8-byte EH prefix belonging to the NEXT function.

    An 8-byte prefix (a .text pointer + an .rdata pointer) precedes a function;
    when an extent is derived as [sym, next sym) it lands at the END of the
    previous symbol's span.  Signature: exactly 2 relocations at len-8, len-4.
    """
    n = len(body)
    if n < 8:
        return body, 0
    offs = {r[0] for r in relocs}
    if (n - 8) in offs and (n - 4) in offs:
        return body[:n - 8], 8
    return body, 0


# PowerPC, decoded by RAW WORD MASKING -- never capstone, which silently drops
# VMX128 words (a dropped row is a false negative shaped like a clean result).
def _op(w):
    return (w >> 26) & 0x3F


def ends_in_unconditional_flow(body):
    """Does the body end in `blr`/`bctr`/unconditional `b`?

    Used ONLY to recognise the known dtk carve defect (a carve truncated at an
    unconditional `bctr`, dropping a trailing `blr`), so that shape becomes
    UNDECIDED_EXTENT instead of a confident false DIFFERENT.
    """
    if len(body) < 4:
        return False
    w = int.from_bytes(body[-4:], "big")
    if _op(w) == 18:                      # b / ba / bl / bla
        return True
    if _op(w) == 19:                      # bclr / bcctr family
        xo = (w >> 1) & 0x3FF
        if xo in (16, 528) and ((w >> 21) & 0x1F) == 20:   # unconditional
            return True
    return False


def extract(objpath, name, is_target_split):
    """(body, relocs, note) for `name`; relocs = [(off, type, target_name)]."""
    data = Path(objpath).read_bytes()
    secs, syms = read_coff(data)
    if secs is None:
        return None, None, "not a COFF object"
    byidx = {s.index: s.name for s in syms}
    target = None
    for s in syms:
        if s.name == name and s.sec > 0 and secs[s.sec - 1].is_code:
            target = s
            break
    if target is None:
        return None, None, "symbol absent or not in a code section"
    sec = secs[target.sec - 1]
    start = target.value
    nxt = [s.value for s in syms
           if s.sec == target.sec and s.value > start
           and is_boundary_symbol(s.name)]
    end = min(nxt) if nxt else len(sec.data)
    body = sec.data[start:end]
    rel = sorted((va - start, typ, byidx.get(si, "?%d" % si))
                 for va, si, typ in sec.relocs if start <= va < end)
    notes = []
    if is_target_split:
        body2, cut = strip_eh_prefix_tail(body, rel)
        if cut:
            body = body2
            rel = [r for r in rel if r[0] < len(body)]
            notes.append("stripped 8B EH prefix of next fn")
    body, pad = strip_tail_padding(body, rel)
    if pad:
        notes.append("stripped %dB trailing padding" % pad)
    return body, rel, "; ".join(notes)


def normalize(body, relocs):
    """Zero the relocated WORD at each site.

    The relocated words are the only bytes that legitimately differ between two
    copies of one function at different addresses.  Their target NAMES are
    compared separately and are NEVER masked -- masking them is precisely what
    makes a wrong callee look identical.
    """
    b = bytearray(body)
    for off, _t, _n in relocs:
        if 0 <= off <= len(b) - 4:
            b[off:off + 4] = b"\x00\x00\x00\x00"
    return bytes(b)


def compare_bodies(ba, ra, bb, rb):
    """Compare two (body, relocs).  Returns (verdict, why, detail).

    verdict in {EQUAL, DIFF_SIZE, DIFF_BODY, DIFF_NAME, UNDECIDED_EXTENT}
    Raises UnresolvedComparison if identity of any relocation slot is unknown --
    the caller turns that into UNDECIDED.
    """
    if len(ba) != len(bb):
        # ⚠ The known dtk carve defect truncates at an unconditional `bctr`,
        # dropping a trailing `blr`.  A size gap with that shape is NOT
        # evidence of anything -- it is our reader, and a confident DIFFERENT
        # here is exactly the false negative that defect already produced once.
        short, lng = (ba, bb) if len(ba) < len(bb) else (bb, ba)
        if (len(lng) - len(short)) <= 8 and lng[:len(short)] == short \
                and ends_in_unconditional_flow(short):
            return ("UNDECIDED_EXTENT",
                    "size differs by %d B but the shorter body is a PREFIX of "
                    "the longer and ends in unconditional flow -- the known dtk "
                    "carve-truncation shape; refusing to call this a difference"
                    % (len(lng) - len(short)), {})
        return ("DIFF_SIZE",
                "size differs (%d vs %d) after identical normalization -- "
                "different-size COMDATs cannot fold" % (len(ba), len(bb)), {})
    na, nb = normalize(ba, ra), normalize(bb, rb)
    if na != nb:
        nw = sum(1 for i in range(0, len(na) - 3, 4)
                 if na[i:i + 4] != nb[i:i + 4])
        return ("DIFF_BODY", "%d/%d non-relocated words differ"
                % (nw, len(na) // 4), {})
    if len(ra) != len(rb):
        return ("DIFF_NAME", "relocation COUNT differs (%d vs %d)"
                % (len(ra), len(rb)), {})
    diffs = []
    for (oa, _ta, name_a), (ob, _tb, name_b) in zip(ra, rb):
        if oa != ob:
            return ("DIFF_NAME", "relocation OFFSETS differ (%d vs %d)"
                    % (oa, ob), {})
        # May RAISE -- by design.  An unresolved slot must not be answerable.
        va, vb = reloc_identity(name_a, name_b)
        if va != vb:
            diffs.append({"off": oa, "a": name_a, "b": name_b})
    if diffs:
        return ("DIFF_NAME",
                "bodies identical but %d resolved relocation TARGET NAMES "
                "differ" % len(diffs), {"name_diffs": diffs[:8]})
    return ("EQUAL", "bodies and all %d resolved relocation targets equal"
            % len(ra), {})


# --------------------------------------------------------------------------
# Template-type extraction, for the internal-inconsistency proof.
# --------------------------------------------------------------------------
def template_args(sym):
    """Crude MSVC template-argument slice of a mangled name.

    Deliberately crude AND deliberately only ever used to prove INCONSISTENCY
    (two different values), never sameness -- so a false 'same' is impossible
    and a false 'different' is the thing the corroboration step must catch.
    """
    m = re.search(r"@\?\$([A-Za-z0-9_]+)@(.*)", sym)
    if not m:
        return None
    return m.group(2)


# --------------------------------------------------------------------------
# Indexes
# --------------------------------------------------------------------------
def build_index(dirs, cache, force=False):
    """symbol name -> [obj paths]."""
    cp = Path(cache)
    if cp.exists() and not force:
        try:
            return {k: [Path(p) for p in v]
                    for k, v in json.loads(cp.read_text()).items()}
        except Exception:
            pass
    idx = collections.defaultdict(list)
    for d in dirs:
        for p in sorted(Path(d).rglob("*.obj")):
            try:
                secs, syms = read_coff(p.read_bytes())
            except Exception:
                continue
            if secs is None:
                continue
            for s in syms:
                if s.sec > 0 and s.name:
                    idx[s.name].append(p)
    cp.parent.mkdir(parents=True, exist_ok=True)
    cp.write_text(json.dumps({k: [str(x) for x in v]
                              for k, v in idx.items()}))
    return idx


_ALIASES = None


def alias_covers(a, b):
    """Are a and b already in one group of symbol_aliases.json? (REPORTED only.)"""
    global _ALIASES
    if _ALIASES is None:
        try:
            al = json.loads((ROOT / "scripts" / "symbol_aliases.json").read_text())
            _ALIASES = [set([g["survivor"]] + (g.get("folded") or []))
                        for g in al.get("groups", [])]
        except Exception:
            _ALIASES = []
    return any(a in g and b in g for g in _ALIASES)


# --------------------------------------------------------------------------
# The adjudicator
# --------------------------------------------------------------------------
class Adjudicator:
    def __init__(self, proj):
        self.proj = Path(proj).resolve()
        cachedir = Path.home() / "tmp" / "s1fold-cache"
        self.tidx = build_index([self.proj / "build/45410914/obj"],
                                cachedir / "target_idx.json")
        self.oidx = build_index([self.proj / "build/45410914/src"],
                                cachedir / "our_idx.json")
        m = json.loads((self.proj / "scripts/target_symbol_map.json").read_text())
        self.addr_of = {}
        self.name_at = {}
        for addr, val in m.items():
            nm = val if isinstance(val, str) else (
                val.get("name") if isinstance(val, dict) else None)
            if not nm:
                continue
            self.name_at[addr.lower()] = nm
            self.addr_of.setdefault(nm, []).append(addr.lower())

    # -- evidence helpers -------------------------------------------------
    def retail_body(self, name):
        objs = self.tidx.get(name)
        if not objs:
            return None, None, "absent from every target obj"
        return extract(objs[0], name, True)

    def our_body(self, name):
        objs = self.oidx.get(name)
        if not objs:
            return None, None, "absent from every compiled obj (we may not instantiate it)"
        return extract(objs[0], name, False)

    def adjudicate(self, a, b, min_words=4):
        """a = retail's spelling (target side); b = our spelling (base side)."""
        r = {"retail": a, "ours": b,
             "already_aliased": alias_covers(a, b),
             "evidence": []}

        aa, ab = self.addr_of.get(a, []), self.addr_of.get(b, [])
        r["addr_retail"], r["addr_ours"] = aa, ab

        # ---- R-TWOADDR: the map places both names, at different addresses.
        # ICF folding means ONE surviving address.  If retail holds two, they
        # did not fold.  Corroborated by retail-vs-retail, the SOUND test:
        # one naming scheme, one extent rule, so the one-sided reader hazard
        # cannot apply by construction.
        if aa and ab and not (set(aa) & set(ab)):
            ba, ra, _ = self.retail_body(a)
            bb, rb, _ = self.retail_body(b)
            if ba is not None and bb is not None:
                try:
                    v, why, det = compare_bodies(ba, ra, bb, rb)
                except UnresolvedComparison as e:
                    v, why, det = "UNRESOLVED", str(e), {}
                if v in ("DIFF_SIZE", "DIFF_BODY", "DIFF_NAME"):
                    r["verdict"] = "REFUTED"
                    r["tier"] = "R-TWOADDR+retail-vs-retail"
                    r["why"] = ("map places the two names at DIFFERENT retail "
                                "addresses (%s vs %s) and retail's own two "
                                "bodies differ: %s" % (aa[0], ab[0], why))
                    r["detail"] = det
                    return r
                if v == "EQUAL":
                    # Two addresses whose bodies are identical is a CONTRADICTION
                    # (ICF should have folded them).  Either a map name is wrong
                    # or something blocked folding.  Not a fold proof.
                    r["verdict"] = "UNDECIDED"
                    r["tier"] = "CONTRADICTION"
                    r["why"] = ("map places both names at different addresses "
                                "(%s vs %s) yet retail's two bodies are "
                                "IDENTICAL -- ICF should have folded them, so a "
                                "map name is likely wrong" % (aa[0], ab[0]))
                    return r
                r["evidence"].append("retail-vs-retail: %s (%s)" % (v, why))
            r["verdict"] = "REFUTED"
            r["tier"] = "R-TWOADDR"
            r["why"] = ("map places the two names at DIFFERENT retail addresses "
                        "(%s vs %s); ICF folding leaves ONE address" % (aa[0], ab[0]))
            return r

        # ---- T-OURICF: our COMDAT for A == our COMDAT for B.  That IS the
        # linker's folding condition applied to two of our own COMDATs, so
        # retail's anonymous labels never enter the argument.
        oa_b, oa_r, oa_n = self.our_body(a)
        ob_b, ob_r, ob_n = self.our_body(b)
        if oa_b is not None and ob_b is not None:
            try:
                v, why, det = compare_bodies(oa_b, oa_r, ob_b, ob_r)
            except UnresolvedComparison as e:
                v, why, det = "UNRESOLVED", str(e), {}
            if v == "EQUAL" and len(oa_b) >= 4 * min_words:
                # ⛔⛔ OUR-SIDE IDENTITY ALONE IS **NOT** A FOLD PROOF, AND AN
                # EARLIER REVISION OF THIS FILE SHIPPED IT AS ONE.  It proves
                # only that /OPT:ICF would fold these two COMDATs IN OUR BUILD.
                # Retail's source is NOT ours -- dc3 (our engine oracle) is
                # NEWER than RB3 -- so two spellings that collapse for us can be
                # genuinely different functions in retail.  The selftest caught
                # this on three recorded `?OnMsg@BandUI@@` overloads.
                # ⇒ retail corroboration is MANDATORY; its ABSENCE is UNDECIDED,
                # never proof.  Fail-closed on missing evidence.
                tb, tr, _ = self.retail_body(a)
                if tb is None:
                    r["verdict"] = "UNDECIDED"
                    r["tier"] = "T-OURICF-NO-RETAIL"
                    r["why"] = ("our two COMDATs are identical (%d B) so OUR "
                                "build folds them -- but retail has no body for "
                                "%s, so nothing corroborates that RETAIL folded "
                                "them. Our source is not retail's source."
                                % (len(oa_b), a))
                    return r
                try:
                    v2, why2, _ = compare_bodies(tb, tr, oa_b, oa_r)
                except UnresolvedComparison as e:
                    v2, why2 = "UNRESOLVED", str(e)
                if v2 != "EQUAL":
                    r["verdict"] = "UNDECIDED"
                    r["tier"] = "T-OURICF-uncorroborated"
                    r["why"] = ("our two COMDATs are identical (so OUR build "
                                "folds them) but retail's body at %s is not "
                                "that code: %s (%s) -- our source may differ "
                                "from retail's"
                                % (aa[0] if aa else "?", v2, why2))
                    return r
                r["verdict"] = "PROVEN_FOLD"
                r["tier"] = "T-OURICF"
                r["why"] = ("our COMDATs for both spellings are byte- and "
                            "relocation-target-identical (%d B, %d relocs) -- "
                            "/OPT:ICF's own folding condition -- AND retail's "
                            "body at addr(%s) is that same code"
                            % (len(oa_b), len(oa_r), a))
                return r
            r["evidence"].append("our(A) vs our(B): %s (%s)" % (v, why))

        # ---- T-RETAIL: retail's body at addr(A) vs our B, names compared.
        tb, tr, tn = self.retail_body(a)
        if tb is None:
            r["verdict"] = "UNDECIDED"
            r["tier"] = "NO-RETAIL-BODY"
            r["why"] = "retail body for %s: %s" % (a, tn)
            return r
        if ob_b is None:
            r["verdict"] = "UNDECIDED"
            r["tier"] = "NO-OUR-BODY"
            r["why"] = "our body for %s: %s" % (b, ob_n)
            return r
        try:
            v, why, det = compare_bodies(tb, tr, ob_b, ob_r)
        except UnresolvedComparison as e:
            r["verdict"] = "UNDECIDED"
            r["tier"] = "UNRESOLVED-RELOC"
            r["why"] = ("bodies comparable but identity of a relocation target "
                        "is unresolved, so no verdict is available: %s" % e)
            return r
        r["detail"] = det
        if v == "EQUAL":
            if len(tb) < 4 * min_words:
                r["verdict"] = "UNDECIDED"
                r["tier"] = "TOO-WEAK"
                r["why"] = ("body is only %d B (<%d words); too little "
                            "information to distinguish same-shaped forwarders"
                            % (len(tb), min_words))
                return r
            r["verdict"] = "PROVEN_FOLD"
            r["tier"] = "T-RETAIL"
            r["why"] = ("retail's body at addr(%s) IS our %s: %s" % (a, b, why))
            return r
        if v == "UNDECIDED_EXTENT":
            r["verdict"] = "UNDECIDED"
            r["tier"] = "EXTENT-DEFECT"
            r["why"] = why
            return r
        r["verdict"] = "REFUTED"
        r["tier"] = {"DIFF_SIZE": "R-SIZE", "DIFF_BODY": "R-BODY",
                     "DIFF_NAME": "R-NAME"}[v]
        r["why"] = "retail's body at addr(%s) is not our %s: %s" % (a, b, why)
        return r


# --------------------------------------------------------------------------
# Families
# --------------------------------------------------------------------------
def family_of(sym):
    """Method-family key of an MSVC mangled name."""
    if not sym.startswith("?"):
        return sym
    # templated special member: ??$?0X@Class@@ (ctor), ??$?1... (dtor), etc.
    m = re.match(r"\?\?\$\?([0-9A-Z])[^@]*@\??\$?([A-Za-z0-9_]+)@", sym)
    if m:
        kind = {"0": "ctor", "1": "dtor"}.get(m.group(1), "op" + m.group(1))
        return "%s:%s" % (kind, m.group(2))
    m = re.match(r"\?\?\$?([A-Za-z0-9_]+)@", sym)          # ??$name@ / ??name@
    if m:
        return m.group(1)
    m = re.match(r"\?\?([0-9A-C])", sym)                    # ??0 ctor, ??1 dtor
    if m:
        rest = re.match(r"\?\?[0-9A-C]\??\$?([A-Za-z0-9_]+)@", sym)
        return ("ctor:" if m.group(1) == "0" else "dtor:" if m.group(1) == "1"
                else "op%s:" % m.group(1)) + (rest.group(1) if rest else "?")
    m = re.match(r"\?([A-Za-z0-9_]+)@", sym)
    if m:
        return m.group(1)
    return sym


def load_class_e(census_path):
    d = json.loads(Path(census_path).read_text())
    rows = [r for r in d["rows"]
            if r["name"] > 0 and r["hard"] + r["reg"] + r["imm"] + r["br"] == 0]
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", default=".")
    ap.add_argument("--census", required=False,
                    help="s1_charge_census.py --out JSON")
    ap.add_argument("--family", action="append", default=[],
                    help="family key(s) to adjudicate; repeatable")
    ap.add_argument("--top", type=int, default=0,
                    help="adjudicate the top N families by class-E bytes")
    ap.add_argument("--list-families", action="store_true")
    ap.add_argument("--pairs", nargs=2, action="append", default=[],
                    metavar=("RETAIL", "OURS"))
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--can-fail", action="store_true",
                    help="mutation test: require known-good verdicts to FLIP")
    ap.add_argument("--json-out", default=None)
    a = ap.parse_args()

    proj = Path(a.project).resolve()
    if a.selftest or a.can_fail:
        return selftest(proj, mutate=a.can_fail)

    if a.pairs:
        adj = Adjudicator(proj)
        for x, y in a.pairs:
            r = adj.adjudicate(x, y)
            print(json.dumps(r, indent=1))
        return 0

    if not a.census:
        print("need --census (or --pairs/--selftest)", file=sys.stderr)
        return 2
    rows = load_class_e(a.census)

    # ---- ANTI-VACUITY: an empty population passes every check by construction,
    # and that PASS is indistinguishable from a real one.
    if not rows:
        print("REFUSE: class-E population is EMPTY. An empty population passes "
              "every check by construction; refusing to report a clean run.",
              file=sys.stderr)
        return 3

    fam_rows = collections.defaultdict(list)
    fam_pairs = collections.defaultdict(collections.Counter)
    for r in rows:
        fams = {family_of(p[0][0]) for p in r["pairs"]}
        for f in fams:
            fam_rows[f].append(r)
        for p in r["pairs"]:
            fam_pairs[family_of(p[0][0])][tuple(p[0])] += p[1]

    order = sorted(fam_rows, key=lambda f: -sum(x["size"] for x in fam_rows[f]))
    if a.list_families:
        print(f"{'FAMILY':<24}{'rows':>6}{'bytes':>10}{'pairs':>7}")
        for f in order[:40]:
            print(f"{f:<24}{len(fam_rows[f]):>6}"
                  f"{sum(x['size'] for x in fam_rows[f]):>10}"
                  f"{len(fam_pairs[f]):>7}")
        print(f"\n{len(order)} families; class E {len(rows)} rows / "
              f"{sum(r['size'] for r in rows)} B")
        return 0

    fams = a.family or order[:a.top or 10]
    adj = Adjudicator(proj)
    assert_anti_vacuity(adj)

    out = {}
    grand = collections.Counter()
    for f in fams:
        pairs = fam_pairs.get(f)
        if not pairs:
            print(f"REFUSE: family {f!r} has NO class-E pairs -- refusing to "
                  f"report a clean run over an empty family.", file=sys.stderr)
            return 3
        res = []
        for (x, y), n in pairs.most_common():
            v = adj.adjudicate(x, y)
            v["instances"] = n
            res.append(v)
        vs = collections.Counter(v["verdict"] for v in res)
        grand.update(vs)
        out[f] = res
        rws = fam_rows[f]
        # A row pays ONLY if EVERY one of its charged pairs closes -- matched_code
        # is all-or-nothing.  So price by rows all of whose pairs are PROVEN.
        proven = {(v["retail"], v["ours"]) for v in res
                  if v["verdict"] == "PROVEN_FOLD"}
        allpairs_proven = [r for r in rws
                           if all(tuple(p[0]) in proven for p in r["pairs"])]
        print(f"\n=== FAMILY {f} ===")
        print(f"  rows {len(rws)} / {sum(x['size'] for x in rws)} B "
              f"(size-if-it-crosses); distinct pairs {len(pairs)}")
        print(f"  verdicts: " + ", ".join(f"{k}={v}" for k, v in vs.most_common()))
        print(f"  rows whose EVERY charged pair is PROVEN_FOLD: "
              f"{len(allpairs_proven)} / {sum(x['size'] for x in allpairs_proven)} B")
        for v in res[:6]:
            print(f"    [{v['verdict']:<12} {v.get('tier',''):<28}] "
                  f"x{v['instances']}  {v['retail'][:58]}")
            print(f"        vs {v['ours'][:58]}")
            print(f"        {v['why'][:150]}")

    print("\n=== GRAND TOTAL ===")
    for k, v in grand.most_common():
        print(f"  {k:<14} {v}")
    if a.json_out:
        Path(a.json_out).write_text(json.dumps(out, indent=1))
        print(f"wrote {a.json_out}")
    return 0


# --------------------------------------------------------------------------
# Anti-vacuity + known-answer validation
# --------------------------------------------------------------------------
def assert_anti_vacuity(adj):
    """Fail LOUDLY if the tree cannot answer name lookups at all.

    ⛔⛔ A fresh worktree's reflinked TARGET objs are PRE-RENAMER: the mangled
    names only exist after a full build runs `obj_target_symbol_renamer`.  Lane
    FOLDPROVE-2's first run reported a unanimous 100/100 REFUTED -- exactly the
    answer it was primed to expect -- because every name it looked up was
    ABSENT.  It was caught ONLY because a symbol count disagreed.
    ⇒ A vacuity that CONFIRMS your hypothesis is the hardest kind to catch, so
    this asserts before any verdict is issued.
    """
    nsym = len(adj.tidx)
    nmang = sum(1 for k in adj.tidx if k.startswith("?"))
    if nsym < 50000 or nmang < 20000:
        raise SystemExit(
            "ANTI-VACUITY FAILURE: target obj index has %d symbols / %d mangled."
            "\nThe tree looks PRE-RENAMER (reflinked worktree never built)."
            "\nRun ./tools/ninja-locked -- NOT `ninja <one>.obj`." % (nsym, nmang))
    # Positive controls: mangled names that MUST resolve on a built tree.  These
    # exist ONLY because obj_target_symbol_renamer ran; on a pre-renamer tree
    # every one is absent and every verdict silently becomes "REFUTED".
    probes = ["?MemFree@@YAXPAX@Z", "??2CriticalSection@@SAPAXI@Z"]
    missing = [p for p in probes if p not in adj.tidx]
    if missing:
        raise SystemExit(
            "ANTI-VACUITY FAILURE: known-name probes %r do NOT resolve in the "
            "target index. The tree is not renamer-complete, so every 'absent' "
            "verdict would be an artifact that AGREES WITH YOUR PRIOR." % missing)
    print(f"[anti-vacuity] target index {nsym} symbols / {nmang} mangled; "
          f"probes resolved: {len(probes)}/{len(probes)} OK")
    return True


def selftest(proj, mutate=False):
    """Known answers first, then prove the gate CAN FAIL.

    A comparator that only ever returns one verdict proves nothing, and a PASS
    is worthless until the gate is shown able to go red.  So: identity positives
    (a symbol against ITSELF must be PROVEN_FOLD), refutation negatives lifted
    from RECORDED withdrawals in symbol_aliases.json (lanes proved these are NOT
    folds), and the Unresolved type's own refusal behaviour.
    """
    adj = Adjudicator(proj)
    assert_anti_vacuity(adj)
    fails = []

    # -- 0. the guard type itself must refuse, not answer ------------------
    u = Unresolved("test", ("lbl_1", "?x@@"))
    for label, fn in (("==", lambda: u == u), ("!=", lambda: u != 1),
                      ("hash", lambda: hash(u))):
        try:
            fn()
            fails.append(f"Unresolved.{label} did NOT raise -- the never-mask "
                         f"guard is INOPERATIVE")
        except UnresolvedComparison:
            pass
    # and a placeholder slot must yield Unresolved, not a forgiving equality
    va, vb = reloc_identity("lbl_8201BA34", "??_7Foo@@6B@")
    try:
        va == vb
        fails.append("placeholder-vs-named relocation compared WITHOUT raising "
                     "-- this is exactly W33's false IDENTICAL")
    except UnresolvedComparison:
        pass

    # -- 0b. the carve-defect guard must be able to FIRE ---------------------
    # ⛔ On the real population this branch fired ZERO times (only 13 of 296
    # size refutations even have a delta <= 8, median 184).  A guard that never
    # fires on real data is indistinguishable from a guard that CANNOT fire --
    # W36 found the project's only gate into COMPLETE had never fired at all.
    # So it is exercised synthetically, in both directions.
    blr = b"\x4e\x80\x00\x20"
    bctr = b"\x4e\x80\x04\x20"
    core = b"\x38\x60\x00\x01" * 4
    if not ends_in_unconditional_flow(core + bctr):
        fails.append("EXTENT guard: `bctr` not recognised as unconditional flow")
    if not ends_in_unconditional_flow(core + blr):
        fails.append("EXTENT guard: `blr` not recognised as unconditional flow")
    if ends_in_unconditional_flow(core + b"\x7c\x08\x02\xa6"):   # mflr
        fails.append("EXTENT guard: `mflr` WRONGLY read as unconditional flow "
                     "-- the guard would swallow real differences")
    v, why, _ = compare_bodies(core + bctr, [], core + bctr + blr, [])
    if v != "UNDECIDED_EXTENT":
        fails.append(f"EXTENT guard CANNOT FIRE: a truncated-at-`bctr` carve "
                     f"read {v}, not UNDECIDED_EXTENT -- this is the known dtk "
                     f"defect that produced a confident false DIFFERENT")
    # ...and it must NOT swallow a genuine size difference
    v2, _, _ = compare_bodies(core, [], core + core, [])
    if v2 != "DIFF_SIZE":
        fails.append(f"EXTENT guard OVER-FIRES: a genuine 16 B size difference "
                     f"read {v2}, not DIFF_SIZE")
    print(f"[selftest] carve-defect guard: fires on truncated-`bctr` carve, "
          f"does not fire on a genuine size gap")

    # -- 1. positive control: the COMPARATOR on identity ---------------------
    # ⚠ An earlier revision asserted `adjudicate(s, s) == PROVEN_FOLD`, which
    # CONFLATES TWO QUESTIONS and failed 3/8 for a legitimate reason: the full
    # adjudication corroborates against RETAIL, so for any function whose body
    # we do not yet reproduce it correctly returns UNDECIDED.  That is the
    # corroboration WORKING, not a comparator bug.  The comparator-level
    # identity control is the sound one: a body against ITSELF must read EQUAL.
    pos = [k for k in sorted(adj.tidx)
           if k.startswith("?") and k in adj.oidx][:400]
    chosen, ok = [], 0
    for s in pos:
        b, r, _ = adj.our_body(s)
        if b is None or len(b) < 32:
            continue
        chosen.append(s)
        try:
            v, why, _ = compare_bodies(b, r, b, r)
        except UnresolvedComparison as e:
            v, why = "UNRESOLVED", str(e)
        if v == "EQUAL":
            ok += 1
        else:
            fails.append(f"IDENTITY control failed: {s[:60]} compared against "
                         f"ITSELF read {v} ({why[:80]})")
        if len(chosen) >= 8:
            break
    if not chosen:
        fails.append("VACUOUS: no identity control candidates found at all")
    print(f"[selftest] comparator identity positives: {ok}/{len(chosen)} EQUAL")

    # -- 2. negative control: RECORDED withdrawals = proven NON-folds -------
    # ⛔ SIZE-BASED withdrawal classes are DELIBERATELY EXCLUDED as known
    # answers.  Lane STLPORT-1 refuted the "+8 B" size premise outright (a
    # one-sided reader billing a SUCCESSOR's EH funclet prefix into a COMDAT
    # span) and GROUNDED-2 RESTORED 6 of 8 size-based withdrawals.  Measured
    # again here on 2026-09-01: the three `?OnMsg@BandUI@@` overloads withdrawn
    # as `SURVIVOR_SIZE_MISMATCH` ("our(F)=64 B") are, in today's tree, BOTH
    # 52 B and byte-identical -- so that record is stale/artifactual and using
    # it as a known answer would calibrate this tool against a defect.
    # FIXPOINT_ROOT_DIFFERS is the clean negative: equal-size roots with
    # DIFFERING bytes, which no one-sided extent artifact can produce.
    al = json.loads((ROOT / "scripts" / "symbol_aliases.json").read_text())
    CLEAN = ("FIXPOINT_ROOT_DIFFERS", "FABRICATED_FOLD_PER_T_VTABLE",
             "CONTRADICTED_VENDOR_CALLEE")
    negs, stale = [], []
    for g in al.get("groups", []):
        for w in (g.get("withdrawn") or []):
            if not isinstance(w, dict):
                continue
            if w.get("class") in CLEAN:
                negs.append((g["survivor"], w["spelling"], w["class"]))
            elif "SIZE" in (w.get("class") or ""):
                stale.append((g["survivor"], w["spelling"], w["class"]))
    negs = negs[:24]
    nref = nund = nbad = 0
    for surv, sp, cl in negs:
        v = adj.adjudicate(surv, sp)
        if v["verdict"] == "REFUTED":
            nref += 1
        elif v["verdict"] == "UNDECIDED":
            nund += 1
        else:
            nbad += 1
            fails.append(f"KNOWN NON-FOLD read as {v['verdict']} ({cl}): "
                         f"{surv[:50]} vs {sp[:50]} :: {v['why'][:110]}")
    print(f"[selftest] clean recorded withdrawals ({len(negs)}): REFUTED={nref} "
          f"UNDECIDED={nund} WRONGLY_PROVEN={nbad}")
    if not negs:
        fails.append("VACUOUS: no clean recorded withdrawals found to test "
                     "against -- the negative control cannot fail")
    # reported, never gated (see the comment above)
    print(f"[selftest] size-class withdrawals present but EXCLUDED as "
          f"contaminated known answers: {len(stale)}")

    # -- 3. CAN IT FAIL? mutate a known-good pair; the verdict MUST flip ----
    if mutate:
        print("[can-fail] mutating identity positives; each MUST flip off "
              "PROVEN_FOLD")
        flipped = 0
        for s in chosen[:5]:
            b, r, _ = adj.our_body(s)
            mut = bytearray(b)
            # flip a non-relocated word so it is a genuine body difference
            reloff = {x[0] for x in r}
            idx = next((i for i in range(0, len(mut) - 3, 4)
                        if i not in reloff), None)
            if idx is None:
                continue
            mut[idx] ^= 0x01
            try:
                v, why, _ = compare_bodies(b, r, bytes(mut), r)
            except UnresolvedComparison:
                v = "UNRESOLVED"
            if v == "EQUAL":
                fails.append(f"CAN-FAIL: mutating a body word did NOT change "
                             f"the comparison for {s[:50]} -- the comparator is "
                             f"blind to body content")
            else:
                flipped += 1
        print(f"[can-fail] body mutation flipped {flipped}/{len(chosen[:5])}")
        if flipped == 0:
            fails.append("CAN-FAIL: no mutation flipped any verdict -- VACUOUS")
        # mutate a relocation TARGET NAME: must flip to DIFF_NAME, not be masked
        nmflip = 0
        for s in chosen[:5]:
            b, r, _ = adj.our_body(s)
            real = [i for i, x in enumerate(r) if not is_placeholder(x[2])]
            if not real:
                continue
            r2 = list(r)
            o, t, n = r2[real[0]]
            r2[real[0]] = (o, t, "?ZZ_NOT_A_REAL_SYMBOL@@YAXXZ")
            try:
                v, why, _ = compare_bodies(b, r, b, r2)
            except UnresolvedComparison:
                v = "UNRESOLVED"
            if v == "DIFF_NAME":
                nmflip += 1
            else:
                fails.append(f"CAN-FAIL: changing a relocation TARGET NAME gave "
                             f"{v}, not DIFF_NAME, for {s[:50]} -- names are "
                             f"being MASKED, the cardinal sin")
        print(f"[can-fail] relocation-name mutation flipped {nmflip} to DIFF_NAME")
        if nmflip == 0:
            fails.append("CAN-FAIL: no relocation-name mutation was detected -- "
                         "VACUOUS (this is the masked-bl trap)")

    print()
    if fails:
        print("SELFTEST FAILURES (%d):" % len(fails))
        for f in fails[:20]:
            print("  ⛔ " + f)
        return 1
    print("SELFTEST PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
