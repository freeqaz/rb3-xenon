#!/usr/bin/env python3
"""scope_map.py -- classify every function in the RB3-360 XEX into a decomp scope
bucket and compute a *meaningful* progress denominator.

The honest objdiff baseline (matched_code / total_code over the WHOLE 11.53 MB
.text) buries real progress under code we will never write from source: the XDK
import thunks and statically-linked BINK video middleware. This tool partitions
every function into one of six buckets so progress can be reported against the
*in-scope* denominator (game + engine + thirdparty + crt) instead.

Buckets
-------
  game        src/band3/, src/network/         IN-SCOPE   (HIGHEST priority)
  engine      src/system/ (Milo engine)        IN-SCOPE
  thirdparty  zlib/ogg/vorbis/tomcrypt/curl/   IN-SCOPE   (mechanical)
              json-c/expat/speex/STLport
  crt         src/xdk/LIBCMT/ (CRT we compile)  IN-SCOPE
  xdk         XEX-imported XDK + BINK middleware OUT-OF-SCOPE
  unknown     residual (classification TODO)    reported separately

Classification is layered, highest-confidence first:
  1. xdk        -- exact addr in xdk.json (import thunks + BINK)            [conf=1.0]
  2. pinned src -- unit has a source_path -> bucket by path                 [conf=1.0]
  3. thirdparty -- addr inside a thirdparty.json library range              [conf=0.9]
  4. engine/game-- addr in engine.json / game.json provenance labels        [conf=label]
  5. spatial    -- propagate the bucket of confident neighbors across an    [conf<=0.5]
                   unlabeled .text run (no LTCG => TUs stay contiguous)
  6. unknown    -- nothing said anything                                    [conf=0.0]

Outputs config/45410914/scope_map.json: {addr_hex: {size, scope, provenance,
confidence, matched}}.

Subcommands: build | report | worklist | classify <addr>
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, "build", "45410914", "report.json")
SCOPE_MAP = os.path.join(ROOT, "config", "45410914", "scope_map.json")

# ID-sweep artifacts (provenance/xdk/thirdparty classification inputs). Committed
# under tools/scope_data so `build` is reproducible from a fresh checkout; the
# generated scope_map.json is gitignored (rebuild it with `scope_map.py build`).
# Override with WN_DATA= to point at a fresh identification-sweep output dir.
WN_DATA = os.environ.get("WN_DATA", os.path.join(ROOT, "tools", "scope_data"))

FN_ADDR_RE = re.compile(r"fn_([0-9A-Fa-f]{8})")
AUTO_ADDR_RE = re.compile(r"auto_\d+_([0-9A-Fa-f]{8})_")

IN_SCOPE = {"game", "engine", "thirdparty", "crt"}
OUT_OF_SCOPE = {"xdk"}

# --- bucket ordering for stable reporting ---
BUCKET_ORDER = ["game", "engine", "thirdparty", "crt", "xdk", "unknown"]


# ---------------------------------------------------------------------------
# source_path -> bucket
# ---------------------------------------------------------------------------
THIRDPARTY_MARKERS = (
    "/zlib/", "/oggvorbis/", "/json-c/", "/curl/", "/tomcrypt/",
    "/speex/", "/expat/", "/stlport/", "/libpng/", "/jpeg/",
)


def bucket_for_source(sp):
    """Map a pinned unit source_path to a scope bucket. Returns None if unknown."""
    if not sp:
        return None
    low = sp.lower()
    # third-party libs (some live UNDER src/system/, so test before engine)
    for m in THIRDPARTY_MARKERS:
        if m in low:
            return "thirdparty"
    # CRT we compile ourselves
    if "/xdk/libcmt/" in low:
        return "crt"
    # XDK middleware glue we ship (nuiapi etc.) -- out of scope
    if low.startswith("src/xdk/") or "/xdk/nuiapi/" in low:
        return "xdk"
    # game layer
    if low.startswith("src/band3/") or low.startswith("src/network/"):
        return "game"
    # Milo engine
    if low.startswith("src/system/"):
        return "engine"
    # root-level platform glue (keygen_xbox.cpp, Memory_Xbox.cpp) -> engine
    if low.startswith("src/") and low.count("/") == 1:
        return "engine"
    return None


def bucket_for_engine_label(src_file):
    """engine.json src_file -> bucket. Mostly src/system/* (engine), but the
    sweep flagged hamobj/ dc3-false-friends and jpeg/ FP utility; those still
    land in engine/thirdparty respectively -- the per-entry confidence/note in
    engine.json already caveats them, and provenance records the file."""
    return bucket_for_source(src_file) or "engine"


SPLITS = os.path.join(ROOT, "config", "45410914", "splits.txt")
SPLIT_HDR_RE = re.compile(r"^(\S.*?):\s*$")
SPLIT_TEXT_RE = re.compile(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")
# Synthetic address space for fns we cannot place on real .text (a handful of
# pinned units with only a .pdata pin). Keyed high so they never collide with
# real .text (max real fn ~0x82C23xxx) and never participate in spatial runs.
SYNTH_BASE = 0xF0000000


def load_splits_text_bases(splits_path):
    """basename(no ext) -> .text start addr, from pinned splits.txt entries."""
    bases = {}
    if not os.path.exists(splits_path):
        return bases
    cur = None
    for line in open(splits_path):
        h = SPLIT_HDR_RE.match(line)
        if h and not line.startswith((" ", "\t")):
            cur = h.group(1)
        elif cur:
            t = SPLIT_TEXT_RE.search(line)
            if t:
                stem = cur.rsplit(".", 1)[0]
                bases[stem] = int(t.group(1), 16)
    return bases


# ---------------------------------------------------------------------------
# load report.json -> list of (addr, size, matched, source_path, unit)
# ---------------------------------------------------------------------------
def load_functions(report_path):
    with open(report_path) as f:
        rep = json.load(f)
    split_bases = load_splits_text_bases(SPLITS)
    funcs = []  # (addr:int, size:int, matched:bool, source_path, unit_name)
    synth = SYNTH_BASE
    for u in rep["units"]:
        unit = u["name"]
        sp = (u.get("metadata") or {}).get("source_path")
        fns = u.get("functions") or []
        if not fns:
            continue

        # -- PINNED SOURCE UNITS (have source_path) --
        # dtk emits clean per-unit relative offsets, so base + rel is exact.
        # Base from any fn_ anchor (base = abs - rel), else splits.txt .text
        # pin, else a synthetic island (handful of .pdata-only pinned units).
        if sp:
            base = None
            for fn in fns:
                m = FN_ADDR_RE.match(fn["name"])
                if m:
                    cand = int(m.group(1), 16) - int(fn.get("address", "0"))
                    if base is None or cand < base:
                        base = cand
            if base is None:
                base = split_bases.get(unit.split("/")[-1])
            if base is None:
                base = synth
                synth += 0x10000
            for fn in fns:
                m = FN_ADDR_RE.match(fn["name"])
                addr = int(m.group(1), 16) if m else base + int(fn.get("address", "0"))
                size = int(fn.get("size", "0"))
                matched = float(fn.get("match_percent_normalized", 0.0)) >= 100.0
                funcs.append((addr, size, matched, sp, unit))
            continue

        # -- CATCH-ALL / auto UNITS (no source_path) --
        # The report's per-fn `address` here is an internal monotonic index, NOT
        # a recoverable VA (catch-all units interleave many non-contiguous
        # regions and the offset drifts vs the true layout). So fn_ functions
        # are placed by their absolute name; NAMED functions (template insts /
        # thunks, all unmatched) are anchored to the nearest *preceding* fn_
        # anchor in listing order -- good enough to inherit that neighbor's
        # bucket via spatial locality -- with a tiny distinct delta to avoid
        # collisions. Functions are listed in `address`-monotonic order.
        last_anchor = None
        named_off = 0
        for fn in fns:
            m = FN_ADDR_RE.match(fn["name"])
            size = int(fn.get("size", "0"))
            matched = float(fn.get("match_percent_normalized", 0.0)) >= 100.0
            if m:
                addr = int(m.group(1), 16)
                last_anchor = addr
                named_off = 0
            else:
                if last_anchor is None:
                    # named fns before the first anchor: use unit-name base.
                    am = AUTO_ADDR_RE.search(unit)
                    last_anchor = int(am.group(1), 16) if am else SYNTH_BASE
                named_off += 1
                # small odd delta keeps these distinct from the anchor + each
                # other without crossing into the next real fn (sizes are >=8).
                addr = last_anchor + named_off  # 1..N within the anchor's slot
            funcs.append((addr, size, matched, sp, unit))
    # de-dup on addr (ICF folds / catch-all named-fn anchoring can land two
    # records on one addr). Precedence, best first:
    #   (1) pinned source_path beats catch-all   (we compile it; ground truth)
    #   (2) matched beats unmatched
    #   (3) larger size
    # Tuple ordering on (has_sp, matched, size) does exactly this.
    by_addr = {}
    for addr, size, matched, sp, unit in funcs:
        rank = (1 if sp else 0, 1 if matched else 0, size)
        cur = by_addr.get(addr)
        if cur is None or rank > cur[0]:
            by_addr[addr] = (rank, size, matched, sp, unit)
    out = [(addr, r[1], r[2], r[3], r[4]) for addr, r in by_addr.items()]
    out.sort()
    return out, rep


# ---------------------------------------------------------------------------
# ID-sweep artifact loaders
# ---------------------------------------------------------------------------
def load_xdk(wn_data):
    p = os.path.join(wn_data, "xdk.json")
    if not os.path.exists(p):
        return set()
    d = json.load(open(p))
    addrs = set()
    for a in d.get("xdk_fn_addrs", []):
        addrs.add(int(a, 16))
    # also fold in any BINK range so we catch gap fns between named exports
    return addrs


def load_xdk_bink_range(wn_data):
    p = os.path.join(wn_data, "xdk.json")
    if not os.path.exists(p):
        return None
    d = json.load(open(p))
    bs = d.get("bink_section") or {}
    if "code_start" in bs and "code_end" in bs:
        return (int(bs["code_start"], 16), int(bs["code_end"], 16))
    return None


def load_thirdparty_ranges(wn_data):
    p = os.path.join(wn_data, "thirdparty.json")
    if not os.path.exists(p):
        return []
    d = json.load(open(p))
    ranges = []  # (start, end, libname)
    for lib, entries in d.items():
        if not isinstance(entries, list):
            continue
        for e in entries:
            if "start" in e and "end" in e:
                ranges.append((int(e["start"], 16), int(e["end"], 16), lib))
    ranges.sort()
    return ranges


def load_provenance(wn_data, fname):
    """engine.json / game.json: {ADDR_HEX_no0x: {src_file, confidence[, note]}}"""
    p = os.path.join(wn_data, fname)
    if not os.path.exists(p):
        return {}
    d = json.load(open(p))
    out = {}
    for k, v in d.items():
        try:
            addr = int(k, 16)
        except ValueError:
            continue
        out[addr] = v
    return out


# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------
def normalize_conf(v):
    """confidence may be a string tier (engine.json) or a float (game.json)."""
    if isinstance(v, (int, float)):
        return float(v)
    tier = {
        "high": 0.85, "medium": 0.65, "low": 0.45, "low-single": 0.35,
        "low-generic": 0.2, "fp-jpeg-util": 0.15,
    }
    return tier.get(str(v), 0.4)


def cmd_build(args):
    funcs, rep = load_functions(REPORT)
    xdk_addrs = load_xdk(WN_DATA)
    bink_range = load_xdk_bink_range(WN_DATA)
    tp_ranges = load_thirdparty_ranges(WN_DATA)
    engine_prov = load_provenance(WN_DATA, "engine.json")
    game_prov = load_provenance(WN_DATA, "game.json")

    def in_tp(addr):
        for s, e, lib in tp_ranges:
            if s <= addr < e:
                return lib
        return None

    n = len(funcs)
    scope = [None] * n          # bucket string
    prov = [None] * n           # provenance string
    conf = [0.0] * n            # confidence float

    for i, (addr, size, matched, sp, unit) in enumerate(funcs):
        # Layer 1: XDK exact addr / BINK range -> OUT-OF-SCOPE.
        if addr in xdk_addrs or (bink_range and bink_range[0] <= addr < bink_range[1]):
            scope[i] = "xdk"
            prov[i] = "xdk.json:import-thunk-or-bink"
            conf[i] = 1.0
            continue
        # Layer 2: pinned-unit source_path (strongest -- we compile it).
        b = bucket_for_source(sp)
        if b:
            scope[i] = b
            prov[i] = "pinned:" + sp
            conf[i] = 1.0
            continue
        # Layer 3: thirdparty library address range.
        lib = in_tp(addr)
        if lib:
            scope[i] = "thirdparty"
            prov[i] = "thirdparty.json:" + lib
            conf[i] = 0.9
            continue
        # Layer 4: engine/game provenance labels (address-keyed).
        if addr in game_prov:
            v = game_prov[addr]
            scope[i] = bucket_for_source(v.get("src_file")) or "game"
            prov[i] = "game.json:" + str(v.get("src_file"))
            conf[i] = normalize_conf(v.get("confidence"))
            continue
        if addr in engine_prov:
            v = engine_prov[addr]
            scope[i] = bucket_for_engine_label(v.get("src_file"))
            note = v.get("note")
            prov[i] = "engine.json:" + str(v.get("src_file")) + (("|" + note) if note else "")
            conf[i] = normalize_conf(v.get("confidence"))
            continue
        # else: leave for spatial propagation.

    # ---- Layer 5: REACH-CAPPED SPATIAL CLUSTER PROPAGATION ----
    # No LTCG => functions of one TU sit in one contiguous .text run, so an
    # unlabeled function immediately adjacent to a confident label is very
    # likely the SAME TU (same bucket). But labels are sparse islands: ~12% of
    # bytes are confidently labeled (layers 1-4), and naive "fill the whole gap
    # from its endpoints" would absorb 5+ MB of genuinely-unidentified code
    # (the largest single gap spans 1.2 MB / 768 fns) into a bucket on no
    # evidence -- destroying the meaning of the denominator.
    #
    # So we propagate a confident edge's label only a bounded REACH into the
    # adjacent gap (REACH_BYTES per side, ~a couple of large TUs worth), and
    # only across functions that stay spatially contiguous (no jump larger than
    # GAP_JUMP between consecutive fns -- a big address jump implies a section
    # boundary / unrelated TU). Whatever the two edges cannot reach in a large
    # gap stays UNKNOWN -- the honest classification-TODO residual the metric
    # reports separately. XDK is an exact-addr island and is never propagated.
    REACH_BYTES = 6144      # ~one big TU of reach from each confident edge
    GAP_JUMP = 256          # consecutive-fn address jump that breaks a TU run

    def is_conf(i):
        return scope[i] is not None and scope[i] != "xdk"

    def fn_end(i):
        return funcs[i][0] + funcs[i][1]

    i = 0
    while i < n:
        if scope[i] is not None:
            i += 1
            continue
        j = i
        while j < n and scope[j] is None:
            j += 1
        left = i - 1
        right = j
        lb = scope[left] if left >= 0 and is_conf(left) else None
        rb = scope[right] if right < n and is_conf(right) else None

        # propagate from the LEFT edge rightward, within reach + contiguity
        if lb is not None:
            anchor_end = fn_end(left)
            prev_end = anchor_end
            for k in range(i, j):
                a = funcs[k][0]
                if a - prev_end > GAP_JUMP:
                    break                      # section/TU break -> stop reach
                if a - anchor_end > REACH_BYTES:
                    break                      # out of reach -> stop
                scope[k] = lb
                prov[k] = "spatial:after-" + lb
                conf[k] = 0.4
                prev_end = fn_end(k)
        # propagate from the RIGHT edge leftward into still-unlabeled fns
        if rb is not None:
            anchor_start = funcs[right][0]
            next_start = anchor_start
            for k in range(j - 1, i - 1, -1):
                if scope[k] is not None:
                    break                      # met the left-side fill
                e = fn_end(k)
                if next_start - e > GAP_JUMP:
                    break
                if anchor_start - funcs[k][0] > REACH_BYTES:
                    break
                # if both edges agree, bump confidence slightly (sandwiched)
                if scope[k] is None:
                    scope[k] = rb
                    prov[k] = ("spatial:between-" + rb) if rb == lb else ("spatial:before-" + rb)
                    conf[k] = 0.5 if rb == lb else 0.4
                next_start = funcs[k][0]

        # anything still unlabeled in this gap = genuine residual
        for k in range(i, j):
            if scope[k] is None:
                scope[k] = "unknown"
                prov[k] = "residual"
                conf[k] = 0.0
        i = j

    # ---- emit ----
    out = {}
    for i, (addr, size, matched, sp, unit) in enumerate(funcs):
        out["%08X" % addr] = {
            "size": size,
            "scope": scope[i],
            "provenance": prov[i],
            "confidence": round(conf[i], 3),
            "matched": matched,
        }
    os.makedirs(os.path.dirname(SCOPE_MAP), exist_ok=True)
    with open(SCOPE_MAP, "w") as f:
        json.dump(out, f, indent=0, sort_keys=True)
    print("wrote %s (%d functions)" % (SCOPE_MAP, len(out)))
    # quick summary
    _print_report(out)


# ---------------------------------------------------------------------------
# report
# ---------------------------------------------------------------------------
def _load_scope_map():
    with open(SCOPE_MAP) as f:
        return json.load(f)


def _print_report(sm):
    by = defaultdict(lambda: [0, 0, 0, 0])  # bucket -> [fns, bytes, matched_fns, matched_bytes]
    for addr, e in sm.items():
        b = e["scope"]
        by[b][0] += 1
        by[b][1] += e["size"]
        if e["matched"]:
            by[b][2] += 1
            by[b][3] += e["size"]
    total_bytes = sum(v[1] for v in by.values())
    total_fns = sum(v[0] for v in by.values())
    in_bytes = sum(by[b][1] for b in IN_SCOPE)
    in_fns = sum(by[b][0] for b in IN_SCOPE)
    in_matched_bytes = sum(by[b][3] for b in IN_SCOPE)
    in_matched_fns = sum(by[b][2] for b in IN_SCOPE)
    out_bytes = sum(by[b][1] for b in OUT_OF_SCOPE)
    unk_bytes = by["unknown"][1]
    unk_fns = by["unknown"][0]

    print()
    print("=== SCOPE BREAKDOWN (per bucket) ===")
    print("%-12s %8s %12s %8s %12s  %6s" %
          ("bucket", "fns", "bytes", "m_fns", "m_bytes", "in?"))
    for b in BUCKET_ORDER:
        fns, byt, mf, mb = by[b]
        flag = "IN" if b in IN_SCOPE else ("OUT" if b in OUT_OF_SCOPE else "n/a")
        print("%-12s %8d %12d %8d %12d  %6s" % (b, fns, byt, mf, mb, flag))
    print("-" * 64)
    print("%-12s %8d %12d %8d %12d" %
          ("TOTAL", total_fns, total_bytes, sum(v[2] for v in by.values()),
           sum(v[3] for v in by.values())))
    print()
    print("=== MEANINGFUL DENOMINATOR ===")
    print("in-scope (game+engine+thirdparty+crt):")
    print("  bytes: %d / %d total = %.2f%% of binary" %
          (in_bytes, total_bytes, 100.0 * in_bytes / total_bytes))
    print("  fns:   %d / %d total = %.2f%% of binary" %
          (in_fns, total_fns, 100.0 * in_fns / total_fns))
    print("  MATCHED-IN-SCOPE: %d/%d bytes = %.3f%%   |  %d/%d fns = %.3f%%" %
          (in_matched_bytes, in_bytes, 100.0 * in_matched_bytes / in_bytes,
           in_matched_fns, in_fns, 100.0 * in_matched_fns / in_fns))
    print("out-of-scope (xdk): %d bytes (%.2f%% of binary) -- excluded" %
          (out_bytes, 100.0 * out_bytes / total_bytes))
    print("unknown (classification TODO): %d bytes (%.2f%%) / %d fns (%.2f%%)" %
          (unk_bytes, 100.0 * unk_bytes / total_bytes,
           unk_fns, 100.0 * unk_fns / total_fns))
    print()
    print("vs honest objdiff metric: matched_code/total_code over WHOLE binary")
    tm = sum(v[3] for v in by.values())
    print("  whole-binary matched bytes: %d / %d = %.3f%%" %
          (tm, total_bytes, 100.0 * tm / total_bytes))


def cmd_report(args):
    _print_report(_load_scope_map())


# ---------------------------------------------------------------------------
# worklist -- size-ranked largest UNMATCHED in-scope clusters
# ---------------------------------------------------------------------------
def cmd_worklist(args):
    sm = _load_scope_map()
    items = sorted((int(a, 16), e) for a, e in sm.items())
    # Build contiguous clusters of same-provenance unmatched in-scope functions.
    # A cluster groups adjacent functions that share a normalized provenance key
    # (pinned source file, thirdparty lib, engine/game src_file, or spatial tag)
    # and are NOT matched and ARE in-scope.
    def prov_key(e):
        p = e["provenance"] or ""
        # collapse to the meaningful identity
        if p.startswith("pinned:"):
            return p
        if p.startswith("thirdparty.json:"):
            return p
        if p.startswith("engine.json:") or p.startswith("game.json:"):
            return p.split("|")[0]
        return "spatial:" + e["scope"]

    clusters = []  # (start, end, scope, prov, n_unmatched, unmatched_bytes, total_bytes)
    cur = None
    for addr, e in items:
        scope = e["scope"]
        in_scope = scope in IN_SCOPE
        unmatched = not e["matched"]
        key = (scope, prov_key(e)) if in_scope else None
        if in_scope and unmatched and cur and cur["key"] == key:
            cur["end"] = addr + e["size"]
            cur["n"] += 1
            cur["umb"] += e["size"]
        else:
            if cur:
                clusters.append(cur)
            if in_scope and unmatched:
                cur = {"key": key, "scope": scope, "prov": prov_key(e),
                       "start": addr, "end": addr + e["size"], "n": 1,
                       "umb": e["size"]}
            else:
                cur = None
    if cur:
        clusters.append(cur)

    clusters.sort(key=lambda c: c["umb"], reverse=True)
    limit = args.limit or 50
    bucket_filter = args.bucket
    print("=== UNMATCHED IN-SCOPE CLUSTERS (size-ranked) ===")
    print("%-10s %-10s %6s %9s  %-9s %s" %
          ("start", "end", "fns", "bytes", "bucket", "provenance"))
    shown = 0
    for c in clusters:
        if bucket_filter and c["scope"] != bucket_filter:
            continue
        prov = c["prov"]
        if prov.startswith("pinned:"):
            prov = prov[len("pinned:"):]
        print("0x%08X 0x%08X %6d %9d  %-9s %s" %
              (c["start"], c["end"], c["n"], c["umb"], c["scope"], prov))
        shown += 1
        if shown >= limit:
            break


# ---------------------------------------------------------------------------
# classify <addr>
# ---------------------------------------------------------------------------
def cmd_classify(args):
    sm = _load_scope_map()
    a = args.addr
    if a.lower().startswith("0x"):
        a = a[2:]
    key = ("%08X" % int(a, 16))
    e = sm.get(key)
    if not e:
        print("addr %s not in scope_map" % key)
        # nearest neighbor
        addrs = sorted(int(x, 16) for x in sm)
        target = int(a, 16)
        import bisect
        i = bisect.bisect_left(addrs, target)
        for j in (i - 1, i):
            if 0 <= j < len(addrs):
                nk = "%08X" % addrs[j]
                print("  nearest 0x%s -> %s" % (nk, json.dumps(sm[nk])))
        return
    print("0x%s -> %s" % (key, json.dumps(e, indent=2)))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("build", help="classify all fns, write scope_map.json")
    sub.add_parser("report", help="per-bucket breakdown + in-scope denominator")
    w = sub.add_parser("worklist", help="size-ranked unmatched in-scope clusters")
    w.add_argument("--limit", type=int, default=50)
    w.add_argument("--bucket", choices=BUCKET_ORDER, help="filter to one bucket")
    c = sub.add_parser("classify", help="classify a single addr")
    c.add_argument("addr")
    args = ap.parse_args()
    {"build": cmd_build, "report": cmd_report,
     "worklist": cmd_worklist, "classify": cmd_classify}[args.cmd](args)


if __name__ == "__main__":
    main()
