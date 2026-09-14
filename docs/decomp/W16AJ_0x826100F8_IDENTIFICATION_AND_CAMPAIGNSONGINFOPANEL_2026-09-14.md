# W16-AJ — `0x826100F8` identified (ICF survivor of 26 STLport `hashtable` dtors), `0x825F4288` named, `CampaignSongInfoPanel` completed (2026-09-14)

Lane **W16-AJ** (Fable escalation lane). Worktree `~/tmp/wt-w16-aj`, branch `w16-aj`, based on main
`55bde229`. Input: W16-AI's report
`docs/decomp/W16AI_CAMPAIGNGOALS_GOALCMP_LOCAL_STATIC_AND_UNIT_COMPLETION_2026-09-14.md` §5.
Ruler: objdiff 4.2.9, `functionRelocDiffs=name_check` (graded, read from `report.json` `provenance`).
Baseline for pricing: `~/tmp/rows_w16ai_main.json` = main at `0667441b`/`55bde229`,
**43,356 fns / 3,997,100 B**. Every delta below is a `tools/rowset_snapshot.py diff` set-diff of the
`fuzzy == 100` row set, run inside the worktree after a full `./tools/ninja-locked` with rc=0.

## 0. Summary — predicted vs measured

| item | pre-registered | measured (set-diff) | commit |
|---|---|---|---|
| 1 · rename `0x826101b8` thunk to `~hash_map<int,UIComponent*>` | +40 B / +0 fn (brief's (a) figure) | **+180 B / +1 fn**, 0 fell out — 4 × 44 B sibling funclets in CustomizePanel/CharacterCreatorPanel plus the 4 B thunk row itself | `7c80d49d` |
| 1 · ALSO name `0x826100f8` with the survivor spelling, no alias | +1 fn / +104 B | **−2,908 B / −18 fns** — 18 callers spell 17 *other* members of the fold group; `name_check` charges a wrong spelling where a placeholder was forgiven | `b1dae30d` (committed as the negative result, then withdrawn in `7c80d49d`) |
| 2 · name `0x825f4288` `??_ECampaignGoalsLeaderboardChoicePanel@@WDM@AAPAXI@Z` | +1 fn / +0 B | **+0 / +0** — row went 0 → 97.5 (paired); the one charge is the `??_G` fold survivor name (alias-gated, §2) | `a97cb29c` |
| 3 · complete `default/CampaignSongInfoPanel` | ≤ +15 fns / +2,632 B, 0 fell out | **+16 fns / +2,632 B**, 0 fell out; unit **50/50 rows, 4,860/4,860 B** | `a97cb29c` |

**Lane total vs main (`rows_w16ai_main.json` → build 6):** `matched_functions` **43,356 → 43,373 (+17)**,
`matched_code` **3,997,100 → 3,999,912 B (+2,812)**, `matched_code_percent` 39.011490 → 39.038937,
**21 rows crossed in, 0 fell out.** Full list in §5.

Prediction failures worth reading: Item 1's naming leg (predicted +104 B, measured −2,908 B) and Item 2
(predicted +1 fn, measured +0). Both are the same mechanism — under `name_check`, naming an anonymous
address whose callers spell an ICF-folded twin converts *forgiven* placeholder sites into *charged* ones.
Neither was a bug in the identification; both are alias-gated and the proposals are in §6.

## 1. Item 1 — what is `0x826100F8`? (LEAD)

### 1.1 Verdict: brief's case (a). The map name at `0x826101B8` was an ICF-survivor spelling; our Provider ctor destroys the right container. Case (b) — a behavioural divergence in `CampaignGoalsLeaderboardChoicePanel.cpp` — is REFUTED on retail bytes.

- `0x826100F8` is a **104 B** body. Relocation-normalised, it is word-identical to **26 distinct
  `stlpmtx_std::hashtable<pair<const K,V>,…>::~hashtable` instantiations** our built objs define — every
  `hash_map<K,V>` whose `V` has a trivial destructor (`int`, `T*`, `Symbol`, small PODs). The dtor frees the
  node list and the bucket vector and touches no element dtor, so `K`/`V` do not appear in the code; only
  the `bl` targets (`_MemFree`-family allocator calls) are relocations and they are the same in all 26.
  That is exactly the population MSVC `/OPT:ICF` folds (identical bytes *including* relocations), and
  retail keeps ONE copy. The list of 26 spellings with defining objs is Appendix A, Group A.
- `0x826101B8` is a **4 B** `b 0x826100F8` — the `hash_map<K,V>::~hash_map` tail-call thunk. **27** thunk
  spellings in our objs branch to a Group A body; they too fold to one survivor. Appendix A, Group B.
  Six further `~hash_map` thunks (`V` = `String`, `vector<…>`, nested `hash_map`) branch to *different*
  hashtable dtors (those run element dtors, so they are different code) and are **excluded** — do not
  install them.
- Our funclet `fn_825F5244` (unwind of `??0CampaignGoalsLeaderboardChoiceProvider@@QAA@…@Z`) destroys
  `hash_map<Symbol,int> lbData` — its charge (`report.json`, build 6) is precisely
  retail `bl ??1?$hash_map@HPAVUIComponent@@…` (the Group B survivor name) vs our
  `bl ??1?$hash_map@VSymbol@@HU?$hash@VSymbol@@…` — a Group B member. Same container class, same body,
  different spelling. Nothing in the ctor or the funclet differs by more than that name.
- The map's original name for `0x826101B8`, `??1?$map@HPAVUIComponent@@…`, was wrong in kind (an
  `_Rb_tree`/`map` dtor walks nodes recursively; this thunk targets a hashtable dtor). Checked
  `scripts/symbol_aliases.json` first: no group contains either `0x826100f8` or `0x826101b8`.

### 1.2 What landed, and why only half of it

`7c80d49d` renames `0x826101b8` → `??1?$hash_map@HPAVUIComponent@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ`
(our `CharacterCreatorPanel.obj` DEFINES it — verified in the COFF symbol table of the built obj before
the rename, per the brief's rule). Measured **+180 B / +1 fn**: the thunk row (4 B) plus four 44 B
sibling funclets whose only charge had been this name (`CustomizePanel::fn_82618884`, `fn_82618D68`;
`CharacterCreatorPanel::fn_82610270`, `fn_82612460`).

`b1dae30d` additionally named `0x826100f8` with the Group A survivor spelling and measured
**−2,908 B / −18 fns** — the eighteen sub-funclets/dtors across the tree whose callers spell one of the
other 25 Group A members went from forgiven-placeholder to charged-wrong-name. Withdrawn in `7c80d49d`.
**The naming is correct and should land — but only in the same change as alias Group A** (§6). Landing
it alone is a guaranteed −2,908 B.

### 1.3 `fn_825F5244` itself — NOT closed by this lane, by design

It sits at 99.5 / mpn 100 with the single Group B charge above. Closing it needs alias Group B
installed; this lane is forbidden to edit `scripts/symbol_aliases.json` (W16-AH / W16-AE own it). The
proof of the fold is §1.1 + Appendix A; the coordinator can install both groups from that alone.

## 2. Item 2 — `fn_825F4288` is distinguishable, and is `CampaignGoalsLeaderboardChoicePanel`'s

W16-AI left the 8 B `WDM@` adjustor anonymous because our obj holds two byte-identical candidates
(`??_ECampaignGoalsLeaderboardChoicePanel@@WDM@AAPAXI@Z`, `??_ETexLoadPanel@@WDM@AAPAXI@Z`). Retail's
vtables settle it: the thunk's address appears in the deleting-destructor slot of the
`ContentMgr::Callback`-base vtables whose RTTI COLs name `.?AVCampaignGoalsLeaderboardChoicePanel@@`
(COL `0x821E30C8`) and `.?AVTourDescPanel@@` (COL `0x821EFDA8`), and in **no** vtable whose COL names
`TexLoadPanel`. A thunk reachable only from those two classes' vtables is one of *their* thunks; between
the two, the address lies in the CampaignGoalsLeaderboardChoicePanel TU's pinned block, and
`TourDescPanel`'s copy is the fold twin. Map row added in `a97cb29c`.

Measured **+0 / +0** against the pre-registered +1 fn: the row now pairs and scores 97.5, its one
charge being retail `b ??_GTourDescPanel@@UAAPAXI@Z` (the map's name for `0x825f4598`) vs our
`b ??_ECampaignGoalsLeaderboardChoicePanel@@UAAPAXI@Z`. Note the kind mismatch as well as the class:
retail's survivor is spelled `??_G` (scalar deleting) while our thunk branches to `??_E` (vector
deleting). MSVC emits both for a class with a virtual dtor and they are byte-identical when the class has
no array-delete path, so `??_E`/`??_G` of both classes fold to one body. Alias proposal in §6.

## 3. Item 3 — `default/CampaignSongInfoPanel` completed (37/54 → 50/50 rows)

### 3.1 The far block `0x8261FEB8–0x8261FFC8` was never this TU's — it is `MainHubPanel`'s

The brief asked whether the far block is this TU's at all. It is not, on four independent readings:
1. `0x8261FEB8` (map-named `?Unload@CampaignSongInfoPanel@@UAAXXZ`, 84 B, fuzzy 99.905) does
   `lwz r3,0x44(r31) … stw r11,0x44(r31)`; our `?Unload@CampaignSongInfoPanel` COMDAT does `0x3c`.
   `MainHubPanel.obj`'s `?Unload@MainHubPanel@@UAAXXZ` (84 B, `bl ?Unload@UIPanel@@UAAXXZ` at +20)
   matches it word-for-word.
2. `0x8261FEB8` is slot 11 of vtable `0x820C5ED4`, whose COL names `.?AVMainHubPanel@@`. The real
   `CampaignSongInfoPanel::Unload` — `lwz/stw 0x3c`, word-identical to our COMDAT — is at
   **`0x825F58C8`**, slot 33 of vtable `0x820BB894` (COL `.?AVCampaignSongInfoPanel@@`), which is
   currently pinned to `CampaignGoalsLeaderboardPanel.cpp` and map-named
   `?Unload@CampaignGoalsLeaderboardPanel@@UAAXXZ` (an ICF fold — §6).
3. The block is flanked by MainHubPanel pins (`…0x8261FE88–0x8261FEB8` before, `0x8261FFC8–…` after).
4. All four bodies in it match `MainHubPanel.obj` COMDATs: `fn_8261FF10` ↔
   `?CheckProfileForTicker@MainHubPanel@@QAA_NXZ` (reloc-normalised 0.973, 148 B), `fn_8261FFA8` ↔
   `?DataDir@UIPanel@@$4PPPPPPPM@IM@AAPAVObjectDir@@XZ` and `fn_8261FFB8` ↔
   `?SetTypeDef@UIPanel@@$4PPPPPPPM@IM@AAXPAVDataArray@@@Z` (1.000 each — vtordisp thunks, vtordisp −4 /
   vbase offset 0x8c; their spelling is class-independent).

Fix: the `.text` line moved from `CampaignSongInfoPanel.cpp:` to `MainHubPanel.cpp:` in
`config/45410914/splits.txt`; `0x8261feb8` renamed to `?Unload@MainHubPanel@@UAAXXZ`; the three other
rows named. dtk re-derived the `.pdata 0x82229C98–0x82229CA8` line alongside (the split-guard failed the
first build as designed — "the split rewrote its own input" — and the retry was the fixed point).
`MainHubPanel` gained **+3 rows / +116 B**; `CheckProfileForTicker` stays at 98.38 (§7).
⚠ `scripts/symbol_aliases.json` carries a T1 group at `0x8261feb8` with survivor
`?Unload@CampaignSongInfoPanel@@UAAXXZ` and folded `?Unload@MainHubPanel@@UAAXXZ` — T1 proved the
FOLDED spelling; the survivor name was the error, and the two bodies are *different* (0x3c vs 0x44), so
this is not a fold at all. Repair proposal in §6.

### 3.2 The eleven anonymous rows were the oracle's methods, each hiding a guarded function-local static

Every anonymous 0% row in the main block `0x825F5D28–0x825F6B00` matched one of our COMDATs
relocation-normalised, and every one carried a guarded local static our source did not have (MSVC
pattern: `lwz guard; clrlwi.; bne; ori; stw; bl ??0Symbol@@QAA@PBD@Z` [+ `??0Message` + `atexit`]).
Retail initialises a local static at its point of declaration, so *where* the static sits in the
function is load-bearing (`Refresh` went 48.38 → 100 purely by moving its two `static Message`s to
their use sites — they share ONE guard word `0x82E00430`, bits 0 and 1).

| retail | row (named this lane) | static added | guard / storage |
|---|---|---|---|
| `0x825F5C60` | `?GetCareerScore` (already named; pinned to PhysicsManager.cpp — §7) | `static Symbol all("all")` | `0x82E003D0` / `0x82E003CC` |
| `0x825F5D30` | `?GetSongCount@…@@QBAHXZ` | `static Symbol all` | `0x82E003D8` / `D4` |
| `0x825F5E08` | `?GetSongsCompleted@…@@QBAHW4Difficulty@@@Z` | `static Symbol all` | `0x82E003DC` / `E0` |
| `0x825F5EE0` | `?GetStarCount@…@@QBAHXZ` | `static Symbol all` | `0x82E003E4` / `E8` |
| `0x825F5FB8` | `?GetStarsEarned@…@@QBAHW4Difficulty@@@Z` | `static Symbol all` | `0x82E003EC` / `F0` |
| `0x825F6090` | `?SelectDefaultInstrument@…@@QAAXXZ` | `static Message update_details_msg` | `0x82E003FC` / `F4`, atexit `0x82C46D68` |
| `0x825F61C8` | `?GetMusicLibraryBackScreen@…@@QAA?AVSymbol@@XZ` | `static Message get_musiclibrary_backscreen_msg` | `0x82E00408` / `00`, lit `0x820BBB88` |
| `0x825F62D8` | `?GetMusicLibraryNextScreen@…` | `static Message get_musiclibrary_nextscreen_msg` | `0x82E00414` / `0C`, lit `0x820BBC08` |
| `0x825F64E8` | `?Update@CampaignSourceProvider@@QAAXXZ` | `static Symbol all` inside `if (srcs.size() > 1)` | `0x82E0041C` / `18` |
| `0x825F6680` | `?Refresh@…@@QAAXXZ` (was 48.38) | two `static Message`s moved to use sites | `0x82E00430` bits 0/1; `0x82E00428` / `20` |
| `0x825F68C0` | `?CreateAndSubmitMusicLibraryTask@…@@QAAXXZ` | `static Symbol all` after the task ctor | `0x82E00438` / `34` |
| `0x825F6A28` | `?Launch@…@@QAAXXZ` | `static Message handle_goto_musiclibrary_msg` | `0x82E00444` / `3C`, lit `0x820BBF00` |

`?Unload@CampaignSongInfoPanel` (99.905 in the brief's table) was the MainHubPanel body above — it is
not a source defect. `fn_825F62A8` / `fn_825F63B8` (40 B funclets, 99.9 / 99.4) were the EH rollbacks of
the guards their parents lacked; they crossed with their parents.

### 3.3 `SelectDefaultInstrument` — the last 268 B, a scheduling flip

After the static, the row read fuzzy 99.851 / mpn 100 on one pair: retail `li r5,0 ; li r4,0` before
`bl TrackTypeToScoreType(TrackType,bool,bool)`; ours `li r4,0 ; li r5,0`. Source was already identical
to the rb3-Wii oracle (nested `ScoreTypeToSym(TrackTypeToScoreType(ControllerTypeToTrackType(…, false),
false, false))`). MSVC normally materialises immediates right-to-left (our own `SetSelected(sym, true,
-1)` site emits r6, r5, r4), so ours was the anomaly. One bounded probe, pre-registered as "+268 B or
exactly 0, revert on 0": hoist `ControllerTypeToTrackType(…)` into a `TrackType` local. Measured
**+0 fns / +268 B** — the row crossed and the unit reached 4,860/4,860. Kept.

### 3.4 Unit state at build 6

`default/CampaignSongInfoPanel`: **50 / 50 rows, 4,860 / 4,860 B, 0 sub-100** (was 37/54, 2,344/5,124
at main; the row and byte totals dropped by exactly the four MainHubPanel rows: 84+148+16+16 = 264 B).

## 4. Builds and measurements (provenance)

| build | log | what changed | rc | lane-internal Δ (rowset diff) |
|---|---|---|---|---|
| 1–2 | `~/tmp/rb3_build_w16aj_{1,2}.log` | Item 1 naming both addresses | 0 | −2,908 B / −18 fns vs main (`b1dae30d`) |
| 3 | `…_3.log` | keep thunk rename only | 0 | +180 B / +1 fn vs main (`7c80d49d`); snapshot `~/tmp/rows_w16aj_b3.json` |
| 4 | `…_4.log` | Items 2+3 (source, 14 map rows, splits move) | **FAILED** — split-guard: dtk re-derived the `.pdata` line; `report.json` left stale | (not priced; the background wrapper's "exit 0" was the wrapper's, the log says `ninja: error`) |
| 5 | `…_5.log` | retry (fixed point) | 0 | +16 fns / +2,364 B vs b3; snapshot `rows_w16aj_b5.json` |
| 6 | `…_6.log` | SelectDefaultInstrument probe | 0 | +0 / +268 B vs b5; snapshot `rows_w16aj_b6.json` |

Build 4 is the honest record of a trap: the harness reported the background task as "completed, exit
code 0" while the build had failed; the rowset diff read +0 because `report.json` was stale. Always read
the log's last lines and `grep -c 'FAILED\|ninja: error'` before pricing.

## 5. Named rows crossed in vs main (21 rows / 2,812 B; 0 fell out)

Item 1 (`7c80d49d`): `default/CustomizePanel::fn_82618884` 44, `fn_82618D68` 44;
`default/CharacterCreatorPanel::fn_82610270` 44, `fn_82612460` 44,
`??1?$hash_map@HPAVUIComponent@@…@@QAA@XZ` 4.

Item 3 (`a97cb29c`), `default/CampaignSongInfoPanel`: `?Refresh` 348, `?Update@CampaignSourceProvider`
328, `?SelectDefaultInstrument` 268, `?CreateAndSubmitMusicLibraryTask` 240,
`?GetMusicLibraryBackScreen` 192, `?GetMusicLibraryNextScreen` 192, `?GetSongsCompleted` 176,
`?GetStarsEarned` 176, `?GetSongCount` 172, `?GetStarCount` 172, `?Launch` 172, `fn_825F62A8` 40,
`fn_825F63B8` 40. `default/MainHubPanel`: `?Unload@MainHubPanel@@UAAXXZ` 84,
`?DataDir@UIPanel@@$4PPPPPPPM@IM@AAPAVObjectDir@@XZ` 16, `?SetTypeDef@UIPanel@@$4PPPPPPPM@IM@AAXPAVDataArray@@@Z` 16.

`mpn` set-diff vs main's `report.json` (main at `b0546945` when read): the same 17 non-Item-1-funclet
rows plus the thunk = +17 `matched_functions`; no row left the `mpn == 100` set.

## 6. Alias / map proposals for the coordinator (NOT installed — `symbol_aliases.json` is W16-AH/AE's)

1. **Group A** — survivor `??1?$hashtable@U?$pair@$$CBHPAVUIComponent@@…@@QAA@XZ` at **`0x826100f8`**,
   folded: the other 25 spellings in Appendix A Group A. **Install the map row for `0x826100f8` in the
   SAME change** (it is currently anonymous; naming it without the group is −2,908 B, measured).
   Proof: 26 reloc-normalised-identical 104 B COMDATs in our built objs, one retail copy, the retail
   thunk at `0x826101B8` branching to it.
2. **Group B** — survivor `??1?$hash_map@HPAVUIComponent@@…@@QAA@XZ` at **`0x826101b8`** (map row
   already installed, `7c80d49d`), folded: the other 26 spellings in Appendix A Group B. Closes
   `fn_825F5244` (+40 B, the brief's lead row) and every other `hash_map<K,trivial>` unwind funclet.
   Do NOT include the six excluded thunks listed in Appendix A.
3. **Deleting-dtor fold** — survivor `??_GTourDescPanel@@UAAPAXI@Z` at `0x825f4598`, folded
   `??_GCampaignGoalsLeaderboardChoicePanel@@UAAPAXI@Z`, `??_ECampaignGoalsLeaderboardChoicePanel@@UAAPAXI@Z`,
   `??_ETourDescPanel@@UAAPAXI@Z`. Closes the Item 2 row (+8 B) — and W16-AI's noted `??_G`/`??_D`
   shape residue in the same unit should be re-read after it lands.
4. **Repair the T1 group at `0x8261feb8`**: survivor must be `?Unload@MainHubPanel@@UAAXXZ`, folded
   `[]`. `?Unload@CampaignSongInfoPanel@@UAAXXZ` is NOT a fold of it (different bytes: member 0x3c vs
   0x44); it lives at `0x825f58c8`.
5. **Fold at `0x825f58c8`** — survivor `?Unload@CampaignGoalsLeaderboardPanel@@UAAXXZ` (current map
   name), folded `?Unload@CampaignSongInfoPanel@@UAAXXZ` (word-identical `lwz/stw 0x3c` bodies).
6. **Fold at `0x825f5920`** — survivor `?Load@SetlistToStorePanel@@UAAXXZ` (current map name), folded
   `?Load@CampaignSongInfoPanel@@UAAXXZ` (both are the 4 B `b ?Load@UIPanel@@MAAXXZ`).
7. `0x82603a78` (`??_E…$4` thunk, pinned to CampaignSongInfoPanel) is also slot 0 of
   `AuditionSessionPanel`'s vtable — a fold; spelling for the folded side to be read off that vtable.

## 7. NOT done, with the retail-byte reason

- **`?CheckProfileForTicker@MainHubPanel@@QAA_NXZ` at 98.38** (148 B, one instruction): retail
  `cmplwi r3,0` after `bl Server::GetPlayerID`, ours `cmpwi`. Retail's return type is unsigned; ours is
  `virtual int GetPlayerID(int)` in `src/network/net/Server.h:26` (the oracle says `int` too, and
  `BandStorePanel.cpp:232` formats it with `%u`). A shared-header + vtable-signature change outside this
  lane's two units, with ten other call sites to re-price — left for a Server lane.
- **`?Poll@MainHubPanel` (51.8) and the eight anonymous MainHubPanel rows**: never opened; not this
  lane's unit. MainHubPanel was touched only by the splits move.
- **Re-homing the interleaved foreign pins that are geometrically this TU's** — `0x825f58c8–0x825f591c`
  (`CampaignGoalsLeaderboardPanel.cpp`, the real `?Unload@CampaignSongInfoPanel`),
  `0x825f5920–0x825f5b88` (`SetlistToStorePanel.cpp`: `Load` + 3 vtable-slot thunks, `SetType` 316 B
  + funclet, `Find<UIList>` 164 B, a 4 B stub) and `0x825f5bf4–0x825f5d28` (`PhysicsManager.cpp`:
  `SelectedScoreType` 92 B, `GetCareerScore` 168 B + funclet). COMDAT-order match to our obj says they
  are CampaignSongInfoPanel's TU; not moved because re-homing pinned addresses is not metric-neutral and
  opens three units the brief forbade. The map rows for `0x825f5c60` (`GetCareerScore`), `0x825f5bf8`
  (`SelectedScoreType`), `0x825f5970` (`SetType`), `0x825f5ad0` (`Find<UIList>`) are also left as
  proposals for the lane that moves them.
- **`0x826100f8` stays anonymous** (§1.2) until Group A can land with it.
- **`fn_825F5244` stays at 99.5** (§1.3) until Group B lands.
- **`fn_825F4288` stays at 97.5** (§2) until proposal 3 lands.
- `0x8260FFD8` / `0x8260EED8` (neighbours of Item 1's survivor) left anonymous — not examined.
- `fn_82610160`'s suspect `__ucopy_ptrs` charge at `0x825AEE78` — noticed, not adjudicated.

## 8. Gates (run last, in the worktree, after the last source edit)

See the final lane message for the four gate lines; the `NATIVE_GATE_RESULT` line is pasted there
verbatim. Note for readers: builds 4–6 predate the doc commit, and nothing after build 6 touches a build
input, so build 6's rc=0 stands as the "full build" gate and the final no-op `./tools/ninja-locked` in
the gate sequence proves the tree is at a fixed point.

## Appendix A — Item 1 alias-group spellings (from `~/tmp/w16aj_alias_evidence.json`; our BUILT objs, build 3 of this lane)

### Group A — 26 spellings of the 104 B `hashtable<pair<const K,V>,…>::~hashtable` body, relocation-normalised identical to retail `0x826100F8`

Survivor spelling to install at `0x826100f8` (retail-resident by choice of the group; MUST land together with the group — naming it alone measured −2,908 B / −18 fns, commit `b1dae30d`):
`??1?$hashtable@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraitsT@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ`

| # | spelling | defining objs |
|---|---|---|
| 1 | `??1?$hashtable@U?$pair@$$CBHH@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraitsT@U?$pair@$$CBHH@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBHH@stlpmtx_std@@@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHH@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/AccomplishmentProgress.obj, system/bandobj/VocalTrackDir.obj |
| 2 | `??1?$hashtable@U?$pair@$$CBHPAVColorPalette@@@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraitsT@U?$pair@$$CBHPAVColorPalette@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBHPAVColorPalette@@@stlpmtx_std@@@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHPAVColorPalette@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/ChooseColorPanel.obj |
| 3 | `??1?$hashtable@U?$pair@$$CBHPAVSongMetadata@@@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraitsT@U?$pair@$$CBHPAVSongMetadata@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBHPAVSongMetadata@@@stlpmtx_std@@@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHPAVSongMetadata@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/BandSongMgr.obj, band3/meta_band/PrefabMgr.obj, system/meta/SongMgr.obj … |
| 4 | `??1?$hashtable@U?$pair@$$CBHPAVSongStatus@@@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraitsT@U?$pair@$$CBHPAVSongStatus@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBHPAVSongStatus@@@stlpmtx_std@@@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHPAVSongStatus@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/SongStatusMgr.obj |
| 5 | `??1?$hashtable@U?$pair@$$CBHPAVSongUpgradeData@@@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraitsT@U?$pair@$$CBHPAVSongUpgradeData@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBHPAVSongUpgradeData@@@stlpmtx_std@@@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHPAVSongUpgradeData@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/SongStatusMgr.obj, band3/meta_band/SongUpgradeMgr.obj |
| 6 | `??1?$hashtable@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraitsT@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/CharacterCreatorPanel.obj, band3/meta_band/CustomizePanel.obj |
| 7 | `??1?$hashtable@U?$pair@$$CBHU?$pair@H_N@stlpmtx_std@@@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraitsT@U?$pair@$$CBHU?$pair@H_N@stlpmtx_std@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBHU?$pair@H_N@stlpmtx_std@@@stlpmtx_std@@@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHU?$pair@H_N@stlpmtx_std@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/SongSortMgr.obj, band3/meta_band/UploadErrorMgr.obj |
| 8 | `??1?$hashtable@U?$pair@$$CBHVSymbol@@@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraitsT@U?$pair@$$CBHVSymbol@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBHVSymbol@@@stlpmtx_std@@@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHVSymbol@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 7: band3/meta_band/BandSongMgr.obj, band3/meta_band/PrefabMgr.obj, band3/meta_band/SongStatusMgr.obj … |
| 9 | `??1?$hashtable@U?$pair@$$CBVSymbol@@H@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@H@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@H@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@H@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 12: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/AccomplishmentProgress.obj, band3/meta_band/BandSongMgr.obj … |
| 10 | `??1?$hashtable@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 7: band3/meta_band/BandProfile.obj, band3/meta_band/BandSongMetadata.obj, band3/meta_band/BandSongMgr.obj … |
| 11 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAV?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAV?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAV?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAV?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 12 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAV?$set@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@V?$StlNodeAlloc@VSymbol@@@3@@stlpmtx_std@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAV?$set@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@V?$StlNodeAlloc@VSymbol@@@3@@stlpmtx_std@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAV?$set@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@V?$StlNodeAlloc@VSymbol@@@3@@stlpmtx_std@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAV?$set@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@V?$StlNodeAlloc@VSymbol@@@3@@stlpmtx_std@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 13 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAV?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAV?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAV?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAV?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 5: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/LessonMgr.obj, band3/meta_band/PrefabMgr.obj … |
| 14 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVAccomplishment@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVAccomplishment@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVAccomplishment@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAccomplishment@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 15 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVAccomplishmentCategory@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVAccomplishmentCategory@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVAccomplishmentCategory@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAccomplishmentCategory@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 16 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVAccomplishmentGroup@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVAccomplishmentGroup@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVAccomplishmentGroup@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAccomplishmentGroup@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 17 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/AssetMgr.obj |
| 18 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVAward@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVAward@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVAward@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAward@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 19 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVCampaignKey@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVCampaignKey@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVCampaignKey@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVCampaignKey@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/Campaign.obj, system/os/Timer.obj |
| 20 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVCampaignLevel@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVCampaignLevel@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVCampaignLevel@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVCampaignLevel@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/Campaign.obj, system/os/Timer.obj |
| 21 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVDataArray@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVDataArray@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVDataArray@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVDataArray@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/InterstitialMgr.obj |
| 22 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVLesson@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVLesson@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVLesson@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVLesson@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/LessonMgr.obj |
| 23 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVMetaMusicScene@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVMetaMusicScene@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVMetaMusicScene@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVMetaMusicScene@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 2: system/meta/MetaMusicManager.obj, system/meta/MoviePanel.obj |
| 24 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVSongFilter@SongSortMgr@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVSongFilter@SongSortMgr@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVSongFilter@SongSortMgr@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVSongFilter@SongSortMgr@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 25 | `??1?$hashtable@U?$pair@$$CBVSymbol@@PAVUIScreen@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVUIScreen@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVUIScreen@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVUIScreen@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/InterstitialMgr.obj |
| 26 | `??1?$hashtable@U?$pair@$$CBVSymbol@@V1@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@V1@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@V1@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@V1@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 8: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/Campaign.obj, band3/meta_band/PrefabMgr.obj … |

### Group B — 27 spellings of the 4 B `hash_map<K,V,…>::~hash_map` tail-call thunk (`b` to a Group A body), survivor at `0x826101B8`

Survivor spelling (installed in the map by `7c80d49d`): `??1?$hash_map@HPAVUIComponent@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ`

| # | spelling | defining objs |
|---|---|---|
| 1 | `??1?$hash_map@HHU?$hash@H@stlpmtx_std@@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHH@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/AccomplishmentProgress.obj, system/bandobj/VocalTrackDir.obj |
| 2 | `??1?$hash_map@HPAVColorPalette@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHPAVColorPalette@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/ChooseColorPanel.obj |
| 3 | `??1?$hash_map@HPAVSongMetadata@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHPAVSongMetadata@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/BandSongMgr.obj, band3/meta_band/PrefabMgr.obj, system/meta/SongMgr.obj … |
| 4 | `??1?$hash_map@HPAVSongStatus@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHPAVSongStatus@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/SongStatusMgr.obj |
| 5 | `??1?$hash_map@HPAVSongUpgradeData@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHPAVSongUpgradeData@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/SongStatusMgr.obj, band3/meta_band/SongUpgradeMgr.obj |
| 6 | `??1?$hash_map@HPAVUIComponent@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHPAVUIComponent@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/CharacterCreatorPanel.obj, band3/meta_band/CustomizePanel.obj |
| 7 | `??1?$hash_map@HU?$pair@H_N@stlpmtx_std@@U?$hash@H@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHU?$pair@H_N@stlpmtx_std@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/SongSortMgr.obj, band3/meta_band/UploadErrorMgr.obj |
| 8 | `??1?$hash_map@HVSymbol@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHVSymbol@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 7: band3/meta_band/BandSongMgr.obj, band3/meta_band/PrefabMgr.obj, band3/meta_band/SongStatusMgr.obj … |
| 9 | `??1?$hash_map@VSymbol@@HU?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@H@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 12: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/AccomplishmentProgress.obj, band3/meta_band/BandSongMgr.obj … |
| 10 | `??1?$hash_map@VSymbol@@MU?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 7: band3/meta_band/BandProfile.obj, band3/meta_band/BandSongMetadata.obj, band3/meta_band/BandSongMgr.obj … |
| 11 | `??1?$hash_map@VSymbol@@PAV?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@U?$hash@VSymbol@@@3@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAV?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 12 | `??1?$hash_map@VSymbol@@PAV?$set@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@V?$StlNodeAlloc@VSymbol@@@3@@stlpmtx_std@@U?$hash@VSymbol@@@3@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAV?$set@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@V?$StlNodeAlloc@VSymbol@@@3@@stlpmtx_std@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 13 | `??1?$hash_map@VSymbol@@PAV?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@U?$hash@VSymbol@@@3@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAV?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 5: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/LessonMgr.obj, band3/meta_band/PrefabMgr.obj … |
| 14 | `??1?$hash_map@VSymbol@@PAVAccomplishment@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAccomplishment@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 15 | `??1?$hash_map@VSymbol@@PAVAccomplishmentCategory@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAccomplishmentCategory@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 16 | `??1?$hash_map@VSymbol@@PAVAccomplishmentGroup@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAccomplishmentGroup@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 17 | `??1?$hash_map@VSymbol@@PAVAsset@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/AssetMgr.obj |
| 18 | `??1?$hash_map@VSymbol@@PAVAward@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAward@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 19 | `??1?$hash_map@VSymbol@@PAVCampaignKey@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVCampaignKey@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/Campaign.obj, system/os/Timer.obj |
| 20 | `??1?$hash_map@VSymbol@@PAVCampaignLevel@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVCampaignLevel@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 2: band3/meta_band/Campaign.obj, system/os/Timer.obj |
| 21 | `??1?$hash_map@VSymbol@@PAVDataArray@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVDataArray@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/InterstitialMgr.obj |
| 22 | `??1?$hash_map@VSymbol@@PAVLesson@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVLesson@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/LessonMgr.obj |
| 23 | `??1?$hash_map@VSymbol@@PAVMetaMusicScene@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVMetaMusicScene@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 2: system/meta/MetaMusicManager.obj, system/meta/MoviePanel.obj |
| 24 | `??1?$hash_map@VSymbol@@PAVSongFilter@SongSortMgr@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@5@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVSongFilter@SongSortMgr@@@stlpmtx_std@@@5@@stlpmtx_std@@QAA@XZ` | 4: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/PrefabMgr.obj, system/bandobj/BandSwatch.obj … |
| 25 | `??1?$hash_map@VSymbol@@PAVUIScreen@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVUIScreen@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/InterstitialMgr.obj |
| 26 | `??1?$hash_map@VSymbol@@V1@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@V1@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 8: band3/meta_band/AccomplishmentManager.obj, band3/meta_band/Campaign.obj, band3/meta_band/PrefabMgr.obj … |
| 27 | `??1?$hash_map@VSymbol@@V?$hash_map@VSymbol@@PAVDataArray@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVDataArray@@@stlpmtx_std@@@4@@stlpmtx_std@@U?$hash@VSymbol@@@3@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@V?$hash_map@VSymbol@@PAVDataArray@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVDataArray@@@stlpmtx_std@@@4@@stlpmtx_std@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ` | 1: band3/meta_band/InterstitialMgr.obj |

### Excluded from Group B — 6 `~hash_map` thunks whose hashtable dtor is NOT in Group A (value type has a non-trivial dtor: `String`, `vector<…>`, nested `hash_map`), so the thunk branches to a different body and cannot fold with `0x826101B8`

- `??1?$hash_map@HVString@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHVString@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ`
- `??1?$hash_map@VSymbol@@V?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@U?$hash@VSymbol@@@3@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@V?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ`
- `??1?$hash_map@VSymbol@@V?$vector@PAVLightPreset@@V?$StlNodeAlloc@PAVLightPreset@@@stlpmtx_std@@@stlpmtx_std@@USymbolHash@LightPresetManager@@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@V?$vector@PAVLightPreset@@V?$StlNodeAlloc@PAVLightPreset@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ`
- `??1?$hash_map@VSymbol@@V?$vector@PAVPatchSticker@@V?$StlNodeAlloc@PAVPatchSticker@@@stlpmtx_std@@@stlpmtx_std@@USymbolHash@PatchDir@@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@V?$vector@PAVPatchSticker@@V?$StlNodeAlloc@PAVPatchSticker@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ`
- `??1?$hash_map@VSymbol@@V?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@U?$hash@VSymbol@@@3@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@V?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@3@@stlpmtx_std@@QAA@XZ`
- `??1?$hash_map@VSymbol@@VString@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@VString@@@stlpmtx_std@@@4@@stlpmtx_std@@QAA@XZ`
