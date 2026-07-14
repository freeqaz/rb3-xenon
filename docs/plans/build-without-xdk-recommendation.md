# Building the RB3Enhanced Same-Instrument Patch Without the Xbox 360 XDK

**Synthesis of three investigation lanes (A: XDK dependency audit, B: OSS build path, C: binary-patch / no-build).**
Date: 2026-07-07. Author: Opus synthesizer.

---

## TL;DR

- **The XDK is NOT required for our feature.** Our one translation unit (`SameInstrumentHooks.c`) touches zero Xbox kernel/XAM/XNet/XOnline API. It needs `cl.exe /c` and ~10 lines of freestanding CRT headers — nothing proprietary.
- **The one irreducible dependency is `cl.exe /c`** (MSVC X360 PowerPC compiler, 16.00.11886.00), which **already runs under `wibo`** on this machine and has built ~3,986 `.obj` files. No XDK bits are reachable from our TU.
- **Recommended first path:** compile the single TU with `cl.exe /c`, relocate the `.obj` into a fixed-VA code cave with a small Python packer, and deliver via a generated Xenia `.patch.toml` over the **prebuilt** RB3E 0.7 DLL. Full playable same-instrument, testable today in Xenia, no link, no `imagexex`, no XDK.

---

## 1. Why the XDK? — the three pieces, and who actually needs them

The Xbox 360 XDK contributes exactly three things to an RB3E build. Mapping each to **our feature** (the single same-instrument TU) vs **RB3E-as-a-whole** (all ~19 TUs + final image):

| XDK piece | What it is | RB3E-whole | **Our TU** |
|---|---|---|---|
| **Headers** | `xtl.h` + `$(XEDK)/include/xbox` CRT, force-included `xbox_intellisense_platform.h` | **YES** — 19 files `#include <xtl.h>` | **NO** — only `<string.h>`/`<stdint.h>`/`<stddef.h>` + local RB3E/game headers. Grep of the *entire transitive header set* for `xtl\|xam\|HANDLE\|DWORD\|__declspec\|WINAPI` = **zero hits** |
| **Import libs** | `xapilib/xboxkrnl/xnet/xonline.lib` (+`xbdm` emu) | **YES** — full DLL link resolves kernel/XAM/net symbols | **NO** — our TU calls the *game* via RB3E's `POKE_B` fixed-address stubs, not the kernel. It emits a plain `.obj` that **adds zero new XEX imports**. CRT need = `memset`/`memcpy` only |
| **`imagexex.exe`** | PE→XEX2 packager (`xex.xml`) | **YES** — produces the final bootable image | **N/A** — whole-image packaging. Replaceable by idaxex/xextool/Xenia; **entirely sidestepped** if we patch a prebuilt image instead of linking a new one |

**Reconciling the lanes:** Lane B was cautious and *proved the import-lib generation mechanism* (`link.exe /LIB /DEF` from free60/Xenia ordinal `.def` files → a real 1980-byte lib) as a fallback. Lanes A and C independently verified our TU needs **no import libs at all**. **There is no contradiction:** import-lib reconstruction matters only for the *full-link* OSS path (Path B below), and even there our TU contributes no imports. For the recommended path it is moot.

**Bottom line on "why the XDK":** RB3E-as-a-whole needs all three pieces. **Our feature needs none of them** — it needs a compiler, and the compiler is already working here without the XDK.

---

## 2. Ranked viable paths (fastest / lowest-risk first)

All tools listed are free/legal and present (or reconstructable) on this machine. **Hard boundary shared by every path:** no free tool yields a *retail-signed* XEX — output boots on RGH/JTAG/devkit/Xenia only. This is identical to RB3E's existing "modded console required" constraint, so it adds no new limitation.

### Path 1 — ★ RECOMMENDED — Compiled blob + Xenia `.patch.toml` over the prebuilt DLL
*(Lane C's recommended route, endorsed by Lanes A & B's compile/link proofs.)*

- **Delivers:** the FULL feature — Layer A (entry-neuter poke), Layer B, Layer C (occupancy-neuter), **and the centerpiece per-watcher gem-list clone** (~500–800 PPC instr, not hand-assemblable). Testable **today** in Xenia; same blob code-caves into the DLL for RGH/JTAG hardware via a free XEX repack.
- **Tools (all free, all present):** `cl.exe /c` under `wibo` (already working) + freestanding `string.h`/`stdint.h`/`stddef.h` from `rb3-xenon/src/xdk/LIBCMT/` + a ~150-line Python `.obj`→code-cave packer + prebuilt `RB3Enhanced.dll` from RB3E 0.7 release + Xenia. Hardware step adds `idaxex`/`xextool` (free) for XEX repack.
- **Steps:**
  1. Unzip `RB3Enhanced_0.7-Xbox.zip` (216 KB, TU5 title 45410914 — **same binary rb3-xenon decomps, no address drift**); keep its `RB3Enhanced.dll` as the boot + hook runtime.
  2. Add a 6-line freestanding `stdint.h` (rb3-xenon's is a 0-byte stub) and pass `-D _XBOX -D RB3E_XBOX`.
  3. `cl.exe /c SameInstrumentHooks.c` → `.obj` (machine 0x01f2 POWERPCFP, verified by Lane B).
  4. Python packer: relocate `.text`/BSS into a fixed-VA code cave; emit a generated `.patch.toml` laying the blob + Layer-A trigger poke + the 4 detours (`*Orig` trampolines) over the prebuilt DLL.
  5. Boot in Xenia → validate same-instrument through song start.
  6. Hardware: code-cave the same blob into the DLL, XEX-repack with `idaxex`/`xextool`.
- **Effort:** ~2–4 days (mostly the packer + deriving ~4 still-unpinned addresses; the 5 gem-clone centerpiece addresses are already VERIFIED).
- **Biggest risk:** packer correctness + deriving the remaining unpinned addresses (Layer C's `TrackNumOfExactType` occupancy clause is not yet pinned). Both are bounded engineering, not blockers.

### Path 2 — OSS full-link path: mint a complete unsigned XEX-DLL with free tools
*(Lane B, proven compile+link, plausible packing.)*

- **Delivers:** a full RB3E DLL rebuilt entirely from source with zero XDK bits — the general solution (also rebuilds RB3E's other TUs, useful beyond this one feature). Boots on RGH/JTAG/devkit/Xenia.
- **Tools:** `cl.exe` + `link.exe` under `wibo` (both proven working) + reconstructed freestanding headers + `link.exe /LIB /DEF` import libs from free60/Xenia ordinal tables + a PE→XEX2 pack step.
- **Steps:** compile all TUs XDK-free → generate needed import libs from public ordinal `.def`s → `link.exe -dll -MACHINE:PPCBE` (proven: valid PPC PE, `li r3,1; blr` DllMain) → **pack PE→XEX2**. Packing has three free closes: **(4a, recommended)** extract released RB3E XEX with local `idaxex xex1tool`, splice `.text`, repack (reuses an already-booting container); (4b) ~300–500-line packer from Xenia's `xex2_info.h` (unsigned RGH/JTAG → no MS key, no AES/LZX when uncompressed); (4c) xorloser XexTool under `wine` (freeware).
- **Effort:** compile+link wiring <0.5 day; Xenia-bootable artifact via 4a ~2–3 days.
- **Biggest risk:** the PE→XEX2 pack (step 4) — no OSS *forward* packer exists (all tools go XEX→PE). It's an effort/validation risk, not a blocker: format is documented, unsigned+uncompressed removes crypto+signing, idaxex is local for the inverse reference. **De-risk by proving 4a extract→identity-repack→boot on the stock DLL before injecting hooks.**

### Path 3 — Baseline: RB3E GitHub Actions CI (has the SDK secret)
- **Delivers:** an official full RB3E DLL built by upstream CI. Legit, zero XDK on our machine.
- **Reality check (why Path 1 beats it):** the SDK secret lives on the **official** repo — a fork won't have it, so this requires either upstream cooperation (PR our TU) or your own secret. Slower loop (push→CI→download), no local iteration, and it builds the whole image rather than a targeted artifact. Good as a *validation cross-check* of Path 1/2 output, not as the primary dev loop.
- **Effort:** low if upstream accepts a PR; otherwise blocked on the secret.

### Path 4 — Baseline: user's own `$XEDK`
- **Delivers:** trivial full build if the user already has a licensed XDK installed. This is precisely the dependency we're trying to avoid, listed only as the reference baseline. Use only if the user *wants* to and already has it.

### Path 0 — Quick spike (NOT a delivery): pure-poke `.patch.toml`, no compile
- **Delivers:** UI-unlock demo ONLY — Layer A (and Layer C once its address is derived) lets a 2nd player select a taken instrument and reach the difficulty screen. At song start it **crashes (MILO_FAIL) or steals notes** because Layer-C reuse and the gem-clone are real code, not pokes.
- **Value:** minutes of effort; useful to prove the TU5 address map + `.patch.toml` plumbing before investing in the packer. Treat as a de-risking step for Path 1, not a feature deliverable.

---

## 3. Recommendation

**Pursue Path 1 first.** It is the fastest route to the *complete* feature (including the gem-clone that pure pokes can't express), it is testable **today** in Xenia with no hardware, and it carries the same blob straight to RGH/JTAG hardware via a free XEX repack. It requires **no linker-for-a-DLL, no `imagexex`, and no XDK** — only `cl.exe /c`, which is already proven working here.

**Sequencing within Path 1:**
1. **Spike (Path 0) first** — hand-write a pure-poke `.patch.toml` (Layer A) over the prebuilt 0.7 DLL and confirm the 2nd player reaches the difficulty screen in Xenia. Validates the TU5 address map + patch plumbing in ~an hour.
2. Then build the packer and compile the TU (the real work).
3. Keep **Path 2** as the general fallback if a fully-rebuilt DLL is ever needed (other TUs, upstreaming), and use **Path 3 (CI)** purely to cross-validate the artifact.

**Why not Path 2 first:** its only unproven step (PE→XEX2 packing) is avoided entirely by Path 1, which patches an already-booting container instead of minting a new image. Path 2 is strictly more work for our single-feature goal.

---

## 4. Open questions / spikes needing a decision

1. **Unpinned addresses.** Layer C's occupancy clause inside `TrackNumOfExactType` (and Layer B's ~15-instr body) are not yet pinned to TU5 VAs. Spike: derive them from rb3-xenon's decomp + the TU5 map. *(Blocks the full feature; the 5 gem-clone centerpiece addresses are already verified.)*
2. **Zip contents assumption.** Lane C inferred `RB3Enhanced_0.7-Xbox.zip` layout from name/size + RB3E convention (no local copy; WebFetch can't unzip). Spike: actually `unzip` it and confirm `RB3Enhanced.dll` + `RB3ELoader.xex` + `rb3.ini`.
3. **Code-cave location.** Where in the prebuilt DLL's address space does the ~800-instr blob + ~320 B BSS live without colliding? Decide: extend a section vs. reuse padding vs. append. Affects both the Xenia `.patch.toml` and the hardware XEX repack.
4. **Hardware XEX repack validation.** Prove `idaxex`/`xextool` round-trips the modified DLL into a JTAG/RGH-bootable XEX (Path 2 step 4a de-risks this generally). Decide after Xenia validation succeeds.
5. **Upstreaming vs. private patch.** Do we PR the TU into RB3E (unlocks Path 3 CI as the official build) or ship a standalone patch? Product/licensing decision, not technical.
