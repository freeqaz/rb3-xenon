# Mat_NG DC3_REV_MEMBER lever — DEFERRED (deep layout, not a one-liner)

**Date:** 2026-06-10  ·  **Branch:** `matng-lever`  ·  **Baseline matched:** 6596 (unchanged — no Mat.h edits applied)

## TL;DR

The `Mat_NG::SetupShader` `+0x3c` finding is **real but is NOT a clean DC3-added-member
block** like TexRenderer (9150f3c) or PostProc (2d82a94). The retail RB3
`RndMat`/`BaseMaterial` layout is **fully reordered AND bool-repacked** relative to our
(DC3-derived) headers — 34 distinct `this`-relative offset deltas with **opposite signs**.
There is no single 0x3c block to gate. Editing the widely-shared `Mat.h` with a 0x3c guess
would corrupt the layout and risk tree-wide regressions. **Per the task's own gate ("deliver
the offset-bracket analysis WITHOUT applying guesses to the widely-shared header"), Mat.h was
left untouched.**

## Headers are byte-identical ours == DC3 (confirmed)

```
diff src/system/rndobj/{BaseMaterial,Mat,Mat_NG}.h  ../dc3-decomp/...  → identical (exit 0)
```

So ours is DC3's layout exactly. The divergence is retail-RB3-vs-DC3. rb3-Wii's `RndMat` is a
*different* class entirely (`RndMat : public Hmx::Object` directly, no BaseMaterial/NgMat split,
flags as `bool : 1` bitfields at 0xac/0xad) — it is the Wii GX path, a weak oracle for the
X360 NgMat layout, but it does corroborate the *flag-packing* idea (see below).

## The single-function finding (what triggered the lever)

`?SetupShader@NgMat@@QAAX_N0@Z` — 3 mismatches, all `mDirty`:

```
[12] lwz r11, 0x188(r31)  vs  0x1c4(r31)   [+0x3c]
[15] lwz r11, 0x188(r31)  vs  0x1c4(r31)   [+0x3c]
[28] stw r11, 0x188(r31)  vs  0x1c4(r31)   [+0x3c]
```

gate-zero PASSES: `mr r31, r3` at entry ⇒ `r31` is `this`. So retail's `mDirty` is 0x3c
**higher** than ours. In isolation this reads like "retail has 0x3c extra members before
mDirty" → i.e. the OPPOSITE of TexRenderer/PostProc (there we'd ADD members, not drop). But...

## The disproof: SetRegularShaderConst full layout map

`?SetRegularShaderConst@NgMat@@IAAX_N@Z` (87.98%) touches the whole class. The `this`-relative
(`r3`/`r31`) member accesses, sorted by retail offset (ours vs retail are NOT a constant shift):

| RETAIL | OURS  | retail−ours | note |
|-------:|------:|------------:|------|
| 0x0    | 0x0   | +0   | vtable |
| 0x28   | 0x2c  | −4   | mColor/Transform region: ours +4 |
| 0x2c   | 0x30  | −4   | |
| 0x30   | 0x34  | −4   | |
| 0x34   | 0x38  | −4   | |
| 0x44   | 0x94  | −80  | **bool flag (retail packs LOW, ours spreads HIGH)** |
| 0x54   | 0x98  | −68  | **bool flag** |
| 0x58   | 0x48  | +16  | ObjPtr region — opposite direction |
| 0xa0   | 0x28  | +120 | **member at retail 0xa0 is at ours 0x28 — full reorder** |
| 0xc2   | 0x17e | −188 | **bool flag (retail clusters low, ours scattered very high)** |
| 0xc3   | 0x11d | −90  | **bool flag** |
| 0xc4   | 0xb0  | +20  | |
| 0xd0   | 0xe8  | −24  | |
| 0xdc   | 0xdc  | +0   | one coincidental match |
| 0xe0   | 0x110 | −48  | |
| 0xec   | 0x16c | −128 | |
| 0xf0   | 0x170 | −128 | |
| 0xf4   | 0x174 | −128 | |
| 0xf8   | 0xcc  | +44  | opposite direction again |
| 0xfc   | 0xd0  | +44  | |
| 0x100  | 0xb4  | +76  | |
| 0x104  | 0xc4  | +64  | |
| 0x108  | 0xc8  | +64  | |
| 0x120  | 0xf4  | +44  | |
| 0x124  | 0x114 | +16  | |
| 0x128  | 0x138 | −16  | |
| 0x12c  | 0x13c | −16  | |
| ...    | ...   | −16  | |
| 0x140  | 0x150 | −16  | |
| 0x150  | 0x100 | +80  | |
| 0x168  | 0x118 | +80  | |
| 0x1a8  | 0x154 | +84  | |
| 0x1ac  | 0x158 | +84  | |
| 0x1c8  | 0x18c | +60  | **= the SetupShader +0x3c (mDirty tail region)** |
| 0x1f0  | 0x1b4 | +60  | tail |

34 distinct deltas, both signs, swings from −188 to +120. This is a **complete member
reordering + bool-flag repacking**, not a block insert.

## Root cause (hypothesis, oracle-corroborated)

Retail RB3 clusters its `bool` material flags **low and tightly** (retail 0x44, 0x54, 0xc2,
0xc3 — adjacent bytes), exactly the shape of rb3-Wii's packed-bitfield flag block
(`mIntensify:1 / mUseEnviron:1 / mPreLit:1 / mAlphaCut:1 / ...` at Wii 0xac/0xad). Our
DC3-derived `BaseMaterial.h` instead declares each flag as a **separate full `bool` byte**
scattered through the class (mUseEnviron@0x3c-comment but compiled ~0x98, mRimLightUnder,
mEnvironMapFalloff, etc.). DC3 (newer engine) refactored the material class — expanded the
packed flags into individual bytes and reordered the members. Retail RB3 predates that
refactor.

The `// 0xNN` comments in `BaseMaterial.h`/`Mat.h` are **stale** (Wii-derived / aspirational):
e.g. `mUseEnviron // 0x3c` but it compiles at ~0x98; `mDirty // 0x228` but it compiles at
0x188. Per hasreal-grind §4, trust the compiled offsets, not the comments — which is exactly
what this table does.

## Why not apply a fix

- No single member or block totals 0x3c — the +0x3c is just the *accumulated tail* delta after
  ~0x80 of reordering swings back. Gating a 0x3c block in Mat.h would not reproduce the
  retail order and would shift OTHER members the wrong way (the −80/+120/−188 entries).
- `Mat.h`/`BaseMaterial.h` are **widely included** (every renderer/material TU). A wrong layout
  guess ripples tree-wide. The honest move is to reconstruct the *full* retail layout first.
- This is the hasreal-grind §3h ("class layout reconstruction, multi-member, deep") /
  §4-scattered case the playbook says to defer.

## What a real fix would require (future work)

Reconstruct the retail `BaseMaterial`/`RndMat` member order + flag packing from the binary:
1. Repack the `bool` flags as a tight low block (candidate: a packed-byte or `:1` bitfield
   group near retail 0x44/0x54 and 0xc2/0xc3), matching rb3-Wii's grouping but as the X360
   byte layout.
2. Reorder the ObjPtr/float/Color members to the retail order implied by the table above
   (retail 0xa0→ours 0x28, retail 0x58→ours 0x48, etc.).
3. Validate against `SetRegularShaderConst` (best probe: 34 member touches) until all
   `this`-relative deltas → 0, then whole-binary A/B.

This is a dedicated multi-session layout pass, gated behind a `RB3_*` define per the
established idiom — not a single-commit lever.
