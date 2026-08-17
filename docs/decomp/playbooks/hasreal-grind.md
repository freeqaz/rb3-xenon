# HAS_REAL near-miss grind — battle-tested playbook

Instructions for a fresh agent grinding the "HAS_REAL" near-miss pool (named functions
scoring `[90,100)` whose residual is claimed to be a genuine struct-offset / register /
opcode diff). Written from a 2026-06-09 pilot that worked **12 diverse targets** end to
end. Read this whole file before touching a target — the wall-recognition section will
save you hours.

> **Headline finding (validate-this-first):** At the current project stage, the named
> `[90,100)` pool is **dominated by walls** (virtual-base vtable layout, boolean-negation,
> char/short-signedness CRT intrinsics, n=1 inline-policy) **and FPR/regalloc/scheduling
> permuter-class** noise. In the pilot, **0 of 12** hand-examined targets were clean
> source-level one-liners. This matches `MEMORY.md`: "NEAR-100 POOL IS FULLY CHARACTERIZED
> — no cheap tooling levers left there." Budget accordingly: most of your time is *triage
> and wall-recognition*, not fixing. A swarm here should expect a **low hit rate** and must
> be ruthless about abandoning walls fast (see §6).

---

## 0. The metric that actually counts (read this or waste a day)

The dashboard / `report.json` `measures.matched_functions` counter is
**`match_percent_normalized >= 100.0`**, computed per function in `build/45410914/report.json`.
A fix only adds tree-wide `+1` if it pushes a function's **report normalized %** from
`<100` to `100`.

Two traps proven in the pilot:

1. **`run_objdiff` "normalized %" ≠ `report.json` normalized %.** The live MCP/objdiff-cli
   `normalized` mode is *more* lenient (it folds away relocation-resolved vtable-slot loads).
   `UtilDrawAxes` reads **100.0% normalized in run_objdiff but is already counted as matched**
   (report normalized 100.0) even though its raw fuzzy is 99.98% — its only residual is a
   `lwz 0x7c` vs `0x80` *vtable-slot* load that the normalizer ignores. **Fixing it is +0.**
   Always confirm a target's *report* normalized is `<100` before investing.

   ```bash
   python3 -c "
   import json; r=json.load(open('build/45410914/report.json'))
   want={'?Foo@Bar@@QAAXXZ'}
   for u in r['units']:
     for f in u['functions']:
       if f['name'] in want:
         print(f'{f.get(\"match_percent_normalized\",0):8.3f} norm  {f.get(\"fuzzy_match_percent\",0):8.3f} fuzzy  {u[\"name\"]}  {f[\"name\"]}')"
   ```

2. **The matched counter uses report-normalized, not raw fuzzy.** Tree-wide in the pilot:
   `report normalized>=100` = 6577 (= `measures.matched_functions`), `raw fuzzy>=100` = 6542.
   The 35-function gap = already-counted functions like `UtilDrawAxes` whose only residual is
   a relocation/vtable-slot artifact. **They are not targets.** mdf2/HAS_REAL classifiers that
   rank by *raw fuzzy* will list these — skip them.

**Decision rule:** target is workable only if `report match_percent_normalized` is in
`[90, 100)`. Generate the real worklist from the report, not from a raw-fuzzy classifier:

```bash
python3 -c "
import json; r=json.load(open('build/45410914/report.json'))
rows=[]
for u in r['units']:
  for f in u['functions']:
    n=f.get('match_percent_normalized',0)
    try: n=float(n)
    except: continue
    nm=f.get('name','')
    if 90<=n<100 and not nm.startswith('fn_') and '??_' not in nm and '?\$' not in nm:
      rows.append((round(n,3), int(f.get('size',0)), u['name'], nm))
rows.sort()
[print(f'{a:8} {b:5} {c:30} {d[:55]}') for a,b,c,d in rows[:60]]
print('COUNT', len(rows))"
```

---

## 1. Setup (do this once)

```bash
cd /home/free/code/milohax/rb3-xenon
scripts/setup_worktree.sh ~/tmp/rb3-grindN grind-hasreal-N
# work ONLY in the worktree; NEVER mutate git in the main repo
# (worktrees + scratch/logs go under ~/tmp = /home/free/tmp, NEVER /tmp — /tmp is a
# RAM-backed tmpfs with no btrfs reflink, so the CoW fast-path silently falls back
# to a full ~660 MB copy there)
cd ~/tmp/rb3-grindN
./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_grindN_build.log | tail -25
python3 -c "import json;print(json.load(open('build/45410914/report.json'))['measures']['matched_functions'])"  # BASELINE
```

Record the baseline matched count. Every fix is judged by **net tree-wide delta** (re-read
this number after a rebuild), not by a single function's %.

---

## 2. The localization loop (per target)

```
PICK (from §0 worklist)  →  CONFIRM report-norm <100  →  DIFF  →  CLASSIFY (§3 wall checklist)
  →  if WALL: abandon, log root cause (§6)  →  next
  →  if FIXABLE: hypothesis → edit → A/B (objdiff) → if better keep, else revert
  →  when fn hits 100: whole-binary rebuild → confirm NET >= +1, zero regressions → commit (§5)
```

**Exact commands (use the MCP orchestrator tools — always pass `project_dir`!):**

```python
# 1. authoritative paired diff (this is ground truth, not the raw .s file)
run_objdiff(symbol="?Foo@Bar@@...", unit="default/Unit", project_dir="/home/free/tmp/rb3-grindN",
            concise=False, full_listing=True, context=3)
# 2. just the mismatches, fast:
run_diff_inspect(symbol="...", mode="mismatches", unit="default/Unit", project_dir="...")
# 3. uniform-offset detection (member-delta fingerprint):
run_diff_inspect(symbol="...", mode="offsets", unit="default/Unit", project_dir="...")
```

**Tooling gaps you WILL hit (don't waste time on them):**
- `run_diff_inspect(mode="asm_listing")` is **broken** for many units — it builds a wrong
  obj path (`ninja: error: unknown target build/45410914/default/system/.../X.obj`). Use
  `run_objdiff(full_listing=True)` instead; it gives the same paired listing.
- The raw `build/45410914/asm/<Unit>.s` file has **anonymous `fn_<hex>` symbols** and the
  function order does NOT line up with the report's relative addresses. Do **not** try to
  hand-locate a function in the `.s` by eyeballing — you will analyze the wrong function
  (the pilot did this once with VocalTrackDir and wasted a probe). Trust `run_objdiff`'s
  symbol-paired listing.
- `tools/struct_db.py` / `lookup_struct_offset` is **STALE this week** and gives offsets
  that don't match the compiled build (it returned "nothing at 0x74" for a member the build
  clearly accesses at 0x74). Trust the *compiled* offsets from objdiff over the DB. Useful
  only as a rough "which member is near offset X" hint.
- Ghidra MCP (port 8002) was **flaky** in the pilot (`ClosedException: File is closed` mid-
  query — the project is single-process and may be locked by an import or another agent).
  `ghidra-decompile.py <addr>` for a *single* function worked; bulk `code_search` did not.

---

## 3. Wall-recognition checklist (the most valuable section)

Run `mode=mismatches`, then match the signature. **If it's a wall, STOP and log it (§6).**
Recognizing a wall in 2 minutes is worth more than a 2-hour failed fix.

### 3a. Virtual-base vtable-slot wall (VBASE_WALL) — most common
**Signature:** one or more `lwz r11, 0xAA(r11)` vs `0xBB(r11)` with a **uniform +4 (one slot)**
delta, where `r11` was just loaded from a vtable (`lwz r11, 0x0(rX)` right before), i.e. a
virtual call `lwz vtbl; lwz slot; mtctr; bctrl`. Often a `subi r29, r3, 0x1dc` /
`lwz r11, -0x1dc(r3)` base-subobject adjust nearby.
**Confirm:** grep the called class's inheritance for `public virtual`:
```bash
grep -rn "public virtual\|: public virtual" src/.../ThatClass.h  # and its bases
```
If any base in the chain is `public virtual Hmx::Object` (e.g. `BandTrack`, `ObjectDir`,
`Performer`, `FlowNode`, `EventTrigger`), the +4 slot delta is a virtual-base layout artifact.
**The header virtual list is byte-identical to DC3** (`diff <(grep virtual ours.h) <(grep
virtual dc3.h)` → identical) and you **cannot** add/remove a member to fix it — the delta is
how the compiler lays out the secondary-base vtable relative to the virtual base, not an
addable member. **ABANDON.** mdf2 pre-tags many of these `VBASE_WALL`; trust that tag.
*Pilot examples:* `VocalTrackDir::TrackReset` (2× +4 via BandTrack/EventTrigger vbases),
`FlowIf::~FlowIf` (uniform −0x60 via FlowNode vbase), `Player::SetMultiplierActive`
(+4 @0x260 via Performer vbase).

### 3b. Boolean-negation wall (subic/subfe vs extrwi) — UNFIXABLE
**Signature:** target `rlwinm rX,rY,0,30,30` + `subic rA,rX,0x1` + `subfe rX,rA,rX`
(mask-bit-in-place then 0/1-normalize) **vs** our single `extrwi rX,rY,1,30` (extract-bit-to-LSB).
Comes from a `bool m = (expr & MASK);` store.
**Tried and FAILED in pilot** (do not repeat): `(expr & MASK) != 0`, `expr & MASK ? true : false`,
`(expr >> n) & 1` — all still emit `extrwi`. The target masks-in-place then normalizes; no
source form reproduces it without changing surrounding codegen. See
`docs/decomp/patterns/unfixable-compiler.md#boolean-negation-subfic-vs-subic`. **ABANDON.**
*Pilot example:* `DxRnd::Present` (`mPIXCaptureState = PIXGetCaptureState() & 2;`).

### 3c. char/short signedness in CRT intrinsics — at_limit
**Signature:** target `cmplwi rX, 0x0` (unsigned compare) vs our `extsb. rA, rX` (sign-extend
byte) — or an extra `extsh` (sign-extend halfword) before a `sthu`. Appears in inlined
`strcpy`/array-fill loops where a `char`/`short` is the loop value or store.
**Root cause:** MSVC X360 default char is *signed*; retail may differ, or the intrinsic's
internal type differs. The source (incl. explicit `(short)`/`(unsigned char)` casts, tried
in pilot) does **not** change it — it's compiler-intrinsic codegen / a `/J` (unsigned-char)
flag-level difference. A per-TU `/J` is too broad/risky and is **not** a sanctioned lever.
**ABANDON** (mark at_limit). *Pilot:* `String::operator=(const char*)` (strcpy terminator
`extsb.` vs `cmplwi`), `MicNull::MicNull` (`extsh` before `sthu` in a `short[]` fill).

### 3d. FPR scheduling / regalloc / CSE — permuter-class, NOT hand-fixable
**Signature:** lots of `diff_arg` on `fmuls`/`fmadds`/`lfs` with **swapped commutative
operands**, **reordered loads** (`lfs ...,0x8` vs `0x0`, `0x4` vs `0x14`), or a value kept in
a **callee-save reg (r26-r31)** in retail vs **reloaded** in ours (an `insert`/`delete` of an
extra `lwz`/`mr`). Also `fmuls+fadds` (target) vs fused `fmadds` (ours), or vice-versa.
**Do not hand-crack these** (`feedback_fuzzy_gap_needs_permuter.md`). They are the `/permute`
skill's job (decomp_synth, FPR-scheduling + bool-materialization). A simple "hoist the member
into a local" does **not** reliably force callee-save promotion (tried on `Bitmap::PixelColor`
— made it worse by introducing a signedness diff). Hand off to a permuter sweep, or skip.
*Pilot:* `TransformNormal`, `Rot::MakeScale`, `Geo::Intersect`, `MemStream::ReadImpl`,
`Archive::Read`, `LightPreset::CacheFrames`, `DataArray::operator<<`.

### 3e. Inline-policy mismatch (bl vs inlined body) — usually n=1, defer
**Signature:** target has the **callee's body inlined** (`lwz ...; stfs ...`) where ours emits
a `bl ?Method@Class@@...`. Marking the callee `inline` *could* fix it but risks regressing every
other caller, and per `MEMORY.md` the inline-policy lever is **TAPPED** (wins need n≥20 callers;
current candidates are n=1). Only chase if you can prove ≥20 callers benefit. *Pilot:*
`RndGroup::SetFrame` inlines `RndAnimatable::SetFrame`.

### 3f. Coverage-stub mirage / unverifiable logic divergence — defer, do NOT guess
**Signature:** target `bl <unknown lbl_8XXXXXXX>` where ours has a literal (`li r3, 0x1`) or a
named call, AND you cannot identify what `lbl_8XXXXXXX` is (Ghidra mislabels it / returns a
bogus `return 0` stub). Changing the body to `return SomeGuessedFunc()` would be an
**unverifiable corruption** — `MEMORY.md` records two such committed corruptions. **Never
commit a body change you can't verify is semantically correct.** Log and defer. *Pilot:*
`BlockMgr::ReadError` (`return 1` path is `bl lbl_82B59210` in retail; identity unresolved).

### 3g. RB3-vs-DC3 layout divergence with no oracle — deep, defer
**Signature:** a single clean member store/load at a wildly different offset (e.g. retail
`stb 1, 0x10(r3)` vs ours `stb 0, 0x34(r3)` — different *value AND* offset), where the DC3
source is byte-identical to ours and **rb3-Wii has no equivalent** (Kinect/gesture code).
The retail RB3 class layout genuinely differs from the (newer) DC3 one. Reconstructing it needs
Ghidra (flaky) + sibling-function triangulation. Deep; defer unless Ghidra is healthy.
*Pilot:* `FreestyleMotionFilter::Deactivate` (retail sets a bool@0x10=true; our DC3-derived
source sets mIsActive@0x38=false — inverted polarity + relocated field, no oracle).

### 3h. Class/struct SIZE divergence (operator new arg) — multi-member, deep
**Signature:** `li r3, 0xAAA` vs `li r3, 0xBBB` feeding `operator new` (a class size). Our class
is N bytes bigger/smaller than retail. Reconstructing which members differ is a multi-member
job, not a one-liner. Defer to a dedicated layout pass. *Pilot:* `DataWriteFile`
(`new TextFileStream`/`new Debug` size 0x144 ours vs 0x100 retail).

### 3i. Different-function address-pairing artifact (dtk paired two distinct fns) — abandon
**Signature:** target/base **body sizes differ** (e.g. ours 0x78 vs retail 0x54), or the
`operator new` size immediates differ (`li r3, 0x1a4` vs `li r3, 0x20c`), yet objdiff reads
99.9% because prologue/epilogue coincide. Common for `NewObject<T>` factory `__unwind$`
funclets: dtk address-paired our function against a structurally different retail function.
The diff is meaningless; **no source edit can fix it** — documented funclet wall /
`project_jeff_asm_misnest` class. *First-executor find (2026-06-09):* the "Rnd +4@0x54
cluster" (44 fns) was entirely this — `lwz X(r31)` where `r31` came from
`subi r31, r12, 0x70` (funclet FRAME slot, not `this`). Decisive test: compare compiled
body sizes + `new`-size immediates; if either differs, abandon.

**Divergent-`bl`-callee tell (2026-06-09 mdgrind, the decisive funclet test):** a funclet
(`subi rX, r12, <imm>` prologue) whose diff includes a `bl`/`b` to a **DIFFERENT callee**
on each side is a mis-paired funclet — even when the differing member `addi` is on a
register *loaded from* the frame slot (`addi r3, r11, 0x8` vs `0x170` where `r11` came
from `lwz r11, OFF(r31)`). That "indirect access" looked like a real member delta and was
the ~87%-polluting MISROUTE-1. **Rule:** funclet + differing `bl`/`b` target ⇒
FUNCLET_PAIRING, abandon. A *genuine* member-delta funclet (the CameraShot family) keeps
**identical `bl` callees** on both sides; only the trailing member offset differs. The
callee may read `fn_<hex>` on one side and a resolved name on the other — that still
counts as divergent when the names differ (dtk just failed to resolve one side).
`tools/wall_classify.py` now routes these `FUNCLET_PAIRING`/`DEFER_DEEP` automatically.

---

## 4. Offset-delta root-causing decision tree (when it IS a member delta)

**Gate zero — confirm the base register is really `this`:** trace where the base register
comes from. `r3` at entry (or copied from it) = `this`. The following are **NOT** `this`,
and a uniform delta keyed off them is **not** a member delta — abandon/defer, do not "fix":

- **Funclet frame pointer:** `r31` (or any reg) derived via `subi r31, r12, <imm>` is the
  exception-runtime frame pointer — `lwz/stw X(r31)` deltas are frame-slot artifacts (§3i).
- **Funclet + divergent `bl` callee (1a):** if the function is a funclet AND the diff has a
  `bl`/`b` whose callee *differs* between sides, it's a mis-paired funclet (§3i tell). The
  member `addi` being on a register *loaded from* the frame (`addi r3, r11, …` after
  `lwz r11, OFF(r31)`) does **not** make it a real member — abandon.
- **Vbase-adjusted `this` (1d):** a `this` that is run through a **large** `subi rX, rX,
  <imm>` (e.g. `0x1a0`, `0xe8`) — a virtual-base / secondary-base adjustor — before the
  member access is a **VBASE wall**, not a member delta. The diverging *adjustor immediate*
  itself (`subi r31, r3, 0xa4` vs `0xe8` in a `??_G<Class>` vector-deleting dtor) IS the
  compiler-computed vbase offset; you cannot add a member to fix it. The CameraShot
  frame-recovered-`this`, when vbase-adjusted, behaves the same way: the indirect load alone
  does **not** clear gate-zero. (Verified: treating the 21 CameraShot entries as a member-add
  netted **−3** — retail CamShot is vbase MI per RTTI. Defer to vbase reconstruction.)
- **Vtable slot (1c):** a differing `lwz rV, 0xNN(rV)` where `rV` was loaded from **offset
  0x0** (the vtable ptr) and feeds `mtctr; bctrl` is a DC3-added/removed **virtual slot**
  (e.g. Rnd uniform +0x20 = 8 slots), not a data member. Fixable only by vtable-layout
  reconstruction — defer.

`tools/wall_classify.py` encodes all four gates (run it first; `--validate` is the
regression suite, and `--mutation-matrix` is the proof that it can go red).
MEMBER_DELTA route was ~87% polluted by these before the 2026-06-09 fix.
`--validate48` was deleted 2026-08-17: 29 of its 48 ground-truth symbols no longer
resolve and 11 more are matched, so it scored a permanent, meaningless 15/48 —
see the wall_classify.py header.

Use `mode=offsets`. If you see a **single dominant UNIFORM SAME-SIGN `this`-relative delta**
(not r1/stack, not a vtable-slot, not a stack-frame `stwu r1,-0xNN` change), it's a real
member-offset divergence. **Real member deltas are uniform same-sign** — a mirrored
(`+N` here, `−N` there) pair is an OFFSET-SWAP / regswap (instruction reorder), which is
PERMUTE-class, **not** a member delta (mdgrind MISROUTE 2; ChunkStream's "+2084" was this).
Decide the cause:

| Observation | Likely cause | Oracle that answers it |
|---|---|---|
| Our member offset is **HIGHER** than retail (delta = ours−retail > 0) | We have an **extra member** before it, or an oversized member (DC3 added it; RB3 lacks it). REMOVE/shrink. | DC3 (`grep -n ... dc3-decomp/.../X.h`) — but DC3 is *newer*; if DC3==ours, RB3 dropped it. |
| Our member offset is **LOWER** than retail | Retail has a member we LACK. ADD it. | DC3 first; cross-check rb3-Wii for game classes. |
| Delta is exactly a known type size (8=list/2ptr, 0xc=ObjDirPtr or list+pad, 0xc/0x18=vector) | A container member's size differs (SIZED_VECTOR / list / ObjPtr). | `MEMORY.md` — sized-vector tree-wide is REFUTED (−504); per-TU rbtree 0x18-vs-0x1c is REAL (see `project_rbtree_4byte_deficit`). |
| Many *scattered* deltas in a ctor (different deltas per member) | Multi-member layout reconstruction, NOT a one-liner | Deep; defer (see DxRnd ctor in pilot). |

**Critical caveat proven in pilot:** the `// 0xNN` offset comments in headers are frequently
**WRONG** (Wii-derived or aspirational). `Synth::mMasterFader // 0x80` but the build accesses it
at 0x74 and retail at 0x68; `VocalPlayer::mVocalParts // 0x358` but it's actually at ~0x390.
**Do not trust the comments — trust the compiled offset objdiff shows.** When ours==DC3 source
exactly, you cannot find the delta by comparing sources; you must reconstruct the real X360
layout from the binary, which is expensive. These "clean single delta" cases were, in the pilot,
*not* clean: the divergence was upstream in a large class with unreliable comments. Treat a
single uniform `this`-relative delta as workable **only if** you can name the specific differing
member with oracle backing; otherwise it's §3h-class.

---

## 5. Gating conventions & landing criteria

**When a divergence IS a real RB3-vs-DC3 difference you must encode** (member present in one
build only), follow the established style:

- **Comment style for retail-only / DC3-only members** (see `src/system/rnddx9/Rnd.h:224`):
  ```cpp
  D3DTexture *mColorRampTex; // 0x394 (retail only; removed in DC3)
  ```
- **#define gate** for behavior/member divergences (see `RB3_HAS_HUE_CONVERGE` in
  `src/system/rndobj/PostProc.{h,cpp}`):
  ```cpp
  #ifdef RB3_HAS_HUE_CONVERGE
      ... members/code only in the variant ...
  #endif
  ```
  Define it per-TU via `extra_cflags` in `config/45410914/objects.json`:
  ```json
  "system/rndobj/PostProc.cpp": { "status": "NonMatching", "extra_cflags": ["/DRB3_HAS_HUE_CONVERGE"] }
  ```
  (per-TU opt-in is the pattern used for the rbtree `RB3_RBTREE_0x1C` lever too.)
- A **`(void)` no-op source comment** documenting an *unfixable* boundary is worth keeping even
  with no match gain (pilot left one in `Rnd_Xbox.cpp` Present, documenting the tried-and-failed
  boolean-negation forms so the next agent doesn't repeat them).

**Landing criteria (all must hold):**
1. Target function reaches **report normalized 100** (rebuild whole binary, re-read report).
2. **Whole-binary NET ≥ +1**, zero regressions elsewhere — header edits can ripple across TUs,
   so always do a full `./tools/ninja-locked` and compare `measures.matched_functions`, not just
   the one unit.
3. **Path-limited commit** on the worktree branch (never `git add -A` in a shared tree; never
   commit to main):
   ```bash
   git add src/system/path/File.cpp config/45410914/objects.json
   git commit -m "unit: <one-line root cause> (+N @100)"   # end with the Co-Authored-By trailer
   ```
4. **Never commit a body/logic change you can't verify semantically** (§3f). Layout/gate changes
   that are byte-exact-validated by objdiff are safe; guessed function bodies are not.

---

## 6. Time-boxing (measured from the pilot)

Per-target budget, hard caps:
- **Triage + classify (run mismatches, match §3 signature): ≤ 5 min.** Most targets resolve to a
  wall here. The pilot's 12 targets averaged ~5-8 min each to classify; none needed >15 min to
  *recognize* the wall.
- **Confirm a wall (grep inheritance / diff virtual lists / check oracle identity): ≤ 5 min.**
- **Attempt a fix (only if §3 says fixable): ≤ 20 min.** If 2-3 source variants (cast forms,
  `!= 0`, hoists, reorders) don't move it, it's permuter-class — **abandon and hand to /permute**.
- **Hard abandon any target at 30 min** with a logged root cause.

Log every abandoned target with its wall class so the swarm doesn't re-examine it. A running
`abandoned.jsonl` (`{symbol, unit, norm%, wall_class, note}`) is cheap and high-value.

---

## 7. What actually has upside (where to point the swarm)

Given §0's headline finding, the *highest-EV* moves are **not** hand-grinding this pool:
1. **Build the report-normalized worklist (not raw-fuzzy)** and a wall-classifier that auto-tags
   3a-3h from `mode=mismatches` output. This turns the swarm from "12 agents each rediscover the
   walls" into "1 read-only pass tags 280 functions; agents only touch the FIXABLE residue."
2. **Route FPR/regalloc/scheduling (§3d, the largest bucket) straight to `/permute`** — it is the
   sanctioned tool for exactly these, and hand-editing them is explicitly discouraged.
3. **Batch the real levers that live elsewhere**: per-TU rbtree 0x18→0x1c (`rbtree_blast.py`),
   the split-cluster EXTENSION vein (`project_unwired_game_tu_vein`), and DC3 byte-identical
   fingerprint transfer — these are documented *positive* veins, unlike this near-100 pool.

The per-fn STRUCT/CODEGEN grind in this pool is real but **thin**: expect a handful of genuine
member-delta or FMA-split wins hiding among ~280 functions, most behind the unreliable-comment
problem (§4). It is a *cleanup* pass to run after the classifier exists, not a primary lever.
