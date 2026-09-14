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

_(filled in after the build)_

---

## What I did NOT do

_(filled in at the end)_
