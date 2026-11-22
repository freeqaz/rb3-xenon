# CharClip transitions: native ref-counting (2026-11-22)

Branch `charclip` (worktree `~/tmp/wt-charclip`), from main `0140ad7c6`.
Source: the deferred row `char/CharClip.cpp:76,98,174,196,287` in
`docs/decomp/NATIVE_ARM_DIVERGENCE_SWEEP_2026-11-22.md` (branch `narms`).

## The defect

Retail ref-counts every clip a `CharClip::Transitions` table names against the
owning clip. Read on `band.exe` with `tools/va_disasm.py`:

| retail | what it does |
|---|---|
| `AddNode` 0x8237D258 | new vector: `bl 0x8275BD08` (`Hmx::Object::AddRef(mOwner)`), `Resize(+0x10)`; existing vector: `Resize(+8)`, memmove `+0x10`/`+8` |
| `Load` 0x8237E258 | one `AddRef` per clip found |
| `Clear` 0x8237D1E8 | `bl 0x8275B378` (`Release(mOwner)`) per vector, then `Resize(0)` |
| `RemoveClip` 0x8237D3C8 | `Release`, then drop the vector |
| `CharClip::Replace` 0x8237D810 | `addi r3,r3,0x28; b Transitions::Replace` |
| `Transitions::Replace` 0x8237D470 | find the vector whose clip is `from`; `dynamic_cast<CharClip*>(to)`; NULL -> `RemoveClip`, else `Release` old, store, `AddRef` new |

So when a named clip dies, `~Object`'s `ReplaceRefs(0)` reaches the owner's
`Replace` and the table drops it. The native arm compiled every
`AddRef`/`Release` out and `CharClip::Replace` was X360-only, so natively a
deleted clip stayed in every table naming it: a dangling `CharClip *` that
`Save` (`clip->Name()`), `CharDriver` and `ClipDistMap` read. The native
`Transitions::Replace` arm that existed (it cast `from` to a `NodeVector *`)
was dead: nothing could call it.

## A second native defect, found by the gate

The first call of the new gate crashed (SIGSEGV in `AddNode`, before any ref
logic): `AddNode` spelled X360's 8-byte `NodeVector` header as literals
(`+0x10`, `+8`/`+16`). Natively the header is 12 bytes (8-byte clip pointer),
so a new vector was allocated 4 bytes short and the next `AddNode` on that clip
memmoved a negative length. Fixed first, in its own commit, with a
`const int kHeader` that is `offsetof(NodeVector, nodes)` natively and `8` on
X360. In shipped data `AddNode` runs only for pre-rev-8 clip files and editor
paths (`ClipDistMap`), which is likely why nothing hit it.

## The fix (src/system/char only)

- Native `NodeVector` gains `ObjOwnerPtr<CharClip> *ref` after `size` (header
  24 bytes natively, so vectors stay 8-aligned; X360 layout unchanged). It is
  heap-held: `Resize`/memmove of the vectors never moves a linked ring node,
  which is what made dc3-decomp's embedded `ObjOwnerPtr` need ring relinking
  and deferred compaction.
- `HoldClip`/`DropClip` sit exactly at retail's `AddRef`/`Release` sites
  (`AddNode` new vector, `Load`, `Clear`, `RemoveClip`). `HoldClip` constructs
  the pointer empty and then assigns, so `ObjOwnerPtr`'s X16 self-seed stays
  NULL (the seed is only for `mX(this, this)` members).
- `CharClip::Replace` now compiles on both builds. Natively a ref that is not a
  transition node goes on to `Hmx::Object::Replace` (which forwards to
  `mSinks`; retail's base `Replace` is empty).
- `Transitions::Replace` is one loop on both builds; natively it matches the
  vector by its node (`it->ref == from`, since `ObjOwnerPtr::Replace` forwards
  itself) and runs retail's two branches.
- Teardown paths: plain `delete` (`ReplaceRefs` snapshot) and the `~ObjectDir`
  cascade (`NullifyAllRefs`: `ObjOwnerPtr::ControlOwner` is the owning clip, so
  its `Replace(nullptr)` runs from the drained `pending` ring) both reach
  `RemoveClip`, which deletes the node; `~ObjRefConcrete` unlinks it with
  `SafeReleaseFromRing` inside a cascade.

## The gate

`native/src/charclip_refs_phase.cpp`, run by `rb3-milo` after the A9 phase
(`native/src/main_milo.cpp`, `native/CMakeLists.txt`). Each case builds clip A
with two transition nodes to clip B.

PREDICTION before running: on the unfixed arm, ring-holds-ref, delete-target,
cascade-target, replace-target and replace-then-delete fail; delete-owner and
cascade-both pass (controls).

Measured with `native/build/rb3-milo <assets> ui/track/gen/tracksystem_meshes.milo_xbox`
through `no610m.sh` (non-ASan build, the configuration `native_health.sh` uses):

| gate | before ref fix (`2c5095de7`) | after (`832ccc6c5`) |
|---|---|---|
| charclip-ring-holds-ref | FAIL: B's ring holds 0 refs from A | PASS: 1 |
| charclip-delete-owner (control) | PASS: 0 | PASS: 0 |
| charclip-delete-target | FAIL: A names 1 clip after `delete B` (dangling) | PASS: 0 |
| charclip-cascade-target | FAIL: A names 1 clip after the cascade (dangling) | PASS: 0 |
| charclip-replace-target | FAIL: B still named, C not, 0 refs | PASS: C named with 2 nodes, refs C 1 / B 0 |
| charclip-replace-then-delete | FAIL: 1 | PASS: 0 |
| charclip-cascade-both (smoke) | PASS | PASS |
| rb3-milo `RESULT` | `FAILED (5 gate failure(s))`, rc=1 | `ALL GATES PASSED`, rc=0 |

The prediction held exactly. Before the AddNode fix (`2c5095de7`'s parent
plus the gate) the phase did not get as far as a gate: SIGSEGV in
`CharClip::Transitions::AddNode` (gdb backtrace: `Link` -> `AddNode`).

## X360 price

`python3 tools/ab_measure.py --worktree ~/tmp/wt-charclip --patch <git diff main HEAD -- src/> --label charclip-native-refcount`
(both src commits, on the shipped `name_check` ruler, objdiff `sha256:ec3badc0f33323e9`;
run dir `.ab_measure_runs/20261122-173818-charclip-native-refcount-3096091`):
leg A and leg B both matched=55265, masked_equal=25360, honest=29905,
code%=59.622105 (`none` ruler 55321 / 60.015200 on both legs); leg B
recompiled 430 objects. **Δ0 on every measure**, as expected: the X360 edits to
`Transitions::Replace`, `Clear` and the header preprocess to the same tokens,
and `AddNode`'s `kHeader` folds to the old literals.

ASan: `native/build-asan` (`-DRB3X_SANITIZE=address`, fixed tree), same
command with `ASAN_OPTIONS=halt_on_error=0:detect_leaks=0`: all 7 gates PASS,
rc=0, **0** `ERROR: AddressSanitizer` reports in the whole run. No positive
control was run for ASan (the gates read table sizes and ring counts, never a
dangling clip), so this shows only that the new teardown paths -- node delete
inside `ReplaceRefs`, inside the cascade's `pending` drain, and the A<->B/A->A
dir -- touch no freed memory.

## Not done

- No native health selftest (per brief); only `rb3-milo` was built and run.
- Only the synthetic gate exercises the new paths. No venue/character load
  (rb3-render, rb3-app) was run to see real `.milo` transition tables load and
  unload under the fix; `Load`'s `HoldClip` is covered by inspection, not by a
  gate.
- Retail's `AddRef` skips a ref owned by the target itself (a self-transition
  A->A registers nothing); natively A's own ring holds the node. It is deleted
  by `~Transitions` before `~Object` walks the ring, and `cascade-both`
  exercises it; not mirrored further.
- `Load`'s native temp-buffer sizing (`temp*4`, X360 byte count) was left: with
  the 24-byte native header a vector needs at most 3x its X360 bytes.
