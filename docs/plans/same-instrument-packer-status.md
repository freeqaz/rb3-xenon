# Same-Instrument packer — status (Stage 4/5 PACK+VERIFY)

_Last updated 2026-07-07. Branch `feature/same-instrument` @ `1686219` (+ uncommitted
enable/pack work). Nothing committed._

## Xenia verification (2026-07-07, `~/code/milohax/xenia` headless build)

Ran the artifact against a real Xenia (`build/bin/Linux/Checked/xenia-headless`) + the
retail XEX (`rb3-xenon/orig/45410914/default.xex`). Findings — static validation is
STRONG; live gameplay is BLOCKED on this host (no 360 game content):

- **XEX identity CONFIRMED** — Xenia loaded it as Title `45410914`, image base `0x82000000`,
  PE name `band.exe`, Media ID `4FC9256F`, entry `0x82816080`. Matches the artifact's target
  exactly (the packer relocated against `band.exe`, the decrypted PE of this XEX).
- **All 4 detour sites are real, relocatable prologues** — `va_disasm` on the loaded image:
  `0x8264B5F8`/`0x8259D948`/`0x8274ACF8`/`0x8276FBB0` each begin `7D8802A6 mflr r12`
  (first instruction PC-independent → HookFunction trampoline safe; the `bl` stack-check is
  the 2nd slot on A/B).
- **Cave is non-destructive** — `0x82C25000..0x82C25AF0` lies in the BINK→BINKBSS
  inter-section gap (BINK ends `0x82C24A10`, BINKBSS starts `0x82C30000`); overwrites no
  section's file data.
- **Cave is LIVE (mapped/committed/writable) — resolved statically from Xenia's loader.**
  `xex_module.cc:533` allocates the whole image `AllocFixed(base, uncompressed_size=image_size,
  Reserve|Commit, Read|Write)` then `memset(buffer,0,image_size)`. The cave (offset `0xC25000`)
  is well inside `image_size` (sections run to `.reloc` ~`0x82F40000`), so it is committed,
  zero-filled, and R/W; Xenia's JIT executes from any committed guest page (that is what
  `writable_code_segments` enables). This retires the Stage-5 "boot-spike: does the cave
  execute + do writes stick" risk without needing to reach gameplay.
- **BLOCKED on this host:** (1) this Xenia fork has **no Canary `.patch.toml` loader** (only a
  bespoke `dc3_nui_patch_resolver` + XEX-internal `xex_apply_patches`) — the toml must be tested
  on real **Xenia Canary**, or applied here by binary-patching the XEX bytes (VA→file-offset)
  instead; (2) **no RB3 360 game content** is present, and vanilla `default.xex` crashes early
  in headless boot (PC `0x8257EB40`, audio/subsystem-stub init — a vanilla missing-content
  crash, NOT our patch, which we did not apply and whose detours are all higher gameplay VAs
  this boot never reaches). So the live 2-guitar acceptance run (independent hits, no
  note-steal, teardown-leak) still needs the user's real Canary Xenia + full game, or hardware.

Net: the artifact is **statically verified correct and correctly targeted**, and the biggest
runtime unknown (cave liveness) is now resolved by loader analysis. What remains is behavioral
acceptance on a setup that boots to gameplay.

---

## LEVEL REACHED: **FULL blob `.patch.toml` — packed, relocated, selftest-GREEN.**

The XDK-free single TU was compiled with cl.exe (under wibo), relocated into a
fixed-VA game-XEX code cave, and emitted as a Xenia-canary `.patch.toml` that
installs Layers A/B/C + the gem-clone centerpiece statically. This is the
acceptance target from `build-without-xdk-recommendation.md` Path 1. No linker,
no imagexex, no XDK, no DLL required.

The pure-poke Layer-A+C fallback was ALSO produced (testable today, documented
boundary) — see below.

---

## Artifacts (all under `/home/free/code/milohax/RB3Enhanced/`)

| File | sha256 (short) | Notes |
|---|---|---|
| `scripts/objcave_pack.py` | `8b8a2056…` | the packer (COFF-PPC → cave blob → toml) |
| `build_patch/SameInstrumentHooks.obj` | `d6f69ca3…` | standalone obj (`-D SI_STANDALONE_PATCH`), machine 0x01F2 |
| `build_patch/SameInstrumentHooks_dllmode.obj` | `c122e9e6…` | DLL-mode regression obj (Stage-1 edits didn't break runtime build) |
| `build_patch/same_instrument_cave.bin` | `a7c95472…` | 2800-byte relocated cave image (blob incl. bss gap) |
| `patches/45410914_same_instrument_full.patch.toml` | `c777e1ac…` | **PRIMARY deliverable** — full feature, static |
| `patches/45410914_same_instrument_uispike_layerAC.patch.toml` | `1162bd4a…` | FALLBACK — pure-poke Layer A+C spike |

Source edits (uncommitted, in `RB3Enhanced/`):
`source/SameInstrumentHooks.c` (Stage 1.4 `SI_STANDALONE_PATCH` block + teardown),
`build_patch/crt/stdint.h` (shadow), `include/*` (Stage 1 pins, prior session).

---

## What the packer does (`scripts/objcave_pack.py`)

`pack --obj OBJ --band band.exe --cave-base 0x82C25000 --out-toml … [--out-blob …]`

1. **Parses the COFF** (machine 0x01F2 POWERPCFP): file header, 33 sections,
   135-entry symbol table (18-byte records + aux), string table (decoded
   directly — link.exe `/symbols` blanks long names, so the COFF is authoritative).
2. **Selects blob sections**: `.text`/`.rdata`/`.data`/`.bss`. **Drops** `.pdata`
   /`.xdata` (SEH unwind — unused for a manual cave), `.debug$S`, `.drectve`,
   `.XBLD$W`.
3. **Lays out at the cave base**, 16-aligned per section: `.text` → `.rdata` →
   `.data` → `.bss`, then a **memset thunk**, then **4 trampolines**.
   Total blob = **0xAF0 (2800) bytes**, cave `0x82C25000 … 0x82C25AF0`.
4. **Resolves every external**:
   * `IsActiveOrig / ResolvePartWaitOrig / ProcessConfigOrig / RecalcGemListOrig`
     → **cave trampolines** = `[orig_first_instr ; b target+4]`. The saved first
     instr is read from band.exe via its PE section table (NOT a flat
     `VA-0x82000000`) and asserted `== 0x7D8802A6` (mflr r12, position-independent).
   * `GameGemDBDuplicate / GameGemDBGetDiffList / GameGemListCopyFrom /`
     `BandUserSetOvershellSlotState / OvershellPanelUpdateAll / GetBandUserFromSlot /`
     `MemFree` → **fixed game VAs** (from `ports_xbox360.h`).
   * `__savegprlr_25/27/28/29 / __restgprlr_25/27/28/29` → **band.exe's own** CRT
     helpers (`config/45410914/symbols.txt`: `__savegprlr` @ `0x82803F00`,
     `__restgprlr` @ `0x82803F50`; full r14..r31 table wired).
   * `memset` → **hand-assembled cave byte-loop thunk** (8 instrs; no game memset
     symbol exists). `MemAlloc`/`memcpy` are NOT referenced.
5. **Applies relocations** (histogram of the standalone obj: REL24=37, REFHI=11,
   REFLO=12, PAIR=23, ADDR32=6):
   * **REL24** (`bl`/`b`): MS COFF stores the addend **PC-relative within the
     object** (in-place disp = `A − va`), so the true addend is recovered as
     `inplace + va`; final disp = `S + A − P`, range-checked ±32 MB. _(This was
     the one non-obvious bug found + fixed during bring-up: forgetting the `+va`
     made every call land short by its own section offset.)_
   * **REFHI+PAIR / REFLO+PAIR**: absolute hi/lo split of the final VA, with the
     standard `+0x8000` carry into the high half. All addends are 0 here (in-place
     immediates 0, PAIR.symidx 0).
   * **ADDR32**: absolute word (only appears in the dropped `.pdata`, so never
     applied).
   * **Aborts loudly** on any other reloc type.
6. **Emits** the `.patch.toml`: header/install notes, `title_id="45410914"`, the
   three TU5 basefile hashes (reused from the uispike toml), one `[[patch]]`
   `is_enabled=false` containing (a) every cave word, (b) an explicit `.bss`
   zero-fill (static-init insurance for `gClaimCount`/`gImplCount`/`gSISetupSeen`),
   (c) `gSameInstrumentEnabled = 1` @ `0x82C25AA0` (the feature switch), (d) the
   four detour pokes `be32 @ target = b hook_entry`.
7. **`selftest`** (all GREEN): every write inside the cave; no conflicting writes;
   each trampoline word1 branches to `target+4`; whole cave disassembles with
   **zero invalid instructions** (capstone PPC32-BE) and **every `b/bl` target lands
   in {cave ∪ pinned game VAs}**; the four detour target prologues re-verified
   `0x7D8802A6` in band.exe.

Independent spot-checks (disassembled from the emitted blob):
```
tramp IsActiveOrig  @0x82C25AD0:  mflr r12 ; b 0x8264b5fc   (= 0x8264B5F8+4)
memset thunk        @0x82C25AB0:  cmplwi r5,0; beqlr; mtctr r5; mr r9,r3;
                                  stb r4,0(r9); addi r9,r9,1; bdnz .-8; blr
IsActiveHook        @0x82C25080:  … bl 0x82c25ad0 (IsActiveOrig) ;
                                  lis r11,0x82C2; lwz r11,0x5AA0(r11)  (= flag 0x82C25AA0)
                                  cmpwi cr6,r11,0; beq …; cmpwi cr6,r3,0; bne …; li r3,1 …
```
The hook body matches the C source exactly, and the flag load resolves to the
correct cave VA — i.e. REL24 + REFHI/REFLO are both proven on real code.

---

## Cave (game XEX `band.exe`, base 0x82000000) — why here, not the DLL

`0x82C25000 … 0x82C25AF0` used of the `0x82C25000 … 0x82C2FFFF` (0xB000) gap
between `BINK` raw-end (`0x82C24C00`) and `BINKBSS` (`0x82C30000`). Owned by no
section, mapped + zero-filled at load. The cave lives in the **game** XEX (not
`RB3Enhanced.dll`) because (a) Xenia secondary-module patch application is
unproven, (b) the prebuilt 0.7 DLL's `config` has no `AllowSameInstrument`. So the
full artifact works **with or without** the DLL, and the `rb3.ini` flag is inert —
the switch is the patch's `is_enabled`. No collision: the 0.7 DLL hooks
`GAME_CT/GAME_DT/ADDGAMEGEM/WILLBENOSTRUM`, none of our four targets.

---

## Resolved-vs-unresolved relocation status

**All relocations in the packed sections are resolved.** No external is left
dangling; `selftest` proves no branch escapes {cave ∪ pinned}. Specifically:
- REL24 (37): all resolved (game fns / trampolines / CRT helpers / memset thunk).
- REFHI/REFLO/PAIR (46): all resolved to `.bss` globals' cave VAs.
- ADDR32 (6): all in dropped `.pdata` (SEH) — intentionally not emitted.

Nothing unresolved. The only *assumption* (not an unresolved reloc) is that Xenia
maps the BINK/BINKBSS gap as patch-writable + executable — see the boot-spike step
below, which the FULL patch's own blob writes already depend on.

---

## Machine-run acceptance — DONE (this session)

1. `objcave_pack.py selftest` GREEN (all assertions).
2. Four detour target prologues in band.exe still `0x7D8802A6`.
3. DLL-build regression: `SameInstrumentHooks.c` recompiles clean **without**
   `SI_STANDALONE_PATCH` (`build_patch/SameInstrumentHooks_dllmode.obj`, EXIT=0);
   its UNDEF set still carries `config`/`HookFunction`/`DbgPrint` → the normal
   RB3E runtime path is intact (Stage-1.4 edits are correctly `#ifdef`-gated).

Xenia is **not installed on this machine**, so the on-console/emulator steps are
user-run (below).

## User-run acceptance (Xenia Canary)

Prereqs in `xenia-canary.config.toml`: `apply_patches = true` +
`writable_code_segments = true`. Copy the toml into `patches/` next to
`xenia_canary.exe`. If your dump's hash isn't in the `hash` list, grep
`xenia.log` for `hash` and add it.

0. **Boot spike FIRST (prove the cave executes + writes stick).** Before the full
   blob, apply a 2-line toml: `be32 @0x82C25000 = 0x38600001` (li r3,1),
   `@0x82C25004 = 0x4E800020` (blr), and `@0x8264B5F8 = b 0x82C25000`. Boot: if
   duplicate instruments become selectable (the Layer-A behavior, now sourced from
   the gap), the gap is mapped, executable, and patch-writable → proceed. _(This
   is the one unproven assumption the full patch rests on.)_
1. Boot regression: full toml present, `is_enabled=false` → identical to stock.
2. `is_enabled=true`: xenia.log shows the patch applied; boots to song select.
3. Two controllers both pick Guitar: no grey-out (A), no ChoosePartWait kickback
   (B), no MILO_FAIL at assignment (C); both reach gameplay.
4. Independent hits: P1 misses a run P2 hits — P2 streak climbs, P1's breaks
   (gem-clone works, no note-steal).
5. Sustains/rolls per-player; simultaneous same-gem hits both register.
6. Clean song end: two distinct scores; no hang/assert.
7. Song A (2 guitars) → menu → song B, ×3: no crash (generation teardown freed
   the previous song's clones); leak sanity.
8. Scale: 3–4 players one instrument; mixed 2 guitars + drums; vocals unaffected.

---

## FALLBACK (also shipped) — pure-poke Layer A+C spike

`patches/45410914_same_instrument_uispike_layerAC.patch.toml` (no build, no DLL):
- Layer A: IsActive → `li r3,1; blr` (as the existing uispike).
- Layer C: `TrackNumOfExactType` occupancy-accept @ `0x8274AC8C`
  `beqlr cr6 (0x4D9A0020) → blr (0x4E800020)` — byte-verified in band.exe — so
  the 2nd same-type claimant reuses player-1's track and **reaches gameplay with a
  shared (note-stealing) chart**. Boundary documented in the file header: shared
  notes, possible Layer-B kickback, vocals affected, demo-only. Use the FULL patch
  for real per-player independence.

---

## What remains (FULL route follow-on — NOT this session)

- **Run the boot spike + user-run checklist in Xenia** (no Xenia on this machine).
  If the primary cave fails the spike, alternates: `0x8225B720–0x8225FFFF`
  (BINKCONS tail) or `0x82C148D4` (.text slack, trampolines only) — re-pack with
  `--cave-base`.
- **Confirm the TU5 XEX hash** for your dump matches one of the three in the toml
  (else add it from xenia.log).
- **rb3.ini-driven flag + RB3E_MSG logging + real DLL rebuild** (Path 2 / upstream
  CI) and **hardware XEX repack** (`idaxex`/xextool) — for on-console use rather
  than emulator. Neither is needed for the Xenia deliverable.
