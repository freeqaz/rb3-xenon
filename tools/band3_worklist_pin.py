#!/usr/bin/env python3
"""Deterministic band3-worklist micro-pin + name (NO broad-oracle scatter).

The safe replacement for the Sonnet `identity_transfer --tu X --apply` step that
caused the GemPlayer disaster (broad near-random oracle -> ~90 scattered foreign
pins). This pins + names ONLY the high-precision worklist addresses for a TU.

Per worklist identity (band3_port_worklist.json -> rb3_addr, wii_demangled, src_path):
  * locate the target VA: in the TU's own pinned span (name-only) / an UNOWNED auto_
    blob (micro-pin [VA, VA+size) under the TU header, then name) / a FOREIGN pin (skip).
  * NAME via gen_game_target_map's safe demangle-and-match (a restricted mini-oracle
    of ONLY this TU's worklist addrs) -> VA -> MSVC-mangled, ADD-ONLY.

Only operates on WIRED TUs (compiled obj exists). Unwired TUs need a source port first.

Usage:
  tools/band3_worklist_pin.py --tu VocalTrack.cpp [--tu Gem.cpp ...]   # specific
  tools/band3_worklist_pin.py --all-wired                              # all wired+unnamed
  (--apply writes splits.txt + runs gen_game_target_map; default is dry-run)

  --worklist PATH   consume a different worklist JSON (default: band3_port_worklist.json
                     in ROOT). Same {rb3_addr, wii_demangled, src_path} schema, e.g.:
  tools/band3_worklist_pin.py --worklist sysnet_port_worklist.json --all-wired
"""
import argparse, json, re, subprocess, sys
from pathlib import Path

# Worktree-aware: default ROOT is the repo containing THIS script (so the tool
# operates on whatever tree it's invoked from — main or a setup_worktree.sh
# worktree). Override with --root for an explicit tree.
ROOT = Path(__file__).resolve().parents[1]
WORKLIST = ROOT / "band3_port_worklist.json"
SPLITS = ROOT / "config/45410914/splits.txt"
OBJECTS = ROOT / "config/45410914/objects.json"
SYMBOLS = ROOT / "config/45410914/symbols.txt"
TMAP = ROOT / "scripts/target_symbol_map.json"


def _rebind_root(root):
    """Re-point all path globals at an explicit tree (for --root)."""
    global ROOT, WORKLIST, SPLITS, OBJECTS, SYMBOLS, TMAP
    ROOT = Path(root).resolve()
    WORKLIST = ROOT / "band3_port_worklist.json"
    SPLITS = ROOT / "config/45410914/splits.txt"
    OBJECTS = ROOT / "config/45410914/objects.json"
    SYMBOLS = ROOT / "config/45410914/symbols.txt"
    TMAP = ROOT / "scripts/target_symbol_map.json"


def load_pins():
    """Return sorted list of (start,end,tu) .text ranges + per-TU set."""
    txt = SPLITS.read_text().split("\n")
    cur = None
    ranges = []
    for ln in txt:
        m = re.match(r"^(\S+\.cpp):", ln)
        if m:
            cur = m.group(1)
        mr = re.search(r"\.text\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)", ln)
        if mr:
            ranges.append((int(mr.group(1), 16), int(mr.group(2), 16), cur))
    return sorted(ranges)


def load_sizes():
    """VA(int) -> size from symbols.txt (name ... size:0xNN)."""
    out = {}
    for ln in SYMBOLS.read_text().split("\n"):
        ma = re.search(r"=\s*\.text:(0x[0-9A-Fa-f]+)", ln)
        ms = re.search(r"size:(0x[0-9A-Fa-f]+)", ln)
        if ma and ms:
            out[int(ma.group(1), 16)] = int(ms.group(1), 16)
    return out


def locate(va, ranges):
    """Return ('own'|'foreign'|'unowned', owning_tu_or_None)."""
    for lo, hi, tu in ranges:
        if lo <= va < hi:
            return ("inpin", tu)
    return ("unowned", None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tu", action="append", default=[])
    ap.add_argument("--all-wired", action="store_true")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--root", default=None, help="operate on this tree (default: script's repo)")
    ap.add_argument("--worklist", default=None,
                     help="worklist JSON to consume (default: band3_port_worklist.json in ROOT; "
                          "e.g. sysnet_port_worklist.json — same {rb3_addr,wii_demangled,src_path} schema)")
    args = ap.parse_args()
    if args.root:
        _rebind_root(args.root)
    global WORKLIST
    if args.worklist:
        wl_path = Path(args.worklist)
        if not wl_path.is_absolute():
            wl_path = ROOT / wl_path
        WORKLIST = wl_path

    rows = json.loads(WORKLIST.read_text())["worklist"]
    objects = OBJECTS.read_text()
    named = {k.lower() for k in json.loads(TMAP.read_text())}
    ranges = load_pins()
    sizes = load_sizes()

    # group worklist by TU (.cpp basename)
    by_tu = {}
    for r in rows:
        base = r["src_path"].rsplit("/", 1)[-1]
        by_tu.setdefault(base, []).append(r)

    def is_wired(tu_cpp, src):
        rel = src.replace("src/", "")
        return f'"{rel}"' in objects

    targets = []
    if args.all_wired:
        targets = [t for t in by_tu if is_wired(t, by_tu[t][0]["src_path"])]
    else:
        targets = [t if t.endswith(".cpp") else t + ".cpp" for t in args.tu]

    mini_oracle = []  # unified_id_rb3wii.json format entries to name
    pins_to_add = {}  # tu -> [(va,end)]
    report = []
    for tu in targets:
        ids = by_tu.get(tu, [])
        if not ids:
            print(f"  {tu}: not in worklist"); continue
        src = ids[0]["src_path"]
        if not is_wired(tu, src):
            print(f"  {tu}: UNWIRED (needs source port first) — skip"); continue
        for r in ids:
            va = int(r["rb3_addr"], 16)
            if r["rb3_addr"].lower() in named:
                continue  # already named
            kind, owner = locate(va, ranges)
            owner_base = owner.rsplit("/", 1)[-1] if owner else owner
            if kind == "inpin" and owner_base != tu:
                report.append((tu, r["rb3_addr"], "SKIP foreign-pin(%s)" % owner)); continue
            sz = sizes.get(va)
            if kind == "unowned":
                if not sz:
                    report.append((tu, r["rb3_addr"], "SKIP no-size")); continue
                pins_to_add.setdefault(tu, []).append((va, va + sz))
                report.append((tu, r["rb3_addr"], "MICRO-PIN+NAME %s" % r["wii_demangled"][:40]))
            else:  # own pin -> name only
                report.append((tu, r["rb3_addr"], "NAME-only %s" % r["wii_demangled"][:40]))
            # mini-oracle entry (gen_game_target_map format)
            mini_oracle.append({
                "rb3_addr": r["rb3_addr"],
                "wii_name": r["wii_demangled"].replace("(...)", "()"),  # placeholder args -> ()
                "bindiff_src": src,
                "similarity": float(r.get("simconf", 30) or 30) / 100.0,
                "confidence": 0.9,
            })

    for tu, va, action in report:
        print(f"  {tu:26s} {va} {action}")
    print(f"\n{len([r for r in report if 'MICRO-PIN' in r[2] or 'NAME' in r[2]])} nameable "
          f"({sum(len(v) for v in pins_to_add.values())} need micro-pins, "
          f"{len([r for r in report if r[2].startswith('SKIP')])} skipped)")

    if not args.apply:
        print("\n(dry-run; pass --apply to write splits + run gen_game_target_map)")
        return 0

    # 1. add micro-pins to splits.txt (under each TU header, ADD-ONLY)
    if pins_to_add:
        txt = SPLITS.read_text()
        for tu, pins in pins_to_add.items():
            pins = [(lo, hi) for lo, hi in pins if f"start:{lo:#x} end:{hi:#x}" not in txt]
            if not pins:
                continue
            block = "".join(f"\t.text       start:{lo:#x} end:{hi:#x}\n" for lo, hi in pins)
            if re.search(rf"^{re.escape(tu)}:", txt, re.M):
                # append .text lines right after the TU header
                txt = re.sub(rf"(^{re.escape(tu)}:\n)", r"\1" + block, txt, count=1, flags=re.M)
            else:
                txt += f"\n{tu}:\n{block}"
        SPLITS.write_text(txt)
        print(f"applied {sum(len(v) for v in pins_to_add.values())} micro-pins to splits.txt")

    # 2. DIRECT demangle-and-match naming (reuse gen_game_target_map primitives,
    #    skip its purity>=0.7 per-TU gate which is for the broad oracle, not this
    #    high-precision per-fn worklist).
    import importlib.util
    from collections import defaultdict
    spec = importlib.util.spec_from_file_location("ggtm", str(ROOT / "tools/gen_game_target_map.py"))
    ggtm = importlib.util.module_from_spec(spec); spec.loader.exec_module(ggtm)
    obj_dir = ROOT / "build/45410914/src"

    # group mini-oracle by TU
    by_tu_oracle = defaultdict(list)
    for e in mini_oracle:
        by_tu_oracle[e["bindiff_src"].rsplit("/", 1)[-1]].append(e)

    cur_map = json.loads(TMAP.read_text())
    added = 0
    for tu, entries in by_tu_oracle.items():
        obj_path = ggtm.find_obj(obj_dir, tu)
        if not obj_path:
            print(f"  {tu}: no compiled obj — skip naming"); continue
        syms = ggtm.parse_coff_defined(obj_path.read_bytes())
        by_cm = defaultdict(list)
        for name, secnum in syms:
            if secnum <= 0 or not name.startswith("?"):
                continue
            cm = ggtm.msvc_class_method(name)
            if cm:
                by_cm[(cm[0], cm[1])].append(name)
        for e in entries:
            if e["rb3_addr"].lower() in {k.lower() for k in cur_map}:
                continue
            p = ggtm.parse_wii_name(e["wii_name"])
            if not p:
                continue
            cls, method, argc, kind = p
            cands = by_cm.get((cls, method))
            if not cands:
                continue
            chosen = cands[0] if len(cands) == 1 else None
            if chosen is None and argc is not None:  # disambiguate by argcount
                ac = [c for c in cands if ggtm.msvc_argcount(c) == argc]
                if len(ac) == 1:
                    chosen = ac[0]
            if chosen:
                cur_map[e["rb3_addr"]] = chosen
                added += 1
                print(f"    NAMED {e['rb3_addr']} -> {chosen[:55]}")
    json.dump(cur_map, open(TMAP, "w"), indent=1)
    print(f"\nadded {added} ADD-ONLY map names (worklist high-precision, no purity gate)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
