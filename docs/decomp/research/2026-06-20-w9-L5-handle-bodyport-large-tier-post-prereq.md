# W9 L5 — handle-bodyport-large-tier-post-prereq (ADVERSARIAL DISCOVER/PLANNER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Verdict: REAL_ACTIONABLE** — the large-tier Handle body-port lever is real, but
it is fundamentally DIFFERENT from the small/mid tier and the frontier's "+20 from
a bare map entry per TU" framing is WRONG. Large Handles do NOT flip from a map
entry — each needs a per-TU handler-list reconciliation (a body-port-class diff)
because our source / the oracles diverge from RB3 retail by 1–3 handlers per TU.
**Mode:** read-only in main; ground truth from target per-unit `.s`, COFF auto_03
symbol VAs, the two prereq worktrees, and DC3 + rb3-Wii oracle handler lists.

## TL;DR — what's RIGHT / WRONG in the frontier

- **RIGHT:** the large tier (47 census fns, instr 131–1604) is genuine, untapped,
  high-aggregate-EV body-port work. NONE of the 47 are paired even in the +188
  spike worktree (`wt-w9-spike-strategyB...` @51fc194) — only the 6 small ones
  from L4 are. All owner TUs are already pinned/wired (no splits.txt edits needed).
- **WRONG #1 (it is NOT a flip).** The frontier/lead say "map entry + objdiff" per
  TU. FALSIFIED: for every large Handle sampled, the source/oracle handler count
  ≠ the retail target's static-symbol-init count. A bare map entry reads <100%.
  Each item is a real PORT: resolve the class, port the handler list, RECONCILE
  DEV-vs-retail handler divergences, then map entry + objdiff to confirm.
- **WRONG #2 (the helper ID).** `fn_8279B788` is **`Symbol::Symbol(const char*)`**
  (the static-`_NEW_STATIC_SYMBOL` interner — confirmed in w6-hashmap2.md:76),
  NOT a "HANDLE-dispatch helper / superclass". It appears once per first-time
  static-symbol init, which is exactly why its call-count = the handler count.
  Using it to "classify superclass" is wrong; use it to COUNT handlers.
- **WRONG #3 (attribution).** The `.s` filename is NOT the Handle's class. `Part.s`
  fn_82437110 = **RndParticleSys::Handle**; `MeshAnim.s` fn_82457428 =
  **RndMeshAnim::Handle**; `Rnd.s` fn_824006A8 (1604 instr) = **Rnd::Handle** (the
  giant), distinct from the already-paired ModalKeyListener::Handle (fn_823FEDE0).
  Every map entry is an attribution claim → set attribution_risk=true.

## The decisive measurement (handler-count divergence)

Per-handler "static-symbol-init" sites = count of `bl fn_8279B788` in the target
function body = count of `HANDLE/HANDLE_EXPR/HANDLE_ACTION/HANDLE_ACTION_IF*`
directives in source. Equal count is NECESSARY (not sufficient) for a flip.

| Handle (real class) | owner .s / VA | TARGET sites | DC3 oracle | rb3-Wii oracle | our src | route |
|---|---|---|---|---|---|---|
| UIList::Handle | UIList fn_827D6EB0 (783i) | **28** | 29 ✗ | — | 29 ✗ | TRIM 1 (src exists) |
| RndParticleSys::Handle | Part fn_82437110 (893i) | **26** | 26 ✓ | (RndParticleSys) | absent | PORT from DC3 (count matches) |
| RndMeshAnim::Handle | MeshAnim fn_82457428 (630i) | **18** | 3 ✗ | (RndMeshAnim) | absent | DC3 false-friend; reconstruct/Wii |
| MidiParser::Handle | MidiParser fn_827C3C10 (594i) | **18** | 19 ✗ | 18 ✓ | 19 ✗ | PORT from rb3-Wii (count matches) |
| BandWardrobe::Handle | BandWardrobe fn_823202E0 (486i) | **15** | — | 18 ✗ | 18 ✗ | TRIM 3 DEV-only handlers |
| AsyncFileHolmes::Handle | AsyncFileHolmes fn_82525CE0 (891i) | **7** | — | (AsyncFileHolmes.cpp) | absent | PORT (verify class) |

Takeaways:
- **No single oracle is authoritative.** DC3 wins Part (26=26); rb3-Wii wins
  MidiParser (18=18); NEITHER matches UIList/MeshAnim/BandWardrobe. Per-TU you must
  pick the oracle whose count matches the target, then verify the ORDER too.
- **The divergence is real game/engine DEV-vs-retail drift**, not a macro artifact.
  BandWardrobe's rb3-Wii oracle has 3 extra handlers (`enable_debug_interests`,
  `list_interest_objects`, `sync_interests` are debug-build-only candidates) +
  `HANDLE_CHECK(0x73A)` debug tail; retail stripped them. UIList source has 1 extra
  vs retail. These are the classic "false friend" trims.

## Prereq state (HARD dependency — confirmed not on main)

main @812e1df still emits the unmodified macros: `MessageTimer timer(...)` in
BEGIN_HANDLERS (Object.h:928) and the sizeof-stripping `MILO_NOTIFY` END_HANDLERS
tail (Object.h:1032). The large Handles read low until the prereq lands.

Two prereq worktrees exist:
- `wt-w9-land-global-begin-handlers-timer-drop` @66be8b2 — **8453 matched (+139)**,
  the clean landable prereq ("BEGIN_HANDLERS: drop debug-only MessageTimer +
  UIComponent resource handlers"). END_HANDLERS keeps `MILO_NOTIFY(...)` and relies
  on per-TU `#define MILO_NOTIFY(...) ((void)(__VA_ARGS__))` overrides (UIComponent.cpp:369 pattern).
- `wt-w9-spike-strategyB-global-end-handlers-pathname-tail` @51fc194 — 8502 (+188),
  the global END_HANDLERS rewrite form + 6 paired small Handles.

The coordinator must land ONE reconciled prereq FIRST (L4 recommends keeping the
per-TU timer-restore TUs). This L5 wave branches off the prereq tip.

## Pin / wired status — ALL owner TUs already pinned (no splits edits)

Verified each candidate VA ∈ its owner TU's pinned `.text` span:
- UIList fn_827D6EB0 ∈ [0x827D2900, 0x827D8D48)
- Part fn_82437110 ∈ [0x82433730, 0x82439D8C)
- MidiParser fn_827C3C10 ∈ [0x827C1218, 0x827C5E38)
- MeshAnim fn_82457428 ∈ [0x82456970, 0x8245E1D0)
- AsyncFileHolmes fn_82525CE0 ∈ [0x825221D0, 0x82527920)
- BandWardrobe (src/system/bandobj/BandWardrobe.cpp) — pinned
- PlatformMgr/PhysicsVolume — pinned

So the per-TU work is: source handler-list port/reconcile + ONE
`scripts/target_symbol_map.json` entry + objdiff confirm. NO relocation/sliver-pin.
attribution_risk=true because the map entry is an attribution claim (class ≠ filename).

## How to execute ONE large-Handle TU (cold recipe, per item)

1. Branch a worktree off the reconciled prereq tip (`scripts/setup_worktree.sh`).
2. Identify the REAL class: the owner `.s` fn is a Handle of SOME class. Resolve
   via the oracle whose `Class::Handle` produces the matching static-sym count
   (`grep -c fn_8279B788` in the target fn vs HANDLE-directive count). Confirm the
   handler SYMBOL ORDER matches (decode target interned-string lbls vs source
   `HANDLE(name,...)` order).
3. Port the handler body from that oracle into our source (DC3 for engine TUs,
   rb3-Wii for game TUs — but pick per-TU by count match, both are false friends).
4. RECONCILE: trim DEV-only handlers retail lacks (the −1/−3 deltas), or add ones
   the oracle dropped. The retail static-sym count is the ground truth.
5. Add the `?Handle@RealClass@@UAA?AVDataNode@@PAVDataArray@@_N@Z` map entry at the
   VA. Force renamer: `rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml`.
6. objdiff the function to TRUE byte-exact 100% normalized.
7. Whole-binary A/B (VERIFY COMMAND); honesty gate; land independently vs prereq tip.

## Sequencing & EV

Strictly AFTER (a) the prereq lands and (b) the small/mid L4 tiers drain. Honest
per-TU EV is +1 (just the Handle) UNLESS the TU is UNPORTED (Part/MeshAnim/
AsyncFileHolmes — porting the whole Handle may reveal-cascade sibling fns). The
giant Rnd::Handle (1604i) is the single highest payoff but highest cost (96 source
lines of handlers in rb3-Wii Rnd.cpp:949). Realistic aggregate for the tractable
subset (count-matching oracle exists): UIList +1, RndParticleSys +1, MidiParser +1,
BandWardrobe +1, plus reveal spillover ≈ +8..+15 across the 47, not +20 uniform.

## Discovered adjacent leads (seed later layers)

- **Census heuristic has false positives** — the END_HANDLERS-shape grep matched
  non-Handle bodies (SongMgr fn_82784830 = SaveCachedSongInfo, 0x458 body, NOT a
  Handle; w6-hashmap2.md:223). Re-validate the 47 by confirming a real
  BEGIN_HANDLERS head (Sym(1) extract + sym-compare chain) before pairing.
- **fn_824006A8 (Rnd.s, 1604i) class-ID** is itself an open puzzle: it is large
  enough to be Rnd::Handle but the Rnd cluster has many Handle methods
  (ModalKeyListener already paired at fn_823FEDE0). Decode its super-bl + interned
  strings against rb3-Wii Rnd.cpp:949 to confirm before pairing.
- **PlatformMgr::Handle (738i, DC3=46 sites)** — DC3 has a huge 46-handler list;
  needs its own target-count + order audit (likely RB3-retail-trimmed; PlatformMgr
  is platform-specific so RB3 may diverge heavily from DC3).
- **Oracle-count-mismatch TUs (UIList −1, BandWardrobe −3, MeshAnim DC3=3-vs-18)**
  are a reusable "DEV-vs-retail handler trim" sub-pattern — could be batch-audited
  by diffing target static-sym count vs source for ALL wired BEGIN_HANDLERS TUs to
  pre-rank which large Handles are 1-line trims (cheap) vs full ports (expensive).
