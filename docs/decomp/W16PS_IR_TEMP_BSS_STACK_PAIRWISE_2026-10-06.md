# W16-PS: levers 4–6 of CAMPAIGN_STATE_2026-10-06 (IR temporaries, `.bss`/stack order, pairwise sweeps)

Lane W16-PS, branch `w16-ps`, rebased onto main `7f41fe33e`. Eight source commits, no permuter, nothing under
`src/network` or `src/xdk`.

The brief was lever 4 of `CAMPAIGN_STATE_2026-10-06.md`, in its listed order:
1. W16-PF's IR-temporary-count technique on the register rows it never reached.
2. W16-PG's explicit `= 0` declaration-order mechanism on stack-slot rows.
3. Pairwise (two-site) variants on W16-PF's stopped rows.

## Result

```
================ A/B RESULT (MEASURED) ================
  patch: ab1 (ad121f8fe261f978)  kinds: ['source']
  leg A: matched=53507 masked=25193 honest=28314 code%=57.896297  (recompiles: 0, settled)
  leg B: matched=53511 masked=25193 honest=28318 code%=57.929047  (recompiles: 18, split=0, patch_steps=6, settle iterations: 2)
  Δmatched=+4  Δmasked_equal=+0  Δhonest=+4  Δcode%=+0.032750pp  Δcode_bytes=+3356
  units at 100% [mpn ruler]: legA 551 -> legB 552  (+100% default/PlayerDiffIcon, MATCHED_ROSE)
```

- The run was `tools/ab_measure.py --worktree ~/tmp/wt-w16-ps-ab --patch <git diff dc238f637 w16-ps -- src>`.
  Leg A was main `7f41fe33e`.
- After the rebase, `git diff main w16-ps -- src` has the same content as that patch (index lines excepted). So the
  measurement is of this branch on current main, and I did not run it a second time.
- Prediction before the run: **+4 fns / +3,356 B**, with the PostProc row fuzzy-only. The measurement agreed
  exactly.

### Row-level diff of the archived legs

From `legA_report.json.gz` vs `legB_report.json.gz`, every row, fuzzy and mpn:

- **DOWN: 0.**
- **UP: 9.**

| row | size | fuzzy before → after | mpn before |
|---|---:|---|---|
| `PropSync(BandCharDesc::Patch&,…)` | 656 | 99.329 → **100** | 100 |
| `PropSync(BandCharDesc::OutfitPiece&,…)` | 480 | 99.333 → **100** | 100 |
| `BandWardrobe::AddDircut` | 504 | 99.921 → **100** | 100 |
| `MemFindHeap` | 184 | 99.565 → **100** | 100 |
| `MemFindAddrHeap` | 96 | 99.958 → **100** | 99.958 |
| `MidiReader::ReadMetaEvent` | 972 | 99.930 → **100** | 99.930 |
| `PlayerDiffIcon::Save` | 136 | 99.941 → **100** | 99.941 |
| `RndText::Save` | 328 | 99.878 → **100** | 99.878 |
| `RndPostProc::Save` | 1,056 | 99.879 → 99.970 (32 → 8 charges) | 99.879 |

- The eight rows that reached 100 add up to 480+656+504+96+184+972+136+328 = **3,356 B**, equal to the measured
  Δcode_bytes.
- Four of them were already at mpn 100, and their bytes moved without the function count moving. That explains
  why Δmatched is +4 while eight rows crossed.

## Method

- **Instrument (`~/tmp/w16ps/probe.py`, scratch, not landed).**
  - Applies a text variant to a temp copy of the TU and compiles it with the TU's exact `build.ninja` command to a
    scratch `/Fo`.
  - Diffs that object against the target with `objdiff.json`'s graded `options` plus `--map-file`.
  - The real source is never touched, so the probe sits outside the patcher/`ninja <one.obj>` hazard. The landed
    result above is the whole-binary A/B.
  - Control: an empty variant reproduces each row's `report.json` fuzzy (for example AddDircut 99.92063 / 126 /
    1 charge).
- **IR-temporary sweeps.** W16-PF's `_pfid(x)` identity wrapper, run over:
  - every call/member-read site (`auto`, 31 probes);
  - every member read (`mem`, 87);
  - drop-a-temp forms (`drop`, 2);
  - post-increment forms (`post`, 5);
  - every variable read (`var`, 300).

  Any site that moved the charge count was followed by a natural spelling. `_pfid` itself is never landed.
- **Anchor census (`anchor_census.py`).** Covers the 95 in-scope register/stack rows. Each differing immediate is
  classed by the definition of its base register:
  - ANCHOR: a `lis`/`addi` off a `.bss`/`.data` symbol;
  - STACK: `r1`/`r31` frame;
  - OTHER.

  Result: **86 MIXED, 7 STACK-only, 2 ANCHOR-only** (`MemFindAddrHeap`, `InitSystem`).
- **Pruning rule for the pair sweep.** Pair only "sensitive" sites: sites whose single `_pfid` probe changed the
  mismatch count while leaving the instruction count unchanged, taken from W16-PF's own outputs.
  - An insensitive site cannot change register numbering, so it cannot combine into a change either.
  - This turns W16-PF's ~1,200 sites into a **157-pair** sweep over the 11 stopped rows that have ≥2 sensitive
    sites.

## What fixed rows, by mechanism

Most of the fixes are **not** the IR-temp lever. The `_pfid` sweeps found the sites, and the fix was a different
source fact each time.

1. **Tested call vs named `bool` local (BandCharDesc ×2, 1,136 B).**
   - The Patch/OutfitPiece PropSync arms assigned `bool synced = PropSync(...)` and tested it on a shared tail.
   - Retail tests the call directly, then calls `SetChanged` inside the arm.
   - The named bool held one more value live across the call, which reranked the callee-saved registers.
   - New macro `SYNC_PROP_MODIFY_TESTED`, local to `BandCharDesc.cpp`.
2. **Accessor vs direct member (AddDircut, 504 B).**
   - `bchar->InstrumentType()` instead of `bchar->mInstrumentType`.
   - The inline accessor adds one IR temporary, which moved the register numbering into retail's order. This is the
     W16-PF lever proper.
3. **`strcmp` argument order (MemFindHeap, 184 B).**
   - `strcmp("physical", name)`. It reads as a register swap on the inlined `strcmp`, but the swap is the operand
     order.
   - Only the live `#else` arm was changed; the `HX_NATIVE` arm is untouched.
4. **`.bss` layout distance (MemFindAddrHeap, 96 B).**
   - Retail reads `gNumHeaps` at `gHeaps+0x254`. Ours sat at `+0x240`.
   - The fix is five stand-in words, `int gMemHeapLayout440[5];`, declared between them.
   - Control: four words did not fix it.
   - The neighbours `MemNumHeaps`/`MemHeapSize` stay at 100 (A/B: 0 rows down).
   - The stand-in is a layout witness, not a recovered name. Retail has 20 bytes of something there that we have
     not identified.
5. **Lexical scope decides stack slots (ReadMetaEvent, 972 B; 17 → 0 charges).**
   - `int ts_m, ts_b, ts_t;` moved from an inner `else` to the time-signature `case` block, after `ts_num, ts_den`.
   - The in-tree comment that marked this row "REFUTED, do not retry" is replaced. The refutation had varied
     declaration order inside the same scope, which is inert, but not the scope itself.
6. **Full-expression lifetime: chained stream writes (PlayerDiffIcon, Text, PostProc).**
   - `bs << a << b` keeps the first temp alive into the second write, so the second gets its own stack slot. Retail
     shows this.
   - A chain sweep (`chain_sweep.py`) over **24** Save/Load rows tried **571** two-to-N chainings. **Only PostProc
     and Text** improved.
   - PostProc: all **1,024** head groupings of its writes were enumerated (`pp_enum.py`). The best leaves 8 charges,
     all inside the inlined `mColorXfm.Save`, which no grouping of the outer writes reaches.

## Lever 4 (IR temporaries): 14 rows / 6,376 B

**4 fixed (1,824 B):** both PropSyncs, AddDircut and MemFindHeap.

**10 stop.** None improved on the `auto`/`mem`/`drop`/`post`/`var` sweeps (425 probes, 0 BETTER). Reasons below.

| row | size | fuzzy | why it stops |
|---|---:|---|---|
| `DoFancyElbow` | 1,040 | 99.846 | `elbowQ` scaling forms are inert. The residue is a register pair at the scale; no site moves it. |
| `ReadSingleXinputJoypad` | 812 | 99.778 | guitar/`rx` register swap. Declaration, local and id variants are all inert. |
| `AddChordLevel` | 488 | 99.221 | `r8`/`r31` swap. Declaration, local and id variants are inert. |
| `NeutralLocalXfm` | 440 | 99.818 | `z`/`y` locals, `Scale` call form, by-ref and id variants are inert or worse. |
| `_S_sort<BSPFace>` | 424 | 97.972 | STLport `list::sort` in a shared header. A variant there moves every instantiation. |
| `operator>(Sphere,Frustum)` | 372 | 99.892 | Last-plane spellings, if-chains and parameter forms are all inert. |
| `MemTruncate` | 284 | 97.113 | A scheduling residue. Declaration orders are inert; `/4` is a code change (`addze`). |
| `InterpTangent` (in `Key.cpp`) | 280 | 99.571 | 34 variable sites are inert. Vector wraps are code changes; `Add` operand order is inert. |
| `__partial_sort<GameGem*>` | 220 | 99.636 | An explicit specialization reproduces the base exactly, but every temp variant is inert. The compare is the shared `GameGem::operator<`. |
| `Multiply(Vector3,Quat)` | 192 | 85.729 | Floating-point expression-tree and store-order scheduling, not register numbering. |

## Lever 5 (`= 0` declaration order on `.bss` and stack rows)

**ANCHOR-only rows (2):**
- `MemFindAddrHeap` is fixed (layout distance, above). It was not an order fix.
- `InitSystem` (228 B) stops after 12 variants: 6 declaration orders of the System statics, each with and without
  `= 0`. All are inert.
  - Retail addresses those statics off the *higher* static with a negative displacement.
  - No order we can write moves the anchor choice.
  - `PreInitSystem` is MIXED, with the same anchor and the same verdict.

**Explicit `= 0` on locals** adds stores, so it is a code change and not a layout lever.

**STACK-only rows (7):**

| row | size | outcome |
|---|---:|---|
| `MidiReader::ReadMetaEvent` | 972 | **fixed** (scope) |
| `PlayerDiffIcon::Save` | 136 | **fixed** (chained write) |
| `FocusTracker::Poll_` | 760 | stops. Scope moves give 7 charges (worse) or leave 5. The second `next` is constructed in place, so no scope reaches it. |
| `DirLoader::Cleanup` | 548 | stops. Brace and scope variants are inert, and the pair sweep is 0/10 (below). |
| `ClosetPanel::CycleCamera` | 480 | stops after 8 variants (details below). |
| `Sphere::GrowToContain` | 372 | **not opened.** 16 of its 18 charges are REG, so it is not a stack row in substance. |
| `fn_822DB278` | 32 | **not opened.** An EH funclet; it follows its parent. |

`CycleCamera` detail:
- Retail puts the `substr` temporary in `str30`'s slot (`r31+120`). Ours shares it with the loop's `substrs[i] + "_"`
  temporary (`+136`).
- **Inert:** a block around the loop body, a block around `str30`'s lifetime, a braceless loop, the loop index
  hoisted, `String(...)` around the substr, and a named length.
- **Code changes:** a named `piece` local; `str30` hoisted before the split.
- W16-PG's scoped substr was already recorded as worse.

## Lever 6 (pairwise sweeps on W16-PF's stopped rows)

- `pair_sweep.py` covered the 11 stopped rows with ≥2 sensitive sites: **157 pairs, 0 BETTER.**

| row | sites | sensitive | pairs | better |
|---|---:|---:|---:|---:|
| VocalTrack::RebuildHUD | 14 | 14 | 91 | 0 |
| VocalPart::SetDifficultyVariables | 9 | 9 | 36 | 0 |
| DirLoader::Cleanup | 6 | 5 | 10 | 0 |
| VocalNoteList::NotesDone | 4 | 4 | 6 | 0 |
| Intersect | 5 | 4 | 6 | 0 |
| Normalize | 3 | 3 | 3 | 0 |
| PerfectOverdriveTracker::Poll_ | 2 | 2 | 1 | 0 |
| VocalPart::HandlePhraseEnd | 4 | 2 | 1 | 0 |
| TrainerGemTab::Render | 2 | 2 | 1 | 0 |
| SongData::ValidateVocalSPPhrases | 2 | 2 | 1 | 0 |
| XboxEntityUploader::ApplyStringVerifyResults | 2 | 2 | 1 | 0 |

- **Reading:** on this population the IR-temp lever does not compose. Where no single site moves a row to a better
  count, no pair of sensitive sites does either.
- Bound on the claim: a pair of *insensitive* sites was excluded by the pruning rule and is untested. So were
  triples.

## What was not done

- **No permuter**, per the standing directive.
- **No `src/network` or `src/xdk`.**
- **`RndText::Save` and `RndPostProc::Save` sit in the VIA-DC3 ring.** Native takes `rndobj/` from DC3, so these two
  fixes buy metric only, not native behaviour. They are flagged here because the campaign state leaves that ring to
  the user.
  - Each change is a write grouping with identical behaviour; the stream receives the same bytes.
- **`_pfid` and the scratch harness are not landed.** They live in `~/tmp/w16ps/`.
- Rows in the census's MIXED class (86) were not opened beyond the lever-4/5/6 populations above.
