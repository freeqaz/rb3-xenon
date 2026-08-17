# Allocator audit — the XMemAlloc callee bug, and making the allocator auditable

**Opened 2026-08-17.** Coordinator doc for the handoff proposing (1) a wrong-callee
fix at `src/Memory_Xbox.cpp:287` and (2) a ranked list of allocator functions worth
decompiling.

> **⚠ STATUS: RESEARCH IN FLIGHT.** Sections marked `[PENDING]` are awaiting
> verification agents and must not be acted on. Sections marked **GROUNDED BY THE
> COORDINATOR** were verified directly against the tree at `0f20a01c` before any
> agent reported, and carry their evidence inline.

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

### 2.3 The hard prerequisite before any map edit `[PENDING]`

Standing project rule, and the reason this is not a rubber stamp: **objdiff pairs by
NAME. If the target row is renamed to a symbol our base obj cannot define, the row
reads 0% permanently, however correct our source is.** Un-pairing is 80.5% of a map
edit's measured delta.

So the map entry for `fn_822735B0` may only be written once we have, as evidence
rather than inference:

1. The **exact** symbol spelling our compiled `Memory_Xbox.obj` emits for
   `XMemAlloc`, read from the COFF symbol table — *not* guessed from the C++
   declaration. `XMemAlloc` is an XDK override and may be `extern "C"` /
   undecorated.
2. Confirmation that our base obj **does** define it today (and if not, what would
   have to change — an obj patcher, a linkage change, or `ForceLinkXMemFuncs`
   swallowing it).
3. `XMemFree` is the working template — same unit, named in
   `scripts/target_symbol_map.json`, scoring **100%**. Whatever rule spells it
   should spell `XMemAlloc`.

`[PENDING — agent 1]`

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

## 4. The ranked target list `[PENDING]`

Handoff's tiers, to be audited against current state, asm-extent sizes, and
pairability. Expect staleness given §1.

- **Tier 1** — `0x827BCA78` `MemHeap::Alloc` 652 B (claim: retail inlines `TryAlloc`
  *without* the `mBlock == nullptr` guard both decomps carry ⇒ derefs at `+0x14c`
  with `gMemLock` held — "that is the crash") · `0x827BCD38` `MemAlloc` 644 B (ours
  is a 20 B `malloc` stub; W0-ALLOC explicitly declined to port it) · `0x827BAEC0`
  `FixedSizeAlloc::RawAlloc` 176 B.
- **Tier 2** — the `Memory_Xbox.cpp` XDK boundary, all unpaired (see §2).
- **Tier 3** — `0x827BD300` `MemInit` 788 B · `0x827BC2D0` (garbled in the handoff)
  · `0x827BC838` `MemPrintOverview` 300 B.

`[PENDING — agent 2: state, true size, play type (identification / source /
body-port), audit value, blockers, priority]`

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

## 7. Ledger

| # | item | state |
|---|---|---|
| 1 | Verify the wrong-callee bug on retail bytes | in flight |
| 2 | Audit the tier 1–3 target list | in flight |
| 3 | Adjudicate the three data-quality snags | **DONE** — §5 |
| 4 | This doc | open |
| 5 | Dispatch implementation lane(s) | blocked on 1–2 |
| 6 | Name + pin the Memory_Xbox allocator rows | blocked on §2.3 |
| 7 | **NEW: fix `tools/scope_map.py`'s pinned-unit address key** (§5.1) | ready to dispatch — 11,122 rows, no metric exposure |
| 8 | **NEW: comment-only refresh of 5 stale TU0 addresses in `MemMgr.h`** (§5.3) | ready to dispatch — run the native gate LAST |
| 9 | **NEW: adjudicate `0x82709EE0`** — stale citation or ICF fold survivor? (§5.3) | open, low priority |

**Nothing is landed. No source, map, or splits file has been edited by this effort.**
