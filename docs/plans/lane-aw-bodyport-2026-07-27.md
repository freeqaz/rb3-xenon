# laneAW — body-port / source-divergence wave (2026-07-27)

**Merge-base for the verify:** main `2551442d` = **39,266** strict, reproduced
clean in `~/tmp/wt-laneAW-land` before any edit.

> ## **+88 strict, 0 losses — 39,266 → 39,355**
> (+86 from nine branches at `2551442d`, then +2 from the deferred
> `laneAW-stl` fragment landed separately at `e5a7196c` — see §8.4.)
>
> ### The nine-branch leg: **+86 strict, 0 losses — 39,266 → 39,352**
> `unit+name` **+86 / −0** · **name-only (unit-agnostic) +86 / −0** · map 25,664
> entries, **0 duplicate VAs** on the raw-line check · `_bijection_arbitrary`
> 1119 / `_icf_arbitrary` 25 / `_denylist` 3 intact · plus **4 VAs converted
> 0.00 % → scored**.
>
> Independently corroborated: the same ten fragments composed at the *earlier*
> base `622899b5` (39,143) measured **+88 / −0** with a full **`OBJCACHE=off`**
> build on **both** legs. Two different bases, ~120 matches of main drift
> apart, agreeing to within the two rows main closed in between — that is the
> cross-check that the composition is independent and the rebase faithful.

Landed on main as nine `--no-ff` merges, `01659cfa`..`d6233b5f`.

| branch | content | measured (own base) |
|---|---|--:|
| `laneAW-cheap` | BandProfile `unk748`, ContentMgr DC3-virtual, PatchDir map ODR, FxSend save revs | **+24** |
| `laneAW-mesh` | one `virtual` keyword in `Mesh.h` | **+11** |
| `laneAW-oneinstr` | sret-temporary → named local, ×7 of 11 | **+11** |
| `laneAW-srcmissing` | 6 absent bodies + 2 map-name repairs | **+9** |
| `laneAW-sweep` | `cmpwi`→`cmplwi` signedness cast ×6 | **+9** |
| `laneAW-char` | CharBones accessors, `RefPtrOf` ring-ref, Track hoisted local | **+8** |
| `laneAW-hamcam` | STL stride name-pool repoints + one under-covering pin | **+6** |
| `laneAW-stl` | 4 closed STL transpositions (map-only) | +4 claimed → **+2 net-new** (§8.4) |
| `laneAW-unitsb` | `Character.h` two compensating vtable errors | **+2** |
| `laneAW-unitsc` | named-local + early-return, −2 guard-bit collateral | **+2** |

Ten Opus workers, ten isolated worktrees. **Every worker refuted at least one
claim, four of them their own.** Three refutations below are worth more than the
matches that accompanied them.

---

## 1. ★★ Two constants in the shared model were WRONG, and one of them is mine

### 1.1 An objdiff `replace` costs **60**, not 5 — and not 100

The lane brief propagated *"`S=1` = one differing immediate, `S=5` = one
differing opcode, `S=100` = one inserted/deleted instruction"* into ten worker
briefs. **`S=5` is wrong.** Two workers converged on the correct value
independently, from unrelated units:

```
diff_arg  = 1      one differing immediate / operand
replace   = 60     one wholly different instruction
insert/delete = 100
```

This is not pedantry — it inverted a real diagnosis. The four `Track` accessors
read **S=60 over N=17**, which the brief's table made look like *~12 differing
opcodes*, i.e. deep body divergence. I flagged that arithmetic to the `char`
worker as a reason to doubt laneAV's "one shared `cmplwi`" record. **laneAV was
right and my doubt was wrong**: S=60 is *exactly one* replaced instruction, and
all four flipped from a single hoisted local.

> **Correct the standing inversion table wherever it is recorded.** Any lane that
> priced an `S=60` row as a multi-instruction body-port has mis-routed it.

### 1.2 `S=1` is a LAYOUT tier — the brief said so, and it is still a trap

The brief's "S=1 is a layout tier wearing an immediate mask" held up, but the
`cheap` worker sharpened it into something more useful: **a whole class-size
delta encodes as exactly one immediate.** The five `MetaPanel::NewObject` rows
are correctly paired and read S=1 while encoding `AppLabel` base **0x290 vs
0x98**; `MeterDisplay`'s adjustor is 0x178 vs 0x84. Pairing was verified correct
in 37 of 42 cases before concluding this.

> **"Cheapest by S" is not "cheapest by work."** An S=1 row is one instruction
> from matching and possibly an entire class re-layout from *being right*.

## 2. ★★ The STL element-sizeof family: 0 of 100 are struct defects

The largest single cluster in the pool (~100 named rows at S≤6:
`__uninitialized_fill_n` 19, `_M_insert_overflow_aux` 16, `__uninitialized_copy`
13, `_M_fill_insert` 12, `resize` 9, …) looked like the recorded *struct-stride
vein*. It is not. Measured by three workers independently, in different units:

| class | count |
|---|--:|
| **(A) genuine element-struct size defect** | **0 of 100** |
| **(B) rotated name pool / ICF placement** | **100 of 100** |
| (C) `StlAlloc.h` named-local lever | 0 remaining — already landed |

Evidence, three unrelated lines:
* **92/100** have a reloc-masked callee whose template element type names a
  *different class* than the base at the same call slot.
* **42/100** are instantiated on element classes that **do not exist in RB3 at
  all** — DC3-only (`NavItem`, `MoveRating`, `DetectFrame`, `FilterOutputFrame`,
  `TransformArea`, `PracticeStep`, `SongPattern`, `FlowMathOp`, …). Their names
  are phantoms on unrelated RB3 VAs: no struct fix can be right and no repoint
  exists.
* The whole population is **pure `diff_arg`** — zero opcode/insert/delete.

The decisive single counter-example: `vector<RndBone>::_M_allocate_and_copy` in
`default/Mesh` — our `mulli 0x4c` (sizeof `RndBone` = 76, **correct**) vs target
`mulli 0xc`, and the target's own next-slot callee is
`__uninitialized_copy<FilePath>`. That VA is the **FilePath** instantiation.
A third worker reproduced it in `MeshAnim`: `vector<Key<vector<Vector3>>>::resize`
target `li r8, 0x50` vs our 16 — a **5× gap**, and `Key<T> = {T value; float frame}`
over a 12-byte `vector` makes 16 provably right; 0x50 is `Key<Transform>`.

> ★ **The non-rotatable oracle:** `li r9/r10, N` in `_M_fill_insert`/`resize`
> carries `sizeof(T)` **literally** and cannot rotate with the name assignment.
> Use it to pin the true element size *before* building. It produced 6 wins in
> `HamCamTransform` from a ~40-line build-free scan.
>
> ★ **Do not "fix" a sizeof in this family.** The recorded
> `ObjVector<DynamicPropertyEntry>::resize` incident (+5/−3, reverted) was not
> bad luck; it is the generic outcome.

### 2.1 ★ The documented ICF trap-check does not work

The standing tell — *"objdiff's Function Call Diff shows a target-only callee
naming a different class"* — **silently passes on genuinely mispaired rows.**
`call_diff.target_only` came back **empty** on the `RndBone`/`FilePath` case that
is provably mispaired: under `functionRelocDiffs=none` the diverging `bl` scores
`equal`, so it never reaches `call_diff`. **The tell only appears in the full
instruction listing.** Anyone applying the check as written will clear rows they
should not.

## 3. ★★ Foreign-class symbols in a unit are usually LEGITIMATE — grep first

`default/HamCamTransform` holds `EyeDesc@CharEyes`, `NavItem@HamNavProvider`,
`WorldDir`, `FlowNode` symbols. This was read as ICF, then as an over-covering
pin, then (by another worker) as "a DC3-only source file pinned onto an RB3 span
that never held that TU". **All three were wrong.**
`src/system/hamobj/HamCamTransform.cpp` lines 215-265 deliberately
`#include`s **nine other owner `.cpp` files**, each wrapped in
`#define gRev gRev_<Owner>` — the project's own COMDAT-scatter-wiring lever,
applied on purpose.

> **Rule: before diagnosing foreign-class symbols in any unit, grep that unit's
> `.cpp` for `#include "….cpp"`.** Main independently reached the same rule at
> `73155b85` while this lane was running.

It also pays forward: the `srcmissing` worker found `rnddx9/Rnd_Xbox.cpp:985`
**already** includes `rnddx9/Mesh.cpp`, and that file is *not* in
`objects.json` — so adding the two DC3 bodies at `dc3-decomp/src/system/rnddx9/
Mesh.cpp:261` and `:307` lands `DxMesh::Fill` + `CheckFurTransformCache` in the
right unit with **no splits work at all**. Two strict, ready to execute.

## 4. Shared causes that cleared several functions each

| lever | fns | note |
|---|--:|---|
| ★ **sret temporary → named local** | **7** | Target: `bl <sret fn>` then `lwz r4,<fixed>(r1)`. Ours: `mr r11,r3` … `lwz r4,0x0(r11)` **plus a spare `mr r3,<this>`** — one inserted instruction, S=100. Cause: a by-value-returned class (`Symbol`, `String`, `FilePath`) passed into another call as an unnamed temporary. `Tour.cpp` already carried a comment describing it at a different call site — known and unapplied. |
| **`BandProfile::unk748`** | **11** | A member present in rb3-Wii that our header dropped; an old `unk6f78pad` tail hack was compensating for the same 4 bytes *in the wrong place*. |
| ★ **one `virtual` keyword** | **11** | See §5 — the whole `RndMesh` cluster. |
| **`cmpwi` → `cmplwi` via an `(int)` cast** | **6** | One-token cast on the tested pointer. **One-directional**: the reverse direction refused all 6 attempts. 12 rows carried the signature. |
| **Track: one hoisted `Player *` local** | **5** | §5.2. |
| **`CharBones` inline accessors** | — | Retail addresses the bone buffer via `&mBones`; DC3 replaced rb3-Wii's inline `Start()/ScaleOffset()/QuatOffset()` with raw `mOffsets[]`. Porting them collapsed **90 `diff_arg` → 10** in one edit — the r27↔r30 rotation was a *symptom*. |
| **`RefPtrOf(it)->RefOwner()`** | — | DC3's `(*it).RefOwner()` devirtualises to `li r3,0`. One statement dissolved a **19-instruction r29↔r30 cascade** (94.4 → 99.8). |
| **ContentMgr `IsCorrupt` is a DC3-inserted virtual** | 2 | Same pattern as the `CONTENTMGR_DC3_VIRTUAL` hack already in that file. |
| **FxSend save revisions** | 2 | RB3's are one lower than DC3's (3 not 4, 6 not 7). |
| **`Drawable` has no class `operator new` in retail** | 2 | Deleting 4 lines. The **mirror** of laneAT's `OBJ_MEM_OVERLOAD` inline-new lever; `RndBitmap::Create` is the same shape in the other direction — hand it to them. |

## 5. Two keystones worth their own entry

### 5.1 `RndMesh` — the brief's own diagnosis was wrong *in kind*

The brief handed the `mesh` worker `[36] diff_arg: lwz [off:+8]` on
`SetNumFaces` as *"a member-offset delta of +8 in `RndMesh`"*, with five
same-percentage siblings. **It is a vtable-slot index, not a member offset** —
two instructions earlier is `lwz r11, 0x0, r31`, which loads the **vptr**. Acting
on the member hypothesis would have been actively wrong on a widely-used engine
base type. `RndMesh`'s member layout is in fact provably *correct*
(`lwz r10, 0x110(r3)` / `addi r3, r10, 0xe4` matches the compiler-reported
offsets exactly). The fix was **one `virtual` keyword**, worth **+11**.

> ★ **Generalisable:** an `[off:+N]` annotation on a `lwz` whose base register was
> just loaded from `this+0` is a **vtable-slot** delta. The fix is a `virtual`
> keyword, not a struct member. Worth re-checking across every S=1 row that was
> routed to layout work.

The worker also killed two of its own candidates with content evidence
(`RndMesh::Save`, already at 100 %, vcalls slot `0x34` on **both** sides; target
`AmbientOcclusion` inlines `geomOwner->mVerts.mNumVerts` with **no vcall**).

### 5.2 The `Track` accessors — laneAV's record corrected, and the wall was not one

laneAV recorded the four `Track` accessors as blocked on `cmplwi` vs `cmpwi`,
with *"a hoisted `Player *player` local — tried and reverted"*. **The hoisted
local is the fix.** The decisive control was in the same file:
`Track::PlayerDisconnectedAtStart` is the only one of the five written with a
hoisted local and was **already at 100 % with `cmplwi`**. Calling
`user->GetPlayer()` twice is what produces the signed compare. All four flipped,
plus `RefreshPlayerHUD` 65.4 → 100 by deleting a `static bool sDump` MILO_LOG
block absent from retail.

## 6. ★ The "one inserted/deleted instruction" tier is mis-named — census

The `oneinstr` worker scanned **110 of 123** rows with full instruction listings
(`/home/free/tmp/laneAW/oneinstr/scan3.json`, regenerable via `scan3.py`):

| share | shape |
|--:|---|
| **40.9 %** | one misplaced **data move** (`mr`/`stw`/`lwz`/`addi`/`li`) — sret temp, EH-frame spill, arg setup |
| 28.2 % | regalloc/scheduling, **no insert/delete at all** |
| 11.8 % | pure `replace` — two adjacent instructions swapped |
| **8.2 %** | redundant narrowing mask (`clrlwi`/`extsb`/`extsh`) |

Inserted/deleted opcode histogram: `mr` 15, `stw` 15, `addi` 12, `li` 9,
`clrlwi` 7, `stb` 6, `lwz` 5. **Exactly one row of 110 involves a `bl`.**

> Only 55 % of the tier contains an insert/delete at all. "One or two inserted
> instructions" does **not** mean "one missing statement" — it almost always
> means a misplaced *data move*. The ~40 % with no insert/delete is
> permuter-class, and the permuter is banned by user directive: **dead budget.**
> Route this band to the **sret** and **EH-spill** shapes only.

## 7. Refutations — including four of our own

* ★ **Ours (mine):** *"S=60 over 17 instructions is ~12 differing opcodes, so
  laneAV's `Track` description must be incomplete."* Wrong — a `replace` costs
  60. laneAV's diagnosis was right; only their *conclusion* was wrong (§1.1).
* ★ **Ours (mine):** *"`default/UIStats` residual 153 does not reproduce."* I
  looked at the wrong unit. The charter means `default/band3/meta_band/UIStats`,
  which does reproduce (202 fns / 47 strict / **155 residual**). **The substance
  survived and got stronger:** 197 of those 202 are anonymous `fn_`, with exactly
  2 named non-strict rows, both 12-byte junk. `RockCentral` is the same shape —
  990 fns, 752 strict, **893 anonymous**, only 20 named-and-scored. **Those units
  are an identity problem with essentially no body-port surface**; the charter's
  "top residual pinned units" framing points at the wrong lane.
* ★ **Ours:** *"the 46 source-missing methods are a 46-row source worklist."*
  Re-derived: **14 actionable / 10 in `auto_03` carves / 22 in unrelated units**,
  and **all 45 present rows read exactly 0.00 %**, not "scored" as laneAV wrote.
* ★ **laneAV's caveat that the private `AAA`/`IAA` rows are probably `/Ob2`
  inline-policy differences — refuted by measurement:** of the 14, **2 were map
  *name* defects** (a body we already ship, mangled for a signature RB3 doesn't
  have), **12 were genuinely-absent bodies**, and **0 were inline-policy**. So
  "retail has a body we don't" means *missing source*.
* ★ **A third mechanism neither list had: DC3-name mangling defects.** 13 of the
  14 names are verbatim `ham_xbox_r.map` transfers — hypotheses, not facts. Two
  had RB3-wrong signatures, so a body we *already ship* could never pair
  (`?GetProfileFromPad@ProfileMgr@@Q**B**A…@AVHamProfile@@` — RB3 has no
  `HamProfile` and the method is not const; the retail body is
  instruction-for-instruction our existing one). **One `map_line_splice.py` line
  → 100 %.** Invisible to any "does our obj define this symbol" test; worth a
  map-wide scan.
* **`BinStream::Read`/`Write` share a cause and it is NOT source-fixable.** One
  extra `clrlwi rX,rX,24` before an `stb` that already truncates. Five variants
  measured and reverted — including DC3's exact `char crypt[512]` form, which is
  **worse** (96.8 %, adds two `extsb`). A **9-row class** (also `SHA1::Final` ×2,
  `deflate/fill_window` ×2, `Band::IsEndOfCoda`, `RGGemMatcher::FretMatchImpl`,
  two vorbis functions, `GameMicManager::SetPlayback`, `MicNull` ctor). Reads as
  toolchain-level mask elision. Needs one owner or a documented KILL.
* **`Geo` is not "cheap FP math".** `OnSide`'s residual is FMA *contraction
  choice*. Twelve compiles across all 6 term orderings plus 6 split/
  parenthesisation forms: floor is score 4, **nothing reaches 100**. The
  named-object-member lever does not apply — we *want* the contraction, we need a
  *different* one. DC3's `Geo.cpp` is character-identical source and its own unit
  is only 70 % matched. Reverted entirely; treat the cluster as the fine-FP wall.
* **`Rot::MakeScale` is not FMA contraction either** — 30 `diff_arg`, zero
  `diff_op`, zero insert/delete: pure FPR-assignment permutation, permuter-class.
* **The sibling-scope overlay lever does not extend to temporaries consumed as
  call arguments.** `BandCrowdMeter::SyncObjects` went **13 → 313** with named
  sibling-`{ }` locals. Reverted. (It works for *named* objects in the same
  parent scope, as recorded — this bounds it.)
* **Ours:** the `Key.h` chained-`operator>>` fix produced **exactly zero** change
  (Morph stayed at 94.444 % with the identical `insert mr r3, r30`). A zero-gain
  edit to a wide-fan-out shared engine header is pure risk, so it was reverted
  (`cbd2bb0c`) — which is why the landed set contains **no shared-header change**.
* **Ours:** `?HasAsFriend@BandUser@@` reverted — the target reaches the `User`
  base via vbtable slot `0xc` from `this` but `0x4` from the `BandUser*`
  argument, so the receiver cannot be a `BandUser`.
* **`SongRecord`, refuted twice.** Swapping `mIsShared`/`mRestricted` flips two
  functions **and breaks two `MusicLibrary::Rebuild*SongData`** (net 0); swapping
  the two *map* entries instead scores 63 %. Both pairings and the layout are
  individually correct ⇒ retail likely has a **fourth bool** our model collapses.
* **`RndShaderMgr::UpdateCache`** — MSVC hoists all 12 `lfs` in declaration order
  then sinks exactly the one load feeding the first store; retail performs no
  such sink. Proven no permutation reaches it (decl==store order 97.4 %, no
  temporaries 67.9 %).
* **The `RefPtrOf` anti-pattern is not a force multiplier** — five other
  `(*it).RefOwner()` sites exist, and none of their enclosing functions is in the
  sub-100 named pool. **0 additional yield.** (Site count is not defect count,
  measured again.)
* `Rnd::OnToggleHeap`'s store-order swap is correct per the target but cascades
  **96.1 % → 76.9 %**. `CharIKFingers::CalculateHandDest` 99.7 → 95.8 on a
  canonical `Multiply(v,m,out)`; do not re-attempt from that angle.

## 8. Two defects found in the measurement apparatus itself

### 8.1 ★ objdiff scores a flow-analysis pseudo-argument — 4 real matches hidden

The four `Campaign::Update*MajorLevelIcon` functions were taken to **byte-perfect
at COFF level** — extracting each function's bytes from the dtk target obj and
ours and masking only relocation fields and `bl` targets gives **0 differing
instruction words in all four** — and objdiff still reports **98.7 %**. The
phantom is `addi r10,r10,0x1` inside an inlined `strcmp(x,"")` loop: identical
bytes (`394A0001`), **neither side has a relocation there**. The extra operand is
objdiff's PPC **flow-analysis pseudo-argument**
(`objdiff-core/src/arch/ppc/flow_analysis.rs`, `generate_flow_analysis_result`)
attaching the string symbol to the target's `r10` but not ours — and it is being
*scored*. A non-COMDAT `static const char[]` workaround changed nothing, and 47
same-shape functions *are* at 100, so it is conditional.

> **This is very likely the recorded "113 target symbols are reloc-masked
> byte-EQUAL to their mapped base and still score below 100 %" class.** Four real
> matches are invisible to the metric. It deserves an owner on the objdiff fork.

### 8.2 The guard-bit renumbering cost of removing a block scope

Rewriting `if (x) { … }` as `if (!x) return; …` flipped two functions and **cost
two others**. Removing the block scope renumbers MSVC's **packed static-init
guard bits** (`$Sn` words hold one bit per static; the scope index is per-TU
cumulative), changing *non-relocated* `rlwinm`/`ori` immediates in **other
functions in the same TU**. **The fix is to keep the block:**
`if (!x) return; { …statics… }`. Predicted to turn +4/−2 into +4/−0; mechanism is
solid, the fix itself is **unverified**. Same mechanism laneGUARDBIT is working.

### 8.3 ★ Two of my own workers double-counted each other — *inside one lane*

`laneAW-stl` measured **+4** from four closed STL transpositions. `laneAW-hamcam`
measured **+6** including repoints of the *same* `EyeDesc ↔ NavItem` pair, derived
from completely different evidence (stl: a binary-wide stride solver; hamcam: a
per-unit opcode-signature grouping). **They agreed character-for-character**,
which is a genuine cross-validation of both tools — and it also means the two
numbers cannot be added.

After hamcam landed, stl's net-new content was **2 transpositions, not 4**, and it
measured **exactly +2 / 0 losses** on its own A/B (39,353 → 39,355, `a102e8b5`).

> The recorded fleet rule *"two lanes on one evidence channel must measure the
> union, never the sum"* has now fired **inside a single lane, between two workers
> I dispatched myself**. Sum of the ten workers' own claims is **+90**; the
> measured union is **+88**. Partition workers by *evidence channel*, not just by
> unit — I partitioned by unit and the STL family cut across it.

### 8.4 Tooling cautions earned the hard way

* **`llvm-nm --defined-only` silently rejects some of these objs**
  (`ShaderMgr.obj`: "not recognized as a valid object file"), so an nm-based
  "does our obj define this symbol" scan **manufactures false absent rows**. Use
  a COFF symbol-table reader.
* **`objdiff-cli` needs `--build --incremental` or it silently measures stale
  objs.**
* ⚠ **Never `pkill -f ninja-locked` on this box** — the pattern is not
  worktree-scoped and matches other lanes' builds. Kill by PID from
  `fuser <worktree>/.ninja-build.lock`. (One worker did this and self-disclosed;
  six other lanes' builds were verified alive afterward, so blast radius appears
  to have been zero.)
* **`OBJCACHE=off` suppresses new serves but does not invalidate objects already
  on disk** — it is not equivalent to `rm -rf build/…/src` plus a cold compile.
  Only a *delta* between two identically-built legs is sound.

## 9. Measurement validity

The upstream **objcache shared-PCH-sidecar bug** (`store.rs:180` hashes the raw
root-relative `/Fp` token, so all worktrees share one sidecar, last-writer-wins)
landed mid-lane, with **ten laneAW worktrees compiling concurrently** — the exact
exposure case. Mitigations actually taken:

* **No worker edited `src/system/obj/Object.h` or `src/system/os/Debug.h`.** The
  landed set contains **no PCH-header change and no shared-header change at all.**
* The **composed verify's both legs were full `OBJCACHE=off` builds** at
  `622899b5`, and the baseline leg reproduced main's published **39,143** exactly.
* Workers who could not afford a cache-off whole-binary leg on the saturated box
  substituted the decisive local check — delete the gain-bearing `.obj`,
  recompile it with `OBJCACHE=off`, re-diff. `sweep` ran a genuinely cold
  `rm -rf build/45410914/{src,pch}` before-leg and got **exactly** its warm
  number, i.e. that worktree was demonstrably not poisoned.
* ★ **The warning was justified and priced: exactly 1.** `srcmissing` read 38,377
  cached and **38,376** cache-clean; the entire discrepancy is one row —
  `fn_827284DC`, a 40-byte anonymous funclet in a unit its diff never touched,
  already excluded as unattributable. Cache-mode-dependent noise, now confirmed
  rather than suspected.

**Operational note:** the box ran at load **240–410 on 32 cores** for most of the
lane, with ~824 concurrent `cl.exe` nearly all in uninterruptible disk sleep —
**CPU 83 % idle while everything blocked on I/O**. A single `RockCentral.obj` took
27 minutes; one worker measured 115 of 976 objects in 90 minutes. Ten concurrent
lanes are net-negative for throughput. **Cap lane concurrency**, or at minimum cap
per-worktree `ninja -j`.

## 10. Named residue

**Ready to execute, no splits work:**
1. `DxMesh::Fill` + `CheckFurTransformCache` — §3, two strict, bodies usable
   verbatim from DC3.
2. `src/system/rnddx9/Tex.cpp` is in **neither** `objects.json` **nor**
   `splits.txt`, while its five `DxTex` functions (`0x82734148`–`0x827350e8`) sit
   inside `ShaderMgr.cpp`'s pinned span. Wire + split-MOVE, or a scatter-wiring
   `#include`. Five rows.
3. The two `CharSleeve::Poll` / `CharForeTwist::Poll` edits, written out verbatim
   in `/home/free/tmp/laneAW-char/PENDING_EDITS.md`, dropped unverified on the
   converge order. `CharSleeve` is a one-line high-confidence fix
   (`mMe.mObject@0x64`, rb3-Wii agrees); `CharForeTwist` is DC3's
   `if (IsNaN(finalfloat)) return;` which rb3-Wii lacks — `#ifdef HX_NATIVE` it.
4. `Player::PopupHelp` — retail names the temp; 1-line sret-lever flip.
5. `PatchDir::UnloadStickerTex` is now **S=3** — only a +4 `mStickersLoading`
   layout delta remains.

**Diagnosed leads worth a lane each:**
* ★ **A systematic sweep for DC3-inserted virtuals.** Three independent hits this
  lane (`ContentMgr::IsCorrupt`, `Character::CollideListSubParts`,
  `BandCharacter::Teleport` at vtable slot 0x2c vs our 0x30). The `Character`
  case is instructive: removing the bogus virtual **alone measures −7** because it
  was compensating for a *second*, real error (`DrawLodOrShadow` non-virtual in
  `Character`, re-virtualised by `BandCharacter`, landing at the vtable tail).
  **Two compensating errors — either alone regresses.**
* ★ **`??1?$ObjRefConcrete@V<T>@@VObjectDir@@@@UAA@XZ`: 86 binary-wide, 72 at
  100 %, 13 stuck at exactly 97.931 %**, all one instruction —
  `lwz r4,0x4(r3)` (pass `mOwner`) vs `mr r4,r3` (pass `this`). One template
  cannot emit two 116-byte bodies under one mangled name, so this is a
  target-side attribution defect, most likely `~ObjOwnerPtr<T>` COMDATs
  mislabelled onto `ObjRefConcrete`. Honest caveat: only 7 of the 13 have a
  direct `ObjOwnerPtr<T>` in-tree. **Decisive cheap probe:** flip
  `~ObjRefConcrete` in `src/system/obj/ObjPtr_p.h:97` to release with `mOwner` —
  13 flip ⇒ one template; ~59 break ⇒ two templates plus a 13-line map repoint.
  *(Two workers found this independently.)*
* **`TransformArea`'s interior is wrong although its `sizeof` (0x70) is right** —
  the retail destructor frees sub-objects at `0x0,0xc,0x18,0x24,0x30,0x3c,0x4c,
  0x5c` plus a bool at `0x6c`; our model is five members plus a `char _pad[0x20]`
  fudge. Keystone for two 14 % functions plus `PropSync`/`Load@TransformCrowd`.
* **The redundant-narrowing-mask class** (9 rows, §7) and the **EH-frame-spill
  class** (`TrackDir::~TrackDir`, `FaderGroup::~FaderGroup` — we emit the spill
  twice, target once — `StreamReceiver360::~StreamReceiver360` ×2, `MetaMusic::
  Load`, both `GetPlayerContributionString`). Neither is source-controllable by
  anything tried.
* **`TexMovie::SetFile` + `OnPlayMovie`**: retail's `DoBeginMovieFromFile` takes
  **2** parameters, not 3. Real signature defect; renaming the callee risks its
  own pairing.
* **A jeff carve bug blocks one row for free:** `_Destroy_Range<CharInterestState>`
  has a perfect 0x10-stride twin at `0x82384FD8`, but dtk swallowed it inside
  `.fn fn_82384EE8` (`CharEyes::Enter`), so there is no COMDAT symbol to name.
* **`laneAW-stl` landed as +2 net-new** (`a102e8b5`) — see §8.3. Its two remaining transpositions (Morph/HamMove `__uninitialized_fill_n`, EventTrigger `_M_create_node`) are in; the other two were already on main via `laneAW-hamcam`. The branch also carries `scripts/harvest/stl_stride_{scan,assign,displace}.py`, still uncommitted to main because its worktree was mid-re-verify at the cut.
  was mid-`OBJCACHE=off` re-verify at the landing cut and `land.sh` correctly
  deferred it as dirty. Four closed STL transpositions; re-run `land.sh
  laneAW-stl` once the worktree is clean.

## 11. Reproducing

```bash
scripts/setup_worktree.sh ~/tmp/wt-X laneX && cd ~/tmp/wt-X
git checkout -- config/45410914/symbols.txt \
  && touch config/45410914/config.yml \
  && rm -f build/45410914/{report.cache,target_symbol_renames.stamp} \
  && OBJCACHE=off ./tools/ninja-locked
python3 /home/free/tmp/laneAS/strictdiff.py snap build/45410914/report.json before.json
# ... merge laneAW-* branches ...
python3 /home/free/tmp/laneAS/strictdiff.py diff before.json after.json
```

Artifacts: `/home/free/tmp/laneAW/` (per-worker target lists + the `oneinstr`
shape census `oneinstr/scan3.json` + `scan3.py`), `/home/free/tmp/laneAW_pool.json`
(the 1,567-row named ≥64 B pool with `S` decomposed),
`/home/free/tmp/laneAW-char/PENDING_EDITS.md`,
`scripts/harvest/stl_stride_{scan,assign,displace}.py` (on `laneAW-stl`).
