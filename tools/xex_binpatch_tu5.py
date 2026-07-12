#!/usr/bin/env python3
"""XEX2 SECTION-MAPPED binary patcher for the TU5 (retail v0.0.5.1) Same-Instrument
code-cave patch.

Unlike the base default.xex (a FLAT loaded image where file_offset = 0x3000 +
(VA - 0x82000000)), the TU5 default_tu5.xex is a "basic"-format XEX whose stored
image is SECTION-MAPPED: each PE section's bytes sit at a section-specific file
offset, so the flat 0x3000+VA formula DRIFTS (empirically .text is +0x8000 off
flat, .data is -0x8000). VA -> XEX file offset must therefore go through the PE
section table.

Method (proven against tools/tu5_va.py's section map):
  * band_tu5.exe (dtk-extracted PE) gives VA -> dtk-PE-file-offset correctly.
  * The XEX stores each section's raw bytes at (dtk_off + delta_section), where
    delta_section is CONSTANT within a section but differs between sections.
  * We recover delta_section per section by anchoring a UNIQUE non-zero 24-byte
    signature taken near the section start and searching it in the XEX. Then
    xex_off(VA) = dtk_off(VA) + delta_section(VA's section).

This is validated at apply-time: every detour site must pre-read 7D8802A6
(mflr r12) and every cave word must pre-read 0 -- if the mapping were wrong these
invariants would fail loudly.

Subcommands:
    apply  --xex ORIG_TU5 --toml TU5_TOML --band BAND_TU5 --out OUT
           [--cave-base 0x82C8A000] [--report JSON] [--writes-json JSON]
    verify --xex PATCHED --toml TU5_TOML --band BAND_TU5 [--orig ORIG_TU5]
           [--cave-bin BIN] [--cave-base 0x82C8A000] [--report JSON]

Never mutates the input XEX; apply copies orig -> out.
"""
import argparse
import hashlib
import json
import os
import re
import struct
import sys

try:
    import tomllib as _toml
except ModuleNotFoundError:
    import tomli as _toml

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
from tu5_va import load_sections, va_to_off  # section-mapped VA->dtk-PE offset

XEX_BASE = 0x82000000
MFLR_R12 = 0x7D8802A6
DEFAULT_CAVE_BASE = 0x82C8A000
CAVE_SPAN = 0x1000  # cave region window for classification (blob is ~0xAF0)


def sha256(b):
    return hashlib.sha256(b).hexdigest()


# ---------------------------------------------------------------------------
# VA -> XEX-file-offset via per-section empirical delta
# ---------------------------------------------------------------------------
class Tu5Mapper:
    def __init__(self, band_path, xex_bytes):
        self.data, self.base, self.secs = load_sections(band_path)
        self.xex = xex_bytes
        self._delta = {}  # section name -> (xex_off - dtk_off)

    def _section_of(self, va):
        for name, sva, vsize, rawptr, rawsize in self.secs:
            if sva <= va < sva + vsize and (va - sva) < rawsize:
                return name, sva, vsize, rawptr, rawsize
        return None

    def _delta_for(self, name, sva, rawptr, rawsize):
        if name in self._delta:
            return self._delta[name]
        # anchor: first unique non-zero 24-byte window in the first 0x800 bytes
        limit = min(rawsize, 0x800)
        for off in range(rawptr, rawptr + limit - 24, 4):
            w = self.data[off:off + 24]
            if w.count(0) < 8:
                j = self.xex.find(w)
                if j >= 0 and self.xex.find(w, j + 1) < 0:
                    self._delta[name] = j - off
                    return self._delta[name]
        raise SystemExit(f"FATAL: could not anchor section {name} in XEX (no unique signature)")

    def va_to_xex(self, va):
        sec = self._section_of(va)
        if sec is None:
            raise SystemExit(f"FATAL: VA {va:#010x} not in any file-backed section of band_tu5.exe")
        name, sva, vsize, rawptr, rawsize = sec
        dtk_off = rawptr + (va - sva)
        delta = self._delta_for(name, sva, rawptr, rawsize)
        xoff = dtk_off + delta
        if not (0 <= xoff and xoff + 4 <= len(self.xex)):
            raise SystemExit(f"FATAL: VA {va:#010x} -> xex off {xoff:#x} out of bounds")
        return xoff, name


def load_writes(toml_path):
    with open(toml_path, "rb") as f:
        doc = _toml.load(f)
    writes = doc["patch"][0]["be32"]
    text = open(toml_path, "r", encoding="utf-8", errors="replace").read()
    regex_n = len(re.findall(r"address\s*=\s*(0x[0-9A-Fa-f]+)", text))
    if regex_n != len(writes):
        sys.exit(f"FATAL: toml nesting mismatch: parsed {len(writes)} vs regex {regex_n}")
    return [(int(w["address"]), int(w["value"]) & 0xFFFFFFFF) for w in writes]


def rd32(buf, off):
    return struct.unpack_from(">I", buf, off)[0]


def classify(va, cave_base):
    if cave_base <= va < cave_base + CAVE_SPAN:
        return "cave"
    return "detour"


def detect_detours(writes, cave_base):
    """Detours = writes outside the cave whose value is a plain `b`."""
    det = {}
    for va, val in writes:
        if classify(va, cave_base) == "detour":
            if (val & 0xFC000003) != 0x48000000:
                sys.exit(f"FATAL: non-cave write {va:#010x}={val:08X} is not a plain b (detour?)")
            disp = val & 0x03FFFFFC
            if disp & 0x02000000:
                disp -= 0x04000000
            det[va] = (va + disp) & 0xFFFFFFFF
    return det


# ---------------------------------------------------------------------------
def cmd_apply(args):
    orig = open(args.xex, "rb").read()
    if orig[:4] != b"XEX2":
        sys.exit("FATAL: not an XEX2 file")
    mapper = Tu5Mapper(args.band, orig)
    cave_base = int(args.cave_base, 16)
    writes = load_writes(args.toml)
    detours = detect_detours(writes, cave_base)

    # --- validation pass (all checks before mutating) ---
    seen = {}
    counts = {"cave": 0, "detour": 0}
    for va, val in writes:
        if va % 4:
            sys.exit(f"FATAL: unaligned address {va:#010x}")
        off, sec = mapper.va_to_xex(va)
        if va in seen:
            sys.exit(f"FATAL: duplicate write {va:#010x}")
        seen[va] = (off, sec)
        cls = classify(va, cave_base)
        pre = rd32(orig, off)
        if cls == "detour" and pre != MFLR_R12:
            sys.exit(f"FATAL: detour site {va:#010x} pre-image {pre:08X} != 7D8802A6 "
                     f"(wrong XEX / section-map error / already patched)")
        if cls == "cave" and pre != 0:
            sys.exit(f"FATAL: cave site {va:#010x} pre-image {pre:08X} != 0 "
                     f"(cave not free / section-map error / already patched)")
        counts[cls] += 1

    # detour displacement sanity
    for va, tgt in detours.items():
        if not (cave_base <= tgt < cave_base + CAVE_SPAN):
            sys.exit(f"FATAL: detour {va:#010x} -> {tgt:#010x} not into cave")

    # --- mutate a copy ---
    buf = bytearray(orig)
    audit = []
    for va, val in writes:
        off, sec = seen[va]
        old = rd32(buf, off)
        struct.pack_into(">I", buf, off, val)
        audit.append({"va": f"0x{va:08X}", "xex_off": f"0x{off:06X}", "section": sec,
                      "old": f"0x{old:08X}", "new": f"0x{val:08X}",
                      "class": classify(va, cave_base)})

    with open(args.out, "wb") as f:
        f.write(buf)

    report = {
        "action": "apply-tu5", "orig": args.xex, "out": args.out, "toml": args.toml,
        "band": args.band, "filesize": len(orig), "cave_base": f"0x{cave_base:08X}",
        "writes_total": len(writes), "counts": counts,
        "detours": {f"0x{va:08X}": f"0x{t:08X}" for va, t in detours.items()},
        "section_deltas": {k: f"0x{v:X}" for k, v in mapper._delta.items()},
        "sha256_orig": sha256(orig), "sha256_out": sha256(bytes(buf)),
    }
    print(json.dumps(report, indent=2))
    if args.report:
        json.dump(report, open(args.report, "w"), indent=2)
    if args.writes_json:
        json.dump(audit, open(args.writes_json, "w"), indent=2)
        print(f"[wrote sidecar {args.writes_json} ({len(audit)} writes)]")
    return 0


# ---------------------------------------------------------------------------
def cmd_verify(args):
    patched = open(args.xex, "rb").read()
    mapper = Tu5Mapper(args.band, patched)
    cave_base = int(args.cave_base, 16)
    writes = load_writes(args.toml)
    detours = detect_detours(writes, cave_base)
    results = []

    def add(name, ok, detail=""):
        results.append((name, ok, detail))

    # (1) all words re-read == toml
    bad = 0
    for va, val in writes:
        off, _ = mapper.va_to_xex(va)
        if rd32(patched, off) != val:
            bad += 1
    add("all_words_match", bad == 0, f"{len(writes)-bad}/{len(writes)} match")

    # (2) detour decode -> b cave
    try:
        import capstone
        md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_32)
    except ImportError:
        md = None
    for va, tgt in detours.items():
        off, _ = mapper.va_to_xex(va)
        w = rd32(patched, off)
        ok_op = (w & 0xFC000003) == 0x48000000
        disp = w & 0x03FFFFFC
        if disp & 0x02000000:
            disp -= 0x04000000
        got = (va + disp) & 0xFFFFFFFF
        txt = f"word={w:08X} -> b {got:#010x}"
        if md:
            ins = next(md.disasm(patched[off:off + 4], va), None)
            if ins:
                txt += f"  [{ins.mnemonic} {ins.op_str}]"
        add(f"detour {va:#010x} -> b {tgt:#010x}", ok_op and got == tgt, txt)

    # (3) cave region == cave.bin (flag word exception handled by toml already carrying 1)
    if args.cave_bin:
        cave = open(args.cave_bin, "rb").read()
        base_off, _ = mapper.va_to_xex(cave_base)
        mism = 0
        first = None
        for i in range(0, len(cave), 4):
            fw = rd32(patched, base_off + i)
            bw = struct.unpack_from(">I", cave, i)[0]
            # toml sets flag=1 where blob has 0; treat a patched 1 over blob 0 as ok
            if fw != bw and not (bw == 0 and fw == 1):
                mism += 1
                if first is None:
                    first = (i, fw, bw)
        add("cave_region==cave.bin", mism == 0,
            f"{len(cave)//4} words, {mism} mismatches" + (f" first={first}" if first else ""))

    # (4) whole-file diff: changed words are exactly the write set (+ no header change)
    if args.orig:
        orig = open(args.orig, "rb").read()
        if len(orig) != len(patched):
            add("whole_file_diff", False, f"size differs {len(orig)} vs {len(patched)}")
        else:
            expect = {}
            for va, val in writes:
                off, _ = mapper.va_to_xex(va)
                expect[off] = val
            changed = set()
            for off in range(0, len(patched) - 3, 4):
                if patched[off:off + 4] != orig[off:off + 4]:
                    changed.add(off)
            extra = changed - set(expect)
            missing = set(expect) - changed
            noop_ok = all(rd32(orig, o) == expect[o] for o in missing)
            add("whole_file_diff exact (changes == writes, no extra)",
                not extra and noop_ok,
                f"changed={len(changed)} expect={len(expect)} extra={len(extra)} "
                f"noop={len(missing)}(all==orig={noop_ok})")

    allok = all(ok for _, ok, _ in results)
    print(f"\n{'PASS' if allok else 'FAIL'}  verify-tu5 {args.xex}")
    print("-" * 100)
    for name, ok, detail in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name:44s} {detail}")
    print("-" * 100)
    print(f"section deltas: {{{', '.join('%s:0x%X' % (k, v) for k, v in mapper._delta.items())}}}")
    print(f"sha256(patched) = {sha256(patched)}")
    if args.report:
        json.dump({"action": "verify-tu5", "xex": args.xex, "all_pass": allok,
                   "sha256": sha256(patched),
                   "results": [{"check": n, "pass": ok, "detail": d} for n, ok, d in results]},
                  open(args.report, "w"), indent=2)
    return 0 if allok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("apply")
    a.add_argument("--xex", required=True)
    a.add_argument("--toml", required=True)
    a.add_argument("--band", required=True, help="band_tu5.exe (dtk PE, for section map)")
    a.add_argument("--out", required=True)
    a.add_argument("--cave-base", default=hex(DEFAULT_CAVE_BASE))
    a.add_argument("--report")
    a.add_argument("--writes-json")
    a.set_defaults(func=cmd_apply)
    v = sub.add_parser("verify")
    v.add_argument("--xex", required=True)
    v.add_argument("--toml", required=True)
    v.add_argument("--band", required=True)
    v.add_argument("--orig")
    v.add_argument("--cave-bin")
    v.add_argument("--cave-base", default=hex(DEFAULT_CAVE_BASE))
    v.add_argument("--report")
    v.set_defaults(func=cmd_verify)
    args = ap.parse_args()
    sys.exit(args.func(args))


if __name__ == "__main__":
    main()
