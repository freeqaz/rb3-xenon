# Spill-store leverage campaign (2026-07-10)

Turning the cracked spill-store-count mechanism into fleet-wide matches: scan
tooling, per-class harvest levers, and a header-trial experiment track.

**Prerequisite reading** (this doc assumes them; do not re-derive):

- `docs/decomp/research/2026-07-10-spill-store-homing-mechanism.md` — the
  canonical three-way mechanism split (address-taken writes / EH-conservative
  homing / true register-pressure spill), proven minimal pairs, the FaderGroup
  specimen, and the direction-inversion tooling trap.
- `docs/decomp/MSVC_X360_REGALLOC.md` — c2.dll coloring chain (declaration
  order → symbol IDs → coloring order → register assignment); scopes what is
  deterministic vs intractable.
- `/home/free/tmp/fg_dtor2_findings.md` + `/home/free/tmp/spillrepro/` —
  scratch specimens (10-shape S-table, `cc.sh` one-off production-flag
  compiler, minimal pairs `at1..at4`, `eh0..eh2`).
- `docs/decomp/patterns/unfixable-compiler.md` "Stack Spill Scheduling" — the
  legacy monolithic bucket this campaign is splitting.

## 1. The mechanisms (operational summary)

For any near-miss whose diff involves an extra/missing `stw rN, slot(r31)`
(or r30/r1 frame base), classify by how the **retail** side uses the slot:

| Class | Retail slot usage | Mechanism | Lever |
|---|---|---|---|
| **A — address-taken** | `addi rX, r31, slot` exists, or slot is reloaded (`lwz`) | One `stw` per source write surviving DSE; writes separated by an opaque call cannot fold (escaped address) | **Source-fixable now.** Match retail's write count/placement: add/remove/move a write across the call boundary. Proven S=1→S=2 minimal pair. |
| **B — EH homing** | Write-only, never reloaded, funclet doesn't read it, function has EH (`__ehfuncinfo$`), value is an inlined container node/iterator | EH-conservative homing of an EH-tracked temp across a throwing call; S = number of independent node-materializing expressions | **Quantized.** Body shape moves S in steps (0, 2, ...); some retail values (e.g. S=1 via value-CSE) are only reachable through shared-header inlining changes → the header-trial track. |
| **C — pressure spill** | Write-only, no EH temp involved, high register pressure | Defensive spill by the allocator | **Unfixable.** Don't chase (per REGALLOC doc). Value of the scan: *positively identifying* these stops agents from burning cycles. |

Diagnostic order matters: check for `addi`/reload first (A), then EH context
(B), else C. The classification is mechanical — that's why it should be a tool,
not an agent judgment call.

**Tooling trap to bake in everywhere:** verify objdiff target/base orientation
against the dtk asm (`build/45410914/asm/<unit>.s`, anonymous `fn_` callees =
target/retail) before reasoning about which side has the "extra" store. The
FaderGroup first pass had it backwards.

## 2. Threads

### T1 — Signature scanner (`scripts/harvest/spill_signature_scan.py`)

Batch classifier over the near-miss pool. Template:
`scripts/harvest/offset_drift_sweep.py` (same skeleton: pool JSON in →
`diff_inspect.run_objdiff_for_symbol` per function → per-function record out,
`--resume` support).

Per function:

1. Run objdiff, collect insert/delete instructions.
2. **Signature gate:** the diff's insert/delete set is dominated by
   store instructions to a frame slot (`stw/stfs/stfd/stb/sth rN, imm(r31|r30|r1)`).
   Record purity: `sole_diff` (nothing else differs), `dominant`
   (≥80% of indels), `present` (any).
3. For each implicated slot, classify A/B/C by scanning the **retail** asm
   side for `addi rX, rBase, slot` / reload `lwz` on the same slot, and for
   EH markers (funclet symbols, `__ehfuncinfo$`, throwing calls adjacent to
   the store).
4. Emit JSON: `{sym, unit, pct, size, purity, direction (ours-extra |
   ours-missing), slots: [{off, class, evidence}], verdict}`.

Output feeds both harvest fan-out (class A) and the sizing decision (class B).

### T2 — Class-A harvest (proven lever, immediate)

For every class-A hit: an agent matches retail's write count/placement in the
source (writes on the correct side of the opaque call, hoist single-assignment
loop-invariant words). Standard worktree flow (`scripts/setup_worktree.sh`
under `~/tmp`), objdiff verify, patch back to main, path-limited commit.
Expected to be a modest but clean vein — every hit is a real strict candidate.

### T3 — Class-B sizing → header-trial campaign

The FaderGroup analysis showed the S-count for thin-node `ObjPtrList`
temps is set by shared-header phrasing (`obj/Object.h` / `obj/ObjPtr_p.h`
thin-node `front()`/`Obj()`/`iterator`/`pop_front()`), not the function body.
DC3 matches the identical body with a different node layout — the header is
the variable.

- **Sizing gate:** count class-B hits where the implicated temp is an
  ObjPtrList/thin-node value (callee `Unlink`, `ObjPtrList` types in the
  demangled name or adjacent calls). If ≥ ~10 functions share the signature,
  a header trial amortizes; if it's 1–2, stay parked (blast radius isn't paid
  for by one dead store).
- **Trial protocol (worktree-isolated, A/B-gated):** each trial = one candidate
  rephrasing of the thin-node accessors in a **fresh buildable worktree**
  (`setup_worktree.sh ~/tmp/wt-htrial-N`), full build, `tools/fresh_report.sh`,
  compare whole-binary strict + fuzzy vs baseline
  (`scripts/harvest/measure_delta.py` + manual fuzzy check — measure_delta
  gates strict only). **Hard gate: zero strict regressions** across the fleet
  (every ObjPtrList user, incl. 100% TUs like MasterAudio). Land only net-up.
- Candidate rephrasings to enumerate (each its own trial): single-read
  `mNodes` caching in `pop_front`, `front()` returning through a named local,
  iterator-mediated `front`, merged `front+pop` helper used by callers, and
  `Unlink` signature variations (value vs ref). The gated
  `RB3_FADERGROUP_DTOR_COALESCE` prototype (byte-identical result) already
  eliminated the erase-iterator-wrapper hypothesis — don't repeat it.

### T4 — Generalize the scan beyond `stw`-to-frame

Second-wave signatures once T1 works: extra/missing **reload** (`lwz` from a
frame slot) as the sole diff (the read side of the same homing decision);
extra/missing `mr` register copies adjacent to calls (coloring artifacts,
cross-ref REGALLOC doc); store *ordering* swaps within a block (scheduling,
likely class C). Each gets a purity/direction record in the same JSON shape so
downstream triage is uniform.

### T5 — Fold into the standing tooling & docs

- Split `docs/decomp/patterns/unfixable-compiler.md` "Stack Spill Scheduling"
  into the three-way table, referencing the research doc. (Was blocked on
  another agent's in-flight edits to that file — re-check `git status` first.)
- Add a `stw-classify` mode to `scripts/analysis/diff_inspect.py` (and thus
  the MCP `run_diff_inspect`) so any agent hitting a store-count mismatch gets
  the A/B/C verdict + lever mechanically, without re-reading the research doc.
- Record class-C hits into `scripts/harvest/nearmiss_verdicts.json` walls so
  the pool generator stops re-serving them.

## 3. Coarse roadmap

| Phase | What | Gate to next |
|---|---|---|
| 0 | This doc + research doc (done) | — |
| 1 | Build T1 scanner; smoke on `??1FaderGroup@@QAA@XZ` (must classify: sole-diff, ours-extra, slot 0x54 → class B; slot 0x50 → class A/agree) | Smoke passes |
| 2 | Full scan over the 85–99.99 pool (~1,700 fns; `gen_nearmiss_pool.py --min 85`); triage counts per class × purity | Scan JSON + triage table |
| 3a | Class-A harvest fan-out (worktree agents, patch-queue land) | Each fix objdiff-verified |
| 3b | Class-B sizing decision → header trials if ≥ ~10 shared-signature fns | Zero-strict-regression A/B per trial |
| 4 | T5 fold-in: pattern-doc split, `stw-classify` mode, verdict walls for class C | Committed, path-limited |
| 5 | T4 second-wave signatures if phase 2 shows adjacent populations | — |

Phases 1–2 run as one workflow (build → scan → triage); phase 3 fans out from
the triage JSON as a second workflow; phase 4 is small enough for a single
agent pass.

## 4. Artifacts / state

- Scan pool: `~/tmp/spill_pool.json`; scan output: `~/tmp/spill_scan.json`;
  triage summary: `~/tmp/spill_scan_triage.md` (scratch — durable results get
  folded back into this doc's §5 as they land).
- Memory: `project_spill_store_mechanism_2026-07-10.md` (mechanism),
  this campaign will get its own topic file at close.

## 5. Results log

*(append as phases complete)*

- 2026-07-10: campaign opened; phases 0–2 launched.
- 2026-07-10, phases 1–2 **complete**. Scanner landed (`6cd19a37` + mid-run
  fixes `d3b7d3c`: companion-row purity + per-side frame-base validation that
  killed 15 false hits). Full scan: **pool was 343 fns** (not ~1,700 — the
  §3 estimate counted raw near-misses before `gen_nearmiss_pool` filters:
  `fn_`-anonymous, ≤44 B, STL, verdict walls), 50 hits, zero scan failures.
  Direction calibration: **objdiff insert = ours-extra, delete = ours-missing**
  (re-verified on FaderGroup after every scanner change). Artifacts:
  `~/tmp/spill_scan.json`, `~/tmp/spill_scan_triage.md`.
  - **Class A (GO, small):** 1 sole + 3 dominant + ~20 present-band.
    Clean strict candidates: `GemPlayer::Handle` (99.86, sole-diff ours-extra
    4th subi/stw pair into address-taken 0x54) and `NetCacheMgr::OnInit`
    (98.5, near-clean ours-missing 0x60). Dominant trio
    (`SongUpgradeMgr::AddUpgradeData`, `RandomGroupSeqInst` ctor, `Hmx::Clip`)
    carry 12–22 other diffs — store lever alone won't close them.
  - **Class B header trial: NO-GO.** Thin-node ObjPtrList signature population
    = **1** (FaderGroup itself) vs the ≥10 gate. Parked stays parked —
    the sizing question is now answered with data, not judgment.
  - **Class C:** zero sole/dominant post-fix (the wall class barely exists at
    high purity); 19 present-band slots noted for T5 verdict walls.
  - **NEW LEAD (from the scanner's false-positive pile):** an ours-missing
    **member sentinel-init ctor family** — retail emits a self-pointing
    intrusive-list head init (`addi r11,r30,0x20; stw r11,0x20(r30)`) that we
    don't, across ~8 ctors/dtors (RndMorph, RndMultiMeshProxy,
    NgSpotlightDrawer, SharedGroup, FileStream dtor, CharDriver, CharIKFoot,
    RndAnimFilter). Smells like ONE shared-header inline-phrasing fix. This —
    not thin-node — is where the §T3 trial protocol gets used.
- 2026-07-10, phase 3 launched as workflow `spill-harvest-and-family-trial`:
  5 class-A fixers (worktree-isolated) + sentinel-family diagnosis/trial
  (fleet A/B gated) + merger (premise-checked, path-limited landings).
- 2026-07-10, phase 3a **landed: +5 strict, zero regressions** (15419→15424;
  commits `0c25ac0b` NetCacheMgr::OnInit, `96ad7a1f`
  SongUpgradeMgr::AddUpgradeData + bonus funclet, `638fbb49` GemPlayer::Handle,
  `4c428976` RandomGroupSeqInst ctor — all strict 100). Workflow was
  quota-paused mid-run; 4/5 fixers had finished, patches landed manually via
  an Opus merger. **New transferable levers from the fixes** (fold into the
  mechanism doc / pattern docs):
  - *Inlinee-`this` homing rule* (GemPlayer): MSVC homes an inlined member
    fn's `this` into a $T stack temp only when the inline instance is live
    across a call; a call-free arm phrased as a direct double-deref
    (`mA->mB = !mA->mB`) can spuriously trigger a dead home — route it
    through a named local (`T* m = mA; m->mB = !m->mB;`).
  - *Single-expression getter-before-call rule* (RandomGroupSeqInst): retail
    `f(x->Call() % container.size())`-style single expressions evaluate the
    inlined getter BEFORE the call, value-CSE it with later loads, and home
    the EH-tracked temp; splitting into named statements kills the temp/store.
    Signature: ours-missing write-only stw before a throwing bl whose slot
    doubles as a conversion temp.
  - *Wrapper-helper homing* (NetCacheMgr, SongUpgradeMgr): retail's source
    calls small same-class/same-TU inline helpers (`FindInt`, `HasUpgrade`);
    /Ob2 inlines them and homes the param copy into an address-taken slot.
    Ours-missing address-taken store near a find/lookup ⇒ look for a helper
    wrapper in the oracle instead of the direct expression.
  - SongUpgradeMgr also fixed a real behavior bug: only the `delete` of the
    old entry is guarded by `find != end`; the new/assign/MarkAvailable runs
    unconditionally.
- 2026-07-10, killed-agent salvage (Opus transcript review of the two
  quota-paused runs; full review `~/tmp/spill_incomplete_review.md`):
  - **Geo::Clip partial LANDED** (`73f2c74`): 95.1→97.6 fuzzy via a
    Clip-local `SubV2` static helper + commutative dot-term reorder;
    zero regressions (A/B vs post-landing baseline). Remaining: f12↔f13
    volatile regswap (permuter-class), Vector2 0x0/0x4 offset swap, and two
    class-B EH-homed `stfs` (quantized). Reported `stuck@97.6` to decomp.db;
    minimal-repro harness preserved at `~/tmp/geoclip_probe/`.
  - **Sentinel-family REDIAGNOSED — scanner's reading was wrong.** The
    "self-pointing sentinel init" is actually retail **inline-expanding a
    second `ObjPtr<T>` constructor we don't have**: retail's 360 header had
    BOTH `ObjPtr(Hmx::Object* owner)` (no AddRef → always /Ob2-inlined:
    3 field stores + vptr install + EH-home of &member) and
    `ObjPtr(Hmx::Object*, T*)` (branch+AddRef → never inlined). Proof: the
    same instantiation is inlined at RndMorph but called out-of-line at
    SampleZone — impossible under one phrasing; oracle-corroborated
    (rb3-Wii writes `mSample(obj, 0)` 2-arg vs 1-arg sites). Full mechanism
    doc: `docs/decomp/research/2026-07-10-objptr-two-ctor-inline.md`.
    Trial state: header split applied in preserved worktree
    `~/tmp/wt-sentinel-family` — **inline fires (mechanism confirmed) but
    expansion is scrambled** (RndMorph 90.5→66.9, wrong regalloc + store
    order). RESUMABLE, not landable: match RndMorph's store/vtable order
    first, then the fleet A/B gate. Population if cracked: **~20 fns at
    43–93%** (876 `??0?$ObjPtr` refs tree-wide). Spin-off TU-local lead:
    `FileStream::~FileStream` retail inlines trivial `DeleteChecksum()`.
