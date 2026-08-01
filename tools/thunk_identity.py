#!/usr/bin/env python3
"""Adjudicate vtordisp/adjustor thunk names by what they BRANCH TO.

Why this exists
---------------
The usual decisive standard for a map repair -- paired body size read back
from report.json -- is STRUCTURALLY INAPPLICABLE to vtordisp thunks. Every
`$4PPPPPPPM@` thunk in retail is 12 bytes (report.json population: 1436@12,
539@16, 1@56, 1@8). Comparing 12 against 12 passes no matter which name sits
on which row, so that check cannot fail and verifies nothing.

A thunk's identity is not its vtable slot and not its size -- it is its branch
target. These stubs are straight-line code terminating in an unconditional `b`
(three words, or FOUR when a nonzero static adjustment adds an `addi`), so the
target decodes straight out of retail band.exe. The target body's plain
(non-thunk) map row then names the method, and the thunk's name must agree
with it.

THREE earlier revisions of this check were VACUOUS; all failure modes are
guarded against here, and the anti-vacuity control below is not optional.

  v1  followed "the first branch found within 24 bytes" with no stopping
      predicate. Once it reached a real body it followed a branch *inside*
      that body; 26 of 29 rows converged on one trampoline region. A
      recursive follower needs a TERMINATION TEST, not a depth limit.

  v2  used .pdata extents to decide "is this a stub?". 12-byte thunks have no
      .pdata record at all, so it bailed at hop 0, reported the thunk as its
      own body, and produced AGREE=0/DISAGREE=29 -- the null answer merely
      restating old != new. (.pdata-absence is not a "not a function" test.)

  v3  decoded only THREE words, so the terminal `b` of every 16-byte thunk
      (at word index 3) was missed -- 539 rows, exactly report.json's 539@16
      population, silently reported "no-branch" and were excluded from every
      sweep. The symptom was visible as a large no-branch count in the control
      line and was read as noise rather than as a structural blind spot.
      See thunk_target() for the fix and its superset validation.

This version decodes up to four words directly and needs no extent source.

The adjustment channel (second, INDEPENDENT column -- lane CF-8)
---------------------------------------------------------------
The branch channel above adjudicates (method, class) only. It is structurally
blind to the DISPLACEMENT: two thunks that jump to the same method at different
static adjustments are indistinguishable to it, and a whole vtable block of such
thunks is exactly the population this tool is pointed at.

The `$4PPPPPPPM@<adj>@` mangling carries that displacement in its SECOND numeric
field, and it must equal the negation of the thunk's own `addi rN,rN,imm`
(absent `addi` == adjustment 0, with no exceptions over all 2,509 sites here).
That makes displacement-level adjudication possible, and it is now a reported
column rather than living only in a lane's scratch solver -- previously the
shipped tool could not reproduce the derivation of the patches it was being used
to bless.

Baseline agreement over the whole map at d2ab626d: 1,429/1,437 (99.44%) in the
12-byte family and 523/539 (97.03%) in the 16-byte family; `--audit` reports the
aggregate over both, 1,953/1,978 = 98.74%.

** The adjustment CONTROL line is close to vacuous by construction -- do not
read it as corroboration. ** The base disagreement rate is only 1.26%, so a
60-row control has (1-0.0126)^60 ~= 47% probability of showing ADJ_MISMATCH=0
purely by chance. An all-ADJ_MATCH control line is therefore the SINGLE MOST
LIKELY outcome even when the instrument is working, and it carries almost no
information -- unlike the branch channel, whose control genuinely mixes (~45
AGREE / ~9 DISAGREE per 60). Judge a run on the BRANCH control; the adjustment
column is a per-row filter, not a calibration.

** Necessary, NOT sufficient. ** This channel is far weaker at discrimination
than the branch channel and must not be read as a second opinion of equal
weight: on the 90 rows lane CF-8 deleted it returned ADJ_MATCH on all 90, while
the branch channel condemned 41 of them outright. A name can carry the right
displacement for its address and still name the wrong method.

Anti-vacuity control
--------------------
`--control N` runs the same instrument over N randomly chosen $4 thunks the
caller is NOT proposing to change. A trustworthy run shows a healthy mix of
AGREE and DISAGREE. If the control returns all-AGREE the instrument is
rubber-stamping; if it returns all-DISAGREE it is miscalibrated. Either way
its verdict on the proposed rows is worthless. Measured baseline on 60
untouched thunks: 27 AGREE / 10 DISAGREE / 1 body-unnamed / 22 no-branch.

Lane CH-1 corrections (2026-08-01) -- FOUR measured defects in the above
-----------------------------------------------------------------------
1. POPULATION IS NAME-DERIVED AND MISSES A THIRD OF THE VEIN. `--audit`
   selects rows by the `$4PPPPPPPM@` string. The population defined by CODE
   (map rows whose retail body is straight-line and ends in an unconditional
   `b`) is 3,067 rows; only 1,980 carry the `$4` mark. The families
   W (adjustor, 139), $2 (protected vtordisp, 14) and $0 (private, 1) were
   never swept, and 254 rows are a BARE `b` that no name-based filter finds.
   Use `--code-population` for the code-derived set.
   -- Sizing note: of the 3,067, only 2,134 are compiler-generated thunks.
   The other 933 are ordinary functions that tail-call (`?SetRotation@
   PatchLayer@@QAAXM@Z`) or C functions (`_blkmov`); "name must equal branch
   target" is simply FALSE for them and must never be applied there.
   -- The W family is 139, NOT the 530 a loose `@@W` regex reports: `W4Enum@@`
   is the mangling of an ENUM PARAMETER TYPE and false-positives 380 times.
   Confirmed by two independent instruments (code shape AND the adjustment
   channel: 139/139 ADJ_MATCH).

2. `method_sig` RETURNS None FOR EVERY MSVC SPECIAL NAME. `??_E`/`??_G`/`??1`
   /`??0` do not have the `?Name@Class@@` shape, so 96 destructor-thunk rows
   came back unparsable and were silently uncounted. See `sig_strict`.

3. ** RECURSION MUST STOP AT THE FIRST *NAMED* TARGET, NOT THE FIRST
   NON-THUNK SHAPE. ** A shape-based termination test reproduces v1's disease:
   a real function that tail-calls is shape-indistinguishable from a thunk, so
   the walker steps THROUGH the correct answer. Measured over 2,134 rows,
   shape-terminated recursion flipped 89 verdicts and 77 of them were
   AGREE -> DISAGREE -- e.g. `?Load@RndDir@@$4...` lands 1-hop on
   `?Load@RndDir@@UAAX...` (exact match), and recursion then follows RndDir::
   Load's own tail-call into `?Load@ObjectDir@@`. Recursion exists ONLY to skip
   ANONYMOUS intermediates. With the corrected rule just 16 of 2,134 rows need
   a second hop.

4. ** DISAGREE OVERSTATES THE DEFECT RATE ~3x BECAUSE CLASS-DIFFERS IS
   LEGITIMATE. ** With multiple inheritance MSVC emits `?M@Derived@@W7...`
   branching to the base implementation `?M@Base@@...`, so the CLASS
   legitimately differs while the METHOD matches -- `?Highlight@RndDir@@$4...`
   -> `?Highlight@RndDrawable@@UAAXXZ` is CORRECT. Split the verdict:
   AGREE (exact) / AGREE_METHOD (class differs, MI base impl) /
   METHOD_DIFFERS (the only candidate-defect class).
   Measured over the 2,134-row population at 8bb0cb69:
       AGREE 1560 | AGREE_METHOD 89 | METHOD_DIFFERS 231 | TARGET_UNNAMED 251
   => candidate defects 231/1880 resolved = 12.29%, not the ~20-27% the
   undifferentiated DISAGREE reports.

Null controls for the METHOD_DIFFERS rate (lane CH-1)
----------------------------------------------------
The detector CAN fail, and was made to:
    treatment (real map)                       12.29%
    shuffle WITHIN a shape group, 3 seeds      67.29 / 70.37 / 70.85%
    global shuffle of all thunk names, 3 seeds 90.64 / 90.80 / 91.97%
The within-shape shuffle is the exact swap objdiff CANNOT see (report.rs:394
masks reloc args), so ~5.6x separation there is the load-bearing number: the
branch channel really does police what the metric cannot.

** A FOLD-AMBIGUITY FILTER LOOKS OBVIOUS AND IS A DEAD INDEX -- DO NOT USE IT. **
The natural story for METHOD_DIFFERS is "the target is a tiny empty virtual that
ICF folded, so the map names one of many arbitrarily". It is REFUTED by its own
control: target bodies <=2 instructions are 6.9% of METHOD_DIFFERS vs 6.7% of
AGREE, and target fan-in >=10 is 14.7% of METHOD_DIFFERS vs 19.5% of AGREE
(i.e. LOWER). Enrichment ~1.0x. Filtering on it "explains away" 45 rows on no
information at all.

Usage
-----
  tools/thunk_identity.py --audit                 # sweep every $4 row in the map
  tools/thunk_identity.py --patch p.patch         # adjudicate a proposed patch
  tools/thunk_identity.py --addrs 0x8231a950,...  # specific rows
  tools/thunk_identity.py --audit --control 60 --json out.json
"""

import argparse
import collections
import json
import random
import re
import struct
import sys

IMAGE_BASE = 0x82000000
DEFAULT_EXE = "orig/45410914/band.exe"
DEFAULT_MAP = "scripts/target_symbol_map.json"
THUNK_MARK = "$4PPPPPPPM@"


class Image:
    """Minimal PE reader for the decompressed retail band.exe."""

    def __init__(self, path):
        self.data = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.data, pe + 6)[0]
        opt = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.secs = []
        for i in range(nsec):
            o = pe + 24 + opt + i * 40
            name = self.data[o:o + 8].rstrip(b"\0").decode("ascii", "replace")
            vsz, va, rsz, praw = struct.unpack_from("<IIII", self.data, o + 8)
            self.secs.append((name, va, vsz, praw, rsz))

    def offset(self, va):
        rva = va - IMAGE_BASE
        for _name, sva, vsz, praw, rsz in self.secs:
            if sva <= rva < sva + max(vsz, rsz):
                return praw + (rva - sva)
        return None

    def word(self, va):
        o = self.offset(va)
        if o is None or o + 4 > len(self.data):
            return None
        return struct.unpack_from(">I", self.data, o)[0]


def _is_branch(w):
    # PowerPC primary opcode 18 == b / bl / ba / bla
    return w is not None and (w >> 26) == 18


def _adjustment_from_code(img, va):
    """The thunk's STATIC ADJUSTMENT, read out of its own `addi rN,rN,imm`.

    The 4-word vtordisp form is
        lwz r11,-4(rN) / subf rN,r11,rN / addi rN,rN,-adj / b target
    and the 3-word form simply omits the `addi`, which is the adjustment being
    zero. So: locate the terminal branch (first branch, see thunk_target), and
    if the instruction immediately before it is an `addi` (primary opcode 14),
    the adjustment is the NEGATED immediate; otherwise it is 0.

    Measured on this binary: `adj == 0` iff no `addi` is present, with no
    exceptions over all 2,509 thunk sites.
    """
    for i in range(4):
        if _is_branch(img.word(va + 4 * i)):
            if i >= 1:
                w = img.word(va + 4 * (i - 1))
                if w is not None and (w >> 26) == 14:      # addi
                    imm = struct.unpack(">h", struct.pack(">H", w & 0xFFFF))[0]
                    return -imm
            return 0
    return None


def _adjustment_from_name(name):
    """The adjustment encoded in the mangled `$4PPPPPPPM@<adj>@` SECOND field.

    MSVC's `$4` (vtordisp) mangling carries two numeric fields; the first is the
    vtordisp offset (always `PPPPPPPM@` = -4 here) and the second is the static
    adjustment.  Numbers are `0`-`9` for 1..10 and an `A`-`P` base-16 run
    terminated by `@` otherwise.
    """
    if not isinstance(name, str):
        return None
    m = re.search(r"\$4([A-P]+@|[0-9])([A-P]+@|[0-9])", name)
    if not m:
        return None
    s = m.group(2)
    if re.fullmatch(r"[0-9]", s):
        return int(s) + 1
    v = 0
    for ch in s[:-1]:
        v = v * 16 + (ord(ch) - 65)
    return v


def _branch_target(img, va):
    w = img.word(va)
    li = w & 0x03FFFFFC
    if li & 0x02000000:          # sign-extend the 26-bit displacement
        li -= 0x04000000
    absolute = (w >> 1) & 1      # AA bit
    return (li if absolute else va + li) & 0xFFFFFFFF


def thunk_target(img, va):
    """Return (target_va, form) for a stub at va, or (None, reason).

    A $4 vtordisp thunk is three or FOUR words ending in `b`:
        lwz   r11, -4(rN)
        subf  rN, r11, rN
        addi  rN, rN, -adj     <-- present only when the static adjustment != 0
        b     target

    v3 fixed two defects in the v2 window (lane CF-5):

    ** The 4-word form was invisible.** The window was three words wide and the
    16-byte thunks put their terminal `b` at word index 3, so the ENTIRE 16-byte
    family -- 539 rows, matching report.json's 539@16 population exactly -- came
    back "no-branch" and was silently excluded from every sweep. Measured: 539 of
    the 540 no-branch rows have a branch at word 3.

    ** "Prefer the longest form" could OVER-READ.** Scanning i=2,1,0 and taking
    the last branch means a genuine 12-byte thunk whose successor happens to
    begin with a branch is decoded against the NEXT function's first word. Since
    a thunk is straight-line code terminating in `b`, the FIRST branch is by
    construction the terminal one, and it can never read past the end. Widening
    to 4 words under prefer-longest flipped one row's verdict; under first-branch
    it flips none.

    Validated as a pure superset before adoption: over the 1,439 rows the v2
    window could resolve at all, first-branch-over-4-words yields an IDENTICAL
    target and verdict for every one (0 differences), and additionally resolves
    the 539 it could not see.
    """
    for i in range(4):
        w = img.word(va + 4 * i)
        if _is_branch(w):
            return _branch_target(img, va + 4 * i), f"{i + 1}w"
    return None, "no-branch"


def method_sig(name):
    """'?Method@Class@@...' -> ('Method', 'Class'); None if not a mangled member."""
    if not isinstance(name, str) or not name.startswith("?"):
        return None
    parts = name[1:].split("@@")[0].split("@")
    return (parts[0], parts[1]) if len(parts) >= 2 else None


def adjudicate(img, tmap, addr, proposed):
    """Compare a proposed name for `addr` against its branch target's row."""
    va = int(addr, 16)
    target, form = thunk_target(img, va)
    adj_code = _adjustment_from_code(img, va)
    adj_name = _adjustment_from_name(proposed)
    if target is None:
        return {"addr": addr, "verdict": "NO_BRANCH", "form": form,
                "adj_code": adj_code, "adj_name": adj_name, "adj": "NO_BRANCH"}
    row = tmap.get("0x%08x" % target)
    rec = {"addr": addr, "target": "0x%08x" % target, "form": form,
           "target_row": row, "proposed": proposed,
           "adj_code": adj_code, "adj_name": adj_name}
    # ---- SECOND, INDEPENDENT CHANNEL: the static adjustment ------------------
    # The branch channel adjudicates (method, class).  It says NOTHING about the
    # displacement, so two thunks to the same method at different adjustments are
    # indistinguishable to it.  The `$4PPPPPPPM@<adj>@` field must equal the
    # thunk's own `addi`, which makes DISPLACEMENT-LEVEL adjudication possible.
    # Baseline over the whole map at d2ab626d: 1429/1437 agree in the 12-byte
    # family (99.44%) and 523/539 in the 16-byte family (97.03%).  Note this
    # channel is much WEAKER at discrimination than the branch channel -- on the
    # 90 rows lane CF-8 deleted it returned ADJ_MATCH on all 90 while the branch
    # channel condemned 41 -- so treat ADJ_MATCH as necessary, never sufficient.
    if adj_name is None:
        rec["adj"] = "ADJ_UNDECODABLE"
    elif adj_code is None:
        rec["adj"] = "ADJ_NO_CODE"
    else:
        rec["adj"] = "ADJ_MATCH" if adj_name == adj_code else "ADJ_MISMATCH"
    if row is None:
        rec["verdict"] = "TARGET_UNNAMED"
    elif method_sig(row) == method_sig(proposed):
        rec["verdict"] = "AGREE"
    else:
        rec["verdict"] = "DISAGREE"
    return rec



# ---------------------------------------------------------------------------
# Lane CH-1 corrections (see the module docstring for the measurements).
# All of this is OPT-IN; default behaviour of --audit/--patch is unchanged.
# ---------------------------------------------------------------------------

_SPECIAL = {"??_E": "DTOR", "??_G": "DTOR", "??1": "DTOR", "??_D": "VBASEDTOR",
            "??0": "CTOR", "??_7": "VFTABLE", "??_8": "VBTABLE", "??_9": "VCALL"}

_VTORDISP = re.compile(r"@@\$([0-4])PPPPPPPM@")
# NB the trailing [A-Z]{2} is load-bearing: without it this fires on the
# parameter-type list `@@HM@Z` and on `W4Enum@@`, which inflates the adjustor
# family from 139 to 530.
_ADJUSTOR = re.compile(r"@@([WXOPGH])((?:[A-P]+@|[0-9]))([A-Z]{2})")


def thunk_kind(name):
    """'$0'..'$4' for vtordisp, 'W'/'X'/'O'/'P'/'G'/'H' for adjustor, else None."""
    if not isinstance(name, str):
        return None
    v = _VTORDISP.search(name)
    if v:
        return "$" + v.group(1)
    ms = list(_ADJUSTOR.finditer(name))
    return ms[-1].group(1) if ms else None


def sig_strict(name):
    """(method, class) INCLUDING MSVC special names -- fixes defect 2.

    Plain `method_sig` returns None for `??_E`/`??_G`/`??1`/`??0`, which
    silently drops every destructor thunk (96 rows here) from the tally.
    Destructor forms are normalised to 'DTOR' because `??_E` (vector deleting)
    is a weak external aliasing `??_G` (scalar deleting); comparing them as
    distinct methods MANUFACTURES false defects.
    """
    if not isinstance(name, str) or not name.startswith("?"):
        return None
    for pre, tok in _SPECIAL.items():
        if name.startswith(pre):
            return (tok, name[len(pre):].split("@@")[0])
    if name.startswith("??$"):
        m = re.match(r"\?\?\$([^@]+)@", name)
        return (m.group(1), "<tmpl>") if m else None
    parts = name[1:].split("@@")[0].split("@")
    return (parts[0], parts[1]) if len(parts) >= 2 else None


def is_forwarder(img, va, maxw=5):
    """(words_before_branch, target) if va is straight-line code ending in an
    unconditional `b`; None otherwise. `bl` is a CALL and is excluded."""
    ws = []
    for i in range(maxw):
        w = img.word(va + 4 * i)
        if w is None:
            return None
        if _is_branch(w):
            if w & 1:
                return None
            return ws, _branch_target(img, va + 4 * i)
        if (w >> 26) in (16, 19):
            return None
        ws.append(w)
    return None


def dethunk_named(img, tmap, va, maxhops=8):
    """Hop to the final body, STOPPING AT THE FIRST NAMED TARGET -- defect 3.

    Recursion exists solely to skip ANONYMOUS intermediates. Terminating on
    "target no longer looks like a thunk" instead is v1's disease: a real
    function that tail-calls has the same shape as a thunk, so the walker steps
    straight through the correct answer (measured: 77 of 89 flips were
    AGREE -> DISAGREE).
    """
    seen = {va}
    hops = 0
    while hops < maxhops:
        d = is_forwarder(img, va)
        if d is None:
            return None, "no-branch", hops
        _ws, tgt = d
        hops += 1
        if tgt in seen:
            return tgt, "cycle", hops
        if ("0x%08x" % tgt) in tmap:
            return tgt, "named", hops
        if is_forwarder(img, tgt) is None:
            return tgt, "unnamed-body", hops
        seen.add(tgt)
        va = tgt
    return va, "depth-cap", hops


def adjudicate_strict(img, tmap, addr, proposed):
    """Three-way verdict -- fixes defect 4.

    AGREE          exact (method, class) match
    AGREE_METHOD   class differs: the legitimate multiple-inheritance case
                   (`?M@Derived@@W7...` -> `?M@Base@@...`). NOT a defect.
    METHOD_DIFFERS the only candidate-defect class.
    """
    tgt, how, hops = dethunk_named(img, tmap, int(addr, 16))
    rec = {"addr": addr, "proposed": proposed, "kind": thunk_kind(proposed),
           "how": how, "hops": hops}
    if tgt is None:
        rec["verdict"] = "NO_BRANCH"
        return rec
    rec["target"] = "0x%08x" % tgt
    row = tmap.get(rec["target"])
    rec["target_row"] = row
    if row is None:
        rec["verdict"] = "TARGET_UNNAMED"
        return rec
    a, b = sig_strict(proposed), sig_strict(row)
    if a is None or b is None:
        rec["verdict"] = "UNPARSABLE"
    elif a == b:
        rec["verdict"] = "AGREE"
    elif a[0] == b[0]:
        rec["verdict"] = "AGREE_METHOD"
    else:
        rec["verdict"] = "METHOD_DIFFERS"
    return rec


def code_population(img, tmap):
    """Map rows whose RETAIL BODY is a forwarder AND whose name is a
    compiler-generated thunk -- the honest denominator (2,134 here), as opposed
    to the 1,980 the `$4` string filter finds or the 3,067 forwarder bodies
    (933 of which are ordinary tail-calling functions the predicate cannot
    adjudicate)."""
    out = {}
    for a, n in tmap.items():
        if not isinstance(a, str) or not a.startswith("0x") or not isinstance(n, str):
            continue
        if thunk_kind(n) is None:
            continue
        if is_forwarder(img, int(a, 16)) is None:
            continue
        out[a] = n
    return out


def parse_patch(path):
    """Pull the '+' side of a target_symbol_map.json patch into {addr: name}."""
    out = {}
    for line in open(path):
        if line.startswith("+ ") and '": "' in line:
            a, n = line[2:].strip().rstrip(",").split('": "')
            out[a.strip().strip('"')] = n.rstrip('"')
    return out


def summarize(records, label):
    c = collections.Counter(r["verdict"] for r in records)
    a = collections.Counter(r.get("adj") for r in records)
    print("%-38s AGREE=%-5d DISAGREE=%-5d unnamed=%-5d no-branch=%d"
          % (label, c["AGREE"], c["DISAGREE"], c["TARGET_UNNAMED"], c["NO_BRANCH"]))
    print("%-38s ADJ_MATCH=%-5d ADJ_MISMATCH=%-5d undecodable=%-5d no-code=%d"
          % ("  \\_ adjustment channel", a["ADJ_MATCH"], a["ADJ_MISMATCH"],
             a["ADJ_UNDECODABLE"], a["ADJ_NO_CODE"]))
    return c


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=DEFAULT_EXE)
    ap.add_argument("--map", dest="tmap", default=DEFAULT_MAP)
    ap.add_argument("--patch", help="adjudicate the + side of a map patch")
    ap.add_argument("--addrs", help="comma-separated thunk addresses to check")
    ap.add_argument("--audit", action="store_true",
                    help="sweep every $4 row currently in the map")
    ap.add_argument("--control", type=int, default=60, metavar="N",
                    help="anti-vacuity control over N untouched $4 rows (0 disables)")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--json", help="write full records here")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    img = Image(args.exe)
    tmap = json.load(open(args.tmap))

    if args.patch:
        proposals = parse_patch(args.patch)
    elif args.addrs:
        proposals = {a.strip(): tmap.get(a.strip())
                     for a in args.addrs.split(",") if a.strip()}
    elif args.audit:
        proposals = {a: n for a, n in tmap.items()
                     if isinstance(n, str) and THUNK_MARK in n}
    else:
        ap.error("need one of --patch / --addrs / --audit")

    records = [adjudicate(img, tmap, a, n) for a, n in sorted(proposals.items())]

    if args.verbose or args.addrs:
        for r in records:
            print("%s [%s] -> %s  %-46s %-15s adj code=%-5s name=%-5s %s"
                  % (r["addr"], r.get("form", "-"), r.get("target", "-"),
                     str(r.get("target_row"))[:44], r["verdict"],
                     r.get("adj_code"), r.get("adj_name"), r.get("adj", "-")))
        print()

    summarize(records, "PROPOSED")

    # --- anti-vacuity control -------------------------------------------------
    if args.control:
        pool = [a for a, n in tmap.items()
                if isinstance(n, str) and THUNK_MARK in n and a not in proposals]
        random.seed(args.seed)
        random.shuffle(pool)
        sample = pool[:args.control]
        ctl = [adjudicate(img, tmap, a, tmap[a]) for a in sample]
        c = summarize(ctl, "CONTROL (untouched $4 rows)")
        # An EMPTY control is the most dangerous outcome of all: it prints a
        # control line, every counter reads 0, and the all-AGREE / all-DISAGREE
        # guards below are both False on zeros -- so the tool exits 0 and the run
        # LOOKS controlled. This is exactly what --audit did (lane CF-5): audit
        # proposes every $4 row, so the pool (rows NOT proposed) is the empty set,
        # and `--audit --control 60` sampled 0 rows and passed. A control that
        # cannot fail is not a control.
        if not sample:
            print("\n*** CONTROL IS EMPTY (%d rows available outside the proposed "
                  "set): the anti-vacuity check made NO observation, so it cannot "
                  "fail and this run is uncontrolled. In --audit mode every $4 row "
                  "is proposed, which leaves no pool by construction."
                  % len(pool), file=sys.stderr)
            return 2
        if c["AGREE"] and not c["DISAGREE"]:
            print("\n*** CONTROL IS VACUOUS: all-AGREE. The instrument cannot "
                  "fail here, so its verdict on the proposed rows means nothing.",
                  file=sys.stderr)
            return 2
        if c["DISAGREE"] and not c["AGREE"]:
            print("\n*** CONTROL MISCALIBRATED: all-DISAGREE on untouched rows.",
                  file=sys.stderr)
            return 2

    if args.json:
        json.dump(records, open(args.json, "w"), indent=1)
        print("wrote %s (%d records)" % (args.json, len(records)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
