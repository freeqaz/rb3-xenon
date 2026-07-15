# Live crash capture — 2-same-instrument song load (2026-07-14)

> **Note (corrected 2026-07-15):** the `wine xextool -m d -c c` step in the
> `SI-LOADABLE-RECIPE` section below turned out to be **load-critical, not
> incidental** — the compressed container is what makes the module load
> (`XexLoadImage` rejects the raw one). It is now automated as
> `tools/oss-xbox-build/pack-si-dll.sh` step [3]; don't run it by hand — use
> `build-si.sh`. Current workflow:
> [`docs/tools/LIVE-DEBUG-RUNBOOK.md`](../../tools/LIVE-DEBUG-RUNBOOK.md).
> Read this doc for the **crash story**.

Captured from the live dev console (192.168.8.180) via XBDM while the native
Format=1 RB3Enhanced.dll (stock repack, **no SI feature**) was running. The game
runs fine until a song with **two of the same instrument** is loaded, then crashes
on the loading page.

## Exception

```
code=0xC0000005 (access violation)   thread=0xf9000000 (main)   first-chance
read at address=0x0000000F           fault PC (Iar)=0x8279da90
```

Fault PC is in **`default.xex`** (RB3 game, base `0x82000000`, size `0xEF0000`) →
**RVA `0x0079DA90`**. Not in our DLL (`0x84000000`) or the loader. `Nova.xex`
(0x91a00000) is also mapped (RB3 Deluxe layer).

## Faulting instruction sequence (default.xex+0x79DA7C … +0x79DA90)

Disassembled from `getmem addr=0x8279da78 length=0x40`:

```
8279DA78  4BFFFD91  bl      0x8279d808            ; LR := 0x8279DA7C
8279DA7C  817D0044  lwz     r11, 0x44(r29)        ; r11 = this->[0x44]  (= 0x46EDCCF8, array base)
8279DA80  5797103A  rlwinm  r23, r28, 2,0,29      ; r23 = r28<<2  (index*4), r28 = -1
8279DA84  7F84E378  mr      r4, r28               ; arg = r28 = 0xFFFFFFFF
8279DA88  7FA3EB78  mr      r3, r29               ; arg = this
8279DA8C  7D77582E  lwzx    r11, r23, r11         ; r11 = *(array + index*4) -> 0xFFFFFFFF (-1)
8279DA90  82CB0010  lwz     r22, 0x10(r11)        ; deref (-1)+0x10 = 0x0000000F  => FAULT
```

**Immediate cause:** `r11 = 0xFFFFFFFF` (the `-1` "not found / unassigned" sentinel)
is dereferenced as a pointer at `+0x10`, wrapping to address `0xF`.

## Register context (thread 0xf9000000)

```
Iar=0x8279da90  Lr=0x8279da7c  Msr=0x00009030  Cr=0x44000488
r3 =0x425ed168 (this)   r29=0x425ed168 (this)   r1(sp)=0x7004f460
r4 =0xffffffff          r11=0xffffffff          r28=0xffffffff   <-- the -1 sentinels
r23=derived from r28    r5 =0x42b5a708          r27=0x45fe0c20
```

Three registers carry `0xFFFFFFFF` (`r4`, `r11`, `r28`) — a lookup keyed on an
invalid/duplicate index (`r28 = -1`) returned `-1`, used unchecked as a pointer.

## The object (`this` = 0x425ed168)

`getmem addr=0x425ed168 length=0x50` (vtable `0x8210BD54`):

```
+0x00 vtable=0x8210BD54
+0x0C = 0x00000002          <-- count = 2  (the two same-instrument tracks)
+0x10 = 0x00000006
+0x14 = 0x00000004
+0x1C = 0x425EFA80 (ptr)
+0x20 = 0xFFFFFFFF          <-- unassigned/duplicate slot (-1)
+0x24 = 0xFFFFFFFF          <-- unassigned/duplicate slot (-1)
+0x28 = 0x0000000F
+0x2C..0x40 = 0x46EE2388 / 0x46A79230 ... (array of ptrs)
+0x44 = 0x46EDCCF8          <-- the array base loaded into r11
```

## Backtrace (default.xex return addresses on the stack)

`0x8279E03C` (near the fault — likely the caller), then
`0x8252016C`, `0x8227CF94`, `0x823D14E8`, `0x8282A178`.

## Diagnosis

With **two players/tracks on the same instrument**, an instrument→slot (or
track→player) index lookup returns **-1** — the base game assumes a distinct/valid
mapping. The code then indexes an array by that -1, loads a **-1 pointer**, and
dereferences it at `+0x10`, crashing on the song loading page. The object even
stores the two -1 slots at `+0x20`/`+0x24` and a count of `2` at `+0x0C`.

This is the **base-game defect the SI feature exists to fix**. The DLL currently on
the console is the *native Format=1 repack of stock RB3Enhanced* (proof-of-load for
the xex-patcher writer) — it does **not** contain the SI feature, so the unguarded
2-same-instrument path runs and faults.

## Next steps

- The fix belongs at/above `default.xex+0x79DA90`: guard the `-1` lookup result
  before the `lwz r22,0x10(r11)` deref (or ensure the instrument→slot map assigns a
  valid slot when two tracks share an instrument — the SI feature's job).
- To reproduce/fix on hardware, load the **SI-feature build** (not the stock repack)
  and re-run the 2-same-instrument load. The xex-patcher writer can now package that
  build natively once its base image is prepared (Track A relocation / the SI cave).
- Full raw capture retained at `/tmp/crash_notify.log` on the dev host for this run.

---

# UPDATE — SI build loads and FIXES crash #1; reveals crash #2 (2026-07-14, 19:07)

The `feature/same-instrument` build was made loadable (thunk-repair + repack, see
`SI-LOADABLE-RECIPE` below) and run on hardware. Loading a 2-same-instrument song
(`SONG_NAME "The Sword (2x Bass Pedal)"`, venue `arena_06`) now **gets past the
original loading-page crash** — the song metadata loads and the venue is set — then
faults at a **different, second** location.

## Crash #2 exception

```
code=0xC0000005   thread=0xf9000000   read at address=0x00000008   first-chance
fault PC (Iar)=0x8274e584   (default.xex+0x74E584)   Lr=0x8274e578
```

## Faulting instruction (default.xex+0x74E570 = function entry)

```
8274E570  7D8802A6  mflr r12            ; save caller return addr -> r12 (=0x8274FC98)
8274E574  48511A15  bl   (prologue helper)
8274E578  FBC1FFE8  std  r30,-0x18(r1)
8274E57C  FBE1FFF0  std  r31,-0x10(r1)
8274E580  9421FF90  stwu r1,-0x70(r1)
8274E584  81630000  lwz  r11, 0(r3)     ; deref first arg r3 = 0x00000008  => FAULT at 0x8
```

`r3 = 0x00000008` (Gpr3, also Gpr31) — the function is called with a near-null
pointer (offset 0x8 of a NULL object) as its **first argument** and dereferences it
immediately. Many registers are 0 (r5,r6,r7,r22,r23,r25,r28,r30).

## Call chain — the SI DLL is IN it

Stack return addresses (r1=0x7004f2b0), top-first:
```
0x827606C4  (default.xex)
0x84011EA4  (RB3Enhanced.dll + 0x11EA4)   <-- the SI feature code
0x8274FC98  (default.xex+0x74FC98 — direct caller of the crashing fn; = r12)
0x8274D364  0x8277B500  0x827501B8  0x8268AEAC   (default.xex)
```

So crash #2 is reached **through the SI feature's own code** (`RB3Enhanced.dll+0x11EA4`
→ game `+0x74FC94` → crashing fn `+0x74E580`). The SI guard cleared crash #1 and let
execution proceed into a **second null-object site** the feature now owns: either a
second place the 2-same-instrument path must populate, or the SI hook leaves an object
un-initialized before this game function reads it.

## Fix direction

- The SI hook at `RB3Enhanced.dll+0x11EA4` sets up (or should set up) the object that
  game fn `default.xex+0x74E580` dereferences via its first arg. That object is NULL
  (arg = 0x8 = &NULL->field8) in the duplicate-instrument case. Ensure the SI path
  populates it (or the caller at `+0x74FC94` guards the NULL) before this call.
- Reproduce: SI build loaded, load `The Sword (2x Bass Pedal)` (or any 2-same song).
- Loadable SI build artifact: `artifacts/RB3Enhanced_si_loadable_26f550c6.dll`.

## SI-LOADABLE-RECIPE (how the from-source SI build was made loadable — no cave splice)

The `feature/same-instrument` from-source build was rejected only because of the
import **thunks** (word1 left as `lwz r11,off(r11)` IAT-indirect form). Repair + repack:
1. In the raw (`-e d -c b`, Format=1) base, for each **type-1** thunk (word0 high byte
   `0x01`), rewrite `word0=0x01|mod|ord`, `word1=0x02|mod|ord`, taking the **ordinal
   from word0** (word1's low 16 bits are the IAT offset, not the ordinal), module index
   from the import-table library order. Leave type-0 IAT slots alone. (68 thunks fixed.)
2. `xextool -m d -c c` → regenerates page-hash chain, import digests, and devkit
   signature, compresses to the proven Format=2 envelope.
3. `xexlint` → clean (0 reject), then hardware load → **LOADS** (RB3ELoader "Loaded",
   RB3E ALIVE, module mapped @0x84000000).

This overturns the earlier "synthesized import table is a dead-end" conclusion — that
was only ever tested with word0 fixed (`fromsource_modfix`); fixing **word1** too makes
the from-source build loadable. **Track-A cave-splice is unnecessary.** The generic
`fix_thunks.py` bug (read ordinal from word1) is fixed in the xex-patcher repo.

---

# UPDATE — crash #2 static analysis + IMAGE-MISMATCH blocker (2026-07-14, fork)

## The SI hook in the chain is `DataNodeGetObjHook`

`RB3Enhanced.dll+0x11EA4` resolves (via `deploy-si-rb3dx/RB3Enhanced.map`) to
**`DataNodeGetObjHook`+0x44** (`DataDebug.obj`; source `RB3Enhanced/source/DataDebug.c:191`).
That +0x44 return address is the `bl DataNodeGetObj(node)` call — the trampoline to the
**original game** `DataNode::GetObj` (`PORT_DATANODEGETOBJ = 0x8274b088`). So crash #2
happens **inside stock `DataNode::GetObj`'s object resolution**, reached because the hook's
type-guard passed the node through:

```c
DataNode *eval = DataNodeEvaluate(node);
if (eval->type != OBJECT && eval->type != STRING_VALUE && eval->type != SYMBOL)
    return NULL;                 // guards node TYPE
return DataNodeGetObj(node);     // <-- crashes INSIDE here (null object, r3=0x8)
```

The guard checks node **type**, not the **null-object** that `GetObj` resolves to for the
duplicate instrument — so it doesn't cover this fault.

## BLOCKER: `orig/45410914/band.exe` is NOT the image on the console

The game-side crash RVAs cannot be resolved against the decompressed image in this repo.
Proven three ways:
- `band.exe @ 0x8274b088` (port's `DataNode::GetObj`) is **mid-function junk**, not a
  prologue — band.exe does not honor the port addresses.
- console crash #1 VA `0x8279DA7C` is a `blr` epilogue in band.exe; the crash #1 code
  actually sits at band.exe `0x82778ADC` (a **-0x25260** offset).
- crash #2's exact function bytes are **absent from band.exe entirely** → not a constant
  relocation, a genuinely **different build** (the RB3 Deluxe / real-console default.xex).

The **port header and the console agree with each other** (GetObj `0x8274b088`, crash
`0x8279DA7C`); band.exe is the outlier. Disassembling +0x74E570 / +0x74FC94 against band.exe
produces unrelated code (confirmed) and will mislead.

## Revised next steps

1. **Cold-reboot** the console (crash-looping first-chance).
2. **Pull the console's actual `default.xex`** (FTP 192.168.8.180) — the RB3DX build — and
   decompress it (`xextool -e u -c u`) into a PE whose VAs match the port header. **Verify**
   `0x8274b088` is a clean prologue and crash #1 code is at `0x8279DA7C` before trusting it.
3. Only then disassemble the **real** crash fn `0x8274E570`, its caller `0x8274FC94`, and
   `DataNode::GetObj 0x8274b088` to identify which object/field (`+0x8`) is null and why the
   duplicate-instrument path yields it.
4. **Fix (pending step 3):** either extend `DataNodeGetObjHook` to detect the
   duplicate-instrument / null-resolution node and return NULL before the `bl DataNodeGetObj`,
   or fix the DTA data the SI feature added that references a per-instrument object slot that
   is null for a shared instrument. Choice depends on step 3.
5. Rebuild → thunk-repair → `xextool -m d -c c` → `xexlint` clean → one hardware cycle.

(Adjacent, out of this directive: the sibling hooks in `DataDebug.c` — `DataSetElemHook`,
`DataOnElemHook` — share the "guard type, not null-resolution" shape and may want the same
review.)

---

# UPDATE — crash #2 FIXED on hardware; crash #3 surfaced (2026-07-14, live)

## Crash #2 root cause + fix (shipped, loads)

Traced on the **real console image** (`_dbg/default.base`, not band.exe): crash #2's
`ObjectDir::FindObject` (`0x82750188`) was entered with a **NULL directory** — the
DTA object resolution (`DataNode::GetObj` → recursive find) seeded the search with a
null "this" dir, and the stock code deref'd `this+8` (the name hashtable) with no
null check → fault at `0x8274E584` (`r3=0x8`).

**Fix (DLL hook, additive):** new `ObjectDirFindObjectHook` in `RB3Enhanced` guards
`dir == NULL` → returns NULL (a null dir has no objects), installed via
`HookFunction(PORT_OBJECTDIRFINDOBJECT=0x82750188, …)`. Files: `ports_xbox360.h`,
`_functions.c` (`RB3E_STUB`), `rb3/Data.h` proto, `DataDebug.c` (hook body),
`rb3enhanced.c` (install). Built XDK-free (`XDK_OSS=H-headers/xdk-oss`, +
`_xdk_stubs.obj`), packed loadable via `pack-si-dll.sh` (xexlint PASS, sha
`e7ffb3a5…`), deployed. **On hardware the 2-same-instrument load advanced past
`0x8274E584`** — crash #2 is fixed.

## Crash #3 (revealed by the fix) — same null dir, one consumer deeper

- Fault `0x82B998D4`: `lbz r11, 0x170(r31)`, **r31 = NULL**; fn `0x82B99878`,
  resolving the object name **`smasher.trans`**.
- The callee `0x8227D418` (whose NULL return takes the faulting not-found branch)
  itself calls `ObjectDir::FindObject` (`0x82750188`) — **the function we just
  guarded**. So the *same* null directory now makes `FindObject` return NULL instead
  of crashing, and this caller derefs `r31` on the not-found path unguarded.
- ⇒ The `return NULL` guard is masking, not root-causing: the real defect is the
  **null object directory** during dup-instrument load (`this->[0xC8]` →
  `0x82BAA4D8` → `->[4]` yields null). Root-cause trace in `CRASH3-TRACE.md`.

## Tooling added (this loop)
- `tools/xdbg.py` — live-crash capture (`xdbg crash`) + symbolized disasm/xref/mem/
  deref against the real console image.
- `tools/oss-xbox-build/pack-si-dll.sh` — PE → loadable, xexlint-gated DLL (one command).
- `docs/plans/si-hw-fix/DEBUG-WORKFLOW.md` — the full build→pack→deploy→capture→fix loop.
