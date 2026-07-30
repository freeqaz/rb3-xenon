# jeff pdata/boundary defects — round 3 design

> **STATUS (2026-07-30): SUPERSEDED — do NOT implement.** A 3-agent Fable
> confirmation pass (reviews at `~/tmp/jeff-r3-review/class{1,2,3}.md`) found all
> three classes are already resolved, would corrupt, or yield ≈0 strict under the
> honest-floor pricing rule (`Δ(matched − masked_equal_functions)`). The original
> 2026-07-12 design text is kept below the line for history.
>
> - **Class 2 (pdata over-split merge): DONE.** Implemented as jeff `7e49a38`
>   `merge_fallthrough_leaf_fragments` (symbol-layer inverse of a670a12, guards P4
>   pdata-sacred / P5 same-split-unit / P6 `JEFF_MERGE_PROTECT`). `Stats::AddRoll`
>   closed at 100% — the real fragments were `fn_826976E0`/`fn_826976F0`, NOT the
>   doc's `826791B0`/`C0` (doc addresses were wrong). ~1,414 merge-groups / 1,727
>   fragments absorbed; moved the honest floor (not masked_equal churn). Residual: none.
> - **Class 1 (truncated-fragment repair): DROP.** Only the census landed
>   (`fc5d2af`, env-gated `JEFF_CLASS1_CENSUS=1`); the extend/merge/demote repair was
>   never built and must NOT be — of 959 terminatorless spans, 522 (54%) are COMPLETE
>   self-pdata noreturn-`bl` functions that extending would corrupt. All 4 doc fixtures
>   already flipped to 100% via grow/clamp + Class-2/4 (MeasureMap `fn_827AB6F0` is now
>   strict 100) with no Class-1 pass. Payoff ≈0 (funclet-class PASS-3 byte-fallback,
>   Δ(matched−masked_equal)≈0). Confirms the standing "Class 1 & 3 NO-GO".
> - **Class 3 (except_data seed-time suppression): DROP as a match lever / DEFER as
>   cosmetic.** The seed-time gap is real in code (`util/xex.rs` L1104-1165 ungated;
>   genuine-EH gate applied only at write time, L1356), BUT the only priced fixture
>   `ProfileMgr::SetSynapseEnabled` (0x825475B8) is ALREADY 100% (one 16B COMDAT — the
>   write-time gate already skips its spurious tail except_data; the doc's "4B/12B
>   split / at_limit" was pre-TU5-flip stale). Real class size ~300 stale TU0-rebase
>   `Object` symbols (not 18k — that was a TU0-era figure), all match-inert; a cold
>   re-split sheds most for free. Payoff ≈+0 strict.
>
> **Net: nothing here is worth building for matched-function gains.** The doc's own
> priority section (below) is also stale — it ranks "struct-layout recon" #1 citing
> GemPlayer/BandProfile, but GemPlayer is effectively closed and BandProfile is a
> 32KB-class archaeology dig (see `hard-targets-triage-2026-07-12.md`). Real next
> levers live on the current frontier — map-defect channels, homing-pool 'op'
> evidence class, residue 75-90% band — see the campaign/measurement memory hubs.
>
> ---
>
> *Original 2026-07-12 design (historical):*

Design doc for a third jeff (local dtk fork, `/home/free/code/milohax/jeff`)
boundary-tooling session, covering the three residual function-boundary defect
classes surfaced by waves 36–40. Rounds 1–2 landed as jeff `a670a12`
(PDATA-less leaf synthesis) and `c8b21dd` (merge_tail_blocks boundary respect);
this doc scopes what those passes deliberately did NOT touch.

Key source file: `/home/free/code/milohax/jeff/src/cmd/xex.rs` (split-time
symbol repair pipeline, `split_write_obj_exe` ~L1171-1178:
`grow_undersized_function_symbols` → `clamp_oversized_function_symbols` →
`synthesize_reloc_targeted_leaf_functions` → `prune_overlapping_phantom_functions`).
Supporting code: `.pdata` seeding in `src/util/xex.rs` ~L1067-1140
(word1 = `prolog_len(8) | func_len_insts(22) | func_type(2)`; `func_type==3` ⇒
8-byte except_data struct immediately before the function start),
`merge_tail_blocks`/`check_tail_block` in `src/analysis/cfa.rs` ~L973-1170,
`genuine_except_data_set` + `.pdata` reconstruction in `src/util/xex.rs`
~L1328-1480.

Evidence sources: memory topic files `project_wave37_2026-07-12.md`,
`project_wave38_2026-07-12.md`, `project_wave39_2026-07-12.md`,
`project_wave40_2026-07-12.md`, `project_jeff_fork.md`,
`project_jeff_asm_misnest.md` (all under
`~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/`).

---

## Problem statement

Three distinct boundary-defect classes remain after the round-1/2 fixes. All
three produce target `.obj` COMDATs whose byte spans do not correspond to a
real compilable function, so objdiff can never pair them against our compiled
bodies — they are structurally unmatched regardless of source quality, and each
one also poisons the identification surface around it.

### Class 1 — Truncated-fragment pins (symbol span has no terminator)

A persisted function symbol's `[addr, addr+size)` contains **no terminating
instruction** (`blr`/`bctr`/unconditional `b`): the symbol is a *piece* of a
larger unidentified function whose real tail lives in the next symbol (or in
unclaimed gap bytes).

- **Scale (wave-37 classifier):** 77/279 = **28%** of net-new game/engine
  leaves in the wave-36 corpus were `TRUNCATED_FRAGMENT` shape — "genuinely
  bogus dtk split boundaries (no terminator in claimed range; real tail in
  next symbol) — SKIP-class."
- **Concrete cases (wave-40, all currently at_limit):**
  - `DrivenPropertyEntry` @ `0x8275CAB0`
  - `Metronome` @ `0x826D1F88`
  - `AccomplishmentProgress` @ `0x82565EE8`
  - `MeasureMap` @ `0x827AB6F0`
  - Related label≠body mismatches: `BufStream` @ `0x827A7104` (true start
    `0x827A70C0`), `CheatProvider` @ `0x823E1AF0` (true `0x823E1AC0`).
- **Authoritative signals (wave-39 methodology lessons):**
  - dtk's own **"ends within symbol" compile error is the authoritative
    boundary-fix signal** — use it as the PRIMARY oracle, not a fallback.
  - dtk report.json (pdata-derived) beats Ghidra wherever they disagree.
  - **Ghidra trap:** decompile-at-address SNAPS to the enclosing function —
    55% (81/146) of Ghidra-claimed leaf starts were false. The rigorous test is
    comparing the decompiled `FUN_XXXXXXXX` *signature address* against the
    claimed address; mismatch = not a real function start.

### Class 2 — AddRoll-class `.pdata` over-splits (one body, 2+ pdata entries)

Retail `.pdata` itself sometimes splits ONE contiguous compiled function into
2+ consecutive entries. Since a670a12's leaf-synthesis guard treats the pdata
partition as authoritative-and-function-granular ("a pdata-anchored function
never contains a separate function", verified non-nesting across all 56,836
RB3 entries), jeff currently emits both fragments as separate function symbols
and nothing downstream can reunite them.

- **Concrete case (wave-38, currently at_limit):** `Stats::AddRoll`'s 36-byte
  body → retail `.pdata` emits `fn_826791B0` (16B) + `fn_826791C0` (20B,
  **zero xrefs**); our compile emits one contiguous function. objdiff pairs
  our 36B fn against the 16B fragment → permanent mismatch.
- **The design question:** can jeff detect and MERGE these `.pdata`-induced
  over-splits? Note this is the **inverse** of both landed fixes: a670a12/
  c8b21dd *synthesize missing* function starts and *protect* split boundaries;
  this class needs to *remove* a pdata-derived function start. It is also
  distinct from `merge_tail_blocks` (cfa.rs), which merges *gap* blocks after
  a pdata end — here the second fragment IS a pdata entry, not a gap.
- **Scale:** one confirmed case; family size unknown. A cheap census is part
  of the plan (see below) — the detection predicate is fully computable from
  the pdata table + reloc set, so sizing costs one scan.

### Class 3 — `except_data` / EH COMDAT splits interrupting one logical function

`func_type==3` pdata records place an 8-byte `except_data_<addr>` struct
immediately before the function start (`src/util/xex.rs` ~L1108-1140). RB3's
retail `.pdata` contains **stray func_type==3 records pointing mid-function**
(~18,098 of ~27,160 except_data symbols sit on live instructions — established
by the b1bc97c investigation, `project_jeff_fork.md`). b1bc97c fixed the
*byte corruption* (spurious except_data no longer zeroed/EH-reloc'd in
`write_coff`, gated by `genuine_except_data_set`), but the stray record still
does structural damage at **seed time** in `src/util/xex.rs`:

1. The stray entry's start address is inserted into `obj.known_functions` /
   `obj.pdata_funcs` → a bogus *pdata-anchored* function start mid-body, which
   every downstream repair pass then treats as untouchable ground truth.
2. The `except_data_<addr>` Object symbol lands inside live code and acts as a
   split point when COMDATs are carved.

- **Concrete case (wave-40, currently at_limit):**
  `ProfileMgr::SetSynapseEnabled` — dtk splits the retail body into **4B/12B
  COMDATs around `except_data`**. "except_data straddles at pin boundaries"
  is a recurring noise line item across waves (wave-6 calibrated 9,037/18,097
  `except_data` symbols.txt entries as spurious, `project_wave6_2026-07-10.md`).
- **Scale:** ~18k spurious except_data symbols exist binary-wide; the subset
  that actually *fragments a function we care about* is unknown — census
  needed. Even where it doesn't fragment, it inflates symbols.txt noise and
  pin-boundary friction.

### Fleet-wide value framing

Class 1 alone gated 4 of wave-40's 15 micro-port candidates and disqualified
28% of the wave-37 leaf corpus before any porting effort was spent. Classes
2–3 are individually small but are *permanent* at_limit walls — no amount of
source-side work can close them; only jeff can. All three also degrade the
identification surface (a truncated fragment fingerprints as garbage; a
fragmented body never byte-matches its oracle sibling).

---

## Prior art (rounds 1–2, landed)

- **`a670a12` — `synthesize_reloc_targeted_leaf_functions`** (xex.rs L584-985):
  post-CFA pass that promotes/synthesizes PDATA-less leaf functions CFA
  absorbed into an oversized non-pdata parent or left as bare `lbl_` labels.
  Guards: reloc proof-of-entry (non-jumptable, non-EH source), `T-4` hard flow
  terminator (`is_hard_flow_terminator`, L569-582), word-at-T decodable,
  **pdata partition authoritative — never splits a pdata-anchored function**.
  Idempotency was the central design constraint: the first iteration
  restricted parent-splitting to DATA/vtable-sourced candidates precisely so a
  re-split (which re-runs CFA and would otherwise re-merge `bl`-targeted tails)
  converged to byte-stable symbols.txt; the `_once`/fixed-point wrapper
  (L957-985) makes a single split emit the fully-converged set. Validated
  +16 strict, 0 losses.
- **`c8b21dd` — merge_tail_blocks boundary respect** (cfa.rs + xex.rs):
  `merge_tail_blocks` no longer re-merges across a persisted split boundary
  when symbols.txt already declares both sides as separate functions — which
  is what allowed relaxing the leaf pass to `bl`-referenced (code-sourced)
  candidates. Validated +62 strict, 0 losses, symbols.txt byte-stable across
  up to 3 re-splits from a warm build dir.

**How round 3 relates:** rounds 1–2 taught jeff to *create* function starts
that pdata lacks, holding the pdata table itself sacred. Round 3 is the
complement and, for classes 2–3, the **inverse direction**: the pdata table
is itself wrong in two known ways (over-split entries; stray mid-function
func_type==3 records), so jeff must learn to *merge/suppress* pdata-derived
boundaries under tightly-proven conditions. Class 1 is a *repair* pass in the
same spirit as `grow_undersized_function_symbols` (commit `39e482f`), extended
to non-pdata symbols. All three passes slot into the existing repair pipeline
in `split_write_obj_exe` and reuse its audit-logging + persisted-symbols.txt
conventions.

---

## Proposed approach per class

### Class 1 — terminator-completeness repair pass (`repair_terminatorless_functions`)

**Data sources:** the persisted/CFA symbol set; `.text` bytes
(`is_hard_flow_terminator` semantics, xex.rs L569); `obj.pdata_funcs` and the
reloc-target set (both already computed for the leaf pass).

**Detection:** after `grow`/`clamp` and before (or interleaved with) leaf
synthesis, scan every `ObjSymbolKind::Function` in code sections: decode the
span; if **no instruction in `[addr, addr+size)` is a hard flow terminator**
(and the span doesn't end at a jumptable/EH continuation — reuse the leaf
pass's `excluded_source_ranges`), the symbol is a truncated fragment. This is
exactly the wave-37 classifier's `TRUNCATED_FRAGMENT` predicate
(`scripts/grind/classify_funclets.py`), moved into jeff where it can act.

**Repair, in preference order:**
1. **Extend** to the next hard flow terminator IF the intervening bytes and the
   terminator are not claimed by (a) a pdata-anchored start or (b) a
   reloc-proven entry (leaf-pass guard 1). This mirrors
   `grow_undersized_function_symbols` but uses the terminator scan rather than
   pdata length as the authority (these symbols have no pdata entry by
   construction — a pdata-anchored symbol already gets grown to its pdata
   length).
2. **Merge forward** if extension would swallow the *next symbol entirely* and
   that next symbol is itself un-referenced (zero incoming relocs, not
   pdata-anchored, not in the persisted-boundary set c8b21dd protects) — i.e.
   the "next symbol" is really this function's tail. Emit as one symbol; log
   the absorbed name for audit.
3. **Demote** (rename `__FRAGMENT_<name>`, kind Unknown — the
   `prune_overlapping_phantom_functions` convention, xex.rs L987) when neither
   applies, so the fragment at least stops rendering as a pairable function
   and the wave classifiers can skip it without re-deriving the predicate.

**Idempotency/safety:** option 1/2 only ever *lengthen or merge* non-pdata
symbols, monotone like the leaf pass, so a fixed-point wrapper converges the
same way `synthesize_reloc_targeted_leaf_functions` does. The c8b21dd
persisted-boundary guard must treat a round-3 merge the same as a round-2
split: once symbols.txt records the merged extent, a re-split's CFA must not
re-shorten it (this falls out of `Symbols::add` keeping existing sizes, but
must be A/B-proven over 3 re-splits, per the wave-36 caveat). The 4 wave-40
addresses above are the acceptance fixtures; dtk's "ends within symbol" error
count on the full build is the regression gate (it should only decrease).

### Class 2 — pdata over-split merge pass (`merge_pdata_fallthrough_fragments`)

**Data sources:** the raw `.pdata` entry table (start VA + `func_len` per
entry, parsed at `src/util/xex.rs` L1076-1105); the module-wide reloc-target
set; `.text` bytes.

**Detection heuristic** — consecutive pdata entries `E1`, `E2` are fragments of
one function IFF all of:
1. **Exact adjacency:** `E1.start + E1.func_len*4 == E2.start` (no gap, no
   padding word).
2. **Fall-through:** the last instruction of `E1` is **not** a hard flow
   terminator — control flows off the end of `E1` into `E2`. (For AddRoll,
   the 16B fragment falls through / conditionally branches into the 20B one.)
3. **No independent entry proof for `E2`:** `E2.start` has **zero incoming
   relocations** module-wide (no `bl`, no vtable slot, no data pointer, no
   jumptable — AddRoll's `fn_826791C0` has zero xrefs) and is not a persisted
   named symbol in symbols.txt.
4. Neither entry is func_type==3-anchored EH-special (avoid interacting with
   class 3 until both land; a combined-pass ordering note below).

**Action:** synthesize ONE function symbol spanning `[E1.start, E2.end)`;
record the merge (audit log `Merging pdata fragments ...`). Do NOT rewrite the
original `.pdata` *section bytes* — the reconstruction path
(`src/util/xex.rs` L1403-1480) derives the emitted target `.pdata` from the
symbol table, so the merged symbol naturally emits one entry; verify objdiff
scoring is `.text`-driven for these units (it is — `.pdata` is a separate
section diff) and note the target/base `.pdata` entry-count delta as expected
noise.

**Critical invariant update:** a670a12's guard 5 doc-comment asserts the pdata
partition is function-granular. Class 2 *refines* that claim: pdata is a clean
non-overlapping **partition** (still true — the merge preserves it), but not
every entry is a whole function. The leaf pass's "never split a pdata-anchored
function" guard is unaffected (we merge entries, never split them), but the
doc-comment and the `pdata_anchored` set construction must be updated so a
merged-away `E2.start` no longer counts as an anchored start on re-split —
i.e. the merge must run (or be re-derived deterministically) **before** the
leaf pass consumes `pdata_anchored`, every split, so the pass pipeline stays
order-deterministic and symbols.txt byte-stable.

**Sizing census first (cheap, do before implementing):** a standalone scan of
predicates 1–3 over all 56,836 entries tells us the family size for ~an hour
of work. If it's single-digit, consider whether a symbols.txt hand-merge +
c8b21dd's persistence guard already suffices (i.e. is the *pass* even needed,
or just a one-time repair + the existing guard?). Design decision deferred to
the census result.

### Class 3 — stray func_type==3 suppression at seed time

**Data sources:** `.pdata` `func_type` field (`word >> 30`,
`src/util/xex.rs` L1087); the genuine-EH criterion from
`genuine_except_data_set` (word1 of the 8-byte struct must resolve to a
code-section handler VA — empirically always `__CxxFrameHandler` @
`0x82804210`, per b1bc97c).

**Detection:** at pdata seed time (util/xex.rs L1109 `if func_type == 3`),
apply the genuine-EH test *immediately* (the full module is loaded at this
point, so the cross-unit handler VA resolves — same argument as
`genuine_except_data_set`'s placement). If the 8-byte struct before
`start_addr` does NOT decode as genuine EH:
1. **Do not** add the `except_data_<addr>` / `except_record_<addr>` symbols.
2. **Do not** seed `start_addr` into `known_functions`/`pdata_funcs` as an
   anchored start *if* it lies strictly inside another pdata entry's span or
   fails the leaf pass's entry proof (T-4 terminator + reloc reference) —
   this is what un-fragments `SetSynapseEnabled`'s 4B/12B carve.
3. Also skip the handler/record `known_functions` insertions (L1127-1140) for
   spurious records — those currently seed function starts from instruction
   bytes misread as addresses.

This moves b1bc97c's write-time classification to seed time, killing the
structural damage rather than just the byte corruption. `write_coff`'s
existing `genuine_except_data` gate stays as a belt-and-suspenders check
(symbols.txt caches from older splits may still carry stray symbols; the
existing skip path handles them).

**Interaction ordering:** class 3 runs earliest (seed time), shrinking the
input to classes 1–2; class 2 runs next (pdata-table-level); class 1 runs in
the symbol-repair pipeline where grow/clamp already live. Land and A/B each
class **separately** — a combined landing makes a matched-set regression
unattributable.

### Staging discipline (applies to all three — non-negotiable)

- **Build in a COPY, never in place:** clone/rsync jeff to
  `~/tmp/jeff-round3/` and `cargo build --release` THERE. The wave-34
  incident (`project_wave34_2026-07-11.md`): a concurrent in-place
  `cargo build --release` to the configure.py-referenced path
  (`../jeff/target/release/dtk`) corrupted main's symbols.txt mid-wave.
  Every rb3-xenon build (main + all worktrees) invokes that binary; a
  mid-flight swap or a regressed binary silently corrupts split output
  fleet-wide.
- **Gated swap:** A/B-validate the staged binary in a dedicated worktree
  (`scripts/setup_worktree.sh ~/tmp/wt-jeff-r3 jeff-r3`), then atomically swap
  `../jeff/target/release/dtk` (backup the old binary first) during a
  quiescent window — mirroring the wibo staging discipline in CLAUDE.md and
  the wave-34/36 "gated swap" precedent.
- **Commit jeff source in the fork immediately after the swap** — both landed
  commits' messages note the binary was installed before the commit existed;
  the commit is what "prevents a future cargo build from silently reverting
  it." Same here.

---

## Validation plan

Per class, in an isolated worktree with a warm build dir:

1. **Full matched-SET diff, never raw count.** Extract the set of
   `match_percent_normalized == 100` function names from `report.json` before
   and after; diff the SETS. The asm mis-nest history
   (`project_jeff_asm_misnest.md`) and the wave-30 hazard (a zero-map pin
   silently corrupting an unrelated 100% neighbor) prove raw-count deltas can
   hide compensating gains/losses. Acceptance = additions ≥ intended fixtures,
   **losses = 0 or individually explained** (the BandDirector-style objdiff
   re-pairing artifact is the known benign class — verify byte-identical
   target asm before accepting any loss).
2. **Counting authority:** `report.json` `match_percent_normalized == 100`
   (objdiff-cli report generate), the same authority
   `scripts/sync_match_percent.py` promotes from. Not objdiff-cli raw
   `matched_functions` (the 394-vs-1028 methodology gap in
   `project_jeff_asm_misnest.md` remains unreconciled).
3. **Re-split idempotency:** from the warm build dir, `touch
   config/45410914/config.yml && ninja` up to **3 times**; symbols.txt must
   reach a byte-stable fixed point and the matched SET must be identical at
   every round (the wave-36 caveat: judge by the matched set, byte-stability
   can take up to 3 rounds).
4. **Class-specific gates:**
   - Class 1: the 4 wave-40 fixture addresses gain a terminator-complete
     symbol; dtk "ends within symbol" error count monotonically decreases;
     global overlap-scan vs full splits.txt clean (wave-39: caught the
     Key.cpp/Color.cpp cross-TU fuse).
   - Class 2: `Stats::AddRoll` pairs as one 36B function and closes (source
     already exists — it was at_limit purely on the split);
     `fn_826791C0` gone from symbols.txt; emitted target `.pdata` entry count
     drops by exactly the merge count.
   - Class 3: `ProfileMgr::SetSynapseEnabled` emits as one COMDAT; grep the
     split log for the new suppression audit line; spurious except_data count
     in symbols.txt drops toward the ~18k census; **no genuine EH function
     regresses** (spot-check a known EH-heavy 100% unit).
5. **Audit logging:** every merge/extend/suppress logs at `info` with
   address/size/reason — the greppable-audit convention every prior pass
   (prune, clamp, grow, synthesize) established.
6. `cargo test` in jeff (119+ tests at last count) + clippy clean, per fork
   convention.

---

## Risks

- **jeff asm mis-nest hazard (systemic).** Any symbol whose size straddles a
  real function corrupts COFF COMDAT carving for its *neighbors*
  (`project_jeff_asm_misnest.md`) — a buggy merge that overshoots is exactly
  the phantom-symbol failure mode we spent round 0 pruning. The prune pass
  runs last and would catch overlaps, but a merge that *legally* spans two
  real functions (false-positive on class 2's zero-xref test — e.g. a
  vtable-only or export-only second fragment) would sail through. Mitigation:
  class 2's reloc test must use the *post-`tracker.apply`* reloc set (all
  references resolved), and the census reviews every candidate by hand before
  the pass is trusted un-audited.
- **Contamination incident (wave-34).** In-place rebuild of the live dtk
  binary corrupted main's symbols.txt mid-wave. Staging discipline above is
  the mitigation; additionally, do round-3 A/B work only against a worktree's
  private build dir.
- **symbols.txt is committed dtk output** (wave-34: "symbols.txt now committed
  dtk output") and self-perpetuates via `Symbols::add` keeping existing
  entries. Every class changes what dtk emits there; a half-landed state
  (new binary, old symbols.txt, or vice versa) is inconsistent. Land each
  class as: swap binary → full re-split → commit symbols.txt + splits.txt
  churn in the same rb3-xenon commit as the match promotions, and the jeff
  source commit in the fork. Stale caches from older splits must be handled
  by the passes (idempotent re-repair), not assumed absent.
- **Invariant drift.** Class 2 weakens the "pdata entry == function" reading
  that a670a12's guard 5 documents. If the doc-comment and `pdata_anchored`
  construction aren't updated together, a future session will "fix" the merge
  as a bug. The design deliberately keeps the stronger true invariant (pdata
  spans still partition `.text`) and must say so in code comments.
- **Class 3 false negatives on exotic EH.** The genuine-EH criterion assumes
  all real handlers resolve to code (empirically `__CxxFrameHandler`-only in
  RB3 per b1bc97c). If TU5-era or Bink/XDK regions use a different handler
  arrangement, suppression could drop a real EH struct → subtle unwind-data
  mismatch in the emitted `.pdata`. Mitigation: the criterion is unchanged
  from the already-landed `genuine_except_data_set` (only its *application
  point* moves earlier), so the risk is not new — but the spot-check in
  validation gate 4 exists for this.
- **Ghidra-derived boundary claims are untrustworthy** (wave-39: 55% snap
  artifact rate). All boundary decisions in these passes come from pdata,
  relocs, and instruction decoding — never from Ghidra. Keep it that way.

---

## Estimated payoff & priority

**Honest sizing (strict closes):**
- Class 1: 4 known wave-40 fixtures are *boundary-blocked, not source-blocked*
  ⇒ near-term +2 to +4 once re-bounded (they still need body-ports if
  un-ported — wave-39's lesson that newly-bounded leaves are port *targets*).
  The 77-fragment class (28% of the wave-37 corpus) converts from SKIP-class
  to identifiable surface — value is mostly *unblocked identification*, maybe
  +10–30 strict downstream over subsequent waves, not immediate.
- Class 2: +1 confirmed (AddRoll, source exists). Census may find a small
  family; assume single digits until measured.
- Class 3: +1 confirmed (SetSynapseEnabled) + symbols.txt noise reduction
  (~9k spurious except_data lines) that pays as reduced pin friction, not
  matches.

**Total: roughly +5–15 strict near-term, +10–30 unlocked surface** — a
modest-but-permanent unblock, plus tool-health value (the "ends within symbol"
error class and 28% fragment rate stop taxing every future wave).

**Priority vs. the other remaining pools (wave-40 strategic close-out):**
1. **Struct-layout recon** — higher expected strict yield per unit of effort
   (BandProfile's 3.2KB unmodeled tail, GemPlayer's foundational mUser drift
   etc. each gate *multiple* functions, and gains compound across the TU) and
   requires no fleet-risk tooling swap. Do first.
2. **jeff round 3 (this doc)** — second. Smaller immediate yield, but it is
   the ONLY path for its cases (permanent at_limit otherwise), it de-noises
   every future identification pass, and the census steps are cheap enough to
   run opportunistically. Class 1 is the highest-value/lowest-risk of the
   three and could land alone.
3. **Big divergent bodies** (VocalPlayer::Poll 4936B, BandSongMgr/Campaign
   Handle stack-layout class) — each is a dedicated expensive session for +1;
   do when the above are drained or when a session has the right shape.
   (Permuter-class near-misses stay blocked per user directive.)
