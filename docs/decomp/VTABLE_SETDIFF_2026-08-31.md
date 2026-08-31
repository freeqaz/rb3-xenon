# Lane SETDIFF — six vtable `SET_DIFFER` slots, adjudicated on retail bytes

**Date:** 2026-08-31 · **Branch:** `setdiff-vt` · **Base:** `843c7e98`
**Scope:** the 6 slots over 5 classes of `tools/vtable_order_sweep.py --sweep`'s
9 `SET_DIFFER` rows, excluding StreakMeter / AppLabel / GameMicManager (owned by
the concurrent lane #548 SLOTMAP).

**Headline:** `SET_DIFFER` **9 → 3**, `SAME` **967 → 973**. The 3 that remain are
exactly the 3 out of scope. Net **+3 matched functions / +400 matched bytes**,
with **0 units falling off either ruler** on any of the three A/B runs.

| # | change | Δmatched | Δcode_bytes | Δcode% |
|---|---|---:|---:|---:|
| 1 | Family A — `TrackWatcherImpl` slots 11/12 + map | **+2** | **+340** | +0.003319pp |
| 2 | Family C — `DxShaderMgr` slot 11 map + missing body | **+1** | **+60** | +0.000584pp |
| 3 | Family B — 3 map renames (ModifierMgr ×2, BandSongMgr) | +0 | +0 | +0.000000pp |
| | **total** | **+3** | **+400** | **+0.003903pp** |

All three measured with `tools/ab_measure.py --from-dirty`, graded
`functionRelocDiffs=name_check` ruler, both legs settled to zero work, both legs
at a `symbols.txt` split fixed point.

---

## 0. What refutes the brief

Three of the brief's load-bearing claims did not survive contact with the bytes.
Listing them first, per house rule.

1. **⛔ The declaring header for Family A is NOT `BeatMatchControllerSink.h`.**
   `class TrackWatcherImpl {` has **no base class**; it includes that header only
   for the `GemHitFlags` enum. The transposed pair is declared in
   `TrackWatcherImpl.h`. The brief's slot arithmetic happened to land on 11
   anyway, but off the wrong class — `BeatMatchControllerSink`'s own order is
   irrelevant to this vtable and must not be "fixed" to match it.

2. **⛔ Family B is refuted outright.** The brief framed it as "we override a
   virtual retail INHERITS" and warned that deleting an override changes
   behaviour. Retail **overrides all three exactly as we do**. No source change
   was warranted; all three are map defects. (This reproduces the sweep
   docstring's own Wave-6 score card: 6 map defects, 0 source defects.)

3. **⚠ The missing Family A mirror mismatch is not a fold.** The brief
   pre-empted "only one mismatch where a transposition predicts two" by pointing
   at RealGuitar's 37 folded slots. The real reason is different: RealGuitar's
   slot 12 is `0x8279eba0`, a **16-byte leaf stub that is not a `.pdata`
   BeginAddress**, so it has no map entry at all and is excluded as `unnamed`,
   not as folded. Same sub-`.pdata` stratum CLAUDE.md records under AUDIT-NC.

And one refutation of **my own** mid-lane reasoning, which is the most useful
line in this document:

4. **⚠ I "refuted" Family A with a circular test and had to retract it.** Retail's
   two `TrackWatcher` forwarders both read
   `lwz r3,0(r3); lwz r11,0(r3); lwz r11,IMM(r11); mtctr; bctr` and differ **only
   in `IMM`**. Reading the map's labels on those twins and concluding "retail
   agrees with our order" is exactly trap #3 in the sweep's own docstring — *an
   off-by-one is invisible to a test that uses the numbering being tested*. Both
   our source **and** the map were wrong, in the same direction, which is
   precisely why the naive check agreed with itself.

---

## 1. Family A — `TrackWatcherImpl` slots 11/12 (RealGuitar / Keyboard / Joypad)

**Verdict: our declaration order was wrong AND the map was wrong. Both fixed.**

Ours was `FretButtonDown(10) / RGFretButtonDown(11) / FretButtonUp(12)`.
Retail's is `FretButtonDown(10) / FretButtonUp(11) / RGFretButtonDown(12)`.

### The non-circular chain

The anchor must not be a name. It is a **string**:

1. Retail `0x82790fe0` loads `??_C@_0BD@BGNEMMNI@…` = `"(%2d%10.1f UP\t%d)\n"` —
   the literal token `UP` — and makes the two-arg float sink call
   (`lwz r3,0x30(r3); lfs f1,0x7c(r31); lwz r11,0x2c(r11); bctrl`). Our
   `BeatMatcher::FretButtonUp` is the only function in the tree shaped that way.
   ⇒ `0x82790fe0` **is** `BeatMatcher::FretButtonUp`, established with no map.
2. Its last call, decoded as a raw branch displacement from the retail word at
   `0x82791098`, is **`bl 0x8279d768`**.
3. `0x8279d768` = `lwz r11, 0x2c(r11)` ⇒ **slot 11**.
   `0x8279d750` = `lwz r11, 0x30(r11)` ⇒ slot 12.
4. Mirror control: `BeatMatcher::RGFretButtonDown` (136 B, no format string,
   calls `mSink->FretButtonDown(…, mNow)`) branches to `0x8279d750`.

⇒ `TrackWatcher::FretButtonUp` is `0x8279d768`, dispatching **slot 11**. The map
had these two twins swapped.

### Independent corroboration, using no names at all

| class | retail slot 11 | retail slot 12 |
|---|---|---|
| Joypad | `0x827a06e0` = `b KillSustainForSlot` (a fret **release**) | `0x826c3888`, the universal empty-`blr` fold group, **occ 1433** |
| Keyboard | `0x8279ff60`, unique | `0x826c3888`, same empty stub |
| RealGuitar | `0x8279eb28`, unique | `0x8279eba0` = `lwz r11,0xb8(r11); bctr` |

Neither Joypad nor Keyboard overrides `RGFretButtonDown`, so their slot 12
holding the inherited empty `RGFretButtonDown(int){}` is exactly right — and
**both their `FretButtonUp` bodies are non-empty**, so slot 12 cannot be
`FretButtonUp`. RealGuitar's slot 12 dispatches through `0xb8/4 = slot 46`,
which is `RecordFretButtonDown` — i.e. literally our
`RGFretButtonDown(int i) { RecordFretButtonDown(i); }`.

Definition order corroborates once more: `TrackWatcher.cpp` defines
`RGFretButtonDown` (line 165) before `FretButtonUp` (167), matching
`0x8279d750 < 0x8279d768`.

### Why the map got it wrong

The two forwarders are 20-byte twins identical but for one immediate — nothing a
shape-based fingerprint matcher can separate. `BeatMatcher`'s pair differ hugely
in shape and **are** named correctly; only the twin pair was swapped.

### Both halves must land together

Either edit alone mispairs both forwarders and measures negative: objdiff pairs
target↔base **by name**, so fixing the header without the map leaves our
slot-11-emitting `FretButtonUp` pinned to the slot-12 address.

**Measured:** Δmatched **+2**, Δhonest +2, Δcode_bytes **+340**, Δcode%
+0.003319pp. Only unit moved: `default/BeatMatcher` 82→84. Units at 100%
150→150 (mpn) and 122→122 (fuzzy), **0 fell off**.
Prediction was +340 B exactly. The **+2 functions was NOT predicted** — I
expected these arg-only rows to already sit at `mpn == 100`; they did not, so a
relocation-name `diff_arg` on a `bl` does move `mpn` here.

---

## 2. Family C — `DxShaderMgr` slot 11

**Verdict: our source order was right; the MAP was wrong — and the wrong pin was
concealing a function we never wrote.**

### objdiff is structurally unable to answer this

Our `SetPConstant(bool)` scored **100.0%** against retail `0x82735bf0`, the
address the map assigned it. That is not evidence. The target's callee is
`fn_8285D248`, a **placeholder** name, which `name_check` forgives — and the V
and P bool setters are byte-identical apart from that single `bl`. Our body
matches *either* of them at 100%. Same family as the CLAUDE.md rule that a
folded and a wrong callee score identically.

### Settled on bytes

```
0x82735bf0  … bl 0x8285d248      the two setters differ in ONE word
0x82735e88  … bl 0x8285d2a8

0x8285d248  … addi r11, r11, 0x9e0    the two callees differ in ONE instruction
0x8285d2a8  … addi r11, r11, 0x9e4
```

The callee computes `((StartRegister >> 5) + K) * 4`, so

* `K = 0x9e0` → `0x2780` = `m_Constants` (`0x480`) + `VertexShaderB` (`0x2300`)
* `K = 0x9e4` → `0x2790` = `m_Constants` (`0x480`) + `PixelShaderB`  (`0x2310`)

(offsets straight out of `src/xdk/d3d9i/d3d9.h`'s `_D3DConstants`).
⇒ `0x8285d248` **is** `D3DDevice_SetVertexShaderConstantB`, so slot 11 is the
**V** overload and the true `SetPConstant(bool)` is the previously-unnamed
`0x82735e88`.

### Three independent corroborations

* The 14 setters occupy **two contiguous address blocks in identical relative
  order** (`bf0 c30 c68 cc0 da0 e50` vs `e88 ec8 f00 f58 36038 36158`). The map's
  claim put `SetPConstant(bool)` at the *lowest* address of the V block, splitting
  the P block across both.
* The sole intra-vtable fold pairs slot 7 with slot 15 — exactly the V/P pair of
  the same argument type (`RndTex`).
* The four offsets a prior lane pinned from retail call sites
  (`// 0x18, 0x24, 0x3c, 0x40` = slots 6, 9, 15, 16) all agree, and with
  `RndTex` pinned at 15 the MSVC overload-reversal rule that
  `rndobj/ShaderMgr.h:95` documents forces `SetPConstant(bool)` to slot **18**.

### The concealed defect

`DxShaderMgr::SetVConstant(VShaderConstant, bool)` was **declared in both
`ShaderMgr.h`s and defined nowhere in the tree**. The wrong pin hid it by letting
`SetPConstant(bool)` collect 60 bytes against the vertex function. Added the body
(mirror of the P version) plus the `D3DDevice_SetVertexShaderConstantB`
declaration. Fixing the map alone would have converted a falsely-scoring row into
a permanent 0% one.

**Measured:** Δmatched **+1**, Δcode_bytes **+60**, Δcode% +0.000584pp. Only unit
moved: `default/system/rnddx9/ShaderMgr` 29→30. 0 units fell off.
Predicted +60 B / +1 fn before the run; measured exactly that.
Native-neutral: `rnddx9/ShaderMgr.cpp` is not in the native build (`TheShaderMgr`
is a stub in `native/src/milo_link_stubs.cpp`).

---

## 3. Family B — ModifierMgr ×2, BandSongMgr ×1

**Verdict: refuted. Retail overrides all three exactly as we do. Map defects.**

### ModifierMgr slots 11/12 — the discriminator is FAN-OUT, not the body

Retail's bodies genuinely are `li r3,1` / `li r3,0` in a 28-byte frame, which
*looks* like the base's inline `{return true;}` / `{return false;}`. But **our
overrides collapse to the same thing** — `IsModifierUnlocked` is `{return true;}`,
so both of `IsActive`'s branches fold — so the bytes alone cannot separate the two
readings. What can, measured across the **37** retail vtables that inherit
`UIListProvider::Mat` at slot 2:

| slot | inherited (shared) | ModifierMgr |
|---|---|---|
| 11 `IsActive` | `0x82533618` in **21** of 37 (occ 234) | `0x825896e8` in **1** |
| 12 `IsHidden` | `0x823591e8` in **36** of 37 (occ 1235) | `0x825896c0` in **1** |

An inherited slot is *shared*; a unique `occ=1` address inside the class's own
`.text` cluster is an *override*. The map corroborates itself — every other
`occ=1` slot-11 entry is spelled `?IsActive@<ThatClass>@@`
(`OvershellPartSelectProvider`, `CymbalSelectionProvider`, `StoreMenuProvider`).
Only ModifierMgr got the base's name.

### BandSongMgr slot 13 — settled by the body alone

`0x8257ad58` is
`lwz r11,0x164(r3); lwz r9,0x160(r3); subf; divwu r11,r11,12; addic r10,r11,-1; subfe r3,r10,r11`
— `(end-begin)/12 != 0` over a `vector<String>` (12-byte elements) at
`this+0x160`, i.e. our `return !mContentAltDirs.empty()`. The base is
`{ return false; }`.

**Measured:** Δ0 on every key. `none` control **FLAT**, which the tool itself
classifies as *"consistent with a pure RE-name (reloc_eq makes renaming free on
both rulers)"*. Landed on accuracy, not on the metric, per the standing
directive. Aggregate Δfuzzy +0.000155pp, from:

| row | before | after |
|---|---|---|
| `HasContentAltDirs@BandSongMgr` | fuzzy 24.25 / mpn 24.88 | **72.50 / 75.00** |
| `IsActive@ModifierMgr` | 28.57 | 28.57 (name only) |
| `IsHidden@ModifierMgr` | 28.57 | 28.57 (name only) |

BandSongMgr improved 48pp because our override is now paired against the right
target instead of against our own base inline; it just did not cross 100, so it
contributed 0 bytes.

---

## 4. Found but deliberately left alone

* **`ModifierMgr::IsActive` / `IsHidden` sit at 28.57%.** Retail's are 28-byte
  trivial returns; ours do not collapse that far. A real body/inlining
  divergence, but a body-port question rather than a vtable-order one.
* **`0x8279eba0` is still unnamed** — it is provably
  `RealGuitarTrackWatcherImpl::RGFretButtonDown` (see §1), but it is a
  sub-`.pdata` 16-byte leaf, so the renamer has no carved symbol to rename and
  naming it is a separate, separately-measured bet. Not attempted.
* **ModifierMgr slot 10** is `withheld` as `nonvirtual_name`: the map calls
  `0x82367fb0` `?GetNumGigs@TourDesc@@QBAHXZ`, a non-virtual, where we have
  `NumData@ModifierMgr`. Both are `int f() const`, so this is plausibly a real
  ICF fold-alias rather than a defect. Not adjudicated — the sweep correctly
  declined to charge it, and it is not one of the six.
* **StreakMeter / AppLabel / GameMicManager** — untouched, out of scope
  (lane #548 SLOTMAP owns those `$4PPPPPPPM@` adjustor thunks).

## 5. Reusable lessons

* **A twin pair distinguished only by an immediate cannot be labelled by shape**,
  so any map name on it is a coin flip — and reading it back is circular. Anchor
  on a **string constant** and follow raw branch displacements.
* **`occ` (fan-out across all retail vtables) is the instrument for
  "inherited vs overridden"**, and it works even when the two candidate bodies
  are byte-identical. A shared base implementation must appear in many tables.
* **A 100% objdiff row is not evidence of callee identity** when the target's
  callee is a placeholder name — `name_check` forgives it, so both the right and
  the wrong twin score 100.
* **A wrong map pin can hide missing source**, and the byte-level symptom is
  benign (a clean 100%). The DxShaderMgr row had concealed an entirely
  unimplemented virtual for as long as the pin existed.

## 6. Verification

* `scripts/verify_objs_patched.py --check` → `OK: tree is a fixed point of 6
  post-compile passes`.
* Sweep re-run at the branch tip: `SET_DIFFER 3` (was 9), `SAME 973` (was 967),
  `PERMUTED 0`, charged slots 5102 → 5103.
* Native gate result line is recorded in the lane report.
