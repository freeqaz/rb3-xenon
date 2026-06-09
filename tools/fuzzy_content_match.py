#!/usr/bin/env python3
"""Similarity-based cross-binary function id (DC3->RB3): recover matches the
EXACT masked-hash matcher (tools/dc3_content_match.py) drops.

Why a *fuzzy* sibling to dc3_content_match.py?
---------------------------------------------
dc3_content_match.py only emits a DC3 name<->RB3 addr pair when the reloc-masked
SHA1 is identical AND globally unique on BOTH sides. That uniqueness gate throws
away a real, recoverable class of matches:

  * ICF / structurally-identical bodies. Scalar deleting dtors (??_G...@@UAAPAXI@Z),
    ?ClassName@ accessors, template helpers (??$_Copy_Construct, ?insert@?$list)
    are byte-identical across many classes -> their masked hash is shared by N
    functions, so the 1:1-unique filter rejects ALL of them. Empirically 58/59
    of the >=99%-similar non-exact pairs have an IDENTICAL exact-masked hash to
    their DC3 twin; they were rejected ONLY for non-uniqueness.

This tool SCOPES the comparison per pinned unit (RB3 span <-> the unit's DC3
obj), which disambiguates by LOCATION instead of global uniqueness, recovering
those bodies. It then surfaces high-similarity pairs NOT already in the exact
match set or target_symbol_map.

Metric
------
word_eq (recommended): same-length fraction of equal big-endian 4-byte words
after zeroing the UNION of both sides' COFF reloc word-offsets. Closest analogue
to objdiff's normalized equality; length-mismatch -> skip (can never be byte-100).
  reg_eq : additionally masks PowerPC register operand fields (regalloc
           tolerance). EMPIRICALLY ADDS ~NOTHING here (640 vs 639 candidates) --
           these pairs are whole-word-similar, not regalloc-divergent. Kept for
           experimentation.
  ngram  : opcode-only 4-gram Jaccard, length-tolerant. Looser; its only unique
           contribution is length-mismatched pairs, which CANNOT reach byte-100,
           so it adds no DIRECT wins. Use only for structural/permuter triage.

VALIDATED CONVERSION (2026-06-03, clean A/B in a worktree, baseline 3554):
  91 word_eq candidates injected -> +52 matched_functions, ZERO regressions.
  Per-band objdiff-100% conversion (disjoint bands, valid units):
    sim==100% (n=58): 48 -> 83%      sim 95-99% (n=7): 3 -> 43%
    sim 90-95% (n=23): 15 -> 65%     overall 66/88 addrs = 75% (=> +52 unique,
    ICF collapse). The 10 non-converting that landed 80-99% are permuter targets.
  RECOMMENDATION: wire the >=99% band (83% conversion, all genuinely byte-equal
  to DC3). The 90-99% band is a coin-flip -- triage, don't auto-commit.

GOTCHA (load-bearing): sim==100% vs DC3 does NOT guarantee objdiff-100%. objdiff
compares target<->OUR compiled base, and DC3 is only a proxy for it. ~17% of
sim=1.0 pairs landed <80% because OUR ported source for that unit emits the
function differently (or not at all) -- the function is byte-identical to DC3 but
our build diverges from DC3 there. So conversion is bounded by how faithfully our
source tracks DC3, not by similarity.

Usage
-----
  tools/fuzzy_content_match.py                       # default curated units
  tools/fuzzy_content_match.py --units Mesh,Crowd    # specific units
  tools/fuzzy_content_match.py --all-engine          # every pinned engine unit
  tools/fuzzy_content_match.py --metric word_eq --min-sim 0.99 --out cand.json
  tools/fuzzy_content_match.py ... --emit-tsm new_names.json  # ready to merge
"""
import argparse, glob, hashlib, json, os, re, struct, sys, bisect
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dc3_obj_source import DC3_OBJ_DIR as _DC3_CANON

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RB3_OBJ_GLOB = os.path.join(ROOT, "build", "45410914", "obj", "auto_03_*_text.obj")
# Canonical retail-DC3 TARGET tree (shared via dc3_obj_source).
DC3_DIR = _DC3_CANON
TSM = os.path.join(ROOT, "scripts", "target_symbol_map.json")
SPLITS = os.path.join(ROOT, "config", "45410914", "splits.txt")
OBJECTS = os.path.join(ROOT, "config", "45410914", "objects.json")
EXACT = os.path.join(ROOT, "dc3_content_match.json")

PSEUDO = ("fn_", "sub_", "FUN_", "__unwind", "__catch", "__tls", "$")

DEFAULT_UNITS = ["LightPreset","TexBlender","Utl","BandCharacter","BandDirector",
                 "Crowd","CameraShot","Part","Mesh","Spotlight","MeshAnim",
                 "EventTrigger","BandCamShot","Rnd","CharClip","HamCamTransform",
                 "MidiInstrument","SpotlightDrawer","Group","Instance","File"]


def read_coff_functions(path):
    """Yield {name, code(bytes), reloc(set of word-offsets), size} per code COMDAT."""
    data = open(path, "rb").read()
    if len(data) < 20:
        return
    machine, nsec, ts, symptr, nsym, opt, chars = struct.unpack_from("<HHIIIHH", data, 0)
    if machine != 0x01F2:
        return
    strtab_off = symptr + nsym * 18
    def read_str_at(off):
        end = data.index(b"\x00", strtab_off + off)
        return data[strtab_off + off:end].decode("latin1")
    def sym_name(raw):
        if raw[0:4] == b"\x00\x00\x00\x00":
            return read_str_at(struct.unpack_from("<I", raw, 4)[0])
        return raw.rstrip(b"\x00").decode("latin1")
    secs = []
    for i in range(nsec):
        o = 20 + i * 40
        name, vsz, vaddr, rawsz, rawptr, relptr, lnptr, nrel, nln, sc = \
            struct.unpack_from("<8sIIIIIIHHI", data, o)
        if name[0:1] == b"/":
            nm = read_str_at(int(name.rstrip(b"\x00")[1:]))
        else:
            nm = name.rstrip(b"\x00").decode("latin1")
        secs.append((nm, rawsz, rawptr, relptr, nrel))
    sec_sym = {}
    i = 0
    while i < nsym:
        o = symptr + i * 18
        nm_raw = data[o:o + 8]
        val, sec, typ, cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
        if sec > 0 and cls in (2, 6) and val == 0 and sec not in sec_sym:
            sec_sym[sec] = sym_name(nm_raw)
        i += 1 + naux
    for idx, (nm, rawsz, rawptr, relptr, nrel) in enumerate(secs, start=1):
        if not nm.startswith(".text") or rawsz == 0 or idx not in sec_sym:
            continue
        code = bytes(data[rawptr:rawptr + rawsz])
        reloff = set()
        if relptr and nrel:
            for r in range(nrel):
                ro = relptr + r * 10
                if ro + 10 > len(data):
                    break
                rva, symidx, rtype = struct.unpack_from("<IIH", data, ro)
                if rva % 4 == 0 and rva + 4 <= rawsz:
                    reloff.add(rva)
        yield {"name": sec_sym[idx], "code": code, "reloc": reloff, "size": rawsz}


def words(code):
    return [code[i*4:i*4+4] for i in range(len(code)//4)]


def masked_words(code, reloc, mask_regs=False):
    out = []
    for i, w in enumerate(words(code)):
        if i*4 in reloc:
            out.append(b"\x00\x00\x00\x00"); continue
        if mask_regs:
            v = struct.unpack(">I", w)[0]
            v &= ~(0x1F << 21) & ~(0x1F << 16) & ~(0x1F << 11)
            out.append(struct.pack(">I", v))
        else:
            out.append(w)
    return out


def word_eq_frac(ac, ar, bc, br, mask_regs=False):
    if len(ac) != len(bc):
        return None
    aw = masked_words(ac, ar | br, mask_regs)
    bw = masked_words(bc, ar | br, mask_regs)
    if not aw:
        return None
    return sum(1 for x, y in zip(aw, bw) if x == y) / len(aw)


def opcodes(code):
    out = []
    for w in words(code):
        v = struct.unpack(">I", w)[0]; p = v >> 26
        out.append((p << 10) | ((v >> 1) & 0x3FF) if p in (19,31,59,63) else (p << 10))
    return out


def ngram_jaccard(ac, bc, n=4):
    oa, ob = opcodes(ac), opcodes(bc)
    if len(oa) < n or len(ob) < n:
        sa, sb = set(oa), set(ob)
        return len(sa & sb) / len(sa | sb) if sa and sb else 0.0
    ga = {tuple(oa[i:i+n]) for i in range(len(oa)-n+1)}
    gb = {tuple(ob[i:i+n]) for i in range(len(ob)-n+1)}
    return len(ga & gb) / len(ga | gb)


def parse_splits(path):
    UNIT = re.compile(r'^(\S+\.(?:cpp|c|cc)):\s*$')
    TXT = re.compile(r'^(\s*)\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')
    units = {}; cur = None
    for line in open(path):
        m = UNIT.match(line)
        if m:
            cur = m.group(1); continue
        if cur:
            t = TXT.match(line)
            if t and cur not in units:
                units[cur] = (int(t.group(2), 16), int(t.group(3), 16))
    return units


def find_dc3_obj(base):
    hits = glob.glob(os.path.join(DC3_DIR, "**", base + ".obj"), recursive=True)
    if not hits and base.startswith("Band"):  # RB3 Band* == DC3 Ham*
        hits = glob.glob(os.path.join(DC3_DIR, "**", "Ham" + base[4:] + ".obj"),
                         recursive=True)
    hits.sort(key=lambda p: (0 if "/system/" in p else 1, len(p)))
    return hits[0] if hits else None


def all_engine_units(splits, dc3_dir):
    o = json.load(open(OBJECTS))
    eng = set()
    for grp in ("engine", "hamobj", "main"):
        for fn in o[grp].get("objects", {}):
            eng.add(os.path.splitext(fn.split("/")[-1])[0])
    pinned = {os.path.splitext(k.split("/")[-1])[0] for k in splits}
    return sorted(b for b in eng if b in pinned and find_dc3_obj(b))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--units", default="", help="comma list of unit basenames")
    ap.add_argument("--all-engine", action="store_true",
                    help="scan every pinned engine/hamobj unit with a DC3 obj")
    ap.add_argument("--metric", default="word_eq", choices=["word_eq","reg_eq","ngram"])
    ap.add_argument("--min-sim", type=float, default=0.90)
    ap.add_argument("--min-size", type=int, default=16)
    ap.add_argument("--splits", default=SPLITS)
    ap.add_argument("--tsm", default=TSM)
    ap.add_argument("--dc3-dir", default=DC3_DIR)
    ap.add_argument("--out", default="/dev/stdout")
    ap.add_argument("--emit-tsm", default="",
                    help="also write a {addr:name} JSON ready to MERGE into "
                         "target_symbol_map.json (skips addrs already present)")
    args = ap.parse_args()
    if args.dc3_dir != DC3_DIR:
        globals()["DC3_DIR"] = args.dc3_dir

    splits = parse_splits(args.splits)
    if args.all_engine:
        units = all_engine_units(splits, DC3_DIR)
        print(f"--all-engine: {len(units)} pinned units with a DC3 obj", file=sys.stderr)
    else:
        units = [u.strip() for u in args.units.split(",") if u.strip()] or DEFAULT_UNITS

    tsm = json.load(open(args.tsm))
    rev_tsm = {v: int(k, 16) for k, v in tsm.items() if k.lower().startswith("0x")}
    tsm_addr = set(rev_tsm.values())
    exact = json.load(open(EXACT))
    exact_rb3 = {int(e["rb3_addr"], 16) for e in exact}
    exact_pairs = {(int(e["rb3_addr"], 16), e["dc3_name"]) for e in exact}

    # all RB3 functions (addr -> fn)
    rb3 = {}
    for f in sorted(glob.glob(RB3_OBJ_GLOB)):
        for fn in read_coff_functions(f):
            m = re.match(r"fn_([0-9A-Fa-f]+)$", fn["name"])
            a = int(m.group(1), 16) if m else rev_tsm.get(fn["name"])
            if a is None:
                continue
            if a not in rb3 or fn["size"] > rb3[a]["size"]:
                fn["addr"] = a; rb3[a] = fn
    addrs = sorted(rb3)
    print(f"RB3 fns: {len(rb3)}", file=sys.stderr)

    cands, summary = [], []
    for base in units:
        cpp = next((k for k in splits
                    if os.path.splitext(k.split("/")[-1])[0].lower() == base.lower()), None)
        if not cpp:
            continue
        lo, hi = splits[cpp]
        dc3obj = find_dc3_obj(base)
        if not dc3obj:
            continue
        dc3fns = [fn for fn in read_coff_functions(dc3obj)
                  if fn["size"] >= args.min_size and not fn["name"].startswith(PSEUDO)]
        span = [a for a in addrs[bisect.bisect_left(addrs, lo):bisect.bisect_left(addrs, hi)]
                if rb3[a]["size"] >= args.min_size]
        n9 = n95 = n99 = 0
        for a in span:
            if a in exact_rb3 or a in tsm_addr:
                continue
            rf = rb3[a]; best = None
            for df in dc3fns:
                if (a, df["name"]) in exact_pairs:
                    continue
                if args.metric == "ngram":
                    s = ngram_jaccard(rf["code"], df["code"])
                else:
                    s = word_eq_frac(rf["code"], rf["reloc"], df["code"], df["reloc"],
                                     mask_regs=(args.metric == "reg_eq"))
                if s is not None and (best is None or s > best[0]):
                    best = (s, df["name"], df["size"])
            if best and best[0] >= args.min_sim:
                cands.append({"rb3_addr": "0x%08X" % a, "dc3_name": best[1],
                              "unit": base, "sim": round(best[0], 4),
                              "metric": args.metric, "rb3_size": rf["size"],
                              "dc3_size": best[2]})
                n9 += 1; n95 += best[0] >= 0.95; n99 += best[0] >= 0.99
        if n9:
            summary.append((base, n9, n95, n99))

    cands.sort(key=lambda c: -c["sim"])
    json.dump(cands, open(args.out, "w"), indent=1)
    summary.sort(key=lambda s: -s[1])
    print(f"\n{'unit':24s} {'>=90':>5s} {'>=95':>5s} {'>=99':>5s}", file=sys.stderr)
    for u, a, b, c in summary[:40]:
        print(f"{u:24s} {a:5d} {b:5d} {c:5d}", file=sys.stderr)
    t9 = sum(s[1] for s in summary); t95 = sum(s[2] for s in summary); t99 = sum(s[3] for s in summary)
    print(f"{'TOTAL':24s} {t9:5d} {t95:5d} {t99:5d}", file=sys.stderr)
    print(f"wrote {args.out} ({len(cands)} candidates)", file=sys.stderr)

    if args.emit_tsm:
        new = {c["rb3_addr"]: c["dc3_name"] for c in cands
               if c["rb3_addr"].lower() not in
               {k.lower() for k in tsm if k.lower().startswith("0x")}}
        json.dump(new, open(args.emit_tsm, "w"), indent=1)
        print(f"wrote {args.emit_tsm} ({len(new)} new names to merge)", file=sys.stderr)


if __name__ == "__main__":
    main()
