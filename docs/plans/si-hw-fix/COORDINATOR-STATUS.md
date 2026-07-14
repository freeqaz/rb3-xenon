# Same-Instrument hardware-failure investigation — coordinator status (INTERIM)

Date: 2026-07-09. Coordinator: Fable. Status: **evidence-gathering in progress** —
3 Sonnet subagents still running (console byte verify / RB3E DLL poke analysis /
grey-out call-path trace). This file checkpoints everything established so far so a
re-run does not redo work. Checkpoints expected from subagents:
`console-bytes.md|.json`, `rb3e-dll-analysis.md|.json`, `greyout-path.md` (this dir).

## Established facts (verified this session, with citations)

1. **The shipped artifact's own doc flags the likely hardware defect.**
   `rb3-xenon/docs/plans/same-instrument-tu5-retarget.md` §5 + §8.1: the TU5 cave was
   placed at **0x82C8A000 in `.data` (RW, NOT executable)** because no file-backed
   executable zero run exists on TU5. §8.1 verbatim: cave execution was only proven
   under **Xenia's JIT (`writable_code_segments`)**; "For a strict-NX / on-console
   target, a code cave in an EXECUTABLE section is needed." The base-binary artifact
   (packer-status doc) used the BINK→BINKBSS gap instead — that choice was Xenia-only
   too. **The static XEX cave was never NX-safe for real hardware.**

2. **Patch content is correct in the artifact.** `/tmp/xbxpull/verify_default.xex`
   (sha256 `9c5965ad…`, the expected patched hash) passes ALL checks via
   `xex_binpatch_tu5.py` verify: 675/675 words, 4 detour sites = `b` into cave
   (0x826684C0→0x82C8A080, 0x825B6488→0x82C8A0C0, 0x8276FA08→0x82C8A8B0,
   0x82794740→0x82C8A590), cave region == cave.bin, flag @0x82C8AAA0 = 1
   (see `/tmp/si-hw-fix/verify_default_report.json`). The older
   `/tmp/xbxpull/default.xex` snapshot is CLEAN TU5 (`6639ce…`) — a pre-upload pull.
   **What the console holds RIGHT NOW is being re-pulled by subagent 1.**

3. **Behavioral contradiction that constrains the root cause.** Observed: boots fine,
   grey-out exactly stock, NO crash. If the IsActive detour were live in memory and
   the `.data` cave non-executable, the overshell screen would CRASH on first
   IsActive call. "Silently stock" is therefore only consistent with one of:
   (a) the detoured functions are never called on a Deluxe+RB3E console (UI path
       bypass — e.g. RB3E's `POKE_B(PORT_BUILDINSTRUMENTSELECTION=0x82668C70, …)`
       replacement changing the flow, or Deluxe dta UI);
   (b) the detours are not in the running image (RB3E runtime poke clobber; wrong
       image launched; TU/xexp shadowing; upload didn't land);
   (c) the cave DOES execute on hardware but `gSameInstrumentEnabled` reads 0 at
       runtime (something zeroes that .data word) — every hook then falls through
       to Orig = exactly stock behavior, no crash. (Retarget §8.2 admits a
       data-driven writer could touch the "unreferenced" zero run.)
   Subagents are discriminating (a) vs (b) vs (c).

4. **RB3E hook mechanics (clobber surface).** `RB3Enhanced/source/utilities.c:10-17`
   `HookFunction` copies the target's FIRST word into its stub then writes a
   PC-relative `b`. If the Sep-2025 console DLL hooks any of our 4 sites, it (i)
   clobbers our detour and (ii) relocates our PC-relative branch word incorrectly
   into its stub. Upstream-0.7 hook set reportedly does NOT include our 4 targets
   (packer-status doc), but the console DLL is newer — being verified from the
   pulled binary.

5. **The DLL-integrated implementation already exists.** Fork
   `/home/free/code/milohax/RB3Enhanced` branch `feature/same-instrument` @ `397b2a3`:
   `source/SameInstrumentHooks.c` full feature (Layers A/B/C + gem-clone),
   `InitSameInstrument()` wired into `ApplyHooks()` (`source/rb3enhanced.c:473-474`),
   gated by `rb3.ini AllowSameInstrument`. CAVEAT (retarget §9): the same-instrument
   PORT_* block in `include/ports_xbox360.h` (lines ~168-172) still holds BASE-TU0
   VAs (IsActive 0x8264B5F8 etc.) — a console (TU5) DLL build MUST swap in the TU5
   VAs from retarget §2a/2b: IsActive 0x826684C0, Resolve 0x825B6488, ProcessConfig
   0x8276FA08, RecalcGemList 0x82794740, GameGemDB::Duplicate 0x827932C8,
   GetDiffGemList 0x827931C8, CopyFrom 0x8278E168, SetOvershellSlotState 0x8268BAF0,
   UpdateAll 0x825B70D0, GetBandUserFromSlot 0x82682B60, MemFree 0x827BC430,
   TheBandUserMgr 0x82E023B8 (unchanged).

6. **XDK-free build reality** (`build-without-xdk-recommendation.md`): the single TU
   compiles clean XDK-free (`cl.exe /c` under wibo — already produced
   `SameInstrumentHooks_tu5.obj`, reloc histogram identical to base). A FULL DLL
   rebuild is Path 2 (OSS full-link; PE→XEX2 pack unproven — risk) or Path 3 (upstream
   CI secret — blocked for forks). A promising hardware-specific hybrid: RB3E DLL
   loads at **0x84000000 (executable image)**, which is within ±32MB PPC `b` range of
   all 4 detour sites — so the cave blob could live in executable space (DLL image or
   a runtime-allocated executable page) while detours stay static, OR the DLL itself
   installs the hooks at runtime like every other RB3E hook (the original design).

## Decision framework (pending evidence)

- If (b) upload/image mismatch → trivial redeploy; re-verify bytes; then NX question
  still applies (expect crash next) → still need executable-cave fix.
- If (a) Deluxe/RB3E bypasses the stock grey-out path → static Layer-A detour is dead
  code on this console; fix must integrate with the RUNNING DLL's flow (RB3E-DLL
  integration, i.e. fork DLL with SameInstrumentHooks + TU5 ports), because the
  replacement UI logic lives there.
- If (c) flag/cave zeroed at runtime, or NX-crash-avoidance ambiguity → move cave out
  of `.data`: DLL-integrated hooks are again the clean answer (executable by
  construction, applied at the right time, ordered with RB3E/Deluxe's own pokes).
- In ALL branches the strategic answer trends to **RB3E-DLL integration** on this
  console (it already runs RB3ELoader); the static cave remains the Xenia-only
  vehicle. Toolchain: full-DLL build without XDK is the open risk — options are
  OSS full-link (Path 2, PE→XEX2 pack unproven), splice-into-released-DLL (Path 2
  step 4a analog, idaxex/xextool), upstream PR/CI, or binary code-cave into the
  console's own DLL image (keeps Deluxe's exact DLL, adds our blob into executable
  DLL space + one call from its ApplyHooks tail).

## Deploy/test plan sketch (to finalize after evidence)

1. FTP GET current `/Usb1/Games/rb3/default.xex`, sha256 it (discriminates (b) now).
2. Whatever artifact ships next must include a **visible boot canary** (e.g. a
   harmless observable change) so "our code is running" is provable independent of
   the feature.
3. Pass/fail observable: on instrument-select, P2 can highlight+select Guitar while
   P1 holds Guitar (Layer A); then both reach gameplay (B+C); independent hit
   detection / no note-steal (gem-clone).
4. Rollback: `/Usb1/Games/rb3/default_pre_si.xex` is the clean-TU5 backup already on
   the console.

## Subagent tracker

- a03b5a67… (Sonnet): console default.xex bytes + PE section flags of cave + TU/xexp
  + launch.ini + rb3.ini recon → `console-bytes.md|.json`. RUNNING.
- ac158878… (Sonnet): console RB3Enhanced.dll pull + poke/hook address extraction +
  overlap verdict + DLL identity → `rb3e-dll-analysis.md|.json`. RUNNING.
- a1d1d284… (Sonnet): grey-out ground truth (Wii decomp + Ghidra base xrefs) — is
  IsActive necessarily executed on the overshell; other selection gates →
  `greyout-path.md`. RUNNING.
- Opus decision agent: NOT yet spawned (waits on the three above).

## UPDATE (session end, 2026-07-09 ~03:30 UTC) — console-bytes evidence landed

Subagent 1 completed (`/tmp/si-hw-fix/console-bytes.md`). Live console UNREACHABLE
this session (off/not on LAN — ping+FTP 0 replies), so verification used the copy
pulled from the console at 03:12 UTC today (`/tmp/xbxpull/verify_default.xex`):

1. **Patch IS on the console (as of that pull):** sha256 == expected patched hash
   `9c5965ad…`; all 4 detour words present + decode to the correct cave branches;
   cave == blob (700 words); flag @0x82C8AAA0 == 1; whole-file diff == clean TU5 +
   exactly the intended 675 writes. Hypothesis 3 (patch not installed) RULED OUT.
   `/tmp/xbxpull/default.xex` is a stale PRE-patch capture (clean TU5) — ignore.
2. **Cave section is NON-EXECUTABLE:** embedded PE section table (offset 0x3000,
   byte-identical to dtk's band_tu5.exe table): `.data` 0x82C64400-0x82E5A2AC has
   Characteristics **0xC0000040 = READ|WRITE|INITIALIZED_DATA, no EXECUTE**. The
   cave 0x82C8A000 falls inside it. `.text`/BINK are 0x60000020 EXECUTE|READ.
   On real hardware (which honors XEX page protections, unlike Xenia's
   writable_code_segments JIT) a branch into the cave faults.

### Root-cause synthesis (confidence: high on mechanism, medium on which gate hit first)

Observed = boots, stock grey-out, NO crash. With the detour bytes proven present,
"silent stock + no crash" means **the Layer-A detour site 0x826684C0 was never
executed** during the failing session. Two compatible contributors:
  (i) on a Deluxe+RB3E console the stock part-select path is altered — RB3E
      POKE_Bs its own BuildInstrumentSelectionList over 0x82668C70
      (rb3enhanced.c:422, OvershellHooks.c:24) 0x7B0 bytes past our Layer-A site,
      and Deluxe rewrites the overshell UI — so the stock grey-out gate we hooked
      is plausibly bypassed/dead on this stack; and/or the TU5 skeleton-match pin
      for IsActive (retarget §2a notes "body diverges") is wrong.
 (ii) **even if/where the detours ARE reached, the .data cave is NX on hardware**
      → instruction-storage exception. Layers B/C/RecalcGemList fire at part-claim
      and song start, so the current artifact would CRASH there once Layer A were
      fixed. The static-cave artifact is unfit for this console on TWO independent
      grounds.

Pending (subagents still running at termination; checkpoints will land in this
dir): rb3e-dll-analysis.md (does the Sep-2025 DLL poke our sites; DLL identity),
greyout-path.md (is IsActive on the render path; xrefs/vtable evidence).

### Fix decision (coordinator recommendation, pre-Opus-ratification)

**Integrate into RB3Enhanced.dll (path b) — the original design.** Rationale:
- The console already runs RB3ELoader; RB3E's HookFunction mechanism is
  hardware-proven executable (its stubs run on real 360s today) — no NX problem.
- Runtime hooks apply in-order WITH the RB3E/Deluxe pokes on the live UI flow,
  eliminating the load-order/bypass class of failure.
- Feature source is COMPLETE on fork branch `feature/same-instrument` @397b2a3
  (SameInstrumentHooks.c + InitSameInstrument in ApplyHooks, rb3.ini-gated).
- REQUIRED CHANGE before build: swap the same-instrument PORT_* block in
  include/ports_xbox360.h (~lines 168-172 + gem-clone fns) from BASE-TU0 to TU5
  VAs (retarget doc §2a/2b; list reproduced in "Established facts" §5 above).
  ALSO first re-validate the Layer-A TU5 pin (0x826684C0) against greyout-path
  findings — if Deluxe/RB3E bypasses stock IsActive, hook the RB3E-replacement
  path instead (e.g. gate inside its BuildInstrumentSelectionList analog).
- Toolchain (user refuses XDK): full-DLL build is the open blocker. Options in
  order: (1) splice/code-cave the compiled TU (SameInstrumentHooks_tu5.obj,
  already built XDK-free) into the console's exact Sep-2025 RB3Enhanced.dll with
  objcave-packer-style relocation into DLL executable space + one added call from
  its ApplyHooks tail (idaxex/xextool for XEX-DLL repack, all free); (2) OSS
  full-link path (build-without-xdk doc Path 2, PE→XEX2 pack unproven); (3) PR
  upstream so official CI builds it. Flag: none of these is yet proven end-to-end
  — this is the honest blocker statement.
- Meanwhile the STATIC XEX should be REVERTED on console (copy
  default_pre_si.xex back over default.xex) once any DLL-based artifact ships,
  to remove the latent NX-crash landmines at song start.

### Deploy + test plan (for the main agent / user)

1. Console was OFFLINE this session — power it on before anything.
2. Re-pull /Usb1/Games/rb3/default.xex, sha256 vs 9c5965ad… (confirm no drift).
3. When a corrected artifact exists: user (not agents) FTPs it; keep
   default_pre_si.xex as rollback.
4. PASS observable, staged: (A) P2 can highlight+SELECT Guitar while P1 holds
   Guitar on the overshell; (B) both advance to difficulty select without
   kickback; (C) both reach gameplay; (D) P1-miss/P2-hit independence (no
   note-steal); (E) clean song end, two scores; repeat song twice (leak check).
5. Include a boot canary in the next artifact (e.g. RB3E_MSG or a visible
   string change) so "our code ran" is provable independent of the feature.

## UPDATE 2 (terminating) — greyout-path evidence landed

`greyout-path.md` (subagent 3) key results:
- **IsActive IS necessarily executed**: it is a public virtual
  (`?IsActive@…@@UBA_NH@Z`), vtable-dispatched from provider-agnostic
  `system/ui/UIList*` engine code (`UIList::Refresh` every poll, CanScrollBack/
  Next, DisableData…). Ghidra base xrefs: 0 direct `bl`, 1 DATA xref @0x820d4c60
  (vtable slot). NOT DTA/scene-bypassable; a first-instruction detour intercepts
  all dispatch. → Hypothesis 1 (Deluxe UI bypass) WEAKENED…
- …and REFUTED at the image level: **RB3DX xex byte-diff vs clean TU5 = only 170
  bytes differ (DLC-cache hooks); all 13 same-instrument-relevant functions incl.
  IsActive are byte-identical.** Deluxe does not touch this code path in the xex.
- Subagent 3 also independently flags the stale BASE VA (0x8264B5F8) in
  ports_xbox360.h — not the failure here (console file verified carrying the TU5
  detours), but it re-raises the question of whether the TU5 pin 0x826684C0
  (skeleton match, "body diverges") is truly IsActive. UNRESOLVED — needs
  Ghidra-on-TU5 / runtime confirmation.

### Final root-cause statement (as of termination)

Patch bytes are present and correct on the console file; Deluxe doesn't bypass
the hook point; IsActive is necessarily on the render path. The remaining
consistent explanations for "stock behavior, no crash", in order of probability:
1. **NX cave**: if the RGH kernel enforces XEX page protections, the first
   IsActive call should fault — since it doesn't crash, either NX is NOT
   enforced (then see 2/3) or the Layer-A site truly isn't executing (see 3).
2. **Cave executes but the enable flag / cave state is not what we wrote at
   runtime** (e.g. loader/game zeroes the .data zero-run) → hooks pass through
   to Orig = exactly stock. Testable only with runtime memory access.
3. **TU5 Layer-A pin wrong** (0x826684C0 may not be IsActive — skeleton-matched
   with divergent body) → detour sits on a rarely/never-called function; the
   real IsActive runs unhooked = stock; no crash. Testable via Ghidra-on-TU5
   vtable walk (find the TU5 vtable slot analogous to base 0x820d4c60).
4. RB3E Sep-2025 DLL poke overlap — analysis agent (ac158878…) still running at
   termination; its checkpoint will land in this dir.
Regardless of which of 1-4 is the trigger, the static-cave artifact carries the
NX defect as a latent song-start crash risk and MUST NOT remain the vehicle.

**Chosen fix path: RB3E-DLL integration (fork branch feature/same-instrument),
with the ports_xbox360.h same-instrument block updated to TU5 VAs AND the
Layer-A TU5 pin re-verified via the TU5 vtable before shipping.** XDK-free
delivery options (unproven, honest blocker): splice compiled TU into the
console's own DLL (idaxex/xextool), OSS full-link (PE→XEX2 pack risk), or
upstream PR/CI. Immediate next actions for a follow-up session:
(1) read rb3e-dll-analysis.md when it lands; (2) power on console, re-pull
default.xex + rb3.ini + launch.ini + TU recon; (3) Ghidra-on-TU5 vtable check of
0x826684C0; (4) Opus decision agent over the complete evidence set.
