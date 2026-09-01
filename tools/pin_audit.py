#!/usr/bin/env python3
"""pin_audit.py -- sliver / over-pin / displaced-pin detector (READ-ONLY).

Systematizes the sliver-pin vein from
``docs/decomp/research/2026-06-11-sliver-pin-hunt.md`` (S1 detectors + ALL
SEVEN false-positive filters, S5 batch-tool verdict).  Detection only: the
tool never modifies splits, maps, objs or anything else.  Application stays
per-unit (worktree + whole-binary A/B + the wave-1 honesty gate).

Signatures (S1)
---------------
  A  sliver/under-pin   compiled own-class .text  >  2 x pin_size + 0x800
  B  over-pin           pin_size  >  1.5 x compiled total .text + 0x1000
  C  displaced          >=4 own-class HEAD-OWNED map entries cluster OUTSIDE
                        the pin (gap <= 0x3000, span < 0x10000)

False-positive filters (S1, all seven baked in)
-----------------------------------------------
  F1 free-function TUs: a raw-B unit whose in-range map census is mostly its
     OWN content (names defined by its own compiled obj, or own-class methods)
     is a port-depth deficit, not an over-pin (FFT, MemHeap, keygen_xbox,
     GuitarController).  Verdict requires the census, not byte arithmetic.
  F2 argument-class extraction: cluster evidence counts only HEAD-OWNED
     symbols (the mangled head path names the class); `?OnMsg@EventTrigger@@
     ...PAVAnim@@` never counts as an "Anim" entry.
  F3 inline-scatter classes (Symbol/DataArray/DataNode/BinStream/Object...):
     out-of-pin copies are legit per-TU inline/ICF copies.  Clusters must be
     tight AND carry >= --min-substantive substantive (non-ctor/dtor/thunk/
     trivial-accessor) methods; scatter classes are additionally tagged
     `scatter_class` so the worklist consumer treats them as recon-first.
  F4 suffix-strip artifacts: a cluster sitting inside the pin of a unit that
     legitimately owns the cluster's class (Rnd entries inside Rnd.cpp's pin,
     seen from Rnd_NG.cpp) is dropped, not reported.
  F5 DC3-only content-transfer pins (Ham*/Move*...): signature-A units with
     NO in-binary displaced cluster and no rb3-Wii oracle source are dropped
     -- no evidence a real RB3 TU exists.
  F6 stub-farm mirage: clusters whose named entries average < 0x60 bytes of
     spacing are the retail coverage-breadcrumb stub farm profile
     (Accomplishment 20 fns / 0x468) -> deferred with `stub_farm_risk`.
  F7 duplicate unit basenames (CubeTex, FxSend*): every join is keyed by the
     splits.txt src path, and the report is joined as
     ``default/<src-minus-.cpp>`` -- never by bare basename.

Ranking (the EV re-rank rule)
-----------------------------
Candidates are ranked by RETAIL-MAP NAMED-METHOD COUNT inside the candidate
range -- NOT by oracle/compiled-obj fn counts, which systematically
overestimate (UILabel refutation: 64 oracle obj fns vs 10 retail named
methods interleaved in UIListDir).  est = +substantive/2 .. +2x named
(wave-3 calibration band; low confidence by design).

Interleave flagging (Object/DirLoader refutation fe603cc)
---------------------------------------------------------
A cluster located inside ANOTHER unit's pin is never a clean relocation; it
is emitted with location=INTERLEAVE and an explicit shared-boundary-recon
action.  COMDAT interleaves fail the honesty gate at apply time -- treat
these as recon-first, expect refutation.

Usage
-----
    venv/bin/python tools/pin_audit.py --json /tmp/pin_audit.json
    venv/bin/python tools/pin_audit.py --splits /tmp/old_splits.txt   # what-if
    venv/bin/python tools/pin_audit.py --unit UIList                  # debug one
    venv/bin/python tools/pin_audit.py --exclude UIList,CharEyes,...  # mark done

Inputs (defaults project-relative):
    config/45410914/splits.txt            pin ranges (src-path keyed)
    scripts/target_symbol_map.json        retail VA -> MSVC mangled name
    build/45410914/report.json            per-unit measures (optional, stats)
    build/45410914/src/**                 compiled objs (COFF, own-content)
    build/45410914/asm/**                 dtk .s artifacts (optional, spans)
"""

import argparse
import bisect
import json
import os
import re
import struct
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

TOOLS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS_DIR))
import map_lint  # noqa: E402  (parsers: splits, map, mangling, asm spans)

PROJECT_ROOT = TOOLS_DIR.parent
DEFAULT_SPLITS = PROJECT_ROOT / "config" / "45410914" / "splits.txt"
DEFAULT_MAP = PROJECT_ROOT / "scripts" / "target_symbol_map.json"
DEFAULT_REPORT = PROJECT_ROOT / "build" / "45410914" / "report.json"
DEFAULT_OBJ_ROOT = PROJECT_ROOT / "build" / "45410914" / "src"
DEFAULT_ASM_DIR = PROJECT_ROOT / "build" / "45410914" / "asm"
DEFAULT_RB3WII_SRC = PROJECT_ROOT.parent / "rb3" / "src"
DEFAULT_DC3_SRC = PROJECT_ROOT.parent / "dc3-decomp" / "src"

# --- thresholds (doc S1; overridable on the CLI) ---------------------------
A_FACTOR, A_SLACK = 2.0, 0x800
B_FACTOR, B_SLACK = 1.5, 0x1000
CLUSTER_GAP = 0x3000
CLUSTER_MIN = 4
CLUSTER_SPAN_MAX = 0x10000
SUBSTANTIVE_MIN = 3
STUBFARM_SPACING = 0x60          # F6: avg named-entry spacing below this
OWN_FRAC_PORT_DEPTH = 0.5        # F1: in-pin census own-fraction at/above this
AUTO_BLOB_FRAC = 0.7             # cluster entries unpinned at/above this
EXTEND_TO_NEXT_PIN_MAX = 0x3000  # propose hi=next pin when tail gap is small
SLIVER_PIN_MAX = 0x400           # current pin this small => "relocate" wording

# F3: classes whose ctors/templates ICF-fold into hundreds of TUs.
SCATTER_CLASSES = {
    "Symbol", "DataArray", "DataNode", "BinStream", "Object", "String",
    "ObjectDir", "Hmx", "DataNodeValue", "ObjRef", "ObjPtr", "ObjPtrList",
}

# Method names that are legitimately tiny / auto-generated -- never count as
# substantive cluster evidence.
_TRIVIAL_METHODS = {
    "ClassName", "StaticClassName", "GetType", "Type", "Name", "Mats",
    "NewObject", "InitObject", "New", "Register", "StaticNew",
}

PLATFORM_SUFFIXES = ("_Xbox", "_xbox", "_NG", "_Win", "_PS3")


# ---------------------------------------------------------------------------
# mangled-name helpers (F2: HEAD-OWNED attribution only)
# ---------------------------------------------------------------------------

def head_classes(mangled: str) -> List[str]:
    """User classes named by the mangled HEAD path only (the path that owns
    the symbol): ``?Method@Inner@Outer@@...`` -> [Inner, Outer];
    ``??0Class@@`` -> [Class].  Argument/template classes are NEVER returned
    (F2) -- ``?Fn@Unit@@...PAVAnim@@`` yields [Unit], not Anim.  Free
    functions / template free functions / pure-template heads yield []."""
    if not mangled.startswith("?"):
        return []
    body = mangled[1:]
    if body.startswith("?"):
        rest = body[1:]
        if rest[:1] == "$":
            return []                      # ??$templateFreeFn@...
        if rest[:1] == "_":
            rest = rest[2:]                # ??_G / ??_E / ??_7 ... skip code
        else:
            rest = rest[1:]                # ??0 / ??1 / ??4 ... skip code
        head = rest.split("@@", 1)[0]
        segs = [s for s in head.split("@") if s]
    else:
        head = body.split("@@", 1)[0]
        segs = head.split("@")[1:]         # segs[0] is the method name
    out: List[str] = []
    for s in segs:
        if s.startswith("$") or s.startswith("?"):
            return out                     # template path component: stop
        if map_lint._is_user_class(s):
            out.append(s)
    return out


_METHOD_NAME = re.compile(r"^\?([A-Za-z_][A-Za-z0-9_]*)@")


def is_substantive(mangled: str) -> bool:
    """A head-owned symbol that names a real method body: not a ctor/dtor/
    operator/thunk, not a trivial accessor.  This is the cluster-evidence
    currency (F3) and the ranking currency (EV re-rank rule)."""
    if not head_classes(mangled):
        return False
    m = _METHOD_NAME.match(mangled)
    if not m:
        return False                       # ??0/??1/??_G/operators
    return m.group(1) not in _TRIVIAL_METHODS


# ---------------------------------------------------------------------------
# COFF compiled-obj stats (per-symbol COMDAT .text sections)
# ---------------------------------------------------------------------------

_SYM_NOISE = re.compile(
    r"^(__unwind|__ehfuncinfo|__catchsym|__tryblocktable|__catch\$|\$|__imp_"
    r"|\.|__real|__CT|__TI|_CT|_TI)")


def parse_coff(path: Path):
    """-> (sections[{name,size}], symbols[(name, secnum, type, storage_class)])"""
    data = path.read_bytes()
    if len(data) < 20:
        return [], []
    nsec = struct.unpack_from("<H", data, 2)[0]
    sym_off = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    opt = struct.unpack_from("<H", data, 16)[0]
    strt = sym_off + nsym * 18

    def longname(raw: bytes) -> str:
        nm = raw.split(b"\x00")[0].decode("ascii", "replace")
        if nm.startswith("/"):
            try:
                so = int(nm[1:])
                end = data.index(b"\x00", strt + so)
                nm = data[strt + so:end].decode("ascii", "replace")
            except Exception:
                pass
        return nm

    secs = []
    off = 20 + opt
    for i in range(nsec):
        e = off + i * 40
        if e + 40 > len(data):
            break
        secs.append({"name": longname(data[e:e + 8]),
                     "size": struct.unpack_from("<I", data, e + 16)[0]})
    syms = []
    i = 0
    while i < nsym:
        e = sym_off + i * 18
        if e + 18 > len(data):
            break
        nb = data[e:e + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            so = struct.unpack_from("<I", nb, 4)[0]
            a = strt + so
            try:
                end = data.index(b"\x00", a)
                name = data[a:end].decode("ascii", "replace")
            except Exception:
                name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii", "replace")
        secnum = struct.unpack_from("<h", data, e + 12)[0]
        typ = struct.unpack_from("<H", data, e + 14)[0]
        scls = data[e + 16]
        aux = data[e + 17]
        syms.append((name, secnum, typ, scls))
        i += 1 + aux
    return secs, syms


class ObjStats:
    __slots__ = ("path", "total_text", "n_fns", "n_own_fns", "own_text",
                 "defined", "own_fn_names")

    def __init__(self, path: Path, own_classes: Set[str]):
        self.path = path
        secs, syms = parse_coff(path)
        text_secs = {i: s["size"] for i, s in enumerate(secs, 1)
                     if s["name"].startswith(".text")}
        self.total_text = sum(text_secs.values())
        self.defined: Set[str] = set()
        seen: Set[str] = set()
        fns: List[Tuple[str, int]] = []
        for name, secnum, typ, scls in syms:
            if secnum > 0 and name:
                self.defined.add(name)
            if (secnum in text_secs and (typ >> 4) == 2 and scls in (2, 3)
                    and name and not _SYM_NOISE.match(name)
                    and name not in seen):
                seen.add(name)
                fns.append((name, secnum))
        self.n_fns = len(fns)
        own_secs: Set[int] = set()
        self.own_fn_names: List[str] = []
        for name, secnum in fns:
            # own = head-owned by an own class, OR a TU-local template
            # instantiation whose ELEMENT is an own class (ObjPtr<CharEyes>
            # helpers are own content for size purposes; mirrors the dossier's
            # calibration numbers).  Inside the unit's own obj the element
            # attribution carries no arg-class FP risk.
            if (set(head_classes(name)) & own_classes
                    or set(map_lint.mangled_classes(name)) & own_classes):
                self.own_fn_names.append(name)
                own_secs.add(secnum)
        self.n_own_fns = len(self.own_fn_names)
        self.own_text = sum(text_secs.get(s, 0) for s in own_secs)


# ---------------------------------------------------------------------------
# unit model
# ---------------------------------------------------------------------------

class PUnit:
    __slots__ = ("src", "name", "lo", "hi", "blocks", "own", "obj", "report",
                 "source_path", "majority")

    def __init__(self, src: str, lo: int, hi: int, blocks=None):
        self.src = src                                  # F7: the join key
        self.name = re.sub(r"\.cpp$", "", os.path.basename(src))
        # lo/hi is the SPAN (covers gaps owned by other units); blocks is the
        # truth.  See map_lint.Unit -- 728 of 1,275 units are multi-block.
        self.lo, self.hi = lo, hi
        self.blocks = list(blocks) if blocks else [(lo, hi)]
        self.own: Set[str] = set()
        self.obj: Optional[ObjStats] = None
        self.report: Optional[dict] = None
        self.source_path: Optional[str] = None
        self.majority: Optional[str] = None

    @property
    def pin_size(self) -> int:
        """Bytes actually pinned -- sum of blocks, gaps EXCLUDED.

        Was ``self.hi - self.lo``, which for a multi-block unit measured only
        the LAST block (because map_lint dropped the others), and would now
        over-measure by every gap if it used the span.  Both are wrong; the
        sum is right.  pin_size feeds the A (sliver/under-pin) and B
        (over-pin) detectors, so 728 units were being screened on a wrong size.
        """
        return sum(hi - lo for lo, hi in self.blocks)


def base_stem(stem: str) -> Optional[str]:
    for suf in PLATFORM_SUFFIXES:
        if stem.endswith(suf):
            return stem[: -len(suf)]
    return None


_TOKEN_SPLIT = re.compile(r"[A-Z]+[a-z0-9]*|[a-z0-9]+")


def _name_tokens(name: str) -> Set[str]:
    """CamelCase/underscore tokens of length >= 3, lowercased.  Used to test
    whether a majority class plausibly belongs to a unit by name (RndMesh ~
    Mesh.cpp share 'mesh'; DxRnd ~ Rnd_Xbox share 'rnd'; LEAPCORE ~ System
    share nothing)."""
    return {t.lower() for t in _TOKEN_SPLIT.findall(name) if len(t) >= 3}


def _tokens_relate(a: Set[str], b: Set[str]) -> bool:
    """True when any token of one is a prefix of a token of the other
    (Part ~ RndParticleSys via part/particle)."""
    for x in a:
        for y in b:
            if x.startswith(y) or y.startswith(x):
                return True
    return False


# F5: DC3-franchise game classes (deliberate one-off content-transfer pins;
# the dossier's list).  Tree-presence alone is NOT the gate -- rnddx9/synth
# platform TUs are absent from rb3-Wii yet real on 360 (RenderState, Mat_NG).
DC3_FRANCHISE_PREFIXES = (
    "Ham", "Move", "RhythmBattle", "HollaBack", "Gesture", "SkeletonClip",
    "PoseFatalities", "FacePipeline", "Nui", "Flashcard", "Crew",
)


def _dc3_franchise(name: str) -> bool:
    for p in DC3_FRANCHISE_PREFIXES:
        if name == p or (name.startswith(p) and len(name) > len(p)
                         and not name[len(p)].islower()):
            return True
    return False


def build_units(splits_path: Path, report_path: Optional[Path],
                obj_root: Path, amap: Dict[int, str]) -> List[PUnit]:
    raw = map_lint.parse_splits(splits_path)
    units = [PUnit(u.src, u.text_lo, u.text_hi, u.blocks) for u in raw]

    # report join: "default/" + src minus .cpp (path-keyed, F7)
    runits: Dict[str, dict] = {}
    if report_path and report_path.exists():
        rep = json.load(open(report_path))
        for ru in rep.get("units", []):
            runits[ru["name"]] = ru
    for u in units:
        key = "default/" + re.sub(r"\.cpp$", "", u.src)
        u.report = runits.get(key)
        if u.report:
            u.source_path = (u.report.get("metadata") or {}).get("source_path")

    # own-class families: stem (+ platform-stripped base)
    stems = {u.name for u in units}
    for u in units:
        u.own = {u.name}
        b = base_stem(u.name)
        if b:
            u.own.add(b)

    # majority head-class of in-pin map entries (filename != class is the
    # engine norm: Mesh.cpp -> RndMesh).  Claim it as own only when no other
    # unit is literally named after it.
    vas = sorted(amap)
    for u in units:
        i = bisect.bisect_left(vas, u.lo)
        cc: Counter = Counter()
        while i < len(vas) and vas[i] < u.hi:
            hc = head_classes(amap[vas[i]])
            if hc:
                cc[hc[-1]] += 1            # outermost class
            i += 1
        if cc:
            maj, majn = cc.most_common(1)[0]
            u.majority = maj
            # Claim the majority class as own only when it carries real weight
            # (>=3 entries -- a junk-scatter pin must not legitimize its
            # squatters, the SongDB case), no other unit is literally named
            # after it (F4), AND it shares a name token with the unit stem
            # (Mesh->RndMesh, Rnd_Xbox->DxRnd yes; System->LEAPCORE no -- a
            # mis-pinned/vendor squatter majority must not become "own").
            if (majn >= 3 and (maj not in stems or maj in u.own)
                    and _tokens_relate(_name_tokens(maj), _name_tokens(u.name))):
                u.own.add(maj)

    # compiled obj (path-preference resolution per dossier S6)
    for u in units:
        p = None
        if u.source_path:
            cand = PROJECT_ROOT / "build" / "45410914" / Path(
                u.source_path).with_suffix(".obj")
            if cand.exists():
                p = cand
        if p is None and obj_root.exists():
            hits = list(obj_root.rglob(u.name + ".obj"))
            if hits:
                # prefer a hit whose tail matches the splits src dir
                srcdir = os.path.dirname(u.src)
                if srcdir:
                    pref = [h for h in hits if srcdir in str(h)]
                    hits = pref or hits
                hits.sort(key=lambda h: len(str(h)))
                p = hits[0]
        if p is not None:
            try:
                u.obj = ObjStats(p, u.own)
            except Exception:
                u.obj = None
    return units


# ---------------------------------------------------------------------------
# oracle presence (F5)
# ---------------------------------------------------------------------------

def cpp_basenames(root: Path) -> Set[str]:
    if not root.exists():
        return set()
    return {p.stem for p in root.rglob("*.cpp")}


# ---------------------------------------------------------------------------
# clustering + location
# ---------------------------------------------------------------------------

def cluster_vas(entries: List[Tuple[int, str]], gap: int) -> List[List[Tuple[int, str]]]:
    out: List[List[Tuple[int, str]]] = []
    cur: List[Tuple[int, str]] = []
    last = None
    for va, name in sorted(entries):
        if last is not None and va - last > gap:
            out.append(cur)
            cur = []
        cur.append((va, name))
        last = va
    if cur:
        out.append(cur)
    return out


class PinIndex:
    def __init__(self, units: List[PUnit]):
        # One span PER BLOCK, not per unit.  Indexing by (lo, hi) span made
        # at() claim every gap between a multi-block unit's blocks -- code
        # that belongs to OTHER units -- and made gap_around() blind to any
        # gap interior to a unit.
        self.spans = sorted((b0, b1, u) for u in units for b0, b1 in u.blocks)
        self.los = [s[0] for s in self.spans]

    def at(self, va: int) -> Optional[PUnit]:
        i = bisect.bisect_right(self.los, va) - 1
        if i >= 0 and self.spans[i][0] <= va < self.spans[i][1]:
            return self.spans[i][2]
        return None

    def gap_around(self, lo: int, hi: int) -> Tuple[Optional[Tuple[int, int, str]],
                                                    Optional[Tuple[int, int, str]]]:
        """(prev_pin, next_pin) around [lo,hi) as (lo,hi,src) tuples."""
        prev = nxt = None
        for s, e, u in self.spans:
            if e <= lo and (prev is None or e > prev[1]):
                prev = (s, e, u.src)
            if s >= hi and nxt is None:
                nxt = (s, e, u.src)
        return prev, nxt


# ---------------------------------------------------------------------------
# audit
# ---------------------------------------------------------------------------

def audit(units: List[PUnit], amap: Dict[int, str], rb3wii: Set[str],
          dc3: Set[str], args) -> dict:
    pin_index = PinIndex(units)
    stems = {u.name for u in units}
    # src -> unit, for the GAINCELL test below (see F8_host_already_pairs).
    unit_by_src: Dict[str, PUnit] = {u.src: u for u in units}
    vas = sorted(amap)
    head_list_by_va: Dict[int, List[str]] = {
        va: head_classes(name) for va, name in amap.items()}
    head_by_va: Dict[int, frozenset] = {
        va: frozenset(h) for va, h in head_list_by_va.items()}

    srctext_cache: Dict[str, str] = {}

    def unit_srctext(u: PUnit) -> str:
        """Unit's own source text (for free-function ownership: an unported
        own free fn is in the source but NOT in the compiled obj -- FFT)."""
        if u.source_path is None:
            return ""
        if u.source_path not in srctext_cache:
            p = PROJECT_ROOT / u.source_path
            try:
                srctext_cache[u.source_path] = p.read_text(errors="replace")
            except Exception:
                srctext_cache[u.source_path] = ""
        return srctext_cache[u.source_path]

    _FREE_BASE = re.compile(r"^\?([A-Za-z_][A-Za-z0-9_]{3,})@@")

    def entries_in(lo: int, hi: int) -> List[Tuple[int, str]]:
        i = bisect.bisect_left(vas, lo)
        out = []
        while i < len(vas) and vas[i] < hi:
            out.append((vas[i], amap[vas[i]]))
            i += 1
        return out

    candidates: List[dict] = []
    filtered: List[dict] = []
    deferred: List[dict] = []

    for u in units:
        rep_m = (u.report or {}).get("measures", {}) if u.report else {}
        mf = rep_m.get("matched_functions") or 0
        tf = rep_m.get("total_functions") or 0
        ustats = {
            "unit": u.name, "src": u.src,
            # ⚠ size != hi - lo for a multi-block unit, and that is CORRECT:
            # lo/hi is the SPAN (gaps included, and the gaps belong to other
            # units), size is the bytes actually pinned.  nblocks says which
            # kind of row you are looking at -- do not "fix" the mismatch.
            "pin": {"lo": hx(u.lo), "hi": hx(u.hi), "size": hx(u.pin_size),
                    "nblocks": len(u.blocks)},
            "mf": mf, "tf": tf,
            "obj": ({"path": str(u.obj.path.relative_to(PROJECT_ROOT)),
                     "total_text": hx(u.obj.total_text),
                     "own_text": hx(u.obj.own_text),
                     "n_fns": u.obj.n_fns, "n_own_fns": u.obj.n_own_fns}
                    if u.obj else None),
        }

        a_fired = bool(u.obj) and u.obj.own_text > A_FACTOR * u.pin_size + A_SLACK
        b_fired = bool(u.obj) and u.pin_size > B_FACTOR * u.obj.total_text + B_SLACK

        # ---- in-pin census (F1, B verdicts) -------------------------------
        in_pin = entries_in(u.lo, u.hi)
        own_in = foreign_in = 0
        foreign_by_class: Dict[str, List[Tuple[int, str]]] = defaultdict(list)
        for va, name in in_pin:
            hcs = head_by_va[va]
            is_own = (u.obj and name in u.obj.defined) or bool(hcs & u.own)
            if not is_own and not hcs:
                # headless = free function.  An UNPORTED own free fn is absent
                # from the compiled obj but present in the unit's source (F1,
                # the FFT fft_altivec case).
                m = _FREE_BASE.match(name)
                if m and m.group(1) in unit_srctext(u):
                    is_own = True
            if is_own:
                own_in += 1
            else:
                # attribute foreigners by the innermost head class that is a
                # pinned unit's stem, else the outermost head class (method
                # owners only; STL/template/free helpers don't accuse -- F2/F3)
                hl = head_list_by_va[va]
                if hl and not map_lint.is_stl_or_template(name):
                    foreign_in += 1
                    key = next((s for s in hl if s in stems), hl[-1])
                    foreign_by_class[key].append((va, name))
        own_frac = own_in / max(1, own_in + foreign_in)

        # ---- signature B verdict (carve > port-depth > sparse-scatter) ----
        if b_fired:
            carves = []
            for cls, ents in foreign_by_class.items():
                for cl in cluster_vas(ents, CLUSTER_GAP):
                    cl_subs = [e for e in cl if is_substantive(e[1])]
                    if len(cl) >= CLUSTER_MIN and len(cl_subs) >= args.min_substantive:
                        carves.append((cls, cl, cl_subs))
            if carves:
                for cls, cl, cl_subs in carves:
                    clo, chi = cl[0][0], cl[-1][0]
                    home = next((w for w in units if cls in w.own), None)
                    candidates.append({
                        **ustats, "sig": "B",
                        "action": "carve %s TU out of %s pin "
                                  "[0x%08X..0x%08X] (.s recon to draw the "
                                  "boundary; %s)" % (
                                      cls, u.name, clo, chi,
                                      ("home unit %s currently pinned at %s"
                                       % (home.name, home and hx(home.lo)))
                                      if home else "no pinned home unit"),
                        "range": {"lo": hx(clo), "hi": hx(chi)},
                        "named": len(cl), "substantive": len(cl_subs),
                        "sample": [n for _, n in cl_subs[:6]],
                        "location": "INSIDE_PIN:%s" % u.src,
                        "flags": ["carve"],
                        "carve_class": cls,
                        "carve_home_unit": home.src if home else None,
                        "est": est_str(len(cl_subs), len(cl)),
                    })
            elif own_frac >= args.own_frac or foreign_in < CLUSTER_MIN:
                filtered.append({**ustats, "sig": "B", "reason": "F1_port_depth",
                                 "detail": "raw over-pin but in-range census is "
                                           "%d own vs %d foreign (own_frac %.2f) "
                                           "-- pin correct, deficit is port "
                                           "depth" % (own_in, foreign_in, own_frac)})
            else:
                deferred.append({**ustats, "sig": "B",
                                 "reason": "sparse_scatter",
                                 "detail": "over-pin but in-range census is "
                                           "sparse scatter (%d own / %d "
                                           "foreign, no coherent foreign "
                                           "cluster) -- needs fingerprint/"
                                           "Ghidra ID, not a boundary move "
                                           "(AsyncFileHolmes class)"
                                           % (own_in, foreign_in)})

        # ---- signature C: displaced own clusters --------------------------
        out_entries = []
        for va in vas:
            if u.lo <= va < u.hi:
                continue
            if not (head_by_va[va] & u.own):
                continue
            # namespace-claim guard: when an INNER head segment is itself a
            # pinned unit's class, that unit wins attribution
            # (?x@TDStretch@soundtouch@@ belongs to TDStretch.cpp, never to a
            # SoundTouch.cpp that token-matched the namespace).
            hl = head_list_by_va[va]
            if any(s in stems and s not in u.own for s in hl[:-1]):
                continue
            out_entries.append((va, amap[va]))
        located_cluster = False
        for cl in cluster_vas(out_entries, CLUSTER_GAP):
            if len(cl) < CLUSTER_MIN:
                continue
            clo, chi = cl[0][0], cl[-1][0]
            if chi - clo >= CLUSTER_SPAN_MAX:
                continue
            subs = [e for e in cl if is_substantive(e[1])]
            if len(subs) < args.min_substantive:
                continue
            # location votes
            hosts: Counter = Counter()
            unpinned = 0
            for va, _ in cl:
                h = pin_index.at(va)
                if h is None:
                    unpinned += 1
                elif h is not u:
                    hosts[h.src] += 1
            n = len(cl)
            cluster_classes = {c for va, nm in cl for c in head_classes(nm)} & u.own
            # F4: inside the pin of a unit that owns the class -> legit home
            if hosts:
                top_src, top_n = hosts.most_common(1)[0]
                host = next(w for w in units if w.src == top_src)
                if cluster_classes & host.own:
                    filtered.append({**ustats, "sig": "C",
                                     "reason": "F4_class_home",
                                     "detail": "cluster [0x%08X..0x%08X] sits in "
                                               "%s's pin which legitimately owns "
                                               "class(es) %s" % (
                                                   clo, chi, host.name,
                                                   sorted(cluster_classes & host.own))})
                    continue
            located_cluster = True
            # ranking census over the candidate range MINUS the current pin.
            # Only OWN-attributed names rank (EV re-rank rule); foreign named
            # methods in the range are a contamination warning (honesty-gate
            # risk), and STL/template/free entries are neutral ride-alongs.
            rng = [e for e in entries_in(clo, chi + 1)
                   if not (u.lo <= e[0] < u.hi)]
            named_all: List[Tuple[int, str]] = []
            subs_all: List[Tuple[int, str]] = []
            foreign_rng: List[Tuple[int, str]] = []
            for va2, nm2 in rng:
                hl2 = head_list_by_va[va2]
                if (head_by_va[va2] & u.own
                        and not any(s in stems and s not in u.own
                                    for s in hl2[:-1])):
                    named_all.append((va2, nm2))
                    if is_substantive(nm2):
                        subs_all.append((va2, nm2))
                elif hl2 and not map_lint.is_stl_or_template(nm2):
                    foreign_rng.append((va2, nm2))
            spacing = (chi - clo) // max(1, n - 1)
            flags = []
            if len(foreign_rng) >= 3:
                flags.append("foreign_named_in_range:%d" % len(foreign_rng))
            if cluster_classes & SCATTER_CLASSES:
                flags.append("scatter_class")
            if spacing < args.stubfarm_spacing:
                flags.append("stub_farm_risk")
            if u.obj is None:
                flags.append("no_compiled_obj")   # synth-belt: nothing to pair
            elif u.obj.total_text < 0x500:
                flags.append("thin_obj_EV_low")   # stub scaffold (Stream.obj)

            sig = "A+C" if a_fired else "C"
            prev_pin, next_pin = pin_index.gap_around(clo, chi + 1)
            dc3_only_unit = _dc3_franchise(u.name)
            if not (_oracle_names(u) & rb3wii):
                flags.append("no_rb3wii_oracle")
            if unpinned / n >= args.auto_blob_frac:
                location = "AUTO_BLOB"
                below = [va for va, _ in cl if va < u.lo]
                above = [va for va, _ in cl if va >= u.hi]

                def pins_between(a: int, b: int) -> List[PUnit]:
                    return [w for s, e, w in pin_index.spans
                            if s >= a and e <= b and w is not u]

                if below and above:
                    # cluster brackets the pin (FileMerger/PostProc profile)
                    action_kind = "extend lo+hi"
                    plo, phi = clo, _propose_hi(chi, next_pin)
                elif above:
                    between = pins_between(u.hi, clo)
                    if not between:
                        action_kind = "extend hi"
                        plo, phi = u.lo, _propose_hi(chi, next_pin)
                    elif all(w.pin_size <= SLIVER_PIN_MAX for w in between):
                        # only sliver pin(s) squat between pin hi and the
                        # cluster (MovieSys-on-SongMgr profile): compound move
                        action_kind = ("extend hi after relocating sliver(s) "
                                       + ",".join(w.name for w in between))
                        plo, phi = u.lo, _propose_hi(chi, next_pin)
                        flags.append("requires_sliver_eviction")
                    else:
                        action_kind = ("relocate" if u.pin_size <= SLIVER_PIN_MAX
                                       or mf == 0 else "second cluster -- recon")
                        plo, phi = clo, _propose_hi(chi, next_pin)
                elif below:
                    between = pins_between(chi + 1, u.lo)
                    if not between:
                        action_kind = "extend lo"
                        plo, phi = clo, u.hi
                    elif all(w.pin_size <= SLIVER_PIN_MAX for w in between):
                        action_kind = ("extend lo after relocating sliver(s) "
                                       + ",".join(w.name for w in between))
                        plo, phi = clo, u.hi
                        flags.append("requires_sliver_eviction")
                    else:
                        action_kind = ("relocate" if u.pin_size <= SLIVER_PIN_MAX
                                       or mf == 0 else "second cluster -- recon")
                        plo, phi = clo, _propose_hi(chi, next_pin)
                else:
                    action_kind = "relocate"
                    plo, phi = clo, _propose_hi(chi, next_pin)
                action = ("%s pin to [0x%08X..0x%08X) (AUTO-BLOB; prev %s nxt %s)"
                          % (action_kind, plo, phi,
                             pin_desc(prev_pin), pin_desc(next_pin)))
                if action_kind.startswith("relocate") and mf > 0:
                    flags.append("drops_%d_existing_matches" % mf)
            else:
                host_src = hosts.most_common(1)[0][0] if hosts else "?mixed"
                if dc3_only_unit:
                    # F5: a DC3-only class "displaced" into another unit's pin
                    # is fuzzy-transferred naming on structurally-similar RB3
                    # fns (HamCamShot-in-BandCamShot), not a hidden TU.
                    filtered.append({**ustats, "sig": sig,
                                     "reason": "F5_dc3_only_interleave",
                                     "detail": "dc3-only class cluster "
                                               "[0x%08X..0x%08X] inside %s's pin"
                                               " = content-transfer naming, not"
                                               " a TU" % (clo, chi, host_src)})
                    continue
                location = "INTERLEAVE:%s" % host_src
                flags.append("interleave")
                action = ("shared-boundary recon with %s -- %d/%d entries inside "
                          "its pin; interleave risk (Object/DirLoader precedent "
                          "fe603cc: expect COMDAT interleave refutation unless "
                          "per-fn .s proves contiguity)" % (
                              host_src, sum(hosts.values()), n))
                plo, phi = clo, chi

            # pairing readiness: realized gain needs OUR compiled obj to define
            # the named methods (wave-3 calibration precondition: "source
            # already compiled and map entries exist")
            defined = sum(1 for _, nm2 in named_all
                          if u.obj and nm2 in u.obj.defined)

            # ---- GAINCELL: can a re-home pay AT ALL? -------------------
            # objdiff pairs target<->base BY NAME PER UNIT, so moving an
            # address to us pays through exactly ONE channel: names OUR obj
            # defines that the HOST's obj does not.  If that set is empty the
            # host already pairs everything and the move is Delta 0 at best --
            # and NEGATIVE where the asymmetry runs backwards (lane
            # SHAREDBOUND measured Rnd's host defining 43/44 against our 0, so
            # a re-home would have DESTROYED 43 pairings).
            #
            # Measured 2026-09-01: GAINCELL == 0 on ALL 29 INTERLEAVE rows,
            # every one of them genuine retail COMDAT layout rather than a
            # mis-pin.  The house repair for these is SOURCE-side --
            # `#include "the.cpp"`, already applied at 254 sites tree-wide --
            # never a pin move.  Filing them as candidates cost a full lane.
            gaincell = None
            if location.startswith("INTERLEAVE:") and u.obj:
                host_u = unit_by_src.get(host_src)
                if host_u is not None and host_u.obj:
                    gaincell = sum(
                        1 for _, nm2 in named_all
                        if nm2 in u.obj.defined
                        and nm2 not in host_u.obj.defined)

            row = {**ustats, "sig": sig, "action": action,
                   "range": {"lo": hx(plo), "hi": hx(phi)},
                   "cluster": {"lo": hx(clo), "hi": hx(chi), "n_own": n,
                               "own_substantive": len(subs),
                               "avg_spacing": hx(spacing)},
                   "named": len(named_all), "substantive": len(subs_all),
                   "obj_defines": "%d/%d" % (defined, len(named_all)),
                   "sample": [nm for _, nm in subs[:6]],
                   "location": location, "flags": flags,
                   "prev_pin": pin_desc(prev_pin), "next_pin": pin_desc(next_pin),
                   "est": est_str(len(subs_all), len(named_all))}
            if gaincell is not None:
                row["gaincell"] = gaincell
            if named_all and defined == 0:
                row["flags"] = flags + ["obj_defines_none_yet"]
            if gaincell == 0:
                # Both branches are unpayable TODAY, but for different reasons
                # and with different futures -- do not collapse them.  A
                # drained-vein verdict is only as broad as its cause (lane
                # THUNK3, 2026-09-01: a closure scoped to one operation
                # silently blocked +16 fns of work under another).
                if defined == 0:
                    # We compile nothing of this cluster yet, so of course we
                    # transfer nothing.  REOPENS on its own once the source
                    # lands -- this is a "not yet", not a structural refutation.
                    row["reason"] = "F8a_we_define_nothing_yet"
                    row["detail"] = (
                        "GAINCELL=0 because our obj defines 0/%d cluster "
                        "names -- unpayable TODAY, but re-check once this "
                        "source is compiled." % len(named_all))
                else:
                    # We define them AND the host defines them too: the host
                    # already pairs everything a re-home could transfer.
                    # Structural, and it does not reopen.
                    row["reason"] = "F8_host_already_pairs"
                    row["detail"] = (
                        "GAINCELL=0 with our obj defining %d/%d: %s's obj "
                        "defines every one of them too, so a re-home "
                        "transfers no pairing and can only lose them.  "
                        "Genuine COMDAT interleave; the repair, if any, is "
                        "source-side (#include the .cpp)."
                        % (defined, len(named_all), host_src))
                filtered.append(row)
                continue
            if ("stub_farm_risk" in flags or "no_compiled_obj" in flags
                    or "thin_obj_EV_low" in flags):
                row["reason"] = ("F6_stub_farm" if "stub_farm_risk" in flags
                                 else "no_or_thin_obj_EV0")
                deferred.append(row)
            else:
                candidates.append(row)

        # ---- signature A without any located cluster ----------------------
        if a_fired and not located_cluster:
            if _dc3_franchise(u.name) and not (_oracle_names(u) & rb3wii):
                filtered.append({**ustats, "sig": "A", "reason": "F5_dc3_only",
                                 "detail": "no in-binary displaced cluster and "
                                           "no rb3-Wii oracle source -- DC3-only "
                                           "content-transfer pin, not a candidate"})
            else:
                _, next_pin = pin_index.gap_around(u.lo, u.hi)
                gap = (next_pin[0] - u.hi) if next_pin else 0
                deferred.append({**ustats, "sig": "A",
                                 "reason": "blind_extension",
                                 "detail": "own .text %s >> pin %s but no "
                                           "out-of-pin map cluster; TU may "
                                           "continue past hi (gap-to-next %s). "
                                           "Blind extension + honesty gate only;"
                                           " zero named evidence -> rank bottom"
                                           % (hx(u.obj.own_text),
                                              hx(u.pin_size), hx(gap)),
                                 "next_pin": pin_desc(next_pin),
                                 "est": "+0-? (no named evidence)"})

    # cross-cluster obstacle pass: a proposed extension/relocation range that
    # swallows ANOTHER unit's located cluster will trip the honesty gate
    # (>=8-contiguous foreign named run; Part-extension-vs-Accomplishment-
    # stub-farm profile).  Flag, don't drop -- the boundary may be trimmable.
    obstacles = []
    for row in candidates + deferred:
        cl = row.get("cluster")
        if cl:
            obstacles.append((int(cl["lo"], 16), int(cl["hi"], 16), row["unit"],
                              "stub_farm_risk" in row.get("flags", [])))
    for row in candidates:
        rg = row.get("range")
        if not rg:
            continue
        plo, phi = int(rg["lo"], 16), int(rg["hi"], 16)
        for olo, ohi, ouni, ostub in obstacles:
            if ouni == row["unit"]:
                continue
            ov = min(phi, ohi) - max(plo, olo)
            if ov > 0 and ov >= (ohi - olo) // 2:
                row.setdefault("flags", []).append(
                    ("crosses_stub_farm:%s" if ostub
                     else "overlaps_cluster_of:%s") % ouni)

    # EV re-rank rule: retail-map named/substantive counts, desc.
    candidates.sort(key=lambda c: (-c.get("substantive", 0), -c.get("named", 0)))
    return {"candidates": candidates, "filtered": filtered, "deferred": deferred}


def _oracle_names(u: PUnit) -> Set[str]:
    names = {u.name}
    b = base_stem(u.name)
    if b:
        names.add(b)
    return names


def _propose_hi(chi: int, next_pin) -> int:
    """Last named VA -> proposed pin hi: extend-to-next-pin when the tail gap
    is small (clean per wave-3), else last-entry+pad with refine note."""
    if next_pin and next_pin[0] - chi <= EXTEND_TO_NEXT_PIN_MAX:
        return next_pin[0]
    return (chi + 0x200) & ~0xF


def pin_desc(p) -> Optional[str]:
    if not p:
        return None
    return "%s@[0x%08X..0x%08X)" % (p[2], p[0], p[1])


def est_str(substantive: int, named: int) -> str:
    return "+%d-%d (0.5x substantive .. 2x named; wave-3 calibration, low conf)" \
        % (max(1, substantive // 2), max(2, 2 * named))


def hx(v: int) -> str:
    return "0x%X" % v


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--splits", default=str(DEFAULT_SPLITS))
    ap.add_argument("--map", default=str(DEFAULT_MAP))
    ap.add_argument("--report", default=str(DEFAULT_REPORT))
    ap.add_argument("--obj-root", default=str(DEFAULT_OBJ_ROOT))
    ap.add_argument("--asm-dir", default=str(DEFAULT_ASM_DIR))
    ap.add_argument("--rb3wii-src", default=str(DEFAULT_RB3WII_SRC))
    ap.add_argument("--dc3-src", default=str(DEFAULT_DC3_SRC))
    ap.add_argument("--unit", default=None,
                    help="restrict output to units whose src contains this "
                         "(case-insensitive; detection still runs globally)")
    ap.add_argument("--exclude", default="",
                    help="comma list of unit names to MARK excluded "
                         "(done/landed/refuted/in-flight) in the output")
    ap.add_argument("--min-substantive", type=int, default=SUBSTANTIVE_MIN)
    ap.add_argument("--own-frac", type=float, default=OWN_FRAC_PORT_DEPTH)
    ap.add_argument("--stubfarm-spacing", type=lambda s: int(s, 0),
                    default=STUBFARM_SPACING)
    ap.add_argument("--auto-blob-frac", type=float, default=AUTO_BLOB_FRAC)
    ap.add_argument("--json", default=None, help="write the worklist here")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    amap = map_lint.load_map(Path(args.map))
    units = build_units(Path(args.splits), Path(args.report),
                        Path(args.obj_root), amap)
    rb3wii = cpp_basenames(Path(args.rb3wii_src))
    dc3 = cpp_basenames(Path(args.dc3_src))

    res = audit(units, amap, rb3wii, dc3, args)

    excl = {e.strip() for e in args.exclude.split(",") if e.strip()}

    def _is_excluded(name: Optional[str]) -> bool:
        if not name:
            return False
        stem = re.sub(r"\.cpp$", "", os.path.basename(name))
        return stem in excl or (base_stem(stem) or stem) in excl

    for c in res["candidates"]:
        c["excluded"] = (_is_excluded(c["unit"])
                         or _is_excluded(c.get("carve_class"))
                         or _is_excluded(c.get("carve_home_unit")))

    if args.unit:
        needle = args.unit.lower()
        for k in ("candidates", "filtered", "deferred"):
            res[k] = [r for r in res[k] if needle in r["src"].lower()]

    meta = {
        "generated": time.strftime("%Y-%m-%d %H:%M:%S"),
        "splits": args.splits, "map": args.map, "report": args.report,
        "n_units": len(units), "n_map_entries": len(amap),
        "baseline_matched": _baseline(Path(args.report)),
        "thresholds": {"A": "own_text > %gx pin + 0x%X" % (A_FACTOR, A_SLACK),
                       "B": "pin > %gx total_text + 0x%X" % (B_FACTOR, B_SLACK),
                       "cluster": "gap<=0x%X n>=%d span<0x%X substantive>=%d"
                                  % (CLUSTER_GAP, CLUSTER_MIN,
                                     CLUSTER_SPAN_MAX, args.min_substantive)},
    }
    out = {"meta": meta, **res}

    if not args.quiet:
        print("pin_audit: %d units, %d map entries -> %d candidates, "
              "%d filtered, %d deferred"
              % (len(units), len(amap), len(res["candidates"]),
                 len(res["filtered"]), len(res["deferred"])))
        for c in res["candidates"][:40]:
            print("  [%-3s] %-22s %s%s  named=%d sub=%d  %s  %s"
                  % (c["sig"], c["unit"],
                     "EXCL " if c.get("excluded") else "",
                     c.get("location", ""), c.get("named", 0),
                     c.get("substantive", 0),
                     c.get("range", {}).get("lo", "?"),
                     ",".join(c.get("flags", []))))
        if len(res["candidates"]) > 40:
            print("  ... +%d more" % (len(res["candidates"]) - 40))

    if args.json:
        with open(args.json, "w") as f:
            json.dump(out, f, indent=1)
        print("wrote %s" % args.json, file=sys.stderr)
    return 0


def _baseline(report_path: Path) -> Optional[int]:
    try:
        return json.load(open(report_path))["measures"]["matched_functions"]
    except Exception:
        return None


if __name__ == "__main__":
    sys.exit(main())
