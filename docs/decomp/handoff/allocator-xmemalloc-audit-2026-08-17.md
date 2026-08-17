# Allocator audit — the XMemAlloc callee bug, and making the allocator auditable

**Opened 2026-08-17.** Coordinator doc for the handoff proposing (1) a wrong-callee
fix at `src/Memory_Xbox.cpp:287` and (2) a ranked list of allocator functions worth
decompiling.

> **✅ STATUS: CLOSED 2026-08-17.** All research reported; both fixes and both map
> names **landed and verified on main** (§6.5, `4f5b0cac`), plus a tooling defect the
> handoff surfaced by accident (§5.5, `5dd5e4f0`). Follow-on queue at the end of
> §6.5. Sections marked **GROUNDED BY THE COORDINATOR** were verified directly
> against the tree at `0f20a01c` before any agent reported.
>
> **The three things worth carrying out of this effort:**
> 1. **Pairability is a correctness instrument, not a scoring one.** Two live
>    behavioural bugs sat in the allocator *because* their rows were unpaired, and
>    were therefore invisible to the census built to find exactly that defect.
> 2. **`mpn` is structurally incapable of registering a wrong-callee fix** — measured
>    identical to the last digit across two independent fixes (§6.5).
> 3. **An enrichment ratio computed with a blind-spot-bearing detector describes the
>    DETECTOR** (§5.5) — the lesson that doubled the known size of the scope_map bug.

---

## 0. The goal, stated by the user — read this before pricing anything

> *"our goal here is to have the code be correct as a reference that we can audit +
> use for debugging against retail code."*

**This is a CORRECTNESS effort, not a byte-yield effort.** Bytes are still measured
— an A/B is how we prove a change did what we predicted, and a *surprise* in the
delta is diagnostic — but `matched_code` is **not** the ranking criterion for
anything in this doc. This is the standing
[accuracy-beats-headline-percent](../../../CLAUDE.md) directive applied to a
specific cluster.

Two consequences that shape every recommendation below:

- **A row objdiff cannot pair is a row nobody can audit.** That is not a scoring
  complaint; it is the reason this bug survived. See §2.
- **Our source is a reference document.** Where our body is a stub (`MemAlloc`'s
  644 B retail body vs our 20 B `malloc` stub — W0-ALLOC's own note), anyone
  debugging against our source is debugging a fiction. Porting a real body has
  audit value even when it moves few bytes.

**Naming and pinning `fn_822735B0` so objdiff pairs it is an intended outcome of
this effort**, alongside the source fix — the user has called for it explicitly.
Whether it lands *today* depends on §2.3's safety question, which is a hard
prerequisite, not a formality.

---

## 1. GROUNDED BY THE COORDINATOR — this handoff arrives mid-campaign

The handoff reads as if the allocator were unexplored. It is not. Landed
**2026-08-16/17**, all on `main`:

| commit | lane | what it did |
|---|---|---|
| `972c5c0a` (merge `436bfb22`) | MAPID-1 | Named retail `0x827BCD38` = `?MemAlloc@@YAPAXHH@Z`. Measured **−1,656 B** — deliberately net-negative, accuracy over headline. Converted ~304 forgiven placeholder call sites into checked ones. |
| `7ad797ca` (merge `005ee35e`) | W0-ALLOC | Fixed **7** wrong-callee allocator-spelling sites, **+1,736 B**, predicted to the byte. |
| `76b7e8cb` | — | Retired the `_MemAlloc` / `_MemFree` **phantom declarations** (12 more wrong callees), **+1,696 B**. |
| `0ede2b9c`, `9290e50f` (merge `f9001452`) | SRCPORT-1 | Ported `_MemAllocTemp`'s real body; retail's STL allocator has no temp guard and no 0x100 threshold. |

**⇒ The handoff's central thesis is already this project's confirmed doctrine.**
`?_MemAllocTemp@@YAPAXHH@Z` exists in retail (96 B, 23 refs per `005ee35e`) and is
a *genuinely different allocator*, so calling it where retail calls the persistent
one is a **behavioural bug**, not naming noise. The handoff is right; it is also
the fourth lane in a row to be right about it.

**What is genuinely NEW here: the XMemAlloc site itself is still open.**
`src/Memory_Xbox.cpp:287` reads `_MemAllocTemp(size, __FILE__, 0x107, type, align)`
at `HEAD`, and **no landed commit has touched `src/Memory_Xbox.cpp`** (verified:
`git log --follow` returns nothing for it in the campaign window; it appears in no
commit's `--stat` above).

---

## 2. Why W0-ALLOC's census walked straight past this bug

**GROUNDED BY THE COORDINATOR.** From `build/45410914/report.json`, unit
`default/Memory_Xbox` (fuzzy **20.90209%**, matched_code **552 / 3,636 B**):

```
fn_822735B0            204   fuzzy=None   mpn=0.0     <- XMemAlloc, ANONYMOUS, UNPAIRED
XMemFree               140   fuzzy=100.0  mpn=100.0   <- NAMED, and complete
fn_82272EE8             68   fuzzy=None   mpn=0.0     <- AllocType (claimed)
fn_8227351C            100   fuzzy=None   mpn=0.0     <- AllocAlign (claimed)
fn_822734E0             60   fuzzy=None   mpn=0.0     <- alleged jump-table dispatcher
fn_82272F2C            824   fuzzy=None   mpn=0.0     <- MemAllocFailed (claimed)
fn_82273420             80   fuzzy=None   mpn=0.0     <- PhysicalAlloc (claimed)
fn_82273470             68   fuzzy=None   mpn=0.0     <- PhysicalAllocTracked (claimed)
```

`fuzzy=None` is protobuf-JSON omitting a default: it means **0**, and combined with
`mpn=0.0` it means the row never paired at all.

**The mechanism, stated plainly:** W0-ALLOC's census enumerated retail call sites of
`0x827BCD38` and classified each by whether our body agreed. Its accounting was
`155 agree + 135 no base body + 7 our-body-no-reloc + 7 disagree = 304`. A row that
objdiff never pairs contributes **no comparison at all** — `fn_822735B0` sits in the
"no base body" class, so the wrong callee was **structurally invisible** to the very
instrument built to find wrong callees in this exact allocator.

> ★ **This is the generalisable finding of the effort so far, and it is worth
> more than the fix.** W0-ALLOC's merge message already records one blind spot
> (it enumerated only sites where *retail* spells `MemAlloc`, missing the
> mirror-image `JsonCalloc` bug). This is a *second, different* blind spot in the
> same census: **unpaired rows are invisible to callee adjudication, and an
> unpaired row looks exactly like a row with nothing wrong.** Any future
> callee-census must report its unpaired population as a coverage gap, not omit
> it silently.

⇒ **Naming the row is not cosmetic and not a scoring play.** It is what converts
this function from unauditable to auditable, and per standing project knowledge the
expected payout of naming an anonymous address is precisely **bug exposure, not
bytes** ([naming-an-anon-address-exposes-real-bugs](../../../CLAUDE.md)). We should
expect the row to begin pairing and *immediately* show the wrong callee.

### 2.3 The prerequisite — **CLEARED. The name is `XMemAlloc`, and it is safe.**

Standing rule: objdiff pairs by NAME, and renaming a target row to a symbol our base
obj cannot define leaves it at 0% **permanently**. So this had to be evidence, not
inference. Read from the **COFF symbol table** of `build/45410914/src/Memory_Xbox.obj`:

| | our base obj | map today | retail row |
|---|---|---|---|
| `XMemFree` | **`XMemFree`**, sec 120, class 2 EXTERNAL, **defined**, COMDAT 140 B | `"0x822732c0": "XMemFree"` | 140 B, **100/100** |
| `XMemAlloc` | **`XMemAlloc`**, sec 117, class 2 EXTERNAL, **defined**, COMDAT 220 B | **absent** | `fn_822735B0`, 204 B, **0%** |

Both are declared in the same `extern "C" {` block (`src/xdk/xapilibi/xbox.h:13`,
`XMemAlloc` at :121, `XMemFree` at :123) ⇒ both undecorated. **XMemAlloc follows
XMemFree's rule exactly — same unit, one function later, same linkage.**

**Safety, affirmatively:** our base obj **defines `XMemAlloc` today** (section index
117 > 0, storage class 2, a real 220-byte COMDAT with 20 relocations). It is *not*
swallowed by `ForceLinkXMemFuncs`, which is its own separate 8 B
`?ForceLinkXMemFuncs@@YAHXZ` COMDAT. And the name is **injective**: `"XMemAlloc"`
occurs **0 times** as a map value today. ⇒ adding `"0x822735b0": "XMemAlloc"` cannot
produce the permanently-0% row the standing rule warns about.

**Will naming expose the bug? YES — verified, not assumed.** Once paired, the `+0x3c`
relocation differs (`?MemAlloc@@YAPAXHH@Z` vs `?_MemAllocTemp@@YAPAXHH@Z`). Neither
is a placeholder (`fn_`/`lbl_`/…), so `name_check` will **not** forgive it, and a
check of `scripts/symbol_aliases.json` (1,529 groups) confirms **no alias group
carries either symbol** as survivor, folded member, or withdrawn entry. The charge
will be live and visible. **That is the auditability win, and it is the deliverable.**

---

## 3. The bug — **CONFIRMED on retail bytes**

Provenance: `report.json` at `provenance.diff_config = functionRelocDiffs=name_check`
(the shipped graded ruler), `tool_commit 88b425bc3bad`. Unit `default/Memory_Xbox`:
`total_code` 3,636 / `matched_code` 552 (15.181518%), 19 of 45 functions matched.

### 3.1 (a) The branch target — CONFIRMED, and this is the ONLY decisive evidence

Read from the retail image, not the split asm: `orig/45410914/band.exe`, `.text`
VA `0x82270000` / raw `0x264E00` ⇒ file offset **`0x2683EC`**, bytes **`48 54 97 4D`**.
Decoded by hand: opcode `0x4854974D >> 26 = 18` (branch), `LI = 0x0054974C`, `AA=0`
(relative), `LK=1` (linked) ⇒ target `0x822735EC + 0x0054974C` = **`0x827BCD38`** =
`?MemAlloc@@YAPAXHH@Z`.

Corroborated at COFF level: retail's `Memory_Xbox.obj` section `/22` carries a
type-0x6 relocation at **+0x3c** to `?MemAlloc@@YAPAXHH@Z`; **our** obj carries, at
the **same +0x3c**, a type-0x6 relocation to `?_MemAllocTemp@@YAPAXHH@Z`. Same
offset, different callee. Retail `.pdata` extent for `0x822735B0` is **exactly 204**
(decoder calibrated on two known answers first).

### 3.2 (b) The missing refcount bumps — TRUE, but a WITNESS THAT CANNOT FAIL

All 51 instructions decoded: there is **no `+0x44` access anywhere** in the 204
bytes. **But this proves nothing**, and should not have been offered as evidence.
Retail's `?_MemAllocTemp@@YAPAXHH@Z` lives at **`0x827BCFF0`** (96 B) and the bumps
are *inside it*:

```
827BD00C: bl -> 827BB898     (ThreadMemStack(true))
827BD010: lwz  r11, 0x44(r3)
827BD014: addi r11, r11, 1
827BD018: stw  r11, 0x44(r3)   <-- mTempRefs++
827BD024: bl -> 827BCD38      (MemAlloc)
```

Our source calls the **out-of-line** `_MemAllocTemp`, so our compiled `XMemAlloc`
contains no `+0x44` pattern either. **The witness is present under both hypotheses.**
Claim (a) alone carries the verdict.

### 3.3 (c) AllocType on the heap path — CONFIRMED for retail, but THE PATCH'S PREMISE IS WRONG

Every `bl` in the body, offsets relative to `0x822735B0`: `+0x04` `__savegprlr_29` ·
`+0x30` **AllocAlign** · **`+0x3c` `?MemAlloc@@YAPAXHH@Z`** · `+0x5c` `memset` ·
`+0x6c` `XMemAllocDefault` · `+0x7c` `GlobalMemoryStatus` · `+0x88` `XMemSizeDefault`
· `+0xa8` **AllocType** · `+0xbc` `MemTrackAlloc` · `+0xc8` `__restgprlr_29`.

AllocType is called **once, at +0xa8, in the physical branch**. The heap branch calls
only AllocAlign and MemAlloc — as claimed.

> ★★★ **BUT A PREDICTION FAILED HERE, AND THE FAILURE IS THE USEFUL PART.** The agent
> predicted our 220 B vs retail's 204 B was exactly the 16 B cost of the stray
> `AllocType` call. **Wrong.** Our COMDAT has **exactly one** AllocType relocation, at
> `+0xb4` — *in the physical branch*, matching retail's single call at `+0xa8`. MSVC
> **already dead-code-eliminated** the heap-branch call, because the `_MemAllocTemp`
> macro swallows `type`, leaving the variable unused and the call (pure,
> anon-namespace, visible body) removable. Our first five call sites land at
> `+0x04 / +0x30 / +0x3c / +0x5c / +0x6c` — **byte-identical offsets to retail**; the
> whole first 0x6C aligns. The entire 16 B delta lives in the **physical** branch.
>
> ⇒ **Removing `AllocType` from the match path buys ZERO bytes.** Do it for source
> honesty — it makes the source say what the binary does, which is this effort's
> stated goal — but **it is not the fix and must not be sold as one.** A plausible
> arithmetic coincidence (220 − 204 = 16) nearly ratified the wrong mechanism.

### 3.4 (d) The behavioural stake — CONFIRMED, with the mechanism

`MemMgr.cpp:458` defines `_MemAllocTemp(size, align)` as
`MemDoTempAllocations tmp; return (MemAlloc)(size, align);`, and the retail body
above proves it. Per the placement logic at `MemMgr.cpp:380`/`:405`, a live temp ref
selects `MemHeap::kLastFit` — top-down from the temp region — where plain `MemAlloc`
uses the default bottom-up strategy.

**Concrete consequence:** `XMemAlloc` is the XDK's *global* allocation hook, so
**every non-physical XDK allocation — D3D, D3DX, XAudio, XAPI, XACT, XGRAPHICS, XUI,
XMV — is currently placed top-down on the temp heap instead of the normal heap.**
Same defect class W0-ALLOC fixed for vertex and stream buffers, and by fan-in
plausibly the **largest single instance of it in the binary**.

### 3.5 Patch assessment — directionally right, needs three corrections

**Correct as written:** the parenthesized `(MemAlloc)(size, align)` is *required* —
`MemMgr.h`'s macro force-zeros align, and align is genuinely non-zero here (our
`AllocAlign` heap path returns `0x10`/`8`; retail's `fn_82273588` returns
`li r3,0x10` / `li r3,8`). Matches `src/system/synth/Mic.cpp:38`. `type` is used
nowhere else in the non-physical branch, so deleting it is safe. The `MILO_ASSERT`s
at 0xf9/0x10d are **free** — confirmed *empirically*, not just by reading the macro:
our heap-branch call offsets are byte-identical to retail's through `+0x6c`, which
could not happen if the asserts emitted anything.

**Corrections:**

1. **Drop the claim that removing `AllocType` fixes anything measurable** (§3.3). The
   only load-bearing edit is `_MemAllocTemp` → `MemAlloc`.
2. **The `#ifdef HX_NATIVE` split is optional.** A one-line change with `type`
   deleted gives the identical match build. If the split is kept, note it also
   changes **native** behaviour (native stops using the temp heap here) — correct per
   retail, but state it deliberately rather than let it happen as a side effect.
3. ⛔ **Expect ZERO bytes from the source patch alone.** `matched_code` keys on
   `fuzzy == 100` and the row is **unpaired** — it reads 0% before *and* after.
   **The map entry and the source fix must land TOGETHER**, or the fix is invisible
   on every instrument. Even together, the residual 16 B of physical-branch
   divergence keeps the row below 100 until that is addressed too.

---

## 3.6 Sibling anonymous rows — ranked, and TWO HANDOFF IDs REFUTED

**SAFE TO NAME** (real, whole, `.pdata`-confirmed functions):

| row | name | confidence | evidence |
|---|---|---|---|
| `fn_822735B0` | **`XMemAlloc`** | VERY HIGH | §2.3 + §3.1 |
| `fn_82273420` | **`?PhysicalAlloc@@YAPAXH@Z`** | HIGH | `.pdata` len 80; body is `XPhysicalAlloc(size,-1,0,4)` (`li r6,4; li r5,0; li r4,-1`) → `XPhysicalSize` → `gPhysicalUsage +=`. The `4` in r6 distinguishes it from `PhysicalAllocTracked`, which takes alignment as a parameter and calls `MemTrackAlloc` (retail's body calls neither). |
| `fn_82273470` | **`?PhysicalFree@@YAXPAX@Z`** | HIGH | ⚠ **the handoff said `PhysicalAllocTracked` — REFUTED.** `.pdata` len 68; body is `XPhysicalSize` → `gPhysicalUsage -=` → `XPhysicalFree`. Exactly two calls, no `MemTrackFree`, so not `PhysicalFreeTracked` (already named at `0x822733B8`, 84 B, 100%) and not an *alloc* at all. |

Note both Physical rows have geometry mismatches that are **the point, not a
problem**: ours are 124 B and 76 B vs retail's 80 B and 68 B, because our source
carries a `MemAllocFailed` failure branch and an `if (address != 0)` guard that
retail lacks. Pairing them surfaces exactly that.

⛔ **DO NOT NAME — these are dtk mis-carves, not functions:**

- **`fn_82272EE8` is the ENTRY of AllocType**, not a whole function — it computes
  `isPhys` and `type = (attrs>>16)&0xFF`, indexes a byte table at `lbl_8200E610` and
  `bctr`s. The source function spans `0x82272EE8..0x822732C0` = **984 B across three
  rows** (68 + 824 + 92); our obj has **one** 1,164 B COMDAT. Naming the 68 B
  fragment would pair 68 target bytes against 1,164 base bytes — a *misleading*
  pairing, worse than none.
- **`fn_822734E0` is the ENTRY of AllocAlign.** ⚠ **The handoff had this inverted**,
  calling `fn_8227351C` AllocAlign and `fn_822734E0` a "dispatcher". Span
  `0x822734E0..0x822735B0` = **208 B across four rows**; ours is one 224 B COMDAT.
- **`fn_82272F2C`, `fn_82273264`, `fn_8227351C`, `fn_82273580`, `fn_82273588` are not
  independently callable — PROVEN, not assumed.** `fn_82273264`'s *first* instruction
  is `cmplwi cr6, r10, 0x7f`, reading `r10` **live-in** from `fn_82272EE8`'s
  `extrwi`; `fn_82273588`'s first instruction reads `r11` live-in from `fn_822734E0`.
  `fn_8227351C` is twelve `li r3,<pow2>; blr` pairs reachable only by `bctr`. None is
  a `.pdata` BeginAddress.
- ⚠ **`fn_82272F2C` is NOT `MemAllocFailed`** (the handoff's guess). Its blocks return
  AllocType's string literals, read out of `.rdata`: `"XTL:D3D"`, `"XTL(phys):D3D"`,
  `"XTL:D3DX"`, `"XTL:XAUDIO"`, `"XTL:XAPI"`, with the default path returning
  `"XTL:Game"` / `"XTL(phys):Middleware"` / `"XTL:Unknown"`. **`MemAllocFailed` has
  no standalone retail row in this region at all** — retail inlined a reduced version
  into XMemAlloc, of which only `GlobalMemoryStatus` survives.

Extra caution even if the carve is ever fixed: 4–7 are anonymous-namespace statics
spelled `?AllocType@?A0x7a439e55@@YAPBDK@Z`, whose hash MSVC derives from machine
name + source path. A map entry would need the post-`anon_ns`-patcher spelling and
would be fragile across environments. Another reason to defer.

★ **Bonus source-correctness divergence, independent of the callee bug:** retail's
AllocType dispatches on `type - 0x80` over `[0, 0x1a]` (type ∈ `[0x80, 0x9A]`), with
`type ≤ 0x7f` → `"XTL:Game"`, `type ≥ 0xc0` → `"XTL:Middleware"`, else
`"XTL:Unknown"`. **Our source switches on `type` at base 0** and has no
Game/Middleware/Unknown default. That explains the 1,164 vs 984 byte gap.

---

## 3. The bug `[PENDING]`

Claimed: our `_MemAllocTemp(size, __FILE__, 0x107, type, align)` — which
`MemMgr.h`'s macro rewrites to `(_MemAllocTemp)(size, align)` — should be plain
`(MemAlloc)(size, align)`.

Handoff's three tells, all under verification:

- **(a)** bytes `48 54 97 4D` at `0x822735EC` decode as `bl` → `0x822735EC +
  0x0054974C = 0x827BCD38` = `?MemAlloc@@YAPAXHH@Z` (the address MAPID-1 named).
- **(b)** No inlined `MemDoTempAllocations` refcount bumps anywhere in
  `fn_822735B0` — the temp guard's ctor/dtor would appear as `lwz`/`addi`/`stw`
  around the call.
- **(c)** Retail does not call `AllocType` on the heap path — only on the physical
  path, feeding `MemTrackAlloc`. If so, our `const char *type = AllocType(attrs);`
  is a stray call in the match build.

`[PENDING — agent 1: per-claim verdict with the decoded instructions]`

### 3.1 Coordinator's note on the proposed patch

The handoff's patch uses `(MemAlloc)(size, align)` — **parenthesized**. That is
correct and load-bearing: `MemMgr.h:212`'s function-like macro
`#define MemAlloc(size, file, line, name, ...) (MemAlloc)((size), 0)` **force-zeros
align**, so the unparenthesized form would silently discard a real alignment.
`src/system/synth/Mic.cpp` is the cited house precedent for the bypass.

Verified inline (`src/Memory_Xbox.cpp:279-310`): `type` is used **only** on the line
under discussion within the non-physical branch — the physical branch calls
`AllocType(attrs)` separately at line 306. So if (c) confirms, moving the `type`
local under `HX_NATIVE` is clean and leaves no unused variable. `[PENDING]`
confirmation that this is what retail does.

---

## 4. The ranked target list — **AUDITED. Roughly half does not survive.**

Every size below was verified against the `.fn`/`.endfn` extent in
`build/45410914/asm/*.s` and **all agree with `report.json` exactly**, so the
"8,852 B billed for a 12 B body" hazard did not fire here. No candidate appears in
`scripts/symbol_aliases.json` (checked by `command grep -a` on every address and
name — zero hits) ⇒ **none of these bytes are alias-forgiven, and none is
uncollectable for want of a proven fold.**

### 4.1 ★★★ THE REORDERING FINDING: three distinct pairability blockers

- **Blocker A — anonymous target row.** The base obj already defines the right
  mangled name; **one map row makes it pair.** Cheap, and the only thing between us
  and seeing the divergence.
- **Blocker B — MIS-HOMED PIN: the name exists, but in the wrong obj.**
  `fn_827BCA78` sits in **MemMgr**'s target obj while our
  `?Alloc@MemHeap@@QAAPAHHHAAH@Z` is defined in **MemHeap.obj**. Naming it would
  produce a row that reads **0% forever** — the exact standing hazard. `fn_827BD300`
  and `fn_827BC2D0` have the same shape in the opposite direction.
  ★ **The remedy is already in-tree and proven, and it is NOT re-homing pins**
  (measured non-neutral): `src/system/utl/MemHeap.cpp:545-560` carries a
  `#ifndef HX_NATIVE` **"retail TU-reunification"** block that duplicates definitions
  into whichever unit owns the address — retail compiled MemHeap + the free `Mem*`
  API into **one** TU; the MemMgr/MemHeap split is **DC3's, not retail's**.
  `MemNumHeaps`/`MemHeapSize`/`MemFindAddrHeap`/`MemPushHeap`/`MemPopHeap`/
  `MemPushTemp` are all at 100% because of it.
- **Blocker C — the row is a PHANTOM (dtk mis-carve).** Uncollectable at the current
  carve; the work would be a **split-carve fix, not decompilation**.

### 4.2 Candidates

| # | row | true extent | unit | now | play | blocker | bytes if crossed |
|---|---|---:|---|---|---|---|---:|
| 1 | `?MemAlloc@@YAPAXHH@Z` `0x827BCD38` | **644** | MemMgr | **paired**, fuzzy 2.57 | **body port** | **none — unblocked** | 644 |
| 2 | `fn_827BCA78` MemHeap::Alloc | 652 | MemMgr | anon, 0% | ident + TU-reunify + body port | **B** | 652 |
| 3 | `fn_827BAEC0` FixedSizeAlloc::RawAlloc | 176 | PoolAlloc | anon, 0% | **identification** | A only | 176 |
| 4 | `fn_822735B0` **XMemAlloc** | 204 | Memory_Xbox | anon, 0% | **identification** | A only | 204 |
| 5 | `fn_827BC838` MemPrintOverview | 300 | MemMgr | anon, 0% | signature fix + ident | A + wrong param type | 300 |
| 6 | `fn_82273420` PhysicalAlloc | 80 | Memory_Xbox | anon, 0% | identification | A only | 80 |
| 7 | `fn_82273470` **PhysicalFree** | 68 | Memory_Xbox | anon, 0% | identification | A only | 68 |
| 8 | `fn_827BD300` MemInit | 788 | MemHeap | anon, 0% | ident + TU-reunify | B | 788 |
| 9 | `fn_827BC2D0` heap-config parse | 344 | MemHeap | anon, 0% | ident (**unsettled**) + reunify | B + ID | 344 |
| — | AllocType/AllocAlign complex (6 rows) | 1,184 | Memory_Xbox | anon, 0% | **none** | **C** | **0** |
| — | `fn_82279710…50` ×5 | 5×16 | Memory_Xbox | anon, 0% | **not allocator code** | — | 0 |

Near-term collectible: **644 B unblocked + 528 B behind one map row each + 300 B
behind a signature fix.** Carve-blocked: **1,184 B.**

### 4.3 ⛔ Handoff claims REFUTED or STALE

1. **`fn_82272F2C` = MemAllocFailed, 824 B — REFUTED.** It is AllocType's jump-table
   **arm block**. (Both agents independently. The handoff's own suspicion — the CSV
   string is missing — was the tell; the conclusion drawn from it was wrong.)
2. **`fn_8227351C` = AllocAlign — REFUTED.** That is AllocAlign's *arm block*; the
   head is `fn_822734E0`.
3. **`fn_82273470` = PhysicalAllocTracked — REFUTED.** It *subtracts* from
   `gPhysicalUsage` and calls `XPhysicalFree`. It is `PhysicalFree(void*)`.
4. **"five 16 B XMemFree/XMemSize stubs" — REFUTED.** All five are **virtual-base
   adjustor thunks** (`lwz r11,-4(r3); subf r3,r11,r3; subi r3,0x3c; b <target>`);
   the first branches to `?Highlight@RndDir@@UAAXXZ`. Not allocator code at all.
5. **"both decomps carry the `mBlock == nullptr` guard" — HALF REFUTED.** DC3 does
   (separate `TryAlloc`, `MemHeap.cpp:314`). **rb3-Wii has no `TryAlloc` at all** —
   `Heap::Alloc` holds `FreeBlockInfo` directly, structurally like retail.
   **Neither oracle describes RB3-360.**
6. **"derefs at `+0x14c`" — NOT REPRODUCED.** The null-path derefs are at
   `+0x0/+0x4/+0x8` of `r30`. "With `gMemLock` held" is right on the main-thread path
   only (`Abandon()` fires only when `!MainThread`).
7. **"`out_of_mem_alloc_info.csv` absent from retail" — CONFIRMED**, and the
   instrument discriminates: the same scan *finds* `Allocation failure` at
   `0x117460`, the very `lbl_82117460` the asm references.
8. ⚠ **STALE IN-SOURCE NOTE.** `src/system/utl/MemMgr.cpp:416` still says *"IT CANNOT
   SCORE UNTIL 0x827bcd38 IS NAMED … Do NOT name it first."* **MAPID-1 named it on
   2026-08-16.** The prescribed order was inverted by events; the port is now simply
   unblocked. Fix this comment when the port lands.

### 4.4 ★★★★ THE SECOND LIVE BUG — same class, same cause, found only because we asked about pairability

**`src/system/utl/PoolAlloc.cpp:148`** — `sPoolBuf = _MemAllocTemp(gBigHunk, …)`,
where retail is `li r4,0; bl fn_827BCD38` = `MemAlloc(gBigHunk, 0)`.

> **A pool chunk is never freed. Allocating it from the temp heap is a genuine
> behavioural bug**, not a metric artifact.

It survived W0-ALLOC's census for **precisely the same reason** as XMemAlloc: its row
is unpaired, so the comparator never saw it. ⇒ **Two wrong-callee bugs of the class
W0-ALLOC fixed were sitting live in this tree, both invisible to the instrument
built to find them, both surfaced by asking a *pairability* question rather than a
scoring one.** This is the coordinator's §2 point demonstrated twice over:
**pairability is a correctness instrument, not a scoring one.**

### 4.5 Audit-value findings — the part that matters most for the stated goal

**The OOM crash path is real, and our source misdescribes it.** In retail
`fn_827BCA78` the strategy switch dispatches to the four fit functions, then
`lwz r30,0x70(r31); cmplwi r30,0; bne .L_823F21BC`. The **fall-through** is the
failure report — `FreeBlockStats` → `MainThread` → `gInsideMemFunc=0;
gMemLock->Abandon()` → `MakeString("Allocation failure…")` → `MemPrintOverview(-3,…)`
→ `~String` — **and it then falls straight into `.L_823F21BC`, whose first
instruction is `lwz r9,0x4(r30)`, dereferencing the NULL block.** There is no
`return`, and the switch's `default:` arm branches into the same path. That is a
compiled-out-`MILO_FAIL` crash path: the dev build halts in the assert, retail walks
off the end. **Our tree returns `nullptr` cleanly — so anyone debugging an OOM
against our source would predict a graceful null return and be wrong.**

**An oracle-fidelity error with a clean answer.** `fn_827BC838` is
`MemPrintOverview`, and its second parameter is a **`TextStream&`** — retail
constructs a `String`, appends via `??6TextStream@@…`, passes it, destroys it, and
`Str.h:65` confirms `class String : public TextStream`. **rb3-Wii's
`MemPrintOverview(int, TextStream&)` is correct; DC3's `(int, char* const)`, which
our tree uses, is wrong.** Three call sites: `Rnd.cpp:1527`, `MemHeap.cpp:426`,
`Memory_Xbox.cpp:270`. (Textbook instance of the standing rule that DC3 is newer and
not automatically right for RB3.)

**Dead code we inherited that retail does not have:** the `gMemTracker` /
`fopen("alloc_fail.txt")` / `SpitAllocInfo` block at `MemHeap.cpp:406-413` (string
absent from `band.exe`); the `printf("PoolAlloc warning…")` at `PoolAlloc.cpp:143`;
the `if(!ptr) MemAllocFailed(...)` branch in `PhysicalAlloc`. ~~⚠ **Where retail's
`MemAllocFailed` lives is UNRESOLVED** — no retail counterpart located, and its only
distinguishing string is absent.~~

✅ **RESOLVED 2026-08-17 (lane W0-XMEM2): it does not live anywhere.** Retail never
compiled the body. Inside retail's `XMemAlloc` the entire failure branch is two
instructions — `addi r3, r1, 0x50 ; bl fn_8283C980` (`GlobalMemoryStatus`) — with
**no argument setup**, and retail's frame is `0x90` where ours was `0x70`: the
**+0x20 delta is exactly `sizeof(MEMORYSTATUS)`**, i.e. the local was inlined into
`XMemAlloc`'s own frame and both parameters are unused. A `band.exe` scan finds
**none** of the function's distinguishing literals — `want %d, have %d`,
`total phys`, `out_of_mem_alloc_info.csv`, `devkit:` — while the **controls in the
same `.rdata` neighbourhood all fire** (`XTL:D3DX`, `XTL(phys):Middleware`,
`POOL REPORT`), so the scan is capable of finding strings here and the absences are
real. Corroboration: retail has no failure branch in `PhysicalAlloc` /
`PhysicalAllocTracked` either — `fn_82273350` runs `XPhysicalAlloc` →
`XPhysicalSize` → `MemTrackAlloc` with no null test between them.

⚠ **The near-miss that had to be ruled out by hand, recorded because §4.3.7 came
within one character of the wrong answer:** `band.exe` **does** contain
`"Allocation failure, "` — comma and space included — at `0x117460`, which looks
exactly like this function's format-string prefix. Dumping the whole string shows it
is `'Allocation failure, heap "%s", want %d bytes\n   lFrags=…'`, i.e. **MemHeap's**
report sharing a coincidental prefix. **A prefix probe was too weak; only reading the
whole string settled it.**

⚠ **The orchestrator DB is stale and must not be used as a state source here** — zero
prior attempts on any candidate, and it shows `?MemFree@@` at 0.0%/`AT_LIMIT` where
`report.json` says 48.55%.

### 4.6 Recommended priority (by AUDIT value, per the §0 goal)

1. **Port `?MemAlloc@@YAPAXHH@Z` (644 B)** — paired, unblocked, and the body is
   already reverse-engineered from retail bytes at `MemMgr.cpp:356-424`. Today
   *every* allocation in our source ends at `malloc()`, so heap selection, the
   temp-heap fallback and the `kNoHeap` physical path are **all fiction**.
2. **`fn_822735B0` XMemAlloc + `fn_827BAEC0` RawAlloc (380 B)** — cheapest real work
   in the list: one map row each, and each **immediately exposes a live temp-heap
   misrouting**. A/B each naming *separately*.
3. **`fn_827BCA78` MemHeap::Alloc (652 B)** — highest behavioural value, highest
   effort; needs the reunification duplicate *and* a body neither oracle supplies.
   Landing it makes the OOM crash path legible.
4. **`fn_827BC838` MemPrintOverview (300 B)** — fix the signature to `TextStream&`
   first (3 sites), then name.
5. **PhysicalAlloc + PhysicalFree (148 B)** — small, pair on naming.
6. **Defer** MemInit / `fn_827BC2D0` (1,132 B, dual-blocked, one identity unsettled).
7. **Do not fund** the AllocType/AllocAlign complex or the five thunks.

---

## 5. Data-quality snags — **ADJUDICATED** (agent 3, read-only)

All three verdicts are **MIXED**, and the most valuable output is a defect the
handoff did not know it had found.

### 5.1 ★★★ SNAG 1 — the observation is real, the diagnosis fails on all three counts, and underneath it is a NEW GENERAL DEFECT in `tools/scope_map.py`

The reported row reproduces exactly — `config/45410914/scope_map.json` key
`822734E0` says `"provenance": "pinned:src/system/obj/Object.cpp"`,
`"matched": true`, `"confidence": 1.0`, `"size": 116`.

**But it is not a mis-pin, not ICF, and not the `spatial:*` tier:**

- **Not a mis-pin.** `splits.txt` (`Memory_Xbox.cpp .text 0x82272EE0–0x822736B4`),
  `symbols.txt` (`fn_822734E0 … size:0x3C`) and `report.json` (unit
  `default/Memory_Xbox`) **all agree** the address is Memory_Xbox's. No map or pin
  is wrong.
- **Not ICF.** The Object.cpp row is `?SetTypeDef@Object@Hmx@@UAAXPAVDataArray@@@Z`,
  whose **true** address is `0x8275AB18` — inside Object.cpp's own pinned block.
  Nothing folded. `0x822734E0` is **absent from `target_symbol_map.json` entirely**.
- **Not the spatial tier.** Provenance is `pinned:` at **confidence 1.0**. The
  documented 33.76% spatial-FP caveat does not apply; this is a *worse* class — a
  **confidence-1.0 phantom**.

**Byte geometry is what exposed it**, exactly as the standing rule prescribes:
scope_map claims **116 B** where symbols.txt and report.json say **0x3C = 60 B**,
and the range `0x82272D30–0x82273760` holds **~80 mutually-overlapping rows
attributed to 12 different TUs**. Functions cannot overlap; that layout is
impossible.

**Root cause.** `tools/scope_map.py`'s pinned-unit branch resolves a mangled-named
function as `base + report_relative_offset`. `report.json`'s per-fn `address` is a
**per-unit cumulative offset**, so for a **multi-block** unit this computes
`first_block_start + cumulative offset` — *the identical synthetic formula
`CLAUDE.md` documents for dtk's `.s` address columns, independently reimplemented in
a second tool.* Object.cpp has 15 `.text` blocks from `0x82272DB8`, and
`0x822734E0 − 0x82272DB8 = 0x728` is precisely SetTypeDef's cumulative offset. The
function's own comment acknowledges the hazard for catch-all units and the correct
VA-lookup machinery **already exists there** — it simply is not applied to pinned
units.

**Scale, with a control that discriminates:**

| population | mangled-named fns | placed outside every real block of own unit |
|---|---:|---:|
| single-block pinned units (**control**) | 2,188 | 192 (**8.78%**) |
| multi-block pinned units (**treatment**) | 20,296 | 10,930 (**53.85%**) |

**6.1× enrichment; 11,122 rows / 2,100,900 B / 464 units affected.** And the
decisive validation: of the bad rows carrying a true map address, **11,120 of
11,120 (100%) land inside a real block of their own unit** ⇒ the functions and the
**unit attributions are correct; only the address key is fabricated.**

**Identity of `fn_822734E0` — the handoff was substantially right.** Referenced
exactly twice tree-wide (scan keyed on `.fn`): its own definition, and
`fn_822735B0`. That sole caller does `mr r3,r4 ; bl fn_822734E0 ; mr r4,r3 ;
bl fn_827BCD38` — decode the align nibble, then `MemAlloc(size, align)`. It is a
**Memory_Xbox.cpp-internal align-nibble decoder** (`extrwi r11,r3,4,4`, `bctr` into
the `li r3,<pow2>; blr` table at `fn_8227351C`). Same TU, confirmed three ways.

**The ICF intuition was right at different addresses:** `0x82273580` →
`?CamOverride@RndDrawable@@…` where the bytes are `li r3,0; blr` (every
`return NULL` folds), and `0x822734D8` → `??2Node@?$ObjPtrList@…` where the bytes
are `mr r4,r5; b fn_827BCD38`. Textbook 8-byte fold survivors; neither is in
`scripts/symbol_aliases.json`.

**Recommended (not applied):** do **not** touch `splits.txt` or
`target_symbol_map.json` — there is no defect there, and re-homing is not
metric-neutral. Fix `tools/scope_map.py` to resolve pinned-unit mangled names by
real VA (the catch-all branch already does this). **Until then, `scope_map.json`
must not be queried by address range** — its per-function `scope`/`provenance` is
sound, its address key is not, and the failure is silent and returns a confident,
well-formed answer. That is exactly how it produced this handoff.

### 5.2 SNAG 2 — mechanism is KNOWN DOCUMENTED; the Memory_Xbox half is a HANDOFF ERROR

Confirmed as the documented 2026-08-04 behaviour: `MemMgr.cpp` has **11 `.text`
blocks**, and `.fn fn_827BCD38` renders at column `0x823F233C` = first block start
`0x823F1898` + cumulative `0xAA4`. The `.fn` names are correct. So *"don't read
addresses off MemMgr.s"* is right — but it is the pre-existing universal rule, not a
new finding.

**Refuted: "Memory_Xbox.s is self-consistent."** That unit has **five** `.text`
blocks and is self-consistent only in block 1 (which coincidentally starts at the
unit base). From block 2 on, the columns are synthetic — `fn_82279708` renders at
`0x82273724`, `fn_822797B0` at `0x8227377C`, `fn_822BB900` at `0x82273784`.

⚠ **This is the dangerous half**: it licenses precisely the misreading the rule
forbids, on a file where 3 of 5 blocks are wrong. No code change — correct the note.
A single-block unit is the only safe case, and *"looks self-consistent"* is not a
test for that; the `splits.txt` block count is.

### 5.3 SNAG 3 — the addresses ARE stale (hypothesis correct); every load-bearing conclusion SURVIVES

Both citations date to `8201efb6` (2026-05-29) and `4505979f` (2026-06-16) —
**before** the TU5 flip of 2026-07-15. Full audit of `src/system/utl/MemMgr.h`:

| line | cited | claims | TU5 status |
|---|---|---|---|
| 79 | `0x82797500` / `0x827975C8` | MemTemp ctor/dtor | ⛔ **STALE** — no function at either |
| 97 | `0x827BC270` | MemPushTemp | ✅ CURRENT = `?MemPushTemp@@YAXXZ` |
| 97 | `0x827BC2A0` | dtor helper | ✅ CURRENT (unnamed, presumably MemPopTemp) |
| 192 | `0x827977D0` | MemAlloc | ⛔ **STALE** → **`0x827BCD38`** `?MemAlloc@@YAPAXHH@Z` |
| 203/205 | `0x827979D8` | _MemAllocTemp | ⛔ **STALE** → **`0x827BCFF0`** `?_MemAllocTemp@@YAPAXHH@Z` (**0x60 = 96 B**) |
| 235 | `0x82798360` | example caller | ⛔ **STALE** — no function |
| 304-307 | `0x82709EE0` | operator new | ⚠ exists, but map names it `??_GRandomGroupSeq@@UAAPAXI@Z` — unsupported |
| 315 | `0x8240F380` | RndDir example | ✅ CURRENT = `?NewObject@RndDir@@SAPAVObject@Hmx@@XZ` |

The 96 B at `0x827BCFF0` matches W0-ALLOC's independently-reported figure exactly.
The mix is explained by `31ec215a` (2026-07-16, the day *after* the flip), which
rebased part of the header and left the older citations.

**The conclusions survive, and are now evidence-backed twice over.** The current
mangled names encode arity directly, so they testify about the **ABI**, not merely
about addresses: `?MemAlloc@@YAPAXHH@Z` = `void*(int,int)` → **2 args** (the
header's central claim); `?_MemAllocTemp@@YAPAXHH@Z` → 2 args;
`?MemFree@@YAXPAX@Z` → 1 arg; `?MemPushTemp@@YAXXZ` → 0 args. Retail bytes at the
call site corroborate: `mr r3,r29 ; mr r4,r3 ; bl fn_827BCD38` — exactly two
argument registers, no string loads.

⇒ *"comment cites a stale address"* = **YES, 5 of 9**. *"comment's conclusion is
wrong"* = **NO, none of them.** Recommended: comment-only refresh of the 5 stale
citations; **do not change the macro design**; separately adjudicate `0x82709EE0`
(stale citation, or an ICF survivor name on a folded 8-byte body?).

⚠ **A method the agent tried and discarded, recorded so nobody repeats it:**
`symbols.txt` retains stale `except_data_<X>` label names on rebased addresses
(8,656 of 9,142 mismatched), which *looks* like a free TU0→TU5 correspondence
oracle. It is not — the implied mapping is **non-monotonic**, so it cannot be a
rebase correspondence. It would have "confirmed" `0x827977D0 → 0x827BC430`, which is
`MemFree`, **not** MemAlloc: a wrong answer with a plausible shape.

### 5.5 ✅ LANDED as `5dd5e4f0` (lane SCOPEMAP-VA) — and the lane corrected §5.1 twice

Fix: one shared `resolve_named_va()` (target_symbol_map → symbols.txt → dup
disambiguation by block containment, 99.99% coverage); the catch-all branch now
calls it too, so there is **one** address-resolution implementation instead of
three. Unresolvable rows are parked and **counted**, never given a plausible
address. New `scope_map.py validate-addrs` asserts containment **and**
size-vs-`symbols.txt`, exit 1 on failure.

⛔⛔ **CORRECTION 1 — the defect was ~2× the size recorded in §5.1, and §5.1's
"control" was NOT a control.** Measured against true pre-fix addresses: **22,090 of
23,036 pinned named rows (95.89%)** carried a fabricated address, not the 11,240
(48.79%) a containment test can see. The other **47.10% landed inside ANOTHER BLOCK
OF THE SAME UNIT**, where containment is structurally blind. ⇒ single-block units are
**87.53% fabricated while reading only 7.62% "bad"**, so **§5.1's 6.1× enrichment
measures DETECTABILITY, not defect rate.**

> ★★★ **The reusable lesson: an enrichment ratio computed with a
> blind-spot-bearing detector describes the DETECTOR, not the population.** §5.1's
> figure was passed down as though it sized the defect; it sized the instrument.

⛔ **CORRECTION 2 — §5.1's recommended block-walk fallback is WRONG.** Implemented
and measured: it recovers the true VA for only **5.20%** of rows (4–12 B errors
accumulating, because pinned spans include alignment/EH padding the cumulative
offsets do not). Rejected, with an in-code comment so it is not reintroduced.

**Proof it discriminates** (a PASS is worthless until the gate is shown to fail): a
mutation reintroducing `base + rel` gives `FAIL 11,240` / exit 1; the fixed tree
gives `FAIL 0` / exit 0; and 11,240 matches an independently written auditor to the
row. **Control unchanged**: `fn_` anchor rows 40,858 changed **0**, catch-all rows
5,332 changed **0**, zero drift in size/matched/source_path/fuzzy, key set identical.

★ **Arithmetic self-validation on a route sharing no logic with the above:**
`scope_map.json` now holds exactly `total_functions` = **69,226** keys with zero
collisions. **The fabricating version held 68,576 — it had been silently LOSING 650
functions to colliding synthetic keys**, and nothing had noticed.

⚠⚠ **DEPLOYMENT GOTCHA, caught by the coordinator after the merge:**
`config/45410914/scope_map.json` is **gitignored**, so **merging the fix does NOT
refresh the artifact everyone reads.** Immediately post-merge, main's on-disk file
was still the stale 68,576-key version *still containing the bad `822734E0` row*,
while `validate-addrs` reported **PASS** — because the gate recomputes from
`report.json` through the fixed code path rather than reading the artifact. ⇒ **run
`python3 tools/scope_map.py build` after landing**, and do not read the gate's PASS
as a statement about the file on disk. Verified after regenerating: 69,226 keys,
`822734E0` → `pinned:src/Memory_Xbox.cpp` size 60, and `8275AB18` → SetTypeDef size
116 (**previously absent from the map entirely**).

### 5.4 ★ The cross-cutting finding

**SNAGs 1 and 2 are the same bug in two different tools.**
`first_block_start + cumulative offset` is documented for dtk's `.s` output, and
`tools/scope_map.py` independently reimplements it for pinned units. The documented
warning is phrased as an **`.s`-file property** — *"key on `.fn`, never the address
column"* — so it **did not generalise**, and scope_map's version is harder to catch
because it emits a **confidence-1.0 attribution** rather than an obviously-synthetic
column. The trap should be restated as a property of **per-unit relative offsets in
multi-block units generally**, which covers both.

---

## 6. Behavioural stake

`XMemAlloc` is the XDK's allocator override, so every non-physical D3D/XUI
allocation routes through it. Temp = LastFit (top-down); plain `MemAlloc` = FirstFit
(bottom-up). If the bug is confirmed, **our source currently claims XDK allocations
live at the top of the heap when retail interleaves them into the front of main** —
long-lived allocations mixed with everything else. That makes it a live suspect for
the fragmentation divergence already noted at `FixedSizeAlloc::RawAlloc`, and it is
exactly the class of thing this effort exists to get right. `[PENDING]` verification
of the strategy plumbing.

---

## 6.5 ✅ LANDED — lane W0-XMEM (`4f5b0cac`), verified by the coordinator on main

Both bugs fixed **and** both rows named, in one lane. Commits: map
`64709708`/`2463c8d2`, source `0d0296ea`/`ecb4556e`.

**Coordinator's independent verification** (main rebuilt with a forced re-split
after the map edit, since the lane measured pre-rebase):

| row | before | after (measured on main) |
|---|---|---|
| `fn_822735B0` → **`XMemAlloc`** (204 B) | **unpaired, 0 / 0** | **fuzzy 89.37255 / mpn 89.76471** |
| `fn_827BAEC0` → **`?RawAlloc@FixedSizeAlloc@@MAAPAHH@Z`** (176 B) | **unpaired, 0 / 0** | **fuzzy 55.204544 / mpn 59.295456** |

Unit fuzzy: `default/Memory_Xbox` 20.90 → **25.938395**; `default/PoolAlloc`
31.27 → **36.198486**. All four figures reproduce the lane's to the last digit.

★ **A bonus cross-check the verification produced for free.** Whole-binary on main
reads **44,506 / 3,760,352 B / 36.435173%** against the lane's baseline of
44,505 / 3,760,224 / 36.433937% — a difference of **+1 function / +128 B**. That is
**not ours** (this lane measured Δ0 on all four legs); it is exactly the **+128 B
predicted exactly** claimed by lane **W28-UISRC**, which merged in between. Two
independent lanes' arithmetic closes to the byte.

**All four legs predicted 0/0/0 and measured 0/0/0**, `none` control flat on both
map legs. The MAPID-1-style negative did **not** occur, for a *checkable* reason
rather than luck: every caller of `fn_822735B0` is in the unpairable `xdk` no-source
class or inside a still-anonymous function, and `fn_827BAEC0` has **no code call
sites at all** (vtable-only) — there were no forgiven placeholder sites to convert.

### ★★★ The prediction that failed — and it is the most important line in this doc

The lane expected the callee fixes to raise the rows' fuzzy visibly. Fuzzy moved
**+0.098** and **+0.114** — while **`mpn` stayed IDENTICAL TO THE LAST DIGIT on
both** (89.764710 → 89.764710; 59.295456 → 59.295456).

> A wrong callee is a relocation-name **arg** penalty — precisely the class `mpn`
> excludes *by construction*. **Two genuine behavioural fixes moved the
> `matched_functions` ruler by literally zero**, reproduced independently on two
> functions.

⇒ concrete proof of the §0 premise: on this defect class the headline ruler is
**structurally incapable** of registering the correctness win. Land these on merit.

### Naming exposed the bug — and caught a tool restating its own input

The moment the row paired, objdiff charged the site `diff_arg bl [sym]`, **target
`?MemAlloc@@YAPAXHH@Z` vs base `?_MemAllocTemp@@YAPAXHH@Z`** — invisible before —
**and simultaneously labelled it `LINKER_MERGED … ICF (RarelyHandFixable)`**, i.e.
it called a callee we hold retail-byte proof is simply *wrong* an unfixable fold.
After the source fix the pattern block **disappears entirely**. A fresh live
instance of MPNGAP-1: **the `AT_LIMIT`/`LINKER_MERGED` label on a `diff_arg`-only
stratum restates its own input.**

### ⛔ Two corrections this lane made to THIS DOC

1. **§3.5 correction 2 was WRONG.** "The `#ifdef HX_NATIVE` split is optional" holds
   for the match build only. The 2-arg `MemAlloc(int,int)` exists **only** under
   `#ifndef HX_NATIVE`, so an unguarded `(MemAlloc)(size, align)` binds `align` to
   `const char *file` and **does not compile natively**. It is **mandatory** for a
   tree that must pass the native gate. The arm was kept and pointed at `MemAlloc`,
   so **native also stops using the temp heap** — deliberate and correct per retail.
2. **§3.6's "bonus divergence" is half wrong.** Our `AllocType` *does* have the
   Game/Middleware/Unknown default. Only the **base-0 vs base-0x80 dispatch** half
   of that divergence stands.

`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0
failed=0 rc=0` — and it relinked every target, so it genuinely exercised both
changed `HX_NATIVE` arms.

### Follow-on queue, in priority order

1. ✅ **DONE — `RawAlloc`'s inherited `printf` block** (lane W0-XMEM2). The scan
   reproduces with its control intact. ⛔ **But the queue's prediction — "very
   likely the only thing between that row and 100" — was WRONG, and usefully so.**
   Removing it took the row 55.204544 → 74.7 only; **two further divergences the
   queue did not know about** were needed:
   - **The pool is walked in INT UNITS, not bytes.** Retail emits `srawi`+`slwi` as
     a *non-folding pair* at two sites; MSVC folds the byte-wise `(size >> 2) << 2`
     into a single `clrrwi`, so retail cannot have written that. A non-folding
     `srawi`/`slwi` pair is the signature of **`int *` pointer arithmetic** — the
     `>> 2` is the source's, the `slwi 2` is the compiler's `sizeof(int)` scaling,
     emitted by a different pass so the two never combine.
   - ★ **The global ADDRESSING shape, which was the biggest single lever (74.7 →
     93.5).** Retail co-addresses `gBigHunk`(+0)/`gSmallHunk`(+4) through one
     materialized base register and hoists a **separate** `lis` for `sPoolBuf` and
     `sPoolEnd`; we did the exact mirror image, costing an extra `lis` and
     cascading through the whole register assignment. **Rule** (in-tree precedent:
     `MemTrackLogState` in `utl/MemTrack.cpp`, then confirmed here): MSVC
     co-addresses internal-linkage **statics** whose layout it chose itself, but
     gives each **external** symbol its own relocation ⇒ *a compile-time +4
     displacement between two globals implies **one aggregate***. So retail's
     `gBigHunk`/`gSmallHunk` are one aggregate and its `sPoolBuf`/`sPoolEnd` are
     **not** file statics.
   ⚠ **NEGATIVE RESULT — do not re-run it:** swapping the `sPoolBuf`/`sPoolEnd`
   **declaration order** *does* control `.bss` order (verified: the base register
   moved) but is **INERT** for the addressing choice — 74.7 before and after. The
   obvious lever was the wrong one; linkage and aggregation were the real ones.
   **Final: fuzzy 93.52273 / mpn 97.72727, A/B +0 fn / +0 B (predicted 0/0).**
   Residual is 180 vs 176 B and is now *purely* register allocation — a consistent
   permutation (retail r30=`sPoolBuf` base, r31=hunk base; ours inverted, with
   `sPoolEnd` in r29 on **both** sides) plus one trailing register move because
   retail holds `buf` in `r3` across the shared tail. **No stack-slot or offset
   diffs**, which is the case the docs call inert for declaration reordering, and
   the permuter is OFF by standing directive. **Stopped deliberately.**
2. ✅ **DONE — XMemAlloc reaches `fuzzy 100.0 / mpn 100.0`** (lane W0-XMEM2), A/B
   **+1 fn / +204 B, predicted +1/+204 exactly**, `none` control +204 (the correct
   shape for a source patch — a flat `none` would have meant the gain was bought
   with relocation names alone). Two fixes: `MemAllocFailed` reduced to its retail
   remnant (see the RESOLVED block in §4.5), and — **new, not in this queue** —
   **`MemTrackAlloc` takes SIX parameters, not eight**: both retail call sites
   (`fn_822735B0`, `fn_82273350`) set up `r3..r8` only, with no `r9`/`r10` and no
   stores to the outgoing argument area. The X360 EABI passes integer arguments 7
   and 8 in `r9`/`r10`, so their absence is **decisive rather than suggestive**,
   and two independent sites agree. `file`/`line` kept under `HX_NATIVE`. Zero
   metric risk: `0x827C43E0` is anonymous, so the callee name is a forgiven
   placeholder however we spell it.
   ⛔⛔ **TRAP, recorded because it produced a confident false `AT_LIMIT`:**
   `run_objdiff` reported **99.7%** and *"AtLimit (High) … these are
   linker/path-derived; **no source mutation can close them**"*, citing 3
   `ANONYMOUS_NAMESPACE_HASH` charges on `gPhysicalUsage` (`?A0x7a439e55` target vs
   `?A0x5be8b7be` base). **That was a PHANTOM.** The MCP builds a single `.obj`
   incrementally, which **SKIPS THE 6 OBJ PATCHERS** — and `obj_anon_ns` is one of
   them, i.e. *part of the ruler* (`CLAUDE.md` documents exactly this hazard). A
   full `ninja` + `report.json` read gives **100.0**. The tell was that the same
   instruction read **equal on both sides before the edits**, and the map does
   carry `0x82cbc63c → ?gPhysicalUsage@?A0x7a439e55@@3HA`. ⇒ **Never accept an
   anon-namespace-hash `at_limit` from an incremental single-obj diff.**
3. **Port `?MemAlloc@@YAPAXHH@Z`'s 644 B body** (§4.6 #1) — paired, unblocked, and
   today every allocation in our source ends at `malloc()`.
4. **`MemPrintOverview` signature → `TextStream&`** (3 sites) then name it.
5. **Fix the stale note at `MemMgr.cpp:416`** — still says *"Do NOT name 0x827bcd38
   first"*; events inverted that on 2026-08-16.

⚠ Minor, recorded: `?Refill@FixedSizeAlloc@@IAAXXZ` and
`?RawAlloc@ReclaimableAlloc@@…` are attributed to unit
`default/ConnectionStatusPanel`, not `default/PoolAlloc` — the cluster straddles two
pinned units. Both score; nothing broken.

---

## 7. Ledger

| # | item | state |
|---|---|---|
| 1 | Verify the wrong-callee bug on retail bytes | **DONE** — §3, CONFIRMED |
| 2 | Audit the tier 1–3 target list | **DONE** — §4 |
| 3 | Adjudicate the three data-quality snags | **DONE** — §5 |
| 4 | This doc | **DONE** |
| 5 | Dispatch implementation lane(s) | ✅ **LANDED** `4f5b0cac` — §6.5 |
| 6 | Name + pin the Memory_Xbox allocator rows | ✅ **LANDED** — both rows paired, §6.5 |
| 7 | **NEW: fix `tools/scope_map.py`'s pinned-unit address key** (§5.1) | ✅ **LANDED** `5dd5e4f0` — see §5.5 |
| 8 | **NEW: comment-only refresh of 5 stale TU0 addresses in `MemMgr.h`** (§5.3) | ready to dispatch — run the native gate LAST |
| 9 | **NEW: adjudicate `0x82709EE0`** — stale citation or ICF fold survivor? (§5.3) | open, low priority |

**Nothing is landed. No source, map, or splits file has been edited by this effort.**
