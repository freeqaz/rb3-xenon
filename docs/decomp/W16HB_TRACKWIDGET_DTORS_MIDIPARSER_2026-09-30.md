# W16-HB — TrackWidgetImp dtor un-naming + MidiParser `fn_` naming sweep (2026-09-30)

Branch `w16-hb`, worktree `~/tmp/wt-w16-hb`. Map-only lane
(`scripts/target_symbol_map.json`); no source touched.
Predictions were committed before measuring: `docs/decomp/W16HB_PREDICTIONS_2026-09-30.md`.

## 1. Revert: the two `??1?$TrackWidgetImp<T>` names were wrong

Commit `3111589f4` (rebased to `259a087fb`) named three rows; two were wrong and are
removed by `e9c186587` (surgical, one line each; `AddInstance` kept):

| VA | wrong name | retail body | our body under that name |
|---|---|---|---|
| `0x827e2b30` | `??1?$TrackWidgetImp@UInstance@RndMultiMesh@@@@UAA@XZ` | 76 B, EH frame: `bl` list-clear on `this+4`, then store `lbl_8211D0DC` | 16 B: store `??_7TrackWidgetImpBase@@6B@`, `blr` |
| `0x827e2c98` | `??1?$TrackWidgetImp@VMeshInstance@@@@UAA@XZ` | same shape, `bl fn_827E1708` (list<MeshInstance> clear) | same 16 B |

`lbl_8211D0DC` pairs with `??_7TrackWidgetImpBase@@6B@` in objdiff's own
listing. `TrackWidgetImp<T>` owns no list — the list is a member of the
derived class (`MatWidgetImp::Instances()` returns `this+4`) — so a body that
destroys `this+4` and then falls through to the base vtable store is a
**derived** destructor with `~TrackWidgetImp<T>` / `~TrackWidgetImpBase`
inlined. Both rows scored **fuzzy 20.79** under the wrong names (the Batch 1
prediction of 100 **failed**).

Not applied here, recorded for a later lane: our `TrackWidget.obj` DEFINES
`??1MatWidgetImp@@UAA@XZ` and `??1ImmediateWidgetImp@@UAA@XZ` (EXTERN,
sclass 2). `0x827e2c98` (MeshInstance) is the natural `MatWidgetImp`
candidate; `0x827e2b30` (RndMultiMesh::Instance) has two derived candidates
(`ImmediateWidgetImp` inline dtor, `MultiMeshWidgetImp` out-of-line in
`TrackWidgetImp.cpp`) and needs adjudication before naming.

## 2. MidiParser `fn_` rows: ZERO names qualify

Population: unit `default/MidiParser`, 66 unnamed `fn_` rows below fuzzy 100.
Candidate names: EXTERN function symbols our compiled
`build/45410914/src/system/midi/MidiParser.obj` defines that are neither a row
name in the unit nor placed at any VA in the map (name-injectivity).

Instruments, both keyed on relocation-masked instruction words (never the
metric):

1. `tools/body_match.py <target> <ours> --all` — bijective hash.
   **1 PROVEN** (`fn_827E5318` ↔ `list<MidiParser*>::insert`), plus one
   5-ours × 2-retail `??_G` class (refused by the tool as ICF-ambiguous).
2. A same-size masked word-diff matrix (every unnamed row × every candidate)
   to catch near-identical bodies. Only three rows reach 0 differing words —
   exactly the three above. Everything else is ≥5 words off (the 40 B rows
   are EH funclets, 5+ words from any candidate), or sits in a 16–31-member
   equivalence class (8/16 B accessors), which bytes cannot adjudicate.

All three byte-identical hits were then **refused on relocation evidence** —
the masked field is where the type identity lives:

| row | byte-identical to | refusal |
|---|---|---|
| `fn_827E5318` (100 B) | `?insert@?$list@PAVMidiParser@@…` | retail `bl fn_827E4BF0` allocates a **0x5C-byte node** (`li r3,0x5c`) and copy-constructs the element at `+8` via a 60 B ctor ⇒ `list<X>::insert` for an ~84 B `X`. `list<MidiParser*>`'s node is 12 B and our `_M_create_node` is 64 B, not 72 B. `insert`'s body is element-type-independent; only its callee is not. |
| `fn_827E5998` (76 B) | 5 `??_G` (DataEventList, exception, __Named_exception, logic_error, length_error) | retail calls dtor `fn_827E5438` (216 B, unnamed). Our five call `??1DataEventList` (map `0x827ed930`), `??1exception` (`0x822743f0`), `??1logic_error` (`0x824f9cd0`), `??1__Named_exception` (`0x82295940`), or a 4 B `??1length_error` — none is `0x827E5438`. |
| `fn_827E63A8` (76 B) | same 5 | retail calls dtor `fn_827E62E0` (112 B, unnamed) — same contradiction. |

⇒ Byte-true, identity-false — the `_bijection_arbitrary` hazard in miniature.
The unit's `0x827E4098`–`0x827E6598` stretch interleaves TrackWidget /
TrackWidgetImp ranges and its 0% rows (node size 0x5C, derived `??_G`s) look
like **other TUs' code carried in MidiParser's split**, i.e. a
unit-attribution question, not a naming one. Not pursued here.

## 3. Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hb --patch <git diff 41fa02a36..w16-hb -- scripts/target_symbol_map.json>`
— the whole branch diff vs `main` at `41fa02a36` is ONE map line (`0x827e2a50` →
`?AddInstance@ImmediateWidgetImp@@UAAHVTransform@@M@Z`); the two dtor lines
net out against the revert. Map-only, forced re-split both legs, both legs at
a `symbols.txt` fixed point, 0 recompiles, `renamer_patched=1834`, ruler
`name_check`, objdiff-cli sha256 `c1b7d95240a35cd6` both legs.

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.001968pp   (legA 50.537804 -> legB 50.539772)
leg A/B: matched=44520 masked=23329 honest=21191 code%=42.034893
```

Prediction (committed first): Δmatched 0, Δcode 0 B, Δmasked_equal 0,
Δfuzzy ≈ +216 × 0.9313 ≈ +201 B ≈ **+0.0020 pp** — **held on every key**.
`none` control flat (+0 B), as expected for a map-only edit whose row stays
below 100. `AddInstance` reads fuzzy **93.13** / mpn **93.22** on the branch
build: it pairs now, but crossing to 100 is source work (not attempted).
Run dir: `~/tmp/wt-w16-hb/.ab_measure_runs/20260930-074604-w16-hb-branch-vs-main-3710465/`.

## 4. Gates

**Validator** — `python3 tools/icf_alias_finder.py --validate` on the
branch build (full `./tools/ninja-locked` after re-attaching the branch;
freshness precondition verified): `VALIDATE: PASS -- 1479 map-consistent,
250 tolerated (enumerated above), 0 contradicted, 1730 total`, rc=0.

**Native gate** — `tools/native_build_gate.sh` is run as the lane's LAST
action, on the tree this commit lands; its `NATIVE_GATE_RESULT` line is in the
lane report (map/doc-only lane, so no native-relevant path changed).

## Deliberately not done

- Did not name `??1MatWidgetImp` / `??1ImmediateWidgetImp` (outside brief; `0x827e2b30` is two-way ambiguous).
- Did not re-home or re-pin any MidiParser range.
- Did not name any 8/16 B accessor row by class-bijection.
