# Homing-pool `op` (opcode) evidence-class — implementation plan (2026-07-30)

> **STATUS (2026-07-30):** PLAN, not landed. Read-only review of current `main`
> (`matched_functions=40890`, `masked_equal_functions=1509`, honest floor
> **39,381**). Confirms the premises in
> `memory/project_homing_pool_truths_2026-07-29.md` against the current tree and
> turns the "next lever = the unimplemented `op` evidence class for the 679 TIEs"
> into a concrete build spec with an **honest yield estimate** and a landing
> protocol. **Recommendation: BUILD (small, safe, reusable) behind a hard
> held-out precision gate; expect single-digit score, fund it as identification
> infra, run it as a TAIL on body-port waves — not as a standalone score lane.**

## References (embedded for cold pickup)

- Primary truth: `memory/project_homing_pool_truths_2026-07-29.md` (lane BP-6,
  branch `laneBP6 @ c3887dee`, +15 honest; pool 27,120/4,823; decomposition
  17,927 homed + 12,455 no-evidence + **679 TIE** + 729 NO-WINNER + 14 folds;
  refill only 25 distinct `(name,VA)` pairs; `--trust-file` REFUTED ⇒ `--no-sym`;
  pins-without-map-names = +0).
- The resolver (where TIEs are produced and where `op` plugs in):
  `scripts/harvest/multi_content_disambiguate.py` — `op` documented lines 32-34,
  **never implemented**; `evaluate()` lines 416-515; `decode_slots()` lines
  300-350 (already computes `rec['op'] = w >> 26`, used only for kind
  classification); `classify_va()` 353-392; `func_table()` 138-182
  (`fn['words'][off]` = our **unmasked** instruction word).
- Pool producer: `scripts/harvest/homing_scan.py` — a "record" is
  `dict(name,size,nreloc,n_hits,hits=[VA…],n_unmapped,cls)`; `masked_eq()`
  (125-133) masks BOTH sides at reloc offsets, so **opcodes at masked offsets
  were never compared** — that is the residual signal `op` mines.
- Pricing rule: `memory/project_honest_floor_2026-07-29.md` §5 — price every
  landing by **Δ(`matched_functions` − `masked_equal_functions`); if honest Δ ≈ 0,
  DISCARD.** Named matches cannot be inflated by objdiff pass-2b
  (`hub_measurement` / `project-objdiff-oversub-disclosure`).
- Measurement traps: `memory/project_bandexe_read_traps_2026-07-29.md` §3-ter
  (`symbols.txt` DRIFT moves ~5 functions silently — `git checkout --
  config/45410914/symbols.txt` on BOTH legs) and the same-split-leg rule.
- Identification-era caveats: `memory/project_identification_toolstack_2026-07-25.md`
  (the honesty rule: a mispaired ICF twin reads a clean 100%; refuse ties, count
  them; `--validate` is held-out ground truth, not a self-check;
  `IDENTICAL-BODY FAMILY` = systematic fake-100 generator).
- Map-defect channel this feeds/endangers:
  `memory/project_map_defect_channels_2026-07-29.md` (a wrong confident name =
  a new false-100).
- Tooling caveats: `docs/decomp/TOOLING.md` §3 (stale-obj hazard — the resolver
  reads objs ninja hands it, so build a clean worktree first) and §4
  (`multi_content_disambiguate.py --help` currently raises inside argparse's
  formatter — fix that as part of this work, see Phase 0).

---

## 1. The pipeline: records → TIEs → resolution

**Record.** `homing_scan.py` compiles our objs, and for each function ≥32 B
extracts `(masked_body, reloc_offsets)`. It masks our reloc offsets, then for
every retail `.pdata` entry of *identical FunctionLength* masks the **same**
offsets in band.exe's `.text` and compares. `hits` = every retail VA that is
reloc-masked byte-identical to us. Classified `UNIQUE` / `UNIQUE-ICF` / `MULTI`
/ `ALL-MAPPED` / `NOMATCH`. Plain-`UNIQUE` was landed directly (round 4 +1,107);
`MULTI`/`UNIQUE-ICF` were left unpinned because **masking destroys the only
discriminator and a mispair still reads 100%.**

**Resolution.** `multi_content_disambiguate.py` restores the discriminator. For
each `MULTI`/`UNIQUE-ICF` record it re-parses our obj (`func_table`) to recover,
per masked offset, the symbol/token our relocation points at (a `??_C@` string,
an `__real@/__xmm@` float, or a mapped callee), plus `fn['words'][off]` = our
**unmasked** instruction word. `decode_slots()` decodes what each candidate
retail VA *materialises* at that offset (branch target / `lis+lo` address /
D-form immediate). `evaluate()` scores every candidate:

- `agree[c]`  += offsets where the candidate's resolved content == our expected
  token (or mapped VA);
- `conflict[c]` += offsets where it resolves to *different decodable* content.

`winners = [c for c if agree[c] and not conflict[c]]`.

**A TIE (line 488-489) is `len(winners) > 1`:** two or more candidate retail VAs
each satisfy some content evidence and contradict none. The existing evidence
classes cannot separate them — either both reference the *same* string/float
(genuine ICF-degenerate content), or each agrees on a *different* offset with
neither contradicting, or the discriminating slots decode to nothing. **679 such
TIEs remain** (BP-6 full-tree sweep). They are **refused, not guessed** — the
honesty rule (identification-toolstack) forbids rank-and-guess because a wrong
pick is invisible in the score forever.

**Why 679 are stuck:** content is exhausted on them. Note the dominant tie
family — `OBJ_CLASSNAME` / `DECLARE_MESSAGE` / `OBJ_SET_TYPE` bodies — is
byte-twin *modulo relocations*, and its one discriminator (the `StaticClassName`
callee) is itself a relocation of identical opcode form (`bl`). Those ties are
**opcode-blind** and `op` will not touch them; `op`'s reachable slice is the
narrower "same masked skeleton, different instruction *form*" population.

---

## 2. The `op` evidence class — design

### 2.1 Core observation (sound, false-negative-free)

A COFF relocation on X360 (`REFHI 0x10` / `REFLO 0x11` / `PAIR 0x12` / branch)
patches only the **operand field** of an instruction — the 16-bit immediate or
the 24-bit branch displacement. **It never touches the top 6 opcode bits.**
Therefore `fn['words'][off] >> 26` (our unmasked word) is the **true opcode** of
the real function at that offset, invariant under linking.

`homing_scan.masked_eq()` zeroed the whole 4-byte word on both sides, so the
opcode at every masked offset was **excluded from the byte compare** — it is
free residual signal sitting in `band.exe`, already decoded by `decode_slots()`
as `rec['op']`.

Define, per candidate `c` and masked offset `off`:

```
our_op   = fn['words'][off] >> 26            # ground truth, from our obj
their_op = slots[c][off]['op']               # decoded from band.exe .text
```

- `their_op == our_op` → **op-AGREE** for `c` at `off` (WEAK — opcodes repeat).
- `their_op != our_op` → **op-CONFLICT** for `c` at `off` (**HARD exclusion**).

**Soundness:** the true home has `their_op == our_op` at every masked offset by
construction (it is the same function; only operands were relocated). So
op-CONFLICT can **never** exclude the true candidate ⇒ zero false negatives. An
op-CONFLICT is a definitive "this candidate is not our function."

### 2.2 Reach beyond content

`op` reaches offsets the content classes cannot. `evaluate()` only builds an
`exp` entry for offsets whose reloc points at a *decodable* token
(string/float/mapped-callee); a reloc to an internal `.text` label or an
unmapped global yields **no** content evidence and is skipped today. `op`
compares opcodes at **every** masked offset regardless of what the reloc targets
— so it can exclude a rival at a slot content is blind to. This is the material
widening.

### 2.3 Exclusion-only — the critical safety property

`op` **must never upgrade a candidate to winner on its own.** An opcode *match*
is weak (a `lwz`/`bl` agreement is nearly free and shared by many functions);
only an opcode *mismatch* carries information. So:

- The winner still must hold an **independent content AGREE** (`str`/`vfstr`/
  `f32`/`f64`, or trust-gated `sym`). `op`-agree alone does **not** resolve.
- `op` contributes only **CONFLICT** to rival exclusion, plus **AGREE at the
  winner's own already-confirmed slots** to keep the honesty clause coherent
  (§2.4). It never manufactures a positive identity.

This is what stops `op` from planting a name where the true home was never a
candidate (the `NO-DETERMINED-HOME` failure mode): with no content AGREE there
is no winner, `op` or not.

### 2.4 Integration into `evaluate()` (exact wiring)

Fold `op` into the existing `agree`/`conflict` dicts, tagged so it does **not**
count toward the `strong` / `min_sym_agree` content thresholds:

1. After the content loop builds `agree`/`conflict`/`resolved`, add an `op` pass
   over **all** `off in fn['offs']` (not just `exp` offsets):
   ```
   our_op = (fn['words'].get(off, 0)) >> 26
   for c in cands:
       rec = slots[c].get(off)
       if rec is None: continue          # band couldn't decode this slot
       if rec['op'] == our_op: op_agree[c].add(off)
       else:                  op_conflict[c].add(off)
   ```
   Keep `op_agree`/`op_conflict` **separate** from the content sets.

2. Winner filter becomes:
   ```
   winners = [c for c in cands
              if agree[c] and not conflict[c] and not op_conflict[c]]
   ```
   (A content-winner with any op-CONFLICT is excluded — it is not our function.)

3. TIE break: if the content winner set had >1 but op-exclusion (`op_conflict`)
   removes all but one, that survivor is the resolution. If >1 survive, still
   `TIE` (report `op` did not decide).

4. Honesty clause (lines 493-499) — extend the "rival positively excluded at a
   slot the winner confirmed" test to accept an op-slot: a rival is validly
   excluded if `(conflict[c] | op_conflict[c]) & (agree[w] | op_agree[w])` is
   non-empty, i.e. the rival differs (content or opcode) at an offset where the
   winner positively matches (content or opcode). Because the winner has zero
   `op_conflict`, it holds `op_agree` at exactly the offsets where a rival was
   op-excluded, so the same-slot requirement is satisfiable.

5. Verdict labelling: when the *only* thing separating winner from the tied
   rivals is `op` (i.e. without `op` this would have been `TIE`), emit a new
   verdict `RESOLVED-OP` (winner's identity still carried by content AGREE; `op`
   only broke the tie). Keep it distinct so `--validate` can measure its
   precision **independently** — do not silently merge it into
   `RESOLVED-STRONG/SYM`.

6. Emit `op` evidence into the record's `evidence` list (offset + `our_op` +
   `their_op` of each excluded rival) so a landing is auditable.

### 2.5 What `op` will and will not crack (be concrete)

- **WILL:** structural twins that share a masked skeleton but differ in
  instruction form — e.g. one candidate does `lfs f,off(rA)` (op 48) where ours
  does `lwz r,off(rA)` (op 32); or `addi` (op 14, address compute) where ours is
  `lwz`/`stw` (load/store through the pointer); or a `bl` (op 18) where ours is a
  non-branch. Also any tie whose discriminating slot our content pass skipped for
  lack of a decodable token but whose opcode differs.
- **WILL NOT:** the `OBJ_CLASSNAME`/`DECLARE_MESSAGE`/`OBJ_SET_TYPE` family
  (identical `lis/addi/bl` opcodes, differ only in the *string operand* — a
  content problem, and per BP-6 unreliable by construction because the
  discriminator is a relocation); genuine ICF folds (physically one body — a
  VA→name map cannot express two merged functions anyway).

---

## 3. Honest yield estimate (priced under Δ matched − masked_equal)

Brutal, and deliberately pessimistic per the pool-size≠yield lesson (27k → 25
real pairs):

| stage | count | basis |
|---|--:|---|
| TIEs entering `op` | 679 | BP-6 full-tree decomposition |
| minus opcode-blind classname/message/settype family | large majority | dominant tie family, §2.5 |
| ties where a rival differs in opcode at some masked slot AND a unique winner survives | **~35–100 (guess, unmeasured)** | must be measured by Phase 1 `--validate`; do not fund on this row |
| of those, genuine NEW home (not already-homed; true home actually in hit set) | fraction | TIEs are already the *refused residue*; NO-DETERMINED-HOME is common |
| lands as a REAL named pin (pin + map fragment TOGETHER, in a paying span) | fraction | pins-without-names = +0 (BP-6); needs in-span (`span_predictor.py`) |
| **honest Δ(matched − masked_equal)** | **single digits, plausibly ~3–12** | each real break = one *named* body ⇒ not maskable, so it is honest if it pairs |

**State plainly:** the realistic score yield is **single digits**. If Phase 1
`--validate` shows `op` produces `<~5` correct unique breaks, the score case is
≈0 and the landing would be **DISCARDED under the honest-floor rule** — but
**ship the code anyway** as identification infra (§6 rationale). Do **not**
budget a score-driven lane around 679; budget a one-time `--validate`
measurement plus a tail-hook.

---

## 4. Interfaces & gotchas (confirmed against current tree)

- **`--no-sym`, not `--trust-file`.** REFUTED at current tree state (BP-6):
  held-out `--validate` gave `--trust-file` RESOLVED-SYM **71.9%** (laneG claimed
  ≥95.1%) with 7 demonstrated errors *inside the strong class*, because map
  repairs grew the trust set 2,191→3,359, `sym` fired 4.4× more, and a `sym`
  CONFLICT on the true candidate flips which rival is excluded — **the weak
  evidence class corrupts the strong one.** `--no-sym` gives 0 tool errors (all
  misses are TRUTH-CONFLICT = the map was already wrong). ⇒ **Run `op` under
  `--no-sym`.** This is also *convenient*: `op` is itself map-free, so the whole
  `op` path needs no `target_symbol_map.json` and cannot inherit its mispairs.
- **Pins + fragment map-name land TOGETHER.** A pinned range with no map name
  stays an anonymous `fn_<addr>` objdiff cannot pair ⇒ **+0** (BP-6: all +15 were
  carried by the 24-row map fragment; pins alone = 0). Every `op` resolution
  must emit both a `splits.txt` pin (or confirm the VA is already inside a
  pinned span) and a `target_symbol_map.json` fragment entry, applied via
  `homing_gen4.py` / `homing_apply4.py` (the resolver already emits
  homing-scan-format so these consume it unchanged).
- **In-span pre-check.** Repair/reveal only pays when the true home is pinned AND
  inside the span of the unit that compiles the symbol
  (`span_predictor.py`; identification-toolstack). Run it before landing;
  mind its two defects (`.c`-unit mis-flag; self-confirm — use a sentinel `tu`).
- **`symbols.txt` DRIFT.** `git checkout -- config/45410914/symbols.txt` on
  **both** A/B legs (moves ~5 functions silently otherwise — bandexe-read-traps
  §3-ter).
- **Same-split legs / worktree-local deltas.** A DELTA is only valid within one
  worktree with both legs in the same split state; ABSOLUTE counts are **not**
  portable across worktrees (±2 band). Take the baseline in a worktree already
  through `touch config.yml` + full build.
- **`rm -f build/45410914/report.cache`** before every whole-binary A/B leg
  (report-cache-stale-ab: false +110 seen).
- **Append-only / dedup map.** One retail VA may be claimed by only one
  `(unit,name)`; the resolver already drops contested VAs (`DROP-CONTESTED`).
  Preserve the name-injectivity guards (`_name_injectivity_comment`) — never
  introduce a duplicate name or a duplicate VA.
- **Stale-obj hazard.** Build a clean worktree (`scripts/setup_worktree.sh
  ~/tmp/wt-op op`) before running; the resolver reads
  `build/45410914/src/<tu>.obj` by path (TOOLING §3).
- **`--help` is currently broken** (`TypeError` in argparse formatter, TOOLING
  §4). Fix it in Phase 0 so the tool is runnable/documented before adding `op`.

---

## 5. Risks

1. **★ Top risk — false-confident TIE break → NEW false-100.** If `op` leaves a
   unique winner whose true home was never in the hit set, we plant a wrong name,
   which reads a clean 100% forever and feeds the map-defect channel
   (`project_map_defect_channels`, `project_bandexe_read_traps` §4 —
   `??1NewAwardPanel` is a live example of an at-100.0 mispair). **Mitigations:**
   `op` is exclusion-only + winner must hold independent content AGREE (§2.3) +
   held-out `--validate` gate (Phase 1) + `--no-sym` + refuse remaining ties. The
   `OBJ_SET_TYPE` root trap (BP-6) means `op` deliberately does **not** act on
   that family (opcode-blind anyway).
2. **`decode_slots` mis-resolution.** It mis-pairs some `lis`/D-form pool
   addresses (noted for unaligned floats). This affects the *content* path, not
   the raw `rec['op']` read used by `op` (opcode bits are read directly from the
   decoded word) — but keep `op` reading `rec['op']` straight from the word, not
   from any resolved `value`.
3. **Yield theatre.** 679 is blast radius, not yield
   (`feedback-site-count-is-not-defect-count`). Refuse to quote a headline off
   679; quote only the `--validate`-measured unique-break count and the
   post-landing Δ(matched − masked_equal).
4. **Measurement contamination.** `symbols.txt` drift, cross-worktree absolutes,
   stale report.cache, stale objs — all §4. Any one silently manufactures ±2..
   +110.
5. **band.exe VA→offset trap.** `.text` is RVA `0x270000` / raw `0x264E00`
   (0xB200 delta); the resolver's `Band` class already uses the section table
   correctly (`text_bytes` computes `va - tva`), so `op` inherits the correct
   mapping — but sanity-anchor `off(0x824DAAD0)==0x004CF8D0` if touching the
   reader.

---

## 6. Is the tooling worth building even at low yield?

**Yes — build it, with eyes open.** Rationale:

- **Cheap:** ~40 LOC inside an existing, understood tool; no new file, no new
  data source, `decode_slots` already computes `rec['op']`.
- **Safe by construction:** map-free, exclusion-only, false-negative-free; cannot
  create a false-100 on its own (winner still needs content AGREE).
- **Closes the design:** `op` is the last documented-but-unbuilt evidence class;
  finishing it makes the resolver's docstring true and its verdicts complete.
- **Reusable infra:** the opcode-exclusion primitive also strengthens the
  `--validate` held-out measurement and the `--trust-audit` map integrity check,
  and it re-fires for free as a **tail** on every future body-port wave that
  refills the pool (identification-toolstack: run the scan as a tail, never as
  its own lane).
- It is **not** worth convening a dedicated score lane around, staffing multiple
  agents, or promising a count. Treat any score as a bonus.

---

## 7. Phased execution order

- **Phase 0 — make the tool runnable (prereq).** Fix the `--help` `TypeError`
  (TOOLING §4). Stand up a clean worktree (`setup_worktree.sh ~/tmp/wt-op op`),
  full `ninja-locked`, regenerate a fresh whole-tree `homing_scan.py` merged
  result over all live objs (the pool must be current, not BP-6-stale).
- **Phase 1 — implement `op` + MEASURE before landing anything.** Wire §2.4.
  Run `--validate` (held-out ground truth: functions whose home is already
  mapped, all hits fed as candidates) under `--no-sym`, reporting
  `RESOLVED-OP` precision **separately**. **Gate:** proceed only if
  `RESOLVED-OP` precision ≥ ~99% AND the unique-break count is non-trivial. If
  precision is low, stop — the design is unsound in practice; keep the code
  disabled behind a flag.
- **Phase 2 — dry-run the resolve path.** Run without `--validate` over
  `MULTI,UNIQUE-ICF` under `--no-sym`; collect the `RESOLVED-OP` proposals. For
  each, run `span_predictor.py` (sentinel `tu`, mind the `.c` defect) to confirm
  the winner VA is in a paying span. Discard any not in-span.
- **Phase 3 — land pin + fragment TOGETHER.** `homing_gen4.py` /
  `homing_apply4.py` to emit `splits.txt` pins (gap-fill, never duplicate header)
  + `target_symbol_map.json` fragment. Preserve injectivity guards.
- **Phase 4 — verify honestly.** In one worktree, both legs same split state:
  `git checkout -- config/45410914/symbols.txt`; `rm -f
  build/45410914/report.cache`; `touch config.yml`; `python3 configure.py`
  (if objects changed); full `ninja-locked`; whole-binary strict-set diff vs a
  pickled baseline, BOTH name-only and by-(unit,name). Price by
  **Δ(matched_functions − masked_equal_functions)**. If honest Δ ≈ 0 →
  **DISCARD the landing** (keep the code).
- **Phase 5 — wire as a tail.** Add the `op`-enabled resolve to the
  `homing_scan_all.sh` tail hook so it re-fires after body-port waves; never
  schedule it as a standalone score lane.
