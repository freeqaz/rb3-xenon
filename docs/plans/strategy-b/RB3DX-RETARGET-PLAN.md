# RB3DX Retarget Plan — from-source RB3E DLL on RB3DX + Xenia same-instrument validation

**Date:** 2026-07-13. **Status:** PLAN (read-only investigation done; nothing implemented).
**Executor:** Opus ultracode workflow, lane-by-lane. **Author:** Fable planning agent.

---

## 0. Headline finding — the brief's premise is WRONG, and that's good news

The task brief assumed *"RB3DX is RELOCATED vs TU5 — every ports_xbox360.h VA is
wrong for it"* and asked for an address-derivation campaign (map / signature-scan /
Ghidra). **This is refuted by an existing byte-verified study**:

> `rb3-xenon/docs/plans/clean-tu5-vs-rb3dx-divergence.md` (2026-07-07, COMPLETE,
> byte-verified): **RB3DX ≡ clean TU5 + 170 bytes** (92 in `.text` / 10.3 MB, 15
> `.rdata`, 63 `.data`, ~20 short spans, all in *unnamed* regions). Section tables
> byte-for-byte identical → identical VA↔offset mapping. **12,817 / 12,817 mapped
> functions byte-identical at the same VA** (100.000%). All 7 SI patch functions +
> Layer-B helpers + CRT thunks + cave 0x82C8A000 + `PORT_THEBANDUSERMGR` identical,
> same VA, in both. Verdict: **"ONE patch serves both."**

Cross-checks done for this plan (2026-07-13):

- `rock-band-3-deluxe/platform/xbox/default.xex` sha1 **c5a17091**cb44c0119424390a1738d161995e430e
  == `/srv/torrents/games/arbys/rb3/default.xex` (the exact binary the divergence
  study and all Xenia bring-up work used). One RB3DX xex, everywhere.
- The RB3DX repo **does not build its xex from source**. `build.ninja:50`:
  `build out/xbox/default.xex: copy platform/xbox/default.xex` — the xex is a
  prebuilt binary checked into the repo; `configure_build.py xbox + ninja` builds
  the **ARK/DTA side only**. So (a) "does the build emit a symbol map" = **NO, and
  never will** — there is no xex compile step. The user's fork-and-build capability
  controls DTA/ARK content (that's how `dx_check_for_dupe → dupe_allowed TRUE`
  shipped), not code addresses.
- The "RB3DX is RELOCATED" memory notes (IsHost 0x823CECE0 → 0x823E1700, the
  clean_tu5-symbols.txt mismatch at 0x827BC838) all describe relocation **vs the
  rb3-xenon decomp base (TU0/retail, Aug 2010)** — not vs TU5. `ports_xbox360.h`
  is TU5-native (v0.0.5.1, entry 0x8283CD20), and TU5 ≡ RB3DX.

**Consequence:** Track 1 collapses from an address-derivation campaign into a
**cheap audit + packaging + regression-gate** job. The existing TU5-targeted
from-source DLL is, address-wise, already the RB3DX DLL. The dominant cost of the
whole effort is Track 2's Xenia hub-load crash (PC 0x82BCEFE4), as the brief
suspected.

---

## 1. Verified current state (evidence paths)

| Fact | Evidence |
|---|---|
| Strategy-B from-source DLL builds: 51 TUs under wibo + MSVC-X360 16.00.11886, K-link script | `rb3-xenon/tools/oss-xbox-build/K-link/build_xbox_ossp.sh` (compile/link/all; `-dll -entry:_DllMainCRTStartup -XEX:NO`, **no `-MAP` yet**) |
| PE→unsigned-XEX2-DLL packer + current TU5 SI DLL artifact | `rb3-xenon/tools/xex2pack/xex2pack.py`; `work/boot.xex` (8,724,480 B, 2026-07-12); `work/xenia_boot.log` + `xenia_verify.log`: 0 "unresolved" hits, module launches |
| SI feature source; 4 detours + 5 support POKEs, all `PORT_*` from ports_xbox360.h | `RB3Enhanced/source/SameInstrumentHooks.c` (SI_HOOK ×4 @526-529, SI_POKE_B ×5 @519-523; `SameInstReady()` @492 gates install on non-zero ports; **install is unconditional, hook BODIES gated on `config.AllowSameInstrument`**); called from `rb3enhanced.c:474` inside `ApplyHooks()` |
| ports_xbox360.h is TU5-only, 197 `PORT_` defines; SI set at the VAs in the brief (0x826684C0, 0x825B6488, 0x8276FA08, 0x82794740, 0x827932C8, 0x827931C8, 0x8278E168, 0x8268BAF0, 0x825B70D0) | `RB3Enhanced/include/ports_xbox360.h:168-180` |
| RB3E init touches ~127 `PORT_` sites (ApplyPatches/ApplyHooks in `StartupHook`) — the whole DLL, not just SI, runs on TU5 addresses | `RB3Enhanced/source/rb3enhanced.c` (grep `PORT_` = 127) |
| RB3DX ≡ TU5 divergence study; per-function ≥64-byte compare of every SI address; 170-byte total delta | `rb3-xenon/docs/plans/clean-tu5-vs-rb3dx-divergence.md` §2a/2b/2c |
| Flat images + map for offline analysis | clean TU5 PE `rb3-xenon/_tu5probe/clean/band_clean_tu5.exe` (sha1 5f3f667a); RB3DX flat image `/tmp/rb3dx-wf/rb3dx_decomp.bin` (xex1tool `-b`, base 0x82000000, recipe in jit-fault-wiki doc-09 §L1); `base_to_tu5_map.json` at `rb3-xenon/.claude/worktrees/tu5-migrate/_tu5probe/tu5_migrate/` |
| Xenia SI harness exists but is wired to the OLD spliced DLL | `xenia/src/xenia/emulator.cc`: `--si_load_dll` (block ~L5056) `LoadUserModule(call_entry=false)` then **host-emulates** the 4 detours with a **hardcoded** `kHooks` table of old-DLL targets `{0x8402D918, 0x8402D958, 0x8402DEF0, 0x8402DCE8}` (the `InitSameInstrument @0x8402DFA8` in the cvar help text is also old-DLL); `--si_hook_verify` (thread @L3651, checker @L970+) decodes the 4 site words → PASS iff `b` into `[0x84000000,0x84040000)` |
| Xenia RB3DX ceiling: `--rb3dx_skip_calibration` (UNCOMMITTED, emulator.cc:245/784/3749) reaches **main_hub_screen** 2/2, then deterministic guest crash **PC 0x82BCEFE4 / EA 0x7FEA1A80**, SIGSEGV 2→8, host terminate ~30 s. Song select/load sit behind it | memory `project_xenia_seh_fault_wiki.md` (2026-07-12 entry); `xenia git status`: `M src/xenia/emulator.cc` (skip-cal + `--rb3dx_offline_join` + `--rb3dx_ui_probe`, all uncommitted); crash PC in `xenia/docs/jit-fault-wiki/01-symptom-and-evidence.md:49`, `BRIEF-main-hub-load-stall.md:248` |
| EA 0x7FEA1A80 was seen EARLIER as a *recovered / non-fatal* fault during the pre-skip-cal era | `jit-fault-wiki/07-fix-and-verification.md:208` ("SIGSEGV=4 last_fault=0x17FEA1A80 … recovered"), `BRIEF-main-hub-load-stall.md:274` ("unrelated to the stall") — post-skip-cal it recurs and escalates to terminate |
| Hardware context: user's RGH runs **clean TU5 xex (6639ce25) + RB3DX dupe_allowed ARK (patch_xbox.hdr 6a5b174a) + spliced DLL (60389001)**; T5 (does RGH load the RAW DLL) + all behavioral tests UNTESTED; crash-at-song-load is the predicted **H1 vector[-1]** failure if hooks aren't live | memory `project_same_instrument_rb3dx.md` |
| On RB3DX the SI *selection* gate is DTA (`dx_check_for_dupe`), so Layers A/B of the DLL target dead code there; the load-bearing DLL hooks on RB3DX are **H1 ProcessConfig + H2 RecalcGemList** (crash fix + gem clone) | same memory file, wave-4/5 root cause |
| Xenia boot recipe for RB3DX | boot dir `/tmp/rb3dxboot` (default.xex + gen/charnames/AvatarAwards/nxeart symlinks); flags `--protect_zero=false --gpu=vulkan --local_user_count=2` + committed fixes 9096dd4d0 (XamEnumerate) & e248d624c (GetInfo OOM); scripted-input nav ladder in doc-09 §L4 |

---

## 2. Reframed strategy

1. **There is no "retarget".** The deliverable formerly called "boot_rb3dx.xex" is
   the *same* DLL, revalidated: an **audit proves** the 197 TU5 ports and every
   RB3E code-write site miss RB3DX's 170 patched bytes, and a **regression gate
   script** re-proves it against any future RB3DX xex drop (if upstream ever ships
   a new prebuilt xex, re-run the gate; only if it someday truly relocates does the
   documented signature-scan fallback activate).
2. **The Xenia harness, not the DLL, needs rewiring** (old spliced-DLL VAs are
   hardcoded; our from-source DLL has different in-DLL addresses → link `-MAP`).
3. **The validation ceiling is the hub crash.** Root-causing PC 0x82BCEFE4 is its
   own workstream with unknown cost; everything else is small and parallelizable.
   Intermediate milestones make the effort pay out even if the crash fix stalls.
4. **Bonus milestone:** with two pads and NO DLL, Xenia + dupe_allowed ARK should
   *reproduce the user's hardware song-load crash* (H1 vector[-1]). That is
   independently valuable (confirms the wave-5 crash theory end-to-end) and is the
   perfect negative control for the DLL-on run.

---

## 3. Phases

### Phase 0 — Hygiene + baseline snapshot (small; do first)

- **P0.1** In `xenia`: isolate and commit `--rb3dx_skip_calibration` as its own
  default-off, title-gated (0x45410914) commit. `src/xenia/emulator.cc` is
  currently `M` and also carries `--rb3dx_offline_join` + `--rb3dx_ui_probe`
  (kept-as-diagnostics). Commit **only the skip-cal hunks** (git add -p), leave the
  diagnostics uncommitted or land them in a separate diagnostics commit — decide at
  execution; do NOT `git stash`/checkout in shared repos, do NOT touch other
  agents' unstaged edits.
- **P0.2** Record baseline artifacts + hashes in
  `docs/plans/strategy-b/checkpoints/rb3dx-baseline.json`: RB3DX xex c5a17091,
  clean-TU5 PE 5f3f667a, boot.xex sha, dupe_allowed ARK {hdr 6a5b174a, ark
  aacfbb9d}, flat image `/tmp/rb3dx-wf/rb3dx_decomp.bin` (re-derivable:
  `xex1tool -b <out> rock-band-3-deluxe/platform/xbox/default.xex`; /tmp is
  volatile — regenerate, don't depend on it).
- **Acceptance:** xenia builds green with skip-cal committed; baseline JSON exists.

### Phase 1 — Track 1: RB3DX compatibility audit + regression gate (small, parallel-safe)

Replaces the brief's address-derivation ask. One script, one doc.

- **P1.1** Write `rb3-xenon/tools/oss-xbox-build/rb3dx_port_audit.py`:
  1. Extract/locate both flat PEs (clean TU5 + RB3DX, identical section tables →
     shared `va_to_off`; reuse `tu5-migrate/tools/tu5_va.py` helpers).
  2. Recompute the byte-diff spans (expect the known ~20 spans / 170 B — do not
     trust the 2026-07-07 numbers blindly; re-derive).
  3. Parse **all 197** `PORT_` defines from `ports_xbox360.h`.
  4. Classify every port: for *hooked/poked function* ports compare the first
     N=64 bytes clean-vs-RB3DX at the VA; for *data/BSS* ports check section +
     that no diff span covers them.
  5. Extract RB3E's full **write-site set** (every `HookFunction`, `POKE_B`,
     `POKE_BL`, `POKE_32`, patches.c writes — grep-driven over
     `RB3Enhanced/source/`) and intersect with the RB3DX diff spans. This catches
     the behavioral-collision case (RB3E and Deluxe patching the same check, e.g.
     the `0x82575f9c bne→nop` style sites) that the SI-only §2b check didn't cover.
  6. Emit `rb3dx_port_audit.json` + human table; **exit nonzero on any overlap**.
- **P1.2** Decision point on the (expected-empty) overlap list: any collision gets
  a manual ruling (skip the RB3E patch on RB3DX via a config default, or accept
  RB3E-wins ordering — DLL writes happen at StartupHook, after xex load, so RB3E
  overwrites Deluxe bytes at a collided site; that must be a *decision*, not an
  accident).
- **P1.3** Document the fallback that we are NOT doing (for future readers): if a
  future RB3DX xex relocates, the recipe is signature-scan (à la xenia
  `--rb3dx_offline_join` IsHost self-location) using clean-TU5 function bodies as
  templates, seeded from `base_to_tu5_map.json` + rb3-xenon named functions.
  Pointer to this plan section is enough; no implementation.
- **Acceptance:** audit script runs green (0 unreviewed overlaps) against
  c5a17091; committed to rb3-xenon; doc updated. **Deliverable statement:** "the
  TU5 DLL *is* the RB3DX DLL for xex c5a17091" or the reviewed exception list.

### Phase 2 — Track 1: build variant + linker map + deploy package (small)

- **P2.1** Add `-MAP:$STAGE/RB3Enhanced.map` to `LFLAGS` in
  `build_xbox_ossp.sh` (MSVC link.exe supports it; XDK-free). Rebuild + repack.
  Extract and record: `InitSameInstrument` VA, the 4 hook-body VAs
  (`IsActiveHook`, `ResolveWaitStatesHook`, `ProcessConfigHook`,
  `RecalcGemListHook`), and `config` struct VA + `AllowSameInstrument` field
  offset (from `RB3Enhanced/include/config.h` layout) — all needed by Phase 3.
- **P2.2** **No `ports_rb3dx.h`.** Per Phase 1 there is nothing to retarget;
  adding a parallel header would be a maintenance trap. Instead: (i) add a comment
  block at the top of `ports_xbox360.h` in the fork ("valid for clean TU5 *and*
  RB3DX c5a17091 — see divergence doc + rb3dx_port_audit.py"), (ii) optional
  `-D RB3E_TARGET_NAME=\"tu5+rb3dx\"` build-tag only if a distinct artifact
  label is wanted. If reviewers insist on two artifacts, `boot_rb3dx.xex` may be a
  byte-copy of `boot.xex` with a different name — say so honestly in DEPLOY.md.
- **P2.3** Assemble `rb3-xenon/tools/oss-xbox-build/deploy-si-rb3dx/`:
  `RB3Enhanced.dll` (packed from-source DLL), `rb3.ini` with
  `AllowSameInstrument=true`, `DEPLOY.md` pinning: RB3DX xex c5a17091 (from
  `rock-band-3-deluxe/platform/xbox/`), dupe_allowed ARK
  {6a5b174a, aacfbb9d} from `rock-band-3-deluxe/out/xbox/gen/`, rollback notes
  (mirrors existing `deploy-si/DEPLOY.md`). Note: the hardware DLL *load path*
  (how the RGH loads RB3Enhanced.dll) is unchanged from the existing deployment
  and is xex-agnostic; T5 remains a hardware-only unknown.
- **Acceptance:** rebuilt DLL + .map committed (fork `feature/same-instrument`);
  deploy dir complete; VAs recorded in `checkpoints/rb3dx-baseline.json`.

### Phase 3 — Track 2a: Xenia harness rewire + hook-install validation (medium; independent of the hub crash)

- **P3.1** Rewire `--si_load_dll` for the from-source DLL. Two options; pick (a):
  - **(a) preferred — stop hardcoding:** keep `LoadUserModule(call_entry=false)`;
    replace the host-emulated `kHooks` table with a **guest-thread call of
    `InitSameInstrument`** at a new cvar `--si_init_va=0x84......` (from the
    Phase-2 map), reusing the proven `__savegprlr_23` override + register
    snapshot/restore mechanism already referenced in the cvar help text.
    `InitSameInstrument` is self-contained (SameInstReady = compile-time-constant
    ports; SI_POKE/SI_HOOK write trampolines) and safe without CRT init.
  - (b) fallback: keep host-emulated detour writes, read the 4 site→target pairs
    from a JSON via `--si_hook_table=<path>` generated from the map.
- **P3.2** Handle the config nuance (from the brief, confirmed in source):
  `call_entry=false` skips DllMain/config load → `config.AllowSameInstrument`
  stays 0 → hooks install but run **pass-through**. Add
  `--si_force_allow_va=0x84......` (VA of the flag byte/int from the map): after
  DLL load, poke 1. Without this, Phase-5 behavioral validation silently tests
  nothing — treat this cvar as REQUIRED for the SI-on runs and assert in the log.
- **P3.3** `--si_hook_verify` needs no logic change (it already checks "b into
  [0x84000000,0x84040000)") — but re-verify its window covers our DLL's .text
  size (boot.xex is 8.7 MB packed; if mapped image extends past +0x40000, widen
  the window from the module's actual mapped bounds instead of constants).
- **P3.4** Run matrix on **RB3DX xex** boot (`/tmp/rb3dxboot` recipe +
  `--rb3dx_skip_calibration`):
  1. No DLL → verifier reads `0x7D8802A6` at all 4 sites (negative control).
  2. `--si_load_dll --si_init_va=…` → verifier PASS ×4 into DLL space.
  3. Boot proceeds to the current ceiling (main_hub crash PC 0x82BCEFE4)
     **unchanged** — proves the DLL introduces no new pre-hub regression.
- **Acceptance:** matrix logs checked into
  `docs/plans/strategy-b/checkpoints/` (grep-able PASS lines); emulator.cc changes
  committed default-off + title-gated (DC3-inert by construction).

### Phase 4 — Track 2b: root-cause the hub-load crash PC 0x82BCEFE4 (THE LONG POLE, unknown cost)

Honesty first: **song-select and song-load validation are gated on this phase.**
It is a hard, open Xenia-bringup frontier; every previous gate in this campaign
(SEH, OOM, splash, calibration) took a dedicated multi-agent investigation. Budget
accordingly and let Phases 1-3 land regardless.

- **P4.1 Characterize** (cheap, half a day): regenerate the flat image
  (`xex1tool -b`), capstone-disasm ±0x200 around 0x82BCEFE4, recover the faulting
  instruction + operand registers (recipe = doc-09 §L1, which did exactly this for
  0x827bcbd8). Identify the function: inverse-lookup `base_to_tu5_map.json`
  (TU5 VA → base VA → rb3-xenon named symbol); cross-reference the RB3 Wii decomp
  (`~/code/milohax/rb3`, engine ~75% matched) and DC3 source for a source-level
  oracle of what the code is computing.
- **P4.2 Explain the EA**: 0x7FEA1A80 sits just under 2 GB. Hypotheses to triage
  *in order*:
  1. **Recovered-fault class gone fatal** — the same EA appeared pre-skip-cal as a
     benign *recovered* SIGSEGV (doc-07:208). Diff what changed: is the crash the
     MMIO watch-clear "recover-without-advancing-PC" path escalating (cf. doc-09
     livelock), or a genuinely new read/write?
  2. **Sign-extension / negative-offset arithmetic** off a large base (guest
     0x8… + negative → 0x7F… hole), suggested by the near-2GB EA.
  3. **Unmapped heap tail** — guest heap region Xenia doesn't back (precedent:
     zero-page backing fix fb864e3e; a targeted backing/guard for the 0x7F… hole
     may be a legitimate emulator-side fix if real hardware maps it).
- **P4.3 Fix menu** (choose after P4.1/P4.2 evidence): (a) emulator-side mapping/
  recovery fix (preferred if it's a Xenia gap — like the last four gates all
  were); (b) title-gated default-off guest patch at the faulting site; (c) if the
  code path is skippable (e.g. an optional hub visual), a skip-lever like the
  calibration one. Follow the campaign ack rule: land default-off + title-gated,
  DC3 A/B before flipping anything on.
- **Acceptance ladder:** (i) faulting function named + source-oracle understood;
  (ii) fix candidate implemented; (iii) main_hub renders and survives 60 s;
  (iv) scripted nav PLAY NOW → QUICKPLAY → song_select reachable (doc-09 §L4
  input ladder, START not A for title→hub).

### Phase 5 — End-to-end SI validation in Xenia (gated on Phase 4)

Boot config for all runs: RB3DX xex c5a17091 + **dupe_allowed ARK** (repoint the
`gen/` symlink in the boot dir at `rock-band-3-deluxe/out/xbox/gen/` — the stock
torrent ARK does NOT contain the dta fix), `--protect_zero=false --gpu=vulkan
--local_user_count=2 --rb3dx_skip_calibration` + Phase-4 lever.

- **P5.1 NEGATIVE CONTROL (also a standalone deliverable):** no DLL, two pads,
  scripted nav: P2 joins, both select GUITAR (dta gate now allows it), pick song →
  **expect the H1 crash at song load** (`ProcessConfig → TrackNumOfType == -1 →
  vector[-1]` DSI). If it reproduces: we have *reproduced the user's hardware
  crash in Xenia* — huge derisk, confirms wave-5 theory, gives a crash signature
  to assert against. If it does NOT reproduce, stop and re-derive (the hardware
  crash may be something else, e.g. the DLL never loading on RGH at all — T5).
- **P5.2 SI-ON:** `--si_load_dll --si_init_va --si_force_allow_va
  --si_hook_verify`, same nav → song **loads and reaches gameplay** with both
  players on guitar. Capture frames (`--dump_frames_path`) for both P5.1/P5.2.
- **P5.3 Stretch (optional):** brief gameplay soak — both trackwatchers scoring
  independently (H2 gem-clone behavioral check); known-accepted residuals R3-R5
  (stem mute / FxSend / shared difficulty) are NOT failures.
- **Acceptance:** P5.1 crash signature captured; P5.2 song-load PASS logs +
  frames; a short RESULTS.md in `docs/plans/strategy-b/`.

### Phase 6 — Hardware deployment (user-driven, out of Xenia scope)

Ship `deploy-si-rb3dx/` (Phase 2) to the RGH drive: either keep clean TU5 xex
(6639ce25, current deployment — equally valid per divergence study) or switch to
RB3DX xex c5a17091; dupe_allowed ARK; from-source DLL + `AllowSameInstrument=true`
ini. Keep the existing rollback set. Symptom→lane table from
`project_same_instrument_rb3dx.md` still applies (crash@load=H1/T5,
note-steal=H2). Xenia P5 results define exactly what to expect.

---

## 4. Sequencing & parallel lanes

```
P0 (hygiene)          — first, tiny
P1 (audit)      ─┐
P2 (map+deploy) ─┼─ independent, run as 2-3 parallel Opus lanes
P3 (harness)    ─┘   (P3 needs P2's map VAs → start P3 after P2.1, ~hours)
P4 (hub crash)  — start immediately in its own lane; LONG POLE, unknown cost
P5 (validation) — gated on P3 + P4 (P5.1 negative control only needs P4)
P6 (hardware)   — gated on P5, user-driven
```

Value ladder if P4 stalls (report these as real milestones, not consolation
prizes): audit-proven DLL/RB3DX compatibility (P1) → from-source DLL with map +
deploy package (P2) → hooks verified installed at correct VAs on a booting RB3DX
in Xenia (P3.4) → hardware test can proceed on P1-P3 evidence alone (the
hardware never had the hub crash — that's Xenia-only).

## 5. Top risks

1. **P4 cost is unbounded.** Deterministic + disasm-able is promising, but the
   last four gates each took a dedicated investigation. Mitigation: milestone
   ladder above; P5.1 negative control only needs P4 (not P3), so schedule it the
   moment the hub opens.
2. **Audit finds a real collision** between an RB3E write-site and a Deluxe span
   (would mean RB3E fights RB3DX's own hook). Low probability (SI set already
   cleared); handled by P1.2 ruling, worst case = default-off that one RB3E patch.
3. **`--si_load_dll` guest-thread InitSameInstrument call misbehaves** without
   CRT init (e.g. TOC/r13 expectations). Fallback P3.1(b) (host-emulated table
   from the map) is mechanical and proven.
4. **P5.1 fails to reproduce the hardware crash** → the user's crash is NOT H1;
   plan explicitly stops and re-derives rather than shipping a fix for the wrong
   bug.
5. **Future RB3DX xex drop changes bytes.** Regression gate (P1) re-runs in
   minutes; signature-scan fallback documented but unimplemented (correctly —
   YAGNI until it fires).

## 6. Repo hygiene (binding)

- RB3E DLL work: `RB3Enhanced` fork branch `feature/same-instrument` (397b2a3+);
  build/pack tooling + docs: `rb3-xenon` (this plan, audit script, deploy dirs).
- Xenia levers: `xenia` repo; emulator.cc currently has UNCOMMITTED
  `--rb3dx_skip_calibration` / `--rb3dx_offline_join` / `--rb3dx_ui_probe` —
  Phase 0 commits skip-cal hunks only, `git add -p`, never `-A`/stash.
- RB3DX repo: read-only except ARK rebuilds already established; the xex is
  never modified (runtime/DLL patching only).
- All new Xenia cvars: default-off + title-gated 0x45410914 (DC3-inert), per the
  campaign ack rule.

## 7. Quick command appendix

```bash
# RB3DX flat image (re-derive; /tmp is volatile)
xex1tool -b /tmp/rb3dx-wf/rb3dx_decomp.bin \
  ~/code/milohax/rock-band-3-deluxe/platform/xbox/default.xex   # base 0x82000000

# DLL build + pack (Strategy B, K-link)
XDK_OSS=... rb3-xenon/tools/oss-xbox-build/K-link/build_xbox_ossp.sh all   # + add -MAP (P2.1)
python3 rb3-xenon/tools/xex2pack/xex2pack.py ...                            # -> boot.xex / RB3Enhanced.dll

# Xenia RB3DX boot (current ceiling = hub crash)
xenia --protect_zero=false --gpu=vulkan --local_user_count=2 \
  --rb3dx_skip_calibration --si_load_dll --si_init_va=0x84...... \
  --si_force_allow_va=0x84...... --si_hook_verify \
  --scripted_input="8s:A@0,...,28s:START@0,..." /tmp/rb3dxboot/default.xex

# Nav ladder + SI address table: jit-fault-wiki doc-09 §L4 / §"Same-instrument patch"
```
