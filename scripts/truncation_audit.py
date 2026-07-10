#!/usr/bin/env python3
"""
truncation_audit.py — find NAMED near-miss target functions truncated by a stale
symbols.txt boundary (the "swallowed tail" artifact), binary-wide.

Round-3 generalization of the wave-9 sb1-boundary fixes (commit 72f94e5). Unlike
symbols_hygiene.py (which trusts .pdata for boundaries — incomplete on X360,
where leaf functions with no unwind info get NO .pdata entry), this uses the
authoritative pair of oracles:

  * our compiled .obj — one COMDAT symbol per real C++ function, so the true
    function extent = distance to the next symbol in the same section.
  * the retail .text  — the last instruction of the *target* carve: a real
    function always ends in a hard terminator (blr/bctr/b/bclr). If the target's
    last instruction is NOT a terminator, symbols.txt cut the function short and
    it falls through into the next (swallowed) symbol.

A candidate is a CLEAN truncation (safe to fix, will turn green if the body
matches) iff the target is an exact matching *prefix* of our compiled function:
objdiff instruction_summary has diff_arg==diff_op==replace==delete==0, insert>0,
equal*4 == target_size, insert*4 == (base_size - target_size). Everything else is
body divergence that merely also has a bad boundary — do NOT auto-fix those (no
match% gain, and extending a divergent target can regress its partial match).

Fix for a clean candidate: resize the head fn_ in symbols.txt to base_ext and
delete any fn_/lbl_ (NOT except_data_) strictly inside the new extent. Guards:
never delete a fragment that is currently green or named in target_symbol_map
(A/B set-diff still required — deletion is not net-neutral).

Usage:
  python3 scripts/truncation_audit.py                 # audit, print clean+dirty
  python3 scripts/truncation_audit.py --classify      # also run objdiff (slow)
  python3 scripts/truncation_audit.py --json OUT
"""
import struct, os, glob, json, re, bisect, sys, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BAND = os.path.join(ROOT, "orig/45410914/band.exe")
SYMS = os.path.join(ROOT, "config/45410914/symbols.txt")
SPLITS = os.path.join(ROOT, "config/45410914/splits.txt")
TMAP = os.path.join(ROOT, "scripts/target_symbol_map.json")
REPORT = os.path.join(ROOT, "build/45410914/report.json")
OBJDIR = os.path.join(ROOT, "build/45410914/obj")
CLI = os.path.join(ROOT, "bin/objdiff-cli")


def obj_func_extents(path):
    """mangled_name -> extent (distance to next distinct symbol value in section)."""
    d = open(path, "rb").read()
    _, nsec, _, symptr, nsym, optsz, _ = struct.unpack("<HHIIIHH", d[:20])
    secoff = 20 + optsz
    secrawsz = [struct.unpack("<I", d[secoff + i * 40 + 16:secoff + i * 40 + 20])[0]
                for i in range(nsec)]
    strtab = symptr + nsym * 18

    def name(rec):
        if rec[:4] == b"\x00\x00\x00\x00":
            off = struct.unpack("<I", rec[4:8])[0]
            e = d.index(b"\x00", strtab + off)
            return d[strtab + off:e].decode("latin1")
        return rec[:8].rstrip(b"\x00").decode("latin1")

    bysec = {}
    i = 0
    while i < nsym:
        rec = d[symptr + i * 18:symptr + i * 18 + 18]
        val, secnum, _, _, naux = struct.unpack("<IhHBB", rec[8:18])
        if secnum > 0:
            bysec.setdefault(secnum, []).append((val, name(rec)))
        i += 1 + naux
    extent = {}
    for secnum, lst in bysec.items():
        lst.sort()
        secsz = secrawsz[secnum - 1]
        for j, (val, nm) in enumerate(lst):
            nxt = secsz
            for k in range(j + 1, len(lst)):
                if lst[k][0] > val:
                    nxt = lst[k][0]
                    break
            extent.setdefault(nm, nxt - val)
    return extent


def text_reader(path):
    d = open(path, "rb").read()
    peoff = struct.unpack("<I", d[0x3C:0x40])[0]
    coff = peoff + 4
    _, nsec, _, _, _, optsz, _ = struct.unpack("<HHIIIHH", d[coff:coff + 20])
    opt = coff + 20
    sechdr = opt + optsz
    for i in range(nsec):
        o = sechdr + i * 40
        nm = d[o:o + 8].rstrip(b"\x00").decode("latin1")
        if nm == ".text":
            imagebase = struct.unpack("<I", d[opt + 28:opt + 32])[0]
            va = imagebase + struct.unpack("<I", d[o + 12:o + 16])[0]
            raw = struct.unpack("<I", d[o + 20:o + 24])[0]
            return lambda a: struct.unpack(">I", d[raw + (a - va):raw + (a - va) + 4])[0]
    raise RuntimeError("no .text")


def is_terminator(w):
    if w == 0x4E800020: return "blr"
    if w == 0x4E800420: return "bctr"
    if (w >> 26) == 18: return "b" if not (w & 1) else "bl"
    if (w >> 26) == 19 and ((w >> 1) & 0x3FF) == 16: return "bclr"
    return None


def classify_clean(unit_stem, name):
    """Return True if target is an exact matching prefix of base (clean truncation)."""
    tgt = os.path.join(OBJDIR, unit_stem + ".obj")
    bases = glob.glob(os.path.join(ROOT, "build/45410914/src/**", unit_stem + ".obj"),
                      recursive=True)
    if not (os.path.exists(tgt) and bases):
        return None
    tmp = "/tmp/_trunc_audit_diff.json"
    r = subprocess.run([CLI, "diff", "-1", tgt, "-2", bases[0], "--summary",
                        "--analyze", "-f", "json", "-o", tmp, name],
                       capture_output=True, text=True)
    try:
        d = json.load(open(tmp))
    except Exception:
        return None
    s = d["instruction_summary"]
    ts, bs = d["target_size"], d["base_size"]
    return (s["diff_arg"] == 0 and s["diff_op"] == 0 and s["replace"] == 0
            and s["delete"] == 0 and s["insert"] > 0
            and s["equal"] * 4 == ts and s["insert"] * 4 == (bs - ts))


def main():
    name2abs = {}
    for k, v in json.load(open(TMAP)).items():
        if k[:2] in ("0x", "0X"):
            name2abs.setdefault(v, int(k, 16))

    unit_start = {}
    cur = None
    for line in open(SPLITS):
        s = line.strip()
        m = re.match(r"^([\w./\-]+\.\w+):$", s)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r"\.text\s+start:0x([0-9A-Fa-f]+)", s)
        if m and cur:
            unit_start.setdefault(os.path.splitext(os.path.basename(cur))[0],
                                  int(m.group(1), 16))

    sym_addrs = []
    for line in open(SYMS):
        m = re.match(r"^(\S+)\s*=\s*\.text:0x([0-9A-Fa-f]+);", line)
        if m and (m.group(1).startswith(("fn_", "lbl_", "except"))):
            sym_addrs.append(int(m.group(2), 16))
    sym_addrs = sorted(set(sym_addrs))

    word = text_reader(BAND)
    rep = json.load(open(REPORT))
    objs = {}
    for p in glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True):
        objs.setdefault(os.path.splitext(os.path.basename(p))[0], p)

    cache = {}
    cands = []
    for u in rep["units"]:
        stem = u["name"].split("/")[-1]
        p = objs.get(stem)
        if not p:
            continue
        if p not in cache:
            try:
                cache[p] = obj_func_extents(p)
            except Exception:
                cache[p] = {}
        ext = cache[p]
        for f in u.get("functions", []):
            if f["match_percent_normalized"] >= 100.0:
                continue
            nm = f["name"]
            if nm.startswith(("fn_", "lbl_")):
                continue
            be = ext.get(nm)
            a = name2abs.get(nm)
            if be is None or a is None:
                continue
            tsz = int(f["size"])
            if be <= tsz:
                continue
            last = word(a + tsz - 4)
            if is_terminator(last) in ("blr", "bctr", "b", "bclr"):
                continue  # target ends cleanly -> our-longer is body divergence, not truncation
            lo = bisect.bisect_right(sym_addrs, a)
            hi = bisect.bisect_left(sym_addrs, a + be)
            inside = sym_addrs[lo:hi]
            if not inside:
                continue
            cands.append({"unit": stem, "name": nm,
                          "mp": round(f["match_percent_normalized"], 2),
                          "abs": hex(a), "tgt": hex(tsz), "base_ext": hex(be),
                          "delta": be - tsz})

    do_classify = "--classify" in sys.argv
    if do_classify:
        for c in cands:
            c["clean"] = classify_clean(c["unit"], c["name"])
    cands.sort(key=lambda c: -c["mp"])

    print(f"truncation-shaped named near-misses (base_ext>target, "
          f"target ends w/o terminator, swallowed symbol present): {len(cands)}")
    if do_classify:
        clean = [c for c in cands if c.get("clean")]
        print(f"  CLEAN (exact prefix, auto-fixable): {len(clean)}")
        print(f"  body-divergent (do not auto-fix)  : "
              f"{sum(1 for c in cands if c.get('clean') is False)}")
    for c in cands:
        tag = ("CLEAN" if c.get("clean") else
               ("body " if c.get("clean") is False else "  ?  "))
        print(f"  [{tag}] {c['mp']:6.2f}% {c['tgt']}->{c['base_ext']} "
              f"{c['name'][:48]} [{c['unit']}]")

    if "--json" in sys.argv:
        out = sys.argv[sys.argv.index("--json") + 1]
        json.dump(cands, open(out, "w"), indent=1)
        print("wrote", out)


if __name__ == "__main__":
    main()
