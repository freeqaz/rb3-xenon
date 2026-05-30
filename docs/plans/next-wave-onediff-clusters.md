# Next-wave one-diff clusters (2026-05-29, main @1904)

Read-only survey of the post-1904 near-miss queue (`build/45410914/report.json`),
**excluding units owned by active worktrees** (CamShot/Band\*, LightPreset,
Shader/NgStats files, DepthBuffer3D, Part, MemMgr/MemTracker family). These are
clusters where *most/all* near-misses are at **99%+** — i.e. one or two
instruction diffs from matching — signalling a **single shared per-class cause**
(usually a struct-member-offset skew). Each cluster = one focused worktree agent,
net-positive-or-revert gate, orchestrator cherry-picks.

Diff class is read via:
`bin/objdiff-cli diff -p . -u default/<UNIT> '<MANGLED>' -c functionRelocDiffs=none -f json --include-instructions`

## Cluster ranking (unowned, real-named near-misses 50–99.99%)

| near | @99%+ | unit | diagnosis |
|---|---|---|---|
| 6 | **6** | **CharFaceServo** | **DIAGNOSED below — one +8 member.** Highest EV. |
| 6 | 5 | Instance (WorldInstance/SharedGroup) | SharedGroup ctor 86.9% is the real target; 5 others already @99.9% — likely ObjPtr/RndGroup MI residue |
| 6 | 4 | MemcardMgr_Xbox | TBD |
| 6 | 3 | MidiInstrument | TBD |
| 5 | 3 | system/rndobj/CubeTex | TBD (rndobj — check vs RndTex base) |
| 4 | 4 | UISlider | TBD (UIComponent base?) |
| 4 | 2 | Cache_Xbox | TBD |
| 3 | 3 | TexRenderer (RndTexRenderer) | TBD — InitTexture 99.8% + 3 funclets |
| 3 | 3 | MemStream | WriteImpl/Compact/ctor all @99.8% — small, self-contained |
| 3 | 2 | FileStream | TBD |
| 3 | 1 | LightHue | mKeys +4 residue (post-String) — see baseclass doc |

## CharFaceServo — ⚠️ DIAGNOSIS OBSOLETE (2026-05-30): 6 fns ALREADY MATCHED

**The +8 below was closed by the ObjPtr=0xc landing.** All 6 named fns
(ApplyProceduralWeights, SetClips, SetClipType, TryScaleDown, Copy, fn_82390144)
are now fuzzy=100, counted in matched_functions. The current residual is a
DIFFERENT problem: 7 MI-adjustor thunks (`fn_823901B4/8239053C/82390568/82390594/
823905C0/823905EC/82390618`, 99.8) with a uniform **+0x10** from the CharBones
**`virtual Hmx::Object` diamond** (CharFaceServo's own members already match — do
NOT touch the header). Full writeup: `objptr-family-relayout-migration.md` §13.3.
The original +8 diagnosis is retained below only for historical context.

## CharFaceServo — (HISTORICAL) FULLY DIAGNOSED

Unit `default/CharFaceServo`, header `src/system/char/CharFaceServo.h`. All 6
named near-misses (`ApplyProceduralWeights`, `SetClips`, `SetClipType`,
`TryScaleDown`, `Copy`, + `fn_82390144`) share ONE cause.

**Evidence** (`ApplyProceduralWeights` @99.81%, 216 bytes — every diff is
`diff_arg` immediate, +8):

| target offset | base offset | access |
|---|---|---|
| `0xa8` | `0xb0` | `lwz` (ptr member) |
| `0xc0` | `0xc8` | `lwz` (ptr member) |
| `0xd0` | `0xd8` | `lfs` (float) |
| `0xd4` | `0xdc` | `lfs` (float) |
| `0xdc` | `0xe4` | `lfs` (float) |
| `0xe0` | `0xe8` | `lbz`/`stb` (bool/byte) |

All member accesses **≥ ~0xa8 are +8 in our build**; everything below (`0x38`
via the clip ptr, `0x8`, `0x0`) matches. So **exactly one member at/before 0xa8
is 8 bytes too big** in our `CharFaceServo` layout, shifting everything after it.

**The `// 0xNN` annotations in `CharFaceServo.h` are STALE** — written when
`ObjPtr` was 0x14 (pre-`f09aab3` keystone). The header shows two ObjPtrs before
the divergence: `mClips ObjPtr<ObjectDir> @0x7c` and `mBaseClip ObjPtr<CharClip>
@0x94`. With `ObjPtr` now 0xc, the real offsets differ from the comments. The
+8 culprit is one member in the `0x7c–0xa8` window that is still 8 bytes larger
than retail.

**Fix recipe for the agent:**
1. Worktree off main. Build, confirm CharFaceServo baseline.
2. Get the ACTUAL current layout: read `build/45410914/asm/CharFaceServo.s` for
   the base-side store offsets in the ctor, OR `tools/struct_db.py`. Compare
   member-by-member against the target offsets above.
3. The retail target wants the first post-`mBaseClip` member (a ptr) at `0xa8`.
   Determine what member is 8 too big — candidates: an ObjPtr that didn't pick
   up 0xc here, a `Symbol`-vs-`String` typo (String 0xc vs Symbol 0x4 = +8), or
   a DC3-only member. Cross-check `~/code/milohax/rb3/src` (rb3-Wii has named
   `CharFaceServo`) and `~/code/milohax/dc3-decomp/src`.
4. Shrink/retype/remove the offending member; rebuild; verify all 6 fns close
   and `measures.matched_functions` rises. Net-positive-or-revert.

Likely a **+6** flip (all 6 fns), possibly more via callers. Confidence HIGH
(the +8 is bit-stable across 7 accesses in one function).

## Notes
- Instance/SharedGroup: **DIAGNOSED (2026-05-30) — see `objptr-family-relayout-migration.md`
  §13.1.** The lever is `mSharedGroup` retail@0x148 vs ours@0x140 (+8), which decomposes
  into TWO offsetting bugs: (1) **ObjDirPtr 0x10→0xc** (drop injected `mOwner`; `operator=`
  `fn_824D77D0` proves `mObject@4,mLoader@8`) and (2) an **unisolated RndDir +0xc base
  deficit** (retail RndDir is 0xc bigger; headers match DC3 yet compiled offset differs).
  Both must land together for Group to flip. Not a standalone cluster — it's a bug-#1 P4 tail.
- MemStream + FileStream + TexRenderer are small, self-contained, low-risk —
  good for batching into one agent.
- LightHue `mKeys +4` may auto-resolve if the LightPreset agent's struct work
  touches shared Key types — re-measure after that lands.

## NEW LEVER — UIComponent base-layout reconstruction (from UISlider agent)

The UISlider agent (2026-05-29) found `UISlider::OnMsg(ButtonDownMsg)` (@75%) is
NOT a local fix — the source body is byte-identical to DC3's 100%-matching
version. It's blocked behind a **foundational UIComponent base-class layout
divergence**: retail's UIComponent base is **~252 bytes (0xFC) + ~9 vtable slots
LARGER** than DC3's.

| item | DC3 binary | our build | **retail target** |
|---|---|---|---|
| `mCurrent` | 0x80 | 0x70 | **0x14c** |
| `mNumSteps` | 0x84 | 0x74 | **0x150** |
| `mVertical` | 0x88 | 0x78 | **0x154** |
| ScrollSelect base | 0x44 | 0x34 | **0x140** |
| SetCurrent vtable slot | 0x2c | 0x2c | **0x50 (+9 slots)** |

Evidence: retail ctor `fn_827E47F0` lays the ScrollSelect vptr at 0x140 and a
2nd vptr at 0x188 with member refs to 0x190 → retail UIComponent footprint
~0x140 vs DC3 ~0x44. **Opposite polarity to `0d206a3`** (UIListProvider, where
DC3 had EXTRA slots retail lacked) — here retail has many extra slots/bytes DC3
lacks, so UIComponent genuinely needs first-class retail reconstruction.

**Blast radius: 15 UI units / 79 matched functions.** **UPDATE (2026-05-29): the
full retail layout is now RECONSTRUCTED + verified — see the dedicated doc
`docs/plans/ui-base-layout-reconstruction.md`.** Critical correction: the layout
fix alone registers **+0** (UI units lack pinned `.text` splits AND need body
ports). The "closes for free" premise is FALSE — it's a **3-step effort**
(land layout → pin splits → port bodies). Do the ObjPtr migration first (UI uses
ObjPtr/ObjDirPtr members). Another instance of `project-engine-baseclass-layout-wall`.

## Open leads (found in passing, not yet actioned)
- ~~**MeshAnim `mVertColorsKeys` element-type divergence**~~ **— RESOLVED INVALID
  (2026-05-30 agent).** Our `Keys<std::vector<Hmx::Color>>` (16-byte, `srawi 4`) is
  **already correct**; DC3's `MeshAnim.h` is byte-identical. The `srawi 3` (÷8) the
  lead pointed at is the **VertTexs `Vector2`** path, NOT colors — proven via Ghidra:
  the 16-byte colors `Print` loop (target `0x8245A370`) calls per-element printer
  `0x824E22F8` reading **4 floats** (`Hmx::Color`); the 8-byte loop (`0x8245A2C0`)
  calls `0x824DB360` reading **2 floats** (`Vector2`). rb3-Wii's `Color32` (4-byte)
  is the **Wii GX platform outlier**, not the 360 target. Retyping to Color32 would
  REGRESS (wrong ÷4 stride + member-offset shift). DO NOT pursue. The **real**
  MeshAnim near-miss blockers (separate tasks): (1) **dynamic-init guard bit-clears**
  on the `SetType@RndMeshAnim` once-flag (`fn_82457ED8/82458160/824580D0`: target
  `clrrwi`/`rlwinm 0,N,M` vs our mask) → **guard/dynamic-init patcher** territory
  (`scripts/`, dc3 `configure.py:301-357`), not source; (2) **RndMeshAnim/RndMesh
  class-layout delta** — dtors `fn_82458138/82458180` show target frame `0x140` +
  member `+0x88` vs our `0x80`/`+0x94` destructing a `_Vector_base<Vector3>` → a real
  layout task, distinct from the color element type.
- **DancerSequence** RB3-extra-member TU reconstruction (task #29 writeup): pin the
  full `DancerSequence.cpp` TU, derive ~9 scalar members from `Load fn_824847A8`
  (reads `this+0x1c..0x3c`), then the dtor cluster flips.

## Refs
- `docs/plans/recon-structural-levers-2026-05-29.md` (Levers 1–5)
- `docs/plans/engine-baseclass-layout-bugs.md` (the 7 base-class bugs)
- objdiff CLI shape above; orchestrator MCP `run_objdiff` is the wrapper.
