# W16-U — four VT1 our-side divergences, nine weak contradictions, and a first re-home batch

Lane W16-U, branch `w16-u`, based on main `631973b1`. Picks up the three items
W16-S (`93fa4303`, doc `W16S_T1_ALIAS_IDENTIFICATION_VS_FOLD_2026-09-14.md`)
gathered evidence for and did not act on.

Standing rule applied throughout: **ACCURACY BEATS HEADLINE %**. `mpn` is
arg-blind and cannot register a wrong-callee or wrong-body fix, so a correct
fix that measures Δ0 — or negative — is landed on merit and said so.

Worktree baseline, measured before any edit (full build rc=0, read from
`build/45410914/report.json` with `int()`-coercion, defaults omitted):

    matched_functions  43272
    matched_code       3960812

---

## Item 1 — four addresses where retail provably folded two methods and our two bodies differed

**Committed `af0d4c1e`.**

### What the bytes showed, and the structural finding

All four addresses are **12-byte vtordisp adjustor thunks**, not method bodies:

    8163fffc   lwz   r11, -4(r3)
    7c6b1850   subf  r3, r11, r3
    4bxxxxxx   b     <target>

The first eight bytes are byte-identical across every thunk in the binary
(1,806 of them over 1,806 distinct destinations, by a scan of retail `.text`).
So **the fold at a thunk address is decided entirely by the branch
destination**, and the fix is never at the aliased address — it is always one
remove out, in the destination body. That reframes all four rows: the brief
asks "which of our two COMDATs matches retail", but at a thunk the question is
"which of our two *destination* bodies matches retail".

Each row is therefore a SLOT-level truth about the destination, and all four
destination bodies are now masked-equal to retail:

| thunk | → destination | retail | our survivor | our folded (fixed) |
|---|---|---|---|---|
| `0x823af220` | `0x823aee98` | 204 B | `CharData::Handle` 204 ✓ | `CharWeightable::Handle` 268→204 |
| `0x824863e8` | `0x82485880` | 120 B | `RndCamAnim::SyncProperty` 120 ✓ | `RndLightAnim::SyncProperty` 392→120 |
| `0x82493ca0` | `0x82493840` | 124 B | `RndMotionBlur::Save` 124 ✓ | `CharTransDraw::Save` 128→124 |
| `0x8234ebc8` | `0x826c3888` | 4 B (`blr`) | `BandTrack::Copy` 4 ✓ | `BandDirector::Replace` 112→4 |

The source fixes:

- **`src/system/char/CharWeightable.cpp`** — `HANDLE_VIRTUAL_SUPERCLASS` →
  `HANDLE_SUPERCLASS(Hmx::Object)` behind `#ifdef HX_NATIVE`. The virtual form
  emits the `ClassName() == StaticClassName()` guard, which is the 64 B surplus
  and the extra `?StaticClassName@CharWeightable@@` relocation.
- **`src/system/rndobj/LitAnim.cpp`** — three `SYNC_PROP` lines moved behind
  `#ifdef HX_NATIVE`, leaving `SYNC_SUPERCLASS(RndAnimatable)` only. This is
  the same shape lane CP-2 already landed on the sibling `RndCamAnim` in
  `rndobj/CamAnim.cpp`.
- **`src/system/char/CharTransDraw.cpp`** — `SAVE_REVS(2, 1)` → `SAVE_REVS(1, 0)`.
- **`src/system/bandobj/BandDirector.cpp`** — body gated to `#ifdef HX_NATIVE`;
  retail's is a bare `blr`.

### A negative result worth recording: the in-tree comment was wrong twice

`BandDirector.cpp` carried

    // retail 0x8227C290: two superclass Replace calls, then returns the
    // dynamic_cast<BandDirector*> of the first arg

Both halves are false. `0x8227C290` is **+0x78 inside `?BandInit@@YAXXZ`**
(0x8227c218, 1008 B) — a pre-TU5 address introduced in wave-24 `e7f71e90`, and
every TU0-era address has been invalid since main targeted TU5 on 2026-07-15.
And the body the comment *describes* is `?Copy@BandDirector@@` at `0x8228CD58`,
not `Replace` at all.

**A near-miss I caught and am recording because the next lane will hit it.**
Our 112 B `?Replace@BandDirector@@` masked-matches retail `0x8228cd58`. Reading
that as "our Replace is right" would have been wrong: **the masked comparison
is vacuous for call-shaped bodies**, because masking zeroes exactly the
relocated words that carry all the information. Decoding the `bl` destinations
showed retail calls `?Copy@Object@Hmx@@` twice ⇒ that body is
`BandDirector::Copy`, which is already at `fuzzy == 100`. No retail function
anywhere matches our `Replace`.

### Predicted vs measured

Predicted **Δ0 functions / Δ0 bytes** (the aliases already forgive these sites).

Measured, by set-diff of the `fuzzy == 100` row set on a full build:

    matched_functions  43272 -> 43272   (Δ0, exact — prediction correct)
    matched_code       3960812 -> 3960772   (Δ-40 B — prediction MISSED)

The miss is one row: `default/Waypoint :: fn_823DCC54`, 40 B, anonymous,
`masked_equal = true`, `mpn` unchanged at 100.0, `fuzzy` 99.5. It is an **EH
cleanup funclet** (it calls `~DataNode`) that objdiff pairs by byte signature
rather than by name. Retail's funclet uses frame `-0xB0`/`+0x60` where ours
uses `-0xA0`/`+0x58`; two other COMDATs in `Waypoint.obj` still carry retail's
exact shape. So this is objdiff's 1-to-1 funclet **assignment** shifting under
a changed candidate set — not a missing body and not a wrong one. Landed on
merit per the standing rule.

---

## Item 2 — the nine WEAK `CONTRADICTED_ON_RETAIL` rows

**Committed `4b681f7c`** (evidence text only; no `survivor`/`folded` moved).

All nine share one mechanism, and the census's WEAK label is accurate about it:
**9/9 have retail's callee DEFINED and our callee UNDEFINED**, so W16-S's
partition closure fails *closed* rather than concluding "not a fold".

The brief's lead — `??_GUIPanel` vs `??_EUIPanel` — resolves as **MSVC mangling,
not a source bug**. MSVC names the vtordisp adjustor `??_E<Class>@@$4…` and
points it at the *vector* deleting dtor, but only instantiates the vector form
on demand. In a compile-never-link build that reference dangles while
`??_G<Class>@@UAA…` is the body actually defined — which is precisely the
"retail callee defined, ours undefined" shape, arriving from the mangling
scheme rather than from us spelling anything wrong.

| class | rows | disposition |
|---|---|---|
| PROVEN fold | 3 | T1 evidence recorded in the group |
| REAL our-side divergence | 2 | flagged; membership left in place (see below) |
| absent vendor source | 2 | out of scope by standing directive |
| never instantiated | 2 | a use site would be metric fitting |

**Proven (evidence recorded):** `0x823d6938` RndTransformableRemover ↔
RndTransformable (the two `??_G` bodies are 80/80 B and differ *only* in the
`??_D` callee, and `??_DRndTransformableRemover` ≡ `??_DRndTransformable`
byte-identical at 108 B ⇒ folds under closure); `0x825ed5a0` ClosetPanel ↔
InterstitialPanel (`??_G` 88/88 B byte-identical *including relocation names*);
`0x82b908d8` TrackPanelInterface ↔ UIPanel (`??_G` 68/68 B byte-identical).

**Real divergences, deliberately NOT withdrawn:** `??_GGemTrainerLoopPanel` and
`??_GTourChallengeResultsPanel` are 80 B, routing through a `??_D<Class>` vbase
destructor, against the survivor's 68 B. Different-size COMDATs cannot fold
under any closure, so the alias is forgiving a real divergence. I did not
withdraw them because **the STLPORT-1 trap is exactly a size argument made with
a one-sided reader** — a size test cancels the artifact on both sides, so a
withdrawal here needs a two-sided normalization I did not build. Flagged in the
evidence text, which is the honest state.

⇒ **The brief's "act on (i) portable now" set is EMPTY**, with a per-row reason.
`0x827f42a8` (the SIZE row) untouched per the brief.

Measured Δ: **0 functions / 0 bytes, as predicted and structurally guaranteed** —
`gen_symbol_alias_map.py` renders `[survivor, *folded]` at one bucket address,
so an evidence-only edit cannot change the rendered map at all.

---

## Item 3 — first re-home batch (PREDICTION, pre-registered before the map edit)

### Screening

Of the 175 `IDENTIFICATION_NOT_A_FOLD` census rows: 137 have a report row for
the survivor S; **0 are at `fuzzy == 100`**, so a re-home cannot lose
already-matched bytes from that row; and **11 (10 distinct addresses) have the
new spelling N defined in the same base obj**, i.e. pairable after the rename.
The other 164 would un-pair permanently and are skipped.

### The mechanism, which makes this batch far safer than the raw caller counts suggest

Every one of the 10 is a **clean survivor swap**: N is *already* a folded member
of the group whose survivor is S. Two consequences:

1. **No membership is lost**, so no `withdrawn` record is needed and there is no
   clobber. (Group gi=24 at `0x822dea78`, which carries W16-I's restored
   `set<TrackWidget*>::clear` membership, is not in this batch at all.)
2. **The alias edit is metric-inert.** `gen_symbol_alias_map.py` emits
   `syms = [survivor, *folded]` all sharing the group's bucket address, and
   `parse_msvc_map` groups whatever shares an address. Swapping which member is
   `survivor` renders the same set at the same address ⇒ a byte-identical map.

Therefore the **caller-side risk is structurally near-zero**: a caller emitting
S while the target row now says N is forgiven by the group in either direction.
This retires my own earlier read that `0x822b4bd0` (`callers_emit_S` = 115) and
`0x827fb758` (804) were high-risk rows — those charges are forgiven by the
equivalence class, not by which name happens to be survivor. The only live
change is **which of our base-obj symbols the target row pairs against**.

### Retail-byte adjudication (re-verified here, not inherited from the census)

Masked compare of retail's `.pdata` extent against **both** our COMDATs:
**N matches 11/11; S matches 4/11**. Those 4 are *not* ambiguous — they are the
call-shaped bodies where masking destroys the evidence. Resolving each branch
destination by name through the map settles every one of them for N:

| addr | retail calls | our S calls |
|---|---|---|
| `0x822b4bd0` | `~EventCall` | `~BitmapOverride` |
| `0x825c2430` | `RemoteMachineLeftMsg::Type` | `NewRemoteMachineMsg::Type` |
| `0x827bd990` | `String::find_last_of(char)` | `FixedString::find_last_of(char)` |
| `0x827fb758` | `vector<Vector3>` copy ctor | `_Rb_tree<int,float>` copy ctor |

Map hygiene checked per row: the map currently spells S at all 10 addresses, N
is used at no other address (no duplicate-name collision), and N has no
existing target row.

**One irreducible choice, recorded rather than dressed up as evidence.**
`0x8266c440` has two competing N spellings, `~_Vector_base<Triangle>` and
`~vector<Triangle>`. Both are 36 B with the *identical* single relocation
`?MemOrPoolFreeSTL@@YAXHPAX@Z` at +0x1c, and retail's branch at +0x1c goes
exactly there — so the two fold together in our build and **ICF destroyed which
name the call site meant**. Either is equally correct; I take `~vector<Triangle>`.
The current S there (`~vector<pair<RndTexBlendController*,float>>`) is still
provably wrong — two non-relocated words differ — so the re-home is warranted
regardless of which Triangle spelling wins.

### PREDICTION (written before the edit)

Each row's target symbol is renamed S→N, so it pairs against our N COMDAT,
which is masked- and reloc-name-equal to retail. Each should reach
`fuzzy == 100`.

| # | addr | unit | size | fuzzy now | mpn now | predicted |
|---|---|---|---|---|---|---|
| 1 | `0x82b9b1f8` | GemManager | 92 | 0.00 | 0.87 | +92 B, +1 fn |
| 2 | `0x823ff400` | Tex | 268 | 91.91 | 93.10 | +268 B, +1 fn |
| 3 | `0x822b4bd0` | BandCamShot | 76 | 99.74 | 99.74 | +76 B, +1 fn |
| 4 | `0x827fb758` | UIList | 60 | 99.67 | 99.67 | +60 B, +1 fn |
| 5 | `0x82336af8` | BandCharDesc | 40 | 11.00 | 12.00 | +40 B, +1 fn |
| 6 | `0x825c2430` | BandMachineMgr | 132 | 99.85 | 99.85 | +132 B, +1 fn |
| 7 | `0x827bd990` | Str | 116 | 99.83 | 99.83 | +116 B, +1 fn |
| 8 | `0x8266c440` | TexBlender | 36 | 98.89 | 100.00 | +36 B, +0 fn |
| 9 | `0x826e3ce8` | VocalPlayer | 136 | 10.12 | 12.76 | +136 B, +1 fn |
| 10 | `0x82826b10` | LabelShrinkWrapper | 132 | 58.21 | 59.12 | +132 B, +1 fn |

**Aggregate prediction: +1,088 bytes, +9 functions, caller side Δ0.**

This is an upper bound in the sense that it assumes each renamed row reaches
exactly 100; it is *not* hedged downward, so a shortfall is a real miss to be
diagnosed and reported, not absorbed.

### MEASURED

Full build rc=0, set-diff of the `fuzzy == 100` row set, measured against the
post-item-1 state (43272 / 3960772):

    matched_functions  43272 -> 43281      (+9)
    matched_code       3960772 -> 3961860  (+1,088 B)

**Prediction exact on both measures.** All ten rows crossed to `fuzzy == 100`
for their full size, and the caller side moved by exactly 0.

| # | addr | unit | predicted | measured |
|---|---|---|---|---|
| 1 | `0x82b9b1f8` | GemManager | +92 B | +92 B ✓ |
| 2 | `0x823ff400` | Tex | +268 B | +268 B ✓ |
| 3 | `0x822b4bd0` | BandCamShot | +76 B | +76 B ✓ |
| 4 | `0x827fb758` | UIList | +60 B | +60 B ✓ |
| 5 | `0x82336af8` | BandCharDesc | +40 B | +40 B ✓ |
| 6 | `0x825c2430` | BandMachineMgr | +132 B | +132 B ✓ |
| 7 | `0x827bd990` | Str | +116 B | +116 B ✓ |
| 8 | `0x8266c440` | TexBlender | +36 B | +36 B ✓ |
| 9 | `0x826e3ce8` | VocalPlayer | +136 B | +136 B ✓ |
| 10 | `0x82826b10` | LabelShrinkWrapper | +132 B | +132 B ✓ |

Two rows read as LOST in the set-diff and **neither is a regression**:

- fuzzy `-40 B  default/Waypoint :: fn_823DCC54` — item 1's already-diagnosed EH
  funclet reassignment, carried in because the diff is against the lane baseline.
- mpn `-36 B  default/TexBlender :: ~vector<pair<RndTexBlendController*,float>>`
  — the **same address under its old name**. Report rows are keyed by
  (unit, symbol), so a rename necessarily reads as one row leaving and one
  arriving. This is also exactly why the function delta is +9 and not +10: that
  row was already at `mpn == 100` before the rename, which the prediction table
  states for row 8.

`python3 tools/icf_alias_finder.py --validate`: **PASS**, 0 CONTRADICTED
(1,383 map-consistent, 248 tolerated, 1,632 total).

Committed `52f1cc51`; prediction pre-registered in `373adf05`.

---

## Item 4 — W16-Q's three scattered bodies: DECLINED, with the evidence

Located precisely. All three are anonymous `fuzzy == 0` rows sitting in a
**neighbouring** unit:

| body | row | size |
|---|---|---|
| `CharWeightable` | `default/CharEyes :: fn_823AE888` | 144 B |
| `CharBonesMeshes` | `default/Rot :: fn_8237B338` | 216 B |
| `RndLightAnim` | `default/MeshAnim :: fn_82471518` | 140 B |

The mechanism is pairability, not source quality: the retail address is pinned
into the neighbouring unit, while our definition lives in a different TU, so the
neighbour's base obj cannot define the symbol and **objdiff pairs by name**.
Total upside if all three then matched: **500 B**.

Declined, for three reasons I want on the record rather than a shrug:

1. The fix is a **scatter-include** of a whole TU into its neighbour
   (`CharWeightable.cpp` → `CharEyes.cpp`, etc.). That duplicates *every* symbol
   in the included TU into the neighbour's object, which is precisely the shape
   that broke the native link in the `mtx.cpp` / `TexRenderer.cpp` incident.
2. It is a **splits/build-wiring decision with its own blast radius**, which
   W16-Q explicitly considered and deferred for the same reason. Re-homing the
   pin instead is not cheaper — CLAUDE.md records that re-homing an
   already-pinned address is *not* metric-neutral, and the address sits inside a
   contiguous retail TU block, so moving the pin would carve the neighbour.
3. It additionally needs a map name for each anonymous address, and naming an
   anonymous address **converts forgiven call sites into checked ones** — a bet
   that pays in bug exposure rather than bytes (MAPID-1).

500 B is not worth spending this lane's remaining budget on a change I could not
then gate properly. Handing it on intact.

**One finding worth passing along:** W16-Q listed `CharBonesMeshes` 216 B as a
*fourth* unpairable body, but `?SyncProperty@CharBonesMeshes@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z`
(216 B) now reads `fuzzy == 100` in `default/CharBonesMeshes`. The
216 B still-unpaired row is the separate anonymous one in `default/Rot` above.
A lane picking this up should re-derive the three rows rather than inherit
W16-Q's list — the ceiling moves both ways.

---

## What I did NOT do

- **Did not touch `scripts/symbol_aliases.json`'s `survivor`/`folded` to resolve
  item 1.** The brief forbids it and the aliases are proven; all four fixes are
  in source.
- **Did not withdraw the two real item-2 divergences**
  (`??_GGemTrainerLoopPanel`, `??_GTourChallengeResultsPanel`). The argument for
  withdrawal is a *size* argument, and the STLPORT-1 trap is exactly a size
  argument made with a one-sided reader — a size test cancels the artifact on
  both sides. A withdrawal needs a two-sided normalization I did not build.
- **Did not touch `0x827f42a8`** (the SIZE row), per the brief.
- **Did not touch group gi=24 at `0x822dea78`.** It carries W16-I's restored
  `set<TrackWidget*>::clear` membership from `dcd8d6fe`; it is not in this batch.
- **Did not re-home the other 164 `IDENTIFICATION_NOT_A_FOLD` rows.** Their new
  spelling is not defined in the same base obj, so a rename would un-pair them
  permanently (0% at any source quality). They need source or pinning work
  first, not a map edit.
- **Did not act on the 4 not-actionable item-2 rows** — two are vendor/middleware
  with absent source (out of scope by standing directive), two are a copy ctor
  that is simply never instantiated, where manufacturing a use site would be
  metric fitting rather than a fix.
- **Did not attempt item 4** (reasons above).
- **Did not re-audit the remaining T1 alias memberships** for the same
  identification-vs-fold confusion. W16-Q flagged that the T1 instrument is
  structurally one-sided, so others may carry the same error; that is a
  lane-sized sweep, not a tail-end task.

---

## Gates

Run in the worktree, in order, as the lane's last actions after the last source
edit. Gate lines pasted verbatim, not paraphrased.

1. Full build: `BUILD rc=0` (`~/tmp/rb3_build_w16u_3.log`).
2. `python3 scripts/verify_ruler_agreement.py --check` → rc=0:

       OK: both objdiff-cli entry points resolve the same ruler.

3. `python3 scripts/verify_objs_patched.py --verify-manifest` → rc=0:

       [patch-state] OK: 1210 decomp, 3093 target objects match 2026-09-14T14:47:58Z (tree_sha256=30c06d36c0be09b7)

4. `tools/native_build_gate.sh` → rc=0:

       NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0

   `skipped=0` as required.

---

## Lane totals

| | matched_functions | matched_code |
|---|---|---|
| baseline (`631973b1`) | 43,272 | 3,960,812 |
| after item 1 (`af0d4c1e`) | 43,272 | 3,960,772 |
| after item 3 (`52f1cc51`) | **43,281** | **3,961,860** |
| **lane net** | **+9** | **+1,048 B** |

The lane net byte figure is +1,048 rather than +1,088 because item 1 cost 40 B
to a funclet reassignment while closing four genuine our-side divergences at
proven retail folds. Under the standing rule that is the correct trade and it is
landed on merit: `mpn` is arg-blind and cannot register a wrong-body fix, so the
four item-1 corrections are worth exactly 0 to the metric and everything to
correctness.
