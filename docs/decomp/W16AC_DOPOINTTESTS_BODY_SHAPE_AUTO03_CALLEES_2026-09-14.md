# W16-AC — `DxRnd::DoPointTests` body shape (75.10 → 99.87) and the four `auto_03_8273D658_text` rows (→ `rnddx9/Utl.cpp`, +4 / +356 B)

Lane W16-AC (Fable), 2026-09-14. Worktree `~/tmp/wt-w16-ac`, branch `w16-ac` off main `196fe28c`.
Ruler: `report.json`, graded `name_check`. Every number below is from a full `./tools/ninja-locked`
(rc=0) followed by a `report.json` read with `int(x.get(k, 0))`; every change priced by
`tools/rowset_snapshot.py diff` (set-diff of the `fuzzy==100` row set), never by mismatch count.

| | `matched_functions` | `matched_code` | code% |
|---|---:|---:|---:|
| baseline (main `196fe28c`, lane-internal) | 43,301 | 3,988,808 | 38.930557 |
| after Item 1 (V7, `48db2ad2`) | 43,301 | 3,988,808 | 38.930557 |
| **after Item 2 (`3bad5ca5`)** | **43,305** | **3,989,164** | **38.934032** |

Item 1 moved `DoPointTests` from 75.10 to 99.87 fuzzy without crossing (Δ0 on both keys — the row is
1,392 B and all-or-nothing). Item 2 crossed four rows: +4 / +356 B, predicted exactly.

---

## Item 1 — `?DoPointTests@DxRnd@@AAAXXZ` (1,392 B, `default/Rnd_Xbox`)

### 1.1 What the three extra callee-saved registers hold in retail

Retail prologue saves r16–r31 (`__savegprlr_16`, frame −0x210); ours at baseline saved r19–r31
(frame −0x1F0). The brief asked for the three values. Read off the retail listing
(`build/45410914/asm/Rnd_Xbox.s`, keyed on `.fn ?DoPointTests…`, never the address column):

| reg | first def (retail addr) | value | live across |
|---|---|---|---|
| r16 | prologue region | `lis r16, sPointTestFence@ha` (`lbl_82C76FFC`) | the whole loop — every fence store/load |
| r17 | prologue region | `li r17, 1` | loop — stored as the "query issued" flag / `true` |
| r18 | prologue region | `li r18, 0` | loop — `nullptr`/0 stores |
| r20 | prologue region | `li r20, -1` | loop — `mAreaQueryIdx = mPointQueryIdx = -1` stores |
| r19 | prologue region | `addi r19, this, 0x1ac` (`&mPointTestQueries`) | loop |
| r24 | prologue region | `this+0x114` | loop |
| r22 | `0x82362404` | `lis r22, TheShaderMgr@ha` (`lbl_82C76CE0`) | the Matrix4 temporary + loop |
| r23 | `0x82362554` | `addi r23, r28, 0x8` = `&test.mAreaQueryIdx` | the query create/end calls |

So the "three values" are not three hoisted computations but the consequence of retail keeping **small
constants and one global base** (`sPointTestFence@ha`, `1`, `0`, `-1`) in callee-saved registers because
they are used on both sides of many `bl` sites inside the loop. Ours reloaded them per use because the
loop body's stores were structured differently (see V4/V6/V7). Once the body had retail's shape the
prologue moved to r16 by itself at V4 — no explicit hoisting was ever needed or written.

### 1.2 Variant ledger (all measured on `report.json`, graded)

| variant | commit | fuzzy / mpn | what changed | pre-registered | held? |
|---|---|---:|---|---|---|
| baseline | `196fe28c` | 75.10345 / 76.85632 | — | — | — |
| V1 | `1a5e2627` | 80.90 / 82.34 | DC3-tuned body + retail corrections | prologue → r16 | **no** (prologue unmoved) |
| V2+V3 | `23d07380` | 85.55 / 86.20 | inlined `CreateAndBeginQuery`/`EndQueryFrame`/`BeginQueryFrame` wrappers (cached manager pointer survives the `bl`); dropped DC3's `HiResScreen` early-out; local `RndShaderMgr &shaderMgr` | +2–4 pp each | yes |
| V4 | `9876d5b3` | 89.07 / 89.82 | `resize(mPointTests.size(), RndPointTest())` right-to-left evaluation; loop stores reordered | prologue → r16 | **yes** — prologue matched from here |
| V5 | in `c1826ec7` msg | 86.72 / 87.44 | named `const Hmx::Matrix4 &` for the `SetVConstant` temporary | +1 pp | **no — regression**, reverted |
| V6 | `c1826ec7` | 99.41092 / 99.69827 | `vtx`/`verts` vertex-buffer locals at **function scope** (MSVC stack-slot overlay is lexical, so loop-body locals got their own slots and shifted the whole frame) | frame −0x210 + slot map | yes |
| V7 | `48db2ad2` | **99.87069 / 99.98563** | read the flare back from `test.mFlare` instead of the local (`clrrwi` store-to-load forward) | the `clrrwi` site only; r27↔r28 was predicted to survive | half — the `clrrwi` closed **and** r27↔r28 flipped with it |
| V8 | (uncommitted, reverted) | 99.43965 / 99.98563 | `unsigned int &areaIdx/&pointIdx` references at loop head, used at the 4 query sites | "most likely Δ0, possibly the 8-site r22↔r23 swap flips" | **no — regression**: swap stayed, and the refs re-introduced an r28↔r29 loop-pointer swap + 5 `fadds` operand flips (31 charged sites) |
| V9 | (uncommitted, reverted) | 99.25288 / 99.41092 | hoisted `RndShaderMgr &shaderMgr = TheShaderMgr;` above `SetTransform`, called `shaderMgr.SetTransform` | "Δ0 most likely" | **no — regression** on both keys (an instruction changed, not just a register) — confirms the DrawRect comment in `Rnd.cpp:158-168` that hoisting the reference costs a register |

Whole-binary keys were unchanged (43,301 / 3,988,808) on every variant — the row never crossed.
Final state is **V7** (`48db2ad2`); V8 and V9 were reverted with `git checkout -- src/system/rnddx9/Rnd_Xbox.cpp`
inside the worktree (restore build 10 rc=0, rowset diff vs V7 empty).

### 1.3 Residual at V7 — what could NOT be sourced (for the next lane)

Two charged classes remain; neither has a source-visible knob I could find:

1. **`bl _M_fill_insert` at idx 97** — retail calls the `MidiParser::Note` instantiation, ours the
   `RndPointTest` one. This is the ICF fold-alias withdrawn by ALIAS-CONSOLIDATION 2026-08-19; it is
   correct that it stays withdrawn (different `T`, different node deallocator — not provably folded).
   Unfixable by source; do not re-add the alias.
2. **r22↔r23 callee-saved swap, 8 sites.** Retail assigns `lis TheShaderMgr@ha` (`lbl_82C76CE0`) to
   **r22** (first def `0x82362404`; uses `0x82362418`, `0x82362454`, `0x823627AC`) and
   `&test.mAreaQueryIdx` (`addi rX, r28, 0x8`) to **r23** (first def `0x82362554`; uses
   `0x823626E0` `mr r4,r23`, `0x82362710` / `0x82362738` `lwz r4,0(r23)`). Ours has the two values in
   the opposite registers at all 8 sites, every instruction otherwise identical. Both cheap probes aimed
   at it (V8: give the index a reference at the loop head; V9: bind the manager earlier) made the
   function worse. The allocation order is a pure allocator tie-break between a value defined before the
   loop and one defined inside it; `MSVC_X360_REGALLOC.md`'s declaration-order lever is measured inert
   for register-only swaps and `fixable-liveness.md` lists no construct for "which of two equally-live
   values gets the lower register". Permuter is OFF by directive. Left as-is.

### 1.4 Constructs that HELD (reusable)

- Inline the tiny manager wrappers (`CreateAndBeginQuery` etc.) so the cached `mOcclusionQueryMgr`
  pointer survives the `bl` — MSVC will not keep a `this->member` load live across a call.
- `resize(mPointTests.size(), RndPointTest())` — args are evaluated right-to-left; the temporary's ctor
  runs before `size()`.
- Loop-body locals that must share the outer frame's slots must be declared at **function scope**
  (V6, −0x20 of frame and the whole slot map).
- Pass the unnamed temporary `Hmx::Matrix4(xfm)` directly and declare `RndShaderMgr &shaderMgr =
  TheShaderMgr;` **at** the `SetVConstant` site (V3 + V9 negative): naming the temporary (V5) or
  hoisting the reference (V9) each cost a register.
- Read a value back through the member it was just stored to when retail shows `clrrwi rX, rY, 0`
  after the store (store-to-load forwarding, V7).

---

## Item 2 — the four unpinned rows of `default/auto_03_8273D658_text`

### 2.1 Identification

| row | size | retail body | identity |
|---|---:|---|---|
| `fn_8273D658` | 20 | `mr r11,r4; li r5,0; li r4,0; mullw r3,r3,r11; b fn_82857368` | `MakeVertexBuffer(num,size,…)` → `D3DDevice_CreateVertexBuffer(num*size, 0, D3DPOOL_DEFAULT)` |
| `fn_8273D670` | 20 | `mr r11,r4; li r6,1; li r4,8; mullw r3,r3,r11; b fn_82857440` | `MakeIndexBuffer(num,size,fmt)` → `D3DDevice_CreateIndexBuffer(num*size, 8, fmt, D3DPOOL_MANAGED)` |
| `fn_8273D688` | 156 | null-check; `bl fn_82856FC0` (GetDesc → `desc`@0x70); `bl fn_82857368` (Create(desc.Size, Usage, Pool)); `bl fn_82737548` ×2 (`&lock`@0x60/`in`, `&lock`@0x50/`out`); `bl memcpy`; `bl fn_82857430` ×2 (Unlock) | `CloneVertexBuffer(D3DVertexBuffer*)` |
| `fn_8273D728` | 160 | same with `fn_82856FF0` / `fn_82857440` (+`desc.Format` in r5) / `fn_827375E8` / `fn_828574F0` | `CloneIndexBuffer(D3DIndexBuffer*)` |

Callees (none named in `target_symbol_map.json` except `memcpy` at `0x8282a900`; all placeholders are
forgiven by `name_check`): `fn_82856FC0` = `D3DVertexBuffer_GetDesc`, `fn_82856FF0` =
`D3DIndexBuffer_GetDesc`, `fn_82857368` = `D3DDevice_CreateVertexBuffer`, `fn_82857440` =
`D3DDevice_CreateIndexBuffer`, `fn_82857430` = `D3DVertexBuffer_Unlock`, `fn_828574F0` =
`D3DIndexBuffer_Unlock`. `fn_82737548` / `fn_827375E8` (both 0xA0 B, `__savegprlr_29`, in
`RenderState.s`) store vtables `lbl_82101A2C`/`lbl_82101A34`, call `D3DResource_IsSet` on
`TheDxRnd.Device()`'s `0x1c4`, unbind stream sources 0/1 if set, then `Lock` and store the returned
pointer at `+8` — they are the out-of-line `BufLock<D3DVertexBuffer>::BufLock` /
`BufLock<D3DIndexBuffer>::BufLock` COMDATs (retail placed them with `RenderState.obj`; DC3's map
places its copies with `Mesh.obj`).

TU decision: **a separate TU, `system/rnddx9/Utl.cpp`, not `Rnd_Xbox.cpp`.** DC3's leaked map
(`ham_xbox_r.map:51470-51486`) lists exactly these four, consecutive, as `rnddx9:Utl.obj`; DC3's
`objects.json` has `system/rnddx9/Utl.cpp` as `Matching`; retail places the four in the exact gap
between `Rnd_Xbox.cpp`'s last `.text` block end (`0x8273D658`) and `CubeTex.cpp` (`0x8273D7D0`),
summing byte-exactly to `0x8273D7C8`, where an 8-byte EH prefix (`82 82 95 30` → `0x82829530`)
precedes CubeTex and is excluded from the pin.

### 2.2 Witness

Our tree already had `src/system/rnddx9/Utl.cpp` (byte-identical to DC3's modulo include slashes)
but it was in nobody's `objects.json` and `#include`d by no TU — so no COFF in the tree had a COMDAT
of this shape (our `RenderState.obj` defines no `BufLock`/`VBLock`/`IBLock` symbol either). After
wiring, `build/45410914/src/system/rnddx9/Utl.obj` defines
`?MakeVertexBuffer@@YAPAUD3DVertexBuffer@@HII_N@Z`,
`?MakeIndexBuffer@@YAPAUD3DIndexBuffer@@HIW4_D3DFORMAT@@@Z`,
`?CloneVertexBuffer@@YAPAUD3DVertexBuffer@@PAU1@@Z`,
`?CloneIndexBuffer@@YAPAUD3DIndexBuffer@@PAU1@@Z` and the two out-of-line
`??0?$BufLock@UD3D{Vertex,Index}Buffer@@@@QAA@…` ctors (same compiler, same `/O1` decision as retail).

### 2.3 Wiring and measured Δ (`3bad5ca5`)

- `config/45410914/objects.json`: `"system/rnddx9/Utl.cpp": "NonMatching"`.
- `config/45410914/splits.txt`: heading **`system/rnddx9/Utl.cpp:`** (full path — `system/rndobj/Utl.cpp:`
  and `system/obj/Utl.cpp:` already exist) with `.text start:0x8273D658 end:0x8273D7C8`; dtk back-filled
  `.pdata start:0x8223AB48 end:0x8223AB58` (two records — the 20 B leaf thunks touch neither stack nor
  LR and get none). The first build after the edit stopped at `verify_split_current.py --complete`
  because the split rewrote `splits.txt`; the retry was the fixed point.
- `scripts/target_symbol_map.json`: the four names added (round-trip asserted identical first).
- `src/system/rnddx9/Utl.cpp`: the four `DX_ASSERT`s removed (see below).

| step | predicted | measured | set-diff |
|---|---|---|---|
| wiring only (build 12) | Δ0 reattribution; +4/+356 if the bodies match | **Δ0** (43,301 / 3,988,808); rows at fuzzy 0.0 / 0.0 / 44.87 / 46.25 | empty — naming the four addresses charged **no caller row** |
| `DX_ASSERT` removed (build 13) | +4 / +356 if the r30↔r31 swap is a consequence of the macro | **+4 fns / +356 B** (43,305 / 3,989,164 / 38.934032) | CROSSED IN exactly `?CloneIndexBuffer…` 160, `?CloneVertexBuffer…` 156, `?MakeVertexBuffer…` 20, `?MakeIndexBuffer…` 20; FELL OUT none |

The one construct: DC3 (newer) added `DX_ASSERT` after each of the four allocations
(`DxRnd::Error` + `MakeString` + `Debug::Fail`, ~30 instructions and a frame for the 20 B thunks).
RB3 retail has none — the `Make*` rows are bare `b D3DDevice_Create*Buffer` tail calls and the `Clone*`
rows go straight from `Create` into the `BufLock` ctors. `DX_ASSERT` itself is right for RB3 (12 uses in
`Rnd_Xbox.cpp` match retail at 100), so the removal is per-site with a comment carrying the retail
addresses, not a macro change. The r30↔r31 swap in `Clone*` dissolved with the macro — a symptom, as the
standing rule says.

---

## Gates (run last, in the worktree, after the last source edit)

See the lane's final report for the verbatim `NATIVE_GATE_RESULT` line; the chain was full build rc=0 →
`verify_ruler_agreement.py --check` → `verify_objs_patched.py --verify-manifest` → `native_build_gate.sh`.

## NOT done, and why

- The r22↔r23 swap in `DoPointTests` (§1.3): two probes regressed; no further construct identified.
  The `mtx.cpp` TU-include hypothesis (whether `Rnd_Xbox.cpp` including `math/mtx.cpp` changes
  `Matrix4` inlining) was not tested — it would cascade to the other rnddx9 units.
- The `_M_fill_insert<MidiParser::Note>` alias at idx 97 was deliberately not re-added.
- `0x82737548` / `0x827375E8` were **not** map-named as the `BufLock<>` ctors. They are identified
  (§2.1) but naming them is a bet paying in bug exposure, not bytes, and the callers are in
  `RenderState`/`Mesh`, outside this lane; recorded here for a map lane.
- The 72 B `resize<RndPointTest>` row and the unnamed 836 B `fn_8273B818` row in `default/Rnd_Xbox`
  were not pursued.
- The two remaining sub-100 rows in `default/Rnd_Xbox` beyond `DoPointTests` were not opened.
- `#pragma once` at the top of `Utl.cpp` (inherited from DC3) was left in place; it compiled without a
  diagnostic.
