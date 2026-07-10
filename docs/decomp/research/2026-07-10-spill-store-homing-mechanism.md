# Spill-store count mechanism: address-taken writes vs EH temp homing (2026-07-10)

Two-agent Opus investigation extending `docs/decomp/MSVC_X360_REGALLOC.md` (the c2.dll
coloring instrumentation) from *which register* to *how many stack stores*. Trigger: the
project owner rejected an at_limit verdict on `FaderGroup::~FaderGroup` (0x826EF120,
97.5%, one extra `stw r27, 0x54(r31)`) — "it IS controllable, we just don't know how yet."

Specimen work: `/home/free/tmp/fg_dtor2_findings.md` (full S-count table) and minimal
repros at `/home/free/tmp/spillrepro/` (`cc.sh` production-flag one-off compiler,
`at1..at4`, `eh0..eh2`, `r1_base`). Both are scratch — the durable content is below.

## TL;DR — the three-way split for `stw rN, slot(r31)` count mismatches

The pattern doc's "Stack Spill Scheduling" bucket conflates three populations with
different fixability. Diagnose by how the slot is USED in the retail asm:

| Slot usage in retail | Mechanism | Fixable? | Lever |
|---|---|---|---|
| Address taken (`addi rX, r31, slot`) or reloaded (`lwz`) | Ordinary member store into an **address-taken (memory-resident) local** | **YES — source** | One `stw` per source write surviving DSE; writes separated by an **opaque call** cannot fold (escaped address). Add/remove a write on the far side of the call; loop-invariant words hoist if assigned once before the loop |
| Write-only, never reloaded, funclet doesn't read it, function has EH, value is an inlined container node/iterator | **EH-conservative homing** of an EH-tracked temp across a throwing call | **PARTIALLY** — S is controllable but quantized (see below) | Body shape sets the number of node-materializing expressions; each independent materialization → one homed temp. Shared single iterator → non-homed temp (S=0) |
| Write-only, no EH involvement, high register pressure (e.g. `ObjPtrVec<T>::sort` spilling `size()`) | Register-pressure defensive spill | **NO** | Genuine `MSVC_X360_REGALLOC.md` territory — don't chase |

## Case 1 mechanism (proven on minimal pairs, S=1→S=2 with a one-line diff)

For a memory-resident local (address escapes into a call), c2.dll emits **one `stw` per
source-level write that survives dead-store elimination**. Two writes to the same field:

- separated by an **opaque call** → **2 stores** (aliasing forbids folding across the
  escape) — `at2_two.cpp`
- adjacent, or separated by pure arithmetic → **1 store** (DSE folds) — `at3/at4`
- loop-invariant word assigned once before the loop → store **hoisted** to preheader

This is a front-end/DSE/aliasing effect fully visible in `/FAs` — no c2.dll
instrumentation needed. Directly actionable when a near-miss shows an extra/missing
store to an address-taken local: match the *write count and placement* of the retail source.

## Case 2 mechanism (the FaderGroup specimen — S is bimodal, retail's S=1 unreachable)

`~FaderGroup` retail: `0x50` = `this`, address-taken (passed as `FaderGroup* const&` to
the inlined `_Rb_tree::erase_unique`), hoisted — case 1, both sides agree. `0x54` = the
inlined `ObjPtrList` **node** pointer, **write-only**, stored once immediately before the
throwing `bl Unlink`, never read again (funclet reads the register save area, not the
slot). EH forensics: retail `except_record` maxState=1, single unwind entry — **identical
EH frame to ours**; the hidden-dtor-temporary hypothesis is dead.

S-count = number of distinct **EH-tracked node locals** MSVC materializes, swept over 10
body shapes (table in the findings file):

- ≥2 independent node-materializing expressions (`front()`, `pop_front()`,
  `erase(begin())`, `*begin()` — any pairing) → **S=2** (both temps homed; our /FAs shows
  `$T49020` + `$T49053` sharing slot 0x54)
- one shared iterator serving both access and erase (`it=begin(); *it; erase(it)`, incl.
  the `it=erase(it)` return-flows-back form) → **S=0** (node collapses to a tight
  non-homed r27 temp)
- **S=1 never occurs.** Retail value-CSEs the two `mFaders.mNodes` reads into ONE
  EH-tracked local and homes it once — a value-CSE + EH-liveness combination our
  thin-node `ObjPtr_p.h`/`obj/Object.h` inlining does not reproduce.

A sanctioned per-TU-gated header prototype (`/DRB3_FADERGROUP_DTOR_COALESCE`, alternate
`pop_front` inlining `Node* node=mNodes; Unlink(node); delete node;`) compiled cleanly
(no PCH conflict in the synth dir) and produced **byte-identical /FAs** — the extra temp
is not the erase-iterator wrapper; both phrasings normalize to the same IR.

Key cross-check: **DC3's Faders.cpp is 100% with an IDENTICAL dtor body** — but DC3 uses
the polymorphic `ObjRefConcrete` node (`mObject@0xc`) while RB3 retail uses the thin node
(`mObject@0`, 12 B, out-of-line `Unlink`). Same source, different header, different temp
count: the variable lives entirely in the shared thin-node header phrasing.

## Direction-inversion lesson (tooling)

The original diff read ("retail has the extra store") was **backwards** — retail is the
40-instruction lean side; we emit 41. Cross-check objdiff target/base orientation against
the dtk asm (`build/45410914/asm/<unit>.s`, anonymous `fn_` callees = target) and the
`/FAs` listing (named temps = ours) before reasoning about a one-instruction delta.

## Status / remaining lever

- `~FaderGroup` pinned at **97.5%** (fuzzy credit; `report_result` at_limit in decomp.db).
- Only remaining route to strict-100: an **ungated** shared-header rephrasing of the
  thin-node `front()`/`Obj()`/`iterator`/`Node` to induce retail's value-CSE-then-home.
  Fleet-wide blast radius (every ObjPtrList user, incl. existing 100% TUs like
  MasterAudio), search not derivable a priori, payoff = 1 dead store on this fn — parked
  unless the same S=2-vs-S=1 signature shows up across many near-misses, in which case a
  header-trial campaign amortizes. A scan for that signature (extra write-only
  `stw` to an EH-home slot as the sole diff) is the natural sizing step.
- `docs/decomp/patterns/unfixable-compiler.md` "Stack Spill Scheduling" should be split
  per the three-way table above (file had concurrent in-flight edits at the time of this
  writing — fold in later, referencing this doc).
