# LTO / LTCG vs ICF investigation — is the retail XEX hiding our matches?

**Date:** 2026-06-06. **Question (project owner):** the retail RB3 build "feels like an
LTO build" — are we missing matches because of link-time optimization, and if we
detect/work-around it can we (a) gauge true progress, (b) target the touched areas to
get match% up, (c) fingerprint *around* the optimization to transfer decomp from
other projects programmatically?

**Bottom line:** The premise is **refuted with high confidence — the retail XEX is
NOT an LTCG/LTO build.** The only link-time optimization present is **ICF**
(`/OPT:ICF`, on by default), and it is **not meaningfully suppressing our match%**
(verified naming/ICF-recoverable lever in the near band = **1 function**, not the
~625 an earlier pass claimed). Our 7.0% is honest; the 91% at 0% is simply
undecompiled, not optimization-blocked. The real programmatic lever is **content
fingerprint transfer from DC3** — **1,765 byte-identical candidates at 0%**, which
dwarfs anything ICF offers.

---

## 1. Verdict: NO LTCG (high confidence)

Verified by two parallel Opus forensics passes on the RB3 binary itself (not just the
DC3 assumption the docs inherited). DC3 flags confirmed via
`../dc3-decomp/config/373307D9/config.json`: `/O1 /Oi /GR /EHsc`, **no `/GL`, no
`/LTCG`** in cflags or ldflags; ldflags carry `/MAP` and **no `/DEBUG`**, so the
linker runs `/OPT:REF`+`/OPT:ICF` by default. Four independent codegen signatures on
RB3 retail all read no-whole-program-opt:

| Test | LTCG would show | RB3 retail shows | Source |
|---|---|---|---|
| TU spatial grouping | functions scattered by hotness (`/ORDER`/PGO) | **626 pinned TU `.text` spans, 0 overlaps, 0 cross-TU intrusions**; MasterAudio 43 fns in 8.43 KB | `config/45410914/splits.txt` |
| Cross-TU inlining | callees inlined; no separate-TU byte-match possible | **326+ byte-exact matches contain cross-TU `bl`** (e.g. `fn_82758AA4`→`fn_82738050` across MasterAudio→Object) | `build/45410914/report.json` + asm |
| Float const pooling | single base reg for float+static array | **97.6% of 1,959 float loads use a dedicated `lis`-per-float**; 47 "pooled" are a benign within-page peephole | `build/45410914/asm/*.s` |
| DC3 (known no-LTCG) | — | RB3 codegen **character-identical** to DC3 | port 8000 vs 8002 |

**Consequence for the worry:** match-count distribution is 100%: 4,597 (7.0%) ·
95–99.9%: ~800 · **0%: ~59,700 (91%)**. The 91% at 0% are *not decompiled yet* — not
blocked by any optimization. Matches saturate exactly where source has been ported.

**The DC3 `/GL /LTCG` experiment is unnecessary and wouldn't work cleanly:** `/GL`
emits IL objects with no real PPC codegen until a whole-program relink, producing a
binary that matches *neither* retail target. We already have direct evidence from
RB3's own bytes that beats what that experiment could show.

## 2. ICF reality (the real, but small, link-time effect)

The inherited docs' headline **"31,754 folded symbols → 3,068 addresses" is a
DC3 figure** (from DC3's leaked `ham_xbox_r.map`). RB3 has no symbol names, so the
ICF story is **inverted**: RB3's 65,558 functions are mostly the *kept
representatives*; the folded-away twins simply don't exist as RB3 addresses. The
DC3-era `decomp.db.has_linker_merged`/`merged_symbols` and `bin/merged-symbols` are
**stale/absent** for RB3 (they depended on the leaked map).

**Denominator is honest:** `total_functions=65558` is *post-fold* (one entry per
kept address). Folds *deflate* it (pre-fold ≈94k). No hidden denominator credit.

## 3. The "+625 recoverable by linking" claim was an artifact (cautionary)

An ICF-quantification pass claimed **~625 functions are byte-correct, blocked only by
an ICF/merged call target, recoverable by a link-aware compare (+0.95pp)**. This is
**wrong.** Two coarse detectors conflate things that are opposite in meaning:

- `detect_linker_merged` (`../objdiff/objdiff-cli/src/cmd/analysis.rs`) fires on **any**
  `bl` to a differently-named function — it cannot tell genuine ICF (callee
  byte-identical, linker folded) from *calling the wrong function* (a real bug).
- `classify_nearmiss.py`'s `OFFSET` class lumps **relocation-address noise**
  (recoverable) together with **raw struct-field displacement deltas** (real bugs).

Direct inspection of the differing instructions (all `reloc=None`, raw displacements)
showed the near pool is **struct-layout bugs**, e.g. `NgEnviron::UpdateApproxLighting`
`lwz r11,0x2c` vs `0xdc` — a consistent **+0xB0 field shift** (the coupled-base-class
wall), **not** link-recoverable noise.

### Honest near-miss taxonomy — `tools/true_progress.py` (NEW, this investigation)

Splits the coarse `OFFSET` into FRAME_RECON / STRUCT_OFF / NAME_RELOC / REG / OPCODE
per instruction, then buckets each function by its *worst* residual. Run on [99,100):

```
720 near-miss functions:
  FRAME_ONLY    373   subi r31,r12 frame/funclet pairing noise + ICF-dtor naming
  STRUCT_WORK   183   real coupled-base-class layout bugs
  CODEGEN_WORK  163   real permuter/source work
  RECOVERABLE     1   pure naming/ICF  <-- the ONLY verified link-recoverable fn
```

- **Verified naming/ICF lever = 1**, not 625.
- **FRAME_ONLY (373)** are the `subi r31,r12` funclet-reconstruct artifacts that the
  `engine_baseclass_layout_wall` memory already flags as **tooling noise** (known
  jeff asm mis-nest / objdiff funclet over-subscription — `project_jeff_asm_misnest`,
  `project_objdiff_fork`). Fixable in the diff *tooling*, not by linking, not by
  source. Worth a targeted pass but it's a separate known track.
- **STRUCT_WORK + CODEGEN_WORK (346)** are genuine remaining grind (the structural
  layout wall + permuter targets), already tracked.

Re-run honest progress anytime: `tools/true_progress.py --lo 99 --hi 100`
(or `--lo 90 --hi 100` for the wider pool). Writes `/tmp/true_progress.json`.

## 4. The REAL programmatic lever: DC3 content fingerprint transfer

ICF *helps* content fingerprinting (folding requires identical strings/constants, so
content survives). `tools/global_fuzzy_index.py` (reloc-masked MinHash/Jaccard over
opcode shingles) → `global_fuzzy_pairs.json` already holds **2,171 RB3↔DC3 pairs**,
**2,157 at jaccard=1.0**, **1,765 at jaccard=1.0 AND identical size** (byte-identical
candidates). Cross-referenced against `report.json`:

- **All 1,765 are currently at 0%** — genuinely fresh, unpinned backlog (none are
  stuck-at-99 wall residual). **328,120 bytes across 175 DC3 source objects.**
- Ranked targets (full list: `docs/plans/fingerprint-transfer-backlog-2026-06-06.json`):
  `ccodec.obj` 138 · `CharFeedback.obj` 132 · `Dir.obj` 120 · `Flow.obj` 94 ·
  `Spotlight.obj` 84 · `Cheats.obj` 83 · `InlineHelp.obj` 59 · `PracticeChoosePanel`
  42 · `volumemeter` 40 · `MeshDeform` 38 · `CharBone` 38 …

This is the proven engine-split/content-match transfer that already landed **+1566**
(`project_engine_split_relocation`: `dc3/game_content_match`, `relocate_engine_splits`,
`pin_identified`, `fingerprint_pipeline.py`). The "+1566 backlog DRAINED" note
predates this fresh `global_fuzzy_pairs.json` run (it's untracked/new).

**Yield caveat (don't over-claim — same discipline that killed the +625):** jaccard=1.0
+ same-size is a very strong byte-identity signal, but the DC3 source must still
compile byte-exact under our flags. A subset will hit the **struct-layout wall** from
§3 and land at 99% instead of 100%. Treat 1,765 as *candidates*, not guaranteed
matches; expect a large-but-<100% yield, leaf/codec/util files (ccodec, WaveFile,
volumemeter) converting most cleanly, layout-heavy ones (Mesh*, Char*) partially.

## 5. Recommended forward path

1. **Stop treating LTO as a blocker** — it isn't present. Correct the inherited docs
   (§6). Match% is honest.
2. **Harvest the 1,765-candidate DC3 transfer backlog** (`fingerprint-transfer-backlog-2026-06-06.json`)
   via the existing content-match pin/port kit, top-of-list first
   (ccodec/Dir/CharFeedback). This is the highest-EV programmatic lever — bigger and
   more real than any ICF idea.
3. **(Separate known track)** the FRAME_ONLY 373 funclet-pairing noise is a jeff/objdiff
   *tooling* fix (`project_jeff_asm_misnest`), not linking — could surface up to ~373
   near-misses to 100% if the funclet over-subscription is fixed.
4. **STRUCT_WORK 183 / CODEGEN_WORK 163** remain the coupled-base-class layout grind +
   permuter targets — already tracked (`structural-grind-pass` skill, permuter).

## 6. Inherited-doc corrections (DC3 verbatim, wrong/unverified for RB3)

- `docs/decomp/patterns/verifiable-icf.md` §"LTCG/Global Pooling" + §"Float Constant
  Pooling": correctly say LTCG does NOT apply — keep, but they're framed as "for other
  projects." Now verified to apply to RB3 too: **no LTCG, no float pooling.**
- `docs/decomp/TECHNICAL_NOTES.md:877,881`: "target binary is a **debug build** … float
  constant pooling **UNDER INVESTIGATION**" — the "debug build" claim is DC3-inherited
  and unverified for RB3; the float-pooling "investigation" is **closed: ~0 true
  pooling sites** (the 47 are a benign within-page peephole). Annotated inline.
- The `31,754 folded / merged_symbols / bin/merged-symbols` machinery is **DC3-only**;
  RB3 has no leaked map. ICF fold-set membership for RB3 must be derived from
  byte-identity, not a symbol map.

## Key files / refs (cold-pickup)
- Tools: `tools/true_progress.py` (new), `tools/classify_nearmiss.py`,
  `tools/fuzzy_progress.py`, `tools/global_fuzzy_index.py`, `tools/fingerprint_pipeline.py`
- Data: `global_fuzzy_pairs.json`, `build/45410914/report.json`,
  `config/45410914/splits.txt`, `docs/plans/fingerprint-transfer-backlog-2026-06-06.json`
- objdiff ICF detector: `../objdiff/objdiff-cli/src/cmd/analysis.rs::detect_linker_merged`
- DC3 flags: `../dc3-decomp/config/373307D9/config.json`
- Related memory: `project_engine_split_relocation`, `project_engine_baseclass_layout_wall`,
  `project_jeff_asm_misnest`, `project_objdiff_fork`, `feedback_verify_assumptions`
