# The spatial screen's remaining fires — lane W10-A, 2026-09-13

Branch `w10-spatial-fires`, worktree `~/tmp/wt-w10-a`, base main **`dee126a1`**
(asserted ancestor before editing). Ruler **`name_check` (graded)**, read from
`report.json`'s `provenance.diff_config` — not assumed. Predecessor:
`docs/decomp/SPATIAL_SCREEN_2026-09-13.md` (lane W9-A, merge `88c944d4`).

Baseline at this lane's first full build (full build first — a reflinked tree's
target objs are pre-renamer; the build's own `CHECK TARGET OBJS RENAMED` step
reported **25,523 / 29,034 map names present (87.9%)**, so no mangled-name
lookup below is vacuous):

```
matched_functions   42,749
matched_code        3,871,308 B
matched_code_percent  37.783768
fuzzy_match_percent   49.134560
total_code          10,245,956
```

That reproduces W9-A's post-rebase figures to the last digit, which is the
control that main and this worktree are the same tree.

---

## 0. Headline

- **Re-ran the screen rather than inheriting its fire list** (W9-A's own
  instruction). Main had moved by 8 map rows — W9-A's two fixes plus the
  PlatformMgr four-name cycle (`74063c30`). Result: **37 fires**, which is
  W9-A's 39 minus exactly its own two fixes, and the ≥44 B stratum is **19**
  (21 − 2). The PlatformMgr repair neither added nor removed a fire.
- ⛔ **The first briefed blocker was FALSE, and testing it literally was worth
  the whole lane.** W9-A deferred `0x826B03D8` because *"`RGTrainerPanel.cpp`
  has no `.text` pin at all"*. It has **five**. The heading is
  `band3/game/RGTrainerPanel.cpp`, and `grep '^RGTrainerPanel.cpp:'` returns 0
  — the bare-vs-nested heading trap CLAUDE.md says has already broken four
  lanes. This was the fifth, and it presented as a legitimate "not applicable".
- ⛔ **The second briefed lead's proposed NAME is refuted.** W9-A suggested
  `0x82605CA8` is `?SyncProperty@BandStorePanel@@`. The compiler layout puts
  `MsgSource` at **0x88** in BandStorePanel and nothing at 0x60, while the body
  adjusts `this` by **−0x60** before tail-calling `?SyncProperty@StorePanel@@`.
  Still a true positive; still not that name.
- ★ **A second, independent instrument fell out of one fire and found rows the
  spatial screen structurally cannot see** — the `RegisterFactory` /
  `StaticClassName` identity screen (§3). 21 bodies whose registered class
  disagrees with their map name or that are unnamed entirely.
- Four changes measured, **every one predicted before measuring**:
  **+2 functions / +196 B** of bytes, plus two deliberate accuracy repairs.
- **Updated FP rate — and it moves in BOTH directions, which is the honest
  result.** Seven of W9-A's nine unresolved fires are now adjudicated, and most
  went FP. Over the screen's whole lifetime (W9-A's original 21):
  **FP 13/19 = 68.4% adjudicated** (precision 31.6%) against W9-A's 66.7%, and
  **conservative bound 15/21 = 71.4%** against its 81.0%. So the optimistic
  figure got slightly *worse* and the pessimistic figure got substantially
  *better* — which is exactly what quoting both was for. ⚠ My own first draft
  of this bullet said "60.0% / 68.4%, both better"; recomputing killed it.

---

## 1. The re-derived fire list, with provenance

`python3 tools/spatial_map_screen.py --json ~/tmp/w10a_fires.json`, on the map
at `dee126a1`. The tool's three controls run on every invocation and all passed
in the same run that produced these findings:

```
CONTROL vacuity     PASS  participating=17487 suppressed=11646
CONTROL negative    PASS  0x824302B0 silent on the current map
CONTROL positive    PASS  0x824302B0 flagged with W8-C reverted
                          (owner=RndEnviron host=RndPostProc flank=5 dist=0x1ea60 host_lacks_Save=True)
```

**37 fires**, of which **19** are in the ≥ 44 B adjudication stratum W9-A fixed
before adjudicating (below ~40 B a retail body is 2–6 generic instructions and
cannot discriminate between candidate names — that stratum measures the
instrument's blind spot, not the screen). I kept W9-A's stratum boundary rather
than re-choosing it, precisely so the FP rates compose.

★ **Arithmetic worth stating, because it is a control on the screen itself:**
39 − 2 = 37 and 21 − 2 = 19. The only fires that disappeared are the two W9-A
fixed, so the screen is silent on its own repairs — the negative control
holding on live data, not just on the fixture.

### 1.1 A cheap discriminator I added: does the PIN agree with the NAME?

The screen never reads `splits.txt`, so "which unit currently pins this
address" is independent information. It is *not* decisive on its own — W8-C's
whole point was that a pin can be moved to agree with a wrong name — but as a
prior it sorts the stratum usefully:

| addr | size | fuzzy | owner (map name) | pinned unit | pin agrees |
|---|---:|---:|---|---|---|
| `0x82351420` | 276 | 0.00 | GamePanel | BandTrack | no |
| `0x826b03d8` | 224 | 0.00 | TexMovie | TexMovie | yes |
| `0x8232bfb0` | 212 | 100.00 | Message | BandWardrobe | no |
| `0x826b94b0` | 184 | 100.00 | VocalNoteList | VocalTrainerPanel | no |
| `0x824523f8` | 156 | 100.00 | Spotlight | TrackDir | no |
| `0x8247c820` | 156 | 100.00 | RndParticleSys | Part | yes |
| `0x825e0328` | 128 | 100.00 | PassiveMessageQueue | OvershellSlot | no |
| `0x82605ca8` | 120 | 99.83 | RndParticleSysAnim | TrackWatcherImpl | no |
| `0x82469750` | 104 | 100.00 | ReclaimableAlloc | MeshAnim | no |
| `0x8231a0d8` | 96 | 100.00 | AppMiniLeaderboardDisplay | AppMiniLeaderboardDisplay | yes |
| `0x82815870` | 80 | 100.00 | UIListWidget | UIListSlot | no |
| `0x8235cc80` | 72 | 100.00 | AccomplishmentManager | Rnd_Xbox | no |
| `0x82b5d278` | 72 | 100.00 | Synth | Synth | yes |
| `0x82688298` | 72 | 100.00 | MultiplayerAnalyzer | SongDB | no |
| `0x8258bab8` | 68 | 100.00 | Tour | Tour | yes |
| `0x824444e8` | 64 | 100.00 | TexMovie | TexMovie | yes |
| `0x826ac0e8` | 52 | 100.00 | GemTrainerLoopPanel | GemTrainerPanel | no |
| `0x823a1d38` | 52 | 98.85 | Sfx | CharMeshHide | no |
| `0x82874718` | 44 | 99.55 | RGTrainerPanel | RGTrainerPanel | yes |

⚠ Note `0x82605ca8`: name says RndParticleSysAnim, host says BandStorePanel,
pin says TrackWatcherImpl — a **three-way** disagreement, which is why it is
the hardest row in the set and why I did not move it.

---

## 2. Per-fire adjudication

Verdicts are on **retail bytes** — the `.s` body at the address (keyed on the
`.fn fn_<addr>` symbol, never the synthetic address column), its callees
resolved through the map, its `this`-relative offsets — checked against
compiler-authoritative class layouts. A "fold" verdict counts as FP, as in
W9-A: the screen's claim is *candidate map defect*, and a fold does not
vindicate it.

### 2.1 TRUE POSITIVES

| addr | map name | what it really is | evidence | disposition |
|---|---|---|---|---|
| `0x826b03d8` | `??0TexMovie@@IAA@XZ` | **`??0RGTrainerPanel@@QAA@XZ`** | body calls `??0RGGemMatcher@@QAA@XZ` for a sub-object at `+0x11c`; `TexMovie : RndDrawable, RndPollable` cannot contain one. Neighbours are `?Poll@RGTrainerPanel@@` / `?ClassName@RGTrainerPanel@@`; `??1RGTrainerPanel@@` right after at `0x826B0790`; `??0RGTrainerPanel@@` absent from the map | **FIXED, +1 fn / +100 B** |
| `0x82874718` | `?Exit@RGTrainerPanel@@UAAXXZ` | a **D3D XBM capture helper** (exact name unknown) | sits dead centre of an unbroken `?XBM*@D3D@@` run and calls `?XBMEndCapture@D3D@@`; the real `Exit` is `0x826ADC38` | **FIXED (name → null), Δ0** |
| `0x823a1d38` | `?Init@Sfx@@SAXXZ` | **`?Init@CharMeshHide@@SAXXZ`** | the body *is* `RegisterFactory(CharMeshHide::StaticClassName(), …)`; a Milo `Init()` registers its **own** class | **NOT fixed — chain, see §3.2** |
| `0x82605ca8` | `?SyncProperty@RndParticleSysAnim@@` | a `StorePanel` subclass override, class **unsettled** | body does `subi r3, r29, 0x60` then tail-calls `?SyncProperty@StorePanel@@`; `RndParticleSysAnim : RndAnimatable` has no StorePanel base | **NOT fixed — identity unsettled** |
| `0x8228c878`† | `?Init@BandCamShot@@SAXXZ` | **`?Init@BandCharacter@@SAXXZ`** | registers `BandCharacter::StaticClassName` | **FIXED, +1 fn / +52 B** |
| `0x8264d058`† | `?Init@CharMeshHide@@SAXXZ` | **`?Init@AppMiniLeaderboardDisplay@@`** | registers `AppMiniLeaderboardDisplay::StaticClassName` | **NOT fixed — chain, see §3.2** |

† not spatial-screen fires — found by the second instrument (§3).

### 2.2 FALSE POSITIVES — the same one mechanism W9-A identified

W9-A's eight decisive FPs are unchanged (a class whose methods are split across
more than one TU legitimately has a member inside another class's run). I
re-derived four more, all on the pin-agreement + body-consistency pair:

| addr | map name | why it is legitimate |
|---|---|---|
| `0x8247c820` | `?UpdateSphere@RndParticleSys@@` | pinned in `Part.cpp`, which **is** RndParticleSys's own TU; `lbz -0x18(r31)` is the secondary-base `this` a `RndDrawable` vtable slot receives |
| `0x8231a0d8` | `?DrawShowing@AppMiniLeaderboardDisplay@@` | pinned in its own TU; body is WorldXfm_Force / SetWorldXfm / `RndDrawable::Draw` |
| `0x824444e8` | `?OnGetRenderTextures@TexMovie@@` | pinned in its own TU; a 16-instruction forwarder to the free `?GetRenderTextures@@YA…@Z` |
| `0x8258bab8` | `?HasTourDesc@Tour@@QBA_NVSymbol@@@Z` | pinned in its own TU; the only call is `_Rb_tree<Symbol, pair<const Symbol,bool>>::_M_find(Symbol)` — exactly a `map<Symbol,bool>` lookup, which is what a const `HasX(Symbol) -> bool` is |
| `0x824523f8` | `?UpdateSphere@Spotlight@@` | pin does **not** agree (TrackDir), but the body takes `addi r3, r31, 0x24` as its `RndTransformable` sub-object, which fits `Spotlight : RndDrawable, RndTransformable, RndPollable` (RndDrawable is ~0x24) and **not** `RndGroup`, which has three bases and puts its transformable much later |

⚠ **`0x824523f8` and `0x8247c820` are both `UpdateSphere`, both 156 B, both at
fuzzy 100 — the byte-identical-family shape the task warns about.** I tested it
rather than assuming: reloc-normalised, they are **NOT** identical (`lbz
0xc0(r31)` vs `lbz -0x18(r31)`), so neither score is being carried by a
forgiven anonymous relocation on the other's behalf, and each stands alone.

### 2.3 STILL UNRESOLVED (2 of 19)

| addr | map name | what I established | what is missing |
|---|---|---|---|
| `0x82351420` | `?SetGameOver@GamePanel@@QAAX_N@Z` (276 B, fuzzy 0) | **narrowed, not solved.** Our `GamePanel::SetGameOver()` is a header-inline **no-arg** one-liner while the map name takes a `bool` — a signature contradiction of the MPNGAP-1 kind. Pin **and** host both say `BandTrack`. `?SetGameOver@Game@@QAAX_N@Z` already exists at `0x82677228`, so it is not that either | the body's three string constants (`lbl_8201D58C`, `lbl_82000C55`, `lbl_8201D5F8`) would name it, but those `.rdata` labels are not in the split set, so they are not cheaply readable |
| `0x8235cc80` | `?GetAward@AccomplishmentManager@@` (72 B, fuzzy 100) | pinned in **`Rnd_Xbox.cpp`**, host `Tour` — pin, host and name all disagree. Its one call is `hashtable<pair<const Symbol,int>>::_M_find`, i.e. a `hash_map<Symbol,int>`, while the name returns `Award*` | not decisive: the map could legitimately be `Symbol → index` |

**I did not adjudicate the 18 fires below 44 B** — same reason as W9-A: not
"clean", *not testable* by retail bytes.

---

## 3. A second instrument, which fell out of one fire

`0x823a1d38` adjudicated in one step because a Milo `X::Init()` **states its own
identity**: the whole body is

```
Hmx::Object::RegisterFactory(X::StaticClassName(), X::NewObject)
```

so the class named by the `StaticClassName` callee **is** the owner. That is a
self-witnessing shape, and it generalises into a screen with no thresholds and
nothing to tune.

### 3.1 The screen, and its own false alarm

Over all 74 `?Init@…@@SAXXZ` map rows, **4** register a different class. One is
legitimate: `?Init@Sequence@@` registers **six** subclasses (`SfxSeq`,
`WaitSeq`, `ParallelGroupSeq`, `RandomGroupSeq`, `RandomIntervalGroupSeq`,
`SerialGroupSeq`) — the base-registers-its-subclasses idiom. So the predicate
is "the body registers exactly ONE class **and that class differs from the map
row's owner**".

Widened from map rows to **every body that calls `RegisterFactory`**, so it also
sees rows that are *unnamed*, it returns 21. ⚠ **One of those 21 is a false
alarm of my own making**, and it is worth recording because it shows the
predicate has to be about the CLASS, not the method: `0x8232ffb8` is
`?Register@BandWardrobe@@SAXXZ` registering `BandWardrobe` — `Register()`
legitimately has this exact body (it is what `REGISTER_OBJ_FACTORY_FUNC`
emits), so only a **class** disagreement is a defect. With that corrected:

| class | n | note |
|---|---:|---|
| **mis-named** (registered class ≠ map owner) | **3** | `0x8228C878`, `0x823A1D38`, `0x8264D058` |
| **unnamed** (identity proven by the body, no map name) | **17** | a pure identification vein |
| false alarm of the method-name form of the predicate | 1 | `?Register@BandWardrobe@@` |

★ **This screen sees what the spatial one structurally cannot.** `0x8228C878`
sits *inside BandCharacter's own unbroken run* — exactly where a BandCharacter
method belongs — so no spatial rule can fire on it. The relationship is the
same as the one W9-A described between its screen and
`map_lint --check class_mixing`: complementary frames, neither subsuming the
other.

### 3.2 The chain, and why two of the three are still open

The three mis-named rows are not independent; two form a displacement chain:

```
0x823A1D38  map says ?Init@Sfx@@            body registers CharMeshHide
0x8264D058  map says ?Init@CharMeshHide@@   body registers AppMiniLeaderboardDisplay
```

so renaming the first **requires** renaming the second in the same change — both
are pinned into `CharMeshHide.cpp`, i.e. the same target obj, and two rows
cannot carry one name there. And the second needs a **pin move** as well: our
`AppMiniLeaderboardDisplay.obj` defines `?Init@AppMiniLeaderboardDisplay@@`, but
the target obj for `0x8264D058` is `CharMeshHide.obj`, which cannot. I left this
one for a follow-up rather than bundle a three-part coupled change into a lane
that had already landed four measured changes.

`0x8228C878` was the clean member — pin already correct, name free — and is
committed.

### 3.3 The alias file had already seen this, and modelled it wrongly

⚠ Per the standing rule (*grep `symbol_aliases.json` before believing a
relocation-name find*) I checked, and it was the most interesting corroboration
of the lane.

`scripts/symbol_aliases.json` `groups[249]` records **T1 evidence** — *"RB3
retail bytes at the survivor address are byte-identical (modulo relocated
fields) to our compiled body for `?Init@BandCharacter@@SAXXZ`"* — at address
`0x8228c878`. That is precisely my claim, reached independently.
`icf_alias_build.py` explained the byte identity as an **ICF fold**; lane
ALIAS-REPAIR (2026-08-19) then withdrew the membership as
`UNDER_PARTITIONED_ICF_CLOSURE` because the resolved operands disagree with the
survivor. `groups[1105]` has the same shape for `0x8264d058` /
`?Init@AppMiniLeaderboardDisplay@@`.

**Both steps reasoned correctly about the right observation with the wrong
model.** The simplest explanation was never on the table: the address simply
*is* the other class's function, and the map calls it something else. ⇒ **a
mis-named map row PRESENTS TO AN ICF ALIAS BUILDER AS A FOLD CANDIDATE**,
because the retail body really is the other class's body. Anyone triaging
`UNDER_PARTITIONED_ICF_CLOSURE` withdrawals should test "is this row simply
mis-named?" before reaching for a partition.

---

## 4. The changes — predicted vs measured

Every A/B: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w10-a --from-dirty`,
one change per run, ruler `name_check`, per-unit effects checked, committed
immediately.

| # | change | kind | predicted | measured | per-unit |
|---|---|---|---|---|---|
| 1 | `0x826B03D8` → `??0RGTrainerPanel@@` + close the byte-exact pin hole (**coupled**) | map+splits | Δfns **[+1,+2]**, Δbytes **[+100,+324]** | Δfns **+1**, Δbytes **+100**, Δcode% +0.000972pp, Δfuzzy +0.002070pp | unit net (ALL units) **+1** == whole-binary Δmatched; units@100 162→162, **0 fell off** |
| 2 | `0x8228C878` → `?Init@BandCharacter@@` | map-only | Δfns **+1**, Δbytes **+52** | Δfns **+1**, Δbytes **+52**, Δcode% +0.000510pp, Δfuzzy +0.000000pp | +1; 0 fell off |
| 3a | `0x82874718` → null (D3D helper) and `0x826ADC38` → `?Exit@RGTrainerPanel@@`, pins moved | map+splits | Δfns **0**, Δbytes **0** | Δfns **+0**, Δbytes **+0**, Δcode% +0.000000pp, Δfuzzy **−0.000210pp** | 0; 0 fell off; `none` **−44 B** |
| 3b | `RGTrainerPanel::Exit` clears `TheRGTrainerPanel` (`stw`) | source | Δfns **+1**, Δbytes **+44** | Δfns **+1**, Δbytes **+44**, Δcode% +0.000430pp, Δfuzzy +0.000024pp | +1; 0 fell off; 1 leg-B recompile |
| 4 | name 9 anonymous `Init` bodies | map-only | Δfns **[+5,+9]**, Δbytes **[+260,+468]** | **Δfns +0, Δbytes +0**, Δfuzzy **+0.006336pp** | 0 reached 100, **0 fell off**; `none` FLAT |

**Lane total: +3 functions / +196 B**, and the deltas **compose exactly** — the
final full build reads `matched_functions` **42,752**, `matched_code`
**3,871,504 B**, `matched_code_percent` **37.785680**, `fuzzy`
**49.142780**, i.e. +3 / +196 / +0.001912pp / +0.008220pp on the baseline, which
is the arithmetic sum of the four runs.

### 4.1 Two predictions landed on the nose, and both for the same reason

Changes 2 and 3b were predicted *exactly*, from the charge arithmetic rather
than from intuition. Per charged argument with `N = size/4`, a relocation-name
charge costs `5/N`:

- `?BandInit@@YAXXZ` sat at fuzzy 99.96032, size 1008 ⇒ N=252 and
  `100 − 99.96032 = 0.03968 = 2 × 5/252` — **exactly two** relocation-name
  charges, so `objdiff`'s charged-site list was worth reading before predicting.
  It named both: one *was* my rename, the other was not fixable by me (§7.3).
  Because `matched_code` is all-or-nothing per row, closing one of two buys
  **zero** bytes from a 1008 B row — so I predicted +52 from the renamed row
  alone, and got +52.
- Change 1 was priced the same way: `?NewObject@RGTrainerPanel@@` at 99.8,
  size 100 ⇒ `100 − 99.8 = 0.2 = 5/25`, exactly one charge, which had to be the
  `bl` to the address being renamed. +100, measured +100.

★ **The rule this lane confirms: price from `report.json`'s charged-site list
and the `5/N` arithmetic, not from a mismatch count and not from the caller
count alone.** W9-A missed by 8× by not looking at callers at all; I got these
right by looking at callers *and* checking what their residual charge actually
was.

### 4.2 Change 4 is the one I got wrong, and it is the most useful row here

I predicted `[+260,+468] B` and measured **+0**. The reasoning was "each row is
a 13-instruction body our objs already define, so pairing completes it". Two
things were wrong with it:

1. ⛔ **I priced from the ASM BODY (13 instructions = 52 B) while the scored row
   is 88 B.** That is the targeting hazard CLAUDE.md records — *report.json
   sizes are not the body you were looking at*. Six of the nine rows are 88 B,
   `?Init@RndText@@` is 260 B and `?Init@DxMultiMesh@@` is 104 B.
2. **Pairing is necessary, not sufficient.** At 88 B, `N=22` and
   `100 − 59.0909 = 40.9 = 9 × (100/22)` — **nine of 22 instructions fully
   mismatch**. That is a real body difference, not a naming one.

★ **But the failure is informative in a way the success would not have been:
SIX of the nine land on EXACTLY 59.090908** (`MiniLeaderboardDisplay`,
`MeterDisplay`, `UIButton`, `UISlider`, `LabelShrinkWrapper`,
`LabelNumberTicker`; `BandLabel` is 58.636364). An identical score across six
independent UI/ham classes is one systematic cause, i.e. a **force-multiplier
body fix** — and it could not even be *asked about* while those rows were
anonymous and unpaired. That is the pairability-as-correctness-instrument
lesson: naming bought no bytes and made a defect visible.

---

## 5. The false-positive rate, re-measured

Same stratum rule as W9-A (≥ 44 B), fixed before adjudicating, not re-tuned
after. Same conservative convention: a "fold" verdict counts as FP.

| verdict | n | of 19 |
|---|---:|---:|
| **TRUE POSITIVE** | **4** | 21.1% |
| **FALSE POSITIVE** | **13** | 68.4% |
| **UNRESOLVED** | **2** | 10.5% |

Two denominators, because they answer different questions and only one of them
is comparable to W9-A's:

| | FP over adjudicated | conservative bound |
|---|---|---|
| W9-A, its 21 fires | 8/12 = **66.7%** | 17/21 = **81.0%** |
| **screen lifetime, the same 21** | 13/19 = **68.4%** | 15/21 = **71.4%** |
| this lane, the 19 that survived its fixes | 13/17 = 76.5% | 15/19 = 78.9% |

★ **The optimistic figure got slightly worse and the pessimistic figure got
substantially better.** That is the expected shape when a lane works down the
unresolved pile: W9-A's easy adjudications were the true positives, so the
residue skewed FP (76.5% on the surviving subset), while the conservative bound
— which had *assumed* all nine unresolved were FP — improved by 9.6 pp once
seven of them were actually examined. **Neither figure alone would have told
the truth**, which is the argument for W9-A's decision to quote both.

★ **And precision must still be read against selectivity.** 19 rows out of
17,487 participating (**0.11%**), of which 4 are real identity defects and two
have now been repaired for measured bytes. A ~30% precision at that selectivity
is a very large enrichment.

⚠ **The screen's yield per lane is falling, and the reason is structural, not a
defect.** W9-A took the two largest, most obviously-wrong rows (496 B and
708 B). What is left in this stratum is dominated by the one benign mechanism
W9-A already named — a class whose methods are split across more than one TU —
and the residue is two rows that resist retail-byte adjudication. **I would not
fund another pass over this stratum**; §7 says where the value went instead.

---

## 6. What this lane did NOT do

- **Did not adjudicate the 18 fires below 44 B.** Not clean — *not testable*.
  Unchanged from W9-A, and for the same reason.
- **Did not fix `0x82605CA8`** (`?SyncProperty@RndParticleSysAnim@@`, 120 B,
  fuzzy 99.83). It is a true positive — the body adjusts `this` by −0x60 and
  tail-calls `?SyncProperty@StorePanel@@`, which `RndParticleSysAnim :
  RndAnimatable` cannot do — but ⛔ **W9-A's proposed name is refuted**: the
  compiler layout (`class_layout_report.py BandStorePanel`, sizeof 276) puts
  `MsgSource` at **0x88** and nothing at 0x60, so it is not
  `?SyncProperty@BandStorePanel@@`. Name, host and pin disagree three ways.
  Moving it would risk un-pairing a row that currently scores 99.83, for an
  identity I cannot yet state.
- **Did not finish the `Init` chain** (`0x823A1D38` + `0x8264D058`). Both are
  decisively identified; the fix is a three-part coupled change (two renames
  that must move together plus a pin move to `AppMiniLeaderboardDisplay.cpp`)
  and I stopped rather than bundle it in behind four already-measured changes.
- **Did not name `0x82874718`.** It is a D3D XBM helper, but *which* one is a
  guess; it is set to `null`, following the map's own 109 deliberately-unclaimed
  rows.
- **Did not touch `scripts/symbol_aliases.json`.** I read `groups[249]`,
  `groups[250]` and `groups[1105]` and they corroborate two of the renames, but
  the corroboration is a *reading*, not an edit, and the groups' `folded` lists
  forgive nothing that my changes touch.
- **Did not patch `tools/map_lint.py`** (W9-A's handoff 4) or
  `tools/dc3_map.py` (its handoff 5). Still open, still shared helpers.
- **Did not re-tune any screen threshold**, and deliberately kept W9-A's ≥44 B
  stratum boundary so the FP rates compose.
- **Did not chase `0x82351420`'s string constants** past one attempt — the
  `.rdata` labels it references are not in the split set.

---

## 7. Handoffs

1. ★ **Finish the `Init` chain — it is fully diagnosed, only the mechanics
   remain.** `0x823A1D38` → `?Init@CharMeshHide@@SAXXZ` and `0x8264D058` →
   `?Init@AppMiniLeaderboardDisplay@@SAXXZ`, as **one** change (they collide on
   the name otherwise, and both are pinned into `CharMeshHide.cpp`), with
   `0x8264D058`'s 52 B pin moved to `band3/meta_band/AppMiniLeaderboardDisplay.cpp`
   because only that base obj defines the name. Both rows currently read 98.85.
2. ★★ **The 17 unnamed `RegisterFactory` bodies are an identification vein with
   the identity PROVEN BY THE BODY** (§3). This lane took the 9 that need no pin
   move; the remaining 8 need one:
   `?Init@PatchRenderer@@` (pinned BandSwatch), `?Init@BandList@@`
   (CharSignalApplier), `?Init@RndWind@@` (TransAnim), `?Init@DxParticleSys@@`
   and `?Init@DxMat@@` (CubeTex), `?Init@TexMovie@@` (WavMgr),
   `?Init@TextFile@@` (ViewSetting), `?Init@SynthSample360@@` (SynthSample —
   and this one's base obj does **not** define the name, so it needs source
   work too). Each is ~52 B.
   ⚠ **Price the naming bet first**: naming an anonymous address converts
   *forgiven* placeholder call sites into *checked* ones, so a caller at fuzzy
   100 can fall off. In this wave none could — every caller was an init
   aggregator already sub-100 (`?BandInit@@` 99.9603, `?PreInit@Rnd@@` 99.9673,
   `?Init@UIManager@@` 99.9896) — which is why the downside was bounded at zero
   bytes. **Re-derive that, do not inherit it.**
3. ★ **`?BandInit@@YAXXZ` is 1008 B behind ONE remaining charge, and it is
   probably an alias, not a bug.** Its charged-site list had exactly two
   `diff_arg` sites; this lane closed one (`?Init@BandCamShot@@`). The survivor
   is target `?insert@list<Hmx::Object*>` vs our `?insert@list<void(*)()>` at
   `TheDebug.AddExitCallback(BandTerminate)`. Those are **identical machine code
   for any pointer element**, i.e. the ICF fold shape (`TEMPLATE_ARGS_DIFFER`
   *is* what a fold looks like) — so **changing our container type would be
   wrong**; our `AddExitCallback(void(*)())` is semantically right. If
   `tools/icf_alias_finder.py` can prove the fold on retail bytes, that single
   group is worth **+1008 B**, and `?PreInit@Rnd@@` (1836 B, three charges) and
   `?Init@UIManager@@` (1916 B, one charge) are the same shape of prize.
4. **`0x82351420`** (276 B, fuzzy 0) is narrowed but open. Our
   `GamePanel::SetGameOver()` takes no arguments while the map name takes a
   `bool`; pin and host both say `BandTrack`; `?SetGameOver@Game@@QAAX_N@Z` is
   already taken at `0x82677228`. Whoever can read the `.rdata` at
   `lbl_8201D58C` / `lbl_82000C55` / `lbl_8201D5F8` can probably name it in one
   step — those are its EventTrigger name and two `Symbol` literals.
5. ⛔ **Two of W9-A's own handoffs were wrong, and both failed *quietly*.**
   Handoff 2 said `RGTrainerPanel.cpp` has no `.text` pin (it has five — bare vs
   nested heading); handoff for `0x82605CA8` proposed
   `?SyncProperty@BandStorePanel@@` (refuted by the compiler layout). Neither
   error looked like an error. **Test a briefed obstacle literally before
   accepting it** — the first one was worth +100 B and a whole defect class.
6. **`?Init@Sequence@@` is a worked example of a benign multi-registration**
   and should stay on any future screen's allowlist: a base whose `Init()`
   registers its subclasses is correct, not a defect.
7. **Re-run both screens after any pin wave** (W9-A's handoff 6 still stands),
   and note the spatial screen's residue in the ≥44 B stratum is now down to two
   rows — the value has moved to the identification vein in (2).

---

## 8. Reproducing

```bash
python3 tools/spatial_map_screen.py --json ~/tmp/w10a_fires.json   # 37 fires; controls run first
python3 tools/spatial_map_screen.py --break-control positive       # must exit 3
python3 tools/icf_alias_finder.py --validate                       # 0 contradicted
```

The `RegisterFactory` screen of §3 was run ad hoc and is not committed as a
tool; it is ~30 lines — index every `.fn` body in `build/45410914/asm/`, keep
those calling `?RegisterFactory@Object@Hmx@@SAXVSymbol@@P6APAV12@XZ@Z`, take the
set of `?StaticClassName@X@@SA` callees resolved through the map, and report
the bodies where that set has exactly one element which is not the map row's
owner class. ⚠ Two things it MUST do or it lies: resolve callees **through the
map** (the `.s` shows only `bl fn_XXXXXXXX`), and key on the **`.fn fn_<addr>`
symbol**, never the `.s` address column, which is synthetic past a block
discontinuity. Promoting it to `tools/` is a reasonable follow-up; I did not,
because a tool with no measured FP rate is exactly what W9-A's §3 warns
against, and its only measurement so far is 3 defects + 1 self-inflicted false
alarm out of 21.
