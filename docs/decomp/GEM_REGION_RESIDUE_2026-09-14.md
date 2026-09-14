# W16-I — W16-B's flagged residue in the Gem / GemTrack / GamePanel region

**Lane:** W16-I (Opus) · **Date:** 2026-09-14 · **Branch:** `w16-i` · **Ruler:** shipped
`functionRelocDiffs=name_check` throughout; every number below is whole-binary
`build/45410914/report.json` after `touch config/45410914/config.yml` + a full
`./tools/ninja-locked`.

All five items were **adjudication on retail bytes**. Source of the items:
`docs/decomp/ROCKCENTRAL_ONMSG_ESCALATION_2026-09-14.md` (W16-B) §"Not done" + finding 7,
and `docs/decomp/THUNK_HANDOFF_REPINS_2026-09-14.md` (W16-F) §3c/§5. The §5 leads are
written up in that doc's new **§6**, not duplicated here.

## Ledger

| step | commit | change | predicted | measured | running total |
|---|---|---|---|---|---|
| baseline | — | — | — | — | 43,147 / 3,928,112 |
| item 5a | `d170a4ea` | `0x822d3920` → `Copy@EndingBonus` + re-home, 4 thunks | +5 / +44 | **+5 / +44** | 43,152 / 3,928,156 |
| item 1 | `2c9681c0` | `GetLoopTick` ×2 map names + HamRibbon/TrainerPanel re-pin | +2 / +216 | **+6 / +656** ⚠ | 43,158 / 3,928,812 |
| item 2 | `765ba2b9` | `set<TrackWidget*>::clear` alias membership | +2 / +616 | **+2 / +616** | 43,160 / 3,929,428 |
| item 5b | `73c9ce58` | `0x823aedf0` → `Replace@CharWeightable` `$2` thunk | +1 / +12 | **+1 / +12** | 43,161 / 3,929,440 |

**Lane total: +14 matched_functions / +1,328 matched_code bytes**, code% 38.345 → 38.351130.
Four of five predictions were exact; the one miss is analysed below because an
under-prediction is more informative than the four hits.

**No `src/` file was touched by this lane** (`git diff --name-only main...HEAD` = three
files, all under `config/` and `scripts/`), so **the native gate does not apply** and was
not run. Stated explicitly so silence is not read as coverage.

---

## Item 1 — `DrawBeatLine@GemTrack` (684 B)

**Verdict: (a) the MAP NAME is wrong; our source is right.** Not a fold, not a source bug.

Retail's `bl` at +368 resolved to the map name `?OnIsSpeechSupportable@SpeechMgr@@QBA_NXZ`.
Three independent instruments all refuse that name:

1. **Call-site semantics.** The `bl` at +368 is immediately followed at +372 by
   `bl ?TickToBeat@@YAMH@Z` = `float TickToBeat(int)`. So the first callee's return feeds
   an `int` tick parameter. A `QBA_NXZ` — nullary, `const`, returning `bool` — cannot
   produce a tick, and takes no arguments where the site passes some.
2. **The oracle.** rb3-Wii `GemTrack.cpp:522` is literally `i2 = TickToBeat(GetLoopTick(i3));`,
   and `TrainerPanel.cpp:434` defines the free function `int GetLoopTick(int)` with exactly
   the 36-byte "spill one out-param, tail into the 2-arg form" shape retail has.
3. **Size.** Retail's inner callee at `0x826CA800` is 180 B — our `?GetLoopTick@@YAHHAAH@Z`.
   `IsSpeechSupportable` is 428 B. Different functions.

**Not a fold.** Our two 36-byte candidates are byte-identical but their *relocations*
differ, and MSVC folds only COMDATs identical **including relocations**; `symbol_aliases.json`
had 0 hits for either spelling.

**What changed** (`2c9681c0`): `0x826ca800` → `?GetLoopTick@@YAHHAAH@Z`, `0x826ca8b8` →
`?GetLoopTick@@YAHH@Z`; plus a mis-pin fix — HamRibbon owned a 180 B island inside
TrainerPanel's territory, so `0x826CA800-0x826CA8B4` was deleted from HamRibbon and
TrainerPanel's two blocks merged to `0x826C97B0-0x826CB4A0`.

**⚠ Predicted +2 / +216, measured +6 / +656 — my worst prediction of the lane, and the
cause is doctrinal.** I priced only the two renamed rows and assumed no caller would
cross, *despite CLAUDE.md stating plainly that caller-financing is the dominant term when
repairing a wrong name* (MAPDEF-3: a wrong name is financed by its callers, and the
un-charging is 80.5% of a map edit's delta). Four caller rows crossed. Full attribution of
the surplus: **620 B from 4 caller rows**, **36 B from `GetLoopTick(int)` itself**, and
**+1 function / 0 B from a row sitting at `mpn` 100 / fuzzy 98.889** — the arg-blind
split (DB-4) doing exactly what it is documented to do. **The lesson I then applied to
every later item: enumerate the charged CALLERS before predicting, not just the renamed row.**

⚠ Also recorded: the step-2 build **failed once (rc=1) on the split-guard**, because dtk
re-derives `.pdata` from `.text` and its output was not a fixed point of its input. This is
documented behaviour, not a defect. **The trap worth naming is that `report.json` still
held step-1 values at that moment**, so a careless read would have reported "Δ0" off a
stale file. Recovery was one more build (rc=0).

---

## Item 2 — `RemoveAllInstances@Gem` (416 B)

**Verdict: (b) a genuine ICF fold. Our spelling was a missing member of an existing,
already-T1-proven alias group.** The brief offered "proven fold" or "container-type bug in
our header"; it is the former. Our `Gem::mWidgets` is `std::set<TrackWidget*>` in both our
tree and rb3-Wii, so there was never a container-type bug.

**⚠ This overturns a verdict I reached earlier in this same lane, and the error is the
reusable part.** I first tested our `set<TrackWidget*>::clear` against the **survivor's
spelling** (the `map<Symbol,CharLipSync*>` instantiation), found they cannot fold, and
concluded "wrong map name, not a fold". Both premises were true and the conclusion did not
follow. **The question is never whether our spelling folds with the survivor's spelling —
it is whether our spelling matches the RETAIL BYTES AT THE ADDRESS.** Here the survivor
name is genuinely wrong *and* the address genuinely holds a fold class we belong to; those
are not alternatives.

**The gate, two levels, because level 1 is vacuous.**

| | | |
|---|---|---|
| **L1** | retail `.pdata` extent at `0x822dea78` is 80 B; our COMDAT is 80 B, **0** unmasked differing words | proves nothing alone — every `_Rb_tree::clear` body is identical except its single relocation at `+0x24`, so **all three** candidates pass, including the impossible one |
| **L2** | retail's `bl` at +36 → `0x822dd9a0` (92 B); our `_M_erase` is 92 B, **0** unmasked diffs, and both relocation target names agree (self-recursion `+0x28`, `?MemOrPoolFreeSTL@@YAXHPAX@Z` `+0x38`) | the discriminating level — this is what T1's "relocation TARGET NAMES compared" means |
| **negative control** (required to fail, **and does**) | the survivor's own `map<Symbol,CharLipSync*>` `_M_erase` differs at +48: `li r3,24` vs retail's `li r3,20` | map node = 16 B base + 8 B `pair<const Symbol, CharLipSync*>`; set node = 16 B + a 4-byte value |
| **positive control** | the group's existing member `set<Symbol>::clear` (restored by W9-D on the same flat L1_T1 basis) passes identically with `li r3,20` | `set<Symbol>` and `set<TrackWidget*>` share a 4-byte value type ⇒ one node size, one deallocator, one body |

My earlier node-size reading was **right about the byte and wrong about what it licensed**:
`li r3,20` says "the value type is 4 bytes", which is true of `set<Symbol>` *and*
`set<TrackWidget*>` — it identifies a **family, not a member**.

**What changed** (`765ba2b9`): the TrackWidget spelling added to group `0x822dea78`'s
`folded` list with an `added` record carrying the evidence above.
`tools/icf_alias_finder.py --validate` → **PASS, 0 CONTRADICTED**. The file's 53
pre-existing multi-address spellings were asserted **unchanged** across the edit.

**Predicted +2 / +616, measured +2 / +616 exactly**, priced from `report.json`'s charged-site
list rather than a mismatch count (RESIDUAL-1):

| row | size | charges | this pair | outcome |
|---|---|---|---|---|
| `?RemoveAllInstances@Gem@@QAAXXZ` | 416 B | 1 | 1 | 99.95192 → **100** |
| `??4?$_Rb_tree@PAVTrackWidget…@Z` | 200 B | 1 | 1 | 99.9 → **100** |
| `??1Gem@@QAA@XZ` | 96 B | 2 | 1 | predicted **not** to cross; measured 99.58334 → 99.79166, **0 B** |

**Not done, deliberately.** The survivor spelling is provably not the body at that address,
but renaming it is a separate, riskier edit: `CharLipSync.obj` defines no set-family
spelling, so a rename without a re-home would un-pair the row outright. *Proving a name
wrong does not make renaming safe.* Recorded as a follow-on lead.

⚠ **Formatting note that cost a redo:** `json.dump(indent=2)` rewrites all ~75k lines of
`symbol_aliases.json`. Only **`indent=1` + a trailing newline** round-trips it byte-exactly.
Check the round-trip before editing any large JSON here, or the diff is unreviewable.

---

## Item 3 — `0x827C91A0` (36 B) and its caller `0x82695178`

**Verdict: (c) was already done, (b) leave it anonymous, (a) the caller is an UNDER-PORTED
BODY — not a naming defect.**

- **(c)** Already complete: `0x827c91a0` is **absent** from `target_symbol_map.json`
  (verified directly). W16-B had removed the refuted `??__ETheLocale@@YAXXZ`. Nothing to do.
- **(b)** Leave it a placeholder. An exhaustive oracle grep finds **no** `SecondsToTick` /
  `SecToTick` in rb3-Wii, DC3 **or** our tree, so there is no named helper to name it after.
  A placeholder target name is **forgiven**; a wrong name is **charged** — so leaving it
  anonymous is not a deferral, it is the correct end state.
- **(a)** `0x82695178` identified on retail bytes: a 628 B `.pdata` head containing
  `bl 0x827c91a0` at +44, immediately after `bl ?Seconds@TaskMgr@@`, together with
  `GetMaxValue@TourProperty`, 4× `fmod`, `??0Symbol@@`, `GetTrackPanelDir` and 3×
  `MakeString<Symbol,…>`. That matches the oracle's debug-HUD `UpdateNowBar`.

**The useful finding is what this resolves.** W16-B recorded an apparent contradiction
between `TimeConversion.cpp:50` (correct) and `GamePanel.cpp:461-476`. There is no
contradiction: our port of the caller is a **one-liner** where retail's is 628 B. The
region's residue here is **absent source, not a wrong name** — which is why no map edit
could have helped, and why none was made.

---

## Item 4 — `Handle@OvershellSlot` — PRICED, deliberately not ground

The brief said price it, don't grind it. Priced from `report.json`'s charged-site list.

**1 charged site out of 2,319 instructions**, on a **9,276 B** row at fuzzy 99.99784 /
`mpn` 99.9978. Because `matched_code` is all-or-nothing per row and `mpn` is *also* below
100, the entire **9,276 B and +1 function** sit behind that single charge — the largest
single prize encountered in this lane.

**Class of the charge: relocation-name.** Not regalloc, and — importantly — **not a
source-visible construct at the charged site**, because our call site is provably correct:

```
+0x14d0 addi r3,r11,16
+0x14d4 bl   <…>
+0x14d8 mr   r4,r3          ; int return becomes arg 2
+0x14dc lwz  r3,168(r26)    ; r3 = member at +0xA8
+0x14e0 bl   0x826c3888     ; (this*, int)
```

`0xA8` is exactly `OvershellSlot::mMuteUsersProvider` (our header, `// 0xa8`), and the
`int` comes from the preceding call — i.e. `mMuteUsersProvider->ToggleMuteStatus(i)`, with
`OvershellSlot::ToggleMuteUser` inlined into `Handle` by `/Ob2`. Our instruction is
byte-identical; only the relocation **name** differs.

**Fold or wrong callee? Neither, exactly.** Retail's callee at `0x826c3888` is
`4e800020` — a bare `blr`, **a 4-byte EMPTY function**, and an existing `FT-EMPTY` alias
group there already folds `??3@YAXPAX0@Z` (placement delete) and `BandTrack::Copy`. Our
`SessionUsersProvider::ToggleMuteStatus` is **56 B of real code**. So the divergence is in
the **callee's body**, not the call site: retail's compiled to nothing, ours did not.

⚠ `pdata_extent(0x826c3888)` returns the entry beginning at `0x826C3874` — `0x826c3888` is
**not** a `.pdata` BeginAddress. That is expected, not an error: an empty leaf stub touches
neither stack nor LR and gets **no unwind record** (the AUDIT-NC sub-`.pdata` stratum).

**Why ours is 56 B:** our tree has **no `VoiceChatMgr.cpp` at all** — only a header
declaring `void ToggleMuteStatus(User*)` — so the call is emitted out-of-line. rb3-Wii has
a real vector-based body (`VoiceChatMgr.cpp:80`). Our `SessionUsersProvider::ToggleMuteStatus`
is otherwise **character-identical** to rb3-Wii's, with no platform guard.

**Why I did not "fix" it.** Realising the 9,276 B needs *two* changes: (a) a `src/` change
making the callee compile to nothing, on the speculation that RB3-360 stubs the voice-chat
mute path — a behaviour-removing change resting on inference, and rb3-Wii is the **DEV**
build, so it is a weak oracle for a 360 retail question; and (b) an alias whose only byte
evidence would be *"both are a 4-byte `blr`"* — the weakest possible tier, and precisely
the class where ICF destroyed which name the site meant. Installing (b) **without** (a)
would be a fabricated alias worth 9,276 B: exactly the integrity hazard CLAUDE.md calls
"the one data file where 'the score went up' is not evidence of anything". Retail's map
names **zero** `VoiceChatMgr` symbols, so it neither corroborates nor contradicts.

**Recommended decisive experiment for a follow-on lane** (cheap, non-metric): determine
independently whether RB3-360 retail has any `VoiceChatMgr` body at all — e.g. locate the
`unk30` mute-list vector operations in retail, or the `voice_chat_disabled` message string
and its referencing code. If retail has no mute-list mutation, (a) is established on
evidence rather than inference and the 9,276 B becomes collectable honestly.

---

## Item 5 — W16-F's handed-on leads

Written up in **`docs/decomp/THUNK_HANDOFF_REPINS_2026-09-14.md` §6** (§6.1 `0x822d3920`,
§6.2 the three NODEF heads, §6.3 the `0x8252a598` verification), per the brief. Summary:

- **`0x822d3920` = `?Copy@EndingBonus@@UAAX…`** — positively identified, better than the
  `null` the brief allowed. Two independent instruments agreed 4-for-4 across the thunk
  family; map + pins fixed in one commit. **+5 / +44, predicted exactly.**
- **`0x823aedf0` = `?Replace@CharWeightable@@$2PPPPPPPM@A@AAX…`** — **corrects W16-F's
  "no definition of `Replace`"**. We define it; the thunk digit encodes **access**, so a
  protected virtual's thunk is `$2` while all seven public siblings are `$4`. Searching for
  the `$4` form finds nothing and reads like "no definition". **+1 / +12, predicted exactly.**
- **`0x822af088` / `0x822aec00` (PatchRenderer) stay `null` — IDENTIFIED-NO-SOURCE.**
  `PatchRenderer` has no `.cpp` anywhere in our tree. Both bodies exist in rb3-Wii
  (`PatchRenderer.cpp:94-102` and `:46`) and **not** in DC3; oracle locations recorded for a
  future port. Naming them now would install names no compiled obj defines ⇒ 0% forever.
- **`0x8252a598`** — W16-F's refusal verified on retail bytes: the body is `return "";`
  (the byte at `0x82000C55` is `0x00`), so every such function in the program is identical
  **by construction**. Irreducible; do not re-litigate without a non-byte oracle.

---

## Contradictions of the brief, stated explicitly

1. **Item 2's fallback branch is unnecessary.** The brief said a refuted fold implies "our
   `Gem` holds the wrong map type — a real container-type bug — and the fix is the header".
   The fold is **proven**, and `Gem::mWidgets` is `std::set<TrackWidget*>` in both trees, so
   that branch was never live.
2. **Item 5b's premise is wrong for one of the three heads.** The brief (following W16-F)
   says all three "need SOURCE, not pins". `CharWeightable::Replace` needed neither source
   nor a pin — it needed the **correct spelling**. The other two do need source.
3. **W16-B's transferable claim does not generalise.** Its note that raw `band.exe` reads
   "via the PE section table and via linear file offset both failed validation — do not use
   them" is false as a general rule: a known-answer test refuted it, and this lane's entire
   retail-byte method reads `band.exe` through the PE section table via
   `tools/pdata_extent.py`, whose `--selftest` PASSes with its must-fail control intact.

## Not done

- **No `src/` change anywhere**, hence **no native gate** (not applicable, not skipped).
- **Did not rename the `0x822dea78` survivor** — un-pairing risk, see item 2.
- **Did not collect `Handle@OvershellSlot`'s 9,276 B** — see item 4 for the reason and the
  experiment that would unlock it honestly.
- **Did not port `PatchRenderer`** (needs a new TU + `objects.json` + splits) or **match
  `CharWeightable::Replace`'s body** (retail 144 B vs our 120 B, 23 real unmasked diffs; and
  retail's body at `0x823ae888` sits *outside* CharWeightable's pinned span, so it is a pin
  question as well as a source one).
- **Did not open `??1Gem`'s surviving charge** (`DeleteAll<vector<NetSavedSetlist*>>` vs
  `<vector<Tail*>>`) — a separate fold question, 96 B.
- **Did not run the permuter** (standing directive: OFF). The 180 B `?GetLoopTick@@YAHHAAH@Z`
  residual (fuzzy 98.889 / `mpn` 100.0) is a pure r10↔r11 swap across 7 instructions in
  `(tick-start)%(end-start)`, with no source-visible construct; declaration order is measured
  inert for register-only swaps.
