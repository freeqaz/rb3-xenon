# Gap composition atlas — frozen snapshot 2026-07-08

> Status: FROZEN SNAPSHOT | Date: 2026-07-08 | Tool: `tools/gap_atlas.py`
> RFC: `02-gap-composition-atlas.md`

This is the frozen baseline snapshot referenced by
`docs/plans/paths-to-100/02-gap-composition-atlas.md` and
`18-metrics-and-dashboard.md`. All future ROI is accounted as deltas against
these numbers. Regenerate at any time with `tools/gap_atlas.py`; re-snapshot
after each landing wave.

## Provenance

- **Source:** `build/45410914/report.json` (objdiff canonical output).
- **report.json generated at:** main `a1312de` (per RFC 02; the `report.json`
  is the frozen input file, mtime 2026-07-06).
- **Tool commit:** `a0833e6` (worktree `p100-atlas`, branch off main `a0833e6`).
- **Regeneration:** `tools/gap_atlas.py` (text) / `--markdown` / `--json` /
  `--check` (verify against the frozen RFC baseline; exits non-zero on mismatch).
- **Reproduction verified:** `tools/gap_atlas.py --check` → **RESULT: REPRODUCED**
  (all 19 RFC baseline figures match exactly; consistency invariant reconciles).

## Whole-binary top line

```
STRICT functions  11,240 / 65,619  (17.13%)
STRICT code       962,656 / 11,074,108 B  (8.69%)
  => UNMATCHED code = 10,111,452 B  (91.31%)
total_data 4,118,360 B (matched 16 B)
total_units 2,456
```

## Pinned / auto split

| Class  | units | total_code (B) | matched (B) | matched% | total fns | matched fns |
|--------|------:|---------------:|------------:|---------:|----------:|------------:|
| PINNED |  773 |      3,132,020 |     962,656 | **30.74%** |    23,494 |      11,240 |
| AUTO   | 1683 |      7,942,088 |           0 | 0.00% |    42,125 |           0 |

## The five buckets

Every unmatched byte falls into exactly one bucket. Buckets 1–2 are inside
pinned units; buckets 3–5 are the auto/data remainder.

| # | Bucket | Bytes | % of unmatched | Matchability class |
|---|--------|------:|---------------:|--------------------|
| 1 | Pinned real-body near/mid-miss (>44 B) | 1,925,408 | 19.0% | oracle-exists / portable-source |
| 2 | Pinned boilerplate (funclets + ≤44 B stubs) | 243,956 | 2.4% | boilerplate |
| 3 | Unpinned auto_ .text (game+engine+mw+CRT) | 7,942,088 | 78.5% | mixed — see 3a/3b |
| 4 | Bink / middleware .text | 65,292 | 0.65% | opaque (no source) |
| 5 | Data sections (.rdata/.data/.idata) | 4,118,360 | n/a (own denom) | anchoring signal |

Bucket 2 breakdown:

```
funclet/unwind ($ in name):    729 fns     111,476 B
small ≤44 B (non-funclet):   4,462 fns     132,480 B
                             5,191 fns     243,956 B
```

Bucket 3 breakdown (auto .text, 923 units carry .text):

```
named symbols (reachable, uncredited):   7,086 fns  2,627,100 B
anonymous fn_<addr> (never identified): 35,039 fns  5,314,988 B
```

Bucket 1 fns: 7,116.

Bucket 4 note: the Bink `.text` figure is the BINK unit's `total_code`
(65,292 B = sum of its 137 function sizes), **not** the raw PE `BINK`/`.text`
section byte size (65,552 / 65,332). Middleware data sections (BINKCONS 10,528,
BINKBSS 17,312, BINKDATA 15,700, .XBMOVIE 12) are data, not code.

Bucket 5 data sections: `.rdata` 2,028,144 + `.data` 2,045,420 + `.idata` 1,228.

## Consistency invariant

```
bucket1 + bucket2           = 2,169,364  (== pinned unmatched)
bucket1 + bucket2 + bucket3 = 10,111,452
whole-binary unmatched code = 10,111,452
reconciled: YES
```

`buckets_reproduced = true` — `tools/gap_atlas.py --check` confirms every RFC 02
"Current state" figure to the byte.
