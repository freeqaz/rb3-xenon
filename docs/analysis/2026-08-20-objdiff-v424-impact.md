# objdiff v4.2.4 impact on rb3-xenon: 2,338 functions leave the matched set, and most of what charged them is a naming layer

**Date:** 2026-08-20
**Repo:** `rb3-xenon` (Rock Band 3, Xbox 360 / MSVC Xenon). **Not** `../rb3` (Wii/MetroWerks),
**not** `../dc3-decomp`. These three share symbol names and address ranges; every number
below is rb3-xenon's.
**Binary under test:** `bin/objdiff-cli` -> `/home/free/code/milohax/objdiff/target/release/objdiff-cli`,
`objdiff-cli 4.2.4 (39144b470916, xxh3 b163150cbf5cfa90)`.
**Scope:** measurement and analysis only. No source file was modified.

## 0. What changed in the tool

objdiff commit `b14ba45` (shipped in v4.2.4) stops folding a *vetted* relocation-name
disagreement into `arg_diff_score`. Because
`match_percent_normalized = diff_score - arg_diff_score`, a failing `Reloc`/`Reloc`
argument used to land in both terms and cancel to exactly zero — the canonical score was
structurally blind to a wrong callee. Under `functionRelocDiffs=name_check`, and only
there, that penalty now stays in `diff_score` and reaches the canonical score.

Three noise classes stay folded (`objdiff-core/src/diff/code.rs`, `vetted_reloc_name_diff`):
MSVC register save/restore helpers (`is_regalloc_save_helper`), splitter placeholder names
(`is_placeholder_symbol_name`: `fn_`/`lbl_`/`jumptable_`/`code_`/`data_`/`bss_`/`rdata_`
+ hex), and function-local-static scope ordinals (`local_static_ordinal_only_diff`).

## 1. Configuration: confirmed

`objdiff.json` sets it under a top-level `options` block, exactly as the change requires:

```json
"options": { "functionRelocDiffs": "name_check" }
```

Confirmed live in both reports' `provenance.diff_config`, whose first entry is
`functionRelocDiffs=name_check`. rb3-xenon is in scope for the behaviour change.

## 2. The existing report was NOT a usable baseline — and why

`build/45410914/report.json` was preserved before anything was regenerated:

- `build/45410914/report.pre-v424-20260820T213531.json` (in-tree; `build/` is gitignored)
- `/tmp/rb3x_report_pre_v424_20260820_213531.json`

Its `provenance` reads `tool_version 4.2.3`, `tool_commit 88b425bc3bad-dirty`, written
`2026-08-20 06:18`. **It cannot be A/B'd against a fresh report**, because two unrelated
things landed on `main` afterwards:

- `0fee87d3` / `6d8f9e32` (09:33, 09:46) — VTGRIND waves; source changed (`RndFont`,
  5 spurious `virtual`s removed), so the base objects differ.
- `c70d076e` / `25dd39b4` (21:34, 21:35) — ALIAS-PARTITION-DPRIME; `scripts/symbol_aliases.json`
  changed, so `icf_aliases.map` differs: `map_file_hash 20ab20f9854ae761` / 5,472 entries
  in the old report vs `2d4e5b8dbc114116` / 5,449 after.

A straight old-vs-new diff would have conflated the tool change with a source change and a
symbol-map change. It was not used for any number in this document.

**The controlled experiment instead.** A full `ninja` was run first (443 edges, clean exit)
to bring objects, the alias map and `report.json` to one consistent state. Then two reports
were generated over *those same prebuilt objects*, same project, same args, `--no-cache` on
both, so the **only** variable is the binary:

| | binary | commit | report |
|---|---|---|---|
| A (control) | `objdiff-cli 4.2.3` | `6d50daef2857` — the direct parent of the fix, built into `/tmp/objdiff-ctl-6d50dae` so the shared `bin/objdiff-cli` symlink (also used by `dc3-decomp` and `../rb3`) was never touched | `/tmp/report_A_4.2.3.json` |
| B (treatment) | `objdiff-cli 4.2.4` | `39144b470916` | `/tmp/report_B_4.2.4.json` |

Both reports record `map_file_hash 2d4e5b8dbc114116`, 5,449 entries, 3,091 units,
69,219 functions, identical `diff_config`. The in-tree `build/45410914/report.json` that
`ninja` produced carries `tool_version 4.2.4` / `tool_commit 39144b470916`, confirming the
build really did use the new binary.

## 3. Headline, with denominators

| measure | 4.2.3 | 4.2.4 | delta |
|---|---|---|---|
| `matched_functions` | 44,510 / 69,219 | 42,172 / 69,219 | **-2,338** |
| `matched_functions_percent` | 64.303154% | 60.925465% | **-3.3777 pp** |
| `matched_code` | 3,759,304 / 10,245,956 B | 3,759,304 / 10,245,956 B | **0** |
| `matched_code_percent` | 36.690613% | 36.690613% | 0 |
| report `fuzzy_match_percent` | 48.990124 | 48.981503 | **-0.008621 pp** |
| `complete_units` | 1 / 3,091 | 1 / 3,091 | 0 |
| `masked_equal_functions` | 22,911 | 22,911 | 0 |

- **Dropped functions: 2,751. Risen: 0.** Direction holds — the change can only cost points.
- **Left the matched set (was exactly 100.0, now below): 2,338 functions, 559,736 bytes.**
- 413 of the 2,751 were already below 100 and dropped further.
- Magnitude: median **0.185 pp**, max 5.000 pp, min 0.003 pp. 2,151 of 2,751 land in the
  0.05–0.5 pp band. This is a wide, shallow effect: one charged site on a large otherwise-
  matched function.

Compared with DC3 (328 dropped / 54 unmatched / -0.1117 pp headline), rb3-xenon loses
**~43x more functions** but **~13x less headline**. The reason is section 6.

### `matched_code` did not move, and that is not a bug

`objdiff-cli/src/cmd/report.rs:1099` credits `matched_code` on `match_percent == 100.0`
(fuzzy), whereas line 1137 credits `matched_functions` on
`match_percent_normalized == 100.0`. The change touches only the normalized score, so the
byte total is by construction unaffected. Quote the function count for this change, not the
byte count.

## 4. Control: PASSES, with one field-name trap

**Per-function `fuzzy_match_percent` is byte-identical on all 69,219 functions in all 3,091
units.** Zero differ. `diff_score` was not touched, exactly as intended.

⚠ **The report's top-level and per-unit `fuzzy_match_percent` DID move** (-0.008621 pp at
top level; 616 of 3,091 units differ). This is **not** a control violation and must not be
reported as one. `report.rs:1096` accumulates

```rust
measures.fuzzy_match_percent += match_percent_normalized * symbol.size as f32;
```

— the *aggregate* field named `fuzzy_match_percent` is a size-weighted mean of
`match_percent_normalized`, colliding with the *per-item* field of the same name, which
really is fuzzy. So the aggregate is expected to move and the per-item values are the real
control. Anyone auditing this change on another repo will hit this; read the per-function
values.

## 5. What the 2,338 were charged on

Every dropped function was re-diffed with
`bin/objdiff-cli diff -p . -u <unit> <symbol> -o - -f json --include-instructions`
(2,751 diffs), and every instruction row whose *symbol* argument differs between target and
base was collected: **5,655 raw rows**. objdiff's own three carve-out predicates were then
re-implemented and applied, so what remains is what the tool actually charges:

| filter | rows removed |
|---|---|
| `__savegprlr_NN` / `__restgprlr_NN` register helpers | 168 |
| `fn_`/`lbl_` placeholder names | 746 |
| function-local-static scope ordinal only | ~322 |
| one side not a symbol (shape mismatch — stays folded) | few |
| **`vftable_<hex>` — an UNCOVERED placeholder prefix, see below** | 34 |
| **remaining: genuinely charged sites** | **4,385 on 2,731 functions** |

2,747 of the 2,751 dropped functions carry at least one charged site. The four that do not
are `default/MoveMgr ??2SpotlightDrawer@@SAPAXI@Z`,
`default/PropertyEventProvider ??_DPropertyEventProvider@@QAAXXZ`,
`default/Rnd_Xbox ?resize@vector<RndPointTest>...` and
`default/Morph ?resize@vector<RndMorph::Pose>...` — unexplained by this extraction, worth a
look, but 0.15% of the population.

Method check (DC3 caution (a): a name sweep is evidence a class exists and no evidence about
any row). Four sampled functions were re-diffed under both rulers; each is *equal* under
`functionRelocDiffs=none` and charged only under `name_check`, so the charge really is the
relocation name and nothing else:

| function | `none` | `name_check` |
|---|---|---|
| `?Init@ByteGrinder@@QAAXXZ` | 100.0 | 99.32822 |
| `?AddObject@BandCharacter@@QAAXPAVObject@Hmx@@@Z` | 100.0 | 99.98834 |
| `?UpdateOverlay@NgRnd@@MAAMPAVRndOverlay@@M@Z` | 100.0 | 99.73384 |
| `?AssignPrefabsToSlots@PrefabMgr@@QAAXXZ` | 91.24272 | 91.14563 |

### Adjudication against retail addresses

Every charged name was resolved through the union of `scripts/target_symbol_map.json`,
`config/45410914/symbols.txt`, `build/45410914/icf_aliases.map` and
`scripts/symbol_aliases.json` (259,136 names):

| verdict | sites |
|---|---|
| base-side name absent from every target-side naming source — not adjudicable | 2,595 |
| both names resolve, to **different** retail addresses — candidate real divergence | **1,782** |
| target-side name absent | 5 |
| both resolve to the **same** address (alias-map gap — should have been exempt) | 3 |

The 1,782 sit on **1,137 distinct functions, 907 of which read exactly 100.0 before**. 818
of the 1,782 are same-function-name / different-template-or-class-argument pairs
(`list<CharClip*>::insert` vs `list<Object*>::insert`, `ObjRefConcrete<FlowLabel>::~` vs
`ObjRefConcrete<EventTrigger>::~`, `vector<float*>::_M_fill_insert` vs
`vector<float>::_M_fill_insert`) — the near-clone family, where two retail bodies differ by
one instruction.

## 6. The dominant caveat: on rb3-xenon this measures a reconstructed naming layer

**All 1,782 target-side names in the DIFF_ADDR class come from
`scripts/target_symbol_map.json`.** That file's own `_comment` says what it is:

> Most `0x` entries are auto-generated by `tools/fingerprint_match.py gen_target_map`
> (bindiff conf>=0.95, similarity>=0.96, cross-TU callees included).

28,998 of its entries are addresses named by BinDiff similarity, and it carries its own
`_bijection_arbitrary` list of **1,025 addresses whose name assignment it records as
arbitrary**. **239 of the 1,782 charged sites land on one of those addresses.**

This is DC3's caution (c) at industrial scale, and it is the structural difference from DC3:
DC3's target names largely come from a real linker map, rb3-xenon's are largely
reconstructed. So the honest reading of the -3.38 pp is *not* "rb3-xenon has 2,338 wrong
callees". It is: **`name_check` now scores the target-naming layer, and on this repo that
layer is a BinDiff product.** Some of what it exposes is real source divergence; a large
share is incomplete ICF alias coverage in `scripts/symbol_aliases.json` and mis-assigned
BinDiff names.

A worked refutation, so this is not a hypothetical. `?AddObject@BandCharacter@@` and
`?Hookup@CharHair@@` are both charged on
`?Link@ObjPtrList<RndMesh>@` (target, `0x8227D020`) vs
`?Link@ObjPtrList<CharCollide>@` (ours, `0x823A4FA8`). `0x823A4FA8` *is* a real split
boundary — but its first instructions in `orig/45410914/band.exe` are
`c1a30200 c0040008 ec006828` (`lfs`, `lfs`, `fmuls`), floating-point maths that
`ObjPtrList<T>::Link` has no use for. The name at that address is BinDiff-assigned and
semantically implausible. **Do not chase this as a source bug without re-adjudicating the
name.** Same shape applies to `?Hit@GemPlayer@@` charged on
`?QueueEnumJob@PlatformMgr@@` (`0x82B93F48`) — a `PlatformMgr` job queue call inside a gem
hit is not credible.

### An upstream gap this surfaced: `vftable_<hex>`

rb3-xenon's splitter names target vtables `vftable_820BA4E4`; our side emits
`??_7CampaignGoalsLeaderboardPanel@@6B@`. That is exactly the placeholder class
`is_placeholder_symbol_name` exists to exempt, but its prefix list
(`fn_`, `lbl_`, `jumptable_`, `code_`, `data_`, `bss_`, `rdata_`) does not include
`vftable_`. **34 sites across 16 functions** — including `??0CampaignGoalsLeaderboardPanel@@`,
`??0SyncMachineMsg@?A0x5fd33732@@`, `??0OpenWaitingGateMsg@?A0xb0de99ba@@`,
`??1PrefabChar@@`, `??0CharEyes@@` — are charged on nothing but that spelling. Adding
`vftable_` (and `vbtable_`) to the prefix list in `objdiff-core/src/diff/code.rs` would
remove them. They are excluded from every count in section 5 onward.

## 7. Strongest real leads

Ranked by how little they depend on the BinDiff naming layer. The string-literal ones are the
strongest available evidence on this repo, because MSVC's `??_C@` mangling encodes the
literal's *content*, and the content was checked directly against `orig/45410914/band.exe`
(PE image base `0x82000000`, `.rdata` at VA `0x82000400`).

**1. `?AssignPrefabsToSlots@PrefabMgr@@QAAXXZ`** — `default/band3/meta_band/PrefabMgr`,
91.24272 -> 91.14563.
Retail references the literal at `0x82011994`; our source passes the one at `0x82011668`.
Retail bytes: `0x82011994` = `b'female\x00\x00feet_skin.mat\x00'`, `0x82011668` = `b'male\x00\x00\x00\x00'`.
So retail says **"female"** where we say **"male"**. The name is verified against the bytes, not
trusted from config. (Direction caveat: this function plausibly touches both genders, so read
the source before assuming a swap rather than an ordering difference.)

**2. `?UpdateOverlay@NgRnd@@MAAMPAVRndOverlay@@M@Z`** — `default/Env_NG`, 100.0 -> 99.73384.
Retail calls `??$MakeString@PBDPBD@@` (`MakeString<const char*, const char*>`, `0x8229D148`);
we call `??$MakeString@HH@@` (`MakeString<int,int>`, `0x82399348`). These are two genuinely
different retail bodies — retail's opens `stwu r1,-0x880(r1)` (it reserves the big string
buffer) against ours at `-0x70`. This is the known cross-repo `MakeString` class: we are
passing integers where the original passes strings. 17 sites of this exact pair binary-wide.

**3. `?Init@ByteGrinder@@QAAXXZ`** — `default/ByteGrinder`, 100.0 -> 99.32822.
Retail registers `?getRandomSequence32B@@YA?AVDataNode@@PAVDataArray@@@Z` (`0x827255F0`);
we register `...32A` (`0x827256C8`). The two bodies are distinct in retail (they differ in one
`bl` displacement: `48103c69` vs `48103b91`), so this is a swapped DataFunc registration, not a
fold. Two adjacent near-identical functions is also the classic BinDiff-confusion shape — worth
a source read to confirm which way round.

**4. `?SymbolToAudioType@@YA?AW4SongInfoAudioType@@VSymbol@@@Z`** —
`default/system/utl/SongInfoAudioType`, 2.04762 -> 1.76190. Three charged literal sites; the
comparison chain carries the wrong set/order of audio-type names — retail
`"drum"`/`"guitar"`/`"vocals"` against our `"guitar2"`/`"vocals"`/`"harm2"`. All four literals
were confirmed present at the named retail addresses (`0x82010E1C` `drum`, `0x82013FA8`
`guitar`, `0x8201DB6C` `vocals`). The function is at 2%, so this is a rewrite lead rather than
a one-line fix, but it says exactly which names the chain should test.

**5. `?Handle@Synth@@UAA?AVDataNode@@PAVDataArray@@_N@Z`** — `default/CharMeshHide`,
100.0 -> 99.99138. Retail calls `??$Find@VFlow@@@Synth@@` (`0x826FE3A8`); we call
`??$Find@VObject@Hmx@@@Synth@@` (`0x826FE428`). The two instantiations exist separately in
retail and differ by exactly one `bl` target, so they did not fold — a real wrong template
argument on the DTA `find` handler.

Also worth a look, same DIFF_ADDR class, previously perfect: `?Handle@MusicLibrary@@` (6,160 B),
`?Handle@GemPlayer@@` (5,612 B), `?Handle@PlatformMgr@@` (3,112 B),
`?Load@RndText@@UAAXAAVBinStream@@@Z`, `?SyncDir@WorldInstance@@`,
`?CopyTypeProperties@@YAXPAVObject@Hmx@@0@Z`,
`?Load@OutfitConfig@@UAAXAAVBinStream@@@Z`.

## 8. The other large class: incomplete ICF alias coverage

The most-repeated charged pairs are not source bugs at all, they are folds
`scripts/symbol_aliases.json` has not recorded yet:

| sites | target | ours |
|---|---|---|
| 102 | `?insert@list<CharClip*>` | `?insert@list<Object*>` |
| 91 | `??3BinStream@@SAXPAX@Z` | `OggFree` |
| 75 | `??A?$map<Symbol,...>` | `?DataRegisterFunc@@` |
| 50 | `??1ObjRefConcrete<FlowLabel>` | `??1ObjRefConcrete<EventTrigger>` |
| 36 | `??1ObjRefConcrete<CharLookAt>` | `??1ObjRefConcrete<RndMesh>` |
| 26 | `?_M_fill_insert@vector<float*>` | `?_M_fill_insert@vector<float>` |

The 4-byte tail-call thunks are the purest form: `??3Task@@SAXPAX@Z` is a single
`b ?MemFree@@YAXPAX@Z` on our side and the target's same-named 4-byte symbol branches to
`?clear@_Rb_tree<Symbol,CatData>::clear()`. Every such thunk is 100.0 -> 95.0 (5.00 pp on one
instruction), which is where the 100 functions in the 2–10 pp band come from. Growing the alias
groups is the lever for this class, not source edits.

## 9. Recommendations

1. **Quote the function count, not the byte count**, for this change: `matched_code` is
   fuzzy-gated and did not move.
2. **Do not treat the -3.38 pp as 2,338 wrong callees.** On this repo the ruler now scores a
   BinDiff-reconstructed naming layer. 239 charged sites sit on addresses the map itself
   flags `_bijection_arbitrary`.
3. **Fix `vftable_<hex>` upstream** — 34 sites / 16 functions, pure spelling.
4. **Feed the near-clone pairs back into `scripts/symbol_aliases.json`.** The top-6 pairs
   alone account for 380 sites.
5. **Triage the string-literal charges first.** They are content-verifiable against
   `band.exe` and need no trust in the naming layer at all.
6. If a per-function regression gate is wanted on `matched_functions`, rebase its baseline —
   the 2,338 is a one-time ruler step, not a regression.

## Reproduction

```sh
cd /home/free/code/milohax/rb3-xenon
ninja                                            # objects + alias map + report.json (4.2.4)
git -C ../objdiff worktree add /tmp/objdiff-ctl-6d50dae 6d50dae   # parent of the fix
cargo build --release -p objdiff-cli --manifest-path /tmp/objdiff-ctl-6d50dae/Cargo.toml
/tmp/objdiff-ctl-6d50dae/target/release/objdiff-cli report generate -p . -o /tmp/report_A_4.2.3.json --no-cache
bin/objdiff-cli                                  report generate -p . -o /tmp/report_B_4.2.4.json --no-cache
# then A/B per function on match_percent_normalized, with per-function
# fuzzy_match_percent as the control (must be identical on all 69,219).
```

Per-site adjudication re-diffs each dropped symbol with
`bin/objdiff-cli diff -p . -u <unit> <symbol> -o - -f json --include-instructions`, applies
objdiff's own carve-out predicates, and resolves both names through
`scripts/target_symbol_map.json` + `config/45410914/symbols.txt` +
`build/45410914/icf_aliases.map` + `scripts/symbol_aliases.json`. Confirm any individual row
by re-running it with `-c functionRelocDiffs=none`: equal there and charged under `name_check`
is the only attribution that counts.
