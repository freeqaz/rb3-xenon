#!/usr/bin/env python3
"""Census the MILO_* debug emissions that RETAIL ACTUALLY KEPT.  (lane MILOKEEP-1)

WHY THIS EXISTS
---------------
Our tree strips the whole MILO_* family (the macros are `#ifdef HX_NATIVE`, which
the match build never defines).  Several lanes have asked, per-site and by hand,
"did retail keep THIS one?" and at least one got the answer wrong in both
directions.  This answers it for the WHOLE BINARY, without the metric, without
the symbol map, and without a disassembler.

THE INSTRUMENT
--------------
Every MILO_* emitter -- MILO_WARN/NOTIFY/FAIL/LOG and the _ONCE variants -- ends
up calling a member of the single global `Debug TheDebug`:

    MILO_LOG    -> TheDebug << MakeString(...)      -> TextStream::operator<<
    MILO_NOTIFY -> TheDebugNotifier << MakeString(...) -> TheDebug.Notify(...)
    MILO_WARN   -> TheDebugWarner   << ...          -> TheDebug.Warn(...)
    MILO_FAIL   -> TheDebugFailer   << ...          -> TheDebug.Fail(...)

The emitter classes are EMPTY one-line wrappers, so /Ob2 inlines them away and
counting callers of `DebugFailer::operator<<` proves nothing (it is 0 whether or
not sites survive -- this is what misled an earlier reading).  References to the
TheDebug GLOBAL are immune to that: they are the complete superset of surviving
emission sites, and they are found by scanning .text for the lis/addi pair that
materialises its address.

TWO TRAPS THIS TOOL IS BUILT AROUND, both of which have cost real yield here:
  * A surviving STRING is not a surviving CALL SITE.  A named global materialises
    its literal with no call behind it (Dir.cpp's kNotObjectMsg), and so does a
    stripped-eval sink that still takes the format string as a parameter (retail
    has an out-of-line MiloStripEval).  Presence of a literal OVER-COUNTS.
  * A raw substring search over-counts too: '--------\\n' hits inside
    '--------------------\\n'.  Literals are matched NUL-delimited.

USAGE
    python3 tools/milo_emission_census.py sinks    # binary side (authoritative)
    python3 tools/milo_emission_census.py census   # source side + adjudication
    python3 tools/milo_emission_census.py selftest # prove the scan can fire/fail
"""
import os, re, sys, json, struct, bisect, collections

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(REPO, "orig/45410914/band.exe")
THE_DEBUG = 0x82CC9874          # ?TheDebug@@3VDebug@@A
STREAM_OP = 0x827C3BB0          # TextStream::operator<< (ICF: Symbol/const char*)
DEBUG_NOTIFY = 0x8250F538       # Debug::Notify  (MainThread/CritSec/Wait body)
DEBUG_FAIL = 0x8250F6D0         # ?Fail@Debug@@QAAXPBDPAX@Z
DEBUG_EXIT = 0x8250F820         # Debug::Exit (calls StopLog)

MACROS = ["MILO_NOTIFY_ONCE", "MILO_WARN_ONCE", "MILO_LOG_ONCE", "MILO_PRINT_ONCE",
          "MILO_NOTIFY_BETA", "MILO_FAIL_DTA", "MILO_NOTIFY", "MILO_WARN",
          "MILO_FAIL", "MILO_LOG"]          # longest-first: _ONCE must win
MACRO_RE = re.compile(r"\b(" + "|".join(MACROS) + r")\s*\(")
STRLIT = re.compile(r'"((?:[^"\\]|\\.)*)"')


class Bin:
    def __init__(self, path=EXE):
        self.d = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.d, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.d, pe + 6)[0]
        optsz = struct.unpack_from("<H", self.d, pe + 20)[0]
        opt = pe + 24
        self.imagebase = struct.unpack_from("<I", self.d, opt + 28)[0]
        self.sections = []
        for i in range(nsec):
            o = opt + optsz + i * 40
            nm = self.d[o:o + 8].rstrip(b"\0").decode("latin1")
            vs, va, rs, rp = struct.unpack_from("<IIII", self.d, o + 8)
            self.sections.append(dict(name=nm, va=self.imagebase + va, vsz=vs,
                                      raw=rp, rsz=rs))

    def sec(self, n):
        return next(s for s in self.sections if s["name"] == n)

    def text(self):
        s = self.sec(".text")
        return s["va"], self.d[s["raw"]:s["raw"] + min(s["vsz"], s["rsz"])]

    def pdata_begins(self):
        s = self.sec(".pdata")                      # X360 .pdata is BIG-ENDIAN
        blob = self.d[s["raw"]:s["raw"] + min(s["vsz"], s["rsz"])]
        return [struct.unpack_from(">II", blob, i)[0]
                for i in range(0, len(blob) - 7, 8)
                if struct.unpack_from(">II", blob, i)[0]]

    def bl_sites(self):
        """Every bl in .text: insn & 0xFC000003 == 0x48000001."""
        base, blob = self.text()
        out = []
        for i in range(0, len(blob) - 3, 4):
            w = struct.unpack_from(">I", blob, i)[0]
            if (w & 0xFC000003) == 0x48000001:
                li = w & 0x03FFFFFC
                if li & 0x02000000:
                    li -= 0x04000000
                out.append((base + i, (base + i + li) & 0xFFFFFFFF))
        return out

    def refs_to(self, targets, window=12):
        """lis+addi / lis+ori materialisation of any addr in `targets`."""
        base, blob = self.text()
        out = collections.defaultdict(list)
        hi = {}
        for i in range(len(blob) // 4):
            w = struct.unpack_from(">I", blob, i * 4)[0]
            op, rt, ra = w >> 26, (w >> 21) & 31, (w >> 16) & 31
            if op == 15 and ra == 0:
                hi[rt] = ((w & 0xFFFF) << 16, i)
            elif op in (14, 24) and ra in hi:
                hv, hidx = hi[ra]
                if i - hidx <= window:
                    if op == 14:
                        s = w & 0xFFFF
                        v = hv + (s - 0x10000 if s & 0x8000 else s)
                    else:
                        v = hv | (w & 0xFFFF)
                    if (v & 0xFFFFFFFF) in targets:
                        out[v & 0xFFFFFFFF].append(base + i * 4)
        return out

    def exact_literal(self, pat):
        """NUL-delimited occurrences only -- substring hits are rejected."""
        hits = []
        for s in self.sections:
            if not s["rsz"]:
                continue
            blob = self.d[s["raw"]:s["raw"] + min(s["vsz"], s["rsz"])]
            i = blob.find(pat)
            while i >= 0:
                if (i == 0 or blob[i - 1] == 0) and \
                   i + len(pat) < len(blob) and blob[i + len(pat)] == 0:
                    hits.append((s["name"], s["va"] + i))
                i = blob.find(pat, i + 1)
        return hits


def load_map():
    p = os.path.join(REPO, "scripts/target_symbol_map.json")
    m = json.load(open(p))
    return {int(k, 16): v for k, v in m.items()
            if re.fullmatch(r"0x[0-9a-fA-F]+", k) and isinstance(v, str)}


class Funcs:
    def __init__(self, b, sym):
        s = b.sec(".text")
        lo, hi = s["va"], s["va"] + s["vsz"]
        st = set(a for a in b.pdata_begins() if lo <= a < hi)
        st |= set(a for a in sym if lo <= a < hi)
        self.starts, self.sym = sorted(st), sym

    def owner(self, a):
        i = bisect.bisect_right(self.starts, a) - 1
        return self.starts[i] if i >= 0 else None

    def name(self, a):
        return self.sym.get(a, "(unnamed %08x)" % a) if a else "?"


def unescape(s):
    out, i = bytearray(), 0
    tbl = {"n": 10, "t": 9, "r": 13, "0": 0, "\\": 92, '"': 34, "'": 39}
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s) and s[i + 1] in tbl:
            out.append(tbl[s[i + 1]]); i += 2; continue
        if s[i] == "\\" and i + 1 < len(s):
            out.append(ord(s[i + 1])); i += 2; continue
        out.append(ord(s[i]) if ord(s[i]) < 256 else 63); i += 1
    return bytes(out)


def collect_sites():
    """Every MILO_* invocation in src/, with its literal format string."""
    sites = []
    for root, _d, files in os.walk(os.path.join(REPO, "src")):
        for f in files:
            if not f.endswith((".cpp", ".h", ".c", ".hpp")):
                continue
            p = os.path.join(root, f)
            txt = open(p, encoding="utf-8", errors="replace").read()
            txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
            txt = re.sub(r"//[^\n]*", "", txt)
            for m in MACRO_RE.finditer(txt):
                ls = txt.rfind("\n", 0, m.start()) + 1
                if txt[ls:m.start()].strip().startswith("#define"):
                    continue                     # the definitions, not uses
                depth, i, instr, args = 0, m.end() - 1, None, None
                while i < len(txt):
                    c = txt[i]
                    if instr:
                        if c == "\\":
                            i += 2; continue
                        if c == instr:
                            instr = None
                    elif c in "\"'":
                        instr = c
                    elif c == "(":
                        depth += 1
                    elif c == ")":
                        depth -= 1
                        if depth == 0:
                            args = txt[m.end():i]; break
                    i += 1
                fmt, rest, parts = None, (args or "").strip(), []
                while True:
                    mm = STRLIT.match(rest)
                    if not mm:
                        break
                    parts.append(mm.group(1)); rest = rest[mm.end():].lstrip()
                if parts:
                    fmt = "".join(parts)
                sites.append(dict(file=os.path.relpath(p, REPO), macro=m.group(1),
                                  line=txt.count("\n", 0, m.start()) + 1, fmt=fmt))
    return sites


def cmd_sinks():
    b, sym = Bin(), load_map()
    F = Funcs(b, sym)
    bl = b.bl_sites()
    byfn = collections.defaultdict(list)
    for s, t in bl:
        byfn[F.owner(s)].append(t)
    ms = {a for a, n in sym.items() if n.startswith("??$MakeString@")}
    refs = b.refs_to({THE_DEBUG})[THE_DEBUG]
    fns = sorted({F.owner(s) for s in refs})
    print("references to TheDebug (%08x): %d in %d functions" %
          (THE_DEBUG, len(refs), len(fns)))
    cat, emis = collections.Counter(), []
    for o in fns:
        c = byfn.get(o, [])
        hm, hs = any(t in ms for t in c), STREAM_OP in c
        if hm and hs:
            k = "EMISSION (MakeString + TextStream::operator<<)"
        elif DEBUG_NOTIFY in c:
            k = "EMISSION (Debug::Notify)"
        elif DEBUG_FAIL in c:
            k = "EMISSION (Debug::Fail)"
        elif hs:
            k = "stream-only (TheDebug << literal)"
        elif DEBUG_EXIT in c:
            k = "Debug::Exit (API use)"
        else:
            k = "Debug API use (no emission)"
        cat[k] += 1
        if k.startswith("EMISSION"):
            emis.append((k, o, F.name(o)))
    for k, v in cat.most_common():
        print("   %-48s %d" % (k, v))
    print("\nSURVIVING FORMATTED EMISSIONS (%d):" % len(emis))
    for k, o, n in emis:
        print("   %08x %-56s %s" % (o, n[:56], k))
    print("\nsink caller counts (whole binary):")
    tc = collections.Counter(t for _, t in bl)
    for nm, a in [("Debug::Notify", DEBUG_NOTIFY), ("Debug::Fail", DEBUG_FAIL),
                  ("Debug::Exit", DEBUG_EXIT)]:
        who = [F.name(F.owner(s)) for s, t in bl if t == a]
        print("   %-14s %08x  %d  %s" % (nm, a, tc.get(a, 0), who))


def cmd_census():
    b, sym = Bin(), load_map()
    F = Funcs(b, sym)
    byfn = collections.defaultdict(list)
    for s, t in b.bl_sites():
        byfn[F.owner(s)].append(t)
    ms = {a for a, n in sym.items() if n.startswith("??$MakeString@")}
    sites = collect_sites()
    wf = [s for s in sites if s["fmt"]]
    print("MILO_* invocation sites in src/ : %d" % len(sites))
    print("  with a literal format string  : %d  (CHECKABLE)" % len(wf))
    print("  non-literal first arg         : %d  (NOT CHECKABLE)" % (len(sites) - len(wf)))
    bym = collections.Counter(s["macro"] for s in sites)
    for m in MACROS:
        if bym.get(m):
            print("     %-18s %5d" % (m, bym[m]))
    uniq = {}
    for s in wf:
        uniq.setdefault(s["fmt"], []).append(s)
    checkable = {f: ss for f, ss in uniq.items() if len(unescape(f)) >= 6}
    print("\ndistinct format strings (len>=6): %d" % len(checkable))
    present = {f: (b.exact_literal(unescape(f)), ss)
               for f, ss in checkable.items()}
    present = {f: v for f, v in present.items() if v[0]}
    print("PRESENT in retail (exact, NUL-delimited): %d" % len(present))
    kept = 0
    for f, (h, ss) in sorted(present.items()):
        addrs = {a for _s, a in h}
        rs = [r for a in addrs for r in b.refs_to(addrs).get(a, [])]
        owners = sorted({F.owner(r) for r in rs})
        emit = any(any(t in ms for t in byfn.get(o, [])) and
                   STREAM_OP in byfn.get(o, []) for o in owners)
        kept += bool(emit)
        print("\n  %-56s" % repr(f)[:56])
        print("    at %s | .text refs=%d" %
              (", ".join("%s@%08x" % x for x in h), len(rs)))
        for o in owners:
            print("      owner %08x %s" % (o, F.name(o)[:60]))
        print("    VERDICT: %s" % ("KEPT EMISSION" if emit else
                                   "string only -- NO emission behind it"))
        for s in ss[:3]:
            print("      src %-16s %s:%d" % (s["macro"], s["file"], s["line"]))
    print("\nkept-emission strings=%d  string-only=%d" % (kept, len(present) - kept))


def cmd_selftest():
    """A census that cannot fire is worse than none.  Prove both directions."""
    b = Bin()
    ok = True
    n = len(b.bl_sites())
    print("[%s] bl scan finds %d sites in .text" % ("PASS" if n > 100000 else "FAIL", n))
    ok &= n > 100000
    r = b.refs_to({THE_DEBUG}).get(THE_DEBUG, [])
    print("[%s] refs_to fires on TheDebug: %d" % ("PASS" if r else "FAIL", len(r)))
    ok &= bool(r)
    counts = {w: len(b.refs_to({THE_DEBUG}, window=w).get(THE_DEBUG, []))
              for w in (4, 12, 48)}
    plateau = len(set(counts.values())) == 1
    print("[%s] window-sensitivity plateau %s" % ("PASS" if plateau else "FAIL", counts))
    ok &= plateau
    for bogus in (0x82CC9878, 0x829ABCDE):
        z = b.refs_to({bogus}).get(bogus, [])
        print("[%s] negative control %08x -> %d refs" %
              ("PASS" if not z else "FAIL", bogus, len(z)))
        ok &= not z
    pos = b.exact_literal(b"Could not find %s in dir \"%s\"")
    print("[%s] exact_literal fires on a known-present string: %d" %
          ("PASS" if pos else "FAIL", len(pos)))
    ok &= bool(pos)
    neg = b.exact_literal(b"Cannot render to texture")
    print("[%s] exact_literal correctly absent for the refuted notify: %d" %
          ("PASS" if not neg else "FAIL", len(neg)))
    ok &= not neg
    sub = b.exact_literal(b"--------")
    print("[note] substring-prone literal '--------' exact hits: %d "
          "(a raw .count() would over-report)" % len(sub))
    print("\nSELFTEST:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "sinks"
    sys.exit({"sinks": cmd_sinks, "census": cmd_census,
              "selftest": cmd_selftest}[cmd]() or 0)
