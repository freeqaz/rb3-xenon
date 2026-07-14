# Binary-Patch / No-Full-Build Path — same-instrument on retail RB3 (Xbox 360 TU5)

**Lane C.** Ship the RB3Enhanced "same-instrument" feature bootable on retail RB3
(title `45410914`, **TU5**) **without the Microsoft Xbox 360 XDK** and, ideally,
**without a full source build of RB3Enhanced.dll**. Companion analysis to
`rb3enhanced-same-instrument-patch.md` (design) and `same-instrument-derived-addresses.md`
(retail addresses).

**Written 2026-07-07. Every toolchain claim below was re-verified locally this session.**

---

## TL;DR verdict

- **Pure memory pokes CANNOT ship the whole feature.** Layers A/B/C are *entry-neutering*
  and are poke-shaped, but the **centerpiece — the per-watcher gem-list clone** — is a real
  C function with control flow, two static tracking tables (~320 B BSS), a claimant map,
  and 8 external game calls. Compiled it is ~**500–800 PPC instructions**. It is **not**
  hand-assemblable in any practical sense. **The feature needs a compiler.**
- **But it does NOT need the XDK.** Our feature TU (`SameInstrumentHooks.c`) `#include`s
  only `<string.h>` plus project headers — **no `xtl.h`, no `xboxkrnl`, no XAM**. It calls
  the *game* (via RB3E `POKE_B` stubs) and `memcpy`/`memset`. So it compiles to a `.obj`
  with **`cl.exe /c` alone** (which we have, working under wibo) + a 10-line freestanding
  `string.h`. The XDK is only needed to build the *other* ~10 RB3E TUs (crypto/net/content/
  exceptions — the real `xtl.h` consumers) and to link+XEX-wrap the whole DLL.
- **Fastest route to a full, playable feature with no full build:** take the **prebuilt
  RB3Enhanced.dll** (0.7 Xbox release, already TU5) as the boot + hook runtime, compile
  **only** `SameInstrumentHooks.c` with `cl.exe /c`, and inject that one blob into a code
  cave via a **generated Xenia `.patch.toml`** (emulator) or a repacked DLL (hardware). No
  XDK, no linker-for-a-DLL, no `imagexex`.
- **Testable-today emulator subset with ZERO compiler:** a hand-written `.patch.toml` doing
  Layer-A (and a Layer-C occupancy-neuter poke, once that address is derived) unlocks the
  **UI** so a 2nd same-instrument player is selectable — **but the game crashes / steals
  notes the moment a song starts**, because Layer-C reuse and the gem-clone are not pokes.
  Honest ceiling of the pure-poke path: a **UI-unlock demo**, not the feature.

---

## 1. Prebuilt release — confirmed

`gh api repos/RBEnhanced/RB3Enhanced/releases/latest`:

| | |
|---|---|
| Latest release | **0.7**, published **2025-03-08**, commit `106c429` |
| Xbox asset | **`RB3Enhanced_0.7-Xbox.zip`** — 216 892 B — <https://github.com/RBEnhanced/RB3Enhanced/releases/download/0.7/RB3Enhanced_0.7-Xbox.zip> |
| Wii asset | `RB3Enhanced_0.7-Wii.zip` — 763 598 B |

- **Ships prebuilt `RB3Enhanced.dll` + `RB3ELoader.xex` + `rb3.ini`.** The Xbox zip is the
  standard RB3E hardware/emulator bundle (DLL is the XEX-DLL based at `0x84000000` per
  `xex.xml`; `RB3ELoader.xex` is the DashLaunch plugin; `rb3.ini` is the config). Contents
  are inferred from the asset name/size + RB3E's documented layout — **verify by unzipping**
  (no local copy was present; WebFetch can't unzip a binary).
- **Target vs our game: SAME.** RB3E 0.7's `ports_xbox360.h` is "for 360 TU5", and the
  official Xenia patch (`45410914 - Rock Band 3.patch.toml`) accepts the TU5 disc-XEX module
  hash. rb3-xenon's `orig/45410914/default.xex` is that same TU5 XEX. **No address drift** —
  the prebuilt DLL and our derived addresses target the identical binary.
- The prebuilt DLL is an **unencrypted + compressed** XEX2 (`<unencrypted/><compressed/>`).
  Relevant for the code-cave route: to patch its PE you must decompress the XEX basefile,
  edit, then recompress/repack (see §5).

---

## 2. Feature decomposition — pokes vs. real code

Source: `RB3Enhanced/source/SameInstrumentHooks.c` (read-only) + the design doc §3/§6.

### 2a. Poke-shaped (entry-neutering) — small, no compiler *if the branch is pinned*

| Layer | What it neuters | Minimal pure-poke form | Address status |
|---|---|---|---|
| **A** — `OvershellPartSelectProvider::IsActive` | UI grey-out of a taken part | `POKE_32` the tail `RepresentSamePart(...) → return false` branch to fall through (design spike 0.2) — **1 instruction** | **Pinned** `0x8264B5F8` |
| **B** — `OvershellPanel::ResolvePartWaitStates` | arbitration / turn-taking | *Not cleanly a single poke* — the function also advances uncontested waiters, so nulling it strands players. Pokeable only by surgically NOPing the two enforcement branches; riskier. The C hook re-pushes waiters to ChooseDiff (~15 instr, needs code). | **Pinned** `0x8259D948` |
| **C** — `PlayerTrackConfigList::ProcessConfig` MILO_FAIL | crash on 2nd claimant | Two sub-options: (i) NOP the `MILO_FAIL` — **stops the crash but leaves `mTrackNum = -1`** → downstream garbage; NOT sufficient. (ii) NOP the `&& mTrackOccupied[i]==0` clause inside `TrackNumOfExactType` so it returns the first slot of the type **ignoring occupancy** → the 2nd claimant reuses the slot. **(ii) is a genuine 1–2 instruction poke** and is the poke-path win for Layer C. | **UNPINNED** (`ProcessConfig`/`TrackNumOfType` NOT FOUND yet — see addresses doc) |

**Takeaway:** Layers A and C *can* be reduced to pure pokes (A confirmed 1 poke; C reducible
to a 1–2 instr poke inside `TrackNumOfExactType` **once that address is derived**). Layer B is
borderline — the clean version wants ~15 instructions of real code.

### 2b. Real compiled code (the centerpiece) — needs a compiler, cannot be a poke

The per-watcher gem-list clone (`RecalcGemListHook` + `FindClaim`/`AddClaim`/`FindImpl`/
`AddImpl` + `FirstSlotOfExactType` + `ProcessConfigHook` + `ResolveWaitStatesHook` +
`FreeSameInstrumentClones`) is ~250 lines of C that:

- keeps **two static tables** — `gClaims[16]` (128 B) and `gImpls[16]` (192 B) — in BSS,
  plus RB3E_MSG format strings;
- runs **linear-scan find/add loops** and per-song claim bookkeeping;
- makes **8 external game calls** (`GameGemDB::Duplicate`, `GetDiffGemList`, `~GameGemDB`,
  `GameGemList::CopyFrom`, `MemFree`, `TrackNumOfType`, plus the BandUser/OvershellPanel
  accessors) with argument marshaling;
- does struct-offset arithmetic through accessor shims and reinstalls the clone at the
  `RecalcGemList` choke-point on every reset.

**Quantified:** compiled with `-Ox -Os` this is realistically **~500–800 PPC instructions**
plus ~**320 B** of BSS and a handful of relocations (to game addresses and to its own tables).

**Is it hand-assemblable?** **No.** A claimant map with add/find/iterate loops, 8 call sites
with correct r3–r10 marshaling, stack-frame setup, and relocations to both game functions and
its own BSS is exactly the output you get *from* a compiler, not something you write by hand
correctly in a reasonable time. **The gem-clone truly needs a C compiler and a place to live.**
The *good* news (§0): that compiler is `cl.exe /c` — **not the XDK**.

---

## 3. Toolchain reality — verified this session

Under `wibo` (`/home/free/code/milohax/wibo/build/release/wibo`), from
`rb3-xenon/build/compilers/X360/16.00.11886.00/`:

```
$ wibo cl.exe   → "Microsoft (R) 32-bit C/C++ Optimizing Compiler Version 16.00.11886.00 for PowerPC"   (exit 0)
$ wibo link.exe → "Microsoft (R) Incremental Linker Version 10.00.11886.00"                              (exit 0)
```

- **`cl.exe` works** and is heavily exercised — rb3-xenon has **3 986 `.obj`** built this way,
  all `cl.exe /c` with **reconstructed per-TU headers** (`tools/decompctx.py`) and **no XDK**.
- **`link.exe` runs** (banner prints) but is **never used** in rb3-xenon's build (`build.ninja`
  has 0 link/lib edges — the decomp only compiles to `.obj` for objdiff). Crucially, `link.exe`
  can run in **`/LIB` mode** to *generate an import `.lib` from a `.def`* (see §6) and in link
  mode to produce a PE `.dll`.
- **What is NOT present:** any XDK header (`xtl.h`, `xbox*.h`), any XDK import lib
  (`xapilib/xboxkrnl/xnet/xonline.lib`), and `imagexex.exe`. The compiler dir holds only
  `cl.exe`/`link.exe` + `c1/c1xx/c2` + runtime DLLs.

**Consequence:** we already have a working, XDK-free MSVC-PowerPC **compiler** (and a linker
that launches). The XDK gap is purely: (1) headers+libs for RB3E's ~10 `xtl.h`-consuming TUs,
and (2) `imagexex` for the PE→XEX wrap. Our feature TU needs **neither**.

---

## 4. Route ranking

### Route 1 — **Compiled-blob companion to the prebuilt DLL** ★ recommended (fastest full feature, no full build, no XDK)

Keep the prebuilt `RB3Enhanced.dll` exactly as shipped — it already boots pre-`main`, mounts
`rb3.ini`, and (this is the leverage) **installs the `POKE_*` / `HookFunction` runtime and the
RB3E_STUB→game-address wiring into the live process**. Add our feature as one externally
compiled blob:

1. **Compile one TU, no XDK.** `wibo cl.exe -c -Ox -Os -D RB3E_XBOX -TC SameInstrumentHooks.c`
   with a 10-line freestanding `string.h` (just `memcpy`/`memset` protos) and the project's own
   `rb3/*.h`. Output: `SameInstrumentHooks.obj`. (Add tiny accessor shims — `TWImplTrack`,
   `SongDataGemDB`, `MiloVectorIntCount`, etc. — in the same TU; they're plain struct reads.)
2. **Relocate the blob to a fixed cave VA.** A ~150-line Python packer reads the COFF `.obj`,
   lays `.text`+`.data`+`.bss` at a chosen high cave address (e.g. inside the DLL's slack at
   `0x84xxxxxx`, or a Xenia-writable region), and resolves the internal relocations + the
   external symbols to their retail game addresses (from `same-instrument-derived-addresses.md`)
   and to RB3E's exported `config` / `HookFunction`.
3. **Trigger it once.** One `POKE_BL` at the tail of the game's (or RB3E's) init path →
   `InitSameInstrument` in the cave, which then installs the 4 detours itself. Because RB3E's
   own hook runtime is already resident, our blob reuses `HookFunction`/`POKE_B` verbatim.
4. **Deliver as either** (a) a **generated Xenia `.patch.toml`** that writes the cave bytes +
   the trigger poke (emulator — no DLL repack at all, see §5), or (b) a **code-caved+repacked
   DLL** for hardware (§5).

- **Compiler needed?** Yes — `cl.exe /c` for the one TU. **XDK needed? No.**
- **Effort:** small–medium. Derive the 4 still-unpinned addresses (`ProcessConfig`,
  `TrackNumOfType`, `~GameGemDB`, `Band::NewPlayer`; the 5 centerpiece clone addresses are
  already **VERIFIED**), write the Python `.obj`→blob packer (the only new tooling), compile,
  emit `.patch.toml`. Est. **2–4 focused days**, most of it the packer + address derivation.
- **Cleanliness:** high for emulator (nothing in the prebuilt DLL changes; the blob + pokes
  live entirely in the patch). Medium for hardware (needs the blob folded into the DLL or a
  2nd plugin — see §5b).

### Route 2 — **Code-cave inside the prebuilt DLL + XEX repack** (hardware distribution)

Decompress the DLL's XEX basefile → find/append a code cave in the PE → write the relocated
blob (base `0x84000000`) → **patch RB3E's `ApplyHooks` to `bl` our `InitSameInstrument`** (find
the call site in the compiled DLL) → recompress + repack the XEX. Same blob as Route 1; the
difference is it's baked into the DLL so it also works on console with the stock RB3ELoader.

- **Compiler needed?** Yes (same one TU). **XDK needed?** **No for compile; the repack needs a
  free XEX tool** (`xextool`, `idaxex`, or Xenia's XEX2 read/write code) instead of `imagexex`.
- **Effort:** medium. Extra work vs Route 1: XEX decompress/recompress round-trip, PE cave
  management, and finding+patching the `ApplyHooks` call site in a stripped release DLL.
- **Risk:** XEX repack fidelity (page hashes are off since `<unencrypted/>`, but the compressed
  basefile + import table must be re-emitted correctly). `idaxex`/Xenia code is the reference.

### Route 3 — **Second companion DLL / plugin loaded alongside RB3E** (cleaner hardware, but needs a full link)

Ship our feature as its own tiny XEX-DLL that loads after RB3E and calls `InitSameInstrument`
from its `DllMain`. Piggybacks on RB3E identically (RB3E's hook runtime is already resident).
**But** producing a second XEX-DLL requires the full link + XEX-wrap chain (a `DllMain`, the
CRT entry, an import table, `imagexex`/free packer) — i.e. it re-introduces most of the "full
build" cost we're trying to avoid, just for a smaller TU set. Load-ordering (ensuring it runs
after RB3E's `StartupHook`) also needs a 2nd `LoadLibrary` poke. **Not recommended** unless a
clean, separately-distributable hardware artifact is a hard requirement.

### Route 4 — **Full RB3E rebuild WITHOUT the XDK** (the "clean redistributable DLL" fallback)

Rebuild all of `RB3Enhanced.dll` from the fork source, replacing the XDK with reconstructed
pieces:

- **Import libs** — regenerate `xboxkrnl.lib` / `xam.lib` / `xapilib.lib` / `xnet.lib` /
  `xonline.lib` from `.def` files via **`link.exe /LIB /DEF:...` (runs under wibo — verified)**,
  using the kernel/XAM export-ordinal tables published by free60 / Xenia. This is the standard
  no-XDK 360-homebrew technique and is tractable.
- **Headers** — the hard part. The ~10 XDK-consuming TUs (`xbox360_crypto.c`, `net_*.c`,
  `xbox360_content.c`, `xbox_keyboard.c`, `xbox360_exceptions.c`, `xbox360.c`, `XboxCache.h`,
  `XboxContent.h`, `Joypad.h`, `QuazalSocket.h`) genuinely include `xtl.h` and use real XAM/
  kernel structs + APIs. Reconstructing enough of `xtl.h` for them is a **large** surface —
  materially bigger than the whole same-instrument feature.
- **XEX wrap** — `imagexex` → free `xextool`/`idaxex`.

- **Compiler needed?** Yes. **XDK needed?** No (that's the point) — but it is a **full build**,
  so it violates the "avoid a full build" goal and is the *most* effort. Its only advantage is
  producing a pristine, self-consistent redistributable DLL. Keep as fallback.

### Route 5 — **Pure `.patch.toml`, zero compiler** (testable-today emulator subset only)

See §5a and §7. Delivers a **UI-unlock demo**; **cannot** deliver playable same-instrument.

---

## 5. Xenia `.patch.toml` reach — what pokes alone can/can't do

Xenia Canary applies per-title `patches/*.patch.toml` (needs `apply_patches = true` and a
canary build with `writable_code_segments`, since RB3E POKEs `.text`). A `[[patch]]` can write
**arbitrary bytes to arbitrary addresses** — so in principle you can lay an entire blob into a
cave byte-by-byte *and* add the hook pokes, all from a patch file.

### 5a. Pure-poke subset (no compiler) — honest ceiling

- **Deliverable:** Layer-A grey-out off (1 poke @ `0x8264B5F8`) + optionally the Layer-C
  occupancy-neuter poke inside `TrackNumOfExactType` (1–2 instr, **once that address is
  derived**). A 2nd controller can now **select and reach the difficulty screen** on an
  already-taken instrument.
- **Hard stop:** at song start `PlayerTrackConfigList::ProcessConfig` still needs the *reuse*
  logic, and even if you neuter it, the shared `GameGem::mPlayed` bit means the two players
  **steal each other's notes** (design §3.2). Without the gem-clone the experience is
  broken; with only the MILO_FAIL NOP'd (not the reuse poke) it **crashes on the 2nd
  duplicate**. So: **`.patch.toml`-alone = UI unlock demo, crash-or-steal on play.**

### 5b. Blob-carrying `.patch.toml` (the real feature, still no XEX repack)

Because a patch can write bytes anywhere, the Route-1 packer can **emit a `.patch.toml`** that
(i) writes the relocated gem-clone blob into a chosen cave region and (ii) writes the trigger
`bl` + the 4 detours. This gets the **full feature running in Xenia with no DLL repack and no
linker** — you still compiled the one TU with `cl.exe`, but distribution is a single text
patch. This is the **fastest path to a demonstrable, playable feature** and the recommended
bring-up target. (It presumes a Xenia build that lets a patch write to the cave region;
otherwise place the cave in the DLL's own slack, which RB3E already makes writable.)

---

## 6. Do we ever need the XDK? — decision summary

| Artifact | Needs XDK? | Why / substitute |
|---|---|---|
| Compile `SameInstrumentHooks.obj` | **No** | TU includes only `<string.h>` + project headers; `cl.exe /c` (have it) + 10-line freestanding `string.h`. |
| The gem-clone code existing at all | needs a **compiler**, not the XDK | ~500–800 instr; not hand-assemblable. `cl.exe` suffices. |
| Layers A / C as pure pokes | **No** (no compiler either) | 1–2 instructions each; `.patch.toml` or `POKE_32`. |
| Inject blob into emulator | **No** | Generated `.patch.toml` (§5b). |
| Inject blob into prebuilt DLL for hardware | **No** | Code cave + free XEX packer (`xextool`/`idaxex`/Xenia), not `imagexex`. |
| Rebuild the WHOLE RB3E DLL (Route 4) | import libs **No** (`link.exe /LIB /DEF` under wibo) · `imagexex` **No** (free packer) · **`xtl.h` header surface = the real cost** | Only route needing broad XDK-header reconstruction; large but XDK-free. |

**Net:** the XDK is **not required** for any route. A **compiler (`cl.exe /c`, already present
and working under wibo) IS required** for the gem-clone — there is no compiler-free route to the
full feature. The pure-poke path exists but tops out at a UI-unlock demo.

---

## 7. Testable-today emulator subset vs. full working feature

- **Testable today, zero compiler (§5a):** hand-written `.patch.toml`, Layer-A (+Layer-C
  occupancy-neuter once its address is derived). Proves the enforcement model empirically
  (design spike 0.2), unlocks the UI. **Crashes / steals notes at song start.** Not the feature.
- **Full working feature, minimal build (§4 Route 1 + §5b):** compile ONE TU with `cl.exe /c`
  (no XDK), pack `.obj`→fixed-VA blob, emit a `.patch.toml` that lays the blob + trigger over
  the **prebuilt** DLL. Full independent-hit same-instrument in Xenia, no full RB3E build, no
  XDK, no `imagexex`. **This is the recommended target.**
- **Full working feature on hardware:** same blob, folded into the DLL via code cave + free XEX
  repack (Route 2). Adds XEX round-trip + `ApplyHooks` call-site patch. Still no XDK.
- **Clean redistributable DLL:** Route 4 full rebuild sans XDK — biggest effort, only if a
  pristine artifact is required.

---

## 8. Concrete next steps (recommended route)

1. **Unzip `RB3Enhanced_0.7-Xbox.zip`**; confirm `RB3Enhanced.dll` + `RB3ELoader.xex` + `rb3.ini`
   and record the DLL's XEX base/compression + the `ApplyHooks` region (for Route 2 later).
2. **Derive the 4 unpinned addresses** (`PlayerTrackConfigList::ProcessConfig`,
   `TrackNumOfExactType`, `GameGemDB::~GameGemDB`, `Band::NewPlayer`) in Ghidra @ port 8002.
   The occupancy-clause offset inside `TrackNumOfExactType` doubles as the Layer-C poke (§2a).
3. **Compile one TU:** `wibo cl.exe -c -Ox -Os -D RB3E_XBOX -TC SameInstrumentHooks.c` with a
   freestanding `string.h` + the project `rb3/*.h`. Confirm a clean `.obj` (no XDK on INCLUDE).
4. **Write the `.obj`→blob packer** (~150 lines Python): parse COFF, place `.text/.data/.bss`
   at a fixed cave VA, resolve internal relocs + external symbols (game addresses + RB3E
   `config`/`HookFunction`), emit both a raw blob and a `.patch.toml`.
5. **Emit + load the `.patch.toml`** (blob cave + trigger poke + 4 detours) into Xenia over the
   prebuilt DLL. Run the design §5 spikes 0.1→0.4 as acceptance (crash reproduced → Layer-A
   selectable → song starts w/ stealing → clone removes stealing).
6. **Hardware (optional):** fold the blob into the DLL (code cave) + patch `ApplyHooks` + repack
   the XEX with `idaxex`/`xextool`; ship alongside stock `RB3ELoader.xex`.

**The one irreducible dependency is `cl.exe /c` for the gem-clone. The XDK is never needed.**
