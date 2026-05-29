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

## CharFaceServo — FULLY DIAGNOSED (do this first)

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
- Instance/SharedGroup: the `SharedGroup::SharedGroup(RndGroup*)` ctor @86.9% is
  the lever; the 5 @99.9% (TryPoll/GetDistanceToPlane/Poll/AddPolls/PropSync)
  hang off SharedGroup/WorldInstance layout. Related to RndGroup MI (baseclass
  doc bug #3) — verify the `mGroup+0x34` offset is now correct post-`f09aab3`.
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

## Refs
- `docs/plans/recon-structural-levers-2026-05-29.md` (Levers 1–5)
- `docs/plans/engine-baseclass-layout-bugs.md` (the 7 base-class bugs)
- objdiff CLI shape above; orchestrator MCP `run_objdiff` is the wrapper.
