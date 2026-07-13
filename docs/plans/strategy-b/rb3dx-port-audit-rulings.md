# RB3DX port-audit rulings (plan Phase 1, P1.2)

**Date:** 2026-07-13. **Tool:** `rb3-xenon/tools/oss-xbox-build/rb3dx_port_audit.py`
(exit 0 = all overlaps reviewed). **Output:** `rb3dx_port_audit.json` (same dir).

## Inputs (re-derived this run, not trusted from prior docs)

| Input | Path | sha1 |
|---|---|---|
| RB3DX xex (canonical) | `rock-band-3-deluxe/platform/xbox/default.xex` | `c5a17091` |
| RB3DX PE (dtk `xex extract`) | (temp) | `2fbdbc6b` — byte-identical to the divergence-study PE |
| clean TU5 PE | `rb3-xenon/_tu5probe/clean/band_clean_tu5.exe` | `5f3f667a` |
| ports | `RB3Enhanced/include/ports_xbox360.h` | 197 `PORT_` |

**Section tables byte-identical** (shared VA↔offset). Re-derived byte delta:
**170 bytes in 38 strict-contiguous spans** (the 2026-07-07 study's "~20 spans"
was those 38 coalesced across small gaps; the 170-byte total reproduces exactly).

## Same-instrument feature surface: COLLISION-FREE

All 4 SI detour VAs + 5 SI support/helper ports + `PORT_THEBANDUSERMGR` are
outside every RB3DX diff span, and every SI *code* port's first 64 bytes are
byte-identical clean↔RB3DX (`first64_differs=False`). `PORT_THEBANDUSERMGR` is a
`.data` zero-init global (not file-backed) — same VA in both, uncovered.
→ **The TU5 same-instrument DLL IS the RB3DX same-instrument DLL for c5a17091.**

## General RB3E ↔ Deluxe write-site collisions: 8 VAs, all ruled

The write-site scan is platform-guard-aware (models the `RB3E_XBOX` DLL build;
Wii/PS3 `#ifdef` blocks excluded — e.g. the Wii `POKE_B(PORT_MULTIPLAYER_CRASH)`
at `rb3enhanced.c:251` is correctly dropped, only the xbox `POKE_BL` at `:253`
counts). 115 game-image write-sites resolved; 8 intersect a RB3DX diff span.

Byte-level: for each, the value RB3E writes at `[VA,VA+4)` was compared against
what RB3DX statically holds. **RB3E writes at StartupHook/DllMain, after xex
load → RB3E-wins ordering at every collided site.**

| VA | RB3E write | clean → RB3DX | RB3E == RB3DX? | verdict |
|---|---|---|---|---|
| `0x82516320` PORT_SETDISKERROR | `BLR` 4e800020 | 7d8802a6 → 4e800020 | entry word **identical** | BENIGN_IDEMPOTENT |
| `0x82270f40` PORT_FASTSTART_CHECK | `NOP` 60000000 | 41820010 → 60000000 | **identical** | BENIGN_IDEMPOTENT |
| `0x82579098` PORT_SONGBLACKLIST | `LI(3,0)` 38600000 | 4bffc521 → 38600000 | **identical** | BENIGN_IDEMPOTENT |
| `0x82575f9c` PORT_SONGMGR_ISDEMO_CHECK | `NOP` 60000000 | 4082000c → 60000000 | **identical** | BENIGN_IDEMPOTENT |
| `0x82ae6880` PORT_MULTIPLAYER_CRASH | `BL`→FIX 4bd449b9 | 480d62d1 → 4bd449b9 | **identical** | BENIGN_IDEMPOTENT |
| `0x82272e90` PORT_APP_RUN | `BL`→RunNoDbg 4bffd1f1 | 4bffd541 → 4280d1f1 | **divergent** | RB3E_WINS_BY_DESIGN |
| `0x82c4c47c` PORT_XEKEYSSETKEY_STUB | `B`→DLL hook 48…… | 01010242 → 01010159 | **divergent** | RB3E_WINS_MOOTED |
| `0x82c4c48c` PORT_XEKEYSAESCBC_STUB | `B`→DLL hook 48…… | 0101024b → 0101015b | **divergent** | RB3E_WINS_MOOTED |

### Interpretation

- **5 BENIGN_IDEMPOTENT.** RB3E writes the *exact bytes RB3DX already holds*. This
  proves RB3 Deluxe statically bundled these five RB3Enhanced patches (disk-error
  bypass, fast-start, song-blacklist stub, IsDemo bypass, online-MP crash fix).
  RB3E re-applying them is a no-op. (ISDEMO is additionally `RB3E_IsEmulator()`-gated,
  so on RGH hardware RB3DX's static nop stands untouched.)
- **`0x82272e90` RB3E_WINS_BY_DESIGN.** RB3E's DllMain redirects the `main()` call
  site App::Run→RunWithoutDebugging (0x4bffd1f1); RB3DX has its own redirect
  (0x4280d1f1) there. DllMain runs at PROCESS_ATTACH, *before* App::Run executes, so
  RB3E's value is final. RB3DX is designed to run *with* the RB3E DLL loaded, so this
  supersession is expected. Not same-instrument-related. **Action:** confirm boot at
  Phase-5 (negative-control and SI-on runs both exercise App::Run).
- **`0x82c4c47c`/`0x82c4c48c` RB3E_WINS_MOOTED.** RB3E's `POKE_B` overwrites the whole
  XeKeys import thunk with a branch to its XeCrypt redirect; RB3DX edited only the
  thunk's low-16 (a different import RVA). Because RB3E fully replaces the XeKeys code
  path, the thunk's underlying import target is mooted. Crypto import stubs only; not
  gameplay, not same-instrument.

### Ruling

No collision touches the same-instrument feature. The three divergent collisions are
non-SI, RB3E-owned, and resolved by RB3E's load-after-xex ordering (which Deluxe's
design assumes). No RB3E patch needs to be defaulted-off on RB3DX. The two Phase-5
boot runs already exercise App::Run and the crypto path, giving behavioral coverage
of the `RB3E_WINS_*` sites at no extra cost.

## Regression gate

`rb3dx_port_audit.py` re-extracts the RB3DX PE from the xex via dtk and re-derives
everything, so pointing `--rb3dx-xex <new>` at a future Deluxe xex drop re-proves
compatibility in seconds. If the section tables ever differ (true relocation) it
exits 3 and the plan's P1.3 signature-scan fallback activates. Rulings live in the
`RULINGS` dict in the script (keyed by VA); an unreviewed overlap exits 2.
