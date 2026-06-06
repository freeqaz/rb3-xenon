#!/usr/bin/env python3
"""Census of the R-B tree +4 coupled-base blast radius.

Retail STLport `_Rb_tree` is 0x1c; ours is 0x18 (missing one 4-byte member after
`_M_key_compare`, see _tree.h:314-316). EVERY std::map/set/multimap/multiset member
in the codebase is therefore 4 bytes too small, shifting every member declared AFTER
a map/set member by -4 relative to retail. Classes that nonetheless MATCH today do so
because someone manually inserted a 4-byte compensation pad after the map/set member;
once the tree itself grows to 0x1c those pads DOUBLE-count and must be unwound.

This tool enumerates, deterministically:
  * every class that EMBEDS one or more map/set/multimap/multiset members  (will IMPROVE)
  * candidate manual compensation pads adjacent to those members           (must UNWIND)
  * per-unit match state from report.json                                  (regression risk)

Usage:
  python3 tools/rbtree_blast.py                # human summary
  python3 tools/rbtree_blast.py --json         # machine-readable, for workflow agents
  python3 tools/rbtree_blast.py --pads-only    # just the unwind candidate list
"""
import json, os, re, sys, argparse
from collections import defaultdict

ROOT = "/home/free/code/milohax/rb3-xenon"

MAP_TYPE = re.compile(r'\b(?:std::)?(multimap|multiset|map|set)\s*<')
# a member declaration line ending in `name;` (no parens => not a function)
MEMBER = re.compile(r'(\b[A-Za-z_]\w*)\s*;\s*(?://.*)?$')
CLASS_OPEN = re.compile(r'\b(?:class|struct)\s+([A-Za-z_]\w*)\b')
PAD_NAME = re.compile(r'(?i)\b(unk\w*|pad\w*|_pad\w*|reserved\w*|filler\w*|dummy\w*|mPad\w*|mUnk\w*)\s*;')
SCALAR_TYPE = re.compile(
    r'^\s*(?:unsigned\s+|signed\s+)?'
    r'(?:int|short|long|char|bool|float|void\s*\*|u8|u16|u32|s8|s16|s32|'
    r'unsigned|uint\w*|int\w*|DWORD|WORD|BYTE)\b'
)

def strip_line(s):
    # crude comment strip; good enough for member scanning
    s = re.sub(r'//.*$', '', s)
    return s

def scan_header(path):
    """Return (map_members, pad_members) found in this header.
    map_members: list of dict(cls, member, type, line)
    pad_members: list of dict(cls, member, line, raw)
    """
    try:
        lines = open(path, errors="ignore").read().splitlines()
    except Exception:
        return [], []
    map_members, pad_members = [], []
    # class scope stack: list of (name, depth_at_open)
    stack = []
    depth = 0
    pending_class = None  # class name seen, awaiting its '{'
    for i, raw in enumerate(lines, 1):
        line = strip_line(raw)
        cm = CLASS_OPEN.search(line)
        if cm and '{' not in line.split(cm.group(0))[0]:
            # found a class/struct keyword; its body opens at the next '{'
            pending_class = cm.group(1)
        opens = line.count('{')
        closes = line.count('}')
        # attribute member lines to innermost open class (depth-based, before brace update)
        cur_cls = stack[-1][0] if stack else None
        if cur_cls and ';' in line and '(' not in line and '{' not in line:
            mm = MAP_TYPE.search(line)
            if mm:
                nm = MEMBER.search(line)
                if nm:
                    map_members.append(dict(cls=cur_cls, member=nm.group(1),
                                            type=mm.group(1), line=i, file=path))
            pn = PAD_NAME.search(line)
            if pn and SCALAR_TYPE.match(line):
                pad_members.append(dict(cls=cur_cls, member=pn.group(1).split(';')[0].strip(),
                                        line=i, file=path, raw=raw.strip()))
        # update brace depth + class stack
        for _ in range(opens):
            if pending_class is not None:
                stack.append((pending_class, depth))
                pending_class = None
            depth += 1
        for _ in range(closes):
            depth -= 1
            if stack and stack[-1][1] == depth:
                stack.pop()
    return map_members, pad_members

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--pads-only", action="store_true")
    ap.add_argument("--report", default=os.path.join(ROOT, "build/45410914/report.json"))
    ap.add_argument("--src", default=os.path.join(ROOT, "src"))
    a = ap.parse_args()

    headers = []
    for dp, _, fns in os.walk(a.src):
        # skip stlport itself (the template def, not a consumer)
        if "stlport" in dp:
            continue
        for f in fns:
            if f.endswith((".h", ".hpp")):
                headers.append(os.path.join(dp, f))

    by_class_maps = defaultdict(list)
    by_class_pads = defaultdict(list)
    files_with_maps = set()
    for h in headers:
        mm, pm = scan_header(h)
        for m in mm:
            by_class_maps[m["cls"]].append(m)
            files_with_maps.add(h)
        for p in pm:
            by_class_pads[p["cls"]].append(p)

    # match state per class via unit basename heuristic
    rep = json.load(open(a.report)) if os.path.exists(a.report) else {"units": []}
    unit_by_base = {os.path.basename(u["name"]): u for u in rep.get("units", [])}
    def state(cls):
        u = unit_by_base.get(cls)
        if not u:
            return dict(unit=None, fns=0, matched=0, near=0)
        fns = u.get("functions", [])
        matched = sum(1 for f in fns if f.get("match_percent_normalized", 0) >= 100)
        near = sum(1 for f in fns if 90 <= f.get("match_percent_normalized", 0) < 100)
        return dict(unit=u["name"], fns=len(fns), matched=matched, near=near)

    # build per-class records, classify
    records = []
    for cls, maps in by_class_maps.items():
        pads = by_class_pads.get(cls, [])
        # a pad is "suspicious" (likely map-compensation) if its line is within
        # ~3 declaration-lines after a map member line
        map_lines = sorted(m["line"] for m in maps)
        suspicious = []
        for p in pads:
            after = [ml for ml in map_lines if 0 < p["line"] - ml <= 8]
            if after:
                suspicious.append(dict(p, after_map_line=max(after)))
        st = state(cls)
        records.append(dict(cls=cls, n_maps=len(maps), maps=maps,
                            n_pads=len(pads), suspicious_pads=suspicious, **st))

    records.sort(key=lambda r: (-r["n_maps"], -(r["near"]), r["cls"]))

    all_suspicious = [sp for r in records for sp in r["suspicious_pads"]]

    out = dict(
        n_files_with_maps=len(files_with_maps),
        n_classes_with_maps=len(by_class_maps),
        total_map_members=sum(len(v) for v in by_class_maps.values()),
        n_suspicious_pads=len(all_suspicious),
        records=records,
        suspicious_pads=all_suspicious,
    )

    if a.pads_only:
        for sp in all_suspicious:
            rel = os.path.relpath(sp["file"], ROOT)
            print(f"{rel}:{sp['line']}  {sp['cls']}::{sp['member']}  (after map @line {sp['after_map_line']})  |  {sp['raw']}")
        print(f"\n{len(all_suspicious)} suspicious compensation pads")
        return

    if a.json:
        print(json.dumps(out, indent=2))
        return

    print(f"=== R-B tree +4 blast radius ===")
    print(f"files with map/set members : {out['n_files_with_maps']}")
    print(f"classes embedding map/set  : {out['n_classes_with_maps']}")
    print(f"total map/set members      : {out['total_map_members']}")
    print(f"suspicious comp. pads      : {out['n_suspicious_pads']}\n")
    print(f"{'class':32s} {'#map':>4} {'#pad':>4} {'fns':>4} {'matched':>7} {'near':>4}  unit")
    for r in records[:60]:
        print(f"{r['cls'][:32]:32s} {r['n_maps']:>4} {len(r['suspicious_pads']):>4} "
              f"{r['fns']:>4} {r['matched']:>7} {r['near']:>4}  {r['unit'] or '-'}")
    print(f"\n(showing top 60 of {len(records)} classes; --json for all, --pads-only for unwind list)")

if __name__ == "__main__":
    main()
