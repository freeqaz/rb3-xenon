# W16-HM — engine (`src/system/`) fuzzy sweep: the 1–90% band

**Branch** `w16-hm`, rebased onto main `110a5450`. **Ruler** `name_check` (graded,
read from `report.json` `provenance.diff_config`).
**Population:** every *named* row in `src/system/` units with
`1 ≤ fuzzy_match_percent < 90`, excluding `src/system/{bandobj,rndobj,char,rnddx9}`
(another session). Ranked by `size × (100 − fuzzy)` off the worktree's own freshly
built `report.json` at `1ee4b902`: **206 rows**.

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hm-base --patch <main..w16-hm diff>`,
leg A = main `110a5450`, one run over the whole branch diff (30 files; kinds map +
splits + source, so both legs were force-re-split and read at a `symbols.txt` fixed
point). objdiff-cli pinned across legs; leg A settled at 0 recompiles, leg B 700.

```
leg A: matched=44851 masked=23399 honest=21452 code%=42.803753
leg B: matched=44878 masked=23401 honest=21477 code%=42.848606
Δmatched=+27  Δmasked_equal=+2  Δhonest=+25  Δcode%=+0.044853pp  Δcode_bytes=+4596
Δfuzzy=+0.036638pp   (legA 51.818542 -> legB 51.855180)
units at 100% [mpn]: 215 -> 215   [all-rows-fuzzy]: 189 -> 189
control none: Δmatched_code=+5600 B (default ruler +4596 B)
```

**Row-level diff of the two archived leg reports: 31 rows up, 0 down.** The 31 are
the 27 band rows in §2 plus four `fn_` rows (UI ×3, Instance ×1) that went
99.5 → 100 with their parents. The only "unit regression" the tool prints,
`ColorPalette 2 -> 0`, is the re-home in §4.3: every row that left ColorPalette
reappears in GameGemList at an equal or higher score (`~vector<GameGem>` 0 → 100,
`fn_826AB31C` 0 → 100, `fn_826AB190` 99.5 → 100, the two fill rows 100 → 100).
Seven rows are GONE/NEW pairs only because their map names were respelled
(§4.2). Six stay at 100, and the seventh, the ColorSet reader, goes 86.83 → 100.

Prediction before running: net positive, 0 rows down, Δmatched at least the named
band rows that reached `mpn == 100`. It held. **Δmatched +27 decomposes
exactly:** 22 named band rows + 1 `fn_` row cross `mpn == 100` (23); the ColorSet
reader rename adds +1; the GameGem re-home nets +3 (5 in GameGemList, 2 out of
ColorPalette); the six other renames net 0.

## 2. Rows, before → after

Before = leg A (main `110a5450`; identical to the lane's starting snapshot at
`1ee4b902` for every row below). After = leg B. `fuzzy_match_percent` from the
archived leg reports.

| row | B | before | after | what fixed it |
|---|---:|---:|---:|---|
| `CamShot::StartAnim` | 552 | 8.41 | **99.93** | RB3 body: `HandleType` not `Export`, `dynamic_cast<WorldDir*>(Dir())`, no HamWardrobe, inline anim loop, unsigned crowd index |
| `UIManager::Poll` | 792 | 56.11 | **99.95** | DC3 extras native-only; pop branch keeps `mTransitionScreen` (**behavioural**); residue = ICF ctor name |
| `Multiply(Matrix3, Matrix3, Matrix3&)` | 680 | 28.79 | 74.15 | row-wise with an alias-safe `b == out` path (**behavioural** in the alias case). Residue: FP accumulation order |
| `InlineHelp::PreLoad` | 224 | 6.43 | **100.00** | RB3 rev dialect (§4.1): plain stream, two TU rev shorts, no rev-5 resource dir, no PushRev |
| `UIScreen::Enter` | 384 | 46.01 | **100.00** | rb3-Wii Enter; DC3 post-proc count + glitch report kept native-only |
| `Debug::Fail` | 264 | 33.94 | 93.86 | RB3 match path: heap push brackets, no stack-trace strings, no Modal; 2-arg `CaptureStackTrace`. Residue: branch layout + uninlined `Timer::Sleep` thunk |
| `LabelNumberTicker::PreLoad` | 244 | 43.00 | **100.00** | RB3 rev dialect (aligned aggregate) |
| `UIManager::Terminate` | 316 | 59.75 | **100.00** | rb3-Wii minus CheatProvider/Automator; removes the callback Init added (**behavioural**) |
| `UIList::Copy` | 360 | 64.87 | **99.94** | rb3-Wii Copy (no `mListDir` copy, no `Update()`); residue = ICF-folded `SetMaxDisplay` name |
| `MidiInstrument::Load` | 200 | 37.80 | **100.00** | rb3-Wii: plain rev, `SampleZone::gRev`, raw stream to the zone readers (§4.2) |
| `LabelShrinkWrapper::PreLoad` | 140 | 17.03 | **100.00** | RB3 rev dialect |
| `UIComponent::Copy` | 168 | 31.43 | **100.00** | rb3-Wii: resource triple, `Object::Copy`, then virtual `CopyMembers` (**behavioural**, §4.4) |
| `InlineHelp::Copy` | 148 | 24.05 | **100.00** | members move to a `CopyMembers` override (retail fn_82316840); Copy only re-runs `Update()` |
| `SpotlightDrawer::Load` | 152 | 26.11 | **100.00** | rb3-Wii Load (max rev 5); params loader `(BinStream&, int)` with DC3 field list |
| `WorldReflection::Load` | 228 | 57.93 | **100.00** | RB3 rev dialect |
| `ColorPalette::Load` | 244 | 61.87 | **100.00** | RB3 rev dialect; `ColorSet` is 0x20 (§4.3) |
| `WorldInstance::PostLoad` | 216 | 58.33 | **100.00** | base PostLoad first, then pop into the TU rev statics PreLoad uses; raw stream to `LoadPersistentObjects` |
| `CamShot::EndAnim` | 196 | 57.29 | **100.00** | same: `HandleType`, no HamWardrobe, inline EndAnim loop |
| `InlineHelp::Update` | 180 | 56.38 | **100.00** | `UIComponent::Update` then `mResource->Dir()->Find<BandLabel>` (not UILabel -- different RTTI, cannot be a fold) |
| `UIList::PreLoad` | 116 | 38.34 | **100.00** | RB3 rev dialect + rb3-Wii `PreLoadWithRev(BinStream&, int)` inlined |
| `FxSendDistortion::Load` | 144 | 51.19 | **100.00** | RB3 rev dialect (rev +0 / alt +4 in this TU) |
| `UISlider::Copy` | 112 | 37.46 | **100.00** | base copy then `mSelectToScroll` only |
| `Sequence::Load` | 220 | 74.05 | **100.00** | rb3-Wii: local rev (no statics), rev > 3 guard |
| `UIProxy::PostLoad` | 176 | 69.39 | **100.00** | popped rev into TU statics (initialised, §4.1) |
| `UIList::Update` | 116 | 54.76 | **100.00** | base Update, `mListDir` from `dynamic_cast<UIListDir*>(mResource->Dir())`, CreateElements; no edit-mode Refresh |
| `LabelNumberTicker::PostLoad` | 68 | 29.06 | **100.00** | no PopRev; base PostLoad then virtual `Update()` |
| `vector<ColorSet>::resize` | 124 | 84.03 | 99.84 | `ColorSet` 0x20 (§4.3); residue = callee name |

Outside the band, also up in the leg diff: `fn_828036B0` 99.45 → 100,
`fn_82804218` / `fn_82804240` 99.5 → 100 (UI), `fn_824ED0D8` 99.5 → 100 (Instance);
the ColorSet vector reader 86.83 → 100 (renamed, §4.2); `~vector<GameGem>` 0 → 100 and
`fn_826AB31C` 0 → 100 (re-homed, §4.3).

## 3. Rows examined and left, and why

27 of the 206 rows moved. I examined these and did not fix them:

- **Wrong map names.** The retail body cannot be the named function:
  - `PreloadPanel::Load` @`0x827B4668` runs a typedef-array loop over a global
    manager.
  - `HAQManager::GetButtonText` @`0x82BB1788` and `OSCMessenger::MakeOSCAddress`
    @`0x82BB2A20` are both a mod-13 / xor key routine.
  - `DevHostname(Symbol)` @`0x8250F898` is a `Debug` member: it takes `this` and
    reads 0xc / 0xf8 / 0xfc, then calls `Fail`.
  - The triage (target-vs-base call-set overlap, `~/tmp/w16hm/triage.py`) flags
    the same class for `__uninitialized_fill_n<DrivenPropertyEntry>` (1,348 B vs
    our 96 B), `operator delete` (148 vs 4 B) and
    `ReceiveUpstreamEEPROMWriteResponse` (164 vs 16 B).
  - Map untouched for all of these.
- **`LightPreset::Copy`** (936 B, 37.7). Retail is rb3-Wii's Copy, with four
  `x[i]->AddRef(this)` loops through `Hmx::Object::AddRef(ObjRefOwner*)`. That
  function is itself at 69.7 and our `Clear()` releases nothing. Porting only the
  AddRefs would leak references, so it needs the ref model first.
- **`SpotlightDrawer::DrawShadow`** (328 B, 38.2). Retail casts the target to
  `Character` and calls `Character::DrawShadow(const Transform&, const Plane&)`,
  with the plane built from normal (0,0,1) through the character's position
  raised by 1.5. Ours is DC3's `(Transform, float)` on `RndDrawable`. The fix
  lives in `src/system/char/`, which is out of scope.
- **`CSHA1::Transform`** (5,856 B, 55.7). Retail loads the state in order
  `a,b,c,d,e`; ours is hand-ordered `c,b,a,d,e`. The natural order scored
  **53.9 (worse)**, so I reverted it. The rest is scheduling in the unrolled
  rounds.
- **`FaderGroup::Load`** (212 B, 74.2). Already rb3-Wii's text. The gap is the
  `ObjPtrList` implementation, not this function.
- Residue on rows in §2:
  - `Debug::Fail` 93.86: retail emits `beq`+`b` pairs where we emit `bne`,
    guards the callback loop where we jump to the test, and does not inline the
    `Timer::Sleep` thunk defined later in the TU. No source spelling tried moved
    these.
  - `Multiply(Matrix3…)` 74.15: FP accumulation order inside each row.
    Retail's row 0 orders are x = y+z+x, y = z+y+x, z = y+x+z; an explicit
    helper with those orders scored **20.5** and grew the body to 776 B, so it
    was dropped.

The other ~170 rows were not examined.


## 4. Findings worth reusing

### 4.1 RB3's load-rev dialect is the dominant engine defect in this band

**13 of the 27 fixes are this one mechanism.** DC3 loads through a stack
`BinStreamRev d(bs, revs)`, guards the version, and `PushRev`/`PopRev`s.
RB3 retail (rb3-Wii's `ObjMacros.h` shape) does none of that:

- it reads a plain `int`, splits it into two TU `unsigned short` statics, and
  re-reads the rev short for every later test;
- there is no guard and no push/pop;
- the raw `BinStream` goes to the base loads and element readers.

Some functions (Sequence, MidiInstrument, SpotlightDrawer) keep the rev in a stack
local and have no statics at all.

**Placing the two shorts.** Retail puts alt at +0 and rev at +4 in most TUs
(FxSendDistortion's is the reverse). Three spellings, measured:

- *Separate uninitialised statics* are co-addressed, but MSVC placed rev first
  whatever the declaration order. I tried both orders, with and without
  `align(4)`, and a probe function that referenced alt first.
- *Separate statics initialised `= 0`* follow declaration order. Found in
  `world/Instance.cpp`, then confirmed on InlineHelp.
- *One aligned aggregate* (`ui/UIListArrow.cpp`'s fix) places them right, but
  turns a function's single direct read (`lhz lbl+4@l`) into `addi` + `lhz 4`.
  That cost InlineHelp's ActionElement reader 2.6 pp and Instance's
  `LoadPersistentObjects` 0.6 pp.

⇒ **Use initialised separate statics where another function reads the rev
directly; the aggregate is fine where only the co-addressing function touches
it.**

### 4.2 A reader named `…BinStreamRev&…` whose caller passes the raw stream is misnamed

Seven map names spelled a reader's stream parameter as `BinStreamRev&`, which is
DC3's spelling copied from our old source. In each case retail's caller passes
the raw `bs` register. The rows scored 100 anyway, because the reader bodies are
ABI-identical, so nothing flagged them. Renamed to the `BinStream&` spelling:

- the ActionElement vector reader and element reader;
- the three SampleZone readers (`ObjVector` reader, `operator>>`, `Load`);
- the `vector<ColorSet>` reader;
- `WorldInstance::LoadPersistentObjects`.

All seven are at 100 after a forced re-split. The ColorSet reader was 86.83,
because our `BinStreamRev` overload also reached the wrong `ColorSet` size.

### 4.3 The ColorSet 0x44 pad was fitted to two misnamed rows

`7eb4704d` (2026-06-09, **TU0 era**) padded `ColorSet` to 0x44 because
`__uninitialized_fill_n<ColorSet>` and `_M_fill_insert<ColorSet>` step by 0x44.
On TU5 retail bytes, the real ColorSet code steps by **0x20**: the reader
`0x824DFE48`, `resize` `0x826F95C0`, and `ColorPalette::Load` itself. The two
0x44 rows are `vector<GameGem>` instantiations: `0x8278DEF0` calls
`_M_fill_insert_aux<GameGem>`.

Fix: removed the pad, renamed both map entries to the GameGem spellings
GameGemList.obj already defines, and moved their `.text` (plus the block holding
`~vector<GameGem>`) from `ColorPalette.cpp` to `GameGemList.cpp`. `0x8278DEF0`
fills the exact hole in GameGemList's span. This is a re-home, which is not
metric-neutral by design. It was priced inside the §1 A/B: +3 matched, 0 rows
down.

### 4.4 Behavioural fixes riding along

- **`UIComponent::Copy` never dispatched `CopyMembers`.** So `UILabel::CopyMembers`
  and the new `InlineHelp::CopyMembers` were dead on Copy. Retail calls it.
- **`UIManager::Terminate` removed an unregistered callback.** It removed
  `TerminateCallback`, but `Init` adds `UITerminateCallback`.
- **`UIManager::Poll` nulled `mTransitionScreen` in the pop branch.** Retail and
  rb3-Wii keep it.
- **`Multiply(Matrix3…)` alias case.** The first rewrite of the `b == out` path
  wrote rows before reading all of `b`. Caught by reading retail, which loads all
  nine `b` elements before any store. The committed version computes every row
  first.
- **`WorldInstance` had two rev stores.** PreLoad wrote one and PostLoad another
  (`sPersistRev`), where retail has one. `LoadPersistentObjects` now reads the rev
  PostLoad just popped.

## 5. What I did not do

- Nothing in `src/system/{bandobj,rndobj,char,rnddx9}`. `CrowdAudio.cpp`
  (bandobj) textually includes `ui/InlineHelp.cpp`, so it recompiles with these
  edits; the file itself is untouched.
- No alias or `symbols.txt` edits. The map edits are the nine renames in
  §4.2/§4.3; the splits edit is the §4.3 re-home.
- I left `UIList::PreLoadWithRev(BinStreamRev&)` in place for BandList/HamList
  (other lanes' files) and added rb3-Wii's `(BinStream&, int)` overload beside it.
- Permuter not run (standing directive).


## 6. Native gate

`tools/native_build_gate.sh` on the branch tip `b56645bf` (all source commits in
place), run after the A/B as the last build action:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Only this docs-only commit follows it. It touches no build input.

