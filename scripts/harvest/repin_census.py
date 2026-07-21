#!/usr/bin/env python3
"""repin_census — batch-8 truncated/fragmented-pin census.

Finds UNDER-CARVED and FRAGMENTED pins by locating unpinned .text runs that are
ADJACENT (either direction) or in an INTER-FRAGMENT GAP of an already-pinned TU
AND attributable to that same TU by string fingerprint.

Mechanism (why this is free yield): under /O1 no-LTCG, retail preserves TU source
order contiguously in .text. A pin that covers only part of its TU leaves the rest
unpinned right next to it (ShaderMgr was a BACKWARD under-carve — the run sits
*before* its own pin). Extending/merging the pin to the TU's true extent exposes
those bodies to objdiff; automap then pairs them for near-free strict flips.

Attribution: each unpinned run's fingerprint strings are tested for membership in
the source text of its neighbouring pinned TUs (own tree src/, then rb3-Wii and
dc3 oracles). The neighbour with the strongest overlap owns the run; the OTHER
neighbour's score is recorded as interleave/ambiguity risk. Runs whose best score
is below --floor are UNATTRIBUTED (new-TU wire candidates, not repin candidates).

Three candidate shapes:
  FORWARD   run sits right AFTER a pin, attributed to that (preceding) TU  -> extend end
  BACKWARD  run sits right BEFORE a pin, attributed to that (following) TU -> extend start
  INTERNAL  run sits in a GAP BETWEEN TWO FRAGMENTS of one TU             -> merge/fill

Read-only. Emits a ranked table + JSON. Extensions down to ~256 B are reported.

Usage:
    repin_census.py                       # full MAIN-region census
    repin_census.py --min-bytes 256
    repin_census.py --floor 0.30          # attribution score floor
    repin_census.py --json OUT.json
    repin_census.py --region 0x82270000,0x82840000
"""
import argparse, bisect, json, os, re, glob, sys

ROOT = "/home/free/code/milohax/rb3-xenon"


def load_pins():
    tus = {}
    cur = None
    for ln in open(f"{ROOT}/config/45410914/splits.txt"):
        m = re.match(r'^(\S+\.(cpp|c)):', ln.strip())
        if m:
            cur = m.group(1)
            tus.setdefault(cur, {"frags": []})
        mt = re.search(r'\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', ln)
        if mt and cur:
            tus[cur]["frags"].append((int(mt.group(1), 16), int(mt.group(2), 16)))
    out = {}
    for tu, d in tus.items():
        if not d["frags"]:
            continue
        fr = sorted(d["frags"])
        out[tu] = dict(frags=fr, start=fr[0][0], end=fr[-1][1],
                       pinned_bytes=sum(e - s for s, e in fr), nfrag=len(fr))
    return out


_SRC_CACHE = {}
_SRC_ROOTS = [f"{ROOT}/src", f"{ROOT}/../rb3/src", f"{ROOT}/../dc3-decomp/src"]

def src_text(tu):
    if tu in _SRC_CACHE:
        return _SRC_CACHE[tu]
    base = os.path.basename(tu)
    texts = []
    for r in _SRC_ROOTS:
        for hit in glob.glob(f"{r}/**/{base}", recursive=True):
            try:
                texts.append(open(hit, errors="ignore").read())
            except Exception:
                pass
    blob = "\n".join(texts).lower()
    _SRC_CACHE[tu] = (blob, bool(texts))
    return _SRC_CACHE[tu]

def overlap_score(strings, tu):
    txt, have = src_text(tu)
    if not have:
        return None, 0            # no source oracle -> unknown, not zero
    denom = sum(1 for s in strings if len(s) >= 4)
    if not denom:
        return None, 0
    hits = sum(1 for s in strings if len(s) >= 4 and s.lower() in txt)
    return hits / denom, hits


_FN_RX = re.compile(r'^\s*\.fn\s+([^,]+)')
# a real instruction line: /* VA FILEOFF  BB BB BB BB */\t<mnemonic>
_INSTR_RX = re.compile(r'^\s*/\* [0-9A-Fa-f]{8} [0-9A-Fa-f]{8}  [0-9A-Fa-f ]+\*/\t')

def load_dtk_targets(build_dir):
    """Authoritative dtk target-function table: VA -> size (bytes) from every
    build asm. dtk's function boundaries differ from fingerprints.json in some
    regions (e.g. MusicLibrary: fp sees 11 big fns, dtk sees 65) — dtk is what
    the target obj / automap actually pairs, so it is the honest yield signal."""
    from pathlib import Path
    tgt = {}
    asmdir = Path(build_dir) / "asm"
    if not asmdir.exists():
        return tgt
    for asm in asmdir.rglob("*.s"):
        cur_va = None; nins = 0; in_fn = False
        for line in asm.read_text(errors="ignore").splitlines():
            fm = _FN_RX.match(line)
            if fm:
                m = re.match(r'fn_([0-9A-Fa-f]{8})$', fm.group(1))
                cur_va = int(m.group(1), 16) if m else None
                in_fn = True; nins = 0
                continue
            if line.startswith('.endfn'):
                if in_fn and cur_va is not None:
                    tgt[cur_va] = nins * 4     # PPC: 4 B / instruction
                in_fn = False
                continue
            if in_fn and _INSTR_RX.match(line):
                nins += 1
    return tgt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-bytes", type=int, default=256)
    ap.add_argument("--region", default="0x82270000,0x82840000")
    ap.add_argument("--gap", type=lambda x: int(x, 0), default=0x800)
    ap.add_argument("--floor", type=float, default=0.30)
    ap.add_argument("--build-dir", default=None,
                    help="worktree build/45410914 to read authoritative dtk target "
                         "sizes from (enables the real automap-yield column + rank)")
    ap.add_argument("--json", default="/home/free/tmp/repin_batch8.json")
    args = ap.parse_args()
    lo, hi = [int(x, 16) for x in args.region.split(",")]

    tus = load_pins()
    iv = sorted((s, e, tu) for tu, d in tus.items() for s, e in d["frags"])
    starts = [s for s, _, _ in iv]

    def pin_at(va):
        i = bisect.bisect_right(starts, va) - 1
        return iv[i][2] if (i >= 0 and iv[i][0] <= va < iv[i][1]) else None

    # inter-fragment gaps per TU: (gap_lo, gap_hi, tu)
    frag_gaps = []
    for tu, d in tus.items():
        fr = d["frags"]
        for a, b in zip(fr, fr[1:]):
            if b[0] > a[1]:
                frag_gaps.append((a[1], b[0], tu))
    frag_gaps.sort()
    fg_lo = [g[0] for g in frag_gaps]

    def containing_frag_gap(s, e):
        i = bisect.bisect_right(fg_lo, s) - 1
        if i >= 0 and frag_gaps[i][0] <= s and e <= frag_gaps[i][1]:
            return frag_gaps[i][2]
        return None

    dtk = load_dtk_targets(args.build_dir) if args.build_dir else {}
    dtk_vas = sorted(dtk)

    fp = json.load(open(f"{ROOT}/fingerprints.json"))
    fns = sorted(((int(k, 16), v["size"], v.get("strings", []))
                  for k, v in fp.items()), key=lambda x: x[0])

    runs = []
    cur = []
    for va, sz, strs in fns:
        if not (lo <= va < hi):
            continue
        if pin_at(va) is not None:
            if cur:
                runs.append(cur); cur = []
            continue
        if cur and va - (cur[-1][0] + cur[-1][1]) > args.gap:
            runs.append(cur); cur = []
        cur.append((va, sz, strs))
    if cur:
        runs.append(cur)

    rows = []
    for run in runs:
        s = run[0][0]; e = run[-1][0] + run[-1][1]
        nb = e - s; nf = len(run)
        if nb < args.min_bytes:
            continue
        # real-body proxy: functions large enough to not be an ICF thunk / EH
        # funclet (>=0x40 B). raw byte/fn counts are inflated by 32-B thunks that
        # rarely contribute proportional strict flips.
        nbody = sum(1 for va, sz, st in run if sz >= 0x40)
        body_bytes = sum(sz for va, sz, st in run if sz >= 0x40)
        # authoritative dtk yield: target fns (and >=0x40 bodies) dtk emits in span
        dtk_fns = dtk_big = None
        if dtk:
            i = bisect.bisect_left(dtk_vas, s)
            dtk_fns = 0; dtk_big = 0
            while i < len(dtk_vas) and dtk_vas[i] < e:
                dtk_fns += 1
                if dtk[dtk_vas[i]] >= 0x40:
                    dtk_big += 1
                i += 1
        strset = {x for _, _, st in run for x in st if len(x) >= 4}
        strings = sorted(strset)

        # neighbours
        j = bisect.bisect_left(starts, s) - 1
        while j >= 0 and iv[j][1] > s:
            j -= 1
        pv = iv[j] if j >= 0 else None
        k = bisect.bisect_left(starts, e)
        nx = iv[k] if k < len(iv) else None
        gap_prev = (s - pv[1]) if pv else None
        gap_next = (nx[0] - e) if nx else None

        sc_pv, h_pv = overlap_score(strings, pv[2]) if pv else (None, 0)
        sc_nx, h_nx = overlap_score(strings, nx[2]) if nx else (None, 0)

        ig = containing_frag_gap(s, e)
        sc_ig, h_ig = overlap_score(strings, ig) if ig else (None, 0)

        # choose attribution
        opts = []
        if pv and sc_pv is not None:
            opts.append(("FORWARD", pv[2], sc_pv, h_pv, gap_prev, sc_nx))
        if nx and sc_nx is not None:
            opts.append(("BACKWARD", nx[2], sc_nx, h_nx, gap_next, sc_pv))
        if ig and sc_ig is not None:
            opts.append(("INTERNAL", ig, sc_ig, h_ig, 0, max(sc_pv or 0, sc_nx or 0)))
        if not opts:
            continue
        opts.sort(key=lambda o: -o[2])
        label, own, sc, hits, gap, other_sc = opts[0]
        d = tus[own]
        # est yield: BeatMatcher proof calibration = +27 measured / dtkBIG 66 -> ~0.41.
        # dtkBIG is the ceiling (target bodies dtk will emit once pinned); realistic
        # strict capture is ~0.3-0.5x of that (automap pair rate x byte-match rate).
        est_yield = round(0.41 * dtk_big) if dtk_big is not None else round(0.41 * nbody)
        rows.append(dict(
            run_start=s, run_end=e, run_bytes=nb, run_fns=nf,
            run_body_fns=nbody, run_body_bytes=body_bytes,
            dtk_tgt_fns=dtk_fns, dtk_tgt_big=dtk_big, est_yield=est_yield,
            n_strings=len(strings),
            owner_tu=own, attrib=label, score=round(sc, 3), str_hits=hits,
            gap_to_pin=gap, other_neighbor_score=(round(other_sc, 3) if other_sc else None),
            owner_pinned_bytes=d["pinned_bytes"], owner_nfrag=d["nfrag"],
            owner_span=f"{d['start']:08x}..{d['end']:08x}",
            prev_tu=os.path.basename(pv[2]) if pv else None,
            next_tu=os.path.basename(nx[2]) if nx else None,
            sample_strings=strings[:14],
        ))

    attributed = [r for r in rows if r["score"] >= args.floor]
    # rank: confidence band, then authoritative dtk-body yield (fall back to fp
    # body count when no build-dir), then mass
    def yield_key(r):
        return r["dtk_tgt_big"] if r["dtk_tgt_big"] is not None else r["run_body_fns"]
    attributed.sort(key=lambda r: (-(r["score"] >= 0.60), -yield_key(r), -r["run_bytes"]))
    json.dump(dict(region=args.region, floor=args.floor, rows=attributed,
                   weak=[r for r in rows if r["score"] < args.floor]),
              open(args.json, "w"), indent=1)

    print(f"# runs {len(runs)}  attributed(>={args.floor}) {len(attributed)}  "
          f"weak {len(rows)-len(attributed)}  dtk_targets={len(dtk)}", file=sys.stderr)
    print(f"{'OWNER_TU':36s} {'DIR':9s} {'RUN-SPAN':21s} {'dtkBIG':>6s} {'est':>3s} "
          f"{'fpBODY':>6s} {'SCR':>4s} {'OTH':>4s} {'GAP':>5s} {'NFR':>3s}")
    for r in attributed:
        gap = "" if r["gap_to_pin"] is None else f"{r['gap_to_pin']:#x}"
        oth = "" if r["other_neighbor_score"] is None else f"{r['other_neighbor_score']:.2f}"
        db = "-" if r["dtk_tgt_big"] is None else str(r["dtk_tgt_big"])
        print(f"{os.path.basename(r['owner_tu']):36s} {r['attrib']:9s} "
              f"{r['run_start']:08x}..{r['run_end']:08x} {db:>6s} {r['est_yield']:3d} "
              f"{r['run_body_fns']:6d} {r['score']:4.2f} {oth:>4s} {gap:>5s} {r['owner_nfrag']:3d}")


if __name__ == "__main__":
    main()
