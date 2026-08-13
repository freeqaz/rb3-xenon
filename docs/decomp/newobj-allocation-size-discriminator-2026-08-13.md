# The allocation-size immediate: an UNMASKED discriminator for masked-class rows

Lane NEWOBJ-1, 2026-08-13. Instrument: `tools/newobj_size_screen.py`
(registered in `tools/screen_gate.py` as `newobj_size` + `newobj_ancestor`).

## Why this instrument is different

CLAUDE.md records **3,262 named rows / 368,948 B** whose name↔address assignment
is *unconstrained by the metric*, because objdiff masks relocations: a `bl` to
the right constructor and a `bl` to the wrong one score identically. Every audit
of that population so far needed **external** evidence (RTTI strings, vtable
membership, retail bytes read by hand).

`X::NewObject()` is a textbook member of the masked class — every one is
alloc-then-construct with an identical shape. But:

```
li      r3, <sizeof>      <-- A PLAIN IMMEDIATE. *** NOT MASKED. ***
bl      <operator new>
bl      <T::T()>          <-- masked; scores the same against any ctor
```

So the allocated size is a fact about the row that **the metric itself can see**,
and the compiler independently reports `sizeof(T)`.

A second, stronger signal came out of the same bytes. Milo's *tagged* allocator
takes the class `Symbol`, so retail emits `bl <T::StaticClassName>` first — and
that callee holds the class name as a **string literal**. `0x824576f8` loads
`"Text"` at `0x8203bca8`. That names the true class **map-independently**.

## ⚠ The ancestor check — without it the screen is wrong about most of what it says

**An inherited static names the BASE, not the derived class.** `NgMat : public
RndMat` does not override `StaticClassName`, so a *correct* `NgMat::NewObject`
calls `RndMat::StaticClassName` and tags `"Mat"`. Reading that as non-identity is
the "a callee NAME never witnesses a member type" trap in a new costume.

The check is exact rather than heuristic because **the compiler computes it**:
our own obj emits a reloc to `?StaticClassName@RndMat@@`, stating which class the
tag *should* name. A mismatch is a finding only when *our* expected tag differs
from retail's.

| | count |
|---|---|
| naive "retail tag ≠ the row's own class name" flags | 32 |
| suppressed as inherited (`DxCam:RndCam`, `NgSpotlightDrawer`, `FxSendDistortion360`, `NgMat`) | **23 (71.9%)** |
| survive | 9 |

A third class exists and must not be collapsed into "false pairing": when
retail's tag names **the row's own class**, retail is *agreeing* with the map —
the pairing is corroborated and the divergence is that *our* class inherits
`StaticClassName` where retail's declares its own (`OUR_TAG_TOO_GENERIC`).

## Population — the discriminator does NOT generalise past the seed case

Extent-bounded (see below), over every named row in `target_symbol_map.json`:

| | count |
|---|---|
| named rows allocating a compile-time-**constant** size | **632** |
| **CHECKABLE** (allocated type recoverable) | **210 (33.2%)** |
| — of those, the `NewObject` seed case | 207 |
| — from anywhere else | **3** |
| type **UNRECOVERABLE** from the row | 422 (66.8%) |

`li r3, N; bl operator_new` inside `Foo::Bar()` proves N bytes were allocated but
never *of what*, so it is not a checkable fact. **This is a real and useful
negative: the vein is the 209 `NewObject` rows and essentially nothing else.**

⚠ The survey's first number was **inflated and the tool now refuses it**: a fixed
96-instruction window ignored function extents, so tiny adjustor thunks read
their *neighbour's* allocation — `0x82b5ada8` and `0x82b5adb0` are **8 bytes
apart** and both "found" the same 180-byte alloc. Bounding at the next known
symbol drops **26** falsely-credited rows and cuts "checkable beyond the seed
case" from 5 to 3.

### Census verdicts (209 rows; 207 target-decoded, 202 both sides)

`AGREE 189 · FALSE_PAIRING_TAG 9 · SIZE_DISAGREE 3 · OUR_TAG_TOO_GENERIC 1 · UNCHECKABLE 7`
⇒ **13 of 202 (6.4%) disagree.**

## Re-adjudication of lane BODIES-6's four findings

Two confirmed, **one refuted**, one confirmed but **relocated to a different class**.

| BODIES-6 claim | verdict |
|---|---|
| `UIPanel` 264≠104 — false pairing | **CONFIRMED.** Compiler: `sizeof(UIPanel)=104`, `sizeof(PreloadPanel)=164`, and the layout shows `[UIPanel]` base members, so PreloadPanel *derives* from UIPanel. Retail's PreloadPanel row allocates exactly 164 at fuzzy 100.00/mpn 100.00. A 264-byte UIPanel cannot be a sub-object of a 164-byte class ⇒ `0x8268b9a0` is not UIPanel::NewObject. Our layout is **not** implicated. |
| `SkeletonClip` 456≠4732 — false pairing | **CONFIRMED and strengthened.** Three independent strands: the tag string is `"Text"`, the callee is `RndText::StaticClassName`, and **our own `RndText::NewObject` allocates exactly 456**. |
| `PropertyEventProvider` −4 — real defect | **REFUTED.** Not a size defect at all. Retail uses the *plain untagged* `operator new` and constructs `??0MsgSource@@QAA@XZ` (a **public** ctor); we use the *tagged* `MemAlloc` + `StaticClassName` and construct `??0PropertyEventProvider@@` (protected). Compiler: **`sizeof(MsgSource) = 68`** = retail's allocation exactly, with a vbptr/vtordisp matching the row's virtual-base adjust tail. ⇒ `0x8274df40` is **`MsgSource::NewObject`**. |
| `NgMat` −16 — real defect | **CONFIRMED REAL, but the root cause is not in NgMat.** |

## ★ The NgMat −16 has a single root cause with wide reach — HANDOFF

`NgMat::NewObject`'s **only** mismatch across 28 instructions is
`li r3, 0x250` (592) vs our `0x260` (608); the vtable store is `??_7NgMat@@6B@`,
so the pairing is genuine. The 16 bytes are inherited:

- compiler: `sizeof(BaseMaterial) = 396 (0x18c)`, `sizeof(RndMat) = 412 (0x19c)`
- RndMat's **only** own members are `ObjPtr<MetaMaterial> mMetaMaterial` (12 B)
  + `mToggleDisplayAllProps` + `mOwnsMetaMat` + `mUpdatingFromMetaMat` + 1 pad
  = **exactly 16 bytes**
- retail's row tagged `RndMat` allocates **396** ⇒ **retail's RndMat adds nothing
  to BaseMaterial**
- `RefreshState`'s offset histogram: **+16 on 71 of 86 instructions (82.6%)**,
  uniform — our members are shifted, not reordered
- **binary-absence proof, with controls**: `meta_material`, `MetaMaterial`,
  `owns_meta_mat`, `toggle_display_all_props`, `updating_from_meta` all occur
  **0 times** in `band.exe`, while 7 of 8 BaseMaterial DTA props
  (`next_pass`, `emissive_map`, `normal_map`, …) are present. The screen can fire.

⇒ **Our `RndMat` carries a 16-byte MetaMaterial block that RB3 retail does not
have** (DC3 is newer and added it). Blast radius is small — `Mat.h` + `Mat.cpp`,
29 references — but it is a *feature removal*, not a layout edit, and it shifts
every RndMat subclass.

**DELIBERATELY NOT DONE.** Removing the feature needs its own lane and a full
whole-binary A/B; a rushed removal could regress the whole material family. The
target is exact and pre-measured: `sizeof(RndMat)` must become **396**, which
takes `NgMat` to 592 and should collapse `RefreshState`'s uniform +16.

This also re-explains the `BaseMaterial` row: `0x8240f5d0` is retail's
**`RndMat::NewObject`** (396, tagged `RndMat`); our `BaseMaterial::NewObject` is
paired to it because both are 396. **The size matched by coincidence and only the
tag exposed it** — the clearest demonstration that the tag adds power beyond size.

## False pairings — REPORTED, NOT FIXED (`target_symbol_map.json` is out of scope)

A map edit needs its own forced re-split and A/B. Sizes are ours/retail.

| row VA | map name | retail tag (string) | ours | retail | note |
|---|---|---|---|---|---|
| `0x824576f8` | SkeletonClip | RndText (`"Text"`) | 4732 | 456 | **strongest** — size + name + our RndText=456 |
| `0x8240f5d0` | BaseMaterial | RndMat (`"Mat"`) | 396 | 396 | is retail's `RndMat::NewObject`; see above |
| `0x8227b408` | PhotoSpotlightPositioner | BandRetargetVignette | 80 | 80 | unrelated classes (both `RndPollable`) |
| `0x8227b9b8` | HamPhotoDisplay | UnisonIcon | 564 | 564 | unrelated |
| `0x8227bb98` | NavigationSkeletonDir | OverdriveMeter (`"OverdriveMeterDir"`) | 648 | 648 | unrelated |
| `0x82329778` | SkeletonViz | LayerDir | 548 | 548 | unrelated |
| `0x82b5a2e0` | FxSendReverb360 | FxSendChorus | 180 | 180 | **swapped pair** |
| `0x82b5adc0` | FxSendChorus360 | FxSendReverb | 180 | 180 | **swapped pair** |

⚠ Five of these (SkeletonClip, PhotoSpotlightPositioner, HamPhotoDisplay,
NavigationSkeletonDir, SkeletonViz) are `gesture`/`hamobj` — **Dance Central
subsystems with no RB3 counterpart**, i.e. the same class of DC3-only pin that
`a0d03243` deleted 16 of at Δmetric exactly zero.

**NOT false pairings — our source is missing a `StaticClassName` override:**

| row VA | map name | retail tag | note |
|---|---|---|---|
| `0x8231d220` | StarDisplay | `StarDisplay` | retail names the row's OWN class ⇒ pairing corroborated |
| `0x8264c828` | AppMiniLeaderboardDisplay | `MiniLeaderboardDisplay` | its **base** (`AppMiniLeaderboardDisplay : public MiniLeaderboardDisplay`); ours resolves to `UIComponent` because *our* `MiniLeaderboardDisplay` does not declare its own. Classified `FALSE_PAIRING_TAG` by the tool only because the rule tests exact class equality — **treat as `OUR_TAG_TOO_GENERIC`.** |

## Rows at mpn 100 that are demonstrably wrong

`BaseMaterial`, `FxSendChorus360`, `FxSendReverb360` and `StarDisplay` all sit at
**mpn 100.00 — counted in `matched_functions`** — while the tag proves the row is
wrong. Another instance of the at-100% defect class, found from the metric's own
input rather than external evidence.
