# Near-miss harvest — evaluation-order sculpting waves

Playbook for pushing named, real-bodied near-misses (96–99.99% fuzzy) to
byte-exact 100 via **source-shape edits**, run as parallel subagent lanes.
Proven over 4 rounds (2026-07-01 → 2026-07-03): **+28 strict landed, 0 net
regressions** (round-1 +3, round-2 +5, round-3 +9, round-4 +11). Hit rate per
attempted candidate ≈ 60–75% once walls are filtered out.

Context: the 2026-06-30 codegen-tooling investigation (verdict **KILL**, see
`docs/decomp/research/2026-06-30-nearmiss-codegen-inventory.md`) proved the
99%-band fine-regalloc residuals are compiler fixed points no permuter reaches.
What *does* work is this playbook's opposite bet: the sub-99.9 band is mostly
**wrong source shape** (evaluation order, DC3-drift, type width), not compiler
noise — and a subagent reading the 3–10 instruction residual can usually
reconstruct the retail shape in ≤5 tries.

## 1. Candidate selection

Regenerate the pool fresh from the current `build/45410914/report.json` every
round — main moves fast and stale lists waste lanes:

```python
import json
r = json.load(open('build/45410914/report.json'))
cands = []
for u in r['units']:
    for f in u.get('functions', []):
        m = f.get('metadata', {})
        pct = f.get('match_percent_normalized', f.get('fuzzy_match_percent', 0))
        name = m.get('demangled_name') or f.get('name','')
        if 96.0 <= pct < 99.999 and f.get('size',0) > 44 and not name.startswith('fn_'):
            cands.append({'pct': round(pct,5), 'size': f['size'],
                          'unit': u['name'], 'sym': f['name']})
cands.sort(key=lambda c: -c['pct'])
```

Filters, in order:
- **`size > 44`** — ≤44-byte entries are funclet/thunk artifacts (1096 of the
  1811 near-misses in the 06-30 inventory were these). Not harvestable here.
- **Named only** (no `fn_8XXXXXXX`) — you need an oracle body to compare.
- **Skip STLport/template instantiations** — those close via header/layout
  campaigns, not body sculpting.
- **Skip the known walls** (§5) and anything a previous round marked at-limit
  in `docs/decomp/plans/post-codegen-kill-streams-2026-06-30.md` RESULTS
  sections or the roadmap doc.
- Sweet spot is **96–99.5%** with size 100–1500 bytes: big enough to have a
  real shape divergence, small enough to reason about. The 99.9x band is worth
  a lane only when the residual is 1–3 instructions of an obviously
  source-reachable kind (compare signedness, load width, extra `mr`).

## 2. Lane discipline (the rule that keeps waves clean)

**Local `.cpp` edits ONLY. No header edits inside a wave. Ever.**

Round-3 allowed header edits: 3 shared-header changes → net +32 but **23
strict regressions** (vtable-slot shifts, layout cascades, funclet stubs), a
full bisection, and a preserved-branch cleanup. Round-4 enforced
local-.cpp-only: 12 wins, exactly **1** regression, and it was a same-TU
funclet coupling — trivially attributed and dropped.

When a lane discovers the true fix is a header change (member drift, virtual
slot, default arg), it must **report the header-need as a finding** — TU,
member, evidence — and move on. The coordinator queues it as its own
A/B-gated single change (see `hasreal-grind.md` / the struct campaigns).

Other lane rules:
- One shared worktree per wave (`scripts/setup_worktree.sh ~/tmp/wt-<wave>`),
  lanes partitioned by TU so they never touch the same file.
- **Worktree PCH trap:** the reflinked PCH embeds main-repo paths; the first
  real recompile of a PCH-eligible TU (most of `src/system`) fails with
  double-include / type-redefinition errors. Fix once per worktree:
  `touch src/system/decomp_pch.cpp && ./tools/ninja-locked build/45410914/pch/system.pch`.
- **Always pass `project_dir=<worktree>` to `run_objdiff`/`run_analyze_function`**
  — without it the tool builds main and your edits are invisible (reads as a
  mysterious frozen %).
- Verify per-fn with `run_objdiff(symbol, project_dir=wt)` after each shape
  try; `full_listing=true, concise=false` when you need dataflow context
  around an isolated mismatch.
- ≤5 shape attempts per function, then bank the diagnosis and move on. The
  wall taxonomy (§5) exists so lanes don't burn budget re-proving cliffs.
- A lane's deliverable is: per-fn verdict (100 / at-limit+why / header-need),
  the exact edit, and the residual diagnosis for non-wins.

## 3. Technique catalog (what actually closes functions)

The core move: read the residual diff, then reconstruct **retail's evaluation
order** with local statement surgery. MSVC `/O1` schedules loads/stores in
source-encounter order more than you'd expect; most 97–99.9% misses are the
source computing the right values in the wrong order.

Ordering / CSE levers:
- **Hoist a global/member read into a local before a store** — retail read it
  early into a callee-saved reg (e.g. `unsigned short rev = gRev;` before the
  `mColors[1]` store in `OutfitPiece>>`).
- **Introduce (or remove) a local CSE** — a named temp forces one load reused;
  inlining the expression forces two loads. Match whichever retail did.
- **`T* p = x->field;` hoisted above a call** keeps the pointer in a
  callee-saved register across the call, matching retail's reload-free tail.
- **Split a pointer chain** (`a->b->c` → `B* b = a->b;` then `b->c`) or inline
  it — controls the number and order of dependent loads.
- **Single-assign members** — `m = 0; … m = x;` leaves a dead store/`mr`;
  collapse to one assignment (setters are common offenders).
- **Inline dead `auto`/named temps** that leave stray `mr` copies.
- **Opaque-pointer store to block CSE** — `float* p = &m.y; *p = v;` prevents
  the compiler folding an adjacent `m.x` load across the store when retail
  reloads it.

CSE / call-shape levers (round-5 additions):
- **Syntax-form CSE defeat** — MSVC /O1's CSE is *syntax-form-sensitive*: when
  retail computes a value twice (two `slwi`), mix the forms (`x * 8` in one
  spot, `x << 3` in the other) to force both computations (SHA1::Update).
- **Qualified devirtualized call** — `MoggClip::Stop(0);` (explicitly
  qualified) inlines the virtual's body in place, reproducing retail's
  this-adjust + frame shape; the unqualified call emits a vcall.
- **Separate file statics vs struct members** — statics are leaf
  section-relative addresses scheduled *before* `li` arg setup; struct-member
  addresses are derived arithmetic scheduled *after*. Retail config blobs that
  look like a struct may be N separate statics (MicManagerXbox).
- **Address-shape for indexed byte access** — keep `base + 4` as its own
  pointer and index with the *variable* (`rowPtr[pixelY]` → `lbzx`) instead of
  folding to a constant displacement (`lbz 0x4`) (DecodeDxtColor).
- **Ctor-arg-order diagnosis** — when a 2-int ctor's args could be swapped,
  retail's `(bool)` normalization (`subic`/`subfe`) pins which position the
  bool-typed value occupies (UsbMidiGuitar RG messages).

Type-width / instruction-selection levers:
- **`(unsigned int)` cast on an index bounds check** — `(unsigned)idx >=
  vec.size()` flips signed `cmpw` → retail's unsigned `cmplw` (closed the
  entire LightPreset PropSync ×4 family in one edit).
- **`unsigned int` char-loop temp** reproduces retail `cmplwi` where
  `strcpy`-style code emits signed `extsb.`.
- **`(int)` cast on a pointer null-check** flips unsigned `cmplwi` → signed
  `cmpwi` (and vice versa by removing it).
- **Per-TU `#undef HANDLE_MESSAGE` + redefine with `(unsigned char)` on the
  OnMsg return** reproduces retail's bool truncation (`clrlwi r3,r3,24`).

Layout / linkage levers (still TU-local):
- **Explicit `= 0` initializer on a global** moves it `.bss` → `.data`,
  changing intra-TU data ordering (retail anchor-displacement fixes).
- **Function-local `static Symbol`/`static Message`** forces a method
  out-of-line when retail has a real body but ours inlines away
  (`MusicLibrary::PushHeaderDataToScreen`). Pair with a
  `scripts/target_symbol_map.json` entry or the pin reads a false 0%.

DC3-drift reverts (the most common *root cause*):
- dc3-decomp is **newer** than retail RB3 — it added members, params, default
  args, virtuals. When the residual smells structural, diff our source against
  `lookup_rb3wii` first; reverting to the Wii-era shape (fewer args, no extra
  member touch, different callee) is the fix. TU-local drift reverts are fair
  game in-wave; header-level drift goes through the header-need channel (§2).

## 4. Measurement + landing protocol (non-negotiable)

1. All lanes report → coordinator reviews edits in the worktree.
2. **Composed whole-binary A/B, run TWICE** (determinism gate run1 == run2):
   ```bash
   setsid nohup scripts/harvest/ab_supervise.sh <worktree> <log> & disown
   ```
   (the supervisor does `rm -f build/45410914/target_symbol_renames.stamp;
   touch config/45410914/config.yml; NINJA_JOBS=6 tools/fresh_report.sh` in a
   retry loop — external pkill sweeps kill long builds silently; detach +
   marker-based monitoring, never PID-based).
3. `scripts/harvest/measure_delta.py <base.json> <new.json>` — VERDICT WIN
   requires net > 0 **and** 0 strict regressions **and** 0 fuzzy regressions
   (fuzzy drop > 1.0%).
4. **Regression? Bisect cheap before rebuilding**: incremental per-symbol
   objdiff on the touched objs attributes a regression to a specific edit in
   minutes (round-3: reverting the devirt returned `Character::EnableBlinks`
   to 100 — confirmed attribution without a full A/B). Drop the guilty edit,
   re-run the composed A/B.
5. `tools/icf_alias_check.py` — HONEST required (no ≤44-byte stub-fold
   inflation counted as wins).
6. Land via cherry-pick/`scripts/harvest/land.sh` onto main; never mutate main
   directly. After landing: `python3 configure.py` and grep the build for
   `Missing configuration for` on your TUs.
7. **Preserve, don't delete, mixed results** (owner directive): net-positive
   batches with coupled regressions go on a `followup/<wave>` branch + a
   handoff doc (`docs/decomp/handoff/round3-shared-header-followups-2026-07-02.md`
   is the template). Round-3's preserved branch became the owner's +25 land.
8. Update `docs/decomp/plans/post-codegen-kill-streams-2026-06-30.md` (or the
   current roadmap) with a RESULTS(date) block: wins, walls hit, new
   techniques, follow-ups.

## 5. Wall taxonomy (do NOT burn lane budget on these)

Source-unreachable compiler behavior, proven across rounds:

| Wall | Signature | Verdict |
|---|---|---|
| `lfd` vs `lfs` zero | retail loads double-0.0, ours float-0.0; any compare rewrite reschedules the whole FP DAG | at-limit (CalcScale, Waypoint::ShapeDeltaBox) |
| Volatile-reg alloc cycle | GPR/FPR volatile assignment permutes but count is a fixed point | at-limit (SkinVertex 97.5) |
| u8-narrowing mask | compiler value-range propagation elides/adds `clrlwi` | at-limit |
| CSE-reload | retail reloads a value our CSE folds, and no opaque-store shape blocks it | at-limit (BinStream::Write) |
| memcpy dst-hoist | retail hoists the dst address into a temp inside the intrinsic expansion | at-limit (BlockMgr::AddTask) |
| Funclet stack-slot coupling | body hits 100 but the fix's extra locals shift an EH-cleanup funclet's temp slot | near-win, needs the exact retail shape with no frame growth (MidiParser::AddMessage) |
| strcpy `extsb.` | retail's inline strcpy uses unsigned compare; ours signed — only closes when the loop is open-coded (the `unsigned int` temp lever) | try lever once, else at-limit |

Known at-limit individuals: `SkinVertex`, `BinStream::Write`,
`BlockMgr::AddTask`, `CalcScale`, `Waypoint::ShapeDeltaBox`,
`Geo::CheckBSPTree`, `MidiParser::PushIdle`, `InitMakeString`.

## 6. Wave shape that works

- **4 Fable lanes × ~4 candidates** each, one wave = one worktree = one
  composed A/B. Fable subagents have a high hit rate on per-fn residual
  reading; the constraint that matters is lane discipline, not model effort.
- Sort candidates into lanes by directory so each lane builds oracle context
  once (synth lane, bandobj lane, meta_band lane, …).
- Prompt must include: the §2 rules verbatim, the §3 catalog, the §5 walls,
  the worktree path, and the `project_dir` warning.
