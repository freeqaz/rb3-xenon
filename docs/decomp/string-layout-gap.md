# String / FilePath layout gap — investigation (2026-05-28)

**Status: the "String is 8 bytes here but the target wants 12 (second vptr from
MI)" hypothesis is REFUTED.** The 360 target's `String` is **8 bytes with a
single vtable pointer**, and our `src/system/utl/Str.h` already compiles to that
exact layout. `LightHue::mKeys` is at **0x38 in the target** and our build
already honors it (functions sit at 99.7–99.8%, not the ~50% a real 4-byte
member shift would cause). The residual is the FP-register-swap fuzzy-gap class,
not a struct-size bug. **Recommendation: do NOT touch `String`/`FilePath`.**

This doc records the verified layouts, the real root cause of the prior agent's
confusion (Wii-stride annotations bleeding into 360 game-code headers), and a
safe protocol for if anyone wants to re-verify.

---

## 1. Verified layouts (ours vs target vs dc3)

### 1.1 Target (retail XEX) — ground truth from the binary

Source: `build/45410914/asm/LightHue.s` (dtk-split target disassembly).

- `LightHue::TranslateColor` = target `fn_824D4B08` (`.text:0x410`):
  - `824D4B24  lwz r11, 0x38(r3)` — `mKeys` begin pointer
  - `824D4B2C  lwz r10, 0x3c(r3)` — `mKeys` end pointer
  - `824D4B30  addi r30, r3, 0x38` — `&mKeys`
- `LightHue::OnSaveDefault` (target `fn_824D48D0`, `.text:0x1D8`):
  - `824D4914  lwz r4, 0x34(r30)` — `mPath.mStr` (FilePath member, offset +4)
  - `824D494C  addi r4, r30, 0x38` — `&mKeys`

So in the **target**:
| member            | offset | size |
|-------------------|--------|------|
| `mLoader`         | 0x2c   | 4    |
| `mPath` (FilePath)| 0x30   | **8**|
| ↳ vptr            | 0x30   | 4    |
| ↳ mStr            | 0x34   | 4    |
| `mKeys`           | 0x38   | …    |

`mPath` occupies exactly 0x30..0x38 = **8 bytes**, with the vtable pointer at
0x30 and `mStr` at 0x34. `mKeys` therefore lands at **0x38**.

### 1.2 Target `String` is one vptr + mStr (8 bytes) — proven by named dc3 symbol

dc3 has the leaked `.map`, so its `String` ctor is named. From
`../dc3-decomp/build/373307D9/asm/system/utl/Str.s`, `??0String@@QAA@XZ`:

```
827CDB14  stw r10, 0x4(r3)            ; mStr (= gEmpty+4) at String+0x4
...
827CDB28  lis  r11, "??_7String@@6B@"@ha
827CDB34  stw  r11, 0x0(r31)          ; vtable "??_7String@@6B@" at String+0x0
```

Single store of a single vtable (`??_7String@@6B@`) at offset 0x0; `mStr` at
0x4. **There is no second vptr.** MSVC folds the MI-inherited `TextStream`
virtuals into `String`'s one primary vtable because `FixedString` (the primary
base) is non-polymorphic and `TextStream` carries no data members — the secondary
base contributes 0 bytes. Total `String` size = **8 bytes**.

### 1.3 Ours — header is byte-identical to dc3, compiles to the same 8 bytes

- `src/system/utl/Str.h:24-57` `FixedString` — one member `char *mStr` (0x0),
  non-polymorphic. (Capacity is **not** a member: it lives at `mStr-4`, see
  `Str.h:33` `capacity()` and `Str.cpp:84-93` ctors.)
- `src/system/utl/TextStream.h:7-32` `TextStream` — has `virtual ~TextStream()`
  + pure `virtual Print`, **no data members**.
- `src/system/utl/Str.h:62` `class String : public FixedString, public TextStream`.
- `src/system/utl/FilePath.h:6` `class FilePath : public String` (adds only
  static members `sRoot`/`sNull`, no instance fields).

`diff` of our `Str.h`, `TextStream.h`, `FilePath.h`, `LightHue.h`/`.cpp` against
`../dc3-decomp/src/system/...` is **empty** — the files are identical.
`tools/struct_db.py info` and `lookup_struct_offset LightHue 0x38` both report
`LightHue::mKeys at 0x38` — our headers already *declare* 0x38, i.e. they assume
an 8-byte FilePath/String.

### 1.4 Cross-binary corroboration

- dc3 unit `default/system/utl/Str` is **100% matched** (incl. copy ctor
  `??0String@@QAA@ABV0@@Z`) with this exact header. Same compiler / flags / engine.
- Our `default/LightHue`: `TranslateColor` 99.7%, ctor `??0LightHue` 99.8%,
  `??_GLightHue` 100% (`build/45410914/report.json`).
- dc3 `default/system/world/LightHue` unit aggregate 98.9% — same near-miss
  residual on the same source, confirming it is engine-wide codegen noise, not a
  layout error.
- Our `LightHue::TranslateColor` source is **byte-identical** to dc3's.

**Conclusion:** our `String`/`FilePath`/`LightHue` are 8-byte-correct and match
the target. The 0.3% on `TranslateColor` and 0.2% on the ctor is the
FP-register-swap fuzzy gap (see `feedback_fuzzy_gap_needs_permuter.md`), which
needs the source permuter, not a layout edit.

---

## 2. Why the prior agent saw "12 / second vptr"

Two independent things misled the prior analysis:

1. **rb3-Wii's `String` really is a different, 12-byte class.**
   `../rb3/src/system/utl/Str.h:26` is `class String : public TextStream` (single
   inheritance) with **explicit `unsigned int mCap; char *mStr;` members** →
   Wii layout = vptr(4) + mCap(4) + mStr(4) = **12 bytes**. That is the Wii
   MWCC source/ABI, *not* the 360 engine. The 360 engine (dc3 + us) uses the
   `FixedString + TextStream` MI variant where capacity lives at `mStr-4`
   instead of as a member, giving 8 bytes.

2. **The Wii 12-byte stride leaked into band3 game-code header annotations.**
   `src/band3/` headers were ported from rb3-Wii and carry Wii-era `// 0xNN`
   offset comments. Example `src/band3/meta_band/BandMachine.h:29-32`:
   ```
   String mCurrentSongPreview; // 0x54
   String mPrimaryBandName;    // 0x60   (+0xC)
   String mPrimaryProfileName; // 0x6c   (+0xC)
   int    mPrimaryMetaScore;   // 0x78   (+0xC)
   ```
   The **12-byte (0xC) stride** between consecutive `String` members encodes the
   *Wii* String size. These annotations are **unverified against the 360
   target** — `BandMachine` is not compiled/matched yet (no unit in
   `report.json`). On 360 these strides should be **8**, not 0xC; the comments
   are stale Wii artifacts, not evidence that 360 String is 12 bytes.

So "String is 12 in the target" conflated (a) the Wii source and (b) Wii-stride
comments in not-yet-built game headers, with (c) the actual 360 target — which is
8 bytes, as the LightHue target asm and dc3's named ctor both prove.

---

## 3. Blast radius (informational — no change recommended)

Members embedded **by value** across `src/`:
- `String mXxx;` by-value sites: **194**
- `FilePath mXxx;` by-value sites: **23**

If anyone *did* change `String`'s compiled size, all ~217 of these (plus every TU
that constructs/copies a String on the stack) would shift. Because the target is
8 bytes and we already produce 8 bytes, **the correct action changes 0 of these.**
The risk runs the other way: forcing a 12-byte String (e.g. re-adding an `mCap`
member or a TextStream-first reorder to materialize a 2nd vptr) would *regress*
every currently-matching String-bearing function — exactly the failure recorded
at `src/system/utl/Str.h:59-61` (a `TextStream, FixedString` reorder dropped the
String copy ctor 96.7% → 39.4% by emitting an extra `TextStream` ctor call).

The band3 `// 0xNN` Wii-stride annotations on String members are cosmetically
wrong for 360, but they are only comments; they do not affect codegen and should
be corrected lazily as each band3 TU is actually matched (the real 360 offsets
fall out of objdiff at that point). That is bookkeeping, not a layout fix.

---

## 4. Ranked options

1. **DO NOTHING to `String`/`FilePath`/`FixedString`/`TextStream`.** ✅ recommended.
   The layout is verified-correct (8 bytes, one vptr) and matches the target.
   Pursue `LightHue`'s last 0.3% via the permuter (FP-regswap class), not a
   struct edit.
2. *(rejected)* Add an `mCap` member or pad to 12 bytes — would diverge from
   dc3 (100% with the current header) and the proven target, regressing ~213
   member sites. No upside.
3. *(rejected)* Reorder bases to `TextStream, FixedString` to force a 2nd vptr —
   the documented `Str.h:59-61` regression; the target has only one vptr anyway.
4. **Lazy cleanup (optional, low priority):** as each `src/band3/` TU that embeds
   a String/FilePath is brought to a match, fix its stale Wii-stride `// 0xNN`
   comments to the real 360 offsets observed in objdiff. Comment-only; never
   gate a fix on it.

---

## 5. Safe validation protocol (if re-verification is ever wanted)

This investigation was read-only. To re-confirm without disturbing the shared
tree (a permuter is live on the main build dir — never re-SPLIT/build there):

1. Spin up an isolated worktree: `scripts/setup_worktree.sh /tmp/wt-strcheck`.
2. In the worktree only, build and objdiff the canary set **before any change**
   to capture the baseline:
   - `default/LightHue`: `?TranslateColor@LightHue@@QAAXABVColor@Hmx@@AAV23@@Z`
     (99.7%), `??0LightHue@@IAA@XZ` (99.8%) — direct FilePath/mKeys offset users.
   - `default/Str`: `??0String@@QAA@XZ`, `??1String@@UAA@XZ`,
     `??0String@@QAA@ABV0@@Z` (copy ctor — the `Str.h:59-61` canary).
   - One band3 String-bearing TU once it's matchable, e.g. `Gem`/`Lyric`.
   Use the orchestrator `run_objdiff` with `project_dir=<worktree>` (it pairs the
   anonymous target `fn_` to the mangled base via the renamer; a bare
   `objdiff-cli diff -1 obj/X.obj -2 src/.../X.obj` will fail to pair because the
   target obj symbols are anonymous).
3. Any String-layout edit must show **net-positive across that whole canary set**
   in the worktree before it is even considered for the main tree. Given the
   evidence here, no such edit exists — expect every candidate to regress the
   copy-ctor canary.

---

## References
- `src/system/utl/Str.h` (esp. `:24-62`, prior-reorder warning `:59-61`)
- `src/system/utl/TextStream.h:7-32`, `src/system/utl/FilePath.h:6`
- `src/system/world/LightHue.h:33-35`, `src/system/world/LightHue.cpp:60-74`
- Target asm: `build/45410914/asm/LightHue.s` (`fn_824D4B08`, `fn_824D48D0`)
- dc3 named ctor: `../dc3-decomp/build/373307D9/asm/system/utl/Str.s`
  `??0String@@QAA@XZ`; dc3 `Str` unit 100% in
  `../dc3-decomp/build/373307D9/report.json`
- Wii (different ABI): `../rb3/src/system/utl/Str.h:26` (`String : public
  TextStream`, explicit `mCap`+`mStr`, 12 bytes)
- Wii-stride leak example: `src/band3/meta_band/BandMachine.h:29-32`
- Memory: `feedback_fuzzy_gap_needs_permuter.md` (String paragraph),
  `feedback_verify_assumptions.md`
