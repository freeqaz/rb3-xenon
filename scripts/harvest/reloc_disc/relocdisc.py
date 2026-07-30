#!/usr/bin/env python3
"""laneAS-B — RELOCATION-CONTENT discriminator for reloc-masked byte twins.

★ THIS FILE IS A LIBRARY. It has no CLI, no argparse and no __main__ guard.
  Running it directly prints nothing and exits 0 — that is not a broken install,
  it simply has no entry point. (An earlier version of this docstring advertised
  "--heldout" and "--emit" modes that were never implemented here; laneBT5
  corrected it. Treat "no output, rc=0" in this tree as failure until proven
  otherwise — cf. class_layout_report.py, fixed in b55f10d8.)

  The real entry points, all with __main__ guards, are:
    lblindex.py            <worktree> <lblidx.json>      build the lbl VA->bytes index
    bodyidx.py             <worktree> <bodyidx.pkl>      build the ICF body index
    heldout_reloc.py       --worktree --lblidx [--out]   leave-one-out precision
    emit_reloc_frag.py     --worktree --lblidx --bodyidx --funnels --out --census
    collision_adjudicate.py / collision_control.py       map name-collision channel
  See README.md in this directory for the pipeline and the measured precision.

A masked-byte-equal class is equal *only because the relocated words were
zeroed*. This scores each candidate base symbol against the target function by
recovering what was zeroed on both sides and checking consistency:

  base reloc symbol NAME  --resolve-->  VA   ==?  target operand VA
                          --decode--->  bytes ==? bytes living at target VA

Resolution channels
  MAP      name -> VA via scripts/target_symbol_map.json (inverted) and
           config/45410914/symbols.txt (dtk helper/label names)
  CONTENT  ??_C@_0..  string literal   -> demangled bytes vs bytes at lbl_<VA>
           __real@/__realdb@/__xmm@    -> constant bytes vs bytes at lbl_<VA>
  NAME     target operand is itself a name (helpers) -> direct string compare

Verdict per candidate: (agree, contradict, unknown, shape_mismatch).
Decisive iff exactly one candidate has contradict==0 and shape_mismatch==0.

Modes
  --heldout   leave-one-out precision over real map entries
  --emit      produce a shippable fragment for the live EXACT_AMBIG pool
"""
import argparse
import importlib.util
import json
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R  # noqa: E402

FUNCLET_LIKE = re.compile(r"^(__unwind\$|__catch\$|\?\?__E|\?\?__F|fn_)")
PPC_PAIR = 0x12          # IMAGE_REL_PPC_PAIR: "symbol index" is really an addend
HELPER_RX = re.compile(r"^__(save|rest)(gpr|fpr|vmx)")

# --------------------------------------------------------------------------
# MSVC string-literal demangling:  ??_C@_0<len>@<hash>@<chars>@
# --------------------------------------------------------------------------
_SPECIAL = ",/\\:. \n\t'-"
_STR_RX = re.compile(r"^\?\?_C@_(?P<w>[01])(?P<rest>.*)@$")


def _demangle_chars(enc: str) -> bytes:
    out = bytearray()
    i = 0
    while i < len(enc):
        c = enc[i]
        if c != "?":
            out.append(ord(c) & 0xFF)
            i += 1
            continue
        if i + 1 >= len(enc):
            break
        n = enc[i + 1]
        if n == "$":
            if i + 3 >= len(enc):
                break
            hi, lo = enc[i + 2], enc[i + 3]
            if not ("A" <= hi <= "P" and "A" <= lo <= "P"):
                return bytes(out)
            out.append(((ord(hi) - 65) << 4) | (ord(lo) - 65))
            i += 4
            continue
        if n.isdigit():
            out.append(ord(_SPECIAL[int(n)]))
            i += 2
            continue
        if "a" <= n <= "z":
            out.append(0xE0 + (ord(n) - 97))
            i += 2
            continue
        if "A" <= n <= "Z":
            out.append(0xC0 + (ord(n) - 65))
            i += 2
            continue
        return bytes(out)
    return bytes(out)


def string_bytes(name: str):
    """(is_wide, decoded prefix bytes) for a ??_C@ literal symbol, else None."""
    m = _STR_RX.match(name)
    if not m:
        return None
    rest = m.group("rest")
    # rest = <lenmangled>@<hash>@<chars>
    parts = rest.split("@")
    if len(parts) < 3:
        return None
    chars = "@".join(parts[2:])
    return (m.group("w") == "1", _demangle_chars(chars))


_REAL_RX = re.compile(r"^__(real|realdb|xmm)@([0-9a-fA-F]+)$")


def const_bytes(name: str):
    m = _REAL_RX.match(name)
    if not m:
        return None
    h = m.group(2)
    if len(h) % 2:
        return None
    try:
        return bytes.fromhex(h)
    except ValueError:
        return None


_R0_RX = re.compile(r"^\?\?_R0(.+)@8$")


def rtti_name_bytes(name: str):
    """MSVC ??_R0<type>@8 type descriptor: the mangled type name lives inline at
    offset 8 of the record, as '.' + <type> + NUL."""
    m = _R0_RX.match(name)
    if not m:
        return None
    return (8, ("." + m.group(1)).encode("latin1") + b"\0")


# --------------------------------------------------------------------------
class Disc:
    def __init__(self, wt: Path, lblidx: dict, resolver: R.Resolver, S):
        self.wt = wt
        self.lbl = lblidx
        self.res = resolver
        self.S = S

    def cmp_one(self, bname: str, ttok: str):
        """-> ('AGREE'|'CONTRA'|'UNK'|'NOISE', channel)."""
        if ttok is None:
            return ("UNK", "none")
        tva = R.token_va(ttok)
        bn = self.S.anon_ns_strip(bname)
        if tva is None:
            # target operand is itself a symbolic name (helper, or a name dtk
            # already knows). Direct comparison.
            if ttok == bn:
                return (("NOISE" if HELPER_RX.match(ttok) else "AGREE"), "name")
            if HELPER_RX.match(ttok) and HELPER_RX.match(bn):
                return ("CONTRA", "name")
            return ("UNK", "name")
        # --- MAP channel
        vas = self.res.vas(bn)
        if vas:
            if tva in vas:
                return (("NOISE" if HELPER_RX.match(bn) else "AGREE"), "map")
            return ("CONTRA", "map")
        # --- CONTENT channel
        raw = self.lbl.get(tva) or self.lbl.get(str(tva))
        sb = string_bytes(bn)
        if sb is not None:
            if raw is None:
                return ("UNK", "string")
            have = bytes.fromhex(raw)
            wide, want = sb
            n = min(len(want), len(have), 32)
            if n == 0:
                return ("UNK", "string")
            return (("AGREE" if have[:n] == want[:n] else "CONTRA"), "string")
        rt = rtti_name_bytes(bn)
        if rt is not None:
            if raw is None:
                return ("UNK", "rtti")
            have = bytes.fromhex(raw)
            off, want = rt
            n = min(len(want), max(0, len(have) - off))
            if n < 6:
                return ("UNK", "rtti")
            return (("AGREE" if have[off:off + n] == want[:n] else "CONTRA"), "rtti")
        cb = const_bytes(bn)
        if cb is not None:
            if raw is None:
                return ("UNK", "const")
            have = bytes.fromhex(raw)
            n = min(len(cb), len(have))
            if n == 0:
                return ("UNK", "const")
            return (("AGREE" if have[:n] == cb[:n] else "CONTRA"), "const")
        return ("UNK", "none")

    def score(self, trel, brel):
        """trel: [(off, token)]   brel: [(off, name, type)]"""
        tb = defaultdict(list)
        for off, tok in trel:
            tb[off].append(tok)
        bb = defaultdict(list)
        for off, nm, ty in brel:
            if ty == PPC_PAIR:
                continue
            bb[off].append(nm)
        agree = contra = unk = 0
        shape = 0
        det = []
        for off in sorted(set(tb) | set(bb)):
            ts, bs = tb.get(off, []), bb.get(off, [])
            if not ts or not bs:
                shape += 1
                continue
            # ★ cmp_one returns (verdict, channel) -- must be unpacked. Comparing
            # the raw tuple to a string made agree/contra permanently 0 (laneBT5).
            v, ch = self.cmp_one(bs[0], ts[0])
            det.append((off, bs[0], ts[0], v, ch))
            if v == "AGREE":
                agree += 1
            elif v == "CONTRA":
                contra += 1
            elif v == "UNK":
                unk += 1
        return dict(agree=agree, contra=contra, unk=unk, shape=shape, det=det)


# --------------------------------------------------------------------------
def unit_iter(wt: Path):
    od = json.loads((wt / "objdiff.json").read_text())
    for u in od["units"]:
        tp, bp = u.get("target_path"), u.get("base_path")
        if not tp or not bp:
            continue
        if u["name"].rsplit("/", 1)[-1].startswith("auto_"):
            continue
        yield (u["name"], wt / tp,
               wt / (tp.replace("/obj/", "/asm/")[:-4] + ".s"), wt / bp)


def decide(disc, tinfo, cands, use_r1=False, use_r2=False):
    """Restrict scoring to DISCRIMINATING relocation offsets: offsets where the
    candidates do not all carry the same base symbol name. A contradiction on a
    shared offset penalises every candidate equally and only means our
    resolution is broken there, so it must not be allowed to eliminate anyone.

    R1 (optional): drop offsets no candidate can explain (resolution failure).
    R2 (optional): drop ICF-degenerate offsets (two competing callee symbols are
        themselves reloc-masked byte twins -> they fold to one address).

    -> (pick, tier, per-candidate detail)
    """
    tb = {}
    for off, tok in tinfo["relocs"]:
        tb.setdefault(off, tok)
    cmaps = []
    for c in cands:
        m = {}
        for off, nm, ty in c["relocs"]:
            if ty == PPC_PAIR:
                continue
            m.setdefault(off, nm)
        cmaps.append(m)
    alloff = set(tb)
    for m in cmaps:
        alloff |= set(m)
    disc_offs = [o for o in sorted(alloff)
                 if len({m.get(o) for m in cmaps}) > 1]

    bi = getattr(disc, "bodyidx", None)
    if use_r2 and bi is not None:
        keep = []
        for o in disc_offs:
            nms = {m.get(o) for m in cmaps if m.get(o)}
            hs = defaultdict(set)
            for n in nms:
                for h in bi.get(disc.S.anon_ns_strip(n), ()):
                    hs[h].add(n)
            if any(len(v) > 1 for v in hs.values()):
                continue
            keep.append(o)
        disc_offs = keep

    mat = []
    for c, m in zip(cands, cmaps):
        row = {}
        for o in disc_offs:
            bn, tt = m.get(o), tb.get(o)
            if bn is None and tt is None:
                continue
            if bn is None or tt is None:
                row[o] = ("SHAPE", "shape", bn, tt)
                continue
            v, ch = disc.cmp_one(bn, tt)
            row[o] = (v, ch, bn, tt)
        mat.append(row)

    if use_r1:
        disc_offs = [o for o in disc_offs
                     if any(r.get(o, ("",))[0] == "AGREE" for r in mat)]

    vs = []
    for c, row in zip(cands, mat):
        agree = contra = unk = 0
        chans = Counter()
        det = []
        for o in disc_offs:
            if o not in row:
                continue
            v, ch, bn, tt = row[o]
            det.append((o, bn, tt, v, ch))
            if v == "AGREE":
                agree += 1
                chans[ch] += 1
            elif v in ("CONTRA", "SHAPE"):
                contra += 1
            elif v == "UNK":
                unk += 1
        vs.append((c, dict(agree=agree, contra=contra, unk=unk,
                           chans=dict(chans), det=det)))

    if not disc_offs:
        return None, "NO_DISCRIM_RELOC", vs
    surv = [(c, s) for c, s in vs if s["contra"] == 0]
    if len(surv) == 1:
        c, s = surv[0]
        tier = "DECISIVE" if s["agree"] > 0 else "ELIM_ONLY"
        return disc.S.anon_ns_strip(c["name"]), tier, vs
    if not surv:
        return None, "ALL_CONTRA", vs
    anyev = any(s["agree"] or s["contra"] for _, s in vs)
    return None, ("TIE_WITH_EVIDENCE" if anyev else "NO_EVIDENCE"), vs
