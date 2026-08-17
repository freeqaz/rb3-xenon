# The sympair queue — rows realizable by a relocation-NAME correction

**Lane W7-SYMPAIR, 2026-08-17.** Tool: `tools/sympair_queue.py`. Data:
`docs/decomp/sympair-queue.tsv` (per-row) + `docs/decomp/sympair-pairs.tsv`
(repeated-pair census). Promoted from lane W2-ENGINE's throwaway
`~/tmp/w2engine/{charged,sweep}.py` (`b5a242c4`), which found two map defects
worth **+12,780 B** and would otherwise have died in scratch.

## What it measures

For every row, the **charged-site list** on the grader's ruler, split into
relocation-symbol-pair charges (`diff_arg` → `SYM target -> ours`) vs everything
else. Rows whose **only** charges are symbol pairs are *realizable by naming
work alone* — `matched_code` is all-or-nothing per row, so the ranking that
matters is **size-if-it-crosses**.

⚠ Never price one of these rows from a mismatch count. "N/N instructions equal"
is instruction-level and excludes `diff_arg`; a row can read "all equal" and
score 98.4% graded.

## Self-validation

`--selftest` replays a **frozen fixture** of W2's original engine top-45 sweep
(`tools/testdata/sympair_w2_control.json`) and asserts **23 rows / 41,088 B**,
W2's reported figure. It reproduces **exactly**. The fixture is frozen because
W2's worktree was removed — it is not regenerable, and it is the only surviving
record of that population. The gate is **proven able to fail**
(`--selftest --mutate` → 22 rows / 36,164 B, exit 1).

## Headline: the queue is large and mostly UNREACHABLE

Whole-binary, game **and** engine, all sizes, at `ed4797e7`:

| | rows | bytes |
|---|---:|---:|
| population scanned (`mpn==100`, `0<fuzzy<100`, named, non-auto) | 2,540 | 643,336 |
| **crossable — only charges are symbol pairs** | **2,340** | **536,528** |

That 536,528 B headline is **not** a work queue. Triaged by what would have to
be true for the row to cross:

| class | rows | bytes | share |
|---|---:|---:|---:|
| `FOLD_FANIN` — ≥2 of our distinct functions hit one target address | 1,099 | 285,548 | **53.22%** |
| `ALL_OURS_UNMAPPED` — our callee has no retail address at all | 724 | 158,300 | 29.50% |
| `MIXED/UNKNOWN` | 415 | 76,060 | 14.18% |
| `ALL_RECIPROCAL` — transposed map bijection | 102 | 16,620 | **3.10%** |

**`FOLD_FANIN` is the ICF-survivor signature and is irreducible**: retail folded
N identical bodies, the map can spell only one of them, and our N distinct
spellings are charged at every site. The big `Handle` rows are exactly this —
`BandUI::Handle` (3,564 B) calls **one** address, `OnMsg(UIComponentSelectMsg)`,
where our source calls three different `OnMsg` overloads. `OvershellPanel`,
`PlatformMgr` and `Rnd` all show the same shape.

`ALL_OURS_UNMAPPED` is the identification backlog, not noise — but naming an
anonymous address has **zero byte upside** by itself (`name_check` already
forgives placeholder targets); its payout is bug exposure.

⇒ **Only ~3% of the queue carries the signature of a fixable naming defect.**

## The one signature that separates fold from defect

A fold maps N names onto **one** address. It therefore **cannot** produce a
*transposition* across **two distinct** addresses. So a **reciprocal pair** —
`target T → ours O` in one row and `target O → ours T` in another, with T and O
at different retail addresses — is not a fold. It is a wrong or arbitrary map
bijection. There are **54** such pairs.

This connects directly to machinery the map already carries:
`target_symbol_map.json`'s **`_bijection_arbitrary`** list (1,025 addresses)
records names "assigned by a bijection over a reloc-masked BYTE-IDENTICAL
equivalence class… WHICH name belongs on WHICH VA is NOT established", and its
own comment notes that since the `name_check` flip a wrong pick is a **real byte
charge** and refining one is positive-yield repair (lane MAPDEF-3, +108 B / 9 rows).

⛔ **But only 11 of the 54 reciprocal pairs are on a flagged address** — and only
8.03% of the queue's bytes touch the flagged population at all. **43 of the 54
transpositions are in the UNFLAGGED map.** The existing flag under-counts the
arbitrary population; reciprocity finds cases the flag missed.

## ⛔ Why this lane shipped NO map swap — the anchors are circular

The obvious next step is to swap the transposed names. It is not that simple,
and the trap is worth recording because it looks solved from three angles and
is not.

Worked case, `default/FileCache` (a complete STL sort machinery instantiated for
two types, `FileCacheEntry`/`Priority` and `MoveDetector`/`MoveDetectorCmp`,
sitting in two address clusters `0x8234xxxx` and `0x82519xxx`):

* Retail `sort<FileCacheEntry,Priority>` @`0x82349c58` calls
  `__introsort_loop<MoveDetector,…>` @`0x823498f0`.
* Retail `sort<MoveDetector,MoveDetectorCmp>` @`0x82519f38` calls
  `__introsort_loop<FileCacheEntry,…>` @`0x82519e38`.

`sort<T,Cmp>` calls `__introsort_loop<T,Cmp>` by template construction — our
source has **no freedom** at that edge. So **the map is provably
self-contradictory here.** That much is settled.

*Which* permutation is correct is **not**. Three anchors were tried and each
failed:

1. **"Our body scores 100% under the current name."** The two retail
   `__introsort_loop` bodies genuinely differ — `lwz r10,0x28(r10)` vs
   `lwz r10,0x0(r10)`, and `cmpw` vs `cmplw` (signed vs **unsigned**) — so the
   100% pairing is real byte evidence, not relocation-masked. This says the
   *callees* are named right. But applied to the neighbouring
   `__insertion_sort` / `__final_insertion_sort` pairs it disagrees with (2).
2. **Call-edge token consistency** (do this address's callees carry this name's
   type tokens?) returns `CONSISTENT_AS_IS` for `__introsort_loop` and
   `SWAP_PROVEN` for `__insertion_sort` and `__final_insertion_sort` — i.e. it
   contradicts itself within one cluster. ⚠ **It is contaminated by construction:
   the callee names it trusts are drawn from the same suspect map.** A local
   consistency check over a mutually-inconsistent name set is a fixed-point
   problem, not a proof.
3. **TU spatial grouping.** `splits.txt` pins **both** clusters to
   `FileCache.cpp` — the narrow `0x82347770–0x82347930` and the broad
   `0x825184c8–0x8251a070` — so unit attribution cannot discriminate either.
   This is a `_splits_fill_unresolved` case: byte-true, ownership unestablished.

A swap here would move the metric (+224 B for the `sort` pair alone) and would
be a **guess**. Per the standing rule that an unproven rename is the same
integrity hazard as a fabricated alias, none was made.

★ **The transferable lesson:** *the reciprocity test proves a map defect EXISTS
without proving which side is wrong.* Existence and assignment are separate
claims, and this queue currently supports only the first.

## ✅ One case WAS settled — with an anchor outside the map (`80fca393`)

`Rnd::Handle`'s two screen-dump charges. The map had `?OnScreenDump@Rnd@@` and
`?OnScreenDumpUnique@Rnd@@` **transposed** (`0x824130f0` ↔ `0x82413098`), and
`src/system/rndobj/Rnd.h` carried a deliberate comment *reasoning from that
wrong name* — "retail OnScreenDump calls vtable+0x70 (slot 28), so ScreenDump
must stay at slot 28 and ScreenDumpUnique is declared FIRST" — which had been
elaborated into a load-bearing **vtable ordering decision**.

The anchor that settled it is external to the map: the `.rdata` dispatch
strings in retail `Rnd::Handle` (`0x82413350`). `'screen_dump'` at +4544 is
followed by `bl 0x82413098`; `'screen_dump_unique'` at +4668 by
`bl 0x824130f0`. The two retail bodies are otherwise identical and differ
**only** in the vtable slot they call (`+0x6C` vs `+0x70`). Our own
`OnScreenDump` calls `ScreenDump(da->Str(2))` ⇒ retail's `ScreenDump` is the
`+0x6C` slot ⇒ **ScreenDump is the LOWER slot and is declared FIRST.**

Measured **Δ exactly 0** on every key — landed anyway (accuracy > headline).
Mechanism verified so that Δ0 could not mean "nothing happened":
`Rnd::Handle` went **3 charged sites → 1**, fuzzy 99.99065 → 99.99688, and the
survivor is precisely the `FOLD_FANIN` pair predicted to remain — which is why
the 6,416 B row still does not cross.

★ Note what this cost and bought: the *byte* yield was zero and the *accuracy*
yield was a corrected vtable and a retracted header claim. Budget for that
ratio on this vein.

## What would settle it

An anchor **outside** the map: retail `.rdata` string content at the call site
(W2's `copy_cats` method), RTTI `??_R4`/type-name presence, or a caller-set
semantic argument (`tools/retail_callers.py`). Those worked for W2 on two rows
worth 12,780 B. They are per-case and do not scale to 54 pairs cheaply.

⚠ Note the size ceiling before funding that work: **the entire `ALL_RECIPROCAL`
class is 16,620 B**, and a transposition must be fixed on *both* sides to pay.
