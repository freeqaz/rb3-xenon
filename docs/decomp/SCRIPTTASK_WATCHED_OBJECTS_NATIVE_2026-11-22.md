# Native ScriptTask watches the objects its script names (2026-11-22, lane `scripttask`)

Closes the `obj/Task.cpp` row deferred by `NATIVE_ARM_DIVERGENCE_SWEEP_2_2026-11-22.md`:
natively, a `ScriptTask` did not register on the objects it watches, so when one of them died
the task did not end. Only `mThis` was tracked.

## Retail (clean TU5 `band.exe`, read with `python3 tools/va_disasm.py`)

- `ScriptTask::UpdateVarsObjects` 0x827497C0: for each object not already in `mObjects` and not
  `mThis`, `list::insert` (0x8274995C) and then `bl 0x8275BD08`
  (`Hmx::Object::AddRef(ObjRefOwner *)`) at **0x82749968**, with r3 = the object and r4 = the
  task.
- `~ScriptTask` 0x82749F28: a loop at 0x82749F60..0x82749F74 calls `bl 0x8275B378` (`Release`)
  on every `mObjects` entry, with r4 = the task.
- `ScriptTask::Replace` **0x82749450**: it returns if `to != 0`. Otherwise, if `from == mThis`
  (`0x40(r3)`) or `from` is in `mObjects` (the list walk at 0x82749464..0x82749490), it calls
  the scalar-deleting dtor (vtable slot 0, `li r4,1`).
- `ThreadTask::Replace` **0x827494B0**: if `mExecuting` (`0x58`) is zero, it tail-calls
  `ScriptTask::Replace`. Otherwise it tail-calls `list::remove_if` on `mObjects` (0x82748100):
  it only drops the object, and the task survives.

## Native before the fix

`mObjects` was a `std::list<Hmx::Object *>` of raw pointers that were never linked into any
ring (the native ring is node-model, so `AddRef(this)` does not apply). The object's death never
reached the task. `ThreadTask::Replace` compared the ring node `from` against raw object
pointers, so it never matched. The old native `ScriptTask::Replace` also fell through to
`delete this` after `Hmx::Object::Replace` for any non-`mThis` ref. That was unreachable, since
the task held no other refs.

## Fix (HX_NATIVE only; the X360 arm is unchanged token for token)

Natively, `mObjects` is an owner-control `ObjPtrList<Hmx::Object>`, the idiom `RndGroup` and
native `TypeProps` already use. `push_back` links one node into the watched object's ring. On
the object's death the node forwards `Replace(node, 0)` to the task, both through
`ReplaceRefs` (a plain delete) and through the `NullifyAllRefs` owner-step drain (an
`~ObjectDir` cascade). The native `Replace` maps `from` to its node:

- `to == 0`: delete the task (0x82749450).
- `to != 0` (`ReplaceRefsFrom`): erase the node. In retail the ring walk has already taken the
  task off the old object's list, and the empty `Replace` body never adds it to the new one, so
  the task stops watching both objects.
- `ThreadTask` while executing: erase only that node (0x827494B0 `remove_if`).

`~ScriptTask` needs no body: the list's destructor unlinks every node, which is the native form
of retail's `Release` loop.

## Gate (`native/src/script_task_watch_phase.cpp`, run by rb3-milo)

| gate | before fix | after |
|---|---|---|
| `control-scripttask-this-delete` (mThis dies → task deleted) | PASS | PASS |
| `control-scripttask-unwatched-delete` (an unwatched object dies → task lives) | PASS | PASS |
| `scripttask-watched-delete` (plain delete of the watched object) | **FAIL** (task still live) | PASS |
| `scripttask-watched-cascade` (watched object dies in `~ObjectDir`'s cascade) | **FAIL** | PASS |
| `scripttask-reflist` (the watched object's ref count is +1 while watched, back to base after `~ScriptTask`) | **FAIL** (0/0/0) | PASS (0/1/0) |
| `threadtask-executing-drop` (the thread's own `{delete $v}` kills a watched object; the task stays live, watched 1 → 0) | **FAIL** (1 → 1) | PASS |

I predicted these results before the runs and they matched. The pre-fix run printed
`RESULT: FAILED (4 gate failure(s))`, and those four were the only failures in it. The post-fix
run printed `RESULT: ALL GATES PASSED`. PASS lines went from 42 to 46 (logs
`~/tmp/rb3_scripttask_milo_{pre,post}.log`, both run through `no610m.sh`).

## X360 A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-scripttask --revert 0ead2b474`: leg A has the
fix and leg B reverts it, so the signs below are leg B minus leg A, and there is nothing to
invert at 0. The ruler is `name_check` and objdiff is sha256:ec3badc0f33323e9. Both legs read
matched=55268, masked_equal=25360, honest=29908, code%=59.665310 and fuzzy 64.709015. Units at
100% went 635 → 635. Leg B recompiled 631 objs. Result: **Δmatched +0, Δcode_bytes +0, Δfuzzy +0**,
as predicted. The X360 arm is the same tokens, and only its comment moved. Run record:
`~/tmp/wt-scripttask/.ab_measure_runs/20261122-210527-0ead2b474-525366/`.

## Native health

I ran `no610m.sh tools/native_health.sh --selftest` in the worktree with the fix committed:

```
NATIVE_GATE_RESULT verdict=PASS expected=19 verified=19 skipped=0 partial=0 failed=0 rc=0
NATIVE_HEALTH_RESULT verdict=INCOMPLETE link=PASS link_verified=19 link_expected=19 link_skipped=0 runtime=INCOMPLETE runtime_ran=16 runtime_total=18 gates_pass=133 gates_fail=0 unrunnable=rb3-frame:nogpu,rb3-render:nogpu selftest=PARTIAL scatter_unlinked=10 scatter_dirb=1 scatter_multihost=10 rc=3 handpose_controls=0/3 handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none retail_config=12/12 unhandled_calls=0
```

rc=3 is an environment problem, not this change. rb3-milo ran and passed all 46 of its gates
(`OK rb3-milo -- rc=0, 46 gate(s) passed`). The two unrunnable targets (rb3-frame, rb3-render)
need a GPU. The host's NVIDIA kernel module is 615.78.08 and its userspace is 620.70.06
(`nvidia-smi`: "Driver/library version mismatch"), so the NVIDIA Vulkan ICD does not load.
Inside `no610m.sh` that leaves no adapter, and rb3-render falls back to the Null backend
(`[FAIL] gpu-real-adapter`). The PARTIAL selftest is the three handpose controls, which need
rb3-render; the other 20 controls went red as designed. The health run should be repeated after
the host reboots.

## Other users of the same arm (retail `Object::AddRef(ObjRefOwner *)` with the owner itself)

I surveyed every `AddRef(<owner>)` / `Release(<owner>)` call site in `src/system`, `src/band3` and
`src/network/net`:

- **Native equivalent present:** `MsgSource` (`MsgSourceSinkRef`, W16-UF), `TypeProps`
  (owner-control `ObjPtrList`), `DirLoader::mProxyDir` (native `ObjOwnerPtr`),
  `CharClip::Transitions` (native per-node `ref`; gated by `charclip_refs_phase`). The
  `ObjPtr`/`ObjPtrList`/`ObjDirPtr` specializations in `BandCharacter.cpp`,
  `TrackPanelDirBase.cpp`, `UIFontImporter.cpp` and `Dir.h` are smart pointers that are their
  own native ring node.
- **Same gap, not fixed here: `world/LightPreset.cpp`.** Retail AddRefs every entry of
  `mSpotlights`, `mEnvironments`, `mLights` and `mSpotlightDrawers` (in `Copy`, `Load` and the
  `Add*` helpers, released in the `Remove*` helpers), and `LightPreset::Replace` removes a dying
  entry or retargets it. Natively all four are raw `std::vector`s whose AddRef/Release are
  `#ifndef HX_NATIVE`, and the native `Replace` casts `from` to `Hmx::Object *`, which is a
  ring node natively. So a Spotlight, RndEnviron, RndLight or SpotlightDrawer that dies before
  its preset leaves a dangling pointer in the preset. LightPreset is compiled natively
  (ENGINE_WORLD, so rb3-render and rb3-app). The fix needs a ring node per entry across those
  four vectors and about 31 native guards, plus its own gate. It is a separate lane.
