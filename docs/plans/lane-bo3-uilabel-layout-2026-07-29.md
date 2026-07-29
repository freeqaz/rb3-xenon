# lane BO-3 — reconstructing `UILabel`'s retail-360 member layout (2026-07-29)

**Branch:** `laneBO3-layout` (from main `9df262c9`, `measures.matched_functions` = 40,302)
**Mission:** `docs/plans/tu-pin-wave-2026-07-29.md` §8quater — replace
`UILabel::mUnkTU5Tail[0xAC]` with real members, then cash the functions it gates.

> **Result in one line:** the layout is **fully recovered with zero unaccounted
> bytes** and every member offset is backed by a retail access site. See §5 for the
> honest yield number.

## 0. Provenance check — why the `.text` VA→file-offset hazard does not apply here

A concurrent lane hit a serious trap: `.text` in `orig/45410914/band.exe` is RVA
`0x00270000` / raw `0x00264E00`, a **`0xB200` delta**, so computing a file offset as
`va - 0x82000000` is valid **only for `.rdata`** (where `PointerToRawData ==
VirtualAddress`). A naive mapping disassembles unrelated bytes.

**No offset in this document came from a raw image read.** Every displacement was
read out of `build/45410914/asm/*.s` — dtk's split disassembly, which already
applies the correct section mapping and prints VA and raw offset side by side.
Verified two ways:

```
# the cross-lane anchor, from build/45410914/asm/Spotlight.s:
/* 824DAAD0 004CF8D0  7D 88 02 A6 */   mflr r12          # off(0x824DAAD0) == 0x004CF8D0 ✓

# a line this document actually relies on, from build/45410914/asm/UILabel.s:
/* 827F2438 007E7238  80 63 01 44 */   lwz r3, 0x144(r3) # 0x7F2438 - 0x7E7238 == 0xB200 ✓
```

The only raw-image reads in the lane were of **property-name string literals** in
`.rdata` (§3b), where the naive mapping is valid — and those supplied member
*names*, never offsets, so a bad read there could not have corrupted the layout.
Finally, the whole table is independently reproduced by the compiler (§3e), which is
the authoritative channel.

Related trap, also avoided: **`.pdata` absence is not a "not a function" test** —
frameless leaf functions are systematically absent from the X360 `.pdata` table, and
accessors are exactly that shape. No claim here rests on a `.pdata` absence.

### 0b. Audit of the compiler channel

A concurrent lane also found that `scripts/harvest/class_layout_report.py` can
**exit 0 while emitting nothing**, which is indistinguishable from "the class has no
members" if you are not checking. Every compiler report cited in this document was
therefore audited for non-emptiness:

| file | bytes | `size(` rows | used? |
|---|---|---|---|
| `UILabel.txt` (pre-change) | 17,435 | 19 | yes |
| `UILabel_v2.txt` (post-change) | 20,059 | 4 | yes |
| `UIComponent.txt` | 12,236 | 31 | yes |
| `HamLabel.txt` | 6,989 | 2 | yes |
| `String.txt` / `Symbol.txt` | 11,423 / 8,588 | 59 / 37 | yes |
| `ObjPtr.txt` / `ObjDirPtr.txt` | 34,527 / 3,186 | 99 / 11 | yes |
| `RndText.txt` | 19,727 | 98 | yes |
| `BandLabel.txt`, `AppLabel.txt` | **0** | **0** | **NO — excluded** |

The two empty runs were detected and discarded rather than read as "no members";
`BandLabel`'s answer came from retail asm instead, which settled it more directly.
(The tool also hangs at 0% CPU under concurrent `wibo` — budget 10–45 min per class
when other lanes are building, and pass `--tu <path.cpp> --raw`; auto-TU-resolution
is unreliable.)

**The layout does not depend on the compiler channel at all.** Every offset in §3 is
independently proven from retail access sites; the compiler is corroboration, and
the arithmetic closing to exactly 0xD4 with no gaps is a third, independent check.
The decisive empirical check is the build itself: a wrong layout in a header this
widely included would have regressed matched functions, and the measured result is
**+40 / −0**.

---

## 1. The two anchors that unlock everything

Neither anchor is guessable; both come straight out of retail asm
(`build/45410914/asm/UILabel.s`, from the pinned `UILabel.cpp` `.text` span
`0x827F2148..0x827F7AD8`).

### 1.1 `UIComponent` nvsize = 0x140 ⇒ UILabel's own members start at 0x140

Already recorded in `src/system/ui/UIComponent.h` and independently re-confirmed
below by the fact that 0x140 is the lowest UILabel-owned offset any retail UILabel
function touches.

### 1.2 UILabel's `Hmx::Object` vbase is at 0x218, and retail's virtuals address members **negatively**

`UILabel::PreLoad` (retail `0x827F4EC8`) is a virtual introduced in the
`Hmx::Object` **virtual base**. MSVC therefore compiles its body with `this`
pointing at the `Hmx::Object` sub-object, so every member access is a *negative*
displacement. The proof that the bias is exactly 0x218:

```
827F52D8   subi r3, r31, 0x218     ; r3 = the real UILabel*
827F52DC   bl   fn_827F4C80        ; = AltFontResourceFileUpdated(true)
```

and the same function's super-call:

```
827F4EF8   subi r3, r31, 0xd4      ; UIComponent::PreLoad expects Hmx::Object-biased
827F4F10   bl   fn_82800B08        ; this;  0x218 - 0x144 = 0xd4  ⇒ UIComponent's own
                                   ; Hmx::Object vbase sits at 0x144, i.e. nvsize 0x140
                                   ; + a 4-byte vtordisp.  Self-consistent.
```

So **`r31 == UILabel* + 0x218`**: the `Hmx::Object` vbase sits at 0x218, the
vtordisp word immediately before it at `0x214`, and the own-member region is
therefore exactly **[0x140, 0x214) = 0xD4 bytes**. (UILabel's non-virtual size *as a
base* is 0x214; 0x218 is a vbase offset, not a size — a distinction §3c returns to,
because conflating the two is what made the old header's comment confusing.)

Every displacement quoted from a `UILabel` virtual in this document is therefore
given as `K`, where **`offset = 0x218 - K`**. Non-virtual member functions take a
plain `UILabel*` and use absolute offsets.

---

## 2. The exhaustive-enumeration trick (why there are no gaps)

The retail **`UILabel` constructor** (`0x827F3D50`) constructs every non-trivial
member in declaration order. Each one shows up as an `addi` of `this` into `r3`:

```
827F3E6C  stw  r29, 0x140(r30)     827F3F10  addi r3, r30, 0x1b0
827F3E74  stw  r3,  0x144(r30)     827F3F30  addi r3, r30, 0x1c0
827F3E78  addi r3,  r30, 0x148     827F3F64  addi r3, r30, 0x1e0
827F3E88  addi r3,  r30, 0x154     827F3FA4  addi r3, r30, 0x1fc
827F3EA0  addi r11, r30, 0x160     827F3FB0  addi r10, r30, 0x208
827F3EAC  addi r3,  r30, 0x168
827F3EC0  stw  r11, 0x164(r30)
827F3EC8  addi r3,  r30, 0x174
```

That list is **complete** — it is the whole set of aggregate members. Combined with
the scalar offsets from `PreLoad`, every one of the 0xD4 bytes is claimed. Two
consequences:

* There is **no `ObjVector<LabelStyle>` in retail RB3-360.** `LabelStyle` /
  `mLabelStyles` are a **DC3-only** rewrite of UILabel that had been carried into
  this tree by mistake, together with `mLabelText`(DC3 sense), `mIconChar`,
  `mTextEmpty` and `mDirty`.
* Nothing is left over, so we do not need to hedge with filler bytes.

**Reusable methodology for the next opaque-tail class** — this is the general
recipe, and it is cheap:

1. Find any virtual of the class that is introduced in a virtual base; read its
   `subi rX, rThis, K` super-call / self-call to recover the vbase bias `K`. The
   class's own members then end at `K - 4` (the vtordisp word). The cheapest tell
   is a call that passes the *unbiased* `this` — e.g. `subi r3, r31, 0x218` right
   before a call to a plain non-virtual member function.
2. Find the constructor and read off the `addi this, <off>` sequence — that is the
   complete, ordered list of aggregate members and it bounds every gap.
3. Find the `PreLoad`/`Load` rev-gated read chain — `BinStream::Read(&member, 4)`
   gives you every scalar offset *and its width* (this is how we learned RB3's
   `mAlignment`/`mCapsMode`/`mFitType` are 4-byte enums, not the packed `uchar`s
   the Wii header shows).
4. Cross-check two or three offsets from a completely unrelated function so the
   bias itself is falsifiable.
5. Take member *names and order* from the oracle; take *offsets and widths* only
   from retail. Assume nothing transfers: on this class alone the oracle was wrong
   about three members' widths, three members' storage class, one member's
   existence, and the declaration order of four more (§4).
6. Get the class's true total from a `NewObject`/factory function's
   `li r3, <size>` before `operator new` — `0x827F4DC0` gave `sizeof(UILabel)`
   = 0x24C for free.
7. Confirm the arithmetic with `scripts/harvest/class_layout_report.py` (the
   compiler), and get every component type's size the same way. Never take a size
   from a `// 0xHEX` header comment.
8. If the class's `SyncProperty` is in the span, decode it and read each arm's
   property-name string out of `orig/45410914/band.exe`. That turns a table of
   bare offsets into a table of *named* members and is the single strongest piece
   of evidence available.

Two more opaque-tail classes in the tree are open to exactly this recipe:
`src/system/flow/Flow.h:51` (`char mUnkTU5_0x30[0x50]`) and
`src/system/flow/FlowSound.h:70` (`char mUnkA4[0x28]`).

---

## 3. The recovered layout, with per-member evidence

`this` in the table is the real `UILabel*`. `K` is the displacement seen in the
Hmx::Object-biased virtuals, where `offset = 0x218 - K`.

| offset | type | member | proving site | evidence |
|---|---|---|---|---|
| 0x140 | `UILabelDir*` | `mLabelDir` | `827F2D1C lwz r3,0x140(r31)` → `fn_8280FD48` (UILabelDir mat-variation lookup); ctor `827F3E6C stw` | PROVEN |
| 0x144 | `RndText*` | `mText` | **`0x827F2438` is the entire function `lwz r3,0x144(r3); blr` = `TextObj()`**; FitText `827F585C lwz r3,0x144(r30)`; ctor `827F3E74 stw`; 30 further sites | PROVEN (3 independent) |
| 0x148 | `String` (0xC) | `mLabelText` (Wii `unk114`) | ctor `827F3E78 addi r3,r30,0x148`; PostLoad `827F772C` assigns `mIcon`/`mEditText` into `r31-0xd0` | PROVEN |
| 0x154 | `ObjPtr<RndFont>` (0xC) | `mFont` | ctor `827F3E88`; `Font()` `827F2CFC lwz r3,0x15c(r3)` = ObjPtr payload; `827F2D48 addi r3,r31,0x154` → ObjPtr assign | PROVEN |
| 0x160 | `Symbol` | `mCurFontMatVariation` (Wii `unk12c`) | `Font()` `827F2D0C lwz r10,0x160` compared against `0x1d0`; `827F2D5C stw` writes it back | PROVEN |
| 0x164 | `Symbol` | `mTextToken` | PreLoad `K=0xb4` → `BinStream >> Symbol`; ctor `827F3EC0 stw` | PROVEN |
| 0x168 | `String` (0xC) | `mEditText` | PreLoad `K=0xb0`, `gRev > 0xD`; ctor `827F3EAC`; **`SetEditText` (`827F4B68`) assigns the `const char*` arg straight into 0x168** | PROVEN incl. name |
| 0x174 | `String` (0xC) | `mIcon` | PreLoad `K=0xa4`, `gRev > 0xE`; ctor `827F3EC8`; PostLoad `!mIcon.empty()` via its char* at 0x17c | PROVEN (3 independent) |
| 0x180 | `float` | `mTextSize` | PreLoad `K=0x98`; FitText `lfs f1,0x180(r30)` passed as the `size` arg to `GetStringDimensions`, and `mTextSize > 0.0f` guard | PROVEN (2 independent) |
| 0x184 | `RndText::Alignment` (4) | `mAlignment` | PreLoad `K=0x94` (**4-byte** read); PreLoad `gRev<4` fixup does `lwz r11,-0x94(r31)` then tests bits `&1`, `&4`, `&0x10`, `&0x40` — exactly the Wii source | PROVEN (2 independent) |
| 0x188 | `RndText::CapsMode` (4) | `mCapsMode` | PreLoad `K=0x90`, 4-byte read | PROVEN |
| 0x18c | `bool` (+3 pad) | `mMarkup` | PreLoad `K=0x8c`, `gRev > 7`, `BinStream >> bool` | PROVEN |
| 0x190 | `float` | `mLeading` | PreLoad `K=0x88` | PROVEN |
| 0x194 | `float` | `mKerning` | PreLoad `K=0x84`; **and** the `gRev <= 0x12` else-arm `lfs f0,-0x84(r31); stfs f0,-0x3c(r31)` = `mAltKerning = mKerning` | PROVEN (2 independent) |
| 0x198 | `float` | `mItalics` | PreLoad `K=0x80`, `gRev > 4` | PROVEN |
| 0x19c | `FitType` (4) | `mFitType` | PreLoad `K=0x7c`, `gRev > 2`, 4-byte read | PROVEN |
| 0x1a0 | `float` | `mWidth` | PreLoad `K=0x78`; FitText `lfs f0,0x1a0(r30)` in `mWidth > 0.0f` and `w > mWidth`; PreLoad `gRev<4` `lfs f12,-0x78(r31)` in the `xfm.v.x -= mWidth/2` fixup | PROVEN (3 independent) |
| 0x1a4 | `float` | `mHeight` | PreLoad `K=0x74`; `gRev<4` `lfs f12,-0x74(r31)` in the `xfm.v.z += mHeight/2` fixup | PROVEN (2 independent) |
| 0x1a8 | `int` | `mFixedLength` | PreLoad `K=0x70`, `gRev > 5`, **4-byte** read (Wii models it as `short`) | PROVEN |
| 0x1ac | `int` | `mReservedLine` | PreLoad `K=0x6c`, `gRev > 6`, 4-byte read | PROVEN |
| 0x1b0 | `String` (0xC) | `mPreserveTruncText` | PreLoad `K=0x68`, `gRev > 9`; ctor `827F3F10`; FitText `827F5834 addi r4,r30,0x1b0` | PROVEN (3 independent) |
| 0x1bc | `float` | `mAlpha` | PreLoad `K=0x5c`, `gRev > 10` | PROVEN |
| 0x1c0 | `ObjPtr<UIColor>` (0xC) | `mColorOverride` | PreLoad `K=0x58`, `gRev > 0xC`, `ObjPtr::Load(bs,1,0)`; ctor `827F3F30` | PROVEN |
| 0x1cc | `bool` (+3 pad) | `mUseHighlightMesh` | PreLoad `K=0x4c`, `gRev > 0x10`, `>> bool` | PROVEN |
| 0x1d0 | `Symbol` | `mFontMatVariation` | PreLoad `K=0x48`, `gRev > 0x14`; `Font()` `827F2D08 lwz r11,0x1d0` compared to `0x160` | PROVEN (2 independent) |
| 0x1d4 | `Symbol` | `mAltMatVariation` | PreLoad `K=0x44`, `gRev > 0x16` | PROVEN |
| 0x1d8 | `float` | `mAltTextSize` | PreLoad `K=0x40`, `gRev > 0x11` | PROVEN |
| 0x1dc | `float` | `mAltKerning` | PreLoad `K=0x3c`, `gRev > 0x12`, plus the fallback copy from `mKerning` | PROVEN (2 independent) |
| 0x1e0 | `ObjPtr<UIColor>` (0xC) | `mAltTextColor` | PreLoad `K=0x38`, `ObjPtr::Load`; ctor `827F3F64` | PROVEN |
| 0x1ec | `float` | `mAltZOffset` | PreLoad `K=0x2c`, `gRev > 0x13` | PROVEN |
| 0x1f0 | `float` | `mAltItalics` | PreLoad `K=0x28`, `gRev > 0x17` | PROVEN |
| 0x1f4 | `float` | `mAltAlpha` | PreLoad `K=0x24`, `gRev > 0x17` | PROVEN |
| 0x1f8 | `bool` (+3 pad) | `mAltStyleEnabled` | PreLoad `K=0x20`, `gRev > 0x11`, `>> bool` | PROVEN |
| 0x1fc | `String` (0xC) | `mAltFontResourceName` | PreLoad `K=0x1c`, `gRev > 0x15`, followed immediately by `subi r3,r31,0x218; bl AltFontResourceFileUpdated` | PROVEN |
| 0x208 | `ObjDirPtr<ObjectDir>` (0xC) | `mObjDirPtr` | ctor `827F3FB0 addi r10,r30,0x208`; `827F4CF4`/`827F4D38` | PROVEN |
| 0x214 | — | (vtordisp) | forced: `mObjDirPtr` ends at 0x214 and the vbase is at 0x218 | PROVEN by arithmetic |

Sum check: `0x214 - 0x140 = 0xD4`, and the members above sum to exactly `0xD4`.
**No gaps, no leftovers.** The only slack is the natural 3-byte tail padding after
each of the three `bool`s (0x18d–0x18f, 0x1cd–0x1cf, 0x1f9–0x1fb).

### 3b. Independent re-derivation

The table above was derived by the lane lead from `FitText`, `PreLoad`, `PostLoad`,
`SetEditText`, `Font()` and `DrawShowing`. A second worker then re-derived it from a
*different* function set — the constructor (`0x827F3D50`), `Save` (`0x827F2E98`),
`SyncProperty` (`0x827F6458`), `LabelUpdate` (`0x827F6258`) and `AdjustHeight`
(`0x827F5410`) — and reached the **identical** result with zero disagreement.

The `SyncProperty` pass is the strongest single piece of evidence in the lane: its
29 arms were decoded and each arm's **property-name string was read directly out of
`orig/45410914/band.exe`** at the `lbl_` address the arm references. So every member
below has a *named retail string* bound to its offset — `text_token`→0x164,
`edit_text`→0x168, `icon`→0x174, `text_size`→0x180, `alignment`→0x184,
`caps_mode`→0x188, `markup`→0x18c, `leading`→0x190, `kerning`→0x194,
`italics`→0x198, `fit_type`→0x19c, `width`→0x1a0, `height`→0x1a4,
`fixed_length`→0x1a8, `reserve_lines`→0x1ac, `preserve_trunc_text`→0x1b0,
`alpha`→0x1bc, `color_override`→0x1c0, `use_highlight_mesh`→0x1cc,
`font_mat_variation`→0x1d0, `alt_mat_variation`→0x1d4, `alt_text_size`→0x1d8,
`alt_kerning`→0x1dc, `alt_text_color`→0x1e0, `alt_z_offset`→0x1ec,
`alt_italics`→0x1f0, `alt_alpha`→0x1f4, `alt_style_enabled`→0x1f8,
`alt_font_resource_name`→0x1fc.

### 3c. Size, and the four-byte correction

`UILabel::NewObject` (`0x827F4DC0`) does `li r3,0x24c; bl operator new` ⇒
**`sizeof(UILabel) == 0x24C`**. The UILabel vbtable (`lbl_821257F0 = [-4, 0x214,
0x240]`) and the ctor's `subi r10,r11,0x214; stwx r10,r11,r30` place the layout as:
own members `[0x140,0x214)`, vtordisp at 0x214, `Hmx::Object` vbase at 0x218,
second vtordisp at 0x240, second (8-byte) vbase at 0x244.

So **UILabel's non-virtual size *as a base of a derived class* is 0x214**, and the
0x218 figure quoted in the old header comment was the `~UILabel` *calling
convention* (`this` = `UILabel* + its own vbase offset`), not a size. `BandLabel`
constructs and destroys its next base at `this+0x214`
(`addi r3,r30,0x214; bl fn_82801BF8`; `stw lbl_82038954,0x214(r30)`), which is only
possible if UILabel ends at 0x214.

**The reconstruction does not change the size.** `cl.exe
/d1reportSingleClassLayoutUILabel` on the *pre-change* header reports
`class UILabel size(588)` = 0x24C — already correct — with `(vtordisp for vbase
Object)` at 532 = 0x214 and `(vtordisp for vbase RndHighlightable)` at 576 = 0x240.
The old block happened to total 0xD4 as well (4 + 4 + 0xC + 4 + `ObjVector` 0x10 +
0xAC = 0xD4), so the old `mUnkTU5Tail[0xAC]` had the right **total** and a wrong
**interior**. This is good news for risk: the change is purely interior, so ring-B
derived classes (BandLabel/AppLabel/HamLabel/UIButton/BandButton) are **not**
shifted.

### 3d. Compiler-verified component sizes

The member arithmetic rests on four type sizes, all taken from
`/d1reportSingleClassLayout` rather than from header comments:

| type | size |
|---|---|
| `String` | 12 (0xC) — payload `char*` at +8 |
| `Symbol` | 4 |
| `ObjPtr<RndFont>` / `ObjPtr<UIColor>` | 12 (0xC) — payload at +8 |
| `ObjDirPtr<ObjectDir>` | 12 (0xC) |

(An early hand estimate that `String` was 0x10 was **wrong** and would have
corrupted the whole tail; retail `PreLoad` settles it directly — the
`mPreserveTruncText` read at `K=0x68` (0x1b0) is followed by `mAlpha` at `K=0x5c`
(0x1bc), a gap of exactly 0xC.)

Summing the reconstructed members with those sizes gives exactly
`0x140 → 0x214`. Independent of the retail evidence, the arithmetic closes.

### 3e. The reconstruction, verified by the compiler

`/d1reportSingleClassLayoutUILabel` run against the **reconstructed** header returns
`sizeof(UILabel) = 588 (0x24C)`, nvsize `532 (0x214)`, vbases `Object` @0x218
(vtordisp @0x214) and `RndHighlightable` @0x244 (vtordisp @0x240) — **identical to
the pre-change `mUnkTU5Tail[0xAC]` version in every one of those numbers**. All 35
members land on exactly the offsets claimed in §3. The reconstruction is therefore
**layout-neutral**: it cannot shift a derived class, and it cannot regress anything
that was matching against the placeholder.

Three independent confirmations of nvsize 0x214:
1. this compile;
2. a `HamLabel` compile — identical member offsets, and `HamLabel`'s
   `UITransitionHandler` base lands at 532 = 0x214, `sizeof(HamLabel)` = 0x290;
3. retail asm — `build/45410914/asm/HamLabel.s:99` `addi r4, r3, 0x214` (the only
   `0x2xx` this-offset in the file), and `BandLabel.s` shows `addi r3,r11,0x214`
   into a `UITransitionHandler` method plus two 0xc-stride objects at 0x218 and
   0x224 each read at `+8` — i.e. `UITransitionHandler{vfptr@0x214, mInAnim@0x218,
   mOutAnim@0x224}`.

★ That last point **corrects the old header comment**, which asserted "BandLabel's
`UITransitionHandler` base must land at 0x218". 0x218 is `mInAnim` (base + 4); the
base is at 0x214. The old comment's *conclusion* (a 0xAC tail) happened to be right
for the wrong reason.

### 3f. Other header comments the compiler contradicts

Collected while establishing the anchors; all are the same DC3-staleness class, and
all are recorded here so the next lane does not re-discover them:

* `src/system/ui/UILabel.h` (pre-change) — **every** member comment was wrong
  (`mText 0xd0`→0x140, `mTextToken 0x114`→0x144, `mLabelText 0x118`→0x148,
  `mIconChar 0x120`→0x154, `mTextEmpty`→0x155, `mDirty`→0x156,
  `mLabelStyles 0x124`→0x158).
* `src/system/ui/UILabel.h` — `LabelStyle::mFontResource // 0x14` is really **0xc**,
  and `sizeof(LabelStyle)` is **0x1c**, not 0x28. (DC3's `ObjPtr` is 0x14; ours is
  0xc.)
* `src/system/ui/UITransitionHandler.h` — `mOutAnim // 0x18` → **0x10**;
  `mAnimationState // 0x2c` → **0x1c**; `mChangePending // 0x30` → **0x20**;
  `mOutAnimStarted // 0x31` → **0x21**. Same stale-0x14-ObjPtr cause, and retail
  `BandLabel.s` independently confirms the compiler.
* `src/system/rndobj/Text.h` — every member comment is written **relative to the own
  block**, so add **+0xD0** for an absolute offset.
* `src/system/ui/UIComponent.h` — clean; all comments agree with the compiler.

---

## 4. Retail-vs-Wii-dev divergences discovered

`../rb3` is the Wii **DEV** decomp; each divergence below would have cost a whole
function had it been carried over blindly. Adding to the §7 standing list:

1. **`mAlignment`, `mCapsMode`, `mFitType` are 4-byte enums in retail-360**, not the
   packed `unsigned char`s at 0x1a0..0x1a2 that the Wii header shows. Retail
   `PreLoad` reads 4 bytes *directly into the member*; the Wii source reads into
   `int` locals, `MILO_ASSERT`s them `< 255`, then narrows.
2. **`mMarkup`, `mUseHighlightMesh`, `mAltStyleEnabled` are three separate `bool`s**
   at 0x18c / 0x1cc / 0x1f8, not a `:1` bitfield triple sharing byte 0x1a3.
3. **`mFixedLength` / `mReservedLine` are `int`, not `short`.**
4. **Retail has a real `String` member at 0x168** (`mEditText`); the Wii decomp
   models that stream field as a discarded local
   (`if (gRev > 0xD) { String s; bs >> s; }`).
5. Consequently the retail member *order* differs from the Wii header's tail: retail
   places the scalars `mAlignment`/`mCapsMode`/`mFitType` up beside `mTextSize`,
   whereas the Wii header has them packed at the very end before
   `mAltFontResourceName`.

6. **`UILabel::PostLoad` has a third arm in retail.** Wii has
   `if (!mIcon.empty()) unk114 = mIcon; else SetTextToken(mTextToken);`. Retail
   (`0x827F76B0`) inserts `else if (!mEditText.empty() && AllowEditText())
   mLabelText = mEditText;` between them.
7. **`UILabel::SetEditText(const char*)` is an empty stub in the Wii dev build**
   but has a real body in retail (`0x827F4B68`) that writes `mEditText` and then
   re-dispatches through `SetDisplayText` (vtable slot 0x58).
8. **`LabelUpdate` takes two `bool`s in retail** (`0x827F6258`, called
   `LabelUpdate(false, true)` from `PostLoad`), matching the Wii signature — the
   tree's DC3-derived one-argument `LabelUpdate(bool)` was wrong.

9. **Retail's `Save` is a real rev-0x18 serializer** (`0x827F2E98`, 740 B); the Wii
   source has only the `SAVE_OBJ` assert stub.
10. **Retail's `text_size` / `alt_text_size` propsync arms store and load the raw
    float** — no `GetPctHeightFromTextSize` / `GetTextSizeFromPctHeight` conversion
    as in the Wii source.
11. Retail declaration *order* differs from the Wii header's for
    `mFitType`/`mAlignment`/`mCapsMode`/`mMarkup`: the Wii header packs all four
    into 0x1a0–0x1a3 at the very end of the class; retail widens three of them to
    4-byte enums and relocates the group into the middle, at 0x184–0x18f and 0x19c.
    This is exactly what produced the two "extra bytes" puzzles the reconstruction
    started from (the 0xC between `mText` and `mTextSize` is `mEditText`; the 0x10
    between `mTextSize` and `mWidth` is this relocated group).

Incidentally, `String`'s char* payload sits at **+8** within the 0xC-byte object
(PostLoad reads `mEditText`'s at 0x170 and `mIcon`'s at 0x17c) — useful for
decoding any other Milo class.

## 4b. A tooling defect found on the way

`scripts/target_symbol_map.json` maps `0x827F4EC8` to
`?Copy@UILabel@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z`. That function is
unambiguously **`UILabel::PreLoad`** — it opens with `LOAD_REVS` (`lwz` a packed
word, `srwi 16`, `sth` the pair into the `gRev`/`gAltRev` globals at
`lbl_82E0756C`), calls `UIComponent::PreLoad`, and is a pure rev-gated
`BinStream` read chain. The **same mispair repeats one slot along**: `0x827F76B0`
is mapped `?Load@UILabel@@…` but is unambiguously **`UILabel::PostLoad`**. Both
mispairs are consistent with the map being built from a *virtual-order* alignment
that this tree's UILabel vtable no longer shares with retail — so the whole
`UILabel` region of the map is suspect, and the reported match% of
`?Copy@UILabel@@` (9.75%) and `?Load@UILabel@@` (19.73%) are measuring the wrong
target functions entirely.

A third, even starker case: `0x827F2438` is mapped
`??0?$_STLP_alloc_proxy@PAULabelStyle@UILabel@@…` but the entire function is

```
827F2438  lwz r3, 0x144(r3)
827F243C  blr
```

i.e. `UILabel::TextObj()`. More generally, **every `…LabelStyle…` name the map
carries for this unit is DC3-transferred junk** — retail RB3's `UILabel` has no
`ObjVector<LabelStyle>` at all, and the `LabelStyle`-named COMDATs pinned into the
`UILabel.cpp` split (0x822A6878, 0x8234B270/B4D0/C298/C778/D080, stride 0x1c) lie
*outside* the `0x827F2148..0x827F7AD8` span. It is worth checking whether the same
DC3 transfer contaminated other engine units.

Note that a mispair is **worse than a gap**: a gap reads a truthful 0%, whereas a
mispair reads a plausible-looking number against the wrong target and silently
misdirects anyone who grinds it. This is an instance of the known **map-mispair** class
(`docs/plans/tu-pin-wave-2026-07-29.md` §7 / the `objdiff pct INVERTS` memo): a
mispaired name reads as a false low match% and silently misdirects anyone who
grinds it. The neighbouring `?Copy@UILabel@@$4PPPPPPPM@A@AAX…` thunk at
`0x827F5338` is correspondingly the `PreLoad` adjustor thunk.

---

## 4c. ★ The ceiling: `RndText` is DC3-shaped too

The single most important negative result of this lane. Retail
`UILabel::DrawShowing` (`0x827F4910`) compiles `mText->GetFont()` to an **inline**
member load `lwz r11, 0xec(r11)` off the `RndText*`. This tree's
`src/system/rndobj/Text.h` carries the **DC3** member block (ends near 0xcc, has
`ObjVector<Style> mStyles`, `std::vector<FontMapBase*> mFontMaps`, …) and has
nothing at 0xec. Retail RB3's `RndText` is the Wii shape
(`ObjOwnerPtr<RndFont> mFont` — object at 0xe4, payload at 0xec, since ObjPtr's
payload is at +8).

The compiler confirms the mismatch exactly: `/d1reportSingleClassLayoutRndText` on
this tree gives `class RndText size(464)` with own members starting at 216 (0xD8) —
`String mText` 0xD8, `mWidth` 0xE4, `mHeight` 0xE8, **`mCircle` 0xEC**. So at the
very offset where retail keeps the font pointer, this tree has a `float`.

So fixing `UILabel` is **necessary but not sufficient**. Every UILabel-family
function that *inlines* an `RndText` field access stays capped until `RndText`'s
layout is reconstructed the same way. Functions that merely *call* out-of-line
`RndText` methods are unaffected.

**`RndText` is therefore the natural next lane**, and it is a strictly bigger prize
than `UILabel` was: `RndText` is a base/member of the entire label, list, text and
HUD families. The §2 recipe applies to it verbatim. Two smaller opaque-tail classes
are also still outstanding and would fall to the same recipe:
`src/system/flow/Flow.h:51` (`char mUnkTU5_0x30[0x50]`) and
`src/system/flow/FlowSound.h:70` (`char mUnkA4[0x28]`).

## 5. Yield — the honest number

### 5.0 Measured result

| branch | whole-binary `matched_functions` | delta |
|---|---|---|
| main `9df262c9` (baseline) | 40,302 | — |
| `laneBO3-layout` (layout + `UILabel.cpp` port) | 40,338 | +36 |
| `laneBO3-map` (homing reveal wave, independent) | 40,308 | +6 |
| **`laneBO3-merge` (both)** | **40,342** | **+40** |

The two levers are **not** fully additive (36 + 6 = 42, measured 40): two of the map
reveals are made redundant by the port. **+40 with zero regressions is the lane's
verified number**, from a full build of the merged branch.

Verified by the lane lead independently of the worker that produced it: re-reading
both `report.json`s and diffing the strict-100 sets gives **+37 / −0 by
`(unit, name)` and +37 / −0 by bare `name`** (the one-function difference from the
`measures.matched_functions` delta of +36 is a counting-convention difference, so the
conservative **+36** is the number quoted). **Zero regressions anywhere in the tree.**

All 36 gains land in `default/UILabel`, which goes **67/158 → 104/158**. The other
twelve ring units are exactly flat, which is the expected result given §3e (the
layout is size-neutral, so nothing downstream should move) — and it is the useful
control: it means the gain is the port, not a global perturbation.

★ **Attribution caveat, so nobody double-counts.** Every one of the 37 gains has an
**anonymous `fn_XXXXXXXX`** target name, and the size histogram is
**10 × 32 B, 18 × 40 B, 8 × 44 B, 1 × 64 B** — i.e. **26 of 37 are the 40/44-byte
shape** that lane BO-5 is separately classifying as "one instruction from matching"
in this very unit. This is a real interaction, not a coincidence: these are the
member-accessor and small-forwarder bodies whose correctness depends directly on the
member offsets. They are counted here as this lane's yield because the layout is what
made them correct, but whoever reconciles the lanes should count them **once**.

### 5.1 Why the number is small relative to the brief

**The layout change on its own flips essentially nothing** — the yield above is the
*port* that the layout unblocked. That is worth stating plainly, because the mission
brief expected "~5 functions in BandButton alone and presumably many more across the
whole UILabel family", and the BandButton figure in particular did not materialise.

A full census of the family (12 units, 209 sub-100% functions) found that outside
`UILabel.cpp` there are exactly **7** functions anywhere in the tree that are
blocked *by a UILabel tail member*:

| function | unit | before | what it needs from the tail |
|---|---|---|---|
| `BandLabel::PreLoad` | BandLabel | 6.03% | mFitType, mWidth, mHeight, mLeading, mAlignment, mKerning, mTextSize, mCapsMode, mTextToken |
| `BandButton::PreLoad` | BandButton | 0.00% | the same 8, + `BandLabel::LoadOldBandTextComp` |
| `BandButton::Update` | BandButton | 0.73% | mLabelDir, mFontMatVariation, `UILabel::Update` |
| `BandButton::DrawShowing` | BandButton | 76.10% | `mText->GetFont()`, `UpdateAndDrawHighlightMesh` (→ mUseHighlightMesh, mLabelDir) |
| `ScoreDisplay::SetAlphaColor` | ScoreDisplay | 89.47% | `mCombinedLabel->mAlpha` |
| `UIListLabelElement::Draw` | UIListLabel | 0.00% | mAlpha, mAltAlpha |
| `LabelShrinkWrapper::UpdateAndDrawWrapper` | LabelShrinkWrapper | 0.00% | `Alignment()` → mAlignment |

Everything else that greps positive for those member names is a same-named member
of an unrelated class (`RndTex::mWidth`, `NoteTube::mAlpha`, `Award::mIcon`,
`StarDisplay::mAlignment`, …) — **false positives**. Site count is not defect count.

Inside `UILabel.cpp` the tail gates a **body port**, not a layout flip: 16 of its 91
sub-100% functions provably touch tail members (upper bound ~40 once the
biased-base-register bodies are resolved), but the unit is a **DC3 port** that is
missing 25 RB3 functions outright, so those bodies have to be written before the
offsets can matter. The 38 EH funclets in that unit ride along with their parents
and are not independently blocked.

The cause that is genuinely larger than the layout is **the missing `UILabel.cpp`
body port** — 25 absent RB3 functions to write, ~10 DC3-only ones to remove — and
above it sits **§4c: `RndText` is DC3-shaped**, which caps the draw/update path
whatever we do to `UILabel`.

A second candidate cause, the `target_symbol_map.json` gap, was this lane's first
hypothesis and is **REFUTED — see §5b**. The refutation is recorded rather than
quietly deleted, because the premise it rested on is written into `CLAUDE.md`.

### Loss-risk denominator
Computed from the exact `ninja -t deps` header closure (1070 objs):
* Ring A — units that *inline-bake* a UILabel member offset (`TextObj()`,
  `GetTextToken()`, `Style/LStyle`): 9 units, **280** currently-matched functions.
* Ring B — units that *derive* from UILabel (size/vtable sensitive): BandLabel,
  AppLabel, HamLabel, UIButton, BandButton: 5 units, **280**.
* **A∪B ≈ 560 matched functions is the honest loss-adjudication denominator**,
  provided the reconstruction preserves `sizeof(UILabel)` (it does — the compiler
  reports 0x24C both before and after, see §3c) and the anchored offsets
  0x144 / 0x164 / 0x1BC (it does). Measured ring baseline: **567** matched.
* The full transitive `#include "ui/UILabel.h"` closure is 290 objs / 22,381
  matched functions, but those see `UILabel` only through opaque pointers — that
  number is the theoretical closure, **not** a risk estimate.

## 5b. ★ A refuted premise: an unmapped target is NOT an automatic 0%

`CLAUDE.md` states, of the pre-compile `obj_target_symbol_renamer` step, that
"without a map entry a pinned game TU reads a false 0%". This lane took that at face
value, measured the gap (34 map entries inside the `UILabel.cpp` span; 130 of the
unit's 158 target functions still anonymous `fn_XXXXXXXX`; 175 of the family's 209
sub-100% functions unmapped) and concluded the map was the dominant ceiling. **That
conclusion was wrong, and it was worth the build cycle to find out.**

The premise does not generalise from *units* to *individual functions*. The local
objdiff fork has a **cross-TU byte-identical promotion pass**
(`../objdiff/objdiff-core/src/diff/mod.rs` ≈1045-1130 — the same pass that
`masked_equal_functions` discloses), so an unnamed target that is byte-identical to
a base symbol is credited **without** any map entry. Measured tree-wide:

* **21,275 of the ~40,300 matched functions (53%) have anonymous `fn_`/`lbl_`
  target names.**
* Inside `default/UILabel` specifically: **45 anonymous targets already read
  100.00**, 43 read partial, only 42 read 0.

So the map gap is real arithmetic but not a ceiling. Two further consequences:

* **The direction of causation is the opposite of what was assumed.**
  `homing_scan.py` proposes a name only when our compiled body is already
  byte-identical to retail, so a unit whose source is the wrong shape *can never
  home, by construction*. The UILabel map gap is **downstream** of the body port,
  not upstream of it.
* **Fixing the three confirmed mispairs (§4b) pays zero metric.** A reverse-homing
  scan over all 1,068 objs found **zero** reloc-masked byte-identical bodies for any
  of the seven candidate VAs. Renaming them would replace a truthful 0% with a
  truthful partial %. They are still worth fixing for **correctness** — a mispair
  reads a plausible number against the wrong target and misdirects whoever grinds it
  next — but not for score, and not before the merged tree exists.

What the map lever *did* pay, honestly: running the existing homing tooling
(`homing_scan.py` + `homing_gen4.py --reveal-frag`) over the 12 label-family objs
gave 9 map-only reveals plus 1 pinnable gap (`system/ui/UILabelDir.cpp`,
`0x82812080` = `??1UILabelDir@@UAA@XZ`). Only **3 of the 9** could pair — a reveal
only pays when the covering unit's compiled obj also *defines* the symbol — for a
verified whole-binary **40,302 → 40,308 (+6, 0 losses)**, unit-agnostic by
`(unit,name)` and by bare `name`.

### An out-of-lane finding worth someone's time
While sizing the above, a **full-tree** gen4 scan (1,068 objs, 1,279 unique) came
back with **89 map-only reveals + 20 pinnable gaps**, of which **63 of the 89 pass
the payability filter** — roughly **80 cheap strict matches sitting available right
now**, concentrated in the `Rnd.cpp` / `Synth.cpp` / `UI.cpp` `NewObject` families
and `band3/game/TrackerDisplay.cpp`. `docs/plans/homing-scan-round5-2026-07-26.md`
declared that flywheel drained at 27,896 matched; the tree is now at 40,302 and it
has **clearly refilled**. Artifacts left in place:
`~/tmp/laneBO3/homing_all/merged.json`, `~/tmp/laneBO3/reveal_all.json`,
`~/tmp/laneBO3/hgen4_all_{blocks,map_fragment}.json`. This is not this lane's work
and is not counted in its yield.

## 6. What stayed blocked, and why

* Anything that inlines an `RndText` field — see §4c. `RndText` is the next lane.
* `BandButton::PreLoad` and `BandLabel::PreLoad` are additionally **map-mispaired**
  (§4b), so they will read a false low % until the map is corrected, independent of
  the layout.
* `HamLabel` has no rb3-Wii oracle file at all, so its residual is unclassified.
* The permuter is banned for this lane, so codegen-class residues
  (e.g. `BandButton::SetState`, 98.1%, whose source is already the faithful Wii
  body) are deliberately left alone.
