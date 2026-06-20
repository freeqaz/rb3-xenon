# W9 L10 — Land Handle prereq: a7175af then Family-A b3b419e

**Date:** 2026-06-20
**Mode:** Adversarial DISCOVER/PLANNER (Opus, layer 10), read-only in main.
**Verdict:** REAL_ACTIONABLE — composition is sound; emit one coordination-land
work-item (+217 → new main baseline ~8531) plus the downstream Handle/reveal
frontier this unblocks.

## Frontier item

`land-handle-prereq-a7175af-then-family-a-b3b419e` (kind=coordination-land, est +217):
fast-forward/cherry-pick **a7175af** (+196) onto main@812e1df, then **b3b419e**
(+21). Both single commits on parent 812e1df, disjoint files, compose cleanly.
Discard tier2 117cab5 (+130 strict subset). Unblocks the downstream Handle/reveal
wave.

## Ground truth (git-verified)

- `a7175af^ == b3b419e^ == 9fb9016^ == 117cab5^ == 812e1df` (all rooted on main HEAD).
- `d941d09^ == a7175af` — **d941d09 is a CHILD of a7175af, not a sibling.** The
  LEAD's "FF branch d941d09" therefore lands a7175af + d941d09 (a +0 cleanup), NOT
  a7175af + b3b419e. Cherry-pick is the unambiguous path; see work-item plan.
- `git diff a7175af 9fb9016` is **empty** — identical diff, confirmed.
- File sets are **disjoint**:
  - a7175af: `config/45410914/objects.json`, `scripts/target_symbol_map.json`,
    `src/system/obj/Object.h`, `src/system/ui/UIComponent.cpp`, `.../UIComponent.h`
  - b3b419e: `src/band3/bandtrack/VocalTrack.cpp`, `src/band3/game/VocalPlayer.cpp`,
    `src/system/bandobj/BandCamShot.cpp`, `src/system/beatmatch/GuitarController.cpp`,
    `src/system/obj/ObjMacros.h`
  → no file overlap; cherry-pick of the second never conflicts on the first.

## Two Handle macro families, separately gated (no cross-conflict)

- **Family-B = `Object.h` BEGIN_HANDLERS / END_HANDLERS** (a7175af). Timer gated
  behind a NEW `MILO_MESSAGE_TIMERS` (undefined by default = retail timer-OFF);
  opt-IN per-TU via `objects.json` `/DMILO_MESSAGE_TIMERS` for the 6 TUs whose
  retail Handle DID emit the timer: BandDirector, CharBoneDir, UI, Dir, Group,
  CharLipSync (all verified to include `obj/Object.h`, Family-B). END_HANDLERS /
  END_CUSTOM_HANDLERS now emit the comma-eval `(void)(PathName(this), sym)` tail
  directly (HX_NATIVE keeps the real notifier).
- **Family-A = `ObjMacros.h` BEGIN_HANDLERS / HANDLE_CHECK** (b3b419e). Timer gated
  behind `defined(MILO_DEBUG) && defined(HX_NATIVE)` (timer-OFF for the match
  build); opt-IN to timer-ON via per-TU `#undef BEGIN_HANDLERS` for 3 counterexample
  TUs: VocalTrack, VocalPlayer, BandCamShot. HANDLE_CHECK `_warn` arm emits the
  comma form `((void)("...", __FILE__, line_num, PathName(this), sym))` — matches
  rb3-Wii oracle release `Debug.h:151` `#define MILO_WARN(...) (void)(__VA_ARGS__)`.
  GuitarController drops its now-redundant per-TU `#undef MILO_WARN` + `#undef
  BEGIN_HANDLERS` (now the global match-build default); its Handle stays 97.81%
  (near-miss, not a 100% match → no regression risk).

Both headers define `BEGIN_HANDLERS`; a TU uses whichever it includes (Object.h
does NOT include ObjMacros.h; ObjMacros.h does NOT include Object.h).

## The one composition interaction — VocalTrack — is SAFE (net-zero)

`VocalTrack.cpp` is the ONLY TU including BOTH headers:
`#include "obj/ObjMacros.h"` (line 28) then `#include "obj/Object.h"` (line 29).
Last-defined wins → **Object.h's BEGIN_HANDLERS resolves** for VocalTrack.

- On **main@812e1df**: Object.h BEGIN_HANDLERS is unconditional **timer-ON** ⇒
  VocalTrack::Handle frame is timer-ON. `fn_82B71AA4` (40-byte String-dtor unwind
  funclet coupled to that frame) is **matched at 100.0% on main** (confirmed in
  `build/45410914/report.json`).
- Under **a7175af ALONE**: Object.h flips to timer-OFF default; VocalTrack is NOT
  in the 6-TU `/DMILO_MESSAGE_TIMERS` list ⇒ VocalTrack::Handle would be timer-OFF
  ⇒ **`fn_82B71AA4` would regress 100→~99.9** in the intermediate (a7175af-landed,
  b3b419e-not-yet) state.
- Under **COMBINED a7175af + b3b419e**: b3b419e adds a per-TU `#undef
  BEGIN_HANDLERS` + timer-ON redefine in VocalTrack.cpp at line 2733 — AFTER both
  includes and BEFORE the `BEGIN_HANDLERS(VocalTrack)` usage (line 2742). This
  wins unconditionally ⇒ VocalTrack::Handle is timer-ON = matches main = **net-zero
  for VocalTrack**, `fn_82B71AA4` protected.

**Consequence for landing order:** the per-commit A/B numbers do NOT simply add in
the intermediate state. Specifically, landing a7175af FIRST and measuring before
b3b419e may show VocalTrack down by 1 (fn_82B71AA4). This is a *transient* dip that
b3b419e closes. The coordinator should treat the **composed** A/B (both landed) as
the gate: headline net must == +217 (8314 → 8531), not gate the intermediate.
(If a strictly-monotonic intermediate is desired, land b3b419e FIRST — its files
are disjoint and it is net +21 standalone with no dependence on a7175af; then
a7175af, whose VocalTrack regression is pre-empted by b3b419e's already-present
restore. Either order composes to the same final tree; b3b419e-first avoids the
transient dip.)

## Other safety checks (all clear)

- 7 new `target_symbol_map.json` VAs (CharIKFoot, ModalKeyListener, WorldInstance,
  UIComponent::OnGetResourcesPath, UIComponent::Handle, UIListSlot, UIListCustom)
  have **zero collisions** with existing main entries.
- Macro-redefinition pattern (Object.h redefining BEGIN_HANDLERS without `#undef`
  after ObjMacros.h already defined it) **already exists on main** for VocalTrack
  and builds fine — no NEW redefinition-warning risk introduced.
- Only one TU on main carries a per-TU `#undef BEGIN_HANDLERS`: GuitarController
  (b3b419e removes it). The other per-TU `#undef MILO_WARN/NOTIFY` TUs (Gem, Utl,
  Bitmap, BinStream) have **no Handle region** and don't include ObjMacros.h ⇒ the
  macro changes don't touch them.
- a7175af's UIComponent.cpp TU-local `#pragma push_macro("MILO_NOTIFY")` redefine
  is dead-but-harmless (END_HANDLERS no longer uses MILO_NOTIFY). d941d09 (+0,
  child of a7175af) removes it; optional to land, strictly cleaner.
- 117cab5 (+130) touches Object.h only and is a **strict subset** of a7175af
  (+196). Discard confirmed correct.

## Verify command (whole-binary honest A/B)

```
rm -f build/45410914/target_symbol_renames.stamp \
  && touch config/45410914/config.yml \
  && NINJA_JOBS=8 tools/fresh_report.sh
# read measures.matched_functions; re-run once for splits-only FP clearance.
```

Expected new main baseline: **8531** (8314 + 196 + 21).

## Downstream frontier this unblocks (next layer)

364 TUs contain a `BEGIN_HANDLERS`/`BEGIN_CUSTOM_HANDLERS` Handle region. The two
macro fixes make the Handle frame byte-shape correct binary-wide (timer-off default
+ surviving PathName tail). a7175af already revealed 7 now-byte-exact Handle fns via
new symbol-map entries; the SAME reveal pattern likely flips dozens more Handle fns
in already-pinned Handle-bearing TUs that just lack a `target_symbol_map.json` entry
(self-validating reveal: anon `fn_<addr>` already byte-exact, reads 0% without the
name). This is the "downstream Handle/reveal wave." Candidate-rich pinned TUs (have
Handle region, in objects.json): GameMode, GemTrack, AccomplishmentProgress,
OvershellSlot, Player, GamePanel, AccomplishmentManager, CharIKHead, CharLipSyncDriver,
CharBonesBlender, Character, FileMerger, the Char* family, the Flow* family. Seeded
as a discovered_frontier item for a later layer (recon-gate the reveal sweep AFTER
this land establishes the 8531 baseline).
