#!/usr/bin/env python3
"""Adjudicate whether each retail MSVC EH funclet is pinned to the unit that EMITTED it.

WHAT IT ANSWERS
    A funclet (`__unwind$N` / `__catch$N`, rendered `fn_<VA>` by dtk) is a piece of
    its PARENT function's COMDAT.  `config/45410914/splits.txt` pins address ranges
    to units; if a boundary falls between a parent and its own funclet, the funclet
    lands in a unit whose object never emitted it, and objdiff then pairs it by byte
    signature against whatever twin that wrong unit happens to supply (W16-BF §5).
    Verdict per funclet:
        HOMED       >=1 referencing parent lives in the funclet's pinned unit
        MIS-PINNED  referencing parents exist, none of them in the pinned unit
        ORPHAN      the only referencing parent sits in an unpinned (`auto_*`) region,
                    so no pin move can pair it

MECHANISM (all big-endian; retail image only, no build artifacts required)
    1. Every `0x19930522` FuncInfo in orig/45410914/band.exe (8,541 of them).
    2. Its UnwindMap (maxState x {toState, action}) and TryBlockMap
       (nTryBlocks x 20B -> HandlerType x 16B -> addressOfHandler) give the funclets
       it owns: 25,784 distinct unwind targets + 537 catch targets = 26,321.
    3. Parent = the 8-byte retail EH prefix {__CxxFrameHandler, &FuncInfo}; the
       function starts 4 bytes after the FuncInfo word.  A catch funclet carries its
       OWN prefix pointing at the parent's FuncInfo, so a FuncInfo can have 2 prefix
       sites; the parent is the site whose start is not itself a funclet target.
    4. Pinned unit = splits.txt `.text` ranges, keyed on the FULL heading path
       (never basename(): 707 bare vs 569 nested headings, and `Movie.obj` genuinely
       collides between rnddx9/ and rndobj/).

POPULATION
    All 26,321 funclet targets reachable from a FuncInfo in the retail image.

BLIND SPOTS / WHAT IT CANNOT SEE
    * Non-C++-EH funclets, and any funclet whose FuncInfo my magic scan misses.
    * It does NOT prove a receiving unit's compiled object supplies a byte-equal
      twin -- a re-home into a unit with no matching COMDAT drops the row to 0%.
      Price a move separately (see W16-BJ doc); PINHOME-1: re-homing is NOT
      metric-neutral, unlike adding a pin over auto_* code.
    * `report.json`'s per-row `address` field is a SECTION-RELATIVE OFFSET, not a VA.
      This tool keys on the `fn_<VA>` row NAME.  Using `address` as a VA is a silent
      vacuity (measured: 0/59,105 rows agree that way).
    * ORPHAN is a statement about OUR pinning, not about retail.

REWRITE MODE (--emit-splits / --apply, added by lane W16-BM)
    Mechanically moves every MIS-PINNED funclet's address span out of its donor
    unit's `.text` line(s) and into its parent's unit, emitting a new splits.txt.
    ONLY `.text` lines are touched -- `.pdata` is DERIVED OUTPUT (dtk clears and
    re-derives the whole `.pdata` split set every run), so editing it is both
    pointless and harmful.  Asserted in code, not promised in prose:
      A1 the covered-address set is IDENTICAL before and after (a partition:
         no byte gained, lost, or double-covered) -- computed from the ranges
      A2 no two output ranges overlap, within or across headings
      A3 every changed/added/removed line is a `.text` line or belongs to a
         wholly-deleted heading entry
      A4 no heading survives with zero `.text` lines; a heading whose last
         block drains has its ENTIRE entry removed (an empty unit emits a
         42-byte obj and report.json hard-fails on it -- see CLAUDE.md)
      A5 every new range boundary coincides with a known symbol start or an
         original block boundary (never carves a function in half)
      A6 ranges inside a heading come out sorted and maximally merged
      A7 idempotence: re-classifying the emitted file reports 0 MIS-PINNED
         among the moved rows, with ORPHAN/UNPINNED-FUNCLET counts unmoved
    A catch funclet's own 8-byte EH prefix at addr-8 travels WITH it (measured:
    all 59 catch rows in the MIS-PINNED class have {__CxxFrameHandler,&FuncInfo}
    there and none of those 8 bytes lies inside another report row).

TWIN PROBE (--twin-probe) -- what it can and CANNOT predict
    Does the RECEIVING unit's compiled object supply a relocation-masked body
    twin for the funclet?  Used to split a sweep into "twin available" (expected
    to hold/gain) and "remainder" (expected to lose false-twin credit).
    ⛔ It slices our COFF ITSELF rather than through `coff_bodies_ext.
    function_bodies_ext`, because that yielder calls `is_aux_code_symbol` and so
    SKIPS every `__unwind$` / `__ehhandler$` symbol -- i.e. exactly the funclet
    bodies wanted here.  Built on it, every row would read "no twin": a silently
    vacuous decisive negative.
    ⛔⛔ W16-BJ §5.5: a relocation-masked comparator predicts
    `matched_functions` (mpn) and is STRUCTURALLY INCAPABLE of predicting
    `matched_code` (fuzzy), because it masks exactly the relocation-name
    information `name_check` charges.  State which measure you are predicting.

SELF-VALIDATION (run with --validate; all must hold)
    8,541 FuncInfos - 25,784 unwind targets - 537 catch targets
    537 extra EH-prefix sites, every one a catch funclet
    splits-derived pin == report.json unit membership on every fn_<VA> row
"""
import argparse, bisect, collections, json, os, pickle, re, struct, sys
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from eh_state_screen import Image          # reuse, do not rewrite

MAGIC   = 0x19930522
HANDLER = 0x82829530


def load_eh(img):
    """-> (funclet -> {'funcinfos': set, 'kinds': set}), {funcinfo -> parent}"""
    d = img.d
    text_lo = text_ra = text_sz = None
    for va, vsz, ra, rsz in img.secs:
        if img.base + va == 0x82270000:
            text_lo, text_ra, text_sz = 0x82270000, ra, rsz
    text_hi = text_lo + text_sz
    # 1. FuncInfos
    fis = []
    needle = struct.pack('>I', MAGIC)
    for va, vsz, ra, rsz in img.secs:
        if rsz == 0:
            continue
        blob, off = d[ra:ra + rsz], 0
        while True:
            i = blob.find(needle, off)
            if i < 0:
                break
            if i % 4 == 0:
                fis.append(img.base + va + i)
            off = i + 1
    # 2. funclet targets
    funclets = {}
    for f in fis:
        ms, pu = img.be32(f + 4), img.be32(f + 8)
        nt, pt = img.be32(f + 0x0C), img.be32(f + 0x10)
        if pu and ms and 0 < ms < 4096:
            for k in range(ms):
                a = img.be32(pu + 8 * k + 4)
                if a and text_lo <= a < text_hi:
                    e = funclets.setdefault(a, {'funcinfos': set(), 'kinds': set()})
                    e['funcinfos'].add(f); e['kinds'].add('unwind')
        if pt and nt and 0 < nt < 4096:
            for t in range(nt):
                ent = pt + 20 * t
                nc, ph = img.be32(ent + 12), img.be32(ent + 16)
                if not ph or not nc or nc > 4096:
                    continue
                for c in range(nc):
                    h = img.be32(ph + 16 * c + 12)
                    if h and text_lo <= h < text_hi:
                        e = funclets.setdefault(h, {'funcinfos': set(), 'kinds': set()})
                        e['funcinfos'].add(f); e['kinds'].add('catch')
    # 3. parents via the 8-byte EH prefix
    fiset, text = set(fis), d[text_ra:text_ra + text_sz]
    sites = {}
    for i in range(4, len(text) - 4, 4):
        w = struct.unpack_from('>I', text, i)[0]
        if w in fiset and struct.unpack_from('>I', text, i - 4)[0] == HANDLER:
            sites.setdefault(w, []).append(text_lo + i)
    parents, ambig = {}, []
    for f, vas in sites.items():
        cand = [v + 4 for v in sorted(vas)]
        nonf = [c for c in cand if c not in funclets]
        if len(nonf) == 1:
            parents[f] = nonf[0]
        else:
            parents[f] = None
            ambig.append((f, cand, nonf))
    return funclets, parents, sites, fis, ambig


def load_pins(root):
    blocks, heading = [], None
    for line in open(os.path.join(root, 'config/45410914/splits.txt')):
        if not line.strip():
            continue
        if not line[0].isspace():
            m = re.match(r'^(\S.*?):\s*$', line)
            if m and line.strip() != 'Sections:':
                heading = m.group(1)
            continue
        m = re.match(r'\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', line)
        if m and heading:
            blocks.append((int(m.group(1), 16), int(m.group(2), 16), heading))
    blocks.sort()
    return blocks


def unit_lookup(blocks):
    st = [b[0] for b in blocks]
    def f(a):
        i = bisect.bisect_right(st, a) - 1
        return blocks[i][2] if i >= 0 and blocks[i][0] <= a < blocks[i][1] else None
    return f


def norm_unit(h):
    return None if h is None else 'default/' + re.sub(r'\.(cpp|c|cc|cxx|s|asm)$', '', h)


def classify(root):
    img = Image(os.path.join(root, 'orig/45410914/band.exe'))
    funclets, parents, sites, fis, ambig = load_eh(img)
    blocks = load_pins(root)
    unit_of = unit_lookup(blocks)
    out = {}
    for a, e in funclets.items():
        pars = sorted({parents[f] for f in e['funcinfos'] if parents.get(f)})
        punits = [unit_of(p) for p in pars]
        pin = unit_of(a)
        if not pars:
            v = 'NO-PARENT'
        elif pin is not None and pin in punits:
            v = 'HOMED'
        elif all(pu is None for pu in punits):
            v = 'ORPHAN'
        elif pin is None:
            v = 'UNPINNED-FUNCLET'
        else:
            v = 'MIS-PINNED'
        out[a] = dict(addr=a, kind='/'.join(sorted(e['kinds'])), fanin_fi=len(e['funcinfos']),
                      parents=pars, parent_units=punits, pinned=pin, verdict=v)
    return out, dict(funcinfos=len(fis), funclets=len(funclets), sites=sites,
                     ambig=ambig, blocks=len(blocks), parents=parents)


# ══════════════════════════════════════════════════════════════════════════════
# W16-BM: mechanical re-home of MIS-PINNED funclets (splits.txt `.text` only)
# ══════════════════════════════════════════════════════════════════════════════
SPLITS_REL = 'config/45410914/splits.txt'
TEXT_FMT = '\t.text       start:0x%08X end:0x%08X\n'
HDR_RE = re.compile(r'^(\S.*?):\s*$')
SEC_RE = re.compile(r'^\s+(\S+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)\s*$')


class Refuse(Exception):
    """An assertion that must ABORT rather than emit a plausible-looking file."""


def load_splits_file(path):
    """-> (lines, entries). entries: [{heading, hidx, secs:[(idx,sec,s,e)], other:[idx]}]"""
    lines = open(path).readlines()
    entries, cur = [], None
    for i, l in enumerate(lines):
        if l.strip() and not l[:1].isspace():
            m = HDR_RE.match(l)
            if m and l.strip() != 'Sections:':
                cur = dict(heading=m.group(1), hidx=i, secs=[], other=[])
                entries.append(cur)
            else:
                cur = None
            continue
        if cur is None:
            continue
        m = SEC_RE.match(l)
        if m:
            cur['secs'].append((i, m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
        else:
            cur['other'].append(i)
    return lines, entries


def text_ranges(entries):
    """-> {heading: [(start, end), ...]} over `.text` lines only."""
    out = {}
    for e in entries:
        r = [(s, en) for _i, sec, s, en in e['secs'] if sec == '.text']
        if r:
            out[e['heading']] = sorted(r)
    return out


def merge_touching(ivs, heading='?'):
    """Sort + merge ranges that TOUCH; REFUSE on any overlap.
    ⚠ ONLY for computing the covered-address SET (A1). Applying it to a heading's
    lines collapses the thousands of pre-existing ADJACENT blocks the hand-
    maintained file keeps separate (measured: 6,683 -> 4,361 `.text` lines), which
    is a whole-file reformat AND changes dtk's `.pdata` derivation -- jeff emits one
    `.pdata` range PER `.text` BLOCK. Use sort_check + merge_dirty on real lines."""
    out = []
    for s, e in sorted(ivs):
        if e <= s:
            raise Refuse('A2: empty/inverted range 0x%08X-0x%08X in %s' % (s, e, heading))
        if out and s < out[-1][1]:
            raise Refuse('A2: overlapping ranges in %s: 0x%08X-0x%08X vs 0x%08X-0x%08X'
                         % (heading, out[-1][0], out[-1][1], s, e))
        if out and s == out[-1][1]:
            out[-1] = (out[-1][0], e)
        else:
            out.append((s, e))
    return [tuple(x) for x in out]


def sort_check(ivs, heading='?'):
    """Sort and REFUSE on overlap/emptiness (A2). Never merges."""
    out = []
    for s, e in sorted(ivs):
        if e <= s:
            raise Refuse('A2: empty/inverted range 0x%08X-0x%08X in %s' % (s, e, heading))
        if out and s < out[-1][1]:
            raise Refuse('A2: overlapping ranges in %s: 0x%08X-0x%08X vs 0x%08X-0x%08X'
                         % (heading, out[-1][0], out[-1][1], s, e))
        out.append((s, e))
    return out


def merge_dirty(ivs, dirty, heading='?'):
    """Merge a range ONLY where a MOVED span touches its neighbour (A6). Ranges the
    move never touched are left exactly as the file had them."""
    out, mark = [], []
    for s, e in sort_check(ivs, heading):
        d = (s, e) in dirty
        if out and out[-1][1] == s and (d or mark[-1]):
            out[-1] = (out[-1][0], e); mark[-1] = mark[-1] or d
        else:
            out.append((s, e)); mark.append(d)
    return [tuple(x) for x in out]


def _drop_drained(after):
    """Remove every heading left with zero `.text` ranges; returns their names.
    Extracted so a mutation test can sabotage it and show A4 trip."""
    drained = sorted(h for h, v in after.items() if not v)
    for h in drained:
        after.pop(h)
    return drained


def covered_set(ranges_by_heading):
    """Global covered-address interval set, normalized. REFUSES on cross-heading overlap."""
    flat = [iv for r in ranges_by_heading.values() for iv in r]
    return merge_touching(flat, heading='<global>')


def load_row_sizes(root):
    """{VA: size} from report.json `fn_<VA>` rows. int()-coerced (values are JSON strings)."""
    p = os.path.join(root, 'build/45410914/report.json')
    if not os.path.exists(p):
        raise Refuse('report.json absent -- funclet extents are unknown; refusing to guess')
    rep = json.load(open(p))
    out = {}
    for u in rep.get('units', []):
        for f in u.get('functions', []):
            n = f.get('name', '')
            if n.startswith('fn_') and len(n) == 11:
                out[int(n[3:], 16)] = int(f.get('size', 0))
    return out


def build_plan(root, res, sizes, bars=(), img=None):
    """One move record per MIS-PINNED funclet. Bars are matched on the FULL heading
    AND on its basename (deliberately over-broad: a concurrency collision costs more
    than a skipped row). Returns (plan, skipped)."""
    blocks = load_pins(root)
    unit_of = unit_lookup(blocks)
    starts = [b[0] for b in blocks]

    def block_of(x):
        i = bisect.bisect_right(starts, x) - 1
        return blocks[i] if i >= 0 and blocks[i][0] <= x < blocks[i][1] else None

    barset = set(bars)
    plan, skipped = [], []
    for a in sorted(res):
        r = res[a]
        if r['verdict'] != 'MIS-PINNED':
            continue
        donor = r['pinned']
        if len(r['parents']) != 1:
            raise Refuse('0x%08X has %d parents -- fan-in 1 is assumed by the mover'
                         % (a, len(r['parents'])))
        recv = unit_of(r['parents'][0])
        if recv is None:
            raise Refuse('0x%08X: parent 0x%08X is unpinned (should be ORPHAN, not MIS-PINNED)'
                         % (a, r['parents'][0]))
        hit = [h for h in (donor, recv)
               if h in barset or h.split('/')[-1] in barset]
        if hit:
            skipped.append(dict(addr=a, donor=donor, recv=recv, bar=sorted(set(hit))))
            continue
        sz = sizes.get(a, 0)
        if sz <= 0:
            raise Refuse('0x%08X has no report row / zero size -- extent unknown' % a)
        s0 = a
        # A catch funclet owns the 8-byte EH prefix at addr-8, so it travels with it --
        # BUT ONLY when those 8 bytes are inside the SAME donor block.  Measured: some
        # catch funclets sit exactly AT a block start, so the prefix belongs to the
        # preceding (different) heading and moving it would straddle two donors.  The
        # prefix is an `except_data_` symbol objdiff marks Hidden (numerator AND
        # denominator), so leaving it behind costs nothing.
        blk = block_of(a)
        if (img is not None and 'catch' in r['kind'] and img.be32(a - 8) == HANDLER
                and blk is not None and a - 8 >= blk[0]):
            s0 = a - 8
        plan.append(dict(addr=a, size=sz, start=s0, end=a + sz, donor=donor,
                         recv=recv, kind=r['kind']))
    return plan, skipped


def apply_plan(root, plan, sizes, out_path=None):
    """Rewrite splits.txt `.text` lines for `plan`. Returns a stats dict.
    Every assertion below REFUSES (raises) rather than emitting a suspect file."""
    path = os.path.join(root, SPLITS_REL)
    lines, entries = load_splits_file(path)
    before = text_ranges(entries)
    before_cov = covered_set(before)

    # ── recompute per-heading ranges ────────────────────────────────────────
    after = {h: list(v) for h, v in before.items()}
    moved_spans = []
    dirty = collections.defaultdict(set)      # heading -> {(s,e)} spans the move created
    for h in after:
        after[h] = sort_check(after[h], h)
    for mv in plan:
        d, rc, s, e = mv['donor'], mv['recv'], mv['start'], mv['end']
        if d not in after:
            raise Refuse('donor heading %r has no .text lines' % d)
        rem, hit = [], False
        for bs, be in after[d]:
            if e <= bs or s >= be:
                rem.append((bs, be)); continue
            if not (bs <= s and e <= be):
                raise Refuse('0x%08X-0x%08X straddles donor %s block 0x%08X-0x%08X'
                             % (s, e, d, bs, be))
            hit = True
            if bs < s:
                rem.append((bs, s))
            if e < be:
                rem.append((e, be))
        if not hit:
            raise Refuse('0x%08X-0x%08X not inside any block of donor %s' % (s, e, d))
        after[d] = sorted(rem)
        after.setdefault(rc, []).append((s, e))
        dirty[rc].add((s, e))
        moved_spans.append((s, e, d, rc))
    for h in list(after):
        after[h] = merge_dirty(after[h], dirty.get(h, set()), h)      # A6

    # ── A1: the covered-address set is IDENTICAL ────────────────────────────
    after_cov = covered_set({h: v for h, v in after.items() if v})
    if after_cov != before_cov:
        bb = sum(e - s for s, e in before_cov); aa = sum(e - s for s, e in after_cov)
        raise Refuse('A1: covered-address set CHANGED (%d intervals/%d B -> %d/%d B)'
                     % (len(before_cov), bb, len(after_cov), aa))

    # ── A5: every new boundary lands on a known symbol / block edge ─────────
    known = set()
    for va, sz in sizes.items():
        known.add(va); known.add(va + sz)
    for h, v in before.items():
        for s, e in v:
            known.add(s); known.add(e)
    for mv in plan:
        known.add(mv['start'])                      # catch prefix start
    bad = [(h, s, e) for h, v in after.items() for s, e in v
           if s not in known or e not in known]
    if bad:
        raise Refuse('A5: %d new boundary/ies not on a known symbol edge, e.g. %s'
                     % (len(bad), bad[:3]))

    # ── A8: a heading no move touched must be BYTE-IDENTICAL ───────────────
    touched = {mv['donor'] for mv in plan} | {mv['recv'] for mv in plan}
    reformatted = [h for h, v in after.items()
                   if h not in touched and v != before.get(h)]
    if reformatted:
        raise Refuse('A8: %d untouched heading(s) were REFORMATTED, e.g. %s'
                     % (len(reformatted), reformatted[:5]))

    # ── A4: a heading whose last .text block drained loses its WHOLE entry ──
    drained = _drop_drained(after)
    if any(not v for v in after.values()):
        raise Refuse('A4: a heading survived with zero .text ranges (an empty unit '
                     'emits a 42-byte obj and report.json hard-fails on it)')

    # ── emit, preserving formatting; only .text lines may differ ───────────
    out, deleted_lines = [], set()
    for e in entries:
        if e['heading'] in drained:
            deleted_lines.add(e['hidx'])
            for i, _s, _a, _b in e['secs']:
                deleted_lines.add(i)
            for i in e['other']:
                deleted_lines.add(i)
    first_text = {}
    for e in entries:
        t = [i for i, sec, _s, _en in e['secs'] if sec == '.text']
        if t:
            first_text[e['heading']] = min(t)
    text_line_idx = {}
    for e in entries:
        for i, sec, _s, _en in e['secs']:
            if sec == '.text':
                text_line_idx[i] = e['heading']
    for i, l in enumerate(lines):
        if i in deleted_lines:
            continue
        h = text_line_idx.get(i)
        if h is None:
            out.append(l); continue
        if first_text.get(h) == i:                    # emit the whole new set here
            for s, en in after[h]:
                out.append(TEXT_FMT % (s, en))
        # every other original .text line of this heading is dropped
    new_text = ''.join(out)

    # ── A3: with the deleted entries excluded, ONLY `.text` lines may differ ─
    import difflib
    kept = [l for i, l in enumerate(lines) if i not in deleted_lines]
    changed_non_text = []
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(
            a=kept, b=out, autojunk=False).get_opcodes():
        if tag == 'equal':
            continue
        for l in kept[i1:i2] + out[j1:j2]:
            m = SEC_RE.match(l)
            if m and m.group(1) == '.text':
                continue
            changed_non_text.append(l)
    if changed_non_text:
        raise Refuse('A3: %d non-.text line(s) changed, e.g. %r'
                     % (len(changed_non_text), changed_non_text[:3]))

    if out_path:
        open(out_path, 'w').write(new_text)
    return dict(moved=len(plan), spans=len(merge_touching(
                    [(s, e) for s, e, _d, _r in moved_spans], '<moved>')),
                drained=drained, before_lines=sum(len(v) for v in before.values()),
                after_lines=sum(len(v) for v in after.values()),
                before_bytes=sum(e - s for s, e in before_cov),
                after_bytes=sum(e - s for s, e in after_cov), text=new_text)


# ── twin probe ───────────────────────────────────────────────────────────────
def our_code_slices(objpath):
    """(name, body, relocs) for EVERY type-0x20 code def -- INCLUDING `__unwind$`
    and `__catch$`, which `coff_bodies_ext.function_bodies_ext` deliberately drops
    via is_aux_code_symbol(). Building a FUNCLET twin test on that yielder returns
    'no twin' for every row: a silently vacuous decisive negative."""
    from pathlib import Path as _P
    from icf_fold_evidence import parse_coff, IMAGE_SCN_CNT_CODE
    from coff_bodies_ext import eh_boundaries, eh_prefix_end, IMAGE_REL_PPC_PAIR
    data = _P(objpath).read_bytes()
    sections, symbols = parse_coff(data)
    idx_name, by_sec = {}, {}
    for s in symbols:
        if s is None:
            continue
        idx_name[s['idx']] = s['name']
        if s['section'] > 0:
            by_sec.setdefault(s['section'] - 1, []).append(s)
    for si, sec in enumerate(sections):
        if not (sec['chars'] & IMAGE_SCN_CNT_CODE):
            continue
        defs = [s for s in by_sec.get(si, [])
                if s['name'] != sec['name'] and s['storage'] in (2, 3) and s['type'] == 0x20]
        if not defs:
            continue
        raw = sec['raw']
        pts = sorted({(s['value'], s['name']) for s in defs})
        bounds = sorted({v for v, _n in pts} | {len(raw)})
        nxt = {v: bounds[i + 1] for i, v in enumerate(bounds[:-1])}
        marks = eh_boundaries(by_sec.get(si, []))
        rel = {}
        for (o, i, t) in sec['relocs']:
            if t != IMAGE_REL_PPC_PAIR:
                rel.setdefault(o, idx_name.get(i, '?'))
        for v, name in pts:
            if v % 4 or v >= len(raw):
                continue
            end = eh_prefix_end(nxt.get(v, len(raw)), v, marks, raw, rel)
            rl = [(o - v, idx_name.get(i, '?'), t) for (o, i, t) in sec['relocs'] if v <= o < end]
            yield name, raw[v:end], rl


def mask_at(body, relocs):
    b = bytearray(body)
    for off, _n, _t in relocs:
        if off + 4 <= len(b):
            b[off:off + 4] = b'\x00\x00\x00\x00'
    return bytes(b)


def unit_base_objs(root):
    """{heading-derived unit name: base obj abspath} from objdiff.json."""
    cfg = json.load(open(os.path.join(root, 'objdiff.json')))
    out = {}
    for u in cfg.get('units', []):
        b = u.get('base_path')
        if b:
            out[u['name']] = os.path.join(root, b)
    return out


def twin_probe(root, plan, img):
    """Per move: does the RECEIVING unit's compiled obj supply a relocation-masked
    twin of the retail funclet body? -> {addr: (bool, matched_symbol_or_None)}

    ⛔ This predicts `mpn`/`matched_functions` only (W16-BJ §5.5): the masking
    removes exactly the relocation-NAME information `name_check` charges."""
    bases = unit_base_objs(root)
    by_recv = {}
    for mv in plan:
        by_recv.setdefault(mv['recv'], []).append(mv)
    out, no_obj = {}, set()
    for recv, mvs in by_recv.items():
        unit = norm_unit(recv)
        p = bases.get(unit)
        if not p or not os.path.exists(p):
            no_obj.add(recv)
            for mv in mvs:
                out[mv['addr']] = (False, None)
            continue
        bysize = {}
        for name, body, rl in our_code_slices(p):
            bysize.setdefault(len(body), []).append((name, body, rl))
        for mv in mvs:
            f = img.v2f(mv['addr'])
            if f is None:
                out[mv['addr']] = (False, None); continue
            want = img.d[f:f + mv['size']]
            hit = None
            for name, body, rl in bysize.get(mv['size'], []):
                if mask_at(want, rl) == mask_at(body, rl):
                    hit = name; break
            out[mv['addr']] = (hit is not None, hit)
    return out, no_obj

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=ROOT)
    ap.add_argument('--validate', action='store_true')
    ap.add_argument('--addrs', nargs='*', default=[])
    ap.add_argument('--verdict')
    ap.add_argument('--limit', type=int, default=30)
    ap.add_argument('--pickle')
    # ── W16-BM rewrite mode ────────────────────────────────────────────────
    ap.add_argument('--emit-splits', metavar='OUT',
                    help='write a re-homed splits.txt to OUT (does not touch the tree)')
    ap.add_argument('--apply', action='store_true',
                    help='rewrite config/45410914/splits.txt IN PLACE')
    ap.add_argument('--exclude-heading', action='append', default=[], metavar='H',
                    help='concurrency bar: skip any row whose donor OR receiver is H '
                         '(matched on the full heading AND on its basename); repeatable')
    ap.add_argument('--batch', choices=['twin', 'notwin', 'all'], default='all',
                    help="twin = rows whose RECEIVER already supplies a masked body twin; "
                         "notwin = the remainder; all = both")
    ap.add_argument('--moves-json', metavar='OUT', help='write the move plan + twin verdicts')
    ap.add_argument('--twin-probe', action='store_true', help='report twin availability only')
    ap.add_argument('--twin-control', action='store_true',
                    help='anti-vacuity control: run the twin probe on HOMED rows in their '
                         'OWN unit, where a twin MUST exist for a fuzzy==100 row')
    a = ap.parse_args()
    res, meta = classify(a.root)
    print("FuncInfos=%d funclet targets=%d .text pins=%d" % (meta['funcinfos'], meta['funclets'], meta['blocks']))
    fan = Counter(r['fanin_fi'] for r in res.values())
    print("fan-in (funclet -> #FuncInfos):", dict(sorted(fan.items())))
    print("verdicts:", dict(Counter(r['verdict'] for r in res.values())))
    if a.validate:
        ok = True
        ok &= meta['funcinfos'] == 8541
        ok &= meta['funclets'] == 26321
        # Extra prefix sites split into two populations, and the split is the point:
        #   537 are catch funclets carrying their OWN prefix (not folded parents)
        #     4 are the surplus parents of the ONE folded no-action FuncInfo
        #       (0x820A1EF0, maxState=1 action=0 -- it owns NO funclet).
        # That folded record is the POSITIVE CONTROL: it proves this scanner can
        # see .xdata folding, so funclet fan-in==1 everywhere is a measured
        # absence of funclet-level ICF, not a blind spot.
        extra_catch = extra_other = 0
        for f, vas in meta['sites'].items():
            for v in sorted(vas)[1:] if len(vas) > 1 else []:
                pass
            starts = [v + 4 for v in sorted(vas)]
            for st in starts:
                if st in res and 'catch' in res[st]['kind']:
                    extra_catch += 1
        extra_other = sum(len(v) - 1 for v in meta['sites'].values()) - extra_catch
        print("extra EH-prefix sites: %d catch-funclet + %d folded-parent (expect 537 + 4)"
              % (extra_catch, extra_other))
        ok &= extra_catch == 537 and extra_other == 4
        print("ambiguous FuncInfos:", len(meta['ambig']), "(expect 1: the folded no-action record)")
        print("VALIDATE:", "PASS" if ok else "FAIL")
        if not ok:
            sys.exit(3)
    for s in a.addrs:
        r = res.get(int(s, 16))
        print(r)
    if a.verdict:
        sel = [r for r in res.values() if r['verdict'] == a.verdict]
        sel.sort(key=lambda r: r['addr'])
        print("--- %s: %d ---" % (a.verdict, len(sel)))
        for r in sel[:a.limit]:
            print("  0x%08X %-6s parents=%s parent_units=%s pinned=%s" % (
                r['addr'], r['kind'], [hex(p) for p in r['parents']], r['parent_units'], r['pinned']))
    if a.pickle:
        pickle.dump(res, open(a.pickle, 'wb'))

    # ── W16-BM: twin control / probe / rewrite ─────────────────────────────
    if a.twin_control or a.twin_probe or a.emit_splits or a.apply or a.moves_json:
        img = Image(os.path.join(a.root, 'orig/45410914/band.exe'))
        sizes = load_row_sizes(a.root)
        if a.twin_control:
            blocks = load_pins(a.root)
            uo = unit_lookup(blocks)
            ctl = [dict(addr=k, size=sizes.get(k, 0), start=k, end=k + sizes.get(k, 0),
                        donor=r['pinned'], recv=r['pinned'], kind=r['kind'])
                   for k, r in sorted(res.items())
                   if r['verdict'] == 'HOMED' and sizes.get(k, 0) > 0]
            tw, _no = twin_probe(a.root, ctl, img)
            rep = json.load(open(os.path.join(a.root, 'build/45410914/report.json')))
            fz = {}
            for u in rep['units']:
                for f in u.get('functions', []):
                    fz[f['name']] = float(f.get('fuzzy_match_percent', 0.0))
            import collections as _c
            tab = _c.Counter()
            for m in ctl:
                band = 'fuzzy==100' if fz.get('fn_%08X' % m['addr'], 0.0) == 100.0 else 'fuzzy<100'
                tab[(band, tw[m['addr']][0])] += 1
            print('--- TWIN CONTROL on HOMED rows (twin sought in their OWN unit) ---')
            for band in ('fuzzy==100', 'fuzzy<100'):
                y, n = tab[(band, True)], tab[(band, False)]
                print('  %-10s TWIN %6d   NO-TWIN %6d   twin-rate %.1f%%'
                      % (band, y, n, 100.0 * y / max(1, y + n)))
            return
        plan, skipped = build_plan(a.root, res, sizes, bars=a.exclude_heading, img=img)
        print('plan: %d movable rows, %d skipped under a bar' % (len(plan), len(skipped)))
        for s in skipped:
            print('  SKIPPED 0x%08X  %s -> %s  (bar: %s)'
                  % (s['addr'], s['donor'], s['recv'], ','.join(s['bar'])))
        tw, no_obj = twin_probe(a.root, plan, img)
        ntw = sum(1 for v in tw.values() if v[0])
        print('twin available in receiver: %d / %d   (receivers with no base obj: %d)'
              % (ntw, len(plan), len(no_obj)))
        if a.batch == 'twin':
            plan = [m for m in plan if tw[m['addr']][0]]
        elif a.batch == 'notwin':
            plan = [m for m in plan if not tw[m['addr']][0]]
        print('batch=%s -> %d rows / %d B' % (a.batch, len(plan), sum(m['size'] for m in plan)))
        if a.moves_json:
            json.dump(dict(plan=plan, skipped=skipped,
                           twin={('0x%08X' % k): v[0] for k, v in tw.items()},
                           twin_sym={('0x%08X' % k): v[1] for k, v in tw.items()},
                           no_base_obj=sorted(no_obj)),
                      open(a.moves_json, 'w'), indent=1)
            print('wrote', a.moves_json)
        if a.twin_probe and not (a.emit_splits or a.apply):
            return
        out = a.emit_splits or (os.path.join(a.root, SPLITS_REL) if a.apply else None)
        if out:
            st = apply_plan(a.root, plan, sizes, out_path=out)
            print('EMIT %s: .text lines %d -> %d   covered bytes %d -> %d (must be equal)'
                  % (out, st['before_lines'], st['after_lines'],
                     st['before_bytes'], st['after_bytes']))
            print('  merged moved spans: %d   headings DRAINED (entry removed): %s'
                  % (st['spans'], st['drained'] or 'none'))


if __name__ == '__main__':
    main()
