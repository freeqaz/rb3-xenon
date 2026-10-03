# W16-OY — hoisted-address register numbering: the construct, and a sweep (2026-10-03)

**Branch** `w16-oy`, off main `63b16dfbb` (rebased onto it before measuring). **Ruler** `name_check` (graded, from
`report.json` `provenance.diff_config`). **The permuter was not run.** No map, splits, alias or `symbols.txt` edits.
Follows up W16-OW §3.3 (`W16OW_PLACEHOLDER_REGISTER_ROWS_2026-10-03.md`), which characterised this pattern and left it
unsolved.

## 1. The pattern

When a loop uses several global or RTTI addresses, the compiler hoists each `lis`/`addi` pair into the loop preheader.
The `addi` results go to callee-saved registers, and those match retail. The `lis` temporaries go to volatile registers
r9–r11, and that numbering is what differs:

| row | emission order of the `lis` | retail | ours (before) |
|---|---|---|---|
| `HeaderSortNode::FinishSort` | `SubheaderSortNode`, `SortNode`, `OwnedSongSortNode` RTTI | r11, r10, r9 | r10, r9, r11 |
| `ProfileMgr::CheckProfileWebLinkStatus` / `…SetlistStatus` | string literal, static `Symbol`, `TheNet` | r11, r10, r9 | r10, r9, r11 |

Emission order is the same on both sides. Only the register assigned to each `lis` differs.

## 2. What decides the numbering (scratch compiles, cl 10224, the build's own flags)

Method: standalone test files and copies of the real TUs compiled with the build's `cl.exe` and flags, with `/FAs`
listings. Scripts live in `~/tmp/w16oy/` (not committed).

**The numbering does not follow source order. It depends on a function-wide count of IR temporaries, and that count
moves it monotonically.** In a loop that uses three globals `G1`, `G2`, `G3`, putting *n* distinct global-address
uses (`use(H0); use(H1); …`) before the loop gives:

| n | `lis` G3 / G2 / G1 (emission order) |
|---|---|
| 0, 1 | r9 / r10 / r11 (ascending) |
| 2 | r10 / r9 / r11 (the "mixed" state: our two ProfileMgr rows and FinishSort) |
| 3 … 33 | r11 / r10 / r9 (descending: retail's state for all three rows) |

The effect saturates (n = 3, 4, 5, 9, 10, 11, 15, 16, 17, 24, 31, 32, 33 all read the same), so this is not a
hash or modulo artefact. A hoisted *pair* behaves the same way with a lower threshold: ascending for n ≤ 1,
descending for n ≥ 2. The callee-saved assignments never move. Only the volatile `lis` registers do.

What counts and what does not (each case measured):

| counts | does not count |
|---|---|
| a distinct global/literal/float-constant address materialised **before** the loop | the same global used repeatedly (one distinct address) |
| a compiler stack temporary (`$T`) anywhere in the function, **including after the loop** (by-value `Symbol`/struct argument, const-ref-bound temporary) | named scalar or 4-byte struct locals, even with their address taken |
| an inline accessor call in the loop body (its return value is an IR temporary) | dead references, `((void)x)`, constant-false branches, inline calls the optimizer removes entirely (fully scalarised temporaries) |
| a named local bound to a tested call result (some functions) | calls with unused results, large immediates (`lis` + `ori`) |
| | global addresses used only **after** the loop, TU-level declaration order, symbols referenced in earlier functions |

So the numbering depends on how many IR temporaries the function builds before register allocation. Several
spellings that change only that count produce identical machine code apart from these registers. That is the
construct. Three such spellings were found and used:

1. **Inline accessor instead of a direct member load (adds temporaries).** `FinishSort`:
   `song->mSongRecord->mData->IsDownload()` → `song->GetSongRecord()->Data()->IsDownload()` at both sites.
   Any one of the two sites is enough, and so is either half of the chain (`GetSongRecord()->mData` or
   `mSongRecord->Data()`). A named `bool isDownload` local at the same spot does **not** move it.
2. **Bind a tested call result to a named local (adds temporaries).** `ProfileMgr::Check*Status`:
   `if (netServer->GetPlayerID(padnum))` → `int playerID = netServer->GetPlayerID(padnum); if (playerID)`. Binding
   `HasValidSaveData()` or `IsAccomplished()` to a named `bool` works equally well (each gave retail's triple
   with no other change). A `(bool)` cast, `!!`, `!= 0` or `== false` on the same expressions has no effect, and `== true`
   changes code.
3. **Drop a named local (removes temporaries).** `AnimController` (`rndobj/Utl.cpp`): retail is on the *ascending*
   side of the pair threshold. `Hmx::Object *owner = RefPtrOf(it)->RefOwner(); … dynamic_cast<…>(owner)` →
   `dynamic_cast<…>(RefPtrOf(it)->RefOwner())`. This also fixes the row's second charge, the `mr r5/r6` and `li r4/r7`
   argument-setup order before `__RTDynamicCast`, which was the same allocation decision.

Each spelling was checked by diffing the full function listing against the base. The only differences are the hoisted
`lis`/`addi` registers (and, in `AnimController`, the argument-setup order that retail also has), plus compiler label
numbers.

How a lane applies this: identify which direction the row needs (retail descending in emission order = more
temporaries, ascending = fewer). To confirm that one more or one fewer temporary moves it, wrap a tested expression in a
throwaway inline identity template (`template <class T> inline T id(T x) { return x; }`; probe only, never landed).
Then pick a natural spelling of the same kind at that spot. In ProfileMgr the probe moved the numbering when wrapped
around any of the three tested call results, and not when wrapped around `IsOnline()`, `GetPadNum()`, `GetServer()`
or the `prog` reference.

### Ruled out (no effect on the numbering, or a code change)

- **FinishSort:** `FOREACH_POST`; `const_iterator`; explicit iterator declared before the loop; `SortNode *const cur`;
  `cur` declared outside the loop; `SongNodeType t = cur->GetType(); switch (t)`; an if/else chain instead of the
  switch (code change); `this->unk3c`; `song` declared at function scope; `!= nullptr`; inner list bound to a
  reference; inner loop as an explicit `it2++` loop; `dynamic_cast<…>(*it)` in the subheader case; the
  subheader cast through a separately declared local; braceless `if`/`else` arms; increment order (code change). Dropping `cur` and dereferencing `*it` gives retail's
  numbering but reloads `*it` (code change).
- **ProfileMgr:** static `Symbol` at function scope (code change); `= "…"` / `= Symbol("…")` copy-init; `(&TheNet)`;
  `Net &net = TheNet`; `TheNet.mServer` / `TheRockCentral.mState` direct (removing a layer: inert); index loop
  (code change); `const_iterator`; `this->GetSignedInProfiles()`; `const vector&` / direct-init of `profiles`;
  `prog` as a pointer; `Symbol(acc)` argument (code change); an early-return form of the `IsOnline` test;
  `it[0]`.
- **EventTrigger::Replace (not reproduced, §3).**

## 3. Sweep: every in-scope row with the pattern

`tools/charge_classify.py` on the rebased tree (`63b16dfbb`) gave 1,845 sourced sub-100 rows. A scanner
(`~/tmp/w16oy/hoist_scan.py`) diffed each in-scope row at the graded ruler. It flagged rows containing a run of ≥ 2
consecutive `lis` whose volatile registers are the same set on both sides in a different order. Excluded:
`src/network/**` and the Quazal block, per the standing directive.

- register-charged classes (`REG_ONLY`, `STACK_REG`, `NAME+REG`, `ARG_OTHER:*register`): 202 rows scanned → **5 hits**
- every class: 1,374 rows scanned → **no further hits**. This scan overlapped the first rebuild of the three edited
  rows, so they read as already fixed in it. No other object changed during the scan.

| row | B | before | after | lever |
|---|---:|---:|---:|---|
| `HeaderSortNode::FinishSort` (`meta_band/SongSortNode.cpp`) | 380 | 99.684 | **100** | accessors (§2.1) |
| `ProfileMgr::CheckProfileWebLinkStatus` | 292 | 99.589 | **100** | `int playerID` (§2.2) |
| `ProfileMgr::CheckProfileWebSetlistStatus` | 292 | 99.589 | **100** | `int playerID` (§2.2) |
| `AnimController` (`rndobj/Utl.cpp`) | 156 | 98.718 | **100** | drop `owner` (§2.3) |
| `EventTrigger::Replace` | 636 | 99.874 | 99.874 | **not reproduced** |

**`EventTrigger::Replace`:** the pair (`ObjectDir`, `EventTrigger` RTTI) in the third loop's preheader is r11/r10 in
ours and r10/r11 in retail, i.e. retail is on the low-temporary side. It sits behind two earlier loops that already
materialise three RTTI addresses. Adding 1–3 constants before or between the loops does not move it (saturated), and
none of these removals moved it: `FromIs` replaced by the direct comparison at each of its five sites and at all of
them; `(*it).`; a hoisted `fromObj`; `while` form; a `ProxyCall &call` reference; named locals for the two casts; an
identity wrapper at 8 sites. It stays with the permuter class.

All four fixed rows were already at `mpn` 100, so the gain is bytes only. objdiff's verdict on the three triple rows
was `AtLimit (High confidence) … ADDRESS_RELOCATION_NOISE … no source mutation can close them`, and on the two pair rows
`REGISTER_SWAP … run the permuter`. Both verdicts were wrong for these four rows.

## 4. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-oy-ab --patch <git diff main w16-oy -- . ':!docs' ':!tools'>`
- **Worktree:** fresh, `scripts/setup_worktree.sh` at main `63b16dfbb`.
- **Patch:** 3 source files. objdiff-cli sha `c1b7d952`, stable across legs.
- **Run dir:** `~/tmp/wt-w16-oy-ab/.ab_measure_runs/20261003-070249-branch-3801426/`.

**Prediction, written before the run:** +0 functions (all four rows already at `mpn` 100), +1,120 B (380 + 292 + 292
+ 156).

```
leg A: matched=53309 masked=25150 honest=28159 code%=57.445347  (recompiles: 0, settled)
leg B: matched=53309 masked=25150 honest=28159 code%=57.456280  (recompiles: 7, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.010933pp  Δcode_bytes=+1120
units at 100% [mpn ruler]: 531 -> 531   [all-rows-fuzzy ruler]: 469 -> 469
```

**Measured: +0 / +1,120 B, as predicted.** Row-level diff of the archived legs: 4 rows up (the four above, each to
fuzzy 100), **0 rows down**, 68,913 rows on both legs, none appeared or vanished.

## 5. Gates

Native gate, run last on the final code (only this docs-only commit follows):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 6. Not done

- **The permuter was not run** (standing directive).
- `EventTrigger::Replace` (§3) was not reproduced.
- The register-numbering rows that are not hoisted-`lis` runs (W16-OW §3.2's other rows) were not re-opened with this
  lever. The `$T`/accessor/named-local temporaries count is a plausible lever for some of them, but none were measured.
- The scanner and scratch harness in `~/tmp/w16oy/` were not committed.
