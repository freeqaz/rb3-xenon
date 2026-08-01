#!/usr/bin/env python3
"""MAP-INDEPENDENT class<->function attribution from retail RTTI + vtables.

WHY THIS EXISTS (lane CH-4): lane CG-1 adjudicated destructor map rows by
asking *the map* what a retail callee is. That is sound only if the callee's
own map row is correct -- CG-1 said so explicitly and did not check it. This
tool answers "which class does retail itself say function A belongs to?"
using ONLY bytes in band.exe, so it can police the map from outside.

THE CHAIN (MSVC 32-bit, non-image-relative):
    ??_R0 TypeDescriptor   +0 type_info vftable ptr, +4 spare, +8 ".?AVFoo@@"
    ??_R4 CompleteObjLoc   +0 sig=0, +4 offset, +8 cdOffset,
                           +12 -> ??_R0, +16 -> ??_R3
    vtable                 [-4] -> ??_R4 ; slots at [0..] are .text VAs

So: string -> TD -> COL -> vtable -> slots. Every step is a retail byte read.

NOT the same instrument as CG-1's DEAD vtable-reference detector. That one
asked "is address A referenced by ANY data word?" -- undirected, fired on
58.7% of random .pdata function starts (1.15x enrichment). This one is
DIRECTED: it asks "is A in the vtable that RTTI labels class C?", and the
untreated-population control below measures how selective that is.

CONFOUNDS THIS TOOL DOES NOT HIDE:
  - Inheritance: a non-overriding derived class's vtable repeats the base's
    slot value, so one VA can legitimately belong to several classes. Reported
    as a set, never collapsed to one answer.
  - ICF: reloc-identical bodies fold, so one VA can serve two classes.
    Indistinguishable from inheritance here, and treated the same way.
"""
import struct, sys, os, json, re, collections, random

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from retail_reader import Image, insns, branch_target  # noqa: E402


class Rtti:
    def __init__(self, img):
        self.img = img
        self.rdata = next(s for s in img.secs if s['name'] == '.rdata')
        self.text = next(s for s in img.secs if s['name'] == '.text')
        self.rd_lo = img.base + self.rdata['va']
        self.rd_hi = self.rd_lo + self.rdata['rsz']
        self.tx_lo = img.base + self.text['va']
        self.tx_hi = self.tx_lo + self.text['vsz']
        self.tds = {}        # td_va -> name (".?AVFoo@@")
        self.cols = {}       # col_va -> dict(td, offset, cd)
        self.vtables = {}    # vt_va  -> col_va
        self._scan()

    def is_text(self, va):
        return self.tx_lo <= va < self.tx_hi

    def is_rdata(self, va):
        return self.rd_lo <= va < self.rd_hi

    def _blobs(self):
        """(va_lo, bytes) for every file-backed section we care about.
        ⚠ TypeDescriptors live in .data (type_info is WRITABLE), while COLs and
        vtables live in .rdata (const). Scanning only .rdata finds ZERO TDs --
        that was this lane's first failed selftest."""
        out = []
        for s in self.secs_all:
            if s['rsz'] == 0 or s['name'] not in ('.rdata', '.data'):
                continue
            lo = self.img.base + s['va']
            out.append((lo, self.img.d[s['ptr']:s['ptr'] + s['rsz']]))
        return out

    def _scan(self):
        self.secs_all = self.img.secs
        blobs = self._blobs()

        # --- step 1: TypeDescriptors, located by their name string ---------
        for lo, blob in blobs:
            for m in re.finditer(rb'\.\?A[VUW][\x20-\x7e]{0,400}?@@\x00', blob):
                td_off = m.start() - 8
                if td_off < 0:
                    continue
                spare = struct.unpack_from('>I', blob, td_off + 4)[0]
                if spare != 0:
                    continue
                self.tds[lo + td_off] = m.group()[:-1].decode('ascii', 'replace')

        # --- step 2: Complete Object Locators pointing at a TD -------------
        for lo, blob in blobs:
            n = len(blob)
            for off in range(0, n - 20, 4):
                if struct.unpack_from('>I', blob, off)[0] != 0:   # signature
                    continue
                tdp = struct.unpack_from('>I', blob, off + 12)[0]
                if tdp not in self.tds:
                    continue
                r3 = struct.unpack_from('>I', blob, off + 16)[0]
                if not self.is_rdata(r3):                          # ??_R3
                    continue
                self.cols[lo + off] = dict(
                    td=tdp,
                    offset=struct.unpack_from('>I', blob, off + 4)[0],
                    cd=struct.unpack_from('>I', blob, off + 8)[0])

        # --- step 3: vtables = word pointing at a COL, slots follow --------
        for lo, blob in blobs:
            n = len(blob)
            for off in range(0, n - 8, 4):
                w = struct.unpack_from('>I', blob, off)[0]
                if w in self.cols:
                    if self.is_text(struct.unpack_from('>I', blob, off + 4)[0]):
                        self.vtables[lo + off + 4] = w

    def slots(self, vt_va, maxn=400):
        """Consecutive .text words starting at vt_va. Stops at the first
        non-.text word -- which in practice is the next vtable's COL ptr."""
        out = []
        va = vt_va
        while len(out) < maxn:
            w = self.img.word(va)
            if w is None or not self.is_text(w):
                break
            out.append(w)
            va += 4
        return out

    def cls_of_td(self, td_va):
        nm = self.tds.get(td_va)
        return nm

    def build_attribution(self):
        """VA -> {class name -> [(vtable, slot_index, col_offset)]}"""
        attr = collections.defaultdict(lambda: collections.defaultdict(list))
        vt_slots = {}
        for vt, col in self.vtables.items():
            nm = self.cls_of_td(self.cols[col]['td'])
            sl = self.slots(vt)
            vt_slots[vt] = (nm, self.cols[col]['offset'], sl)
            for i, fn in enumerate(sl):
                attr[fn][nm].append((vt, i, self.cols[col]['offset']))
        self.vt_slots = vt_slots
        self.attr = attr
        return attr


def demangle_class(name):
    """'.?AVFoo@@' -> 'Foo' ; '.?AVBar@Hmx@@' -> 'Bar@Hmx'."""
    if not name.startswith('.?A'):
        return None
    body = name[4:]
    if body.endswith('@@'):
        body = body[:-2]
    return body


# ------------------------------------------------------------------ selftest
def selftest():
    img = Image()
    r = Rtti(img)
    ok = True
    print(f'TypeDescriptors {len(r.tds)}   COLs {len(r.cols)}   '
          f'vtables {len(r.vtables)}')

    # CONTROL 1 (positive, documented): CLAUDE.md records 2,220 ??_R4 COLs in
    # retail .rdata as the verified /GR evidence. We must land near that.
    near = 2000 <= len(r.cols) <= 2500
    print(f'  [C1] COL count {len(r.cols)} vs documented 2,220 -> '
          f'{"OK" if near else "OFF -- parser suspect"}')
    ok &= near

    # CONTROL 2 (positive, named): classes we know exist must be present.
    names = {demangle_class(v) for v in r.tds.values()}
    want = ['MasterAudio', 'RndTex', 'Object@Hmx', 'RndDir']
    for w in want:
        hit = w in names
        print(f'  [C2] class {w!r} present -> {"OK" if hit else "MISSING"}')
        ok &= hit

    # CONTROL 3 (FAIL ON DEMAND): a class name that cannot exist must be
    # absent. If this "finds" something the matcher is hallucinating.
    bogus = 'ZzNotARealClass_CH4'
    hit = bogus in names
    print(f'  [C3] fail-on-demand: bogus class {bogus!r} present={hit} -> '
          f'{"OK (absent)" if not hit else "BROKEN"}')
    ok &= not hit

    # CONTROL 4 (FAIL ON DEMAND, RANDOMIZED NULL).
    # ⚠ The first version of this control was WRONG and I am recording why.
    # It re-ran the COL scan reading the TD pointer at +8 instead of +12 and
    # expected far fewer hits; it found 1337 vs 2220 and "failed". That is a
    # STRUCTURAL ALIAS, not a real signal: for a true COL at X, the window
    # starting at X+4 has `offset`(==0) where the signature is read and lands
    # on X+12 -- the real TD pointer -- when it reads +8. The shifted window
    # simply re-discovers the same COLs. It measured window alignment, not
    # whether the TD field carries information.
    # The correct null keeps the scan, the section, and the alignment
    # identical (the state met in the wild) and randomizes only the thing
    # under test: the SET OF TD ADDRESSES.
    real_tds = set(r.tds)
    lo_hi = []
    for lo, blob in r._blobs():
        lo_hi.append((lo, len(blob)))
    random.seed(99)
    fake = set()
    while len(fake) < len(real_tds):
        lo, ln = random.choice(lo_hi)
        a = lo + 4 * random.randrange(ln // 4)
        if a not in real_tds:
            fake.add(a)
    nfake = 0
    for lo, blob in r._blobs():
        n = len(blob)
        for off in range(0, n - 20, 4):
            if struct.unpack_from('>I', blob, off)[0] != 0:
                continue
            if struct.unpack_from('>I', blob, off + 12)[0] not in fake:
                continue
            if not r.is_rdata(struct.unpack_from('>I', blob, off + 16)[0]):
                continue
            nfake += 1
    print(f'  [C4] randomized-TD null: {nfake} COLs from {len(fake)} FAKE TD '
          f'addresses vs {len(r.cols)} from {len(real_tds)} real -> '
          f'{"OK (TD identity is load-bearing)" if nfake < len(r.cols) // 20 else "BROKEN (scan fits noise)"}')
    ok &= nfake < len(r.cols) // 20

    # CONTROL 5 (UNTREATED-POPULATION NULL): how often does a RANDOM .text
    # function start appear in some vtable? This is the base rate the
    # attribution must beat. The null must reproduce the state met in the
    # wild -- FUNCTION STARTS -- not arbitrary shifted addresses (that is the
    # exact error CG-1 documented, which turned 1.15x into a fake 45x).
    r.build_attribution()
    pd = pdata_starts(img)
    random.seed(1234)
    samp = random.sample(pd, 2000)
    infn = sum(1 for a in samp if a in r.attr)
    print(f'  [C5] untreated null: {infn}/2000 = {100*infn/2000:.1f}% of random '
          f'.pdata function starts appear in SOME vtable')
    print(f'       (denominator printed on purpose; attribution claims must be '
          f'read against this base rate)')

    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


def pdata_starts(img):
    """Function start VAs from .pdata (RUNTIME_FUNCTION: BeginAddress, flags)."""
    s = next(x for x in img.secs if x['name'] == '.pdata')
    out = []
    for off in range(s['ptr'], s['ptr'] + s['rsz'], 8):
        va = struct.unpack_from('>I', img.d, off)[0]
        if va == 0:
            continue
        out.append(va)
    return out


def pdata_sizes(img, shift=8):
    """VA -> size, from .pdata RUNTIME_FUNCTION word 2.
    Xenon layout: prologLen = data & 0xFF, funcLen = (data >> 8) & 0x3FFFFF,
    in INSTRUCTIONS (x4 for bytes).

    ⚠ CG-1 found a real >>2-vs->>8 bug in its own copy of this. Do not trust
    the constant -- `validate_sizes` below is the control, and `shift` is a
    parameter precisely so the wrong value can be demonstrated to fail."""
    s = next(x for x in img.secs if x['name'] == '.pdata')
    out = {}
    for off in range(s['ptr'], s['ptr'] + s['rsz'], 8):
        va, w2 = struct.unpack_from('>II', img.d, off)
        if va == 0:
            continue
        out[va] = ((w2 >> shift) & 0x3FFFFF) * 4
    return out


def validate_sizes(img, shift=8):
    """CONTROL: for adjacent .pdata entries, start+size should land exactly on
    the next start (functions are laid out contiguously). Returns the exact-hit
    rate. A wrong shift cannot pass this."""
    sizes = pdata_sizes(img, shift)
    starts = sorted(sizes)
    exact = over = 0
    for a, b in zip(starts, starts[1:]):
        e = a + sizes[a]
        if e == b:
            exact += 1
        elif e > b:
            over += 1
    n = len(starts) - 1
    return exact, over, n


if __name__ == '__main__':
    sys.exit(selftest())
