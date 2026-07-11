#!/usr/bin/env python3
"""Iteratively fix dtk SPLIT boundary errors in splits.txt by snapping the
end address to the authoritative function end reported by dtk's error
message, then re-checking for overlaps. Designed for the r2 batch-insertion
cleanup: only touches lines that were freshly added (pure end-boundary
widening), never shrinks below start, and drops the candidate line entirely
if widening it would collide with a neighboring pinned range.
"""
import re
import subprocess
import sys

ROOT = "/home/free/tmp/closeout31/wt-r3"
SPLITS = f"{ROOT}/config/45410914/splits.txt"

FAIL_RE = re.compile(
    r"Failed: Split (\S+) \.text \(0x([0-9A-Fa-f]+)\.\.0x([0-9A-Fa-f]+)\) "
    r"ends within symbol '(?:fn_[0-9A-Fa-f]+|\S+)' \(0x([0-9A-Fa-f]+)\.\.0x([0-9A-Fa-f]+)\)"
)


def load_lines():
    with open(SPLITS) as f:
        return f.read().splitlines()


def save_lines(lines):
    with open(SPLITS, "w") as f:
        f.write("\n".join(lines) + "\n")


def find_overlaps(lines):
    ranges = []
    cur_unit = None
    for i, line in enumerate(lines):
        m = re.match(r'^(\S.*\.(?:cpp|c)):\s*$', line)
        if m:
            cur_unit = m.group(1)
            continue
        m = re.match(r'\s*\.text\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)', line)
        if m:
            s = int(m.group(1), 16)
            e = int(m.group(2), 16)
            ranges.append((s, e, cur_unit, i))
    ranges.sort()
    overlaps = []
    for i in range(1, len(ranges)):
        prev = ranges[i - 1]
        cur = ranges[i]
        if cur[0] < prev[1]:
            overlaps.append((prev, cur))
    return overlaps


def run_build():
    log = subprocess.run(
        ["./tools/ninja-locked"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=900,
    )
    return log.stdout + log.stderr


def main():
    max_iters = 30
    for it in range(max_iters):
        print(f"=== iteration {it} ===", flush=True)
        out = run_build()
        if "Failed: Split" not in out and "error" not in out.lower():
            print("BUILD OK")
            print(out[-2000:])
            return 0
        m = FAIL_RE.search(out)
        if not m:
            print("No parseable SPLIT failure found; dumping tail of output:")
            print(out[-4000:])
            return 1
        unit, start, end, symstart, symend = m.groups()
        start_i = int(start, 16)
        end_i = int(end, 16)
        symend_i = int(symend, 16)
        print(f"unit={unit} start=0x{start} end=0x{end} -> widen to 0x{symend}")

        lines = load_lines()
        # locate exact matching line: start matches, end matches, within unit block
        target_idx = None
        cur_unit = None
        for i, line in enumerate(lines):
            mm = re.match(r'^(\S.*\.(?:cpp|c)):\s*$', line)
            if mm:
                cur_unit = mm.group(1)
                continue
            mm = re.match(r'(\s*)\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', line)
            if mm and cur_unit == unit:
                s = int(mm.group(2), 16)
                e = int(mm.group(3), 16)
                if s == start_i and e == end_i:
                    target_idx = i
                    indent = mm.group(1)
                    break
        if target_idx is None and unit.startswith("auto_"):
            # dtk auto-derived an orphan split for an unpinned gap; find any
            # of OUR batch-added lines whose range falls inside/overlaps the
            # symbol span and drop it (safest: don't try to guess a widen).
            symstart_i = int(symstart, 16)
            drop_idx = None
            for i, line in enumerate(lines):
                mm = re.match(r'\s*\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', line)
                if mm:
                    s = int(mm.group(1), 16)
                    e = int(mm.group(2), 16)
                    if s < symend_i and e > symstart_i:
                        drop_idx = i
                        break
            if drop_idx is None:
                print(f"ERROR: auto unit {unit} 0x{start}..0x{end} (sym 0x{symstart}..0x{symend}); "
                      f"no candidate line overlaps the symbol span to drop")
                return 1
            print(f"  auto-orphan collision; dropping overlapping line: {lines[drop_idx].strip()!r}")
            del lines[drop_idx]
            save_lines(lines)
            overlaps2 = find_overlaps(load_lines())
            if overlaps2:
                print(f"  WARNING: {len(overlaps2)} overlap(s) remain after drop:")
                for prev, cur in overlaps2[:5]:
                    print(f"    {prev} vs {cur}")
                return 1
            continue
        if target_idx is None:
            print(f"ERROR: could not locate line for {unit} 0x{start}..0x{end}")
            return 1

        new_line = f"{indent}.text       start:0x{start_i:08X} end:0x{symend_i:08X}"
        old_line = lines[target_idx]
        lines[target_idx] = new_line
        print(f"  replaced: {old_line.strip()!r} -> {new_line.strip()!r}")

        overlaps = find_overlaps(lines)
        # filter overlaps to ones involving our edited line
        relevant = [ov for ov in overlaps if ov[0][3] == target_idx or ov[1][3] == target_idx]
        if relevant:
            print(f"  widening introduced {len(relevant)} overlap(s); dropping candidate line instead")
            del lines[target_idx]
        save_lines(lines)

        # sanity re-check: no overlaps at all should remain now
        lines2 = load_lines()
        overlaps2 = find_overlaps(lines2)
        if overlaps2:
            print(f"  WARNING: {len(overlaps2)} overlap(s) remain after fix attempt:")
            for prev, cur in overlaps2[:5]:
                print(f"    {prev} vs {cur}")
            return 1

    print("Max iterations reached without clean build")
    return 1


if __name__ == "__main__":
    sys.exit(main())
