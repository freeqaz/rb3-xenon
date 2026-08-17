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

## 5. Data-quality snags `[PENDING]`

1. `0x822734E0` allegedly mis-pinned to `src/system/obj/Object.cpp` while being
   `AllocAlign`'s jump-table dispatcher. Handoff suspects ICF fold-survivor naming.
2. `MemMgr.s` address columns disagree with its `.fn` names. **Coordinator's prior:
   this is the DOCUMENTED synthetic-address-column behaviour for multi-block units**
   (`CLAUDE.md`, 2026-08-04) — dtk computes the column as `first_block_start +
   cumulative offset`. Expect KNOWN, not a new defect.
3. `MemMgr.h`'s cited addresses (`0x827977D0`, `fn_827979D8`) don't resolve; handoff
   suspects TU0-vs-TU5 staleness (the flip was 2026-07-15).

`[PENDING — agent 3: verdict per snag]`

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
| 3 | Adjudicate the three data-quality snags | in flight |
| 4 | This doc | open |
| 5 | Dispatch implementation lane(s) | blocked on 1–3 |
| 6 | Name + pin the Memory_Xbox allocator rows | blocked on §2.3 |

**Nothing is landed. No source, map, or splits file has been edited by this effort.**
