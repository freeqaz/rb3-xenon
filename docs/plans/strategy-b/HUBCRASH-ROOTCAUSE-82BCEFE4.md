# Hub "crash" PC 0x82BCEFE4 / EA 0x7FEA1A80 — root-cause brief

**Lane:** RB3DX-RETARGET-PLAN Phase 4.1/4.2 (characterize + bounded-fix decision).
**Date:** 2026-07-13. **Status:** CHARACTERIZED. **hubOpen:** false (no emulator
fix warranted at this PC — see verdict).

## TL;DR

**0x82BCEFE4 is NOT a crash. It is Xenia's normal, working XMA-audio MMIO
emulation path**, mis-reported as `crash_guest` because the periodic thread-status
dump prints the *last soft-fault PC*, and every XMA hardware-register write in
Xenia is a deliberate SIGSEGV-trap-and-emulate. The faulting instruction is a
`stwbrx` (byte-reverse indexed store) + `eieio` — the canonical device-register
write — into the XMA decoder register aperture that Xenia intentionally leaves
PROT_NONE so it can intercept. The write is decoded (`movbe`), dispatched to
`XmaDecoder::WriteRegister`, and RIP advances. Recoverable **by construction**.
The two prior investigations already flagged it "benign recovered … unrelated to
the stall" (jit-fault-wiki BRIEF §9; doc-01:52). The hub-crash campaign has been
chasing a red herring: the real ~30 s termination is elsewhere and must be
re-captured with better instrumentation.

## The faulting instruction (real RB3DX bytes)

Flat image `xex1tool -b` of `rock-band-3-deluxe/platform/xbox/default.xex`
(sha1 c5a17091, base 0x82000000). Capstone PPC32-BE, `off = VA − 0x82000000`.

```
0x82bcef08  mflr r12; bl __savegprlr; stwu r1,-0xa0(r1)   ; <-- function entry
...  (per-context loop; index = (ptr - base)/0x40, base = *(r24-0x3098))
0x82bcefb8  li     r10, 1
0x82bcefbc  subf   r11, r11, r3          ; r11 = ptr_diff
0x82bcefc0  srawi  r11, r11, 6           ; index = diff / 64   (0x40-byte entries)
0x82bcefc4  clrlwi r11, r11, 0x10        ; index &= 0xFFFF
0x82bcefc8  rlwinm r9, r11, 0x1b,0x15,0x1f ; word_off = (index>>5) & 0x7FF
0x82bcefcc  sth    r11, 0x50(r31)        ; stash index
0x82bcefd0  clrlwi r11, r11, 0x1b        ; bit = index & 0x1F
0x82bcefd4  addis  r9, r9, 0x1ffb        ; \
0x82bcefd8  slw    r11, r10, r11         ;  |  mask = 1 << bit
0x82bcefdc  addi   r9, r9, -0x7960       ;  |  materialize XMA aperture base
0x82bcefe0  slwi   r10, r9, 2            ; /   r10 = (word_off + 0x1ffa86a0) << 2
0x82bcefe4  stwbrx r11, 0, r10           ; <<< FAULT: store mask, EA = r10, byte-rev
0x82bcefe8  eieio                        ; I/O ordering barrier  (== MMIO write)
```

- **Instruction:** `stwbrx r11, 0, r10` (`7d60552c`) — Store-Word-Byte-Reverse
  Indexed. RA=0 → **EA = r10**. Immediately followed by `eieio`. The byte-reverse
  store + I/O barrier is the textbook PowerPC uncached-**device-register** write.
- **EA = 0x7FEA1A80 is a hardcoded constant**, not a corrupted pointer:
  `(0x1ffb0000 − 0x7960) << 2 = 0x1ffa86a0 << 2 = 0x7FEA1A80` (verified; matches
  `last_fault=0x17FEA1A80` = host membase + guest 0x7FEA1A80 exactly). The
  `word_off` term (index>>5) adds +4 per 32 contexts; with index<32 it is 0, so
  the first write lands exactly on the aperture base seen in every report.

## What 0x7FEA1A80 is

**The Xbox 360 XMA audio-decoder hardware register aperture.**
`xenia/src/xenia/apu/xma_decoder.cc:113` registers it:
```
memory_->AddVirtualMappedRange(0x7FEA0000, 0xFFFF0000, 0x0000FFFF, this,
                               MMIOReadRegisterThunk, MMIOWriteRegisterThunk);
```
0x7FEA1A80 = base 0x7FEA0000 + 0x1A80 → register index `0x1A80/4 = 0x6A0`.
`xma_register_table.inc:68`: **0x06A0 = `Context0Clear`** (the context-clear
bitmap group). `XmaRegisterFile::kRegisterCount = 0x4000`, so 0x6A0 is well
in-bounds; `XmaDecoder::WriteRegister` handles it (xma_decoder.cc:334-345 —
iterates the bitmap, calls `context.Clear()` per set bit). Fully emulated, no OOB,
no assert.

## Function identity

Inverse-lookup of `base_to_tu5_map.json` (tu5-migrate worktree):
`tu5_va=0x82bcef08 size=272 base_va=0x82b9bd40 sym=XMAHALAllocateContexts`
(**HIGH** confidence, `body_identical=true`). Neighbors are all
XAUDIO2/XMAHAL (`XMAHALFreeContexts` @0x82bcf018, `CX2SourceVoice`, …). This is
the **XMA hardware-abstraction layer** allocating decoder contexts for audio
playback — a stock XAudio2/XMA library routine, NOT rbdxcache/Deluxe-injected
code (body-identical clean-TU5 == RB3DX). It loops over the requested context
list and pokes `Context0Clear` to reset each newly-allocated hardware context.

## Why it soft-faults, and why it is benign

Xenia emulates guest MMIO by leaving the device aperture **PROT_NONE** and
catching the access in the SIGSEGV handler:
1. Guest `stwbrx` → PPC JIT lowers to `ByteSwap`+`Store` → x86 **`movbe [mem],reg`**.
2. `movbe` into PROT_NONE 0x7FEA1A80 → host SIGSEGV.
3. `MMIOHandler::TryDecodeLoadStore` recognizes `movbe` store
   (mmio_handler.cc:144, `0F 38 F1` → is_load=false, byte_swap=true).
4. Dispatch → `XmaDecoder::WriteRegister(0x…1A80, mask)` → `Context0Clear` →
   `context.Clear()`; **RIP is advanced past the store**.

So each XMA register write is exactly one recovered soft-fault. The observed
count (`SIGSEGV 2→8`, and `168` in the longer stall log) is a *handful* of
audio-init register pokes — **three orders of magnitude below the OOM livelock's
~11,700/s** (jit-fault-wiki doc-09 §L1). It decodes and advances every time; it is
not a recover-without-progress spin.

## Triage of the plan's three EA hypotheses (in order)

1. **Recovered-fault-gone-fatal — CONFIRMED as the mechanism, REFUTED as fatal.**
   The EA is the recovered XMA-MMIO path, and it is benign by construction
   (decodes + advances). What changed with `--rb3dx_skip_calibration` is only that
   the game now runs far enough to *start audio* (main_hub music → XMA context
   allocation), so these normal writes appear where earlier boots never reached
   audio init. The "crash" label is a misattribution: the thread-status dump's
   `crash_guest` field records the **last soft-fault PC**, and the most recent
   soft-fault in a running title is almost always an XMA register poke. This
   matches both prior write-ups calling it "benign recovered … unrelated."
2. **Sign-extension / negative-offset off a large base — REFUTED.** 0x7FEA1A80 is
   a deliberate compile-time constant (`addis 0x1ffb / addi -0x7960 / slwi 2`)
   that equals Xenia's XMA aperture base to the byte. No pointer arithmetic off a
   guest base is involved; no register is mistranslated.
3. **Unmapped heap tail — REFUTED.** It is not heap. It is a **mapped MMIO device
   range** (`AddVirtualMappedRange(0x7FEA0000, …)`); the PROT_NONE is intentional
   (the trap *is* the emulation). Backing it with real memory would **break** XMA
   audio by removing the intercept — the opposite of a fix.

## Fix recommendation

**Do NOT implement an emulator mapping/backing/guard fix at 0x82BCEFE4.** There is
no defect here; the fault is correct XMA MMIO emulation. A backing/mirror (the
fb864e3e-style precedent the plan floated) would silently disable XMA register
interception and corrupt audio. `hubOpen` stays **false**: no fix was warranted,
so none was implemented.

The real work item is a **characterization re-do, not a fix**:
- The `crash_guest`/`last_fault` fields in the thread-status dump only name the
  last recovered soft-fault. To find the true ~30 s terminator, instrument Xenia
  to log the **actual fatal thread + host backtrace at teardown** and the last
  **non-0x7FEAxxxx** guest PC per thread. Strong prior candidate: the pre-existing
  host-teardown core-dump (`handles_.empty()` / `terminate`, doc-07:187) that
  fires when the headless harness exits — i.e. main_hub may already be *surviving*
  and the "crash" is an exit-path artifact.
- Concretely: add a title-gated (0x45410914) counter that (a) suppresses/rate-caps
  the XMA soft-fault log spam so it stops masking the real event, and (b) prints
  the terminating signal's origin. Then re-run the `/tmp/rb3dxboot` +
  `--rb3dx_skip_calibration` ladder and check whether main_hub renders 60 s and
  scripted nav (START at title, doc-09 §L4) reaches song_select **without** any
  emulator change. That test — not a patch to this PC — is what would set
  hubOpen=true.

## Evidence / repro

```
# flat image (base 0x82000000; /tmp volatile)
xex1tool -b /tmp/rb3dx-wf/rb3dx_decomp.bin \
  ~/code/milohax/rock-band-3-deluxe/platform/xbox/default.xex   # xex sha1 c5a17091
# disasm the store
python3: capstone PPC32 BE, off = 0x82BCEFE4-0x82000000  ->  7d60552c  stwbrx r11,0,r10
```
- Map: `rb3-xenon/.claude/worktrees/tu5-migrate/_tu5probe/tu5_migrate/base_to_tu5_map.json`
  → `XMAHALAllocateContexts` @ tu5 0x82bcef08 / base 0x82b9bd40.
- Xenia: `apu/xma_decoder.cc:113` (range), `:294-345` (WriteRegister/Context*Clear),
  `apu/xma_register_table.inc:68` (0x06A0=Context0Clear),
  `cpu/mmio_handler.cc:144` (movbe-store decode).
- Prior art: jit-fault-wiki `BRIEF-main-hub-load-stall.md:248,274`,
  `01-symptom-and-evidence.md:49-57`, `07-fix-and-verification.md:208`.
