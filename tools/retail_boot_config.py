#!/usr/bin/env python3
"""The system config the Xbox retail game reads at boot, rebuilt without the engine.

Retail boots as App::App -> SystemPreInit(argc, argv, "config/band_preinit_keep.dta").
Before that file is read, two functions put macros in the DTA macro table:

  SystemPreInit (0x82510EC8) calls PlatformMgr::RegionInit (0x8251BD28), which maps
      XGetGameRegion() to a region (0xFF and every non-European code give NA) and
      calls SetRegion, which defines "REGION_" + upper(region symbol).
  PreInitSystem (0x82510BB8) defines HX_XBOX, HX_WIN, HX_NG and _SHIP, then one
      macro per `-define` option (a retail boot passes none), then calls
      BeginDataRead and DataReadFile(config).

The shipped .dtb files test those macros with #ifdef / #ifndef, so the config a
native target reads depends on them. This tool computes the retail answer two
ways that share no code with the engine:

  macros   the boot macro sequence, decoded from the retail image itself: the
           call order in SystemPreInit, and the string passed to each
           Symbol(const char *) that feeds a DataSetMacro call in PreInitSystem.
  view     the config DataReadFile returns, by decrypting the shipped .dtb files
           straight out of the ark (native/tools/ark_extract.py's reader) and
           loading them with this file's own copy of DataArray::Load's rules
           (conditionals, macro splicing, #define/#undef, #include, #merge with
           DataMergeTags, the session read cache, per-array file paths).
  check    compares a native dump (native/src/retail_boot_macros.h,
           DumpConfig) with `view` under the macros from `macros`. Exit 0 equal,
           1 different, 2 could not run.

The dump format is shared with retail_boot_macros.h: one line naming the boot
macros found in the macro table before the read (sorted), then one node per line
with two spaces of indent per array level.

Usage:
  tools/retail_boot_config.py macros
  tools/retail_boot_config.py view   [--assets DIR] [--drop NAME] [--out FILE]
  tools/retail_boot_config.py check  NATIVE_DUMP [--assets DIR]
"""

import argparse
import difflib
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "native", "tools"))
from ark_extract import Ark, Rand2  # noqa: E402

DEFAULT_ASSETS = os.path.expanduser("~/code/milohax/rb3/orig-assets/xbox-zip")
DEFAULT_EXE = os.path.join(ROOT, "orig", "45410914", "band.exe")
SYMBOL_MAP = os.path.join(ROOT, "scripts", "target_symbol_map.json")
SYMBOLS_TXT = os.path.join(ROOT, "config", "45410914", "symbols.txt")
PREINIT_CONFIG = "config/band_preinit_keep.dta"
SYSTEM_CONFIG = "config/band_keep.dta"  # App::App -> SystemInit; decoded by retail_system_config

M_SYSTEM_PREINIT = "?SystemPreInit@@YAXPBD@Z"
M_PREINIT_SYSTEM = "?PreInitSystem@@YAXPBD@Z"
M_REGION_INIT = "?RegionInit@PlatformMgr@@QAAXXZ"
M_DATA_INIT = "?DataInit@@YAXXZ"
M_DATA_SET_MACRO = "?DataSetMacro@@YAXVSymbol@@PAVDataArray@@@Z"
M_SYMBOL_CTOR = "??0Symbol@@QAA@PBD@Z"
M_APP_CTOR = "??0App@@QAA@HPAPAD@Z"
M_SYSTEM_INIT = "?SystemInit@@YAXPBD@Z"


# ---------------------------------------------------------------- the image --
class Image:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        d = self.d
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        opt = struct.unpack_from("<H", d, pe + 20)[0]
        self.base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        self.secs = []
        off = pe + 24 + opt
        for i in range(nsec):
            name, vs, va, rs, ra = struct.unpack_from("<8sIIII", d, off + i * 40)
            self.secs.append((va, max(vs, rs), ra, rs))

    def off(self, addr):
        rva = addr - self.base
        for va, size, ra, rs in self.secs:
            if va <= rva < va + size and rva - va < rs:
                return ra + rva - va
        raise ValueError("address %#x is not backed by the image" % addr)

    def word(self, addr):
        return struct.unpack_from(">I", self.d, self.off(addr))[0]

    def cstr(self, addr):
        o = self.off(addr)
        return self.d[o:self.d.index(b"\0", o)].decode("latin-1")

    def contains(self, s):
        return (s.encode("latin-1") + b"\0") in self.d


def load_names():
    with open(SYMBOL_MAP) as f:
        m = json.load(f)
    by_name = {}
    for k, v in m.items():
        if k.startswith("0x") and isinstance(v, str):
            by_name.setdefault(v, int(k, 16))
    sizes = {}
    with open(SYMBOLS_TXT) as f:
        for line in f:
            if "type:function" not in line or "size:" not in line or ".text:" not in line:
                continue
            addr = int(line.split(".text:")[1].split(";")[0], 16)
            sizes[addr] = int(line.split("size:")[1].split()[0], 16)
    return by_name, sizes


LOGICAL_XO = {444, 28, 316, 124, 60, 412, 476, 284, 24, 536, 792, 824, 26, 954, 922,
              986, 27, 539, 794, 58}


def decode_calls(img, start, size):
    """Linear decode of [start, start+size): every `bl`, with the constant
    addresses held in r3/r4 at the call (lis + addi/ori pairs only; every call
    clobbers r3-r12, so a value is only known if it was built since the last
    call)."""
    regs = {}
    calls = []
    for a in range(start, start + size, 4):
        w = img.word(a)
        op = w >> 26
        rd = (w >> 21) & 31
        ra = (w >> 16) & 31
        imm = w & 0xFFFF
        simm = imm - 0x10000 if imm & 0x8000 else imm
        if op == 15:  # addis / lis
            regs[rd] = ((regs.get(ra, 0) if ra else 0) + (simm << 16)) & 0xFFFFFFFF \
                if (ra == 0 or ra in regs) else None
        elif op == 14:  # addi / li
            if ra == 0:
                regs[rd] = simm & 0xFFFFFFFF
            elif regs.get(ra) is not None:
                regs[rd] = (regs[ra] + simm) & 0xFFFFFFFF
            else:
                regs[rd] = None
        elif op == 24:  # ori: ori rA, rS, imm -- destination is the ra field
            regs[ra] = (regs[rd] | imm) if regs.get(rd) is not None else None
        elif op == 18 and (w & 3) == 1:  # bl
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            calls.append((a, (a + li) & 0xFFFFFFFF, regs.get(3), regs.get(4)))
            for r in range(3, 13):
                regs.pop(r, None)
        elif op == 31:
            xo = (w >> 1) & 0x3FF
            rb = (w >> 11) & 31
            if xo == 444 and rd == rb:  # mr rA, rS
                regs[ra] = regs.get(rd)
            elif xo in LOGICAL_XO:      # logical/shift forms write rA
                regs[ra] = None
            else:                       # arithmetic and indexed loads write rD
                regs[rd] = None
        elif op in (21, 23, 25, 26, 27, 28, 29, 30):  # rotate/logical-immediate: rA
            regs[ra] = None
        elif op in (7, 8, 10, 11, 12, 13) and op not in (10, 11):  # mulli/subfic/addic: rD
            regs[rd] = None
        elif op in (32, 33, 34, 35, 40, 41, 42, 43, 46, 58):  # integer loads: rD
            regs[rd] = None
    return calls


def retail_boot_macros(exe=DEFAULT_EXE):
    """Return (sequence, evidence lines). Raises if the image does not show the
    expected shape -- a decode that finds nothing must not read as 'no macros'."""
    img = Image(exe)
    by_name, sizes = load_names()
    ev = []
    need = [M_SYSTEM_PREINIT, M_PREINIT_SYSTEM, M_REGION_INIT, M_DATA_INIT,
            M_DATA_SET_MACRO, M_SYMBOL_CTOR]
    for n in need:
        if n not in by_name:
            raise RuntimeError("symbol map has no %s" % n)
    spi = by_name[M_SYSTEM_PREINIT]
    pis = by_name[M_PREINIT_SYSTEM]

    # 1. SystemPreInit: RegionInit before DataInit before PreInitSystem.
    order = {}
    for i, (_, tgt, _, _) in enumerate(decode_calls(img, spi, sizes[spi])):
        for n in (M_REGION_INIT, M_DATA_INIT, M_PREINIT_SYSTEM):
            if tgt == by_name[n] and n not in order:
                order[n] = i
    if len(order) != 3 or not (order[M_REGION_INIT] < order[M_DATA_INIT]
                               < order[M_PREINIT_SYSTEM]):
        raise RuntimeError("SystemPreInit %#x call order not as expected: %r" % (spi, order))
    ev.append("SystemPreInit %#x calls RegionInit (call #%d), DataInit (#%d), "
              "PreInitSystem (#%d)" % (spi, order[M_REGION_INIT], order[M_DATA_INIT],
                                       order[M_PREINIT_SYSTEM]))

    # 2. RegionInit -> SetRegion builds "REGION_%s" from the upper-cased region
    #    symbol. XGetGameRegion() == 0xFF (and every non-European code) is NA.
    for s in ("REGION_%s", "na"):
        if not img.contains(s):
            raise RuntimeError("image has no string %r" % s)
    ev.append("RegionInit %#x: region 0xFF -> kRegionNA; SetRegion formats "
              "'REGION_%%s' -> REGION_NA" % by_name[M_REGION_INIT])

    # 3. PreInitSystem: each Symbol(const char *) whose result feeds the next
    #    DataSetMacro call, in call order, up to the `-define` option loop.
    seq = []
    pending = None
    n_set = 0
    for a, tgt, r3, r4 in decode_calls(img, pis, sizes[pis]):
        if tgt == by_name[M_SYMBOL_CTOR]:
            pending = img.cstr(r4) if r4 is not None else None
        elif tgt == by_name[M_DATA_SET_MACRO]:
            n_set += 1
            if pending is not None:
                seq.append(pending)
            pending = None
    if n_set != len(seq) + 1:
        raise RuntimeError("PreInitSystem %#x: %d DataSetMacro calls but %d constant "
                           "names (expected exactly one non-constant: the -define loop)"
                           % (pis, n_set, len(seq)))
    ev.append("PreInitSystem %#x: DataSetMacro(Symbol(<const>)) x%d in order %s, then "
              "the -define loop" % (pis, len(seq), " ".join(seq)))
    return ["REGION_NA"] + seq, ev


def retail_system_config(exe=DEFAULT_EXE):
    """The file App::App passes to SystemInit, decoded from the retail image
    (the constant in r3 at the `bl SystemInit`). Returns (name, evidence line).
    Raises unless there is exactly one such call with a constant argument."""
    img = Image(exe)
    by_name, sizes = load_names()
    for n in (M_APP_CTOR, M_SYSTEM_INIT):
        if n not in by_name:
            raise RuntimeError("symbol map has no %s" % n)
    app = by_name[M_APP_CTOR]
    hits = [(a, r3) for a, tgt, r3, _ in decode_calls(img, app, sizes[app])
            if tgt == by_name[M_SYSTEM_INIT]]
    if len(hits) != 1 or hits[0][1] is None:
        raise RuntimeError("App::App %#x: expected one SystemInit call with a constant "
                           "argument, found %r" % (app, hits))
    name = img.cstr(hits[0][1])
    return name, ("App::App %#x: bl SystemInit at %#x with r3 = \"%s\""
                  % (app, hits[0][0], name))


# --------------------------------------------------------- the DTB loader --
INT, FLOAT, VAR, FUNC, OBJECT, SYMBOL, UNHANDLED, IFDEF, ELSE, ENDIF = range(10)
ARRAY, COMMAND, STRING, PROPERTY, GLOB = 16, 17, 18, 19, 20
DEFINE, INCLUDE, MERGE, IFNDEF, AUTORUN, UNDEF = 32, 33, 34, 35, 36, 37
SYMBOLIC = (SYMBOL, IFDEF, DEFINE, INCLUDE, MERGE, IFNDEF, UNDEF, FUNC, VAR)


class Arr:
    __slots__ = ("nodes", "file", "line")

    def __init__(self, file):
        self.nodes = []
        self.file = file
        self.line = 0


def file_get_path(f):
    i = max(f.rfind("/"), f.rfind("\\"))
    if i < 0:
        return "."
    if i == 0 or (i > 0 and f[i - 1] == ":"):
        return f[:i + 1]
    return f[:i]


def file_get_base(f):
    i = max(f.rfind("/"), f.rfind("\\"))
    b = f[i + 1:] if i >= 0 else f
    j = b.rfind(".")
    return b[:j] if j >= 0 else b


def file_make_path(root, f):
    """os/File.cpp FileMakePath, for paths with no drive."""
    if f[:1] in ("/", "\\", ""):
        s = f
    else:
        s = root + "/" + f
    s = s.replace("\\", "/").lower()
    cur_slash = s.startswith("/")
    dirs = []
    for p in [x for x in s.split("/") if x]:
        if p[0] != ".":
            dirs.append(p)
        elif p == "..":
            if dirs and dirs[-1][0] != ".":
                dirs.pop()
            else:
                dirs.append(p)
    if not dirs:
        return "/" if cur_slash else "."
    return ("/" if cur_slash else "") + "/".join(dirs)


def cached_data_file(f):
    """DataFile.cpp CachedDataFile for a non-local path."""
    if ".dtb" in f:
        return f
    return "%s/gen/%s.dtb" % (file_get_path(f), file_get_base(f))


class Loader:
    def __init__(self, ark, macros):
        self.ark = ark
        self.macros = {m: [(INT, 1)] for m in macros}  # DataArrayPtr(1): (1)
        self.cond = []            # gDataArrayConditional
        self.cache = {}           # gReadFiles, live for one read session
        self.gfile = ""           # DataArray::gFile
        self.autoruns = []

    def defined(self):
        return all(self.cond)

    # DataReadFile inside an open read session.
    def read_file(self, path):
        cached = cached_data_file(path)
        if cached in self.cache:
            return self.cache[cached]
        raw, _ = self.ark.read(cached)
        if raw is None:
            raise FileNotFoundError("%s (as %s) is not in the ark" % (path, cached))
        seed = struct.unpack("<I", raw[:4])[0]
        rng = Rand2(seed)
        body = bytearray(raw[4:])
        for i in range(len(body)):
            body[i] ^= rng.next() & 0xFF
        self.buf, self.pos = bytes(body), 0
        self.gfile = path
        if not self.u8():
            arr = None
        else:
            arr = self.load_array()
        self.cache[cached] = arr
        return arr

    def take(self, n):
        b = self.buf[self.pos:self.pos + n]
        if len(b) != n:
            raise ValueError("truncated dtb (%s)" % self.gfile)
        self.pos += n
        return b

    def u8(self):
        return self.take(1)[0]

    def i16(self):
        return struct.unpack("<h", self.take(2))[0]

    def i32(self):
        return struct.unpack("<i", self.take(4))[0]

    def sym(self):
        return self.take(self.i32()).decode("latin-1")

    def read_node(self):
        t = self.i32()
        if t in SYMBOLIC:
            return (t, self.sym())
        if t == FLOAT:
            return (t, struct.unpack("<f", self.take(4))[0])
        if t in (STRING, GLOB):
            n = self.i32()
            return (t, self.take(n) if t == STRING else self.take(-n if n < 0 else n))
        if t in (ARRAY, COMMAND, PROPERTY):
            return (t, self.load_array())
        if t == OBJECT:
            return (t, self.sym())
        if t in (UNHANDLED, INT, ELSE, ENDIF, AUTORUN):
            return (t, self.i32())
        raise ValueError("unrecognized node type %#x in %s" % (t, self.gfile))

    def load_array(self):
        arr = Arr(self.gfile)
        size = self.i16()
        arr.line = self.i16()
        self.i16()  # mDeprecated
        out = arr.nodes
        k = 0
        while k < size:
            node = self.read_node()
            k += 1
            t, v = node
            if not (self.defined() or t in (IFDEF, IFNDEF, ELSE, ENDIF)):
                continue
            if t == SYMBOL and v in self.macros and self.macros[v] is not None:
                out.extend(self.macros[v])
                continue
            if t == AUTORUN:
                self.autoruns.append((arr.file, self.read_node()))
                k += 1
            elif t == DEFINE:
                m = self.read_node()
                k += 1
                if m[0] != ARRAY:
                    raise ValueError("#define %s body is not an array" % v)
                self.macros[v] = m[1].nodes
            elif t == UNDEF:
                self.macros[v] = None
            elif t == IFDEF:
                self.cond.append(self.macros.get(v) is not None)
            elif t == IFNDEF:
                self.cond.append(self.macros.get(v) is None)
            elif t == ELSE:
                self.cond[-1] = not self.cond[-1]
            elif t == ENDIF:
                self.cond.pop()
            elif t in (INCLUDE, MERGE):
                macro = self.macros.get(v)
                if macro is not None:
                    src = macro
                else:
                    path = file_make_path(file_get_path(arr.file), v)
                    state = (self.buf, self.pos)
                    got = self.read_file(path)
                    self.buf, self.pos = state
                    if got is None:
                        raise FileNotFoundError(path)
                    src = got.nodes
                if t == INCLUDE:
                    out.extend(src)
                else:
                    if not src:
                        raise ValueError("empty merge file %s" % v)
                    merge_tags(out, src)
                self.gfile = arr.file
            else:
                out.append(node)
        return arr


def tag_key(node):
    t, v = node
    if t == SYMBOL:
        return ("s", v)
    if t == INT:
        return ("n", v & 0xFFFFFFFF)
    if t == FLOAT:
        return ("n", struct.unpack("<I", struct.pack("<f", v))[0])
    return ("p", id(v))


def merge_tags(dest, src):
    """DataUtl.cpp DataMergeTags over node lists (dest mutated in place)."""
    if src is dest:
        return
    for node in list(src):
        if node[0] != ARRAY or not node[1].nodes:
            continue
        key = tag_key(node[1].nodes[0])
        found = None
        for d in dest:
            if d[0] == ARRAY and d[1].nodes and tag_key(d[1].nodes[0]) == key:
                found = d[1]
                break
        if found is None:
            dest.append(node)
        else:
            merge_tags(found.nodes, node[1].nodes)


def defines_only(node):
    t, v = node
    if t != COMMAND or not v.nodes or v.nodes[0] != (SYMBOL, v.nodes[0][1]):
        return False
    head = v.nodes[0][1]
    if head == "func":
        return True
    return head == "do" and all(defines_only(n) for n in v.nodes[1:])


def read_config(assets, macros, config=PREINIT_CONFIG):
    ld = Loader(Ark(assets), macros)
    cfg = ld.read_file(config)
    return cfg, ld


def find_array(arr, tag):
    """DataArray::FindArray(Symbol, false): the first child array tagged `tag`."""
    for t, v in arr.nodes:
        if t == ARRAY and v.nodes and v.nodes[0] == (SYMBOL, tag):
            return v
    return None


def strip_editor_data(cfg):
    """os/System.cpp StripEditorData, run at the end of InitSystem: every
    (editor ...) under an (objects <class> ...) entry, and under each of its
    (types <type> ...) entries, is cut back to its tag."""
    objs = find_array(cfg, "objects")
    if objs is None:
        raise ValueError("no (objects ...) section to strip")
    for t, cls in objs.nodes[1:]:
        if t != ARRAY:
            raise ValueError("(objects ...) entry is not an array")
        ed = find_array(cls, "editor")
        if ed is not None:
            del ed.nodes[1:]
        types = find_array(cls, "types")
        if types is not None:
            for tt, ty in types.nodes[1:]:
                if tt != ARRAY:
                    raise ValueError("(types ...) entry is not an array")
                ed = find_array(ty, "editor")
                if ed is not None:
                    del ed.nodes[1:]


def read_full_config(assets, macros, system=SYSTEM_CONFIG):
    """The config after SystemInit: PreInitSystem's read of the preinit file,
    then InitSystem's read of `system` in the SAME read session (BeginDataRead in
    PreInitSystem, FinishDataRead at the end of InitSystem), then
    DataMergeTags(system, preinit) -- tags only the preinit file has are added,
    the system file wins every other -- then StripEditorData. DataReplaceTags
    moves the merged contents into the preinit arrays' storage and changes no
    content, so it has no counterpart here."""
    ld = Loader(Ark(assets), macros)
    pre = ld.read_file(PREINIT_CONFIG)
    cfg = ld.read_file(system)
    merge_tags(cfg.nodes, pre.nodes)
    strip_editor_data(cfg)
    return cfg, ld


# ------------------------------------------------------------ the dump --
def esc(b):
    out = []
    for c in b:
        if c == 0x22 or c == 0x5C:
            out.append("\\" + chr(c))
        elif 0x20 <= c < 0x7F:
            out.append(chr(c))
        else:
            out.append("\\x%02x" % c)
    return "".join(out)


def dump_nodes(nodes, depth, lines):
    pad = "  " * depth
    for t, v in nodes:
        if t == INT:
            lines.append("%si %d" % (pad, v))
        elif t == FLOAT:
            lines.append("%sf %.9g" % (pad, v))
        elif t == SYMBOL:
            lines.append("%ss %s" % (pad, v))
        elif t == STRING:
            lines.append('%st "%s"' % (pad, esc(v)))
        elif t == VAR:
            lines.append("%sv %s" % (pad, v))
        elif t == FUNC:
            lines.append("%sfn %s" % (pad, v))
        elif t == OBJECT:
            lines.append("%so %s" % (pad, v))
        elif t == UNHANDLED:
            lines.append("%su" % pad)
        elif t == GLOB:
            lines.append("%sg %s" % (pad, v.hex()))
        elif t in (ARRAY, COMMAND, PROPERTY):
            o, c = {ARRAY: "()", COMMAND: "{}", PROPERTY: "[]"}[t]
            lines.append(pad + o)
            dump_nodes(v.nodes, depth + 1, lines)
            lines.append(pad + c)
        else:
            lines.append("%s? %d" % (pad, t))


def dump(cfg, macros, table=None):
    lines = ["# boot macros: " + " ".join(sorted(macros))]
    dump_nodes(cfg.nodes, 0, lines)
    if table is not None:
        dump_macros(table, lines)
    return lines


MACRO_HEADER = "# macro table after the read"


def dump_macros(table, lines):
    """Every macro defined when the read session ends, sorted by name: one
    `m NAME` line, then its value's nodes one level in. A driver's own
    DataSetMacro shows up here, so the macro table is held to retail too."""
    lines.append(MACRO_HEADER)
    for name in sorted(k for k, v in table.items() if v is not None):
        lines.append("m " + name)
        dump_nodes(table[name], 1, lines)


# --------------------------------------------------------------- main --
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["macros", "view", "check"])
    ap.add_argument("native", nargs="?")
    ap.add_argument("--assets", default=os.environ.get("RB3_ASSETS", DEFAULT_ASSETS))
    ap.add_argument("--exe", default=DEFAULT_EXE)
    ap.add_argument("--drop", action="append", default=[],
                    help="view only: leave this macro out (to see what it gates)")
    ap.add_argument("--full", action="store_true",
                    help="the config after SystemInit (preinit + the file App::App "
                         "passes to SystemInit, merged, editor data stripped) plus "
                         "the macro table, instead of the preinit config alone")
    ap.add_argument("--reference",
                    help="check only: compare with this earlier `view` output "
                         "instead of rebuilding it (its macro line and shape are "
                         "still checked)")
    ap.add_argument("--out")
    a = ap.parse_args()

    try:
        macros, ev = retail_boot_macros(a.exe)
        sysfile, sev = retail_system_config(a.exe)
    except Exception as e:  # noqa: BLE001
        print("retail_boot_config: cannot derive the boot sequence: %s" % e, file=sys.stderr)
        return 2
    if a.cmd == "macros":
        for line in ev + [sev]:
            print("  " + line)
        print("retail boot macros, in order: " + " ".join(macros))
        print("retail system config: " + sysfile)
        return 0

    want = None
    if a.cmd == "check" and a.reference:
        try:
            want = open(a.reference, encoding="latin-1").read().splitlines()
        except OSError as e:
            print("retail_boot_config: cannot read reference %s: %s" % (a.reference, e),
                  file=sys.stderr)
            return 2
        # A reference is only a cache of `view`: it must be the full view under
        # the retail macros, or the comparison is against something else.
        if (not want or want[0] != "# boot macros: " + " ".join(sorted(macros))
                or (MACRO_HEADER in want) != a.full):
            print("retail_boot_config: reference %s is not a%s view under %s"
                  % (a.reference, " --full" if a.full else "", " ".join(macros)),
                  file=sys.stderr)
            return 2
        nsec = sum(1 for x in want if x == "(")  # informational only
    else:
        use = [m for m in macros if m not in a.drop]
        try:
            if a.full:
                cfg, ld = read_full_config(a.assets, use, sysfile)
            else:
                cfg, ld = read_config(a.assets, use)
        except Exception as e:  # noqa: BLE001
            print("retail_boot_config: cannot read the shipped config: %s" % e,
                  file=sys.stderr)
            return 2
        # An #autorun is a command run at load. One that only defines a script
        # function ({func ...}, optionally inside {do ...}) adds no config nodes;
        # any other kind could, and this reader cannot run it.
        bad = [f for f, n in ld.autoruns if not defines_only(n)]
        if bad:
            print("retail_boot_config: #autorun block(s) that do more than define a "
                  "function, in %s; this reader cannot execute them" % ", ".join(bad),
                  file=sys.stderr)
            return 2
        want = dump(cfg, use, ld.macros if a.full else None)
        nsec = len(cfg.nodes)

    if a.cmd == "view":
        text = "\n".join(want) + "\n"
        if a.out:
            open(a.out, "w").write(text)
            print("wrote %s (%d lines, %d top-level sections, macros %s%s)"
                  % (a.out, len(want), nsec, " ".join(use),
                     ", after SystemInit(%s)" % sysfile if a.full else ""))
        else:
            sys.stdout.write(text)
        return 0

    if not a.native or not os.path.isfile(a.native):
        print("retail_boot_config: no native dump at %r" % a.native, file=sys.stderr)
        return 2
    got = open(a.native, encoding="latin-1").read().splitlines()
    if len(got) < 2:
        print("retail_boot_config: native dump %s is empty" % a.native, file=sys.stderr)
        return 2
    what = "after SystemInit(%s)" % sysfile if a.full else "preinit"
    if got == want:
        print("CONFIG-VIEW: EQUAL %s -- %d lines, %s, macros %s"
              % (a.native, len(want), what, " ".join(macros)))
        return 0
    diff = list(difflib.unified_diff(want, got, "retail", "native", n=1, lineterm=""))
    nd = sum(1 for d in diff if d[:1] in "+-" and d[:3] not in ("+++", "---"))
    print("CONFIG-VIEW: DIFFERENT %s -- %d differing lines (retail %d lines, native %d), %s"
          % (a.native, nd, len(want), len(got), what))
    for d in diff[:40]:
        print("  " + d)
    return 1


if __name__ == "__main__":
    sys.exit(main())
