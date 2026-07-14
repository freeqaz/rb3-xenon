# Wave 4 — Same-Instrument (SI): the REAL gate, and the fix

Date: 2026-07-09
Agent: wave4 SYNTHESIS+BUILD (Opus)
Status: **ROOT CAUSE DECISIVELY FOUND. Fix is DTA-side, not XEX-side.**

---

## TL;DR

The same-instrument block on this console is **not** in any of the four native
C++ functions we patched. It lives in **RB3 Deluxe's DTA scripts**. RB3DX
**deleted** the vanilla C++ wait-state path our hooks serviced and re-implemented
same-instrument blocking in script as one function, `dx_check_for_dupe`.

- **Fix = a one-token DTA edit** (`dx_check_for_dupe` → always return `TRUE`),
  recompiled into the RB3DX patch ARK. **No XEX change is required to ship SI.**
- The four-hook static-cave XEX (`default_tu5_patched.xex`) targets **dead code**
  on RB3DX — every ladder rung was inert because the hooks are architecturally
  off the RB3DX decision path, *independent of NX / cave-executability*. The NX
  question is now **moot for shipping SI**.
- We were **not** 100% certain of the code path before this wave. We had the
  wrong layer entirely (native C++ grey-out model vs. RB3DX DTA state-machine).

---

## 1. Root cause

### What the player experiences
P2 (guitar controller) highlights the Guitar row, presses A → **click SFX plays,
nothing happens**. No grey-out anywhere, even stock.

### The real gate (verified in the shipped RB3DX repo, HEAD `9635d57`)

RB3DX ships a script function:

`_ark/dx/overshell/dx_overshell_funcs.dta`, `{func dx_check_for_dupe ($slot) ...}`
(lines 37–118; comment verbatim: *"function to allow 5L/Pro instruments together
but still block same parts from being selected"*).

Logic: `$dupe_allowed = TRUE`; for the caller's selected part, iterate every
*other* user — if any other user's `get_track_type` equals the same track
(`kTrackGuitar`, `kTrackBass`, `kTrackKeys`, `kTrackDrum`, the Pro variants, or
`kTrackVocals`), set `$dupe_allowed = FALSE`. Returns `$dupe_allowed` (line 117).

It is called from **three sites** in `_ark/ui/overshell/slot_states.dta`:

| Site | State | Line |
|---|---|---|
| `kState_ChoosePartWarn` → `SELECT_MSG`/confirm_yes | 3196 |
| `kState_NotBattlePartWarn` → `SELECT_MSG`/confirm_yes | 3227 |
| `kState_ChoosePartWait` → **`enter`** | 3253 |

All three use the identical pattern:

```
{if_else {dx_check_for_dupe {$this get_slot_num}}
   {$this show_state kState_ChooseDiff}   ; dupe allowed → advance
   {$this show_state kState_ChoosePart}   ; dupe blocked → BOUNCE back
   ;{$this show_choose_part_wait}         ; dx - deprecated vanilla behavior
}
```

The `kState_ChoosePartWait` `enter` site (3253) is the one that produces the
reported bug: P2 clicks Guitar → native `select_part kTrackGuitar` fires (button
SFX) → `SelectPartImpl` routes to `kState_ChoosePartWait` → its DX-overridden
`enter` immediately re-runs `dx_check_for_dupe` → returns `FALSE` (P1 already
holds `kTrackGuitar`) → `{$this show_state kState_ChoosePart}` **silently bounces
back**. Sound, then nothing. Exact match to the report.

### The load-bearing RB3DX change: vanilla path deprecated

At `slot_states.dta:3199, 3230, 3256`, every `{$this show_choose_part_wait}` is
**commented out** with `; dx - deprecated vanilla behavior`. `show_choose_part_wait`
is the entry to the native C++ wait-state negotiation
(`OvershellPanel::ResolvePartWaitStates`, the function our Layer-B cave stub
replaces). **RB3DX no longer calls it.** Therefore the entire native
`ResolvePartWaitStates` → `ProcessConfig` machinery is bypassed by the DX flow,
and any XEX patch to it is inert on this console.

---

## 2. What we previously got wrong (answering the user directly)

> "Were we 100% certain of the code path?"

**No — and the certainty gap was the whole problem.** Concretely:

1. **Wrong layer.** We modeled SI as a native C++ *grey-out* gated by
   `OvershellPartSelectProvider::IsActive`, and built a 4-hook static cave around
   `IsActive` / `ResolvePartWaitStates` / `ProcessConfig` / `RecalcGemList`. On
   RB3DX the gate is a **DTA script** (`dx_check_for_dupe`); the native grey-out
   path is not the enforcement mechanism here.

2. **The ladder rungs were tautologies, not tests.** Forcing `IsActive` true
   (rungs A/B) changed nothing because the click never consulted `IsActive` for
   its accept/reject decision — the DTA state machine did. Rung C
   (`ResolvePartWaitStates` replacement) was inert because RB3DX **deprecated the
   very path that calls it**. So rung C's silence was *not* evidence of NX / cave
   failure at all — the hook is simply off the RB3DX decision path. We had
   attributed rung C inertness to a possible NX defect; that attribution is now
   retired.

3. **"No grey-out even stock" is fully explained.** RB3DX does not use the
   `IsActive` grey model on this screen (rows are always active,
   `part_unresolved TRUE` is hardcoded in the states); it blocks at *confirm/enter*
   time via `dx_check_for_dupe`. There was never a grey-out to lift.

4. **We never needed the console at all to settle this.** The decisive evidence
   is plaintext in the open-source RB3DX repo. The Xenia NX lane and the console
   ladder were chasing a mechanism (native hook execution) that does not gate this
   feature on RB3DX.

**What is now verified:** the exact gate function, its three call sites, its
return semantics, and the fact that the vanilla C++ path is commented out — all
read directly from RB3DX HEAD `9635d57` in this session (not inferred).

**What remains genuinely uncertain (open risk, see §6):** whether allowing two
*identical* track types then starts a song cleanly on retail TU5, or trips a
downstream `ProcessConfig` assert.

---

## 3. THE FIX (DTA)

### 3a. Source edit — one line, single point

File: `_ark/dx/overshell/dx_overshell_funcs.dta`
Function: `dx_check_for_dupe` (the ACTIVE one, lines 37–118; ignore the
`#ifdef DISABLED` duplicate at 119–164).

Change **line 117**, the function's return value:

```diff
-   $dupe_allowed ; returns dupe TRUE/FALSE check back to slot_states
+   TRUE ; SI: force-allow same-instrument duping (was $dupe_allowed)
```

That makes the function return `TRUE` unconditionally → all three call sites take
the `{$this show_state kState_ChooseDiff}` branch → P2's Guitar pick advances to
difficulty select instead of bouncing. Single point, uniform effect, minimal blast
radius.

**Equivalent fallback** (if you prefer to leave the function intact): at each of
the three `slot_states.dta` call sites (3196 / 3227 / 3253) replace
`{dx_check_for_dupe {$this get_slot_num}}` with literal `TRUE`. Same result; three
edits instead of one.

Note: RB3DX's `dx_check_for_dupe` *already* permits cross-family duping
(5-lane Guitar + Pro Guitar, different track types). This edit additionally
permits **exact same-part** duping (two 5-lane Guitars), which is the requested SI
behavior.

### 3b. Build & deploy (RB3DX build system, at `/tmp/rb3dx`, HEAD `9635d57`)

RB3DX compiles DTA → DTB with `dtab` and repacks with `arkhelper` via a ninja
build. The overshell DTA lands in the **xbox patch ARK**:

```
# from the rb3dx repo root, after applying the §3a edit:
python dependencies/python/configure_build.py xbox --fun
dependencies/<platform>/ninja      # linux: dependencies/linux/ninja
# outputs:
#   out/xbox/gen/patch_xbox.hdr
#   out/xbox/gen/patch_xbox_0.ark   <-- contains the recompiled slot_states/dx_overshell DTBs
#   out/xbox/default.xex            <-- UNCHANGED for SI; native code untouched
```

Deploy to the console: back up the existing `gen/patch_xbox.hdr` +
`gen/patch_xbox_0.ark`, then replace them with the freshly built pair. **The
`default.xex` does not need to change for SI** — leave the console's current xex
(patched or stock) in place.

### 3c. Advanced alternative (no full rebuild)

If a full RB3DX rebuild is impractical, binary-patch the compiled `dx_overshell_funcs.dtb`
node inside `patch_xbox_0.ark`: locate the `dx_check_for_dupe` return node and
swap the trailing `$dupe_allowed` variable-ref node for a literal `TRUE` (kInt 1)
node. This is fiddlier and error-prone; the source-rebuild path (§3b) is
recommended.

---

## 4. Why NO new XEX fix artifact was built

The task asked for a new XEX that patches "the real gate per the wii-path lane's
TU5 VA" (`OvershellPanel::ResolvePartWaitStates` @0x825B6488). **That path is
deprecated on RB3DX** (`show_choose_part_wait` commented out, §1). A ResolvePartWaitStates
XEX patch would be a byte-perfect fix to code RB3DX never executes — it would ship
the exact dead-artifact class that produced the wave1–3 dead-end. I therefore did
**not** build it, and I did not spend effort re-verifying its VA against
`band_tu5.exe`, because the patch it would enable is moot on this console.

The existing four-hook `default_tu5_patched.xex` can remain flashed or be rolled
back to stock — it is **neutral** to the DTA fix either way (its hooks are off the
RB3DX path).

---

## 5. Canary — optional, diagnostic only (no longer on the fix path)

The wave4 canary artifacts still exist and remain useful **only** to answer the
long-standing "did our image boot / is `OvershellPartSelectProvider` even the
provider feeding this list" question. They are **not needed to ship the DTA fix**.

- `default_tu5_canary_static.xex` — sha256 `053c79a6364923aff7f674f069ec31668f8ed30a264720a007822af34ad8e02a`
  — pure `.text`, 1 word, cannot crash. Blanks the part-row **names** if
  `OvershellPartSelectProvider::Text` renders this screen. A pure image/provider
  probe.
- `default_tu5_canary_wave4.xex` — sha256 `2a0588480faade2b24931c4f488db57c5f78e13ffa370cd9a1e57a24ace00835`
  — static + cave-routed dynamic (blank **icons** iff the `.data` cave executes).
  An NX probe.

Given the DTA root cause, running these is now a *curiosity* (confirming image +
provider + NX for the record), not a prerequisite. If the user wants zero extra
console cycles, skip them and go straight to the DTA ARK.

---

## 6. User test script (what to look for, in order)

**Primary test — the DTA fix:**
1. Build the patched RB3DX ARK (§3b) and deploy `gen/patch_xbox.hdr` +
   `gen/patch_xbox_0.ark` to the console (back up the originals first).
2. Boot, go to the 2-player instrument-select / song-settings overshell.
3. P1: pick Guitar (5-lane). P2 (guitar controller): highlight Guitar, press A.
   - **PASS:** P2 advances to the **difficulty** screen (no bounce). Both players
     end up on Guitar. This is the fix working.
   - **FAIL (still bounces, click-then-nothing):** the deployed ARK did not take —
     confirm you replaced the DX patch ARK the console actually loads (mount order:
     patch ARK must win over base), and that the DTB recompiled (check build log
     for `slot_states.dtb` / `dx_overshell_funcs.dtb`).
4. Start the song with both players on Guitar.
   - **PASS:** song plays, both guitar lanes get gems. Done.
   - **FAIL (crash/hang at song start, or one lane steals the other's notes):**
     the downstream native path needs the assist — see §6a.

**Optional canary (only if you want the image/provider/NX record):**
- Flash `default_tu5_canary_static.xex`, open a guitar part list. Blank **names**
  = our image + `OvershellPartSelectProvider` live. Normal names = different image
  or provider.
- Then `default_tu5_canary_wave4.xex`: blank **icons too** = `.data` cave executes
  (NX exonerated); crash on opening the list = NX convicted.

### 6a. Assist patch, only if step 4 crashes/steals notes
If two *identical* parts crash at song start or steal notes on retail TU5, the
downstream native functions become the needed assist (these are the wave1–3
Layer-C/D hooks, whose TU5 VAs are already pins-confirmed):
- `PlayerTrackConfigList::ProcessConfig` @0x8276FA08 (prevents the 2nd same-type
  claimant assert),
- `TrackWatcherImpl::RecalcGemList` @0x82794740 (prevents note-stealing).

Deliver those two via the RB3Enhanced DLL runtime-patch path (or a targeted XEX
poke) — **not** the `.data` cave, since NX-executability of the cave is unproven.
Do this **only if the on-console test in step 4 actually fails**; RB3DX already
ships cross-family dupes working end-to-end, so retail TU5 may not assert at all
(MILO_FAIL is typically compiled out of retail).

---

## 7. Rollback

- **DTA fix:** restore the backed-up `gen/patch_xbox.hdr` + `gen/patch_xbox_0.ark`
  (the pre-edit RB3DX ARK). No xex change means the xex is unaffected.
- **XEX (if canaries were flashed):** reflash any known-good xex —
  `default_tu5_sipatched.xex` or `default_pre_si.xex` — on the console.

---

## 8. Open risks

1. **Exact-same-part song start on retail TU5 (§6a).** Unknown until tested;
   RB3DX's existing cross-family dupe support suggests it is likely fine, but the
   `ProcessConfig` assert path was authored for the debug build. Mitigation ready
   (§6a) if it bites.
2. **ARK mount/priority.** The console must load the *rebuilt* patch ARK, not a
   stale cached copy or a higher-priority ARK. Verify the DX patch ARK is the one
   the game mounts for `ui/overshell/slot_states`.
3. **RB3DX version skew.** This analysis is against RB3DX HEAD `9635d57`
   (`develop`). If the console runs a different RB3DX build, re-confirm
   `dx_check_for_dupe` still exists and the three call sites are unchanged before
   applying (they have been stable for a long time, but verify).
4. **Vocals nuance.** `dx_check_for_dupe` has a `{$user is_local}` extra clause for
   vocals; forcing `TRUE` also allows two local vocalists on the same harmony part.
   If undesired, use the per-call-site fallback (§3a) and leave the vocals states
   alone. For guitar/bass/keys/drums this is a non-issue.

---

## 9. Evidence index
- Gate function: `/tmp/rb3dx` (HEAD 9635d57) `_ark/dx/overshell/dx_overshell_funcs.dta:37-118`
- Call sites + deprecated vanilla: `_ark/ui/overshell/slot_states.dta:3196,3227,3253` and `:3199,3230,3256`
- Lane checkpoints: `/tmp/si-hw-fix/wave4/{rb3dx-dta,wii-path,canary,review}.json`
- Canary artifacts + shas: §5 above; `.writes.json` sidecars alongside each xex
- Build system: `/tmp/rb3dx/scripts/build_xbox.bat`, `dependencies/python/configure_build.py`, `dependencies/{linux,windows}/{dtab,arkhelper,ninja}`
