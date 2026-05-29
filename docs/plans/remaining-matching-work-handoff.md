# Remaining matching work — handoff (2026-05-29)

Snapshot for a fresh session to pick up. **Current: ~1920 matched / ~1.47%**
(HEAD moves under concurrent work — re-check `git log` + `build/45410914/report.json`
`measures.matched_functions` before starting). Milo Engine slice ~19% matched.

**How engine matching advances:** base-class / struct-layout fixes that *cascade*.
The source permuter is **low-yield** on the current ≥99% tail — most of it is
layout-blocked or scoring-gated, not codegen.

**Protocol for every fix:** isolated `scripts/setup_worktree.sh <path> <branch>`
worktree → edit → `./tools/ninja-locked 2>&1 | tee /tmp/rb3_build_<task>.log` →
`measures.matched_functions` must rise (else `git checkout` revert) → native
compile spot-check → land with a **path-limited** commit (`git commit -- <files>`,
never `git add -A` — the main tree is shared). **The binary is the arbiter**
(`build/45410914/asm/*.s`, Ghidra MCP on :8002, `bin/objdiff-cli`). **rb3-Wii
(`../rb3`) is the layout oracle, NOT DC3** — DC3 is newer and carries extra
members/virtuals retail lacks (see Pattern 1).

---

## Three reusable diagnostic patterns (all confirmed this session)

1. **DC3-added member RB3 lacks.** Our DC3-derived headers carry fields/virtuals
   retail (older) doesn't have → struct too big → uniform **+N** offset on every
   member access. Fix: `#ifdef HX_NATIVE`-guard the extra; X360 build drops it.
   Confirmed in: Rnd(`Watcher` 0x40), Rnd(PostProc-override ObjPtr+blacklight 0x60),
   ObjectDir(`unk8c` 4), RndDrawable(`mClipPlanes`), Object(`mSinks`),
   Loader(`mLoadCount`/`mLoadStartMs` 8), UIListProvider(`Handle`/`CanSelect` vtable
   slots), DepthBuffer3DAttachment(Vector3-widen + `unk20`, 0x10), RndFancyParticle
   (tile+momentum, 0x1c). **Direction of the skew has been wrong in agent briefs
   twice — RE the ctor first, don't assume.**

2. **Allocator debug-arg stripping.** Retail compiles out MemTrack `__FILE__`/line/
   name. Strip on the X360 path (`#ifdef HX_NATIVE` keeps the debug form). Tell: a
   near-miss emits `lis/addi` string-constant loads right before an alloc/free `bl`.
   Landed **+77** this session across POOL_OVERLOAD, the MemOrPool/STL dispatcher,
   and the MemFree/MemAlloc leaf (an arg-stripping macro in `MemMgr.h`).

3. **DC3-wrong-base / MWCC-vs-MSVC vbptr.** A class declared `: public Hmx::Object`
   (non-virtual) when retail derives a **virtual-base intermediate** (Object placed
   at the object *tail*), and/or a virtual whose signature mismatches an inherited
   virtual gets a **spurious extra vtable slot / vfptr**. Confirmed: `MidiParser`
   should be `: public MsgSource` (`MsgSource : virtual Hmx::Object`, Object vbase
   @0xB0); and `MsgSource::Replace(Hmx::Object*,Hmx::Object*)` was wrongly virtual
   (sig differs from inherited `ObjRefOwner::Replace(ObjRef*,...)`) → extra vfptr →
   `mSinks` +4. Made non-virtual → retail single-vbptr `MsgSource`=0x18.

---

## Remaining levers

### A. vbptr / DC3-wrong-base sweep  *(engine; novel — found via MidiParser)*
`MsgSource` base is fixed (commit `a847f4f`), correcting layout for all 24
MsgSource-derived classes. **But 23/24 are band3/network GAME** (Player, GameMode,
GameMicManager, GamePanel, BandMachineMgr, BandStorePanel, BandUI, ClosetMgr,
InputMgr, Matchmaker, MetaPerformer, OvershellPanel, ProfileMgr, SaveLoadManager,
SessionMgr, UIEventMgr, RockCentral, NetSession, Server, SessionSearcher,
VoiceChatMgr, WiiFriendMgr, WiiMessenger) → **game = fidelity grind, low near-term
yield** (see Don't-chase). Only `MidiParser` (engine) was actionable, and it's done.

**Sweep result (read-only recon — mostly EXHAUSTED for engine):**
- **Pattern (B) — mismatched-signature virtual → spurious extra vfptr — is fully
  resolved.** `MsgSource::Replace` was the *only* instance in the engine (a grep of
  the 2-arg `Replace(Hmx::Object*, Hmx::Object*)` shape across system/band3/network
  finds no other). Nothing left to chase here.
- **Pattern (A) — wrong non-virtual `: public Hmx::Object`: the only remaining
  ENGINE lead is `MidiInstrument`** (`src/system/synth/MidiInstrument.h`, ~23
  near-misses). A synth-TU ctor (`fn_822B3318`) builds Object at tail `this+0x78`
  via a vbptr AND embeds a `CharWeightable` at `this+0x8`, diverging from both
  oracles. **LOW–MED confidence — VERIFY FIRST:** `fn_822B3318` may be a co-compiled
  *sibling* (`SampleInst`/`NoteVoiceInst`, tied via `DECOMP_FORCEFUNC`), not
  MidiInstrument itself. Confirm via the class-name string off the ctor's final
  vtable (`PTR_FUN_8201ed94`) before editing; then move the correct class to a
  virtual-Object / CharWeightable-bearing base.
- **`CharWeightable` is RULED OUT** (correctly modeled): it legitimately carries
  BOTH vfptr@0 and vbptr@4 because it introduces a *new* virtual (`SetWeight`) not in
  Object; the dc3 `mWeight@0x8` annotation is right. It's a real base of ~18 Char*
  classes.
- **Discriminator rule (use for any future virtual-base suspect):** a virtual-Object
  class that introduces a brand-new virtual is *supposed* to have both a vfptr and a
  vbptr (CharWeightable case); one that introduces none should have a vbptr only
  (MsgSource case). That tells a legit two-pointer layout from the bug.
- The broader 152-class `: public Hmx::Object` pool was triaged: Object-FIRST MI
  classes (Object at offset 0) show no tail-offset signal, and the already-virtual
  classes (EventTrigger, FlowNode, UIPanel, ObjectDir, Sound, Profile, …) are correct
  — their near-misses are ordinary codegen, not this pattern.

### B. Allocator-ABI tail  *(engine; easy)*
The +77 win left the **`MemMgr.cpp` internals** on the debug stub: `_MemAllocTemp`,
`MemResizeElem`, `MemRealloc` still take the 4/5-arg `(...,file,line,name,align)`
form and route through a stub because **`MemHeap::Alloc` is not yet decompiled**
(see `MemMgr.cpp` comments ~lines 337–358). Decompile `MemHeap::Alloc`, wire the
retail 2-arg path, strip the debug args (HX_NATIVE-guard). Yield: `MemMgr.cpp`
self-match + temp-alloc near-misses.

### C. RndParticleSys own-member bloat (Bug B)  *(Part.cpp; base-class-adjacent)*
`RndFancyParticle` is fixed (commit `69b5983`); `RndParticleSys` itself is ~0x28–0x48
too big: retail `mLife`@0x130 (ours 0x158, −0x28), `mSubSampleXfm`@0x308 (ours 0x320,
−0x18), highest member ~0x330 (ours ~0x3e8). Needs a ctor field-by-field diff
(Ghidra) to localize the DC3-extra members — partly entangled with the Rnd\* MI base
sizes. Unlocks Part `Save`/`Copy`/`SetMesh` (currently 77–81%).

### D. ObjectDir-subclass pinning  *(pin-only; harvests a landed fix)*
The ObjectDir `unk8c` removal (commit `270238d`) made the layout correct for all
ObjectDir subclasses, but only the already-pinned `CharBoneDir` converted (+7). Pin
`CharIKFoot` (~8 near-misses), `CharBone`, `CharFaceServo` `.text` spans in
`splits.txt` to harvest the rest. Mechanical (derive span → `touch config.yml` →
re-SPLIT → objdiff). *(CharFaceServo is fully diagnosed in
`next-wave-onediff-clusters.md`.)*

### E. Per-class one-diff clusters  *(already diagnosed — use as the work queue)*
Two prior recon docs enumerate the remaining per-class layout clusters; treat them
as the ready-to-execute queue:
- **`docs/plans/next-wave-onediff-clusters.md`** — fully diagnoses **CharFaceServo**
  (one +8 member) and ranks Instance/SharedGroup, MemcardMgr_Xbox, MidiInstrument,
  CubeTex, UISlider, Cache_Xbox, TexRenderer, MemStream, FileStream, LightHue.
- **`docs/plans/recon-structural-levers-2026-05-29.md`** — LightPreset Entry/Keyframe
  struct sizes, NgStats counters (`TheNgStats->mMats++` etc., retail-stripped),
  SAVE_REVS/INIT_REVS version mismatches (Part 0x29 vs 0x25, MatAnim 7 vs 4).

---

## Do NOT chase (verified dead ends — saves a fresh session from re-deriving)

- **Global `Hmx::Object` shrink to 0x18** — RULED OUT. Object IS 0x28 (ctor
  `lbl_82737FE8` writes to +0x24, ring head @0x20). A recon proposed shrinking it
  across ~800 subclasses; verified false against the binary before acting. The
  MidiParser +0x10 was the wrong-base issue (Lever A), not Object size.
- **Global `ObjPtr` reorder** — the keystone `{next@0, prev@4, mObject@8}`=0xc is
  CORRECT (ring dtor `fn_82738050` / ring-free `fn_82451A48`). Per-class members read
  at member+0 (e.g. `BandDirector::mMerger`) are the **leaf-workaround** (make that
  one member a raw `T*`), NOT a global change.
- **DataNode size** — confirmed correctly **8 bytes** (`mValue`@0, `mType`@4). Not a
  lever.
- **band3 game matching at scale** — fidelity grind. The ported rb3-Wii *dev* source
  is structurally different from retail bytes; pairing is already solved and is not
  the bottleneck; real-bodied band3 fns sit at 0–11% with zero near-misses. See
  `docs/plans/game-code-pairing.md`.
- **EH-cleanup funclets** (~257 fns, 40–48B @99.85–99.99%, the
  `subi r31,r12,N; stwu -0x60; ... bl ~Dtor; blr` skeleton — incl. ~35 in the
  MidiParser unit) — target-only catch handlers, **NOT source-fixable**. Gated on
  parent-frame match + an **objdiff scoring change** (filter target-only `fn_`
  funclets / fix the scorer). Don't hand-chase; fix the scorer instead.
- **Permuter on the ≥99% engine tail** — LightPreset/MeshAnim/Gen/TexBlender ~87 fns
  are codegen artifacts (funclet static-guard `rlwinm` vs `clrrwi`, `lbl_` vs named
  relocs, regalloc); a full permuter campaign flipped 0/100% this session. Only worth
  targeted `/permute` on the ≤128B / ≥99.9% head.
- **The vbptr/wrong-base sweep beyond Lever A** — RULED OUT as a broad lever.
  `CharWeightable` is correctly modeled (legit vfptr+vbptr); Pattern (B) has no
  instance left after MsgSource; the already-`: public virtual Hmx::Object` classes
  (EventTrigger, FlowNode, etc.) are correct and their near-misses are codegen, not
  layout. `MidiInstrument` is the *single* remaining (verify-first) lead — don't
  re-survey the 152-class pool.

---

## Refs
- Memory: `project_rb3_xenon_roadmap.md`, `project_engine_baseclass_layout_wall.md`,
  `project_game_code_instrumentation.md` (game-code = grind).
- Recon/plan docs: `docs/plans/{next-wave-onediff-clusters,
  recon-structural-levers-2026-05-29, engine-baseclass-layout-bugs,
  struct-offset-sweep, game-code-pairing}.md`.
- Tooling: `scripts/permuter_targets.py rank` (writes the ranked ≥99% queue);
  `bin/objdiff-cli diff -u default/<UNIT> '<MANGLED>' -f json --include-instructions`;
  Ghidra MCP :8002 (`/ghidra-decompile`, `/struct-info`, `/vtable`);
  `scripts/setup_worktree.sh`.
