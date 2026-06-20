# W9 L10 — reveal_sweep `--handlers` mode (val!=0 macro-body reveal tooling)

**Date:** 2026-06-20 · **Base:** main @812e1df (8314 matched) · **Mode:** adversarial
discover/planner (Opus L10), READ-ONLY in main.

## Verdict: REAL_ACTIONABLE

The frontier's diagnosis is **correct and verified at the COFF level**, but the EV
framing needs splitting. The mechanism is real; the **Handle slice (~31) is already
spoken for** by the in-flight W9 wave; the **fresh, independently-landable yield is the
SyncProperty/Save/Load slice (~12) which does NOT depend on the Handle prereq.**

## Mechanism verified (ground truth, not hypothesis)

`tools/reveal_sweep.py:read_coff_base` yields exactly one body per `.text` section,
keyed on `val == 0 and not name.startswith(".")`. Confirmed in
`build/45410914/src/system/synth/FxSend.obj` sec 145 (`.text` COMDAT):
- val==0 leader = the bare `.text` section symbol (cls=3, name starts `.`) → EXCLUDED.
- `?Handle@FxSend@@...` body lives at **val=0x8 cls=2** → never yielded.
- rest of section = `$M64xxx` thunk labels + `__unwind$` funclets at various val!=0.

So `read_coff_base` structurally cannot see any macro-bodied virtual that MSVC packs
behind a catch/funclet leader. Binary-wide census of OUR compiled objs (class-2
defining symbols at val!=0 = MISSED):

| kind | total class-2 | val!=0 (missed) |
|---|---|---|
| Handle | 601 | **323** |
| SyncProperty | 499 | **220** |
| Load | 577 | 78 |
| Save | 520 | 14 |
| Replace | 157 | 0 |

(val!=0 ≠ "revealable" — most aren't byte-exact. But every byte-exact one among them is
INVISIBLE to the current tool.)

## The correct algorithm (target-driven extent — the load-bearing fix)

My first naive prototype produced **46 adjustor-thunk false positives + 0 real** because
two traps bite:
1. **`$2/$3/$4...` vtable-adjustor thunks** (`?Handle@BandCamShot@@$4PPPPPPPM@A@...`)
   are byte-identical across classes once the branch reloc is masked; a single thunk VA
   (e.g. 0x8227CD50) matched BandDirector's Handle **and** SyncProperty **and** Save
   **and** Load simultaneously. MUST reject `@@\$[0-9A-Z]`.
2. **body-extent miscalc.** A macro body's COMDAT also holds `$M*`/`__unwind$` internal
   labels at val!=0; using "next-higher val" as the body end truncates the 244-byte
   FxSend Handle to 0x10 (16 B). And using the full section rawsz OVER-reads (the COMDAT
   tail funclet is split into a separate `fn_` on the target side).

**Fix (validated):** read the base body from `val` to **section rawsz**, then **truncate
to the TARGET `fn_`'s exact size** before the reloc-masked `word_eq_frac` compare. The
target `fn_<addr>` (val==0, exact size, via existing `read_coff_functions`) is the body
WITHOUT the trailing funclet, so truncation aligns the two. Gate: reject `$`-thunks,
`MIN_SIZE 0x18`, and **1:1 (addr ↔ class-name) uniqueness within the unit** (the
per-class-name key the frontier names — it sidesteps cross-class self-similarity that
dumps these into reveal_sweep's 1556 `ambiguous` edges).

Prototype: `/tmp/handlers_final.py <Kind...>`.

### Validation (decisive)
- **Prereq worktree** (`wt-w9-reconcile-handle-prereq-FINAL`, 9fb9016): Handle → **31
  unique** (superset of the frontier's verified 26 + 5 char-tier; **zero thunk FPs**).
- ScrollSelect `SyncProperty` (fn_827F81E8) drilled to the byte: base body at val=0x8
  sec 219 (0xe0 section), target fn_ = 184 B; **raw bytes differ, masked word_eq = 1.0
  = BYTE-EXACT** — the canonical self-validating reveal (a wrong addr cannot read 1.0).

## The fresh, independently-landable yield (the actionable)

Running the validated algorithm on **CURRENT MAIN @812e1df (no prereq):**

| kind | unique reveals on plain main |
|---|---|
| Handle | **0** (need END_HANDLERS PathName-tail prereq → byte-exact only post-prereq) |
| SyncProperty | **10** |
| Save | **1** |
| Load | **1** |

SyncProperty/Save/Load macros have NO PathName-tail issue, so these **12 are byte-exact
on plain main** — pure val!=0-aware reveals, no source/pin/port/prereq. All 12:
- verified UNMAPPED on main AND unclaimed by every in-flight Handle worktree;
- their target `fn_<addr>` PRESENT in the target obj (pairing-ready).

```
0x82300DD8  ?SyncProperty@CrowdAudio@@...        CrowdAudio       516
0x823AD908  ?SyncProperty@CharIKFoot@@...        CharIKFoot       304
0x823C5080  ?SyncProperty@CharBone@@...          CharBone         820
0x823F43F0  ?SyncProperty@RndDrawable@@...       Draw             360
0x824D9B98  ?SyncProperty@WorldInstance@@...     Instance         600
0x827C6098  ?SyncProperty@MidiParserMgr@@...     MidiParserMgr    184
0x827E61C8  ?SyncProperty@UIListDir@@...         UIListDir       1524
0x827F81E8  ?SyncProperty@ScrollSelect@@...      ScrollSelect     184
0x827F8BE0  ?SyncProperty@UIListArrow@@...       UIListArrow      632
0x827F9AA8  ?SyncProperty@UIListCustom@@...      UIListCustom     292
0x824C68B8  ?Save@Spotlight@@UAAXAAVBinStream@@@Z Spotlight        688
0x82726900  ?Load@DataNode@@QAAXAAVBinStream@@@Z  DataNode         436
```

## Relationship to L7/L8 (generic reveal_sweep) and the Handle wave

- **L8** = generic reveal_sweep val==0 byte-exact (20 candidates) — orthogonal slice.
- **L10 (this)** = the val!=0 COMDAT-buried slice that L8/L9 explicitly flag as the
  tool gap (`2026-06-20-w9-L9-wired-handle-pairing-wave-post-prereq.md` lines 111-118:
  "worth folding into reveal_sweep as a `--handlers` mode").
- **Handle slice is DONE elsewhere**: `wt-w9-w9-handle-reveal-batch-25-on-prereq`
  (acd4590) already carries prereq c45629b + the 25 Handle reveals, branched off main.
  So the L10 tooling's *Handle* value is the GENERALIZED RE-RUN (auto-harvest future
  waves + the SyncProperty/Save/Load families), not those 25 specific matches.

## Self-containment / packaging

Two independent items:
- **A1 (no prereq, lands vs main@8314):** add the 12 SyncProperty/Save/Load reveals to
  `scripts/target_symbol_map.json` in one worktree; `rm
  build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml &&
  ninja`; keep only 100%-landing entries; whole-binary A/B. Pure map-add,
  attribution_risk=FALSE (self-validating). Est +12.
- **A2 (tooling):** fold the validated target-driven matcher into
  `tools/reveal_sweep.py` as `--handlers [--kinds Handle,SyncProperty,Save,Load]`
  (Sonnet implements; validate against `/tmp/handlers_final.py`'s 31-Handle control in
  the prereq worktree + the 12 main-baseline SyncProperty/Save/Load). Then the
  post-prereq Handle wave + every future port-then-pin wave can `--handlers` sweep
  automatically instead of one-off `/tmp` scripts. Tool value, not a direct +N.

## Discovered frontier (seeds next layer)
- **post-prereq SyncProperty residue:** after the Handle prereq lands, re-run
  `--handlers SyncProperty` — the prereq rebuilds many UI/char TUs; expect a few more
  SyncProperty/Save/Load to flip byte-exact (the 1524-B UIListDir SyncProperty already
  pairs; its siblings may too). Recon-gated on the landed prereq.
- **the 1556 ambiguous edges** (same-size byte-exact methods blocked by 1:1) — an
  ordering/VA-vs-pin-span disambiguator could recover a fraction (L8 already named this;
  low confidence, bytes don't establish identity).
