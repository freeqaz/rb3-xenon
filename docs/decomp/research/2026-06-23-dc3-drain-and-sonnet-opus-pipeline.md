# DC3-oracle drain + the Sonnet-executes/Opus-reviews pipeline verdict (2026-06-23)

Session continuation: user asked to "exhaust the DC3 oracle first (high leverage), test if
Sonnet can do the work + class-A, loop to drain the pools, then the big frontier." Results.

## 1. DC3-oracle drain — exhausted at +46 strict (+ fuzzy)
The dc3_oracle.json (33987 pairs, engine sim 0.90) drains in TIERS:
- **Tier-1 — name targets INSIDE already-pinned engine TUs: +43 strict** (+ fuzzy staircase
  ≥90 +83, ≥95 +69). Eligibility funnel: 18948 sim≥0.7 → only **176 net-new** (the rest are
  already named or foreign/unpinned). 43 of 173 own bodies byte-reproduced to 100% (the simple
  ones); the rest land 82-94% (fuzzy "close enough"). CHEAP one-pass naming. dc3_name_eligible.py.
- **Tier-2 — pin the wired-but-unpinned engine TUs: ICF-scatter-WALLED, only Trig.cpp +3.**
  Of 34 TUs: 15 SCATTER_ICF, 10 SINGLE_BODY (+1 each), 5 NO_ORACLE (inlined), 1 DC3-newer, **1
  contiguous (Trig)**. The apparent contiguous clusters are 32-byte ICF stub/adjustor thunks;
  the real bodies are scattered into foreign pins or inlined away under /Ob2. **The engine
  unmatched pool hits the SAME ICF-scatter wall as the game layer** — so the DC3 oracle's strict
  leverage was the already-pinned naming (+43), NOT new engine spans.
⇒ DC3 oracle = a great IDENTIFIER + a +46 strict harvest + a fuzzy lever; NOT the thousands the
raw 18863 count suggested. The scattered engine real bodies require per-function identity_transfer
micro-pins (the big frontier), not contiguous span pins.

## 2. The Sonnet-executes / Opus-reviews pipeline — VERDICT
Tested across two task types (Opus plans, Sonnet executes per task, Opus reviews):
- **Naming / analysis work (tier-1): EXCELLENT.** Sonnet computed the per-subdir eligible naming
  entries correctly; Opus review approved 173/176, rejected 3 foreign. Cheap + reliable. ✅
- **Port work (class-A, 8 TUs): Sonnet does the MECHANICS, but the pipeline OVER-PRODUCES ICF-stub
  INFLATION — and the Opus REVIEW step approved some of it.** Of 8 Sonnet ports: 5 were
  inflation/foreign/funclet-only (BandCharacter-r2 +137 = **0 real-bodied**, NextSongPanel +39 = 0
  real, MetaPerformer +119 = 3 real + 8 FOREIGN, MetaPanel +69 = funclet-only, CharacterCreatorPanel
  = self-flagged 0, Synth-r2 = self-flagged icf_clean=false). The Opus reviewer **land-recommended
  MetaPanel +69** despite itself noting "64/69 are ≤44B funclets." Only 2 were genuinely honest:
  **AssetTypes +3** (3 named logic methods) + **BandDirector-r2 +139** (21 real-bodied anchors, 0
  foreign — composed-verified, held).
- ⭐ **DECISIVE LESSON: the honesty JUDGMENT must stay with the COORDINATOR's `icf_alias_check`
  tool, not the review agent.** Without the coordinator gate, ~+364 of fake matches would have
  landed. Sonnet is fine for the mechanical port + analysis; the review agent eyeballs and is too
  lenient on stub-fold inflation. FIX for future Sonnet/Opus port waves: the Opus review MUST run
  `icf_alias_check.py --worktree` and REJECT on stub-domination (real-bodied anchors = 0), not judge
  by description. The decisive signal: **0 real-bodied anchors = INFLATION (reject); ≥N real-bodied
  + 0 foreign = honest own cluster (the stubs are own funclets).**

## 3. NEW sub-vein — range-2 extensions of already-wired TUs
BandDirector-r2 (+139) added a SECOND `.text` range to an already-wired+matched TU (its source
already compiles the methods; the 2nd contiguous cluster just needed a pin). Works when the cluster
has real-bodied anchors (BandDirector: 21 real → +139 held). FAILS when 0-real (BandCharacter-r2:
0 real → pure stub inflation, rejected). A modest fresh sub-vein for wired TUs with a contiguous 2nd
cluster — gate every one on icf_alias_check real-bodied-anchor presence.

## State
main @dafe9eb, **STRICT 10215 / session 6932→10215 (+3283)**. Fuzzy whole 10.65%, staircase
≥90=11905 / ≥95=11665. DC3 oracle drained; class-A thinning + inflation-prone (coordinator gate
essential). NEXT = the big frontier: ICF-scattered methods (game+engine) via per-fn identity_transfer
micro-pins + DC3/Wii oracle naming → fuzzy "close enough" + the few real bodies as strict. The owner
(freeqaz) is concurrently building a band3 porting worklist + identity oracle — COORDINATE there.
