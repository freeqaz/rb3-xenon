#!/usr/bin/env python3
"""
localstatic_symbol_scan.py  -- REUSABLE scanner for the local-static-Symbol-accessor vein.

Finds small, still-anonymous (fn_XXXXXXXX, UNMAPPED) standalone accessors whose body is the
canonical "function-local `static Symbol foo("name");` + `return member == foo;`" shape that
retail keeps OUT-OF-LINE but OUR source inlined (by comparing a member to a synthetic Symbols2.h
global).  Adding the local static back to our source restores the out-of-line function so it
byte-matches the retail fn_ -> +1 strict once mapped.

Canonical target-asm shape (exemplar fn_8258C7D8 = Campaign::CanSaveSetlists, 0x60):
    mflr / bl __savegprlr_N / stwu                       ; frame
    lis/lwz GUARD ; clrlwi. ; bne .Ldone                 ; local-static guard test
    ori GUARD,0x1 ; stw GUARD                            ; set guard on first call
    lis/addi r4,&STRING ; mr r3,&static ; bl <Symbol()>  ; first-call Symbol ctor(const char*)
  .Ldone:
    lwz member ; lwz static ; subf ; cntlzw ; extrwi ; ret ; member == Symbol -> bool

The ctor is Symbol::Symbol(const char*) = ??0Symbol@@QAA@PBD@Z at retail fn_8279B788.

Read-only.  Writes only the candidates JSON (default ~/tmp/ls-vein/candidates.json).

Usage:
    python3 scripts/harvest/localstatic_symbol_scan.py            # scan, print table, write json
    python3 scripts/harvest/localstatic_symbol_scan.py --json OUT # custom output path
    python3 scripts/harvest/localstatic_symbol_scan.py --all      # don't cap the printed game table
"""
import argparse, json, os, re, sys, subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]           # .../rb3-xenon
ASM  = REPO / "build/45410914/asm"
SYMBOL_CTOR = "fn_8279B788"                            # Symbol::Symbol(const char*)
MAP_JSON = REPO / "scripts/target_symbol_map.json"
OBJECTS  = REPO / "config/45410914/objects.json"
SRC_DIRS = ["src/band3", "src/network", "src/system", "src"]
SYMBOLS_HDRS = [REPO / f"src/system/utl/Symbols{n}.h" for n in ("", "2", "3", "4")]
EXEMPLAR = "fn_8258C7D8"

def load_symbols2():
    """set of names declared `extern Symbol <name>;` across Symbols{,2,3,4}.h — the synthetic
    globals our source compares against (the inline-accessor lever hinges on these)."""
    s = set()
    for hdr in SYMBOLS_HDRS:
        if hdr.exists():
            for ln in hdr.read_text(errors="replace").splitlines():
                m = re.match(r"\s*extern\s+Symbol\s+(\w+)\s*;", ln)
                if m:
                    s.add(m.group(1))
    return s

# ---------------------------------------------------------------- rdata string map
def build_string_map():
    """VA(int) -> string text, from dtk-emitted rdata .s `.string` directives (authoritative:
    these bytes come straight out of the target default.xex)."""
    m = {}
    comment_va = re.compile(r"^#\s+\.rdata:\S+\s+\|\s+0x([0-9A-Fa-f]{8})\s+\|")
    strline = re.compile(r'^\s*\.string\s+"(.*)"\s*$')
    for s in ASM.rglob("*.s"):
        txt = s.read_text(errors="replace").splitlines()
        cur_va = None
        for ln in txt:
            mc = comment_va.match(ln)
            if mc:
                cur_va = int(mc.group(1), 16)
                continue
            ms = strline.match(ln)
            if ms and cur_va is not None:
                m[cur_va] = clean_str(ms.group(1))
                cur_va = None
    return m

def clean_str(raw):
    """Decode dtk .string escapes (octal \\NNN, \\t, \\n, \\", \\\\) and truncate at the first
    NUL — a fixed-size .rdata buffer prints trailing \\000 padding we don't want."""
    out = []
    i = 0
    while i < len(raw):
        c = raw[i]
        if c == "\\" and i + 1 < len(raw):
            n = raw[i + 1]
            if n in "01234567":                     # octal escape \NNN
                oct_digits = raw[i + 1:i + 4]
                m = re.match(r"[0-7]{1,3}", oct_digits)
                val = int(m.group(0), 8)
                out.append(chr(val)); i += 1 + len(m.group(0)); continue
            mp = {"n": "\n", "t": "\t", '"': '"', "\\": "\\"}
            out.append(mp.get(n, n)); i += 2; continue
        out.append(c); i += 1
    s = "".join(out)
    nul = s.find("\x00")
    if nul >= 0:
        s = s[:nul]
    return s

# ---------------------------------------------------------------- category map (game vs engine)
def build_category_map():
    """asm-basename-stem -> progress_category ('game'|'engine'|'network')."""
    d = json.loads(OBJECTS.read_text())
    stem2cat = {}
    for grp, gd in d.items():
        cat = gd.get("progress_category", "unknown")
        for k in gd.get("objects", {}):
            stem = Path(k).stem            # e.g. system/bandobj/BandDirector.cpp -> BandDirector
            # first writer wins unless a game category later overrides engine
            prev = stem2cat.get(stem)
            if prev is None or (prev == "engine" and cat in ("game", "network")):
                stem2cat[stem] = cat
    return stem2cat

def unit_category(asm_path, stem2cat):
    rel = str(asm_path.relative_to(ASM))
    # nested subtree paths are already authoritative
    if rel.startswith("band3/"):   return "game"
    if rel.startswith("network/"): return "network"
    if rel.startswith("system/"):  return "engine"
    return stem2cat.get(asm_path.stem, "unknown")

# ---------------------------------------------------------------- .fn block parsing
FN_START = re.compile(r"^\.fn\s+(\S+?),")
FN_END   = re.compile(r"^\.endfn")
# preceding size comment: "# .text:0x400 | 0x8258C7D8 | size: 0x60"
SIZE_CMT = re.compile(r"^#\s+\.text:\S+\s+\|\s+0x([0-9A-Fa-f]{8})\s+\|\s+size:\s+0x([0-9A-Fa-f]+)")

def iter_fn_blocks(path):
    lines = path.read_text(errors="replace").splitlines()
    i = 0
    last_size = None
    last_va = None
    while i < len(lines):
        ln = lines[i]
        mc = SIZE_CMT.match(ln)
        if mc:
            last_va = int(mc.group(1), 16)
            last_size = int(mc.group(2), 16)
            i += 1; continue
        mf = FN_START.match(ln)
        if mf:
            name = mf.group(1)
            body = [ln]
            j = i + 1
            while j < len(lines) and not FN_END.match(lines[j]):
                body.append(lines[j]); j += 1
            if j < len(lines):
                body.append(lines[j])
            yield name, last_va, last_size, body
            i = j + 1
            last_size = last_va = None
            continue
        i += 1

# ---------------------------------------------------------------- shape analysis
CTOR_CALL = re.compile(r"\bbl\s+" + re.escape(SYMBOL_CTOR) + r"\b")
# addi r4, rX, lbl_XXXXXXXX[+0xNN]@l   (immediate address-of-string into r4)
R4_ADDI   = re.compile(r"\baddi\s+r4,\s*r\d+,\s*lbl_([0-9A-Fa-f]{8})(?:\+0x([0-9A-Fa-f]+))?@l")
R4_LWZ    = re.compile(r"\blwz\s+r4,\s*lbl_[0-9A-Fa-f]{8}")
GUARD_ORI = re.compile(r"\bori\s+r\d+,\s*r\d+,\s*0x1\b")
GUARD_CLR = re.compile(r"\b(clrlwi\.|rlwinm\.\s+r\d+,\s*r\d+,\s*0,\s*31,\s*31)")
CMP_CNTLZW= re.compile(r"\bcntlzw\b")
CMP_EXTR  = re.compile(r"\b(extrwi|rlwinm)\b")

def code_lines(body):
    """asm instruction lines only (strip the leading '/* .. */' comment column)."""
    out = []
    for ln in body:
        # a real instruction line looks like: /* VA FILEOFF BYTES */\tmnemonic ops
        m = re.match(r"^/\*.*?\*/\s+(.*)$", ln)
        if m:
            out.append(m.group(1).strip())
        else:
            # labels / directives like .fn/.endfn/.Lxxx:
            out.append(ln.strip())
    return out

def analyze(body, strmap):
    cl = code_lines(body)
    joined = "\n".join(cl)
    ctor_count = len(CTOR_CALL.findall(joined))
    has_guard  = bool(GUARD_ORI.search(joined)) and bool(GUARD_CLR.search(joined))
    has_cmp    = bool(CMP_CNTLZW.search(joined)) and bool(CMP_EXTR.search(joined))

    # collect string args: nearest preceding `addi r4, rX, lbl@l` before each ctor call
    strings = []
    string_vas = []
    ptr_arg = False
    for idx, line in enumerate(cl):
        if CTOR_CALL.search(line):
            # scan backwards up to 12 instrs for the r4 source
            found = None
            for k in range(idx - 1, max(-1, idx - 13), -1):
                ma = R4_ADDI.search(cl[k])
                if ma:
                    va = int(ma.group(1), 16) + (int(ma.group(2), 16) if ma.group(2) else 0)
                    found = va
                    break
                if R4_LWZ.search(cl[k]):
                    ptr_arg = True
                    break
                if re.search(r"\bmr\s+r4,", cl[k]):
                    break
            if found is not None:
                string_vas.append(found)
                strings.append(strmap.get(found))
    # dedupe preserving order
    seen = set(); uv = []; us = []
    for va, st in zip(string_vas, strings):
        if va in seen: continue
        seen.add(va); uv.append(va); us.append(st)
    return dict(ctor_count=ctor_count, has_guard=has_guard, has_cmp=has_cmp,
                string_vas=uv, strings=us, ptr_arg=ptr_arg)

# ---------------------------------------------------------------- source cross-reference
def src_grep(sym, unit_stem=None):
    """Search our source for a member==<sym> comparison (the inlined accessor).
    Collects all hits, then prefers one whose source-file stem matches the unit.
    Returns dict(file,line,code,method_sig) best-effort or None."""
    pats = [rf"==\s*{re.escape(sym)}\b", rf"\b{re.escape(sym)}\s*=="]
    hits = []
    for pat in pats:
        try:
            r = subprocess.run(["grep", "-rnE", "--include=*.cpp", "--include=*.h",
                                pat, str(REPO / "src")],
                               capture_output=True, text=True, timeout=90)
        except Exception:
            continue
        for hit in r.stdout.splitlines():
            m = re.match(r"^(.*?):(\d+):(.*)$", hit)
            if not m:
                continue
            fpath, lno, content = m.group(1), int(m.group(2)), m.group(3)
            c = content.strip()
            if "extern Symbol" in c or c.startswith("Symbol "):
                continue                                  # skip the Symbols2 decl itself
            # reject comparisons whose OTHER operand isn't a real member/accessor:
            # `award == gNullStr`, `rank == 0`, `x == Symbol()` are local vars, not the lever.
            other = re.sub(rf"\b{re.escape(sym)}\b", "", c)
            if re.search(r"==\s*(0|gNullStr|Symbol\(\)|\"\")", other) or \
               re.search(r"(gNullStr|Symbol\(\)|\"\")\s*==", other):
                continue
            hits.append((fpath, lno, c))
    if not hits:
        return None
    # prefer a hit whose file stem matches the asm unit; then .cpp over .h; then first
    def score(h):
        stem = Path(h[0]).stem
        return (0 if (unit_stem and stem == unit_stem) else 1,
                0 if h[0].endswith(".cpp") else 1)
    hits.sort(key=score)
    fpath, lno, content = hits[0]
    sig = enclosing_sig(Path(fpath), lno)
    return dict(file=os.path.relpath(fpath, REPO), line=lno, code=content, method_sig=sig)

def enclosing_sig(path, lno):
    try:
        lines = path.read_text(errors="replace").splitlines()
    except Exception:
        return None
    # walk up to nearest line that looks like a function definition head
    for k in range(lno - 1, max(-1, lno - 40), -1):
        s = lines[k].strip()
        if "::" in s and "(" in s and not s.startswith(("//", "*", "/*")) and "==" not in s:
            return s.rstrip("{ ").strip()
        # one-liner accessor inside class body: `bool CanFoo() const { return x == sym; }`
        if re.match(r"^[A-Za-z_].*\b[A-Za-z_]\w*\s*\([^;]*\)\s*(const)?\s*\{", s) and "==" in s:
            return s.split("{")[0].strip()
    return None

# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default=os.path.expanduser("~/tmp/ls-vein/candidates.json"))
    ap.add_argument("--all", action="store_true", help="print full game table (no 40 cap)")
    args = ap.parse_args()

    print("[*] building rdata string map ...", file=sys.stderr)
    strmap = build_string_map()
    print(f"    {len(strmap)} strings", file=sys.stderr)
    stem2cat = build_category_map()
    mapped = {k.lower() for k in json.loads(MAP_JSON.read_text()).keys()}   # 0x.... identified
    sym2 = load_symbols2()
    print(f"[*] {len(mapped)} already-identified (mapped) fn addresses; "
          f"{len(sym2)} Symbols2 globals", file=sys.stderr)

    total_ctor_sites = 0          # unmapped fn_ blocks w/ >=1 Symbol-ctor call (pre size/shape)
    candidates = []

    for s in sorted(ASM.rglob("*.s")):
        rel = str(s.relative_to(ASM))
        if rel.startswith("auto_") or "_rdata" in rel or "_pdata" in rel:
            continue
        cat = unit_category(s, stem2cat)
        for name, va, size, body in iter_fn_blocks(s):
            if not re.match(r"^fn_[0-9A-Fa-f]{8}$", name):
                continue                                   # only anonymous/unmapped names
            va_key = "0x" + name[3:].lower()
            if va_key in mapped:
                continue                                   # already identified -> excludes exemplar
            if SYMBOL_CTOR not in "\n".join(body):
                continue
            total_ctor_sites += 1
            if size is None or size > 0x140:
                continue
            info = analyze(body, strmap)
            if info["ctor_count"] < 1 or not info["has_guard"]:
                continue
            # exclude big handler chains defensively
            if info["ctor_count"] > 4:
                continue

            resolved = [x for x in info["strings"] if x]
            unit_stem = Path(s).stem
            # only Symbols2 globals can be the inline-accessor lever target
            lever_syms = [x for x in resolved if x in sym2]
            xref = None
            for st in lever_syms:
                xref = src_grep(st, unit_stem)
                if xref:
                    break

            # confidence
            has_str = len(resolved) > 0
            xref_unit_match = bool(xref) and (Path(xref["file"]).stem == unit_stem)
            if xref and has_str and cat in ("game", "network"):
                # a real inlined member==Symbols2-global accessor in our source = the lever.
                # cross-unit xref (source file != asm unit) is plausible but unverified -> medium.
                conf = "high" if xref_unit_match else "medium"
            elif xref and has_str:
                conf = "medium" if xref_unit_match else "low"   # engine, deprioritized
            else:
                conf = "low"                                     # identification-only

            candidates.append(dict(
                fn_addr="0x" + name[3:].upper(),
                size="0x%X" % size,
                unit=rel[:-2],                              # strip .s
                category=cat,
                ctor_count=info["ctor_count"],
                has_guard=info["has_guard"],
                has_compare_return=info["has_cmp"],
                ptr_arg=info["ptr_arg"],
                symbols=resolved,
                symbols2_globals=lever_syms,
                symbol_vas=["0x%08X" % v for v in info["string_vas"]],
                source_file=(xref or {}).get("file"),
                method_signature_src=(xref or {}).get("method_sig"),
                method_demangled=None,
                source_match_code=(xref or {}).get("code"),
                xref_unit_match=xref_unit_match,
                confidence=conf,
            ))

    # rank: game/network before engine, then confidence, then has compare-return, then size
    catrank = {"game": 0, "network": 0, "unknown": 1, "engine": 2}
    confrank = {"high": 0, "medium": 1, "low": 2}
    candidates.sort(key=lambda c: (catrank.get(c["category"], 3),
                                   confrank[c["confidence"]],
                                   0 if c["symbols"] else 1,
                                   0 if c["has_compare_return"] else 1,
                                   int(c["size"], 16)))

    game = [c for c in candidates if c["category"] in ("game", "network")]
    eng  = [c for c in candidates if c["category"] not in ("game", "network")]

    out = dict(
        symbol_ctor=SYMBOL_CTOR,
        total_ctor_sites=total_ctor_sites,
        total_candidates=len(candidates),
        game_candidates=len(game),
        engine_candidates=len(eng),
        candidates_game=game,
        candidates_engine=eng,
    )
    Path(os.path.dirname(args.json)).mkdir(parents=True, exist_ok=True)
    Path(args.json).write_text(json.dumps(out, indent=2))

    # ---- ranked table (game first) ----
    def row(c):
        syms = ",".join(s for s in c["symbols"]) or ("<ptr-arg>" if c["ptr_arg"] else "<none>")
        src = c["source_file"] or "-"
        return (f'{c["fn_addr"]}  {c["size"]:>6}  {c["confidence"]:<6} {c["category"]:<7} '
                f'{c["unit"]:<40.40} {syms:<32.32} {src}')
    print("\n=== GAME/NETWORK candidates (ranked) ===")
    print(f'{"fn_addr":<12}{"size":>6}  {"conf":<6} {"cat":<7} {"unit":<40} {"symbols":<32} source')
    shown = game if args.all else game[:40]
    for c in shown:
        print(row(c))
    print(f"\n[game/network total {len(game)}; showing {len(shown)}]")
    print(f"[engine total {len(eng)} (deprioritized)]")
    print(f"[total_ctor_sites (unmapped fn_ w/ ctor call, pre size/shape) = {total_ctor_sites}]")
    print(f"[wrote {args.json}]")

if __name__ == "__main__":
    main()
