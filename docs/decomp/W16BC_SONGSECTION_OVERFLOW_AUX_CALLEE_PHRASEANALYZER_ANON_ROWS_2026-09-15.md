# W16-BC — `0x82787ed0`'s overflow_aux callee, PhraseAnalyzer's six anonymous rows, AZ §3.3's two

**Lane:** W16-BC · **Date:** 2026-09-15 · **Branch:** `w16-bc` · **Base:** `43beee5c2647`
**Commits:** `82753299` (Item 1) · `31e993ca` (Item 2) · `d0ba48ff` (Item 3) · this doc
**Ruler:** shipped `name_check`, read from `report.json`'s `provenance.diff_config`. **No `none`-ruler
control was run anywhere in this lane** — `matched_functions` is not ruler-invariant on objdiff 4.2.9
(W16-AR §3), so a `none` leg on a map change measures the ruler, not the change.

| stage | commit | matched_functions | matched_code | code% | fuzzy |
|---|---|---|---:|---|---|
| baseline | `43beee5c` | 43,497 | 4,034,268 | 39.374060 | 49.667630 |
| Item 1 | `82753299` | 43,499 | 4,034,800 | 39.379253 | 49.668160 |
| Item 2, source guard **only** | — | 43,499 | 4,034,800 | 39.379253 | 49.668160 |
| Item 2 + map batch | `31e993ca` | 43,501 | 4,035,268 | 39.383823 | 49.673737 |
| Item 3 | `d0ba48ff` | 43,501 | 4,035,268 | 39.383823 | 49.675890 |
| **lane total** | | **+4** | **+1,000 B** | **+0.009763 pp** | +0.008260 |

The briefed baseline copy and my own measured baseline agree on every key with **0 row
disagreements**, so nothing here is inherited arithmetic.

---

## 1. Item 1 — `0x82787ed0` is `RGRollChord`'s 24 B overflow_aux, not `pair<int,int>`'s (`82753299`)

**Briefed premise:** the charge holding `default/SongLayout::push_back<SongSection>` (128 B, fuzzy
99.84) sits at callee `0x82787ed0`, whose map row spells `_M_insert_overflow_aux<vector<pair<int,int>>>`;
settle it as (i) a genuine fold, (ii) a wrong map name, or (iii) neither.

**Predicted:** one alias group, survivor = the map-resident `pair<int,int>` spelling, +1 fn / +128 B.
**Measured: +2 fns / +532 B** — and the predicted *orientation* was wrong, which is the finding.

### 1.1 The incumbent name is refuted on bytes

| side | size | relocs | element stride | verdict |
|---|---:|---:|---|---|
| retail `0x82787ed0` | **404 B** | 8 | `li r21,0x18` / `divw r11,r11,r21` / `mulli r3,r23,0x18` ⇒ **24 B** | — |
| our `<pair<int,int>>`, all 20 definers | **396 B** | 7 | 8 B | **self-pair REFUTED, flat AND chased** |
| our `<vector<RGRollChord>>` (SongParser.obj) | **404 B** | 8 | 24 B | **CHASED T1 PROVEN** |
| our `<UPartInfo>` | 404 B | — | — | same size but **NOT masked-equal** ⇒ no tie |

`class RGRollChord { int mString[6]; }` (`src/system/beatmatch/RGChords.h:3`) is exactly 24 B, matching
the measured 0x18 stride. The fold is real by pigeonhole: `retail_bodytwins` **1**, `our_bodytwins`
**6**, every one a 24 B element type.

★ **AZ §4's "both element types are 16 B" is REFUTED.** `class SongSection`
(`src/system/hamobj/SongLayout.h`) is `Range mMeasureRange`(0x0) + `Range mPatternRange`(0x8) +
`Symbol mPattern`(0x10) + `SongPattern *mSongPattern`(0x14) = **0x18 = 24 B**. The conclusion AZ drew
survives; the stated reason did not, and the 24 B stride is what identified the family.

### 1.2 What was installed, and what deliberately was not

One alias group at `0x82787ed0`: survivor = the `RGRollChord` spelling (map-resident after the repair),
folded = the `SongSection` spelling, evidence CHASED T1. The map row was **repaired**, not aliased
around — repairing a wrong name pays via its callers; an unproven alias lifts the score by construction.

Rename safety was measured before the edit, not assumed: only **2** retail callers, neither calling the
`pair<int,int>` spelling on our side, and the renamed row lands at fuzzy 85.158.

Four further spellings adjudicate **CHASED T1 PROVEN** (`RawPhrase`, `TrainerSection`, `FatFingerData`,
`UIMesh`) and were **NOT installed**: no caller charges them, so a membership would be inert
forgiveness. `Key<Transform>` is **REFUTED on bytes**, so HamRibbon's 99.607 row is *not* covered by
this group — do not brief it as such.

### 1.3 Why the prediction missed high

+2/+532 rather than +1/+128 because pre-existing alias groups at `0x827ffa50` and `0x827ffa00` already
forgave the other charges on the second row, which crossed as soon as this one was repaired. I
investigated the surplus rather than booking it.

---

## 2. Item 2 — PhraseAnalyzer's six anonymous rows (`31e993ca`)

**Predicted:** `0x8278c160` crosses at +408 B; the other three named rows pair but do not cross (Δ0)
and their callers stay at 100. Net **+1 / +408 B**, FELL OUT 0.
**Measured: +2 / +468 B, FELL OUT 0.**

```
CROSSED IN 2  (+468 B)
   +   408 B  default/system/beatmatch/PhraseAnalyzer::?Analyze@PhraseAnalyzer@@QAAXXZ
   +    60 B  default/system/beatmatch/PhraseAnalyzer::?IsUnisonPhrase@PhraseAnalyzer@@QBA_NH@Z
FELL OUT 0   (-0 B)
```

### 2.1 `fn_8278C160` (408 B) = `?Analyze@PhraseAnalyzer@@QAAXXZ` — the lane's one real source fix

Our body was **592 B**. Two blocks of rb3-Wii **dev-build** code are unguarded in our copy, and
`../rb3/src/system/beatmatch/PhraseAnalyzer.cpp` is byte-for-byte identical to ours in this function —
so the oracle cannot adjudicate it and retail bytes decide:

1. the *"Phrases don't quite coincide"* `MILO_WARN`/`MILO_LOG` block. Both macros are `((void)0)` in the
   match build, **but their ARGUMENTS are not** — `MakeString`, `SongFullPath`, `TickFormat`, `TrackName`
   are real calls the compiler must still emit.
2. the trailing `Verify()` call.

Retail's `fn_8278C160` carries relocations for exactly `__savegprlr_23`, `RawPhraseCmp`,
`sort<RawPhrase*>`, `SetPhraseIDs` ×3, `memcpy`, `TrimExcess<RawPhrase>`, `__restgprlr_23` — and **none**
for `MakeString`/`SongFullPath`/`TickFormat`/`TrackName`, nor for `Verify` **nor any of `Verify`'s own
callees** (`GetTrackTypes`, String ctors, `TrackTypeToSym`, `operator+=`), which rules out `/Ob2` having
inlined that single call site.

Guarded per the house pattern `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`:

| | size | words equal | masked-equal | relocations |
|---|---:|---|---|---|
| before | 592 B | — | — | + MakeString/SongFullPath/TickFormat/TrackName/Verify |
| after | **408 B == retail 408 B** | **102/102** | **True** | identical offset-for-offset, type-for-type, **name-for-name**, incl. the predicted `__savegprlr_18` → `_23` register-pressure drop |

★ **The source fix ALONE measured Δ0** (`CROSSED IN 0, FELL OUT 0`) — the row was anonymous, so it could
not pair. The fix and the map name are worth 408 B **only together**; either alone is worth nothing.
That is the pairability doctrine biting in the most expensive possible direction.

### 2.2 `fn_8278B638` + `fn_8278B66C` were ONE 60-byte function — and dtk said so independently

Reconstructed by hand first: `fn_8278B638` (52 B) and `fn_8278B66C` (8 B) are one
`?IsUnisonPhrase@PhraseAnalyzer@@QBA_NH@Z`, over-carved at an internal branch target. Both
`41 98 00 30` from +4 and `41 99 00 18` from +28 target `0x8278B66C`, whose body is exactly our tail
`li r3,0 ; blr`; the head is 13/13 words identical over retail's full 52 B extent.

⭐ **Naming the head made dtk merge them, which is independent corroboration I did not predict.**
jeff's Class-4 `merge_branch_reached_overcarve_tails` fired and `symbols.txt` went

```
-fn_8278B638 = .text:0x8278B638; // type:function size:0x34
-fn_8278B66C = .text:0x8278B66C; // type:function size:0x8
+fn_8278B638 = .text:0x8278B638; // type:function size:0x3C
```

The `[split-guard]` correctly refused that build (rc=1, "THE SPLIT REWROTE ITS OWN INPUT"); per the
guard's own instructions the retry is a fixed point (build 7, rc=0, 0 guard lines) and the rewritten
file is committed. The merge cost nothing because **both** halves were anonymous 0% rows, and it is
where the unpredicted +60 B came from.

⚠ Reader artifact worth recording: `fn_8278B66C` read **3,288 B** through `coff_bodies_ext.py`'s
COMDAT-span reader versus 8 B in `report.json` — the documented one-sided STLPORT-1 artifact. The `.s`,
keyed on `.fn`, gave the true 8 bytes.

### 2.3 Two rows named on CALL-SITE identity, not body identity — and the falsifiable test

`fn_8278B3D8` (92 B) and `fn_8278B678` (88 B) **fail** a body compare (47.8% and 22.7% word agreement).
They are named anyway, on a different and stronger instrument: each has exactly one retail referencer,
and in each case our side calls precisely that spelling at precisely that offset inside a caller whose
body is byte-identical to retail's.

| retail row | sole retail referencer | fuzzy of caller | our callee at the same offset |
|---|---|---|---|
| `fn_8278B3D8` 92 B | `PhraseAnalyzer::SetPhraseIDs` +124 | 100.0 | `??0PhraseData@PhraseAnalyzer@@QAA@HHH@Z` |
| `fn_8278B678` 88 B | `SongDB::GetNumOverdrivePhrases` +8 | 100.0 | `?NumPhrases@PhraseAnalyzer@@QBAHH@Z` |
| `fn_8278B638` 60 B | `SongDB::IsUnisonPhrase` +8 | 100.0 | `?IsUnisonPhrase@PhraseAnalyzer@@QBA_NH@Z` |

Because `name_check` **forgives** a placeholder target, naming converts each of those sites from
*forgiven* to *checked* — so this is a **falsifiable** bet, not a judgement call: a wrong name charges
the site and drops a caller that currently reads fuzzy 100. **All three callers still read 100.000
after the build, and FELL OUT is 0.**

The rows themselves buy no bytes, and that was predicted. What they buy is visibility:

| row | before | after |
|---|---|---|
| `??0PhraseData@PhraseAnalyzer@@QAA@HHH@Z` | anonymous, 0% | **fuzzy 60.130 / mpn 62.522** |
| `?NumPhrases@PhraseAnalyzer@@QBAHH@Z` | anonymous, 0% | **fuzzy 48.364 / mpn 51.773** |

The divergence was always there; as anonymous 0% rows it was **indistinguishable from "unidentified"**.
That exposure is the payout of naming an anonymous address — bug exposure, not bytes.

★ **Deviation from the brief, stated plainly.** The brief licensed naming only with "a definer match AND
a body compare that passes". Two of these four rows fail the body compare. I named them on the
caller-identity instrument above and on the measured `FELL OUT 0`; a reviewer who disagrees can revert
exactly two map lines at zero byte cost. Recording it as a deviation rather than folding it into the
rule is the point.

### 2.4 `fn_8278C2F8` (76 B) — identified, deliberately NOT named

`??_GFillInfo@@UAAPAXI@Z`, masked-equal — but defined by our **`SongData.obj`**, not
`PhraseAnalyzer.obj`. objdiff pairs by name **within a unit**, so naming it in PhraseAnalyzer's unit
would read 0% forever; its true owner is `SongData.cpp` and the fix is a splits proposal, out of this
lane's scope. It has **zero** retail referencers, so there is no caller channel either. One relocation
also differs: ours `??3@YAXPAX@Z` vs retail `??3BinStream@@SAXPAX@Z`.

---

## 3. Item 3 — AZ §3.3's two rows, settled with the Item 2 instrument (`d0ba48ff`)

Opened only because the caller census is **new evidence AZ did not have**; AZ named exactly what would
settle each row and both discriminators are now supplied.

**Predicted: +0 fns / +0 B on both, FELL OUT 0, both callers hold at 100.** **Measured exactly that** —
`CROSSED IN 0`, `FELL OUT 0`, headline measures unchanged, `fuzzy_match_percent` 49.673737 → 49.675890
(the pairing channel).

### 3.1 `fn_822ABCE0` (124 B) — AZ's two-way tie broken from the caller side

AZ left `resize<OldColorOption>` vs `resize<Overlay@OutfitConfig>` tied at 31/31 and judged it
unbreakable because retail's two callees are themselves fold survivors. The tie breaks from the other
direction: exactly one retail body references it — `?resize@?$ObjVector@VOldColorOption@@@@QAAXI@Z` in
`band3/bandtrack/Gem`, at **+72**, fuzzy **100.0** — and our `Gem.obj` calls
`vector<OldColorOption>::resize` at that same +72. Retail's own reloc list corroborates independently:
its `_M_erase` slot is already spelled `_M_erase<vector<OldColorOption>>`.

Named `?resize@?$vector@VOldColorOption@@…QAAXIABVOldColorOption@@@Z`. Row **0% → fuzzy 99.839**, held
by **exactly one** charge: retail's `_M_fill_insert` slot points at `0x822ab0b0`, which the map
mis-spells (§3.3). Caller held at 100.0.

### 3.2 `fn_822ABDD8` (112 B) = `?NewObject@OutfitConfig@@SAPAVObject@Hmx@@XZ` — a real allocation bug

AZ asked for "a retail-byte match against another `New`-family body". There is one: retail's 112 B body
is **masked-equal** to the generic `New` shape our objs hold at 112 B (`NewObject@CharIKFoot` in
`OutfitConfig.obj`, `NewObject@RndTransProxy` in six others, 25/28). Two retail referencers, both an
addr16 hi/lo pair inside `?Init@OutfitConfig@@SAXXZ` (fuzzy 100.0) at +20/+28, where our `Init` takes
the address of exactly this spelling — retail registers it as the class factory, as we do.

★ **AZ's "our `NewObject@OutfitConfig` is 148 B" is REFUTED — it is 100 B** in all three definers.
Neither figure is retail's 112 B, and the relocations say why:

| side | size | relocations | words |
|---|---:|---|---|
| retail `fn_822ABDD8` | 112 B | `StaticClassName@OutfitConfig`, `MemAlloc`, ctor at `fn_822AB3E0` | — |
| ours | 100 B | `??2OutfitConfig@@SAPAXI@Z` (class-specific `operator new`), `??0OutfitConfig@@QAA@XZ` | **5/25** |

Our *other* classes take retail's tagged path (`CharIKFoot`, `RndTransProxy`: 112 B, masked-equal), so
this is a **per-class macro divergence in `OutfitConfig`**, not a codegen difference — a real
allocation-path bug, filed in §5. Row **0% → fuzzy 86.929**; caller held at 100.0.

### 3.3 `0x822ab0b0` — the map name is WRONG, and renaming it would still be a REGRESSION

| pair | FLAT T1 | CHASED T1 |
|---|---|---|
| incumbent `<TransformCrowd>` **self-pair** | REFUTED (masked bodies match, reloc targets disagree) | **REFUTED** — `BYTES-DIFFER` on `_M_fill_insert_aux<TransformCrowd>` vs retail's `<OldColorOption>` |
| retail `0x822ab0b0` vs our `<OldColorOption>` | REFUTED | **PROVEN** — `SLOT-FOLD-OK` on `PoolAlloc`, `MemOrPoolAlloc`, `__uninitialized_copy`, `_M_clear_after_move`, `_M_insert_overflow_aux` |

Both bodies are 108 B and **27/27 words identical**. The differing `_M_fill_insert_aux` bodies
additionally prove the two `_M_fill_insert` instantiations **cannot have folded** — ICF requires
identical relocations — so this is a **wrong name, not a fold**, and no alias can or should paper over
it.

⛔ But a bare rename is a regression: **only `HamCamTransform.obj` defines the `<TransformCrowd>`
spelling** (`<OldColorOption>` is defined by `Gem.obj`, `OutfitConfig.obj`, `ExternalMic.obj`), so the
row — today **fuzzy 99.62963**, one charge from crossing — would become permanently unpairable. This is
the "proving a name wrong ≠ renaming is safe" trap exactly.

**Hand-off, worth +232 B:** AZ §3.4's re-home of `.text 0x822AB0B0–380` from `HamCamTransform.cpp` to
`OutfitConfig.cpp` (whose obj *does* define the correct spelling) **AND** the rename, in one commit.
+108 B on that row, plus the **+124 B already pre-positioned** at `0x822abce0` by §3.1. A `splits.txt`
edit, which this lane is forbidden.

⚠ Instrument note: my first `icf_pair_adjudicate.py --pairs` run used a dict schema instead of the
`[survivor, ours]` list it expects, and it returned a confident **CHASED T1: REFUTED** rather than an
error — a vacuous verdict shaped like a decisive one, in the tool whose own docstring warns about
exactly that. The tell was `survivor : survivor` echoed back as a symbol name.

---

## 4. Filed for W16-BA — the `get_allocator<RawPhrase>` membership

AZ §6 flags our `?get_allocator@?$vector@URawPhrase…` as a candidate membership in **W16-BA's**
`StlNodeAlloc<_List_node<int>>` ctor `blr` fold group. If it holds, it lifts
`??0?$vector@URawPhrase…` (120 B, currently **99.833**). It is BA's group, not mine — I did not touch
it, and it is recorded here so the membership is not lost. Note the group is a relocation-free `blr`
thunk, i.e. the class CLAUDE.md calls irreducible: which name the call site meant was destroyed by ICF,
so this must be settled on our-side COMDAT identity plus a caller census, never by flat T1 alone.

---

## 5. NOT DONE — with reasons

| item | reason |
|---|---|
| Re-home + rename `0x822ab0b0` (`_M_fill_insert<TransformCrowd>` → `<OldColorOption>`) | needs a `splits.txt` edit; this lane edits **no** splits line. Fully evidenced in §3.3, **+232 B** |
| Name `fn_8278C2F8` = `??_GFillInfo@@UAAPAXI@Z` | owner is `SongData.cpp`, not PhraseAnalyzer; naming it in the wrong unit reads 0% forever. Splits proposal, §2.4 |
| Fix `OutfitConfig::NewObject`'s allocation path | source fix in `OutfitConfig` (class `operator new` → tagged `StaticClassName`+`MemAlloc`, as `CharIKFoot` already does). Outside this lane's three items; evidence in §3.2 |
| Install the 4 further CHASED-T1-PROVEN `overflow_aux` memberships (`RawPhrase`, `TrainerSection`, `FatFingerData`, `UIMesh`) | **no caller charges them** — inert forgiveness, and an inert membership is an integrity cost with no payout |
| Cover HamRibbon's 99.607 row via the `0x82787ed0` group | `Key<Transform>` is **REFUTED on bytes**. Do not brief it as covered |
| `get_allocator<RawPhrase>` membership | belongs to W16-BA's group — §4 |
| AZ §3.4's `HamCamTransform` `0x822AB3D8–838` half | not my heading, and no new evidence beyond AZ's |
| A `none`-ruler control | `matched_functions` is not ruler-invariant on objdiff 4.2.9 (W16-AR §3) — a `none` leg would measure the ruler, not the change |
| Permuter | off by standing directive |
| `fn_8278B66C` as a separate row | absorbed by dtk's own over-carve merge (§2.2); there is nothing left to name |

---

## 6. Gates — all run in the worktree, native gate last

| gate | result |
|---|---|
| full build | **rc=0**, 0 `split-guard` lines (builds 7 and 8; build 6 rc=1 was the guard, recovered per its own instructions) |
| `scripts/verify_ruler_agreement.py --check` | **rc=0** — both objdiff-cli entry points resolve the same ruler |
| `scripts/verify_objs_patched.py --verify-manifest` | **rc=0** — 1,215 decomp + 3,114 target objects match, `tree_sha256=900460f3aa37abff` |
| `tools/icf_alias_finder.py --validate` | **PASS** — 1,402 map-consistent, 247 tolerated, 1 exempt, **0 contradicted**, **1,650** total |
| `tools/native_build_gate.sh` | see below — mandatory here because this lane edits `src/system/beatmatch/PhraseAnalyzer.cpp` |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`verified=18`, **`skipped=0`**, `rc=0`. The gate ran at `d0ba48ff`, which carries every `src/` change in
the lane; the only commit after it is this documentation file, which the native build does not read.
