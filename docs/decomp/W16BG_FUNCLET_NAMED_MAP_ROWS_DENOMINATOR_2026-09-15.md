# W16-BG — the 24 funclet-named map rows that objdiff drops from BOTH halves of the ruler

Lane **W16-BG**, 2026-09-15. Worktree `~/tmp/wt-w16-bg`, branch `w16-bg`, based on main
`a5a974aa` (dispatch); baseline measures from `81c55a92`, a roadmap-only diff away.
Scope: **map rows only**, at exactly 24 addresses. No `splits.txt` line, no alias group, no
`src/` file was touched (W16-BH and W16-BI own adjacent surfaces).

Adjudicating W16-BF §5.1's flagged hazard: `scripts/target_symbol_map.json` carries 24 rows
whose value is an MSVC EH-funclet name (21 `__unwind$NNNNNN`, 3 `__catch$NNNNNN`), introduced by
`783ebf34` (laneAP, 2026-07-26, "un-gated byte-twin identification -> +157"). Because objdiff
flags such COFF names `Hidden`, those 24 retail functions appear in **neither the numerator nor
the denominator** of `report.json`, while the other ~26,000 retail funclets keep their `fn_`
placeholder names, stay in `total_functions`/`total_code`, and pair by byte signature.

---

## 0. Briefed figures, tested literally before use

| briefed | measured here | verdict |
|---|---|---|
| 24 rows with a funclet-name value (21 unwind / 3 catch) | **24** (21 / 3) | confirmed |
| `fn_822FD26C` size `0x44`, `fn_824B1684` size `0x2C` | 68 B, 44 B | confirmed |
| baseline 43,525 / 4,038,804 B / 39.4183 % | `matched_functions` **43525**, `matched_code` **4038804**, `matched_code_percent` **39.418335** | confirmed |
| baseline `total_code` 10,246,004 / `total_functions` 69,216 / `masked_equal` 23,053 | **10246004** / **69216** / **23053** | confirmed |
| `read.rs` ~line 150 / `report.rs` ~line 1225 | read.rs **149–162**, report.rs **1225** | confirmed (exact lines below) |

Reader self-validation (the house rule that a census must reproduce a known figure): my
FuncInfo scanner finds **8,541** `0x19930522` magic hits in `orig/45410914/band.exe` — exactly
the count CLAUDE.md records from an independent measurement. Renamer liveness, the
reflinked-worktree trap: the built target objs carry **81,047** mangled (`?…`) names, far above
the brief's 27,000 floor, so every "absent" below is a real absence.

---

## 1. The pairing mechanism — why `fn_`-named funclets pair but these 24 vanish

**In one paragraph, with file:line.** `objdiff-core/src/obj/read.rs:147-162` sets
`SymbolFlag::Hidden` on any **COFF** symbol whose name starts with `except_data_`, `__unwind`,
`__catch` or `__comdat_gap` — a name-and-format test, so it fires on *both* sides of the diff.
Two consumers read that flag, and they read it for opposite purposes. Row emission:
`objdiff-cli/src/cmd/report.rs:1138` binds `let obj = target.as_ref().or(base.as_ref())`, i.e.
**the report iterates the TARGET object's symbol list**, and `report.rs:1225` `continue`s on
`symbol.flags.contains(SymbolFlag::Hidden)` — so a *target-side* Hidden name emits no row at all,
removing the function from `total_functions` and its bytes from `total_code` as well as from the
numerator. Pairing, by contrast, **does not consult `Hidden`**: the candidate collector inside
`pair_funclets_by_bytes` (`objdiff-core/src/diff/mod.rs:1556`, collector at **1573-1575**) skips
only `used`, `size == 0` and `SymbolFlag::Ignored`, and admits anything `is_funclet_like`
(`diff/mod.rs:880-898`) accepts — which is `__unwind$N`, `__catch$N`, `__unwind__merged_*`,
`??__E*`/`??__F*`, **and `fn_` + exactly 8 hex digits**. So the ~26,000 ordinary retail funclets
pair *target-side-`fn_`* against *base-side-`__unwind$N`* on masked bytes (passes 1–3, disclosed
as `masked_equal_symbol`), and the base side's Hidden-ness is irrelevant because no row is ever
emitted from the base object anyway. **The asymmetry is entirely target-side naming: a retail
funclet left as `fn_<VA>` is a scored row that pairs by bytes; the same funclet renamed to
`__unwind$N` by our pre-compile renamer pairs identically and is then deleted from the report.**
These 24 are the only target symbols in the tree in that state (`tgt_funclet_syms` is 1 or 2 per
affected unit, and 0 report rows tree-wide are named `__unwind`/`__catch`).

Two consequences worth stating because they shaped the decision:

1. **The score effect of un-naming is nearly pure disclosure.** Pairing already happens today
   (Hidden does not block `collect`), so the base-side funclet these 24 consume is consumed in
   both worlds. What changes is whether a row exists to be counted.
2. **But it is not *exactly* pure.** `diff/mod.rs:1611-1613` sorts both candidate lists **by
   symbol name** to make the greedy pass-2/pass-3 assignment independent of symbol-table order.
   Renaming a candidate from `__unwind$291569` to `fn_822FD26C` moves it in that sort, so inside
   an *ambiguous* (over-subscribed) signature group a different member can win — the
   "displaced funclet pairings" laneAP itself measured as its 5 losses. Collateral of ±a few rows
   in the 22 affected units is therefore expected, not anomalous.

---

## 2. Adjudication of all 24 on retail bytes

**Instrument.** All 8,541 retail `FuncInfo` structures (`magic 0x19930522`) were located by
scanning every section of `orig/45410914/band.exe` (big-endian), then each one's **UnwindMap**
(`maxState` × `{toState, action}`) and **TryBlockMap** (`nTryBlocks` × 20 B → `HandlerType` ×
16 B → `addressOfHandler`) was walked, yielding **25,784** distinct unwind-funclet targets and
**537** catch-handler targets. Parent attribution is the 8-byte retail EH prefix
(`DCD __CxxFrameHandler; DCD __ehfuncinfo$…`, handler word `0x82829530` here) located by
searching the image for the FuncInfo VA as a word: the function begins 4 bytes after it. The
`Image` class and prefix convention are reused from `tools/eh_state_screen.py` rather than
rewritten.

**Result: 24 of 24 are true EH funclets — unanimous.** Every one is referenced from a retail
FuncInfo table (21 from an UnwindMap `action` slot, 3 from a HandlerType `addressOfHandler`).
None is a mis-carved ordinary function; none required a different treatment.

| # | funclet addr | size | kind | retail EH ref | parent fn | parent map name | parent unit | funclet's PINNED unit | base obj has that `__unwind$` name? | unit funclet-pair rate | verdict |
|---:|---|---:|---|---|---|---|---|---|---|---:|---|
| 1 | `0x822fd26c` | 68 | unwind | UnwindMap `action` | `fn_822FC508` | `— (anonymous)` | `default/auto_03_822FC4F8_text` | `default/VocalTrackDir` | **no** | 50.3% | TRUE FUNCLET → un-name |
| 2 | `0x822fd2b0` | 68 | unwind | UnwindMap `action` | `fn_822FC508` | `— (anonymous)` | `default/auto_03_822FC4F8_text` | `default/VocalTrackDir` | **no** | 50.3% | TRUE FUNCLET → un-name |
| 3 | `0x82318bbc` | 44 | unwind | UnwindMap `action` | `fn_823187F0` | `— (anonymous)` | `default/RGUtl` | `default/DepthBuffer3D` | **no** | 70.7% | TRUE FUNCLET → un-name |
| 4 | `0x8232380c` | 68 | unwind | UnwindMap `action` | `fn_82323690` | `??0InstrumentDifficultyDisplay@@QAA@XZ` | `default/InstrumentDifficultyDisplay` | `default/InstrumentDifficultyDisplay` | **no** | 92.3% | TRUE FUNCLET → un-name |
| 5 | `0x82323850` | 68 | unwind | UnwindMap `action` | `fn_82323690` | `??0InstrumentDifficultyDisplay@@QAA@XZ` | `default/InstrumentDifficultyDisplay` | `default/InstrumentDifficultyDisplay` | **no** | 92.3% | TRUE FUNCLET → un-name |
| 6 | `0x8235c974` | 40 | unwind | UnwindMap `action` | `fn_8235C678` | `— (anonymous)` | `default/CharClip` | `default/CharClip` | **no** | 76.9% | TRUE FUNCLET → un-name |
| 7 | `0x82451bc0` | 32 | unwind | UnwindMap `action` | `fn_82450B60` | `?SyncProperty@RndPartLauncher@@UAA_NAAVDa…` | `default/PartLauncher` | `default/AccomplishmentPlayerConditional` | **no** | 88.0% | TRUE FUNCLET → un-name |
| 8 | `0x8246454c` | 40 | unwind | UnwindMap `action` | `fn_824644E0` | `??0DrawString@@QAA@PBDABVVector2@@ABVColo…` | `default/Graph` | `default/Graph` | **no** | 100.0% | TRUE FUNCLET → un-name |
| 9 | `0x824b1684` | 44 | catch | HandlerType `addressOfHandler` | `fn_824B14E8` | `— (anonymous)` | `default/LightPreset` | `default/LightPreset` | **no** | 88.0% | TRUE FUNCLET → un-name |
| 10 | `0x824f8244` | 44 | unwind | UnwindMap `action` | `fn_824F8208` | `??$_Copy_Construct@U?$pair@$$CBVString@@V…` | `default/RockCentral` | `default/Watcher` | **no** | 87.5% | TRUE FUNCLET → un-name |
| 11 | `0x82527b48` | 32 | unwind | UnwindMap `action` | `fn_82527AF0` | `?Type@VirtualKeyboardResultMsg@@SA?AVSymb…` | `default/VirtualKeyboard` | `default/VirtualKeyboard` | **no** | 100.0% | TRUE FUNCLET → un-name |
| 12 | `0x8253cc94` | 40 | unwind | UnwindMap `action` | `fn_8253CBD8` | `??$PropSync@VViewSettingsProvider@@@@YA_N…` | `default/MusicLibrary` | `default/MusicLibrary` | **no** | 98.1% | TRUE FUNCLET → un-name |
| 13 | `0x8263f2f0` | 40 | unwind | UnwindMap `action` | `fn_8263EDF0` | `?Poll@UGCPurchasePanel@@UAAXXZ` | `default/band3/meta_band/UGCPurchasePanel` | `default/band3/meta_band/BandProfile` | **no** | 96.2% | TRUE FUNCLET → un-name |
| 14 | `0x82675a44` | 40 | unwind | UnwindMap `action` | `fn_826759E0` | `?NewObject@FadePanel@@SAPAVObject@Hmx@@XZ` | `default/band3/game/Game` | `default/band3/game/Game` | **no** | 97.5% | TRUE FUNCLET → un-name |
| 15 | `0x82703ad0` | 40 | catch | HandlerType `addressOfHandler` | `fn_82703A68` | `— (anonymous)` | `default/system/rndobj/Utl` | `default/StandardStream` | **no** | 88.7% | TRUE FUNCLET → un-name |
| 16 | `0x82703b68` | 40 | catch | HandlerType `addressOfHandler` | `fn_82703B00` | `— (anonymous)` | `default/StandardStream` | `default/system/rndobj/Utl` | **no** | 50.6% | TRUE FUNCLET → un-name |
| 17 | `0x82709bf8` | 40 | unwind | UnwindMap `action` | `fn_82709BB0` | `?NewObject@RandomGroupSeq@@SAPAVObject@Hm…` | `default/Sequence` | `default/DepthBuffer3D` | **no** | 70.7% | TRUE FUNCLET → un-name |
| 18 | `0x8274d430` | 40 | unwind | UnwindMap `action` | `fn_8274D198` | `?Execute@DataArray@@QAA?AVDataNode@@XZ` | `default/DataArray` | `default/DataArray` | **no** | 100.0% | TRUE FUNCLET → un-name |
| 19 | `0x827799ac` | 40 | unwind | UnwindMap `action` | `fn_827796F8` | `— (anonymous)` | `default/SongData` | `default/CharLipSync` | **no** | 70.0% | TRUE FUNCLET → un-name |
| 20 | `0x827ae070` | 40 | unwind | UnwindMap `action` | `fn_827ADFF8` | `??0?$vector@UPressRec@@V?$StlNodeAlloc@UP…` | `default/HeldButtonPanel` | `default/HeldButtonPanel` | **no** | 100.0% | TRUE FUNCLET → un-name |
| 21 | `0x82b5ae14` | 40 | unwind | UnwindMap `action` | `fn_82B5ADC0` | `?NewObject@FxSendReverb360@@SAPAVObject@H…` | `default/system/synth_xbox/Synth` | `default/system/synth_xbox/Synth` | **no** | 90.8% | TRUE FUNCLET → un-name |
| 22 | `0x82b62d3c` | 40 | unwind | UnwindMap `action` | `fn_82B62CD8` | `— (anonymous)` | `default/system/synth_xbox/FxSendCompress` | `default/system/synth_xbox/FxSendCompress` | **no** | 100.0% | TRUE FUNCLET → un-name |
| 23 | `0x82b6f13c` | 44 | unwind | UnwindMap `action` | `fn_82B6F100` | `??$_Copy_Construct@V?$vector@MV?$StlNodeA…` | `default/Synapse_dsp` | `default/Synapse_dsp` | **no** | 71.9% | TRUE FUNCLET → un-name |
| 24 | `0x82b7cc24` | 32 | unwind | UnwindMap `action` | `fn_82B7CAE8` | `?SetType@TourDescPanel@@UAAXVSymbol@@@Z` | `default/TourDescPanel` | `default/TourDescPanel` | **no** | 73.3% | TRUE FUNCLET → un-name |

Notes on the table, all measured:

- **The 3 catch funclets carry their own 8-byte EH prefix**, pointing at the parent's FuncInfo —
  which is why a naive prefix scan reports two candidate "parents" per catch funclet
  (`0x82075fcc` → `0x824b14e8` *and* `0x824b1684`). Parent is the lower address.
- **`base obj has that `__unwind$` name?` is `no` for all 24.** laneAP found each name as a
  relocation-masked byte twin in *some* one of the ~1,024 compiled objects, searched globally;
  none of those twins is in the object the funclet is actually **pinned** to. So these names
  never bought an in-unit name match, and cannot: objdiff diffs per unit.
- **The funclet's pinned unit routinely differs from its parent's unit** — 9 of 24
  (e.g. `0x82451bc0`'s parent is in `PartLauncher`, the funclet is pinned in
  `AccomplishmentPlayerConditional`; `0x82703ad0`/`0x82703b68` are *swapped* across
  `StandardStream` and `system/rndobj/Utl`). Pairing is therefore against whatever byte-twin
  that unit's object happens to supply, which is the BF §5 false-pairing shape. It is not this
  lane's to fix: correcting it is a `splits.txt` boundary move, and I own no headings.
- 4 of 24 have a parent that is itself **anonymous** in the map, one of them in an unpairable
  `auto_*` unit (`0x822fd26c`/`0x822fd2b0` → `auto_03_822FC4F8_text`). Parent naming is a
  separate row and a separate question; nothing here depends on it.

**Verdict: the uniform treatment applies to all 24 — delete the map rows**, so each retail
funclet is treated exactly like the other ~26,000: placeholder-named, in the denominator,
paired by signature. There is no accuracy argument for naming a funclet after our own
compiler's per-TU ordinal (`$291569` is an artifact of our build, not a retail fact), and a
name that removes its own row cannot be audited by any score.

---

## 3. Prediction, pre-registered before the edit (commit `661c30b1`)

| key | baseline | predicted | basis |
|---|---:|---:|---|
| `total_functions` | 69,216 | **69,240** (+24) | certain: 24 rows stop being Hidden |
| `total_code` | 10,246,004 | **10,247,068** (+1,064) | certain: Σ the 24 `symbols.txt` sizes (68,68,44,68,68,40,32,40,44,44,32,40,40,40,40,40,40,40,40,40,40,40,44,32) |
| `matched_functions` | 43,525 | **≈43,545 (+20)**, range +16…+24 | per-unit base rate (below) |
| `masked_equal_functions` | 23,053 | **≈+20**, tracking the above | these can only pair via the funclet byte-signature fallback, which is what `masked_equal` discloses |
| `matched_code` | 4,038,804 | **≈4,039,677 (+873 B)**, range +0…+1,064 | size-weighted by the same rate |
| `matched_code_percent` | 39.418335 | **≈39.4228 (UP ~+0.004 pp)**, bounds 39.414240…39.424624 | see below |

**Basis for the rate.** Rather than guess, I measured the *untreated population in the same
units*: among report rows named `fn_<8 hex>` whose address is in my retail unwind/catch
reference set, across the 22 units these 24 are pinned to, **1,364 of 1,772 (77.0%)** score
`fuzzy_match_percent == 100` today. Per-unit rates range from 50.3% (`VocalTrackDir`) to 100%
(`DataArray`, `Graph`, `VirtualKeyboard`, `HeldButtonPanel`, `FxSendCompress`); expectation
summed per row gives **+20.0 functions / +873 B**.

⚠ **I predict `matched_code_percent` goes UP, which contradicts the brief's hypothesis that it
would go slightly DOWN.** The brief reasoned the denominator would grow faster than the
numerator. That is only true if the added rows match at below the binary's average rate; these
rows are expected to match at ~82% by bytes against a whole-binary 39.4%, so adding them lifts
the ratio. The accuracy argument is unchanged either way and is the reason for the change — a
code% move in *either* direction is acceptable here, because the denominator is becoming truer
(standing directive: accuracy beats headline %). The prediction is recorded so that a miss is
legible.

### 3.1 Measurement (build 2, `~/tmp/rb3_build_w16bg_2.log`, `BUILD rc=0`)

Applied: the 24 rows deleted from `scripts/target_symbol_map.json` (`git diff --stat` =
`1 file changed, 24 deletions(-)`, and the `-U0` diff is exactly those 24 key lines — no
reformat). `touch config/45410914/config.yml` + full `./tools/ninja-locked`; the re-split left
`config/45410914/symbols.txt` **undrifted** (a map edit changes the renamer, not the carving),
and the renamer re-ran: `[APPLIED] 3114 files checked, 1838 files patched, 86288 total symbol
renames`, `[renamed-check] 25834/29355 map names present … = 88.0% (floor 40%)`.

| key | baseline | predicted | **measured** | delta | prediction error |
|---|---:|---:|---:|---:|---:|
| `total_functions` | 69,216 | 69,240 | **69,240** | **+24** | **0 — exact** |
| `total_code` | 10,246,004 | 10,247,068 | **10,247,068** | **+1,064** | **0 — exact** |
| `matched_functions` | 43,525 | 43,545 (16–24) | **43,548** | **+23** | +3 (inside range) |
| `masked_equal_functions` | 23,053 | 23,073 | **23,076** | **+23** | +3 |
| `matched_code` | 4,038,804 | 4,039,677 (0–1,064) | **4,039,828** | **+1,024** | +151 (inside range) |
| `matched_code_percent` | 39.418335 | 39.422763 | **39.424236** | **+0.005901 pp** | +0.001473 pp |
| `fuzzy_match_percent` | 49.690746 | — | **49.695950** | +0.005204 pp | — |

**Set-diff of the `fuzzy == 100` row set** (`tools/rowset_snapshot.py`, baseline saved from this
same tree before the edit and verified set-identical to `~/tmp/rows_w16bd_main.json`:
`CROSSED IN 0 / FELL OUT 0`, so the tree was a true main baseline):

```
CROSSED IN : 23 rows, 1024 B
FELL OUT   : 0 rows, 0 B
NET bytes  : +1024
```

All 23 crossed rows are among the 24 addresses (68 B ×2 VocalTrackDir, 68 B ×2
InstrumentDifficultyDisplay, 44 B DepthBuffer3D/LightPreset/Watcher/Synapse_dsp, 40 B ×11,
32 B ×3). **Nothing else in the binary moved in either direction** — the displaced-funclet
collateral laneAP measured as its own 5 losses, and which §1 note 2 predicted was possible, did
**not** occur.

### 3.2 The two prediction misses, stated rather than smoothed

1. **+23 matched, not +20; +1,024 B, not +873.** My rate came from the *untreated* population in
   the same 22 units — 1,364/1,772 = 77.0% of retail EH-funclet placeholder rows score 100. The
   actual crossing rate was **23/24 = 95.8%**. The error is a **selection effect I under-weighted
   and should have reasoned about in advance**: laneAP named these 24 precisely *because* each
   had an exact relocation-masked byte twin in our objects, so they are the funclets with the
   most stereotyped bodies — exactly the population most likely to find an in-unit twin too. ⇒
   **an unselected base rate is a FLOOR for a selected population, not an estimate of it.** The
   control was still worth running: it bounded the answer correctly and made this miss legible.
2. **`matched_code_percent` went UP (+0.005901 pp); the brief predicted DOWN.** The brief's
   reasoning — denominator grows faster than numerator — only holds if the added rows match below
   the binary average. They match at 96% against a whole-binary 39.4%, so they can only lift the
   ratio. I pre-registered the contradiction (§3) and it held. The accuracy case never depended
   on the sign: a truer denominator is the point, and this one happened to pay in both.

### 3.3 The one row that did not cross

`default/CharLipSync::fn_827799AC` (40 B) reads **`fuzzy 99.3 / mpn 99.8`, `masked_equal: true`**.
So **24 of 24 paired by byte signature** — the pairing hypothesis is unanimous — and 23 of them
land at 100. This row also explains `masked_equal_functions` moving **+23 rather than +24**: that
counter increments only inside `match_percent_normalized == 100.0` (it discounts *credit*), so a
flagged-but-99.8 row is disclosed on the row and not in the measure. That is the documented
superset relationship between the flag and the counter, reproducing exactly.

---

## 4. Sibling sweep (item 3 — report only, nothing edited)

Over all **29,484** map rows (list values flattened; the 16 non-hex metadata keys guarded):

| class | count |
|---|---:|
| value matches an objdiff `Hidden` prefix — `__unwind*` | **21** |
| … `__catch*` | **3** |
| … `except_data_*` | **0** |
| … `__comdat_gap*` | **0** |
| value is a name `is_placeholder_symbol_name` forgives (`fn_`/`lbl_`/`jumptable_`/`code_`/`data_`/`bss_`/`rdata_`/`vftable_` + hex) | **0** |

⇒ **The hazard class is exactly the 24 rows this lane treated; it has no siblings.** There is no
map row that names a thing with a placeholder name, so the "map row naming a placeholder" variant
does not exist here. The placeholder predicate was read from
`objdiff-core/src/diff/code.rs:998-1012` (note it includes `code_` and `vftable_`, which the
brief's list omitted, and tolerates one leading underscore).

The 16 non-hex keys are metadata/bookkeeping (`_comment`, `_denylist`,
`_denylist_unadjudicated`, `_icf_arbitrary`, `_bijection_arbitrary`,
`_internal_linkage_allow`, `_splits_fill_unresolved_comment`, …); several are **list**-valued,
which is why every scan here flattens lists and guards `int(k, 16)`.

---

## 5. Gates

All in the worktree, in the brief's order; the native gate was the last action.

| gate | result |
|---|---|
| full build (`~/tmp/rb3_build_w16bg_2.log`) | `BUILD rc=0`; `config/45410914/symbols.txt` undrifted |
| `python3 scripts/verify_ruler_agreement.py --check` | rc=0 — "OK: both objdiff-cli entry points resolve the same ruler" |
| `python3 scripts/verify_objs_patched.py --verify-manifest` | rc=0 — "[patch-state] OK: 1215 decomp, 3114 target objects match … tree_sha256=ea4c0761fa8ac8ea"; denylist OK (6 addresses, 495,628 symbols scanned) |
| `python3 tools/icf_alias_finder.py --validate` | rc=0 — "VALIDATE: PASS -- 1404 map-consistent, 247 tolerated, **0 contradicted**, 1652 total" |
| `tools/native_build_gate.sh` (LAST) | verbatim below |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Commits on `w16-bg`: **`661c30b1`** (adjudication + pre-registered prediction, before the edit)
and **`c412d8c6`** (the 24-row deletion + measurement), plus this record.

---

## 6. NOT done, and why

1. **No `splits.txt` line touched**, though the adjudication found a real defect there: **9 of 24
   funclets are pinned to a different unit than their parent function** (`0x82451bc0` parent in
   `PartLauncher` / funclet in `AccomplishmentPlayerConditional`; `0x82703ad0` and `0x82703b68`
   *swapped* across `StandardStream` and `system/rndobj/Utl`; `0x822fd26c`/`0x822fd2b0` whose
   parent sits in the unpairable `auto_03_822FC4F8_text`). These are the BF §5 false-pairing
   shape: the funclet pairs against whatever byte-twin its pinned unit's object happens to
   supply, not against its own parent's funclet. **Filed, not applied** — the brief bars me from
   every `splits.txt` line, and W16-BH/W16-BI own adjacent headings. A boundary lane should price
   these nine; note that a re-home is **not** metric-neutral (PINHOME-1: re-homing changes which
   base obj is consulted, unlike a pure addition).
2. **The 4 anonymous parents were not named** (`fn_822FC508`, `fn_823187F0`, `fn_824B14E8`,
   `fn_82703A68`, `fn_82703B00`, `fn_827796F8`, `fn_82B62CD8`, `fn_8235C678` — 8 rows have no map
   name). Naming an anonymous address is a *bet* under `name_check`, paying in bug exposure rather
   than bytes (MAPID-1), and identification was not this lane's item. `fn_822FC508`'s unit has no
   base obj at all, so a name there could not pair regardless.
3. **`fn_827799AC` not pushed from 99.3 to 100.** It is a funclet pairing against a near-twin in
   `CharLipSync`; closing it is a source or pin question, not a map one, and no map edit can
   reach it.
4. **The sibling classes were swept and NOT edited** (item 3 is report-only): 0 `except_data_*`,
   0 `__comdat_gap*`, 0 placeholder-named values. Nothing to act on — the class is closed, not
   deferred.
5. **No `none`-ruler control run**, per the brief: `matched_functions` is not ruler-invariant on
   objdiff 4.2.9 (W16-AR), so a `none` leg would have measured the ruler, not the change. The
   control that *is* valid here — a same-tree row-set baseline proven set-identical to main's —
   was run instead.
6. **No `ab_measure` run.** This is a map-only change measured on one tree with a verified-clean
   baseline, a full build on both legs, and a 0-collateral set-diff; `ab_measure`'s value is its
   refusals around settling and re-split convergence, and the re-split here converged with
   `symbols.txt` undrifted. Stated so the omission is a choice on the record, not an oversight.
7. **No `src/` file and no alias group touched**, as instructed.

**What would change the verdict.** The claim "all 24 are true EH funclets, so un-naming is the
uniform treatment" would be overturned by: a retail `.pdata`/`.xdata` reading in which one of the
24 addresses is *not* reachable from any FuncInfo `UnwindMap.action` or
`HandlerType.addressOfHandler` (my scan covers all 8,541 FuncInfos in the image, so this would
have to be a FuncInfo my magic-scan missed, or a non-C++-EH funclet kind); or evidence that one
of the 24 is *both* a funclet and an independently-callable function (a `bl` to it from outside
its parent's extent), which would make a mangled name legitimate. Neither appears: no map row of
the 24 was referenced as a call target in the adjudication, and the crossing behaviour (24/24
paired by funclet byte signature) is what a pure-funclet population looks like. The weaker claim
most worth auditing is **§3.2's selection-effect explanation** for the +23-vs-+20 miss: it is a
post-hoc rationalisation of a 3-row difference and could be tested properly by measuring the
in-unit twin rate of laneAP-named vs unnamed funclets as two populations — I did not do that,
because the treatment was already decided by the adjudication and the miss was inside my
pre-registered range.
