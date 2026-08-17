# rb3-xenon docs index

Single entry point for every doc under `docs/`. Find the right reference, skip the stale ones.

**Project:** decompilation of Rock Band 3 for Xbox 360 (PowerPC), compiling C++ to matching
retail machine code. Decomp effort concentrates on the **game layer** (`src/band3/`,
`src/network/`); the Milo engine (`src/system/`) is effectively pre-solved via DC3. Full
framing in `../CLAUDE.md` — **read that first**, it is the authoritative current-state doc.

## How to read this index

- **No tag** = current / evergreen reference that matches repo reality.
- **`[HIST]`** = dated historical record (research log, per-task handoff, executed/superseded
  plan). Kept as a searchable archive; its match-counts and "current state" claims are frozen
  at its date. Do not trust these for today's numbers.
- On **2026-07-06** the inherited dtk-template boilerplate docs and several stale current-state
  docs were audited and amended: each now carries a `> **STATUS (2026-07-06):**` banner right
  under its title telling you whether it's accurate, historical, or superseded (and by what).
  If a doc has no banner, it was judged accurate as-is.

## Known traps (read before you touch anything)

- **No leaked map for RB3.** There is NO `ham_xbox_r.map` for this binary. That map is
  **DC3's** (`../dc3-decomp/orig/373307D9/ham_xbox_r.map`); RB3's functions are anonymous
  `fn_8XXXXXXX`. Any doc implying `orig/45410914/ham_xbox_r.map` exists is wrong — symbols come
  from `tools/fingerprint_match.py`, `decomp.db`/`report.json`, and the Ghidra+BinDiff /
  `apply_symbols.py` pipeline. See `tools/GHIDRA.md`, `tools/REFERENCE.md`.
- **Target is a retail `/O1 /Oi /GR /EHsc` size-optimized release with ICF — NOT a debug build
  and NOT LTCG/LTO.** ICF (identical-COMDAT folding) is a separate linker feature that IS active.
  Verdict evidence: `plans/lto-vs-icf-investigation-2026-06-06.md`.
- **`MILO_DEBUG` does NOT gate `MILO_ASSERT` — `HX_NATIVE` does.** `src/macros.h:3`
  force-defines `MILO_DEBUG` tree-wide, and several commit messages and header comments claim
  this is "to keep `MILO_ASSERT` live". **It is not**: `MILO_DEBUG` appears nowhere in
  `os/Debug.h`; the whole `MILO_*` family is `#ifdef HX_NATIVE`, and the match build passes
  **no `/D` at all**, so `MILO_ASSERT(cond,line)` is `((void)(cond))`. The force-define's only
  effect is to switch on rb3-Wii **dev-build** code that retail compiled out. ⚠ But do **not**
  blanket-remove it — the measured whole-binary control is **−21 functions** (one guard,
  `BandCharacter`'s `toggle_interests_overlay` handler, is genuinely in retail and costs −22 by
  itself). Adjudicate per site against target asm. Full census, the `HX_NATIVE` house fix
  pattern, and the TU-local-`#undef` ODR hazard:
  `decomp/patterns/milo-debug-force-define.md`.
- **Worktrees and build logs go under `~/tmp` (= `/home/free/tmp`), never `/tmp`.** `/tmp` is
  RAM-backed tmpfs with no btrfs reflink — it fills up and defeats CoW. Use
  `scripts/setup_worktree.sh` + `~/tmp/rb3_build_<task>.log`.
- **Match-counts age fast.** Any doc dated ≤ 2026-06 carries a matched-function count from its
  era (e.g. 394, 3919, 6568, 9793). Current progress lives in the orchestrator DB
  (`decomp.db`) / `build/45410914/report.json` and MEMORY.md, not in these docs.
- ★★★ **RULER CHANGE 2026-08-02 — every "honest" number anywhere under `docs/` predating
  that date is on the OLD ruler.** The objdiff fork now discloses **all** funclet
  byte-signature pairings in `masked_equal_functions` rather than pass-2b
  over-subscription only: **1,096 → 22,640**, so honest (`matched − masked_equal`)
  moved **42,358 → 20,814** (disclosure share 2.52% → **52.10%**). **No score key moved**
  — `matched_functions`, `matched_code_percent`, `fuzzy_match_percent` are all identical;
  this was disclosure, not scoring. Baseline at `f48bcad7`: **43,454 matched / 22,640
  masked_equal / 20,814 honest / 38.810524 code% / fuzzy 45.912785 / `total_code`
  10,688,688 / `total_functions` 69,357**. Authoritative record:
  **[decomp/RULER_CHANGE_2026-08-02.md](decomp/RULER_CHANGE_2026-08-02.md)**.
  ⚠ **Those are the values AT `f48bcad7` — point-in-time, not current.** In particular
  **`total_code` is not a constant and must never be memorised or hardcoded**: it moves
  whenever splits pins change, and pin waves have moved it by hundreds of KB (see the
  `PIN_WAVES_AND_DENOMINATOR` entry in §1). **Read `total_code` / `total_functions` from
  `build/45410914/report.json`** at the commit you are measuring. For scale only: it read
  **10,320,692** at `HEAD` on **2026-08-13** — quoted as a dated observation, not a
  constant to reuse.
  Three consequences for reading this index:
  1. **A dated doc's honest figure is CORRECT AS HISTORY.** `[HIST]` records and lane
     logs recording "honest N at the time" have not been rewritten and must not be —
     they are the evidence trail. Read them on their own ruler.
  2. **Deltas survive the flip; absolutes do not.** A pre-flip Δhonest remains valid as
     a delta *on the old ruler*, but pre- and post-flip Δhonest values are **not
     comparable and must never be chained or summed.** (Δmatched and Δcode% are
     unaffected — those keys did not move.)
  3. **A tool can be stale too, not just prose.** The flip silently broke
     `tools/guard_funclet_census.py`'s derived estimator, whose "phantom credit" line
     assumed masked_equal ≈ over-subscription only; under the new disclosure it fires
     on *every* funclet pairing. Its `--deficit` mode is ruler-independent and is the
     one to use. If a tool derives anything from `masked_equal_functions`, re-check it.
- **Two different "mapped"s — never compare them.** dtk's own progress box says *mapped* for
  bytes **pinned** to a `splits.txt` unit (the prerequisite to matching). `tools/scope_map.py`'s
  dashboard footer counts bytes **classified into a scope tier** by any of its 8 layers, pinned
  or not — always the larger number. The dashboard therefore labels its own axis
  **"tier-classified"**, not "mapped".
- **The scope-tier percentages depend on a gitignored cache.**
  `config/45410914/scope_map.json` is addr-keyed to ONE target build and is not committed. If it
  is absent (fresh checkout), corrupt, or keyed to an older XEX revision, the ~65k anonymous
  `fn_8XXXXXXX` functions fall into `unknown`, the per-tier **denominators collapse to
  pinned-only**, and every tier % in the dashboard reads **INFLATED** and is not comparable to
  main's. The dashboard now prints a banner in that case; the fix is always
  `python3 tools/scope_map.py build` (~1 s). `scripts/setup_worktree.sh` reflinks the cache into
  new worktrees, but a cache produced before a target re-base (e.g. the TU0→TU5 flip) must be
  rebuilt everywhere, main included. The headline `binary NN% matched` line is read straight
  from `report.json` and is always honest.
- **A warm worktree carries THOUSANDS of stale `auto_*_text.s` files beside the live
  ones — reading them yields FALSE content evidence.** `build/45410914/asm/` is never
  cleaned, so carves from dead `splits.txt` generations accumulate. Measured 2026-07-29
  in a fresh worktree immediately after a clean full `./tools/ninja-locked`:
  **12,950 `.s` files total, of which only 3,967 are live** (= `report.json`'s
  `total_units`, exactly) and **8,983 are stale — 4,618 of them `auto_03_*_text.s`**,
  dated as far back as 2026-06-02. Any scan that globs `asm/**/*.s` (or, equivalently,
  `obj/**/*.obj`) reads another era's carve as if it were current. **Filter by mtime
  against `config/45410914/config.json`** (`find build/45410914/asm -name '*.s' -newer
  config/45410914/config.json`), or better, **enumerate from `objdiff.json`**, which
  `configure.py` regenerates from the live `objects.json` + `splits.txt`. Same class of
  bug as the census-universe defect in `plans/decomp-state-2026-07-19.md` (laneAY).
  > **CORRECTED 2026-07-29 (lane BM):** two parts of the advice above are now
  > superseded. (1) **mtime is NOT a usable freshness proxy** — 72 of 90 named
  > orphans carried the *same day's* date, because `splits.txt` gets rewritten
  > between two split runs minutes apart, and `asm/Faders.s` (live) and
  > `asm/system/synth/Faders.s` (orphan) coexist ten minutes apart. Use
  > **`objdiff.json` membership** only, via `scripts/harvest/live_units.py`;
  > the remedy tool is **`scripts/prune_orphan_asm.py`**, which deletes the
  > orphans by `config.json` membership and thereby fixes every downstream
  > reader at once. (2) Re-measured on main 2026-07-29: **12,994 `.s` / 3,932
  > live / 9,062 stale = 69.7% stale** (and 13,016 `.obj`, same 9,062 stale);
  > `objdiff.json` lists 3,862 live units.
  > ★ **And `live_units.py` itself has a residual defect:** `filter_live()`
  > joins on **basename**, so the 34 unit basenames that exist both flat and
  > nested (`MusicLibrary`, `CrowdAudio`, `LightHue`, `EventTrigger`, `deflate`,
  > `ctr`, `CharIKRod`, `Instance`, …) keep **68 stale files** through the
  > filter. The survivors are the tiny 2 KB nested orphans — exactly the shape
  > that reads as "a unit with almost no content". Full offender list and the
  > correct join (`live_target_paths()` with `obj/→asm/`, `.obj→.s`) in
  > **[decomp/TOOLING.md](decomp/TOOLING.md) §3**.
- **`scripts/harvest/resolve_splits_union.py` is a line-UNION and CANNOT propagate
  deletions — a deletion-valued patch silently no-ops.** `land.sh` calls it to resolve
  a conflicted `config/45410914/splits.txt` during `git rebase main`; under rebase,
  *ours* is main and *theirs* is the lane's branch. The tool seeds its output from
  **ours** and grafts only the lines **theirs added vs the merge base** (its own
  docstring, line 17: "No removals are propagated"). So every pin the lane **removed**
  is still present in main's copy and survives — the removal is silently discarded and
  `land.sh` still reports `READY:`. This matters because unpinning is a real, landed
  fix shape (phantom-shell TUs in `01a0e9fa`, XDK-territory spans in laneXDKPIN).
  ⇒ **If your patch's value is a deletion in `splits.txt`, do not land it through
  `land.sh` on a conflict — resolve by hand and re-verify with
  `scripts/harvest/overlap_check.py`.** (Contrast `resolve_json_union.py`, which *is*
  3-way and does respect deletions.)
- **A new `splits.txt` unit HEADER is not a newly-pinned TU.** `splits.txt` keys on the
  header spelling, not on the source file, so `Crowd.cpp:` and `system/world/Crowd.cpp:`
  are two keys for one `.cpp`. Diff by **basename** before claiming first-ever coverage
  — `01a0e9fa`'s "24 TUs get their FIRST pinned range ever" is wrong for 23 of the 24
  for exactly this reason (worked example in `plans/decomp-state-2026-07-19.md`).
- **A thunk-identity tool that does not fold `??_G`/`??_E` manufactures phantom
  defects.** MSVC names a deleting-destructor *body* `??_G<C>` but every adjustor thunk
  of it `??_E<C>`, and it spells simple adjustors `W<n>`, not `$4`. A check that looks
  for a `$`-name scoped `??_G<C>@@` asks for a symbol that cannot exist for any class.
  This produced a 41-class "missing virtual override" worklist that cost a lane a full
  investigation before `26284d0d` refuted it (NO `virtual` was missing anywhere). The
  vetted primitives now live in one place — `scripts/harvest/thunk_shape.py`.
- ★ **`orig/45410914/band.exe` is the DECOMPRESSED RETAIL PE** (imagebase
  `0x82000000`, extracted from `default.xex`). You can read retail's **real
  bytes** — literal pool, `.pdata`, instruction stream — at any VA instead of
  inferring from the symbol map, and it **dissolves the ICF confounder** (one
  physical body, not N symbol aliases). 40 tools already consume it
  (`funclet_cascade_rank.py`, `switch_frame_census.py`, `map_verify.py`,
  `localstatic_symbol_audit.py`, `symbols_hygiene.py`, `tools/va_disasm.py`,
  `tools/xex_string_at.py`, …). **Prefer it over any derived artifact.**
  ★ To recover a dispatch-arm list *in retail's exact order*, don't run `strings`
  on it — pull the target fn out of `build/45410914/asm/<Unit>.s`, grep its
  `addi r4, r11, lbl_…` sequence, and resolve each label in
  `auto_00_82000400_rdata.s`. Cheaper, ordered, and it tells you *where to
  insert* (laneBK, `94244fbd` — the commit message was gutted by shell
  backticks; the doc is authoritative).
- ★ **A function can score 100% and still be WRONG.** Relocation-masked
  operands (string/constant addresses) are invisible to the normalized diff, so
  a wrong constant or wrong literal is undetectable by any scanner that looks
  *below* 100%. 40 such correctness fixes landed 2026-07-29 at **exactly 0
  metric movement** (`plans/realbug-fixes-2026-07-29.md`).
  ★★ **But 43% of that worklist were MAP MISPAIRS, not source bugs.** Rule:
  *if retail's diverging operands coherently describe a **different** function —
  a sibling, a template twin, another class — the defect is in
  `scripts/target_symbol_map.json`, not in the source.* Fix the map; do not
  "fix" the source to match a function you were never paired against.
- **Element-stride arithmetic refutes a map binding only when the sizes actually
  disagree.** `Key<T> = {T value; float frame}`, so `Key<Quat>` and `Key<Color>`
  are **each exactly 0x14** (Quat and Color are both 4 floats) — a 0x14 STL
  element stride is *consistent* with them and proves nothing. Only
  `Key<Vector3>` (12+4 → **16**) is refuted arithmetically. Check the arithmetic
  per type; prefer call-graph closure or automap-EXACT byte identity as the
  discriminator. (Correct table already in `plans/tu-pin-wave-2026-07-29.md` §4.2.)
- **`scripts/harvest/handler_list_diff.py` is BROKEN — it emits FALSE SURPLUS.**
  When retail string extraction returns nothing it returns `[]`, not `None`, so
  the caller's `is None` guard passes and *every* handler we have is reported as
  surplus. It claimed 9 surplus handlers for `StorePanel` that rb3-Wii confirms
  retail HAS. Do not act on its output. See `decomp/TOOLING.md` §4.
- **`scripts/harvest/reloc_correspondence.py` is a whole-binary batch job, not a
  per-lane gate.** A single `--symbol` invocation timed out at **10 minutes** —
  it runs three whole-binary oracle passes before the filter applies. Use
  `--census`. See `decomp/TOOLING.md` §4.
- ★ **A near-100 row may be scoring against the WRONG target body.**
  `scripts/target_symbol_map.json` is address-keyed and had one mangled name
  stamped onto several VAs — which a linked image can never do. objdiff pairs by
  name and is blind to the relocation targets that separate byte-twin thunks,
  deleting dtors and template bodies, so the wrong copy still scores, and at 99.5%
  it is **one edit from minting a byte-exact witness against a function that is
  not the target**. Byte-exact is the *admission* gate, so that witness is
  unrecoverable after the fact. Enforced since 2026-08-13 by
  `tools/map_name_injectivity.py` (ninja edge `map_name_injectivity_check`, an
  input of both the default `progress` target and `build/45410914/report.json`;
  it was wired to `progress` **only** until 2026-08-13, so `ninja
  build/45410914/report.json` — what `sync_match_percent.py --build` runs —
  exited 0 without it) — the per-unit checks `icf_class_bijection.py` and
  `tu5_map_apply_fragment.py` are unit-scoped and never caught it.
  **`??$__destroy_aux@ULevelData@@...` at `0x82b5b1d0` / `0x82b63ec8` is still
  unadjudicated and denylisted — do not close it.** Full record:
  [decomp/MAP_NAME_INJECTIVITY.md](decomp/MAP_NAME_INJECTIVITY.md).

---

## 1. Start here / current state

- [../CLAUDE.md](../CLAUDE.md) — project framing, build tracks, decomp priority, worktree/git
  rules, toolchain wiring. Authoritative current state.
- ★★★ [decomp/CAMPAIGN_STATE_2026-08-14.md](decomp/CAMPAIGN_STATE_2026-08-14.md) —
  **the campaign consolidation: what is drained, what is open, and what will lie to you.**
  Rewritten end-of-session at `3eb85dfd` after **47 lanes** landed in one day.
  ★★★ **Every large lever was sized this session and they all deflate** — alias
  forgiveness (720,992 B but **82.51% already proven**), the ruler gap (**a strict
  subset** of an already-drained stratum, `|B \ A| = 0`), `TEMPLATE_ARGS` (**51%
  fold**), "313 wrong-callee defects" (**54% fold-shaped**), identification
  (**~0.2% of `total_code`**), the body-write surface (**~5 kB, not 52 kB**),
  epilogue over-carve (**2 live rows of 246 blocks**). **⇒ there is no big lever
  left.** Carries the measured pairable decomposition (the 2.76 MB of headroom by
  stratum — ⚠ its largest block is *absence of identification*, not divergence),
  the re-measured **62.87%** ceiling, the whole session's measured movement
  (**+150,180 B / +49 fns**, ~53% of it alias bookkeeping), a catalogue of
  **~35 instrument failures grouped by failure mode**, and the adjudication rules
  that survived. Read §0 first — it is a one-minute router.
- ★★★ [decomp/PIN_WAVES_AND_DENOMINATOR_2026-08-09.md](decomp/PIN_WAVES_AND_DENOMINATOR_2026-08-09.md) —
  **`total_code` was INFLATED and pinning corrects it.** dtk bills an unbounded
  symbol in an unpinned region out to the next boundary — **one 204-byte function
  was billed 210,136 B, ~2% of the binary, in a single row.** Waves 1–2 pinned
  1,710,652 B / 199 units with matching keys at exactly Δ0, and `total_code` fell
  325,804 B ⇒ code% 40.77 → **42.06 as a CORRECTION, not progress**. ⇒ **every
  historical code% over an unpinned-heavy tree was UNDERSTATED.** Includes the
  byte-for-byte-identical control proving pins are otherwise denominator-neutral,
  and three measured vacuous instruments — ⛔ **`lbl_` symbol NAMES LIE about
  their own address** (2 independent instances), `fn_<addr>` names are rewritten
  by the renamer, and auto-unit names are unstable across splits (a name-keyed
  diff read "937 new units" when the truth was 31).
- ★ [decomp/RULER_CHANGE_2026-08-02.md](decomp/RULER_CHANGE_2026-08-02.md) — **the
  authoritative record of the `masked_equal_functions` disclosure flip** (honest
  42,358 → 20,814, no score key moved). **Read this before quoting or comparing any
  "honest" figure**, and before trusting a pre-2026-08-02 Δhonest. Includes the
  11-key no-change verification, the mid-run-swap hazard, the `ab_measure.py`
  same-ruler guard, and the rollback. **Amended 2026-08-06** with `@none` /
  `@name_check` tags on every absolute and the honest band below.
- ★ [decomp/RELOCNAME_AUDIT_ALIGNMENT_2026-08-06.md](decomp/RELOCNAME_AUDIT_ALIGNMENT_2026-08-06.md) —
  **how much the `name_check` ruler actually withdraws: `matched_code` is
  overstated by [0.23, 4.00] pp `@none`, not the 11.89 pp a raw flip implies**
  (62.2% of the 12,679 charged pairs are proven ICF folds). Alignment with
  decomp-synth's independent audit; the CV-4 class-(b) residual chain kept
  current (**717 fns / 115,568 B** still UNKNOWN, not CV-4's 1,223); why the
  instrument CV-4 "named and did not build" was in fact built **twice** and
  neither supersedes the other; and the alias-map A/B (`symbol_aliases.json`
  272 → 521 groups, **+0.813114 pp @name_check at EXACTLY 0 on every default
  key**). Its GENUINE residue ships as
  [decomp/relocname-genuine-worklist-WS4.tsv](decomp/relocname-genuine-worklist-WS4.tsv)
  — a worklist, never applied.
- [decomp/TOOLING.md](decomp/TOOLING.md) — ★ **the audited tooling inventory (2026-07-29)**:
  every tool in `tools/`, `scripts/`, `scripts/harvest/` run and status-graded
  (WORKING/BROKEN/SUPERSEDED/ONE-SHOT), a "start here for task X" routing table, the
  ground-truth artifact table (incl. `band.exe`), the stale-build-dir offender list, and
  the known-defective set. **Read this before running any scanner.**
- [plans/decomp-state-2026-07-19.md](plans/decomp-state-2026-07-19.md) — **live state & veins
  doc** (strict count, PIVOT POINT: cheap veins exhausted → deep grind), updated as waves land.
- [plans/paths-to-100/README.md](plans/paths-to-100/README.md) — **paths-to-100 RFC set
  (2026-07-08, 20 RFCs + ranked index)**: every remaining vein sized against the two walls
  (identification recall, body-divergence), verify-before-assert, with PURSUE/PILOT/DO-NOT
  verdicts and a settled do-not-re-litigate list. Read the README ranking first.
- [plans/frontier-workstreams-2026-07-02.md](plans/frontier-workstreams-2026-07-02.md) —
  tracking doc for the 7 frontier streams (ws1-ws7); superseded as "live state" by
  decomp-state-2026-07-19 above.
- [decomp/handoff/wave-loop-SOP-2026-06-20.md](decomp/handoff/wave-loop-SOP-2026-06-20.md) —
  wave-loop SOP: discover / execute / audit / reduce + harvest/land protocol.
- [plans/rb3enhanced-same-instrument-patch.md](plans/rb3enhanced-same-instrument-patch.md) —
  runtime-mod plan (not decomp): fork RB3Enhanced to add multiple-players-on-one-instrument
  to retail RB3 TU5. Uses rb3-xenon as address oracle. 3 enforcement layers + gem-list
  clone centerpiece (installed at the `RecalcGemList` re-borrow choke-point, not at
  watcher ctor); build/boot pipeline (Xenia `.patch.toml` mechanism resolved), Phase-0
  spikes, fingerprint-based address cookbook. Derived + prologue-verified:
  `IsActive 0x8264B5F8`, `ResolvePartWaitStates 0x8259D948`.

### 2026-07-29 results (main 39,382 → 39,743, coordinator-verified)

Each of these is a **pricing or refutation** — read the verdict before re-opening the vein.

- [plans/attribution-frontier-census-2026-07-29.md](plans/attribution-frontier-census-2026-07-29.md) —
  the dtk auto-carve pool's ceiling is **+25..+85, not thousands**. Root cause: `auto_03_*`
  units have a `target_path` but **no `base_path`**, so objdiff never *attempts* pairing —
  only a `splits.txt` claim changes that. 98.3% have no byte-twin. Breadcrumb count
  corrected **17,771 → 9,308**.
- [plans/rb3-360-vs-wii-coverage-2026-07-29.md](plans/rb3-360-vs-wii-coverage-2026-07-29.md) —
  **there is no large 360-exclusive frontier.** The SKUs differ by a swapped platform layer
  plus ~18 RBN-authoring classes; **Quazal is on BOTH SKUs** (97/103 TUs present in the Wii
  binary). The real gap is **141 Wii-oracle TUs / 2,505 fns whose 360 location was never
  found** → [plans/wii-oracle-tu-location-2026-07-29.md](plans/wii-oracle-tu-location-2026-07-29.md).
  Tool: `scripts/harvest/oracle_coverage_matrix.py`.
- [plans/reloc-correspondence-audit-2026-07-29.md](plans/reloc-correspondence-audit-2026-07-29.md) —
  the strict count is **substantially sound**: 65.5% / 43.8% evidenced, **5.2% / 2.7%
  DIVERGENT**, the rest undecidable (`.bss` + externs are *unobservable*, **not** suspect).
  ⚠ carries a `CORRECTED 2026-07-29` banner — two of its named defects were re-adjudicated.
- [plans/realbug-fixes-2026-07-29.md](plans/realbug-fixes-2026-07-29.md) + `plans/laneBH_realbugs.json` —
  the **reloc-masked defect class**: functions scoring **100% with wrong constants/strings**,
  invisible because every scanner looks *below* 100%. 40 correctness fixes at exactly 0 metric
  movement. ★ **43% of the worklist turned out to be MAP MISPAIRS, not source bugs** (see the
  rule in Known traps).
- **The local-static frame cascade** — ⚠ *doc pending*: as of this audit neither
  `plans/nearmiss-drive-to-zero-2026-07-29.md` nor `plans/localstatic-cascade-drain-2026-07-29.md`
  exists on disk (lane BK owns the latter and is still writing). Recording the verdict here so
  it is not lost:: retail builds each dispatch `Symbol` as a
  *function-local* static; ours used globals ⇒ 2 extra callee-saves ⇒ the parent frame shifts
  ⇒ and since **every EH funclet encodes the parent frame in its first instruction**, one
  per-TU macro gate flips a whole cascade. **76% of the 96–100% band is funclets, not
  near-miss functions.** ★ **Early-return restructuring measured NEGATIVE (98.3 → 34.1) —
  do not use it.**
- [plans/branch-audit-2026-07-29.md](plans/branch-audit-2026-07-29.md) — **UNMERGED ≠
  UNLANDED**: lanes land by patch, so a stale branch can be net-HARMFUL to land.
- [plans/nothrow-scatter-pricing-2026-07-29.md](plans/nothrow-scatter-pricing-2026-07-29.md) —
  channel **CLOSED by a control group**: scatter units' EH-deletion rate (2.66%) is *below*
  plain units' (3.55%).
- [plans/gapfill-pricing-and-nearmiss-open-2026-07-29.md](plans/gapfill-pricing-and-nearmiss-open-2026-07-29.md) —
  gap-channel pricing + the `tools/scope_map.py` dropped-function fix (laneBE).
- [plans/lane-bf-stl-instantiation-mispair-verdict.md](plans/lane-bf-stl-instantiation-mispair-verdict.md) —
  CLASS VERDICT: the STL-instantiation near-miss band is **MAP MISPAIR**, not source drift.
- ★ **laneBK's dispatch-arm method** (landed `94244fbd`; the commit message was gutted by
  shell backticks, so the doc + the Known-traps entry above are the authoritative record):
  to recover retail's exact dispatch-arm list *in order*, pull the target fn from
  `build/45410914/asm/<Unit>.s`, grep its `addi r4, r11, lbl_…` sequence, and resolve each
  label in `auto_00_82000400_rdata.s`.

- ★★ [decomp/NEXT_WAVE_BRIEF_2026-08-05.md](decomp/NEXT_WAVE_BRIEF_2026-08-05.md) —
  **START HERE to pick up the campaign cold.** How to establish a real baseline (and why
  the numbers in it must not be trusted as current), the primary target with its ready
  queue, the drained/refuted ledger, the settled rev model, and the 13 instrument hazards
  — every one of which produced a *clean wrong answer* rather than an obvious failure.

### 2026-08-03 results (waves EB→EE, 43,848 → ~43,872 matched · 254 → 255 units)

Ten lanes. The metric moved modestly; **six lanes returned a correction to an instrument
or to a prior lane's claim**, three of them refuting the premise they were dispatched on.
Read the verdict before re-opening any of these.

- ★ [decomp/OBJDIFF_DIFF_VS_REPORT_SETTLED_2026-08-03.md](decomp/OBJDIFF_DIFF_VS_REPORT_SETTLED_2026-08-03.md) —
  **SETTLED: `objdiff-cli diff` and `report generate` disagree on ZERO rows for real.**
  The "64% disagree" was a **field-pairing error**: "normalized" names two orthogonal axes
  (`diff`'s is *relocation*-normalized; `report`'s `mpn` is *arg-penalty-excluded*), and
  **`diff` never emits `mpn` at all** — so the reported sign was *arithmetically forced* by
  the field chosen. The 14.75 pp residue is one flag (`ppc.calculatePoolRelocations`).
  **Conversion rule:** `diff.normalized_match_percent == report.fuzzy_match_percent` exactly
  under `-c functionRelocDiffs=none -c ppc.calculatePoolRelocations=false`; **`mpn` is NOT
  derivable from any `diff` field** ⇒ a sub-100 `run_objdiff` reading never proves a row
  unmatched. `run_objdiff` was aligned to the grader at `131c723d` — **never compare a
  reading across that commit**. ⚠ `diff_inspect.py`/`stack_layout.py` deliberately stay
  **unaligned**: `functionRelocDiffs=none` masks wrong `bl` callees, and defect-visibility
  beats grader-alignment for a diagnosis tool. **The two-ruler split is intentional.**
- ★ [decomp/EC3_DEFECT_DENSITY_BY_FUZZY_2026-08-03.md](decomp/EC3_DEFECT_DENSITY_BY_FUZZY_2026-08-03.md)
  + `decomp/defect-signature-census-EC3.tsv` — **defect density by fuzzy stratum over the
  FULL population (1,644 named charged rows).** Codegen share rises monotonically with fuzzy
  (~60×), but ⛔ **"the real source defects are in the LOW-% rows" is REFUTED**: `<40` is only
  62.1% source-shaped (37.3% of it is map/foreign + unwritten stubs) vs **90.9%** in the
  middle, and `≥99` is still 61.1% source-shaped at a median of **2** mismatches vs **40**.
  ⇒ **Rank by DEFECT SIGNATURE, not by fuzzy%; the fix is the FILTER, not a reversed sort.**
  Resolves the apparent conflict with "crossing probability falls with fuzzy" — density and
  work-to-cross are different questions; both collapse to *work the high-fuzzy band, filtered
  by signature*.
- ★★ **The body-port class is THE vein** — `decomp/bodyport-queue-EE2.tsv`.
  `SOURCE_INSDEL` 549 rows/289,752 B **+** `SOURCE_CALLCOUNT` 462/267,024 B =
  **1,011 rows / 556,776 B — 61.5% of named charged rows, 73.1% of charged bytes.**
  ⛔ **`SOURCE_INSDEL` alone is the WRONG SCOPE** (drops 46%: one root cause splits across
  both classes — a missing *argument copy* surfaces as call-count, not insert/delete).
  ⛔ **No cheap targeting filter exists** — `Load`/`Save`/`Copy` enrich only **1.31×** and the
  class spans 385 units. A per-row vein read off retail bytes. Queue: 43 rows, fuzzy ≥97 and
  ≥500 B, **66,088 B / +0.6183 pp if all cross**.
- [decomp/UNIT_COMPLETION_FRONTIER_EB3_2026-08-03.md](decomp/UNIT_COMPLETION_FRONTIER_EB3_2026-08-03.md)
  + [decomp/EB3_COMPLETABLE_FRONTIER_FINDINGS_2026-08-03.md](decomp/EB3_COMPLETABLE_FRONTIER_FINDINGS_2026-08-03.md) —
  re-census: source-only ceiling **253 → 293**, COMPLETABLE **39, not ~6**. ★★★ **The ceiling
  MOVES — and lane EC-2 then moved it DOWN to 290, so it is NOT MONOTONIC.** Never treat a
  prior lane's ceiling as a floor; re-measure it like `total_code`.
  ⛔ Statement-order permutation is **INERT** (10 variants, all byte-identical — MSVC /O1
  normalises independent assignment order).
- [decomp/EC2_MISATTRIBUTION_SIZED_2026-08-03.md](decomp/EC2_MISATTRIBUTION_SIZED_2026-08-03.md)
  + [decomp/EC4_SLIVER_PIN_CLASS_SIZED_BINARY_WIDE_2026-08-03.md](decomp/EC4_SLIVER_PIN_CLASS_SIZED_BINARY_WIDE_2026-08-03.md) —
  **misattributed rows, sized twice; read EC-4 second, it CORRECTS EC-2.** EC-2's
  "WHOLE-block pins are 80% foreign / 9.58× enriched" is **n=17 and CONFOUNDED BY ISOLATION**
  — the control was **5× less isolated** than the population it nulled; standardised, **1.23×**
  with no stratum significant. ★★★ The proof: at `iso=0%`, rows **byte-equal to retail —
  therefore provably attributed correctly — read 86.75% "foreign"**, because with no LTCG
  `.text` groups TUs **by subsystem** and a sliver pin is nothing but a TU boundary.
  ⛔ MID blockers are **0.94% and 2.33× enriched, NOT 0.0%** (that was an n=31 artifact).
  Rescued: a same-source-**directory** oracle reads 2.63× standardised ⇒ **~36 real rows
  binary-wide (3.16%)**. ⛔ **Convertibility is ZERO — do not make this a standing sweep.**
- **Extent-carving vein DRAINED** (`5555db76`) — ★★★ the celebrated **"null 0/15,576" could
  not fire**: its condition (`base_size > claimed_size`) is unsatisfiable on an already-matched
  row, so the zero was *structural*, not empirical. The rule that actually worked has two
  unstated witnesses (**no exact retail `.pdata` record** + **last instruction is not a
  terminator**), 26/26, rejecting 17 of 21 candidates. Symmetric direction is **empty**
  (`OVER_PINNED = 0` binary-wide); 400 W2 hits reduce to **10 named+charged, none able to
  cross**. Tooling: `scripts/harvest/extent_sweep/`. **Do not fund another extent lane.**
- **Uniform-immediate-delta class CLOSED** — `decomp/uniform-immediate-delta-classification-ED2.tsv`.
  47 rows / 9,268 B ⇒ **only 2 rows / 900 B real**; 23 (49%) are ICF fold aliases, 16 STL
  stride, 3 stack codegen. ★★★ **The r31 trap eats the BIGGEST rows** — MSVC establishes a
  **second frame pointer** (`subi r31,r1,N`), so "check for r1" is necessary but
  **insufficient**; the 980 B `CharIKHand::Load` is a `String` temp at frame+104 vs +120.
  ⛔ The 16 `_M_insert_overflow_aux` rows do **not** resolve to wrong `sizeof(T)` — **retail's
  own immediates contradict themselves** (`NavItem` 12/32/48/96) ⇒ fold aliases, no class to
  name. ⚠ "47" is a **subset**; the full uniform-delta population is **91 rows / 32,992 B**.
- ★★ **The rev dialect is THREE dialects, and is a per-TU READING OFF TARGET BYTES, never a
  rule** (landed `044ffc1a`, `fa65ca2a`; +6 / +8 / +5 matched across three lanes).
  Retail **never constructs** a `BinStreamRev` — it **CASTS** a raw `BinStream&` (`??_R0`
  count **0**, vs 1 each for `BinStream`/`MemStream`/`FileStream`, with `/GR` on).
  **(1)** `lwz`+`cmpwi` on a **stack slot** ⇒ *no storage at all* — the static burns a
  callee-saved register and inflates the frame; **(2)** `lhz`+`cmplwi` off a **base register**
  ⇒ keep one aggregate; **(3)** two separate statics (CharHair). Our source's
  `__declspec(align(4))` **predicts nothing**. ⚠⚠ **`RB3_OBJPTR_INLINE_OWNER_CTOR` is
  TU-WIDE**: the define that closes a `Load` **destroys constructor rows in the same TU** —
  observed in *both* polarities, and **the whole-binary aggregate read POSITIVE every time**.
  Remedy is per-site 1-arg/2-arg `ObjPtr` spelling. ★ The `revB` gate is load-bearing:
  keeping `d >>` for **container** reads preserved 412 B of byte-exact retail code that
  DY-1's `bs >>` conversion would have deleted.

### Active worklists (open work to pull from)

- [plans/auto03-sourceless-guard-funclets-2026-08-02.md](plans/auto03-sourceless-guard-funclets-2026-08-02.md) —
  the **75 source-less `auto_03_*` guard funclets** in 15 units, sized and identified
  (lane DA-4). Pin geometry is free everywhere (0–12 B gaps); identification is the only
  cost. **Tier 1 = `auto_03_82B7EFBC` is `TourChallengeResultsPanel.cpp`** — header already
  in-tree, 120-line Wii oracle, span hard-bracketed at both ends. ⚠ **50.7% of the pool is
  RBN audition code with NO oracle** (rb3-Wii, DC3 and our tree all return zero) — the
  biggest half is the least tractable, so budget it as reconstruction, not a port.
- [plans/lane-da4-rndspline-mispair-2026-08-02.md](plans/lane-da4-rndspline-mispair-2026-08-02.md) —
  `?Copy@RndSpline@@` @ `0x8247a6c0` is a **CONFIRMED map defect** (it is
  `RndLine::SyncProperty`; interns `"width"`, `PropSync`s `mWidth` at `this-0x34` on
  compiler-authoritative layout). REPORTED, NOT EDITED. ⛔ **The repair is blocked by a
  name collision at `0x8247b638`** and must be one atomic edit; 5 more scrambled thunks in
  `0x8247b620–0x8247b810` are flagged but unadjudicated.

- [plans/band3-port-worklist-loose.md](plans/band3-port-worklist-loose.md) — loose 301-fn
  game-code worklist (0.85 precision), for ws2 regen.
- [plans/sysnet-port-worklist.md](plans/sysnet-port-worklist.md) — strict 290-fn engine+netcode
  worklist (0.967 precision; 46 safe-first core).
- [plans/sysnet-port-worklist-loose.md](plans/sysnet-port-worklist-loose.md) — loose 474-fn
  engine+netcode worklist (BSim 10-15, 0.85 precision).
- [plans/workstreams-2026-07-02/ws2-worklist-regen.md](plans/workstreams-2026-07-02/ws2-worklist-regen.md) — WS2 worklist regen, 775-candidate worklist, open.
- [plans/workstreams-2026-07-02/ws5-caseb-campaign.md](plans/workstreams-2026-07-02/ws5-caseb-campaign.md) — WS5 case-B campaign, partially executed, remaining ids open.
- [plans/workstreams-2026-07-02/ws7-dead-lever-reaudit.md](plans/workstreams-2026-07-02/ws7-dead-lever-reaudit.md) — dead-lever re-audit: 2 CONFIRMED_DEAD, 5 PARTIAL_REOPEN.
- [decomp/handoff/port-frontier-2026-07-02-plan.md](decomp/handoff/port-frontier-2026-07-02-plan.md) — port-frontier wave: 9 TUs ranked with port/pin specs (owner-WIP cautions).
- [decomp/handoff/w5-plan-2026-07-02.md](decomp/handoff/w5-plan-2026-07-02.md) — Wave-5 plan: 7 Opus port lanes + 1 Sonnet pins lane roster + SOP.

---

## 2. Evergreen references

### Build & config formats

- [config.md](config.md) — dtk config format (banner: real config in `config/45410914/config.json`).
- [objects.md](objects.md) — objects.json format + this repo's NonMatching / splits pinning workflow.
- [splits.md](splits.md) — splits.txt per-source-file section-range format.
- [symbols.md](symbols.md) — symbols.txt format (mangled names, addresses, attributes).
- [dependencies.md](dependencies.md) — toolchain deps (banner: sibling forks at fixed paths, not auto-download).
- [getting_started.md](getting_started.md) — [HIST] bootstrap (superseded; project already bootstrapped).
- [github_actions.md](github_actions.md) — [HIST] template CI (the real workflow ran once and failed; verify via `gh`).
- [reference/DATABASE_SCHEMA.md](reference/DATABASE_SCHEMA.md) — decomp.db SQLite schema.
- [reference/FREE60_XEX_FORMAT.md](reference/FREE60_XEX_FORMAT.md) — [HIST] archived XEX format reference.
- [decomp/OBJECT_MATCHING.md](decomp/OBJECT_MATCHING.md) — object-level match requirements: COFF sections, linking, what must match.

### Compiler / codegen references (MSVC X360, same flags as us)

- [decomp/MSVC_X360_REGALLOC.md](decomp/MSVC_X360_REGALLOC.md) — reverse-engineered register allocator. ⚠ Its "declaration order controls assignment" framing is **corrected**: order controls *stack slots*, and is measured inert for register-only swaps. Registers follow liveness/scheduling — see [decomp/patterns/fixable-liveness.md](decomp/patterns/fixable-liveness.md).
- ★ [decomp/patterns/fixable-liveness.md](decomp/patterns/fixable-liveness.md) — **register swaps are symptoms, not causes.** The five levers that actually move them, the negative-results table (12+ byte-identical variants; two zero-gain sweeps), the **Triage Split** that decides which functions are worth opening at all, and the three-part floor-evidence standard. Read before any `REGISTER_SWAP` residual.
- [decomp/TECHNICAL_NOTES.md](decomp/TECHNICAL_NOTES.md) — compiler patterns & session lessons: regalloc, static init, control flow, merged fns.
- [decomp/XBOX360_FLOATING_POINT_CODEGEN.md](decomp/XBOX360_FLOATING_POINT_CODEGEN.md) — FP codegen: `/fp:` flags, contraction pragmas, FPU patterns.
- ★ [decomp/patterns/fixable-fp-reassociation.md](decomp/patterns/fixable-fp-reassociation.md) — **under `/fp:fast`, MSVC reassociates only what you did NOT group: explicit parentheses are the barrier, not term order.** A bare sum reaches 2 of 6 product orders, parenthesised reaches all 6. Refutes DR-3's "term order is not source-controllable" (its *measurement* was right, its conclusion wrong — and as written into `math/Mtx.h` it turned away two lanes) and "named temporaries are inert" (site-specific in **both** directions). Carries the verified retail-chain decode for all three sites, the measured-inert list, and the `Plane::Dot`/`CollidePlane` constraint that makes per-call-site the only zero-blast-radius lever. ⚠ Records three claims in its own source commit that did **not** reproduce.
- [decomp/PRAGMA_INDEX.md](decomp/PRAGMA_INDEX.md) — navigation index for the pragma doc suite.
- [decomp/PRAGMA_CODEGEN_SUMMARY.md](decomp/PRAGMA_CODEGEN_SUMMARY.md) — quick reference for pragmas affecting instruction selection.
- [decomp/PRAGMA_MATCHING_CHECKLIST.md](decomp/PRAGMA_MATCHING_CHECKLIST.md) — step-by-step guide to applying pragmas.
- [decomp/XBOX360_PRAGMA_REFERENCE.md](decomp/XBOX360_PRAGMA_REFERENCE.md) — complete X360 pragma reference: scope rules, flag interactions.

### Matching methodology

- [decomp/patterns/INDEX.md](decomp/patterns/INDEX.md) — **master pattern index** (fixable/unfixable/harmful codegen patterns; start here for a specific mismatch).
- ★ [decomp/INSTRUMENT_DESIGN.md](decomp/INSTRUMENT_DESIGN.md) — **how to build a scanner/control/census that can actually FAIL.** Eight failure shapes with worked DC..DG cases (vacuous control, silently-vacuous scanner, one-label classifier, base-rate error, improvement-as-false-positive, unsettled read, stale census, structurally-blind metric — including that the match build never links) + 15 design rules and the in-repo fixtures. Read before designing any instrument whose zeros or confirmations you plan to act on.
- [decomp/playbooks/README.md](decomp/playbooks/README.md) — playbook overview + shared invariants.
- [decomp/playbooks/bodyport-wave.md](decomp/playbooks/bodyport-wave.md) — body-port campaign playbook.
- [decomp/playbooks/hasreal-grind.md](decomp/playbooks/hasreal-grind.md) — HAS_REAL near-miss grind playbook.
- [decomp/playbooks/nearmiss-harvest.md](decomp/playbooks/nearmiss-harvest.md) — evaluation-order-sculpting harvest waves (96–99.99% named fns; local-.cpp-only lanes; technique catalog + wall taxonomy).
- [decomp/playbooks/offset-drift-sweep.md](decomp/playbooks/offset-drift-sweep.md) — mechanical layout/header-drift sweep (85–99.99%; the header-edit complement of nearmiss-harvest; one fix closes many fns; recon-before-edit discipline).
- [decomp/playbooks/levers-that-pay.md](decomp/playbooks/levers-that-pay.md) — field guide to the matching levers of waves DC..DG (2026-08-02, `4c1ae369..362217af`, +63 matched / units-at-100 149→176): precondition / trap / worked example / LIVE-DRAINED-REFUTED state for storage-class divergence, LOAD_REVS dialect, SAVE_OBJ stubs, coupled halves + boundary moves, per-instantiation inlining traits, ObjPtr ctor ordering, container member types, and the retail-shape layout port. Carries the reachable-ceiling targeting partition and a drained/refuted ledger. Pairs with [decomp/INSTRUMENT_DESIGN.md](decomp/INSTRUMENT_DESIGN.md) — that one is control discipline, this one is what to try.
- [decomp/patterns/false-layout-drift.md](decomp/patterns/false-layout-drift.md) — offset diffs that are NOT layout bugs (anchor-bias, vbase mirage, diagonal pairing); rule out before editing a header.
- [decomp/patterns/milo-debug-force-define.md](decomp/patterns/milo-debug-force-define.md) — the `MILO_DEBUG` force-define trap: what it really gates (**not** `MILO_ASSERT` — that is `HX_NATIVE`), the full census, the `HX_NATIVE` house fix pattern, the measured **−21** blanket-removal control, and the TU-local-`#undef` ODR hazard.
- [decomp/EH_FUNCLET_CASCADE.md](decomp/EH_FUNCLET_CASCADE.md) — **the EH-funclet cascade rule** and
  how to read a guard-bit timeline: a funclet flips on its parent's frame SIZE alone, the census
  (2,720 funclets / 1,004 parents, only ~13% source-blocked), the three failure shapes, the five
  root-cause families, and the two hazards found while diagnosing this campaign's own regressions —
  (1) when a macro takes over a behaviour, every prior hand-rolled emulation of it becomes a defect;
  (2) **comma form vs function call is an ARGUMENT-ORDER decision, not only a copying decision**
  (MSVC evaluates function args right-to-left, the comma operator left-to-right). Read alongside the
  corrected comment block in `src/system/os/Debug.h`.
  ★ Also carries the **mirror rule**: a funclet dropping 100.0 → 99.9 right after a *correct* parent
  edit is the parent's frame growing 16 bytes, counted twice — **it must not veto the parent fix**.
  Worked A/B on `ObjectDir::Iterate` / `fn_8274FEC8`, plus the free control (an edit that leaves the
  funclet at exactly 100.0 did not move the frame).
- [plans/funclet-cascade-lever-2026-07-25.md](plans/funclet-cascade-lever-2026-07-25.md) — [HIST]
  the 9-worktree funclet-lever campaign (map `__unwind$N` purge +124, COMDAT-scatter splits,
  scatter-include inlining collapse). ⚠ **§14 and §22 carry correction banners**: they predate
  `a2e737ab`, which moved the `MILO_WARN` copy into the macro (`MiloStripEval`, +34), so the
  per-call-site `String(x)` fix they prescribe is now itself a defect (`eab1c3f6`).
- [decomp/patterns/at-limit-systemic.md](decomp/patterns/at-limit-systemic.md) — systemic at-limit
  classes, incl. §8 the **MSVC temp-slot ASSIGNMENT permutation** wall with a cheap prefilter
  (`set(target slots) == set(base slots)`) so you can classify BEFORE investing.
- [decomp/UPSTREAM_PORT_WORKFLOW.md](decomp/UPSTREAM_PORT_WORKFLOW.md) — porting matching impls from DC3 / rb3-Wii when theirs is closer. ⚠ Now carries the **upstream-is-not-a-correctness-certificate** anti-pattern: `../og-dc3-decomp` (= `rjkiv/dc3-decomp`) still has the `ObjectDir::Iterate` type-filter bug we fixed in `dd144927`/`5260e280`. When a shared-engine function's *logic* is at stake, check `../rb3` too and trust agreement between two independent trees over the higher match%.
- [decomp/callgraph-triangulation.md](decomp/callgraph-triangulation.md) — the batch rb3↔dc3 identification oracle, **plus** (2026-08-04) the manual intra-binary variant: pin an unnamed target function by reading its `bl` out of a **100%-matched caller**'s aligned instruction table, cross-check by callee-set signature, and the refuted-candidate discipline (`fn_82752668` was NOT `Iterate`; pinning it would have manufactured a permanently unfixable diff).
- [decomp/identity-transfer.md](decomp/identity-transfer.md) — per-function identity transfer for ICF-scattered TUs (case-A vs case-B).
- [decomp/pin-candidates.md](decomp/pin-candidates.md) — unified oracle→pin ranker: 5 oracle sources → consensus tiers + ranked splits wave.
- [decomp/callgraph-triangulation.md](decomp/callgraph-triangulation.md) — vote rb3 anonymous fns via anchor callsites vs dc3 named fns.
- [decomp/rtti-vtable-transitivity.md](decomp/rtti-vtable-transitivity.md) — transfer dc3 vtable slot names onto rb3 anonymous fns via RTTI+vtable.
- [decomp/handoff/objdiff-caseb-fork-banked.md](decomp/handoff/objdiff-caseb-fork-banked.md) — banked objdiff fork: case-B cross-unit identity transfer + landing gate.
- [decomp/handoff/worktree-build-tooling-findings-2026-07-01.md](decomp/handoff/worktree-build-tooling-findings-2026-07-01.md) — worktree build findings: SIGPIPE fix, scoped prime, PCH reflink (refuted).
- [plans/engine-reuse-and-asset-rendering.md](plans/engine-reuse-and-asset-rendering.md) — proof DC3 engine renders RB3-360 assets; why decomp value is in game layer (CLAUDE.md-referenced).
- [plans/coupled-base-and-body-port-playbook.md](plans/coupled-base-and-body-port-playbook.md) — [HIST] reference playbook: coupled-base (family blast) vs body-port classes.

---

## 3. Tooling

### Hardware / live debugging (RB3Enhanced on the console)

- [tools/LIVE-DEBUG-RUNBOOK.md](tools/LIVE-DEBUG-RUNBOOK.md) — **the live-debugging runbook**:
  console facts + topology (direct `192.168.8.180`; the relay-era
  `tools/oss-xbox-build/xbox.sh` is deprecated), `build-si.sh` edit→run loop,
  observability channels (XBDM notify / RB3E UDP / HTTP), `/execute` live DTA
  introspection (returns evaluation results), `xdbg` crash capture, recovery ladder.
- [../tools/oss-xbox-build/BUILD-AND-DEPLOY.md](../tools/oss-xbox-build/BUILD-AND-DEPLOY.md) —
  build/pack/deploy pipeline internals (XDK-free compile, load-critical `xextool -m d -c c`
  compress step, 8 hard-won gotchas).
- [plans/si-hw-fix/README.md](plans/si-hw-fix/README.md) — SI hardware campaign entry point:
  `DEBUG-WORKFLOW.md` (crash→analyze→hook-fix loop + crash ledger), load-blocker
  root-cause record, worked crash traces.

### Ghidra / decompiler

- [tools/GHIDRA.md](tools/GHIDRA.md) — primary Ghidra MCP integration doc (banner: DC3-map assumptions relabeled; RB3 uses fingerprint/apply_symbols pipeline).
- [tools/GHIDRA_SETUP.md](tools/GHIDRA_SETUP.md) — quick Ghidra setup + RB3 disclaimers + XEX loader integration.
- [tools/GHIDRA_MANUAL_SETUP.md](tools/GHIDRA_MANUAL_SETUP.md) — GUI-only setup (no MCP) for manual import/analysis.
- [tools/XEXLOADERWV.md](tools/XEXLOADERWV.md) — XEXLoaderWV Ghidra extension for X360 binary loading.

### objdiff / analysis / orchestrator

- [decomp/TOOLING.md](decomp/TOOLING.md) — ★ **the audited tooling inventory (2026-07-29)**:
  ~350 tools across `tools/`, `scripts/`, `scripts/harvest/`, each actually invoked and
  status-graded; routing table; ground-truth artifacts (incl. `band.exe`); the
  stale-build-dir offender list; the known-defective set; verified `configure.py` patcher
  wiring and MCP tool list.
- [plans/claude-md-proposed-2026-07-29.md](plans/claude-md-proposed-2026-07-29.md) —
  **PROPOSAL, NOT APPLIED**: 7 ready-to-apply CLAUDE.md amendments from the 2026-07-29
  audit (band.exe oracle, stale-artifact hazard, the "100% ≠ correct" defect class,
  TOOLING.md pointer, skills enumeration, local-static lever + early-return anti-lever,
  `land.sh` deletion hole), each with evidence. Also lists what was re-verified as
  already correct.
- [decomp/MAP_NAME_INJECTIVITY.md](decomp/MAP_NAME_INJECTIVITY.md) — ★ **the map's NAME-injectivity
  invariant and the ninja gate that enforces it** (`tools/map_name_injectivity.py`,
  `map_name_injectivity_check`, 2026-08-13). Why one mangled name at two VAs is a live path to
  a byte-exact witness against the WRONG target body; why the per-unit checks
  (`icf_class_bijection.py`, `tu5_map_apply_fragment.py`) never caught it; the eight disproved
  name claims nulled and the two `__destroy_aux@ULevelData` addresses that are **still
  unadjudicated** — read that section before touching either. Carries a lane-J3 correction
  banner (gate reach, the 11th row at `name_check`, three cleared floor certifications) plus
  the `gated_map_write.py` P5/P7/Q4 cross-reference and a forward worklist. Companion:
  [decomp/handoff/laneJ2-at-limit-clearance-2026-08-13.md](decomp/handoff/laneJ2-at-limit-clearance-2026-08-13.md).
- [tools/INDEX.md](tools/INDEX.md) — **tool-selection index** (MCP orchestrator tools, Ghidra CLI, analysis utilities).
- [tools/REFERENCE.md](tools/REFERENCE.md) — command reference for symbol lookup (banner: no RB3 map; corrected pointers).
- [tools/WORKFLOW.md](tools/WORKFLOW.md) — decomp tool workflow narratives (new fns, near-matches, pattern analysis).
- [tools/UNICORN_FUNCTION_RUNNER.md](tools/UNICORN_FUNCTION_RUNNER.md) — Unicorn differential function execution (PPC32 BE emulation).
- [tools/objdiff/CLI_OPTIONS.md](tools/objdiff/CLI_OPTIONS.md) — objdiff-cli options, output formats, pattern detection.
- [tools/objdiff/USAGE.md](tools/objdiff/USAGE.md) — extended objdiff-cli reference (report queries, analysis, markdown).
- [tools/objdiff/JSON_EXTENSIONS.md](tools/objdiff/JSON_EXTENSIONS.md) — milohax fork extensions: data-symbol diffs + CFG structures.
- [tools/objdiff/LEARNINGS.md](tools/objdiff/LEARNINGS.md) — patterns, diagnostics, fixability decision trees from objdiff work.
- [tools/objdiff/AGENT_WORKFLOW.md](tools/objdiff/AGENT_WORKFLOW.md) — [HIST] DC3-heritage design note (live workflow is the orchestrator MCP tools).
- [tools/orchestrator/INCREMENTAL_BUILDS.md](tools/orchestrator/INCREMENTAL_BUILDS.md) — incremental vs full build strategy + metrics.

### Harvest / identification scanners (`scripts/harvest/`, read-only unless noted)

> **STATUS (2026-07-29):** there IS now a standalone doc —
> **[decomp/TOOLING.md](decomp/TOOLING.md)**, an audited inventory of all ~350
> tools (each one actually invoked, not just read). The notes below stay as the
> curated highlights; TOOLING.md is the complete table plus the defect list.

Each tool's module docstring is its detailed reference, and they are
long and evidence-carrying. Read the docstring before running one; several encode
a refutation you would otherwise re-derive. Newest first.

- `thunk_shape.py` — **the single definition** of the MSVC-X360 adjustor-thunk
  primitives: `shape()` (decode the instruction sequence — a tail call is not a
  thunk), `td()`/`prefix()` (name encoding, W-form **and** `$4`-form), `norm()`
  (`??_G`/`??_E` fold). Imported by `thunk_identity_namer.py` and
  `dupname_identity_resolver.py` so they cannot drift apart again.
- `thunk_identity_namer.py` — names retail adjustor thunks **by construction**: the
  mangled name is a total function of (callee prefix, vtordisp, this-adjust), then
  confirmed against the unique symbol in the owning obj with that exact encoding.
  Emits proposals; applies nothing. Landed `26284d0d` (+18).
- `thunk_edge_audit.py` — thunk **scope must equal callee scope**, a self-consistency
  oracle over the existing map (the axiom is verified on our own objs: 7522/7522 for
  genuine adjustor mangling). Surfaces hard contradictions, not heuristics.
- `dupname_identity_resolver.py` — what is ACTUALLY at a duplicate-name VA, resolved
  from **trust-gated** callees to a fixpoint. ⚠ Its residue path once invented a
  41-class phantom "missing virtual" worklist; rewritten 2026-07-29 on
  `thunk_shape.py`, with ungated verdicts now prefixed `UNGATED_` and a loud banner.
  Read the "WHY THE RESIDUE PATH WAS REWRITTEN" block in its docstring.
- `dupname_rebijection.py` — makes the map injective on NAME (a linked image resolves
  each COMDAT/extern name to exactly one VA). Applied in `560dffb3` (−105, user-approved).
- `run_interleave_scan.py` — finds **uniform-stride runs** in retail `.text` (one TU's
  same-shaped COMDAT family, e.g. the `StaticClassName` block) whose `splits.txt`
  attribution is not constant across the run ⇒ a foreign pin reached into the middle
  of another unit's COMDAT block. A mis-carve detector that % cannot see.
- `gap_content_evidence.py` — proves gap ownership by **content** rather than
  adjacency: decodes every `lbl_<VA>` data reference in dtk's auto-carve asm as a C
  string and classifies SELF (a source path naming the claimant unit — the hardest
  ownership evidence available, and address-independent) / SRC / STR.
- `localstatic_tu_census.py` — per-unit done-vs-straggler census for the local-static
  conversion lever. ⚠ Counting only `static Symbol` massively over-reports; and see
  the corrected predicate in its docstring (parent-at-100, not whole-TU).
- `ls_guard_timeline.py` — reads a target VA's function-local-static timeline in
  guard-BIT order (= source declaration order) with each static's data address and
  string literal, which turns a conversion into a mechanical transcription.
- `localstatic_symbol_audit.py` — the identical-body (`StaticClassName` / `Type()`)
  family audit: the **string operand is the only sound discriminator**, because all
  453 bodies are identical modulo relocations and objdiff's normalized diff ignores
  those. Re-derives in ~40 s; re-run it rather than quoting its counts from a doc.
- `resolve_splits_union.py` / `resolve_json_union.py` / `land.sh` — the landing path.
  ⚠ The splits resolver is a line-union that **cannot propagate deletions**; see the
  known-traps list above.

### Permuter

- [permuter/INDEX.md](permuter/INDEX.md) — **C++ permuter doc index**: patterns, CLI, architecture, beam/hill-climb search.
- [permuter/guided-permuter.md](permuter/guided-permuter.md) — diagnosis-guided permutation using objdiff mismatches.
- [permuter/bsf-engine.md](permuter/bsf-engine.md) — BSF register-allocation tracing for guided declaration reordering.
- [permuter/evolution/OVERVIEW.md](permuter/evolution/OVERVIEW.md) — permuter architecture upgrade (SourceEditor, ast_queries); phases 1-3.
- [decomp/patterns/PERMUTER_ROI_ANALYSIS.md](decomp/patterns/PERMUTER_ROI_ANALYSIS.md) — permuter coverage vs documented patterns; ROI rankings.

### LLM grind loop / OSS-model eval / training data — REMOVED FROM THIS REPO

> **STATUS (2026-08-13):** this section listed 12 docs covering the LLM grind loop, the
> OSS-model eval bench, and the training/reasoning corpus. **None of them exist on `main`'s
> history** — the training-data/reasoning-corpus effort docs were deliberately removed from
> this public repo, and the entries were left behind as dead links. They are **not** being
> resurrected here. The 12 dead entries were deleted on 2026-08-13 (lane RECOVER); recover the
> text from git history on an unmerged branch if you need it, or look in `decomp-synth`, which
> owns the grind loop itself.
>
> ⚠ One caveat for anyone re-adding: `--agent-tools` mode (documented by the removed
> `grind-agentic-tools.md`) is the mode that **moves a worktree's `.git` to a sidecar** and
> loses it if the run is killed — see the worktree-recovery box in `../CLAUDE.md`. That
> operational hazard is documented there, not here.

### VMX128 (Ghidra SLEIGH support)

- [vmx128/README.md](vmx128/README.md) — VMX128 Ghidra support overview; phases 1-4 (13,836 instructions validated).
- [vmx128/ISA_REFERENCE.md](vmx128/ISA_REFERENCE.md) — VMX128 instruction set reference.
- [vmx128/REGISTER_ENCODING.md](vmx128/REGISTER_ENCODING.md) — VMX128 7-bit register field encoding.
- [vmx128/VCMPBFP128_SEMANTICS.md](vmx128/VCMPBFP128_SEMANTICS.md) — vcmpbfp128 semantics (2-bit result codes per lane).
- [vmx128/GHIDRA_IMPLEMENTATION.md](vmx128/GHIDRA_IMPLEMENTATION.md) — all 77 instructions with full pcode semantics.
- [vmx128/DC3_VMX128_USAGE.md](vmx128/DC3_VMX128_USAGE.md) — DC3 binary VMX128 usage analysis (37,020 instructions).
- [vmx128/COMPARISON_REPORT.md](vmx128/COMPARISON_REPORT.md) — stock vs modified Ghidra validation on DC3.
- [vmx128/REFERENCE_SOURCES.md](vmx128/REFERENCE_SOURCES.md) — authoritative VMX128 doc sources + local clone paths.
- [vmx128/TESTING.md](vmx128/TESTING.md) — VMX128 Ghidra headless testing/validation guide.
- [vmx128/PLAN.md](vmx128/PLAN.md) · [vmx128/PHASE4_TODO.md](vmx128/PHASE4_TODO.md) · [vmx128/GESTURE_TARGETS.md](vmx128/GESTURE_TARGETS.md) · [vmx128/SESSION_CONTEXT.md](vmx128/SESSION_CONTEXT.md) · [vmx128/SESSION_HANDOFF.md](vmx128/SESSION_HANDOFF.md) — [HIST] plan/session snapshots.

---

## 4. Archives

Dated, append-only records. Descriptions preserved so the archive stays greppable; treat all
numbers and "current state" as frozen at the doc's date.

### 4a. Research — dated investigation records (all [HIST])

- [decomp/fuzzy-reconstruction-frontier-2026-06-21.md](decomp/fuzzy-reconstruction-frontier-2026-06-21.md) — state snapshot (9793 matched): body-divergence wall, oracle inventory, tooling.
- [decomp/near-miss-classification-2026-06-06.md](decomp/near-miss-classification-2026-06-06.md) — near-miss root-cause classification + ranked lever list.
- [decomp/partial-match-porting-strategy.md](decomp/partial-match-porting-strategy.md) — partial→100% conversion model; baseline 3919 matched.
- [decomp/matng-deferral.md](decomp/matng-deferral.md) — Mat_NG deferred: member reorder too risky for shared Mat.h.
- [decomp/string-layout-gap.md](decomp/string-layout-gap.md) — RESOLVED: String/FilePath layouts byte-identical to dc3; do NOT touch.
- [decomp/plans/fuzzy-locator-reconstruction-design.md](decomp/plans/fuzzy-locator-reconstruction-design.md) — locator-first reconstruction (pilot FALSIFIED per its own update).
- [decomp/plans/codegen-matcher-investment-prompt.md](decomp/plans/codegen-matcher-investment-prompt.md) — option-B codegen-matcher prompt (executed → KILL verdict).
- [decomp/plans/post-codegen-kill-streams-2026-06-30.md](decomp/plans/post-codegen-kill-streams-2026-06-30.md) — post-codegen-KILL streams; executed, +3..+9.
- [decomp/identity-transfer/PIPELINE-DESIGN.md](decomp/identity-transfer/PIPELINE-DESIGN.md) — identity-transfer pipeline design; bottleneck = source-port byte-exactness.
- [decomp/identity-transfer/B2-FINDINGS-oracle-wall.md](decomp/identity-transfer/B2-FINDINGS-oracle-wall.md) — B2: oracle VA misattribution dominant; vein thin.
- [decomp/identity-transfer/research/01-tooling-audit.md](decomp/identity-transfer/research/01-tooling-audit.md) — tooling audit; binding constraint = body divergence.
- [decomp/identity-transfer/research/02-objdiff-caseb-fork.md](decomp/identity-transfer/research/02-objdiff-caseb-fork.md) — objdiff case-B fork audit: do-no-harm passes; banked.
- [decomp/identity-transfer/research/03-backlog-inventory.md](decomp/identity-transfer/research/03-backlog-inventory.md) — backlog: 375 eligible game TUs, 975 realA case-A methods.
- [decomp/identity-transfer/research/04-sourceport-bottleneck.md](decomp/identity-transfer/research/04-sourceport-bottleneck.md) — source-port bottleneck: BandProfile 0/64 dissected.

The `decomp/research/` folder is a dense, dated investigation log (~100 files, 2026-06-10 →
2026-07-02) covering the body-port waves (w3-w13), hash_map cluster hunts, Handle-macro reveal
cascades, pin audits, and the structural-levers-exhausted capstone. Notable capstones/entry points:

- [decomp/research/2026-06-21-structural-levers-exhausted-capstone.md](decomp/research/2026-06-21-structural-levers-exhausted-capstone.md) — CAPSTONE: cheap + structural matching levers exhausted (June).
- [decomp/research/2026-06-22-classA-tupure-harvest-results.md](decomp/research/2026-06-22-classA-tupure-harvest-results.md) — Class-A TU-pure span harvest: +126 composed w3-w13; vein thinning.
- [decomp/research/2026-06-22-dc3-oracle-built-engine-naming-dead.md](decomp/research/2026-06-22-dc3-oracle-built-engine-naming-dead.md) — DC3↔RB3 BinDiff oracle built; engine naming dead for strict, alive for fuzzy.
- [decomp/research/2026-06-23-dc3-drain-and-sonnet-opus-pipeline.md](decomp/research/2026-06-23-dc3-drain-and-sonnet-opus-pipeline.md) — DC3-oracle drain exhausted at +46 strict; Sonnet/Opus pipeline.
- [decomp/research/2026-06-30-nearmiss-codegen-inventory.md](decomp/research/2026-06-30-nearmiss-codegen-inventory.md) — near-miss codegen inventory (feeds post-codegen kill streams).
- [decomp/research/2026-06-21-dc3-engine-oracle-feasibility.md](decomp/research/2026-06-21-dc3-engine-oracle-feasibility.md) — DC3 engine body-oracle feasibility GO; game-layer Wii wall.

The remaining `decomp/research/*` files are per-lever / per-TU scout logs — grep the folder by
TU name (SongMgr, SongStatusMgr, BandSongMgr, UIComponent, Waypoint, Campaign, SavedSetlist,
Handle-macro families, hash_map clusters) when reviving a specific investigation. Their
one-line descriptions are catalogued in the 2026-07-06 audit
(`~/tmp/docs-audit-2026-07-06.md`, "decomp/research" section).

### 4b. Handoff — per-task agent records (all [HIST] unless noted above)

Per-TU / per-wave landing records from the port campaigns (mostly 2026-07-01/02). Grep by TU:

- **[CURRENT] Cross-repo port of dc3's 2026-08-04 regswap/AT_LIMIT sweep:**
  [handoff/dc3-regswap-sweep-port-2026-08-04.md](decomp/handoff/dc3-regswap-sweep-port-2026-08-04.md)
  — 4 live bugs ported (OSC parser ×3, Locale MemPopTemp + altCfg, Trans
  preserve-scale bit); `Locale::Init` identified as `fn_827C9AF8` and the
  DC3-era devkit-override block gated behind `HX_NATIVE` (**67.9% → 87.2%**).
  Three trap-references worth reading before any cross-repo port:
  (a) the **dc3 orchestrator MCP silently measures dc3** when handed an
  rb3-xenon `project_dir` — it returned dc3's 93.3% for a function whose RB3
  value is 92.2%; (b) **`MILO_LOG` and `MILO_NOTIFY` expand identically** in the
  match build, so dc3's diagnostic-macro lever is byte-neutral and unfalsifiable
  here; (c) the **parameter-home-area inline-counting lever DOES survive retail
  `/O1`** (measured: 8,711/82,230 fns = 10.59%, 23,378 dead home stores, vs
  10.34% in dc3's debug build) — the prediction that `/O1` would elide them is
  wrong. Also: `NgMat::RefreshState`'s reciprocal-multiply numeric divergence is
  **reproduced on RB3 retail** (4 target `fdivs` vs our 2 reciprocals + 4
  `fmuls`) and is **not `/fp:`-gated** (`/fp:precise` *and* `/fp:strict` both
  byte-identical, control run).

- CharClipGroup: [handoff/charclipgroup-flip-RESULT-2026-07-02.md](decomp/handoff/charclipgroup-flip-RESULT-2026-07-02.md) · [handoff/charclipgroup-objvector-flip-READY.md](decomp/handoff/charclipgroup-objvector-flip-READY.md) (banner: superseded by RESULT).
- Member-delta / MetaPanel / span waves: [handoff/exec-r1-member-delta-run-2026-07-02.md](decomp/handoff/exec-r1-member-delta-run-2026-07-02.md) · [handoff/exec-r2-metapanel-run-2026-07-02.md](decomp/handoff/exec-r2-metapanel-run-2026-07-02.md) · [handoff/exec-r3-span-confirm-run-2026-07-02.md](decomp/handoff/exec-r3-span-confirm-run-2026-07-02.md).
- ws1/ws3/ws4 exec: [handoff/exec-ws1-waveA-run-2026-07-02.md](decomp/handoff/exec-ws1-waveA-run-2026-07-02.md) · [handoff/exec-ws1-waveA-p1-verdicts.md](decomp/handoff/exec-ws1-waveA-p1-verdicts.md) · [handoff/exec-ws3-optionc-run-2026-07-02.md](decomp/handoff/exec-ws3-optionc-run-2026-07-02.md) · [handoff/exec-ws4-round3-run-2026-07-02.md](decomp/handoff/exec-ws4-round3-run-2026-07-02.md) · [handoff/round3-shared-header-followups-2026-07-02.md](decomp/handoff/round3-shared-header-followups-2026-07-02.md).
- ws3 p2-p4: [handoff/ws3-p2-motionblur-softparticles-2026-07-02.md](decomp/handoff/ws3-p2-motionblur-softparticles-2026-07-02.md) · [handoff/ws3-p3-moggclip-2026-07-02.md](decomp/handoff/ws3-p3-moggclip-2026-07-02.md) · [handoff/ws3-p4-navlist-scantool-2026-07-02.md](decomp/handoff/ws3-p4-navlist-scantool-2026-07-02.md).
- BeatMatcher/BeatMatchController: [handoff/w3-port-beatmatcher-handoff.md](decomp/handoff/w3-port-beatmatcher-handoff.md) (banner: verified & pinned, 1 revert) · [handoff/port-beatmatchcontroller-handoff.md](decomp/handoff/port-beatmatchcontroller-handoff.md).
- w3 ports: [handoff/w3-joypadcontroller-handoff.md](decomp/handoff/w3-joypadcontroller-handoff.md) · [handoff/w3-pins-handoff.md](decomp/handoff/w3-pins-handoff.md) · [handoff/w3-port-gemtrackdir-handoff.md](decomp/handoff/w3-port-gemtrackdir-handoff.md) · [handoff/w3-sliptrack-handoff.md](decomp/handoff/w3-sliptrack-handoff.md).
- w5 wave: [handoff/w5-closure-2026-07-02.md](decomp/handoff/w5-closure-2026-07-02.md) · [handoff/w5-bandlist-handoff.md](decomp/handoff/w5-bandlist-handoff.md) · [handoff/w5-baseguitartrackwatcherimpl-handoff.md](decomp/handoff/w5-baseguitartrackwatcherimpl-handoff.md) · [handoff/w5-endingbonus-handoff.md](decomp/handoff/w5-endingbonus-handoff.md) · [handoff/w5-notetube-handoff.md](decomp/handoff/w5-notetube-handoff.md) · [handoff/w5-sfx-handoff.md](decomp/handoff/w5-sfx-handoff.md) · [handoff/w5-tambourinemanager-handoff.md](decomp/handoff/w5-tambourinemanager-handoff.md) · [handoff/w5-trackwatcher-handoff.md](decomp/handoff/w5-trackwatcher-handoff.md).
- Port-frontier TUs: [handoff/port-ADSR-port-frontier-2026-07-02.md](decomp/handoff/port-ADSR-port-frontier-2026-07-02.md) · [handoff/port-bandpatchmesh-handoff.md](decomp/handoff/port-bandpatchmesh-handoff.md) · [handoff/port-MemMgr-port-frontier-2026-07-02.md](decomp/handoff/port-MemMgr-port-frontier-2026-07-02.md) · [handoff/port-songdata-handoff.md](decomp/handoff/port-songdata-handoff.md) · [handoff/port-songparser-handoff.md](decomp/handoff/port-songparser-handoff.md) · [handoff/port-trackwatcherimpl-handoff.md](decomp/handoff/port-trackwatcherimpl-handoff.md) · [handoff/port-TrackWatcherImpl-port-frontier-2026-07-02.md](decomp/handoff/port-TrackWatcherImpl-port-frontier-2026-07-02.md).
- Landing / reconcile: [handoff/land-bt-land-plan-2026-07-02.md](decomp/handoff/land-bt-land-plan-2026-07-02.md) · [handoff/land-vtd-land-plan-2026-07-02.md](decomp/handoff/land-vtd-land-plan-2026-07-02.md) · [handoff/tambourine-reconcile-2026-07-02.md](decomp/handoff/tambourine-reconcile-2026-07-02.md) · [handoff/verify-ab-reliability-2026-07-01.md](decomp/handoff/verify-ab-reliability-2026-07-01.md).

### 4c. Historical plans (executed / superseded)

- [plans/decomp-state-and-roadmap-2026-06-09.md](plans/decomp-state-and-roadmap-2026-06-09.md) — [HIST] state/roadmap (6568 matched; banner → frontier-workstreams).
- [plans/path-to-100.md](plans/path-to-100.md) — [HIST] original roadmap (394 matched; honest ceiling estimate).
- [plans/execution-schedule.md](plans/execution-schedule.md) — [HIST] dependency-aware roadmap superseding path-to-100.
- [plans/band3-port-worklist.md](plans/band3-port-worklist.md) — [HIST] strict 232-fn game-code worklist — drained.
- [plans/exploratory-techniques.md](plans/exploratory-techniques.md) — [HIST] identification POCs: callgraph, RTTI, vtable transitivity (+2,735 union).
- [plans/lto-vs-icf-investigation-2026-06-06.md](plans/lto-vs-icf-investigation-2026-06-06.md) — [HIST] VERDICT: retail XEX is NOT LTO/LTCG, only ICF (trap-reference).
- [plans/engine-baseclass-layout-bugs.md](plans/engine-baseclass-layout-bugs.md) — [HIST] foundational layout bugs: ObjRef/ObjPtr, ObjectDir, vbptr.
- [plans/objptr-family-relayout-migration.md](plans/objptr-family-relayout-migration.md) — [HIST] ObjRef/ObjPtr re-layout migration (233-file blast).
- [plans/objptr-regression-analysis-2026-05-30.md](plans/objptr-regression-analysis-2026-05-30.md) — [HIST] post-landing ObjPtr: +54 net, 4 regressed units.
- [plans/hmx-object-layout.md](plans/hmx-object-layout.md) — [HIST] Hmx::Object 0x2c→0x28 correction (landed).
- [plans/ui-base-layout-reconstruction.md](plans/ui-base-layout-reconstruction.md) — [HIST] UIComponent retail layout (0x140) reconstructed.
- [plans/structural-readiness-2026-06-03.md](plans/structural-readiness-2026-06-03.md) — [HIST] struct-layout readiness audit (4094-matched era). **Its `layout_fix_rank.py` fan-out counts are INFLATED toward struct evidence — read the 2026-08-17 correction banner before quoting any of them (task #114).**
- [plans/struct-offset-sweep.md](plans/struct-offset-sweep.md) — [HIST] engine near-misses 82% struct-offset bugs.
- [plans/recon-structural-levers-2026-05-29.md](plans/recon-structural-levers-2026-05-29.md) — [HIST] 5-lever survey: StlNodeAlloc, LightPreset, NgStats, SAVE_REVS, EH funclets.
- [plans/next-levers-2026-05-29.md](plans/next-levers-2026-05-29.md) — [HIST] post-strict-oracle lever ranking.
- [plans/next-wave-onediff-clusters.md](plans/next-wave-onediff-clusters.md) — [HIST] 99%+ one-diff clusters ranked by cause.
- [plans/permuter-readiness.md](plans/permuter-readiness.md) — [HIST] permuter queue generator wired; 240 fns in 80-99.99% band.
- [plans/permuter-sweep-struct-cascades-2026-05-29.md](plans/permuter-sweep-struct-cascades-2026-05-29.md) — [HIST] 151-fn sweep: 1 permuter win.
- Identification / oracle plans: [plans/bindiff-integration.md](plans/bindiff-integration.md) · [plans/bindiff-vs-rb3wii.md](plans/bindiff-vs-rb3wii.md) · [plans/game-code-anchoring.md](plans/game-code-anchoring.md) · [plans/game-code-pairing.md](plans/game-code-pairing.md) · [plans/game-oracle-triage.md](plans/game-oracle-triage.md) · [plans/jeff-vtable-detector.md](plans/jeff-vtable-detector.md) · [plans/jeff-residual-overlaps.md](plans/jeff-residual-overlaps.md) — [HIST].
- Pin/port waves: [plans/pin-tier2-clusters.md](plans/pin-tier2-clusters.md) · [plans/pin-wave-2.md](plans/pin-wave-2.md) · [plans/porting-backlog-ranked.md](plans/porting-backlog-ranked.md) · [plans/porting-wave-1.md](plans/porting-wave-1.md) · [plans/wave5-session-2026-05-28.md](plans/wave5-session-2026-05-28.md) · [plans/wire-missing-config-units.md](plans/wire-missing-config-units.md) · [plans/match-first-fn.md](plans/match-first-fn.md) — [HIST].
- Band3/port-era: [plans/bandobj-port.md](plans/bandobj-port.md) · [plans/meta_band-port-breaking-changes.md](plans/meta_band-port-breaking-changes.md) · [plans/codegen-iteration-targets.md](plans/codegen-iteration-targets.md) · [plans/remaining-matching-work-handoff.md](plans/remaining-matching-work-handoff.md) · [plans/session-handoff-2026-05-27.md](plans/session-handoff-2026-05-27.md) · [plans/instrumentation-patcher-experiment.md](plans/instrumentation-patcher-experiment.md) — [HIST].
- Data artifact: [plans/fingerprint-transfer-backlog-2026-06-06.json](plans/fingerprint-transfer-backlog-2026-06-06.json) — [HIST] fingerprint-transfer backlog snapshot.

### 4d. Buildspeed round-2 campaign ([HIST], landed 2026-07-02)

The wibo-fork + 9-dir PCH + Rust objcache work — all live on main. See CLAUDE.md's build
sections for current mechanics; these are the design/execution records.

- [plans/buildspeed/00-overview.md](plans/buildspeed/00-overview.md) — campaign overview (FULLY LANDED).
- [plans/buildspeed/01-wiring-window-1.md](plans/buildspeed/01-wiring-window-1.md) · [02-objcache-crate.md](plans/buildspeed/02-objcache-crate.md) · [03-pch-verify.md](plans/buildspeed/03-pch-verify.md) · [04-wibo-residual.md](plans/buildspeed/04-wibo-residual.md) · [05-pch-land.md](plans/buildspeed/05-pch-land.md) · [06-objcache-integration.md](plans/buildspeed/06-objcache-integration.md) · [07-wibo-merge-stage.md](plans/buildspeed/07-wibo-merge-stage.md) · [08-objcache-wire-and-wibo-deploy.md](plans/buildspeed/08-objcache-wire-and-wibo-deploy.md).
- [plans/buildspeed/09-worktree-seeding.md](plans/buildspeed/09-worktree-seeding.md) — warm-state `.ninja_log`/`.ninja_deps` seeding (banner: EXECUTED/LANDED).

### 4e. Frontier workstream plans (2026-07-02)

Execution docs behind the master `frontier-workstreams-2026-07-02.md`. ws2/ws5/ws7 have open
work (see §1); the rest are [HIST] executed.

- [plans/workstreams-2026-07-02/ws1-sysnet-drain.md](plans/workstreams-2026-07-02/ws1-sysnet-drain.md) — [HIST] Wave A executed (+46).
- [plans/workstreams-2026-07-02/ws3-optionc-port-then-pin.md](plans/workstreams-2026-07-02/ws3-optionc-port-then-pin.md) — [HIST] option-C harvest (+85).
- [plans/workstreams-2026-07-02/ws4-round3-banked-repair.md](plans/workstreams-2026-07-02/ws4-round3-banked-repair.md) — [HIST] banked repairs (+25).
- [plans/workstreams-2026-07-02/ws6-reconstruction-prep.md](plans/workstreams-2026-07-02/ws6-reconstruction-prep.md) — [HIST] reconstruction prep, awaiting downstream.

### 4f. Claude-memory archive exports (2026-07-29)

Verbatim exports of retired Claude persistent-memory topic files (frontmatter included),
made during the 2026-07-29 memory restructure. TU0-era: **every raw address in these is
invalid** (target flipped to TU5 on 2026-07-15); the durable levers/verdicts were distilled
into the surviving memory graph. Primary searchable record — do not edit.

- [decomp/history/memory-archive-wave-campaign-2026-07.md](decomp/history/memory-archive-wave-campaign-2026-07.md) — [HIST] waves 2-40 session records (2026-07-09..12, main 11,583→15,822), crack-farm deploy/saturation (⚠ B2 key echoed — rotate before reuse), superseded renamer diagnosis. 43 memory files.
- [decomp/history/memory-archive-early-campaign-2026-06.md](decomp/history/memory-archive-early-campaign-2026-06.md) — [HIST] June-2026 mega-sessions + 2026-07-02 frontier rounds (main 6,932→~11,120). 8 memory files.

### 4g. Non-md data artifacts

Indexed as data (not audited): `decomp/dc3-residual/ranked.json`,
`decomp/gameid/{crossval_agree,VERDICT}.json`, `decomp/matng-abandoned.jsonl`,
`decomp/research/2026-06-11-pin-audit-worklist.json`,
`decomp/research/2026-06-21-{bsim-seedprop-densification,songsortnode-va-confirmation}.json`,
`plans/fingerprint-transfer-backlog-2026-06-06.json`, `images/*.png` (dtk-template screenshots).

---

## 5. Lane + campaign records, 2026-07-07 → 2026-08-12 (all [HIST] unless noted)

> Added by the **2026-07-29 tooling/docs audit (lane BM)**. The 2026-07-06 audit
> predates ~3 weeks of daily lane records, and some older docs were never linked:
> **307 `.md` files under `docs/` were unindexed and therefore invisible to the
> next agent — 193 of them dated after that audit.** They are indexed here by
> their own H1 title (not re-summarised — no claim below has been re-verified).
> Treat every match-count and "current state" claim in them as **frozen at the
> file date**. Per-lane `.json` worklists (72 files) and `.log`/`.obj`/`.png`
> artifacts are deliberately not listed individually — grep `docs/plans/*.json`
> by lane letter or symbol name.

### 2026-08-12 — `name_check` / COMDAT-fold / anon-namespace lane records

> Indexed 2026-08-13 (lane RECOVER); these were written but never linked.

- [plans/namecheck-alias-fixpoint-2026-08-12.md](plans/namecheck-alias-fixpoint-2026-08-12.md) — the `name_check` alias fixpoint, and what the map-coverage lever is actually worth — `2026-08-12`
- [plans/wrong-callee-triage-2026-08-12.md](plans/wrong-callee-triage-2026-08-12.md) — the `name_check` "wrong callee" lane is symbol NAMING, not source — `2026-08-12`
- [plans/comdat-fold-gate-2026-08-12.md](plans/comdat-fold-gate-2026-08-12.md) — the homonym oracle does not generalise; the comparator was the bottleneck — `2026-08-12`
- [plans/fold-thunk-alias-gate-2026-08-12.md](plans/fold-thunk-alias-gate-2026-08-12.md) — the fold-thunk class is 9 alias groups and 27 refusals, not one finding — `2026-08-12`
- [plans/laneT-anon-ns-per-symbol-2026-08-12.md](plans/laneT-anon-ns-per-symbol-2026-08-12.md) — per-symbol anon-namespace hashes: +23 / −1, and dc3's evidence does not transfer — `2026-08-12`
- [plans/laneT-mempoptemp-and-anon-ns-2026-08-12.md](plans/laneT-mempoptemp-and-anon-ns-2026-08-12.md) — MemPopTemp was a real divergence AND a wrong diagnosis — `2026-08-12`

### 2026-07-29..30 — laneBO / BP / BQ / BS map-attribution + carve records

> Indexed 2026-08-13 (lane RECOVER); written during the 07-29/07-30 map-attribution
> campaign but never linked. Descriptions are the files' own H1 titles — no claim
> below has been re-verified, and every match-count is frozen at the file date.

- [plans/lane-bo2-collapse-rows-2026-07-29.md](plans/lane-bo2-collapse-rows-2026-07-29.md) — laneBO2 — draining laneBL's §6.1 "rows that DO collapse" — `2026-07-29`
- [plans/lane-bo3-uilabel-layout-2026-07-29.md](plans/lane-bo3-uilabel-layout-2026-07-29.md) — lane BO-3 — reconstructing `UILabel`'s retail-360 member layout — `2026-07-29`
- [plans/lane-bo4-byte-search-locator-2026-07-29.md](plans/lane-bo4-byte-search-locator-2026-07-29.md) — laneBO4 — the compile-and-byte-search TU locator — `2026-07-29`
- [plans/lane-bo8-icf-funclet-audit-2026-07-29.md](plans/lane-bo8-icf-funclet-audit-2026-07-29.md) — lane BO-8 — the 21,314 funclets at 100%: benign fold or over-count? — `2026-07-29`
- [plans/two-instrument-reconciliation-2026-07-29.md](plans/two-instrument-reconciliation-2026-07-29.md) — the two-instrument reconciliation: when the string anchor and the RTTI span disagree — `2026-07-29`
- [plans/lane-bp4-map-contradiction-adjudication-2026-07-29.md](plans/lane-bp4-map-contradiction-adjudication-2026-07-29.md) — lane BP-4 — ICF map-contradiction adjudication, and what the worklist was actually measuring — `2026-07-29`
- [plans/lane-bp5-uisyncnetmsgs-map-fragment-2026-07-29.md](plans/lane-bp5-uisyncnetmsgs-map-fragment-2026-07-29.md) — lane BP-5 — map fragment justification — `2026-07-29`
- [plans/lane-bp6-multi-content-justification-2026-07-29.md](plans/lane-bp6-multi-content-justification-2026-07-29.md) — lane BP-6 — map fragment justification (24 entries) — `2026-07-29`
- [plans/lane-bp6-multi-content-refill-2026-07-29.md](plans/lane-bp6-multi-content-refill-2026-07-29.md) — lane BP-6 — MULTI / UNIQUE-ICF content-join refill — `2026-07-29`
- [plans/lane-bp7-map-ownership-2026-07-29.md](plans/lane-bp7-map-ownership-2026-07-29.md) — lane BP-7 — map ownership: stream-direction, phantom drain, and a new decisive channel — `2026-07-29`
- [plans/lane-bq1-jobA-minileaderboard-carve-2026-07-30.md](plans/lane-bq1-jobA-minileaderboard-carve-2026-07-30.md) — lane BQ-1 job A — carving MiniLeaderboardDisplay out of MetaPanel's 44-span mega-unit — `2026-07-30`
- [plans/lane-bq1-jobB-rndtext-carve-2026-07-30.md](plans/lane-bq1-jobB-rndtext-carve-2026-07-30.md) — lane BQ-1 job B — carving RndText's three out-of-TU bodies home — `2026-07-30`
- [plans/lane-bq1-jobC-staticclassname-chains-2026-07-30.md](plans/lane-bq1-jobC-staticclassname-chains-2026-07-30.md) — lane BQ-1 job C — BP-7's 19 open StaticClassName chains, adjudicated — `2026-07-30`
- [plans/lane-bs2-scoredisplay-carve-2026-07-30.md](plans/lane-bs2-scoredisplay-carve-2026-07-30.md) — lane BS-2 — the ScoreDisplay 3-cycle, and what the attribution-carve channel actually pays — `2026-07-30`
- [plans/map-defect-channels-plan-2026-07-30.md](plans/map-defect-channels-plan-2026-07-30.md) — map-defect channels: execution plan — `2026-07-30`
- [plans/homing-op-evidence-plan-2026-07-30.md](plans/homing-op-evidence-plan-2026-07-30.md) — homing-pool `op` (opcode) evidence-class: implementation plan — `2026-07-30`
- [plans/message-timer-retail-absence-and-staticclassname-pool-2026-07-30.md](plans/message-timer-retail-absence-and-staticclassname-pool-2026-07-30.md) — MESSAGE_TIMER: the lever was already pulled in June — the residual was six wrong per-TU restores — `2026-07-30`
- [plans/start-auto-timer-retail-absence-2026-07-30.md](plans/start-auto-timer-retail-absence-2026-07-30.md) — START_AUTO_TIMER is absent from retail — proven, gated, metric-neutral — `2026-07-30`
- [plans/branch-audit-slice3-2026-07-30.md](plans/branch-audit-slice3-2026-07-30.md) — branch audit slice 3 (the 47 oldest branches, 2026-05/06) — laneBT-4 — `2026-07-30`
- [plans/jeff-round3-review/class1.md](plans/jeff-round3-review/class1.md) — jeff round-3 review, DOC CLASS 1 — `.pdata` boundary (read-only confirm+plan) — `2026-07-30`
- [plans/jeff-round3-review/class2.md](plans/jeff-round3-review/class2.md) — jeff round-3 review, DOC CLASS 2 — "AddRoll-class `.pdata` over-splits" — `2026-07-30`
- [plans/jeff-round3-review/class3.md](plans/jeff-round3-review/class3.md) — jeff round-3 review, DOC CLASS 3 — `except_data` / EH COMDAT seed-time suppression — `2026-07-30`

### 2026-07-29 — today's landed records

- [plans/reloc-correspondence-audit-2026-07-29.md](plans/reloc-correspondence-audit-2026-07-29.md) — laneBH — reloc-correspondence audit: how much of the strict count is REPRODUCTION vs SHAPE (2026-07-29) — `2026-07-29`
- [plans/wii-oracle-tu-location-2026-07-29.md](plans/wii-oracle-tu-location-2026-07-29.md) — laneBD — locating the 141 Wii-oracle TUs with no 360 position (2026-07-29) — `2026-07-29`
- [plans/gapfill-pricing-and-nearmiss-open-2026-07-29.md](plans/gapfill-pricing-and-nearmiss-open-2026-07-29.md) — laneBE — pricing the gap channel + the scope_map dropped-function fix (2026-07-29) — `2026-07-29`
- [plans/nothrow-scatter-pricing-2026-07-29.md](plans/nothrow-scatter-pricing-2026-07-29.md) — Pricing the scatter-include nothrow mechanism (laneBG, 2026-07-29) — `2026-07-29`
- [plans/branch-audit-2026-07-29.md](plans/branch-audit-2026-07-29.md) — Branch / worktree audit — 2026-07-29 (laneBC) — `2026-07-29`
- [plans/attribution-frontier-census-2026-07-29.md](plans/attribution-frontier-census-2026-07-29.md) — laneBA — the attribution frontier: census of the dtk auto-carve pool (2026-07-29) — `2026-07-29`
- [plans/rb3-360-vs-wii-coverage-2026-07-29.md](plans/rb3-360-vs-wii-coverage-2026-07-29.md) — RB3-360 vs rb3-Wii vs DC3 — oracle coverage map (Lane BB, 2026-07-29) — `2026-07-29`

### Lane-campaign records, 2026-07-24..27 (map / splits / identity seam)

- [plans/lane-bf-stl-instantiation-mispair-verdict.md](plans/lane-bf-stl-instantiation-mispair-verdict.md) — laneBF W6 — CLASS VERDICT: the STL-instantiation near-miss band is MAP MISPAIR — `2026-07-29`
- [plans/lane-aw-bodyport-2026-07-27.md](plans/lane-aw-bodyport-2026-07-27.md) — laneAW — body-port / source-divergence wave (2026-07-27) — `2026-07-27`
- [plans/three-address-adjudication-2026-07-27.md](plans/three-address-adjudication-2026-07-27.md) — Three-address adjudication — CharEyes / CharSignalApplier / HamMove contest (2026-07-27) — `2026-07-27`
- [plans/lane-au2-objptr-replace-2026-07-26.md](plans/lane-au2-objptr-replace-2026-07-26.md) — laneAU-2 — the `ObjPtr<T>::Replace` family (2026-07-26) — `2026-07-26`
- [plans/lane-au-4-forceblock-and-stale-asm-2026-07-26.md](plans/lane-au-4-forceblock-and-stale-asm-2026-07-26.md) — laneAU-4 — sizing two inherited veins: the `DECOMP_FORCE*` sweep and the stale-`.s` trap — `2026-07-26`
- [plans/laneAU3-eviction-plan-reprice-2026-07-26.md](plans/laneAU3-eviction-plan-reprice-2026-07-26.md) — Lane AU-3 — honest re-price of the laneH eviction plan (2026-07-26) — `2026-07-26`
- [plans/lane-ar-map-ownership-2026-07-26.md](plans/lane-ar-map-ownership-2026-07-26.md) — laneAR — map ownership round 2 (2026-07-26) — `2026-07-26`
- [plans/lane-av-arbitrary-and-scope-2026-07-26.md](plans/lane-av-arbitrary-and-scope-2026-07-26.md) — laneAV — the `_bijection_arbitrary` re-decision, and a scope predicate that was wrong — `2026-07-26`
- [plans/lane-as-perunit-identity-allbands-2026-07-26.md](plans/lane-as-perunit-identity-allbands-2026-07-26.md) — laneAS — the per-unit identity channel across ALL size bands (2026-07-26) — `2026-07-26`
- [plans/lane-ap-residue-funnel-2026-07-26.md](plans/lane-ap-residue-funnel-2026-07-26.md) — laneAP — the "unreachable ~4,400" funnel: 40% of it was never a source problem (2026-07-26) — `2026-07-26`
- [plans/lane-aq-identity-funnel-2026-07-26.md](plans/lane-aq-identity-funnel-2026-07-26.md) — Lane AQ — the >68 B anonymous pool: identity funnel (2026-07-26) — `2026-07-26`
- [plans/lane-ao-map-ownership-2026-07-26.md](plans/lane-ao-map-ownership-2026-07-26.md) — laneAO — single-owner round on `scripts/target_symbol_map.json` (2026-07-26) — `2026-07-26`
- [plans/laneAN/objdiff-84byte-cap.md](plans/laneAN/objdiff-84byte-cap.md) — laneAN — the "84-byte cap" in objdiff funclet byte-signature pairing — `2026-07-26`
- [plans/lane-al-autocarve-2026-07-26.md](plans/lane-al-autocarve-2026-07-26.md) — laneAL — the `auto_03_*` unowned-address pool: funnel and verdict (2026-07-26) — `2026-07-26`
- [plans/lane-an-pdata-parentage-2026-07-26.md](plans/lane-an-pdata-parentage-2026-07-26.md) — laneAN — `.pdata` parent-funclet association: a HARD attribution signal (2026-07-26) — `2026-07-26`
- [plans/lane-am-diffunit-2026-07-26.md](plans/lane-am-diffunit-2026-07-26.md) — laneAM — the DIFFERENT-UNIT gap pool: a margin rule and its precision/yield curve (2026-07-26) — `2026-07-26`
- [plans/lane-ak-icf-bijection-2026-07-26.md](plans/lane-ak-icf-bijection-2026-07-26.md) — Lane AK — the oracle lane, and the seam it actually found — `2026-07-26`
- [plans/lane-ah-layout-oracle-2026-07-26.md](plans/lane-ah-layout-oracle-2026-07-26.md) — Lane AH — the layout oracle, and the `S=1` tier it unlocks (2026-07-26) — `2026-07-26`
- [plans/lane-ai-joint-map-splits-2026-07-26.md](plans/lane-ai-joint-map-splits-2026-07-26.md) — Lane AI — joint owner of `target_symbol_map.json` + `splits.txt`, round 2 — `2026-07-26`
- [plans/lane-ag-deep-body-ports-2026-07-26.md](plans/lane-ag-deep-body-ports-2026-07-26.md) — Lane AG — the deep-body-port residue (the ~45% every tooling lane declined) — `2026-07-26`
- [plans/lane-ae-unemitted-symbols.md](plans/lane-ae-unemitted-symbols.md) — Lane AE — the unemitted-symbol class — `2026-07-26`
- [plans/lane-ae-nowhere-triage-2026-07-26.md](plans/lane-ae-nowhere-triage-2026-07-26.md) — Lane AE round 2 — NOWHERE-pool triage — `2026-07-26`
- [plans/laneAD-joint-2026-07-26.md](plans/laneAD-joint-2026-07-26.md) — laneAD — the symbol map and splits.txt run to a JOINT fixpoint (2026-07-26) — `2026-07-26`
- [plans/laneAC-holes-2026-07-26.md](plans/laneAC-holes-2026-07-26.md) — laneAC — splits "holes" and the refilled WRONG-UNIT pool (2026-07-26) — `2026-07-26`
- [plans/laneAB-map-repair-2026-07-26.md](plans/laneAB-map-repair-2026-07-26.md) — Lane AB — symbol-map repair round 2 — `2026-07-26`
- [plans/laneAA-structural-clusters-2026-07-26.md](plans/laneAA-structural-clusters-2026-07-26.md) — Lane AA — working the STRUCTURAL identical-percentage clusters — `2026-07-26`
- [plans/identical-pct-cluster-scan-2026-07-26.md](plans/identical-pct-cluster-scan-2026-07-26.md) — The identical-percentage cluster scan — how much of the sub-100 pool is shared-cause? — `2026-07-26`
- [plans/splits-move-lane-2026-07-26.md](plans/splits-move-lane-2026-07-26.md) — Splits MOVE lane (laneQ) — wrong-unit `.text` spans, 2026-07-26 — `2026-07-26`
- [plans/inline-policy-header-bucket-2026-07-26.md](plans/inline-policy-header-bucket-2026-07-26.md) — Inlined-by-us / out-of-line-in-retail — sizing the header-inline bucket — `2026-07-26`
- [plans/scatter-include-inlining-collapse-2026-07-26.md](plans/scatter-include-inlining-collapse-2026-07-26.md) — Scatter-include inlining collapse — scanner, measured pool size, and a control group — `2026-07-26`
- [plans/identification-rtti-and-bigfamily-2026-07-26.md](plans/identification-rtti-and-bigfamily-2026-07-26.md) — RTTI-via-EH, and why BIG-FAMILY should stay unfunded (lane M, 2026-07-26) — `2026-07-26`
- [plans/homing-scan-round5-2026-07-26.md](plans/homing-scan-round5-2026-07-26.md) — Identification flywheel — round 5, driven to a fixed point (2026-07-26) — `2026-07-26`
- [plans/identification-discriminators-2026-07-25.md](plans/identification-discriminators-2026-07-25.md) — Identification discriminators beyond callee-side content (lane K, 2026-07-25) — `2026-07-25`
- [plans/laneH-map-rotation-repair-2026-07-25.md](plans/laneH-map-rotation-repair-2026-07-25.md) — Lane H — cycle-aware repair of the mispaired `target_symbol_map` entries (2026-07-25) — `2026-07-25`
- [plans/laneG-multi-content-join-2026-07-24.md](plans/laneG-multi-content-join-2026-07-24.md) — Lane G — content join over the homing scan's MULTI residue (2026-07-24) — `2026-07-24`
- [plans/homing-scan-round4-2026-07-24.md](plans/homing-scan-round4-2026-07-24.md) — Homing scan round 4 — full-tree sweep (2026-07-24) — `2026-07-24`
- [plans/batch5-ranked-2026-07-24.md](plans/batch5-ranked-2026-07-24.md) — fpcarve BATCH-5 — honest ranked target list & channel refresh — `2026-07-24`
- [plans/map-audit-2026-07-24.md](plans/map-audit-2026-07-24.md) — target_symbol_map.json — whole-map mispair audit (2026-07-24) — `2026-07-24`
- [plans/bindiff-r2-mispair-verdicts-2026-07-24.md](plans/bindiff-r2-mispair-verdicts-2026-07-24.md) — BinDiff round-2 mispair adjudication — 36 conflicts — `2026-07-24`
- [plans/bindiff-r2-anchored-2026-07-24.md](plans/bindiff-r2-anchored-2026-07-24.md) — BinDiff DC3→RB3 identification — ROUND 2 (anchored second pass), 2026-07-24 — `2026-07-24`
- [plans/native-scope-map-2026-07-24.md](plans/native-scope-map-2026-07-24.md) — RB3-Xenon — NATIVE-SCOPE decomp map — `2026-07-24`
- [plans/paths-to-100/17-unicorn-equivalence-lane.md](plans/paths-to-100/17-unicorn-equivalence-lane.md) — Unicorn behavioral equivalence — triage lane and secondary credit metric — `2026-07-08`

### Wave records: homing / fpcarve / repin / carve-pilot

- [plans/repin-batch11-stub-census.md](plans/repin-batch11-stub-census.md) — Batch-11 — Stub / full-file-port TU census — `2026-07-24`
- [plans/repin-batch10.md](plans/repin-batch10.md) — Fingerprint carve — BATCH 10 (batch-9 seed execution) — `2026-07-24`
- [plans/repin-batch9.md](plans/repin-batch9.md) — Fingerprint carve — BATCH 9 (repin tail + named near-miss harvest) — `2026-07-24`
- [plans/repin-batch8.md](plans/repin-batch8.md) — Fingerprint carve — BATCH 8 (fresh lower-threshold repin census) — `2026-07-21`
- [plans/fpcarve-batch7.md](plans/fpcarve-batch7.md) — Fingerprint carve — BATCH 7 (game-repin residue drain) — `2026-07-21`
- [plans/fpcarve-batch6.md](plans/fpcarve-batch6.md) — Fingerprint carve — BATCH 6 (round-4 FIRST-PRINCIPLES census) — `2026-07-21`
- [plans/fpcarve-batch5.md](plans/fpcarve-batch5.md) — Fingerprint carve — BATCH 5 outcome (2026-07-21) — `2026-07-21`
- [plans/fpcarve-batch4.md](plans/fpcarve-batch4.md) — Fingerprint carve — BATCH 4 outcome (2026-07-21) — `2026-07-21`
- [plans/fpcarve-batch3.md](plans/fpcarve-batch3.md) — Fingerprint carve — BATCH 3 (2026-07-20) — `2026-07-21`
- [plans/fpcarve-batch2.md](plans/fpcarve-batch2.md) — Fingerprint carve — BATCH 2 (2026-07-20) — `2026-07-20`
- [plans/fpcarve-batch1.md](plans/fpcarve-batch1.md) — Ranked carvable GAME TUs — string-fingerprint channel (batch 1, 2026-07-20) — `2026-07-20`
- [plans/carve-pilot-2026-07-20.md](plans/carve-pilot-2026-07-20.md) — Carve-pilot: BinDiff-hint → wired TU loop, measured (2026-07-20) — `2026-07-20`
- [decomp/research/2026-07-10-spill-store-homing-mechanism.md](decomp/research/2026-07-10-spill-store-homing-mechanism.md) — Spill-store count mechanism: address-taken writes vs EH temp homing (2026-07-10) — `2026-07-10`

### TU5 migration + address adjudication

- [plans/tu5-p5-progress.md](plans/tu5-p5-progress.md) — TU5 P5 — post-flip matching progress — `2026-07-20`
- [plans/tu5-landing-runbook.md](plans/tu5-landing-runbook.md) — TU5 Re-base Landing Runbook — `2026-07-19`
- [plans/ghidra-tu0-tu5-crossport.md](plans/ghidra-tu0-tu5-crossport.md) — Ghidra TU0→TU5 cross-port — leveraging the "banks" model for RB3-Xenon — `2026-07-16`
- [plans/tu5-p5-manifest.md](plans/tu5-p5-manifest.md) — TU5 re-base — P5 enumerated-drop manifest — `2026-07-15`
- [plans/si-hw-fix/TU5-RECALCGEMLIST-ANALYSIS.md](plans/si-hw-fix/TU5-RECALCGEMLIST-ANALYSIS.md) — TU5 RecalcGemList Non-Execution Analysis + Real Song-Load mGemList Path — `2026-07-15`
- [plans/clean-tu5-vs-rb3dx-divergence.md](plans/clean-tu5-vs-rb3dx-divergence.md) — Clean retail TU5 vs RB3 Deluxe (RB3DX) — production + divergence — `2026-07-07`
- [plans/tu5-rewritten-functions-analysis.md](plans/tu5-rewritten-functions-analysis.md) — TU0→TU5 rewritten-function analysis (the 81 genuine MISS) — `2026-07-07`
- [plans/tu5-execution-status.md](plans/tu5-execution-status.md) — RB3 Xenon → TU5 Migration — Execution Status — `2026-07-07`
- [plans/same-instrument-tu5-retarget.md](plans/same-instrument-tu5-retarget.md) — Same-Instrument patch — TU5 (retail v0.0.5.1) retarget — `2026-07-07`
- [plans/base-to-tu5-map.md](plans/base-to-tu5-map.md) — base(TU0) → TU5 function map — migration keystone (P1) — `2026-07-07`
- [plans/rb3xenon-tu5-migration-plan.md](plans/rb3xenon-tu5-migration-plan.md) — rb3-xenon: base(TU0) → TU5 Migration Plan — `2026-07-07`
- [plans/base-to-tu5-map-spike.md](plans/base-to-tu5-map-spike.md) — base(TU0) → TU5 function-remap SPIKE — proof on a real sample (2026-07-07) — `2026-07-07`
- [plans/tu5-acquisition.md](plans/tu5-acquisition.md) — TU5 acquisition + validation (Lane A) — 2026-07-07 — `2026-07-07`
- [plans/tu5-anchoring-impact.md](plans/tu5-anchoring-impact.md) — rb3-xenon base(TU0) → TU5 — ANCHORING IMPACT (Lane B, 2026-07-07) — `2026-07-07`
- [plans/tu5-migration-scope.md](plans/tu5-migration-scope.md) — rb3-xenon base(TU0)→TU5 migration — PLANNER scope (2026-07-07) — `2026-07-07`

### paths-to-100 RFC set (2026-07-08)

- [plans/paths-to-100/04-pinning-at-scale.md](plans/paths-to-100/04-pinning-at-scale.md) — Pinning at scale — automating splits.txt backfill for the unpinned majority — `2026-07-08`
- [plans/paths-to-100/14-systematic-symbol-sweeps.md](plans/paths-to-100/14-systematic-symbol-sweeps.md) — Systematic sweeps — local-static-Symbol, guard thunks, and other one-pattern-many-functions fixes — `2026-07-08`
- [plans/paths-to-100/11-permuter-farm.md](plans/paths-to-100/11-permuter-farm.md) — MSVC permuter farm — automated source-permutation search at scale — `2026-07-08`
- [plans/paths-to-100/atlas-snapshot-2026-07-08.md](plans/paths-to-100/atlas-snapshot-2026-07-08.md) — Gap composition atlas — frozen snapshot 2026-07-08 — `2026-07-08`
- [plans/paths-to-100/19-shiftable-relink-milestone.md](plans/paths-to-100/19-shiftable-relink-milestone.md) — Shiftable relink milestone — full splits, reloc-normalized equivalence, bootable XEX — `2026-07-08`
- [plans/paths-to-100/20-native-port-and-engine-reuse.md](plans/paths-to-100/20-native-port-and-engine-reuse.md) — Native port + DC3 engine reuse — the playable-RB3 track and milo-native-engine extraction — `2026-07-08`
- [plans/paths-to-100/18-metrics-and-dashboard.md](plans/paths-to-100/18-metrics-and-dashboard.md) — Metrics of record and progress dashboard — vein ROI accounting — `2026-07-08`
- [plans/paths-to-100/10-middleware-and-denominator.md](plans/paths-to-100/10-middleware-and-denominator.md) — Middleware strategy — Bink, Quazal, XDK/CRT and the honest denominator — `2026-07-08`
- [plans/paths-to-100/09-sibling-title-oracles.md](plans/paths-to-100/09-sibling-title-oracles.md) — Sibling-title oracles — RB1/RB2/TBRB/GDRB/LRB/devkit/TU builds as identification sources — `2026-07-08`
- [plans/paths-to-100/06-oracle-refresh-loops.md](plans/paths-to-100/06-oracle-refresh-loops.md) — Oracle refresh loops — iterative re-diffing as matches accumulate — `2026-07-08`
- [plans/paths-to-100/13-codegen-idiom-library.md](plans/paths-to-100/13-codegen-idiom-library.md) — MWCC-to-MSVC codegen idiom library — systematic source-idiom translation — `2026-07-08`
- [plans/paths-to-100/02-gap-composition-atlas.md](plans/paths-to-100/02-gap-composition-atlas.md) — Gap composition atlas — what exactly is the unmatched 91% — `2026-07-08`
- [plans/paths-to-100/03-master-sequencing-roadmap.md](plans/paths-to-100/03-master-sequencing-roadmap.md) — Master sequencing — dependency-ordered roadmap to maximum match — `2026-07-08`
- [plans/paths-to-100/01-endgame-definitions.md](plans/paths-to-100/01-endgame-definitions.md) — What does 100% mean — endgame taxonomy and recommended target — `2026-07-08`
- [plans/paths-to-100/05-data-xref-anchoring.md](plans/paths-to-100/05-data-xref-anchoring.md) — Data-xref anchoring — vtables, RTTI, and .rdata/.data pins as an identification signal — `2026-07-08`
- [plans/paths-to-100/07-icf-constraint-solver.md](plans/paths-to-100/07-icf-constraint-solver.md) — ICF-aware global assignment — constraint-solving identification — `2026-07-08`
- [plans/paths-to-100/16-auto-landing-pipeline.md](plans/paths-to-100/16-auto-landing-pipeline.md) — Auto-landing pipeline — verification lanes, regression locks, and policy-gated merges — `2026-07-08`

> **STATUS (2026-08-13):** four RFCs of this 21-RFC set are **absent from `main`'s history**
> and their entries were removed on 2026-08-13 (lane RECOVER) rather than left dangling:
> `08-ml-embedding-triage`, `12-grind-fleet-v2`, `15-ghidra-guided-synthesis`,
> `21-crack-farm-cpu-training-capture`. The first, second and fourth are training-corpus /
> LLM-fleet work deliberately kept out of this public repo. The RFC **numbering is therefore
> sparse — that is expected, not a missing file.**

### SI hardware campaign / strategy-b OSS build / RB3Enhanced

- [plans/si-hw-fix/wave8/FROMSOURCE-COMPRESS.md](plans/si-hw-fix/wave8/FROMSOURCE-COMPRESS.md) — WAVE8 — From-Source SI DLL: XexTool compression + hardware test — `2026-07-15`
- [plans/si-hw-fix/CRASH-2same-instrument-2026-07-14.md](plans/si-hw-fix/CRASH-2same-instrument-2026-07-14.md) — Live crash capture — 2-same-instrument song load (2026-07-14) — `2026-07-15`
- [plans/si-hw-fix/SI-DLL-LOAD-INVESTIGATION.md](plans/si-hw-fix/SI-DLL-LOAD-INVESTIGATION.md) — Why our RB3Enhanced.dll won't load on a real Xbox 360 — end-to-end investigation — `2026-07-15`
- [plans/http-bringup-and-rb3eloader-fix-2026-07-15.md](plans/http-bringup-and-rb3eloader-fix-2026-07-15.md) — RB3E HTTP bring-up + RB3ELoader crash fix — session log 2026-07-15 — `2026-07-15`
- [plans/si-hw-fix/COORDINATOR-STATUS.md](plans/si-hw-fix/COORDINATOR-STATUS.md) — Same-Instrument hardware-failure investigation — coordinator status (INTERIM) — `2026-07-14`
- [plans/si-hw-fix/CRASH3-TRACE.md](plans/si-hw-fix/CRASH3-TRACE.md) — CRASH #3 root-cause trace — null smasher-plate dir on 2-same-instrument load (2026-07-14) — `2026-07-14`
- [plans/si-hw-fix/wave8/WAVE8-STATUS.md](plans/si-hw-fix/wave8/WAVE8-STATUS.md) — Wave 8 — DLL Load-Compat: root cause is the XEX container, not the code — `2026-07-14`
- [plans/si-hw-fix/wave8/PATH-REVIEW.md](plans/si-hw-fix/wave8/PATH-REVIEW.md) — WAVE8 — Path Review: cleanest route to a loadable SI RB3Enhanced.dll — `2026-07-14`
- [plans/si-hw-fix/wave8/DEBUG-TOOLING.md](plans/si-hw-fix/wave8/DEBUG-TOOLING.md) — Xbox 360 RGH Debug Tooling — Source Acquisition & Linux Buildability — `2026-07-14`
- [plans/si-hw-fix/wave8/REMOTE-DEBUG-CAPABILITIES.md](plans/si-hw-fix/wave8/REMOTE-DEBUG-CAPABILITIES.md) — RB3Enhanced remote-debug capabilities — what we have, what a patch adds — `2026-07-14`
- [plans/si-hw-fix/wave8/HEADER-DIFF.md](plans/si-hw-fix/wave8/HEADER-DIFF.md) — WAVE8 — XEX2 Header Diff: why the spliced DLL is rejected by XexLoadImage — `2026-07-14`
- [plans/si-hw-fix/wave8/COMPRESSION-LEADS.md](plans/si-hw-fix/wave8/COMPRESSION-LEADS.md) — XEX2 LZX Compression — Tooling Recon (2026-07-14) — `2026-07-14`
- [plans/strategy-b/checkpoints/rb3dx-finish/DLL-HW-LOAD-COMPAT.md](plans/strategy-b/checkpoints/rb3dx-finish/DLL-HW-LOAD-COMPAT.md) — DLL hardware-load compatibility triage — from-source RB3Enhanced.dll vs known-good nightly — `2026-07-14`
- [plans/strategy-b/checkpoints/rb3dx-finish/HUB-STALL-GPUNULL-CHARACTERIZATION.md](plans/strategy-b/checkpoints/rb3dx-finish/HUB-STALL-GPUNULL-CHARACTERIZATION.md) — main_hub stall — gpu=null characterization (de-mask + ui_probe) — `2026-07-13`
- [plans/strategy-b/HUBCRASH-ROOTCAUSE-82BCEFE4.md](plans/strategy-b/HUBCRASH-ROOTCAUSE-82BCEFE4.md) — Hub "crash" PC 0x82BCEFE4 / EA 0x7FEA1A80 — root-cause brief — `2026-07-13`
- [plans/strategy-b/INTEGRATED-STATUS.md](plans/strategy-b/INTEGRATED-STATUS.md) — Strategy B — Integrated Status (all lanes consolidated) — `2026-07-13`
- [plans/strategy-b/checkpoints/rb3dx-finish/MATRIX-RESULTS.md](plans/strategy-b/checkpoints/rb3dx-finish/MATRIX-RESULTS.md) — Phase 3 — Xenia harness rewire + hook-install validation (matrix results) — `2026-07-13`
- [plans/strategy-b/rb3dx-port-audit-rulings.md](plans/strategy-b/rb3dx-port-audit-rulings.md) — RB3DX port-audit rulings (plan Phase 1, P1.2) — `2026-07-13`
- [plans/strategy-b/RB3DX-RETARGET-PLAN.md](plans/strategy-b/RB3DX-RETARGET-PLAN.md) — RB3DX Retarget Plan — from-source RB3E DLL on RB3DX + Xenia same-instrument validation — `2026-07-13`
- [plans/strategy-b/UNRESOLVED-LEDGER.md](plans/strategy-b/UNRESOLVED-LEDGER.md) — Strategy B — link unresolved-symbol ledger (from-source RB3Enhanced.dll) — `2026-07-12`
- [plans/strategy-b/checkpoints/verify/X-packer-verify-handoff.md](plans/strategy-b/checkpoints/verify/X-packer-verify-handoff.md) — Adversarial verify of Lane X (PE→XEX2 packer) — VERDICT: CONFIRMED — `2026-07-12`
- [plans/strategy-b/checkpoints/H-headers-handoff.md](plans/strategy-b/checkpoints/H-headers-handoff.md) — Lane H — XDK-free `<xtl.h>` header reconstruction — HANDOFF — `2026-07-12`
- [plans/strategy-b/checkpoints/X-packer-handoff.md](plans/strategy-b/checkpoints/X-packer-handoff.md) — Lane X — PE→XEX2 packer + identity round-trip (HANDOFF) — `2026-07-12`
- [plans/strategy-b/checkpoints/L-importlibs-handoff.md](plans/strategy-b/checkpoints/L-importlibs-handoff.md) — Lane L — Import Libraries (XDK-free) — HANDOFF — `2026-07-12`
- [plans/strategy-b/checkpoints/K-link-handoff.md](plans/strategy-b/checkpoints/K-link-handoff.md) — Lane K — Full XDK-free compile+link recipe (handoff) — `2026-07-12`
- [plans/strategy-b-full-oss-rb3e-build.md](plans/strategy-b-full-oss-rb3e-build.md) — Strategy B — Full XDK-free rebuild of RB3Enhanced.dll from source — `2026-07-12`
- [plans/si-hw-fix/WAVE6-DLL-BUILD-PLAN.md](plans/si-hw-fix/WAVE6-DLL-BUILD-PLAN.md) — WAVE 6 — RB3Enhanced.dll (same-instrument H1+H2) BUILD + DEBUG PLAN — `2026-07-09`
- [plans/si-hw-fix/wave6/VALIDATION-LADDER.md](plans/si-hw-fix/wave6/VALIDATION-LADDER.md) — Wave 6 — SI-fix DLL Validation & Debug Ladder (no XDK, no remote debugger) — `2026-07-09`
- [plans/si-hw-fix/WAVE5-SONGSTART-RISK.md](plans/si-hw-fix/WAVE5-SONGSTART-RISK.md) — WAVE 5 — Song-start risk verdict: why RB3DX blocks same-part, and what our DTA-only fix ships — `2026-07-09`
- [plans/si-hw-fix/WAVE4-REAL-GATE-AND-FIX.md](plans/si-hw-fix/WAVE4-REAL-GATE-AND-FIX.md) — Wave 4 — Same-Instrument (SI): the REAL gate, and the fix — `2026-07-09`
- [plans/si-hw-fix/WAVE2-ROOTCAUSE-AND-FIX.md](plans/si-hw-fix/WAVE2-ROOTCAUSE-AND-FIX.md) — Same-Instrument hardware failure — Wave-2 root cause + fix decision — `2026-07-09`
- [plans/si-hw-fix/TEST-LADDER.md](plans/si-hw-fix/TEST-LADDER.md) — Same-Instrument Hardware Test Ladder (wave 2 output — HELD pending Xenia validation) — `2026-07-09`
- [plans/si-hw-fix/rb3e-dll-analysis.md](plans/si-hw-fix/rb3e-dll-analysis.md) — RB3Enhanced.dll analysis — PARTIAL / checkpoint (bailed early, console offline) — `2026-07-09`
- [plans/si-hw-fix/greyout-path.md](plans/si-hw-fix/greyout-path.md) — Overshell instrument-select grey-out — ground truth (Layer A / IsActive) — `2026-07-09`
- [plans/si-hw-fix/console-bytes.md](plans/si-hw-fix/console-bytes.md) — Console default.xex byte verification — Same-Instrument TU5 patch — `2026-07-09`
- [plans/same-instrument-packer-status.md](plans/same-instrument-packer-status.md) — Same-Instrument packer — status (Stage 4/5 PACK+VERIFY) — `2026-07-07`
- [plans/same-instrument-compile-recipe.md](plans/same-instrument-compile-recipe.md) — Same-Instrument TU — XDK-free cl.exe compile recipe — `2026-07-07`
- [plans/rb3e-07-dll-layout.md](plans/rb3e-07-dll-layout.md) — RB3Enhanced 0.7 (Xbox 360) — release fetch + DLL layout + code-cave decision — `2026-07-07`
- [plans/same-instrument-derived-addresses.md](plans/same-instrument-derived-addresses.md) — Same-Instrument Patch — Derived Retail Addresses (Xbox 360, TU5, XEX base 0x82000000) — `2026-07-07`
- [plans/oss-build-path.md](plans/oss-build-path.md) — Lane B — Open-Source Build Path for an Xbox 360 XEX-DLL (no XDK) — `2026-07-07`
- [plans/binary-patch-path.md](plans/binary-patch-path.md) — Binary-Patch / No-Full-Build Path — same-instrument on retail RB3 (Xbox 360 TU5) — `2026-07-07`
- [plans/xdk-dependency-audit.md](plans/xdk-dependency-audit.md) — XDK Dependency Audit — LANE A — `2026-07-07`

### Native port — the X ladder (X0 → X22, 2026-08-01 → 2026-08-03)

> Indexed 2026-08-13 (lane RECOVER); the whole 27-doc ladder was written but never linked.
> This is the per-milestone record of the native engine bring-up referenced by `../CLAUDE.md`
> ("as of X4d the native build loads and renders real venue roots … and drives characters
> from real `CharClip`s"). Read them in order — **each milestone routinely REFUTES the
> previous one's stated cause**, so a mid-ladder doc read alone will hand you a cause that a
> later doc overturned. Numbers are frozen at each file's date.

- [plans/spike-x0-engine-dc3-flavor-2026-08-01.md](plans/spike-x0-engine-dc3-flavor-2026-08-01.md) — SPIKE-X0 — does milo-native-engine's `dc3` GPU backend flavor compile against rb3-xenon's headers? — `2026-08-01`
- [plans/x1-engine-link-2026-08-01.md](plans/x1-engine-link-2026-08-01.md) — X1 — link milo-native-engine into rb3-xenon, and prove it with a frame — `2026-08-01`
- [plans/x2-object-graph-load-2026-08-01.md](plans/x2-object-graph-load-2026-08-01.md) — X2 — a real `.milo_xbox` loads as a live object graph from the mounted ark — `2026-08-01`
- [plans/x3-first-render-2026-08-01.md](plans/x3-first-render-2026-08-01.md) — X3 — the first rendered frame, through the engine's dc3 WebGPU backend — `2026-08-01`
- [plans/x4a-venue-render-2026-08-02.md](plans/x4a-venue-render-2026-08-02.md) — X4a — venue render: what reached the GPU, and the wall that stopped the venue root — `2026-08-02`
- [plans/x4b-animation-2026-08-02.md](plans/x4b-animation-2026-08-02.md) — X4b — animation: a posed character, and the two SILENT defects in the way — `2026-08-02`
- [plans/x4c-init-audit-2026-08-02.md](plans/x4c-init-audit-2026-08-02.md) — X4c — the init audit, and why `kNewGfx` was never what emptied the frame — `2026-08-02`
- [plans/band3-native-unblock-priority-2026-08-02.md](plans/band3-native-unblock-priority-2026-08-02.md) — venue-unblock priority: the 14 factory-miss classes, and why they are NOT `band3` — `2026-08-02`
- [plans/x4d-venue-root-2026-08-03.md](plans/x4d-venue-root-2026-08-03.md) — X4d — the venue root loads, and the wall was four bytes — `2026-08-03`
- [plans/x5-scene-2026-08-03.md](plans/x5-scene-2026-08-03.md) — X5 — a character renders inside the venue, and the crowd was there the whole time — `2026-08-03`
- [plans/x6-placement-2026-08-03.md](plans/x6-placement-2026-08-03.md) — X6 — the crowd is placed, and the placement shipped in the file all along — `2026-08-03`
- [plans/x7-band-on-stage-2026-08-03.md](plans/x7-band-on-stage-2026-08-03.md) — X7 — stage positions were baked in the venue; the "ScatterIncludes lane" was three one-line guards — `2026-08-03`
- [plans/x8-band-render-2026-08-03.md](plans/x8-band-render-2026-08-03.md) — X8 — the wall was never `SetupDir`: it was `InlineProxy` bypassing a virtual — `2026-08-03`
- [plans/x9-band-marks-2026-08-03.md](plans/x9-band-marks-2026-08-03.md) — X9 — the band renders on its shipped marks; the wall was a guard copied from the wrong container — `2026-08-03`
- [plans/x10-band-geometry-2026-08-03.md](plans/x10-band-geometry-2026-08-03.md) — X10 — the geometry was already there; the probe was reading the wrong array — `2026-08-03`
- [plans/x11-mesh-geometry-2026-08-03.md](plans/x11-mesh-geometry-2026-08-03.md) — X11 — the empty meshes were never missing; they were LOADED, then RELEASED — `2026-08-03`
- [plans/x12-hand-pose-2026-08-03.md](plans/x12-hand-pose-2026-08-03.md) — X12 — the hands are correctly posed; the instrument was measuring light targets — `2026-08-03`
- [plans/x13-animated-pose-2026-08-03.md](plans/x13-animated-pose-2026-08-03.md) — X13 — the hands survive animation; the band does not survive *placement* — `2026-08-03`
- [plans/x14-band-placement-2026-08-03.md](plans/x14-band-placement-2026-08-03.md) — X14 — the band lands on its marks; the repair was in the tree and was never called — `2026-08-03`
- [plans/x15-poll-unblock-2026-08-03.md](plans/x15-poll-unblock-2026-08-03.md) — X15 — `Poll()` runs, and X14's cause for why it could not is REFUTED — `2026-08-03`
- [plans/x16-ownerptr-class-2026-08-03.md](plans/x16-ownerptr-class-2026-08-03.md) — X16 — the null is a class of 14, repaired upstream; X15's cause is REFUTED — `2026-08-03`
- [plans/x17-pose-residual-2026-08-03.md](plans/x17-pose-residual-2026-08-03.md) — X17 — pose residual and rebind skip share a SET, not a CAUSE; X16's one-defect hypothesis refuted causally, confirmed set-wise — `2026-08-03`
- [plans/x18-gate-and-roots-2026-08-03.md](plans/x18-gate-and-roots-2026-08-03.md) — X18 — the gate was OVER-REPORTING: 123/123 residual bones are engine publications — `2026-08-03`
- [plans/x19-sharing-and-scope-2026-08-03.md](plans/x19-sharing-and-scope-2026-08-03.md) — X19 — the sharing is BOTH real and a lookup artifact; the blocker was a driver coupling defect, not a shared-`src/` default — `2026-08-03`
- [plans/x20-textures-2026-08-03.md](plans/x20-textures-2026-08-03.md) — X20 — OutfitConfig is registered, the bill is derived, and registration was NOT the texture fix — `2026-08-03`
- [plans/x21-compose-path-2026-08-03.md](plans/x21-compose-path-2026-08-03.md) — X21 — `SyncOutfitConfig` IS reached; the compose pass is never dispatched; the dc3 backend cannot host it — `2026-08-03`
- [plans/x22-shared-material-2026-08-03.md](plans/x22-shared-material-2026-08-03.md) — X22 — the shared `char_shared.milo` material is real, is fixed, and is NOT why the band is pink — `2026-08-03`

### Native port cycles

- [plans/native-cycle14.md](plans/native-cycle14.md) — Batch-14 map-recovery foreman — results (2026-07-24) — `2026-07-24`
- [plans/native-cycle13.md](plans/native-cycle13.md) — Cycle-13 map-recovery foreman — results (2026-07-24) — `2026-07-24`
- [plans/native-cycle12.md](plans/native-cycle12.md) — Native critical-path cycle 12 — results (2026-07-24) — `2026-07-24`

### Grind / eval / training-corpus (2026-07-16..20)

- [plans/router-measured-priors.md](plans/router-measured-priors.md) — Router measured priors — `2026-07-24`
- [decomp/research/2026-07-20-hy3-log-analysis.md](decomp/research/2026-07-20-hy3-log-analysis.md) — hy3 champion-run log analysis (2026-07-20) — `2026-07-20`
- [plans/triage-buckets-2026-07-19.md](plans/triage-buckets-2026-07-19.md) — Divergence triage — priced buckets — `2026-07-19`

> **STATUS (2026-08-13):** seven further entries in this section (prompt-v4, frontier-selection-hy3val,
> terse-CoT distillation, runaway-model fix, eval-harness speed, c2rs eval speedup,
> reasoning-compaction review) pointed at docs **absent from `main`'s history** and were removed
> on 2026-08-13 (lane RECOVER). Same reason as the grind/eval section above: training-corpus and
> eval-harness effort docs are deliberately not carried in this public repo. The three entries
> that remain above are the ones that still resolve.

### decomp/patterns + decomp/research + decomp/handoff additions

- [decomp/patterns/fixable-fp-reassociation.md](decomp/patterns/fixable-fp-reassociation.md) — Fixable Patterns: `/fp:fast` Reassociation — the Parentheses Are the Barrier — `2026-08-13`
- [decomp/handoff/countorcreate-expandeddetails-bodyport-DEFER.md](decomp/handoff/countorcreate-expandeddetails-bodyport-DEFER.md) — DEFER handoff — NextSongPanel::CountOrCreateExpandedDetails (fn_82645320) body-port — `2026-07-21`
- [decomp/handoff/platformmgr-msgsource-rebase-LANDABLE.md](decomp/handoff/platformmgr-msgsource-rebase-LANDABLE.md) — LANDABLE — PlatformMgr MsgSource re-base (+4 strict, 0 lost) — batch-6 lever #2 — `2026-07-21`
- [decomp/handoff/profile-getpadnum-virtual-DEFER.md](decomp/handoff/profile-getpadnum-virtual-DEFER.md) — DEFER handoff — Profile::GetPadNum "missing-virtual" (batch-6 lever #1) — `2026-07-21`
- [decomp/handoff/storepreviewmgr-0x60-DEFER.md](decomp/handoff/storepreviewmgr-0x60-DEFER.md) — DEFER handoff — StorePreviewMgr 0x58→0x60 layout (batch-6 lever #3) — `2026-07-21`
- [decomp/research/2026-07-20-stl-element-stride-ground-truth.md](decomp/research/2026-07-20-stl-element-stride-ground-truth.md) — STL element-stride ground-truth (fill_insert / fill_n / resize / erase near-miss family) — `2026-07-20`
- [decomp/research/vsig-flags-2026-07-11.md](decomp/research/vsig-flags-2026-07-11.md) — dc3-drift virtual-signature flags — audited list (round 2, 2026-07-11) — `2026-07-11`
- [decomp/research/2026-07-10-objptr-two-ctor-inline.md](decomp/research/2026-07-10-objptr-two-ctor-inline.md) — "Sentinel-init ctor family" diagnosis — 2026-07-10 — `2026-07-10`
- [decomp/patterns/fixable-inline-boundary.md](decomp/patterns/fixable-inline-boundary.md) — Fixable Patterns: Inline Boundary — `2026-07-10`
- [decomp/patterns/fixable-struct-layout.md](decomp/patterns/fixable-struct-layout.md) — Struct Layout Mismatches — `2026-07-10`
- [decomp/handoff/offset-drift-sweep-r2-2026-07-10.md](decomp/handoff/offset-drift-sweep-r2-2026-07-10.md) — Offset-drift sweep ROUND 2 (2026-07-10, baseline 14,450) — `2026-07-10`
- [decomp/patterns/fixable-declarations.md](decomp/patterns/fixable-declarations.md) — Fixable Patterns: Declarations — `2026-07-10`
- [decomp/handoff/offset-drift-sweep-2026-07-10.md](decomp/handoff/offset-drift-sweep-2026-07-10.md) — Offset-drift sweep — systematic layout-drift detection (2026-07-10) — `2026-07-10`
- [decomp/handoff/game-layout-followups-2026-07-10.md](decomp/handoff/game-layout-followups-2026-07-10.md) — Game (band3) retail layout — post-base-drop follow-ups (2026-07-10) — `2026-07-10`
- [decomp/handoff/flow-phantom-pins-2026-07-10.md](decomp/handoff/flow-phantom-pins-2026-07-10.md) — flow/ TU pins are phantom — retail RB3 has no Flow system (2026-07-10) — `2026-07-10`
- [decomp/handoff/round5-header-needs-2026-07-07.md](decomp/handoff/round5-header-needs-2026-07-07.md) — Round-5 harvest — header-need follow-ups (2026-07-07) — `2026-07-10`
- [decomp/patterns/unfixable-compiler.md](decomp/patterns/unfixable-compiler.md) — Hard Patterns: Compiler — `2026-07-06`
- [decomp/patterns/verifiable-icf.md](decomp/patterns/verifiable-icf.md) — Verifiable Patterns: ICF (Identical COMDAT Folding) — `2026-07-06`
- [decomp/research/2026-07-02-ws2-loose-band-judging.md](decomp/research/2026-07-02-ws2-loose-band-judging.md) — WS2 — Loose-band (BSim simconf 10–15) worklist regen + honesty gate — `2026-07-02`
- [decomp/research/2026-07-02-span-confirm-triage.md](decomp/research/2026-07-02-span-confirm-triage.md) — ws7-R3 span-confirm triage — unpinned candidate identity check (2026-07-02) — `2026-07-02`
- [decomp/patterns/fixable-bool-mask.md](decomp/patterns/fixable-bool-mask.md) — Fixable Patterns: Bool Mask — `2020-01-01`
- [decomp/patterns/fixable-casting.md](decomp/patterns/fixable-casting.md) — Fixable Patterns: Casting — `2020-01-01`
- [decomp/patterns/fixable-comparison.md](decomp/patterns/fixable-comparison.md) — Fixable Patterns: Comparison — `2020-01-01`
- [decomp/patterns/fixable-control-flow.md](decomp/patterns/fixable-control-flow.md) — Fixable Patterns: Control Flow — `2020-01-01`
- [decomp/patterns/fixable-copy-ctor.md](decomp/patterns/fixable-copy-ctor.md) — Fixable: Bodyless Copy Constructor Declarations — `2020-01-01`
- [decomp/patterns/fixable-fsel-fma.md](decomp/patterns/fixable-fsel-fma.md) — Fixable Patterns: fsel Intrinsic and FMA Pragma — `2020-01-01`
- [decomp/patterns/fixable-loop-condition.md](decomp/patterns/fixable-loop-condition.md) — Fixable Patterns: Loop Condition Subtraction — `2020-01-01`
- [decomp/patterns/fixable-macros.md](decomp/patterns/fixable-macros.md) — Fixable Patterns: Handler Macros — `2020-01-01`
- [decomp/patterns/fixable-operators.md](decomp/patterns/fixable-operators.md) — Fixable Patterns: Operators — `2020-01-01`
- [decomp/patterns/harmful-avoid.md](decomp/patterns/harmful-avoid.md) — Harmful Patterns: Avoid These — `2020-01-01`
- [decomp/research/2026-06-10-bodyport-pool.md](decomp/research/2026-06-10-bodyport-pool.md) — Body-port pool re-rank — 2026-06-10 (post-refill, 6851 baseline) — `2020-01-01`
- [decomp/research/2026-06-10-force-multipliers.md](decomp/research/2026-06-10-force-multipliers.md) — Force-multiplier lever hunt — 2026-06-10 (read-only research handoff) — `2020-01-01`
- [decomp/research/2026-06-10-routed-residue.md](decomp/research/2026-06-10-routed-residue.md) — Routed near-miss residue triage — MEMBER_DELTA + UNKNOWN buckets (2026-06-10) — `2020-01-01`
- [decomp/research/2026-06-10-static-symbol-worklist.md](decomp/research/2026-06-10-static-symbol-worklist.md) — Static-Symbol-guard + MessageTimer worklist — `2020-01-01`
- [decomp/research/2026-06-11-accomplishmentprogress-compound.md](decomp/research/2026-06-11-accomplishmentprogress-compound.md) — AccomplishmentProgress compound fix — research dossier (2026-06-11) — `2020-01-01`
- [decomp/research/2026-06-11-bandgame-head4.md](decomp/research/2026-06-11-bandgame-head4.md) — Band head +4 / Game head +4 — RESEARCH-COMPLETE: REFUTED (zero measurable value; per-TU divergence, not a header fix) — `2020-01-01`
- [decomp/research/2026-06-11-bp4-accprog.md](decomp/research/2026-06-11-bp4-accprog.md) — BP4 lane: AccomplishmentProgress residuals — recon dossier (2026-06-11) — `2020-01-01`
- [decomp/research/2026-06-11-bp4-object.md](decomp/research/2026-06-11-bp4-object.md) — BP4 lane `object` — Hmx::Object root-class bodies (recon dossier, 2026-06-11) — `2020-01-01`
- [decomp/research/2026-06-11-bp4-songmgr.md](decomp/research/2026-06-11-bp4-songmgr.md) — BP4 lane dossier — songmgr (2026-06-11, read-only recon @ main 78a6ee6, baseline 7785) — `2020-01-01`
- [decomp/research/2026-06-11-bp4-uicomp.md](decomp/research/2026-06-11-bp4-uicomp.md) — BP4 lane `uicomp` — UIComponent finishers (recon dossier, 2026-06-11) — `2020-01-01`
- [decomp/research/2026-06-11-bp4-vbase-deep.md](decomp/research/2026-06-11-bp4-vbase-deep.md) — bp4 deep-dive: WHY the banked ObjectDir-vbase patch is net-0 (2026-06-11) — `2020-01-01`
- [decomp/research/2026-06-11-bp4-vocaltrack.md](decomp/research/2026-06-11-bp4-vocaltrack.md) — BP4 recon — lane `vocaltrack` (PORT-THEN-EXTEND) — 2026-06-11 — `2020-01-01`
- [decomp/research/2026-06-11-map0x1c-sweep.md](decomp/research/2026-06-11-map0x1c-sweep.md) — RB3_MAP_0x1C follow-up sweep — results (2026-06-11) — `2020-01-01`
- [decomp/research/2026-06-11-obj-orphan-worklist.md](decomp/research/2026-06-11-obj-orphan-worklist.md) — obj_orphan Cleanup Worklist — 2026-06-11 — `2020-01-01`
- [decomp/research/2026-06-11-object-dirloader-boundary-refutation.md](decomp/research/2026-06-11-object-dirloader-boundary-refutation.md) — Object / DirLoader / Dir Triple Boundary — REFUTED (2026-06-11) — `2020-01-01`
- [decomp/research/2026-06-11-player-plus4-layout.md](decomp/research/2026-06-11-player-plus4-layout.md) — Player +4 layout — RESOLVED: it's `utl/SongPos.h` (DC3 `mPhrase`), not Player.h — `2020-01-01`
- [decomp/research/2026-06-11-sliver-pin-hunt.md](decomp/research/2026-06-11-sliver-pin-hunt.md) — Sliver/Over/Displaced Pin Hunt — Binary-Wide Worklist (2026-06-11) — `2020-01-01`
- [decomp/research/2026-06-11-uicomponent-virtuals.md](decomp/research/2026-06-11-uicomponent-virtuals.md) — UIComponent missing-virtuals reconstruction — research dossier (2026-06-11) — `2020-01-01`
- [decomp/research/2026-06-11-vtable-walls.md](decomp/research/2026-06-11-vtable-walls.md) — Vtable-order walls — rdata-obj slot recovery (2026-06-11) — `2020-01-01`
- [decomp/research/2026-06-11-w5-finishers.md](decomp/research/2026-06-11-w5-finishers.md) — Wave-5 finishers dossier (2026-06-16, read-only scout @ main, baseline 8038) — `2020-01-01`
- [decomp/research/2026-06-16-w5-hashmap.md](decomp/research/2026-06-16-w5-hashmap.md) — Wave-5 hashmap lane — AccomplishmentManager + SongMgr hash_map vein (2026-06-16) — `2020-01-01`
- [decomp/research/2026-06-16-w5-pinaudit.md](decomp/research/2026-06-16-w5-pinaudit.md) — W5 Pin-Audit Triage — 2026-06-16 — `2020-01-01`
- [decomp/research/2026-06-16-w6-hashmap2.md](decomp/research/2026-06-16-w6-hashmap2.md) — Wave-6 hashmap2 lane — SongMgr surgical conversion + candidate elimination (2026-06-16) — `2020-01-01`
- [decomp/research/2026-06-16-w6-pinaudit2.md](decomp/research/2026-06-16-w6-pinaudit2.md) — Wave-6 Pin-Audit Re-Triage — 2026-06-16 — `2020-01-01`
- [decomp/research/2026-06-16-w6-waypoint-audit.md](decomp/research/2026-06-16-w6-waypoint-audit.md) — Wave-6 Waypoint.cpp pin-relocation audit (adversarial honesty gate) — `2020-01-01`
- [decomp/research/2026-06-19-w7-hashmap-blobs.md](decomp/research/2026-06-19-w7-hashmap-blobs.md) — W7 Hash-map Blob Scout — 2026-06-19 — `2020-01-01`
- [decomp/research/2026-06-19-w7-hashmap-thin.md](decomp/research/2026-06-19-w7-hashmap-thin.md) — W7 Hashmap-Thin: MoviePanel + FixedSizeSaveableStream Scan — `2020-01-01`
- [decomp/research/2026-06-19-w7-pinaudit3.md](decomp/research/2026-06-19-w7-pinaudit3.md) — Wave-7 Pin-Audit Round 3 — 2026-06-19 — `2020-01-01`
- [decomp/research/2026-06-19-w8-character-relocate-pin-audit.md](decomp/research/2026-06-19-w8-character-relocate-pin-audit.md) — Wave-8 Character.cpp pin-relocation audit (adversarial honesty gate) — `2020-01-01`
- [decomp/research/2026-06-19-w8-fsss-residual-and-getid-pinext-audit.md](decomp/research/2026-06-19-w8-fsss-residual-and-getid-pinext-audit.md) — W8 adversarial honesty audit — `fsss-residual-and-getid-pinext` — `2020-01-01`
- [decomp/research/2026-06-19-w8-hashmap-exhaustion.md](decomp/research/2026-06-19-w8-hashmap-exhaustion.md) — W8 — hash_map vein EXHAUSTION claim: ADVERSARIAL FALSIFICATION — `2020-01-01`
- [decomp/research/2026-06-19-w8-nearmiss-bport.md](decomp/research/2026-06-19-w8-nearmiss-bport.md) — Wave-8 B-tier near-miss body-port assessment (adversarial planner) — `2020-01-01`
- [decomp/research/2026-06-19-w8-pinaudit-recheck.md](decomp/research/2026-06-19-w8-pinaudit-recheck.md) — Wave-8 Pin-Audit Re-check — Adversarial Refutation Audit (2026-06-19) — `2020-01-01`
- [decomp/research/2026-06-19-w8-uicomp-reconstruction.md](decomp/research/2026-06-19-w8-uicomp-reconstruction.md) — W8 — UIComponent reconstruction (C1/C2/C3): adversarial audit — `2020-01-01`
- [decomp/research/2026-06-19-w8-wall-ledger-audit.md](decomp/research/2026-06-19-w8-wall-ledger-audit.md) — WALL ledger adversarial audit (C5–C9) — 2026-06-19 (wave-8) — `2020-01-01`
- [decomp/research/2026-06-20-w10-deferred-ports.md](decomp/research/2026-06-20-w10-deferred-ports.md) — W10 — deferred-ports: re-derive 4 wave-9 game-port TUs onto main@9037 — `2020-01-01`
- [decomp/research/2026-06-20-w10-gameport-leads.md](decomp/research/2026-06-20-w10-gameport-leads.md) — W10 — gameport-leads (DISCOVER/PLANNER, READ-ONLY in main) — `2020-01-01`
- [decomp/research/2026-06-20-w10-hashmap-clusteralpha.md](decomp/research/2026-06-20-w10-hashmap-clusteralpha.md) — W10 — hashmap-clusteralpha: SongStatusMgr (cluster-α) — ALREADY-BUILT branch + pin-extension — `2020-01-01`
- [decomp/research/2026-06-20-w10-pinaudit-r4.md](decomp/research/2026-06-20-w10-pinaudit-r4.md) — W10 — pin_audit r4 (post-wave-9 neighbours) — `2020-01-01`
- [decomp/research/2026-06-20-w11-AppLabel.md](decomp/research/2026-06-20-w11-AppLabel.md) — W11 DISCOVER dossier — "AppLabel" @ [0x825BB090, 0x825BB5B8) — `2020-01-01`
- [decomp/research/2026-06-20-w11-Campaign.md](decomp/research/2026-06-20-w11-Campaign.md) — WAVE-11 DISCOVER — Campaign.cpp (~0x82590910 anchor) — `2020-01-01`
- [decomp/research/2026-06-20-w11-MiniLeaderboardDisplay.md](decomp/research/2026-06-20-w11-MiniLeaderboardDisplay.md) — W11 Discovery — MiniLeaderboardDisplay (ENGINE/bandobj) — `2020-01-01`
- [decomp/research/2026-06-20-w11-MusicLibraryNetSetlists.md](decomp/research/2026-06-20-w11-MusicLibraryNetSetlists.md) — W11 — MusicLibraryNetSetlists: identify + port + wire + pin the head gap below SongStatusMgr — `2020-01-01`
- [decomp/research/2026-06-20-w11-PrefabMgr.md](decomp/research/2026-06-20-w11-PrefabMgr.md) — W11 PrefabMgr — port-then-pin dossier (2026-06-20) — `2020-01-01`
- [decomp/research/2026-06-20-w11-VoiceoverPanel-megacluster.md](decomp/research/2026-06-20-w11-VoiceoverPanel-megacluster.md) — W11 — VoiceoverPanel megacluster scout (meta_band panel belt) — `2020-01-01`
- [decomp/research/2026-06-20-w12-MainHubPanel.md](decomp/research/2026-06-20-w12-MainHubPanel.md) — Wave-12 MainHubPanel.cpp — port-then-pin DISCOVER (DEFER, no contiguous TU) — `2020-01-01`
- [decomp/research/2026-06-20-w12-ManageBandPanel.md](decomp/research/2026-06-20-w12-ManageBandPanel.md) — W12 dossier — ManageBandPanel (band3/meta_band) — `2020-01-01`
- [decomp/research/2026-06-20-w12-PatchSelectPanel.md](decomp/research/2026-06-20-w12-PatchSelectPanel.md) — W12 dossier — PatchSelectPanel (band3/meta_band) — `2020-01-01`
- [decomp/research/2026-06-20-w12-SaveLoadManager.md](decomp/research/2026-06-20-w12-SaveLoadManager.md) — W12 — SaveLoadManager (band3/meta_band) — DISCOVER dossier — `2020-01-01`
- [decomp/research/2026-06-20-w12-SavedSetlist.md](decomp/research/2026-06-20-w12-SavedSetlist.md) — W12 — SavedSetlist: locate + port + wire + pin the Campaign↔LockStepMgr gap — `2020-01-01`
- [decomp/research/2026-06-20-w12-SongSort-family.md](decomp/research/2026-06-20-w12-SongSort-family.md) — W12 — SongSortNode / SongSort family (band3/meta_band) — DEFER (COMDAT-scatter, no pinnable span) — `2020-01-01`
- [decomp/research/2026-06-20-w12-belt-gap-bisect.md](decomp/research/2026-06-20-w12-belt-gap-bisect.md) — W12 — meta_band belt-gap bisection (the two BIG un-bisected gaps) — `2020-01-01`
- [decomp/research/2026-06-20-w13-AccomplishmentConditional-evict.md](decomp/research/2026-06-20-w13-AccomplishmentConditional-evict.md) — Wave-13 AccomplishmentConditional sliver-evict audit (DISCOVER, read-only main) — `2020-01-01`
- [decomp/research/2026-06-20-w13-SaveLoadManager.md](decomp/research/2026-06-20-w13-SaveLoadManager.md) — W13 DISCOVER — SaveLoadManager (band3/meta_band) — `2020-01-01`
- [decomp/research/2026-06-20-w13-SavedSetlist-retry.md](decomp/research/2026-06-20-w13-SavedSetlist-retry.md) — W13 — SavedSetlist RETRY: header overload already landed → clean self-contained port — `2020-01-01`
- [decomp/research/2026-06-20-w13-gapA-bisect-port.md](decomp/research/2026-06-20-w13-gapA-bisect-port.md) — W13 — GAP A bisection + first-TU port (CriticalUserListener ↔ OvershellSlot) — `2020-01-01`
- [decomp/research/2026-06-20-w13-gapB-bisect-port.md](decomp/research/2026-06-20-w13-gapB-bisect-port.md) — W13 — GAP B bisect-port (the REST of Gap B above/around Award) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L1-bandsongmgr-port.md](decomp/research/2026-06-20-w9-L1-bandsongmgr-port.md) — W9 L1 — BandSongMgr port-then-pin (frontier "bandsongmgr-port") — `2020-01-01`
- [decomp/research/2026-06-20-w9-L1-hashmap-cluster-alpha.md](decomp/research/2026-06-20-w9-L1-hashmap-cluster-alpha.md) — W9 L1 — hashmap-cluster-alpha: ADVERSARIAL DRILL → owner IDENTIFIED — `2020-01-01`
- [decomp/research/2026-06-20-w9-L1-hashmap-unconverted-census.md](decomp/research/2026-06-20-w9-L1-hashmap-unconverted-census.md) — W9-L1 — hash_map UNCONVERTED-caller census (ground-truth COFF) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L1-uicomp-handle-port.md](decomp/research/2026-06-20-w9-L1-uicomp-handle-port.md) — W9 L1 — UIComponent::Handle port (frontier "uicomp-handle-port"): adversarial drill — `2020-01-01`
- [decomp/research/2026-06-20-w9-L10-handle-pairing-wave-85-rebased-on-landed-prereq.md](decomp/research/2026-06-20-w9-L10-handle-pairing-wave-85-rebased-on-landed-prereq.md) — W9 L10 — Handle-pairing wave "85" (rebased on landed prereq a7175af) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L10-land-handle-prereq-a7175af-then-family-a-b3b419e.md](decomp/research/2026-06-20-w9-L10-land-handle-prereq-a7175af-then-family-a-b3b419e.md) — W9 L10 — Land Handle prereq: a7175af then Family-A b3b419e — `2020-01-01`
- [decomp/research/2026-06-20-w9-L10-reveal-sweep-generic-50-on-landed-prereq.md](decomp/research/2026-06-20-w9-L10-reveal-sweep-generic-50-on-landed-prereq.md) — W9-L10 dossier — "reveal-sweep-generic-50-on-landed-prereq" — `2020-01-01`
- [decomp/research/2026-06-20-w9-L10-reveal-sweep-handlers-mode-tooling.md](decomp/research/2026-06-20-w9-L10-reveal-sweep-handlers-mode-tooling.md) — W9 L10 — reveal_sweep `--handlers` mode (val!=0 macro-body reveal tooling) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L2-flowmanager-cuepoint-panel-tu-825BC.md](decomp/research/2026-06-20-w9-L2-flowmanager-cuepoint-panel-tu-825BC.md) — W9 L2 dossier — "flowmanager-cuepoint-panel-tu-825BC" frontier (REFUTED-as-stated, REAL-as-cluster) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L2-handle-check-pathname-systemic.md](decomp/research/2026-06-20-w9-L2-handle-check-pathname-systemic.md) — W9 L2 — handle-check-pathname-systemic (DISCOVER/adversarial) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L2-songupgrademgr-neighbour-tu-id.md](decomp/research/2026-06-20-w9-L2-songupgrademgr-neighbour-tu-id.md) — W9 L2 — SongUpgradeMgr neighbour-TU identification (0x82632C98+) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L2-w9-cluster-alpha-825b8-owner-id.md](decomp/research/2026-06-20-w9-L2-w9-cluster-alpha-825b8-owner-id.md) — W9 L2 — cluster-alpha @0x825B8738 OWNER ID: **SongStatusMgr** (CONFIRMED) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L3-end-handlers-pathname-tail-forcemult.md](decomp/research/2026-06-20-w9-L3-end-handlers-pathname-tail-forcemult.md) — W9 L3 — end-handlers-pathname-tail-forcemult (ADVERSARIAL DISCOVER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L3-handle-blob-pin-owner-id.md](decomp/research/2026-06-20-w9-L3-handle-blob-pin-owner-id.md) — W9 L3 — handle-blob-pin-owner-id (ADVERSARIAL DISCOVER/PLANNER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L3-metaband-manager-neighbour-pin-chain.md](decomp/research/2026-06-20-w9-L3-metaband-manager-neighbour-pin-chain.md) — W9 L3 dossier — metaband-manager-neighbour-pin-chain (ADVERSARIAL) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L3-metaham-dc3-rosetta-batch-id.md](decomp/research/2026-06-20-w9-L3-metaham-dc3-rosetta-batch-id.md) — W9 L3 — DC3 meta_ham Rosetta batch-ID of RB3 manager owners — `2020-01-01`
- [decomp/research/2026-06-20-w9-L4-rockcentral-leaderboard-panel-tu.md](decomp/research/2026-06-20-w9-L4-rockcentral-leaderboard-panel-tu.md) — W9 L4 — "rockcentral-leaderboard-panel-tu" frontier drill (2026-06-20) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L4-songmgr-family-hashmap-cluster-8255f.md](decomp/research/2026-06-20-w9-L4-songmgr-family-hashmap-cluster-8255f.md) — W9 L4 — songmgr-family-hashmap-cluster-8255f (BandSongMgr) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L4-voiceoverpanel-storemainpanel-megacluster-825fc.md](decomp/research/2026-06-20-w9-L4-voiceoverpanel-storemainpanel-megacluster-825fc.md) — W9-L4 dossier — "voiceoverpanel-storemainpanel-megacluster-825fc" — `2020-01-01`
- [decomp/research/2026-06-20-w9-L4-wired-handle-pairing-wave-85.md](decomp/research/2026-06-20-w9-L4-wired-handle-pairing-wave-85.md) — W9 L4 — wired-handle-pairing-wave-85 (ADVERSARIAL DISCOVER/PLANNER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L5-handle-bodyport-large-tier-post-prereq.md](decomp/research/2026-06-20-w9-L5-handle-bodyport-large-tier-post-prereq.md) — W9 L5 — handle-bodyport-large-tier-post-prereq (ADVERSARIAL DISCOVER/PLANNER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L5-reconcile-handle-prereq-FINAL.md](decomp/research/2026-06-20-w9-L5-reconcile-handle-prereq-FINAL.md) — W9 L5 — reconcile-handle-prereq-FINAL (ADVERSARIAL DISCOVER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L5-songsortmgr-family-cluster-pin.md](decomp/research/2026-06-20-w9-L5-songsortmgr-family-cluster-pin.md) — W9 L5 adversarial scout — "songsortmgr-family-cluster-pin" — `2020-01-01`
- [decomp/research/2026-06-20-w9-L5-songstatusmgr-hashmap-pin-evict.md](decomp/research/2026-06-20-w9-L5-songstatusmgr-hashmap-pin-evict.md) — W9 L5 — songstatusmgr-hashmap-pin-evict: VERIFIED REAL, worktree LAND-READY — `2020-01-01`
- [decomp/research/2026-06-20-w9-L6-family-a-handle-check-comma-form.md](decomp/research/2026-06-20-w9-L6-family-a-handle-check-comma-form.md) — W9 L6 — family-a-handle-check-comma-form (ADVERSARIAL DISCOVER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L6-songselect-panel-family-port-scout.md](decomp/research/2026-06-20-w9-L6-songselect-panel-family-port-scout.md) — W9 L6 scout — songselect-panel-family-port-scout — `2020-01-01`
- [decomp/research/2026-06-20-w9-L6-songstatusmgr-residual-bodyports.md](decomp/research/2026-06-20-w9-L6-songstatusmgr-residual-bodyports.md) — W9 L6 — SongStatusMgr residual: REVEAL-dominated, not bodyport — `2020-01-01`
- [decomp/research/2026-06-20-w9-L6-wired-handle-pairing-wave-85.md](decomp/research/2026-06-20-w9-L6-wired-handle-pairing-wave-85.md) — W9 L6 — wired-handle-pairing-wave-85 (ADVERSARIAL DISCOVER/PLANNER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L7-family-a-handle-check-comma-form.md](decomp/research/2026-06-20-w9-L7-family-a-handle-check-comma-form.md) — W9-L7: "Family A HANDLE_CHECK comma-form" — REFUTED — `2020-01-01`
- [decomp/research/2026-06-20-w9-L7-handle-reveal-cascade-round2-binary-wide.md](decomp/research/2026-06-20-w9-L7-handle-reveal-cascade-round2-binary-wide.md) — W9 L7 — handle-reveal-cascade-round2-binary-wide (ADVERSARIAL DISCOVER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L7-meta-music-flow-blob-id.md](decomp/research/2026-06-20-w9-L7-meta-music-flow-blob-id.md) — W9 L7 — meta-music-flow-blob-id (owner-TU identification) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L7-reveal-audit-tool-port-then-pin-branches.md](decomp/research/2026-06-20-w9-L7-reveal-audit-tool-port-then-pin-branches.md) — W9 L7 — reveal-audit-tool + port-then-pin reveal residue — `2020-01-01`
- [decomp/research/2026-06-20-w9-L8-land-familyb-reconcile-handle-prereq.md](decomp/research/2026-06-20-w9-L8-land-familyb-reconcile-handle-prereq.md) — W9 L8 — land-familyb-reconcile-handle-prereq (ADVERSARIAL DISCOVER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L8-map-coverage-reveal-audit-tool.md](decomp/research/2026-06-20-w9-L8-map-coverage-reveal-audit-tool.md) — W9 L8 — map-coverage reveal-audit-tool: TOOL EXISTS, lever = run the existing pipeline — `2020-01-01`
- [decomp/research/2026-06-20-w9-L8-songstatusmgr-residual-deepen.md](decomp/research/2026-06-20-w9-L8-songstatusmgr-residual-deepen.md) — W9 L8 — songstatusmgr-residual-deepen: REAL but base STILL NOT ON MAIN — `2020-01-01`
- [decomp/research/2026-06-20-w9-L8-wired-handle-pairing-wave-post-familyb.md](decomp/research/2026-06-20-w9-L8-wired-handle-pairing-wave-post-familyb.md) — W9 L8 — wired-handle-pairing-wave-post-familyb (ADVERSARIAL DISCOVER/PLANNER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L9-family-a-handle-check-comma-form.md](decomp/research/2026-06-20-w9-L9-family-a-handle-check-comma-form.md) — W9-L9: Family-A HANDLE_CHECK comma-form reconcile — REAL_ACTIONABLE (+21, MEASURED) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L9-handle-attribution-realclass-id-sweep.md](decomp/research/2026-06-20-w9-L9-handle-attribution-realclass-id-sweep.md) — W9 L9 — handle-attribution-realclass-id-sweep (ADVERSARIAL DISCOVER/PLANNER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L9-handle-prereq-LAND-and-rebase-base.md](decomp/research/2026-06-20-w9-L9-handle-prereq-LAND-and-rebase-base.md) — W9 L9 — handle-prereq LAND-and-rebase-base (ADVERSARIAL DISCOVER) — `2020-01-01`
- [decomp/research/2026-06-20-w9-L9-wired-handle-pairing-wave-post-prereq.md](decomp/research/2026-06-20-w9-L9-wired-handle-pairing-wave-post-prereq.md) — W9 L9 — wired-handle-pairing-wave-post-prereq (DISCOVER dossier) — `2020-01-01`
- [decomp/research/2026-06-21-bsim-seedprop-densification.md](decomp/research/2026-06-21-bsim-seedprop-densification.md) — VT-BSim Seed-Propagation Densification — Experiment Verdict (2026-06-21) — `2020-01-01`
- [decomp/research/2026-06-21-dc3-engine-vein-yield-pilot.md](decomp/research/2026-06-21-dc3-engine-vein-yield-pilot.md) — DC3 engine-vein yield pilot (LightPreset) — NEGATIVE on cheap matching; the real lever is foundational struct-reconstruction — `2020-01-01`
- [decomp/research/2026-06-21-songsortnode-va-confirmation.md](decomp/research/2026-06-21-songsortnode-va-confirmation.md) — SongSortNode STEP-1 VA-confirmation sweep (HARD-FRONTIER #2, work item 1) — `2020-01-01`
- [decomp/research/2026-06-21-string-anchor-recall-probe.md](decomp/research/2026-06-21-string-anchor-recall-probe.md) — String/symbol-literal anchoring as an orthogonal recall lever (2026-06-21 probe) — `2020-01-01`
- [decomp/research/2026-06-24-pivot-bodyport-classb-results.md](decomp/research/2026-06-24-pivot-bodyport-classb-results.md) — Post-class-A pivot results — body-port tails + class-B identity-transfer (2026-06-24) — `2020-01-01`
- [decomp/research/2026-06-30-option-C-scan-directions.md](decomp/research/2026-06-30-option-C-scan-directions.md) — Option-C "different scan" investigation — 4 Opus probes + synthesis (2026-06-30) — `2020-01-01`
- [decomp/research/2026-06-30-topo-locator-design.md](decomp/research/2026-06-30-topo-locator-design.md) — Topological Locator — design + verdict (2026-06-30) — `2020-01-01`

### tools/ hardware + console references

- [tools/PS2-GUITAR-ADAPTER-MAP.md](tools/PS2-GUITAR-ADAPTER-MAP.md) — PS2→USB guitar adapter — device identity + control map — `2026-07-19`
- [tools/GUITAR-CRASH-STATE-2026-07-19.md](tools/GUITAR-CRASH-STATE-2026-07-19.md) — Guitar session — crash state dump (2026-07-19) — `2026-07-19`
- [tools/JRPC2-CONSOLE-CALLS.md](tools/JRPC2-CONSOLE-CALLS.md) — Calling console functions live — JRPC2 over XBDM — `2026-07-19`
- [tools/DTA-NAVIGATION-NOTES.md](tools/DTA-NAVIGATION-NOTES.md) — DTA navigation notes — driving RB3 headlessly via RB3E `/execute` — `2026-07-15`

### Other

- [plans/jeff-scattered-unit-addresses.md](plans/jeff-scattered-unit-addresses.md) — dtk asm listings printed synthetic addresses for scattered units — `2026-07-26`
- [plans/switch-frame-census-lever.md](plans/switch-frame-census-lever.md) — The switch-frame lever, automated — `scripts/harvest/switch_frame_census.py` — `2026-07-25`
- [plans/slm-setstate-reconstruction.md](plans/slm-setstate-reconstruction.md) — SaveLoadManager::SetState — dedicated reconstruction project — `2026-07-25`
- [plans/saveloadmanager-port-log-2026-07-20.md](plans/saveloadmanager-port-log-2026-07-20.md) — SaveLoadManager port — LOG / handoff — `2026-07-24`
- [plans/bindiff-transfer-spike-2026-07-20.md](plans/bindiff-transfer-spike-2026-07-20.md) — BinDiff DC3→RB3 identification spike — GO (2026-07-20) — `2026-07-20`
- [plans/remaining-bytes-decomposition-2026-07-20.md](plans/remaining-bytes-decomposition-2026-07-20.md) — Remaining-bytes decomposition + path-to-100 review (2026-07-20) — `2026-07-20`
- [plans/session-2026-07-18-summary.md](plans/session-2026-07-18-summary.md) — Session summary — 2026-07-18 (near-miss cracks + identification stack) — `2026-07-18`
- [plans/review-2026-07-18-next-focus.md](plans/review-2026-07-18-next-focus.md) — Review 2026-07-18 — next best areas of focus — `2026-07-18`
- [plans/jeff-leaf-split-fix-status.md](plans/jeff-leaf-split-fix-status.md) — jeff leaf-split (Class-2 pdata over-split) fix — status — `2026-07-17`
- [plans/triage-nearmiss-100-2026-07-16.md](plans/triage-nearmiss-100-2026-07-16.md) — ≥99% near-miss triage — 100-fn sample (2026-07-16) — `2026-07-16`
- [plans/recarve-pipeline.md](plans/recarve-pipeline.md) — Recarve pipeline — programmatic TU-attribution repair — `2026-07-12`
- [plans/hard-targets-triage-2026-07-12.md](plans/hard-targets-triage-2026-07-12.md) — Remaining hard-targets triage — 2026-07-12 — `2026-07-12`
- [plans/roadmap-2026-07-12.md](plans/roadmap-2026-07-12.md) — rb3-xenon Roadmap — 2026-07-12 (decision doc) — `2026-07-12`
- [plans/jeff-pdata-boundary-round3.md](plans/jeff-pdata-boundary-round3.md) — jeff pdata/boundary defects — round 3 design — `2026-07-12`
- [plans/spill-leverage-campaign-2026-07-10.md](plans/spill-leverage-campaign-2026-07-10.md) — Spill-store leverage campaign (2026-07-10) — `2026-07-12`
- [plans/tu-wiring-census-2026-07-10.md](plans/tu-wiring-census-2026-07-10.md) — TU-Wiring Census — orphan map-entry analysis (2026-07-10) — `2026-07-10`
- [plans/build-without-xdk-recommendation.md](plans/build-without-xdk-recommendation.md) — Building the RB3Enhanced Same-Instrument Patch Without the Xbox 360 XDK — `2026-07-07`

---

## Maintenance

- **New docs must be linked here** in the right section — an unlinked doc is invisible to the
  next agent.
- **`decomp/research/` and `decomp/handoff/` are append-only archives.** Add new dated records;
  do not rewrite old ones. When a record is superseded, add a banner pointing forward rather
  than deleting.
- Keep each entry to one line: relative link + ≤120-char description.
- When a plan is executed or superseded, move it into §4 and, if it makes current-state claims,
  give it a `> **STATUS (YYYY-MM-DD):**` banner under its title.
