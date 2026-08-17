# `tools/bindiff_match.json` — pointer record (the artifact itself is gitignored)

> **Why this file exists:** `bindiff_match.json` is ~3.7 MB, **gitignored, and
> has zero git history** — so on disk it is indistinguishable from scratch
> nobody ever acted on. **Two separate lanes (DL-1 in August, W3-IDENT on
> 2026-08-17) were briefed that the Ghidra+BinDiff identification route had
> "never been executed" and burned budget rediscovering that it had.** A
> gitignored artifact is an invisible institutional memory. This pointer is
> the fix; do not delete it, and do not commit the artifact itself.

## The artifact

| | |
|---|---|
| path | `tools/bindiff_match.json` (gitignored, regenerable) |
| what it is | BinDiff structural matches, **DC3 → RB3 retail**, used to transfer DC3's named functions (from the leaked `ham_xbox_r.map`) onto RB3's anonymous `fn_8XXXXXXX` |
| ⛔ **status** | **TU0-era and ADDRESS-DEAD — only 3.13% of its addresses are valid on TU5.** Main has targeted TU5 since 2026-07-15. Do **not** consume it without re-deriving addresses. |

## It was executed — three times

| round | commit | result |
|---|---|---|
| r1 | `e5293a8c` | +286 map entries, precision 89.8% → 96.4% |
| r2 (anchored) | `aa86fb41` | +263 entries, **98.6%** top band |
| repoints | `5ff856bc` | 35 of 36 conflicts resolved |

≈ **549 map names landed in total.**

## Why it stopped — a calibrated kill, not fatigue (lane DL-1, `254e80bd`)

- Scoping to the correct DC3 `.obj` moves top-1 accuracy **46.8% → 80.7%**,
  with a **sabotage leg at 0.0%** — i.e. the control *could* fail and didn't.
- ⛔ **But the decoy null has p95 = 1.000, so NO THRESHOLD EXISTS** that
  separates a true match from a decoy.
- ⛔⛔ **The structural verdict:** *the bottleneck is LOCATION, not scoring
  power — and obtaining a location prior **is what pinning a TU means**.*
  The method is therefore **circular for exactly the unpinned population it
  keeps being proposed for.**
- Coverage bound: DC3-BinDiff names only **shared engine** code — measured
  **9% yield on game code**, which is the priority layer.

## Toolchain reality (resolved 2026-08-17, both prior docs were wrong)

The Ghidra service does **not** use `/opt/ghidra`. It uses the **VMX128 fork
at Ghidra 12.2**, which **already has `XEXLoaderWV` installed** — so
CLAUDE.md's "needs a Ghidra 12.1 rebuild" is stale. The one genuine gap is
that **BinExport is not installed** into that fork; per the kill above,
closing it is not currently worth funding.

## What to fund instead

The **body-identity channel** (`tools/ident_body_channel.py`) — measured,
small, and real: **234 rows / 21,224 B = 0.206% of `total_code`** reachable
in total. Full adjudication:
`docs/decomp/W3_IDENTIFICATION_ADJUDICATED_2026-08-17.md`.
