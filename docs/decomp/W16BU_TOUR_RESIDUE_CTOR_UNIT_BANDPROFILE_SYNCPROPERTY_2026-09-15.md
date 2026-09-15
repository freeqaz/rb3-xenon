# W16-BU — Tour-cluster residue: the `??0Tour@@` unit question, four source rows, the `0x8258bab8` BandProfile decode, and the `0x8235c2e0` SyncProperty naming A/B

Lane W16-BU, 2026-09-15. Branch `w16-bu`, based on main `fe020891ba7e`.
Worktree `/home/free/tmp/wt-w16-bu`. Ruler `name_check` (graded, read from
`report.json`'s `provenance.diff_config`), objdiff 4.2.9.

**Headline, measured in this worktree, both ends on a full `./tools/ninja-locked`:**

| | `matched_functions` | `matched_code` | `matched_code_percent` | `total_functions` | `total_code` |
|---|---:|---:|---:|---:|---:|
| baseline | 43,651 | 4,054,964 | 39.571945 | 69,240 | 10,247,068 |
| final | **43,675** | **4,066,408** | **39.683624** | 69,240 | 10,247,068 |
| Δ | **+24** | **+11,444 B** | **+0.111679 pp** | 0 | 0 |

`fuzzy_match_percent` 49.804825 → 49.808640. `masked_equal_functions`
23,107 → 23,111. Set-diff of the `fuzzy==100` row set: **24 rows in
(11,580 B), 3 rows out (136 B)**; 2 of the 3 "out" rows are the same
funclets re-attributed from `Tour` to `BandProfile` by the §4 re-home, so the
only genuine loss is the intended 72 B `?SyncProperty@Tour@@` (§5).

---

## 1. Baseline verification

The brief's figures were tested literally before anything was built on them,
per the standing "read the in-tree record first" rule. Build 1, rc=0.
**Every briefed headline key and all six briefed rows reproduced exactly**:
`matched_functions` 43,651 / `matched_code` 4,054,964 / `matched_code_percent`
39.571945 / `total_functions` 69,240 / `total_code` 10,247,068 / fuzzy
49.804825 / `masked_equal_functions` 23,107, ruler `name_check`. Baseline
row set saved to `~/tmp/w16bu/rows_base.json` (40,979 rows).
`~/tmp/rows_w16bs_main.json` was never written to.

Two of the brief's *analytical* claims did not survive, and both are
corrected below (§2 and §3): the `??0Tour@@` size premise, and W16-BS's
`SetTokenFmt` diagnosis. The brief's item-5 premise also did not survive
(§5). The numeric baseline was sound; the interpretations were not.

## 2. `??0Tour@@` — identity adjudicated on retail bytes, then re-homed

**The brief and W16-BS both record this row as "0.0, 660 B ours vs 300
retail". The 660 B is a reader artifact, not a source defect.** Our
`fn_size` is **300 B — exactly retail's 300** (+216 B of EH funclets on both
sides). The 660 came from measuring a COMDAT *span* (which includes the
successor's funclets) against a `.pdata` *function extent*. This is the
STLPORT-1 artifact class recurring verbatim, and it is why the row was
briefed as a source problem when it never was one.

**Identity, established on retail bytes independently of the map:** vptr
store, the argument stores, two container constructions through the same
callee, the `TourWeightManager` construction, `SetName`, and two `AddSink`
calls. It is `Tour::Tour`. Masked on the object's own 29 relocation offsets:
**0 differing words out of 75.**

**Why the row read 0.0:** objdiff pairs by NAME *within the unit the pin puts
the address in*. The 300 B sat inside the `BandSongMetadata.cpp` pin, and
`BandSongMetadata.obj` cannot define `??0Tour@@`. Nothing about our source
was wrong.

**Cut point, derived rather than guessed:** `0x8235FF14 − 0x8235FD10 = 0x204
= 516` = our COMDAT size (300 + 216); and `0x8235FCE0 + 40 = 0x8235FD08`
leaves the 8-byte EH prefix outside the new block. The new start is a
function start (dominant convention, 4,863 starts vs 247 non-starts).

`.text` edit only — `.pdata` is derived output and dtk re-derived the split
at `0x822004A8` by itself. The first build after the move failed by design
("THE SPLIT REWROTE ITS OWN INPUT"); recovery was one build, and
`symbols.txt` was unchanged.

Measured: the **alias alone was Δ0 exactly**, as pre-registered; the
**re-home was +5 fns / +384 B** (predicted central +300 B/+1 fn, band
[+300, +516]). Crossed in: `??0Tour@@` 0.0→100.0 (300 B), `fn_8235FE3C`
(40 B), `fn_8235FEBC` (44 B). Fell out: none.

⚠ **A fabricated alias was caught and withdrawn before it landed.** A
26-spelling `hash_map<Symbol,*>` ctor group was built in which every spelling
was byte-identical to retail `0x8255d480` modulo one relocation — which
*looks* like proof. Checking the group against the map showed two of those
spellings are mapped to **different** retail addresses (`0x825af9e8`,
`0x82278a20`), and those bodies differ from `0x8255d480` in exactly one word:
the `+0x30` `bl`, targeting three different hashtable constructors. The broad
group was withdrawn; only the narrow, directly-observed group was installed
(`scripts/symbol_aliases.json`, now 1,657 groups). This is the
CLAUDE.md hazard that an unproven alias lifts the score *by construction*.

## 3. The four source rows

All four were priced from `report.json`'s **charged-site lists**, never from
a mismatch count (RESIDUAL-1). Pre-registered band [0, +604 B].

| row | size | fuzzy before | source edit | fuzzy after | Δ bytes |
|---|---:|---:|---|---:|---:|
| `?InitializeTour@Tour@@QAAXXZ` | 284 | 83.30986 | `static Symbol tour("tour")` inside the function | **100.0** | +284 |
| `?GetCurrentQuestDisplayName@TourPerformerImpl@@` | 88 | 71.63636 | delete `if (!pQuest) return quest;` | **100.0** | +88 |
| `?UpdateTourPlayerContributionLabel@TourPerformerImpl@@` | 112 | 33.75 | local static **+** hoist the `String` before it | **100.0** | +112 |
| `?GetConclusionText@Tour@@QBA?AVSymbol@@XZ` | 120 | 95.5 | `MILO_ASSERT` for the `if`, **keep** the named local | **100.0** | +120 |

Measured in two steps: +1 fn/+332 B then +3 fns/+272 B; **all four rows reach
`fuzzy == 100`**, total +604 B gross across the two commits.

**`InitializeTour`** — retail indices 8–18 are an MSVC function-local static
guard: guard word `0x82CBE908`, static Symbol `0x82CBE904`,
`clrlwi. r9,r11,31` / `bne` / `ori r11,r11,0x1` / `stw`, then
`bl ??0Symbol@@QAA@PBD@Z` on literal `0x82000C50` (read out of retail as
`"tour"`). We referenced the file-scope `?tour@@3VSymbol@@A` from
`utl/Symbols.h`, which carries no guard at all.

**`GetCurrentQuestDisplayName`** — we emitted an extra `mr.`/`bne`/`stw`/`b`:
the `if (!pQuest) return quest;` early return. The rb3-Wii DEV oracle **has**
that check; retail (88 B) does not. Retail bytes outrank the oracle. W16-BS
diagnosed this row correctly.

**`UpdateTourPlayerContributionLabel`** — two independent defects, and the
first alone was not enough (33.75 → 59.107 → 100.0). (a) the same local-static
guard (guard `0x82CBE9FC`, Symbol `0x82CBE9F8`, literal `0x8203E6B0` =
`"generic_string"`); (b) **evaluation order**: retail materializes
`GetPlayerContributionString(user)` into the `String` temp at `r31+0x50`
*before* the guard sequence. Emitting the guard first keeps `this`, `label`
and `user` live across the `??0Symbol@@` call, so we saved **r27–r31**
(`bl __savegprlr_27`, frame `0x90`) where retail saves **r29–r31**
(`__savegprlr_29`, frame `0x80`). Every remaining `diff_arg` (indices
1,2,3,23,27,29,33,34) was downstream of that one difference — the register
and frame deltas were **symptoms**, exactly as `fixable-liveness.md` warns.

> ⛔ **W16-BS's diagnosis of this row is REFUTED, with the mechanism.** BS
> recorded "retail instantiates `SetTokenFmt<char*>`, ours `<const char*>`".
> The textual difference is real — index 30 is `??$SetTokenFmt@PAD@` vs our
> `??$SetTokenFmt@PBD@` — but it is **not a charged site**: objdiff forgives
> it through the ICF alias map (5,563 equivalence entries loaded), so its
> Match column is blank and it contributes nothing. Chasing it would have
> been unpayable work on an already-forgiven site. Note this is *not* the
> BQ §9.1 class the brief warns about, which applies when the instantiation
> **sizes differ**; here they fold, which is why they are forgiven.

**`GetConclusionText`** — after removing the `if`, exactly **three** charged
instructions remained, and they named the cause: retail loads the Symbol from
the fixed slot `lwz r4, 0x50(r1)`, while we routed through the returned
pointer (`mr r11, r3` / `lwz r4, 0x0(r11)`) — a named local vs a nested
temporary. The first edit removed the spurious `if` (the real win) but also
collapsed the named `Symbol tourSym` into the argument; restoring it closed
the row. **The structural template was already in the same file at fuzzy
100.0**: `Tour::GetAnnouncement` is named-local + `MILO_ASSERT` + no `if`.

Retail's per-function asymmetry here is real and mirrored in the oracle:
`Tour::GetTourGigGuideMap` genuinely **has** `if (pTourDesc)` (ours is
fuzzy 100.0) while `GetAnnouncement` uses `MILO_ASSERT` (ours is fuzzy
100.0). Only `GetConclusionText` had the `if` added on our side.

**Fell out and came back:** `fn_82361020`, a 40 B unnamed EH funclet with
`masked_equal=true`, dropped to 99.9 on both rulers in the first commit and
returned to 100.0 in the second. objdiff pairs unnamed funclets by byte
signature; changing the TU changed the pool. Bookkeeping churn, not a code
regression — same class as PAIRFIX's `fn_8267F574`.

## 4. `0x8258bab8` — decode, verdict, and why the rename was safe

**Verdict: `BandProfile::HasCampaignKey`, not `Tour::HasTourDesc`.**

Evidence, in order of weight:

1. **Compiler layout report** (authoritative; not the `// 0xHEX` comments):
   `BandProfile` has `mCampaignKeys` at **0x80**, `unk88` at 0x98,
   `mUnlockedModifiers` at 0xb0.
2. **All three `Has*(Symbol)` members are the same 68 B body**, differing
   only at word `0x14` — the member offset. Retail `0x8258bab8` uses
   **`+0x80`** ⇒ `mCampaignKeys` ⇒ `HasCampaignKey`.
3. **Control:** our `HasSeenHint` against its mapped `0x8258bf60` gives **0
   differing words**, both using `+0x98`. The instrument discriminates.
4. **Spatial:** the block was a `Tour.cpp` island entirely surrounded by
   `BandProfile.cpp`, and the 924 B `fn_8258BB08` inside it calls
   `fn_8258AB30`, which lies inside BandProfile.cpp's own block.

**Source defect exposed by the decode:** ours was 92 B against retail's 68 B
— an extra `if (MetaPanel::sUnlockAll) return true;`. The rb3-Wii DEV oracle
has it; retail does not, and a scan of **all 589 retail 92-byte functions
found 0** carrying our variant. Guarded with the house pattern
(`#ifdef HX_NATIVE`) so the native build keeps the behaviour and the match
build gets retail's body.

**Was renaming safe?** Only because it was coupled with the re-home, and that
is the whole point. Our `Tour.obj` does not define
`?HasCampaignKey@BandProfile@@`; our `BandProfile.obj` does. Renaming alone
would have left the row permanently 0%. Moving the `.text` block to
`BandProfile.cpp` *and* renaming in the same commit is what made the name
definable. Cost of retiring `?HasTourDesc@Tour@@` (80.29): **zero** — it
contributed 0 bytes and 0 functions.

Measured **+68 B / +1 fn**, exactly the central prediction.

## 5. `0x8235c2e0` — the SyncProperty naming A/B

**The brief's premise did not survive, and neither did my own
counter-premise. The measurement settled it.**

The brief framed this as naming an address to convert "~18 forgiven
`parent::SyncProperty` call sites" into checked ones. In fact `0x8235c2e0`
was **already named** `?SyncProperty@Tour@@…` and that row was **already at
fuzzy 100.0 / mpn 100.0** (72 B, `default/Tour`). So the operation is a
*rename of a working row*, and the "naming an anonymous address" economics
(a bet paying in bug exposure) do not apply.

I pre-registered **−72 B / −1 fn with zero upside**, on four grounds. Three
held; one was wrong:

- **Held:** an alias cannot rescue a renamed row. objdiff pairs by NAME, and
  the only pairing path consulting `SymbolEquivalences` is
  `pair_funclets_by_bytes` (`objdiff-core/src/diff/mod.rs:836`), restricted to
  `__unwind$NNN` / `__catch$NNN` / `fn_<addr>` funclets.
- **Held:** our `Tour.obj` defines `?SyncProperty@Tour@@` (COFF grep: 1) and
  not `?SyncProperty@Object@Hmx@@` (0); and no re-home is available because
  `0x8235c2e0` sits inside Tour.cpp's **own** pinned block
  `0x8235C080–0x8235C328`.
- **Held:** the Tour row did un-pair to fuzzy 0 / mpn 0, losing its 72 B,
  for exactly the predicted reason.
- ⛔ **WRONG: "zero code call sites."**

**Measured A/B** — both legs with a forced re-split iterated to the
`symbols.txt` fixed point (hash unchanged, `1e8375f974934af4`), with
`report.json` + `report.cache` wiped before each read:

| leg | map row | `matched_functions` | `matched_code` | `matched_code_percent` |
|---|---|---:|---:|---:|
| A | `?SyncProperty@Tour@@` | 43,661 | 4,056,020 | 39.582250 |
| B | `?SyncProperty@Object@Hmx@@` | **43,675** | **4,066,408** | **39.683624** |
| | **Δ** | **+14** | **+10,388 B** | **+0.101374 pp** |

Leg A reproduced the earlier un-resplit build to the last digit, so the
forced re-split contributed exactly 0 and the delta is entirely the change.
Leg B reproduced across two independent builds.

Rowset set-diff A→B: **14 rows in (10,460 B), 1 row out (72 B)**. Every
crossing row is a `?SyncProperty@<Class>@@` body in a different unit:
ChordShapeGenerator 2,644 · RndTexRenderer 1,380 · CalibrationPanel 1,244 ·
SongSectionController 1,116 · CharBone 820 · TexMovie 580 · PatchPanel 576 ·
CustomizePanel 468 · PatchSelectPanel 448 · RndPollAnim 272 ·
ChordbookPanel 228 · StorePanel 228 · StoreInfoPanel 228 · GemTrainerPanel 228.
Out: `?SyncProperty@Tour@@` 72.

The charged site is directly visible, e.g. `CharBone::SyncProperty` index 199:

```
target  bl ?SyncProperty@Tour@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z
base    bl ?SyncProperty@Object@Hmx@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z
```

⇒ the old map name was charging **14 correct calls as wrong callees**.

> ⛔⛔ **What this exposed about my own instrument.** My "zero code call
> sites" claim came from a batch instruction scan of 1,605 symbols across 14
> units that returned 0 for every unit. objdiff's `--batch` JSON instruction
> rows use the keys `index` / `target` / `base` / `match_type`; my filter
> required `instruction` and `diff_kind`, so it **discarded every row** and
> reported a clean, decisive-looking zero. The positive control I ran
> alongside it — 260 occurrences of `??0Symbol@@QAA@PBD@Z` in the raw stdout
> — only established that relocation names appear *somewhere in the text*, a
> stage the broken filter never reached. **The control exercised a different
> code path than the claim rested on, so it could not have failed.** Same
> family as the `grep`-binary and `all([])` vacuities in CLAUDE.md. Only the
> A/B caught it, and it caught it by contradicting a confident prediction.
> The earlier COFF relocation census (122 references, all `.rdata`) was
> *correct as far as it went* — it enumerated data relocations and I wrongly
> read its silence on `.text` as evidence of absence.

**Accuracy, independent of the +10,388 B.** The map holds one name per
address, so whichever spelling is chosen the other folded spellings cannot
pair. `?SyncProperty@Object@Hmx@@` is referenced by **14** of our compiled
call sites; `?SyncProperty@Tour@@` by **0**. The survivor is the base-class
implementation every derived `PROPSYNCS` body calls, so naming it for the
base is the truer identification as well as the better-scoring one. The Tour
row now reading 0 is the correct consequence: our `Tour.obj`'s copy is a
distinct COMDAT the linker discarded in favour of the survivor.

This also resolves the brief's open disjunct. 122 vtable slots **plus** 14
code call sites resolving to one body is a large ICF fold class — **not**
evidence that Tour fails to override `SyncProperty`.

## 6. Gates

All run in the worktree, in order, after a full `./tools/ninja-locked` rc=0:

| gate | result |
|---|---|
| `scripts/verify_ruler_agreement.py --check` | PASS — both entry points resolve the same ruler |
| `scripts/verify_objs_patched.py --verify-manifest` | PASS — 1,219 decomp + 3,105 target objects, `tree_sha256=661d8fded2268b4b` |
| `tools/icf_alias_finder.py --validate` | PASS — 1,407 map-consistent, 249 tolerated, **0 contradicted**, 1,657 total |
| `tools/funclet_homing.py --validate` | PASS |

Native gate line is pasted verbatim at the end of the lane report.

## 7. NOT done, and what would change each

1. **`fn_8258BB08` (924 B, reads 0) is unidentified.** It sits in the block
   re-homed to `BandProfile.cpp` and calls `fn_8258AB30` inside BandProfile's
   own region, so its *home* is settled; its *name* is not. There is **no
   size-924 counterpart anywhere in our `BandProfile.obj`**, so this is
   **absent source, not a naming defect** — no map edit can collect it.
   *Evidence that would change this:* a 924 B `BandProfile` member in the
   rb3-Wii oracle whose body shape matches, or a retail-byte decode of its
   member offsets against the compiler layout report (as in §4) identifying
   which member it touches.

2. **Three `??0Tour@@` EH funclets reach `mpn == 100` without
   `fuzzy == 100`.** They are relocation-name-only residue. Per MPNGAP-1 this
   stratum is ~91% irreducible fold/map noise and is explicitly **not** a
   byte lever; I did not fund it. *Evidence that would change this:* a
   retail-byte adjudication showing the named callee's signature is
   incompatible with the call site (the MPNGAP-1 `Handle@GemPlayer` method),
   which would make it a map defect rather than a fold.

3. **The 88 STALE_SPELLING / 100 UNWITNESSED alias groups were left alone.**
   They forgive 0 bytes today, but pruning on that screen is measured
   harmful (`a745039e` cost +94,616 B to reverse). A Δ0 today would license a
   change that degrades later.

4. **`?SyncProperty@Tour@@`'s 72 B is not recoverable** while the map names
   `0x8235c2e0` for the base class. It is the correct trade (14 rows vs 1).
   *Evidence that would change this:* a second retail address holding a
   distinct non-folded `Tour::SyncProperty` body — there is none; the map has
   no other SyncProperty address in Tour's range.

5. **Nothing was touched in the concurrent lanes' surfaces.** No `MemAlloc`
   call site in my units needed the parenthesized bypass, so there is nothing
   filed for W16-BT; I did not enter `system/dsp/*`, `src/system/utl/MemMgr.*`,
   or the `0x822E`/`0x822F` VocalTrackDir/GemTrackDir units (W16-BV).
