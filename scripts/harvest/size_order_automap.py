#!/usr/bin/env python3
"""size_order_automap — recover fn_<VA> -> MSVC-mangled pairings for a carved
game TU by aligning the compiled obj's function sequence (source/emission order)
against the dtk target obj's function sequence (address order), by SIZE + ORDER,
anchored by reloc-masked BYTE IDENTITY.

Why this works
--------------
Under /O1 no-LTCG, retail preserves TU-internal source order in `.text` (proven
project-wide). So the functions a TU contributes to retail's `.text` appear in
the SAME relative order as the COMDAT sections MSVC emits into our compiled obj.
objdiff pairs target<->base by symbol NAME, but the target side is anonymous
`fn_<VA>` (dtk split) while our side is MSVC-mangled. Aligning the two ordered
sequences by (order, size) — anchored by reloc-masked byte-identity — recovers
the pairing WITHOUT re-mangling anything by hand.

Data sources (per unit, from build/45410914/)
    obj/<rel>/<TU>.obj   dtk target obj   -> address-order fns, sizes, relocs
    asm/<rel>/<TU>.s     dtk target asm   -> fn_<VA> list (rename-IMMUNE: keeps
                                             fn_<VA> even after the target-symbol
                                             renamer mangles the .obj symbols)
    src/<rel>/<TU>.obj   our compiled obj -> mangled fns, sizes, relocs

Algorithm
    1. Reloc-masked byte-identity, set-wise (order-independent): a target fn that
       is byte-identical to EXACTLY ONE compiled fn is a self-proving pairing.
       Tier EXACT. These are hard anchors.
    2. Segment the two sequences between consecutive (order-consistent) anchors,
       and run a monotone size DP inside each segment for the remaining fns.
       size==0 delta + unique-in-segment  -> STRONG
       size delta (<=SIZE_TOL) or ambiguous -> WEAK
    3. Fragment = EXACT + STRONG (never WEAK, never __unwind$ funclets).

Output
    --emit <frag.json>  apply-ready fragment for tu5_map_apply_fragment.py
    a human evidence table (VA, name, sizes, tier, byte-identity%).

Read-only / deterministic: never writes the map, only EMITS a fragment.

Usage
    size_order_automap.py --unit default/band3/meta_band/NewAwardPanel
    size_order_automap.py --unit NewAwardPanel --emit /tmp/frag.json
    size_order_automap.py --validate NewAwardPanel AppLabel NetSync
    size_order_automap.py --span 0x825EFC78,0x825F1E4C   # target-only preview
"""
import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

PROJECT_ROOT = Path(__file__).resolve().parents[2]
BUILD = PROJECT_ROOT / "build" / "45410914"
MAP_PATH = PROJECT_ROOT / "scripts" / "target_symbol_map.json"
SPLITS_PATH = PROJECT_ROOT / "config" / "45410914" / "splits.txt"

SIZE_TOL = 8            # bytes: max |size delta| tolerated for a WEAK size match
IMAGE_SCN_CNT_CODE = 0x20


# ---------------------------------------------------------------------------
# COFF parsing (borrows the layout math from tu5_reloc_masked_correlate.py).
# ---------------------------------------------------------------------------
def _parse_coff(path: Path):
    data = path.read_bytes()
    nsec = struct.unpack_from("<H", data, 2)[0]
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    optsz = struct.unpack_from("<H", data, 16)[0]
    str_start = symoff + nsym * 18
    secs = {}
    base = 20 + optsz
    for i in range(nsec):
        o = base + i * 40
        raw_size = struct.unpack_from("<I", data, o + 16)[0]
        praw = struct.unpack_from("<I", data, o + 20)[0]
        preloc = struct.unpack_from("<I", data, o + 24)[0]
        nreloc = struct.unpack_from("<H", data, o + 32)[0]
        chars = struct.unpack_from("<I", data, o + 36)[0]
        relocs = [struct.unpack_from("<I", data, preloc + j * 10)[0]
                  for j in range(nreloc)]
        secs[i + 1] = dict(raw_size=raw_size, praw=praw, relocs=relocs,
                           chars=chars, idx=i + 1)
    syms = []
    i = 0
    while i < nsym:
        o = symoff + i * 18
        nm = data[o:o + 8]
        if nm[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", nm, 4)[0]
            e = data.index(b"\0", str_start + so)
            name = data[str_start + so:e].decode("latin1")
        else:
            name = nm.split(b"\0")[0].decode("latin1")
        val = struct.unpack_from("<I", data, o + 8)[0]
        secn = struct.unpack_from("<h", data, o + 12)[0]
        typ = struct.unpack_from("<H", data, o + 14)[0]
        sc = data[o + 16]
        naux = data[o + 17]
        syms.append(dict(name=name, val=val, secn=secn, typ=typ, sc=sc))
        i += 1 + naux
    return data, secs, syms


def _ordered_funcs(path: Path) -> List[dict]:
    """Ordered list of code function symbols with reloc-masked bodies.

    Order = (section index, value) which is emission/address order. Each entry:
      {name, secn, val, size, masked}
    A section may hold a function plus its trailing EH funclets (multiple func
    symbols) — bodies are sliced between consecutive symbol values.
    """
    data, secs, syms = _parse_coff(path)
    by_sec: Dict[int, List[dict]] = defaultdict(list)
    for sy in syms:
        if (sy["secn"] > 0 and sy["secn"] in secs
                and sy["typ"] == 0x20 and sy["sc"] in (2, 3)):
            s = secs[sy["secn"]]
            if s["chars"] & IMAGE_SCN_CNT_CODE:
                by_sec[sy["secn"]].append(sy)
    out: List[dict] = []
    for secn in sorted(by_sec):
        s = secs[secn]
        members = sorted(by_sec[secn], key=lambda x: x["val"])
        for k, sy in enumerate(members):
            start = sy["val"]
            end = members[k + 1]["val"] if k + 1 < len(members) else s["raw_size"]
            if s["praw"] == 0 or end <= start:
                continue
            body = bytearray(data[s["praw"] + start:s["praw"] + end])
            for rva in s["relocs"]:
                if start <= rva < end:
                    off = rva - start
                    for b in range(4):
                        if off + b < len(body):
                            body[off + b] = 0
            out.append(dict(name=sy["name"], secn=secn, val=start,
                            size=end - start, masked=bytes(body)))
    return out


# ---------------------------------------------------------------------------
# Target asm: rename-immune address-ordered (VA, size, reloc-masked bytes).
#
# The dtk asm keeps fn_<VA> names even after the target-symbol renamer mangles
# the .obj (verified), and its emission order IS address order (the .obj section
# order is NOT reliably address order — verified on NetSync). Reloc-masked bytes
# built from the asm are byte-identical to obj-reloc-masked bytes (verified
# 101/101 on NewAwardPanel), so identity comparison against the compiled obj is
# consistent.
# ---------------------------------------------------------------------------
_FN_RX = re.compile(r"^\.fn (fn_[0-9A-Fa-f]{8}|\S+),", re.M)
_INSTR_RX = re.compile(
    r"^/\* [0-9A-Fa-f]{8} [0-9A-Fa-f]{8}  "
    r"([0-9A-Fa-f]{2}) ([0-9A-Fa-f]{2}) ([0-9A-Fa-f]{2}) ([0-9A-Fa-f]{2}) \*/"
    r"\t(\S+)(.*)$")
_RELOC_AT = ("@ha", "@l", "@sda21")
_RELOC_SYM = re.compile(r"\b(fn_|lbl_|jmp_|__save|__rest|__fpr|__gpr|__mem|__vec)")


def _operand_relocated(operand: str) -> bool:
    """True if a dtk asm operand carries a relocation (so its 4 instruction
    bytes must be masked). Local `.L_` branches are PC-relative (no reloc)."""
    if any(t in operand for t in _RELOC_AT):
        return True
    for tok in operand.replace(",", " ").split():
        if tok.startswith(".L_"):
            continue
        if tok.startswith("?") or tok.startswith("@"):
            return True
        if _RELOC_SYM.search(tok):
            return True
    return False


def _asm_target_funcs(asm_path: Path) -> List[Tuple[Optional[int], int, bytes]]:
    """Address-ordered [(VA, size, masked_bytes)] from the dtk asm. VA is None
    for a `.fn` that is not fn_<VA> (already-renamed non-VA symbol)."""
    out: List[Tuple[Optional[int], int, bytes]] = []
    cur_va: Optional[int] = None
    in_fn = False
    words: List[bytes] = []
    for line in asm_path.read_text().splitlines():
        fm = _FN_RX.match(line)
        if fm:
            tok = fm.group(1)
            vm = re.match(r"fn_([0-9A-Fa-f]{8})$", tok)
            cur_va = int(vm.group(1), 16) if vm else None
            in_fn = True
            words = []
            continue
        if line.startswith(".endfn"):
            if in_fn:
                body = b"".join(words)
                out.append((cur_va, len(body), body))
            in_fn = False
            continue
        if not in_fn:
            continue
        im = _INSTR_RX.match(line)
        if im:
            b = bytes(int(x, 16) for x in im.groups()[:4])
            if _operand_relocated(im.group(6)):
                b = b"\0\0\0\0"
            words.append(b)
    return out


# ---------------------------------------------------------------------------
# Unit / path resolution.
# ---------------------------------------------------------------------------
def resolve_unit(unit: str) -> Tuple[str, Path, Path, Path]:
    """(unit_name, target_obj, target_asm, compiled_obj) for a unit name/path.

    Accepts 'default/band3/meta_band/NewAwardPanel', a bare 'NewAwardPanel',
    or a path to an obj.
    """
    rel = unit
    if rel.startswith("default/"):
        rel = rel[len("default/"):]
    if rel.endswith(".obj"):
        rel = rel[:-4]
    # bare stem -> search. Prefer a hit whose compiled src/ obj exists (the root
    # obj/<stem>.obj is often a stray split with no source counterpart) and,
    # among those, the deepest path (band3/meta_band/... over a bare root).
    if "/" not in rel:
        hits = list((BUILD / "obj").rglob(rel + ".obj"))
        if not hits:
            raise SystemExit(f"no target obj found for '{unit}' under {BUILD/'obj'}")

        def _has_src(p: Path) -> bool:
            r = str(p.relative_to(BUILD / "obj"))
            return (BUILD / "src" / r).exists()

        hits.sort(key=lambda p: (not _has_src(p), -len(str(p))))
        rel = str(hits[0].relative_to(BUILD / "obj"))[:-4]
    tgt_obj = BUILD / "obj" / (rel + ".obj")
    tgt_asm = BUILD / "asm" / (rel + ".s")
    cmp_obj = BUILD / "src" / (rel + ".obj")
    return "default/" + rel, tgt_obj, tgt_asm, cmp_obj


# ---------------------------------------------------------------------------
# Sequence extraction.
# ---------------------------------------------------------------------------
@dataclass
class TFunc:
    va: int
    size: int
    masked: bytes
    raw_name: str          # obj symbol (fn_<VA> or, if already mapped, mangled)
    idx: int = 0


@dataclass
class CFunc:
    name: str
    size: int
    masked: bytes
    idx: int = 0


def load_target(tgt_obj: Path, tgt_asm: Path) -> List[TFunc]:
    """Target sequence straight from the rename-immune dtk asm."""
    if not tgt_asm.exists():
        raise SystemExit(f"missing target asm: {tgt_asm}")
    out: List[TFunc] = []
    for va, size, masked in _asm_target_funcs(tgt_asm):
        if va is None:
            continue
        out.append(TFunc(va=va, size=size, masked=masked,
                         raw_name=f"fn_{va:08X}"))
    out.sort(key=lambda t: t.va)
    for i, t in enumerate(out):
        t.idx = i
    return out


def load_compiled(cmp_obj: Path) -> List[CFunc]:
    funcs = _ordered_funcs(cmp_obj)
    out = [CFunc(name=f["name"], size=f["size"], masked=f["masked"])
           for f in funcs]
    for i, c in enumerate(out):
        c.idx = i
    return out


def is_internal(name: str) -> bool:
    """Names that must never be emitted as a map pairing (internal/EH/thunk
    artifacts that objdiff does not pair by these mangled forms)."""
    return name.startswith("__unwind$") or name.startswith("$")


_ANON_RX = re.compile(r"\?A0x[0-9a-f]{8}@")


def anon_ns_strip(name: str) -> str:
    """Neutralize MSVC anonymous-namespace hashes (?A0x<hash>@ -> ?A@). Our
    compile and retail derive different hashes from machine-name+source-path;
    the wired `anon_ns` build step reconciles them, so two names that differ
    ONLY here are the same symbol (and are byte-identical)."""
    return _ANON_RX.sub("?A@", name)


# ---------------------------------------------------------------------------
# Alignment.
# ---------------------------------------------------------------------------
@dataclass
class Pairing:
    va: int
    name: str
    tsize: int
    csize: int
    tier: str
    identity: float          # reloc-masked byte identity %  (-1 = not computable)
    note: str = ""


def _identity_pct(a: bytes, b: bytes) -> float:
    if not a and not b:
        return 100.0
    n = max(len(a), len(b))
    if n == 0:
        return 100.0
    same = sum(1 for i in range(min(len(a), len(b))) if a[i] == b[i])
    return 100.0 * same / n


def _segment_dp(T: List[TFunc], C: List[CFunc],
                t_lo: int, t_hi: int, c_lo: int, c_hi: int,
                claimed_c: set) -> List[Tuple[int, int, str]]:
    """Monotone size-DP over T[t_lo:t_hi] x C[c_lo:c_hi] (available C only).

    Returns [(t_index, c_index, tier)] for matched pairs. Skips on either side
    are free-ish; a size-compatible match is rewarded. Tiering by size delta +
    in-segment uniqueness is done by the caller.
    """
    ts = list(range(t_lo, t_hi))
    cs = [c for c in range(c_lo, c_hi) if c not in claimed_c]
    n, m = len(ts), len(cs)
    if n == 0 or m == 0:
        return []
    NEG = float("-inf")

    def score(ti: int, ci: int) -> float:
        d = abs(T[ti].size - C[ci].size)
        if is_internal(C[ci].name):
            return NEG
        if d == 0:
            return 10.0
        if d <= SIZE_TOL:
            return 4.0 - 0.4 * d
        return NEG

    # dp[i][j] best score aligning ts[:i], cs[:j]
    dp = [[0.0] * (m + 1) for _ in range(n + 1)]
    bk = [[0] * (m + 1) for _ in range(n + 1)]  # 0=diag-match 1=skipT 2=skipC
    SKIP_T = -0.5   # target fn with no counterpart (funclet/foreign)
    SKIP_C = -0.05  # compiled fn folded elsewhere (very common, cheap)
    for i in range(1, n + 1):
        dp[i][0] = dp[i - 1][0] + SKIP_T
        bk[i][0] = 1
    for j in range(1, m + 1):
        dp[0][j] = dp[0][j - 1] + SKIP_C
        bk[0][j] = 2
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            best = dp[i - 1][j] + SKIP_T
            arg = 1
            v = dp[i][j - 1] + SKIP_C
            if v > best:
                best, arg = v, 2
            sc = score(ts[i - 1], cs[j - 1])
            if sc != NEG:
                v = dp[i - 1][j - 1] + sc
                if v > best:
                    best, arg = v, 0
            dp[i][j] = best
            bk[i][j] = arg
    # backtrack
    i, j = n, m
    pairs: List[Tuple[int, int]] = []
    while i > 0 or j > 0:
        a = bk[i][j]
        if a == 0 and i > 0 and j > 0:
            pairs.append((ts[i - 1], cs[j - 1]))
            i -= 1
            j -= 1
        elif a == 1 and i > 0:
            i -= 1
        else:
            j -= 1
    pairs.reverse()
    # tier by size-delta + in-segment size uniqueness
    t_size_ct = defaultdict(int)
    for ti in ts:
        t_size_ct[T[ti].size] += 1
    c_size_ct = defaultdict(int)
    for ci in cs:
        c_size_ct[C[ci].size] += 1
    out = []
    for ti, ci in pairs:
        d = abs(T[ti].size - C[ci].size)
        uniq = (t_size_ct[T[ti].size] == 1 and c_size_ct[C[ci].size] == 1)
        if d == 0 and uniq:
            tier = "STRONG"
        else:
            tier = "WEAK"
        out.append((ti, ci, tier))
    return out


def align(T: List[TFunc], C: List[CFunc]) -> List[Pairing]:
    # 1) set-wise reloc-masked byte identity -> EXACT anchors.
    c_by_bytes: Dict[bytes, List[int]] = defaultdict(list)
    for c in C:
        if not is_internal(c.name):
            c_by_bytes[c.masked].append(c.idx)
    anchors: List[Tuple[int, int]] = []     # (t_idx, c_idx)
    claimed_c: set = set()
    claimed_t: set = set()
    exact_notes: Dict[int, str] = {}
    for t in T:
        cands = c_by_bytes.get(t.masked, [])
        cands = [ci for ci in cands if ci not in claimed_c]
        if len(cands) == 1:
            ci = cands[0]
            anchors.append((t.idx, ci))
            claimed_c.add(ci)
            claimed_t.add(t.idx)
    # keep anchors order-consistent in compiled index (drop rare inversions as
    # segment boundaries, but still emit them as EXACT pairings).
    anchors.sort()
    mono: List[Tuple[int, int]] = []
    last_c = -1
    for ti, ci in anchors:
        if ci > last_c:
            mono.append((ti, ci))
            last_c = ci
    boundary = set(id(x) for x in mono)

    pairings: List[Pairing] = []
    for ti, ci in anchors:
        pairings.append(Pairing(
            va=T[ti].va, name=C[ci].name, tsize=T[ti].size, csize=C[ci].size,
            tier="EXACT", identity=100.0,
            note="byte-identical (unique)"))

    # 2) DP within segments bounded by consecutive monotone anchors.
    seg_bounds: List[Tuple[int, int, int, int]] = []
    prev_t, prev_c = -1, -1
    for ti, ci in mono:
        seg_bounds.append((prev_t + 1, ti, prev_c + 1, ci))
        prev_t, prev_c = ti, ci
    seg_bounds.append((prev_t + 1, len(T), prev_c + 1, len(C)))

    for (t_lo, t_hi, c_lo, c_hi) in seg_bounds:
        dp_pairs = _segment_dp(T, C, t_lo, t_hi, c_lo, c_hi, claimed_c)
        for ti, ci, tier in dp_pairs:
            if ti in claimed_t or ci in claimed_c:
                continue
            claimed_t.add(ti)
            claimed_c.add(ci)
            ident = _identity_pct(T[ti].masked, C[ci].masked)
            pairings.append(Pairing(
                va=T[ti].va, name=C[ci].name, tsize=T[ti].size,
                csize=C[ci].size, tier=tier, identity=round(ident, 1),
                note="size+order"))

    pairings.sort(key=lambda p: p.va)
    return pairings


# ---------------------------------------------------------------------------
# Reporting / emit.
# ---------------------------------------------------------------------------
def print_table(unit: str, pairings: List[Pairing], gt: Dict[int, str] = None):
    gt = gt or {}
    print(f"\n=== {unit} — {len(pairings)} proposed pairings ===")
    hdr = f"{'VA':>10} {'tier':<7} {'tsz':>5} {'csz':>5} {'ident%':>7}  name"
    if gt:
        hdr += "   [gt]"
    print(hdr)
    for p in pairings:
        line = (f"0x{p.va:08X} {p.tier:<7} {p.tsize:>5} {p.csize:>5} "
                f"{p.identity:>7}  {p.name[:60]}")
        if gt:
            exp = gt.get(p.va)
            if exp is None:
                mark = "  ·"
            elif exp == p.name:
                mark = "  OK"
            elif anon_ns_strip(exp) == anon_ns_strip(p.name):
                mark = "  OK (anon-ns hash)"
            else:
                mark = f"  XX (gt={exp[:40]})"
            line += mark
        print(line)


def emit_fragment(pairings: List[Pairing], path: Path,
                  cur_map: Dict[str, str]) -> Tuple[int, List[str]]:
    """Write apply-ready fragment (EXACT+STRONG, non-internal, non-colliding)."""
    keys = set(k.lower() for k in cur_map if k.startswith("0x"))
    vals = set(v for v in cur_map.values() if isinstance(v, str))
    frag: Dict[str, str] = {}
    skipped: List[str] = []
    for p in pairings:
        if p.tier not in ("EXACT", "STRONG"):
            continue
        if is_internal(p.name):
            continue
        key = f"0x{p.va:08x}"
        if key.lower() in keys:
            skipped.append(f"{key} addr already mapped")
            continue
        if p.name in vals or p.name in frag.values():
            skipped.append(f"{key} name collision {p.name}")
            continue
        frag[key] = p.name
    path.write_text(json.dumps(frag, indent=1) + "\n")
    return len(frag), skipped


# ---------------------------------------------------------------------------
# Ground-truth (map entries within a unit's pinned span).
# ---------------------------------------------------------------------------
def unit_span(unit_rel: str) -> Optional[Tuple[int, int]]:
    """Parse splits.txt for the .text span of a unit (rel path, no default/)."""
    text = SPLITS_PATH.read_text()
    stem = unit_rel + ".cpp:"
    lines = text.splitlines()
    for i, ln in enumerate(lines):
        if ln.strip() == stem or ln.strip().endswith("/" + unit_rel.split("/")[-1] + ".cpp:"):
            if ln.strip() != stem and not ln.strip().startswith(unit_rel):
                continue
        if ln.strip() == stem:
            for j in range(i + 1, min(i + 6, len(lines))):
                m = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)",
                              lines[j])
                if m:
                    return int(m.group(1), 16), int(m.group(2), 16)
    return None


def ground_truth(unit_rel: str, cur_map: Dict[str, str]) -> Dict[int, str]:
    span = unit_span(unit_rel)
    if not span:
        return {}
    lo, hi = span
    gt = {}
    for k, v in cur_map.items():
        if not k.startswith("0x"):
            continue
        va = int(k, 16)
        if lo <= va < hi:
            gt[va] = v
    return gt


def load_map() -> Dict[str, str]:
    return json.loads(MAP_PATH.read_text())


# ---------------------------------------------------------------------------
# Modes.
# ---------------------------------------------------------------------------
def run_unit(unit: str, emit: Optional[str], show_gt: bool):
    name, tgt_obj, tgt_asm, cmp_obj = resolve_unit(unit)
    if not tgt_obj.exists():
        raise SystemExit(f"missing target obj: {tgt_obj}")
    if not cmp_obj.exists():
        raise SystemExit(f"missing compiled obj: {cmp_obj}")
    T = load_target(tgt_obj, tgt_asm)
    C = load_compiled(cmp_obj)
    pairings = align(T, C)
    cur_map = load_map()
    rel = name[len("default/"):]
    gt = ground_truth(rel, cur_map) if show_gt else {}
    counts = defaultdict(int)
    for p in pairings:
        counts[p.tier] += 1
    print(f"unit={name}  target_fns={len(T)}  compiled_fns={len(C)}")
    print(f"tiers: EXACT={counts['EXACT']} STRONG={counts['STRONG']} "
          f"WEAK={counts['WEAK']}")
    print_table(name, pairings, gt)
    if emit:
        n, skipped = emit_fragment(pairings, Path(emit), cur_map)
        print(f"\n[emit] {n} entries -> {emit}")
        for s in skipped:
            print(f"  skip: {s}")
    return pairings, gt


def run_validate(units: List[str]):
    cur_map = load_map()
    print("=" * 74)
    print("GROUND-TRUTH VALIDATION (proposed EXACT/STRONG vs map entries in span)")
    print("=" * 74)
    grand = defaultdict(int)
    for unit in units:
        name, tgt_obj, tgt_asm, cmp_obj = resolve_unit(unit)
        if not (tgt_obj.exists() and cmp_obj.exists()):
            print(f"\n{unit}: MISSING artifacts, skipped")
            continue
        T = load_target(tgt_obj, tgt_asm)
        C = load_compiled(cmp_obj)
        pairings = align(T, C)
        rel = name[len("default/"):]
        gt = ground_truth(rel, cur_map)
        # precision per tier over pairings whose VA is in ground truth
        per = defaultdict(lambda: [0, 0])   # tier -> [correct, considered]
        emitted_correct = emitted_total = 0
        def eq(a: str, b: str) -> bool:
            return a == b or anon_ns_strip(a) == anon_ns_strip(b)

        for p in pairings:
            if p.va in gt:
                per[p.tier][1] += 1
                if eq(p.name, gt[p.va]):
                    per[p.tier][0] += 1
            if p.tier in ("EXACT", "STRONG") and p.va in gt:
                emitted_total += 1
                if eq(p.name, gt[p.va]):
                    emitted_correct += 1
        cnt = defaultdict(int)
        for p in pairings:
            cnt[p.tier] += 1
        print(f"\n{name}")
        print(f"  target_fns={len(T)} compiled_fns={len(C)} gt_entries={len(gt)}")
        print(f"  proposed: EXACT={cnt['EXACT']} STRONG={cnt['STRONG']} "
              f"WEAK={cnt['WEAK']}")
        for tier in ("EXACT", "STRONG", "WEAK"):
            c, n = per[tier]
            if n:
                print(f"  precision[{tier}]: {c}/{n} "
                      f"= {100.0*c/n:.1f}%")
                grand[tier + "_c"] += c
                grand[tier + "_n"] += n
        # show any mismatches for EXACT/STRONG (should be ~0)
        for p in pairings:
            if p.tier in ("EXACT", "STRONG") and p.va in gt and not eq(p.name, gt[p.va]):
                print(f"    !! {p.tier} MISPAIR 0x{p.va:08X}: got {p.name[:40]} "
                      f"want {gt[p.va][:40]}")
        # coverage: gt entries we did emit correctly
        got = sum(1 for va in gt if any(
            pp.va == va and eq(pp.name, gt[va]) and pp.tier in ("EXACT", "STRONG")
            for pp in pairings))
        print(f"  coverage: {got}/{len(gt)} gt entries recovered "
              f"as EXACT/STRONG")
    print("\n" + "=" * 74)
    for tier in ("EXACT", "STRONG", "WEAK"):
        c, n = grand[tier + "_c"], grand[tier + "_n"]
        if n:
            print(f"OVERALL precision[{tier}]: {c}/{n} = {100.0*c/n:.1f}%")
    ec = grand["EXACT_c"] + grand["STRONG_c"]
    en = grand["EXACT_n"] + grand["STRONG_n"]
    if en:
        print(f"OVERALL precision[EXACT+STRONG]: {ec}/{en} = {100.0*ec/en:.1f}%")


def run_span_preview(span: str):
    lo_s, hi_s = span.split(",")
    lo, hi = int(lo_s, 16), int(hi_s, 16)
    # find the unit whose pinned .text covers [lo,hi)
    print(f"target-only preview for span 0x{lo:08X}..0x{hi:08X}")
    # locate the target asm covering the span. Prefer a real pinned unit over an
    # `auto_*` catch-all blob, then the one with the most in-range functions.
    covers = []
    for asm in (BUILD / "asm").rglob("*.s"):
        vas = [va for va, _, _ in _asm_target_funcs(asm) if va is not None]
        n = sum(1 for v in vas if lo <= v < hi)
        if n:
            covers.append((asm.stem.startswith("auto_"), -n, asm))
    covers.sort()
    hit_unit = covers[0][2] if covers else None
    if hit_unit is None:
        print("  no dtk asm covers this span (unit not carved/pinned yet).")
        print("  Pin the span in splits.txt + `touch config.yml && ninja` first.")
        return
    rel = str(hit_unit.relative_to(BUILD / "asm"))[:-2]
    print(f"  covered by unit: default/{rel}")
    _, tgt_obj, tgt_asm, _ = resolve_unit(rel)
    T = load_target(tgt_obj, tgt_asm)
    inrange = [t for t in T if lo <= t.va < hi]
    print(f"  {len(inrange)} target functions in span:")
    for t in inrange:
        print(f"    0x{t.va:08X}  size={t.size:5d}  {t.raw_name[:50]}")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", help="objdiff unit name / bare stem / obj path")
    ap.add_argument("--emit", help="write apply-ready fragment JSON here")
    ap.add_argument("--validate", nargs="+", metavar="UNIT",
                    help="ground-truth precision over already-mapped units")
    ap.add_argument("--span", help="START,END hex: target-only preview")
    ap.add_argument("--no-gt", action="store_true",
                    help="don't annotate the table with ground truth")
    args = ap.parse_args()

    if args.validate:
        run_validate(args.validate)
    elif args.span:
        run_span_preview(args.span)
    elif args.unit:
        run_unit(args.unit, args.emit, show_gt=not args.no_gt)
    else:
        ap.error("one of --unit / --validate / --span is required")


if __name__ == "__main__":
    main()
