#!/usr/bin/env python3
"""Pinning queue from the TARGET SYMBOL MAP — a TU5-native identification source.

v2 (2026-08-06, lane PIN-C). Queue artifact:
`docs/decomp/pin-queue-symmap-v2-2026-08-06.tsv`. The v1 artifact
(`docs/decomp/pin-queue-symmap-2026-08-06.tsv`) is a dated record referenced by
the PIN-A/PIN-B lane reports — it is NOT regenerable by this script anymore and
must not be overwritten.

Why this exists
---------------
`tools/pin_candidates.py` merges five identification oracles (bindiff,
callgraph, RTTI, vtable, string-autoid) — but **all four of its oracle JSONs are
TU0-era and DEAD**: the 2026-07-15 TU0->TU5 flip re-laid-out `.text`, so only
2.7-5.5% of their addresses are real function starts (chance ~2-3%).
`dead_index_guard` correctly refuses them and the ranker emits 0 rows. It is
inert until those oracles are regenerated.

`scripts/target_symbol_map.json` is **not** dead. Measured 2026-08-06:
**27,952 of 28,009 addresses (99.80%) are real `.text` function starts.** It is
the live TU5 identification oracle, and thousands of the functions it names sit
outside every explicit pin — i.e. in dtk's `auto_*` leftover units, where they
have no source file and can never be matched. For those, identification is
already done and **only the pin is missing**.

This groups those named-but-unpinned functions into contiguous address runs,
**sub-splits each run at DC3-linker-map object boundaries**, and emits one
candidate `.text` span per object with a suggested unit path.

    python3 tools/pin_from_symnames.py                          # summary + head
    python3 tools/pin_from_symnames.py -o docs/decomp/queue.tsv --min-fns 3
    python3 tools/pin_from_symnames.py --emit-snippets 0x829B2EC8

The DC3 linker map as an OBJECT oracle (v2's main change)
---------------------------------------------------------
`../dc3-decomp/orig/373307D9/ham_xbox_r.map` is the leaked MSVC linker map for
Dance Central 3 — same Milo engine, same XDK generation, same compiler. Its
**addresses are DC3's and are USELESS here**; they are never read. What
transfers is the last column: `symbol -> Lib:Object`. We join on the exact MSVC
mangled name we already have from `scripts/target_symbol_map.json`:

    our addr --(target map)--> mangled name --(dc3 map)--> xgraphics:optimize.obj

Measured on the current unpinned population (4,354 functions): **4,328 resolve
to exactly one object (99.40%)**, 2 are ambiguous (name in >=2 objects), 24 are
unresolved. That is far better than the 270/308-rows figure lane PIN-B measured
by hand, because PIN-B counted rows and this counts members. (Counting *any*
last-column token as an object would read 99.84% / 5 unresolved — the extra 19
are names DC3 satisfies from an import thunk, which is not an object. The
stricter number is the true one.)

⚠ **Object attribution is NAME-BASED, NOT BYTE-VERIFIED.** RB3 links a
potentially different XDK build than DC3 does; a vendor library could have been
refactored between the two builds so that a function moved to another object,
and a name that DC3 places in `a.obj` would then be wrong here. The evidence
that this is usually fine is the *coherence* of the result (long uninterrupted
runs of one object, boundaries landing exactly in inter-object gaps), not a
byte comparison. Adjudicate a surprising boundary against retail bytes.

⚠ Entries whose object token is not a real `.obj` — `Lib:xam.xex@21173.0+1861.0`
(import thunks) and `<absolute>` — are treated as **UNRESOLVED**, not as objects.

How runs are sub-split (the exact rule)
---------------------------------------
1. Members = every named, unpinned, non-denylisted function, sorted by address.
   **Templates are members** (see "Template continuity" below).
2. Runs = maximal chains where the gap from one member's end to the next
   member's start is `<= --gap`.
3. Inside a run, walk members in address order. A member with no object
   resolution never cuts. A member whose object differs from the segment's
   current object **starts a new segment, cut immediately before it** — so
   unresolved members trailing a segment stay with the *preceding* object, and
   unresolved members leading a run are absorbed by the *first* object seen.
   Any segment containing such adjacency-assigned members is flagged `ADJ` and
   its `obj_conf` (= directly-resolved members / all members) drops below 1.0.
4. `--min-fns` applies to SEGMENTS, not to pre-split runs.

Suggested unit path (`unit_suggest` / `unit_src`)
-------------------------------------------------
`xgraphics:optimize.obj` -> `xdk/xgraphics/optimize.cpp`, following the
convention lane PIN-B landed (`40d568ce`) and DC3's own splits.txt. Resolution
order, all keyed on the object *stem*:

- `MAIN` — a heading already in our `config/45410914/splits.txt`. **The unit
  EXISTS**: pinning means adding another `.text` line under it, NOT writing a
  second heading. (This is the normal case for a run adjacent to an existing
  pin, e.g. everything below the landed `optimize.cpp` block.)
- `DC3` — a heading in `../dc3-decomp/config/373307D9/splits.txt`. Note our tree
  sometimes uses a *deeper* path than DC3 for the same object
  (`xdk/xgraphics/ucode/compiler/asm/disasm.cpp` vs DC3's flat
  `xdk/xgraphics/disasm.cpp`); MAIN is checked first so our own convention wins.
- `FALLBACK` — constructed `xdk/<lib>/<stem>.cpp` (or `<lib>/<stem>.cpp` when
  `<lib>` is not a known `xdk/` directory).

⛔ **A heading match is only accepted when it is LIB-CONSISTENT** — the path must
contain `/<lib>/` or start with `<lib>/`. This is not a nicety. Bare stem
matching was measured to produce actively dangerous suggestions:
`xgraphics:scheduler.obj` -> Milo's `Scheduler.cpp`, `d3d9i:shader.obj` ->
Milo's `Shader.cpp`, `xonline:session.obj` -> `network/quazal/Session.cpp`,
`LIBCMT:log.obj` -> `network/quazal/Core/Log.cpp` — each of which would aim a
vendor pin at an existing, unrelated, already-matched unit. 23 objects hit that
trap; the lib filter sends them to FALLBACK instead.

Template continuity (v2 fix)
----------------------------
v1 skipped `?$` template/STL COMDATs entirely. That was wrong for *runs*: a
skipped template opens a hole larger than `--gap` and cuts a run in the middle
of an object (~24% of v1 rows had a template-artifact boundary). In v2
templates are ordinary members — their addresses and sizes count for adjacency,
span and `owned_bytes` — and are excluded only from **label voting**. They are
counted in `n_template`. `--vote-templates` lets them vote anyway.

Label confidence (v2 fix) — `cls` is ADVISORY ONLY
--------------------------------------------------
`cls` is a plurality vote over demangled owning-class names. Unmangled C symbols
(all of `xhv2:voicechat.obj`, for instance) vote None. In v1 one bogus map entry
labelled a 27-member run `MetaPerformer` when 26/27 were XDK voice-chat code
(that entry is now denylisted). v2 emits `label_votes` (votes for the winning
class) and `label_conf` = `label_votes / n_fns` with **ALL** members in the
denominator, including None-voters and templates.

⛔ **NEVER derive a splits.txt unit name from `cls`.** Use `unit_suggest`, which
comes from the object oracle. `cls` is a human hint about what the code is.

Provenance flags carried through from the map (do not ignore these)
-------------------------------------------------------------------
- `_denylist` — hand-flagged bad guesses. **Always excluded.**
- `_bijection_arbitrary` (1,119) — name assigned by a bijection over a
  reloc-masked byte-identical equivalence class. The BYTES are right; the NAME
  is one arbitrary choice among twins.
- `_icf_arbitrary` (27) — arbitrary pick among linker-ICF-folded twins.

Both arbitrary classes are fine for **map value** (the address really is that
code) but unreliable for **choosing a source file**, so they are counted per
cluster in `n_arbitrary` rather than silently dropped. A cluster that is mostly
arbitrary is a pin candidate, not a decomp candidate. ⚠ An arbitrary name also
poisons the object join for that member specifically — the bytes are a twin's,
so the object is the twin's object.

Hazards handled deliberately (all from CLAUDE.md)
-------------------------------------------------
- **Never reads a `.s` address column** — SYNTHETIC for multi-block units.
  Addresses come from the map; sizes from `symbols.txt`.
- **Straddles are reported, not dropped** — overlapping an existing pin or a
  sibling usually means interleaved TUs, which is real information.
- **A liveness gate runs on every invocation** and REFUSES below a floor, so
  this tool can never quietly become the next dead index. A second gate refuses
  a DC3 map that parses to implausibly few symbols.

⚠ What this does NOT do
-----------------------
- It does not decide the true object extent. `span_lo`/`span_hi` are the
  **INNER** span (first->last member); the real boundary lies in the gap before
  the next segment. Inner-span vs gap-extended is a judgement call.
- It cannot see functions that are neither pinned nor named — so `n_fns` is a
  **LOWER BOUND** (lane PIN-A: dtk found 29 functions in a span the queue
  advertised as 27; the 2 extras were unnamed).
- It does not split by name family. Where DC3 does not link a library at all,
  a multi-object RB3 region stays one uncut row with `obj=-`. (Measured
  non-issue for the known cases: DC3 *does* link `xhv2`, so PIN-A's
  VoiceChat/XHV2/Loopback region cuts correctly. Name-family splitting remains
  the residual idea for a library DC3 lacks — deliberately NOT implemented.)

⛔ ROWS ARE PIN CANDIDATES, NOT APPLYABLE PINS
----------------------------------------------
A splits.txt heading with no `config/45410914/objects.json` entry is
**HARD-REFUSED by configure.py**. Every pin therefore needs a `NonMatching`
objects.json entry — but **no source file is required**; PIN-B's landed pins
prove that path. `--emit-snippets <span_lo>` prints both fragments for a row.
When `unit_src` is `MAIN` the heading already exists: add a `.text` line to it
and do **not** add a second heading or a second objects.json entry.

✅ Settled by the PIN-A/PIN-B lanes: **pinning is METRIC-NEUTRAL** (delta exactly
0 on matched/masked/honest/code%/total_code, both lanes, settled ab_measure
legs). `auto_*` units are already in the denominator — report keys are
`default/auto_...`, so a `startswith('auto_')` filter is a false negative. A pin
only reattributes bytes; a pin wave needs no A/B.
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from bisect import bisect_right
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SYMBOLS = REPO / "config" / "45410914" / "symbols.txt"
SPLITS = REPO / "config" / "45410914" / "splits.txt"
OBJECTS = REPO / "config" / "45410914" / "objects.json"
TARGET_MAP = REPO / "scripts" / "target_symbol_map.json"



def _find_dc3(*rel: str) -> Path:
    """Locate a dc3-decomp path from main OR from a ~/tmp worktree.

    ⚠ `REPO.parent / "dc3-decomp"` alone is WRONG in a worktree: lanes work in
    `~/tmp/wt-*`, whose parent is `~/tmp`, so the DC3 object oracle silently
    vanishes and every row reads `obj=-` — a false negative shaped exactly like
    "DC3 doesn't know this code" (lane PIN-D hit this and had to hand-symlink
    `~/tmp/dc3-decomp` to work around it). Try the sibling of the *real* repo
    (via the worktree's gitdir), then the sibling of cwd, then the usual
    checkout root. Returns the first that exists, else the sibling guess so the
    error message still names a sensible path.
    """
    cands = [REPO.parent / "dc3-decomp"]
    gitf = REPO / ".git"
    if gitf.is_file():                      # worktree: .git is a gitdir pointer
        try:
            gd = gitf.read_text().split("gitdir:", 1)[1].strip()
            # …/<mainrepo>/.git/worktrees/<name>  ->  …/<mainrepo>
            main_repo = Path(gd).resolve().parent.parent.parent
            cands.insert(0, main_repo.parent / "dc3-decomp")
        except (IndexError, OSError):
            pass
    cands.append(Path.home() / "code" / "milohax" / "dc3-decomp")
    for base in cands:
        p = base.joinpath(*rel)
        if p.exists():
            return p
    return cands[0].joinpath(*rel)


DC3_MAP = _find_dc3("orig", "373307D9", "ham_xbox_r.map")
DC3_SPLITS = _find_dc3("config", "373307D9", "splits.txt")

SYM_RE = re.compile(r"^(?P<name>\S+)\s*=\s*\.text:0x(?P<addr>[0-9A-Fa-f]+);(?P<rest>.*)$")
SIZE_RE = re.compile(r"size:0x([0-9A-Fa-f]+)")
UNIT_RE = re.compile(r"^(?P<unit>\S[^:]*):\s*$")
TEXT_RE = re.compile(r"^\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")

# ' 0005:00001fa8       ?DebugModal@@YAX...@Z 82331fa8 f   App.obj'
MAP_RE = re.compile(
    r"^\s+[0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(?P<name>\S+)\s+[0-9A-Fa-f]{8}\s+"
    r"(?:f\s+)?(?:i\s+)?(?P<obj>\S+)\s*$")

_CLASS_RES = (
    re.compile(r"^\?\?[0-9A-Za-z_]([A-Za-z0-9_]+)@@"),   # ??0Foo@@ / ??1Foo@@
    re.compile(r"^\?[A-Za-z0-9_]+@([A-Za-z0-9_]+)@"),    # ?Bar@Foo@
)
_TEMPLATE = re.compile(r"\?\$")

LIVENESS_FLOOR = 0.50    # dead TU0 oracles scored 0.027-0.055
DC3_MAP_FLOOR = 10_000   # the real map yields ~118,000 distinct names


def owning_class(mangled: str):
    for rx in _CLASS_RES:
        m = rx.match(mangled)
        if m:
            return m.group(1)
    return None


def load_pins(path: Path):
    pins, unit = [], None
    for line in path.read_text(errors="replace").splitlines():
        m = UNIT_RE.match(line)
        if m:
            unit = m.group("unit")
            continue
        m = TEXT_RE.match(line)
        if m:
            pins.append((int(m.group(1), 16), int(m.group(2), 16), unit or "?"))
    pins.sort()
    return pins


def load_headings(path: Path):
    """splits.txt unit headings -> {lowercased file stem: [paths]} (sorted)."""
    by_stem = collections.defaultdict(list)
    for line in path.read_text(errors="replace").splitlines():
        m = UNIT_RE.match(line)
        if m:
            p = m.group("unit")
            by_stem[p.rsplit("/", 1)[-1].rsplit(".", 1)[0].lower()].append(p)
    return {k: sorted(set(v)) for k, v in by_stem.items()}


def load_sizes(path: Path):
    """addr -> size, plus the set of real .text function starts (liveness gate)."""
    sizes, starts = {}, set()
    for line in path.read_text(errors="replace").splitlines():
        m = SYM_RE.match(line)
        if not m or "type:function" not in m.group("rest"):
            continue
        a = int(m.group("addr"), 16)
        starts.add(a)
        sm = SIZE_RE.search(m.group("rest"))
        if sm:
            sizes[a] = int(sm.group(1), 16)
    return sizes, starts


def load_dc3_objmap(path: Path):
    """DC3 linker map -> {mangled name: sorted list of owning .obj}.

    Addresses in the map are DC3's and are DELIBERATELY IGNORED. Only the
    name->object column transfers. Non-object tokens (import thunks
    'lib:foo.xex@21173.0+1861.0', '<absolute>') are dropped, so a name that only
    ever appears as an import reads as UNRESOLVED rather than as a bogus object.
    """
    n2o = collections.defaultdict(set)
    for line in path.read_text(errors="replace").splitlines():
        m = MAP_RE.match(line)
        if not m:
            continue
        obj = m.group("obj")
        if not obj.lower().endswith(".obj"):
            continue
        n2o[m.group("name")].add(obj)
    return {k: sorted(v) for k, v in n2o.items()}


def obj_lib_stem(obj: str):
    lib, _, base = obj.rpartition(":")
    return (lib or None), base.rsplit(".", 1)[0].lower()


class Cover:
    """O(log n) 'is this address inside any pinned range'."""

    def __init__(self, pins):
        self.starts = [p[0] for p in pins]
        self.pins = pins

    def __call__(self, addr: int) -> bool:
        i = bisect_right(self.starts, addr) - 1
        return i >= 0 and addr < self.pins[i][1]


def span_overlap(a: int, b: int, ranges) -> int:
    t = 0
    for r in ranges:
        x, y = max(a, r[0]), min(b, r[1])
        if y > x:
            t += y - x
    return t


class UnitSuggester:
    """obj -> (unit path, source tag). LIB-CONSISTENT matches only."""

    def __init__(self, main_headings, dc3_headings):
        self.tables = (("MAIN", main_headings), ("DC3", dc3_headings))
        self.xdk_libs = set()
        for tbl in (main_headings, dc3_headings):
            for paths in tbl.values():
                for p in paths:
                    if p.startswith("xdk/") and p.count("/") >= 2:
                        self.xdk_libs.add(p.split("/")[1])
        self._cache = {}

    def __call__(self, obj: str):
        if obj in self._cache:
            return self._cache[obj]
        lib, stem = obj_lib_stem(obj)
        res = None
        for tag, tbl in self.tables:
            for p in tbl.get(stem, ()):
                if lib and (f"/{lib}/" in p or p.startswith(f"{lib}/")):
                    res = (p, tag)
                    break
            if res:
                break
        if res is None:
            if lib and lib in self.xdk_libs:
                res = (f"xdk/{lib}/{stem}.cpp", "FALLBACK")
            elif lib:
                res = (f"{lib}/{stem}.cpp", "FALLBACK")
            else:
                res = (f"{stem}.cpp", "FALLBACK")
        self._cache[obj] = res
        return res


HDR = ("obj\tobj_conf\tunit_suggest\tunit_src\tcls\tlabel_votes\tlabel_conf"
       "\tn_fns\tn_template\tn_arbitrary\tn_objres\tspan_lo\tspan_hi"
       "\tspan_bytes\towned_bytes\toverlap_pinned\toverlap_sibling\tflags")


def row_line(c) -> str:
    return (f"{c['obj']}\t{c['obj_conf']:.3f}\t{c['unit']}\t{c['unit_src']}"
            f"\t{c['cls']}\t{c['votes']}\t{c['label_conf']:.3f}"
            f"\t{c['n']}\t{c['ntpl']}\t{c['narb']}\t{c['nobjres']}"
            f"\t0x{c['lo']:08X}\t0x{c['hi']:08X}\t{c['span']}\t{c['own']}"
            f"\t{c['ov_pin']}\t{c['ov_sib']}\t{c['flags']}")


def objects_groups(path: Path):
    """objects.json -> {group: set(unit paths)}. Empty dict if unreadable."""
    try:
        raw = json.loads(path.read_text())
    except Exception:
        return {}
    return {g: set(v.get("objects", {})) for g, v in raw.items()
            if isinstance(v, dict)}


def emit_snippets(c, groups, out):
    """Ready-to-paste splits.txt + objects.json fragments for one row."""
    unit = c["unit"]
    have = sorted(g for g, ks in groups.items() if unit in ks)
    parent = unit.rsplit("/", 1)[0] + "/" if "/" in unit else ""
    # pick the group already holding the most units from the same directory
    guess = max(
        ((g, sum(1 for k in ks if parent and k.startswith(parent)))
         for g, ks in groups.items()),
        key=lambda t: (t[1], t[0]), default=(None, 0))

    print(f"# row: {c['obj']}  {c['n']} fns ({c['ntpl']} template)  {c['own']} B "
          f"owned  unit_src={c['unit_src']}  obj_conf={c['obj_conf']:.3f}  "
          f"flags={c['flags']}", file=out)

    print(f"\n--- config/45410914/splits.txt ---", file=out)
    if c["unit_src"] == "MAIN":
        print(f"# {unit} ALREADY HAS A HEADING — add this .text line under the "
              f"EXISTING heading; do NOT write a second heading.", file=out)
    else:
        print(f"{unit}:", file=out)
    print(f"    .text       start:0x{c['lo']:08X} end:0x{c['hi']:08X}", file=out)
    print(f"# span is the INNER span (first->last member); the true object "
          f"boundary lies in the following gap. Extending is a judgement call.",
          file=out)

    print(f"\n--- config/45410914/objects.json ---", file=out)
    if have:
        print(f"# ALREADY PRESENT in group(s) {have} — do NOT duplicate.", file=out)
    else:
        if guess[0] and guess[1]:
            print(f'# add under group "{guess[0]}" (holds {guess[1]} other '
                  f'{parent}* units):', file=out)
        else:
            print(f"# no group obviously owns {parent or unit} — pick one "
                  f"deliberately:", file=out)
        print(f'            {json.dumps(unit)}: "NonMatching",', file=out)
        print(f"# no source file is required for a NonMatching pin "
              f"(PIN-B's landed pins prove this).", file=out)

    print(f"\n# then: touch config/45410914/config.yml && ./tools/ninja-locked",
          file=out)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", help="write the TSV here")
    ap.add_argument("--min-fns", type=int, default=2,
                    help="drop SEGMENTS with fewer than N unpinned members (default 2)")
    ap.add_argument("--gap", type=lambda s: int(s, 0), default=0x40,
                    help="max byte gap between adjacent functions in one run "
                         "(default 0x40; inter-function padding is small, so a "
                         "tight gap keeps runs honest)")
    ap.add_argument("--clean-only", action="store_true",
                    help="keep only segments with zero pinned AND zero sibling overlap")
    ap.add_argument("--vote-templates", "--include-templates", action="store_true",
                    dest="vote_templates",
                    help="let ?$ template/STL COMDATs vote for the `cls` label. "
                         "⚠ RENAMED in v2: templates are now ALWAYS members of a "
                         "run (v1's skip broke run continuity); this flag only "
                         "controls LABEL VOTING. --include-templates is kept as a "
                         "deprecated alias with the NEW meaning.")
    ap.add_argument("--no-dc3-map", action="store_true",
                    help="run WITHOUT the DC3 object oracle (degraded: no obj "
                         "column, no sub-splitting). Must be explicit — a missing "
                         "map is a REFUSAL, never a silent v1-shaped queue.")
    ap.add_argument("--emit-snippets", type=lambda s: int(s, 0), metavar="SPAN_LO",
                    help="print splits.txt + objects.json fragments for the row "
                         "with this span_lo, then exit")
    ap.add_argument("--objects", default=str(OBJECTS))
    ap.add_argument("--symbols", default=str(SYMBOLS))
    ap.add_argument("--splits", default=str(SPLITS))
    ap.add_argument("--target-map", default=str(TARGET_MAP))
    ap.add_argument("--dc3-map", default=str(DC3_MAP))
    ap.add_argument("--dc3-splits", default=str(DC3_SPLITS))
    args = ap.parse_args(argv)

    log = sys.stderr
    print(f"params: min_fns={args.min_fns} gap=0x{args.gap:x} "
          f"clean_only={args.clean_only} vote_templates={args.vote_templates} "
          f"no_dc3_map={args.no_dc3_map}", file=log)
    raw = json.loads(Path(args.target_map).read_text())
    deny = set(raw.get("_denylist") or [])
    arb = set(raw.get("_bijection_arbitrary") or []) | set(raw.get("_icf_arbitrary") or [])
    named = {int(k, 16): v for k, v in raw.items()
             if k.startswith("0x") and k not in deny}
    arb_addrs = {int(a, 16) for a in arb if a.startswith("0x")}

    pins = load_pins(Path(args.splits))
    sizes, starts = load_sizes(Path(args.symbols))
    if not pins or not sizes or not named:
        print("REFUSING: empty pins / symbols / map — check input paths", file=log)
        return 2

    # ---- liveness gate: never let this become the next dead index -------------
    live = sum(1 for a in named if a in starts) / len(named)
    print(f"target map liveness : {live*100:.2f}% of {len(named):,} addrs are real "
          f".text starts", file=log)
    if live < LIVENESS_FLOOR:
        print(f"REFUSING: liveness {live*100:.2f}% < floor {LIVENESS_FLOOR*100:.0f}% — "
              f"this map looks TU-stale (dead TU0 oracles scored 2.7-5.5%).", file=log)
        return 2

    # ---- DC3 object oracle ----------------------------------------------------
    n2o, suggest = {}, None
    if args.no_dc3_map:
        print("⚠ DEGRADED: --no-dc3-map — no object sub-splitting, obj='-' "
              "everywhere. This is NOT the v2 queue.", file=log)
    else:
        p = Path(args.dc3_map)
        if not p.is_file():
            print(f"REFUSING: DC3 linker map not found at {p}. It is the object "
                  f"oracle; pass --no-dc3-map to run degraded on purpose.", file=log)
            return 2
        n2o = load_dc3_objmap(p)
        print(f"dc3 map name->obj   : {len(n2o):,} distinct names", file=log)
        if len(n2o) < DC3_MAP_FLOOR:
            print(f"REFUSING: only {len(n2o):,} names < floor {DC3_MAP_FLOOR:,} — "
                  f"the map looks truncated or the format changed.", file=log)
            return 2
        sp = Path(args.dc3_splits)
        suggest = UnitSuggester(load_headings(Path(args.splits)),
                                load_headings(sp) if sp.is_file() else {})

    covered = Cover(pins)
    print(f"pinned .text blocks : {len(pins):,}", file=log)

    unpinned = [(a, n) for a, n in named.items() if not covered(a)]
    ub = sum(sizes.get(a, 0) for a, _ in unpinned)
    print(f"NAMED + UNPINNED    : {len(unpinned):,} functions ({ub:,} B)", file=log)

    # ---- members: templates STAY (v2 fix #2) ----------------------------------
    members, n_amb, n_unres = [], 0, 0
    for a, n in sorted(unpinned):
        objs = n2o.get(n, ())
        if len(objs) == 1:
            obj = objs[0]
        else:
            obj = None
            if len(objs) > 1:
                n_amb += 1
            else:
                n_unres += 1
        members.append({"a": a, "n": n, "s": sizes.get(a, 0),
                        "obj": obj, "tpl": bool(_TEMPLATE.search(n))})
    n_tpl = sum(1 for m in members if m["tpl"])
    print(f"  templates KEPT as members: {n_tpl:,} "
          f"(v1 dropped these, breaking run continuity)", file=log)
    if not args.no_dc3_map:
        res = len(members) - n_amb - n_unres
        print(f"  obj resolution      : {res:,}/{len(members):,} "
              f"({res/max(1,len(members))*100:.2f}%) unique  "
              f"{n_amb:,} ambiguous  {n_unres:,} unresolved", file=log)

    # ---- group into CONTIGUOUS ADDRESS RUNS, not by class ---------------------
    # Class grouping is the wrong unit here: several classes routinely share one
    # object (measured — all 120 class-clusters in the D3D shader-compiler region
    # straddle a sibling, because those classes interleave inside a few big vendor
    # objects). A pin is an address range, so cluster by adjacency and *label*
    # with the classes present.
    runs, cur = [], []
    for m in members:
        if cur and m["a"] - (cur[-1]["a"] + cur[-1]["s"]) > args.gap:
            runs.append(cur)
            cur = []
        cur.append(m)
    if cur:
        runs.append(cur)
    print(f"  contiguous runs (gap<=0x{args.gap:x}): {len(runs):,}", file=log)

    # ---- v2 fix #1: sub-split each run at OBJECT transitions -------------------
    segments, n_cuts = [], 0
    for run in runs:
        seg, seg_obj = [], None
        for m in run:
            if m["obj"] is not None and seg_obj is not None and m["obj"] != seg_obj:
                segments.append((seg, seg_obj))
                n_cuts += 1
                seg, seg_obj = [], None
            seg.append(m)
            if m["obj"] is not None and seg_obj is None:
                seg_obj = m["obj"]
        if seg:
            segments.append((seg, seg_obj))
    print(f"  segments after obj sub-split: {len(segments):,} "
          f"({n_cuts:,} cuts at object transitions)", file=log)

    cands = []
    for seg, seg_obj in segments:
        if len(seg) < args.min_fns:
            continue
        lo = seg[0]["a"]
        hi = max(m["a"] + m["s"] for m in seg)
        cls_ct = collections.Counter()
        for m in seg:
            if m["tpl"] and not args.vote_templates:
                continue
            c = owning_class(m["n"])
            if c:
                cls_ct[c] += 1
        top = cls_ct.most_common(3)
        label = ",".join(c for c, _ in top) or "-"
        votes = top[0][1] if top else 0
        nobjres = sum(1 for m in seg if m["obj"] is not None)
        flags = []
        if seg_obj is not None and nobjres < len(seg):
            flags.append("ADJ")
        if seg_obj is None:
            flags.append("NO_OBJ")
        if any(m["obj"] is None and n2o.get(m["n"]) for m in seg):
            flags.append("AMB_OBJ")
        obj = seg_obj or "-"
        unit, usrc = (suggest(seg_obj) if (suggest and seg_obj) else ("-", "-"))
        cands.append({
            "obj": obj, "obj_conf": (nobjres / len(seg)) if seg_obj else 0.0,
            "unit": unit, "unit_src": usrc,
            "cls": label, "votes": votes, "label_conf": votes / len(seg),
            "n": len(seg), "ntpl": sum(1 for m in seg if m["tpl"]),
            "narb": sum(1 for m in seg if m["a"] in arb_addrs),
            "nobjres": nobjres, "lo": lo, "hi": hi,
            "span": hi - lo, "own": sum(m["s"] for m in seg),
            "ov_pin": span_overlap(lo, hi, pins), "flags": ",".join(flags) or "-",
        })

    # ⚠ With run grouping, segments are DISJOINT BY CONSTRUCTION (built from
    # sorted non-overlapping members), so ov_sib is structurally ~0 — it survives
    # from the class-grouping era and is kept only so a future grouping change
    # that reintroduces overlap becomes visible. "CLEAN" therefore effectively
    # means "no overlap with EXISTING PINS", not "passed a sibling test".
    ranges = [(c["lo"], c["hi"], c["obj"], c["cls"]) for c in cands]
    for c in cands:
        c["ov_sib"] = span_overlap(
            c["lo"], c["hi"],
            [r for r in ranges if (r[2], r[3]) != (c["obj"], c["cls"])])

    total = len(cands)
    if args.clean_only:
        cands = [c for c in cands if not c["ov_pin"] and not c["ov_sib"]]
    clean = [c for c in cands if not c["ov_pin"] and not c["ov_sib"]]
    cands.sort(key=lambda c: (bool(c["ov_pin"]), bool(c["ov_sib"]), -c["own"], c["lo"]))

    if args.emit_snippets is not None:
        hit = [c for c in cands if c["lo"] == args.emit_snippets]
        if not hit:
            print(f"REFUSING: no row with span_lo=0x{args.emit_snippets:08X}", file=log)
            return 2
        emit_snippets(hit[0], objects_groups(Path(args.objects)), sys.stdout)
        return 0

    print(f"\nsegments (>={args.min_fns} fns): {total:,}"
          f"    CLEAN (no overlap): {len(clean):,}", file=log)
    print(f"CLEAN owned bytes   : {sum(c['own'] for c in clean):,}"
          f"  in {sum(c['span'] for c in clean):,} B of span", file=log)
    src_ct = collections.Counter(c["unit_src"] for c in clean)
    print(f"  unit_src           : {dict(sorted(src_ct.items()))}", file=log)
    conf_ct = collections.Counter(
        "1.00" if c["label_conf"] >= 0.999 else
        ">=0.75" if c["label_conf"] >= 0.75 else
        ">=0.50" if c["label_conf"] >= 0.50 else
        ">0" if c["label_conf"] > 0 else "0 (UNLABELED)" for c in clean)
    print(f"  label_conf         : {dict(sorted(conf_ct.items()))}", file=log)
    narb = sum(c["narb"] for c in clean)
    if narb:
        print(f"  ⚠ {narb:,} CLEAN members carry an ARBITRARY name "
              f"(bijection/ICF twin) — pin value yes, source-file choice no", file=log)
    nadj = sum(1 for c in clean if "ADJ" in c["flags"])
    if nadj:
        print(f"  ⚠ {nadj:,} segments contain adjacency-assigned members "
              f"(obj_conf < 1.0) — the object boundary there is inferred", file=log)
    if total and len(clean) < total:
        print(f"  ⚠ {total-len(clean):,} segments straddle a pin or sibling — "
              f"interleaved TUs, adjudicate before pinning", file=log)

    lines = [HDR] + [row_line(c) for c in cands]
    if args.out:
        Path(args.out).write_text("\n".join(lines) + "\n")
        print(f"\nwrote {args.out} ({len(cands)} rows)", file=log)
    else:
        print("\n".join(lines[:26]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
