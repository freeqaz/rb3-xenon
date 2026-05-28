#!/usr/bin/env python3
"""
fingerprint_match.py — identify anonymous RB3 functions by cross-referencing the
shared Milo engine against dc3-decomp's (near-fully-named) symbol set.

Why this exists: the RB3 retail XEX is a stripped LTO build. dtk gave us ~66k
functions, 99.6% anonymous `fn_8XXXXXXX`, with no source-path strings and no
RTTI. dc3-decomp is the *same engine*, ~98% named. The optimization- and
binary-invariant signal that survives is **referenced string-literal content**:
a shared engine function references the same string constants in both games.

Stage 1 (this file): extract, for every RB3 function, the set of string literals
and numeric constants it references, plus its callee/size shape. Output a JSON
index and a human-readable report ranked by how distinctive each function's
strings are. Those strings are then grep-able against dc3/rb3-Wii source to
identify the function (see `identify` subcommand).

Usage:
  fingerprint_match.py extract [--asm-dir DIR] [--out FILE] [--lo ADDR --hi ADDR]
  fingerprint_match.py report  [--index FILE] [--min-strings N] [--limit N]
  fingerprint_match.py identify <string-or-fn> [--index FILE] [--src DIR...]
"""
import argparse
import json
import os
import re
import struct
import sys

DEFAULT_ASM = os.path.join(os.path.dirname(__file__), "..", "build", "45410914", "asm")
SYMS = os.path.join(os.path.dirname(__file__), "..", "config", "45410914", "symbols.txt")
# rb3 retail PE (unxex'd) for RTTI scanning; dc3 leaked .map + release PE for
# the named vtable oracle (same defaults as the exploratory POCs).
DEFAULT_RB3_EXE = os.path.join(os.path.dirname(__file__), "..", "orig", "45410914", "band.exe")
DEFAULT_DC3_MAP = os.path.join(os.path.dirname(__file__), "..", "..", "dc3-decomp", "orig", "373307D9", "ham_xbox_r.map")
DEFAULT_DC3_EXE = os.path.join(os.path.dirname(__file__), "..", "..", "dc3-decomp", "orig", "373307D9", "ham_xbox_r.exe")
DEFAULT_DC3_OBJECTS = os.path.join(os.path.dirname(__file__), "..", "..", "dc3-decomp", "config", "373307D9", "objects.json")

# /* 82272EB8 0026E6B8  7D 88 02 A6 */\tmflr r12
INSN_RE = re.compile(r"^/\*\s*([0-9A-Fa-f]{8})\s+[0-9A-Fa-f]{8}\s+[0-9A-Fa-f ]+\*/\t(.*)$")
FN_REF_RE = re.compile(r"\bfn_([0-9A-Fa-f]{8})\b")
LBL_REF_RE = re.compile(r"\blbl_([0-9A-Fa-f]{8})\b")
IMM_RE = re.compile(r"\b0x([0-9A-Fa-f]+)\b")
SYM_FN_RE = re.compile(r"^(\S+) = \.text:0x([0-9A-Fa-f]+);.*type:function(?:.*size:0x([0-9A-Fa-f]+))?")
# rdata: "# .rdata:0x738 | 0x82000B38 | size: 0xA"
RDATA_HDR_RE = re.compile(r"^#\s+\.\w+:0x[0-9A-Fa-f]+\s+\|\s+0x([0-9A-Fa-f]+)\s+\|\s+size:")
STRING_RE = re.compile(r'^\s*\.string\s+"(.*)"\s*$')

# Symbols*.cpp files are the engine's interned-symbol declaration tables
# (`Symbol foo("foo");` for every global symbol). Every consumer in the engine
# references those literals, so file-grep collapses onto them as a systematic
# false positive. Exclude from source-attribution.
SYMBOL_TABLE_RE = re.compile(r"/system/utl/Symbols\d*\.cpp$")

# Cross-game class-name substitutions applied to bindiff-derived dc3 names
# before they go into target_symbol_map.json. dc3 has e.g. HamDirector;
# RB3's equivalent class is BandDirector with the same engine API, base
# classes, and (mostly) the same methods. When BinDiff matches an RB3
# function fn_X to dc3's `?Foo@HamDirector@@...`, the correct rb3-side
# mangled name is `?Foo@BandDirector@@...` — substitute before emitting.
#
# Built from `comm -12 <hamobj/Ham*.h-suffixes> <bandobj/Band*.h-suffixes>`.
# Only includes pairs verified to be the same engine API and class shape;
# Ham* classes without a Band* twin (HamAudio, HamCamTransform, HamDriver,
# HamGameData, HamMaster, etc.) are intentionally absent.
HAM_TO_BAND_CLASSES = {
    # bandobj/ <-> hamobj/ engine parallels (same base classes, same engine API)
    "HamCamShot": "BandCamShot",
    "HamCharacter": "BandCharacter",
    "HamDirector": "BandDirector",
    "HamIKEffector": "BandIKEffector",
    "HamLabel": "BandLabel",
    "HamList": "BandList",
    "HamSong": "BandSong",
    "HamWardrobe": "BandWardrobe",
    # band3/meta_band/ <-> dc3's analogous game-data classes (probable parallels)
    "HamProfile": "BandProfile",
    "HamSongMgr": "BandSongMgr",
    "HamUser": "BandUser",
    "HamUserMgr": "BandUserMgr",
}


def substitute_dc3_class_names(mangled_name):
    """Apply cross-game class-name substitutions to a dc3-side mangled name.

    MSVC mangled names use `@` as the name-chain terminator, so we substitute
    `HamFoo@` -> `BandFoo@` to ensure we hit the class-name slot and never a
    substring of an unrelated identifier. Returns (substituted_name,
    list_of_subs_applied) so the caller can audit.
    """
    out = mangled_name
    applied = []
    for src, dst in HAM_TO_BAND_CLASSES.items():
        needle = src + "@"
        if needle in out:
            out = out.replace(needle, dst + "@")
            applied.append(f"{src}->{dst}")
    return out, applied


def decode_escapes(s):
    # GAS .string escapes: \000 octal, \n \t \\ \"
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            if n in "01234567":
                oct_digits = s[i + 1:i + 4]
                m = re.match(r"[0-7]{1,3}", oct_digits)
                val = int(m.group(0), 8)
                out.append(chr(val))
                i += 1 + len(m.group(0))
                continue
            mp = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "0": "\0"}
            out.append(mp.get(n, n))
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def clean_string(s):
    """dtk emits float-tables and concatenated blobs as `.string`. Keep the
    leading C string (up to first NUL) only if it's a plausible printable token."""
    s = s.split("\x00", 1)[0]
    if len(s) < 3:
        return None
    printable = sum(1 for ch in s if 32 <= ord(ch) < 127)
    if printable / len(s) < 0.9:
        return None
    return s


def load_functions(syms_path):
    """Return sorted list of (addr, size, name) for .text functions."""
    funcs = []
    with open(syms_path, "r", errors="replace") as f:
        for line in f:
            m = SYM_FN_RE.match(line)
            if not m:
                continue
            name, addr_hex, size_hex = m.group(1), m.group(2), m.group(3)
            addr = int(addr_hex, 16)
            size = int(size_hex, 16) if size_hex else 0
            funcs.append((addr, size, name))
    funcs.sort()
    return funcs


def parse_rdata_strings(asm_dir):
    """Map start-address -> decoded string for every .string in rdata/data."""
    addr_to_str = {}
    for fname in os.listdir(asm_dir):
        if not (fname.endswith("_rdata.s") or fname.endswith("_data.s")):
            continue
        cur_addr = None
        with open(os.path.join(asm_dir, fname), "r", errors="replace") as f:
            for line in f:
                h = RDATA_HDR_RE.match(line)
                if h:
                    cur_addr = int(h.group(1), 16)
                    continue
                sm = STRING_RE.match(line)
                if sm and cur_addr is not None:
                    decoded = decode_escapes(sm.group(1))
                    cleaned = clean_string(decoded)
                    if cleaned:
                        addr_to_str[cur_addr] = cleaned
                    cur_addr = None  # consume; next header sets the next addr
    return addr_to_str


def fn_at(funcs, addr):
    """Binary search: index of function whose [addr,addr+size) contains addr."""
    lo, hi = 0, len(funcs)
    while lo < hi:
        mid = (lo + hi) // 2
        if funcs[mid][0] <= addr:
            lo = mid + 1
        else:
            hi = mid
    i = lo - 1
    if i < 0:
        return None
    faddr, fsize, _ = funcs[i]
    if fsize and addr >= faddr + fsize:
        return None
    return i


def extract(args):
    asm_dir = args.asm_dir
    funcs = load_functions(SYMS)
    print(f"[extract] {len(funcs)} functions from symbols.txt", file=sys.stderr)
    addr_to_str = parse_rdata_strings(asm_dir)
    print(f"[extract] {len(addr_to_str)} rdata/data strings", file=sys.stderr)

    text_files = sorted(x for x in os.listdir(asm_dir) if "_text" in x and x.endswith(".s"))
    feats = {}  # fn_addr -> dict

    def fdict(faddr):
        d = feats.get(faddr)
        if d is None:
            d = {"callees": set(), "datarefs": set(), "imms": set(), "n_insns": 0}
            feats[faddr] = d
        return d

    for tf in text_files:
        path = os.path.join(asm_dir, tf)
        print(f"[extract] scanning {tf}", file=sys.stderr)
        with open(path, "r", errors="replace") as f:
            for line in f:
                m = INSN_RE.match(line)
                if not m:
                    continue
                addr = int(m.group(1), 16)
                if args.lo and addr < args.lo:
                    continue
                if args.hi and addr >= args.hi:
                    break  # text is address-ordered; nothing more in range
                ops = m.group(2)
                fi = fn_at(funcs, addr)
                if fi is None:
                    continue
                faddr = funcs[fi][0]
                d = fdict(faddr)
                d["n_insns"] += 1
                for mm in FN_REF_RE.finditer(ops):
                    tgt = int(mm.group(1), 16)
                    if tgt != faddr:
                        d["callees"].add(tgt)
                for mm in LBL_REF_RE.finditer(ops):
                    d["datarefs"].add(int(mm.group(1), 16))
                for mm in IMM_RE.finditer(ops):
                    d["imms"].add(mm.group(1).upper())

    # resolve datarefs -> strings; finalize
    out = {}
    for faddr, d in feats.items():
        strings = sorted({addr_to_str[a] for a in d["datarefs"] if a in addr_to_str})
        out[f"{faddr:08X}"] = {
            "name": f"fn_{faddr:08X}",
            "size": next((s for a, s, _ in funcs if a == faddr), 0),
            "n_insns": d["n_insns"],
            "n_callees": len(d["callees"]),
            "callees": sorted(f"{a:08X}" for a in d["callees"]),
            "imms": sorted(d["imms"]),
            "strings": strings,
        }
    with open(args.out, "w") as f:
        json.dump(out, f, indent=0)
    nstr = sum(1 for v in out.values() if v["strings"])
    print(f"[extract] wrote {len(out)} functions ({nstr} reference >=1 string) -> {args.out}",
          file=sys.stderr)


def report(args):
    with open(args.index) as f:
        idx = json.load(f)
    rows = [v for v in idx.values() if len(v["strings"]) >= args.min_strings]
    # rank by total string length (proxy for distinctiveness)
    rows.sort(key=lambda v: sum(len(s) for s in v["strings"]), reverse=True)
    for v in rows[: args.limit]:
        print(f"{v['name']}  size={v['size']:#x} insns={v['n_insns']} callees={v['n_callees']}")
        for s in v["strings"][:12]:
            print(f"    {s!r}")


def identify(args):
    import subprocess
    needle = args.query
    if re.fullmatch(r"(fn_)?[0-9A-Fa-f]{8}", needle):
        with open(args.index) as f:
            idx = json.load(f)
        key = needle.replace("fn_", "").upper()
        v = idx.get(key)
        if not v:
            print("no such function in index")
            return
        print(json.dumps(v, indent=2))
        strings = v["strings"]
    else:
        strings = [needle]
    for s in strings:
        if len(s) < 4:
            continue
        print(f"\n=== grep src for {s!r} ===")
        for srcdir in args.src:
            try:
                r = subprocess.run(
                    ["grep", "-rIn", "--include=*.cpp", "--include=*.c", "--include=*.h", s, srcdir],
                    capture_output=True, text=True, timeout=60)
                for ln in r.stdout.splitlines()[:8]:
                    print(f"  {ln}")
            except Exception as e:
                print(f"  (grep failed: {e})")


SRC_LIT_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def build_string_to_files(srcdirs, target_strings):
    """One pass over source trees: map each target string -> set of source files
    that contain it as a quoted literal. Efficient regardless of #targets."""
    targets = set(target_strings)
    s2f = {}
    nfiles = 0
    for srcdir in srcdirs:
        for root, _dirs, files in os.walk(srcdir):
            if ".claude" in root or "/build" in root or "/doc/" in root:
                continue
            for fn in files:
                if not fn.endswith((".cpp", ".c", ".h", ".hpp")):
                    continue
                path = os.path.join(root, fn)
                if SYMBOL_TABLE_RE.search(path.replace(os.sep, "/")):
                    continue
                try:
                    with open(path, "r", errors="replace") as f:
                        content = f.read()
                except OSError:
                    continue
                nfiles += 1
                for m in SRC_LIT_RE.finditer(content):
                    lit = decode_escapes(m.group(1))
                    if lit in targets:
                        s2f.setdefault(lit, set()).add(path)
    print(f"[autoid] scanned {nfiles} source files; "
          f"{len(s2f)}/{len(targets)} target strings found in source", file=sys.stderr)
    return s2f


def autoid(args):
    with open(args.index) as f:
        idx = json.load(f)
    all_strings = set()
    for v in idx.values():
        all_strings.update(s for s in v["strings"] if len(s) >= args.min_len)
    s2f = build_string_to_files(args.src, all_strings)

    proposals = []
    for v in idx.values():
        strs = [s for s in v["strings"] if len(s) >= args.min_len]
        if len(strs) < args.min_strings:
            continue
        file_hits = {}
        for s in strs:
            for path in s2f.get(s, ()):  # files containing this string
                file_hits.setdefault(path, []).append(s)
        if not file_hits:
            continue
        best_path, best_strs = max(file_hits.items(), key=lambda kv: len(kv[1]))
        score = len(best_strs)
        proposals.append({
            "fn": v["name"], "size": v["size"], "score": score,
            "n_strings": len(strs), "src": best_path, "matched_strings": sorted(best_strs),
        })
    # rank: most corroborating strings first, then smallest function
    proposals.sort(key=lambda p: (-p["score"], p["size"]))
    with open(args.out, "w") as f:
        json.dump(proposals, f, indent=1)
    print(f"[autoid] {len(proposals)} functions mapped to a source file -> {args.out}",
          file=sys.stderr)
    for p in proposals[: args.limit]:
        src = p["src"].replace("../", "")
        print(f"{p['fn']} size={p['size']:#x} score={p['score']}/{p['n_strings']}  {src}")
        print(f"    {p['matched_strings'][:8]}")


# ---------------------------------------------------------------------------
# bindiff integration: merge_bindiff + bindiff_clusters
# (per docs/plans/bindiff-integration.md)
# ---------------------------------------------------------------------------

def _norm_path(p):
    """Normalize a src path for case-insensitive agreement comparison.
    Keeps `../<oracle>/src/...` prefix; lowercases.
    """
    if not p:
        return None
    return os.path.normpath(p).replace("\\", "/").lower()


def _agreement_eq(autoid_src, bindiff_src):
    if not autoid_src or not bindiff_src:
        return False
    a = _norm_path(autoid_src)
    b = _norm_path(bindiff_src)
    if a == b:
        return True
    # autoid often resolves engine files in rb3-Wii (../rb3/src/system/...)
    # while bindiff resolves the same file in dc3 (../dc3-decomp/src/system/...).
    # Compare by relative-to-src tail so cross-oracle hits count as agreed.
    def tail(p):
        for marker in ("/src/", "\\src\\"):
            idx = p.find(marker)
            if idx >= 0:
                return p[idx + len(marker):]
        return p
    return tail(a) == tail(b)


def merge_bindiff(args):
    """Join autoid string-attribution with bindiff cross-binary matches into
    one unified_id.json record per RB3 function.
    """
    # local import so the script still loads if dc3_map.py is missing
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from dc3_map import parse_map, load_objects, obj_to_cpp, demangle_safe

    with open(args.autoid) as f:
        autoid_list = json.load(f)
    autoid_by_fn = {p["fn"]: p for p in autoid_list}

    with open(args.bindiff) as f:
        bindiff_list = json.load(f)

    mapinfo = parse_map(args.dc3_map)
    objects_db = load_objects(args.dc3_objects)

    # Build the join: union of fn keys
    out = []
    seen_fns = set()
    n_demangle_fail = n_obj_miss = n_map_miss = n_agreed = n_disagreed = 0
    src_counts = {"both": 0, "autoid": 0, "bindiff": 0}

    # Walk bindiff entries first (they're the primary newcomer)
    for e in bindiff_list:
        rb3_fn = e["rb3_fn"]
        seen_fns.add(rb3_fn)
        dc3_name = e["dc3_name"]
        info = mapinfo.get(dc3_name)
        if info is None:
            n_map_miss += 1
            dc3_obj = None
            dc3_cpp = None
            inline_only = False
        else:
            dc3_obj = info["obj"]
            inline_only = "i" in info["tags"]
            dc3_cpp = obj_to_cpp(dc3_obj, objects_db)
            if dc3_cpp is None:
                n_obj_miss += 1

        demangled = demangle_safe(dc3_name)
        if demangled == dc3_name and dc3_name.startswith("?"):
            n_demangle_fail += 1

        autoid_hit = autoid_by_fn.get(rb3_fn)
        if autoid_hit:
            source = "both"
            src_counts["both"] += 1
            agreed = _agreement_eq(autoid_hit.get("src"), dc3_cpp)
            if agreed:
                n_agreed += 1
            else:
                n_disagreed += 1
        else:
            source = "bindiff"
            src_counts["bindiff"] += 1
            agreed = None

        rec = {
            "rb3_fn": rb3_fn,
            "rb3_addr": e["rb3_addr"],
            "size": e.get("size", 0),
            "source": source,
            "dc3_name": dc3_name,
            "dc3_name_demangled": demangled,
            "dc3_obj": dc3_obj,
            "dc3_inline_only": inline_only,
            "bindiff_src": dc3_cpp,
            "similarity": e.get("similarity"),
            "confidence": e.get("confidence"),
            "algorithm": e.get("algorithm"),
        }
        if autoid_hit:
            rec["autoid_src"] = autoid_hit.get("src")
            rec["autoid_score"] = autoid_hit.get("score")
            rec["autoid_n_strings"] = autoid_hit.get("n_strings")
            rec["agreed"] = agreed
        out.append(rec)

    # Then autoid-only entries
    for p in autoid_list:
        if p["fn"] in seen_fns:
            continue
        src_counts["autoid"] += 1
        out.append({
            "rb3_fn": p["fn"],
            "rb3_addr": p["fn"].replace("fn_", "0x"),
            "size": p.get("size", 0),
            "source": "autoid",
            "autoid_src": p.get("src"),
            "autoid_score": p.get("score"),
            "autoid_n_strings": p.get("n_strings"),
        })

    out.sort(key=lambda r: int(r["rb3_addr"], 16))

    with open(args.out, "w") as f:
        json.dump(out, f, indent=1)

    print(f"[merge_bindiff] {len(out)} unified records -> {args.out}", file=sys.stderr)
    print(f"  source breakdown: both={src_counts['both']}  "
          f"bindiff_only={src_counts['bindiff']}  autoid_only={src_counts['autoid']}",
          file=sys.stderr)
    if src_counts["both"]:
        pct = 100 * n_agreed / src_counts["both"]
        print(f"  agreement (intersection): {n_agreed}/{src_counts['both']} "
              f"({pct:.1f}%)  disagreed={n_disagreed}", file=sys.stderr)
    print(f"  map misses: {n_map_miss}  obj->cpp misses: {n_obj_miss}  "
          f"demangle failures: {n_demangle_fail}", file=sys.stderr)


# ---------------------------------------------------------------------------
# callgraph triangulation: triangulate
# (productionizes tools/exploratory/callgraph_triangulate.py — see
#  docs/decomp/callgraph-triangulation.md)
#
# Method: for a known-mapped rb3 function (rb3 fn_A -> dc3 "Foo"), the `bl`
# call sequence in rb3's .s file is positionally aligned with dc3's "Foo".
# So rb3.bl[i] == fn_B (anonymous) and dc3.bl[i] == "Bar" (named) implies
# rb3 fn_B IS dc3 "Bar". Votes accumulate across every named caller; a fn that
# gets >=2 agreeing distinct callers is "multi" (high conf), 1 caller "single".
# ---------------------------------------------------------------------------

# .s function delimiters and bl-target extraction (ported from the POC).
CG_FN_RE = re.compile(r'^\.fn\s+(\S+?),')
CG_ENDFN_RE = re.compile(r'^\.endfn\b')
# /* 82758434 00753C34  4B BA 67 1D */<tab>bl fn_822FEB50   (or bl "?Foo@@...")
CG_BL_RE = re.compile(
    r'/\*\s+([0-9A-Fa-f]{8})\s+\S+\s+\S+ \S+ \S+ \S+ \*/\s+bl\s+(\S.*)$')
CG_ADDR_RE = re.compile(r'/\*\s+([0-9A-Fa-f]{8})')


def _cg_parse_asm_file(path):
    """Parse one .s file into a list of fn dicts {name, start, bl:[(off,tgt)]}.

    Each fn spans `.fn NAME, ...` .. `.endfn`. `start` is the address of the
    first instruction (from its leading /* ADDR ... */ comment); each `bl`
    records (instruction_addr - start, target_symbol).
    """
    fns = []
    cur = None
    with open(path, errors="replace") as f:
        for line in f:
            m = CG_FN_RE.match(line)
            if m:
                if cur is not None:
                    fns.append(cur)
                cur = {"name": m.group(1).strip('"'), "start": None, "bl": []}
                continue
            if CG_ENDFN_RE.match(line):
                if cur is not None:
                    fns.append(cur)
                cur = None
                continue
            if cur is None:
                continue
            am = CG_ADDR_RE.match(line)
            if am and cur["start"] is None:
                cur["start"] = int(am.group(1), 16)
            blm = CG_BL_RE.search(line)
            if blm and cur["start"] is not None:
                ins_addr = int(blm.group(1), 16)
                tgt = blm.group(2).strip().strip('"')
                cur["bl"].append((ins_addr - cur["start"], tgt))
        if cur is not None:
            fns.append(cur)
    return fns


def _cg_load_rb3_fns(asm_dir):
    """rb3 fn start-addr -> {name, start, bl}. Parses every *.s in asm_dir.

    rb3's stripped XEX disassembly names every function `fn_<addr>` (even
    matched ones), so this captures all of them; we key by start address.
    """
    out = {}
    for s in sorted(p for p in os.listdir(asm_dir) if p.endswith(".s")):
        for fn in _cg_parse_asm_file(os.path.join(asm_dir, s)):
            if fn["start"] is not None:
                out[fn["start"]] = fn
    return out


def _cg_load_dc3_fns(dc3_asm_dir):
    """dc3 fn name -> {name, start, bl}. Recursive; first definition wins."""
    out = {}
    for root, _dirs, files in os.walk(dc3_asm_dir):
        for fn_file in sorted(files):
            if not fn_file.endswith(".s"):
                continue
            for fn in _cg_parse_asm_file(os.path.join(root, fn_file)):
                if fn["start"] is None:
                    continue
                if fn["name"] not in out:
                    out[fn["name"]] = fn
    return out


def triangulate(args):
    """Call-graph triangulation: identify anonymous rb3 fns by aligning the
    bl call sequences of known-mapped functions against their dc3 twins.

    Anchor set = unified_id.json entries that carry a dc3_name (the rb3 fn ->
    dc3 name mapping). report.json's 100%-matched functions are also folded in
    as anchors, but in practice they are a subset of unified_id (rb3's .s is
    fully anonymous, so a matched fn only helps as an anchor when its dc3 twin
    is known — which IS the unified_id mapping).

    Output: a NEW-only oracle file (default unified_id_callgraph.json) whose
    records are schema-compatible with unified_id.json (source="callgraph").
    Existing unified_id addresses are never emitted, so a union is collision-
    free. Confidence tiers:
      * multi  (>=2 distinct named callers agree, unanimous) -> confidence 0.97
      * single (exactly 1 caller, unanimous)                 -> confidence 0.80
      * majority (contested but winner has >=--majority-frac) -> confidence 0.70
    The 0.97/0.96 multi-tier confidence/similarity clears gen_target_map's
    default thresholds; single/majority deliberately fall below them so they
    stay manual-review until promoted.
    """
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from dc3_map import (parse_map, load_objects, obj_to_cpp, demangle_safe)

    # --- 1. anchor set: rb3 addr -> dc3 name ---
    with open(args.unified) as f:
        unified = json.load(f)
    rb3_to_dc3 = {}
    unified_addrs = set()
    for r in unified:
        try:
            va = int(r["rb3_addr"], 16)
        except (KeyError, ValueError):
            continue
        unified_addrs.add(va)
        if r.get("dc3_name"):
            rb3_to_dc3[va] = r["dc3_name"]
    print(f"[triangulate] anchors from unified_id: {len(rb3_to_dc3)}",
          file=sys.stderr)

    # report.json matched fns are RB3-side mangled names with no address (the
    # report stores address='0'); they cannot anchor without a dc3 twin, and
    # 391/394 are already covered by unified_id. We surface the count only.
    n_matched = 0
    if args.report and os.path.isfile(args.report):
        rep = json.load(open(args.report))
        for u in rep.get("units", []):
            for fn in u.get("functions", []):
                if fn.get("match_percent_normalized", 0) >= 100.0:
                    n_matched += 1
        print(f"[triangulate] report.json 100%-matched fns: {n_matched} "
              f"(folded via unified_id; no extra anchors — rb3 .s is anonymous)",
              file=sys.stderr)

    # --- 2. sizes (for output records) ---
    funcs = load_functions(args.rb3_symbols)
    addr_to_size = {a: s for a, s, _ in funcs}

    # --- 3. parse asm ---
    print("[triangulate] parsing rb3 asm...", file=sys.stderr)
    rb3_fns = _cg_load_rb3_fns(args.rb3_asm)
    print(f"[triangulate]   rb3 fns parsed: {len(rb3_fns)}", file=sys.stderr)
    print("[triangulate] parsing dc3 asm (recursive)...", file=sys.stderr)
    dc3_by_name = _cg_load_dc3_fns(args.dc3_asm)
    print(f"[triangulate]   dc3 named fns parsed: {len(dc3_by_name)}",
          file=sys.stderr)

    # --- 4. triangulate: positional bl alignment, vote per anonymous callee ---
    from collections import defaultdict, Counter
    votes = defaultdict(Counter)        # fn_anon -> Counter({dc3_name: votes})
    voters = defaultdict(set)           # fn_anon -> set of distinct caller names
    pairings_attempted = pairings_aligned = 0
    for rb3_addr, dc3_name in rb3_to_dc3.items():
        rb3_fn = rb3_fns.get(rb3_addr)
        dc3_fn = dc3_by_name.get(dc3_name)
        if rb3_fn is None or dc3_fn is None:
            continue
        rb3_bls, dc3_bls = rb3_fn["bl"], dc3_fn["bl"]
        pairings_attempted += 1
        if len(rb3_bls) != len(dc3_bls):
            continue  # bl-count mismatch (usually /Ob2 inline divergence)
        pairings_aligned += 1
        for (_o_r, tgt_r), (_o_d, tgt_d) in zip(rb3_bls, dc3_bls):
            if not tgt_r.startswith("fn_"):
                continue  # rb3 callee already named/labelled
            if tgt_d.startswith("fn_") or tgt_d.startswith("lbl_"):
                continue  # dc3 callee also anonymous -> no signal
            votes[tgt_r][tgt_d] += 1
            voters[tgt_r].add(dc3_name)

    # --- 5. decide proposals, NEW-only, enriched via dc3_map ---
    mapinfo = parse_map(args.dc3_map)
    objects_db = load_objects(args.dc3_objects)

    proposals = []
    tier_counts = Counter()
    # cross-verify counters: all-vote overlap, plus unanimous-only (the POC's
    # 94.1% headline metric; faithful port should reproduce it bit-for-bit).
    verify_agree = verify_disagree = 0
    unanim_agree = unanim_disagree = 0
    skipped_existing = skipped_noncanon = 0
    for fn_anon, ct in votes.items():
        addr_part = fn_anon[3:]
        if "+" in addr_part or "-" in addr_part:
            skipped_noncanon += 1
            continue
        try:
            rb3_va = int(addr_part, 16)
        except ValueError:
            skipped_noncanon += 1
            continue
        total = sum(ct.values())
        winner, w_votes = ct.most_common(1)[0]
        n_distinct_callers = len(voters[fn_anon])

        # cross-verify against unified_id (precision check on the overlap)
        if rb3_va in rb3_to_dc3:
            agree = (rb3_to_dc3[rb3_va] == winner)
            verify_agree += agree
            verify_disagree += (not agree)
            if len(ct) == 1:  # unanimous subset == the POC metric
                unanim_agree += agree
                unanim_disagree += (not agree)
        if rb3_va in unified_addrs:
            skipped_existing += 1
            continue  # NEW-only; never clobber an existing oracle entry

        unanimous = (len(ct) == 1)
        if unanimous and n_distinct_callers >= 2:
            tier, confidence, similarity = "multi", 0.97, 0.96
        elif unanimous:
            tier, confidence, similarity = "single", 0.80, 0.80
        elif w_votes / total >= args.majority_frac:
            tier, confidence, similarity = "majority", 0.70, 0.70
        else:
            continue  # too contested to propose
        tier_counts[tier] += 1

        # enrich the winning dc3 name -> obj/cpp/demangled (mirror merge_bindiff)
        info = mapinfo.get(winner)
        if info is not None:
            dc3_obj = info["obj"]
            inline_only = "i" in info["tags"]
            dc3_cpp = obj_to_cpp(dc3_obj, objects_db)
        else:
            dc3_obj = dc3_cpp = None
            inline_only = False
        # apply the same dc3 HamX->BandX class substitution merge_bindiff /
        # gen_target_map use, so the rb3-side name is correct for the symbol map.
        subbed_name, _applied = substitute_dc3_class_names(winner)

        proposals.append({
            "rb3_fn": fn_anon,
            "rb3_addr": f"0x{rb3_va:08x}",
            "size": addr_to_size.get(rb3_va, 0),
            "source": "callgraph",
            "dc3_name": subbed_name,
            "dc3_name_demangled": demangle_safe(winner),
            "dc3_obj": dc3_obj,
            "dc3_inline_only": inline_only,
            "bindiff_src": dc3_cpp,          # named bindiff_src so cluster/map
                                              # tooling (which keys on this) works
            "similarity": similarity,
            "confidence": confidence,
            "algorithm": "callgraph triangulation",
            "cg_tier": tier,
            "cg_distinct_callers": n_distinct_callers,
            "cg_total_votes": total,
            "cg_top_votes": w_votes,
            "cg_alternatives": dict(ct.most_common(5)),
        })

    proposals.sort(key=lambda r: int(r["rb3_addr"], 16))

    with open(args.out, "w") as f:
        json.dump(proposals, f, indent=1)

    overlap = verify_agree + verify_disagree
    precision = 100 * verify_agree / overlap if overlap else 0.0
    unanim_overlap = unanim_agree + unanim_disagree
    unanim_precision = (100 * unanim_agree / unanim_overlap
                        if unanim_overlap else 0.0)
    align_rate = (100 * pairings_aligned / pairings_attempted
                  if pairings_attempted else 0.0)

    print(f"[triangulate] anchors aligned: {pairings_aligned}/"
          f"{pairings_attempted} ({align_rate:.1f}%)", file=sys.stderr)
    print(f"[triangulate] cross-verify precision (all-vote overlap): "
          f"{verify_agree}/{overlap} ({precision:.2f}%)", file=sys.stderr)
    print(f"[triangulate] cross-verify precision (unanimous overlap, POC "
          f"metric): {unanim_agree}/{unanim_overlap} "
          f"({unanim_precision:.2f}%)", file=sys.stderr)
    print(f"[triangulate]   (disagreements are mostly STL/ICF aliases — same "
          f"code body, different MSVC name expansion)", file=sys.stderr)
    print(f"[triangulate] skipped: already-in-unified={skipped_existing} "
          f"non-canonical-target={skipped_noncanon}", file=sys.stderr)
    print(f"[triangulate] NEW proposals: {len(proposals)}", file=sys.stderr)
    for tier in ("multi", "single", "majority"):
        print(f"[triangulate]   {tier:9s}: {tier_counts[tier]}", file=sys.stderr)
    print(f"[triangulate] wrote {args.out}", file=sys.stderr)
    print(f"[triangulate] MERGE: union with unified_id.json — these are "
          f"NEW addrs (source='callgraph'); existing entries untouched. "
          f"Wave tooling (bindiff_clusters/gen_target_map) keys on source in "
          f"('both','bindiff'); pass --unified <merged> after unioning, or "
          f"only the 'multi' tier clears gen_target_map's default thresholds.",
          file=sys.stderr)


# ---------------------------------------------------------------------------
# RTTI + vtable slot transitivity: rtti
# (productionizes tools/exploratory/rtti_vtable_combined.py — see
#  docs/decomp/rtti-vtable-transitivity.md)
#
# Method:
#   1. Walk rb3's NON-STANDARD X360 RTTI (no 0x19930522 COL signature; 12 zero
#      bytes instead) to recover {rb3 vtable VA -> mangled class name + slot
#      fn VAs}. Recovers 1,321 vtables (vs jeff's 342 heuristic scan).
#   2. Parse dc3's leaked .map for `??_7CLASS@@6B@` -> dc3 vtable RVA, read the
#      dc3 PE to get dc3 slot fn VAs, name them via the .map.
#   3. Join by mangled class name. For each common class with a SINGLE vtable
#      on each side, pair slot-by-slot: rb3 slot[i] (anonymous fn_) == dc3
#      slot[i] (named).
#   4. Confidence tier:
#        HIGH = rb3 vt and dc3 vt have IDENTICAL slot counts (no engine drift)
#        LOW  = slot counts differ (drift; positional pairing unreliable)
#      Only HIGH ships by default; LOW is opt-in via --include-low.
# ---------------------------------------------------------------------------

# X360 RTTI: a TypeDescriptor is `vptr(4) spare(4) ".?AV<CLASS>@@\0"`.
RTTI_TD_NAME_RE = re.compile(rb'\.\?A[VU]([^\x00]+)@@\x00')
# dc3 .map "Publics by Value" row: ` 0001:0006d3a0  ?Foo@Bar@@... 82373b80 ...`
DC3_MAP_PUB_RE = re.compile(
    r'^\s+([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})')
# vtable symbol `??_7CLASS@@6B<modifier>@` — group(1)=class, group(2)=modifier.
DC3_MAP_VT_RE = re.compile(r'^\?\?_7(.+?)@@6B(.*)@$')


def _pe_parse(path):
    """Return (image_bytes, img_base, [(name, va, vsize, raw_off), ...])."""
    exe = open(path, 'rb').read()
    pe_off = struct.unpack_from('<I', exe, 0x3c)[0]
    n_sections = struct.unpack_from('<H', exe, pe_off + 6)[0]
    size_oh = struct.unpack_from('<H', exe, pe_off + 20)[0]
    img_base = struct.unpack_from('<I', exe, pe_off + 24 + 28)[0]
    sect_start = pe_off + 24 + size_oh
    sections = []
    for i in range(n_sections):
        o = sect_start + i * 40
        name = exe[o:o + 8].rstrip(b'\x00').decode('ascii', errors='replace')
        vaddr = struct.unpack_from('<I', exe, o + 12)[0]
        vsize = struct.unpack_from('<I', exe, o + 8)[0]
        raw_off = struct.unpack_from('<I', exe, o + 20)[0]
        sections.append((name, img_base + vaddr, vsize, raw_off))
    return exe, img_base, sections


def _pe_text_bounds(sections):
    for name, va, sz, _ in sections:
        if name == '.text':
            return va, va + sz
    raise RuntimeError('no .text section')


def _pe_off_to_va(sections, o):
    for _n, sec_va, vsize, off in sections:
        if off <= o < off + vsize:
            return sec_va + (o - off)
    return None


def _pe_read_va(exe, sections, va, n):
    for _name, sec_va, vsize, raw_off in sections:
        if sec_va <= va < sec_va + vsize:
            offset = raw_off + (va - sec_va)
            return exe[offset:offset + n]
    return None


def _rtti_walk_rb3(rb3_exe_path):
    """Walk rb3's non-standard X360 RTTI. Returns
    {vt_va:int -> {'class': mangled, 'slot_vas': [int]}}.

    Layout (empirically derived; band.exe has NO 0x19930522 COL signature):
      TypeDescriptor: vptr(4)+spare(4)+".?AV<CLASS>@@\\0"
      COL (precedes vtable at vt_va-4): +0..+0x8 = three zero dwords,
                                        +0xC = pTypeDescriptor
      vtable: vt_va-4 -> COL ptr; vt_va+0 = slot 0.
    Recovers ALL named vtables (1,321), not just jeff's 342 heuristic scan.
    """
    exe, _img_base, sections = _pe_parse(rb3_exe_path)
    text_lo, text_hi = _pe_text_bounds(sections)

    # --- TypeDescriptors: a `.?AV<CLASS>@@\0` whose -8 dword is a vptr in .text
    td_records = []  # (td_va, mangled_class)
    for m in RTTI_TD_NAME_RE.finditer(exe):
        name_start = m.start()
        td_off = name_start - 8
        if td_off < 0:
            continue
        vptr = struct.unpack_from('>I', exe, td_off)[0]
        if not (0x82000000 <= vptr < 0x83000000):
            continue
        td_va = _pe_off_to_va(sections, td_off)
        if td_va is None:
            continue
        td_records.append((td_va, m.group(1).decode('ascii', errors='replace')))

    # --- COLs: a >I pointer to a TD preceded by 12 zero bytes (no signature word)
    cols = []  # (col_va, mangled_class)
    for td_va, cls in td_records:
        pat = struct.pack('>I', td_va)
        start = 0
        while True:
            p = exe.find(pat, start)
            if p < 0:
                break
            start = p + 4
            col_start = p - 0xC
            if col_start < 0:
                continue
            if exe[col_start:p] != b'\x00' * 0xC:
                continue
            col_va = _pe_off_to_va(sections, col_start)
            if col_va is not None:
                cols.append((col_va, cls))

    # --- vtables: a >I pointer to a COL; the vtable starts at ref+4
    out = {}
    for col_va, cls in cols:
        pat = struct.pack('>I', col_va)
        start = 0
        while True:
            p = exe.find(pat, start)
            if p < 0:
                break
            start = p + 4
            vt_off = p + 4
            vt_va = _pe_off_to_va(sections, vt_off)
            if vt_va is None:
                continue
            slots = []
            for j in range(200):
                so = vt_off + j * 4
                if so + 4 > len(exe):
                    break
                v = struct.unpack_from('>I', exe, so)[0]
                if not (text_lo <= v < text_hi):
                    break
                slots.append(v)
            if vt_va in out and out[vt_va]['class'] != cls:
                continue  # duplicate; keep first
            out[vt_va] = {'class': cls, 'slot_vas': slots}
    return out


def _dc3_parse_vtables_from_map(dc3_map_path):
    """Return (addr_to_name {rva:int -> name}, vt_to_class {vt_rva:int ->
    (class, modifier)}) from dc3's leaked .map. Vtable symbols (??_7..@@6B..@)
    live in .rdata, so this independent parse is needed (dc3_map.parse_map only
    ingests .text)."""
    addr_to_name = {}
    vt_to_class = {}
    saw_header = False
    with open(dc3_map_path, errors="replace") as f:
        for line in f:
            if 'Publics by Value' in line:
                saw_header = True
                continue
            if not saw_header:
                continue
            m = DC3_MAP_PUB_RE.match(line)
            if not m:
                continue
            rva = int(m.group(4), 16)
            name = m.group(3)
            if rva < 0x82000000:
                continue
            if rva not in addr_to_name:
                addr_to_name[rva] = name
            vm = DC3_MAP_VT_RE.match(name)
            if vm:
                vt_to_class[rva] = (vm.group(1), vm.group(2))
    return addr_to_name, vt_to_class


def _dc3_extract_vtable(rva, exe, sections, addr_to_name, sorted_addrs,
                        text_lo, text_hi):
    """Read a dc3 vtable at rva -> [(slot_idx, slot_va, slot_name_or_None)].
    Slot count is bounded by the next named symbol (vtable end). Stops at the
    first non-.text pointer (RTTI data follows the slots)."""
    import bisect
    i = bisect.bisect_right(sorted_addrs, rva)
    n_slots = 200 if i >= len(sorted_addrs) else min(200, (sorted_addrs[i] - rva) // 4)
    if n_slots <= 0:
        return []
    raw = _pe_read_va(exe, sections, rva, n_slots * 4)
    if raw is None or len(raw) < 4:
        return []
    out = []
    for j in range(min(n_slots, len(raw) // 4)):
        v = struct.unpack_from('>I', raw, j * 4)[0]
        if not (text_lo <= v < text_hi):
            break
        out.append((j, v, addr_to_name.get(v)))
    return out


def rtti(args):
    """RTTI + vtable slot transitivity: identify anonymous rb3 fns by joining
    rb3's RTTI-recovered vtables to dc3's named `??_7CLASS@@6B@` vtables by
    class name, then pairing slots positionally.

    Output: a NEW-only oracle file (default unified_id_rtti.json) whose records
    are schema-compatible with unified_id.json (source="rtti").

    Precision reality (re-measured, not parroted): HIGH-tier raw precision is
    ~71% vs unified_id overlap — BELOW the >=90% bar callgraph clears. So even
    HIGH is emitted to its OWN file (never auto-unioned into the symbol map) at
    confidence 0.80, which is sub-threshold for gen_target_map's 0.95 default.
    It is a manual-review / splits-derivation oracle. Many of the ~29% "misses"
    are ICF aliases (??_E/??_G deleting-dtor pairs, folded template-container
    methods) so the actionable error rate is lower, but it still does not clear
    the auto-merge bar. LOW-tier (~41%) is suppressed entirely unless
    --include-low, and then lands in a SEPARATE file (--out-low).
    """
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from dc3_map import parse_map, load_objects, obj_to_cpp, demangle_safe
    from collections import defaultdict, Counter

    # --- anchor / NEW-only filter set: unified_id addrs + addr->dc3_name ---
    with open(args.unified) as f:
        unified = json.load(f)
    unified_addrs = set()
    rb3_to_dc3 = {}  # int VA -> dc3_name (for cross-verify)
    for r in unified:
        try:
            va = int(r["rb3_addr"], 16)
        except (KeyError, ValueError):
            continue
        unified_addrs.add(va)
        if r.get("dc3_name"):
            rb3_to_dc3[va] = r["dc3_name"]

    # --- 1. rb3 RTTI walk ---
    print("[rtti] walking rb3 X360 RTTI...", file=sys.stderr)
    rb3_vts = _rtti_walk_rb3(args.rb3_exe)
    print(f"[rtti]   rb3 vtables (via RTTI): {len(rb3_vts)}", file=sys.stderr)
    rb3_by_class = defaultdict(list)  # cls -> [(vt_va, slot_vas)]
    for vt_va, info in rb3_vts.items():
        rb3_by_class[info['class']].append((vt_va, info['slot_vas']))
    print(f"[rtti]   distinct rb3 classes: {len(rb3_by_class)}", file=sys.stderr)

    # --- 2. dc3 vtables from .map + PE ---
    print("[rtti] parsing dc3 .map + dc3 PE vtables...", file=sys.stderr)
    dc3_addr_to_name, dc3_vt_to_class = _dc3_parse_vtables_from_map(args.dc3_map)
    dc3_exe, _, dc3_sections = _pe_parse(args.dc3_exe)
    dc3_text_lo, dc3_text_hi = _pe_text_bounds(dc3_sections)
    sorted_dc3_addrs = sorted(dc3_addr_to_name.keys())
    dc3_by_class = defaultdict(list)  # cls -> [{'vt_va','base','slots'}]
    for vt_va, (cls, base) in dc3_vt_to_class.items():
        slots = _dc3_extract_vtable(vt_va, dc3_exe, dc3_sections,
                                    dc3_addr_to_name, sorted_dc3_addrs,
                                    dc3_text_lo, dc3_text_hi)
        dc3_by_class[cls].append({'vt_va': vt_va, 'base': base, 'slots': slots})
    print(f"[rtti]   distinct dc3 classes (??_7CLS@@6B@): {len(dc3_by_class)}",
          file=sys.stderr)

    common_classes = set(rb3_by_class) & set(dc3_by_class)
    print(f"[rtti]   common (rb3 INT dc3) classes: {len(common_classes)}",
          file=sys.stderr)

    # --- 3. slot-by-slot pairing for single-vtable classes ---
    # proposals: rb3_va(int) -> (dc3_name, tier, cls, slot_idx)
    proposals = {}
    pairs_done = 0
    multi_vt_skipped = 0
    for cls in sorted(common_classes):
        rb3_list = rb3_by_class[cls]
        dc3_list = dc3_by_class[cls]
        if len(rb3_list) > 1 or len(dc3_list) > 1:
            multi_vt_skipped += 1  # multiple-inheritance sub-vtables; deferred
            continue
        rb3_vt_va, rb3_slots = rb3_list[0]
        dc3_info = dc3_list[0]
        dc3_slots = dc3_info['slots']
        if not dc3_slots or not rb3_slots:
            continue
        same_layout = len(rb3_slots) == len(dc3_slots)
        n_pair = min(len(rb3_slots), len(dc3_slots))
        pairs_done += 1
        for j in range(n_pair):
            rb3_slot_va = rb3_slots[j]
            dc3_slot_name = dc3_slots[j][2]
            if dc3_slot_name is None:
                continue
            existing = proposals.get(rb3_slot_va)
            if existing and existing[0] != dc3_slot_name:
                continue  # name collision across vtables; keep first
            # same-name (or first) hit: take the latest tier seen, so a
            # later HIGH pairing upgrades an earlier LOW one (POC behaviour).
            tier = 'HIGH' if same_layout else 'LOW'
            proposals[rb3_slot_va] = (dc3_slot_name, tier, cls, j)
    print(f"[rtti]   pairings done: {pairs_done}  "
          f"multi-vt classes skipped: {multi_vt_skipped}", file=sys.stderr)

    # --- 4. cross-verify against unified_id, split NEW vs overlap, by tier ---
    agree = {'HIGH': 0, 'LOW': 0}
    disagree = {'HIGH': 0, 'LOW': 0}
    new_records = {'HIGH': [], 'LOW': []}
    # addrs already in unified_id (incl. autoid-only entries with no dc3_name)
    # are skipped — strict address-based NEW-only, matching `triangulate`. Some
    # of these are autoid string-attributions lacking a dc3_name that `rtti`
    # *could* enrich; we count them but don't clobber the existing entry.
    skipped_existing = {'HIGH': 0, 'LOW': 0}

    mapinfo = parse_map(args.dc3_map)
    objects_db = load_objects(args.dc3_objects)
    funcs = load_functions(args.rb3_symbols)
    addr_to_size = {a: s for a, s, _ in funcs}

    def build_record(rb3_va, dc3_name, tier, cls, slot_idx):
        info = mapinfo.get(dc3_name)
        if info is not None:
            dc3_obj = info["obj"]
            inline_only = "i" in info["tags"]
            dc3_cpp = obj_to_cpp(dc3_obj, objects_db)
        else:
            dc3_obj = dc3_cpp = None
            inline_only = False
        subbed_name, _applied = substitute_dc3_class_names(dc3_name)
        # HIGH = matching slot counts (no drift); confidence below
        # gen_target_map's 0.95 default (HIGH raw precision ~71%, semantic
        # ~80-85% after ICF-alias accounting) so it stays manual-review.
        confidence = 0.80 if tier == 'HIGH' else 0.40
        similarity = 0.80 if tier == 'HIGH' else 0.40
        return {
            "rb3_fn": f"fn_{rb3_va:08X}",
            "rb3_addr": f"0x{rb3_va:08x}",
            "size": addr_to_size.get(rb3_va, 0),
            "source": "rtti",
            "dc3_name": subbed_name,
            "dc3_name_demangled": demangle_safe(dc3_name),
            "dc3_obj": dc3_obj,
            "dc3_inline_only": inline_only,
            "bindiff_src": dc3_cpp,
            "similarity": similarity,
            "confidence": confidence,
            "algorithm": "rtti+vtable transitivity",
            "rtti_tier": tier,
            "rtti_class": cls,
            "rtti_slot": slot_idx,
        }

    for rb3_va, (dc3_name, tier, cls, slot_idx) in proposals.items():
        if rb3_va in rb3_to_dc3:
            if rb3_to_dc3[rb3_va] == dc3_name:
                agree[tier] += 1
            else:
                disagree[tier] += 1
        if rb3_va in unified_addrs:
            skipped_existing[tier] += 1
            continue  # NEW-only
        new_records[tier].append(build_record(rb3_va, dc3_name, tier, cls, slot_idx))

    for tier in ('HIGH', 'LOW'):
        new_records[tier].sort(key=lambda r: int(r["rb3_addr"], 16))

    def prec(tier):
        ov = agree[tier] + disagree[tier]
        return (100 * agree[tier] / ov) if ov else 0.0

    # HIGH ships by default
    with open(args.out, "w") as f:
        json.dump(new_records['HIGH'], f, indent=1)

    # LOW: opt-in, separate file
    wrote_low = False
    if args.include_low:
        with open(args.out_low, "w") as f:
            json.dump(new_records['LOW'], f, indent=1)
        wrote_low = True

    print(f"[rtti] cross-verify precision (vs unified_id overlap):", file=sys.stderr)
    print(f"[rtti]   HIGH: {agree['HIGH']}/{agree['HIGH']+disagree['HIGH']} "
          f"({prec('HIGH'):.1f}%)   LOW: {agree['LOW']}/"
          f"{agree['LOW']+disagree['LOW']} ({prec('LOW'):.1f}%)", file=sys.stderr)
    print(f"[rtti]   skipped (addr already in unified_id, incl. autoid-no-name "
          f"that rtti could enrich): HIGH={skipped_existing['HIGH']} "
          f"LOW={skipped_existing['LOW']}", file=sys.stderr)
    print(f"[rtti]   (many HIGH misses are ICF aliases — ??_E/??_G deleting-"
          f"dtor pairs + folded template-container methods — same code body, "
          f"different MSVC name; semantic precision is higher than raw)",
          file=sys.stderr)
    print(f"[rtti] NOTE: HIGH raw precision ({prec('HIGH'):.0f}%) is BELOW the "
          f">=90%% default-on/auto-merge bar (callgraph multi-tier is 94%%). "
          f"HIGH is written to its OWN file at confidence 0.80 (sub-threshold "
          f"for gen_target_map's 0.95) — it is a manual-review oracle, NOT "
          f"auto-merged into the symbol map.", file=sys.stderr)
    print(f"[rtti] {len(new_records['HIGH'])} HIGH records -> {args.out}",
          file=sys.stderr)
    if wrote_low:
        print(f"[rtti] {len(new_records['LOW'])} LOW records -> {args.out_low} "
              f"(opt-in; ~41% precision — DO NOT auto-merge)", file=sys.stderr)
    else:
        print(f"[rtti] LOW tier suppressed ({len(new_records['LOW'])} records, "
              f"~41% precision); pass --include-low to emit {args.out_low}",
              file=sys.stderr)
    print(f"[rtti] MERGE: HIGH is NEW addrs (source='rtti', confidence 0.80 — "
          f"below gen_target_map's 0.95 default, so manual-review until "
          f"promoted). Union with unified_id.json is collision-free.",
          file=sys.stderr)


# ---------------------------------------------------------------------------
# Vtable slot transitivity (no RTTI): vtable
# (productionizes tools/exploratory/vtable_transitivity.py — see
#  docs/decomp/rtti-vtable-transitivity.md §"vtable subcommand")
#
# Pre-RTTI technique. Uses jeff's 342 heuristically-scanned rb3 `vftable_<addr>`
# symbols (parsed from the *_rdata.s files), resolves the slots we ALREADY know
# (via unified_id), then finds the dc3 vtable whose named slots align at the
# same indices, and transfers the remaining names. Subsumed by `rtti` (which
# recovers 3.8x more vtables) but kept as an orthogonal fallback (+37).
# ---------------------------------------------------------------------------

RB3_VFTABLE_RE = re.compile(r'^\.obj vftable_([0-9A-Fa-f]{8})')
RB3_VFT_SLOT_RE = re.compile(r'^\s*\.4byte\s+(\S+)')
RB3_ENDOBJ_RE = re.compile(r'^\.endobj')


def _parse_rb3_vftables_from_asm(asm_dir):
    """Parse rb3 *_rdata.s for jeff's `.obj vftable_<addr>` records.
    Returns {vt_va:int -> [slot_token, ...]} where tokens are fn_<addr>/lbl_/..."""
    out = {}
    cur_va = None
    cur_slots = None
    for fname in sorted(p for p in os.listdir(asm_dir) if "rdata" in p and p.endswith(".s")):
        with open(os.path.join(asm_dir, fname), errors="replace") as f:
            for line in f:
                m = RB3_VFTABLE_RE.match(line)
                if m:
                    cur_va = int(m.group(1), 16)
                    cur_slots = []
                    continue
                if cur_slots is not None:
                    if RB3_ENDOBJ_RE.match(line):
                        out[cur_va] = cur_slots
                        cur_va = None
                        cur_slots = None
                        continue
                    sm = RB3_VFT_SLOT_RE.match(line)
                    if sm:
                        cur_slots.append(sm.group(1).strip())
    return out


def vtable(args):
    """Vtable slot transitivity (no RTTI): identify anonymous rb3 fns by pairing
    jeff's heuristic `vftable_<addr>` rb3 vtables to dc3's named vtables via
    already-known slot names, then transferring the unnamed slots.

    Output: a NEW-only oracle file (default unified_id_vtable.json) whose records
    are schema-compatible with unified_id.json (source="vtable"). Subsumed by
    `rtti`; kept as an orthogonal fallback.
    """
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from dc3_map import parse_map, load_objects, obj_to_cpp, demangle_safe
    from collections import Counter, defaultdict

    with open(args.unified) as f:
        unified = json.load(f)
    unified_addrs = set()
    rb3_to_dc3 = {}     # int VA -> dc3_name
    rb3_to_dc3_lc = {}  # "fn_xxxxxxxx" lowercase -> dc3_name
    for r in unified:
        try:
            va = int(r["rb3_addr"], 16)
        except (KeyError, ValueError):
            continue
        unified_addrs.add(va)
        if r.get("dc3_name"):
            rb3_to_dc3[va] = r["dc3_name"]
            rb3_to_dc3_lc[r["rb3_fn"].lower()] = r["dc3_name"]

    # --- dc3 vtables (name->slots) ---
    print("[vtable] parsing dc3 .map + dc3 PE vtables...", file=sys.stderr)
    dc3_addr_to_name, dc3_vt_to_class = _dc3_parse_vtables_from_map(args.dc3_map)
    dc3_exe, _, dc3_sections = _pe_parse(args.dc3_exe)
    dc3_text_lo, dc3_text_hi = _pe_text_bounds(dc3_sections)
    sorted_dc3_addrs = sorted(dc3_addr_to_name.keys())
    # name->{'class','slots'} (re-key vt_to_class by the full ??_7 symbol name)
    dc3_vt_contents = {}
    for vt_rva, (cls, base) in dc3_vt_to_class.items():
        slots = _dc3_extract_vtable(vt_rva, dc3_exe, dc3_sections,
                                    dc3_addr_to_name, sorted_dc3_addrs,
                                    dc3_text_lo, dc3_text_hi)
        if not slots:
            continue
        vt_name = dc3_addr_to_name.get(vt_rva, f"vt_{vt_rva:08x}")
        dc3_vt_contents[vt_name] = {'class': cls, 'slots': slots}
    print(f"[vtable]   dc3 vtables extracted: {len(dc3_vt_contents)}",
          file=sys.stderr)

    # --- rb3 vtables (jeff's heuristic vftable_<addr>) ---
    rb3_vt = _parse_rb3_vftables_from_asm(args.rb3_asm)
    print(f"[vtable]   rb3 vftables (jeff scan): {len(rb3_vt)}", file=sys.stderr)

    # index dc3 vts by their named slot fns
    dc3_slot_to_vts = defaultdict(list)
    for vt_name, info in dc3_vt_contents.items():
        for _idx, _va, sname in info['slots']:
            if sname is not None:
                dc3_slot_to_vts[sname].append(vt_name)

    # proposals: rb3_va(int) -> (dc3_name, slot_idx, dc3_vt, evidence, cls)
    proposals = {}
    matched_pairs = 0
    for rb3_va, rb3_slots in rb3_vt.items():
        if not rb3_slots:
            continue
        rb3_known = []  # (slot_idx, rb3_fn, dc3_name)
        for i, slot in enumerate(rb3_slots):
            if not slot.startswith('fn_'):
                continue
            dc3_name = rb3_to_dc3_lc.get(slot.lower())
            if dc3_name:
                rb3_known.append((i, slot, dc3_name))
        if not rb3_known:
            continue
        cand_scores = Counter()
        for i, _fn, dc3_name in rb3_known:
            for vt_name in dc3_slot_to_vts.get(dc3_name, []):
                info = dc3_vt_contents[vt_name]
                if i < len(info['slots']) and info['slots'][i][2] == dc3_name:
                    cand_scores[vt_name] += 1
        if not cand_scores:
            continue
        top_vt, top_score = cand_scores.most_common(1)[0]
        info = dc3_vt_contents[top_vt]
        all_known_aligned = all(
            i < len(info['slots']) and info['slots'][i][2] == dc3_name
            for i, _f, dc3_name in rb3_known)
        if not all_known_aligned:
            continue
        matched_pairs += 1
        n_pair = min(len(rb3_slots), len(info['slots']))
        for i in range(n_pair):
            rb3_slot = rb3_slots[i]
            if not rb3_slot.startswith('fn_'):
                continue
            try:
                rb3_slot_va = int(rb3_slot[3:], 16)
            except ValueError:
                continue
            if rb3_slot.lower() in rb3_to_dc3_lc:
                continue  # already known
            dc3_slot_name = info['slots'][i][2]
            if dc3_slot_name is None:
                continue
            existing = proposals.get(rb3_slot_va)
            if existing and existing[0] != dc3_slot_name:
                continue
            if existing is None:
                proposals[rb3_slot_va] = (dc3_slot_name, i, top_vt, top_score,
                                          info['class'])
    print(f"[vtable]   rb3<->dc3 vtable pairings: {matched_pairs}",
          file=sys.stderr)

    # cross-verify + build NEW records
    mapinfo = parse_map(args.dc3_map)
    objects_db = load_objects(args.dc3_objects)
    funcs = load_functions(args.rb3_symbols)
    addr_to_size = {a: s for a, s, _ in funcs}
    agree = disagree = 0
    new_records = []
    for rb3_va, (dc3_name, slot_idx, top_vt, evidence, cls) in proposals.items():
        if rb3_va in rb3_to_dc3:
            if rb3_to_dc3[rb3_va] == dc3_name:
                agree += 1
            else:
                disagree += 1
        if rb3_va in unified_addrs:
            continue  # NEW-only
        info = mapinfo.get(dc3_name)
        if info is not None:
            dc3_obj = info["obj"]
            inline_only = "i" in info["tags"]
            dc3_cpp = obj_to_cpp(dc3_obj, objects_db)
        else:
            dc3_obj = dc3_cpp = None
            inline_only = False
        subbed_name, _applied = substitute_dc3_class_names(dc3_name)
        new_records.append({
            "rb3_fn": f"fn_{rb3_va:08X}",
            "rb3_addr": f"0x{rb3_va:08x}",
            "size": addr_to_size.get(rb3_va, 0),
            "source": "vtable",
            "dc3_name": subbed_name,
            "dc3_name_demangled": demangle_safe(dc3_name),
            "dc3_obj": dc3_obj,
            "dc3_inline_only": inline_only,
            "bindiff_src": dc3_cpp,
            "similarity": 0.80,
            "confidence": 0.80,
            "algorithm": "vtable transitivity (no RTTI)",
            "vt_class": cls,
            "vt_dc3_vtable": top_vt,
            "vt_slot": slot_idx,
            "vt_pairing_evidence": evidence,
        })
    new_records.sort(key=lambda r: int(r["rb3_addr"], 16))

    with open(args.out, "w") as f:
        json.dump(new_records, f, indent=1)

    ov = agree + disagree
    prec = (100 * agree / ov) if ov else 0.0
    print(f"[vtable] cross-verify precision (vs unified_id overlap): "
          f"{agree}/{ov} ({prec:.1f}%)", file=sys.stderr)
    print(f"[vtable] {len(new_records)} NEW records -> {args.out}",
          file=sys.stderr)
    print(f"[vtable]   (subsumed by `rtti`; orthogonal fallback. NEW addrs, "
          f"source='vtable', confidence 0.80 -> manual-review until promoted.)",
          file=sys.stderr)


# ---------------------------------------------------------------------------

# splits.txt section header: `Foo.cpp:` at column 0
SPLITS_HEADER_RE = re.compile(r"^([A-Za-z0-9_]+\.cpp):\s*$")
SPLITS_RANGE_RE = re.compile(
    r"^\s*\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)\s*$")


def _parse_splits(path):
    """Return dict cpp_basename -> [(start, end), ...] for existing pinned
    clusters."""
    out = {}
    cur = None
    with open(path) as f:
        for line in f:
            h = SPLITS_HEADER_RE.match(line)
            if h:
                cur = h.group(1)
                out.setdefault(cur, [])
                continue
            if cur is None:
                continue
            m = SPLITS_RANGE_RE.match(line)
            if m:
                out[cur].append((int(m.group(1), 16), int(m.group(2), 16)))
    return out


def _load_rb3_symbols(path):
    """Return sorted list of (addr, size, name) for .text functions in RB3
    symbols.txt -- reuse the existing matcher.
    """
    return load_functions(path)


def bindiff_clusters(args):
    """Cluster unified_id.json bindiff entries by their resolved dc3 .cpp,
    compute convex-hull rb3 address ranges, filter by density/span/min-hits,
    and emit:
      * proposed_splits_bindiff.txt for clusters whose .cpp is NOT in
        existing splits.txt
      * cluster_spotcheck.csv for clusters whose .cpp IS already pinned
        (cross-validation report)
    """
    with open(args.unified) as f:
        unified = json.load(f)

    existing = _parse_splits(args.existing_splits)
    # match by basename, since splits.txt headers are basenames
    existing_basenames = {k.lower() for k in existing.keys()}

    funcs = _load_rb3_symbols(args.rb3_symbols)
    funcs_sorted_addrs = [a for a, _, _ in funcs]

    # Group bindiff records by resolved dc3 cpp basename
    clusters = {}  # basename -> list of (addr, size, similarity, dc3_name, demangled, inline_only, src_full)
    for rec in unified:
        if rec.get("source") not in ("both", "bindiff"):
            continue
        src = rec.get("bindiff_src")
        if not src:
            continue
        base = os.path.basename(src)  # e.g. App.cpp
        addr = int(rec["rb3_addr"], 16)
        sz = rec.get("size") or 0
        clusters.setdefault(base, []).append({
            "addr": addr,
            "size": sz,
            "similarity": rec.get("similarity") or 1.0,
            "dc3_name": rec.get("dc3_name"),
            "demangled": rec.get("dc3_name_demangled"),
            "inline_only": rec.get("dc3_inline_only", False),
            "src": src,
        })

    # Helper: # functions in [lo, hi)
    import bisect
    def fns_in_range(lo, hi):
        l = bisect.bisect_left(funcs_sorted_addrs, lo)
        r = bisect.bisect_left(funcs_sorted_addrs, hi)
        return r - l

    proposed = []  # for new clusters
    spotcheck = []  # for already-pinned clusters

    for base, hits in clusters.items():
        addrs = sorted(set(h["addr"] for h in hits))
        n_hits = len(addrs)
        if not addrs:
            continue
        lo = min(addrs)
        # end = max(addr + size); if size==0 fallback to next instruction
        hi = max(h["addr"] + (h["size"] or 4) for h in hits)
        span = hi - lo
        total_fns_in_span = fns_in_range(lo, hi) or 1
        # density: how many distinct bindiff hits / how many fns in convex hull
        density = n_hits / total_fns_in_span
        # density excluding inline-only (those don't have a distinct .text
        # symbol in dc3 but may still have one in rb3; this is an "emitted"
        # confidence axis)
        emitted_hits = sum(1 for h in hits if not h["inline_only"])
        density_emitted = emitted_hits / total_fns_in_span if total_fns_in_span else 0

        record = {
            "cpp": base,
            "src": hits[0]["src"],
            "lo": lo, "hi": hi, "span": span,
            "n_hits": n_hits,
            "n_hits_emitted": emitted_hits,
            "total_fns": total_fns_in_span,
            "density": density,
            "density_emitted": density_emitted,
            "samples": [(h["addr"], h["demangled"] or h["dc3_name"])
                        for h in sorted(hits, key=lambda x: x["addr"])[:5]],
        }

        if base.lower() in existing_basenames:
            # cross-validation: compute in-range vs total
            pinned = existing[base if base in existing else next(
                k for k in existing if k.lower() == base.lower())]
            if not pinned:
                continue
            p_lo = min(p[0] for p in pinned)
            p_hi = max(p[1] for p in pinned)
            in_range = sum(1 for h in hits if p_lo <= h["addr"] < p_hi)
            record["pinned_lo"] = p_lo
            record["pinned_hi"] = p_hi
            record["in_pinned_range"] = in_range
            record["agreement_pct"] = 100 * in_range / n_hits if n_hits else 0
            spotcheck.append(record)
        else:
            # apply filters for NEW cluster proposals
            if span > args.max_span:
                continue
            if n_hits < args.min_hits:
                continue
            if density < args.min_density:
                continue
            proposed.append(record)

    # rank proposed by density (descending), tiebreak by tighter span
    proposed.sort(key=lambda r: (-r["density"], r["span"]))

    # Emit proposed_splits_bindiff.txt
    with open(args.out, "w") as f:
        f.write("# Generated by fingerprint_match.py bindiff_clusters\n")
        f.write("# For human review only -- DO NOT auto-pin. See "
                "docs/plans/bindiff-integration.md.\n")
        f.write(f"# {len(proposed)} candidate clusters "
                f"(density>={args.min_density:.0%}, span<={args.max_span//1024}KiB, "
                f"hits>={args.min_hits})\n#\n")
        for r in proposed:
            f.write(f"# cpp={r['cpp']} hits={r['n_hits']} (emitted={r['n_hits_emitted']}) "
                    f"span={r['span']:#x} density={r['density']:.1%} "
                    f"density_emitted={r['density_emitted']:.1%} "
                    f"src={r['src']}\n")
            for addr, name in r["samples"]:
                f.write(f"#   {addr:#010x}  {name}\n")
            f.write(f"{r['cpp']}:\n")
            f.write(f"\t.text       start:0x{r['lo']:08X} end:0x{r['hi']:08X}\n")
            f.write("\n")

    # Emit cluster_spotcheck.csv next to the splits txt.
    # Include EVERY pinned cluster from splits.txt -- even ones with zero
    # bindiff hits, since "no overlap" is itself a signal (complementary
    # coverage: bindiff and string-autoid see different parts of the binary).
    csv_path = args.out.replace(".txt", ".spotcheck.csv")
    if csv_path == args.out:
        csv_path = args.out + ".spotcheck.csv"

    # spotcheck only has entries for pinned cpps that bindiff *attributed* to
    # (via dc3_obj -> dc3_cpp). Add the remaining pinned cpps with zero hits,
    # plus an *in-range cross-check*: how many bindiff entries (regardless of
    # their attributed dc3_cpp) physically land inside the pinned address
    # range. That catches the MasterAudio case (bindiff hits inside the range
    # are attributed to other .cpps because they're inlined globals).
    seen_pinned = {r["cpp"].lower() for r in spotcheck}
    for cpp, ranges in existing.items():
        if cpp.lower() in seen_pinned:
            continue
        if not ranges:
            continue
        p_lo = min(r[0] for r in ranges)
        p_hi = max(r[1] for r in ranges)
        spotcheck.append({
            "cpp": cpp, "pinned_lo": p_lo, "pinned_hi": p_hi,
            "lo": 0, "hi": 0, "n_hits": 0, "in_pinned_range": 0,
            "agreement_pct": 0.0,
        })

    # Also compute the address-range cross-check independent of attribution.
    # For each pinned range, count any bindiff hit whose rb3_addr lies inside.
    bindiff_addrs = sorted(int(rec["rb3_addr"], 16) for rec in unified
                           if rec.get("source") in ("both", "bindiff"))
    def hits_in_range(lo, hi):
        l = bisect.bisect_left(bindiff_addrs, lo)
        r = bisect.bisect_left(bindiff_addrs, hi)
        return r - l

    for r in spotcheck:
        r["bindiff_addrs_in_range"] = hits_in_range(r["pinned_lo"], r["pinned_hi"])

    spotcheck.sort(key=lambda r: r["cpp"].lower())
    with open(csv_path, "w") as f:
        f.write("cpp,pinned_lo,pinned_hi,attributed_min,attributed_max,"
                "attributed_total,attributed_in_pinned,attributed_agreement_pct,"
                "any_bindiff_in_range\n")
        for r in spotcheck:
            f.write(f"{r['cpp']},{r['pinned_lo']:#010x},{r['pinned_hi']:#010x},"
                    f"{r['lo']:#010x},{r['hi']:#010x},{r['n_hits']},"
                    f"{r['in_pinned_range']},{r['agreement_pct']:.1f},"
                    f"{r['bindiff_addrs_in_range']}\n")

    print(f"[bindiff_clusters] {len(proposed)} new-cluster candidates -> {args.out}",
          file=sys.stderr)
    print(f"[bindiff_clusters] {len(spotcheck)} pinned-cluster spot-checks "
          f"-> {csv_path}", file=sys.stderr)
    # Echo the top 5 candidates to stdout for inline review
    print("Top candidates by density:")
    for r in proposed[:10]:
        print(f"  {r['cpp']:40s}  hits={r['n_hits']:3d} "
              f"(emitted={r['n_hits_emitted']:3d})  span={r['span']:#08x}  "
              f"density={r['density']:.1%}")


def gen_target_map(args):
    """Auto-populate scripts/target_symbol_map.json from unified_id.json.

    Filter: source includes 'bindiff', confidence >= --min-confidence,
    similarity >= --min-similarity, and (unless --include-unpinned) rb3_addr
    falls inside a currently-pinned .text range in splits.txt. Records with
    missing/untrustworthy dc3_name (FUN_*/sub_*) are skipped.

    Why --include-unpinned matters
    ------------------------------
    objdiff pairs a caller's `bl` relocation against the callee by SYMBOL
    NAME. A pinned+compiled function whose call targets live in *unpinned*
    TUs sees those callees as anonymous `fn_<addr>` on the target side, so the
    relocation shows as a `diff_arg` even when the bytes match. Renaming those
    cross-TU callees (regardless of whether they're pinned) lets the relocation
    pair, which can tip a near-match to 100%. With --include-unpinned we emit a
    rename for every high-confidence callee in the binary, not just ones inside
    a pinned range. (Verified empirically: relaxing this ticked
    ChannelData::SetSlippable from a near-miss to a full match.)

    Regression safety
    -----------------
    A WRONG rename (e.g. a flowgraph-shape collision picking the wrong STL
    template instantiation, or the known 0x82759310->DrawBounds fingerprint
    collision) could mispair an existing match's callee. Three guards:
      * --min-similarity (default 0.96) drops low-similarity guesses; the
        observed wrong renames had similarity ~0.90, safe ones >=0.98.
      * A `_denylist` metadata key in the map (list of "0xADDR" strings) is
        never auto-emitted, so hand-flagged bad addresses stay anonymous.
      * Manual entries always win on collision (existing behavior), so a
        hand-verified rename is never overwritten by a bindiff guess.
    NOTE: matched_functions counts fuzzy_match_percent>=100, which already
    TOLERATES relocation-name noise. So even a stray wrong rename cannot
    *drop* a currently-matched function below 100 (an unrenamed `fn_X` and a
    wrongly-renamed one are both just reloc-name noise to the fuzzy scorer);
    the denylist/similarity guards exist to keep the map honest, not because a
    wrong rename can regress the count.

    Merge rule: existing manual entries in target_symbol_map.json are
    PRESERVED. If the same address appears in both manual + auto-gen,
    manual wins (the seed entries were hand-verified). Comment fields
    (keys not starting with 0x) are preserved verbatim.

    Output reports per-TU breakdown of how many fn_<addr> symbols got an
    auto-gen name within each pinned cluster (or [UNPINNED]).
    """
    # Load unified_id
    with open(args.unified) as f:
        unified = json.load(f)

    # Parse splits.txt for pinned .text ranges
    splits = _parse_splits(args.splits)
    # Build a flat list of (lo, hi, cpp) ordered by lo for fast lookup
    pinned = []
    for cpp, ranges in splits.items():
        for lo, hi in ranges:
            pinned.append((lo, hi, cpp))
    pinned.sort()
    pinned_los = [p[0] for p in pinned]

    import bisect
    def find_pinned_cpp(addr):
        i = bisect.bisect_right(pinned_los, addr) - 1
        if i < 0:
            return None
        lo, hi, cpp = pinned[i]
        if lo <= addr < hi:
            return cpp
        return None

    # Load existing map (preserve comment/metadata + manual entries)
    map_path = args.map
    if os.path.isfile(map_path):
        with open(map_path) as f:
            existing = json.load(f)
    else:
        existing = {}

    # Separate metadata (non-0x keys) from address entries
    metadata = {k: v for k, v in existing.items() if not k.lower().startswith("0x")}
    manual = {k: v for k, v in existing.items() if k.lower().startswith("0x")}
    # Apply Ham->Band substitution to existing entries too. Most existing 0x
    # entries were auto-generated by a prior (buggy) gen_target_map run with
    # raw dc3 names; we treat them as candidates for the same substitution.
    # A genuinely-intended HamX entry (unlikely — RB3 doesn't compile hamobj/)
    # would be silently rewritten, accepted tradeoff.
    n_existing_substituted = 0
    rewritten_manual = {}
    for k, v in manual.items():
        new_v, applied = substitute_dc3_class_names(v)
        if applied:
            n_existing_substituted += 1
        rewritten_manual[k] = new_v
    manual = rewritten_manual
    # Normalize manual keys to uppercase hex for collision detection
    manual_norm = {_norm_addr_key(k): (k, v) for k, v in manual.items()}

    # Denylist: addresses that must never be auto-emitted (hand-flagged bad
    # bindiff guesses, e.g. fingerprint collisions). Lives under the `_denylist`
    # metadata key as a list of "0xADDR" strings. Preserved verbatim across runs.
    denylist_raw = metadata.get("_denylist") or []
    denylist = {_norm_addr_key(k) for k in denylist_raw}

    # Filter unified_id records
    auto = {}              # normalized_addr -> (name, conf, cpp_or_UNPINNED)
    per_tu = {}            # cpp -> count of new entries
    skipped_reasons = {"low_confidence": 0, "not_bindiff": 0,
                       "bad_name": 0, "out_of_range": 0, "no_dc3_name": 0,
                       "low_similarity": 0, "denylisted": 0}
    n_class_substituted = 0
    class_sub_breakdown = {}  # "HamFoo->BandFoo" -> count
    for r in unified:
        source = r.get("source") or ""
        if "bindiff" not in source:
            skipped_reasons["not_bindiff"] += 1
            continue
        conf = r.get("confidence") or 0.0
        if conf < args.min_confidence:
            skipped_reasons["low_confidence"] += 1
            continue
        sim = r.get("similarity")
        if sim is not None and sim < args.min_similarity:
            skipped_reasons["low_similarity"] += 1
            continue
        name = r.get("dc3_name")
        if not name:
            skipped_reasons["no_dc3_name"] += 1
            continue
        if name.startswith(("FUN_", "sub_")):
            skipped_reasons["bad_name"] += 1
            continue
        addr_str = r.get("rb3_addr") or ""
        try:
            addr = int(addr_str, 16)
        except ValueError:
            continue
        norm = _norm_addr_key(addr_str)
        if norm in denylist:
            skipped_reasons["denylisted"] += 1
            continue
        cpp = find_pinned_cpp(addr)
        if cpp is None:
            if not args.include_unpinned:
                skipped_reasons["out_of_range"] += 1
                continue
            cpp = "[UNPINNED]"
        # Apply cross-game class-name substitutions (dc3 HamX -> rb3 BandX).
        # bindiff's dc3_name carries the dc3-side class name; our compiled-side
        # symbol carries the rb3-side class name. Substitute before storing.
        subbed_name, applied = substitute_dc3_class_names(name)
        if applied:
            n_class_substituted += 1
            for sub in applied:
                class_sub_breakdown[sub] = class_sub_breakdown.get(sub, 0) + 1
        name = subbed_name
        # If multiple bindiff records hit the same addr (rare but possible),
        # keep the higher-confidence one.
        prev = auto.get(norm)
        if prev is None or prev[1] < conf:
            auto[norm] = (name, conf, cpp)

    # Build merged output: metadata + manual + auto-gen (manual wins on key
    # collision). Sort 0x keys for stable output.
    merged = dict(metadata)  # comment fields first
    n_auto_kept = 0
    n_auto_overridden_by_manual = 0
    for norm, (name, conf, cpp) in auto.items():
        if norm in manual_norm:
            n_auto_overridden_by_manual += 1
            continue
        n_auto_kept += 1
        per_tu[cpp] = per_tu.get(cpp, 0) + 1

    # Re-emit manual entries with their original key casing — but drop any that
    # are denylisted (a hand-flagged bad guess takes precedence over a stale
    # manual entry, so a previously-mistaken rename can be retired cleanly).
    n_manual_denylisted = 0
    for norm, (orig_key, orig_val) in manual_norm.items():
        if norm in denylist:
            n_manual_denylisted += 1
            continue
        merged[orig_key] = orig_val

    # Emit auto entries with normalized "0xUPPERCASE" form, except where a
    # manual entry already claimed the address.
    for norm, (name, conf, cpp) in sorted(auto.items()):
        if norm in manual_norm:
            continue
        merged[norm] = name

    # Write output
    if args.dry_run:
        print(f"[gen_target_map] DRY RUN — would write {len(merged)} entries "
              f"to {map_path}", file=sys.stderr)
    else:
        # Preserve readable formatting (manual file was 4-space indented).
        with open(map_path, "w") as f:
            json.dump(merged, f, indent=4, sort_keys=False)
            f.write("\n")

    # Report
    n_metadata = len(metadata)
    n_manual = len(manual_norm)
    n_total_addrs = sum(1 for k in merged if k.lower().startswith("0x"))
    print(f"[gen_target_map] unified_id records:    {len(unified)}")
    print(f"[gen_target_map] include_unpinned:      {args.include_unpinned}")
    print(f"[gen_target_map] eligible auto-gen:     {len(auto)} "
          f"(after filters)")
    print(f"[gen_target_map] auto-gen kept:         {n_auto_kept}")
    print(f"[gen_target_map] overridden by manual:  {n_auto_overridden_by_manual}")
    print(f"[gen_target_map] manual entries:        {n_manual}"
          f" (denylisted/dropped: {n_manual_denylisted})")
    print(f"[gen_target_map] denylist size:         {len(denylist)}")
    print(f"[gen_target_map] comment/metadata keys: {n_metadata}")
    print(f"[gen_target_map] total entries in map:  {n_total_addrs} + "
          f"{n_metadata} metadata")
    print(f"[gen_target_map] skipped:")
    for reason, n in sorted(skipped_reasons.items(), key=lambda kv: -kv[1]):
        print(f"    {reason:20s}: {n}")
    print(f"[gen_target_map] class-name substitutions: bindiff={n_class_substituted}, "
          f"existing-rewritten={n_existing_substituted}")
    for sub, n in sorted(class_sub_breakdown.items(), key=lambda kv: -kv[1]):
        print(f"    {sub:32s}: {n}")
    print(f"[gen_target_map] auto-gen entries per TU (top 20):")
    for cpp, n in sorted(per_tu.items(), key=lambda kv: -kv[1])[:20]:
        print(f"    {cpp:30s}: {n}")


def _norm_addr_key(k):
    """Normalize address-string keys to uppercase '0xXXXXXXXX' form."""
    k = k.strip()
    if k.lower().startswith("0x"):
        k = k[2:]
    try:
        n = int(k, 16)
    except ValueError:
        return k
    return f"0x{n:08X}"


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    pe = sub.add_parser("extract")
    pe.add_argument("--asm-dir", default=DEFAULT_ASM)
    pe.add_argument("--out", default="fingerprints.json")
    pe.add_argument("--lo", type=lambda x: int(x, 0), default=0)
    pe.add_argument("--hi", type=lambda x: int(x, 0), default=0)
    pe.set_defaults(func=extract)

    pr = sub.add_parser("report")
    pr.add_argument("--index", default="fingerprints.json")
    pr.add_argument("--min-strings", type=int, default=2)
    pr.add_argument("--limit", type=int, default=40)
    pr.set_defaults(func=report)

    pa = sub.add_parser("autoid")
    pa.add_argument("--index", default="fingerprints.json")
    pa.add_argument("--out", default="autoid.json")
    pa.add_argument("--src", nargs="+", default=["../rb3/src", "../dc3-decomp/src"])
    pa.add_argument("--min-strings", type=int, default=2)
    pa.add_argument("--min-len", type=int, default=4)
    pa.add_argument("--limit", type=int, default=40)
    pa.set_defaults(func=autoid)

    pi = sub.add_parser("identify")
    pi.add_argument("query")
    pi.add_argument("--index", default="fingerprints.json")
    pi.add_argument("--src", nargs="+",
                    default=["../dc3-decomp/src", "../rb3/src"])
    pi.set_defaults(func=identify)

    pm = sub.add_parser("merge_bindiff",
        help="Join autoid string-attribution with cross-binary bindiff matches")
    pm.add_argument("--autoid", default="autoid.json")
    pm.add_argument("--bindiff", default="tools/bindiff_match.json")
    pm.add_argument("--dc3-map",
                    default="../dc3-decomp/orig/373307D9/ham_xbox_r.map")
    pm.add_argument("--dc3-objects",
                    default="../dc3-decomp/config/373307D9/objects.json")
    pm.add_argument("--out", default="unified_id.json")
    pm.set_defaults(func=merge_bindiff)

    pc = sub.add_parser("bindiff_clusters",
        help="Propose new splits.txt entries by clustering bindiff hits per dc3 .cpp")
    pc.add_argument("--unified", default="unified_id.json")
    pc.add_argument("--rb3-symbols", default=SYMS)
    pc.add_argument("--existing-splits",
                    default=os.path.join(os.path.dirname(__file__), "..",
                                         "config", "45410914", "splits.txt"))
    pc.add_argument("--out", default="proposed_splits_bindiff.txt")
    pc.add_argument("--min-density", type=float, default=0.03,
                    help="min cluster density (fraction of fns in span that "
                         "are bindiff hits; default 0.03 = 3%%)")
    pc.add_argument("--max-span", type=lambda x: int(x, 0),
                    default=256 * 1024,
                    help="max convex hull span in bytes (default 256KiB)")
    pc.add_argument("--min-hits", type=int, default=3,
                    help="min distinct bindiff hits per cluster (default 3)")
    pc.set_defaults(func=bindiff_clusters)

    pt = sub.add_parser("triangulate",
        help="Call-graph triangulation: identify anonymous rb3 fns by aligning "
             "bl call sequences of known-mapped functions against dc3 twins. "
             "Writes a NEW-only oracle (union with unified_id.json).")
    pt.add_argument("--unified", default="unified_id.json",
                    help="Anchor oracle (rb3 addr -> dc3 name); also the "
                         "NEW-only filter set (default: %(default)s)")
    pt.add_argument("--report", default=os.path.join(
                        os.path.dirname(__file__), "..", "build", "45410914",
                        "report.json"),
                    help="report.json (matched-fn count; informational)")
    pt.add_argument("--rb3-symbols", default=SYMS,
                    help="rb3 symbols.txt for function sizes")
    pt.add_argument("--rb3-asm", default=DEFAULT_ASM,
                    help="rb3 dtk-emitted asm dir (build/45410914/asm)")
    pt.add_argument("--dc3-asm", default=os.path.join(
                        os.path.dirname(__file__), "..", "..", "dc3-decomp",
                        "build", "373307D9", "asm"),
                    help="dc3 dtk-emitted asm dir (recursive)")
    pt.add_argument("--dc3-map",
                    default="../dc3-decomp/orig/373307D9/ham_xbox_r.map")
    pt.add_argument("--dc3-objects",
                    default="../dc3-decomp/config/373307D9/objects.json")
    pt.add_argument("--majority-frac", type=float, default=0.5,
                    help="min winner vote-fraction for a contested (majority) "
                         "proposal (default: %(default)s)")
    pt.add_argument("--out", default="unified_id_callgraph.json",
                    help="Output NEW-only oracle file (default: %(default)s)")
    pt.set_defaults(func=triangulate)

    prt = sub.add_parser("rtti",
        help="RTTI+vtable transitivity: walk rb3's non-standard X360 RTTI to "
             "recover all named vtables, join to dc3's ??_7CLASS@@6B@ vtables "
             "by class name, pair slots. Writes a NEW-only HIGH-tier oracle.")
    prt.add_argument("--unified", default="unified_id.json",
                     help="Anchor oracle + NEW-only filter set (default: %(default)s)")
    prt.add_argument("--rb3-exe", default=DEFAULT_RB3_EXE,
                     help="rb3 unxex'd PE for RTTI scan (default: %(default)s)")
    prt.add_argument("--rb3-symbols", default=SYMS,
                     help="rb3 symbols.txt for function sizes")
    prt.add_argument("--dc3-map", default=DEFAULT_DC3_MAP,
                     help="dc3 leaked .map (vtable RVAs) (default: %(default)s)")
    prt.add_argument("--dc3-exe", default=DEFAULT_DC3_EXE,
                     help="dc3 release PE (vtable contents) (default: %(default)s)")
    prt.add_argument("--dc3-objects", default=DEFAULT_DC3_OBJECTS)
    prt.add_argument("--out", default="unified_id_rtti.json",
                     help="HIGH-tier NEW-only oracle file (default: %(default)s)")
    prt.add_argument("--out-low", default="unified_id_rtti_low.json",
                     help="LOW-tier output (only written with --include-low)")
    prt.add_argument("--include-low", action="store_true",
                     help="Also emit the LOW tier (~41%% precision) to --out-low. "
                          "OFF by default; LOW must NOT be auto-merged.")
    prt.set_defaults(func=rtti)

    pvt = sub.add_parser("vtable",
        help="Vtable slot transitivity (no RTTI): pair jeff's heuristic "
             "vftable_<addr> rb3 vtables to dc3 vtables via known slot names, "
             "transfer the rest. Subsumed by `rtti`; orthogonal fallback.")
    pvt.add_argument("--unified", default="unified_id.json",
                     help="Anchor oracle + NEW-only filter set (default: %(default)s)")
    pvt.add_argument("--rb3-asm", default=DEFAULT_ASM,
                     help="rb3 dtk-emitted asm dir (vftable_ in *_rdata.s)")
    pvt.add_argument("--rb3-symbols", default=SYMS,
                     help="rb3 symbols.txt for function sizes")
    pvt.add_argument("--dc3-map", default=DEFAULT_DC3_MAP,
                     help="dc3 leaked .map (vtable RVAs) (default: %(default)s)")
    pvt.add_argument("--dc3-exe", default=DEFAULT_DC3_EXE,
                     help="dc3 release PE (vtable contents) (default: %(default)s)")
    pvt.add_argument("--dc3-objects", default=DEFAULT_DC3_OBJECTS)
    pvt.add_argument("--out", default="unified_id_vtable.json",
                     help="NEW-only oracle file (default: %(default)s)")
    pvt.set_defaults(func=vtable)

    pgt = sub.add_parser("gen_target_map",
        help="Auto-populate scripts/target_symbol_map.json from unified_id.json"
             " (filter: bindiff source, confidence >= 0.95, addr in pinned"
             " .text range). Manual entries preserved on collision.")
    pgt.add_argument("--unified", default="unified_id.json",
                     help="Input unified_id.json (default: %(default)s)")
    pgt.add_argument("--splits",
                     default=os.path.join(os.path.dirname(__file__), "..",
                                          "config", "45410914", "splits.txt"),
                     help="splits.txt with pinned .text ranges")
    pgt.add_argument("--map",
                     default=os.path.join(os.path.dirname(__file__), "..",
                                          "scripts", "target_symbol_map.json"),
                     help="Target symbol map JSON (default: %(default)s)")
    pgt.add_argument("--min-confidence", type=float, default=0.95,
                     help="min bindiff confidence (default: %(default)s)")
    pgt.add_argument("--min-similarity", type=float, default=0.96,
                     help="min bindiff similarity; drops low-similarity "
                          "flowgraph-shape collisions (default: %(default)s)")
    pgt.add_argument("--include-unpinned", action="store_true", default=True,
                     help="Also emit renames for high-confidence callees that "
                          "live OUTSIDE pinned .text ranges, so cross-TU `bl` "
                          "relocations pair (default: on, for scaffolding)")
    pgt.add_argument("--pinned-only", dest="include_unpinned",
                     action="store_false",
                     help="Restrict to callees inside pinned ranges (old behavior)")
    pgt.add_argument("--dry-run", action="store_true",
                     help="Compute and report counts but don't write the file")
    pgt.set_defaults(func=gen_target_map)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
