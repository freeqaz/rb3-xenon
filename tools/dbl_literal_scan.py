#!/usr/bin/env python3
"""Double-vs-single float literal divergence scanner (lane DBLLIT-1).

Finds functions where OUR build does double-precision intermediate math
(lfd + fmul/fadd/... + frsp) at a site where RETAIL did single (lfs + fmuls).
The usual cause is an unsuffixed C++ floating literal (a `double`) sitting in
a float expression, forcing widen-compute-narrow.

Two independent sides, deliberately built on different substrates so a bug in
one cannot silently produce agreement:

  RETAIL side : parsed from dtk's per-unit asm listings (build/45410914/asm/*.s),
                keyed on the `.fn fn_<addr>` SYMBOL, never the synthetic
                address column (see CLAUDE.md).
  OUR side    : PowerPC opcode bits read straight out of the compiled COFF
                .obj .text sections.  No disassembler, nothing that can
                silently return an empty result.

Usage:
  dbl_literal_scan.py units            # per-unit retail-vs-ours FP mnemonic table
  dbl_literal_scan.py source           # source census of unsuffixed literals
  dbl_literal_scan.py selftest         # prove both readers can fire
"""
import os
import re
import struct
import sys
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASM = os.path.join(ROOT, "build/45410914/asm")
OBJ = os.path.join(ROOT, "build/45410914/src")

# ---------------------------------------------------------------- retail side

# dtk lines look like:  /* 826B2028 006A6E28  C0 01 00 58 */\tlfs f0, 0x58(r1)
ASM_INSN = re.compile(r"^/\*[^*]*\*/\s*([a-z][a-z0-9_.]*)")
ASM_FN = re.compile(r'^\.fn\s+(\S+?)\s*(?:,|$)')
ASM_ENDFN = re.compile(r"^\.endfn")

# The mnemonics that discriminate double from single intermediate math.
DOUBLE_OPS = {"lfd", "stfd", "fmul", "fadd", "fsub", "fdiv", "fmadd", "fmsub",
              "fnmadd", "fnmsub", "fsqrt", "frsp"}
SINGLE_OPS = {"lfs", "stfs", "fmuls", "fadds", "fsubs", "fdivs", "fmadds",
              "fmsubs", "fnmadds", "fnmsubs", "fsqrts"}
FP_OPS = DOUBLE_OPS | SINGLE_OPS


def parse_asm(path):
    """-> {fn_symbol: Counter(mnemonic)} for one dtk .s listing."""
    out = {}
    cur = None
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            s = line.strip()
            m = ASM_FN.match(s)
            if m:
                cur = m.group(1).strip('"')
                out.setdefault(cur, collections.Counter())
                continue
            if ASM_ENDFN.match(s):
                cur = None
                continue
            m = ASM_INSN.match(s)
            if m and cur is not None:
                mn = m.group(1)
                if mn in FP_OPS:
                    out[cur][mn] += 1
    return out


# ------------------------------------------------------------------ our side
# PowerPC: primary opcode = bits 0..5 (word >> 26).
#   lfs  = 48, lfsu = 49, lfd  = 50, lfdu = 51
#   stfs = 52, stfd = 54
#   opcode 59 (float single arith) / 63 (float double arith), XO in bits 26..30
X_XO = 0x000007FE  # bits 21..30 (A-form uses bits 26..30 -> mask 0x3E)
A_XO = 0x0000003E

A59 = {18: "fdivs", 20: "fsubs", 21: "fadds", 22: "fsqrts", 25: "fmuls",
       28: "fmsubs", 29: "fmadds", 30: "fnmsubs", 31: "fnmadds"}
A63 = {18: "fdiv", 20: "fsub", 21: "fadd", 22: "fsqrt", 25: "fmul",
       28: "fmsub", 29: "fmadd", 30: "fnmsub", 31: "fnmadd"}
PRIMARY = {48: "lfs", 49: "lfsu", 50: "lfd", 51: "lfdu",
           52: "stfs", 54: "stfd"}


def decode(word):
    op = word >> 26
    if op in PRIMARY:
        return PRIMARY[op]
    if op == 59:
        return A59.get((word & A_XO) >> 1)
    if op == 63:
        xo = (word & X_XO) >> 1
        if xo == 12:
            return "frsp"
        return A63.get((word & A_XO) >> 1)
    return None


def coff_text_counts(path):
    """-> Counter(mnemonic) over every .text section of an MSVC COFF obj."""
    data = open(path, "rb").read()
    if len(data) < 20:
        return collections.Counter(), 0
    nsec, = struct.unpack_from("<H", data, 2)
    optsz, = struct.unpack_from("<H", data, 16)
    off = 20 + optsz
    counts = collections.Counter()
    words = 0
    for i in range(nsec):
        base = off + i * 40
        if base + 40 > len(data):
            break
        name = data[base:base + 8].rstrip(b"\0").decode("latin1")
        size, = struct.unpack_from("<I", data, base + 16)
        ptr, = struct.unpack_from("<I", data, base + 20)
        if not name.startswith(".text") or ptr == 0 or size == 0:
            continue
        blob = data[ptr:ptr + size]
        for j in range(0, len(blob) - 3, 4):
            w, = struct.unpack_from(">I", blob, j)
            words += 1
            mn = decode(w)
            if mn:
                counts[mn] += 1
    return counts, words


def coff_symbol_counts(path):
    """-> {symbol_name: Counter(mnemonic)} over .text COMDATs of an MSVC COFF obj.

    Instruction words are attributed to the nearest preceding function symbol
    within the same section, so template COMDATs and multi-symbol sections are
    split correctly rather than lumped together.
    """
    data = open(path, "rb").read()
    if len(data) < 20:
        return {}
    nsec, = struct.unpack_from("<H", data, 2)
    symptr, nsym = struct.unpack_from("<II", data, 8)
    optsz, = struct.unpack_from("<H", data, 16)
    stroff = symptr + nsym * 18
    secoff = 20 + optsz

    sections = {}  # 1-based index -> (name, size, ptr)
    for i in range(nsec):
        base = secoff + i * 40
        if base + 40 > len(data):
            break
        name = data[base:base + 8].rstrip(b"\0").decode("latin1")
        size, = struct.unpack_from("<I", data, base + 16)
        ptr, = struct.unpack_from("<I", data, base + 20)
        sections[i + 1] = (name, size, ptr)

    # collect function symbols
    syms = []  # (secno, value, name)
    i = 0
    while i < nsym:
        off = symptr + i * 18
        if off + 18 > len(data):
            break
        raw = data[off:off + 8]
        if raw[:4] == b"\0\0\0\0":
            so, = struct.unpack_from("<I", raw, 4)
            end = data.find(b"\0", stroff + so)
            name = data[stroff + so:end].decode("latin1")
        else:
            name = raw.rstrip(b"\0").decode("latin1")
        value, secno, typ, sclass, naux = struct.unpack_from("<IhHBB", data, off + 8)
        if secno > 0 and typ == 0x20 and sclass in (2, 3):  # EXTERNAL / STATIC fn
            syms.append((secno, value, name))
        i += 1 + naux

    out = {}
    bysec = collections.defaultdict(list)
    for secno, value, name in syms:
        bysec[secno].append((value, name))
    for secno, entries in bysec.items():
        if secno not in sections:
            continue
        sname, size, ptr = sections[secno]
        if not sname.startswith(".text") or ptr == 0:
            continue
        entries.sort()
        blob = data[ptr:ptr + size]
        for idx, (value, name) in enumerate(entries):
            end = entries[idx + 1][0] if idx + 1 < len(entries) else size
            c = collections.Counter()
            for j in range(value, min(end, len(blob)) - 3, 4):
                w, = struct.unpack_from(">I", blob, j)
                mn = decode(w)
                if mn:
                    c[mn] += 1
            prev = out.get(name)
            if prev is None:
                out[name] = c
            else:
                prev.update(c)
    return out


def find_objs():
    """-> {unit_stem: obj_path}"""
    out = {}
    for root, _dirs, fs in os.walk(OBJ):
        for f in fs:
            if f.endswith(".obj"):
                out.setdefault(os.path.splitext(f)[0], os.path.join(root, f))
    return out


# -------------------------------------------------------------- source census

LIT = re.compile(r"(?<![\w.])(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fFlL]?(?![\w.])")


def strip_comments_strings(t):
    """Remove // and /* */ comments and "..." / '...' literals."""
    out = []
    i = 0
    n = len(t)
    while i < n:
        c = t[i]
        if c == "/" and i + 1 < n and t[i + 1] == "/":
            j = t.find("\n", i)
            i = n if j < 0 else j
        elif c == "/" and i + 1 < n and t[i + 1] == "*":
            j = t.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif c in "\"'":
            q = c
            i += 1
            while i < n and t[i] != q:
                i += 2 if t[i] == "\\" else 1
            i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def source_census(paths):
    res = collections.Counter()
    sites = []
    for p in paths:
        try:
            raw = open(p, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        t = strip_comments_strings(raw)
        for m in LIT.finditer(t):
            s = m.group(0)
            if "." not in s and "e" not in s.lower() and "E" not in s:
                continue
            if s[-1] in "fF":
                res["suffixed_f"] += 1
            elif s[-1] in "lL":
                res["suffixed_l"] += 1
            else:
                res["unsuffixed"] += 1
                sites.append((p, t[:m.start()].count("\n") + 1, s))
    return res, sites


# ------------------------------------------------------------------- commands

def cmd_selftest():
    """Prove each reader CAN fire on known-nonempty input before trusting a zero."""
    ok = True
    s = os.path.join(ASM, "PracticePanel.s")
    fns = parse_asm(s)
    tot = sum(sum(c.values()) for c in fns.values())
    nfn = len(fns)
    print(f"[retail] PracticePanel.s: {nfn} fn blocks, {tot} FP insns")
    if tot == 0 or nfn == 0:
        print("  !! retail reader VACUOUS")
        ok = False

    objs = find_objs()
    p = objs.get("PracticePanel")
    c, w = coff_text_counts(p)
    print(f"[ours]   PracticePanel.obj: {w} .text words, {sum(c.values())} FP insns  {dict(c)}")
    if sum(c.values()) == 0:
        print("  !! obj reader VACUOUS")
        ok = False

    # negative control: a made-up opcode stream must decode to nothing
    fake = struct.pack(">I", 0x60000000) * 16  # 16x nop
    n = sum(1 for j in range(0, len(fake), 4)
            if decode(struct.unpack_from(">I", fake, j)[0]))
    print(f"[null]   16 nops decode to {n} FP insns (expect 0)")
    if n != 0:
        ok = False

    # positive control on the decoder itself
    known = {0xC0010058: "lfs", 0xC8010058: "lfd", 0xEC00082A: "fadds",
             0xFC00001A: None, 0xFC000018: "frsp", 0xEC0000F2: "fmuls",
             0xFC0000F2: "fmul"}
    for w_, want in known.items():
        got = decode(w_)
        flag = "ok" if got == want else "MISMATCH"
        if got != want and want is not None:
            ok = False
        print(f"[decode] {w_:08X} -> {got!r} (want {want!r}) {flag}")

    print("SELFTEST", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def cmd_units():
    objs = find_objs()
    rows = []
    for stem, opath in sorted(objs.items()):
        apath = os.path.join(ASM, stem + ".s")
        if not os.path.exists(apath):
            continue
        rfns = parse_asm(apath)
        rc = collections.Counter()
        for c in rfns.values():
            rc.update(c)
        oc, _w = coff_text_counts(opath)
        r_dbl = sum(rc[m] for m in DOUBLE_OPS)
        o_dbl = sum(oc[m] for m in DOUBLE_OPS)
        r_frsp, o_frsp = rc["frsp"], oc["frsp"]
        r_lfd, o_lfd = rc["lfd"], oc["lfd"]
        if o_frsp > r_frsp or o_lfd > r_lfd:
            rows.append((stem, r_frsp, o_frsp, r_lfd, o_lfd, r_dbl, o_dbl))
    rows.sort(key=lambda r: (r[2] - r[1]) + (r[4] - r[3]), reverse=True)
    print(f"{'unit':40s} {'frsp R/O':>10s} {'lfd R/O':>10s} {'dblops R/O':>12s}")
    for stem, rf, of_, rl, ol, rd, od in rows:
        print(f"{stem:40s} {rf:4d}/{of_:<5d} {rl:4d}/{ol:<5d} {rd:5d}/{od:<6d}")
    print(f"\n{len(rows)} units where OUR build has more frsp or lfd than retail")


def load_map():
    import json
    with open(os.path.join(ROOT, "scripts/target_symbol_map.json")) as fh:
        d = json.load(fh)
    return {k.lower(): v for k, v in d.items() if isinstance(v, str)}


def joined_rows():
    """-> list of (unit, symbol, retail Counter, ours Counter) for PAIRED fns."""
    smap = load_map()
    objs = find_objs()
    rows = []
    for stem, opath in sorted(objs.items()):
        apath = os.path.join(ASM, stem + ".s")
        if not os.path.exists(apath):
            continue
        rfns = parse_asm(apath)
        if not rfns:
            continue
        ours = coff_symbol_counts(opath)
        if not ours:
            continue
        for fnsym, rc in rfns.items():
            m = re.match(r"fn_([0-9A-Fa-f]{8})$", fnsym)
            if not m:
                continue
            name = smap.get("0x" + m.group(1).lower())
            if not name:
                continue
            oc = ours.get(name)
            if oc is None:
                continue
            rows.append((stem, name, rc, oc))
    return rows


def cmd_pairs():
    rows = joined_rows()
    dbl = lambda c: sum(c[m] for m in DOUBLE_OPS)
    sgl = lambda c: sum(c[m] for m in SINGLE_OPS)
    paired = len(rows)
    fp_rows = [r for r in rows if dbl(r[2]) + sgl(r[2]) + dbl(r[3]) + sgl(r[3]) > 0]
    # A candidate: OUR function has frsp or lfd that retail's paired fn lacks.
    cands = []
    agree = 0
    for unit, name, rc, oc in fp_rows:
        d_frsp = oc["frsp"] - rc["frsp"]
        d_lfd = oc["lfd"] - rc["lfd"]
        if d_frsp > 0 or d_lfd > 0:
            cands.append((d_frsp + d_lfd, unit, name, d_frsp, d_lfd,
                          rc["lfs"], oc["lfs"], rc["frsp"], oc["frsp"],
                          rc["lfd"], oc["lfd"]))
        else:
            agree += 1
    cands.sort(reverse=True)
    print(f"paired functions            : {paired}")
    print(f"  ... with any FP insn      : {len(fp_rows)}   <-- CHECKABLE population")
    print(f"  retail AGREES with us     : {agree}")
    print(f"  ours has extra frsp/lfd   : {len(cands)}   <-- candidates")
    print()
    print(f"{'d':>4} {'unit':28s} {'lfs R/O':>10s} {'frsp R/O':>10s} {'lfd R/O':>10s}  symbol")
    for d, unit, name, df, dl, rlfs, olfs, rfr, ofr, rlfd, olfd in cands[:80]:
        print(f"{d:4d} {unit:28.28s} {rlfs:4d}/{olfs:<5d} {rfr:4d}/{ofr:<5d} "
              f"{rlfd:4d}/{olfd:<5d}  {name}")
    return 0


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "selftest"
    if cmd == "pairs":
        return cmd_pairs()
    if cmd == "selftest":
        return cmd_selftest()
    if cmd == "units":
        return cmd_units()
    if cmd == "source":
        paths = []
        for root, _d, fs in os.walk(os.path.join(ROOT, "src")):
            for f in fs:
                if f.endswith((".cpp", ".h", ".hpp", ".c")):
                    paths.append(os.path.join(root, f))
        res, sites = source_census(paths)
        print(res)
        print(f"{len(sites)} unsuffixed literal sites")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
