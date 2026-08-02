#!/usr/bin/env python3
"""CROSS-BINARY adjudication of the at-100 wrong-callee class (lane CW-2).

Lane CV-4 censused the class of functions that score 100 on the default ruler
while calling the WRONG function (relocation arguments are masked --
report.rs:394 sets them to None), and split it into (a) wrong-callee candidate,
(b) an unadjudicable BACKLOG of 1,223 fns / 159,276 B, (c) fold-provable/benign
and (d) unreadable. It named the missing instrument and did not build it:
compile OUR body for callee B and compare it against RETAIL BYTES at addr(T).
This is that instrument, plus a second, map-independent channel.

CHANNEL 1 -- cross-binary body test (`Xbin.test`)
    Retail is LINKED, our obj is UNLINKED, so a raw memcmp is vacuous in the
    false-negative direction. Mask using OUR OWN relocation table: zero exactly
    the relocated FIELD on both sides and nothing else --
        0x06 REL24 -> keep opcode|AA|LK, zero the 24-bit LI
        0x10/0x11 REFHI/REFLO -> zero the low 16 bits
        0x12 PAIR -> no code field, ignored
        anything else -> REFUSE (never silently permit)
    Every other immediate, register field and opcode is preserved. Zeroing all
    immediates would have been easier and would MANUFACTURE BENIGN verdicts.
    Anti-vacuity guard: >= 4 words AND relocated words < 50% of the body.

CHANNEL 2 -- locate the callee in retail BY CONTENT (`Locator.hits`)
    Class (b) is defined as "our callee has no identified retail address", so
    every map-based test is structurally unable to fire on it. Our compiled body
    is a fingerprint; scan all 57,733 .pdata bodies of the same length. This
    also closes the ICF-fold escape on a WRONG verdict: if B is found at some
    address other than addr(T) while T occupies addr(T), the two are distinct
    SURVIVING retail functions and cannot have been folded.

CALIBRATION (all measured on this tree, 2026-08-02, at 44a96155)
    channel 1 positive control  ours[X] vs retail@addr(X)   96.67% of 15,509
    channel 1 null, LENGTH-CONDITIONED                       4.24% of 15,154
       -> enrichment 22.8x. The UNCONDITIONED null is vacuous (length almost
          always differs, only 108 decided) and would flatter the instrument.
    channel 2 recall (true address recovered)               79.5% of decided
    channel 2 located-but-wrong-address                      0.9%
    channel 2 RANDOM-OFFSET null                             2.2%

NATURAL EXPERIMENT (better than either synthetic null): run against CV-4's
independent classification, the verdict fires WRONG on 70.3% of its
wrong-callee candidates and on 0 of 1,630 functions in the two classes it
called benign.

UNKNOWN is explicit at every step and NEVER a fallthrough into a charged class
-- that exact defect made CV-4's first classifier manufacture 436 fake
wrong-callee sites out of retail bodies it simply could not read.
"""

import collections, glob, importlib.util, json, os, random, re, struct, sys
from pathlib import Path

import pathlib
ROOT = os.environ.get("CW2_ROOT", str(pathlib.Path(__file__).resolve().parent.parent))
sys.path.insert(0, os.path.join(ROOT, "tools"))
_spec = importlib.util.spec_from_file_location(
    "fs", ROOT + "/tools/maprow_audit/ck4_foldscan.py")
fs = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(fs)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from thunk_identity import Image                      # noqa: E402
from icf_fold_evidence import function_bodies         # noqa: E402
from coff_bodies_ext import function_bodies_ext       # noqa: E402

# CW-2: the EXTENDED reader is the default. The legacy one is kept only so the
# coverage gain it fixes can be re-measured (CW2_LEGACY_READER=1).
USE_LEGACY = os.environ.get("CW2_LEGACY_READER") == "1"

MASK = {0x06: 0xFC000003, 0x10: 0xFFFF0000, 0x11: 0xFFFF0000}
IGNORE = {0x12}


def mask_words(raw, relocs):
    """-> (masked_bytes, n_masked_words) or (None, reason)."""
    b = bytearray(raw)
    touched = set()
    for off, _n, t in relocs:
        if t in IGNORE:
            continue
        m = MASK.get(t)
        if m is None:
            return None, "unknown_reloc_type_0x%02x" % t
        if off + 4 > len(b) or off % 4:
            return None, "reloc_offset_bad"
        w = struct.unpack_from(">I", b, off)[0] & m
        struct.pack_into(">I", b, off, w)
        touched.add(off)
    return bytes(b), len(touched)


def retail_bytes(img, va, length):
    o = img.offset(va)
    if o is None or length < 4 or o + length > len(img.data):
        return None
    return bytes(img.data[o:o + length])


class Xbin:
    def __init__(self, root=ROOT):
        self.root = root
        self.img = Image(fs.BAND)
        self.pd = {va: ln for va, ln, _p, _e in fs.pdata(self.img)}
        self.ours = {}
        self.dup_diff = 0
        for p in glob.glob(root + "/build/45410914/src/**/*.obj", recursive=True):
            try:
                # DC-4: legacy_ok=True -- deliberate legacy hatch
                # (CW2_LEGACY_READER=1); must not trip the frozen-reader warning.
                bl = ([(n, r, rl) for n, r, rl in function_bodies(Path(p), legacy_ok=True)]
                      if USE_LEGACY else
                      [(n, r, rl) for n, r, rl, _o in function_bodies_ext(Path(p))])
            except Exception:
                continue
            for nm, raw, relocs in bl:
                sig = (bytes(raw), tuple(relocs))
                if nm in self.ours:
                    if self.ours[nm] != sig:
                        self.dup_diff += 1
                    continue
                self.ours[nm] = sig
        amap = json.load(open(root + "/scripts/target_symbol_map.json"))
        ak = re.compile(r'^0x[0-9a-fA-F]+$')
        self.name2addr = collections.defaultdict(list)
        for a, nm in amap.items():
            if ak.match(a) and isinstance(nm, str):
                self.name2addr[nm].append(int(a, 16))
        self._rc = {}

    # --- core test ---------------------------------------------------------
    def test(self, name, va):
        """Does OUR compiled body for `name` reproduce RETAIL bytes at `va`?

        -> one of: 'MATCH', 'DIFF', or a 'SKIP:<reason>' string. Never a bool,
        so a skip can never be read as a negative.
        """
        rec = self.ours.get(name)
        if rec is None:
            return "SKIP:no_our_body"
        raw, relocs = rec
        ln = self.pd.get(va)
        if ln is None:
            return "SKIP:no_pdata_extent"
        if ln != len(raw):
            return "SKIP:length_differs"
        rb = retail_bytes(self.img, va, ln)
        if rb is None:
            return "SKIP:unreadable"
        nwords = ln // 4
        if nwords < 4:
            return "SKIP:body_too_small"
        mo, no = mask_words(raw, relocs)
        if mo is None:
            return "SKIP:" + no
        mr, _ = mask_words(rb, relocs)
        if no * 2 >= nwords:
            return "SKIP:mask_ge_half"
        return "MATCH" if mo == mr else "DIFF"


def calibrate_xbin(x, seed=20260802, sample=0):
    """Positive control + length-conditioned null."""
    pop = [n for n in x.ours if len(x.name2addr.get(n, ())) == 1]
    rnd = random.Random(seed)
    if sample and len(pop) > sample:
        pop = rnd.sample(pop, sample)
    bylen = collections.defaultdict(list)
    for n, (raw, _r) in x.ours.items():
        bylen[len(raw)].append(n)

    pos = collections.Counter()
    null = collections.Counter()
    nullc = collections.Counter()
    for n in pop:
        va = x.name2addr[n][0]
        pos[x.test(n, va)] += 1
        # unconditioned null: a random other symbol against the same address
        y = rnd.choice(list(x.ours.keys()))
        if y != n:
            null[x.test(y, va)] += 1
        # LENGTH-CONDITIONED null: same body length, so the length gate cannot
        # do the discriminating for us (an unconditioned null is vacuously 0
        # and would flatter the instrument).
        cand = bylen.get(len(x.ours[n][0]), ())
        if len(cand) > 1:
            z = rnd.choice(cand)
            for _ in range(4):
                if z != n:
                    break
                z = rnd.choice(cand)
            if z != n:
                nullc[x.test(z, va)] += 1
    return pos, null, nullc


def _rate(c):
    d = c["MATCH"] + c["DIFF"]
    return (100.0 * c["MATCH"] / d) if d else float("nan"), d




# ======================= CHANNEL 2 =======================

import collections, os, random, struct, sys



class Locator:
    def __init__(self, x, cap=6000):
        self.x = x
        self.cap = cap
        self.capped = collections.Counter()
        self.bylen = collections.defaultdict(list)
        for va, ln, _p, _e in fs.pdata(x.img):
            self.bylen[ln].append(va)
        ts = [s for s in x.img.secs if s[0] == '.text'][0]
        self.text = (0x82000000 + ts[1], 0x82000000 + ts[1] + ts[2])
        self._cache = {}

    def hits(self, name, shift_rnd=None):
        """-> (list_of_vas, status). status is 'ok' | 'SKIP:<reason>' | 'CAPPED'."""
        key = (name, id(shift_rnd))
        if shift_rnd is None and name in self._cache:
            return self._cache[name]
        rec = self.x.ours.get(name)
        if rec is None:
            return [], "SKIP:no_our_body"
        raw, relocs = rec
        ln = len(raw)
        nwords = ln // 4
        if nwords < 4:
            return [], "SKIP:body_too_small"
        mo, no = mask_words(raw, relocs)
        if mo is None:
            return [], "SKIP:" + no
        if no * 2 >= nwords:
            return [], "SKIP:mask_ge_half"
        cands = self.bylen.get(ln, ())
        status = "ok"
        if len(cands) > self.cap:
            self.capped[ln] = len(cands)
            status = "CAPPED"
            cands = cands[:self.cap]
        out = []
        lo, hi = self.text
        for va in cands:
            a = va
            if shift_rnd is not None:
                a = shift_rnd.randrange(lo, max(hi - ln, lo + 4)) & ~3
            rb = retail_bytes(self.x.img, a, ln)
            if rb is None:
                continue
            mr, _ = mask_words(rb, relocs)
            if mr == mo:
                out.append(va)
        res = (out, status)
        if shift_rnd is None:
            self._cache[name] = res
        return res


def calibrate_locator(x, loc, n=1500, seed=20260802):
    rnd = random.Random(seed)
    pop = [nm for nm in x.ours if len(x.name2addr.get(nm, ())) == 1]
    pop = rnd.sample(pop, min(n, len(pop)))
    rec = collections.Counter()
    mult = collections.Counter()
    nullhits = collections.Counter()
    for nm in pop:
        va = x.name2addr[nm][0]
        hs, st = loc.hits(nm)
        if st.startswith("SKIP"):
            rec[st] += 1
            continue
        rec["CAPPED" if st == "CAPPED" else
            ("FOUND_incl_true" if va in hs else
             ("FOUND_other_only" if hs else "EMPTY"))] += 1
        mult[min(len(hs), 10)] += 1
        hn, _ = loc.hits(nm, shift_rnd=rnd)
        nullhits[min(len(hn), 10)] += 1
    tot = sum(rec.values())
    print("RECALL control on %d mapped symbols:" % tot)
    for k, v in rec.most_common():
        print("   %-28s %6d  (%5.1f%%)" % (k, v, 100.0 * v / tot))
    print("  |hits| multiplicity (treatment): %s"
          % sorted(mult.items()))
    print("  |hits| RANDOM-OFFSET NULL      : %s" % sorted(nullhits.items()))
    if loc.capped:
        print("  CAPPED length buckets (cap=%d), LOGGED not silent: %s"
              % (loc.cap, dict(loc.capped)))
    return rec




# ======================= CLI =======================
def _main():
    import argparse
    ap = argparse.ArgumentParser(description="cross-binary callee adjudication")
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--calibrate", action="store_true",
                    help="run BOTH channels' positive controls and nulls")
    a = ap.parse_args()
    x = Xbin(a.root)
    print("our bodies %d (%d dup-name/differing, LOGGED)  map names %d  pdata %d"
          % (len(x.ours), x.dup_diff, len(x.name2addr), len(x.pd)))
    if a.calibrate:
        pos, null, nullc = calibrate_xbin(x)
        for label, c in (("CH1 POSITIVE CONTROL ours[X] vs retail@addr(X)", pos),
                         ("CH1 NULL unconditioned  (VACUOUS - see docstring)", null),
                         ("CH1 NULL length-conditioned", nullc)):
            r, d = _rate(c)
            print("\n%s\n   MATCH %6d  DIFF %6d  -> %.2f%% of %d decided"
                  % (label, c["MATCH"], c["DIFF"], r, d))
            for k, v in sorted(c.items()):
                if k.startswith("SKIP"):
                    print("      %-30s %6d" % (k, v))
        print()
        loc = Locator(x, cap=20000)
        calibrate_locator(x, loc)


if __name__ == "__main__":
    _main()
