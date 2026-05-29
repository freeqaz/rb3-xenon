# Instrumentation obj-patcher experiment — Phase 1 verdict

**Date:** 2026-05-29. **Worktree:** `.claude/worktrees/instr-patcher` (isolated, off HEAD 92a193f; not committed).
**Build log:** `/tmp/instr.log`. **Map backup:** `/tmp/instr_map_backup.json`.

## TL;DR — the verdict

**NEGATIVE for a useful patcher. The retail "instrumentation" bodies are not a
prologue wrapped around our real accessor — they are *complete replacement
stubs* that carry NO function body and NO return value at all.** The gap on these
42 paired accessors is therefore **not** "missing instrumentation prologue + same
body"; it is "retail discarded the entire body and emitted a uniform breadcrumb
stub." An obj-patcher that injects the bit-poke prologue and *preserves* our
`li r3,N; blr` would still mismatch, because the target has **no `li r3` and no
return logic whatsoever** — it ends `addi r1,r1,0x60; blr` with r3 left
undefined. Matching the target would require *deleting the return statement* from
honest source, i.e. forging code that doesn't return its declared type.

**Recommendation: DEAD END as a code-from-source matching lever.** Do not build
the patcher. These functions cannot be matched by any legitimate compile of the
real C++ source; they are an artifact of a retail instrumentation/coverage build
pass that substituted bodies. (A *byte-forging* obj-transplant could fabricate
them, but that is cosmetic — it produces "matched" functions whose source is a
lie, and it does not generalize to the real-bodied game functions, which is where
the actual decomp value is.)

## What was done

1. **Pairing (Phase-1 step 1) — reproduced and confirmed.** Ran
   `tools/gen_game_target_map.py --area meta_band --purity 0.70 --apply`
   (oracle `unified_id_rb3wii.json`, spans `/tmp/candidate_spans.json`),
   merging **+59 game entries** (9438 → 9497). `touch config.yml` + rebuild.
   The test units now report real fuzzy (pairing works):
   UIEvent 42.5%, Accomplishment 26.5%, AccomplishmentCategory 33.3%,
   AccomplishmentPlayerConditional 30.4%, AccomplishmentSetlist 22.9%,
   AccomplishmentSongFilterConditional 21.0%. **matched_functions delta = +0.**

2. **Decisive per-function disassembly (Phase-1 step 2).** Compared the dtk-SPLIT
   **target** (`build/45410914/asm/{UIEvent,Accomplishment}.s`) against our
   compiled **base** (`scripts/analysis/diff_inspect.py --compare-asm --symbol …`)
   for all 12 UIEvent + 30 Accomplishment paired accessors (42 functions, 2 TUs,
   2 distinct blobs `lbl_82DA0017` and `lbl_82C90838`).

## The target body is identical and bodyless across all 42 functions

Every paired accessor's **entire** retail body is exactly 8 instructions / 32 bytes:

```
stwu   r1, -0x60(r1)          ; alloc 0x60 frame (no reg saves)
lis    r11, <blob>@ha
lwz    r11, <blob>@l(r11)
rlwinm r11, r11, 0, B, B-2    ; clear ONE unique bit (or clrlwi/clrrwi at word edges)
lis    r10, <blob>@ha
stw    r11, <blob>@l(r10)
addi   r1, r1, 0x60           ; dealloc
blr
```

**There is no `li r3`, no `mr r3`, no `lwz r3`, no `bl`, no argument handling.**
Verified mechanically: `r3 referenced? = False` for **42/42** functions.

This is fatal because the paired functions have non-void, non-trivial signatures:

| target fn | demangled | declared return | target sets r3? |
|---|---|---|---|
| 0x825519DC | `TransitionEvent::AllowsOverride() const` | `bool` | **no** |
| 0x8243A18C | `Accomplishment::GetType() const` | `AccomplishmentType` (enum) | **no** |
| 0x8243A1AC | `Accomplishment::GetName() const` | `Symbol` | **no** |
| 0x8243A26C | `Accomplishment::GetSecretPrereqs() const` | `const vector<Symbol>&` | **no** |
| 0x8243A2AC | `Accomplishment::GetDynamicPrereqsNumSongs() const` | `int` | **no** |
| 0x8243A30C | `Accomplishment::IsFulfilled(BandProfile*)` | `bool` (+ ptr arg) | **no** |

A function that *declares* it returns `const vector<Symbol>&` or `Symbol` but
never writes r3 (nor the sret pointer) is not a real implementation. The retail
body has been wholesale replaced by the breadcrumb stub.

### Contrast: real functions are interleaved right next to the stubs

`fn_8243A40C` (pdata flag `0x40000A04`, NOT in the paired stub set) sits between
the stubs and is a genuine function — it saves LR, sets up r3, calls
`bl lbl_822605C0`, restores, returns. So the 0x20 stubs are **not** a dtk
mis-split (the jeff `.fn/.endfn` mis-nest issue): the real bodies exist and have
full prologues; the stubs genuinely have none. dtk attributed boundaries
correctly.

### Our base is the *correct* compile of the real source

rb3-Wii / our source: `bool AllowsOverride() const { return false; }`,
`bool IsTransitionEvent() const { return true; }`, etc. Our `/O1` build emits the
honest, correct `li r3, 0/1; blr`. objdiff's per-function compare for
`TransitionEvent::AllowsOverride`:

```
-  | stwu   r1, -0x60, r1        | ---
-  | lis    r11, lbl_82DA0017    | ---
-  | lwz    r11, lbl_82DA0017,r11| ---
-  | rlwinm r11, r11, 0, 21, 19  | ---
-  | lis    r10, lbl_82DA0017    | ---
-  | stw    r11, lbl_82DA0017,r10| ---
X  | addi   r1, r1, 0x60         | li  r3, 0x0     <- our return value
=  | blr                         | blr
```

The only shared instruction is the trailing `blr` (→ 12.5%/17.5% fuzzy). Our side
*correctly returns the value*; the target *throws the value away* and pokes a bit.

## The bit-assignment rule (characterized, for the record)

Consecutive functions march the cleared bit **down by one** per function:
function index `i` clears PPC bit `base_bit - i` (MSB=bit 0). The `rlwinm
r11,r11,0,MB,ME` uses the single-bit-clear wrap form `MB = (clearbit)+1`,
`ME = (clearbit)-1`; word edges use `clrlwi`/`clrrwi`.

- **UIEvent** (`lbl_82DA0017+0x2CA05`, one 32-bit word): fn#0..#11 clear bits
  20,19,18,…,9. (Decode confirmed: MB/ME `(21,19)`→bit 20, `(20,18)`→bit 19, …)
- **Accomplishment** spans **two** words: fns 0..4 poke offset `+0x1218`
  (bits 26..30 region), then fns 5..29 poke offset `+0x1194` (132 bytes lower),
  bits marching 1,0,31,30,…,6. So the assignment is grouped, not a single
  contiguous run — the blob is a per-feature/per-class coverage bitmap, with each
  function owning exactly one bit.

The rule is *derivable* (a patcher could reproduce it), but that is moot given the
verdict — the prologue is not the gap.

## Why Phase 2 was not built

The brief's Phase-2 premise ("rewrite the 8-byte `li r3,N; blr` body to the
~32-byte instrumented form: inject the bit-poke prologue + **preserve** the
original `li r3,N; blr`") does not describe the target. The target keeps **no**
`li r3,N`. A patcher that preserves our return load produces:

```
stwu; <poke>; addi; li r3,N; blr      (our hypothetical patched body, 9 instr)
```

but the target is:

```
stwu; <poke>; addi; blr               (8 instr, NO r3)
```

— still a guaranteed mismatch (extra `li r3` instruction; different size). To
byte-match we would have to emit a body with **no return value**, which means
either (a) compiling from deliberately-wrong source (`AllowsOverride` returns
nothing), or (b) a raw byte-transplant patcher that fabricates the stub
independent of source. (a) corrupts the source tree; (b) is a cosmetic forgery
that doesn't generalize and adds no engineering value. Neither is worth wiring.

## Scope / generalization estimate

The RB3-Wii oracle most confidently identifies exactly this class — trivial
one-line accessors (`Get*`, `Is*`, `Allows*`, `Has*`) — because their names are
recoverable. These are precisely the functions retail stubbed out. Across the
located meta_band TUs the oracle named ~354 such functions in 9 TUs; the subset
that are pure-stub accessors (the patcher's only candidates) is on the order of
**~40–150 functions** — and **none of them are matchable from real source.** The
real-bodied game functions (the `noObj` entries and larger spans like
`fn_8243A40C`) are untouched by this stubbing and remain a normal per-function
source-fidelity grind.

## Conclusion / recommendation

- **Phase-1 verdict: bodies do not merely differ — they are absent.** The retail
  accessors are uniform replacement stubs (bit-poke, no return), an artifact of a
  retail coverage/instrumentation build pass. This *supersedes* the earlier
  "instrumentation prologue inlined into the accessor" model: there is no body
  underneath the prologue.
- **The instrumentation obj-patcher is a DEAD END for legitimate matching.** It
  cannot flip any of these to 100% without forging source that doesn't return its
  declared type. **# functions that flip with a legitimate patcher: 0.**
- **Where the game-code value actually is:** the real-bodied functions
  (interleaved between stubs, and the `noObj` oracle entries). Those are matchable
  by the normal source-fidelity route. The pairing infrastructure
  (`gen_game_target_map.py`) remains correct and useful — it lets these real
  functions be *measured*; it's just that the headline accessor surface the oracle
  loves is a mirage (stubbed in retail, un-matchable).
- **If anyone still wants the cosmetic count:** a raw byte-transplant patcher
  (synthesize the 32-byte stub + 3 relocations to the blob global, derive the bit
  from the marching rule, resize the section) is mechanically feasible and would
  register ~40–150 "matches" — but they are byte-forgeries, not decompiled source,
  and are explicitly not recommended.

## Artifacts (in this worktree, left intact)

- `scripts/target_symbol_map.json` — +59 game pairing entries merged.
- `config/45410914/splits.txt` — UIEvent/Accomplishment etc. pinned (pre-existing).
- `build/45410914/{asm,report.json}` — post-build target asm + report.
- `/tmp/instr.log` (build), `/tmp/instr_map_backup.json` (pristine map backup).
- No patcher script written (Phase-1 negative).
</content>
</invoke>
