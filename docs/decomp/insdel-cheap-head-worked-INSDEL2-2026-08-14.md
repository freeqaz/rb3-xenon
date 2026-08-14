# `SOURCE_INSDEL` cheap head, second pass — lane INSDEL-2 (2026-08-14)

Tree `a7ba9316` (INSDEL-1 merged) + this lane. Ruler `functionRelocDiffs=name_check`,
read from `report.json` and re-confirmed by driving `objdiff-cli` with the grader's
exact config on every per-row reading.

## Result — +5 functions / +672 B, prediction pre-registered and EXACT

| measure | predicted | measured |
|---|---:|---:|
| Δ`matched_functions` | +5 | **+5** |
| Δ`matched_code` | +672 B | **+672 B** |
| Δ`matched_code_percent` | +0.00651 pp | **+0.006512 pp** |

Leg A `matched` 44,410 · `code%` 36.132328 → leg B 44,415 · 36.138840.
0 regressions · 0 units fell off 100% · unit net +5 == whole-binary +5, in exactly
the five edited units · Native gate **PASS 18/18, 0 SKIPs, rc=0**.

## The dominant find: RB3 has TWO temp-allocation guards and we spelled the wrong one

Three of the five closures are one defect class, and it was found by **reading an
in-source note before touching anything** — `src/system/utl/MemMgr.h`, written by an
earlier lane, already documents both constructs:

| guard | shape | call site |
|---|---|---|
| `MemTemp` | out-of-line ctor/dtor, homes an `int mOld` | `addi r3, <frame>` + `bl <ctor>` |
| `MemDoTempAllocations` | **EMPTY**, inline ctor/dtor each calling one out-of-line helper | a **bare** `bl <helper>`, no this-setup, **no stack slot** |

Rows spelling `MemTemp tmp;` where retail uses the empty guard carry a fixed
signature: **two surplus `addi r3, r31, 0x50`** (scope entry + exit) plus a
`diff_arg` where target has `bl ?MemPushTemp@@YAXXZ` and we have
`bl ??0MemTemp@@QAA@XZ`. Our body is **exactly 8 bytes long** in every instance.

```
?InsertDataEvent@MidiParser@@AAAXMMABVDataNode@@@Z   232 B  96.466 -> 100
?PushBack@@YAXABVDataNode@@@Z                        132 B  93.788 -> 100
?LoadShaderBuffer@RndShaderProgram@@QAAX...          108 B  92.407 -> 100
```

**The evidence is an IDENTITY argument, not a shape one.** The map resolves
`?MemPushTemp@@YAXXZ` to **`0x827BC270`** — precisely the push-helper address
MemMgr.h names for `MemDoTempAllocations` — and the scope-exit call targets
**`fn_827BC2A0`**, its pop-helper. Independently, a whole-binary scan of the target
asm (keyed on `.fn fn_<addr>`, never the synthetic address column) finds **54
functions calling that pair and ZERO calling the out-of-line `MemTemp` ctor/dtor**.

⚠ **The oracle could not have settled this and would have agreed with us**:
`src/system` is a verbatim DC3 copy and DC3 is the newer tree, where the
push/pop/`MemTempRefs` mechanism is the DC3-side divergence. Same trap INSDEL-1 hit
on `DataReadFile` and INSTR-1 on `RndParticleSys::UpdateParticles`.

### ⛔ The remaining ~16 `MemTemp` sites are NOT a funded multiplier

Per-function call counts corroborate several more sites — `DataArray.s`'s single
guarded function calls the push-helper **5 times**, exactly matching the 5 `MemTemp`
scopes in our `DataArray::Load` — but **those functions are ANONYMOUS in the map**
(`Symbol not found in target`). They cannot pair, so the fix is unverifiable by
objdiff and buys **0 bytes**. Left alone deliberately.

⚠ **Correction to an inference worth not repeating:** "absent from the mpn<100
census ⇒ the row is clean" is **FALSE**. Every one of those sites is absent because
it is *unidentified*, not because it matches. The metric is blind there.

## The other two closures

- `?IsLoaded@MetaPanel@@UBA_NXZ` (80 B) — **surplus source guard**. Source read
  `UIPanel::IsLoaded() && mMusic && mMusic->Loaded()`; the two surplus instructions
  were exactly the `&& mMusic` test. Decisive retail evidence **by absence**: no
  null test of `mMusic` anywhere in the body.
- `??0NetStream@@QAA@XZ` (120 B) — **surplus member initializer**. `mSocket(nullptr)`
  is not in retail's init list (retail emits `stb r11,16` with no preceding
  `stw r11,12`); the body assigns `mSocket` on the next line, so it is dead.

## ★★ Refinement of INSDEL-1's targeting rule — and its hit rate

INSDEL-1's rule was *"rank by whether the charge names a SOURCE CONSTRUCT or a
CODEGEN ARTIFACT"*. It held, but this lane found the word **"names" is doing
load-bearing work** and must be read strictly.

**7 rows opened and edited: 5 closed, 2 refuted (71%).** The split is clean and it
is *not* about size or charge count:

- **All 5 that closed had charges naming the construct 1:1** — a *different callee*,
  a surplus *store* matching a named initializer, a surplus *compare* matching a
  `&&` operand. You can point at the source token the charge corresponds to.
- **Both that failed were cases where I INFERRED a source cause from codegen
  BEHAVIOUR** — "retail re-reads the member ⇒ it must be `volatile`", "retail stores
  the global first and reloads ⇒ the source must write through the global". Both are
  plausible, mechanistically motivated, and **wrong**.

⇒ **Sharpened rule: the charge must NAME the construct, not IMPLY it.** A reload, a
store sinking, a materialization, a slot colouring is codegen *behaviour*; a
hypothesis about which source shape would produce it is a bet, and this lane went
0-for-2 on that bet. Rows whose diff shows a token-level substitution are the vein.

## Negative results (also recorded in-source at each site)

- **`Splash::WaitForState` — `volatile int mState` is REFUTED, and it is a trap.**
  It *does* close the row (96.552 → 100, +116 B), but simultaneously knocks
  `Splash::SetImmutableState` off 100 (100.0 → 95.455, 176 → 184 B) for a
  **NET −60 B**. ★ **Measuring only the target row would have landed a
  regression** — the compensating loss is in a *different function of the same
  unit* and is invisible without re-measuring the unit against a reverted control.
  `SetImmutableState` matching at 100 non-volatile is positive proof retail's
  member is a plain `int`.
- **`SpotlightDrawer::Init` — writing through the global is REFUTED.** MSVC sinks
  the store anyway and adds a `clrrwi r3,r3,0`: 94.107 → 90.536 (3 charges → 4).

## Deferred, with reasons (not drained — priced)

| row | B | why not opened |
|---|---:|---|
| `StoreMainPanel::FinishLoad` | 592 | frame-slot homing — the stack-slot class INSDEL-1 closed 0-for-3 |
| `TrackDir::~TrackDir`, `FftIpp::~FftIpp` | 468/196 | same slot-homing class |
| `??8DataNode@@QBA_NABV0@@Z` | 464 | retail tail-branches to an **external folded block** (`b fn_8274AD70`) we emit inline — tooling/merge, no source handle |
| `AccomplishmentProvider::Mat` | 360 | retail shares ONE constant-pool base across both `if` arms (`lfs -28(r11)`); we emit two `__real@` COMDAT loads |
| `FaderGroup::~FaderGroup` | 160 | real duplicate store, but it lives in `ObjPtrList::front()`/`pop_front()` **header** code — blast radius across every list user |
| `BandLabel::Copy` | 116 | generated by the shared `BEGIN_COPYS`/`CREATE_COPY` macro; lever ripples to hundreds of TUs |
| `LightPreset::ApplyState` | 108 | missing trailing call `bl fn_824AAE10`, callee unidentified |
| `MidiParser::PushIdle` | 484 | carries the guard defect **and** a frame-size delta + a 5-arg vs 2-arg `PoolAlloc` — body-port row, not cheap head |

## Tooling

`~/tmp/insdel2/show.py` (per-row charged-instruction dump at the shipped ruler,
inherited from INSDEL-1 with `WT` repointed). Per-row findings are recorded as
**in-source notes** at `MidiParser.cpp`, `DataFile.cpp`, `ShaderProgram.cpp`,
`MetaPanel.cpp`, `NetStream.cpp`, `Splash.h`, `SpotlightDrawer.cpp`.
