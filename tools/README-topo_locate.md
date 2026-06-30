# topo_locate.py — Callee-Set Topological Locator

A scattered-TU game-method identifier that places a Wii method into the RB3-360
retail binary by **confirmed-anchor call-graph topology**: a method calls a set
of named callees; the subset that are matched@100 anchors (with a known retail
VA) pins it into the retail call graph. The retail function that calls that same
set of anchored callees IS the method. Pure call-bag intersection — no
similarity diffusion, no Ghidra/BSim/objdiff in the loop.

Design + verdict + kill criteria:
`docs/decomp/research/2026-06-30-topo-locator-design.md`

All inputs are committed, read-only; runs in seconds. Inputs not present in a
CoW worktree (e.g. the untracked `fingerprints.json`) fall back to the main
repo root automatically.

## Usage

```bash
python3 tools/topo_locate.py --validate                 # held-out precision@1
python3 tools/topo_locate.py --classb-floor             # SongSortNode/BandProfile floor
python3 tools/topo_locate.py --harvest --emit-gate g.json --pin-only p.json
python3 tools/topo_locate.py AppLabel.cpp NetSession.cpp # locate specific TUs
```

`--emit-gate` writes the `{VA:{class:"TOPO_LOCATED",confidence}}` sidecar in
locator.py's format; `--pin-only` writes the `[VA,...]` subset whose topo-VA ==
the oracle's rb3_addr (safe to carve via identity_transfer's existing path).

## VERDICT (2026-06-30): PRIMARY KILL — approach is dead

Measured on the committed assets, NOT estimated:

| metric | value |
| --- | --- |
| held-out N>=2 game-anchor pool | 23 |
| **self-relocation precision@1** | **0.13 (3/23)** |
| trueVA-in-candset rate | 0.13 |
| methods with NO candidate (recall wall) | 18/23 |
| class-B floor (SongSortNode 99 fns / BandProfile 109 fns) | 0 / 0 N>=2 (floor OK) |
| harvest candidates | 5, **0 agree with oracle, 0 agree with crossval** |

precision@1 = 0.13 is far below the design's 0.55 PRIMARY-KILL threshold, so the
approach is dead — do NOT graft P2/P3/P4 signals to rescue it (the recall ceiling
caps the prize regardless of precision).

**Why it dies — the recall wall is structural, not a tuning bug.** A
forward-direction ground-truth check (does the method's true retail VA's own
fingerprint actually call >=2 of the oracle-anchored callee VAs?) gives the same
4/23 ceiling. The Wii->retail callee VAs drift: a Wii method calls two anchored
functions, but in retail those exact callee VAs are ICF-folded so the unified
oracle picked a *different* VA than the one the retail caller targets (e.g.
`AppLabel::Handle` calls SetSongName@82B5F808 but the oracle's
SetOfferCost@8250C690 is only reached by 8250CC78, not the true Handle VA), or
the callee is inlined/devirtualized. This is the doc's flagged constraint —
only 3,089/10,664 anchors appear as a callee at all — biting at the per-method
level. The one bright spot: at `vote_margin >= 2` the calibration table shows
precision 1.0 (3/3), but only 3 of 23 methods ever reach that confidence.

The class-B floor is a CONFIRMING negative (expected): SongSortNode and
BandProfile are string-poor and their callees are themselves unmatched scattered
methods, so topology cannot bootstrap — exactly the consolidated verdict's
class-B-unrecoverable finding, now reproduced with hard numbers.

The deliverable is the hard-numbers negative: confirmed-anchor call-graph
topology re-finds only 3 of 23 held-out game anchors at precision 0.13; class-B
yields ~0; the scattered-TU identification wall stands.
