#!/usr/bin/env python3
"""Find split .text ranges that TRUNCATE a function mid-body.

jeff-INDEPENDENT: works off dtk's emitted target asm (the instruction stream +
.fn/.endfn structural markers), NOT jeff's function-size table (which may be
wrong — tail calls, over-grow, stale cache). When a split's .text end falls
inside a function, dtk emits the function's `.fn` opener but never its `.endfn`
closer (the bytes ran out), and the file's last instruction is a mid-function
op instead of a terminator (blr/b/bctr). Both are unambiguous tells.

Only pinned TUs (those with a .text range in splits.txt) are considered.
"""
import re, sys, json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SPLITS = ROOT / "config/45410914/splits.txt"
ASM = ROOT / "build/45410914/asm"

TERMINATORS = {"blr", "blrl", "bctr", "b", "ba", "rfid", "rfi", "trap", "tw", "twi"}
INSN_RE = re.compile(r"^/\* ([0-9A-Fa-f]{8}) [0-9A-Fa-f]{8} +[0-9A-Fa-f ]+\*/\s*(\S+)")

# pinned TUs + their .text ranges
tus = {}  # basename -> list[(start,end)]
cur = None
for line in SPLITS.read_text().splitlines():
    h = re.match(r"^(\S+\.(?:cpp|c|cc)):\s*$", line)
    if h:
        cur = h.group(1); continue
    t = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
    if t and cur:
        tus.setdefault(cur, []).append((int(t.group(1),16), int(t.group(2),16)))

flagged = []
missing = []
for tu, ranges in sorted(tus.items()):
    # asm mirrors the src tree: band3/bandtrack/GemTrack.cpp -> asm/band3/bandtrack/GemTrack.s
    s = ASM / (tu.rsplit(".", 1)[0] + ".s")
    if not s.exists():
        # fall back to a recursive basename search (case-variant / relocated)
        base = Path(tu).stem
        hits = list(ASM.rglob(f"{base}.s"))
        if not hits:
            missing.append(tu); continue
        s = hits[0]
    text = s.read_text()
    nfn = len(re.findall(r"^\.fn ", text, re.M))
    nend = len(re.findall(r"^\.endfn ", text, re.M))
    # last real instruction
    last_addr = last_mn = None
    for line in text.splitlines():
        m = INSN_RE.match(line)
        if m:
            last_addr, last_mn = m.group(1), m.group(2)
    imbal = nfn - nend
    # A truncation = an UNCLOSED function (.fn with no .endfn). Trailing data
    # (.4byte jump tables / constant pools) after a closed function is benign,
    # so only an imbalance is decisive.
    if imbal > 0:
        # the split end is the max end across ranges
        end = max(e for _,e in ranges)
        flagged.append({"tu": tu, "imbalance": imbal, "last_addr": last_addr,
                        "last_insn": last_mn, "split_end": end,
                        "ranges": [[s_,e_] for s_,e_ in ranges]})

flagged.sort(key=lambda x: (-x["imbalance"], x["tu"]))
print(f"[scan] {len(tus)} pinned TUs, {len(missing)} with no .s file, {len(flagged)} FLAGGED truncations\n")
print(f"{'TU':<46} {'imbal':>5} {'last_insn':<22} {'last_addr':>9} {'split_end':>10}")
print("-"*96)
for f in flagged:
    print(f"{f['tu']:<46} {f['imbalance']:>5} {str(f['last_insn']):<22} {f['last_addr'] or '-':>9} 0x{f['split_end']:08X}")

json.dump(flagged, open("/tmp/truncated_splits.json","w"), indent=1)
print(f"\n[json] /tmp/truncated_splits.json  ({len(flagged)} candidates)")
if missing:
    print(f"[note] {len(missing)} pinned TUs had no matching .s (multi-source/case-variant names): {missing[:8]}{'...' if len(missing)>8 else ''}")
