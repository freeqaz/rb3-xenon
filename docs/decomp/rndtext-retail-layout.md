# RndText — retail RB3-360 layout, FULLY RECOVERED (lane BP-2b, 2026-07-29)

> **STATUS:** layout is **CLOSED** (0xb8/0xb8 bytes attributed, no filler).
> **No source change landed** — the fix is a *port*, not a header edit; see
> "Why no landing". Every row is tagged MEASURED (an instruction in the retail
> binary, or a `cl.exe /d1reportSingleClassLayout` report) or INFERRED or
> EVIDENCE-RAN-OUT.

## TL;DR

- Retail `RndText` is **rb3-Wii-lineage in structure, interface, member order,
  Load/Save order and constructor mem-init list**, with **360-widened types**.
  It is *not* the DC3-generation class our tree carries.
- Base region **[0x0, 0xd8) is byte-identical to our tree** — confirmed five
  independent ways. No base-class surgery needed.
- Own-member block is **[0xd8, 0x190) = 0xb8 bytes**, now **fully attributed**.
  Ours is `[0xd8, 0x198)` = 0xc0 ⇒ **we are exactly 8 bytes too large**, and
  retail `sizeof(RndText)` is **0x1c8** vs our 0x1d0.
- `RndText::Line` is **0x78** (measured four ways), not our DC3-shaped 0x14.
- Fixing this is a **port** (2,719-line `Text.cpp` + 6 other TUs are written
  against the DC3 member set), not a member-block edit.

## Ground-truth anchors

`this` in the retail vbase-relative bodies is the **`Hmx::Object` virtual-base
subobject**, and `RndText* = this - 0x194`. Members are therefore reached as
**negative** displacements off `this`.

| Anchor | Value | Evidence |
|---|---|---|
| `Hmx::Object` vbase subobject | RndText+**0x194** | 9 × `subi rX, r28, 0x194` in `fn_8245A238`; ctor vbtable literal `{-4, 0x190, 0x1BC}` |
| vtordisp word | RndText+**0x190** | `fn_8245C480`: `lwz r11,-0x4(r3)` / `subf r3,r11,r3` (textbook MSVC vtordisp thunk); ctor stores 0 at 0x190 |
| `RndHighlightable` vbase | RndText+**0x1C0** | ctor vbtable `{-4, 0x190, 0x1BC}` + ctor call at 0x1C0 |
| `RndDrawable` subobject | RndText+**0x0** | `subi r3,r28,0x16c` → `(0x194-0x16c) - offD`, `offD = 0x28` = Object's offset inside a **standalone** `RndDrawable` (compiler, sizeof 0x5c) |
| `RndTransformable` subobject | RndText+**0x24** | `subi r3,r28,0xb8` → `(0x194-0xb8) - offT`, `offT = 0xb8` = Object's offset inside a standalone `RndTransformable` (compiler, sizeof 0xec) |
| own members start | **0xd8** | = RndTransformable(0x24) + its own size 0xb4 |
| retail `sizeof(RndText)` | **0x1c8** | 0x194 + 0x34 (Object 0x28 + vtordisp 4 + RndHighlightable vfptr/vbptr 8) |

★ **The trap that cost this lane two false alarms.** Do **not** read the raw
`subi` constants as subobject addresses. A `SYNC_SUPERCLASS`/`LOAD_SUPERCLASS`
call passes `base_offset + that base's OWN internal vbase offset`, because the
base classes are themselves compiled vbase-relative. Reading them naively yields
"RndDrawable@+0x28, RndTransformable@+0xdc", which collides with the own-member
block and looks like a layout contradiction. It is not one. Confirmed three
times independently (SyncProperty, Load, Save).

★ **Sign convention.** Any re-derivation must compute `member = this + delta`
with `delta = off - 0x194` *negative*; e.g. `stw r11,-0xa0(r30)` is `mAlign@0xf4`.
Getting this backwards makes every vbase-relative access silently score
out-of-range and drops Load/Save/Copy/SyncProperty/Handle from the sweep
entirely.

## Recovered own-member table — [0xd8, 0x190), totals exactly 0xb8

```c
// Object vbase @0x194, vtordisp word @0x190, RndHighlightable vbase @0x1C0.
std::vector<Line>          mLines;        // 0x0d8  0x0c  MEASURED (stride 0x78)
ObjOwnerPtr<RndFont>       mFont;         // 0x0e4  0x0c  MEASURED (payload @0x0ec)
float                      mWrapWidth;    // 0x0f0  0x04  MEASURED (ctor = 0.0)
int                        mAlign;        // 0x0f4  0x04  MEASURED (ctor = 0x11 kTopLeft)
int                        mCapsMode;     // 0x0f8  0x04  MEASURED (ctor = 0)
float                      mLeading;      // 0x0fc  0x04  MEASURED (ctor = 1.0)
String                     mText;         // 0x100  0x0c  MEASURED (mCap 0x104, mStr 0x108)
int                        mFixedLength;  // 0x10c  0x04  MEASURED
Style                      mStyle;        // 0x110  0x24  MEASURED
                                          //   font@0x110 size@0x114 italics@0x118
                                          //   Hmx::Color@0x11c brk@0x12c pre@0x12d
                                          //   zOffset@0x130   (ctor color = 1,1,1,1)
bool                       mTextMarkup;   // 0x134  0x01  MEASURED (+3 pad; serialized)
Style                      mAltStyle;     // 0x138  0x24  MEASURED (Load: memcpy 0x24
                                          //                from mStyle@0x110)
bool                       unknown_0x15C; // 0x15c  0x01  MEASURED (+3 pad)
std::map<FontKey,MeshInfo> mMeshMap;      // 0x160  0x18  MEASURED (0x18 flavour)
int                        mDeferUpdate;  // 0x178  0x04  MEASURED (signed; cmpwi)
bool                       unknown_0x17C; // 0x17c  0x01  MEASURED (+3 pad)
void                      *unknown_0x180; // 0x180  0x04  MEASURED (callback iface,
                                          //                virtual slot 1)
float                      unknown_0x184; // 0x184  0x04  MEASURED
float                      unknown_0x188; // 0x188  0x04  MEASURED
bool                       unknown_0x18C; // 0x18c  0x01  MEASURED
                                          // 0x18d-0x18f  EVIDENCE RAN OUT (pad or
                                          //                unreferenced bools)
```

The six `unknown_*` slots have **measured identity but unmeasured names** — their
rb3-Wii counterparts are `unkbp4`, `unkbp5`, `unk128`, `unk12c`, `unk130` and
`unk124b4p1`. They are deliberately left unnamed rather than importing Wii
placeholders as if they were real names.

Cross-checks that make the table self-validating:
- The five scalar properties each have a getter reading `-X(r28)` and a setter
  writing `+Y(r28-0x194)` with `0x194 - X == Y`. Five independent confirmations.
- `GetCurrentStringDimensions` sets `out1 ← 0x188`, `out2 ← 0x184`, matching
  Wii's `f1 = unk130; f2 = unk12c`.
- The ctor's mem-init list is a **1:1 match for rb3-Wii's**, including
  `Color32(-1)` ⇒ retail `Hmx::Color(1,1,1,1)`.
- The dtor tears down in exact reverse declaration order: `mMeshMap@0x160` →
  `String::~String@0x100` → `ObjOwnerPtr::~@0xe4` → `vector<Line>@0xd8`
  (computing `(*(0xe0) - *(0xd8)) / 0x78 * 0x78`).

### `sizeof(std::map)` = 0x18 — reconciled two ways

From `src/system/stlport/stl/_tree.h`: `_AllocProxy _M_header` (EBO'd empty
allocator + `_Rb_tree_node_base{bool _M_color; _Base_ptr _M_parent,_M_left,
_M_right}` = 0x10) + `size_type _M_node_count`@0x10 + `_Compare
_M_key_compare`@0x14 ⇒ **0x18**. From the ctor `fn_82456CB0`: `stb 0,0x160` /
`stw 0,0x164` / `stw &hdr,0x168` / `stw &hdr,0x16c` / `stw 0,0x170` /
`stb <1 byte>,0x174` — exact 1:1 with that member list, last member a 1-byte
comparator at 0x174, padding to **0x178**.

⚠ `_tree.h` documents a per-TU `RB3_RBTREE_0x1C` ABI split. **Text.cpp is the
0x18 flavour — do not gate it.**

### `RndText::Line` — sizeof 0x78, fully measured

| Off | Size | Field | Evidence |
|---|---|---|---|
| 0x00 | 0x24 | `Style lineStyle` | `li r5,0x24` + memcpy |
| 0x24 | 4 | `const char*` (`mStr + startIdx`) | `lwz r8,0x108(r28)`; `add`; `stw …,0x24` |
| 0x28 | 4 | `const char*` (`mStr + endIdx`) | `lwz r11,0x108(r28)`; `stw …,0x28` |
| 0x2c | 4 | `unsigned startIdx` | `stw …,0x2c`; back-ref `lwz r11,-0x48(r11)` (0x78−0x48 = 0x30) |
| 0x30 | 4 | `unsigned endIdx` | `stw …,0x30`, read back |
| 0x34 | 0x40 | `Transform` | `li r5,0x40` + memcpy to `+0x34` |
| 0x74 | 4 | `float` | `stfs f0,0x74(r30)` immediately before `li r28,0x78` |

Stride 0x78 confirmed four ways: `divw` by 0x78, two `mulli …,0x78`, and the
dtor's deallocation arithmetic. Decomposition vs Wii's 0x60: **+0xc** (`Style`:
`Color32`→`Hmx::Color`) **+0xc** (`Line`'s own `Color32`→`Hmx::Color`) = 0x18.

## Identified functions (identity proven behaviourally / by string xref)

| Address | Function | splits.txt state |
|---|---|---|
| `fn_82456CB0` | **constructor** | unclaimed span 0x82456CA4–0x82457060; **absent from the symbol map** |
| `fn_8245A238` | PROPSYNC body (0x8245A238–0x8245AAB4) | pinned, `Text.cpp` |
| `fn_8245B4C0` | HANDLERS body (0x8245B4C0–0x8245BA64) | pinned, `Text.cpp` |
| `fn_8245AE98` | **`Load`** (Object vftable slot 10, thunk `fn_8245BD18`) | pinned, `Text.cpp` |
| `fn_82455928` | **`Save`** (slot 8, thunk `fn_82457260`); writes rev **0x15** | **mis-pinned to `UIFontImporter.cpp`**, mislabelled `?Save@UIFontImporter@@…` |
| `fn_82457790` | `~RndText()` | **mis-pinned to `SkeletonClip.cpp`** |
| `fn_82457EA0` | `??_G` deleting dtor (slot 0, thunk `fn_824570A8`) | **mis-pinned to `Lit_NG.cpp`**; absent from map |
| `fn_82459660` | `UpdateText(bool)` | unclaimed span 0x82457F70–0x82459820 |
| `fn_82455508` / `fn_82459EA8` / `fn_82459840` | `TextASCII` / `SetTextASCII` / `SetColor` | pinned |

PROPSYNC/HANDLERS identity: they reference all six RndText-exclusive property
strings and all nine handler strings, each in exact source order, and the tail
makes exactly **two** `SYNC_SUPERCLASS` calls (rb3-Wii has 2; DC3 has 3).

**Retail `Load` order is identical to rb3-Wii's, item for item** (20 steps, 22
revision gates, max save rev 0x15 = 21 — matching Wii's `ASSERT_REVS(21,0)`).
`Save` writes 14 items in the same order and touches **nothing** in
`[0x138,0x15c)` or `[0x178,0x190)` ⇒ `mAltStyle` and all six tail members are
runtime-only. `gRev`/`gAltRev` are stored as **shorts into one aggregate**
(`lbl_82CC2F7C+4` / `+0`) — consistent with the standing "MSVC does not lay out
.bss in decl order ⇒ gRev/gAltRev must be ONE aggregate" note.

## Wii-vs-DC3 verdict for retail (for the engine-reuse doc)

**rb3-Wii-lineage (decisive):** full Wii property + handler name sets present in
one TU; the three RB3-only `RndText::Init()` config keys
(`text_superscript_scale`, `text_guitar_scale`, `text_guitar_z_offset`);
exactly two `SYNC_SUPERCLASS` calls; keeps `mFont`, `mWrapWidth`, `mLeading`,
`mText`, single `mStyle` + `mAltStyle`, `std::map mMeshMap`; Load/Save order and
ctor mem-init list match Wii 1:1.

**DC3-generation features ABSENT** — zero string hits for `styles`,
`basic_markup`, `circle`, `indentation`, `scroll_delay`, `scroll_rate`,
`update_text`, `blacklight`, `font_color`, `FontMap`, `FontMap3d`. No
`StyleState`, no `ObjVector<Style> mStyles`, no `BlacklightPacket`, no fit/scroll
block. `fit_type` and `kerning` exist but belong to a **UILabel** string cluster.

**360-widened types vs Wii:** `Hmx::Color` (float4) replaces packed `Color32` in
`Style` **and** in `Line`; `mAlign`/`mCapsMode` are 4-byte `int` (not a `u8`
pair); `mFixedLength` is a full `int` (not `int:16`); `mDeferUpdate` is a full
`int` (Wii bitfield); `mTextMarkup` is a `bool` member of RndText at 0x134 (on
Wii a bitfield in the `RndDrawable` base). Retail also **hoists**
`mAlign`/`mCapsMode`/`mFixedLength` ahead of `mStyle`, so declaration order is
genuinely not Wii's even though Load/Save order is.

## Refuted claims — do NOT re-hunt

1. **`0x827DE868` is NOT `RndText::RndText()`.** The map labels it
   `??0RndText@@IAA@XZ`; its asm initialises members out to **0x408**, has three
   0x40-strided `Transform` blocks (0x348/0x388/0x3c8), ~14 identical 0xc-sized
   ObjPtr sub-objects (0x23c…0x324), and does vbtable arithmetic. **MAP MISPAIR.**
   The real ctor is `fn_82456CB0`.
2. **Every `RndText` name in `scripts/target_symbol_map.json` is a DC3-map
   hypothesis.** Its `StyleState`/`FontMap`/`BlacklightPacket`/`ObjVector<Style>`
   manglings are what made the DC3 shape look retail-confirmed. Further measured
   mispairs: `0x8245AE98` claimed `Highlight` but is **`Load`**; `0x8245ADE8`
   claimed `Load` but is the 2-arg slot-2 `Replace`; `0x8245ADC8` claimed `??_E`
   but is the **SyncProperty thunk**; `0x8245C2D8` claimed `?Save@RndText@@$4…`
   but jumps to a different class's thunk block.
3. **`Transform` is NOT a 0x30-vs-0x40 divergence.** False alarm from eyeballing
   `Vec.h`/`Mtx.h`. The compiler says our tree already has `Vector3` sizeof
   **16** (x,y,z + 4 PAD), `Matrix3` **0x30**, `Transform` **0x40** — identical
   to retail. **No fleet-wide blast radius.** (CLAUDE.md's rule holds: ask the
   compiler, never the header comments.)
4. **The "base geometry differs" contradiction was a phantom** — see the ★ trap.
5. **"`mAltStyle` may not exist on retail RndText" — REFUTED.** It exists at
   0x138 as a full `Style`; Load ends with `memcpy(+0x138, +0x110, 0x24)`. The
   apparent 0x20-byte hole at `[0x140,0x160)` was simply mAltStyle's interior.
   Alt-styles are **not** UILabel-only.
6. Still true from BP-2: the VocalTrackDir 4×-call cluster is a generic string
   formatter, not 4 `ObjPtr<RndText>` members; `TrackWidget.s` / `HamLabel.s` /
   `VocalTrackDir.s` pinned ranges contain no direct RndText derefs.

**Generalisable lesson:** three of the four alarms in this lane were *inference
presented as measurement*, and every one dissolved the moment the compiler or the
raw instruction was consulted. Tag inference as inference.

## Reusable tooling lessons (each cost ~5× recall)

1. **`bl __savegprlr_N` is not a call.** Killing r3–r12 at every `bl` per the PPC
   ABI destroys the `this` seed on instruction 2 of *every non-leaf MSVC
   function* (prologue is `mflr r12; bl __savegprlr_29`). Whitelist
   `__savegprlr` / `__savefpr` / `__restfpr`.
2. **Register re-use is a real trap, not a theoretical one.** A `0xd8`/`0xdc`
   pair in `SetColor` is an `RndMesh` vert array with stride **0x60**, not
   `mLines` with stride 0x78. Attribute offsets per-function with base
   provenance, and quarantine foreign-unit sites rather than merging them.
3. **"In the address window" ≠ "is this TU".** 316 functions live in
   0x82454600–0x8245D000 and splits.txt hands chunks of it to Group, ScreenMask,
   NetCacheMgr, UIFontImporter, Faders, SkeletonClip, CharUtl, BandCamShot,
   Lit_NG, TransProxy, BandCharDesc, Wind and Msg.

## Why no landing (the guardrail)

The layout is closed, but rewriting the own-member block is **not a header-only
change**:

- `src/system/rndobj/Text.cpp` is **2,719 lines** implementing the
  DC3-generation class (`mStyles`, `StyleState`, `FontMapBase`/`FontMap`/
  `FontMap3d`, `BlacklightPacket`, the fit/scroll state block).
- Six further files reference DC3-only RndText API: `src/system/ui/UILabel.cpp`,
  `UILabel.h`, `UIListLabel.cpp`, `src/system/bandobj/BandButton.cpp`,
  `src/system/hamobj/HamListRibbon.cpp` (+ `Text.h`).
- The retail and DC3 member sets barely overlap, so no mechanical rename keeps
  the tree compiling: retail has no `mStyles`, `mFontMaps`, `mFitType`,
  `mCircle`, `mHeight`, `mBounds*`, scroll block, `mLineWidths`,
  `mNumLinesRendered` or `mConstructScale` at all.
- Retail `Line` (0x78, embedded `Style` + `Transform`) is source-incompatible
  with our `Line` (0x14, `const u16*` pair).

So the next step is a **port lane** (rb3-Wii `Text.cpp` → MSVC X360), sized like
the SLM-giants body-port lanes. Shrinking our block by the measured 8 bytes
without the right member *set* would be a guess and would shift every currently
correct offset.

## Handoff to the port lane

Everything needed is now measured: the full member table above, the ctor's
default values, the exact Load/Save order with all 22 revision gates, the
`_Rb_tree` 0x18 flavour, and `Line`'s internal layout. Remaining true unknowns
are only `0x18d–0x18f` (padding or unreferenced bools) and the *names* of six
runtime-only members. Wii's `unkbp6`, `unkbp7` and `unk124b4:3` were not located
— retail's ctor zeroes exactly four bools and its `UpdateText` omits Wii's
`unkbp6 = true`, but the Draw/CollectGarbage paths were not audited, so their
absence is INFERRED, not proven.

Also actionable (independent of the port): RndText code is scattered into other
units' pins — `Save` → `UIFontImporter.cpp`, `~RndText` + `RotateLineVerts` →
`SkeletonClip.cpp`, `??_G` → `Lit_NG.cpp`, plus unclaimed spans
0x82456CA4–0x82457060 (ctor) and 0x82457F70–0x82459820 (`UpdateText` and 5
more). Repointing these is expected to be **≈0 for match%** while our source is
the wrong generation, so it should ride along with the port, not precede it.
