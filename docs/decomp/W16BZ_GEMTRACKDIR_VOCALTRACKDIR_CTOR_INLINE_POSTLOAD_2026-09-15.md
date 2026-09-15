# W16-BZ — the `ObjPtr<T>` owner-ctor **inline policy** and **store order** are one lever, and it closes the GemTrackDir / VocalTrackDir large rows

Lane W16-BZ, 2026-09-15. Worktree `~/tmp/wt-w16-bz`, branch `w16-bz`, based on main `beffe2dd8692`.
The lane ran in two sessions; the first was ended twice by a transient API capacity error, not by a wall.
Commits `7b7ff5f0` and `45cb212d` are the first session's, `29baac62` / `37df17d6` / `659de9e7` / `63268c88`
the second's.

---

## 1. Baseline verification (tested literally, not inherited)

The dispatch brief's figures were re-measured in this worktree on a full `./tools/ninja-locked` (rc=0)
before any edit, per the standing rule that a briefed figure is tested before it is built on.

| key | brief (main `beffe2dd`) | measured at lane start (`45cb212d`) |
|---|---:|---:|
| `matched_functions` | 43,679 | **43,762** |
| `matched_code` | 4,067,496 | **4,067,696** |
| `total_code` | 10,247,068 | **10,247,068** ✓ |
| `total_functions` | 69,240 | **69,240** ✓ |
| `matched_code_percent` | — | **39.696194** |
| `fuzzy_match_percent` | 49.838108 | **49.852127** |
| `masked_equal_functions` | 23,111 | **23,194** |

The `matched_functions` / `matched_code` difference from the brief is exactly the first session's two
landed commits, and reproduces `45cb212d`'s recorded MEASURES line to the digit. Ruler confirmed from
`report.json`'s `provenance` block rather than assumed: `functionRelocDiffs=name_check`,
`combineDataSections=true`, `combineTextSections=true`, `ppc.calculatePoolRelocations=false`;
objdiff binary `5a51cd51fe0a353f`, tool commit `a5f0ea903ec1`. Rowset baseline saved to
`~/tmp/rows_w16bz_base.json` (41,009 rows at `fuzzy==100`).

⚠ **One brief warning is now stale and should not be re-propagated.** The ground rules say the MCP
`run_objdiff` "has the same blind spot" as `ninja <one>.obj` (skipping the six obj patchers). On this
tree both MCP entry points route through `ensure_patched_tree()` and have dropped `--build`
(`scripts/orchestrator/mcp_server.py:1890,1908,2218,2225`; `scripts/analysis/diff_inspect.py:1792-1810`),
so they read the patched objects on disk. Every verdict in this document is nevertheless adjudicated on a
full build + `report.json`.

---

## 2. The lever, stated so the next lane can reuse it

This is one mechanism with **two independent knobs**, and the lane's whole yield came from getting both
right. Both are **TU-local `#define`s consumed by `src/system/obj/Object.h` and `obj/ObjPtr_p.h`** — no
header was edited, and the headers stayed out of scope exactly as the brief required.

### Knob 1 — *is the owner ctor inlined at all?* (`RB3_OBJPTR_INLINE_OWNER_CTOR`, + per-site spelling)

Retail inlines `ObjPtr<T>(owner)` as three stores — `mOwner` from the vbtable lookup, the vptr from a
hoisted register, `mObject = 0` — for every `T` with a **virtual** `Hmx::Object` base, and calls the
two-arg ctor **out of line** only for `T` deriving **directly** from `Hmx::Object` (RndTex, RndMat,
RndEnviron, Task, ChordShapeGenerator) and for `T = Hmx::Object` / `T = RndMat` in VocalTrackDir.
The **per-site spelling selects it**: `ObjPtr<T> x(this)` inlines, `ObjPtr<T> x(this, 0)` does not.

The out-of-line form is unmistakable in a diff — a `bl ??0?$ObjPtr@V<T>@@@@QAA@PAVObject@Hmx@@PAV<T>@@@Z`
inserted where retail has three `stw`, plus the *hoisted vtable materialisation* (`lis`/`addi` into a
long-lived register) deleted, because nothing needs it if you never inline.

### Knob 2 — *which store order?* (`RB3_TU_OBJPTR_DEFER_OWNER` vs `RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT`)

`obj/Object.h` already documents three retail orders and — this is the load-bearing part — already warns
that picking the wrong one *"leaves a row stranded in the high 80s LOOKING like a scheduler wall"*:

```
plain / DEFER_OBJECT  {mOwner, vptr-lis, mObject, vptr-addi, vptr-store}
DEFER_OWNER           {vptr-lis, mOwner, mObject, vptr-addi, vptr-store}
```

`DEFER_OBJECT` passes the owner to the **base** mem-init, so the `mOwner` store sits in the base ctor's
scheduling region and floats **above** the vptr materialisation. `DEFER_OWNER` passes nothing and stores
`mOwner` in the **derived** body, which **pins it after**. Both macros are an established per-TU knob
already used by 14 TUs.

★ **The diagnostic that identifies knob 2 is a 4-instruction signature, and it is only visible at the
FIRST materialisation of each distinct `ObjPtr<T>` vtable.** Where the vptr is already live in a
register there is no `lis`/`addi` to reorder and both orders collapse to the same stream — which is why
the wrong order can hide behind dozens of correctly-matching sites:

```
TGT: ---                  ; lis r10,vtbl@h ; stw r11,0xNNN(rX) ; addi rY,r10,vtbl@l
SRC: stw r11,0xNNN(rX)    ; lis r11,vtbl@h ; ---               ; addi rY,r11,vtbl@l
```

### ★★★ The reusable warning this lane paid for

**I classified two clusters as permuter-class scheduling and therefore unreachable, and I was wrong
about both.** On `??0GemTrackDir@@` I read cluster A (4 sites, the `mRotater` vptr/store inversion) and
cluster B (8 sites, a 16-byte block copy into the `unk6b4` `_Rb_tree`, *identical semantics, different
load/store order*) as ordering noise, wrote that "12 of the 16 sites are unreachable here", and moved on.
Both closed as **consequences of the store-order fix** — cluster B without being touched at all. On
`??0VocalTrackDir@@` I pre-registered that clusters 5–7 were a genuine member-**initialisation-order**
difference (retail zero-stores `0x528` before ascending through `0x50c..0x524`, we went strictly
ascending) and therefore beyond the macro's reach; they closed too, and the row went to 100.

⇒ **Before calling an `ObjPtr`-adjacent ordering residue permuter-class, flip knob 2 and rebuild.** It is
one line and one build. A "scheduling"/"register-swap" label here is a symptom, not a diagnosis — the
same lesson `CLAUDE.md` records for `REGISTER_SWAP`, arrived at independently.

---

## 3. Per-row results

All percentages are `report.json` on the graded `name_check` ruler, after a full build.

| row | unit | size | fuzzy before → after | mpn before → after | charged sites before → after | what it was |
|---|---|---:|---|---|---|---|
| `??0GemTrackDir@@QAA@XZ` | GemTrackDir | 2,548 B | 79.273155 → **99.968605** | 79.73627 → 99.968605 | 16 → **4** | knob 1 (27 members, `7b7ff5f0`) + knob 2 (`37df17d6`) |
| `?PreLoad@GemTrackDir@@UAAXAAVBinStream@@@Z` | GemTrackDir | 1,532 B | 91.6188 → **100.0000** ✅ | 91.8277 → 100.0 | ~24 → **0** | knob 1 (3 locals) + knob 2 |
| `??1GemTrackDir@@UAA@XZ` | GemTrackDir | 728 B | 86.46154 → **99.80769** | 86.4890 → 99.80769 | 37 → **7** | two `RELEASE()` calls retail does not make |
| `??0VocalTrackDir@@QAA@XZ` | VocalTrackDir | 3,428 B | 70.958 → **100.0000** ✅ | 71.56 → 100.0 | ~18 → **0** | knob 1 (50 sites, `45cb212d`) + knob 2 (`659de9e7`) |
| `?PostLoad@VocalTrackDir@@UAAXAAVBinStream@@@Z` | VocalTrackDir | 3,656 B | 96.386215 → **96.95952** | 96.4628 → 97.04704 | 57 → **42** | knob 1 at `streakPtr` only; clusters 1–9 remain |

Two rows crossed to `fuzzy == 100` and paid their full size (3,428 B + 1,532 B).

### 3.1 `??1GemTrackDir@@` — retail does **not** release `mArpShapePool` / `mFingerShape`

The brief carried this as "a 44 B surplus; find the surplus construct". It is two `RELEASE(x)` calls.
**Three independent signals agree**, which is why I was willing to act against the oracle:

1. `clusters` shows **one 17-instruction insert cluster (17I / 0D)** carrying exactly the two
   delete-and-null blocks (`bl ??1ArpeggioShapePool@@QAA@XZ` + `bl ??3@YAXPAX@Z`, then the same for
   `FingerShape`), with **no compensating delete cluster anywhere in the function** — absent, not
   relocated.
2. Those blocks are the **only** users of r28/r29, which is why our prologue was `bl __savegprlr_28` /
   `b __restgprlr_28` where retail saves only r30/r31 inline. Clusters 1 and 3 were *consequences*.
3. Byte arithmetic: −17 instrs (−68 B) + 2 extra prologue instrs (+8 B) + 3 extra epilogue instrs
   (+12 B) = **−48 B** against the measured 772 → 728 surplus of **44 B**.

**rb3-Wii, the game-code oracle, DOES release them** (`rb3/src/system/bandobj/GemTrackDir.cpp:83-84`) —
so this is the house case where retail bytes outrank the oracle. Guarded `#ifdef HX_NATIVE` rather than
deleted, so the native host keeps the cleanup and does not leak; the match build never defines
`HX_NATIVE`. The native gate passes 18/18 with the guard in place.

### 3.2 `?PostLoad@VocalTrackDir@@` — W16-BX's diagnosis of this row is **refuted**

BX read the three `OFFSET_SWAP (0xa8, 0xb8)` charges as a **stack-slot swap between the `cols` and
`streakPtr` locals**, tested it by hoisting `streakPtr` above the `gRev < 3` block (96.39 → **93.74**,
reverted, because it makes the ctor/dtor unconditional), and the continuation brief carried
*"reorder the slots WITHOUT moving the construction site"* forward as **the** untried arrangement.

**There is no slot swap.** Cluster 10 of the graded diff is knob 1:

```
TGT: subi r11,r11,0x738; lis r10,lbl_8202547C@h; stw r11,0xac(r31);
     stw r28,0xb0(r31); addi r11,r10,lbl_8202547C@l; stw r11,0xa8(r31)
SRC: subi r4,r11,0x738;  li r5,0; addi r3,r31,0xb8;
     bl ??0?$ObjPtr@VOverdriveMeter@@@@QAA@PAVObject@Hmx@@PAVOverdriveMeter@@@Z
```

`0xa8` vs `0xb8` is the **frame-layout consequence** of our temp being an out-of-line two-arg
construction instead of retail's inlined three stores. Respelling `streakPtr(this, 0)` → `(this)` moved
the row 96.386215 → 96.95952 and removed cluster 10. **No amount of local reordering would ever have
found this**, and a lane spending its budget on the briefed arrangement would have burned it.

---

## 4. Negative results and withdrawn verdicts

1. **WITHDRAWN (mine): "`??0GemTrackDir@@` clusters A and B are permuter-class, 12 of 16 sites
   unreachable."** Measured wrong — both closed under knob 2. See §2. Recorded because a confident
   "unfixable" closes veins and nobody re-opens them.
2. **WITHDRAWN (mine): "`??0VocalTrackDir@@` clusters 5–7 are a member-declaration-order problem the
   macro cannot reach."** Pre-registered, then refuted by the same build that confirmed the rest of the
   prediction. **Do not reach for a header member reorder on that evidence** — the store-order policy
   also produces the apparent member-init-order residue.
3. **REFUTED (W16-BX's): the `cols`/`streakPtr` stack-slot swap in `PostLoad`.** See §3.2.
4. **Carried forward from `45cb212d`, not re-hunted:** the `#pragma push / dont_inline on / pop` wrapper
   is `warning C4068 "unknown pragma"` on this compiler — inert, not a lever.
5. **My prediction for `PostLoad`'s streakPtr fix (~97.5–98) overshot**; measured 96.95952. Direction
   right, magnitude wrong. The row does not cross, as pre-registered.

**Prediction scorecard** (kept because a prediction that fails is the most informative line in a
transcript): 5 pre-registered, **1 exactly right** (PreLoad → 100 after knob 2), **3 wrong in the
favourable direction** (dtor, GemTrackDir ctor, VocalTrackDir ctor all landed higher than predicted),
**1 wrong in the unfavourable direction** (PostLoad magnitude). The systematic bias is that I
under-estimated how much apparent "scheduling" residue is downstream of a single store-order policy.

---

## 5. What I did NOT do, and the specific evidence that would overturn each

1. **The remaining 11 charges in `default/GemTrackDir` are ALL relocation-NAME fold candidates, and I
   did not install an alias for any of them. 3,276 B rides on this.**
   - `??0GemTrackDir@@` (2,548 B, 4 charges): retail `make_pair<String,String>` and
     `vector<SongPattern>::push_back` against our `make_pair<ObjPtr<EventTrigger>,ObjPtr<EventTrigger>>`
     and `vector<pair<ObjPtr<EventTrigger>,ObjPtr<EventTrigger>>>::push_back`, from
     `mDrumRollTrigs.push_back(std::make_pair(trig, trig))` / `mTrillTrigs` (GemTrackDir.cpp:71-72).
   - `??1GemTrackDir@@` (728 B, 7 charges): retail `vector<String>::~vector` (×5) and
     `vector<MoveReplacer>::~vector` (×2) against our `vector<ObjPtr<EventTrigger>>`,
     `vector<ObjPtr<RndPropAnim>>` and `vector<pair<ObjPtr<EventTrigger>,…>>` dtors. Note retail's **one**
     spelling covers **two** of our distinct symbols — the fold signature.
   - **Why not done:** `CLAUDE.md` is explicit that an unproven alias lifts `name_check`
     *by construction* and is an integrity hazard, and the dispatch brief makes alias pairs a
     coordinator decision. `TEMPLATE_ARGS_DIFFER` **is** what a fold looks like *and* what a wrong callee
     looks like; I had no retail-byte proof and would not install on a plausibility argument.
   - **What would overturn it:** run `tools/comdat_fold_gate.py` (the purpose-built adjudicator — it asks
     "is our compiled COMDAT for F the same linked body as retail's body at `addr(S)`?") on these pairs
     with a worklist, and require a **T1** verdict (retail-byte identity with relocation *target names*
     compared, anti-vacuity guards on). A T1 pass on the `vector<…>::~vector` group alone collects
     728 B; a T1 pass on both collects 3,276 B. A **failure** is equally decisive and means our container
     types are wrong, which would be a real defect worth more than the bytes.
     ⚠ Do **not** use a `none`-ruler control to validate any of this: `none` ignores relocation names, so
     it reads flat for a fabricated alias by construction.

2. **`?PostLoad@VocalTrackDir@@` clusters 1–9 (42 charges, 3,656 B).** Cluster 1 is scheduling of the rev
   unpack; clusters 2–8 are one register-allocation/ordering difference threaded through the TypeProps
   `Key`/`Value`/`TypeToString` loop; **cluster 9 is retail INLINING
   `?SetObjConcrete@?$ObjRefConcrete@VObject@Hmx@@VObjectDir@@@@` as
   `if (obj) obj->Release(ref); ref->mObject = 0;` where we call it out of line.**
   - **What would overturn "this row is hard":** cluster 9 is *plausibly this lane's lever again on a
     different `ObjRef` entry point*. `obj/ObjPtr_p.h` already has DEFER-shaped branches consulted by
     `RB3_TU_OBJPTR_DEFER_OWNER` at lines 421/457/533; if one of them (or a sibling knob) governs
     `SetObjConcrete`'s inlining, one define flips it. Given this lane's record — three separate
     "scheduling" clusters that were actually one policy — clusters 2–8 should **not** be assumed
     permuter-class until cluster 9 is closed and the row re-diffed. I ran out of budget, not evidence.

3. **`?ApplyFontStyle@` (1,164 B @74.32), `?SetRange@` (700 B @93.71), `?SetPitch@` (380 B @67.99),
   `?SetPlayerLocal@` (152 B @65.21).** Not opened. Lower priority per the brief and unchanged by
   everything this lane did — which is itself informative: they are **not** ObjPtr-policy rows.

4. **No header edit, no `splits.txt`/map/`objects.json` edit, no alias install, no permuter run.** The
   headers stayed out of scope as instructed; every change in this lane is a TU-local `#define` or a
   per-site spelling in a `.cpp`.

---

## 6. Measures

```
LANE START (45cb212d)  MEASURES 43762 4067696 10247068 69240 39.696194 49.852127 23194
LANE END   (63268c88)  MEASURES 43771 4072816 10247068 69240 39.746160 49.855300 23201
                       delta     +9   +5,120        0      0  +0.049966  +0.003173    +7

BRANCH vs main beffe2dd  43,679 -> 43,771  (+92 matched_functions)
                      4,067,496 -> 4,072,816  (+5,320 matched_code)
                      39.694244 -> 39.746160  (+0.051916 pp)
```

Rowset set-diff (`tools/rowset_snapshot.py`, run inside this worktree):

**Lane (vs `45cb212d`): 6 rows crossed in, 5,120 B; 0 fell out.**

| bytes | row |
|---:|---|
| 3,428 | `default/VocalTrackDir::??0VocalTrackDir@@QAA@XZ` |
| 1,532 | `default/GemTrackDir::?PreLoad@GemTrackDir@@UAAXAAVBinStream@@@Z` |
| 40 | `default/VocalTrackDir::fn_823037C8` |
| 40 | `default/GemTrackDir::fn_822EFA84` |
| 40 | `default/GemTrackDir::fn_822EFAD4` |
| 40 | `default/GemTrackDir::fn_822EFB24` |

**Branch (vs main `beffe2dd`): 11 rows crossed in, 5,320 B; 0 fell out** — the six above plus the five
40 B EH funclets `fn_822EE36C / 3BC / 3E4 / 40C / 45C` from `7b7ff5f0`.

⚠ `+9 matched_functions` against 6 fuzzy-crossings is the documented `mpn`/`fuzzy` ruler split, not an
inconsistency: `mpn` excludes arg-only penalties, so rows can move on one ruler and not the other.

Unit totals: `default/GemTrackDir` 340/421 fns, 23,964 B → **381/421 fns, 25,816 B**.
`default/VocalTrackDir` 436/520 fns, 37,180 B → **487/520 fns, 40,648 B**.

---

## 7. Gate chain

Run in order, all inside `~/tmp/wt-w16-bz`, native gate last.

| # | gate | result |
|---|---|---|
| 1 | full `./tools/ninja-locked` | **rc=0** |
| 2 | `scripts/verify_ruler_agreement.py --check` | **rc=0** — "both objdiff-cli entry points resolve the same ruler" |
| 3 | `scripts/verify_objs_patched.py --verify-manifest` | **rc=0** — 1,219 decomp + 3,105 target objects match, `tree_sha256=c9fdf691d375d5ef`; denylist OK |
| 4 | `tools/icf_alias_finder.py --validate` | **rc=0** — PASS: 1,407 map-consistent, 249 tolerated, **0 contradicted**, 1,657 total |
| 5 | `tools/funclet_homing.py --validate` | **rc=0** — PASS: 25,052 HOMED / 1,226 ORPHAN / 1 MIS-PINNED / 42 UNPINNED-FUNCLET |
| 6 | `tools/native_build_gate.sh` (**last**) | **rc=0** |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0`, so this is full coverage, not the `PASS (INCOMPLETE: …)` shape.

⚠ **Disclosure:** the only change made after the native gate ran is this markdown file under `docs/`.
It is not an input to any build target — `ScatterIncludes.cmake` scans `src/`, and the incident that
motivated the "gate run must be the lane's LAST action" rule was a prose comment added to a **`.cpp`**
(`rndobj/TexRenderer.cpp`), not a file under `docs/`. No `src/` file changed after gate 6.

---

## 8. Commits on `w16-bz`

| sha | row(s) | effect |
|---|---|---|
| `7b7ff5f0` | `??0GemTrackDir@@` | 79.27 → 99.58 (knob 1, 27 members) |
| `45cb212d` | `??0VocalTrackDir@@` | 70.96 → 97.76 (knob 1, 50 sites) |
| `29baac62` | `??1GemTrackDir@@` | 86.46 → 99.81 (the two `RELEASE`s) |
| `37df17d6` | `?PreLoad@GemTrackDir@@`, `??0GemTrackDir@@` | 91.62 → **100.0**, 99.58 → 99.97 (knob 1 + knob 2) |
| `659de9e7` | `??0VocalTrackDir@@` | 97.76 → **100.0** (knob 2) |
| `63268c88` | `?PostLoad@VocalTrackDir@@` | 96.39 → 96.96 (knob 1 at `streakPtr`); refutes BX's slot-swap reading |
