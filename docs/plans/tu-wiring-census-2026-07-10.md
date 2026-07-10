# TU-Wiring Census — orphan map-entry analysis (2026-07-10)

Merge-base: `8ae9244e` (pin audit round 3, →15399 matched).
Worktree: `~/tmp/wt-tucensus`. Tools: `scripts/tu_wiring_census.py`,
`tu_wiring_cluster.py`, `tu_wiring_rank.py`, `tu_wiring_byunit.py`.

## TL;DR — the "~3,900 orphans = unwired TUs" premise is REFUTED

Round-3 close-out estimated ~3,900 unpinned map entries needing TU wiring. The
census re-derives the exact set and finds it is **not** a wireable-TU vein:

| bucket | count | actionable? |
|---|---|---|
| total map entries | 15,008 | |
| pinned (addr in a `splits.txt` `.text` range) | 9,316 | already targeted |
| compiled-not-pinned | 157 | reveal candidates |
| **orphan (neither pinned nor compiled)** | **5,535** | see split below |
| ├─ XDK/middleware (addr ≥ `0x82800000`) | **5,108 (92%)** | **NO** — Microsoft static libs |
| └─ game/engine (addr < `0x82800000`) | **427 (8%)** | mostly residue, not TUs |

**Orphan definition:** a `target_symbol_map.json` entry whose address is in no
pinned `.text` range AND whose mangled name is a defined symbol in no compiled
obj under `build/45410914/src` (COFF section-number > 0 scan).

## Finding 1 — 92% of orphans are XDK/middleware with no oracle

Everything at addr ≥ `0x82800000` is statically-linked Microsoft/middleware
code, verified by class namespace: `XGRAPHICS`, `D3DXShader`, `D3DXTex`, `D3D`,
`XAUDIO2`, `xWMA`, `LEAPCORE`, `LEAPFX`, `OAPIPELINE`, `XAPO*`, `CXAPOBase`,
`CMemory*` pools, `CPartyLib`, `XG_D3DXTex`. Largest clusters:

| #fn | span | dominant |
|---|---|---|
| 645 | `0x828F2728` | XGRAPHICS shader compiler + ShaderPDBBuilder |
| 390 | `0x8287AA90` | D3DXShader::CShaderProgram / C30SWProgram |
| 324 | `0x829FB980` | XGRAPHICS::CFG / Assembler / VRegTable |
| 298 | `0x82A1BB20` | XGRAPHICS IR/Scheduler/Block |
| 265 | `0x829AE6E0` | D3DXShader::Compiler |
| 264 | `0x82B884C0` | XAUDIO2 CX2SourceVoiceXMA / CX2Engine |
| 149/134/92 | `0x82BBEA98`… | xWMA + LEAPCORE voice skins |

These have **no source oracle** — rb3-Wii is Wii (no D3D/XAudio), DC3 also
consumes them as static libs. They are **not decomp targets** (CLAUDE.md: engine
is pre-solved, effort goes to game). **Non-actionable.** Wiring them is
impossible without reconstructing Microsoft's proprietary shader compiler /
audio pipeline from scratch.

## Finding 2 — the 427 game/engine orphans are residue, not unwired TUs

Below `0x82800000`, orphans do **not** form contiguous unwired-TU clusters. They
are the scattered residue of **already-wired or already-pinned** units:

- EH `__unwind$NNNNN` funclets (compiler-generated, follow their owner fn),
- STL template instantiations (`?$_Rb_tree`, `?$vector`, `?$list`, reverse
  iterators — the round-3 "any-owner pin" lever, now drained),
- `??_E`/`??_G` vector/scalar deleting-destructor thunks,
- individual missing functions inside units whose `.cpp` we already compile.

Proof: `PlatformMgr`, `CharServoBone`, `SongSortMgr`, `ProfileMgr`,
`AccomplishmentManager` all **compile today** yet still have orphan entries.
`RndParticleSys`'s orphans (`FreeAllParticles`, `Burst::Emit`, …) attribute by
address to the **wired** `Part.cpp`/`Cam.cpp` units — they are *missing
functions within a wired unit* (body-port completion), not an unwired TU.

## Finding 3 — the pinned set is fully wired

Address-attributing every map entry to its owning `splits.txt` unit: **777 of
778 pinned units are already in `objects.json`.** Only `ctr.c` (a crypto lib)
has a stray pinned-uncompiled function. There is **no pinned-but-unwired TU
vein.** The 30 `splits.txt` unit-blocks not in `objects.json` are all
third-party C libs (Vorbis `psy.c`/`mdct.c`/…, zlib `inflate.c`/`deflate.c`,
json-c, `aes.c`, curl ssl) at addr ≥ `0x82B00000`.

## Finding 4 — genuine unwired-HAVE-SOURCE game TUs are nearly exhausted

127 `.cpp` files exist in `src/` but are absent from `objects.json`. Ranking by
uncompiled map-entry count, the apparent high-count candidates resolve to:
- **wired-but-incomplete stubs**: `NetworkSocket_Win.cpp` (70-line stub vs 13
  retail `WinSockSocket` fns), `ShaderMgr.cpp` (rnddx9) — body-port completion,
  not wiring;
- **8-line forward stubs**: `FxSendSynapse360.cpp`, `FxSendPitchShift360.cpp`
  (functions scattered across a 63 KB span, source won't emit them);
- **engine** (`rnddx9`, `rndobj`) — deprioritized per CLAUDE.md.

A scan for unwired-in-tree files with (a) source line-count matching DC3 and
(b) a contiguous (<32 KB) address span returned **exactly one** clean
candidate — `StreamReceiver360.cpp` — which this session wired (see below).

## Ranked TU-wiring priority (top actionable, game/engine < 0x82800000)

Unwired classes with uncompiled map entries and a *tight* contiguous span
(candidate real TUs; `unc` = uncompiled fns, span in KB):

| rank | class | unc | span KB | addr | source | class |
|---|---|---|---|---|---|---|
| 1 | **StreamReceiver360** | 7 | 3.5 | `0x82B3CDC0` | ours=DC3 (219L) | UNWIRED-HAVE-SOURCE ✅ WIRED +4 |
| 2 | WinSockSocket | 13 | 1.5 | `0x8251DF68` | ours(stub)/DC3 | WIRED-INCOMPLETE (body-port) |
| 3 | TexLoadPanel | 5 | 3.1 | `0x825D72F0` | wii + DC3 | UNWIRED-ORACLE (DC3=DanceCentral divergence) |
| 4 | DxMovie | 7 | 2.6 | `0x82719070` | DC3 rnddx9 | UNWIRED-ORACLE (engine, deprioritized) |
| 5 | RndDir | 6 | 11.2 | `0x823F0468` | DC3 | engine |
| 6 | RndShaderMgr | 5 | 1.6 | `0x82458948` | ours(incomplete) | WIRED-INCOMPLETE (engine) |
| 7 | DxTex | 5 | 3.9 | `0x8270F270` | DC3 rnddx9 | engine |
| 8 | UDebugGraph | 4 | 4.0 | `0x826E1EC0` | DC3 (anon-ns) | engine util |
| 9 | KickPlayerMsg | 4 | 0.5 | `0x826375B8` | wii/DC3 msg | game (msg macro) |
| 10 | CartRow | 3 | 3.2 | `0x822FB9E8` | game store | UNWIRED-ORACLE |

Remaining entries are ≤3 fns each and dominated by STL/`PA*` mangling noise.
BandCamShot::UTarget (8 fns) is **excluded** — wave-18 owns BandCamShot.
StoreOffer (`PAPAVStoreOffer`, 8) is **excluded** — mlstore lane risk.

## Strategic conclusion — redirect future effort

The **TU-wiring vein is drained** for game/engine, mirroring the round-3
pin-vein close-out. Concretely:
1. **Do NOT** chase the 5,108 XDK/middleware orphans — no oracle, not targets.
2. The 427 game/engine orphans are **body-port completion** work inside
   already-wired units (e.g. finish `Part.cpp`'s RndParticleSys, complete the
   `NetworkSocket_Win.cpp`/`ShaderMgr.cpp` stubs), **not** new-TU wiring — route
   them to the standard `bodyport-*` playbooks.
3. Only a handful of genuine small unwired game TUs remain (TexLoadPanel,
   CartRow, KickPlayerMsg-family messages), each 3-5 fns and requiring an
   oracle port with RB3-vs-DC3 divergence handling — low ROI per TU.

## Session wiring: StreamReceiver360.cpp (+4, zero loss)

- `objects.json`: added `system/synth_xbox/StreamReceiver360.cpp: NonMatching`.
- `splits.txt`: pinned `.text 0x82B3CDC0-0x82B3DA40` (span clean: foreign
  `HeadsetXferEffect` ctor precedes at `0x82B3CD28`, `SynthSample` follows at
  `0x82B3DA40`; the two in-span `for_each`/`DeleteAll` helpers are this TU's own
  `list<Voice*>` instantiations).
- Source already present and byte-identical to DC3 (219 lines) — pure wiring.
- Result: **15399 → 15403 matched (+4, 0 regressions)**, dual-metric A/B vs
  merge-base. Matched: `DeleteAll` STL helper + 3 EH funclets. The 6 named
  methods (`Poll`, `SetADSR`, `SetFXSend`, `PauseImpl`, `UpdateADSR`, `Tag`)
  sit at 99.9% = **ADDRESS_RELOCATION_NOISE** (different `.text` layout,
  at-limit per objdiff), not fixable by source.
