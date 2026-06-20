# W9 L9 — wired-handle-pairing-wave-post-prereq (DISCOVER dossier)

**Date:** 2026-06-20 · **Base:** main@812e1df (8314 matched) · **Mode:** adversarial discover/planner, READ-ONLY in main.

## Verdict: REAL_ACTIONABLE — with corrections

The frontier is REAL: with the Object.h Handle-macro prereq applied, a batch of
wired `::Handle` method bodies become byte-exact and need only a
`target_symbol_map` reveal entry. **But the frontier's est/specifics need
correction**, and the load-bearing premise is mis-stated.

### Correction #1 — the prereq is NOT landed on main
The frontier says "On the LANDED prereq base". It is **not landed**. The prereq
commit `9fb9016` ("Handle macros: reconcile MessageTimer gate + END PathName tail
(+196 @100%)") lives only on worktree branches
(`w9-reconcile-handle-prereq-FINAL`, `w9-land-reconcile-handle-prereq-9fb9016`,
`w9-handle-pair-clean-char-tier-7-post-prereq`, `w9-land-familyb-...-FINAL`), none
of which are ancestors of main@812e1df. **Every byte-exactness below depends on
this prereq.** So the reveal batch is NOT independently landable vs main@8314 — it
must carry the prereq. This drives the SELF-CONTAINED packaging (one work-item =
prereq + reveal batch).

What the prereq does (verified by reading the diff + a built worktree):
- `BEGIN_HANDLERS`/`BEGIN_CUSTOM_HANDLERS`: gate the per-handler `MessageTimer`
  behind `#ifdef MILO_MESSAGE_TIMERS` (undefined by default = retail no-timer
  shape); 6 TUs restore it via objects.json `/DMILO_MESSAGE_TIMERS`
  (BandDirector/CharBoneDir/Dir/Group/CharLipSync/UI).
- `END_HANDLERS`/`END_CUSTOM_HANDLERS`: emit `(void)(PathName(this), sym);`
  (CUSTOM uses `dynamic_cast<Hmx::Object*>(this)`), `#ifndef HX_NATIVE`, so the
  retained `bl PathName` (virtual `FindPathName`, fn_82732F68) at the unhandled
  tail lands. (Global release `MILO_NOTIFY` is `((void)sizeof(...))` =
  unevaluated, dropping the side effect.)

**Prereq verified real:** worktree
`wt-w9-handle-pair-clean-char-tier-7-post-prereq` (prereq + a +5 char batch)
builds report.json = **8515** = 8314 + 196 (prereq) + 5 (char), and two of the
prereq's own mapped Handle pairs (`UIListSlot`, `UIComponent`) read **100.0%
normalized** under objdiff there.

### Correction #2 — ConnectionStatusPanel VA in the frontier is WRONG
Frontier lists `ConnectionStatusPanel fn_82795BA0` as a single-HANDLE body. That
is **a real divergence, NOT byte-exact** (frame 0xa0 retail vs 0xb0 ours, 8/53
masked-word equality, extra base instructions — likely a `__ehfuncinfo`/member
difference). The ACTUAL byte-exact ConnectionStatusPanel Handle is
**fn_82795638 (size 312)**, not fn_82795BA0 (size 212, a different fn). Use 82795638.

### Correction #3 — of the 4 "single-HANDLE bodies", 3 hold, 1 is the wrong VA
Reloc-masked body comparison (calibrated: a known-objdiff-100% pair, UIListSlot,
returns firstdiff=-1) in the prereq worktree:
| frontier VA | unit | result |
|---|---|---|
| fn_826F8968 | FxSend | **BYTE-EXACT** (61/61, reloc 16/16) ✓ |
| fn_827E6A10 | UIListDir | **BYTE-EXACT** (74/74, reloc 17/17) ✓ |
| fn_8250FE28 | UserMgr | **BYTE-EXACT** (74/74, reloc 17/17) ✓ |
| fn_82795BA0 | ConnectionStatusPanel | **REFUTED** (frame 0xa0/0xb0, 8/53) — use fn_82795638 instead |

## The harvest — 25 NEW byte-exact unmapped Handle bodies (the actionable deliverable)

A binary-wide sweep over the prereq worktree's compiled base objs for every
`?Handle@<Class>@@UAA?AVDataNode@@PAVDataArray@@_N@Z` body, paired to the
target `fn_<addr>` at its VA by reloc-masked byte-equality (same primitive as
reveal_sweep / fuzzy_content_match `word_eq`, validated against an objdiff-100%
control). All 25 fall INSIDE their owning TU's already-pinned `.text` span
(verified for CrowdAudio/UISlider/JoypadClient/ButtonHolder/ScrollSelect/FxSend/
UserMgr) — pure name-reveal, NO pin extension / relocation / sliver-eviction.
Dedup'd against the char-tier (80a2857) + tier1 (2260dd0) char batches (removed
CharBonesBlender 0x823C6458, already in tier1).

```
"0x823004D8": "?Handle@CrowdAudio@@UAA?AVDataNode@@PAVDataArray@@_N@Z",            // CrowdAudio 1712
"0x8244EE40": "?Handle@RndMatAnim@@UAA?AVDataNode@@PAVDataArray@@_N@Z",            // MatAnim 268
"0x82455138": "?Handle@RndConsole@@UAA?AVDataNode@@PAVDataArray@@_N@Z",            // Console 228
"0x8246B950": "?Handle@RndGenerator@@UAA?AVDataNode@@PAVDataArray@@_N@Z",          // Gen 1016
"0x8246C968": "?Handle@RndParticleSysAnim@@UAA?AVDataNode@@PAVDataArray@@_N@Z",    // PartAnim 396
"0x82475348": "?Handle@RndPollAnim@@UAA?AVDataNode@@PAVDataArray@@_N@Z",           // PollAnim 356
"0x824769F8": "?Handle@RndTexBlendController@@UAA?AVDataNode@@PAVDataArray@@_N@Z", // TexBlendController 460
"0x82477758": "?Handle@RndTexBlender@@UAA?AVDataNode@@PAVDataArray@@_N@Z",         // TexBlender 392
"0x8248B6D0": "?Handle@EventTrigger@@UAA?AVDataNode@@PAVDataArray@@_N@Z",          // EventTrigger 964
"0x824A3170": "?Handle@LightPreset@@UAA?AVDataNode@@PAVDataArray@@_N@Z",           // LightPreset 820
"0x824C8800": "?Handle@Spotlight@@UAA?AVDataNode@@PAVDataArray@@_N@Z",             // Spotlight 552
"0x824D4978": "?Handle@LightHue@@UAA?AVDataNode@@PAVDataArray@@_N@Z",              // LightHue 288
"0x8250FE28": "?Handle@UserMgr@@UAA?AVDataNode@@PAVDataArray@@_N@Z",               // UserMgr 296
"0x82515E18": "?Handle@JoypadClient@@UAA?AVDataNode@@PAVDataArray@@_N@Z",          // JoypadClient 452
"0x826F8968": "?Handle@FxSend@@UAA?AVDataNode@@PAVDataArray@@_N@Z",                // FxSend 244
"0x827059B0": "?Handle@FxSendMeterEffect@@UAA?AVDataNode@@PAVDataArray@@_N@Z",     // FxSendMeterEffect 352
"0x8270EC40": "?Handle@DxRnd@@UAA?AVDataNode@@PAVDataArray@@_N@Z",                 // Rnd 256
"0x82788E90": "?Handle@HeldButtonPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z",       // HeldButtonPanel 428
"0x8278AD70": "?Handle@MoviePanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z",            // MoviePanel 576
"0x82794BA0": "?Handle@ButtonHolder@@UAA?AVDataNode@@PAVDataArray@@_N@Z",          // ButtonHolder 356
"0x82795638": "?Handle@ConnectionStatusPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z", // ConnectionStatusPanel 312
"0x827E5008": "?Handle@UISlider@@UAA?AVDataNode@@PAVDataArray@@_N@Z",              // UISlider 1448
"0x827E6A10": "?Handle@UIListDir@@UAA?AVDataNode@@PAVDataArray@@_N@Z",             // UIListDir 296
"0x827F13B0": "?Handle@UIPicture@@UAA?AVDataNode@@PAVDataArray@@_N@Z",             // UIPicture 416
"0x827F8070": "?Handle@ScrollSelect@@UAA?AVDataNode@@PAVDataArray@@_N@Z",          // ScrollSelect 300
```
(ScrollSelect is a CUSTOM-handler — the prereq's END_CUSTOM form produces it
byte-exact; the frontier's __RTDynamicCast contingency does NOT bite here.)

## Packaging (self-contained vs main@8314)
Because the prereq is unlanded, the cohesive landable unit is **ONE worktree =
prereq (9fb9016) + the 25 reveal entries above**. Either:
- **Path A (recommended):** land the prereq `9fb9016` first (it is its own
  net-+196 self-contained item), THEN this reveal batch is a thin +25 follow-up.
- **Path B:** one worktree branched off main carrying prereq+reveals together
  (net ≈ +221), if the coordinator prefers a single landing.

A clean run of `tools/reveal_sweep.py` on the prereq base ALSO surfaces ~50
generic (non-Handle) byte-exact reveals (Character +7, Waypoint +7, Console x3,
…) — orthogonal to this wave but free alongside it.

## Tooling note
`tools/reveal_sweep.py` MISSES these Handle bodies: its `read_coff_base`
yields only `val==0` section-defining symbols, but a `::Handle` body sits at
`val=8` inside a COMDAT that leads with another symbol (catch/funclet packing),
and the Handle is also highly self-similar across classes → it lands in
reveal_sweep's `ambiguous` (1556 dropped edges), not its strict 1:1 candidates.
The harvest here is a **val≠0-aware, per-class-name body matcher** — worth folding
into reveal_sweep as a `--handlers` mode (see discovered frontier).
