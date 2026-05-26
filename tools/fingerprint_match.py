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
import sys

DEFAULT_ASM = os.path.join(os.path.dirname(__file__), "..", "build", "45410914", "asm")
SYMS = os.path.join(os.path.dirname(__file__), "..", "config", "45410914", "symbols.txt")

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

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
