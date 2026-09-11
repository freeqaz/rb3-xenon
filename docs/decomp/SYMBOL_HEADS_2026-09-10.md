# Lane L5-SYMBOLHEADS — the 19 largest SYMBOL-class heads, adjudicated

**Date** 2026-09-10/11 · **Branch** `l5-symbolheads` off `main@3f9619c5` ·
**Ruler** `functionRelocDiffs=name_check` (graded), resolved from
`build/45410914/report.json` `provenance.diff_config` — never hardcoded ·
**objdiff pinned** `sha256:a5c35b15d7d46ac4` for **every** measurement below, so
the four deltas compose into a branch total.

## Result

| | Δmatched_functions | Δmatched_code |
|---|---:|---:|
| 1. TrackWatcherImpl vtable slots 6/18 (source + map) | +2 | +6,504 B |
| 2. `Synth::Find<Flow>`/`<Object>` (map ×2) | +3 | +2,628 B |
| 3. `0x822e8a78` = `PropSync<ObjPtrList<TrackWidget>>` (map) | +1 | +4,924 B |
| 4. `0x8253d820` = `MusicLibrary::PushSetlistToScreen` (map) | +5 | +2,908 B |
| **branch total** | **+11** | **+16,964 B** |

`matched_functions` 42,305 → 42,316 · `matched_code_percent` 36.843063 →
37.008633 (**+0.16557 pp**). Zero regressions in any run; `units at 100%`
unchanged on both rulers in all four.

Pre-registration honesty: **three of four hit EXACTLY** (+3/+2,628, +1/+4,924,
+5/+2,908). Fix 1 was under-predicted — I registered +1/+5,612 and measured
+2/+6,504, missing the mirror row `?Restart@GemPlayer@@UAAX_N@Z` (892 B) charged
by the same swap.

## The cross-row pattern (asked for before per-row work)

**86% of the ≤3-mismatch band is class SYMBOL, but the class is not homogeneous.
Three shapes, and only one of them paid.**

* **Group A — retail name is a tiny STL/trivial function, ours a game one-liner**
  (`_List_base<PassiveMessage*>::clear` vs `RemoveUser`; `Queue::~Queue` vs
  `ReadEmbeddedFile`; `??__FTheLocale` vs `TickToSeconds`). Every one REFUTED on
  size. These are our stubs sitting against retail's *empty-body ICF survivor*
  at `0x826c3888` (alias group 1539, tier FT-EMPTY, whose only folded member is
  `operator delete(void*,void*)`). **Irreducible without deleting real code.**
* **Group B — sibling methods of one class** (`OvershellPanel::OnMsg<T>` ×3;
  `Tour::GetConclusionText` vs `GetTourGigGuideMap`;
  `TourPerformerImpl::GetCurrentQuestSuccessMessage` vs `…DisplayName`). The
  `OnMsg` pair was **already withdrawn** by lane ALIAS-2 as
  `SURVIVOR_SIZE_MISMATCH` (our 132 B vs retail's 76 B survivor).
* **Group C — template/container-arg** (`Synth::Find<T>`, `PropSync<ObjPtrList<T>>`,
  `ObjPtrList<T>::Link`, `list<T>::insert`). **All four landed fixes came from
  here or from a plain misidentification.** This is the S2-CONTAINER class and it
  is still productive.

**The single most valuable finding: one wrong map row is financed by ALL of its
callers.** `0x8253d820` had five separate sub-100 rows, each with *exactly one*
charge, and in all five the charge was the same name pair. One rename, five
crossings. Conversely `0x822e8a78` had exactly **one** caller — so caller count,
not row size, predicts a map row's yield.

## The fold screen — PROVEN_FOLD 0/23

`tools/s1_fold_family.py --pairs` over all 23 charged pairs:
**REFUTED 18 (78.3%) · UNDECIDED 5 · PROVEN_FOLD 0.** ⇒ **no alias is licensed
anywhere in this worklist**, and `scripts/symbol_aliases.json` is untouched by
this lane. Every REFUTED pair is MAP-vs-CALLEE, which is exactly where all four
wins came from. (Briefed shape was REFUTED 67.3% / PROVEN_FOLD 0.9%; consistent.)

⛔ **The screen was BROKEN and its failure LOOKED LIKE A CLEAN NEGATIVE.**
`s1_fold_family.py` cached its symbol index at one shared path,
`~/tmp/s1fold-cache`, **not keyed by project**. Mine loaded an index minted
2026-09-01 inside `~/tmp/wt-s1fold` — deleted since — so every `retail_body()`
raised `FileNotFoundError` and the tool exited **1**, not the rc=3 fail-closed it
advertises. A verdict-grep over that crash returns **NO VERDICT for all 23 pairs**:
a screen that appears to find nothing. **The deleted tree is the benign case** —
had that worktree still existed the tool would have adjudicated this tree's pairs
against **another tree's objects** and emitted confident verdicts with no symptom.
Fixed here: cache keyed by a digest of the resolved project path, and
`build_index` verifies a sampled cached path still exists and rebuilds rather than
raising.

## Per-row table

Verdict: **MAP_WRONG** = fixed here · **IRREDUCIBLE** · **SOURCE_EXPOSED** = a real
divergence in our source, not closable by a map edit · **UNDECIDED** + what would
settle it. `row B` is the size-if-it-crosses of the whole row, not of the site.

| unit · symbol | row B | site | retail name | our name | s1 | verdict |
|---|---:|---:|---|---|---|---|
| default/OvershellSlot · `?Handle@OvershellSlot` | 9276 | 689 | `?clear@?$_List_base@PAVPassiveMessage@@V?$S…` | `?RemoveUser@OvershellSlot@@QAAXXZ` | REFUTED | IRREDUCIBLE — our stub vs retail empty-body ICF survivor |
| default/OvershellSlot · `?Handle@OvershellSlot` | 9276 | 1336 | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpm…` | `?ToggleMuteStatus@SessionUsersProvider@@QAA…` | REFUTED | IRREDUCIBLE — same (`0x826c3888`) |
| default/system/rndobj/Rnd · `?Handle@Rnd` | 6416 | 781 | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpm…` | `?TakeShot@HiResScreen@@QAAXPBDH@Z` | REFUTED | **SOURCE_EXPOSED** — retail callee IS the empty survivor ⇒ retail `TakeShot` is empty, ours has a body |
| default/OvershellPanel · `?Handle@OvershellPanel` | 5768 | 545 | `?OnMsg@OvershellPanel@@QAA?AVDataNode@@ABVC…` | `?OnMsg@OvershellPanel@@QAA?AVDataNode@@ABVS…` | REFUTED | **SOURCE_EXPOSED** — ours 132 B vs retail survivor 76 B (ALIAS-2 withdrawn) |
| default/OvershellPanel · `?Handle@OvershellPanel` | 5768 | 605 | `?OnMsg@OvershellPanel@@QAA?AVDataNode@@ABVC…` | `?OnMsg@OvershellPanel@@QAA?AVDataNode@@ABVN…` | REFUTED | **SOURCE_EXPOSED** — same |
| default/GemPlayer · `?Handle@GemPlayer` | 5612 | 692 | `?Restart@BeatMatcher@@QAAXXZ` | `?SetAutoplayError@BeatMatcher@@QAAXH@Z` | REFUTED | **MAP_WRONG ✅ FIXED** `1a0cd44f` |
| default/band3/game/Game · `?Handle@Game` | 5428 | 735 | `?TypeDef@Object@Hmx@@QBAPAVDataArray@@XZ` | `?TotalBasePoints@SongDB@@QAAHXZ` | UNDECIDED | UNDECIDED — 8 B forwarder, TOO-WEAK |
| default/GemTrackDir · `?SyncProperty@GemTrackDir` | 4924 | 975 | `?_M_insert@?$_Rb_tree@VSymbol@@U?$less@VSym…` | `??$PropSync@VTrackWidget@@@@YA_NAAV?$ObjPtr…` | REFUTED | **MAP_WRONG ✅ FIXED** `be5b1085` |
| default/Tour · `?Handle@Tour` | 3492 | 172 | `??0Tour@@QAA@PAVDataArray@@ABVSongMgr@@AAVB…` | `?InitializeTour@Tour@@QAAXXZ` | REFUTED | UNDECIDED — 284 B vs 240 B |
| default/Tour · `?Handle@Tour` | 3492 | 456 | `?GetConclusionText@Tour@@QBA?AVSymbol@@XZ` | `?GetTourGigGuideMap@Tour@@QBA?AVSymbol@@XZ` | UNDECIDED | UNDECIDED — unresolved reloc |
| default/UIList · `?SyncProperty@UIList` | 2664 | 340 | `??0?$reverse_iterator@PAH@stlpmtx_std@@QAA@…` | `?SetMaxDisplay@UIListState@@QAAXH@Z` | REFUTED | IRREDUCIBLE — 8 B vs 64 B |
| default/ChordShapeGenerator · `?SyncProperty@ChordShapeGener…` | 2644 | 655 | `?SyncProperty@Tour@@UAA_NAAVDataNode@@PAVDa…` | `?SyncProperty@Object@Hmx@@UAA_NAAVDataNode@…` | REFUTED | UNDECIDED — superclass-chain shape |
| default/DataFile · `?ParseNode` | 2432 | 86 | `??1Queue@@QAA@XZ` | `?ReadEmbeddedFile@@YAPAVDataArray@@PBD_N@Z` | REFUTED | IRREDUCIBLE — 180 B vs 172 B |
| default/DataFile · `?ParseNode` | 2432 | 459 | `r11` | `r30` | — | ⛔ ARITH_COMMUTE — **blocks this row regardless of the two SYMBOL sites** |
| default/DataFile · `?ParseNode` | 2432 | 587 | `??1Queue@@QAA@XZ` | `?ReadEmbeddedFile@@YAPAVDataArray@@PBD_N@Z` | — | IRREDUCIBLE — same pair |
| default/CharMeshHide · `?Handle@Synth` | 2320 | 457 | `??$Find@VFlow@@@Synth@@QAAPAVFlow@@PBD_N@Z` | `??$Find@VObject@Hmx@@@Synth@@QAAPAVObject@H…` | REFUTED | **MAP_WRONG ✅ FIXED** `fe4764d5` |
| default/TourPerformer · `?Handle@TourPerformerImpl` | 2196 | 188 | `?GetCurrentQuestSuccessMessage@TourPerforme…` | `?GetCurrentQuestDisplayName@TourPerformerIm…` | REFUTED | UNDECIDED — sibling getters |
| default/TourPerformer · `?Handle@TourPerformerImpl` | 2196 | 502 | `?SetDancer@AppLabel@@QAAXVSymbol@@@Z` | `?UpdateTourPlayerContributionLabel@TourPerf…` | REFUTED | UNDECIDED — sibling |
| default/RockCentral · `?RecordScore@RockCentral` | 1932 | 408 | `?SystemLocale@@YA?AVSymbol@@XZ` | `?SystemLanguage@@YA?AVSymbol@@XZ` | UNDECIDED | UNDECIDED — unresolved reloc |
| default/UI · `?Init@UIManager` | 1916 | 362 | `?insert@?$list@PAVCharClip@@V?$StlNodeAlloc…` | `?insert@?$list@P6AXXZV?$StlNodeAlloc@P6AXXZ…` | UNDECIDED | UNDECIDED — unresolved reloc |
| default/Instance · `?SyncDir@WorldInstance` | 1840 | 172 | `?insert@?$list@UWeightContext@CharBone@@V?$…` | `?insert@?$list@UObjPair@@V?$StlNodeAlloc@UO…` | REFUTED | IRREDUCIBLE — per-T `list::insert` |
| default/Instance · `?SyncDir@WorldInstance` | 1840 | 322 | `?insert@?$list@UWeightContext@CharBone@@V?$…` | `?insert@?$list@UObjPair@@V?$StlNodeAlloc@UO…` | — | IRREDUCIBLE — same pair |
| default/Instance · `?SyncDir@WorldInstance` | 1840 | 335 | `??0?$list@PAUDep@CharPollableSorter@@V?$Stl…` | `??0?$list@PAVObjRefOwner@@V?$StlNodeAlloc@P…` | REFUTED | IRREDUCIBLE — per-T ctor |
| default/system/rndobj/Rnd · `?PreInit@Rnd` | None | 25 | `?insert@?$list@PAVCharClip@@V?$StlNodeAlloc…` | `?insert@?$list@P6AXXZV?$StlNodeAlloc@P6AXXZ…` | — | — |
| default/system/rndobj/Rnd · `?PreInit@Rnd` | 1836 | 115 | `?NewObject@Object@Hmx@@SAPAV12@XZ` | `?NewObject@DOFProc@@SAPAVObject@Hmx@@XZ` | REFUTED | UNDECIDED — row is mm=3, one fix cannot cross it |
| default/system/rndobj/Rnd · `?PreInit@Rnd` | 1836 | 117 | `?NewObject@Object@Hmx@@SAPAV12@XZ` | `?NewObject@DOFProc@@SAPAVObject@Hmx@@XZ` | — | UNDECIDED — `@l` half of site 115 |
| default/BandCharacter · `?AddObject@BandCharacter` | 1716 | 213 | `?Link@?$ObjPtrList@VRndMesh@@VObjectDir@@@@…` | `?Link@?$ObjPtrList@VCharCollide@@VObjectDir…` | REFUTED | **HANDOFF** — R-TWOADDR, 196 B vs 260 B: one of the two names is wrong |
| default/band3/bandtrack/Gem · `?AddInstance@Gem` | 1704 | 137 | `??__FTheLocale@@YAXXZ` | `?TickToSeconds@@YAMM@Z` | UNDECIDED | UNDECIDED — we emit no `TickToSeconds` body |
| default/band3/bandtrack/Gem · `?AddInstance@Gem` | 1704 | 149 | `??__FTheLocale@@YAXXZ` | `?TickToSeconds@@YAMM@Z` | — | UNDECIDED — same pair |
| default/MusicLibrary · `?SelectNode@MusicLibrary` | 2260 | 549 | `?RemoveLastSongFromSetlist@MusicLibrary@@QAAXXZ` | `?PushSetlistToScreen@MusicLibrary@@QAAXXZ` | REFUTED | **MAP_WRONG ✅ FIXED** `dee84baa` (+4 more rows) |

## Evidence per fix

### 1. `TrackWatcherImpl` vtable slots 6/18 — transposed in BOTH our header AND the map (`1a0cd44f`)

**Two defects that cancelled**, which is why every row involved read a clean
100% and nobody looked. Retail puts `Restart` at slot 6 (`+0x18`) and
`SetAutoplayError` at slot 18 (`+0x48`); our header declared them the other way
round, and the map named the two forwarding thunks (and their `BeatMatcher`
callers) the other way round too. Compared **by name**, the two wrongs paired up.

Three independent retail-byte channels:

1. **Call site.** `GemPlayer::Handle` loads `lbl_820EB464`, which is literally
   `set_auto_play_error` in `band.exe` (file offset `0xeb464`; the adjacent
   `lbl_820EB458` is `remote_hit`, matching our side). It calls `DataNode::Int`,
   moves the result to **r4**, and calls `0x82790748` — the address the map
   spelled `?Restart@BeatMatcher@@QAAXXZ`. **`Restart()` takes no arguments.**
   ⚠ That string row reads "equal" in objdiff only because `lbl_*` is a
   **placeholder** target name, which `name_check` forgives — the string had to be
   read out of retail to be evidence at all.
2. **Vtable.** `0x827947b0` is `stw r4, 0x80(r3); blr` — it stores an **int
   argument**, so it cannot be `Restart()`. It sits at slot 18. Base validated by
   the `??_R4` COL pointer at `vt-4` and `??_G` at slot 0; neighbours corroborate
   independently (`SetCheating` `stb r4,0x7c` and `IsCheating` `lbz r3,0x7c` are a
   matched pair on **one** offset; `SetSyncOffset` takes its float in **f1**).
   Slots 9–13 all point at the single empty-body ICF survivor, exactly where our
   header has five empty/pure virtuals.
3. **Thunk chain.** `0x82790568`→`0x8279d6f0` loads `vt+0x18`;
   `0x82790748`→`0x8279d7b0` loads `vt+0x48`.

**Behavioural bug, independent of the metric and material to the native port:**
our `TrackWatcher::SetAutoplayError` dispatched slot `0x18`, i.e. it called
**Restart**, and `TrackWatcher::Restart` called **SetAutoplayError**.

Both halves must land together — either alone breaks the two 20 B thunk rows,
because the compensating error is what was holding them at 100%.

### 2. `Synth::Find<Flow>` / `<Object>` — both misnamed (`fe4764d5`)

Template-forced call identity (`X<T>` must call `Y<T>`). The two bodies are
identical but for **one** semantic word: `0x826fe3a8` calls
`ObjectDir::Find<Hmx::Object>` **and** `Object::StaticClassName` — two
independent relocations both naming Object — so it is `Synth::Find<Hmx::Object>`;
`0x826fe428` calls `ObjectDir::Find<Sequence>`, so it is `Synth::Find<Sequence>`.
Caller-side corroboration: retail's **only two** callers of `0x826fe3a8`
(`Synth::OnPassthrough`, `Synth::Handle`) both spell `Find<Hmx::Object>` in our
source (`Synth.cpp:566`, `Synth.cpp:140`).

We do not instantiate `Synth::Find<Sequence>`, so `0x826fe428` is now honestly
**unpaired** rather than falsely paired — the accuracy-over-headline trade,
deliberate. `??$Find@VFlow@@@Synth@@` names no address now; it contributed 0 bytes
before (fuzzy 99.6875) so nothing was lost. ⚠ **Δfuzzy is NEGATIVE (−0.0012 pp)
while Δcode_bytes is POSITIVE** — the threshold-vs-mean effect; reading fuzzy%
alone would report a regression that did not occur.

### 3. `0x822e8a78` = `PropSync<ObjPtrList<TrackWidget>>` (`be5b1085`)

Identified from its callee set: `DataNode::Int`, `DataNode::operator=`,
`DataArray::Release`, `PoolAlloc`, `PoolFree`, `ObjPtrList::Link`, and — **twice** —
`??$PropSync@VTrackWidget@@@@YA_NAAPAVTrackWidget@@…`, the *element*-level
PropSync. An `_Rb_tree::_M_insert` cannot call `PropSync<TrackWidget>` or
`DataNode::Int`. `PropSync_p.h:273` accounts for every one of those callees, and
the element call appears exactly twice on both sides (`kPropGet` and
`kPropSet`/`kPropInsert`). **Exactly one retail caller**, the charged row itself ⇒
no cascade. Displaced row sat at fuzzy 0.0000 ⇒ zero bytes lost.

### 4. `0x8253d820` = `MusicLibrary::PushSetlistToScreen` (`dee84baa`)

The best-shaped map defect in the lane: **five** sub-100 rows, each `mm=1`, all
five charging the *same* pair. The 216 B body calls `SetNetUIStateParam`,
`ContentMgr::RefreshInProgress`, `Symbol::Symbol` on the literal
**`refresh_setlist`** (read at `0x820900a8`), `Message::Message(Symbol)`, `atexit`,
`SendMessageToSongSelectPanel` — line-for-line our `MusicLibrary.cpp:1765`. Our
`RemoveLastSongFromSetlist` (`:1841`) does `pop_back`/`SetSyncDirty`/`SendMsg` and
shares **not one** callee; the retail body contains **no removal at all**. The
`atexit` is the function-local `static Message` registration — itself
corroboration, since `PushSetlistToScreen` has two function-local statics and
`RemoveLastSongFromSetlist` has none.

`RemoveLastSongFromSetlist` is a **real** method (its `…Msg` net-message class
members match at 100%); only its *address* was wrong. Its true address is
unidentified and it is now honestly unpaired.

## A/B lines (all on objdiff `sha256:a5c35b15d7d46ac4`)

```
1  Δmatched=+2  Δcode_bytes=+6504  Δcode%=+0.063477pp   42305→42307   pre-reg +1/+5612
2  Δmatched=+3  Δcode_bytes=+2628  Δcode%=+0.025650pp   42307→42310   pre-reg +3/+2628  EXACT
3  Δmatched=+1  Δcode_bytes=+4924  Δcode%=+0.048057pp   42310→42311   pre-reg +1/+4924  EXACT
4  Δmatched=+5  Δcode_bytes=+2908  Δcode%=+0.028386pp   42311→42316   pre-reg +5/+2908  EXACT
```

`none`-ruler control: fix 1 `NOT_APPLICABLE` (source present); fix 2
`REAL_PAIRING` (−128 B, the `Find<Flow>` un-pairing — expected and correct);
fixes 3 and 4 **`ALIAS_SUSPECT`** (name_check up, `none` flat, map-only).
**Both ALIAS_SUSPECTs are answered, not waived**: that shape is equally the
wrong-callee/map-repair signature, the two are separable only by evidence, and
§3/§4 above are that evidence. **No alias was added by this lane** —
`scripts/symbol_aliases.json` is untouched.

## Gates

```
icf_alias_finder --validate  (after a FULL ninja-locked, each of the 4 map edits)
VALIDATE: PASS -- 1370 map-consistent, 219 tolerated, 0 contradicted, 1591 total

NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
scripts/verify_objs_patched.py --verify-manifest: OK 1205 decomp, 3088 target objects
```

## Open handoffs

1. **`ObjPtrList<RndMesh>::Link` @`0x8227d020` vs `<CharCollide>::Link`
   @`0x823a4fa8`** (BandCharacter, 1,716 B). R-TWOADDR, 196 B vs 260 B ⇒ not a
   fold, so **one of the two names is wrong**. Note `0x8227d020` is called *from*
   `PropSync<ObjPtrList<TrackWidget>>` (§3) — a container-level PropSync would call
   `ObjPtrList<TrackWidget>::Link`, so `0x8227d020` is likely misnamed too. Settle
   by its callee set, as in §3.
2. **`Rnd::Handle` @ 6,416 B / `OvershellSlot::Handle` @ 9,276 B** — retail's callee
   is the **empty-body** survivor `0x826c3888`. Retail's `HiResScreen::TakeShot`
   and `SessionUsersProvider::ToggleMuteStatus` are *empty in retail*; ours have
   bodies. Closable only by making ours empty — i.e. a deliberate judgement that
   retail compiled these out — **not** by an alias (ours are not byte-identical to
   an empty body, so no T1 evidence exists). I did **not** do this.
3. **`OvershellPanel::OnMsg<T>` (5,768 B)** — ALIAS-2 withdrew these as
   `SURVIVOR_SIZE_MISMATCH` (ours 132 B, retail's survivor 76 B). Retail calls the
   *same* address at **both** sites, so retail really did fold those handlers into
   one 76 B body. **Our three handler bodies diverge from retail's** — genuine
   source work, and the withdrawal reason is the evidence for it. ⚠ Re-check
   against the STLPORT-1 correction before trusting the size premise.
4. **Pin/unit mis-attribution (splits lane).** `?Handle@Synth@@` (`0x826fee60`) is
   scored inside **`CharMeshHide.cpp`**'s pin (`0x826FEDF8–0x826FFB58`), and
   `Synth::Find<Object>` (`0x826fe3a8`) inside **`Sfx.cpp`**'s. The brief's
   suspicion was correct. It is not score-affecting (rows pair by name) but it
   makes unit attribution misleading.
5. **`tools/crossing_worklist.py` truncates the symbol column.** Row 12 was briefed
   as `?SelectNode@MusicLibrary@@QAAXPAVSortNode@@PAVLocalBandUser@@@Z`; the real
   name ends **`@@_N@Z`**. objdiff answers *"Symbol not found in target"*, which
   reads like a phantom row rather than a truncated copy. Print full names, or
   have consumers resolve against `report.json`.
6. **Not attempted:** the 5 UNDECIDED pairs, and rows whose `mm>1` means one fix
   cannot cross them (`Rnd::PreInit`, `Instance::SyncDir`, `DataFile::ParseNode` —
   the last also carries an independent `ARITH_COMMUTE` charge, a class measured
   PROVEN INERT, so that row's 2,432 B is unreachable by symbol work at all).
