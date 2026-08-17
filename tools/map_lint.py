#!/usr/bin/env python3
"""target_symbol_map consistency linter.

Cross-checks ``scripts/target_symbol_map.json`` (the address->MSVC-mangled-name
map consumed by ``scripts/obj_target_symbol_renamer.py``) against the pinned
unit ``.text`` ranges in ``config/45410914/splits.txt`` and the dtk-split asm
artifacts (``build/45410914/asm/<unit>.s``). It flags map entries that the
renamer would apply to the *wrong* unit or the *wrong kind* of artifact -- the
class of bug that silently mis-pins matches and pollutes per-unit fuzzy.

Three independent incidents motivated this tool (see
``docs/plans/decomp-state-and-roadmap-2026-06-09.md`` wave-2 batch-1/batch-2):

  * MidiInstrument unit mis-pin -- the pinned range was actually the
    BandIKEffector TU; ``?ApplyConstraints@BandIKEffector@`` sat inside a span
    otherwise full of MidiInstrument/SampleZone names. (CLASS_MIXING)
  * VocalTrackDir PreLoad/Deploy/TutorialReset ICF mis-pins -- ``PreLoad`` was
    pinned onto an 8-byte ``except_data_`` block, not a real function.
    (SIZE_KIND)
  * GemManager 8.8 MB scatter -- map names that should be one contiguous TU
    span instead spread across megabytes. (SCATTER, reported per-class)

Checks
------
1. CLASS_MIXING -- for each pinned unit, derive the unit's expected class
   family (from its own filename + the majority class of in-range map names)
   and report in-range map entries whose mangled class is NOT in the family and
   is NOT an STL/template helper. These are the tell-tale of a mis-pinned range
   or a stale leftover from a previous pin.
2. SIZE_KIND -- report map entries whose VA in the unit's ``.s`` is not a real
   ``.fn`` span, or is a real fn but implausibly small (<= --min-fn-size bytes,
   default 8) while the map claims a substantive method. The classic ``PreLoad
   onto except_data_`` case shows up as ``no-fn-at-va`` (or fn-of-size-0x8).
3. SCATTER (OPT-IN, ``--check scatter``) -- for every class that appears in the
   map, report when its symbols span more than --scatter-bytes (default 0x40000
   = 256 KB) of VA, i.e. the class is not a single contiguous TU (the GemManager
   8.8 MB scatter signature). LIMITATION: on the existing map this mostly
   catches engine ICF-ubiquity (Object/Symbol/DataNode inline everywhere), so it
   is opt-in and advisory -- see check_scatter() docstring.

4. OBJ_ORPHAN (OPT-IN, ``--check obj_orphan``) -- the highest-precision check:
   for each pinned unit, flag in-range map entries whose mangled name is NOT a
   symbol defined by the unit's own *compiled* obj (build/45410914/src/...).
   Such a name can never pair (the renamer writes it onto a dtk target symbol
   our obj never produces), so it reads 0% and only pollutes unit fuzzy. This is
   the criterion the BandIKEffector cleanup used (it caught all 12 stale
   MidiInstrument/SampleZone/BoneOp entries with zero false positives, including
   the one CLASS_MIXING kept because BoneOp has no dedicated home unit). Needs
   the compiled objs present; that's why it is opt-in.

The STRING-CONTRADICTION idea from the task brief (a mapped fn whose body
references a string literal absent from the claimed source TU, e.g. Deploy's
'slider.sld') was scoped out: the dtk-split asm references rdata by relocated
label, not inline literal, so confirming it needs an rdata cross-ref pass that
balloons scope for marginal extra coverage over CLASS_MIXING/OBJ_ORPHAN (which
already catch the same mis-pins by class/symbol). Documented here, not built.

Outputs a human report to stdout and (with --json PATH) a machine report.
Exit 1 when any finding is produced (0 with --no-fail or when clean).

Usage
-----
    # lint the whole map, human report + json
    python3 tools/map_lint.py --json /tmp/map_lint.json

    # one unit (matched against the splits.txt header, case-insensitive substr)
    python3 tools/map_lint.py --unit BandIKEffector

    # only one check
    python3 tools/map_lint.py --check class_mixing

Inputs (all project-relative defaults, override with flags):
    scripts/target_symbol_map.json
    config/45410914/splits.txt
    build/45410914/asm/            (dtk-split .s artifacts)
"""

import argparse
import json
import os
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MAP = PROJECT_ROOT / "scripts" / "target_symbol_map.json"
DEFAULT_SPLITS = PROJECT_ROOT / "config" / "45410914" / "splits.txt"
DEFAULT_ASM_DIR = PROJECT_ROOT / "build" / "45410914" / "asm"

# class_mixing + size_kind run by default; scatter + obj_orphan are opt-in
# (scatter is engine-ICF-noisy; obj_orphan needs the compiled objs present).
CHECKS = ("class_mixing", "size_kind", "scatter", "obj_orphan")
DEFAULT_CHECKS = ("class_mixing", "size_kind")
DEFAULT_OBJ_DIRS = (
    PROJECT_ROOT / "build" / "45410914" / "src",
)


# ---------------------------------------------------------------------------
# MSVC mangled-name parsing
# ---------------------------------------------------------------------------

# A user-defined type in an MSVC arg/template list is encoded as a [VUT] code
# immediately preceded by pointer/ref/cv qualifiers (P/A/B/Q/E... runs) or by an
# '@' (template-argument boundary) or '$' (template head). The class name token
# runs to the next '@'. We match the code + name where the code is at the start
# of a "type" position: preceded by '@', '$', or a qualifier run, but NOT by a
# bare alpha (which would be the middle of an identifier).
_TYPE_CODE = re.compile(r"(?:^|[@$]|[PAEBQ]+)([VUT])([A-Za-z_][A-Za-z0-9_]*)@")


def _type_user_classes(s: str) -> List[str]:
    """Extract user-class names from a *signature/template-argument* fragment by
    its ``V<Name>@`` / ``U<Name>@`` type codes. Returns first-seen order,
    STL/namespace names filtered out. Recovers the element TU of an STL
    container/template symbol."""
    out: List[str] = []
    seen = set()
    for m in _TYPE_CODE.finditer(s):
        n = m.group(2)
        if n in seen:
            continue
        seen.add(n)
        if _is_user_class(n):
            out.append(n)
    return out


def _arg_and_template_fragment(mangled: str) -> str:
    """Return the portion of a mangled name where argument/template type codes
    live: the signature after the *first* ``@@`` plus any ``?$...`` template
    head argument lists. Excludes the leading head class path so a class name
    beginning with V/U/T (e.g. VocalTrackDir) is never mistaken for a type
    code."""
    parts = []
    # everything after the first @@ is the function signature (return + args)
    if "@@" in mangled:
        parts.append(mangled.split("@@", 1)[1])
    # template-argument lists: text between '?$<name>@' and its closing.
    # cheap: include every span that follows a '?$' template marker.
    for m in re.finditer(r"\?\$[A-Za-z_][A-Za-z0-9_]*@(.*?)@@", mangled):
        parts.append(m.group(1))
    return "@@".join(parts)


def mangled_classes(mangled: str) -> List[str]:
    """Return user-class names referenced by an MSVC C++ symbol, *owning class
    first*. The owning class identifies which TU the symbol belongs to.

    Resolution:
      * ``?Method@Class@@...``    -> owner = immediate class (Class).
      * ``??0Class@@`` / ``??1Class@@`` (ctor/dtor) -> owner = Class.
      * Head class is a template (``?$vector@...``) or the symbol is a free
        template function (``??$algo@...``) -> owner = the *element* user type
        pulled from the template arguments (e.g. ``vector<SampleZone>`` ->
        SampleZone), so a container helper is attributed to its element's TU.
      * Free function / data / pure-STL with no user element -> [] (benign).

    Returns [] for things with no user-class owner so the caller treats them as
    TU-local / benign noise.
    """
    if not mangled.startswith("?"):
        return []

    elem = _type_user_classes(_arg_and_template_fragment(mangled))

    if mangled.startswith("??"):
        body = mangled[2:]
        op = body[:1]
        rest = body[1:]
        if op in ("0", "1"):
            # ctor / dtor: head segments are the class path; owner = first seg.
            head = rest.split("@@", 1)[0]
            segs = [s for s in head.split("@") if s]
            owner = None
            for s in segs:
                if not s.startswith("$") and _is_user_class(s):
                    owner = s
                    break
            if owner:
                return _dedup([owner] + elem)
            return _dedup(elem)
        if op == "$":
            # template free function (??$algo@<targs>@@namespace@@...): no
            # user-class owner of its own; attribute to the element type.
            return _dedup(elem)
        # operators (??4=, ??_E vtable thunks, ??_R RTTI, ??_C strings, etc.):
        # try the immediate class after the operator code, else element type.
        head = rest.split("@@", 1)[0]
        segs = [s for s in head.split("@") if s]
        for s in segs:
            if s.startswith("$") or s.startswith("?$"):
                # operator of a template class (??4?$vector@...) -> element TU
                return _dedup(elem)
            if _is_user_class(s):
                return _dedup([s] + elem)
        return _dedup(elem)

    # ?Method@Class@@...  -- immediate class is segs[1] (segs[0] is the method).
    # Template-class methods look like ?reserve@?$vector@...@@ where segs[1]
    # begins with '$' (the '?' got split off): treat as template -> element TU.
    head = mangled[1:].split("@@", 1)[0]
    segs = head.split("@")
    if len(segs) >= 2 and segs[1]:
        cls = segs[1]
        if cls.startswith("$") or cls.startswith("?$"):
            return _dedup(elem)
        if _is_user_class(cls):
            return _dedup([cls] + elem)
        return _dedup(elem)
    # ?Name@@...  free function or data
    return _dedup(elem)


def _dedup(seq: List[str]) -> List[str]:
    out: List[str] = []
    seen = set()
    for c in seq:
        if c and c not in seen and _is_user_class(c):
            seen.add(c)
            out.append(c)
    return out


# STL / runtime container & allocator names that are TU-local instantiations and
# must NOT be treated as a foreign class family.
_STL_NAMES = {
    "vector", "list", "map", "set", "multimap", "multiset", "deque",
    "_Rb_tree", "_List_node", "_Tree", "pair", "reverse_iterator",
    "StlNodeAlloc", "allocator", "_Vector_base", "_List_base",
    "__false_type", "__true_type", "basic_string", "string", "_Bvector_base",
    "less", "equal_to", "hash", "priority_queue", "stack", "queue",
    "_Slist_node", "slist", "_Hashtable", "_Rb_tree_node", "_Rb_tree_base",
    "_List_iterator", "_Vector_iterator", "char_traits", "_Tree_iterator",
    # namespaces (appear as type-code names in nested template args)
    "stlpmtx_std", "stlp_std", "std", "stlport",
    # common STL helper/aux template heads
    "_M_erase", "_M_fill_insert", "_M_allocate_and_copy", "_M_insert_overflow",
    "__destroy_range", "__destroy_range_aux", "__uninitialized_copy",
    "__copy_backward", "deallocate", "allocate",
}


def _is_user_class(name: str) -> bool:
    """Heuristic: a name is a *user* class (vs an STL container/primitive or a
    leaked type-code fragment) if it is not in the STL set, is not a template
    head token, and looks like a real C++ identifier rather than an all-caps
    qualifier run (e.g. 'AA', 'AAXAAVBinStream' tails that leak from arg lists).
    """
    if name in _STL_NAMES:
        return False
    if name.startswith("$") or name.startswith("?"):
        return False
    if len(name) < 2:
        return False
    # all-uppercase short tokens (<=3 chars) are almost always type-code leaks
    # (PowerPC/MSVC qualifier runs A/B/E/P/Q/X/Z); real classes have lowercase.
    if name.isupper() and len(name) <= 3:
        return False
    return True


def is_stl_or_template(mangled: str) -> bool:
    """True if the symbol is purely an STL/template instantiation (no user-class
    owner outside the container family) -- these are TU-local and benign."""
    classes = mangled_classes(mangled)
    return len(classes) == 0 and ("?$" in mangled or "@stlp" in mangled)


# ---------------------------------------------------------------------------
# splits.txt parsing -- unit -> .text range
# ---------------------------------------------------------------------------

class Unit:
    __slots__ = ("name", "src", "text_lo", "text_hi", "asm_path")

    def __init__(self, name: str, src: str):
        self.name = name          # display name e.g. "BandIKEffector"
        self.src = src            # splits header e.g. "BandIKEffector.cpp"
        self.text_lo: Optional[int] = None
        self.text_hi: Optional[int] = None
        self.asm_path: Optional[Path] = None


def parse_splits(path: Path) -> List[Unit]:
    units: List[Unit] = []
    cur: Optional[Unit] = None
    in_sections = False
    with open(path) as f:
        for raw in f:
            line = raw.rstrip("\n")
            if not line.strip():
                continue
            if line == "Sections:":
                in_sections = True
                continue
            # A unit header is a non-indented line ending in ':'
            if not line[0].isspace() and line.rstrip().endswith(":"):
                hdr = line.rstrip().rstrip(":")
                if hdr == "Sections":
                    in_sections = True
                    continue
                in_sections = False
                src = hdr
                # display name = basename without .cpp
                base = os.path.basename(hdr)
                name = re.sub(r"\.cpp$", "", base)
                cur = Unit(name, src)
                units.append(cur)
                continue
            if in_sections or cur is None:
                continue
            m = re.match(r"\s*\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
            if m:
                cur.text_lo = int(m.group(1), 16)
                cur.text_hi = int(m.group(2), 16)
    return [u for u in units if u.text_lo is not None]


def resolve_asm(unit: Unit, asm_dir: Path) -> Optional[Path]:
    # src like "system/rndobj/Utl.cpp" -> asm/system/rndobj/Utl.s
    rel = re.sub(r"\.cpp$", ".s", unit.src)
    p = asm_dir / rel
    if p.exists():
        return p
    # fall back to basename
    p2 = asm_dir / (unit.name + ".s")
    if p2.exists():
        return p2
    return None


# ---------------------------------------------------------------------------
# .s artifact parsing -- VA -> (kind, size)
#   kind in {"fn","obj"}; obj = a data/except_data object, not a real function.
# ---------------------------------------------------------------------------

_FN_HDR = re.compile(r"^#\s*\.text:0x[0-9A-Fa-f]+\s*\|\s*0x([0-9A-Fa-f]+)\s*\|\s*size:\s*0x([0-9A-Fa-f]+)")
_FN_DEF = re.compile(r"^\.fn\s+(\S+),")
_OBJ_DEF = re.compile(r"^\.obj\s+(\S+),")
_SYM_DEF = re.compile(r"^\.sym\s+(\S+),")


def parse_asm_spans(path: Path) -> Dict[int, Tuple[str, int]]:
    """Return ``{va: (kind, size_bytes)}`` for every span in the asm file.

    Drives off the ``# .text:0xOFF | 0xVA | size: 0xNN`` header that dtk emits
    before *every* span (fn / data obj / lbl stub), then reads the following
    definition line to learn the kind:

      * ``.fn``  -> ("fn", size)        a real function body.
      * ``.obj`` -> ("obj", size)       a data / except_data object.
      * ``.sym`` (lbl_) -> ("stub", 0)  a size-0 relocated/instrumentation stub.

    A real ``.fn`` at a VA always wins over an ``.obj``/``.sym`` reported at the
    same VA (except_data precedes its fn).

    The header's own ``0xVA`` is where dtk measured this span in *this* asm
    file's own layout -- a position, extracted directly from the target
    bytes -- and its ``size`` is that measurement's byte count. The
    ``fn_<hex>``/``except_data_<hex>``/``lbl_<hex>`` LABEL on the following
    definition line, by contrast, encodes an assigned *identity* (via
    autoid/symbols.txt), which legitimately differs from that measured
    position once a span has been auto-id'd, target-named, or is an
    ICF-folded duplicate placed here under another function's canonical
    address. An earlier version of this function required the header VA to
    equal the label's embedded hex before trusting the header's size, on the
    (false) assumption the two numbers share one address space; in practice
    they routinely don't -- confirmed empirically across units (Splash.s
    78/79 fn spans mismatch, CharBones.s 41/43, vs Tex.s 4/57) with the
    mismatch growing with a TU's accumulated drift from the target layout.
    That gate silently zeroed real, measured, non-zero sizes for the bulk of
    auto-id'd/named spans -- exactly the population this linter exists to
    check -- manufacturing thousands of false "zero-stub" SIZE_KIND findings
    tree-wide (measured 2026-08-06: 3125 of 3322 size_kind findings, vs a
    small few genuinely 0-byte spans). Per dtk's own emission contract (a
    header immediately precedes its span's definition line, always), the
    header's size belongs to whatever span follows regardless of the label's
    identity number, so it is now trusted unconditionally.
    """
    spans: Dict[int, Tuple[str, int]] = {}
    pending_size: Optional[int] = None
    with open(path) as f:
        for raw in f:
            line = raw.rstrip("\n")
            mh = _FN_HDR.match(line)
            if mh:
                pending_size = int(mh.group(2), 16)
                continue
            mf = _FN_DEF.match(line)
            if mf:
                va = _va_from_label(mf.group(1))
                if va is not None:
                    spans[va] = ("fn", pending_size or 0)
                pending_size = None
                continue
            mo = _OBJ_DEF.match(line)
            if mo:
                va = _va_from_label(mo.group(1))
                if va is not None and spans.get(va, (None,))[0] != "fn":
                    spans[va] = ("obj", pending_size or 0)
                pending_size = None
                continue
            ms = _SYM_DEF.match(line)
            if ms:
                va = _va_from_label(ms.group(1))
                if va is not None and va not in spans:
                    spans[va] = ("stub", pending_size or 0)
                pending_size = None
                continue
    return spans


def _va_from_label(label: str) -> Optional[int]:
    # fn_822B0CD0 / except_data_822E7208 / lbl_822... -> trailing hex
    m = re.search(r"_([0-9A-Fa-f]{8})$", label)
    if m:
        return int(m.group(1), 16)
    return None


# ---------------------------------------------------------------------------
# map loading
# ---------------------------------------------------------------------------

def load_map(path: Path) -> Dict[int, str]:
    """Load address -> mangled-name from the target symbol map.

    NULL VALUES (``"0xADDR": null``) mean "deliberately unclaimed" and are
    SKIPPED -- same polarity as ``scripts/obj_target_symbol_renamer.py``, which
    hit this exact defect first and documents the semantics: a null is emitted
    by the delete path in ``tools/maprow_audit/ci1_apply.py`` and means "no
    name, this address stays anonymous".

    ⛔ This loop had no type check, so a None flowed into ``mangled_classes()``
    and killed the whole linter with
    ``AttributeError: 'NoneType' object has no attribute 'startswith'`` --
    ON VALID INPUT.  The map legitimately carries 27 nulls of 29,036 rows
    (measured 2026-08-17, lane W44-REQUEUE), so ``map_lint`` was DEAD, not
    merely fail-open: it could not lint anything, ever, and was not in CI.
    The 5 list-valued rows are metadata keys (``_denylist`` etc.), already
    excluded here by the ``0x`` key prefix test.
    """
    raw = json.load(open(path))
    out: Dict[int, str] = {}
    n_null = n_bad = 0
    for k, v in raw.items():
        if not isinstance(k, str) or not k.startswith("0x"):
            continue
        try:
            addr = int(k, 16)
        except ValueError:
            continue
        if v is None:
            n_null += 1
            continue
        if not isinstance(v, str):
            # A list/dict/number would crash the same way None did.
            print(f"WARN: non-string value for {k!r} ({type(v).__name__}) in "
                  f"{path} -- skipping", file=sys.stderr)
            n_bad += 1
            continue
        out[addr] = v
    if n_null or n_bad:
        print(f"[map] skipped {n_null} null (deliberately unclaimed) and "
              f"{n_bad} non-string row(s) in {path}", file=sys.stderr)
    return out


# ---------------------------------------------------------------------------
# compiled-obj symbol cross-check (highest-precision: a map name our own
# compiled obj never defines can never pair -> pure pollution)
# ---------------------------------------------------------------------------

import struct  # noqa: E402  (kept local to this section)


def _coff_defined_names(data: bytes) -> set:
    """Return the set of symbol names defined (section number > 0) in a COFF
    .obj. Mirrors scripts/obj_target_symbol_renamer.parse_coff_symbols but only
    needs names + section number."""
    out: set = set()
    if len(data) < 20:
        return out
    sym_off = struct.unpack_from("<I", data, 8)[0]
    n = struct.unpack_from("<I", data, 12)[0]
    if sym_off == 0 or n == 0:
        return out
    strt = sym_off + n * 18
    i = 0
    while i < n:
        e = sym_off + i * 18
        if e + 18 > len(data):
            break
        nb = data[e:e + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            so = struct.unpack_from("<I", nb, 4)[0]
            a = strt + so
            if a < len(data):
                end = data.index(b"\x00", a)
                name = data[a:end].decode("ascii", "replace")
            else:
                name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii", "replace")
        secnum = struct.unpack_from("<h", data, e + 12)[0]
        aux = data[e + 17]
        if secnum > 0 and name:
            out.add(name)
        i += 1 + aux
    return out


def find_compiled_obj(unit: Unit, obj_dirs: List[Path]) -> Optional[Path]:
    stem = re.sub(r"\.cpp$", "", os.path.basename(unit.src))
    for d in obj_dirs:
        if not d.exists():
            continue
        hits = list(d.rglob(stem + ".obj"))
        # exclude the dtk-split TARGET obj (build/.../obj/<stem>.obj); prefer
        # the per-source compiled obj under .../src/.
        hits = [h for h in hits if os.sep + "src" + os.sep in str(h)]
        if hits:
            hits.sort(key=lambda p: len(str(p)))
            return hits[0]
    return None


def check_obj_orphan(unit: Unit, in_range: List[Tuple[int, str]],
                     compiled_syms: set) -> List[dict]:
    """Flag in-range map entries whose mangled name is NOT defined by the unit's
    own compiled obj -- the renamer would write a name onto a dtk target symbol
    that our compiled obj never produces, so it can never pair (reads 0% and
    pollutes unit fuzzy). Zero-false-positive: a name absent from the compiled
    obj is, by construction, unmatchable for this unit."""
    findings: List[dict] = []
    for va, name in in_range:
        if not name.startswith("?"):
            continue  # only mangled C++ names are renamer targets
        if name in compiled_syms:
            continue
        owner = mangled_classes(name)
        findings.append({
            "check": "obj_orphan",
            "unit": unit.name,
            "va": "0x%08X" % va,
            "symbol": name,
            "owner_class": owner[0] if owner else None,
            "detail": "name not defined by compiled %s -- unpairable, pollutes "
                      "unit fuzzy" % (os.path.basename(str(unit.asm_path))
                                      if unit.asm_path else unit.name),
        })
    return findings


# ---------------------------------------------------------------------------
# checks
# ---------------------------------------------------------------------------

def unit_class_majorities(units: List[Unit],
                          amap: Dict[int, str]) -> Tuple[Dict[str, Tuple[str, int]],
                                                         Dict[str, str]]:
    """Single pass over all pinned units. Returns:
      * per_unit[unit.name] = (majority_class, majority_count) for the dominant
        user-class owner in that unit's .text range.
      * class_home[class] = unit.name -- the unit that 'owns' a class, chosen as
        (a) the unit whose name == class, else (b) the unit where that class has
        its largest majority count. Used to detect a foreign class that has its
        OWN dedicated pin showing up inside another unit (the mis-pin signal).
    """
    per_unit: Dict[str, Tuple[str, int]] = {}
    # class -> [(unit_name, count_in_unit, is_majority)]
    class_claims: Dict[str, List[Tuple[str, int, bool]]] = defaultdict(list)
    for u in units:
        cc: Counter = Counter()
        for va, name in amap.items():
            if u.text_lo <= va < u.text_hi:
                cls = mangled_classes(name)
                if cls:
                    cc[cls[0]] += 1
        if not cc:
            continue
        maj, majn = cc.most_common(1)[0]
        per_unit[u.name] = (maj, majn)
        for cls, n in cc.items():
            class_claims[cls].append((u.name, n, cls == maj))

    class_home: Dict[str, str] = {}
    for cls, claims in class_claims.items():
        # prefer the unit named exactly after the class
        named = [c for c in claims if c[0] == cls]
        if named:
            class_home[cls] = cls
            continue
        # else the unit where this class has the largest count (majority pref)
        claims_sorted = sorted(claims, key=lambda c: (c[2], c[1]), reverse=True)
        class_home[cls] = claims_sorted[0][0]
    return per_unit, class_home


def check_class_mixing(unit: Unit, in_range: List[Tuple[int, str]],
                       per_unit: Dict[str, Tuple[str, int]],
                       class_home: Dict[str, str],
                       min_foreign: int) -> List[dict]:
    """Flag in-range map entries whose owning class has its OWN dedicated pinned
    unit elsewhere -- the precise mis-pin / stale-leftover signal.

    The family that legitimately belongs to a unit's range is: the unit's own
    name-class, the unit's majority class (filename often != contained class,
    e.g. Mesh.cpp -> RndMesh), and any class that has no home of its own (helper
    classes defined only in this .cpp; STL templates). A class is FOREIGN only
    when ``class_home[class]`` is a *different* unit -- i.e. it is double-claimed
    by a unit that owns it. To suppress engine-ICF noise (a single inlined
    Symbol/Object ctor folded into many TUs), require at least ``min_foreign``
    such entries in this unit before reporting them.
    """
    own = unit.name
    majority = per_unit.get(own, (None, 0))[0]
    legit = {own}
    # The majority class is legitimately part of the unit (filename != class is
    # the norm, e.g. Mesh.cpp -> RndMesh) ONLY when that majority class is not
    # itself homed in a different pinned unit. If a foreign class with its own
    # pin dominates this range, the unit is the suspect -- do NOT whitelist it.
    if majority and class_home.get(majority, own) == own:
        legit.add(majority)

    candidates: List[dict] = []
    for va, name in in_range:
        cls = mangled_classes(name)
        owner = cls[0] if cls else None
        if owner is None or owner in legit:
            continue
        home = class_home.get(owner)
        # foreign only if the class has a *different* home unit (its own pin)
        if home is None or home == own:
            continue
        candidates.append({
            "check": "class_mixing",
            "unit": own,
            "va": "0x%08X" % va,
            "symbol": name,
            "owner_class": owner,
            "owner_home_unit": home,
            "unit_own_class": own,
            "majority_class": majority,
            "detail": "owner class '%s' is homed in unit '%s', not '%s' "
                      "(stale/mis-pinned entry)" % (owner, home, own),
        })

    # group by owner class; require the per-class count to clear min_foreign so a
    # lone ICF-folded engine ctor doesn't trip the check.
    by_owner: Dict[str, List[dict]] = defaultdict(list)
    for c in candidates:
        by_owner[c["owner_class"]].append(c)
    findings: List[dict] = []
    for owner, group in by_owner.items():
        if len(group) >= min_foreign:
            findings += group
    return findings


# compiler prolog/epilog runtime helpers -- never a TU's own function; the map
# should not really carry these, but when it does they are benign noise.
_RUNTIME_HELPER = re.compile(r"^(__(save|rest)(gpr|fpr|vmx)|_savegpr|_restgpr|"
                             r"__C_specific_handler|__security)")


def check_size_kind(unit: Unit, in_range: List[Tuple[int, str]],
                    spans: Dict[int, Tuple[str, int]], min_fn_size: int) -> List[dict]:
    findings: List[dict] = []
    for va, name in in_range:
        if _RUNTIME_HELPER.match(name):
            continue
        info = spans.get(va)
        if info is None:
            findings.append({
                "check": "size_kind",
                "unit": unit.name,
                "va": "0x%08X" % va,
                "symbol": name,
                "kind": "no-span",
                "detail": "no span at this VA in %s (stale/wrong address)"
                          % (unit.asm_path.name if unit.asm_path else "?"),
            })
            continue
        kind, size = info
        if kind == "obj":
            findings.append({
                "check": "size_kind",
                "unit": unit.name,
                "va": "0x%08X" % va,
                "symbol": name,
                "kind": "data-object",
                "detail": "VA is a data/except_data object, not a function "
                          "(ICF mis-pin onto exception data)",
            })
            continue
        if kind == "stub" or size == 0:
            # a size-0 lbl_ stub paired with a substantive method name: the map
            # claims a real body but dtk found only a relocated breadcrumb.
            if _is_substantive_method(name):
                findings.append({
                    "check": "size_kind",
                    "unit": unit.name,
                    "va": "0x%08X" % va,
                    "symbol": name,
                    "kind": "zero-stub",
                    "fn_size": 0,
                    "detail": "VA is a 0-byte stub (relocated / instrumentation), "
                              "but the symbol names a substantive method",
                })
            continue
        # real fn: flag implausibly tiny against a substantive method name
        if size and size <= min_fn_size and _is_substantive_method(name):
            findings.append({
                "check": "size_kind",
                "unit": unit.name,
                "va": "0x%08X" % va,
                "symbol": name,
                "kind": "tiny-fn",
                "fn_size": size,
                "detail": "fn span is only 0x%X bytes but symbol names a "
                          "substantive method" % size,
            })
    return findings


def _is_substantive_method(mangled: str) -> bool:
    """A method/ctor/dtor that we'd expect to have a real body (vs a thunk or a
    trivial accessor that legitimately compiles to a couple instructions)."""
    cm = mangled_classes(mangled)
    if not cm:
        return False
    # ClassName/GetType/etc. accessors are legitimately tiny -- exclude the
    # obvious trivial ones so we don't false-positive on real small fns.
    trivial = re.compile(r"@(ClassName|GetType|Mats|Type|Name)@")
    if trivial.search(mangled):
        return False
    return True


def check_scatter(amap: Dict[int, str], scatter_bytes: int) -> List[dict]:
    """Whole-map: report classes whose symbols span > scatter_bytes of VA.

    LIMITATION (documented): on the *current* map this mostly surfaces engine
    ubiquity, not mis-pins -- base classes (Object, Symbol, DataNode, Vector3,
    String, BinStream) have inlined ctors/dtors and template helpers that ICF-
    fold into hundreds of TUs across the whole 9 MB .text, so they legitimately
    'span' megabytes. It is therefore an OPT-IN check (``--check scatter``).

    Its intended use is the GemManager-class signal: a class that *should* be a
    single contiguous TU but whose oracle/map functions are smeared across
    megabytes (no clusterable home). For game classes with few inline sites that
    discriminates; for engine primitives it does not. Treat hits as candidates
    to eyeball, not as automatic failures.
    """
    by_class: Dict[str, List[int]] = defaultdict(list)
    for va, name in amap.items():
        classes = mangled_classes(name)
        if not classes:
            continue
        by_class[classes[0]].append(va)
    findings: List[dict] = []
    for cls, vas in by_class.items():
        if len(vas) < 2:
            continue
        lo, hi = min(vas), max(vas)
        span = hi - lo
        if span > scatter_bytes:
            findings.append({
                "check": "scatter",
                "class": cls,
                "count": len(vas),
                "span_bytes": span,
                "lo": "0x%08X" % lo,
                "hi": "0x%08X" % hi,
                "detail": "%d symbols of class '%s' span 0x%X bytes (>%s) -- not "
                          "a single contiguous TU" % (
                              len(vas), cls, span, _hbytes(scatter_bytes)),
            })
    findings.sort(key=lambda f: -f["span_bytes"])
    return findings


def _hbytes(n: int) -> str:
    if n >= 0x100000:
        return "%.1fMB" % (n / 0x100000)
    if n >= 0x400:
        return "%dKB" % (n // 0x400)
    return "0x%X" % n


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--map", default=str(DEFAULT_MAP),
                    help="target_symbol_map.json (default: %(default)s)")
    ap.add_argument("--splits", default=str(DEFAULT_SPLITS),
                    help="splits.txt (default: %(default)s)")
    ap.add_argument("--asm-dir", default=str(DEFAULT_ASM_DIR),
                    help="dtk-split .s dir (default: %(default)s)")
    ap.add_argument("--unit", default=None,
                    help="restrict to units whose splits header contains this "
                         "substring (case-insensitive)")
    ap.add_argument("--check", action="append", choices=CHECKS, default=[],
                    help="run only these checks (repeatable; default: all)")
    ap.add_argument("--min-fn-size", type=lambda s: int(s, 0), default=8,
                    help="fn <= this many bytes paired with a substantive "
                         "method name is flagged (default: 8)")
    ap.add_argument("--scatter-bytes", type=lambda s: int(s, 0), default=0x40000,
                    help="class VA span above this is flagged as scatter "
                         "(default: 0x40000 = 256KB)")
    ap.add_argument("--min-foreign", type=int, default=2,
                    help="report a foreign class inside a unit only when it has "
                         ">= this many entries there (suppresses lone ICF-folded "
                         "engine ctors; default: 2)")
    ap.add_argument("--obj-dir", action="append", default=[],
                    help="compiled-obj search root for the obj_orphan check "
                         "(repeatable; default: build/45410914/src)")
    ap.add_argument("--json", default=None, help="write machine report here")
    ap.add_argument("--no-fail", action="store_true",
                    help="always exit 0 even with findings")
    ap.add_argument("--quiet", action="store_true",
                    help="suppress the per-finding human lines (summary only)")
    args = ap.parse_args()

    checks = args.check or list(DEFAULT_CHECKS)
    amap = load_map(Path(args.map))
    all_units = parse_splits(Path(args.splits))
    asm_dir = Path(args.asm_dir)
    for u in all_units:
        u.asm_path = resolve_asm(u, asm_dir)

    # global majority/home computation (always over ALL units so a foreign
    # class's real home can be located even when --unit narrows the report).
    per_unit, class_home = unit_class_majorities(all_units, amap)

    if args.unit:
        needle = args.unit.lower()
        units = [u for u in all_units if needle in u.src.lower()]
    else:
        units = all_units

    obj_dirs = [Path(d) for d in args.obj_dir] or list(DEFAULT_OBJ_DIRS)
    obj_syms_cache: Dict[str, set] = {}

    all_findings: List[dict] = []

    # per-unit checks
    for u in units:
        in_range = sorted(
            ((va, name) for va, name in amap.items()
             if u.text_lo <= va < u.text_hi),
            key=lambda t: t[0])
        if not in_range:
            continue
        if "class_mixing" in checks:
            all_findings += check_class_mixing(u, in_range, per_unit,
                                               class_home, args.min_foreign)
        if "size_kind" in checks:
            if u.asm_path is None:
                all_findings.append({
                    "check": "size_kind", "unit": u.name,
                    "kind": "no-asm",
                    "detail": "no .s artifact for unit (build it before "
                              "size_kind can run)",
                })
            else:
                spans = parse_asm_spans(u.asm_path)
                all_findings += check_size_kind(u, in_range, spans, args.min_fn_size)
        if "obj_orphan" in checks:
            obj = find_compiled_obj(u, obj_dirs)
            if obj is None:
                all_findings.append({
                    "check": "obj_orphan", "unit": u.name,
                    "detail": "no compiled obj found for unit (build it first)",
                })
            else:
                key = str(obj)
                if key not in obj_syms_cache:
                    try:
                        obj_syms_cache[key] = _coff_defined_names(obj.read_bytes())
                    except Exception as exc:  # noqa: BLE001
                        obj_syms_cache[key] = set()
                        all_findings.append({
                            "check": "obj_orphan", "unit": u.name,
                            "detail": "could not read %s: %s" % (obj, exc),
                        })
                all_findings += check_obj_orphan(u, in_range, obj_syms_cache[key])

    # whole-map scatter (skip when --unit narrows scope, it's a global check)
    if "scatter" in checks and not args.unit:
        all_findings += check_scatter(amap, args.scatter_bytes)

    _report(all_findings, units, amap, args)

    if args.json:
        with open(args.json, "w") as f:
            json.dump({
                "summary": _summary(all_findings),
                "findings": all_findings,
            }, f, indent=2)
        print("wrote %s" % args.json, file=sys.stderr)

    if all_findings and not args.no_fail:
        return 1
    return 0


def _summary(findings: List[dict]) -> Dict[str, int]:
    c: Counter = Counter(f["check"] for f in findings)
    return dict(c)


def _report(findings: List[dict], units: List[Unit], amap: Dict[int, str],
            args) -> None:
    by_check: Dict[str, List[dict]] = defaultdict(list)
    for f in findings:
        by_check[f["check"]].append(f)

    print("=" * 72)
    print("target_symbol_map lint  --  %d map entries, %d pinned units"
          % (len(amap), len(units)))
    print("=" * 72)

    if "class_mixing" in by_check and not args.quiet:
        by_unit: Dict[str, List[dict]] = defaultdict(list)
        for f in by_check["class_mixing"]:
            by_unit[f["unit"]].append(f)
        print("\n[CLASS_MIXING] %d entries whose owner class is homed in a "
              "DIFFERENT pinned unit (stale / mis-pin):"
              % len(by_check["class_mixing"]))
        # ⚠ This count is NOT a backlog. Sampled 2026-08-17 (lane W44-REQUEUE)
        # over 728 parsed detail rows: 15.7% are vtable ADJUSTOR THUNKS
        # ($4PPPPPPPM@) and 12.4% are template instantiations -- both are
        # emitted into whichever TU instantiates them, so a "foreign" home is
        # EXPECTED, not a mis-pin. Most of the remainder are free functions
        # whose "owner class" this heuristic derives from the RETURN TYPE
        # (e.g. ?EnableAppChild@@YA?AVDataNode@@... -> owner "DataNode").
        # Adjudicate a row on retail bytes before treating it as a defect.
        print("  ⚠ heuristic: thunks/templates/return-type-derived owners are "
              "expected foreign -- NOT a mis-pin backlog. See load_map() notes.")
        for unit in sorted(by_unit):
            fs = by_unit[unit]
            owners = Counter("%s->home:%s" % (f["owner_class"],
                                              f["owner_home_unit"]) for f in fs)
            print("  %s  (majority=%s)  -> %d foreign: %s"
                  % (unit, fs[0]["majority_class"], len(fs), dict(owners)))
            for f in fs[:6]:
                print("       %s  %s  [%s, home=%s]"
                      % (f["va"], f["symbol"], f["owner_class"],
                         f["owner_home_unit"]))
            if len(fs) > 6:
                print("       ... +%d more" % (len(fs) - 6))

    if "size_kind" in by_check and not args.quiet:
        sk = by_check["size_kind"]
        kc = Counter(f["kind"] for f in sk)
        print("\n[SIZE_KIND] %d entries at a non-function / stub / tiny fn  (%s):"
              % (len(sk), ", ".join("%s=%d" % (k, v) for k, v in sorted(kc.items()))))
        # high-signal kinds first
        order = {"data-object": 0, "zero-stub": 1, "tiny-fn": 2,
                 "no-span": 3, "no-asm": 4}
        for f in sorted(sk, key=lambda f: (order.get(f["kind"], 9),
                                           f["unit"], f.get("va", ""))):
            extra = (" size=0x%X" % f["fn_size"]) if "fn_size" in f else ""
            print("  [%-11s] %-18s %s  %s%s"
                  % (f["kind"], f["unit"], f.get("va", ""),
                     f.get("symbol", ""), extra))

    if "obj_orphan" in by_check and not args.quiet:
        oo = by_check["obj_orphan"]
        by_unit2: Dict[str, List[dict]] = defaultdict(list)
        for f in oo:
            by_unit2[f["unit"]].append(f)
        print("\n[OBJ_ORPHAN] %d map names NOT defined by the unit's compiled "
              "obj (unpairable -> 0%% pollution):" % len(oo))
        for unit in sorted(by_unit2):
            fs = by_unit2[unit]
            print("  %s  -> %d orphan entries" % (unit, len(fs)))
            for f in fs[:8]:
                if "va" in f:
                    print("       %s  %s" % (f["va"], f["symbol"]))
                else:
                    print("       %s" % f["detail"])
            if len(fs) > 8:
                print("       ... +%d more" % (len(fs) - 8))

    if "scatter" in by_check and not args.quiet:
        print("\n[SCATTER] %d classes spanning > %s of VA (not contiguous TUs):"
              % (len(by_check["scatter"]), _hbytes(args.scatter_bytes)))
        for f in by_check["scatter"][:40]:
            print("  %-32s %4d syms  span=%s  [%s..%s]"
                  % (f["class"], f["count"], _hbytes(f["span_bytes"]),
                     f["lo"], f["hi"]))
        if len(by_check["scatter"]) > 40:
            print("  ... +%d more" % (len(by_check["scatter"]) - 40))

    print("\n" + "-" * 72)
    summ = _summary(findings)
    if summ:
        print("FINDINGS: " + ", ".join("%s=%d" % (k, v) for k, v in sorted(summ.items())))
    else:
        print("CLEAN -- no findings")
    print("-" * 72)


if __name__ == "__main__":
    sys.exit(main())
