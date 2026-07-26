#!/usr/bin/env python3
"""localstatic_symbol_audit.py — ground-truth audit of the local-static-Symbol
function family in target_symbol_map.json.

WHY THIS EXISTS
---------------
`Foo::StaticClassName()` (OBJ_CLASSNAME) and `FooMsg::Type()` (DECLARE_MESSAGE)
both compile to the SAME 22-instruction body:

    mflr r12; bl __savegprlr_29; subi r31,r1,0x70; stwu r1,-0x70(r1)
    lis r10,GUARD@ha; lis r11,SLOT@ha; mr r29,r3; addi r30,r11,SLOT@l
    lwz r11,GUARD@l(r10); clrlwi. r9,r11,31; bne .L
    ori r11,r11,1; stw r11,GUARD@l(r10)
    lis r11,STR@ha; mr r3,r30; addi r4,r11,STR@l; bl Symbol::Symbol(char const*)
  .L: lwz r11,0(r30); mr r3,r29; stw r11,0(r29); addi r1,r31,0x70
    b __restgprlr_29

There are 453 of these in the retail TU5 image and every one is byte-identical
to every other except for three *relocations* (guard word, static slot, string).
objdiff's normalized diff mode ignores relocation targets, so ANY member of the
family pairs against ANY other at a fake 100%.  Structural identification
(BinDiff) and unit-anchored heuristics therefore mispair them freely and the
mispair is self-confirming — the honesty trap.

The ONLY sound discriminator is the string operand.  This tool reads it out of
the extracted PE and checks it against the token the mapped class actually
declares (OBJ_CLASSNAME / DECLARE_MESSAGE / hand-written StaticClassName/Type),
scanned from src/, rb3-Wii and dc3-decomp.

USAGE
    python3 scripts/harvest/localstatic_symbol_audit.py            # report
    python3 scripts/harvest/localstatic_symbol_audit.py --json OUT # machine-readable

VERDICTS
    OK          mapped class's declared token == the string the function loads
    MISMATCH    they differ -> the map entry is wrong (repairable if the string
                is unique in the image and resolves to exactly one class)
    AMBIGUOUS   >1 function in the image loads that string (e.g. FxSendReverb /
                FxSendReverb360 both register as "FxSendReverb") -> the string
                cannot discriminate; do not auto-repair
    UNMAPPED    a family member with no map entry at all (free identification)

CAVEAT WHEN APPLYING REPAIRS
    A repair is only *match-neutral or better* when the unit that owns the VA
    (per splits.txt) compiles the corrected symbol, or does not compile the old
    one.  Otherwise you trade a fake 100% for nothing and the whole-binary count
    drops.  Check that before landing; see the `harmful` field in --json output.
"""
import argparse
import bisect
import glob
import json
import os
import re
import struct
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import xex_string_at as X  # noqa: E402

WII = "/home/free/code/milohax/rb3/src"
DC3 = "/home/free/code/milohax/dc3-decomp/src"

SCN_RE = re.compile(r"^\?StaticClassName@([A-Za-z_]\w*)@@SA\?AVSymbol@@XZ$")
TYP_RE = re.compile(r"^\?Type@([A-Za-z_]\w*)@@SA\?AVSymbol@@XZ$")
CLASS_RE = re.compile(r"\b(?:class|struct)\s+([A-Za-z_]\w*)\s*(?::[^;{]*)?\{")
MSG_RE = re.compile(r'DECLARE_MESSAGE(?:_NOINLINE_DTOR)?\s*\(\s*([A-Za-z_]\w*)\s*,\s*"([^"]+)"')


# ---------------------------------------------------------------- source index
def _class_body(txt, m):
    i = m.end() - 1
    d = 0
    j = i
    while j < len(txt):
        if txt[j] == "{":
            d += 1
        elif txt[j] == "}":
            d -= 1
            if d == 0:
                break
        j += 1
    return txt[i:j]


def scan_tree(root):
    """-> (scn, typ): class -> token it declares."""
    scn, typ = {}, {}
    for dp, _, fns in os.walk(root):
        if "/stlport" in dp or "/xdk" in dp or "/vendor" in dp:
            continue
        for fn in fns:
            if not fn.endswith((".h", ".hpp", ".cpp")):
                continue
            try:
                txt = open(os.path.join(dp, fn), errors="replace").read()
            except OSError:
                continue
            for m in CLASS_RE.finditer(txt):
                body = _class_body(txt, m)
                name = m.group(1)
                mm = re.search(r"OBJ_CLASSNAME\s*\(\s*([A-Za-z_]\w*)\s*\)", body)
                if mm:
                    scn.setdefault(name, mm.group(1))
                else:
                    mm = re.search(
                        r'static\s+Symbol\s+StaticClassName\s*\([^)]*\)[^{]*\{[^}]*?"([^"]+)"',
                        body, re.S)
                    if mm:
                        scn.setdefault(name, mm.group(1))
                mt = re.search(
                    r'static\s+Symbol\s+Type\s*\([^)]*\)[^{]*\{[^}]*?"([^"]+)"', body, re.S)
                if mt:
                    typ.setdefault(name, mt.group(1))
            for mm in re.finditer(
                    r'Symbol\s+([A-Za-z_]\w*)::(StaticClassName|Type)\s*\([^)]*\)\s*\{[^}]*?"([^"]+)"',
                    txt, re.S):
                (scn if mm.group(2) == "StaticClassName" else typ).setdefault(
                    mm.group(1), mm.group(3))
            for mm in MSG_RE.finditer(txt):
                typ.setdefault(mm.group(1), mm.group(2))
    return scn, typ


# ------------------------------------------------------------------ PE reading
class Image:
    def __init__(self, exe):
        self.data, self.base, self.secs = X.load_sections(exe)

    def cstr(self, va):
        off, _ = X.va_to_offset(va, self.base, self.secs)
        if off is None:
            return None
        s = X.read_cstring(self.data, off, 300)
        if not s or not all(32 <= c < 127 for c in s):
            return None
        return s.decode("ascii")

    def text(self):
        for name, vaddr, vsize, rawptr, rawsize in self.secs:
            if name == ".text":
                n = min(vsize, rawsize)
                return self.base + vaddr, self.data[rawptr:rawptr + n]
        raise RuntimeError("no .text")


_W = struct.Struct(">I")


def _match_shape(blob, i):
    """Return the string VA if blob[i:] is the local-static-Symbol body."""
    w = [_W.unpack_from(blob, i + k * 4)[0] for k in range(20)]
    if (w[0] != 0x7D8802A6 or (w[1] >> 26) != 18 or w[2] != 0x3BE1FF90
            or w[3] != 0x9421FF90 or (w[4] >> 16) != 0x3D40 or (w[5] >> 16) != 0x3D60
            or w[6] != 0x7C7D1B78 or w[9] != 0x556907FF or w[11] != 0x616B0001
            or (w[13] >> 16) != 0x3D60 or w[14] != 0x7FC3F378 or (w[15] >> 16) != 0x388B
            or (w[16] >> 26) != 18 or w[17] != 0x817E0000 or w[18] != 0x7FA3EB78
            or w[19] != 0x917D0000):
        return None
    hi, lo = w[13] & 0xFFFF, w[15] & 0xFFFF
    if lo & 0x8000:
        hi -= 1
    return ((hi << 16) | lo) & 0xFFFFFFFF


def scan_image(img):
    """-> {string: [va, ...]} for every family member in .text."""
    base, blob = img.text()
    hits = defaultdict(list)
    i, lim = 0, len(blob) - 0x58
    while i <= lim:
        if blob[i] == 0x7D and blob[i + 1] == 0x88 and blob[i + 2] == 0x02 and blob[i + 3] == 0xA6:
            sva = _match_shape(blob, i)
            if sva is not None:
                s = img.cstr(sva)
                if s:
                    hits[s].append(base + i)
        i += 4
    return hits


# --------------------------------------------------------------- unit ownership
def load_units(splits_path, obj_root):
    cur, rng = None, []
    for line in open(splits_path).read().splitlines():
        st = line.rstrip()
        if st and not line[0].isspace() and st.endswith(":") and st[:-1].endswith((".cpp", ".c")):
            cur = st[:-1]
            continue
        mt = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
        if mt and cur:
            rng.append((int(mt.group(1), 16), int(mt.group(2), 16), cur))
    rng.sort()
    starts = [r[0] for r in rng]

    defined = {}
    for f in glob.glob(os.path.join(obj_root, "**", "*.obj"), recursive=True):
        key = os.path.relpath(f, obj_root)[:-4] + ".cpp"
        try:
            syms = set(_coff_defined(open(f, "rb").read()))
        except OSError:
            syms = set()
        defined[key] = syms
        defined.setdefault(os.path.basename(key), syms)

    def owner(va):
        i = bisect.bisect_right(starts, va) - 1
        if i >= 0 and rng[i][0] <= va < rng[i][1]:
            return rng[i][2]
        return None

    return owner, defined


def _coff_defined(data):
    if len(data) < 20:
        return []
    so = struct.unpack_from("<I", data, 8)[0]
    n = struct.unpack_from("<I", data, 12)[0]
    if not so or not n:
        return []
    st, out, i = so + n * 18, [], 0
    while i < n:
        eo = so + i * 18
        if eo + 18 > len(data):
            break
        nb = data[eo:eo + 8]
        if nb[:4] == b"\0\0\0\0":
            ao = st + struct.unpack_from("<I", nb, 4)[0]
            try:
                name = data[ao:data.index(b"\0", ao)].decode("ascii", "replace")
            except ValueError:
                name = ""
        else:
            name = nb.split(b"\0")[0].decode("ascii", "replace")
        if struct.unpack_from("<h", data, eo + 12)[0] > 0:
            out.append(name)
        i += 1 + data[eo + 17]
    return out


# ------------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-dir", default=ROOT)
    ap.add_argument("--json", help="write full results here")
    args = ap.parse_args()
    p = args.project_dir

    img = Image(os.path.join(p, "orig/45410914/band.exe"))
    hits = scan_image(img)
    va2str = {va: s for s, vas in hits.items() for va in vas}
    ambiguous = {s for s, vas in hits.items() if len(vas) > 1}

    trees = {}
    for tag, root in (("xen", os.path.join(p, "src")), ("wii", WII), ("dc3", DC3)):
        if os.path.isdir(root):
            trees[tag] = scan_tree(root)
    order = [t for t in ("xen", "wii", "dc3") if t in trees]

    def declared(cls, kind):
        for t in order:
            d = trees[t][0] if kind == "SCN" else trees[t][1]
            if cls in d:
                return d[cls], t
        return None, None

    rev = defaultdict(lambda: defaultdict(dict))  # token -> kind -> tree -> classes
    for t in order:
        for c, tok in trees[t][0].items():
            rev[tok]["SCN"].setdefault(t, set()).add(c)
        for c, tok in trees[t][1].items():
            rev[tok]["Type"].setdefault(t, set()).add(c)

    def resolve(tok):
        e = rev.get(tok)
        if not e or len(e) != 1:
            return None
        k = list(e)[0]
        for t in order:
            if e[k].get(t):
                cs = sorted(e[k][t])
                return (k, cs[0]) if len(cs) == 1 else None
        return None

    owner, defined = load_units(os.path.join(p, "config/45410914/splits.txt"),
                                os.path.join(p, "build/45410914/src"))
    m = json.load(open(os.path.join(p, "scripts/target_symbol_map.json")))
    mapped = {int(k, 16): v for k, v in m.items() if k.lower().startswith("0x")}

    rows = []
    for va, s in sorted(va2str.items()):
        sym = mapped.get(va)
        u = owner(va)
        d = defined.get(u) if u else None
        rec = {"va": "0x%08x" % va, "string": s, "mapped": sym, "unit": u,
               "ambiguous": s in ambiguous}
        if sym is None:
            rec["verdict"] = "UNMAPPED"
        else:
            mm = SCN_RE.match(sym) or TYP_RE.match(sym)
            if not mm:
                rec["verdict"] = "FOREIGN"          # not even a classname symbol
            else:
                kind = "SCN" if SCN_RE.match(sym) else "Type"
                cls = mm.group(1)
                tok, src = declared(cls, kind)
                rec.update(mapped_class=cls, kind=kind, declared=tok, declared_in=src)
                rec["verdict"] = "OK" if tok == s else ("NO_TOKEN" if tok is None else "MISMATCH")
        if rec["verdict"] in ("MISMATCH", "FOREIGN", "UNMAPPED", "NO_TOKEN") and not rec["ambiguous"]:
            r = resolve(s)
            if r:
                k, cls = r
                rec["repair"] = ("?StaticClassName@%s@@SA?AVSymbol@@XZ" % cls if k == "SCN"
                                 else "?Type@%s@@SA?AVSymbol@@XZ" % cls)
                # a repair is harmful when the owning unit compiles the OLD name
                # but not the corrected one: a fake 100% would be lost for nothing.
                rec["harmful"] = bool(d is not None and sym in d and rec["repair"] not in d)
        rows.append(rec)

    tally = defaultdict(int)
    for r in rows:
        tally[r["verdict"]] += 1
    print("family members in .text: %d   distinct strings: %d   ambiguous strings: %d"
          % (len(va2str), len(hits), len(ambiguous)))
    print("verdicts: " + "  ".join("%s=%d" % kv for kv in sorted(tally.items())))
    rep = [r for r in rows if r.get("repair")]
    print("repairable: %d  (of which harmful-to-apply: %d)"
          % (len(rep), sum(1 for r in rep if r.get("harmful"))))
    for r in rows:
        if r["verdict"] == "MISMATCH":
            print("  MISMATCH %s %-34s declares '%s' but loads '%s'%s"
                  % (r["va"], r.get("mapped_class", "?"), r.get("declared"), r["string"],
                     "  -> " + r["repair"] + (" [HARMFUL]" if r.get("harmful") else "")
                     if r.get("repair") else "  (ambiguous)"))
    if args.json:
        json.dump(rows, open(args.json, "w"), indent=1)
        print("wrote", args.json)


if __name__ == "__main__":
    sys.exit(main() or 0)
