# RB3Enhanced 0.7 (Xbox 360) — release fetch + DLL layout + code-cave decision

Executor: Stage-3 FETCH (Same-Instrument XDK-free patch plan). Session 2026-07-07.
Purpose: fetch the prebuilt RB3E 0.7 Xbox binary and choose a code-cave VA for the
`.patch.toml` packer. **Bottom line up front: the cave does NOT go in the DLL — it
goes in the game XEX (`band.exe`) gap at `0x82C25000` (0xB000 budget), and that VA is
byte-verified below.**

---

## 1. Release fetch — contents + hashes

Downloaded via `gh release download 0.7 -R RBEnhanced/RB3Enhanced -p '*Xbox*'`.
Asset `RB3Enhanced_0.7-Xbox.zip` (216892 B). Unzipped to
`/home/free/code/milohax/rb3-xenon/_rb3e07/rb3e07/`.

| File | Size (B) | sha256 |
|---|---:|---|
| `RB3Enhanced.dll` | 61440 | `c9ceba5b162c5d9c0dd5c0fef2e88371b5b49de324ef116c21e6dfe059ed01d7` |
| `RB3ELoader.xex` | 20480 | `b8e9088cb2a7686973ec8071b297a98cf73269011db84b7a1f36248c032b11e0` |
| `rb3.ini` | 2824 | `a6badd5f8d23d7befbe278ab48c740abf78ffeacecc31eaa23d8c28a59f2da17` |
| `INSTALLING_360.txt` | 2577 | `25fbe7b41231f1644ccb855fa1c4539993d15e5e20b9383d300cbf990446d44b` |
| `rb3/rb3e_index.html` | 22653 | `d150ff05619cd07e8880089f52924c925ecb80dc8db27156489fd9d0469365f6` |
| `rb3/config/gen/preload_subdirs.dtb` | 5529 | `d8d5ed1ea9eabaa7e81eb10a0ffd8641c92c8d616a8feda61a11912ed6fe6f65` |
| `rb3/ui/resource/fonts/gen/game_origins.milo_xbox` | 268414 | `fb04fd510936016a1c5570ad8a725d3b371d7e02f6717bea6bccbfa8406d5035` |
| `rb3/ui/resource/list/gen/list_song_select_browser.milo_xbox` | 364151 | `be60ff2f6f681a13bcb46205f7654fe8ef4a6ff59e69c9d1e49ac274c335910f` |

Expected trio present: **`RB3Enhanced.dll` + `RB3ELoader.xex` + `rb3.ini` — CONFIRMED.**
Plus a `rb3/` rawfiles payload (fonts + song-select milo + preload dtb) and the install README.

### Install paths (from `INSTALLING_360.txt`, closes plan §4.5)
- `RB3ELoader.xex` → root of HDD or USB, selected as a **Dashlaunch Plugin** entry
  (RGH/JTAG only — does not work on retail/softmod).
- `RB3Enhanced.dll` + `rb3.ini` → the RB3 install folder, **or** root of HDD/USB.
- `rb3/` folder contents → merged into the RB3 install folder (or copied to HDD/USB root).
- Loader is the plugin; it injects `RB3Enhanced.dll` into the running title. Success banner:
  "RB3Enhanced 0.7 loaded" on the main menu.
- `rb3.ini` has no `AllowSameInstrument` key (0.7 predates the feature) — **inert for our patch**;
  our feature switch is the `.patch.toml` `is_enabled`, not the ini.

---

## 2. `RB3Enhanced.dll` — XEX2 header (parsed, no XDK)

Parsed the XEX2 container directly (`struct`, big-endian). Optional-header directory (9 entries):

| key | name | value |
|---|---|---|
| `0x000003FF` | FILE_FORMAT_INFO | off `0x2C4` |
| `0x00010001` | (module flags/orig-base word) | `0x88000000` |
| `0x00010100` | ENTRY_POINT | **`0x8401B590`** |
| `0x00010201` | IMAGE_BASE_ADDRESS | **`0x84000000`** |
| `0x000103FF` | IMPORT_LIBRARIES | off `0x0C80` |
| `0x00018002` | (tls/misc) | off `0x2E8` |
| `0x000183FF` | STATIC_LIBRARIES | off `0x2F0` |
| `0x000200FF` | (checksum/timestamp) | off `0x304` |
| `0x00040404` | (page-heap / stack) | off `0x388` |

- **Load base VA = `0x84000000`** (matches plan §4.3 / `xex.xml` convention). ✔
- **Entry point = `0x8401B590`** (image-relative +0x1B590).
- Security info @ `0xE0`: `header_size=0x1E4`, **`image_size=0x40000`** (256 KiB virtual, `0x84000000–0x84040000`).
- **FILE_FORMAT_INFO** @ `0x2C4`: `info_size=0x24`, `encryption_type=0` (**none**),
  `compression_type=2` (**NORMAL = LZX**), `window_size=0x8000`, `first_block_size=0xD800`,
  block sha1 `556e01ff…3f7a`.
- **IMPORT_LIBRARIES**: `xam.xex`, `xboxkrnl.exe` → a standard Xbox 360 XDK-linked module
  (confirms it is a real 360 XEX-DLL, not homebrew-stubbed).

### 2a. PE section table — NOT extractable here, and NOT needed
The PE payload (file offset `0x1000`) is **LZX-compressed** (`compression_type=2`): the file
contains **no** `MZ`/`PE\0\0`/`.text`/`.rdata`/`.data`/`.reloc` ASCII — a raw scan finds none.
Reading the DLL's internal section table (`.text` range, slack, spare section) would require an
LZX decompressor keyed off the block map. **No XEX/LZX tool is available on this machine**
(`xextool`, `idaxex`, `xenia` all absent; only `wine` + the Ghidra-Java `XEXLoaderWV` extension,
which is not a CLI). Encryption is `none`, so a decompressor alone would suffice — but see below.

**This is not a blocker.** The plan's design decision (§4, "read carefully") places the code cave
in the **game XEX's** address space, not the DLL's, for two reasons: (1) it is unproven whether
Xenia applies `.patch.toml` writes to *secondary* modules (the injected DLL) vs only the title
module; (2) the 0.7 DLL's `config` struct has no `AllowSameInstrument` field anyway. So the DLL's
internal layout is informational only — we never write into `0x84xxxxxx`. If a future hardware
(non-Xenia) route needs a DLL-resident cave, decompress first (port `XEXLoaderWV`'s LZX, or run
`idaxex`/`xextool` on a Windows box); the 256 KiB image almost certainly has BSS/`.XBLD` slack, but
that is out of scope for the Xenia artifact this session targets.

---

## 3. Target confirmation — TU5 / title `45410914`

- The DLL imports `xam.xex` + `xboxkrnl.exe` and loads at `0x84000000` — the canonical RB3E
  injected-plugin base. It hooks the running **Rock Band 3** title; RB3E 0.7's `title_id`
  targeting is `45410914` (the same title the packer's `.patch.toml` keys to).
- The game basefile the packer edits is `orig/45410914/band.exe` (decompressed TU5, base
  `0x82000000`) — **its four detour-target prologues were re-verified this session**, all
  `0x7D8802A6` (`mflr r12`), position-independent (safe to copy into a trampoline word0):

  | target VA | function | prologue |
  |---|---|---|
  | `0x8264B5F8` | IsActive | `7D8802A6` ✔ |
  | `0x8259D948` | ResolvePartWaitStates | `7D8802A6` ✔ |
  | `0x8274ACF8` | ProcessConfig | `7D8802A6` ✔ |
  | `0x8276FBB0` | RecalcGemList | `7D8802A6` ✔ |

- **Collision (planner-verified, cited):** the 0.7 DLL runtime-hooks `GAME_CT`/`GAME_DT`/
  `ADDGAMEGEM`/`WILLBENOSTRUM`/… — **none** of our four targets. No detour conflict. Never poke
  `PORT_GAME_CT`/`PORT_GAME_DT` (the DLL's `HookFunction` would relocate our PC-relative `b` into
  its own trampoline and jump wild).

---

## 4. How RB3E installs hooks (so the static blob can piggyback)

From branch `feature/same-instrument` source:

- **Entry**: `source/xbox360.c:143 DllMain` → the loader injects the DLL; RB3E hooks the game's
  App bring-up and runs `StartupHook(ThisApp, argc, argv)` (`rb3enhanced.c:482`), which calls
  `InitialiseFunctions()` (resolves game VAs) then, in the init flow, `InitSameInstrument()`
  (`rb3enhanced.c:474`).
- **Detour primitive** (`source/utilities.c:10`):
  ```c
  void HookFunction(unsigned int OriginalAddress, void *StubFunction, void *NewFunction) {
      unsigned int *orig = (unsigned int *)OriginalAddress;
      unsigned int *stub = (unsigned int *)StubFunction;
      stub[0] = orig[0];                          // save target's 1st instr (mflr r12)
      stub[1] = B(&orig[1], &stub[1]);            // trampoline: branch back to target+4
      orig[0] = B((unsigned int)NewFunction, OriginalAddress); // detour: 1st instr -> B(hook)
  }
  ```
  `B(dest,src) = 0x48000000 + ((dest-src) & 0x3FFFFFF)` (`include/ppcasm.h:20`).

**Piggyback design**: the static `.patch.toml` replicates `HookFunction` *at pack time* inside the
game XEX — for each target it emits a cave trampoline (`word0 = original mflr r12`,
`word1 = B(target+4, tramp+4)`) and a detour poke (`B(hook_entry, target)` over the target's
first instruction). This needs **nothing** from the DLL runtime, so the artifact works **with or
without** the DLL installed (DLL recommended for the rest of RB3E, but not required). RB3E's own
runtime detours branch from game `.text` (`0x82…`) into DLL trampolines (`0x84…`, ~31 MB away,
within ±32 MB REL24) — orthogonal to our in-XEX cave whose branches stay ~5–7 MB local.

*Note on the `B` macro:* RB3E masks `& 0x3FFFFFF` (26 bits incl. low 2) vs the packer spec's
`& 0x3FFFFFC`; identical result because all `dest`/`src` are 4-aligned. Keep the packer's
`0x3FFFFFC` form (it zeroes AA/LK correctly by construction).

---

## 5. Recommended code cave (game XEX) — VERIFIED

`band.exe` PE (base `0x82000000`, 12 sections). Relevant tail:

| section | VA | VSZ | raw end (max(vsz,rsz)) |
|---|---|---|---|
| `.text` | `0x82260000` | `0x009B48D4` | `0x82C14A00` |
| `BINK` | `0x82C14A00` | `0x00010010` | `0x82C24C00` |
| `BINKBSS` | `0x82C30000` | `0x000043A0` | `0x82C343A0` |
| `.data` | `0x82C34400` | `0x001F35EC` | … |

- **Primary cave: `0x82C25000`, budget `0xB000`** (`0x82C25000–0x82C2FFFF`). Sits in the
  inter-section gap after `BINK`'s raw data (`0x82C24C00`) and before `BINKBSS` (`0x82C30000`).
  Machine-verified this session:
  - **owned-by-any-section = False** (outside every `[va, va+max(vsz,rsz))`),
  - budget to `BINKBSS` = **exactly `0xB000`**,
  - zero-filled at load (past `BINK` raw end; `BINKBSS` is `rptr=0` bss) — Xenia maps the image
    contiguously (sections exist on both sides), so the gap pages are committed + zeroed.
- Executability of the gap is the one thing static analysis can't prove → **boot-spike first**
  (plan §4.1): poke `li r3,1; blr` at `0x82C25000` + `b 0x82C25000` at IsActive `0x8264B5F8`,
  confirm the Layer-A spike reproduces in Xenia before packing the real blob.
- **Alternates** if the spike fails: `0x8225B720–0x8225FFFF` (BINKCONS tail, ~18.6 KB — verify
  zeros), or `0x82C148D4–0x82C14A00` (.text tail slack, 300 B — trampolines only).

Size budget check: full blob (reachable hook COMDATs + `.data`/`.bss` globals + 4 trampolines +
optional memset/memcpy stubs) is well under `0xB000`; the `0.7`-era Layer A/B/C hook set is a few
hundred instructions.

---

## 6. Answers to the plan's open questions

- **#1 (addresses)** — CLOSED upstream (Layer-B VAs byte-derived in the plan; four detour
  prologues re-verified `7D8802A6` here).
- **#2 (zip contents)** — CLOSED: trio + `rb3/` rawfiles present, hashes recorded (§1).
- **#3 (cave)** — CLOSED: **`0x82C25000` / `0xB000`** in the game XEX, section-table-verified
  (§5); DLL cave rejected by design (§2a).

Follow-on (not this session): if a hardware/non-Xenia route ever needs the DLL section table,
LZX-decompress `RB3Enhanced.dll` (compression_type=2, window `0x8000`, block map at `0x2C4`) via a
ported `XEXLoaderWV` LZX or `idaxex`/`xextool` on Windows.
