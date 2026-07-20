# hy3 champion-run log analysis (2026-07-20)

**Scope:** OSS-model decomp eval, rb3-xenon. Champion = `tencent/hy3` GA.
**Data source note (important):** the top-level ndjson in
`~/tmp/grind_runs/eval_hy3new_v2/attempts_full.ndjson` is a **dead run** — 144/187
rows are `HTTP 403 Key limit exceeded`, and the 43 "ok" rows are seed baselines
(`after==before`). The **live champion data is `~/tmp/grind_runs/eval_hy3new_v2_r2/`**
(the retry: 172/182 ok rows, mean floored-Δ **11.83** over 45 scored fns, 1 crack@100 =
SymToControllerType 23.5→100). All hy3 numbers below are from `_r2`. Runner-up qwen
data = `~/tmp/grind_runs/eval_qwen_v2full2/`.

Config (manifest): k=3 bootstraps, max_refines=1, `--agent-tools` with
`--tool-calls 3` (discretionary calls/turn; mandatory `score_candidate` verify is
**not** charged), max_tokens=32000, reasoning_max_tokens=12000, base 3a13f6c3.

---

## 1. Crack anatomy

### hy3 crack — SymToControllerType 23.5 → 100.0 (via REFINE)

Attempt sequence (chronological):
| kind | tools | result |
|------|-------|--------|
| seed | – | 23.48 |
| bootstrap#1 | 5 calls / 3 rounds | 23.33 (regressed, floored) |
| bootstrap#2 | 1 call | **69.78** |
| refine (turn 0) | target_asm + ghidra_decompile + score | **100.0** |

The crack came from the **refine loop building on the best bootstrap (69.78)**.
- bootstrap#2's 69.78 candidate was a while-loop with a **duplicated compare** and a
  bugged branch: `if (i<5){ compare; i++; } else if (i!=5) break; compare; i++;` — the
  second (fall-through) compare emitted an extra `ControllerTypeToSym` call at loop
  entry, which the target doesn't have (the mismatch cluster was "yours-only"
  instructions 7-13 + a stack-offset diff `r1+0x50` vs `r1+0x54`).
- In refine, hy3 called **`target_asm`** (saw the real control flow: `cmpwi r31,5;
  blt body; bne none`) and **`ghidra_decompile`**, then reasoned for **34.5k chars**
  to reverse-engineer that the target **shares a single compare block** between the
  `i<5` path (branch-in) and the `i==5` path (fall-through). Winning source:
  ```cpp
  ControllerType SymToControllerType(Symbol s) {
      int i = 0;
      while (1) {
          if (i >= 5) { if (i != 5) break; }   // shared-compare structure
          if (s == ControllerTypeToSym((ControllerType)i)) return (ControllerType)i;
          i++;
      }
      return kControllerNone;
  }
  ```
- Two other insights in the CoT: it recognized **MILO_ASSERT(false, 0x1D) compiles to
  nothing in retail** (correctly omitted it), and that `kControllerNone == 5` (from
  `li r3, 5`). This is a genuine reverse-engineering win, not a lucky rewrite.

**Lesson:** the crack required (a) a decent bootstrap to refine from, (b) `target_asm`
to see real control flow, and (c) long structural reasoning. Refine + assembly-tool was
load-bearing.

### qwen crack — TrainerGemTab::GetLane 61.2 → 100.0 (via BOOTSTRAP #1)

qwen cracked it on the **first bootstrap** (4 calls / 3 rounds, 26k CoT), using
**`struct_layout` + `read_source`** to nail member offsets (mTrackType@0x4,
mLanes@0x8, mLefty@0x150). Winner:
```cpp
int TrainerGemTab::GetLane(int slot) const {
    if (mTrackType == kTrackDrum) {
        if (slot == 0) return 0;
        if (mLefty) slot = mLanes - slot;        // mutate PARAM in place
    } else if (mLefty) {
        slot = mLanes - slot - 1;
    }
    return slot;
}
```

**hy3 saw this same function and only reached 61.9** — its best candidate was
*near-identical* but introduced a **separate `int lane = slot;` local** and a nested
`else { if (mLefty) ... }`:
```cpp
int lane = slot;
if (mTrackType == kTrackDrum) { if (slot==0) return 0; if (mLefty) lane = mLanes - slot; }
else { if (mLefty) lane = mLanes - slot - 1; }
return lane;
```
The extra local forces an extra register/`mr` and a different flow shape → stuck at 62%.
**hy3's miss was an idiom miss: it didn't reuse/mutate the parameter in place.** qwen's
`struct_layout` use also gave it exact offsets; hy3's GetLane attempts used
search/read but never `struct_layout`.

---

## 2. Failure taxonomy (45 scored fns; 25 no-gain, 20 gain, 1 crack)

### (a) Compile-fail — 6 attempts across 3 fns (4.3% of 141 attempts, matches ledger)
All 3 are large **`::Handle` message-dispatch functions** using RB3 DataArray idioms:
- **`DataArray::Sym` wrong arity** — `C2660 'DataArray::Sym': function does not take N
  arguments` (OvershellSlot::Handle:1880, CustomizePanel::Handle:1101). Model guessed
  the DataArray accessor signature.
- **`DataNode::DataNode` no matching overload** — `C2665` (TrackPanel::Handle:904).
- **Undeclared members** — `C2065 'mUserMgr'`, `C2065 'mOvershellPanel'`
  (OvershellSlot::Handle) — model **invented member names** not on the class.
- **static-vs-instance** — `C2352 'BandUserMgr::GetUserFromSlot'` called as static.

Root cause: model doesn't know the exact class member list or the DataArray/DataNode
API, and hallucinates members + accessor signatures for the big handlers.

### (b) Backend/empty — 4 attempts
2× `HTTP 429` (provider "tencent/hy3" rate limit), 1× `HTTP 503` (no healthy
provider), 1× `empty completion` (SongDB::GetVocalNoteListCount). Empty rate ~0.7%,
truncation `stop_reason=length` = **0** attempts. Provider instability (429/503) is a
real but small tax; watch concurrency.

### (c) Valid-compile-but-no-improvement / damaged
~19 fns compiled but didn't beat seed. Two sub-classes:
- **Model damaged a near-match (floored to 0, budget wasted)** — best attempt WORSE
  than seed: CustomizePanel::Handle 97.1→13.9, TrackPanel::Handle 95.4→35.0,
  TourProgress::Handle 94.9→42.9, SongSortMgr::DoesOfferMatchFilter 87.2→69.0,
  EditSetlistPanel::SyncProperty 69.8→54.9, GetSongSpecificEntries 58.9→34.5,
  Player::PollMultiplier 90.1→88.2, PatchPanel::Load 66.1→63.1. **~8 fns where the
  model rewrote a high-seed function wholesale and regressed it.** The floor saves the
  mean, but these are pure wasted spend + missed easy holds.
- **Wall-class held flat** — very-low-seed functions the model can't move
  (GemManager::PollHelper 2.2, SongDB::GetVocalNoteListCount 6.8→6.2,
  GemTrainerPanel::Poll 19.3, VocalPlayer::HandlePhraseEnd 16.8, UIEventMgr::Terminate
  82.7, KeyChain::getMasher 81.4). These are the low-% / regswap / large-body walls.

---

## 3. Refine-loop effectiveness (k=3, max_refines=1)

Of 42 fns that got a refine attempt:
- **refine BETTER than best bootstrap: 4** (incl. the SymToControllerType 23→100 crack)
- refine SAME: 17
- refine WORSE: 21

Raw refine is **net-negative** (21 worse vs 4 better) — but the floor means "worse"
costs nothing on the mean, and the 4 wins include the **only crack of the run**. Refine
is high-variance / low-hit-rate but occasionally decisive. When it works, it's because
the model used `target_asm` on the refine turn to correct control flow (SymToControllerType).
Evidence the refine reads the diff: yes for the crack (explicitly reasons over the
"yours-only" mismatch cluster and stack-offset diff). But most refines re-derive from
scratch and drift worse rather than surgically addressing the shown mismatches.

---

## 4. Tool use (`--tool-calls 3` discretionary budget)

Tool invocation counts across 141 attempts:
| tool | calls |
|------|------|
| search_source | 143 |
| score_candidate (mandatory, uncharged) | 138 |
| read_source | 54 |
| target_asm | 21 |
| struct_layout | 15 |
| ghidra_decompile | 12 |
| resolve_address | 6 |

- **search_source + read_source dominate** and drive the biggest wins: the two enum
  lookups GetSymbolFromAssetType (+87.2) and GetSymbolFromAssetBoutique (+83.3) were
  cracked by search/read finding the oracle table. The best-gainers list is mostly
  bootstrap+search/read (or no-tool for tiny fns).
- **target_asm/ghidra are rare (21/12) but high-leverage**: they produced the crack
  (SymToControllerType) and the only real move on ScoreTypeToTrackType (+26). hy3
  under-uses assembly/struct tools relative to their payoff — and notably **never
  called `struct_layout` on GetLane**, the exact tool qwen used to crack it.
- **Budget pressure is real: 66/141 attempts (47%) used ≥3 discretionary calls**
  (the cap), and 23 attempts recorded 4 charged calls (auto-repair overflow). Nearly
  half of attempts saturate the 3-call budget → supports raising 3→6.
- Per-attempt delta split (confounded — tools cluster on hard/refine attempts): ≥1
  discretionary tool = −5.34 mean; 0 tools = +5.35. Do **not** read this as "tools
  hurt"; it reflects that tools are spent on the hard, low-seed, and refine attempts
  that regress. The *winning* attempts on hard fns are the tool-using ones.

---

## 5. Reasoning behavior

- CoT is emitted as **normal output tokens**, not a separate thinking channel
  (`thinking_blob=None`, `thinking_tokens=None` on every row). Consequence: the
  **12k `reasoning_max_tokens` cap does not bind hy3** — its reasoning competes with
  code inside the 32k `max_tokens` budget.
- CoT length (chars): min 5.1k, p25 22.5k, **median 33.8k**, p75 42.4k, p90 49.2k,
  max 76.2k (≈19k tokens).
- output_tokens: median 10,968, **max 23,461 (under the 32k ceiling)**;
  `stop_reason=length` = **0/141** → no truncation observed this run. Neither the
  reasoning cap nor the output cap is currently the bottleneck.
- Reasoning quality vs score: weak positive correlation, **r=+0.12** (n=131);
  short-CoT (<median) mean per-attempt Δ = −5.74 vs long-CoT +3.81. Longer, structured
  reasoning correlates with better outcomes (confounded by the fact that lazy near-match
  rewrites are short and damaging), but there is no runaway-reasoning pathology.

---

## 6. Top 5 actionable levers (ranked by expected impact)

1. **Pre-supply the target class header (member list) + DataArray/DataNode API cheat-sheet
   for `::Handle`/dispatch functions.** Evidence: 100% of compile-fails (6 attempts, 3
   fns) are invented members (`mUserMgr`, `mOvershellPanel`), wrong `DataArray::Sym`
   arity (C2660), `DataNode` ctor overload (C2665), static-vs-instance (C2352). These
   are the largest functions in the set and currently unreachable. Injecting the class's
   real member declarations + the DataArray/DataNode/Message signatures into the prompt
   would convert compile-fails into scorable attempts. *Highest impact: unblocks the
   biggest fns and removes the entire compile-fail bucket.*

2. **Add a "minimal-diff / don't-regress" guard for high-seed functions.** Evidence:
   ~8 fns where the model rewrote a 66–97% seed wholesale and dropped it
   (CustomizePanel 97→14, TrackPanel 95→35, TourProgress 95→43, SongSortMgr 87→69,
   Player::PollMultiplier 90→88). Instruct the model: "the current source already
   scores X%; make the smallest edit that fixes the shown mismatches; never rewrite from
   scratch," and/or auto-skip refine when seed ≥90% and the diff is regswap-only. Saves
   wasted spend and captures easy holds. *High impact, near-zero cost.*

3. **Add a few-shot for the "mutate the parameter in place, don't introduce a new local"
   idiom.** Evidence: hy3 lost GetLane (61.9 vs qwen 100) purely by adding `int lane =
   slot;` instead of reassigning `slot`. This extra-local anti-pattern shows up in
   MSVC/O1 register-reuse matching broadly. One worked example (GetLane qwen winner)
   would teach it. *Medium-high; this is a recurring near-miss class.*

4. **Raise the discretionary tool budget 3→6 AND nudge hy3 to use `target_asm` +
   `struct_layout` earlier.** Evidence: 47% of attempts saturate the 3-call cap;
   target_asm/ghidra are called only 21/12 times yet produced the run's only crack and
   its best hard-fn moves; hy3 never used `struct_layout` on GetLane where qwen's
   struct_layout call was decisive. A larger budget + a prompt line "before writing code
   for a control-flow or offset mismatch, call target_asm and struct_layout" should
   raise the hard-fn hit rate. *Medium; directly testable via the 3→6 bench.*

5. **Make refine diff-anchored instead of free re-derivation.** Evidence: refine is
   21-worse / 4-better; the 4 wins explicitly reasoned over the objdiff mismatch
   cluster (SymToControllerType), while the 21 losses drift. Constrain the refine prompt
   to "keep the previous best candidate as the base; change ONLY the instructions listed
   in the mismatch clusters," and pass the prior best source as the anchor. Converts
   refine from high-variance to monotone-improving. *Medium; also protects lever #2.*

### Secondary notes
- Provider stability: 2×429 + 1×503 this run; keep concurrency modest.
- The 12k reasoning cap is a non-issue for hy3 (reasoning is inline output, not a
  thinking channel) — do **not** spend effort raising it; the 32k output cap has
  headroom (max seen 23.5k). If anything, guard against reasoning eating the code budget
  on the longest-CoT fns.
