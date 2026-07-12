# Remaining hard-targets triage — 2026-07-12

> **STATUS (2026-07-12):** live diagnostic record. Classifies the deep-grind
> targets called out in the wave-40 close-out ([[project-wave40]]) by their
> objdiff *diagnose* signature, so a future session picks the tractable ones
> without re-running the diagnosis. Counting authority = `report.json`
> `match_percent_normalized==100`; all % below are objdiff normalized against
> main at 15,822 matched.

## TL;DR tractability ranking

| Target | Match | Class | Verdict |
|---|---|---|---|
| Campaign::Handle | 52.9% | body-port (handler exprs + 1 stack local) | **ATTEMPTABLE** — in progress this session |
| BandSongMgr::Handle | 38.5% | systematic reg-swap + stack+4 + 2 code blocks | permuter/source-order — hard |
| VocalTrack::UpdateScrolling | 39.7% | structural (739 swaps / 553 indels / 56 replaces) | **WALL** — needs full source rewrite |
| BandProfile (ctor + tail accessors) | n/a | 32KB-class struct-tail archaeology | **LOW-ROI** — defer |
| GemPlayer | done | 1 permuter near-miss left | effectively closed |
| VocalPlayer 99.9% band | n/a | EH-funclet mirage (all `fn_`) | not real closes |

## #2 Divergent bodies

### Campaign::Handle — `?Handle@Campaign@@UAA?AVDataNode@@PAVDataArray@@_N@Z`
- **1314 instr, 52.9%.** `BEGIN_HANDLERS(Campaign)` macro chain, `Campaign.cpp:910`.
- Real divergence after stripping 478 symbol-reloc-noise diffs is modest:
  - **+24 stack shift across 18 instr** = one extra ~24B local vs retail (top lever).
  - **65 inserts concentrated in the tail** (57 in idx 1159-1222): our compile emits
    a function-local-static (`?$S4@?GK@…@4IA` guard + `?_hs@?IK@…@4VSymbol@@A`) where
    retail loads a plain global `lbl_82DA0017` — a late handler's `static Symbol`
    likely should be a member/file-static.
  - **3 concrete handler-body replaces:** idx 785 retail `bl fn_8258C7D8` vs ours
    inlining `m_symCurrentAccomplishment == acc_createsetlist` (Campaign.cpp:705 —
    retail wraps it in a named helper); idx 937 retail inlines where ours calls
    `HasValidUser()`.
- **Verdict: attemptable body-port.** These are per-handler source differences, not
  permuter noise. Being worked this session (worktree `~/tmp/campaign-handle`).

### BandSongMgr::Handle — `?Handle@BandSongMgr@@UAA?AVDataNode@@PAVDataArray@@_N@Z`
- **729 instr, 38.5%.** Dominant signature: a **systematic r29↔r30 swap across 87
  instructions** + a **+4 stack shift (35 instr)** + two real insert clusters (24 instr
  at idx 126-149, 46 instr at idx 609-654). One real diff_op (idx 533 tailcall vs branch).
- The 87-instr r29↔r30 swap says two locals are allocated in swapped registers vs retail
  — usually a single source-declaration-order difference, but the +4 shift and the two
  24/46-instr code blocks mean there's also genuinely missing/extra source logic.
- **Verdict: permuter/source-order class.** The swap alone is permuter territory (blocked
  per owner); the code-block inserts need the exact retail handler bodies. Lower ROI than
  Campaign — the divergence is less concentrated. Revisit after Campaign proves the
  handler-matching recipe.

### VocalTrack::UpdateScrolling — `?UpdateScrolling@VocalTrack@@QAAXM@Z`
- **2515 instr, 39.7%.** 739 register swaps across **146 distinct pairs**, 553
  insert/delete in **123 clusters**, 56 real replaces, 20 diff_op branch-condition flips.
- This is not a near-miss — the source structure is fundamentally different from retail
  (opposite branch senses, wholesale reordered blocks, divw/lfs/stfs replaces). Our
  `VocalTrack::UpdateScrolling` body was written against the Wii oracle whose logic
  diverges from retail-360.
- **Verdict: WALL.** Closing it means rewriting the function to retail's control flow from
  scratch (Ghidra decompile → re-derive). Not worth it against ~+1 payoff. This is the
  "block-placement wall" from prior waves — confirmed real.

## #3 Struct-layout recon

### BandProfile — 32KB class, 3.2KB unmodeled tail
- Our `BandProfile.h` (182 lines) already tracks the rb3-Wii oracle (180 lines) closely,
  so the tail drift is **not** missing members — it's a base class or embedded member
  whose element sizing differs (Wii vs retail-360 layout), pushing `sizeof(BandProfile)`
  to 0x7c70 while our model ends ~0x6fc0.
- Only ~6 BandProfile *own* methods are mapped (Handle@0x82575AF0 already matched/at_limit,
  HasSeenHint, SaveSize, SetLastCharUsed, GetBandLogoTex, RockCentralOpCompleteMsg ctor).
  The ctor is not even pinned. Most map entries for "BandProfile" are *other* classes'
  methods taking a `BandProfile*` arg (Accomplishment::IsFulfilled etc.) — they don't
  depend on the tail.
- **Verdict: LOW-ROI archaeology.** Reconstructing 3.2KB of embedded sub-objects precisely
  is multi-session work whose payoff is the ctor (one big fn) + a handful of tail
  accessors. Defer until cheaper veins are truly gone; if attempted, derive the tail from
  the retail ctor store offsets + dc3/Wii oracle, and **verify every offset against an
  already-100% sibling's codegen, never against `// 0xNN` header comments** (the wave-38
  Stats-drift refutation lesson: stale comments caused a false +12B drift inference).

### GemPlayer — effectively closed
- 20/22 mapped methods at 100%. Remaining: `UpdateGameCymbalLanes` 96.9% (r27↔r28 swap +
  branch flips = permuter) and `FillComplete` which is **now 100% normalized** (the
  orchestrator DB's 79.1% was stale; a report refresh confirmed no hidden win/loss — it's
  already in the 15,822). The wave-40 "foundational mUser+0x30 base drift" concern is
  **overstated** — if the base layout were wrong the whole class wouldn't be at 100%. Drop
  GemPlayer from the struct-recon list.

### AccomplishmentManager / GemManager retail-only members
- Not yet diagnosed in depth this pass. GemManager.cpp is `NonMatching` (unwired-ish);
  AccomplishmentManager.cpp is wired. Candidate for a future focused pass but no evidence
  it's higher-ROI than Campaign::Handle.

## #4 jeff pdata/boundary round 3
- Full design doc: `docs/plans/jeff-pdata-boundary-round3.md` (2026-07-12). Three classes
  (truncated-fragment repair, AddRoll pdata over-split merge, except_data seed-time
  suppression). Key new finding there: a **seed-time bug** — stray `func_type==3` pdata
  records create bogus mid-body function starts (`util/xex.rs` L1105/1127-1140); b1bc97c
  fixed the byte corruption but not the structural split. Priority: struct-recon >
  jeff-round-3 (+5-15 strict, only-path for its cases) > divergent bodies.

## Method notes (reusable)
- `run_diff_inspect mode=diagnose` gives the root-cause histogram; the **noise budget**
  section separates symbol-reloc/branch-dest noise (ignorable — normalized diff drops it)
  from real inserts/deletes/replaces (the actual work). Judge tractability by the
  *unexplained + real replace/insert* count, not the raw match %.
- A systematic single reg-swap across many instrs = permuter/source-order (skip per owner).
  Concentrated tail inserts + local-static symbols = a specific handler source difference
  (attemptable). 100+ swap pairs + 100+ indel clusters = structural wall (rewrite-only).
