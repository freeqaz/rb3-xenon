#!/usr/bin/env python3
"""Generate target_symbol_map.json entries for RB3 *game* TUs.

Pipeline role
-------------
Game-code identification is solved by the RB3-Wii BinDiff oracle
(`unified_id_rb3wii.json`, repo root, gitignored): it maps every located
RB3-360 target address (`fn_<addr>`) to a human-readable RB3-Wii function name
(Ghidra-demangled, e.g. ``Accomplishment::GetType()_const``) and a source TU
(``band3/src/meta_band/Accomplishment.cpp``).

But pinning a game TU's `.text` span gives +0 matched, because objdiff pairs
the dtk-SPLIT target obj (anonymous `fn_<addr>` symbols) to our compiled obj
(MSVC-mangled symbols) by *name equality*. The names never match, so no pair
forms and matches can't register even when the bytes agree.

`scripts/obj_target_symbol_renamer.py` (wired as a pre-compile build step)
fixes this by rewriting target-side `fn_<addr>` symbols to MSVC-mangled names,
reading an explicit ``{ "0xADDR": "<mangled>" }`` map from
`scripts/target_symbol_map.json`. This tool *generates* those map entries for
game TUs.

How it pairs (demangle-and-match, no re-mangling)
-------------------------------------------------
The oracle already gives us the demangled name per target address. We avoid the
fragile inverse (re-mangling a demangled string into MSVC's exact ABI encoding)
by going the other direction:

  1. Take each oracle entry for a high-purity (>=THRESHOLD) game TU.
  2. Parse its RB3-Wii ``wii_name`` into ``(class, method, argcount)``.
  3. Parse the *defined* MSVC-mangled function symbols in our compiled obj
     (``build/45410914/src/band3/<tu>.obj``) into ``(class, method, argcount,
     mangled)`` by decoding the ``?Method@Class@@...`` / ``??0Class@@...`` head.
  4. Match by ``class::method``; disambiguate same-name overloads by arg count
     (best-effort: arg count parsed from both the demangled signature and the
     MSVC type-code list). Near-signature mismatches from Wii-vs-360 type widths
     (``int`` vs ``long``, ref vs ptr) don't matter — we key on class::method +
     arity, not exact types.
  5. Emit ``{ "0xADDR": "<mangled>" }``.

Only symbols *defined* (section number > 0) in our obj are eligible — external
references and engine inlines pulled into the obj as undefined symbols are
excluded. The match is also scoped to the TU's dominant class(es) so that a
neighbor-TU function interleaved in the span (purity < 100%) does not get
mis-assigned.

Usage
-----
    # write game entries into scripts/target_symbol_map.json (merges, idempotent)
    python3 tools/gen_game_target_map.py --apply

    # dry-run report only
    python3 tools/gen_game_target_map.py

    # restrict to a TU subset / change purity bar / obj source dir
    python3 tools/gen_game_target_map.py --tu Accomplishment.cpp --tu UIEvent.cpp
    python3 tools/gen_game_target_map.py --purity 0.7 --apply
"""

import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ORACLE = Path("/home/free/code/milohax/rb3-xenon/unified_id_rb3wii.json")
DEFAULT_MAP = PROJECT_ROOT / "scripts" / "target_symbol_map.json"
DEFAULT_OBJ_DIR = PROJECT_ROOT / "build" / "45410914" / "src"
DEFAULT_PURITY = 0.70


# ---------------------------------------------------------------------------
# COFF symbol parsing (defined function symbols + their class::method).
# ---------------------------------------------------------------------------
def parse_coff_defined(data: bytes) -> List[Tuple[str, int]]:
    """Return [(name, section_number)] for every symbol. section_number > 0
    means the symbol is defined in a section of this obj (a COMDAT for /O1
    function COMDATs); 0 == undefined/external reference."""
    if len(data) < 20:
        return []
    sym_off = struct.unpack_from("<I", data, 8)[0]
    n = struct.unpack_from("<I", data, 12)[0]
    if sym_off == 0 or n == 0:
        return []
    strt = sym_off + n * 18
    out: List[Tuple[str, int]] = []
    i = 0
    while i < n:
        e = sym_off + i * 18
        if e + 18 > len(data):
            break
        nb = data[e:e + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            so = struct.unpack_from("<I", nb, 4)[0]
            ao = strt + so
            if ao < len(data):
                end = data.index(b"\x00", ao)
                name = data[ao:end].decode("ascii", "replace")
            else:
                name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii", "replace")
        secnum = struct.unpack_from("<h", data, e + 12)[0]
        aux = data[e + 17]
        out.append((name, secnum))
        i += 1 + aux
    return out


# MSVC primitive type-code -> 1 argument; used for arg-count of mangled fns.
# This is a heuristic counter, not a full demangler: it counts top-level type
# codes in the argument list between the calling convention and the @Z/Z end.
_MSVC_PRIM = set("CDEFGHIJKMNO_")  # char,uchar,short,...,double,etc (incl _W/_N via '_')


def msvc_class_method(mangled: str) -> Optional[Tuple[Optional[str], str, str]]:
    """Decode the *head* of an MSVC C++ symbol into (class, method, kind).

    kind in {"method","ctor","dtor","operator","free","data"}. Returns None if
    the symbol is not a recognizable C++ function we care about.

    Handles:
      ?Method@Class@@...           -> (Class, Method, method)
      ??0Class@@...                -> (Class, Class, ctor)
      ??1Class@@...                -> (Class, ~Class, dtor)
      ?Method@@...   (free fn)     -> (None, Method, free)
    Nested classes ``?M@Inner@Outer@@`` collapse to the *immediate* enclosing
    class (Inner) -- matching the oracle which also reports the immediate class.
    """
    if not mangled.startswith("?"):
        return None
    if mangled.startswith("??"):
        body = mangled[2:]
        op = body[:1]
        rest = body[1:]
        head = rest.split("@@", 1)[0]
        segs = [s for s in head.split("@") if s]
        cls = segs[0] if segs else None
        if op == "0":
            return (cls, cls or "", "ctor")
        if op == "1":
            return (cls, ("~" + cls) if cls else "", "dtor")
        # other special names (operators, vtables, etc) -- not matched by name
        return None
    body = mangled[1:]
    head = body.split("@@", 1)[0]
    segs = head.split("@")
    if len(segs) < 2 or segs[1] == "":
        # free function or data symbol: ?Name@@3... (data) / ?Name@@YA... (fn)
        return (None, segs[0], "free")
    method = segs[0]
    cls = segs[1]
    return (cls, method, "method")


def msvc_argcount(mangled: str) -> Optional[int]:
    """Best-effort argument count from an MSVC mangled function symbol.

    Returns None if it can't be determined (then caller falls back to
    class::method-only matching). For ``...XZ`` (void arg list) returns 0.
    This is intentionally conservative: it is only used to break ties between
    same-class same-name overloads.
    """
    # Locate the function type: after @@ comes [access][cc] then return+args.
    if "@@" not in mangled:
        return None
    suffix = mangled.split("@@", 1)[1]
    # access-qualified member fn codes: Q/A/I (public/private/protected) +
    # B/A/... cv ; static S ; virtual U/E etc. Free fn: Y. We just scan past
    # leading letters until we hit the return type, which is hard in general --
    # so we use the reliable special cases only.
    # Void arg list ends in 'XZ' (return-then-X-then-Z) e.g. ?Foo@C@@QBA_NXZ.
    if suffix.endswith("XZ"):
        return 0
    # Otherwise: arg list is the codes between return type and trailing @Z / Z.
    # We can't robustly split return-vs-args without full demangling, so bail.
    return None


def parse_wii_name(name: str) -> Optional[Tuple[Optional[str], str, Optional[int], str]]:
    """Parse a Ghidra-demangled RB3-Wii name into
    (class, method, argcount, kind). Ghidra renders spaces as ``_`` and appends
    ``_const`` for const methods; we strip both. argcount is None when there is
    no parameter list in the name."""
    if not name:
        return None
    # strip Ghidra's trailing const marker (either ")_const" or bare "_const")
    n = re.sub(r"\)_const$", ")", name)
    n = re.sub(r"_const$", "", n)
    if "(" in n:
        sig = n.split("(", 1)[0]
        args = n.split("(", 1)[1].rsplit(")", 1)[0]
    else:
        sig = n
        args = None
    parts = sig.split("::")
    if len(parts) >= 2:
        cls = parts[-2]
        method = parts[-1]
    else:
        cls = None
        method = parts[-1]
    argc: Optional[int] = None
    if args is not None:
        a = args.strip()
        if a == "" or a == "void":
            argc = 0
        else:
            depth = 0
            argc = 1
            for ch in a:
                if ch in "<([":
                    depth += 1
                elif ch in ">)]":
                    depth -= 1
                elif ch == "," and depth == 0:
                    argc += 1
    kind = "method"
    if cls is not None and method == cls:
        kind = "ctor"
    elif method.startswith("~"):
        kind = "dtor"
    return (cls, method, argc, kind)


# ---------------------------------------------------------------------------
# Oracle loading.
# ---------------------------------------------------------------------------
def load_oracle(path: Path) -> List[dict]:
    return json.loads(path.read_text())


def tu_basename(src: str) -> str:
    """band3/src/meta_band/Accomplishment.cpp -> Accomplishment.cpp"""
    return src.replace("\\", "/").rsplit("/", 1)[-1]


def candidate_spans(spans_path: Optional[Path]) -> Dict[str, Tuple[int, int, float]]:
    """Optional: load /tmp/candidate_spans.json keyed by TU basename ->
    (start, end, purity). Used to scope each TU's match to its dominant span."""
    out: Dict[str, Tuple[int, int, float]] = {}
    if not spans_path or not spans_path.is_file():
        return out
    for e in json.loads(spans_path.read_text()):
        out[tu_basename(e["tu"])] = (
            int(e["start"], 16),
            int(e["end"], 16),
            float(e.get("purity", 0.0)),
        )
    return out


def find_obj(obj_dir: Path, tu_base: str) -> Optional[Path]:
    """Locate the compiled obj for a TU under build/.../src (recursive)."""
    stem = tu_base[:-4] if tu_base.endswith(".cpp") else tu_base
    hits = list(obj_dir.rglob(stem + ".obj"))
    # prefer a band3/meta_band path if multiple
    hits.sort(key=lambda p: ("meta_band" not in str(p), len(str(p))))
    return hits[0] if hits else None


# ---------------------------------------------------------------------------
# Core: build map entries for one TU.
# ---------------------------------------------------------------------------
def build_tu_entries(
    tu_base: str,
    oracle_entries: List[dict],
    obj_path: Path,
    span: Optional[Tuple[int, int, float]],
    verbose: bool,
) -> Tuple[Dict[str, str], dict]:
    """Return ({hexaddr: mangled}, stats) for one TU."""
    data = obj_path.read_bytes()
    syms = parse_coff_defined(data)

    # Index defined function symbols by (class, method) -> [mangled...]
    by_cm: Dict[Tuple[Optional[str], str], List[str]] = defaultdict(list)
    for name, secnum in syms:
        if secnum <= 0:
            continue
        if not name.startswith("?"):
            continue
        cm = msvc_class_method(name)
        if cm is None:
            continue
        cls, method, kind = cm
        by_cm[(cls, method)].append(name)

    # Determine the TU's dominant class set from the oracle entries that name
    # this exact TU -- used to reject neighbor-TU functions in low-purity spans.
    in_tu = [e for e in oracle_entries if tu_basename(e["bindiff_src"]) == tu_base]
    tu_classes = defaultdict(int)
    for e in in_tu:
        p = parse_wii_name(e["wii_name"])
        if p and p[0]:
            tu_classes[p[0]] += 1
    dominant_classes = set(tu_classes.keys())

    out: Dict[str, str] = {}
    stats = {
        "tu": tu_base,
        "oracle_in_tu": len(in_tu),
        "matched": 0,
        "no_class_method": 0,
        "no_obj_symbol": 0,
        "ambiguous": 0,
        "out_of_span": 0,
        "wrong_class": 0,
        "details": [],
    }

    for e in in_tu:
        addr = int(e["rb3_addr"], 16)
        if span is not None:
            lo, hi, _pur = span
            if not (lo <= addr < hi):
                stats["out_of_span"] += 1
                continue
        p = parse_wii_name(e["wii_name"])
        if p is None:
            stats["no_class_method"] += 1
            continue
        cls, method, argc, kind = p
        # cls is None for free / file-static functions (oracle name has no
        # "::"). We still match those against the obj's free functions
        # (?Name@@Y... -> (None, name)). Class-method entries are scoped to the
        # TU's dominant class set to reject neighbor-TU interlopers in
        # low-purity spans; free functions are always TU-local so are exempt.
        if cls is not None and dominant_classes and cls not in dominant_classes:
            # neighbor-TU function interleaved in the span -> skip (purity loss)
            stats["wrong_class"] += 1
            continue
        # ctor/dtor: oracle method == class; MSVC head for ctor is (cls, cls)
        # via ??0, dtor (cls, ~cls) via ??1 -- both already normalized.
        cands = by_cm.get((cls, method))
        if not cands:
            stats["no_obj_symbol"] += 1
            continue
        chosen: Optional[str] = None
        if len(cands) == 1:
            chosen = cands[0]
        else:
            # overload: disambiguate by arg count where determinable
            scored = []
            for m in cands:
                ac = msvc_argcount(m)
                if ac is not None and argc is not None and ac == argc:
                    scored.append(m)
            if len(scored) == 1:
                chosen = scored[0]
            else:
                stats["ambiguous"] += 1
                if verbose:
                    stats["details"].append(
                        f"  AMBIG {e['rb3_addr']} {cls}::{method} argc={argc} "
                        f"-> {len(cands)} candidates"
                    )
                continue
        key = f"0x{addr:08X}"
        out[key] = chosen
        stats["matched"] += 1
        if verbose:
            stats["details"].append(f"  {key} {cls}::{method} -> {chosen}")

    return out, stats


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--oracle", default=str(DEFAULT_ORACLE),
                    help="unified_id_rb3wii.json (default: %(default)s)")
    ap.add_argument("--map", default=str(DEFAULT_MAP),
                    help="target_symbol_map.json to merge into (default: %(default)s)")
    ap.add_argument("--obj-dir", default=str(DEFAULT_OBJ_DIR),
                    help="compiled obj search root (default: %(default)s)")
    ap.add_argument("--spans", default="/tmp/candidate_spans.json",
                    help="candidate_spans.json for per-TU span scoping")
    ap.add_argument("--purity", type=float, default=DEFAULT_PURITY,
                    help="min TU purity to include (default: %(default)s)")
    ap.add_argument("--area", default="meta_band",
                    help="restrict to this game source subdir (default: %(default)s); "
                         "empty string = all game TUs")
    ap.add_argument("--tu", action="append", default=[],
                    help="restrict to specific TU basename(s); repeatable")
    ap.add_argument("--apply", action="store_true",
                    help="write merged entries into --map (default: dry-run)")
    ap.add_argument("--verbose", "-v", action="store_true")
    args = ap.parse_args()

    oracle = load_oracle(Path(args.oracle))
    spans = candidate_spans(Path(args.spans))
    obj_dir = Path(args.obj_dir)

    # Determine target TUs: those in candidate_spans with purity>=bar, in area.
    target_tus: List[str] = []
    for tu_base, (lo, hi, pur) in sorted(spans.items()):
        if args.tu and tu_base not in args.tu:
            continue
        if pur < args.purity:
            continue
        # area scoping by checking the oracle's bindiff_src
        if args.area:
            srcs = {
                e["bindiff_src"] for e in oracle
                if tu_basename(e["bindiff_src"]) == tu_base
            }
            if not any(("/" + args.area + "/") in s for s in srcs):
                continue
        target_tus.append(tu_base)

    if args.verbose:
        print(f"Target TUs (purity>={args.purity}, area={args.area or 'all'}): "
              f"{len(target_tus)}")

    all_entries: Dict[str, str] = {}
    all_stats: List[dict] = []
    skipped_no_obj: List[str] = []
    for tu_base in target_tus:
        obj = find_obj(obj_dir, tu_base)
        if obj is None or not obj.exists():
            skipped_no_obj.append(tu_base)
            continue
        entries, stats = build_tu_entries(
            tu_base, oracle, obj, spans.get(tu_base), args.verbose
        )
        all_entries.update(entries)
        all_stats.append(stats)

    # Report
    print("=" * 70)
    print(f"{'TU':<42} {'oracle':>6} {'paired':>6} {'noObj':>6} {'ambig':>6}")
    print("-" * 70)
    tot_oracle = tot_paired = 0
    for s in sorted(all_stats, key=lambda x: -x["matched"]):
        print(f"{s['tu']:<42} {s['oracle_in_tu']:>6} {s['matched']:>6} "
              f"{s['no_obj_symbol']:>6} {s['ambiguous']:>6}")
        tot_oracle += s["oracle_in_tu"]
        tot_paired += s["matched"]
        if args.verbose:
            for d in s["details"]:
                print(d)
    print("-" * 70)
    print(f"{'TOTAL':<42} {tot_oracle:>6} {tot_paired:>6}")
    if skipped_no_obj:
        print(f"\nSkipped (no compiled obj found): {', '.join(skipped_no_obj)}")
    print(f"\nGenerated {len(all_entries)} game target_symbol_map entries "
          f"across {len(all_stats)} TUs.")

    if args.apply:
        map_path = Path(args.map)
        existing: Dict[str, str] = {}
        if map_path.is_file():
            existing = json.loads(map_path.read_text())
        before = len(existing)
        # Merge: game entries override (they are authoritative for game addrs).
        overwritten = sum(1 for k in all_entries if k in existing)
        existing.update(all_entries)
        map_path.write_text(json.dumps(existing, indent=2, sort_keys=True) + "\n")
        print(f"\n[APPLIED] {map_path}: {before} -> {len(existing)} entries "
              f"(+{len(existing) - before} new, {overwritten} overwritten).")
    else:
        print("\n(dry-run) re-run with --apply to merge into the map.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
