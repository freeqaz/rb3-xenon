# rb3-xenon decomp — state & live veins (2026-07-20)

**Current (MEASURED at HEAD `5524a135`, clean full rebuild on main): 41,218
strict-matched functions / honest proxy 39,709 / `matched_code_percent`
35.245968** (honest = matched − masked_equal, per the BO-8 pricing rule;
`build/45410914/report.json`). Denominator is the whole TU5 XEX
(`total_functions` 69,367; `matched_code` 3,729,036 B; `masked_equal` 1,509;
`total_code` 10,580,036).

> **2026-07-30 — from 41,187 / 39,677 / 34.924870 to the above: +31 matched,
> +32 honest, +0.321 pp**, across four landings by **two concurrent sessions**:
>
> | commit | matched | masked | source |
> |---|---|---|---|
> | `f149a4b7` | 41,213 | 1,510 | compiler flip to 10224 (**+26**) |
> | `8bc771f5` | 41,214 | 1,510 | lane CB-3 `RGTrainerPanel::HandleChordLegend` (+1) |
> | `d7bd717d` | 41,215 | **1,509** | *other session* — CC-recrack `String::replace` (+1) |
> | `5524a135` | **41,218** | 1,509 | lane CB-5 Campaign/Game/OvershellSlot (**+3**) |
>
> ★ **The ledger is EXACTLY additive across all four — there was no split churn.**
> The coordinator initially attributed a 1-function gap between its worktree legs
> and main to the ±2 churn floor; that was **wrong**. The gap was entirely the
> *other session's* landing, and once `d7bd717d` is in the chain every figure
> reconciles to the digit. ⇒ ⚠ **In a shared tree, check `git log` before
> explaining a discrepancy as noise** — "churn" is an available excuse that can
> silently absorb another agent's work, and here it would have mis-credited +1.
>
> ⚠ Intermediate figures quoted mid-wave came from **worktree legs off different
> bases** and are not comparable to each other; only HEAD numbers are
> main-measured. Deltas *within* a lane (same worktree, same commit, split frozen)
> remain valid.
>
> ★ Independent corroboration from the other session: it also classed
> `RndLine::SetNumPoints` 98.5 → 69.7 as a **CSE/register-pressure `build_env`
> artifact with no non-contorting source fix** — matching lane CB-2's regalloc
> diagnosis of the same regression, reached by a different route.

> ★★★ **THE COMPILER WAS WRONG FOR MONTHS, AND THE WALL BZ-1 SIZED WAS IT.**
> Retail RB3 was built with X360 `cl.exe` build **10224** (XDK 2.0.11164); the
> fleet had been building with **11886** (the DC3-retail toolchain). `f149a4b7`
> flipped the default to 10224: matched 41,187 → **41,213** (+26), honest
> 39,677 → **39,703** (+26), code% 34.924870 → **35.046990** (**+0.122 pp**),
> `masked_equal` and fuzzy both flat, **zero losses**.
>
> +0.122 pp lands within 4% of the ~0.127 pp lane BZ-1 had measured as
> "one-directionally unreachable" and told us to stop chasing (below, under "two
> veins closed at the MECHANISM level").
>
> ⚠★★★ **THAT MAGNITUDE AGREEMENT IS A COINCIDENCE — DO NOT CITE IT AS
> CONFIRMATION.** The coordinator originally wrote it up here as "two independent
> methods converged"; **lane CB-2's mechanism-level audit refutes that**. Decomposing
> all 26 gainers by the idiom that actually gated them:
>
> | gating idiom | gainers |
> |---|---|
> | strcpy byte-loop `cmplwi`↔`extsb.` (BZ-1 predicted) | **13** |
> | redundant `clrlwi` (ours extra) — **never enumerated** | 7 |
> | extra sign-extend `extsb`/`extsh` — **never enumerated** | 3 |
> | `None`↔`mr` (OvershellPanel) — **never enumerated** | 1 |
> | multi-defect (`inflate` 41 rows, `sprintbuf` 13 rows) | 2 |
> | **pow2 `srawi.`↔`clrrwi.` (BZ-1 predicted)** | **0** |
>
> So only **13 of 26** came from the predicted mechanism, **~half the recovered pp
> comes from idioms BZ-1 never enumerated**, and **one of the two predicted arms
> contributed exactly zero**. The right summary is: *BZ-1 correctly detected that a
> toolchain wall existed and sized it luckily well, but its causal account was
> half wrong.* ★ **Lesson: two numbers agreeing is not two methods converging.**
> A magnitude match across different mechanisms is coincidence until the
> decomposition is checked — and checking it is cheap.
>
> ⛔ **Corollary: the pow2 idiom is still OPEN, not collected.** CB-2 scanned
> instruction **encodings** across all 1,094 objs under both compilers:
> `srawi.` 6,368→6,368 and `clrrwi.` 538→538 (**Δ0**), while the same scanner on
> the same objs resolved the strcpy swing (`extsb.` 305→124, `cmplwi`
> 103,663→103,839) — a **positive control** proving genuine sensitivity, not
> blindness. Total `.text` moved −736 B, so codegen definitely changed; pow2
> selection simply did not. ⇒ BZ-1's 6 fwd / 0 rev has a **different, still-live
> cause, most plausibly source-level** (expression form or signedness, e.g. `x/8`
> vs `x & ~7`). That share of the ~0.127 pp was never recovered and remains
> chaseable.
>
> ⚠ **Every match% figure recorded before `f149a4b7` was measured against the
> wrong compiler.** Pre-flip and post-flip numbers are not comparable, and every
> `at_limit` / `build_env` verdict, near-miss ranking, and instruction-selection
> claim in `docs/decomp/patterns/unfixable-compiler.md` predates it. Wave CB is
> re-triaging exactly that.
>
> ★ **Transferable lesson:** `CLAUDE.md` justified the toolchain by inference —
> *"RB3 is the same Harmonix toolchain era and engine, so likely the same flags."*
> The structurally identical inference about the compiler **build** was wrong and
> cost ~0.12 pp for months. The **flags** rest on the same unverified reasoning
> and are being measured now (lane CB-4). Measure the toolchain; don't infer it.

> ## Lane CB-2 — the canonical post-swap census (2026-07-30)
>
> **Determinism control first:** two independent 10224 builds gave **0
> per-function differences over 69,366 functions**, so the ±2 split-churn floor
> never bit and all 48 changed functions are attributable to the swap alone.
> Compiler identity was verified **at the object level** (`@comp.id` = `0xAB27F0`
> on 40/40 objs leg A, `0x…2E6E` on 60/60 leg B), not from configure's banner.
> Also confirmed: `match_percent_normalized == 100` counts **exactly 41,213** =
> headline `matched_functions`, i.e. that field is what drives the metric.
>
> **Direction histogram — 48 changed of 69,366: 45 up / 3 down.** 26 crossed to
> 100, **0 fell from 100**, 22 moved without crossing. Independent corroboration
> that nothing regressed: of `decomp.db`'s 5,665 `COMPLETE` rows present in the
> report, 62 sit below 100 under 10224 — but **all 62 were already below 100 under
> 11886**, so the swap regressed none of them.
>
> **The one genuine cost: `RndLine::SetNumPoints` 98.500 → 69.693** (default/Line),
> 141 mismatches of 244 instructions at identical 976 B. 10224 emits an extra
> `mr r4, r28`, picks different registers, and allocates a 16-byte larger frame
> (`stwu r1,-0xf0` vs retail `-0xe0`); one inserted instruction desynchronises
> objdiff alignment and cascades. **Class: regalloc**, not a logic regression. It
> never crossed 100 in either direction so Δhonest and Δcode% are both 0 — but it
> is a named, real regression and the swap is not free. (Other two down-movers:
> `fft_matrix_inverse_columnwise` −0.138, `Tail::UpdateVerts` −0.128, VMX
> scheduling noise.)
>
> ### ✅ Regenerated near-miss pool — USE THIS, not BZ-1's handoff
> **`/home/free/tmp/laneCB2out/nearmiss_pool_10224.json`** (+ `SUMMARY.txt`),
> deliberately written **outside** any git worktree so a `worktree remove --force`
> cannot destroy it. 4,158 rows, carrying its own provenance, field definitions and
> regeneration command.
> - **named, 0<pct<100: 2,286 rows / 1,011,268 B / 9.5583 pp** — the actionable set.
> - anonymous: 1,872 / 87,636 B / 0.8282 pp (anon bodies can never pair — see the
>   anon naming gate).
> - Only **22 of 4,158** rows changed with the swap: the pool's *membership* was
>   barely affected. What was stale in BZ-1's handoff was its **calibration and
>   provenance**, not its contents.
>
> ⚠★★ **Two ranking traps, both load-bearing:**
> 1. **`penalty = size·(100−pct)` over the FULL population is dominated by large
>    near-ZERO-pct bodies** (`RndMesh::SyncProperty` 0.99%, `UIList::SyncProperty`
>    0.49%) — those are **unported, not near-miss**. Rank by raw penalty and you
>    will be handed unported giants. Use `ranked_views.near_miss_ge90_by_penalty`
>    (**939 rows / 487,736 B / 4.61 pp**): `SaveLoadManager::SetState` 91.7%,
>    `CharacterCreatorPanel::Handle` 95.2%, `RndPropAnim::Handle` 93.2%,
>    `UILabel::SyncProperty` 94.1%.
> 2. **BZ-1's pool was SIZE-ranked, not penalty-ranked** despite its handoff text
>    claiming otherwise (`ranked_views.ge99_by_size` reproduces its shape exactly).
>    Its `MusicLibrary::Handle` 6,160 row was **already stale — 100.0% even under
>    11886**. And its headline signature (1,061 fns / 245,248 B / 2.32 pp) is **not
>    reproducible** under any of 14 filters tried, because the pool definition was
>    never recorded. ⇒ ★ **A worklist without its generating query is unauditable.**
>    The new artifact embeds both.
>
> ⛔ `decomp.db` re-ingest is **low urgency for correctness**: all rows predate the
> swap (`max(updated_at)` 2026-07-29), but the swap invalidated **0 of 2,539**
> `AT_LIMIT` rows and broke **0** `COMPLETE` rows. Hygiene only.

> ## ⛔ Lane CB-6 — the "99 regressed COMPLETE rows" lead is a QUERY ARTIFACT
>
> CB-3 surfaced "99 of 5,809 `COMPLETE` rows are now below 100%" and the
> coordinator funded a lane to repair them. **There was nothing to repair:
> genuine regressions = 0 of 99.** Buckets: **(a) genuine 0**, (b) deliberate
> correction 1, (c) DB artifact 98, (d) flip-caused 0 (disjoint from all of a/b/c).
>
> ★★★ **The count is a function of how you ask.** `matched_functions` counts
> `match_percent_normalized ≥ 99.999` (verified — reproduces 41,214 exactly), so
> only the first row here is metric-relevant:
>
> | join keying | scoring field | count |
> |---|---|---|
> | `(unit, symbol)` | `match_percent_normalized` | **52** ← metric-relevant |
> | `(unit, symbol)` | `fuzzy_match_percent` | 87 |
> | symbol only | `fuzzy_match_percent` | **99** ← the reported number |
>
> Also: the DB's `current_percent` is a **frozen self-report** — 0 of 5,809
> COMPLETE rows have `current_percent < 100`; the discrepancy exists only against a
> *fresh* report.json. ⇒ ★★ **Always state the join key and scoring field beside a
> cross-source count**, or it is unreproducible. (Same lesson as the pool
> provenance above: a worklist without its generating query is unauditable.)
>
> The 98 artifacts: **35** have `norm ≥ 99.999` but `fuzzy < 99.999` — reloc-masked,
> and they **never left `matched_functions`** (30 are `default/keygen_xbox`);
> **12** are stale unit keys (11 `auto_03_*_text` boundary shifts; 2 score 100.000
> under a renamed unit); **51** are unverified bulk-promotes.
>
> ### ★★★ "COMPLETE" is a BULK RELABEL, not a work record
> **5,619 of 5,809 COMPLETE rows (96.7%) share ONE timestamp, `2026-07-24
> 10:18:06`**, from `scripts/sync_match_percent.py --promote`; 4,854 have
> `last_model = NULL`. **Only 839 of 5,809 (14.4%) carry any attempt record.** So
> "already-solved work that broke" is false for ~96.7% of the ledger. Precedent
> already in-tree: `scripts/reset_false_complete.py` documents an earlier
> false-COMPLETE class where `base_size=0` made objdiff report 100%.
>
> ★ **The one real drop was a correction, and the lane caught itself.**
> `??_GRndScreenMask@@UAAPAXI@Z` 100.000 → 99.950 reads as a genuine vbase
> regression under objdiff. It is not: `42fe0db0` (lane BQ-2) had already proven
> the retail body at `0x824816a8` calls `??_DRndMultiMeshProxy@@`, so the row is a
> **mispair that only scored 100 because the differing `bl` is a relocation masked
> by `functionRelocDiffs=none`**. Repairing it would have re-introduced a
> known-false match. ⇒ **read the commit that caused a drop before "fixing" it.**
>
> ### ★★★ `check_regression_lock.py` is DEAD — and as keyed would DEFER EVERY LANDING
> Executed, not merely read. The *code* is sound (runs, exits 1 correctly, sane
> `--allow-drop`). Everything around it is not: **unwired** (zero executable
> references; `scripts/harvest/land.sh` never calls it — `TOOLING.md` lists it
> "WORKING", true of the code and false of the deployment), **baseline 9 days
> stale** (`a08764f1`, 07-21), and **S/N 38 / 1,664 = 2.3%**. 1,626 flags are
> `100.000 → 0.000` = *key absent* across 153 units, but only **30** of those units
> are gone — the rest are empty shells from `5f93efb6` (laneDUPUNIT), an
> **explicitly net-0** duplicate-unit merge (`AppLabel` 158, `VocalPlayer` 192,
> `Campaign` 134 all still exist with 0 functions).
> ★★★ **A net-0 housekeeping landing looks to this guard like a 1,626-function
> catastrophe — and naming an anon function, our most productive lever, is
> indistinguishable from a regression under its `(unit, fn_name, occ)` keying.**
> That is presumably why it was never wired.
> **Fix list (value order):** (i) split "key absent" from "measured drop", fail only
> on measured drops → 1,664 → 38 immediately; (ii) wire both halves into `land.sh`
> (check before, `snapshot_landing.py` after) — the stale baseline is the root
> cause; (iii) exclude `auto_*` units, whose boundaries shift by construction.
>
> ⚠ Residual uncertainty the lane flagged against its own claim: for the 38 rows
> that were <50% at 07-21 and bulk-promoted on 07-24, **no measured record
> survives**, so `(a) = 0` for them rests on absence-of-attempt-records plus the
> false-100 mechanism, not on a measurement. Also, "unmapped-anon ⇒ unpairable" is
> **not** a valid blanket rule — 249 other unmapped-anon COMPLETE rows read 100.

> ✅ **FLOOR RESOLVED — this is now a direct measurement, not a sum.** BZ-1's
> lander built HEAD itself (BZ-1 `e7dc97dd` + BZ-3 `b16c9e8c` in one worktree,
> `config.yml` touched so the map repoint actually re-splits) and read
> `report.json`: **41,187 / 1,510 / 39,677 / 3,695,064 B / 34.924870**, with
> `total_functions` **69,367 across the re-split** (no split churn).
>
> ★ **The additivity assumption held exactly.** The preceding floor block
> predicted 41,187 / 39,677 / 3,695,064 B by arithmetic and worried that BZ-1's
> `Object.h` / `ObjPtr_p.h` edits — which cascade to ~281 TUs — could perturb the
> units BZ-3 repointed. They did not: the measured triple is identical to the
> summed one (the summed `≈34.924872` was a hand-recomputed percent; the measured
> value is `34.924870`). **A wide PCH-header cascade and a map repoint were
> orthogonal here** — worth knowing, but note this is one observation, not a
> license to stop measuring. The floor discipline again did its job: the floor was
> flagged, the next lane measured, and the flag cost nothing.

> **Lane BZ-1 (`e7dc97dd`) — +2 honest, +744 B, from two DC3-vs-retail body
> defects.** Re-A/B'd at true HEAD (`f181a271`), *not* carried from the lane's
> original run off `66697375`, because BY-2 and BZ-2 landed in between — and BZ-2
> touched two of the same files. Same-split (edits touch no split inputs, so
> `config.yml` was touched on **neither** leg): matched 41,185 → 41,187 (**+2**),
> `masked_equal` flat 1,510, honest 39,675 → **39,677**, `matched_code`
> 3,694,092 → 3,694,836 (**+744 B**), code% 34.915688 → 34.922714
> (**+0.007026**), `total_functions` 69,367 both legs.
>
> **(1) `ObjPtrList<T1,T2>::Load` carried two DC3-era parameters retail lacks.**
> Retail's call site sets exactly `r3=this, r4=bs, r5=1`; we emitted
> `li r7,1; li r6,0; li r5,1`. The mangled tail `_NPAVObjectDir@@1@Z` decodes as
> `(bool, ObjectDir*, bool)`, and **rb3-Wii `ObjPtr_p.h:517` declares the retail
> form `Load(BinStream&, bool)`**. The 4th param was never read and the
> `ObjectDir*` was `nullptr` at every site ⇒ semantics-preserving. Every
> `bs >> someObjPtrList` in the tree had been paying two dead `li` setups.
> **(2) `AsyncFile::Init` doesn't save/restore `UsingCD`** — retail has no
> `bl ?UsingCD@@` and restores with the constant `li r3,1`, i.e. plain
> `SetUsingCD(true)`. CharIKScale::Load 96.47 → 100 (232 B); AsyncFile::Init
> 97.97 → 100 (512 B); **+744 = exactly 232 + 512**, reproduced against the newer
> main.
>
> ★★ **The corroboration is the one-directional spread, not the two 100s.**
> Per-function set diff: **14 functions changed, all 14 UP, 0 down.** Twelve more
> `Load`/`PostLoad` bodies improved without crossing the line — and every one is a
> `bs >> objPtrList` caller (RndAmbientOcclusion 17.11→31.65,
> TrackPanelDirBase::PostLoad 34.09→38.53, FaderGroup 70.28→74.15,
> UIList::PostLoad 65.11→68.68, RndSoftParticles 34.09→36.68, Character
> `operator>>` 87.62→90.70, RndEnviron 80.29→82.60, Spotlight 63.03→64.37,
> CharEyes 56.39→58.72, RndText 58.71→59.30, TrackWidget 21.31→22.03,
> CharPosConstraint 16.31→16.42). A *wrong* signature would have pushed some
> callers away from retail; strictly-monotone improvement across twelve
> independent bodies is evidence the correction is real rather than metric-fitted.
>
> ⚠ **But the family is far smaller than the blast radius.** 78 objs reference the
> symbol; the paired post-filter over all 2,420 named near-misses finds only **13
> callers (9,740 B)**, and 12 carry 24–563 independent mismatches where two `li`
> are a rounding error. **Exactly one function was gated solely by this.** Textbook
> site-count ≠ defect-count.

> **Lane BZ-3 (`b16c9e8c`) — +0 functions, +228 B code, and it REFUTED ITS OWN
> BRIEF.** Same-split A/B off `07795e26`: matched 41,185 → 41,185 (+0),
> `masked_equal` flat 1,510, honest 39,675 → 39,675 (+0), `matched_code`
> 3,694,092 → 3,694,320 (**+228 B**), code% 34.915688 → 34.917840
> (**+0.0021550pp**), `total_functions` 69,367 both legs. Both legs were re-run
> and reproduced bit-for-bit. Per-unit over all 3,917 units: exactly three gained
> (SpotlightDrawer +108, Mic +60, ExternalMic +60), **zero regressed**, per-unit
> sum == aggregate. **+228 = exactly 108+60+60.**
>
> ★★★ **The premise "a differing shift amount means OUR struct size is wrong" is
> WRONG — the lever inverts.** In every resolvable case our size was RIGHT and the
> MAP had named the wrong STL sibling. Three cases settle it at the *language*
> level, no header needed: Mic `vector<const MoveParent*>` ours 4 = `sizeof(T*)`
> ILP32 vs retail 2; ExternalMic `vector<unsigned short>` ours 2 by the standard vs
> retail 4; TexBlender `vector<pair<T*,float>>` ours 8 = 4+4 vs retail 64. **A
> vector of pointers cannot have 2-byte elements.** So the shift amount is a
> **SIBLING DISCRIMINATOR for map pairing, not a `sizeof` oracle**: STL COMDAT
> siblings are byte-identical *except* the shift field, and objdiff folds exactly
> that field away (`powerpc-0.4.1` decodes SH/MB/ME to `Argument::OpaqueU` →
> `arch/ppc/mod.rs:173` `InstructionPart::opaque` → `diff/code.rs:1152`
> `is_immediate == false`), so byte-similarity naming cannot tell twins apart and
> picks arbitrarily. All three fixes are **repoints**, not size changes.
>
> ★★ **Why Δhonest 0 is the CORRECT result, not a null.** These rows were *already*
> credited by `matched_functions` (normalized-100) but **not** by `matched_code`,
> which counts only `match_percent == 100`. A correct repoint is therefore Δ0 on
> the function axis and +size on the code axis. Gate used: land if Δcode% > 0 **and**
> Δhonest ≥ 0. This is the BO-8 amended pricing rule doing real work.
>
> ⛔ **CHANNEL SIZED AND DRAINED — do not re-hunt.** Over the whole 220-function
> norm-100/raw<100 population: `no_sibling` (real body divergence) **192**, anon
> byte-fallback **20**, UNIQUE repoint **3**, ambiguous ICF **2**, already-equal
> **3**. A wholesale shift-amount size oracle is **not feasible**; the vein is
> 3 rows / 228 B. The 2 ambiguous rows were deliberately **not** shipped — they are
> ICF fold classes where *every* candidate is byte-exact, so the metric cannot
> adjudicate correctness and picking one buys **+144 B of false credit**.
>
> ★★ **ANTI-VACUITY GUARD IS LOAD-BEARING FOR ANY MASKED COMPARATOR (new trap).** A
> 16 B vbase thunk `?PreLoad@BandLeadMeter@@$4...` "matched" `?FastCos@@YAMM@Z`
> because the union of both sides' relocated words covered **all four** words — the
> masked compare was **vacuously true**. The guard (≥4 compared words AND ≥50% of
> body) removed 5 of 8 spurious hits, 4 UNIQUE → 3. This is the concrete form of the
> known "masked byte-compare inverts fold adjudication" hazard.
>
> Sub-leads closed, do not re-hunt: the 10 Family-B `rlwinm` bitfield-mask rows
> (incl. the TrackWidget `fn_827E39xx` septet) are **anonymous with no map row** —
> objdiff paired them by fuzzy byte-fallback, the documented funclet class, not
> header bugs. The DateTime rows are **not** a constant defect: retail emits
> `addi r3,r10,<"%04d">` then `addi r4,r11,0x76c` and we emit the same two
> instructions **transposed**, so objdiff pairs them crosswise; `0x76c` (=1900) is
> correct on both sides. A schedule swap, and the permuter is banned.
>
> ➡ **Recommended, NOT done** (fleet-wide scoring change needing its own A/B, and
> rebuilding `../objdiff` silently changes fleet split output): make PPC SH/MB/ME
> count toward the normalized score (`arch/ppc/mod.rs:166-174` → emit as
> `InstructionPart::unsigned`, or widen `is_immediate`). objdiff's own comment says
> wrong-struct-size immediates "must count" — on PPC that intent is defeated.
> Estimated truthful correction: **−21 `matched_functions`**, 15 of them anon
> byte-fallback. ⚠ Scripting gotcha: the config id is **dotted**
> `ppc.calculatePoolRelocations`, not camelCase. Also: `objdiff-cli`'s
> `normalized_match_percent` is a **misnomer** — it is `match_percent` under
> reloc-normalization, *not* `match_percent_normalized`; the CLI never exposes the
> arg-normalized score, only `report.json` does.
>
> ⚠ **Landing note:** a map repoint is a **silent no-op without a re-split**, so both
> legs forced `touch config/45410914/config.yml`; the repoint was then *proved* to
> survive by showing the old mangled name go 1 → 0 and the new one 0 → 1 in the
> three **defining** target objs, with the neighbouring control row
> `?resize@...VSpotlightEntry@SpotlightDrawer...` untouched at 13 occurrences.
> Residual old-name hits persist only in **stale `auto_*` target objs** the split
> did not regenerate — identical in both legs, pre-dating the change. Tool landed:
> `scripts/harvest/sibling_shift_disc.py` (read-only, build-neutral).

> ⚠ **The previous headline (41,171, commit `21474152`) was already stale when it
> was written.** BY-2's lander measured its own lane correctly off `66697375`, but
> committed its `docs(state)` *after* BZ-2's code commit `07795e26` had already
> landed underneath it, so the number it published described a tree that no longer
> existed. **Two landers racing on a shared main can each be individually correct
> and still publish a wrong headline** — the fix is to measure HEAD itself, which
> is what `f181a271` did (41,185 at `07795e26`). Encouragingly the two lanes proved **exactly
> additive** here: 41,169 + 2 (BY-2) + 14 (BZ-2) = **41,185 measured**, with
> `masked_equal` flat at 1,510 and `total_functions` 69,367 unchanged even though
> BY-2 edited `splits.txt` (a split input).

> ✅ **RESOLVED.** The preceding commit `38e579c6` correctly flagged its 41,171 as
> a *floor* and asked the next lane to re-measure at HEAD rather than sum. That
> re-measurement is done and was published as `f181a271` (41,185 measured at
> `07795e26`, one build in a worktree whose 30 changed files were hash-verified
> against HEAD); the headline above has since gone back to being a floor. The floor
> discipline worked exactly as intended — **flagging a known-stale headline is
> strictly better than publishing a summed one.**

> ⬇ **Part of that number is a DELIBERATE −3.** Lane BX-1 (`344ebc69`) deleted
> three map rows that were awarding credit for functions retail does not contain:
> on its own it moved 41,170 → 41,167 matched / 39,660 → 39,657 honest while
> `code%` **rose** 34.810390 → 34.810730. **The first landing priced on being LESS
> wrong rather than more matched** — read it as the headline being *corrected
> downward*, not regressed. Lane BX-2 (`43c9771e`) then added +1 on top, and the
> measured HEAD total confirms the two are cleanly additive (41,170 −3 +1 =
> 41,168, measured 41,168 with `masked_equal` flat at 1,510 and
> `total_functions` 69,367 throughout). See the BX-1 block below.
> ⚠ BX-2 landed without its own `docs(state)` commit, so this headline had to be
> re-measured at HEAD rather than carried from either lane's A/B.

> **Wave BU (2026-07-30) — +15 honest, +0.0101pp code, `masked_equal` FLAT at
> 1517 through every leg** (so no step bought its gain with byte-fallback
> pairings). Attributed legs, one worktree, one split:
>
> | step | matched | masked_equal | honest | code% |
> |---|---|---|---|---|
> | base | 40936 | 1517 | 39419 | 34.497240 |
> | +BU-1 `6c0b1e33` BandStorePanel vbase | 40945 | 1517 | 39428 | 34.504116 |
> | +BU-2 `1ada82c1` StoreOffer 97→100/100 | 40948 | 1517 | 39431 | 34.505970 |
> | +BU-4 `28ba1a45` reloc_disc live funnel | 40951 | 1517 | 39434 | 34.507330 |
>
> ✅ **The 99.8481% SetType/vbase cohort is CLOSED** (BQ-2 → BS-1 → BU-1).
> ★★★ **Two lanes refuted their own briefs, and both refutations were the
> finding.** BU-1: BS-1's "no rb3-Wii oracle exists for BandStorePanel" is
> **false** — the oracle exists and that one correction solved the target.
> BU-2: BT-1's "StoreOffer.cpp is unpinned" is **false** — it was pinned and
> scoring 97/100; `splits.txt` uses the **bare basename** while `objects.json`
> uses the **path**, so a path grep reports a pinned unit as unpinned.
> ⇒ **Verify a claimed absence (of an oracle, of a pin) before building on it.**
> ★★ **Source alone can move nothing**: BU-2's three functions compiled
> *byte-identical to retail* and still scored 0% until map rows were added — the
> false-0% trap masking already-correct code.
> ★★ **Sizing the vbase channel by counting sub-100 neighbours overestimates it**
> — BU-1 predicted +10..+20, measured +9; the 99.8/99.9 bodies were thunks with
> no vbase adjust and none moved.

> **Wave BV (2026-07-30) — +171 honest, +0.172058pp code.** Three lanes landed by
> patch (both branches predate BV-2; a merge would have reverted BV-2's
> `splits.txt` deletion). Each re-verified in its own fresh worktree, both legs
> same-split, `total_functions` 69367:
>
> | | matched | masked_equal | honest | code% |
> |---|---|---|---|---|
> | base `dedf6c34` | 40953 | 1518 | 39435 | 34.513645 |
> | +BV-3 `e22878ef` | 40957 | 1518 | 39439 | 34.518597 |
> | +BV-1 `5482a0a3` | 40964 | 1510 | 39454 | 34.528618 |
> | +BV-4 `1a4ad18c` | 41116 | 1510 | 39606 | 34.685703 |
>
> ★★★ **BV-4 is the wave: +152 honest / +0.157085pp from map rows alone, no
> source change.** The lever is the **anonymous-target naming gate**.
> `pair_funclets_by_bytes` (objdiff-core `diff/mod.rs:1410`) is the only path an
> anonymous target symbol can pair through, and it demands `is_funclet_like` on
> **both** sides — so an anonymous target *body* whose counterpart we already
> supply can never pair at any similarity, and scores exactly 0.0 forever.
> 6,260 such bodies (1.29 MB) sit in already-pinned, already-supplied units.
> Naming them is the whole fix. `masked_equal` flat at 1510, all 152 land at
> 100.0%, net delta exactly +152 ⇒ zero regressions. Vein is far from drained.
> ★★★ **The lane shipped 177 rows; 25 were dropped as sibling/twin mispairs.**
> Each of the 25 claims a mangled name another VA already holds. Cross-unit, so
> obj symbol tables stay clean — but branch-normalized disassembly shows the
> pairs are *structurally identical and semantically different*, differing only
> in an absolute address or a single `bl`: `??1ObjRefOwner@@UAA@XZ` stores a
> different **vtable**, `??0Symbol@@QAA@XZ` a different string constant,
> `?ClassName@MsgSource@@UBA?AVSymbol@@XZ` calls a different StaticClassName,
> `?JoypadUnsubscribe@@YAX...@Z` reads a different global. ⇒ **BV-4's
> "byte-perfect" selector is reloc-masked** and must resolve relocations before
> reuse. Open: the *pre-existing* row may be the mispair in some of the 25;
> adjudicating needs a twin discriminator plus a map DELETE (a no-op without a
> re-split).
> ★★ **A raw byte-compare of two VAs INVERTS the verdict** — PC-relative `bl`
> displacements must be resolved to absolutes first. My first pass called all 25
> "different" for the wrong reason and would have been right by luck.
> ★★ **Refuted en route:** "a mangled name at two VAs means one is false" is
> **not** a valid rule here. A normalized scan of all 77,986 `.text` functions
> finds 2,129 identical-body groups spanning 11,932 VAs (15.3%), dominated by
> 40B/44B funclets — duplicate bodies at distinct addresses are ordinary. The 25
> were rejected on being normalized-*different*, not on being duplicated.
> ⚠ **Trap:** BV-4's own commit `json.dump`-rewrote `scripts/target_symbol_map.json`
> (1-space → 2-space indent, the four compact metadata arrays exploded one element
> per line, trailing newline dropped). A **28,769/27,051-line diff for 177 rows is
> the tell** that a lane bypassed the appliers. Land such a branch by re-splicing
> its rows onto main, never by taking its file.
>
> BV-1 (StoreOfferProvider body port, +7 matched / −8 masked_equal / **+15
> honest** / +0.010021pp) was measured off `e406eef4`, and its leg A reproduced
> BV-3's post-landing headline **exactly** — so the two lanes are fully additive
> with no interaction, and the wave total is a measurement, not a sum of claims.
> ★★ **Pricing by Δmatched alone would have scored one of its legs zero:** the
> BuildList local-static-Symbol leg moved `matched` by +0 while converting five
> EH funclets from masked-equal to honestly equal (−5 `masked_equal`). Δhonest is
> what saw it.
> ★★ **Retail sizeof is 0x4C, not our 0x50 — `mPacks` does not exist.** Proven
> three ways without a map: the ctor writes nothing to 0x4c, no function in the
> 3546-line unit asm touches 0x4c, and `BandStorePanel.s:0x82605204` does
> `li r3, 0x4c` before `operator new`. The Wii DEV oracle's `mPacks` fallback
> loops in FindOffer/FindPack/FindAlbum are likewise absent from retail, as are
> the `mElements.size()` guards (DEV-build artifacts).
> ★ **A third splits-attribution defect in this family:** 312 bytes
> (`0x82663830..0x82663968`) sat under `SongSortNode.cpp` while sandwiched
> between two StoreOfferProvider blocks; it holds three StoreOfferProvider
> methods `Handle` calls straight into. Reclaiming it cost SongSortNode nothing
> (108 matches before and after, code% **rose** 91.72 → 94.21). The block is
> `.pdata`-less — expected for three frameless leaves — so no `.pdata` moved with
> it, which is why the edit is `.text`-only.
>
> **BV-3's** Δ = exactly 524 bytes = 316+84+112+12, a **1:1 attribution to four
> map rows** — no hidden extra match, no regression.
> ⛔ **The reloc_disc COLLISION-repoint pool is CLOSED — metric-inert by
> construction, do not re-hunt.** `report generate` hardcodes
> `functionRelocDiffs=None`, and `masked_equal` never discloses reloc masking, so
> a DECISIVE collision verdict by definition moves a name between two byte twins
> that *already both score 100%*. All 32 applied and measured: **delta 0 on every
> axis**, matching `repoint_supply.py`'s no-build prediction 32/32. It was not a
> silent no-op either (13 symbols changed unit, one body went 0.0%→99.8%) — it
> was live and still moved nothing.
> ★★ **The yield was the mirror class: SIZE-IMPOSSIBLE rows** (mapped target size
> ≠ same-unit base COMDAT size ⇒ can never reach 100% ⇒ repointing is strictly
> non-negative). Census: 1,867 of 21,482 rows, 19 with an exact-byte in-unit
> target, 3 shipped. Plus one phantom-class repoint
> (`?Active@MessageTimer@@` → `?StateName@MetaMusicLoader@@`, owner found via the
> `??_R4` COL at `0x820F9368`).
> ★★ **A phantom row does not necessarily earn false credit — check its score
> first.** Deleting the MessageTimer row would have been worth **0** (it sat at
> 80.0%, never credited); all the value was in the *repoint*.
> ★ **Read the current score, not just the sizes:** `?SetType@UIPicture@@` is
> size-impossible yet already reads 100.0 via objdiff byte-fallback, so
> repointing it is −1. And `?Copy@VocalTrackDir@@` had a byte-perfect,
> right-sized, class-consistent candidate that was **method-wrong**.
> ⚠ `collision_ablation.py`'s clean 0.00% plant rate is **optimistic** — `agree`
> counts shared boilerplate, so same-family siblings defeat it (`--scope-unique`,
> as laneBU4 applied to the live channel, is the fix if anyone revives this).

> **Wave BW (2026-07-30) — +42 honest, +0.062229pp code, `masked_equal` FLAT at
> 1510.** Two lanes landed by patch, each A/B'd in its own fresh worktree off
> `7748e885` with both legs same-split; the combined row is a **measurement** of
> main HEAD's tree, not a sum of the two claims:
>
> | | matched | masked_equal | honest | code% |
> |---|---|---|---|---|
> | base `7748e885` | 41116 | 1510 | 39606 | 34.685703 |
> | +BW-1 `50178cff` reloc-adjudicated anon rows | 41158 | 1510 | 39648 | 34.740448 |
> | +BW-3 `0c91bbb2` Env_NG store + splits hardening | 41158 | 1510 | 39648 | **34.747932** |
>
> (BW-3 measured standalone off the same base as +0.007487pp with all three
> function axes flat. Composing the two lanes predicted 34.747935 and the
> measured combined figure is 34.747932 — a 3e-6 rounding artifact of the 6-dp
> inputs, so the lanes are **cleanly additive with no interaction**.)
>
> ★★★ **BW-1 reopens the anon-naming gate that BV-4 could not adjudicate:
> `scripts/harvest/anon_reloc_cmp.py` resolves relocations instead of masking
> them.** BV-4's selector masks every relocated word on *both* sides, so two
> bodies differing only in which vtable they store, which string they load or
> which function they call compare **equal** — which is why 25 of its 177
> proposals were dropped at landing. The new comparator reads the decompressed
> retail image `orig/45410914/band.exe` **directly rather than going through the
> `lbl_` index**, so any VA can be dereferenced, not merely the ones dtk happened
> to emit a label for; on top of the existing map/name/string/`??_R0`/fp channels
> it adds **vtable identity via the MSVC COL chain** (vtable−4 → COL → +12
> typedesc → +8 `".?AVClass@@"`), generic initialised-data content, and
> **target-vs-target** comparison of two retail bodies with every operand
> resolved to an absolute.
> ★★★ **The ship gate `agree>=4` was chosen on a truth-ablation control, not on
> the metric** — POS 100.00% over 3,688 picks, harmful-among-accepted 2.41%.
> This is the discipline the at-100%-defect class demands: a gate tuned by
> "which threshold maximises matched_functions" is metric-fitted by construction
> (cf. the W9 `MILO_MESSAGE_TIMERS` defect). **All 42 shipped rows had ≥2
> candidate names**, so a unique-name selector could not structurally have taken
> any of them — this is net-new supply, not a cheaper re-pick.
> ★★ **Three comparator defects were found and fixed while building it, each of
> which had been emitting wrong verdicts.** (1) A raw `map` CONTRA implicates
> *either* the row under test *or* the callee's map row — on the 152 landed rows
> the callee was usually at fault; replacing it with an implied-binding oracle
> observed over 19,000 established pairings dropped landed-152 rejects 12 → 3.
> (2) `reloclib` treats every `lbl_`/`jmp_` token as a relocation, but dtk also
> names **intra-function branch targets** that way and mislabels them (live asm
> prints `beq cr6, lbl_8246EED0` for bytes `41 9A 00 54`, a +0x54 branch to
> `0x82481910` inside the same function) — branch targets are now decoded from
> the **encoding** and internal ones demoted, which both removes a bogus
> one-sided "shape" contradiction and returns real control-flow words to the byte
> comparison. (3) `LK` discriminates: a `bl` into one's own COMDAT is direct
> recursion and carries a base-side relocation, so only non-linking branches
> count as internal. ⚠ `reloc_disc`'s copy is **deliberately left untouched** —
> its 99.41% gate is calibrated against the old behaviour.
> ★ Attribution was 1:1 on both axes: +42 matched for 42 rows, and the
> +0.054745pp back-computes to 5,791.7 B against 5,792 B shipped, with all 42
> reading `fuzzy==100` individually.
>
> ★★★ **BW-3 is the canonical "Δhonest 0 is not a failure" case.** Its functions
> were *already* counted as matched, so no function-count axis could move by
> construction and only `code%` could respond. Under the amended BO-8 pricing
> rule — discard only if **both** axes are ≈0 — this is a keeper, and pricing on
> Δhonest alone would have thrown away a real, byte-exact correctness fix.
> ★★★ **`Max` operand order is load-bearing, and getting it backwards REGRESSES.**
> `NgEnviron::UpdateApproxLighting` had a guarded assignment
> (`if (n <= 1) n = 1;`) where retail stores unconditionally — the branch skips
> only the `li r11,1` and falls into a shared `stw`, i.e. a Max. Since `Max(x,y)`
> is `(x < y) ? y : x`, the operand order picks the comparison sense:
> `Max(1,n)` → retail's strict `bgt` (byte-exact), `Max(n,1)` → `bge`. The wrong
> order measured **−1 matched**, not merely +0: it converts an *argument*
> mismatch, which objdiff folds, into an *opcode* mismatch, which it does not.
> ★★★ **The at-100% "wrong control flow" class is 6, not 24 — screen on the reloc
> type before funding it.** 18 of the 24 are a rendering artifact: dtk emits
> `IMAGE_REL_PPC_REL14` against the **containing function symbol** for
> intra-function conditional branches, so objdiff renders the destination from
> the relocation and drops the encoded displacement; retail's real displacement
> lands on the same row in all 18. The discriminator is perfect — all 18
> artifacts carry reloc type 7, all 6 genuine defects carry none.
> ★★ **An unresolvable `splits.txt` heading is now a hard failure**
> (`tools/project.py` + `configure.py`, `RB3_ALLOW_UNRESOLVED_SPLITS=1` to
> unblock). It used to emit `base_path: None` — a unit that can never pair and
> reads 0% forever — announced by one `print` among thousands of build lines,
> which is how the orphan `Rnd.cpp` heading hid for months. This matters because
> 715 of the 719 bare headings resolve **only** via project.py's unique-basename
> alias (BV-2), so one new colliding basename silently kills a pin. `configure.py`
> also now rejects a **duplicate** heading, which dtk does not: it unions both
> blocks into *both* headings, so each unit silently claims the other's ranges.
> ⚠ **Live instance of that hazard, unfixed:** an untracked
> `src/system/os/MasterAudio.cpp` already exists in the tree, and the bare
> `MasterAudio.cpp:` heading resolves to `system/beatmatch/MasterAudio.cpp` via
> the alias — adding the former to `objects.json` makes the alias ambiguous.
> Before this landing that would have been a silent 0%; now it hard-fails naming
> both owners.
> ★★ **Fleet-tooling changes need the generated output byte-compared, and the
> guards exercised.** `build.ninja` / `objdiff.json` / `compile_commands.json`
> were verified byte-identical (sha1 `c4eb9845` / `729b8e2f` / `b744e2e8`)
> pristine-vs-patched with rc=0 both ways, toggling **only** the two tooling
> files so the split state was held fixed — an earlier snapshot taken before the
> leg-A build would have been a false positive if the re-split had moved
> `config.json`. rc=0 under the patched tooling is itself the proof the tree has
> zero unresolved and zero duplicate headings today. All three failure paths were
> then fired live (orphan → rc=1, escape hatch → rc=0, duplicate → rc=1), because
> a guard that always passes is indistinguishable from dead code.
>
> ✅ **DISCHARGED by lane BX-1 `344ebc69` — repointed. Do not re-hunt.** (It was a
> 1-for-1 swap worth +136 B of code%, not +1 matched; the old row already counted.)
> Original entry: **a proven map mispair.**
> `?Store@Target@HamCamShot@@` in unit `BandCamShot` should repoint to
> `?Store@Target@BandCamShot@@QAAXPAV2@@Z`. Our tree has **both**
> `BandCamShot.cpp` (RB3 — `DeleteTargetCache` inside the guard, which is what
> retail does) and `HamCamShot.cpp` (DC3 — `sCache.erase` outside the guard,
> which is our compiled base); the map named retail's function with the **DC3
> class name**, so retail was paired against our DC3 sibling. **The RB3 source is
> already correct — do not "fix" the source.** BW-3 deliberately stayed off
> `scripts/target_symbol_map.json` because BW-1 and BW-2 were contending for it.
> ⚠ A map edit is a silent no-op without a re-split.
>
> ➡ **Carried forward, unowned: BW-1 turned its new instrument on the 152 rows
> BV-4 had ALREADY LANDED, and the audit is the more valuable half of the lane.**
> Tally over the 152 (`~/tmp/laneBW1_v152f.json`): **130 ACCEPT / 20 UNDECIDED /
> 1 NO_UNIT / 1 REJECT**. None of this was corrected — each needs its own lane,
> because a map DELETE or repoint is a **silent no-op without a re-split**.
> - **1 positively-rejected landed row — a candidate false-credit already on
>   main:** `?GetMaxSlots@GemManager@@QBAHXZ` at `0x82b99058` (agree 0, contra 1,
>   nrel 1) — its one resolvable relocation binds to
>   `?GetMaxSlots@TrackConfig@@QBAHXZ`. Thin but *positive* evidence: a
>   contradiction, not an absence.
> - **20 UNDECIDED are withheld, NOT condemned.** All 20 have `nrel==0` — no
>   resolvable relocation at all, so they are **unverifiable by this instrument
>   rather than disproven**. Do not price them as defects.
>   ✅ **CLOSED by BX-1: all 20 decided, ZERO defects, ZERO still unverifiable** —
>   function-start body-uniqueness over all 10.3 MB of `.text`, sound *because*
>   `nrel==0` on both sides. The withhold call was correct. Do not re-hunt.
> - ⚠ **The REJECT is a FALSE reject — see BX-1.** `anon_reloc_cmp.py` mis-handles
>   a `SHAPE` contra whose *missing* side is the target (absence of a dtk label ≠
>   absence of a branch); `?GetMaxSlots@GemManager@@`'s callee is an ICF-fold
>   representative named `RndAnimatable::GetRate`. Safe direction, but the
>   comparator's REJECT count is not trustworthy until fixed.
> - **`??1ObjRefOwner@@UAA@XZ`: NEITHER claimant is right.** ✅ **SETTLED by BX-1:
>   correct — the incumbent row was DELETED, not repointed** (`ObjRefOwner` occurs
>   zero times in `band.exe`; `0x8279e578` is MercurySwitchFilter's dtor).
>   The proposed VA
>   `0x8231f620` was rejected (contra 2 on the bind channel, `contested`, never
>   shipped, and absent from main); main's incumbent is `0x8279e578`, and
>   target-vs-target scores the two bodies **DIFF at 2 words** — so they cannot
>   both be this destructor. Per BW-1's COL resolution the proposed VA stores
>   *UIListCustomTemplate*'s vtable and the incumbent stores
>   *MercurySwitchFilter*'s.
> - ⚠ **A 22nd row was never evaluated at all:**
>   `?NewObject@ObjectDir@@SAPAVObject@Hmx@@XZ` at `0x82752f90` came back
>   `NO_UNIT` (the audit could not locate its unit), so it is neither confirmed
>   nor rejected — an audit that reports 130/152 ACCEPT is silently 130/151 on
>   coverage.

> **Lane BW-2 `ec9c8667` (2026-07-30) — +12 honest, +0.062458pp code,
> `masked_equal` FLAT at 1510.** A/B'd in a fresh worktree off `35f79c67` (the
> post-wave-BW HEAD), both legs same-split, `total_functions` 69367 both:
>
> | | matched | masked_equal | honest | code% |
> |---|---|---|---|---|
> | base `35f79c67` | 41158 | 1510 | 39648 | 34.747932 |
> | +BW-2 `ec9c8667` | 41170 | 1510 | 39660 | **34.810390** |
>
> Two halves: **302 map rows** naming anon target bodies by CALL-reloc coverage
> (type 0x6 / REL24 only — data relocs are a vocabulary artifact, target says
> `lbl_*` where we say `??_C@`), plus **`Rnd::SetPostProcOverride` reduced to the
> rb3-Wii one-liner and moved above `BEGIN_HANDLERS`** (MSVC `/Ob2` only inlines a
> definition it has already seen). The setter carries the entire code% delta:
> `?Handle@Rnd@@` is 6,416 B and went norm **and** fuzzy 100 = +0.0606pp from one
> function. Zero VA and zero name collisions against BW-1's 42 rows — the lanes
> are orthogonal despite sharing the payload file.
>
> ★★★ **Naming pays Δhonest but +0.000000pp code% BY CONSTRUCTION.**
> `report.rs:841` credits `matched_functions` on `match_percent_normalized`
> (`code.rs:285` = `diff_score − arg_diff_score`, i.e. regalloc normalized away),
> while `matched_code` needs RAW fuzzy == 100. Only same-size bodies whose entire
> residual is register allocation can pay honest. A naming lane that reports
> Δcode% ≈ 0 is not underperforming.
>
> ★★★ **Absolute match% is the WRONG axis for pricing an attribution; the margin
> over the best alternative is the right one.** The shipped pool ran 9 rows at
> 100, 84 at 95–99.9, 175 at 80–95, 42 at 50–80, and the obvious move — drop the
> low band — is refuted by control. Three extra split+report legs re-pointed rows
> at their **runner-up** name and re-scored, asking whether objdiff's alignment
> (instrument 2) picks the same partner as call-reloc coverage (instrument 1):
> **151 rows null-tested across all four bands, 0 INVERSIONS**; null scores mean
> 17.4; weak band median ship **75.4 vs null 3.2**. A 50–80 score means *correct
> name, divergent DC3-era body* (ctors, dtors, Load/Save), not a mispair.
>
> ★★★ **The headline metric is BLIND to attribution correctness.** The null leg
> that deliberately mis-named **64** rows measured matched 41,128 / honest 39,618
> — *identical to the correct leg*. A wrong map row costs nothing today and
> poisons later lanes, so it can only be policed by an instrument other than the
> score. This is the same shape as the at-100% defect class.
>
> ★★ **The real risk shape is the TWIN SIBLING, and it is invisible to the score
> band.** Held 8 rows whose measured gap fell under a **natural break in the gap
> distribution** (… 24.7, 26.2, then 30.6 — a 4.4-point empty band, so the
> threshold is data-derived, not chosen): `PropSync<HideDelay>` vs
> `PropSync<ProxyCall>` (gap **0.0**), `??_DRemoteBandUser` vs `??_DLocalBandUser`
> (5.3), `SetConfiguration` vs `ReapplyConfiguration` (11.1), `OpenGateData::Save`
> vs `::Load` (18.2), `DeleteCharacter` vs `RenameCharacter` (19.6),
> `AddRemoteMachine` vs `RemoveRemoteMachine` (21.2), `RndCam::Save` (24.7),
> `CharDriver::FindClip` vs `MyFindClip` (26.2). Holding them cost **zero** on all
> four axes (302-row leg == 310-row leg exactly). ➡ Unowned: those 8 need a twin
> discriminator.
>
> ⛔ **`unified_id_rb3wii.json` and `global_fuzzy_pairs.json` at the repo root are
> TU0-era and effectively DEAD — only 23 of the oracle's 9,301 `fn_` addresses
> resolve against the current report (0.25%).** `setup_worktree.sh` still copies
> both into every worktree as "analysis inputs", and objdiff-cli's
> `--global-byte-eq` **hard-requires** the oracle as its Rule-3 gate — so that
> pass would today gate on a file that resolves a quarter of one percent of
> addresses. Regenerate against TU5 before trusting either.
>
> ★ **A unit whose class names look wrong may be a scatter-include, not a
> mispair.** `?ClientConnect@NetStream@@` attributed inside unit `JoypadMsgs`
> looked like a cross-class error; `src/system/os/JoypadMsgs.cpp:25-40` in fact
> `#include`s both `os/NetStream.cpp` and `synth/Sfx.cpp`, so both it and its
> runner-up `?ToggleHud@Synth@@` belong there. Check for scatter-includes before
> calling a unit/class mismatch a defect.
>
> ★ **17 rows were untestable because their runner-up is a ubiquitous engine
> helper** (`Hmx::Object::SetType` was the runner-up in five different units,
> `BinStream::operator<<(int)` in another) — those names are suppressed in the
> target obj. That is itself reassuring: their "best alternative" is noise, not a
> twin. Of the 159 rows with no live alternative, 26 had a single candidate and
> 116 have their runner-up already bound to a different VA.

> **Lane BX-1 `344ebc69` (2026-07-30) — −3 honest, +0.000340pp code,
> `masked_equal` FLAT at 1510. A CORRECTNESS landing; the negative IS the
> result.** A/B'd in a fresh worktree off `edbd5bbd`, both legs same-split
> (restore `symbols.txt`, `rm report.cache report.json`, `touch config.yml`, full
> build), `total_functions` 69367 both ⇒ zero split churn:
>
> | | matched | masked_equal | honest | code% |
> |---|---|---|---|---|
> | base `edbd5bbd` | 41170 | 1510 | 39660 | 34.810390 |
> | +BX-1 `344ebc69` | **41167** | 1510 | **39657** | **34.810730** |
>
> Exact function-set diff **4 lost / 1 gained**, and the three moving units'
> `matched_code` deltas sum exactly to the global +36 B (DrumPlayer −84,
> MoggClipMap −16, BandCamShot +136). Nothing else in the binary moved.
>
> ★★★ **Absence in `band.exe` is map-independent proof, and it retired a whole
> phantom family.** The substring `ObjRefOwner` occurs **ZERO** times in
> `orig/45410914/band.exe` — not as RTTI, not as a mangled name, not as any
> string — while the positive controls each appear exactly once
> (`.?AVMercurySwitchFilter@@`, `.?AVUIListCustomTemplate@@`,
> `.?AVThreadCallback@@`, and most pointedly `.?AVObjRef@@`: the **base** class's
> RTTI is present, so the absence is specific, not a failed search). Under `/GR` a
> virtual dtor stores its own class's vftable and every vftable's COL bears a
> typedesc string ⇒ no typedesc ⇒ no vftable ⇒ no such dtor exists. Deleted:
> `??1ObjRefOwner@@UAA@XZ` @ `0x8279e578`, `??_GObjRefOwner@@UAAPAXI@Z` @
> `0x8279e588`, `??0ObjRefOwner@@QAA@XZ` @ `0x8251c190`. Each was then positively
> re-identified by walking the COL chain: the first two are **MercurySwitchFilter**
> (the `??_G` is slot 0 of vtable `0x821113B4`), the third is
> **ThreadCallback** (vtable `0x820899AC`). All three trace to `62098fc5`
> (laneAK's reloc-masked byte-class bijection) — the selector that manufactures
> exactly this error, since "store a vtable; blr" is byte-identical to every other
> such body once the relocated word is masked. **This closes BW-1's
> `??1ObjRefOwner` "NEITHER claimant is right" open item: neither was, and the
> incumbent is now gone rather than repointed.**
>
> ★★★ **A mislabelled row can score as a matched FUNCTION while earning ZERO
> matched CODE — use that as a mispair detector.** The repointed row
> (`?Store@Target@HamCamShot@@` → `?Store@Target@BandCamShot@@` @ `0x822b41a0`,
> discharging BW's carried-forward item) read `fuzzy_match_percent` **99.85294**
> but `match_percent_normalized` **100.0**. So it counted toward
> `matched_functions` (normalized ⇒ relocation-masked) yet contributed nothing to
> `matched_code` (which needs raw fuzzy 100): unit BandCamShot went 240 → 240
> functions but **+136 B**. That divergence between the two axes on a single row
> is a cheap, scannable smell for the at-100% defect class. ★ Correction to
> earlier reporting: because the old row already counted, this is a **1-for-1
> swap, not a +1** — the whole payoff is in code%, not function count.
> `BandCamShot.cpp` scatter-includes `hamobj/HamCamShot.cpp`, so our obj defines
> **both** Store symbols and objdiff name-paired retail against the DC3 body while
> the correct RB3 body sat unpaired in the same obj. **The RB3 source was already
> correct and no source file was touched.**
>
> ★★★ **This landing could not have been found, or justified, by the metric.**
> BW-2's null leg mis-named **64** rows deliberately and measured *identical* to
> the correct leg — match% is blind to whether a name is right. So a removal like
> this is only ever defensible on a **non-metric instrument** (binary absence + an
> independently re-walked COL chain). The corollary bit the fleet before: **a map
> DELETE or repoint is a silent no-op without a re-split**, because the renamer
> bakes the name into the target obj. Proven live here after the leg-B re-split —
> `??1ObjRefOwner@@UAA@XZ` and `??_GObjRefOwner@@UAAPAXI@Z` go **1 → 0** in
> `obj/DrumPlayer.obj`, `??0ObjRefOwner@@QAA@XZ` **1 → 0** in
> `obj/MoggClipMap.obj`, and `obj/BandCamShot.obj` goes HamCamShot **1 → 0** /
> BandCamShot **0 → 1**.
>
> ★★ **All 20 of BW-1's UNDECIDED rows were decided: ZERO are defects, ZERO
> remain unverifiable.** The instrument is body-uniqueness at *function-start*
> granularity across all 10.3 MB of `.text`, sound precisely **because** `nrel==0`
> on **both** sides, so nothing is masked. Scope limit, stated honestly: it does
> **not** discriminate once relocations exist. BW-1's withheld-not-condemned call
> was right, and the pool is now closed rather than carried.
>
> ⚠ **BW-1's comparator has a direction-dependent defect —
> `scripts/harvest/anon_reloc_cmp.py` should be fixed.** A `SHAPE` contra whose
> *missing* side is the **target** is unreliable: absence of a dtk label is not
> absence of a branch. That produced a **false REJECT** on
> `?GetMaxSlots@GemManager@@` @ `0x82b99058` (retail is
> `lwz r3,4(r3); b 0x822E4460`, where the callee is an **ICF-fold representative**
> whose map name is `RndAnimatable::GetRate`). The direction is safe — a false
> REJECT is a missed accept, never false credit — but that comparator's REJECT
> count is not trustworthy.
>
> ➡ **Deferred, worth +2 CORRECT matches:** a `splits.txt` boundary move would
> recover MercurySwitchFilter's real dtor pair — `0x8279e578`/`0x8279e588` sit at
> DrumPlayer's tail and `MercurySwitchFilter.cpp` starts 8 B later. Skipped on
> purpose: it changes the split, and the ~2-fn churn floor would contaminate a ±2
> measurement. Needs its own lane designed around that.
>
> ➡ **Unowned, and a genuine source-model divergence:** our tree emits a fully
> polymorphic `ObjRefOwner` (vftable + RTTI + `IsDirPtr`) that **retail does not
> contain in any form** — an `Object.h` divergence with ~281-TU blast radius via
> the PCH. Flagged, untouched. Concretely, a **fourth** row survives on main and
> is suspect under the very same absence argument:
> `"0x8232aec0": "?IsDirPtr@ObjRefOwner@@UAA_NXZ"` (unit BandWardrobe, 8 B,
> currently fuzzy-100). It was deliberately left in place so this landing
> reproduces its verified −3 exactly; an 8-byte body is far too generic for byte
> identity to mean anything, so it needs the COL/absence treatment of its own.

> **Lane BX-2 `43c9771e` (2026-07-30) — +1 honest, +0.009907pp code,
> `masked_equal` FLAT at 1510. The metric payload is one line; the VALUE is the
> negative census that SIZES AND CLOSES the vein.** A/B'd in a fresh worktree off
> `edbd5bbd`, both legs same-split (restore `symbols.txt`,
> `rm report.cache report.json`, `touch config.yml`, full build),
> `total_functions` 69367 both ⇒ zero split churn:
>
> | | matched | masked_equal | honest | code% | matched_code |
> |---|---|---|---|---|---|
> | base `edbd5bbd` | 41170 | 1510 | 39660 | 34.810390 | 3682952 |
> | +BX-2 `43c9771e` | **41171** | 1510 | **39661** | **34.820297** | **3684000** |
>
> ★ The delta is **exactly 1048 B = 1048/10580036**, the full size of
> `?Load@RndPartLauncher@@` with **no offsetting loss anywhere in the binary**. A
> hidden regression would have shown up as a shortfall against that exact figure —
> quote byte-exactness like this when you want evidence a change is *understood*
> rather than lucky.
>
> ⛔ **VEIN SIZED AND CLOSED — do NOT re-hunt "retail inlines what we call".**
> BW-2's `Rnd::Handle` (+0.0606pp from ONE function) was **near-unique, not the
> first of a class**. Whole-binary caller-side `bl` census, 952 units / 21,926
> joined functions: **delta==0 for 21,041 (96.0%)**, delta>0 for only **475
> (2.17%, 244,476 B)**, delta<0 for 410. Per the site-count≠defect-count rule those
> 244 KB are **blast radius, not yield**: 308 of the 475 (65%) sit **below 90%
> fuzzy**, where a `bl`-count difference is a symptom of general divergence rather
> than an isolated inline defect. The band where inlining is plausibly the ONLY
> defect (97–100%) is **6 functions / 5,108 B = +0.0483pp ABSOLUTE CEILING** —
> less than the single `Rnd::Handle` fix already banked.
>
> ★★★ **Retail's ObjPtr inline policy is PER-SITE, not per-TU — so
> `RB3_OBJPTR_INLINE_OWNER_CTOR` is at the WRONG GRANULARITY.** `UITrigger.cpp`
> carries the byte-identical 8-mismatch signature and the define *does* fix
> `?Load@UITrigger@@` (96.32 → 100.0), but it **simultaneously breaks
> `??0UITrigger@@` (100.0 → 86.37)**, whose `mCallbackObject(this)` member-init
> retail calls **out-of-line**. So within ONE TU retail inlines the owner-only
> ObjPtr ctor at a local-variable site and calls it at a member-init site. The
> trade is metric-neutral (+164 B, 0 net functions) but converts a *correct* ctor
> into an incorrect one, so it was **dropped on correctness-over-metric**. This
> also explains the documented global **−121**. A site-level mechanism is the real
> fix; the per-TU switch cannot express what retail actually did.
>
> ✅ **Landing-time control for exactly that hazard:** `??0RndPartLauncher@@` diffs
> **byte-identically with and without the define** (74.9% normalized, same 85
> instructions, same 29 mismatches) ⇒ no UITrigger-style collateral here. That site
> uses the **two-arg `ObjPtr(owner, obj)`** overload, which the owner-only define
> does not govern — and retail inlines that one too (`stw r11,0xc(r30)` where we
> emit `bl ??0?$ObjPtr@VRndParticleSys@@...`), i.e. a **second, uncovered overload**
> for any future site-level mechanism.
>
> ⚠ **UNSETTLED, flagged against its own landing:** `RB3_OBJPTR_INLINE_OWNER_CTOR`
> is a per-TU build switch of exactly the shape of the **metric-fitted
> build-config defect class** (W9's `MILO_MESSAGE_TIMERS` — all 6 per-TU
> restorations wrong). The two addresses its `obj/Object.h` comment cites as
> retail's out-of-line ctor/Load (`fn_8270B9A8` / `fn_8270BAD0`) have retail `bl`
> **in-degree 0**, so that citation does **not** check out against the TU5 image.
> CharEyes (94/99) and Part (77/81) are mostly delta==0 so nothing is obviously
> wrong, but **the toggle-off A/B that would settle it was NOT run.** This landing
> stands on *independent* evidence — a direct read of retail's instruction stream
> at `PartLauncher.cpp:127`, three plain stores and no call — not on match
> improvement, so the opt-in is safe even though the define itself is unvalidated.
>
> ★ **Two new instruments, both control-validated before use.**
> `scripts/harvest/inline_census.py` (caller-side `bl` census: ours from our COFF
> body vs retail decoded from `band.exe` `[A, A+S)`). **Caller-side is
> load-bearing** — the tempting detector "callee in map with retail in-degree 0" is
> **CIRCULAR**, because a function retail inlined everywhere has no out-of-line
> body and is therefore absent from the map entirely (confirmed: zero map entries
> for `Rnd::SetPostProcOverride`, while sibling `GetPostProcOverride` has
> in-degree 1). Δbl is a **pure instruction count**, hence immune to the map
> covering only 27.5k of retail's ~57k functions. Positive control run **both
> ways** against BW-2's Rnd fix.
> `scripts/harvest/string_absence_scan.py` reads literals from **compiled objs**
> (`??_C@` COMDATs), never regex-over-source (a comment has poisoned such a scanner
> here before), and searches whole-file so the 0xB200 `.text` skew cannot apply.
> ★ **Control design matters:** ten hand-guessed positive controls failed twice
> ("Rock Band" — retail spells it "RockBand"), which says nothing about the
> scanner; replaced with a ground-truth control (strings referenced by fuzzy-100
> functions MUST be present) ⇒ 11/4268 = 0.26% false-absence, and those 11 are the
> known at-100% reloc-masked defect class, not scanner error.
>
> ★ **On the retail side only COUNTS are trustworthy, never NAMES.** Name-based
> residue reported `__savegprlr_24` as "we call it, retail never does" across 59
> callers — absurd. Two causes: map coverage (retail's `bl` target is unnamed, so
> our named call looks unmatched) and **ICF folding** (`?EasePolyIn@@` has
> in-degree **2,384** and absorbs every `bl` to its fold address). The lane killed
> its own DataNode hypothesis with the paired post-filter count:
> `?Int@DataNode@@` 784 callers / 687 (87.6%) at delta==0, `?Sym@DataNode@@` 748 /
> 659 (88.1%) ⇒ retail DOES call them; 7.8% vs a 2.17% base rate is noise.
>
> ➡ **Left deliberately:** `RndGenerator::Load` (frame Δ+0x10, `gRevs_Gen`
> divergence — different class); `SetRegularShaderConst` (permuter-class regswaps,
> cannot reach raw 100); the 66 other ObjPtr ctor sites (all 60–85%, many other
> defects). ~~**Adjacent vein, unworked:** 944 functions reference retail-absent
> DC3-era handler names (`get_trans_children`, `run_flow`, `key_intensity`).~~
> ✅ **WORKED AND DRAINED by lane BZ-2 `07795e26` (+14).** The 944 was a *reference
> count*, not a yield estimate — it funnels to 21 actionable and 80 arms removed,
> and all three names above turned out to be **three unrelated phenomena**. See
> the BZ-2 block below.

> **Lane BY-1 `7b28bbc0` (2026-07-30) — +1 honest, +0.004009pp code,
> `masked_equal` FLAT at 1510. It reopens as a PER-SITE lever the exact vein BX-2
> above sized and closed as a per-TU one.** A/B'd in a fresh worktree off
> `e402f16c`, both legs **full** rebuilds (1097 edges / 1094 objs each),
> `config.yml` untouched on *both* legs so the split is frozen rather than merely
> symmetric ⇒ `total_functions` 69367 both, zero churn:
>
> | | matched | masked_equal | honest | code% | matched_code |
> |---|---|---|---|---|---|
> | base `e402f16c` | 41168 | 1510 | 39658 | 34.820637 | 3684036 |
> | +BY-1 `7b28bbc0` | **41169** | 1510 | **39659** | **34.824646** | **3684460** |
>
> ★★★ **Retail's ObjPtr owner-ctor inline policy is PER-SITE, not per-TU — proven
> inside a SINGLE function.** Retail `??0RndParticleSys@@` builds four owner-only
> `ObjPtr`s and splits them **3 inlined / 1 called**: `+0x1c8 mMeshEmitter`,
> `+0x268 mMotionParent`, `+0x274 mBounce` inline to three stores, while
> `+0x1d4 mMat` is `bl fn_8229D9C8`. Offsets are
> `/d1reportSingleClassLayout` ground truth and the callee corroborates the member
> independently (it stores the `ObjPtr<RndMat>` vtable, and 0x1d4 *is* `mMat`).
> **No per-TU switch can express one function with four sites and two expansions.**
>
> ★★ **The lever needs NO new machinery — it is the existing two-arg overload.**
> With `RB3_OBJPTR_INLINE_OWNER_CTOR` on, `mFoo(this)` binds the inline one-arg
> ctor while **`mFoo(this, nullptr)` binds `ObjPtr(Hmx::Object*, T*)`**, whose body
> is out-of-class in `obj/ObjPtr_p.h` and too big for `/Ob2` ⇒ a real `bl`. A TU
> picks its **majority** policy with the define; each minority site opts back out
> by spelling the two-arg overload. That is what puts `?Load@UITrigger@@` **and**
> `??0UITrigger@@` at 100% *simultaneously* — the case BX-2 had to abandon, where
> the per-TU switch fixed `Load` 96.3→100 while breaking the ctor 100→86.37.
>
> ★ Byte-exactness held: **+424 B = exactly `?Load@UITrigger@@` (106 insns)**, and
> a per-unit diff over all **3,917** units shows **exactly one** moves
> (`default/UITrigger`, 49→50 fns) with the other 3,916 measure-identical — zero
> collateral from the `Object.h` PCH cascade (its edits are comment-only). A
> per-function capture rules out a same-size swap: leg A has `Load` at 96.320755%
> with the ctor **already** 100.0; leg B has both at 100.0. Leg A was reproduced
> twice with byte-identical axes.
>
> ★★★ **`RB3_OBJPTR_INLINE_OWNER_CTOR` PASSES the metric-fitted-build-config audit
> (cf. the W9 `MILO_MESSAGE_TIMERS` defect class) — the define is sound, its
> GRANULARITY was wrong.** Toggle-off A/B of every opt-in: ON 41168/39658/34.820637
> vs OFF 41167/39657/34.810730, i.e. its *entire* fleet-wide contribution is
> **+1 fn / +1048 B = `?Load@RndPartLauncher@@`**, one unit. ★★ **But
> "0 metric delta ⇒ inert" is FALSE** — CharEyes and Part are metric-zero while
> their objs change by **6,164 B / 2,662 B**, and metric-zero-but-codegen-changed
> is exactly how the W9 defect hid. So each was judged against retail's instruction
> stream, not the metric: CharEyes **correct** (retail `??0CharEyes@@` has 8 `bl`,
> none an ObjPtr ctor, while the ctor builds 8 `ObjPtr(this)` members), Part
> **correct for 3 of 4** sites.
>
> ⛔ **Citation corrected, and the correction is a TU5 lesson.** The header comment
> cited `fn_8270B9A8` / `fn_8270BAD0` as the out-of-line ctor/Load; those are
> **TU0-era** addresses (written 2026-05-30, before the 2026-07-15 TU5 flip) and in
> TU5 **neither is a function entry** — `0x8270B9A8` is +0x48 inside
> `??1FaderTask@@`, `0x8270BAD0` is +0x90 inside
> `?Replace@?$ObjPtrList@VNoteVoiceInst@@VObjectDir@@@@`. **The prose was right;
> only the addresses were stale.** Also: `ObjPtr` is a **template**, so each `T`
> has its own out-of-line ctor (6 distinct in the map) — a single address pair
> could never have named the family.
>
> ➡ **Do NOT fund a sweep of this.** Naive sizing gives 1,464 near-misses /
> +2.48pp across 204 units, but that is **blast radius**; tightened to shapes that
> can actually carry the defect it is **32 ctor/`Load` functions / 11,632 B /
> +0.1099pp absolute ceiling**, and spot-checks came back **0/2** —
> `??0UIListWidget@@` (98.9%) is arg-setup and `?Load@CharIKScale@@` (96.5%) is the
> *`ObjPtr::Load` argument signature*, neither is ctor-inline policy. Only 2 of 32
> were checked.
> ➡ **Fresh unworked lever spun out of that:** the `ObjPtr::Load` argument-signature
> shape (`li r6,0 / li r5,1`).
> ➡ **Hypothesis, NOT verified:** a THIRD uncovered overload — `ObjOwnerPtr`.
> `mMotionParent` (0x268) is `ObjOwnerPtr<RndTransformable>`, which retail
> **inlines**, but our ctor is declared out-of-line (`ObjPtr_p.h:302`) and the
> define does not govern it. Inferred from layout + retail stream only.

> **Lane BZ-2 `07795e26` (2026-07-30) — +14 honest, +0.090019pp code,
> `masked_equal` FLAT at 1510. The largest `code%` lane of the wave, and it
> DRAINS the 944-reference handler vein flagged above.** DC3 (our engine oracle)
> is NEWER than RB3 retail, so our `BEGIN_HANDLERS` chains carry arms retail never
> had. laneBF (`05ef434a`, +44) proved the lever via stack-frame drift, but frame
> drift only sees surplus plain `HANDLE()` arms (each costs one 8-byte `DataNode`
> temp); a surplus `HANDLE_EXPR`/`HANDLE_ACTION` costs **no frame slot** yet still
> emits a Symbol compare+branch, so it breaks the body invisibly to that scan.
> `scripts/harvest/handler_surplus_census.py` drops the frame-drift precondition
> and censuses all 545 `BEGIN_HANDLERS` classes against two INDEPENDENT
> instruments, acting only on their **conjunction**: PAIRED (handler-name strings
> the retail body itself references, read from the dtk-split listing —
> map-dependent) and ABSENCE (does the name occur anywhere in `band.exe` —
> map-independent, meaningful only for names ≥8 chars, so short names are reported
> `tooshort` and never used as evidence).
>
> Landing A/B in a fresh worktree off `66697375`, **split FROZEN** (`config.yml`
> touched on *neither* leg — cleaner than symmetrising), `report.cache` +
> `report.json` removed before each leg:
>
> | | matched | masked_equal | honest | code% | matched_code |
> |---|---|---|---|---|---|
> | base `66697375` | 41169 | 1510 | 39659 | 34.824646 | 3684460 |
> | +BZ-2 `07795e26` | **41183** | 1510 | **39673** | **34.914665** | 3693984 |
>
> ★★★ **The honest funnel is the lane's main durable output — and it is a
> 45× attrition.** 944 references → 340 non-thunk `?Handle@X@@` in pinned units →
> 336 paired → 249 retail bodies **readable** → 30 surplus → **21 actionable**
> (extra arm AND proven absent) → 19 below 100% → **80 arms removed across 24
> files**. Arm-level: 3,253 `HANDLE*` arms, **406 proven absent across 87
> classes** — but after both waves only **2** classes still had a pinned sub-100
> `Handle`, so **the vein is now DRAINED**, not merely sampled.
>
> ★★★ **The three headline examples decomposed into THREE UNRELATED PHENOMENA** —
> a warning against treating a name list as one class. `run_flow` was a genuine
> extra arm (→ Synth 100.00); `get_trans_children` is absent but retail has
> **`get_children`**, i.e. a **rename** on a body already at 100% via masked reloc
> ⇒ **metric-inert by construction**; and `key_intensity` **is not a handler at
> all**, just an entry in a string array at `BandDirector.cpp:1107`.
>
> ★★ **Two instrument traps, both caught by controls that FAILED FIRST.**
> (a) **vtable adjustor thunks** (`?Handle@Cls@@$4PPPPPPPM@A@AA…`) match the name
> pattern but are a 4-instruction `b real_fn` jump with **zero strings**, so
> *every* arm read as surplus — the control failed **123/347** until thunks were
> excluded, then **1/192 = 0.52%**. (b) **`set_all_to_3D` has an uppercase `D`**
> and the retail-side reader filtered `[a-z0-9_]*`, so uppercase names can never be
> read from retail and **always look surplus**; this regressed `CamShot`
> 93.65→87.05 and was reverted (regex now hardened; only 2 such names exist
> tree-wide). Note the absence instrument had correctly said "present" — **the
> conjunction gate would have blocked it**, and it was reached only by deliberately
> relaxing to `--allow-present`. ⇒ *A control that fails is the instrument working.*
>
> ★★ **Landing audit: "0 regressions" was wrong at the per-function level, though
> the net was right.** The per-function set diff (not just totals) is
> **+16 gained / −2 lost = net +14**. The two losses are 32-byte EH guard-clear
> funclets in `PlatformMgr` (`fn_825162B0`, `fn_825162D0`, 100 → 92.5), each
> differing in **exactly one instruction: the static-init guard BIT INDEX** on
> `?$S5@…Handle@PlatformMgr@@` (target clears bit 3 / bit 2 via `rlwinm`; ours
> clears bit 0 via `clrrwi`). Removing 22 arms **renumbered the function-scope
> static guards**, reshuffling the byte-fallback funclet pairing. Byte-fallback
> funclet stratum ⇒ no source lever, and it doubles as a residual signal that
> `PlatformMgr::Handle` is not yet fully right (it stops at 94.6).
> ⇒ **Always diff the per-function SET; a −2 hides perfectly under a +16.**
>
> **Ten `Handle` bodies driven to exactly 100.00** (all ten confirmed in leg B;
> **four re-verified independently via objdiff with ALL instructions equal** —
> NetCacheMgr 49 instrs, BandUserMgr 296, Synth 580, RndDrawable 207): Synth
> 87.87, RndDrawable 69.88, CharClip 87.62, RndTex 90.43, RndAnimatable 87.55,
> TaskMgr 93.28, VirtualKeyboard 77.14, SongPreview 51.84, NetCacheMgr,
> BandUserMgr 73.83. Large partials: PlatformMgr 53.2→94.6, UIManager 62.5→94.5,
> RndMat 4.3→38.3, SongMgr 57.8→88.4, RndGroup 18.5→49.8, RndLine 77.3→92.7,
> StorePreviewMgr 77.6→90.3.
>
> ⚠ **Metric-units trap for the next reader: the lane's per-body figures are
> `fuzzy_match_percent`, NOT `match_percent_normalized`.** The two differ by
> ~0.5pp and `matched_functions` counts *normalized* == 100 — reconciling the
> lane's numbers against the wrong field makes every claim look ~0.5pp off. One
> figure did not reproduce at all: NetCacheMgr's quoted 11.47 origin reads
> normalized **2.7551** with `fuzzy` **null** in leg A (its destination 100.00 is
> confirmed). Also `?Handle@Synth@@` is compared inside unit
> `default/CharMeshHide` — a 13-span grab-bag unit that absorbed the range;
> pre-existing, identical in both legs, and counted **exactly once** (checked, not
> assumed).
>
> ➡ **Failed predictions worth keeping.** `WorldDir` moved **+0.000** despite six
> jointly-proven arms removed — at 21.5% its gap is elsewhere. And **every** ≥93%
> candidate (ProfileMgr 99.8, Campaign 99.8, QuestFilterPanel 99.2,
> AccomplishmentPanel 98.5, RockCentral) moved **+0.000**: a body that close cannot
> have 3 genuine extra arms, so those were extractor artifacts — harmless but inert.
> ➡ **Left deliberately (do NOT re-hunt as a metric lever):** the **RENAME class**
> (`get_trans_children`→`get_children`, plus `BaseMaterial`, `Watcher`, `PanelDir`,
> `BandSongMetadata`) — real DC3-vs-retail defects, but the string is a masked
> reloc on already-100% bodies ⇒ **Δ0 by construction**; they belong to the
> **at-100% correctness class**, not the metric. Also left: `LabelNumberTicker`
> (the sole control false positive), `CharDriver::default_clip` (**suffix-pooled
> string** — `set_default_clip` contains it, an extractor blind spot), all
> `tooshort` names, and `WorldDir`'s **MISSING** arms (retail has arms *we* lack —
> that needs implementing bodies, not deleting lines).
> ⚠ **zsh trap re-confirmed:** `grep --include=*.cpp` **unquoted** returned
> "0 BEGIN_HANDLERS blocks"; caught only because the glob error printed beside the
> zero. **Quote your globs and print denominators beside zeros.**

> **Lane BY-2 `273066ce` (2026-07-30) — +2 honest, +0.001019pp code,
> `masked_equal` FLAT at 1510.** A/B'd in a fresh worktree off `66697375`; both
> legs same-split (`symbols.txt` restored, `report.cache`+`report.json` removed,
> `config.yml` touched on **both** legs because this lane *changes* splits) ⇒
> `total_functions` 69367 on both, and leg A reproduced main's published headline
> to the digit.
>
> | | matched | masked_equal | honest | code% | matched_code |
> |---|---|---|---|---|---|
> | base `66697375` | 41169 | 1510 | 39659 | 34.824646 | 3684460 |
> | +BY-2 `273066ce` | **41171** | 1510 | **39661** | **34.825665** | **3684568** |
>
> The +2 is a compound: **+3** from a splits boundary move plus map rows, **−1**
> from deleting a false-credit row.
>
> ★★★ **A splits boundary move ALONE pays +0 — the MAP ROWS are what convert it
> into matches.** Three functions at DrumPlayer's `.text` tail belong to
> `MercurySwitchFilter.cpp` (`fn_8279E578` 16 B `??1MercurySwitchFilter@@`,
> `fn_8279E588` 68 B `??_GMercurySwitchFilter@@`, `fn_8279E5D0` 32 B
> `?Reset@AnySignMercurySwitchFilter@@`, all identified map-independently from
> `band.exe`). Moving the boundary measured **+0** as its own leg: the target
> symbols are anonymous `fn_*` and objdiff's byte fallback did not pair them. The
> lane measured that rather than assuming it — **any future splits-only lane
> should budget +0 unless it ships map rows in the same landing.** Per-unit byte
> attribution, reproduced exactly on the landing: DrumPlayer 9/12 848/964 →
> **9/9 848/848 (now a 100% unit)**; MercurySwitchFilter 8/12 948/1264 → 11/15
> 1064/1380, **+116 = 16+68+32**.
>
> ★★★ **dtk OVER-SPLIT one function into three, and a `return NULL` tail was
> being scored as a matched virtual method.** `"0x8232aec0":
> "?IsDirPtr@ObjRefOwner@@UAA_NXZ"` was **live false credit** — the renamer
> applied it and the report scored the fragment fuzzy 100.0. BX-1 left it as
> undecidable because an 8-byte `li r3,0; blr` body is too generic for byte
> identity (41 candidate homes); it is now **disproven, not merely unproven**:
> 0 vtable slots, 0 `bl` call sites across 10.3 MB of `.text`, and 0 occurrences
> of the address anywhere in the 14 MB `band.exe` (all sections, unaligned) while
> positive controls fire. The real function starts at `0x8232AE70`; `0x8232AEC0`
> is literally its `return NULL` tail. BandWardrobe 238/270 22076 → 237/270 22068
> (**−8** = the fragment).
>
> ★★ **A map DELETE is a silent no-op without a re-split** (the renamer bakes the
> name into the target obj), so it was proven post-split two ways: the class-2
> *definition* in the BandWardrobe target obj at val `0x220` reads
> `fn_8232AEC0` instead of the mangled name, and a sweep of **all 3,917 live
> target objs** finds **zero** remaining occurrences. ⚠ A naive `strings` grep
> still shows one hit in `obj/auto_03_8232A52C_text.obj` — that file is a **dead
> leftover** (no `splits.txt` heading, absent from `report.json`, mtime from the
> previous day), a reminder that the obj dir outlives the split set.
>
> **Build-neutral third item:** `scripts/harvest/anon_reloc_cmp.py`'s `SHAPE`
> defect is fixed — a one-sided relocation offset was scored a contra regardless
> of *which* side was missing, but a missing TARGET side is not evidence (target
> relocs are recovered from dtk's printed asm, and absence of a dtk label is not
> absence of a branch). On 19,048 established pairings: 25 affected (0.13%), **all
> 25 REJECT → ACCEPT**; whole-population REJECTs **83 → 58 (−30%)**. POS precision
> **100.00% → 100.00%** (3,733 correct, 0 wrong); NEG harmful 221 → 222, the one
> new plant being the documented BandLabel/HamLabel sibling twin, which correctly
> refuses as *ambiguous* when truth is present. `pool` output **byte-identical**
> (`xbranch` deliberately kept out of `relocs` so candidate selection and every
> calibrated number stay valid). ➡ **YIELD ZERO, measured not assumed** — the
> 628-row sweep is SHIP=0 before *and* after; this fixed REJECT **accounting**
> only. A new `selftest` subcommand pins both failure modes.
>
> ➡ **Carried forward, unworked:** `tt_compare` has the **same defect family**
> and was deliberately NOT fixed — it byte-compares `masked`, and unlabelled
> branch words aren't masked, so ICF twins at different VAs read DIFF; it feeds
> the CONTESTED clause that refuses all 25 of fixture A, and **loosening it is the
> false-credit direction, needing its own control run**. `shape_t` (68
> occurrences) is now the largest residual false-reject cause.
> `reloc_disc/reloclib.py` untouched on purpose (its 99.41% gate is calibrated on
> the old label-trusting behaviour). Still unlocated: where
> `ObjRefOwner::IsDirPtr` actually lives in retail (the "41 homes" problem), and
> the `Object.h` `ObjRefOwner` source divergence (~281-TU PCH blast radius).

> **Wave BT — branch harvest of 248 unmerged branches (2026-07-30).**
> `08047ec1` lane BT-3: **+11 honest, +0.0177pp code, masked_equal flat.**
> ★ **The vein is real but the obvious selection axis is wrong.** BT-2 swept the
> 37 branches with ≥5 changed files — billed as highest-signal — found **0**, and
> recommended defunding the rest. BT-3 then covered all **164** low-file-count
> branches and found the +11. The yield was **not in source**: it was map
> fragments, which correlate with neither branch size nor recency.
> ★ BT-3's method is the reusable part: **invert the index** — compute the
> (branch, file) candidate set once and group **by file**, collapsing 164 branch
> questions into ~130 content questions. Full coverage in 6 builds.
> ⛔ **`git diff main..<branch>` is worthless for this job** — it reports
> everything main gained since the branch point as a "difference". Use the
> branch's own patch (`merge-base..branch`), then per-file blob comparison, and
> even then expect supersession to be the common case.
> ⛔ **Drained by BT-1:** the 2,262-line StorePackedMetadata port can never score
> (Wii-only subsystem, absent from retail — see `src/system/meta/StorePackedMetadata.h`).

> ★★ **QUOTE BOTH AXES — the function count alone is now actively misleading.**
> A one-worktree, one-split A/B of the second session's `3384ec22` over
> `e4052850`:
>
> | | matched | masked_equal | honest | code% |
> |---|---|---|---|---|
> | `e4052850` | 40922 | 1510 | 39412 | 34.44971 |
> | `3384ec22` | 40925 | 1517 | **39408** | **34.47950** |
>
> Honest **−4**, code **+0.0298pp (~3.5 KB)**. I initially read the
> masked_equal +7 as false credit and was **wrong**: this is real body
> improvement that does not cross the 100% threshold, plus more functions
> falling into byte-fallback pairing. For comparison, wave BS's BS-4 leg was
> −2 honest for +0.0039pp — so `3384ec22` is **7.6× the code gain for 2× the
> honest loss**, a *better* code-per-honest-function trade.
>
> ⇒ As the tree approaches the ceiling on easily-crossable functions, an
> increasing share of genuine progress lands as sub-100 body improvement that
> the at-100% count cannot see, and can even score negative. **Price landings on
> honest AND code, and say which one moved.**

> ⚠ **TWO SESSIONS ARE LANDING TO MAIN CONCURRENTLY (2026-07-30).** This
> headline is +12 above the wave-BS chain below because a second session landed
> `9af24b12` (+12, eight one-set source fixes) and `b2d54f2f` (+0, three
> retail-correctness gates) *between* the BS-1 and BS-2 landings. Their commits
> touch only `src/`; wave BS touched `splits.txt`, the symbol map, tools and
> docs — **no overlap, verified, nothing clobbered.** But it means a lane's A/B
> base can go stale mid-wave: always re-measure main directly rather than adding
> lane deltas to a quoted headline. (Wave BS staged by wholesale-copying
> `splits.txt`/`target_symbol_map.json` from a worktree based on the older
> `51e61cf7`; that was safe *only* because the other session touched neither
> file. Prefer a 3-way merge over a wholesale copy while main is shared.)

> **Wave BS (2026-07-30), verified chain — +11 honest, +12 matched, +0.023pp code.**
> Every leg measured by the coordinator in a landing worktree, same split within
> each A/B, `symbols.txt` restored and `report.cache` removed before each read:
>
> | step | matched | masked_equal | honest | code% |
> |---|---|---|---|---|
> | base (`51e61cf7`) | 40898 | 1509 | 39389 | 34.38926 |
> | +BS-3 `f45e94a2` classname literals | 40898 | 1509 | 39389 | 34.38926 |
> | +BS-4 `9599caef` START_AUTO_TIMER | 40897 | 1510 | 39387 | 34.39312 |
> | +BS-1 `4db258e8` vbase layout | 40905 | 1510 | 39395 | 34.40937 |
> | +BS-2 `ebfde7b9` StaticClassName carves | 40910 | 1510 | 39400 | 34.41247 |
>
> Two legs are ≈0 **by construction, not by failure**, and were landed for
> correctness with no win claimed: BS-3's `OBJ_CLASSNAME` literals reach `.text`
> only through a relocation objdiff masks, and BS-4's timer gate traded −2
> at-100% (inside the ±2 split-churn floor) for **+0.00386pp code** — real body
> gains (`WorldDir::Poll` 64.20 → **100.00**) netted down by EH-funclet
> re-pairing churn. On funclet-churning changes, quote `matched_code`.
>
> Note the prior headline 40,896 vs this chain's base 40,898: that gap is the
> documented ~2-function split-churn floor, not drift. Lane BS-3 nearly banked a
> false +2 by differencing against the quoted number instead of measuring its
> own baseline leg — **always measure your own base in your own worktree.**

> ⚠ **OPEN INTEGRITY QUESTION (2026-07-29) — read before quoting this number.**
> laneBE closed a +560 different-unit gap-absorption channel after finding the
> flips were 32-byte static-init **guard-clear cleanups**: our symbol clears
> `RndAnimatable`'s guard, the retail function clears an **unrelated** one. Same
> instruction shape, different object — and `functionRelocDiffs=none` **masks the
> differing relocation**, so it scores 100.0%. Same class as the known
> `StaticClassName`/`Type()` family (453 members, one 22-instruction body,
> distinguishable only by the string operand).
> **RESOLVED 2026-07-29 by laneBH** (`docs/plans/reloc-correspondence-audit-2026-07-29.md`,
> tool `scripts/harvest/reloc_correspondence.py`). Whole-binary census of all
> functions at 100.0, reported as a band between the permissive and conservative
> reading of the weakest oracle:
>
> | | permissive | conservative |
> |---|--:|--:|
> | evidenced (corresponding + no-relocs) | **65.5%** | **43.8%** |
> | **DIVERGENT** | **5.2%** | **2.7%** |
> | UNDECIDABLE | 27.7% | 51.8% |
> | unpaired / shape | 1.6% | 1.6% |
>
> ⇒ **The count is substantially sound.** The headline means: ~17,000–26,000
> functions we can *prove* we reproduced, ~1,060–2,040 we can prove we did not,
> and the rest the binaries **cannot adjudicate** — that residue is `.bss`
> statics and externs with no bytes in either image, i.e. **unobservable, not
> suspect**. Divergence is NOT concentrated in funclets (4.6% vs 5.8% for named
> bodies); it concentrates in **≤16 B adjustor/forwarder thunks (10.2%, 2.2×
> tree)** and the `??__E`/`??__F` static-lifecycle family. Game tier (4.6%) is
> cleaner than engine (5.5%).
>
> **laneTIGHTGAP's +109: STANDS, not reverted.** It measures 5.7×–7.3× the tree
> divergence rate (verdict invariant to strictness) and its 105 pairable credits
> rest on only 64 distinct base symbols — laneBE's guard-clear mechanism
> reproduced symbolically. But reverting would delete 31 evidenced + 42
> undecidable to remove 32 unevidenced (0.08% of the count), and **the defect is
> in the scoring rule (many-to-one masked pairing), not the splits geometry** —
> reverting removes credit without correcting anything. Reclassified in the
> ledger as ≈31 evidenced / ≈42 undecidable / ≈32 unevidenced.
>
> ★**STANDING GATE: price every future gap-absorption channel with
> `reloc_correspondence.py` before landing.** A channel materially above the
> tree's 5.2% divergence rate is buying metric, not program. laneBE's unlanded
> +560 is 100% this class and prices worse; its CLOSE recommendation stands.
> Keep **interior** (same-unit-both-sides) sweeps; close **different-unit
> absorption**.
>
> ★**By-product worth more than the audit: `docs/plans/laneBH_realbugs.json`** —
> **109 named bodies ≥128 B at 100% carry content-proven WRONG constants/strings**,
> invisible to every near-miss scanner because scanners only look *below* 100%.

> **VERIFIED 2026-07-29 at main `5e9996fc` (lane docfix).** Independently
> reproduced, not copied from a lane report: fresh `scripts/setup_worktree.sh`
> worktree at that commit, `rm -f build/45410914/report.cache`, full
> `./tools/ninja-locked` (1,045 edges, exit 0), then read
> `build/45410914/report.json`. **`measures.matched_functions = 39382`**, and an
> independent recount of `match_percent_normalized == 100.0` over every function
> in every unit gives **39,382** as well. Other measures at that build:
> `total_functions 69378 · total_units 3967 · matched_code 3431832 ·
> total_code 10579936 · matched_functions_percent 56.764393 ·
> fuzzy_match_percent 39.03488 · complete_units 1`.
> ⚠ **Do not recount with `fuzzy_match_percent`** — it reads **222 LOW** here
> (39,160), consistent with the error bar recorded further down this doc. Only
> `match_percent_normalized` sums to `matched_functions`.

## ★★★ QUOTE THE HONEST FLOOR, NOT `matched_functions` (2026-07-29)

> **SUPERSEDED IN SCOPE 2026-07-29 by lane BO-8**
> (`lane-bo8-icf-funclet-audit-2026-07-29.md`). Everything below is correct **for
> the supply axis alone** and still measures 3.7% (re-verified by a fresh pass-2b
> compile-out A/B at 40,540: reported 1,517, real 1,466). Two amendments:
> 1. **There is a second, disjoint over-count axis** — relocation-target divergence,
>    measured against the retail bytes. **Full honest band: 37,490 – 38,098, i.e.
>    the headline over-states by 6.0% – 7.5%**, not ~4%.
> 2. **"No real decompilation is affected" is false as a general claim.** It is true
>    of pass-2b surplus (which never touches named symbols). But identity divergence
>    is *worse* among named bodies (5.5% / 4.3%) than among supply-backed funclets
>    (2.4% / 0.56%). The clean statement is: *pass-2b surplus never touches named
>    functions.*
>
> Also settled there: **populating the ICF alias map cannot change any measure** —
> `report generate` hardcodes `functionRelocDiffs=None`, under which `reloc_eq`
> never consults `symbol_equivalences`. Measured: 3 groups → 1,408 groups, Δ = 0.

**`matched_functions` over-counts by ~4%.** The cause is objdiff's own funclet
pass (`pair_funclets_by_bytes`): it pairs each leftover funclet-like target onto
a base partner **without marking that partner used**, so **N targets can all
score 100% against 1 base function**. It is a property of the heuristic, not of
any lane — an independent replay attributed the inflation across **137 distinct
commits** spanning the project's whole history, ~70% of it predating the recent
sweeps.

**Every report now discloses this itself.** Our objdiff fork (branch
`oversub-disclosure`, installed 2026-07-29) populates
`measures.masked_equal_functions`:

```
HONEST FLOOR = matched_functions − masked_equal_functions
```

At install: 39,743 − 1,582 = **38,161 floor**; true honest **38,210** (the floor
sits 0.13% low because 49 of the flagged symbols re-pair onto a genuinely unused
partner when the pass is removed — they were *mis-attributed* credit, not
*unsupported* credit). `configure.py` now **hard-fails** if the fork cannot be
resolved, because the downloaded release omits the field and would silently
restore the inflated headline.

Three things that are settled and should not be re-litigated:
- **All fake credit is on anonymous `fn_` symbols.** Named-function matches are
  structurally unreachable by this pass. **No real decompilation is affected.**
- **Do NOT revert any landing over this.** The blocks that are entirely
  over-subscribed also hold genuine named matches; deleting them measures
  **raw −228 / honest −46**, i.e. the honest metric goes *down*. The fix is
  pricing, not deletion.
- **The naive screen does not work.** "unit `matched_functions` > its base obj's
  function-symbol count" trips **0 of 3,881** units (base objs carry thousands of
  inline/template COMDATs). The correct rule is per-signature:
  `excess = Σ_S max(0, demand_target(S) − supply_base(S))` —
  `scripts/harvest/oversub_guard.py`, wired as a gate into
  `diffunit_gap_apply.py`.

⚠ **The inflation grows with new landings** — re-run the census or read the field;
never quote a past figure.

## ⚠ Corrections to landed commit messages (2026-07-29, lane docfix)

Commits are immutable; these are the corrections. Each was re-derived from the
repository, not from another lane's write-up.

- **`01a0e9fa`'s "24 TUs get their FIRST pinned range ever" is WRONG for 23 of the
  24.** The commit added 24 *path-qualified* unit headers to `splits.txt`
  (`system/world/Crowd.cpp:`, …), but 23 of those basenames **already had pins under
  a bare-basename spelling**. Measured by counting `.text start:` lines per bare
  basename in `git show 01a0e9fa^:config/45410914/splits.txt`: Crowd 14, LightPreset
  17, LightHue 2, Instance 6, CameraShot 59, CharBoneOffset 8, CharIKRod 12,
  CharLipSync 40, CharLipSyncDriver 22, FileMerger 16, Anim 22, AmbientOcclusion 16,
  EventTrigger 46, MeshDeform 16, CrowdAudio 14, EndingBonus 25, LayerDir 10,
  Faders 37, Sequence 44, HeldButtonPanel 6, BandMachineMgr 45, MainHubPanel 24,
  MusicLibrary 12 — **only `system/world/Reflection.cpp` was genuinely new (0)**, and
  it is the only one of the 24 whose qualified spelling still exists on main today
  (the rest were later consolidated back onto the bare spelling). The commit's
  *score* claim (−2) and its map/split mechanism finding are unaffected. **No doc
  repeated this claim** — a tree-wide grep found the phrase only in the commit
  message — so nothing else needed editing; it is recorded here because a lane
  reading `git log` would otherwise inherit it.
  ⇒ ★**A new `splits.txt` unit HEADER is not a newly-pinned TU.** `splits.txt` keys
  on the spelling, not the source file, so the same `.cpp` can hold pins under two
  keys. Diff by **basename**, not by header line, before claiming first-ever coverage.
- **Several recent commit messages quote strict counts that do not reproduce on
  main** (one claimed 37,599 where a clean full rebuild at that commit measured
  37,282). The pattern is lanes quoting a **worktree** measurement as if it were
  main's. The headline above is the one number in this doc that has been rebuilt
  and re-read at HEAD; every other count in this file is dated and belongs to its
  own section. ⇒ **Quote a count only with the commit it was measured at and how
  (`report.cache` cleared? full rebuild? which tree?).**

## ★★ laneAY 2026-07-27 — the census-honesty lane (+28), and the FOURTH census bug

**Landed +28** across four measured legs (all with the full re-split recipe and
both ninja legs; every leg A/B'd unit-agnostically, by name AND by unit+name):
map/UIManager **+1**, laneAY-C **+18**, laneAY-A **+7**, laneAY-B **+2**.

### ★★★ THE LESSON: a census tool's UNIVERSE is as load-bearing as its resolver

`scripts/harvest/localstatic_census_wide.py` had a **sound resolver** (18/18
exact string resolution against reference commit `7d5c413e`) and a **broken
universe**: it enumerated by globbing `build/45410914/obj/**/*.obj`, a directory
that is **never cleaned**. Measured at 39,266:

| class | TUs | excess statics |
|---|---:|---:|
| live compiled units (target + base + report) | 506 | 3,423 |
| target-only pins (no compiled source — nothing to edit) | 127 | 571 |
| **orphans: 8,891 `auto_*` carves + 112 STALE objs** | 957 | **9,911** |

**71% of the reported population was in objs no live unit owns.** The 112 stale
objs are orphans of dead `splits.txt` generations, and because the tool ranked
by `max(variants)` over same-basename objs it **actively preferred them**: its
#1 "actionable" row was `band3/game/VocalPlayer` (46 statics) — a dead carve,
mis-attributed to `Poll`, whose live counterpart `default/VocalPlayer ?Handle@`
is **46/46 = zero excess**, i.e. already done.

★ **THE FIX — enumerate from `objdiff.json`, never from the filesystem.** It is
generated by `configure.py` from the live `objects.json` + `splits.txt` and
carries the authoritative triple per unit: `name` (byte-identical to
`report.json`'s unit name — no `split('/', 1)[-1]` guessing), `target_path`,
`base_path`. Only the **894** units with a `base_path` are editable at all.
`NO_REPORT_PAIRING` went **65 → 0**. Tool: `localstatic_census_v2.py`.

★ **This was the FOURTH bug in this tool family** (after the COFF
`SymbolTableIndex`-as-list-position bug and the `tu + '.cpp'` hardcode). The
brief said "assume a fourth exists until you have checked" — it did. **Assume a
fifth.**

### ★★ SPOT-CHECKS ARE NOT OPTIONAL — the corrected census STILL over-fired

The corrected join gave **84** named-editable excess statics. A converting
worker then found the discriminator at the bench: `localstatic_patch_gen.py`
sets `form=LOCAL_STATIC` on **`guard_va` alone**, and its target-side guard test
is "some `.data` VA both loaded and stored in the window" — which any ordinary
**file-scope static** satisfies (`sResources`, `sCharClipTypes`, …). The
base-side scanner uses the exact `??_B`/`$S` symbol-name test, so the two
disagree on byte-identical code and the row reads as pure excess.

★ **A real function-local static resolves BOTH a guard BIT (sequential across
the group) and a `static_va` (marching down by 4).** Requiring both is now the
`WEAK_GUARD` filter. It drops 207 rows / 623 statics and removes **exactly** the
rows hand audits condemned — `CharBoneDir::Init`, `UIEventMgr::Init`,
`Part InitParticleSystem`, `GemManager` ctor, `BeatMaster::CheckBeat` (all sat
at 100%/99.99%, i.e. nothing to fix), plus `BlockMgr`'s `disc_spin_up` (adding
it measured **82.8 → 67.5**), `BandWardrobe`'s `female`, and both of
`Rnd::DrawTimers`' "statics". **Every row a worker actually converted survives
the filter.**

★ **FINAL HONEST POPULATION: 63 named-editable excess statics / 31 functions /
26 units** — down from 13,932 reported and from the "90 actionable" headline,
41 of whose 90 were stale orphans. A well-measured small number.

### Residue verdicts (two confirmed, one refuted)

- **`?SetType@Object@Hmx@@` @ `0x82804588` — CONFIRMED.** Its body calls
  `??0Object@Hmx@@` + `??0Timer@@` and stores `vftable_82123B0C`, whose `??_R4`
  COL type descriptor reads `.?AVUIManager@@`. It is `??0UIManager@@QAA@XZ`.
  Repointed: 53.61% → 98.23%, then **100%** by dropping `mCurrentScreen(0)`
  (retail's ctor is 228 bytes to our 232, the extra insn being
  `stw r29, 0x2c(r30)`). ⚠ **The generalised scanner
  (`vftable_name_contradiction_scan.py`) finds exactly ONE other candidate
  tree-wide and it is a false positive — the vein is a SINGLETON.** Do not
  re-fund it; find these opportunistically instead (laneAY-A found a second by
  hand: `fn_82351E70` is `BandTrack::SetNetTalking`, not
  `HamDirector::PickIntroShot`).
- **`ContentMgr` / `UILabelDir::SyncProperty` — CONFIRMED, stated reason WRONG.**
  `UILabelDir.cpp` *does* have a pin. The defect is only that `ContentMgr.cpp`'s
  final `.text` line `0x828101A0–0x828109EC` is **exactly 0x84C bytes** =
  precisely `UILabelDir::SyncProperty`. Moving it took the row 0.00 → 75.69 with
  `ContentMgr` holding at 23 matched; the rest came from `UILabelDir.{h,cpp}`
  being a **verbatim DC3 copy that dropped 6 members RB3 keeps**. Target `Save`'s
  asm then yielded retail `rev = 9` (not DC3's 11) and member offsets
  `this+0xF8..0x164` that **independently confirm rb3-Wii's declaration order is
  RB3-360's layout**. Both functions → 100%.
- **`TrackerDisplay` map off-by-one — REFUTED.** 50 of 52 named target symbols
  have byte-exact size agreement with our compiled obj; an off-by-one would
  shift sizes wholesale. The two that differ are body divergences, and the 0.00%
  anonymous rows are a map-COVERAGE gap, not a mis-pairing.

### Other transferable findings

- ★ **The WRONG-UNIT splits channel was NOT drained.** Memory said 665 → 7;
  a re-scan found 30, of which **16 moves landed for +17 with zero real losses**
  (WRONG-UNIT 30 → 11, proposals 24 → 8). "Already drained" was too pessimistic.
- ⚠ **`splits_move.py scan` emits UNSAFE proposals.** The remaining 8 all have
  `n_carved_in_span == 0` and a span START strictly inside an already-carved
  function (+0x4 to +0x400), so applying any splits inside a symbol and
  hard-fails dtk with "ends within symbol" — leaving no `report.json`. They pass
  `apply --dry`'s "audit clean" because the audit checks overlap/inversion/empty
  blocks but **not symbol-boundary alignment**. These are
  `target_symbol_map.json` entries bound to non-function-start addresses
  (ICF/inline artifacts), not splits defects. **Worth adding a boundary-alignment
  refusal to the tool.**
- ★ **The lever generalises past statics into a HANDLER-LIST CENSUS.** Diffing
  target-vs-base guarded-`Symbol` sequences in a `BEGIN_HANDLERS` block localises
  *missing/extra handlers* to the instruction. It produced both of laneAY-B's
  100%s: `MusicLibrary` has no `fake_win` / `FriendsListChangedMsg` /
  `UserLoginMsg` and three extra store handlers; `MetaPerformer` has
  `has_online_scoring` **twice** (bits 14 and 17), both wrongly `#ifdef HX_NATIVE`.
  Same method proved `BandDirector::Handle` has **36** arms where an in-source
  comment claimed 34 (wrong count AND wrong address).
- ★ **A repeated rb3-Wii DEV-only pattern, worth a scanner:** retail lacks the
  guard-`if` + `MILO_WARN` wrappers the Wii dev build added, and several dev-only
  bodies are simply **empty** in retail. Five `Instarank` label updaters,
  `MetaPerformer::UploadDebugStats()`, `Set/ClearCreditsPending`, `IsWinning()`.
  Same family: `BandWardrobe::LoadMainCharacters`' whole `LOADMGR_EDITMODE`
  prefab path is dev-only, and its `char buf[256]` was the *entire* 0x260-vs-
  0x140 frame delta; `BandCharacter::Poll` has **no** `START_AUTO_TIMER` in
  retail; `BlockMgr::Init` does **no** `MemAlloc`.
- ★ **`#pragma auto_inline(off)` is a working MSVC-X360 inline-policy lever and a
  FORCE MULTIPLIER.** Applying it to `MetaPerformer::IsBandNoFailSet` (retail
  calls it out-of-line; `/Ob2` inlined ours) collapsed a **105-instruction
  r25↔r26 callee-saved regswap cascade in one step**. ⇒ Register cascades in big
  `Handle` functions can be downstream of a single inline-policy mismatch — try
  this **before** declaring a function "permuter-class".
- ⚠ **`report.json`'s `match_percent_normalized` ignores register-ARGUMENT
  differences, unlike `run_objdiff`'s headline.** `NextSongPanel::FinishLoad`
  reads 99.5 in objdiff but is a strict **100** in the report. Don't keep tuning
  regswaps thinking you're short of the gate — and don't read a report-100 as
  register-exact.

### What remains here
The named channel is nearly spent (63 statics, top row `ContentMgr` 17). The
real residual is **982 ANONYMOUS rows / 3,349 statics in compiled units** —
`fn_<VA>` target symbols inside pinned spans we compile, whose local statics
prove they are Handle/SyncProperty-shaped bodies. That is an **identification**
problem (map coverage), not a source-edit one. Most concentrated:
`VocalTrackDir` 150, `BandSongMetadata` 95, `CustomizePanel` 75, `BandCharDesc`
67, `PostProc` 61, `UIStats` 61. Also open: two map mispairs found but not
repaired (`0x822a68e0` is an `OutfitConfig::MeshAO` SyncProperty, not
`WorldDir::Save` — the identical 0x168 size is coincidence; and
`TriggerCalibration`'s target takes three pointer args where ours takes
`(this,int)`), and `UILabelDir`'s other 10 functions are unidentified.

## ★★ 2026-07-26 LATE ARC — 36,069 → 38,305 (lanes AN…AV), and what it taught

Ten lanes (AN…AV), each an Opus lead fanning out to its own Opus/Sonnet workers
in isolated worktrees. **+2,236 verified strict, every landing A/B'd
unit-agnostically against a pickled baseline.** Largest single landing:
laneAT **+640**.

### The channel that reopened
★**"Byte identity is drained" was REFUTED — every prior drain was measured
GLOBALLY.** Per-unit, the ICF-prone shapes that defeat global uniqueness are
still unique *inside their own unit*. Mechanism: **`is_funclet_like()` gates BOTH
sides of `pair_funclets_by_bytes`** (objdiff `diff/mod.rs:1423,1438`), so an
anonymous target can never byte-pair with a mangled base name — objdiff
structurally cannot reach these; only a map entry can. laneAQ +276, laneAS +321,
laneAT +520 off this one correction.
★**The size "window" (17–68 B) is a property of the ANONYMOUS POPULATION, not of
the functions:** named >84 B functions are **91.9% strict**, anonymous ones 0.2%;
356 functions at exactly 0.0% → **305 flipped once named**. >84 B is
**supply-limited, not gate-limited**.
★**New channel — existing map entries can be PROVABLY WRONG** (laneAT): 155
named targets below 100% with a free exact in-unit byte twin under a different
name; repairing 120 = **+99 at 82.5%**. Mutually recursive with the twin scan
(a repair frees a name the scan then claims) ⇒ **run alternately to fixpoint.**

### Honest error bars discovered this arc — quote these, not intuitions
- ★**113 target symbols are reloc-masked byte-EQUAL to their mapped base and
  STILL score <100%.** objdiff's normalized diff is **stricter** than masked byte
  equality. That is the error bar on every byte-twin claim.
- ★**Only `match_percent_normalized` sums to `matched_functions`;
  `fuzzy_match_percent` reads 222 LOW.** A ceiling computed off the fuzzy field
  had to be retracted.
- ★**Reloc-masked byte equality is near-vacuous below ~32 B** — the 12-byte
  adjustor body is shared by **1,673 distinct symbols**.
- ★**Fuzzy % is NOT identity evidence** — 249 provably-wrong names measured mean
  **53.7%**, max 89.95%; MSVC PPC prologue boilerplate alone reads ~55%.
- ★**The 99.8/99.9% band is NOT a near-miss queue** — 1,643 forty-byte funclets
  there are objdiff pass-3 **fuzzy pairings of unrelated funclets**.

### Pools that shrank when the tool was un-gated (both were "source problems")
- laneAM's "~4,300 unreachable, we compile no matching body" → its predictor was
  **funclet-shape-gated on the base side**, so ordinary bodies had an empty
  candidate set *by construction*. Un-gated: **1,502 of 3,290 have an exact
  reloc-masked twin we already compile** (not 8). Honest source residue
  **1,445**.
- laneAT's member-defect census was **~64% wrong** (`addi r3,r31,off` is the
  funclet's own FRAME pointer, not a `this` slot). Corrected: 1,732 of 2,659
  frame-related, **390** true member. Then an over-carve scan ate 306 of those
  ⇒ **the header-reconciliation channel is 84 functions, not 1,098**, and the
  splits-attribution channel is correspondingly larger.
⇒ ★**Before concluding "we lack the source", re-run the measurement with the
tool's own gates removed.** Twice in one day that was the whole answer.

### Mechanisms (reusable)
- ★**A funclet flips on the parent's frame SIZE alone — the parent need not
  match.** Controlled: parent held at 99.4%, all 6 funclets at 100.0%. One parent
  fix = 4× multiplier. Tool `scripts/harvest/frame_delta_scan.py`.
  ★**But re-priced by its own worker: realistic yield from the 409-row pool is
  LOW TENS, not hundreds** — 1 of 14 landed, 1 was an ICF trap that would have
  LOST matches, 4 unfixable in source, 8 entangled with body/regalloc. **Sort by
  "frame is the only diff" (≤6 mismatches, all offset-shifts), NOT by pct band;
  below ~97% the frame delta is a SYMPTOM.**
- ★**Sibling-scope overlay:** most +0x10 deltas are one local failing to
  *overlay* onto another's slot. MSVC /O1 stack-colours two disjoint-lifetime
  objects onto the same offset **only when both are block-scoped in the same
  parent scope**. Bare `{ }` around the trailing object fixed `ReplaceSubdir`
  (+4 from 3 lines). **Named objects only** — unnamed temporaries resisted every
  variant and naming them made one case *worse*.
- ★**ICF folds masquerade as sizeof defects.** `ObjVector<DynamicPropertyEntry>::
  resize` looked like a perfect +0x30 sizeof bug; "fixing" it hit 100% but broke
  3 stride-dependent STL functions in the same unit (+5/−3, reverted; 0x80 was
  correct). **Tell: objdiff's Function Call Diff shows a target-only callee
  naming a different class than the base at the same call slot.**
- ★**A large single-parent funclet cluster is as likely to be a WRONG-UNIT carve
  as a layout defect** — 20 funclets that looked like a layout bug were
  `StreakMeter::StreakMeter()` over-carved into Waypoint's span; one splits move
  fixed all 20. **Check whether the class's other members are already pinned
  elsewhere before reconciling a header.**

### Measurement contamination — FIVE live sources, control for ALL
1. ★**`setup_worktree.sh` reflinks main's `.obj` files and main's build dir is
   DIRTY** ⇒ any obj-derived scan before the worktree's own first full build
   reads another lane's uncommitted source. Measured **73 pre-build vs 32
   post-build**; **dirty objs MANUFACTURE evidence**, so it reads as a rich vein.
   ★Second axis: the **map** drifts faster than `src/` (923/549 lines in one
   interval) — check **both** diffs against *current* main, not the branch point.
2. ★**`config/45410914/symbols.txt` is both a dtk INPUT and a regenerated
   OUTPUT** ⇒ a control leg silently retains its own treatment. `git checkout --`
   it on **BOTH** legs. Never commit it.
3. **`dynamic_init` patcher unstable on a first build** ⇒ same number of builds
   per leg; a ±2 drift is documented.
4. ★**The map is NOT a ninja input to the renamer** ⇒ map-only edits need
   `rm -f build/45410914/target_symbol_renames.stamp`, or the edit silently does
   nothing **and reads as a refutation**.
5. **Stale `.s`:** `build/*/asm/*.s` for units no longer in `splits.txt` are
   never regenerated and are silently wrong.

### Tool defects found (fix or route around)
- ★**`span_predictor.py`: `matches()` hardcodes `tu + '.cpp'` ⇒ EVERY `.c` unit
  is mis-flagged WRONG-UNIT** (raw 532 vs corrected **290** — 242 false positives
  in json-c/vorbis/zlib/tomcrypt). It also **self-confirms if used naively** (it
  unions the record's own `tu` into the candidate set) — **pass a sentinel `tu`**.
- ★**A WRONG-UNIT pool is usually NOT map work:** **276 of 290 have no definer
  anywhere**; only 3 unique-definer. **Check `n_definers` first.**
- **`map_rotation_repair.py apply` corrupted the map** — `startswith('"0x')` also
  matched bare array elements of `_bijection_arbitrary`, writing a key/value pair
  *inside* a JSON array; asserts were blind (they filter `isinstance(v,str)`).
  Fixed.
- **`land.sh`** reflows all ~25k map lines (breaks the byte-splice invariant) and
  its **union-merge corrupts splits when two lanes each add `.text` pins**
  (unioned `.pdata` back-fills → hard split failure). **`READY:` is not a
  verify** — run `scripts/harvest/overlap_check.py`.

### Fleet rules adopted this arc
- ★**Never re-serialise `scripts/target_symbol_map.json`** — a `sort_keys`
  rewrite churns ~25k lines and makes the branch unmergeable against every
  concurrent map lane. Appends + one line per repair.
- ★**After composing a splits change, re-run the map tools to fixpoint** — a pin
  move strands every map entry whose VA crossed a unit boundary.
- ★**Re-check cross-lane collisions LATE.** Two correctly-measured fragments
  (+26, +8) rebased to exactly **0** because a concurrent lane had taken every VA.
- ★**Commit every worker worktree early.** Of six orphaned worktrees, the four
  that had committed contributed +32; the two that had not contributed **0**, and
  their buffers did not compile.
- ★**Check that a confirmation test COULD fail.** An inherited 48-entry set was
  refuted **48/48** because the prescribed test was a byte-diff on a class
  *constructed* by byte equality. A leave-one-out that restores the truth to the
  candidate supply likewise reports 100% **vacuously** (same tier: 95.5% wrong
  under abstention).
- ★**Draft findings only AFTER the measurement lands.** Six lanes retracted
  claims written ahead of a real `report.json` A/B — several of those retractions
  were the lane's most valuable output.

### ★★ SANDWICH OVER-CARVE: measured NET NEGATIVE — necessary but NOT sufficient

A "sandwiched" `.text` block (both immediate gap-0 neighbours are the same other
unit) plus **100% definer corroboration** (every anonymous target function in the
block reloc-masked byte-matches a symbol *defined* in the proposed owner's obj)
**still loses matches.** Measured on the 19 strongest blocks / 268 functions
(incl. `SaveLoadManager`→`ProfileMgr` 73 fns, `SessionMgr`→`MetaPerformer` 72):
**−23 net, 49 gained / 72 lost, 18% conversion.** Run twice, identical. Reverted.

★**Why: the definer test asks whether the destination obj DEFINES a byte-identical
symbol — not whether that symbol is ALREADY CLAIMED.** Moving code in gives the
base symbol a second claimant and greedy pairing displaces the incumbent. **The
losses are named symbols in the RECEIVING units, not the donors**
(`??_GBaseSkeleton`, `??1ObjVector<TransformCrowd>`, `?NewObject@BackdropPanel`,
`??0Callback@Loader`). This is **laneAP's leg-B failure returning through a
different door**: moving code between units on byte evidence is not free even
when the evidence is strong *and the destination is right*.

★**The two moves that DID pay (StreakMeter +13, Waypoint→VocalTrackDir +67)
satisfied a third condition incidentally** — both were **pure anonymous funclet
tails whose parent was already pinned to the destination**, so nothing in the
destination competed for a pairing.

★★**THE THIRD PREDICATE WAS BUILT AND DOES NOT SAVE IT — CHANNEL CLOSED.**
Predicate 3 as implemented (supply-vs-demand per reloc-masked signature:
admissible only when `supply − already_claimed ≥ incoming` for **every** signature
in the block) gave 600 sandwiched → 182 scorable → 107 definer-corroborated →
**87 capacity-safe (312 fns)**. Applied all 87: **−10 by (unit,name); by name
+73 / −101 = −28.**
⇒ **Measured NEGATIVE TWICE on DISJOINT candidate sets under TWO different
admission rules** (19 blocks/268 fns → −23; 87 blocks/312 fns → −10).
Per-signature counting is insufficient because destination incumbents are also
displaced by **transitive re-pairing**. ⛔**DO NOT FUND the 716-function pool.**

★**And the two "wins" were probably not a channel at all.** StreakMeter (+13) and
Waypoint→VocalTrackDir (+67) were each a **pure anonymous funclet tail whose
parent is already pinned to the destination, with NO parent function of its own in
the block** — and both were found by *reading* the block, not by any scanner.
Narrower than any computable predicate. ⇒ **Record as a hand-verified special
case, not a queue. Two data points that pay do not make a channel when the
population they were drawn from measures negative twice.**

Tool: `scripts/harvest/sandwich_overcarve.py`. Rows:
`/home/free/tmp/laneAT/sandwich_scored.json`, `sandwich_applied.json`.

★★**TWO OPERATIONAL TRAPS FOR ANY BULK SPLITS MOVER:**
1. **A move that empties a unit's LAST `.text` block HARD-FAILS the build** —
   `Failed to open <unit>.obj: Invalid COFF/PE section headers`. Guard for it.
2. ⛔~~**`.text` and `.pdata` must be moved and restored TOGETHER.**~~
   **FALSIFIED 2026-07-27 — see below. Move only `.text`; `.pdata` follows.**

★★**⛔RETRACTED 2026-07-27 — THE `.pdata` "DELETION LOSES RANGES" CLAIM DID NOT
REPRODUCE.** It was recorded here as fact and propagated; it is **false**.
**Verified rule (jeff `src/util/split.rs:1035-1095`, `update_splits` at
`split.rs:1146`, xex path `src/cmd/xex.rs:2476-2483`):** `split_pdata()`
**clears the ENTIRE `.pdata` split set and re-derives one range per `.text` code
split on EVERY run.** ⇒ **`.pdata` lines in `splits.txt` are DERIVED OUTPUT, not
input. Derivation is never gated on absence.** Confirmed on the deployed fleet
binary (`dtk 1.9.2`, sha `57b52d64`; the `laneAF-va-fragments` diff touches only
`va_fragments`, pdata logic identical).
Empirical: deleting **54** `.pdata` lines (MasterAudio 30 + BandCharacter 24) and
re-splitting **regenerated all 54**, sorted diff empty, 5,254 ranges stable; a
hand-introduced overlapping `.pdata` line was **silently healed** back to
baseline.
⇒ **The observed 5,172 → 4,694 can only have come from the lane's accompanying
`.text` edits (derived count is a function of `.text` splits) or from a split run
that FAILED and left the hand-edited file in place** — e.g. symbols.txt drift.
★**If `.pdata` lines don't regenerate, the SPLIT RUN failed — that is the bug to
chase, not the `.pdata`.** Never hand-edit or hand-carry `.pdata`.

★**The empty-unit trap IS real, but fires at a different stage than reported:**
draining a unit's last `.text` block, the **split SUCCEEDS** and emits a 42-byte
`obj/<unit>.obj`; the build then hard-fails at the **report.json** step with
`Failed to open ./build/45410914/obj/<unit>.obj: Invalid COFF/PE section headers`.
**Remediation (verified): delete the unit's whole `splits.txt` entry.**
`CLAUDE.md` amended accordingly at `6ab38692`.

### ★ Two inference rules for diagnosing a "wrong" unit or a "missing" class

★**Before calling any pin a PHANTOM, `grep '#include "[^"]*\.cpp"'` in the owning
`.cpp`.** Scatter-wiring is deliberate and widespread — a sweep of 49 suspect
units found **20 of 49 scatter-wired** (e.g. `HamCamTransform.cpp` carries **nine**
`#define gRev`-guarded `#include "….cpp"` lines; `Character` 4, `EventTrigger` 5,
`LightPreset` 5, `MeshAnim` 6). **All 49 resolved to a real source file**, so
foreign-class symbols in a unit are usually *intended*, not a mis-pin. A lane
retracted a whole "DC3-only files pinned onto spans that never held them" finding
to this rule.
★Corollary on scope: a tool reading the **compiled `.obj`** already sees
post-include content, so existing scatter wiring does **not** loosen a bucket it
measured. *Adding* a scatter include is a different and much larger change.

★★**ABSENCE FROM `../rb3` DOES NOT PROVE ABSENCE FROM RB3-360 RETAIL.** `../rb3`
is the **Wii dev** decomp, and Wii is the **cut-down SKU**. A class missing there
may still exist in the 360 retail binary. ⇒ Downgrade any "this class does not
exist in RB3" row from **permanently unfixable** to **"no RB3 oracle evidence —
unreachable pending a second source."** (Dance-Central-lineage names like
`HamMove::LocalizedName`, `DancerFrame`, `DetectFrame` remain very unlikely, but
that is a prior, not proof.)

### ★ Two build-monitoring traps (multi-lane box)

★**A bare `pgrep -f "ninja-locked"` COUNTS OTHER LANES' BUILDS.** With 3+ lanes
building concurrently in separate worktrees, a lane's own "progress" readings were
partly other lanes' work — it reported edge counts that were not its own. **Match
processes by `/proc/<pid>/cwd` against your worktree path**, not by a shared tool
name. (The builds are *not* deadlocked when this happens — each worktree has its
own build dir; they merely contend for CPU.)

★**Long uncached builds get truncated by harness task reaping** —
`ninja: build stopped: interrupted by user`, twice on one leg. Detach with
`setsid nohup … & disown` and monitor `build/45410914/report.json` **mtime** as
the completion signal, with a ninja-exit fallback.

### ★ `OBJ_MEM_OVERLOAD` opt-out backlog: measured **ZERO** — a phantom count

The classifier finds **156 OUTLINED-only** classes and only **10** opt-outs were
applied, which reads like ~146 left behind an already-proven +111/+5 lever. **It
is not there.** Of the 139 non-vendor OUTLINED classes (17 are
`soundtouch`/`D3DXShader`/`NUISPEECH`, out of scope):

| | n |
|---|--:|
| no in-tree header — class lives in an **unwired TU** | 62 |
| header exists, **no allocation macro** — unaffected by the change | 75 |
| already declared `MEM_OVERLOAD` — already correct | 2 |
| **declared `OBJ_MEM_OVERLOAD` — needs an opt-out** | **0** |

★**So `scripts/harvest/newobj_inline_classify.py` is a per-class ORACLE FOR FUTURE
PORTS, not a work queue** — when a TU gets wired, look its classes up and declare
each with the macro the bytes say retail used. The **62 with no in-tree header are
exactly the ones that will need it.** Ranking it as a backlog would have sent a
lane after 146 phantom candidates — the same failure mode as the sandwich pool:
**a plausible count that dissolves on contact.**

★**METHOD NOTE (two confidently-wrong answers before the right one):** matching
`OBJ_MEM_OVERLOAD\s*\(\s*(\w+)` returns **nothing** — the macro takes a **line
number**, not a class name; and parsing class bodies mislabels **138 of 139**
(these headers don't survive a naive brace scanner). **File-level presence keyed
on header basename** is what answers it. Both wrong answers were plausible (0
actionable / 138 "no declaration") — the lane only caught them by spot-checking a
header it had written itself. ⇒ **Spot-check any census against a case you know
by hand before believing it.**

### Refusals worth not re-funding
`_bijection_arbitrary` ceiling **+2** (1,205 of 1,207 already at 100) ·
`.pdata` parentage decides **2.8%** of the unreachable pool · the 84-byte
pairing cap **do-not-lift** (p2 36.3% / p2b 43.3% precision; the lift mostly
duplicates the name-side waves silently) · `DECOMP_FORCEBLOCK` is a **silent
no-op under MSVC** (`src/decomp.h:38` gates on `__MWERKS__`), ceiling ≤5 ·
"overlapping data carves" = stale TU0 artifact, **0 defects** · 187 dangling map
entries: dropping 179 measured **net 0** and **destroys identification info**
(a dangling entry becomes valid the moment its TU is wired) · **retiring
proven-wrong bindings is CHEAP** (5 evictions = +0 strict, −0.0007pp fuzzy).

## ★ TRANSFERABLE LEVERS from the 2026-07-25/26 coordinator session (27,223 → 27,816 in-lane)

These are mechanism findings, not one-off fixes. Landed + measured; reusable fleet-wide.

### 1. ⚠ SUPERSEDED — "BULK-CONVERSION LAW" (098f84a8, +177) → the predicate is THE PARENT AT 100
> **CORRECTED 2026-07-29 (lane docfix) by `be2b574c`.** The measurements below are
> reproduced verbatim and still stand. The *rule inferred from them* was wrong, and
> acting on the wrong rule is expensive: it tells a lane to keep converting a whole
> TU while the score falls, when what actually clears the churn is driving **one
> parent function to 100%**.

Original measurement (unchanged): converting ONE function to retail's
`DP_KEYS`/function-local-`static Symbol` form measured **−7** (its 3 new statics
collaterally un-paired 9 already-matching EH funclets — objdiff funclet
over-subscription re-pairing). Converting **all 21 stragglers in the TU
simultaneously = +48** (53 gained / 5 lost).

★★**The operative predicate is THE PARENT FUNCTION REACHING 100%, not "the TU is
fully converted."** Measured in `be2b574c`:
- `NextSongPanel`: moving a **single** static read **−230** mid-flight (230 funclets
  100 → 99.9). The same edit read **+1 / 0 losses** once the parent was driven to 100.
  Whole-TU conversion was never what changed; the parent hitting 100 was.
- `AccomplishmentPanel::LaunchSelectedEntry`: improved **95.6 → 97.5** and still
  measured **−16** (16 EH funclets 100 → 99.9). **Reverted.** Partial credit does NOT
  cancel the funclet re-pairing churn — so "keep going, it'll stabilise" is false for
  any function that stops short of 100.
- ⇒ In `098f84a8` the whole-TU sweep paid because it happened to take its parents
  *to 100*, not because it was whole-TU. A bulk conversion that leaves parents at 97%
  is a **net-negative** operation.

★★**DECLARATION POSITION IS LOAD-BEARING, AND IT IS USUALLY NOT THE FUNCTION TOP.**
The guard-check position in the target body names the declaration point; retail
declares these at the USE SITE.
- `PanelDir::PanelNav` **96.9 → 16.4** when its 3 statics were hoisted to the top. Reverted.
- `TrackPanelDir` **90.6 → 79.6** at the top, **→ 100** with the same statics moved
  inside `if (mScoreboard)`.
- 3 of 4 `NextSongPanel` fixes were **pure placement moves** (no new statics):
  80.7 → 100 on their own.

★**FAKE-100s exist in this vein.** `MusicLibrary::ClientSetPartyShuffleMode` read 100
while its target body held a local static ours lacked (only 12/34 instructions actually
equal). Converting a sibling exposed it at 52.9; adding the static made it a REAL 100.

**⇒ A one-at-a-time trial that reads net-negative is still NOT evidence the lever is dead
— but the retest is "drive THIS parent to 100", not "convert the rest of the TU".**
Tools: `scripts/harvest/ls_guard_timeline.py` (guard-bit order = declaration order, plus
the string literal and storage address — makes each conversion a transcription) and
`scripts/harvest/localstatic_tu_census.py` (per-unit done-vs-straggler; ⚠ counting only
`static Symbol` massively over-reports — most apparent gaps are `static Message` /
`static DataArrayPtr` already present).
Follow with `homing_scan` on the converted obj (bytes changed) — yielded 7 more
plain-UNIQUE homes, +7/−0. ⚠ **`be2b574c`'s full-tree homing sweep (1024 TUs) after the
conversions found 0 new homes** — that follow-up is swept out tree-wide; do not re-run
it per-TU.

### 2. GUARD-BIT TIMELINE = a transcript of the source's static-declaration structure
Bit ORDER gives declaration order; the GAPS between guard-check runs give grouping and placement.
On `RecordPerformance` it named the 7 declaration groups (at their USE SITES — 27/1/1/1/1/23/2, NOT a
switch), the missing key (`hopo_gems_strummed`), and a DEAD key retail declares but never inserts
(`high_gems_hit_high`). 67 → 99.27, frame 0x780→0x680, flipping ALL 79 dependent funclets.
Note: big recorders declare **DataPoint FIRST then keys** — the OPPOSITE of the small getters.

### 3. Container identity from CALL SHAPE
`std::hash_map<Symbol,int>` vs `std::map`: retail called an OUT-OF-LINE ctor/dtor and walked a
null-terminated chain (node+0 next, +4 key, +8 value); `std::map` INLINES its ctor and iterates via
`_M_increment` against `end()`. Correcting it closed a 0x4d0→0x4c0 frame gap (+42 with 41 funclets).

### 4. ⚠ IDENTICAL-BODY FAMILY = a systematic FAKE-100% generator (a380ed69)
Every `Foo::StaticClassName()` (OBJ_CLASSNAME) and `FooMsg::Type()` (DECLARE_MESSAGE) compiles to ONE
identical 22-instruction body; **453 exist in the TU5 image**, differing only in three RELOCATIONS.
objdiff normalized mode ignores relocation targets ⇒ **ANY member pairs against ANY other at a fake
100%**, self-confirming. **The STRING OPERAND is the only sound discriminator.**
Measured: **153 of 405 mapped entries were WRONG**, incl. a contiguous off-by-one shift across 25
consecutive `Char*` slots. Repair landed +10; one VA GAINED a match once UNMAPPED ⇒ **unmapped beats
wrongly-mapped**. Tool: `scripts/harvest/localstatic_symbol_audit.py --json` (re-derives in ~40 s,
flags each repair's harmfulness).
~~**OPEN DEBT: ~51 entries are correct-by-string but currently satisfy a fake 100%**~~
> ✅ **PAID 2026-07-26/27 — this debt is CLOSED.** Do not re-open it as a backlog item.
> - `01a0e9fa` (lanePHANTOM) retired **31 string-proven-wrong** map entries. Cost was
>   **−2, not −31**: repairing the map ALONE costs full price (−30), but repairing
>   **map + SPLIT together** is nearly free — 28 of the 31 were isolated 0x58-byte
>   scatter ranges that existed ONLY because of the wrong map entry, so re-pinning
>   `.text`+`.pdata` to the string-proven owner recovers them 1:1. ★**The blast radius
>   was 2 functions per phantom, not 1** — 26 of the ranges had been grown 0x58 → 0x78
>   by `.pdata`-parentage pins, and that extra 0x20 is the phantom's own `??__F` atexit
>   dtor (26/26 verified by decoding the guard word out of both bodies).
> - `560dffb3` then dropped **528 non-injective duplicate names** (user-approved),
>   measured **−105**. ★**The naive count overstates that debt by ~3x**: 322 of the 528
>   were scoring 100%, but a VA that loses its name reverts to anonymous `fn_<VA>` and a
>   large share **re-pair positionally** as unnamed funclets. Duplicate credit 158 → 1
>   (the legitimate `?NodeCmp@@YAHPBX0@Z`, a file-static qsort comparator with genuinely
>   different bodies in `DataArray.cpp` and `BandWardrobe.cpp` — statics are not COMDATs,
>   so the linker does not dedup them).
>
> **LIVE RESIDUE, re-measured 2026-07-29 on main `5e9996fc`** (`venv/bin/python
> scripts/harvest/localstatic_symbol_audit.py --json`, run in a clean worktree after a
> full `./tools/ninja-locked`): `family members in .text 453 · distinct strings 418 ·
> ambiguous strings 32` → **OK=405 MISMATCH=25 UNMAPPED=20 FOREIGN=2 NO_TOKEN=1**,
> **repairable=3** (of which harmful-to-apply: 0). The three uniquely repairable are
> `0x8227a1a8` Flow→`BandCamShot`, `0x82369ba8` ClipCollide→`CharBone`, `0x8236ac28`
> RndMesh→`CharPollGroup`; the other **22 are the AMBIGUOUS-string** rows
> (`Rnd*`/`Dx*`/`Ng*` triplets, `FxSend*` pairs) — still string-proven wrong, still
> fake-100, but **not uniquely repairable without a second oracle**.
> ⚠ At `01a0e9fa` this residue read **33 MISMATCH / repairable=0**; intervening map lanes
> moved it. The number drifts — **re-run the audit, never quote it from a doc.**

### 5. NOT all "class absent from src/" map entries are contamination
LEAPCORE / XAUDIO2 / NUISPEECH / XGRAPHICS / TrueColor / FaceCore are REAL Xbox360-SDK + Kinect
middleware statically linked into both games (0x82BE0000–0x82BE6000 is a coherent XAUDIO2/LEAPCORE
region). Their defect is a **SPLIT PIN inside XDK library territory** — `System.cpp` pinned
0x82BE28C8–0x82BE4428 (tot=33, comp=1), also `Compress.cpp` 0x82A68050, `GemTrack.cpp` 0x82B93C78.
⇒ splits lane, NOT map lane. **RESOLVED 2026-07-26 (laneXDKPIN).** Content-verified and re-carved:
`System.cpp` .text 0x82BE28C8–0x82BE4428 + its .pdata 0x8225F688–0x8225F7A8 **removed** (all 55 fns are
`CLeapSystem@LEAPCORE`; the span's ctors store 7 vtables and reference the GUID {8bcf1f58-…} @0x821A7D9C,
L"Xbox 360 audio device" @0x821A7DAC, L"Audio" @0x821A7DD8, "SimpList: non-growable list…" @0x821A68BC —
and have NO RTTI COL, i.e. built /GR-, unlike Milo). `Compress.cpp` .text 0x82A68050–0x82A68F38 + .pdata
0x822506A8–0x822506F0 **removed** (XGRAPHICS suffix-tree shader-microcode compressor; loads
"Compression : creates %d subroutines" @0x8217D21C). Our real spans are unaffected: Compress.cpp keeps
0x827CF920–0x827CFA40, whose zlib version string "1.2.1" @0x8211A4A0 confirms it. The GemTrack.cpp
0x82B93C78 item was a FALSE ALARM — 0x82B93xxx is band3/bandtrack territory, no XDK symbol within 0x100000,
and a later lane already re-carved it (TrackPanel.cpp 0x82B93C78–0x82B93CE4, GemTrack from 0x82B93CE4).
A tree-wide re-scan (map-named XDK/middleware symbols inside each pinned .text) found NO other straddler:
next worst is Synth.cpp at 5/96. Measured: +2 matched, 0 lost.

### 6. Source bug, still open
Retail's `FxSend*360` classes register under the **base** token (`FxSendReverb`, not `FxSendReverb360`);
`RndMultiMeshProxy` loads `"RndMultiMeshProxy"` where our `OBJ_CLASSNAME` says `MultiMeshProxy`.
**FIXED 2026-07-26 (laneXDKPIN).** Ground truth read out of the retail StaticClassName bodies:
`FxSendReverb360::StaticClassName` @0x82B59FD0 loads "FxSendReverb" @0x820F4EC0; likewise Wah@0x82B5A888→
"FxSendWah", MeterEffect@0x82B5A680, Synapse@0x82B5A808, Delay@0x82B5A158, PitchShift@0x82B5A788 —
all base tokens (EQ/Chorus/Distortion/Compress/Flanger already were). `RndMultiMeshProxy::StaticClassName`
@0x8240E3C0 loads "RndMultiMeshProxy" @0x8205DD00. No "*360" token exists anywhere in .rdata; the only
360-suffixed strings are .data RTTI type descriptors (`.?AVFxSendReverb360@@`). SynthSample360 fixed too.
⚠ The "disambiguates 32" prediction is **REFUTED**: `ambiguous` in localstatic_symbol_audit.py is an
IMAGE property (≥2 bodies load the same string) and is invariant to our source tokens — 32 distinct
ambiguous strings before AND after. The real, measured payoff is 69→62 MISMATCH / 358→365 OK (7 map
entries vindicated). Score impact is nil by construction (normalized diff ignores the reloc).

### 7. Process hazards that cost real time this session
- **`git apply` aborts ATOMICALLY while still printing per-file "applied cleanly".** Always verify with
  `git status` after any apply that reports an error. Hit twice.
- **`git add <file>` on shared main also stages OTHER lanes' uncommitted edits to that same file** —
  swept 122 foreign map entries into one commit. The `does not match index` error from `git apply` is
  the tell; re-apply with `--exclude=<file>`, hand-add your own lines, and read `git diff --cached`
  (line count is the giveaway) before committing.
- **`build/45410914/report.json` on disk can be weeks stale** — another lane rebuilds it. Check mtime or
  regenerate before quoting ANY number.
- **TU0 → TU5 flip (2026-07-15) invalidated every pre-flip ADDRESS.** Use Ghidra bank
  `default_tu5.xex-c5a170`; never the live default.xex. Cross-check any address against the map.


## 2026-07-20 flywheel session — 18,819 → 18,874 (+55 this session; +185 across the arc)

### Wave close-out addendum (later 2026-07-20): 18,874 → 18,924 (+50)
Final grind-tail wave +28 (LaunchGoal local-static cascade +19; struct-stride
SongPattern/LocalizedName; levers: NOTIFY_ONCE_EVAL flag, qualified-base call,
DrawMode=DC3-minus-1, if-guard vs mask-fold) + correlator r6 +1 + BinDiff r1
+5 (286 map entries, 563 carving hints) + partial-recovery/W-E landings.
**Measured-fundable near-miss pool EXHAUSTED (92/92 attempted, 9.8% tail
rate) — Phase-1 grind CLOSED; pivot to identification (Phase 2) is live:**
see docs/plans/remaining-bytes-decomposition-2026-07-20.md +
docs/plans/bindiff-transfer-spike-2026-07-20.md. Struct-stride RE campaign
in flight (ICF-fold-stride trap documented in memory).

Ran the **body-flip → correlator-harvest → reprice** flywheel to a clean milestone:
- **Grind wave 1** +11 (LEVER-STRING/SYMBOL + BODY-LEVER 70-90; missing-`virtual`
  GameMode cascade +9 discovered).
- **Lane A wave 1** +22, **Lane A wave 2** +14 (retail-absent deletions, HttpGet
  layout, HasPart virtual, dead-stub body restores). Combined Lane A +36.
- **Correlator re-scan** +8 (6 byte-identity additions + 2 invcorr repoints,
  full-rebuild A/B gained 8 / LOST 0, `ce41a0a4`). Near fixed-point.
- **Tooling landed:** `scripts/triage/reprice_router.py` (`48a8ce51`) — grind-outcome
  feedback loop, router self-sharpens each wave; `scripts/harvest/missing_virtual_scan.py`
  (`6726d4ee`) — cascade detector (vein now drained).

**Measured priors (reprice_router, N-gated):** BODY-LEVER yield is 70-90 stratum ONLY
and **thinning: 24.1% → 20.4%** as the band is skimmed; ≥90 dead (4.8%), <70 dead (0%);
certify-skip RELOC-COLOC/STRUCT-ARTIFACT/NEEDS-REVIEW confirmed. **Forecast: ~80 flips +
correlator dividend ≈ ~150 strict remaining in the flywheel → lands near ~19k/69k in
~4-6 more waves. Next order-of-magnitude requires a PIVOT (native/OSS-build/HW) — the
USER's call, to be made while the flywheel still produces.**

---
### [HIST] 2026-07-19 automation build-out — 18,689 → 18,819 (+130)
triage classifier built + 4-round calibrated, MISPAIR heuristic fixed, inverse
correlator built, grind waves drained productive buckets (ZS-inst +9, VocalPlayer
+7, foreman +5, MECH-LEVER +8, STRUCT-cal +4, calibration +7, correlator +2,
BODY-LEVER drain +17). Zero named regressions across ~16 landings.

## ⛔ PIVOT POINT (2026-07-19 pm) — cheap wire-and-flip / near-miss veins EXHAUSTED

Every coordinator-hand-wave vein was probed to exhaustion this session, each
gated cheaply with zero regressions:
- scatter expose-and-fix ≥88 band = **MIRAGE** (mispair / reloc-coloc / struct-artifact)
- struct-recon = **DEAD** (5/5 leads ICF/foreign-offset)
- near-misses = **AT_LIMIT** (regalloc / RB3<DC3 vtable)
- TrackWatcher = **NO-WAVE mirage** (own methods done, span = foreign scatter)
- grouped-globals = **1 fix** (SystemMs), rest banked
- unwired scatter-include = **+52, DRAINED** (7 cands, 5 flipped)
- unwired own-span wire-and-flip = **DRAINED ≈0** (body-port, not wire)

**What remains is DEEP GRIND: body-porting the ~103 unwired engine TUs + the
~5,300 divergent-body long tail (partial→100 via DC3/rb3-Wii oracle).** Per the
user mandate ("avoid deep grind unless high cascade") and Fable review #3, this is
a **work-kind pivot for the USER to decide**, not a unilateral coordinator grind.
Recommendation to bring the user: route the divergent tail to the AUTOMATED
machinery (crack-farm / grind-loop / the training-corpus model) — that pool is
exactly what it's built for — while coordinator attention moves to whichever the
user ranks of native-port / OSS-build / HW streams. Two explicit asks: (a) re-open
permuter or keep banned; (b) fund a divergence-triage pipeline as batch infra.
The id round-5 gate (+~1,000 names) is NOT reachable at the ~+70/session naming
pace, so the flywheel needs a bigger name-feed (body-port waves) to re-open.

## ▶ AUTOMATION BUILD-OUT (2026-07-19 pm, user-directed)

User decisions: **permuter stays BANNED**; **build the divergence-triage
classifier first** (price the automatable yield before funding any fleet), and
concurrently run Opus-foreman/Sonnet-worker grind waves whose outcomes serve as
ground truth to refine the classifier.

Fresh pool (report.json regenerated at 18,689 baseline, cache cleared;
`~/tmp/triage_pool.csv`): 7,723 named divergent fns / 2.97 MB. 6,341 at exactly
0% (unwired/scatter/unmapped mass); the divergent-body pool = 1,382 fns / 440 KB:
0–50: 292 · 50–75: 138 · 75–90: 260 · 90–98: 289 · 98–99.8: 145 · 99.8+: 258
(the 99.8+ band is mostly reloc-coloc residue — skip bucket).

In flight: (a) Fable tooling lead + Opus implementers building
`scripts/triage/divergence_triage.py` in wt-triage — buckets = mispair /
reloc-coloc / struct-artifact / form-divergence / body-port / zero-unwired,
features via batched `objdiff-cli diff -f json` + `scripts/analysis/
diff_inspect.py` analyzers; output `~/tmp/triage_{results.json,buckets.md}`.
(b) Opus grind foreman running 2–3 waves × 4–5 Sonnet workers on the 90–99.8
band (walls excluded via get_attempts), producing verified diffs for
coordinator landing + ranked tooling-gap feedback.

### Results (same day): classifier LANDED + calibrated, campaign +21, main 18,710

`scripts/triage/divergence_triage.py` on main (full pool 36s warm). Landed
gains: missing-instantiation vein +9 (`ba690393` + harvest), VocalPlayer grind
+7, foreman package +5 → **18,710**, zero regressions. Grind campaign ground
truth (24 assignments: 13 flips/3 improves): **route by diff shape, not %**
(screened 12/15 vs unscreened 1/9); I/D-cluster≥3 ≈ flip; regswap-only = skip;
97.5–99.8 = survivor-bias wall band, 78–96 = flip band. Full rules in memory
`project_grind_foreman_groundtruth_2026-07-19.md`.

**FINAL bucket table** (4 calibration rounds; snapshot committed at
`docs/plans/triage-buckets-2026-07-19.md`, regen with
`python3 scripts/triage/divergence_triage.py --jobs 12`): BODY-LEVER 240
(MEASURED per-stratum: 70-90 non-STL 25%, else ≤5%) · LEVER-STRING 41 +
LEVER-SYMBOL 9 (validated off 1 flip each — calibrate in first wave) ·
ZS-INST 17 (probe 2/2) · BODY-PORT 172 · STRUCT-ARTIFACT 175 + FORM-DIVERGENCE
146 (**UNMEASURED estimates — calibrate before funding**) · certified-skip 318
(RELOC-COLOC 160, WALL-VTORDISP 60, WALL-DEADARG 7, ZS-STL 84, STL-CONTAM 7) ·
MISPAIR 191 (map fix first) · UNRELIABLE-EVIDENCE 226 (stale live-diff, re-verify
before routing) · NEEDS-REVIEW 221 · ZERO-UNMAPPED 5,766.

**Honest fleet economics: bankable ≈96 expected flips** (BODY-LEVER ~26-35 +
LEVER-STRING ~36 + ZS-INST ~15 + LEVER-SYMBOL ~8 + BODY-PORT 78-96 ~3);
**estimate-only upside ≈149** (STRUCT 105, FORM 44) pending 20-30-fn calibration
waves. The original 530 was ~2.2× overpriced (BODY-LEVER measured 6.7% vs 80%
priced — calibration wave 30 fns: only 70-90 non-STL flips at 25%, STL 0/6,
mispairs 9/30). Calibration wave itself landed +7 incl. the **codec.h
`__forceinline` alloca lever** (6 vorbis fns / 1 line; intrinsic-wrapper class
swept — UNIQUE instance, closed). decomp.db drift: ~3k strict fns have renamed
symbol keys; treat get_attempts "not found" as unknown, not pass.

**Session arc (2026-07-19 pm, automation build-out): 18,689 → 18,717 (+28)**
— ZS-instantiation vein +9, VocalPlayer grind +7, foreman package +5, calibration
wave +7. Zero named regressions across all landings.

### MISPAIR bucket fixed (2026-07-19 late) — heuristic bug, 68 reclaimed

The MISPAIR prefilter was OVER-FIRING: it flagged `class-name ≠ attributed-unit-name`
as a wrong-pairing, but `CamShot`::Shake in `CameraShot.cpp` (and BSPFace/Geo,
kdTree/AmbientOcclusion, KerningTable/Font) are the SAME thing — the class just
doesn't string-equal the filename. Fixed (landed): Rule-1 delta made relative
(ratio>1.5 not bare delta>64), a2 class-vs-unit now uses a normalized subsequence
+ a cached class→defining-file index (resolves CamShot→CameraShot.cpp), a3 gated
to skip ICF/anon callee noise. **MISPAIR 191→123**; 68 reclassified —
44→BODY-LEVER, 11→WALL-VTORDISP, 4→LEVER-SYMBOL, 3→SCATTER-OWNER, rest.
3/3 hand-probed reclaims were REAL near-misses (CamShot::Shake 95.6%,
BSPFace::Update 94.8%, KerningTable::SetKerning 93.2%). 51 of 66 reclaimed are
≥70%-live grindable; net-new (not already in a running worklist) = 21 at
`~/tmp/grind_bodylever_reclaimed.json`.

### Priors re-measured (2026-07-19 late) — the "bankable 96" deflates further

Every bucket grinded this round came in BELOW its estimate — the pattern holds
(unmeasured priors ≈ multiples over). Landed +12 (MECH +8 → 18,725, STRUCT +4 →
18,729):
| bucket | priced | MEASURED | note |
|---|---|---|---|
| LEVER-SYMBOL | ~90% | **44%** | only real mech vein; named-Symbol evidence ≠ Symbol is the sole mismatch |
| LEVER-STRING | ~85% | **~5%** | heuristic near-NOISE — flags ObjPtr-2ctor(at-limit)/regalloc/struct as "string-reloc"; needs real string-lit-vs-`li 0` check |
| ZS-INST | high | **0% drained** | MakeString.h already all by-value → no const-ref producible; rest = middleware no-source |
| STRUCT-ARTIFACT | 50-70% | **~12-23%** | 68% mislabeled (mostly STL-template mispairs); bimodal (low-band layout bugs + 99.9% STL-stride; mid-band 0/9); most "deltas" are stack-frame-size not members |

STRUCT-ARTIFACT classifier refinements (recommended, not yet coded): quarantine
STL-template symbols unless a same-`T` sibling is already 100%; discard deltas
equal to `target_frame-base_frame`; require `this`-relative displacement; add a
"genuine-but-blocked" sub-label (foundational-MI-base / ICF-fold / multi-site RB3
divergence) so real-but-unflippable drift doesn't count as yield. The
`Hmx::Object+RndOverlay::Callback` MI base is +4 short across ~13 Rnd/Synth
classes — real foundational drift needing a coordinated cross-class fix.
**SongCollision +2 is gated out** (resize+_M_fill_insert flip but sibling
_M_fill_insert_aux regresses 100→99.87 on a contradictory 56B stride = its own
mispair) — becomes a clean +2 once the inverse-correlator repairs that sibling
pairing.

### Inverse correlator LANDED (2026-07-19 late, 18,744) — tool > its +2

`scripts/harvest/invcorr_mispair_repoint.py` (+ additive `relocs_full` in
`tu5_reloc_masked_correlate.py`) repairs `target_symbol_map.json` for true-mispairs,
applying ONLY unique-byte-identical repoints (guaranteed strict flip), reloc-verified
(position-wise (offset,type); PAIR/0x12 excluded; anon `fn_/lbl_/vftable_/…` =
unconfirmable-not-contradiction; contradicted candidates dropped BEFORE uniqueness
so they can't launder through the hamming/fuzzy fallback). Apply recipe: `--class
UNIQUE-IDENTICAL --apply` → `touch config/45410914/config.yml` (renamer never
un-names) → full rebuild → named-set diff both ways. **Of 122 true-mispairs: only
2 UNIQUE-IDENTICAL** (`__final_insertion_sort<MemDiffEntry>`,
`_List_base<OldMMInst>::clear`) → +2 landed, 0 regressions; 6 reloc-contradicted,
82 nomatch, 18 MULTI. **The vein is thin but the TOOL is the asset** — it's the
machinery to generalize over ZERO-UNMAPPED 5,766 (captain's primary post-drain lane).

Two follow-ups it surfaced:
- **SongCollision aux is NOT a map bug** — its true home `fn_825A38E8` exists but
  is 256B vs our 396B (retail out-of-lines fill/uninit_fill_n our /Ob2 inlines);
  repointing won't flip without fixing the inlining. **The gated SongCollision +2
  stays gated** (correction to the earlier "correlator unlocks it" assumption).
- **GemManager is a rotated-neighborhood mispair cluster** — reloc verification is
  self-referential there (PollHelper's target reloc resolves to PollHelper itself;
  the map names it trusts are themselves wrong). Needs a neighborhood re-derivation
  pass over PollHelper/UpdateArpeggios/MsToTick/Poll@NowBar together, not 1-by-1.

**Held (not landed, follow-ups):**
- **True-MISPAIR (120 remaining):** need an inverse-correlator mode that auto-repoints
  target_symbol_map ONLY on a unique byte-identical unmapped `fn_` (guaranteed
  strict flip), Ghidra-confirms low-hamming singles, hard-excludes ??_G/??_E/ICF/
  over-carve. Proven on `GameMode::SetMode` (mapped VA held an unrelated 84B fn;
  repoint to true 0x826901c0 + `touch config.yml` → 0→97% fuzzy, 0 named
  regressions) — but +0 strict alone and map edits are fleet-wide, so NOT landed;
  worth it as batch tooling. Map-fix recipe: repoint + `touch config/45410914/
  config.yml` (renamer never un-names — stale symbol persists without re-SPLIT) +
  full-rebuild + named-set diff both directions.
- **triage_pool.csv is now regenerated from the 18,717 report** (was stale at
  18,689 — flipped fns like VocalPlayer::UpdateMicDisplay were lingering in
  worklists). Regenerate the pool from current report.json before any extraction.

## Captain review (2026-07-19, at 18,742) — "Drain and repair, then re-measure"

Verdict: triage-and-calibrate is a FINISHING tool, not a growth engine — it did
its job (killed 2-12× overpriced priors before fleets burned, 0 regressions) but
realistic remaining strict from THIS machinery is **~+80-120** (~2 sessions):
BODY-LEVER untapped 59@59%≈+35, reclaimed 21≈+10-12, LEVER-SYMBOL≈+4, correlator
gated, STRUCT/FORM residue ~+20-30 deflated. Durable value = the certified-SKIP
fence (~400 fns) + honest routing. The real MASS is ZERO-UNMAPPED 5,766 + the
~5,300 divergent tail — post-drain, the cascade-shaped next vein is **generalizing
the inverse-correlator's reloc-masked byte-identity correlation over ZERO-UNMAPPED
at fleet scale** (identification, ~0.157 flips/name, feeds the round-5 +1,000-name
gate) + automation fleets on the divergent tail. Native/OSS/HW is the USER's call,
present only after the drain.

**Directive (executing):** Wave A = BODY-LEVER drain on untapped 74 (≥50% bar,
STOP if <40%); Wave B = reclaimed-21 (folded into A) + LEVER-SYMBOL 4 + held
one-offs; Wave C = land correlator repairs under guaranteed-flip gate, then
regen pool + re-run comdat_scatter_scan + id-stack stage-1 for the name-flywheel
dividend. After C, if bankable remainder <+30 → bring user the pivot (scale
correlator over ZERO-UNMAPPED as primary lane).

**Red flags fixed (now wave-preflight checklist):** (1) re-ingest decomp.db from
live report before every foreman wave (done at 18,742); (2) regenerate
triage_pool.csv from live report.json + `rm -f report.cache` before every
extraction/A-B leg (done); (3) cap first-touch calibration at 10-fn probes,
escalate to 30 only if the probe clears ~20%.

## Recent arc

| date | strict | delta | driver |
|---|---|---|---|
| 2026-07-17/18 mega-run | 17,445 | +2,081 | identification stack (+1,871 names), lane-B near-pair, naming wave, BandSwatch, struct leads |
| 2026-07-18 review | 17,445 | — | 3 Opus scouts ranked pools; `docs/plans/review-2026-07-18-next-focus.md` |
| 2026-07-19 body-port/recarve/scatter/id-flywheel | **18,621** | **+1,176** | the "mapped-but-0%" pool cracked open (see below) |

The +1,176 came from **one discovery and its flywheel**: the "mapped-but-0%" pool (functions with
real mangled names stuck at 0%) is overwhelmingly **COMDAT-scatter / TU-composition
drift**, NOT missing source. Retail MSVC/X360 (`/O1`, no LTCG) emits each function
into its own COMDAT and the linker scatters them across `.text`; dtk carves the
retail binary into per-source-file target objs by address range, so a function
whose COMDAT landed in unit X's span is attributed to X even though its source
lives in unit Y — and *our* obj for Y is the one that emits the matching bytes,
under a name objdiff never pairs into X.

### The three fix shapes (all landed, all regression-clean)

1. **Owner-TU whole-file include** — append `#include "<owner>.cpp"` to the
   span-owning `.cpp` so its obj emits the scattered COMDATs. INIT_REVS `gRev`
   collisions on double-include → byte-neutral `#define gRev gRev_<Owner>`.
   Landed: bp3 (+26), bp2r (+84), scatter-sweep w1 (+174). Idiom at HEAD in
   `TDStretch.cpp`, `MeshAnim.cpp`, `Console.cpp`.
2. **Retail-arity body duplication under `#ifndef HX_NATIVE`** — when whole-file
   include collides (statics/anon-ns/PROPSYNC barewords), copy just the needed
   bodies into the span owner with extern decls; native keeps canonical defs.
   Landed: bp1 (+36). Idiom in `Debug.cpp`, `DirLoader.cpp`, `MemHeap.cpp`.
3. **Splits gap-fill recarve** — when the auto blob is the missing *middle* of an
   already-pinned TU, add one gap `.text` range + reloc-masked byte-identity map
   entries (`tu5_reloc_masked_correlate.py`); ICF-twin MULTI groups resolve by
   order-preserving assignment; funclets cascade free. Landed: rc1 (+130,
   AccomplishmentPanel), rc3 (+14, TrackWatcherImpl).

**Instrument:** `scripts/harvest/comdat_scatter_scan.py` (~0.9s, re-runnable)
scans the COFF symbol tables of all ~836 compiled objs and splits every named-0%
function into **SCATTER** (emitted by another wired obj → owner-include/dup
fixable) vs **UNWIRED** (no wired obj emits it → gameport pool).

**Kill test before recarving any auto blob:** if the span's pre-mapped names are
emitted by *no* wired obj, the blob is a COMDAT catch-all from unwired classes —
a gameport target, not an attribution gap. (rc2/SongSort `0x826DD570` was
correctly skipped this way: SkillsAwardList / CampaignEra* / a NavListSortMgr
SongSortMgr redesign that matches DC3, not our older port.)

## Captain's plan (2026-07-19, Fable strategic review) — ACTIVE

**Key reframe (overturns the "scatter drained" verdict below):** the ~218
"net-0" scatter residue is NOT dead — it is a **near-miss discovery engine**.
Applying an owner-include PAIRS the scattered body in objdiff, turning an opaque
0% stub into a *diagnosed* fuzzy near-miss with a known owner source file + DC3/
rb3-Wii oracle. This is exactly how UpdateOverlay / UpdateCache / enableAAFilter
were found and then fixed to strict. Net-0 ≠ rejected; it means "here's a paired
body and its diff." **Frame for every wave: judged by strict flips + names fed to
the identification flywheel** (round-5 gate ~+1,000 names; body-ports buy it).

  **sw2-parent-leak guard (F1 discovery, load-bearing):** several sw3 consumers
  are themselves scatter-*owners* included by sw2-era parents (Morph←HamMove,
  DepthBuffer3D←UIList, Gem←OutfitConfig, …). Those parents bracket the include
  with `#define gRev gRev_<Child>` but do NOT set `SW_SCATTER_OWNER_INCLUDE`, so a
  naive owner-append leaks the new body into the parent TU and breaks it. Fix:
  guard the append to fire only in the consumer's PRIMARY TU. `gRev` is a static
  member *variable* (never a macro) in a primary compile, so `#ifndef gRev` is a
  reliable primary-vs-owner discriminator; where an internal block `#undef gRev`s
  before the tail (UIList's BandDirector block), use a stronger top-of-file
  `<UNIT>_SW3_PRIMARY_TU` sentinel instead.

- **Wave 1 — Expose-and-fix:** RAN 2026-07-19. Harvest → `~/tmp/expose_harvest.md`
  (9 freebies / 71 ≥88% / 118 compile-fail). **Actual yield: +10 total (F1
  freebies only; F2=0, F3=0, F4=0).** BIG EV MISS vs the +80–150 estimate — the
  ≥88 band is systematically blocked (recalibration below).
  **NEW CASCADE-SHAPED VEIN — DC3-oversized struct recon (F2 leads).** F2 proved
  the clean-building 99.9x targets miss on a single **struct-size immediate**: our
  DC3-sourced headers declare several structs LARGER than retail. Shrinking each to
  retail size flips its near-miss AND (cascade) every function that touches that
  struct — a shared-struct fix is wide-ripple by nature. Exact leads (each needs
  its own whole-binary A/B; gate DC3-newer fields behind `#ifndef HX_NATIVE`):
  **SongSection 0x18→0xc, RecurseInfo 0x18→0x10, BandIKEffector::Constraint
  0x1c→0xc, StoreMainPanel member −0x18, CharPollGroup base subobject −0x28.**
  This is the "B_STRUCT_OFFSET is the real vein" call (see A_TOOLING ICF memory),
  now with concrete targets. HIGHER EV than the mispair band.
  **PROBE RESULTS (2026-07-19) — REFINED PREDICATE, both wide leads DEAD:**
  S2 CharPollGroup = **misread** (the −0x28 was a member offset 0x50 vs an
  ICF-folded `??_G` dtor's full-object adjust 0x78; layout already matches retail;
  ground-truth against target-asm MEMBER offsets, NOT Ghidra `??_G` adjusts —
  ICF-contaminated). S1 SongSection = size mismatch is **real** (0x18 vs 0xc, DC3
  added mPatternRange+mSongPattern) but **cascade REFUTED** — its only
  `vector<SongSection>` consumers are 2 unimplemented stubs; **zero near-misses
  index it** → 0 flips. **THE RULE: a struct resize flips a near-miss only when a
  near-miss (90–99.99%) actually indexes that struct. Size-mismatch is necessary
  but NOT sufficient.** So the scanner predicate is NOT "struct size ≠ retail" —
  it's "struct size ≠ retail STL-stride AND indexed by ≥1 fn in the 90–99.99%
  band" (join size-deltas against the near-miss pool). The 3 narrow S3 leads
  (RecurseInfo/Constraint/StoreMainPanel) were each derived FROM a near-miss
  (99.9x), so they satisfy the predicate — S3 is the live test of the vein.
  **S3 RESULT — VEIN DEAD (all 5 struct leads mirages, 2026-07-19).** RecurseInfo
  0x10 is real but holds two 0xC Strings (=0x18; can't shrink without global
  String change). Constraint copy-ctor matches 100% at 0x1c (F2's `li 0xc` = a
  mis-paired ICF body). StoreMainPanel ctor matches 100% (F2's `addi 0x88` = a
  BandStorePanel singleton's return+0x88, foreign object). **Conclusion: F2's
  "struct-size" immediates were real numbers but SYSTEMATICALLY ICF-fold or
  foreign-offset artifacts, not oversized fields — the Movie::IsLoading mispair
  lesson generalized to the whole exposed sub-100 band. Do NOT fund a struct-size
  scanner sweep; do NOT re-hunt these. The ≥88 exposed band is a mirage across ALL
  three sub-taxonomies (mispair / reloc-co-location / struct-artifact).** Net from
  the entire struct-recon probe lane: 0, but 0 regressions (verify-before-edit
  gate held on all 5).
  **Mechanism rule (F2, durable):** an owner `.cpp` with its OWN nested
  scatter-includes is UNSAFE via the dialect shim — the push forces Object.h
  dialect and breaks the owner's nested ObjMacros-dialect includes, cascading to
  every TU that includes the consumer. Nested-scatter counts: HamCamTransform=9,
  BandCamShot=3, ViewSetting=2, HamNavList/Spotlight/HolmesClient=1; SAFE (0):
  SongLayout, CharEyes, ClipDistMap, CharPollGroup, TransAnim, FlowSetProperty,
  StoreMainPanel, BandIKEffector.
  **⚠ RECALIBRATION — the ≥88%-but-<100% exposed band is a MISPAIR MIRAGE.** The
  target-symbol renamer labels a physically-adjacent, ICF-shaped-but-semantically-
  DIFFERENT function with the exposed name, so "closing" the near-miss matches our
  code to the WRONG target. F4 proved every tiny "one-liner" was a mispair:
  Movie::IsLoading ("fixing" Movie 4→8B broke 10 MoviePanel funcs, net −9; our
  4-byte Movie is CORRECT, DC3's 8-byte doesn't apply to RB3), NetLoader::
  PostDownload (ours already stores 0x10 correctly), PlatformMgr::QueueEnumJob
  (target tail-calls a DIFFERENT function), OnSeedRandomContext (already 100 in its
  home unit). F3 proved the 99.8x `??_G`/STL residue is gapped by a reloc-arg
  (vtable/callee at a different scattered address) report.json won't forgive. **So
  only the exact-100.00%-on-include freebies flip; the sub-100 band is
  mispairs + struct-divergence + pairing artifacts. Do NOT re-hunt it as cheap
  near-misses.** UniqueFilename is the lone real crack — see vein #3.
  Still-untried Wave-1 items (separate from the mirage band): 3 body-dup cases
  (CameraShot←Flow, PropAnim←PropKeys, CharBonesMeshes←GemManager as `#ifndef
  HX_NATIVE` dup), MidiSynth WorldDir::PropSync trio (splits re-attribution —
  Dir.obj already emits), MemTracker::StopLog (map/splits).
- **Wave 2 — UNWIRED-OWNER SCATTER-WIRING = THE TOP LIVE VEIN (probe P2 GO,
  +9 @3917a0e4).** The winning shape: **117 `.cpp` files exist in-tree with full
  bodies but were never wired** (not in objects.json → no obj emits them; list
  `~/tmp/unwired_cpp_list.txt`). Retail scattered their COMDATs into an
  already-wired unit's `.text` span → a near-free `#include "<owner>.cpp"` append
  to that consumer emits + pairs them. P2: CubeTex.cpp += 4 includes
  (rnddx9/{MultiMesh,Cam,Lit,Part}.cpp) → +9 in ~5 min, 0 regr. Sweep running
  (`~/tmp/uwire_worklist.md`). **~60–65% clean flip rate**; FILTER OUT
  multiple-inheritance dtors (`??1`/`??_D`/`??_G` of 2+-base classes) — they ride
  a shared-base layout delta, only reach 99.x, route to a separate struct stream.
  Prioritize engine files (rnddx9/rndobj/synth/movie/os/net/midi) over gesture/*
  + Dance-Central hamobj/* (mostly Kinect, likely no RB3 target). EV: unknown
  addressable pool, but each hit is ~free. This SUPERSEDES the old "per-symbol
  owner-driven port" framing below — the bodies already exist; only the wiring
  was missing.
  **OWN-SPAN WIRE-AND-FLIP = DRAINED (2026-07-19, gated out ≈0).** The captain's
  "dark own-span pool, engine/lib-heavy, good byte-match prior" thesis was based
  on DC3's tree, not ours: the big C-lib pools don't exist in `src/` (jpeg=1 file
  not 73, zlib=1, oggvorbis=1, net=14/3-unwired not 107). Real unwired pool =
  **~103 engine files**. Best case (26 with pre-carved target objs, 20 compiled)
  → **5 byte-identity hits, ALL noise** (vtable-adjustor thunks + unwind funclets),
  0 real flips, no ≥3-hit clusters. Root cause: DC3-lineage bodies DIVERGE from
  retail RB3, and TU5 map-anchoring already carved every span that byte-matches an
  anonymous region — so the ~77 files with no target obj are precisely the ones
  whose bodies don't match. **These are BODY-PORT targets (partial→port to 100%
  via DC3/rb3-Wii oracle, the `bodyport-batch` skills), NOT wire-and-flip. Do NOT
  build a whole-binary own-span correlator.** With this, ALL cheap wire-and-flip
  and near-miss veins are exhausted → pivot territory (see PIVOT below).
- ~~**Wave 2 (old) — Oracle-backed UNWIRED wiring** (superseded by the above; the
  "port the bodies" premise was wrong — bodies pre-exist, just unwired).~~
  Original target census (for reference): rnddx9 CubeTex 8 Dx* + Rnd_Xbox(3),
  Anim(7), Sequence(8), MemTracker(8), DataPointMgr(5), WaveFile(4), Cam(2); game
  DataArraySongInfo(11), TrainerPanel(5), VocalTrack(3), VocalPlayer(3). SKIP
  oracle-poor (System/LEAPCORE, Mic, FFT, Compress, DSP, rtti/osfinfo).
- **Wave 3 — TrackWatcherImpl beatmatch gameport:** 121 flat-0% NAMED bodies,
  direct oracle `../rb3/src/system/beatmatch/TrackWatcherImpl.cpp`, splits
  already gap-filled (rc3). NOT banned grind — highest-cascade single target
  (biggest name-feed to round-5; RealGuitarTrackWatcherImpl.obj already owns
  scattered spans → landing beatmatch types unblocks chained proposals). Split
  4–6 agents by method cluster, 4488B monster last, accept partial. EV +80–140.
- **Micro-lane (no wave slot):** the 4 named near-miss probes (PreInit,
  InitParams, FindShader, SetTransform) + DxRnd::UpdateScalerParams / UpdateCache
  99.8 / enableAAFilter 99.5 / RingBuffer::Write 91.4 singles; grouped-globals
  **RECON ONLY** (count 80–97 fns citing shared-anchor `lbl_*` base+offset
  addressing — ≥30 → build a source-level global-aggregation mechanism, <10 →
  drop). → `~/tmp/grouped_globals_recon.md`.
- **Between waves:** re-run `comdat_scatter_scan.py` (chained proposals) + id
  stack stage-1 even below the +1,000 gate (~0.15 flips/name).
- **Pivot decision deferred ~3 waves:** after, the long tail is the ~5,300
  nomatch divergent-body pool — choose (a) scale Wave-1 expose-and-fix into a
  systematic divergence-triage pipeline, (b) grouped-globals mechanism if recon
  supports, or (c) pivot work-kind (native/tooling).

## Live veins (ranked by EV)

### 1. COMDAT-scatter sweep — reframed as EXPOSE-AND-FIX (see Captain's plan)
After 3 sweep waves (+661) the scanner reports **275 SCATTER candidates /
218 proposals** still open. Previously called "nearly drained / body-port-grade";
the captain's reframe (above) makes these the **cheapest diagnosed near-miss
fodder on the board** — apply the include to pair the body, harvest the exposed
%, fix the ≥88% ones. Cross-dialect walls unlocked by the wave-3 byte-neutral
shim `obj/dialect_object_{push,pop}.h`. Method is mechanical + gated (per-unit
whole-binary A/B, auto-revert on loss); **re-run the scanner between waves** —
fixing one owner unblocks chained proposals (w1's MidiSynth←PropSync only
appeared after PropSync←Dir landed).

### 2. UNWIRED gameport pool — 327 fns / 138 units
Functions no wired obj emits. Two sub-classes:
- **Engine, oracle-backed (portable):** rnddx9/CubeTex (8 Dx* ctors, DC3 oracle),
  Anim (7), Lit_NG, rnddx9/Rnd, Sequence — DC3 near-verbatim. These are true
  body-ports / TU wirings, ~medium cost.
- **Oracle-poor (defer, hard):** FFT (10 fns, VMX128 hand-asm — DC3's FFT unit is
  only 23%), System/LEAPCORE (32, no oracle), Mic + ExternalMic (25, Xbox voice),
  Compress/XGRAPHICS (10, shader-microcode), GranularSynth/SpectralAnalysis/
  PeakDetector (DSP hand code), rtti/osfinfo (CRT). Lowest ROI — leave for last.
- **Game (band3):** 16 units incl. TrainerPanel (5), DataArraySongInfo (11) —
  rb3-Wii oracle, gameport cost.

### 3. Exposed near-misses (fuzzy → strict fodder) — partly worked (nm +3, sm +3)
Pairing the scattered bodies revealed genuine near-misses hidden as 0% stubs.
DONE: NgRnd::UpdateOverlay/Terminate + MakeWorldSphere (nm, NgStats mSpotlights
strip + Geo.h fix); RndShaderMgr::Terminate/Invalidate + InitShaderOptions (sm,
ShaderType enum 38→26).
**AT_LIMIT (do NOT re-hunt, 2026-07-19):** RndShaderMgr::FindShader 80.3 and
SetTransform 81.7 — our source is byte-identical to the DC3 oracle; both are
pure callee-save-vs-volatile regalloc divergence (permuter-band, banned).
FindShader additionally has a HARD structural blocker: retail RB3 (2010)'s
`RndShaderMgr` vtable has **one fewer virtual than DC3 (2012)** — NewShaderProgram
sits at slot `0x5c` retail vs our `0x60`. DC3 is not an oracle for the vtable
shape; removing a virtual is a wide-ripple header change (re-lays every
ShaderMgr-subclass vtable) with no ground truth for *which* virtual RB3 lacks.
Prerequisite for any revisit: dump a concrete retail ShaderMgr-subclass vtable to
identify the missing virtual — a standalone structural task, not near-miss polish.
REMAINING leads: UpdateCache 99.8; enableAAFilter 99.5 (RateTransposer +16B
member — pad-probe); RingBuffer::Write 91.4; DxRnd::UpdateScalerParams 0%.
MemTracker::StopLog 77 = MISPAIRING (target is a MemFree/dtor, not StopLog —
map/splits fix, not source).
**UniqueFilename — CRACKED (F4 2026-07-19), needs an independent splits pin to
land.** The 2-line fix in `src/system/os/File.cpp` reaches 100.0% normalized
(Ghidra-verified vs `default_tu5.xex`): (a) declare `int i=0` BEFORE `String ret`;
(b) format string is hardcoded `"%s_%06d.bmp"` (drops the `c2` param — retail
ignores it and emits `.bmp` for both callers: Rnd.cpp:499 wants `.bmp`,
LiveCameraInput.cpp:1185 passes `"data"` but retail still emits `.bmp`). Can't land
now: UniqueFilename's COMDAT lives in Rnd's `.text` span, so the only measurement
path (`Rnd ← os/File.cpp` include) reshuffles objdiff pairing and drops
`GetNormalMapTextures` (rndobj/Utl) 100→94.5% — a pairing artifact, not a real
regression (`matched_functions` stays put, Utl.obj byte-identical). Give
UniqueFilename its own `splits.txt` `.text` range (carve out of Rnd's span, like
rc1/rc4 gap-fills) → then the File.cpp fix is a clean +1. Exact patch in F4's
report / this session's transcript.

### 4. Remaining recarve gap-fills
**0x82560660** UI-message run DONE (rc4 +48, UIStats gap-fill). **0x8234FCEC**
DataArray/ObjectDir SKIPPED by kill test (unwired gesture catch-all —
SkeletonFrame from gesture/Skeleton.cpp; recovery = wire that TU first). The
Accomplishment/TrackWatcher blobs are done; SongSort is UNWIRED (vein #2). The
easy gap-fill recarve targets are now exhausted; new ones require wiring an
unwired owner TU first (converges with vein #2).

### 5. Deep grinds (banked, lower EV)
- **TrackWatcher family — CORRECTED CHARACTERIZATION (2026-07-19).** The "121
  flat-0% NAMED bodies" framing is WRONG per the live report: `TrackWatcherImpl`
  is 159 fns / 45 matched / **78 at-0% but ALL anonymous `fn_` (0 named-0)** + 36
  named partials; `RealGuitarTrackWatcherImpl` 40/16/21-anon; family total ~104
  unnamed-0% + ~40 partials. Our source (872 lines, ≈ oracle 859) is largely
  ported. So the 0% pool is an **IDENTIFICATION gap (unmapped targets), not a
  body-port gap** — Wave-2 approach is **correlator-FIRST**: run
  `scripts/harvest/tu5_reloc_masked_correlate.py` on the TrackWatcher-family objs
  to pair our compiled named methods to the target's unnamed `fn_` by
  reloc-masked byte identity → add map entries → the byte-matching bodies flip
  (+ feed the id flywheel). ONLY the residual (unnamed, bodies diverge) + the ~40
  named partials are the actual body-port grind (oracle
  `../rb3/src/system/beatmatch/TrackWatcherImpl.cpp`, largest 4488B). Do NOT fan
  out a 4–6-agent body-port wave before the correlator run scopes the real
  residual.
  **CORRELATOR RUN DONE (2026-07-19) — it's a real body-port grind, NOT a cheap
  id win.** `tu5_reloc_masked_correlate.py TrackWatcherImpl.obj (target) vs our
  compiled obj` → only **14 UNIQUE byte-matches, ALL boilerplate** (`__unwind$`
  funclets + `bad_alloc` dtor); **0 real named methods match.** The 78 unmapped
  bodies are genuinely DIVERGENT (NOMATCH) — our source is a rough Wii port that
  doesn't byte-match 360 retail. So each flip needs a real body-port THEN
  correlator-pairing (unnamed target). EV per Fable (+80-140) is optimistic;
  recommend a SMALL probe (1 agent, ~8 representative bodies, measure port
  hit-rate) before committing 4–6 agents. If hit-rate is low, TrackWatcher is a
  low-ROI grind → pivot to oracle-backed unwired wiring (vein #2) or the
  round-5-prep / user pivot conversation.
  **PROBE VERDICT (2026-07-19): NO-WAVE — TrackWatcher is a MIRAGE.** The premise
  is wrong: TrackWatcherImpl has only 23 named methods, **22 already at 100%**
  (own methods effectively DONE); the "78 anon-0%" are FOREIGN functions
  scatter-interleaved into its 20KB pinned span (BandCrowdMeter, PartAnim,
  HamSupereasyData, Object, DataArray, STL templates — our source already
  `#include`s PartAnim.cpp + BandCrowdMeter.cpp). Correlator confirmed 0 real
  matches. Only residual = `CheckForAutoplay` 92.9% (permuter-class, deferred).
  The real (separate) opportunity buried here is BandCrowdMeter/PartAnim as
  first-class units (~20% near-misses, cross-TU layout problem, NOT clean
  porting). **Do NOT commit a TrackWatcher body-port wave.**
- **Grouped-globals wall** — RECON DONE (2026-07-19): verdict **NARROW, no
  mechanism wave**. Of 441 named 80–97 fns, only **17** are genuinely fold-walled
  and just **2 pure-fold** (the known MemFindAddrHeap/SystemMs). MSVC only shares
  a base register when the globals are *defined in the same TU as the accessor* —
  so cross-TU manager singletons (`TheBandDirector`/`TheLoadMgr`, `TheTaskMgr`/
  `TheUI`, `TheSessionMgr`/`TheSynth`, `ThePlatformMgr`/`region`) are UNFOLDABLE
  by any source change. Only **3 intra-TU clusters are source-fixable** (cheap
  micro-fixes, ~+2–3, fold into scatter campaign not a wave): MemHeap
  `gHeaps`+`gNumHeaps` (extern in MemHeap.cpp), Debug/System `gSystemMs`+
  `gSystemFrac` (extern in Debug.cpp, defined System.cpp), Voice
  `gCommitSyncVoices`+`gCommitTag` (in-TU, declaration-adjacency fix). Detail:
  `~/tmp/grouped_globals_recon.md`. Not a new mechanism — a facet of TU-drift.
- **DxRnd::UpdateScalerParams** (0x82739948) — paired at 0% since the vtable fix,
  genuine body-port lead.
- **BandCharacter −4 container compaction** (cr6), **BandCamShot vbase-MI
  reconstruction** (documented wall, pad-probe-killed the tempting +0x80 tail).

### 6. Identification round-4 — DONE +170 (`39038c09`), FLYWHEEL CONFIRMED
The scatter/recarve campaign's +250 names & 3 new pinned clusters cleared the
round-3 fixed-point gate. Re-running `scripts/harvest/TU5_SCANNER_STACK.md`
yielded **+170 strict** at ~0.157 flips/name (5x the collapsed 0.031 rate),
fixed point in 3 rounds, 6/6 Ghidra spot-checks. **Key insight: the scatter
vein FEEDS the identifier** — every owner-TU-include body flip creates a fresh
clean byte-identity pair the scanner then cracks. Round-5 not warranted until
+~1,000 more names. This coupling means future body-port waves should be
followed by an identification re-run.

## Dead / banned (do NOT re-hunt)
Permuter (user directive — low yield, grinds the box); ≥99 fixwave round-2
(rejected — 80% funclet mirage, ~20-30 fixable, no cascade); lane-B near-pair
residue (drained); A_TOOLING ICF fold mirage; pad-probe deferred struct walls
(drained); local-static mechanical wave; the 3 scatter-sweep w1 lossy candidates
(CameraShot←Flow, PropAnim←PropKeys/AmbientOcclusion, CharBonesMeshes←GemManager
— need body-dup, not whole-file include).

### ⛔ Added by lane BZ-1 (2026-07-30) — two veins closed at the MECHANISM level

**1. ★★★ RB3 retail was built with an OLDER X360 compiler than our DC3-era
toolchain ⇒ ~0.127 pp of residue is one-directionally UNREACHABLE.** Two fixed
instruction-selection differences, each byte-zero tested and each showing a
**strictly one-way** distribution — the signature of a toolchain difference, not
a source defect:
- strcpy idiom: retail `cmplwi` vs our `extsb.` — **17 forward, 0 reverse**, and
  our compiler emits `cmplwi` there **0 times in 1,118 functions**.
- power-of-2 size test: retail `srawi.` vs our `clrrwi.` — **6 forward, 0
  reverse**.

The header is byte-identical to DC3's and a `#pragma intrinsic(strcpy)` probe was
a **no-op**, so there is no source-side lever. Class these `build_env` and do not
chase them. ⚠ This also *caps* specific targets: `BandCharacter::SyncProperty`
(1,560 B) has just 2 replaces but **one is the toolchain-dead `extsb.`/`cmplwi`**,
so it can never reach 100 — do not fund it.

**2. ★★★ `comm_swap` REFUTED (42 fns / 32,412 B / 0.3064 pp) — do not re-hunt.**
Closed twice over, and **re-verified under the correct compiler by lane CB-1
(2026-07-30) — the 10224 flip is completely INERT here**: 0 of 42 functions
changed in *either* direction, with the same 61 mismatched instruction indices
and the same argument strings, function for function, under both compilers
(both legs built in one worktree at one commit, split frozen, `@comp.id`
verified per leg: `0xAB27F0` vs `0xAB2E6E`). So **none of the flip's +26 came
from this pool**, and the 0.3064 pp stays dead.

⚠ **CB-1 also corrected this entry's own evidence — the original claim was ~10×
thinner than it read.** The flagship `NextSongPanel::CountOrCreateExpandedDetails`
(12,220 B, 2 diffs) does contain 50 `add r3,…` sites, but they are **not one
population**: **45 use the pair `(r10, r11)` and are equal on both sides**, so
they are a *different regalloc shape* (fresh `count*8` in r10 vs a
strength-reduced byte-offset induction variable in r28) and carry **no**
information about operand commutativity. The real discriminating population is
**5 sites, split 2 swapped / 3 not** — not 48/2 over 50. "48 of 50 agree" reads
as overwhelming; the actual evidence was five sites. ★ Lesson: **state the
discriminating population, not the site count** — see
`feedback_site_count_is_not_defect_count` (fan-out is blast radius, never yield).

★★★ **The durable argument is source-side, not target-side.** BZ-1 argued from
retail (self-inconsistent ⇒ unreproducible), which depends on retail's
nondeterminism and was weakened by the corrected population size. CB-1's
argument does not: all 50 sites are literally `ptr.Node(count++) =
DataArrayPtr(...)` with zero spelling variation, and `DataArrayPtr::Node` (
`src/system/obj/Data.h:772`) is a **single** forwarding body onto
`DataArray::Node` (`:506`), which is a single body reducing to `return
mNodes[i]`. **One inline body ⇒ one operand order ⇒ all 5 sites flip together,
but retail needs two.** No source edit can produce both. (Coordinator verified
both bodies independently.) The only lever that changes the order at all —
`i[mNodes]` for `mNodes[i]` — trades the 2 gained for the 3 lost *in this
function alone*, with every `Node()` caller in the binary as blast radius.

## Method (stable)
Fable coordinator delegates to Opus agents in `scripts/setup_worktree.sh`
worktrees under `~/tmp`; coordinator independently re-verifies every diff with a
fresh clean-worktree whole-binary A/B (strict set keyed `(unit, name)`, LOST must
be empty) before a path-limited commit on main; `touch config/45410914/config.yml`
before any A/B leg that changed splits/map (renamer re-split trap). Scoreboard:
`docs/plans/tu5-p5-progress.md`. Memory: `project_comdat_scatter_lever_2026-07-19`.
