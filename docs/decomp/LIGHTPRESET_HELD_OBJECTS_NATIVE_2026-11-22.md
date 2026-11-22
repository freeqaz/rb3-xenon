# Native LightPreset holds the objects its four lists name (2026-11-22, lane `lightpreset`)

Closes the `world/LightPreset.cpp` gap that `SCRIPTTASK_WATCHED_OBJECTS_NATIVE_2026-11-22.md`
(branch `scripttask`) found and deferred. Natively, `mSpotlights`, `mEnvironments`, `mLights`
and `mSpotlightDrawers` were raw `std::vector`s whose `AddRef`/`Release` calls were
`#ifndef HX_NATIVE`. Nothing was linked into the held object's ring, so a Spotlight,
RndEnviron, RndLight or SpotlightDrawer that died before its preset stayed in the preset as a
dangling pointer. Retail removes the dead entry or retargets it.

## Retail (clean TU5 `band.exe`, read with `python3 tools/va_disasm.py`)

I scanned 0x824AA400..0x824B8860 for `bl` to `Hmx::Object::AddRef(ObjRefOwner *)`
(0x8275BD08) and `Release` (0x8275B378). In every hit the preset is the owner:

| function | AddRef | Release |
|---|---|---|
| `AddEnvironment` 0x824B3260 / `AddLight` 0x824B3358 / `AddSpotlightDrawer` 0x824B3430 / `AddSpotlight` 0x824B5768 | 0x824B32A0, 0x824B33A0, 0x824B3478, 0x824B57BC | |
| `Copy` 0x824B7738 (each copied entry, no null test) | 0x824B7820, 0x824B789C, 0x824B7928, 0x824B79B4 | |
| `Load` 0x824B7AE8 (each entry `Find` returned non-null) | 0x824B7CAC, 0x824B7DCC, 0x824B7EFC, 0x824B832C | |
| `RemoveEnvironment` 0x824B0230 / `RemoveLight` 0x824B0328 / `RemoveSpotlightDrawer` 0x824B0430 / `RemoveSpotlight` 0x824B20B8 (if non-null) | | 0x824B02FC, 0x824B0404, 0x824B050C, 0x824B2194 |
| `Clear` 0x824B7018 (all four lists) | | 0x824B7094, 0x824B70FC, 0x824B7174, 0x824B71DC |
| `Replace` 0x824B29B8 | 0x824B2C84 | 0x824B2AAC, 0x824B2AF8, 0x824B2BB8, 0x824B2C30 |

`LightPreset::Replace` 0x824B29B8 searches the spotlight, environment, light and drawer lists in
that order, comparing each entry with `from`. With `to == 0` it calls that list's `Remove*`
(`bl 0x824B20B8` / `0x824B0230` / `0x824B0328` / `0x824B0430`), which erases the entry, its
keyframe slots and its state slot, and Releases the object. With `to != 0` it Releases `from`,
stores `__RTDynamicCast(to)` (`bl 0x8282A0C8`) and AddRefs `to` (the uncast pointer, r3 = r28).
Both paths then call `CacheFrames` (`bl 0x824B0FD8` at 0x824B2C9C). If nothing matches, it
returns without chaining to the base.

## Fix (`src/system/world/LightPreset.{h,cpp}`)

- **Native only:** each hold is a heap `ObjOwnerPtr<Hmx::Object>` ring node owned by the
  preset, in `mHeldRefs`. `HoldRef(obj)` is native AddRef (one new node). `DropRef(obj)` is
  native Release (it frees the first node naming `obj`). This is the per-entry heap-node idiom
  `CharClip::Transitions` uses natively, because the lists are indexed vectors that
  `ObjPtrList` cannot replace.
- **All 28 source sites** now use one spelling, `PRESET_ADDREF(x)` / `PRESET_RELEASE(x)`.
  Natively these are `HoldRef`/`DropRef`. On X360 they expand to the same
  `(x)->AddRef(this)` / `(x)->Release(this)` tokens as before. The 28 source sites correspond
  to the 25 retail calls above; retail merges Replace's four AddRefs into the one at
  0x824B2C84. Lines that mention `HX_NATIVE` in the file went from 31 to 10. (The fix commit
  message says "26 sites"; 28 is the correct count.)
- **Native `Replace`:** `from` is a node, not the object. The function finds the node, moves
  it to the front of `mHeldRefs` so that the Release inside the retail body (or inside
  `Remove*`) frees this node and not another node naming the same object, takes the object
  from the node, and runs the retail body unchanged. A ref that is not a held node goes to
  `Hmx::Object::Replace` (the native `mSinks` forwarding, as in `CharClip::Replace` and
  `ScriptTask::Replace`). If the object is in none of the four lists, the node is dropped:
  retail's ring walk has already taken the preset off that object's list.
- **`~LightPreset`** frees any node still left after `Clear`. That happens when a replacement
  failed its cast: retail stores null in the slot but keeps the AddRef on `to`.
- **Kept native divergence:** native `Clear` still empties `mSpotlightDrawers`. Retail
  Releases the drawers and leaves the pointers in place for `Load` to resize over. A rev <= 9
  `Load` never reads drawers, so on retail those unheld pointers would stay. I did not adopt
  that behaviour.

## Gate (`native/src/lightpreset_refs_phase.cpp`, run by rb3-milo)

The gate builds a preset that holds one object of each kind. It uses `PresetProbe`, a subclass
that exposes the protected `Add*` helpers. rb3-milo has no renderer and its `TheRnd` is
zero-filled, which forced two more probes. The SpotlightDrawer factory makes an
`NgSpotlightDrawer` whose constructor creates an RndCam, so the gate uses a base
`SpotlightDrawer` instead. `~RndFlare` calls `TheRnd.RemovePointTest`, so the Spotlight probe
leaves its flare alive and leaks it (rb3-milo and the ASan driver run with LSan off).

| gate | before fix | after |
|---|---|---|
| `lightpreset-ring-holds-refs` (each object's ring names the preset once) | **FAIL** 0/0/0/0 | PASS 1/1/1/1 |
| `control-lightpreset-delete-preset` (preset dies → no ref of it remains) | PASS | PASS |
| `control-lightpreset-unheld-delete` (an object the preset does not hold dies → lists intact) | PASS | PASS |
| `lightpreset-delete-{spotlight,environ,light,drawer}` (plain delete → that list and its state 1 → 0, others kept) | **FAIL** ×4 (1 → 1, dangling) | PASS ×4 |
| `lightpreset-cascade-{…}` (the object dies in its dir's `~ObjectDir` cascade) | **FAIL** ×4 | PASS ×4 |
| `lightpreset-replace` (A->ReplaceRefs(B): slot names B; preset's refs A 0, B 1) | **FAIL** (slot A, 0/0) | PASS |
| `lightpreset-replace-then-delete` | **FAIL** | PASS |
| `lightpreset-copy-refs` (Copy AddRefs each entry) | **FAIL** 0/0/0/0 | PASS 1/1/1/1 |
| `lightpreset-copy-delete` (a light dies → gone from both presets) | **FAIL** | PASS |
| `lightpreset-load-refs` (Save, then Load in the same dir: each kept entry held) | **FAIL** 0/0/0/0 | PASS 1/1/1/1 |
| `lightpreset-load-delete` | **FAIL** | PASS |
| `lightpreset-cascade-all` (two presets and their objects in one dir, deleted; smoke) | PASS | PASS |

I predicted the outcome before the runs: the 15 non-control gates fail before the fix and pass
after it. That is what happened. The pre-fix run printed `RESULT: FAILED (15 gate failure(s))`,
and those 15 were exactly the new gates. The post-fix run printed
`RESULT: ALL GATES PASSED (0 gate failure(s))`. PASS lines went from 43 to 58. Logs are
`~/tmp/rb3_lightpreset_milo_{pre,post}.log`, both run through `no610m.sh`. The census
classification has a control: without its `NON_STUB_RE` line, `tools/native_stub_census.py`
exits 3 on `lightpreset_refs_phase.cpp.o`. With the line it exits 0.

**ASan.** I built rb3-milo with `-DRB3X_SANITIZE=address` (build dir `~/tmp/lp-asan-build`)
and ran it through `no610m.sh` with native_asan.sh's `ASAN_OPTIONS` (`halt_on_error=0`,
`detect_leaks=0`). It reported 0 `ERROR: AddressSanitizer`, passed all 18 lightpreset gates and
printed `RESULT: ALL GATES PASSED`. The run covers the deletion of each held object, both
cascades, and the case where two presets and their objects die in one dir. Log:
`~/tmp/rb3_lightpreset_milo_asan.log`. That the gates passed also shows this build compiled
the fixed source, even though the A/B was checking out its revert in the same worktree at the
time.

## X360 A/B

I ran `python3 tools/ab_measure.py --worktree ~/tmp/wt-lightpreset --revert 56ef239d4`. Leg A
has the fix and leg B reverts it, so the signs are leg B minus leg A; at 0 there is nothing to
invert. The ruler is `name_check` and objdiff is sha256:ec3badc0f33323e9. Both legs read
matched=55268, masked_equal=25360, honest=29908, code%=59.665310 and fuzzy 64.709015. On the
`none` ruler both read matched 55322 and code% 60.032020. Units at 100% stayed at 635 → 635
(mpn) and 548 → 548 (all-rows-fuzzy). Leg B recompiled 106 objs, which are the consumers of
LightPreset.h and LightPreset.cpp. Result: **Δmatched +0, Δhonest +0, Δcode_bytes +0,
Δfuzzy +0**. I predicted this before the run, because the X360 arm expands to the same tokens
and the new header members are `HX_NATIVE` only. Run record:
`~/tmp/wt-lightpreset/.ab_measure_runs/20261122-213614-56ef239d4-1943806/`.

## Native health

I started `no610m.sh tools/native_health.sh --selftest` and stopped it, on the coordinator's
instruction, while it was still in the link gate. Under `no610m.sh` this host gives the GPU
targets no adapter, so the run can only end at rc 3 (`nogpu`), as it did on the scripttask
lane. The coordinator's landing gate builds and runs all 19 targets with GPU access, so this
lane records **no** `NATIVE_GATE_RESULT` or `NATIVE_HEALTH_RESULT` line. Native evidence here
is limited to rb3-milo: the normal build and the ASan build, both above. rb3-render and
rb3-app also compile LightPreset (ENGINE_WORLD); this lane did not build them.

## Not done

- I did not test a failed-cast replacement (`to` of the wrong class).
- I did not add a test for retail's own stale-drawer state after `Clear`, since native
  deliberately does not reproduce it.
- `ReplaceRefsFrom` has no gate of its own here; `lightpreset-replace` uses `ReplaceRefs`.
