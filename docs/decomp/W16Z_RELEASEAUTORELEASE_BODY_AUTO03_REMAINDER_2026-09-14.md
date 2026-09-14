# W16-Z — `DxRnd::ReleaseAutoRelease`, `DxRnd::BeginDrawing`, and the `auto_03_8273CEF0` remainder

Branch `w16-z`, based on main `b4104f3d`. objdiff **4.2.9**, `tool_binary_hash
5a51cd51fe0a353f` on every leg. Ruler `functionRelocDiffs=name_check` (the
shipped/graded one) throughout; every number below is from a full
`./tools/ninja-locked` at rc=0 followed by `build/45410914/report.json`.

| | matched_functions | matched_code |
|---|---:|---:|
| lane baseline | 43,291 | 3,986,972 |
| lane final | **43,298** | **3,988,296** |
| delta | **+7** | **+1,324 B** |

`matched_code_percent` 38.91264 → 38.92556, `fuzzy_match_percent` 49.489254 →
49.51094. **0 rows fell out at any point.**

---

## Item 1 (LEAD) — `?ReleaseAutoRelease@DxRnd@@QAAXXZ`, 652 B: **fuzzy 100 / mpn 100**

### The brief's premise was wrong, and that is the struct-truth deliverable

The brief asked whether our `DxRnd` needs reshaping because retail walks a
member at `0x1c4` where DC3 walks `0x224`. **It does not.** The compiler's own
layout report (`scripts/harvest/class_layout_report.py DxRnd`, authoritative —
header `// 0xHEX` comments are measurably wrong elsewhere in this tree) says our
`DxRnd` is *already* retail-shaped:

| member | our offset | retail uses |
|---|---|---|
| `mD3DDevice` | `0x1c4` | `0x1c4` ✅ |
| `mPendingReleases` | `0x2a4` | `0x2a4` ✅ |
| `mPendingDeletes` | `0x2b0` | `0x2b0` ✅ |
| `mClearColor` | `0x2c` | `0x2c` ✅ |
| `mSuspended` | `0x398` | `0x398` ✅ |
| `mRegAlloc` | `0x39c` | `0x39c` ✅ |

**The `0x1c4` vs `0x224` gap is DC3's tree vs retail, not ours vs retail.** DC3
is newer and its `DxRnd` grew members ahead of `mD3DDevice`. No layout edit was
made, and none was needed — which is the correct outcome under the brief's own
rule ("do NOT shift the layout on a single-row argument").

### Retail-byte reading

652 B at `0x8273CC08` plus two 40 B EH funclets (`fn_8273CE94`, `fn_8273CEBC`).
The body is: clear the vertex/pixel shader and index buffer; a 16-iteration
sampler loop and a 4-iteration stream loop, each passing a D3DTAG "pending mask"
built with a *runtime* index — retail hoists `1<<63` out of both loops
(`li r11,1; rldicr r28,r11,63,63`) and emits one `srd` per iteration, with the
stream loop's index arithmetic `subfic r11,r30,0x5f; mulli r11,r11,0x5556;
srwi r11,r11,16; addi r11,r11,0x20`. Then two vector walks (`0x2a4`, `0x2b0`)
retiring queued releases/deletes, each guarded by `D3DResource_IsSet(elem,
device)` so a still-bound resource is carried into the next frame.

### The one retail-driven correction to DC3's body

DC3 frees the texture page with the 4-argument
`PhysicalFreeTracked(p, __FILE__, 0x452, "")`. Retail's call site sets **only
r3** before the `bl` (r4–r6 are volatile across the `XGGetTextureLayout`
immediately above), so RB3 calls the 1-argument `PhysicalFree`.

⚠ **I first got the reasoning for this wrong and corrected myself, which is
worth recording:** I initially argued the *map name* was wrong because the call
site sets only r3. That inference is invalid — **a callee ignoring trailing
arguments proves nothing about call-site arity.** Dumping the callee body
settled it: it makes three calls including `MemTrackFree`, matching the
4-argument variant. The real explanation is **ICF**: the tracked overload
ignores p2/p3/p4, so both overloads compile to the same body and fold.

That fold could not be *proven* at first, because our `PhysicalFree` was 76 B /
two calls while retail's is 84 B / three — **different-size COMDATs cannot
fold**. That size gap was a **real source defect in our tree**, not an alias
problem: `src/Memory_Xbox.cpp` was missing `MemTrackFree(address)`. Adding it
made the sizes agree and the fold provable.

### The five relocation-name charges, each adjudicated on retail bytes

All 163 instructions were byte-equal after the first build; the only charges
were 5 `diff_arg` relocation names, **all predicted by name before the build**.
Each was adjudicated with `tools/icf_pair_adjudicate.py`, **controls run first**
(`--selftest` and `--chasetest` both discriminate — positive PROVEN, negative
and in-family decoy REFUTED; a gate that cannot fail is worthless):

| pair | verdict |
|---|---|
| `erase<D3DResource*>`, `erase<D3DBaseTexture*>` | FLAT T1 **PROVEN** (84==84, 3 relocs, empty tally) |
| `push_back<D3DResource*>`, `push_back<D3DBaseTexture*>` | flat T1 REFUTED ("relocation TARGETS disagree") → **CHASED T1 PROVEN** via `_M_insert_overflow` → `MemOrPoolAlloc/...STL` → `PoolAlloc` 2-arg/5-arg → `operator new` |
| `PhysicalFree` | REFUTED on size (84 vs 76) = **a real source defect**; after the `MemTrackFree` fix, FLAT T1 **PROVEN** |

Honesty audit `tools/icf_alias_check.py --tu Rnd_Xbox.cpp` → **VERDICT: HONEST**,
rc=0. (`--worktree --base-ref main` cannot run here: `report.json` is a
gitignored build artifact, so it is unreadable at ref `main`.)

⚠ Note the standing hazard this walks next to: **an unproven alias lifts the
score by construction, and the `none` control cannot detect a fabricated one.**
That is why every one of the five is tied to retail bytes rather than to a
flat `none`-is-unmoved reading.

### Prediction vs measured

Predicted `mpn` would read 100 while `fuzzy` lagged, because CLAUDE.md says
`mpn` excludes arg-only penalties. **Measured `mpn == fuzzy == 99.84663` — the
prediction was WRONG for this row.** The doc's general claim still holds (2,757
rows sit at `mpn==100 & fuzzy<100`), but not here. Being wrong *raised* the
prize from "+1 fn, +0 B" to "+1 fn, +652 B".

---

## Item 2 — the 8-row `default/auto_03_8273CEF0_text` remainder (2,248 B)

W16-V refused to pin this on adjacency because `0x8273CEF0` opens with a bit
test on a global at `0x82E04FFC` that DC3's `BeginDrawing` does not have.

### (a) What `0x82E04FFC` is — the question W16-V left open

**It is not a render-state member.** Our `RndRenderState` is a 1-byte stub, and
only two functions in the entire emitted asm touch the address. It is the
**MSVC local-static initialisation guard bit** for
`static Timer *cpuTimer = AutoTimer::GetTimer("cpu")`, with the `Timer*` itself
at `0x82E04FF8`. Retail proves it by construction:

```
lwz r11,0x4ffc(r10) ; clrlwi. r9,r11,31 ; bne <skip>   # test bit 0
ori r11,r11,1 ; stw r11,0x4ffc(r10)                    # set bit 0
addi r3,r31,0x50 ; addi r4,lbl_820010B0 ; bl ??0Symbol@@QAA@PBD@Z
lwz r3,0(r3) ; bl ?GetTimer@AutoTimer@@SAPAVTimer@@VSymbol@@@Z
stw r3,0x4ff8(r11)                                     # store the Timer*
```

`fn_8273D07C` (32 B) is that static's **unwind funclet** (it clears bit 0).

Note the `Symbol` temporary: **RB3's `AutoTimer::GetTimer` takes a `Symbol`**
(`os/Timer.h:293` agrees), where DC3's takes a `const char*`.

### (b)(c) Positive identifications — 4 of 8 rows

| addr | size | identity | witness |
|---|---:|---|---|
| `0x8273CEF0` | 396 | `?BeginDrawing@DxRnd@@UAAXXZ` | 6 named callees + 3 compiler-verified members + 3 compiler-verified vtable slots |
| `0x8273D07C` | 32 | static-init guard unwind funclet | clears bit 0 of `0x82E04FFC` |
| `0x8273D0A0` | 1392 | `?DoPointTests@DxRnd@@AAAXXZ` | `sPointTestFence` at `lbl_82C76FFC`; `0x1a4` `mOcclusionQueryMgr`; `0x1ac/0x1b0` `mPointTestQueries`; calls mapped `?GetQueryResults@RndOcclusionQueryMgr@@QAA_NIAAI@Z` |
| `0x8273D610` | 72 | `?DoWorldEnd@DxRnd@@EAAXXZ` | calls mapped `?DoWorldEnd@Rnd@@MAAXXZ`, then `fn_8273D0A0`, then mapped `?SavePreBuffer@DxRnd@@AAAXXZ` |

Mangled names were read from the **COFF symbol table after a build**, not
guessed — a fresh worktree's reflinked objs are pre-renamer. That is how the
names came out `AAA` (not `QAA`) for `DoPointTests` and `EAA` (not `MAA`) for
`DoWorldEnd`.

### NOT pinned, deliberately

`fn_8273D658`/`fn_8273D670` (20 B each) and `fn_8273D688`/`fn_8273D728`
(156/160 B) are resource-clone helpers and `w*h` size thunks whose callees are
**all unmapped**. Nothing witnesses them to `Rnd_Xbox.cpp`. The brief forbids
adjacency pinning, so they stay in `auto_*` (re-carved as
`auto_03_8273D658_text`). A wrong unit is a re-home hazard later, and re-homing
is **not** metric-neutral (adding a pin over `auto_*` is).

### `?BeginDrawing@DxRnd@@UAAXXZ`, 396 B: **fuzzy 100 / mpn 100**

Not a straight port — **DC3 is newer**. DC3's `BeginDrawing` carries four static
`Timer` blocks with `Start`/`Stop`, `mPrintGlitches` + `MILO_LOG` glitch
reporting, `mCaptureNextFrame` + `PIXCaptureGpuFrame`, and `mGSTiming` +
`PerfCounters`. Retail's 396 bytes contain **one** guard bit, **one** `Timer*`
store and **no** `Timer::Start`/`Stop` at all, so the later additions are absent.

Vtable slots read off the compiler's report rather than guessed: `+0xe4` /
`+0x118` / `+0x120` are slots **57 / 70 / 72** = `Rnd::DrawPreClear` /
`DxRnd::Resume` / `NgRnd::ResetStats`. Float constants were read from the
**decrypted PE** (`orig/45410914/band.exe`), because the `.s` address and
file-offset columns are **synthetic** for multi-block units: `lbl_82033A50` =
`0x437F0000` = 255.0f, `lbl_82000D78` = 0.0f.

First build: **89.23232**, region 60–94 wrong, our body 20 B too long (416 vs
396). Two causes, both witnessed, both fixed:

1. **`MakeColor()` is the wrong helper here.** `Rnd.h:248` packs **alpha** as a
   fourth channel; retail loads exactly three floats (`lfs f13,0x60` /
   `f12,0x64` / `f11,0x68`), three `fmuls`, three `fctidz`. The alpha term
   appeared as a surplus `lfs f10,0x64(r31)` + `fmuls` and an FPR-swap cascade.
   Replaced with the explicit RGB expression — which is exactly what retail's
   `rlwimi r8,r11,8,16,23; clrlwi r11,r8,16; rlwimi r7,r11,8,0,23` computes.
   **`MakeColor` itself is unchanged** (`BeginTiling` at 99.96 and `ModalDraw`
   use it, and retail does pack alpha there).
2. **`SetShaderRegisterAlloc` case 1 used members**
   (`mDefaultVSRegAlloc`/`mDefaultPSRegAlloc`); retail emits `li r5,0x20;
   li r6,0x60`. DC3 promoted these to configurable members later. Witnessed two
   ways: the function has **no standalone COMDAT in retail** (absent from
   `report.json` entirely, so it is only ever seen inlined), and **nothing
   anywhere in `Rnd_Xbox.s` loads `0x3a4`/`0x3a8`/`0x3ac`**. Its only other
   caller (`gesture/StreamRenderer.cpp:655`) has no split unit, so no row
   witnesses the members at all.

Result **fuzzy 100 / mpn 100**, +396 B, 0 fallout.

### `?DoPointTests@DxRnd@@AAAXXZ` — paired at 76.86 mpn / 75.10 fuzzy, NOT closed

This is the honest hand-off item. **The struct layout is NOT the problem, and I
verified that rather than assuming it.** objdiff's "Offset Mismatches
(resolved)" table reports `target 0x1804 vs base 0x1a4
(NgRnd::mOcclusionQueryMgr)`, which reads like a layout defect. It is not — the
table resolves offsets against `this`, but retail's base register there is the
*query manager*, loaded one instruction earlier:

```
lwz r3,0x1a4(r30)      # mOcclusionQueryMgr -- identical to our source
lwz r11,0x1804(r3)     # mCurrentFrameIndex
lwz r10,0x0(r3)        # vptr
subi r11,r11,1 ; clrlwi r11,r11,31 ; stw r11,0x1804(r3)
lwz r11,0x20(r10) ; bctrl          # slot 8 = OnEndFrame
lwz r3,0x1a4(r30)
lwz r11,0x1808(r3)     # mFrameCounter
addi r11,r11,1 ; stw r11,0x1808(r3)
lwz r11,0x1c(r10) ; bctrl          # slot 7 = OnBeginFrame
```

The compiler's layout report for `DxRndOcclusionQueryMgr` (sizeof `0x2010`)
confirms **every one of those offsets is already correct in our tree**:
`0x1804 mCurrentFrameIndex`, `0x1808 mFrameCounter`, vtable slot 7
`OnBeginFrame`, slot 8 `OnEndFrame`.

⇒ **No layout edit is warranted, and I made none.** The residual is source
*structure*: 201 charged sites, prologue saves **r16–r31** (target) vs
**r19–r31** (base) and frame Δ −0x20, i.e. retail keeps three more values live
across calls than our body does. That is a real body-shape rewrite, not a
field fix, and it is left for an escalation lane.

⚠ **The durable win for this row is pairability, not bytes.** Before this lane
it was an `auto_*` row — *structurally invisible to callee adjudication and
indistinguishable from a row with nothing wrong*. It is now a paired 75% row a
later lane can actually work.

### Prediction vs measured (Item 2 pin)

Predicted, before the build: "~Δ0 overall (pinning over `auto_*` is
reattribution); `DoWorldEnd` (72 B) crosses; `DoPointTests` pairs but stays
<100; `BeginDrawing` and the funclet 0%; the 125 `Rnd_Xbox` rows at 100 must not
move."

Measured: `DoWorldEnd` crossed ✅, `DoPointTests` 76.86/75.10 ✅, `BeginDrawing`
0% ✅ (before its body was written), **`fn_8273D07C` WRONG** — predicted 0%,
measured **100 with `masked_equal == TRUE`**. It paired by **EH-funclet byte
signature**, not because we wrote a body. The 32 B is real on the ruler but is a
**disclosed** pairing; recorded as such rather than claimed as body work.

---

## Set-diff of named `fuzzy==100` rows (lane baseline → final)

```
CROSSED IN : 7 rows, 1324 B
   +    652 B  default/Rnd_Xbox::?ReleaseAutoRelease@DxRnd@@QAAXXZ
   +    396 B  default/Rnd_Xbox::?BeginDrawing@DxRnd@@UAAXXZ
   +     92 B  default/system/rnddx9/ShaderMgr::?AutoRelease@DxRnd@@QAAXPAUD3DResource@@@Z
   +     72 B  default/Rnd_Xbox::?DoWorldEnd@DxRnd@@EAAXXZ
   +     40 B  default/Rnd_Xbox::fn_8273CE94
   +     40 B  default/Rnd_Xbox::fn_8273CEBC
   +     32 B  default/Rnd_Xbox::fn_8273D07C
FELL OUT   : 0 rows, 0 B
```

The `ShaderMgr::AutoRelease` row is a **bonus ripple**: it crossed because the
`push_back` fold proven for Item 1 also forgives its call site. Every one of the
121→127 `default/Rnd_Xbox` risk rows held across all four builds.

## Commits on `w16-z`

| sha | what |
|---|---|
| `5f6fceef` | `ReleaseAutoRelease` body ported from DC3 with the `PhysicalFree` correction |
| `0ce2e929` | `Memory_Xbox.cpp` missing `MemTrackFree` + the 5 adjudicated folds in `symbol_aliases.json` |
| `70593ca9` | Item 2 pin: `splits.txt` `.text` end `0x8273CEF0`→`0x8273D658`, 3 map names |
| `5612547a` | `DxRnd::BeginDrawing` body + the two witnessed corrections |

## NOT done, with reasons

1. **`DoPointTests` not closed** (1,392 B, 75.10 fuzzy). Layout verified
   correct; the residual is a body-shape rewrite (prologue r16–r31 vs r19–r31,
   frame Δ −0x20, 201 charged sites). Left paired for an escalation lane.
2. **4 of 8 remainder rows left in `auto_*`** (`fn_8273D658`, `fn_8273D670`,
   `fn_8273D688`, `fn_8273D728`, 356 B total). All their callees are unmapped;
   no witness exists, and the brief forbids pinning on adjacency.
3. **Item 3 skipped** — conditional on W16-X having landed on main with a
   NOT-done list; it had not.
4. **No `DxRnd` layout change** — the brief anticipated one, but the compiler
   says our layout already matches retail at every offset these bodies touch.
   Changing it would have been a regression.
5. **No permuter** (standing directive: OFF).
