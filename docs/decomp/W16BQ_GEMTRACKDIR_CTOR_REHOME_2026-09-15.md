# W16-BQ — the GemTrackDir constructor block: re-homed, named, and the defect that naming exposed

**Branch** `w16-bq` · **base** main `4108276e` · **date** 2026-09-15 · ruler `name_check`
(`functionRelocDiffs=name_check`, `ppc.calculatePoolRelocations=false`, read from
`report.json`'s own `provenance.diff_config`, not assumed).

## 0. Result

| | matched_functions | matched_code | matched_code_percent | total_functions | total_code |
|---|---:|---:|---:|---:|---:|
| **before** (`4108276e`) | 43,550 | 4,041,980 | 39.445232 | 69,240 | 10,247,068 |
| **after** (`88dd73ad`) | 43,605 | 4,042,904 | 39.454250 | 69,240 | 10,247,068 |
| **Δ** | **+55** | **+924 B** | **+0.009018** | **0** | **0** |

`total_code` Δ0 across all four commits: three edits were re-homes/namings and the fourth was
an **addition** over `auto_*` code, which pin-neutrality prices at exactly 0.

**The two kinds of gain are separated by the honest floor (`matched − masked_equal`):**

| commit | Δfns | Δbytes | masked_equal | honest floor | reading |
|---|---:|---:|---:|---:|---|
| main | — | — | 23,054 | 20,496 | baseline |
| A re-home | +51 | +620 | 23,105 (+51) | 20,496 (**+0**) | **100 % disclosure** |
| B naming | 0 | 0 | 23,105 | 20,496 | exposure only |
| C source fix | +4 | +304 | 23,105 | 20,500 (**+4**) | **genuine source progress** |
| D pin+name | 0 | 0 | 23,105 | 20,500 | exposure only |

⇒ **Do not book the +55 as source progress.** Exactly **4** of it is; the other 51 is
attribution accuracy that objdiff discloses as `masked_equal`. The +620 B is real
`matched_code` bought by pairing rows against the right base obj.

## 1. Baseline verification — the brief was right on the numbers

The brief's figures were tested literally in the worktree (full `./tools/ninja-locked` rc=0,
then `report.json`) **before** anything was built on them:

> 43,550 fns / 4,041,980 B / 39.445232 % · `total_code` 10,247,068 · `total_functions` 69,240 ·
> `fuzzy` 49.70534 · `masked_equal_functions` 23,054 · objdiff `5a51cd51fe0a353f` ·
> tool commit `a5f0ea903ec1` · ruler `name_check`

All exact. `tools/rowset_snapshot.py diff ~/tmp/rows_w16bp_main.json` reported
**0 rows crossed in, 0 fell out, net +0** — the lane's starting rowset is bit-identical to the
coordinator's baseline snapshot.

### 1.1 Where the brief was wrong

Three corrections, none fatal to its thesis:

1. **§1 (2) says the map keys `0x8227bcc0` / `0x8227bea0` are `None`.** They are **absent
   keys**, not `None` values. (The map does contain 101 genuine `None` values, so the
   distinction is not academic — a `isinstance(v, str)` guard and a `k in d` test answer
   different questions.)
2. **§3's ctor-bloat premise is a measurement artifact.** See §6 — it compares a COMDAT span
   against a `.pdata` extent, and the true direction is the opposite of the one stated.
3. **§1 (1) predicted `fn_8227BCC0` does `bl StaticClassName` then `li r3,0x7f0`.** Correct, but
   there is an `li r4,0x0` between them that the brief omits — and that `r4` is load-bearing
   evidence (it is the 2-arg `MemAlloc(size, 0)` of shape (b); see §5).

## 2. The identification — four independent legs

**Claim: `0x822ECC48–0x822EE498` (6,224 B), pinned under `VocalTrackDir.cpp:`, belongs to
`GemTrackDir.cpp:`, because its first function `fn_822ECC48` (2,548 B) is
`??0GemTrackDir@@QAA@XZ`.** Confirmed.

### Leg 1 — sizeof + the most-derived flag at the factory

Read from `build/45410914/asm/BandCharacter.s`, keyed on the **`.fn` symbol** (the `.s` address
column is synthetic — `fn_8227BCC0`'s body prints as `8227B540`):

```
fn_8227BCC0:  addi r3,r31,0x50 ; bl fn_8227AC48   (= ?StaticClassName@GemTrackDir@@SA?AVSymbol@@XZ)
              li r4,0x0 ; li r3,0x7f0 ; bl fn_827BCD38   (= ?MemAlloc@@YAPAXHH@Z)
              stw r3,0x54(r31) ; cmplwi r3,0 ; beq … ; li r4,0x1 ; bl fn_822ECC48
fn_8227BEA0:  …same shape… bl fn_8227ADC8 (= ?StaticClassName@VocalTrackDir@@…)
              li r4,0x0 ; li r3,0x770 ; bl fn_827BCD38 ; … li r4,0x1 ; bl fn_822FC508
```

`scripts/harvest/class_layout_report.py` (the compiler, via
`cl.exe /d1reportSingleClassLayout`), re-derived in this worktree:

| class | sizeof | hex |
|---|---:|---|
| GemTrackDir | 2032 | **0x7f0** |
| VocalTrackDir | 1904 | **0x770** |
| TrackDir | 1092 | 0x444 |
| BandTrack | 320 | 0x140 |

The allocation sizes identify the two constructors uniquely, and `li r4,1` is the hidden
most-derived flag both classes need (both have virtual bases).

### Leg 2 — retail's own EH funclets name a base VocalTrackDir does not have ★

This is the strongest leg, and it is the one the brief did not have: it is the **binary stating
the answer** rather than implying it, the same evidence class as W16-BP §2.4's `.xdata` argument.

Decoding the `bl` targets of the block's funclets straight from `orig/45410914/band.exe`:

| funclet | retail callee |
|---|---|
| `fn_822ED63C` (68 B) | `??1Object@Hmx@@UAA@XZ` |
| `fn_822ED680` (68 B) | `??1RndHighlightable@@UAA@XZ` |
| **`fn_822ED6C4` (44 B)** | **`??1TrackDir@@UAA@XZ`** |
| `fn_822ED71C` (40 B) | `??1ObjRef@@QAA@XZ` |
| `fn_822EDF94` (44 B) | `??1?$ObjRefConcrete@VTask@@VObjectDir@@@@UAA@XZ` |
| `fn_822EE45C` (40 B) | `??1?$pair@V?$ObjPtr@VEventTrigger@@@@V1@@stlpmtx_std@@QAA@XZ` |

And the compiler's own hierarchies:

```
GemTrackDir    = TrackDir > PanelDir > RndDir > ObjectDir      (+ BandTrack)
VocalTrackDir  =            RndDir > ObjectDir                 (+ BandTrack)
```

**`VocalTrackDir` has no `TrackDir` base anywhere in its hierarchy**, so a funclet unwinding a
`TrackDir` subobject cannot belong to `??0VocalTrackDir`. The leg is decisive on its own.

### Leg 3 — `.text` geometry (corroborating, and imperfect — say so)

The block is an island inside GemTrackDir's run `0x822E33E8–0x822EF430`, but its immediate
`.text` neighbours are **not** both GemTrackDir:

```
822EC56C-822EC868  GemTrackDir.cpp
822EC868-822EC9B8  SongLayout.cpp
822EC9B8-822ECBC4  Cache_Xbox.cpp
822ECBC8-822ECC48  SongLayout.cpp
822ECC48-822EE498  <<< the block
822EE498-822EF430  GemTrackDir.cpp
```

So this is *not* a "same heading before and after" hole. CLAUDE.md prices geometry alone at
**66.24 % precision**, and W16-BP caught two live false positives with it. Geometry is
corroboration here, never the argument.

### Leg 4 — `.pdata` geometry (emergent, not sought)

After the `.text` move, dtk **re-derived** the block's `.pdata` line
`0x821F91E8–0x821F9498` (696 B = 87 entries, matching the 87 report rows exactly) and placed it
**immediately adjacent** to GemTrackDir's own `0x821F9498–0x821F9650`. In `.pdata` space the
block abuts GemTrackDir. This was not evidence I went looking for; the splitter produced it.

## 3. Per-function census of the block, with dispositions

`config/45410914/symbols.txt` over `0x822ECC48–0x822EE498`: **109 entries** = 88 `type:function`
+ 21 `type:label`; `report.json` carries **87 rows / 6,212 B**. Block size 6,224 B (4 B align pad).

| class | rows | bytes | disposition | evidence |
|---|---:|---:|---|---|
| `fn_822ECC48` — the ctor body | 1 | 2,548 | **→ GemTrackDir** | legs 1+2 |
| anonymous EH funclets (40/44/68/8 B) | 85 | 3,656 | **→ GemTrackDir** | leg 2; they unwind GemTrackDir's bases |
| `?SetUsed@BandTrack@@UAAX_N@Z` @ `0x822EE488` | 1 | 8 | **→ GemTrackDir** | see below |
| **total** | **87** | **6,212** | | |

**No boundary split was needed** — nothing in the block adjudicates to a third owner.

### 3.1 The block held ZERO matched bytes — which is what bounded the risk

Measured before the edit: of the 87 rows, **0 at `fuzzy==100`**; 55 rows / 2,364 B partial;
32 rows / 3,848 B at 0. Per W16-BP §2.3 that makes the **in-block downside exactly 0 by
construction**, and the move a free bet.

⚠ It does **not** bound the *out-of-block* downside. Both units' anonymous funclets pair by byte
signature against a pool, and moving 87 target rows changes pool composition on both sides.
W16-BP measured this running neutral (commit C, 13 in / 13 out) and positive (commit D, +180).
Here it ran positive.

### 3.2 The DEF leg, and why it is near-vacuous for this block

**86 of the 87 rows are anonymous.** Anonymous rows pair by byte signature, never by name, so
the DEF leg is **inapplicable by construction** (W16-BP §2.3).

For the one named row, the DEF test was run over **all 1,216 compiled objs** rather than against
a neighbour (W16-BP §2.2):

| symbol | definers | class |
|---|---:|---|
| `?SetUsed@BandTrack@@UAAX_N@Z` | 3 — `BandTrack.obj`, **`GemTrackDir.obj`**, **`VocalTrackDir.obj`** | NARROW, and **non-discriminating** |

Both source and destination define it — it is a shared `BandTrack` base COMDAT, so DEF cannot
choose between them. The measured outcome matches exactly: it **crossed in** under GemTrackDir
and **fell out** of VocalTrackDir, **net 0**.

## 4. Commit A — the re-home. Predicted vs measured

**Edit:** one `.text` line moved from the `VocalTrackDir.cpp:` heading to `GemTrackDir.cpp:`
(cited by heading name; line numbers in any prior note are dead). `.pdata` lines are derived and
were **not** touched — dtk re-derived one, as recorded in leg 4. `VocalTrackDir.cpp:` retains 41
other `.text` blocks, so no entry deletion was required.

Re-split iterated to a `symbols.txt` + `splits.txt` fixed point. The first build **failed by
design**: the split-guard detected that dtk had rewritten its own input (the `.pdata`
re-derivation). That is the documented recovery path — inspect what it wrote, rebuild once,
which reached a fixed point at iteration 3.

| | prediction | measured |
|---|---|---|
| Δ`matched_functions` | +35 … +55 | **+51** ✅ in band |
| Δ`matched_code` | band [−500, +2,400], **central +1,500** | **+620** ⚠ in band, **2.4× below centre** |
| Δ`total_code` | exactly 0 | **0** ✅ |

**The failed half of the prediction, and why.** I priced every 99.45 %/99.9 % funclet as a
crossing — ~55 partial rows / 2,364 B. Only **14 rows / 628 B** actually reached `fuzzy==100`.
The other ~41 reached `mpn==100` without reaching `fuzzy==100`: they re-paired correctly but
retain a relocation-**name** charge, which `mpn` excludes and `matched_code` does not forgive.
That is precisely CLAUDE.md's "Δfunctions and Δbytes are separate measures" running in the open,
and it is why **+51 functions bought only 14 rows' worth of bytes**.

**Crossed in (14 rows, 628 B)** — all into `default/GemTrackDir`:
`fn_822ED63C` (68), `fn_822ED680` (68), `fn_822ED6C4` (44), `fn_822ED6F0` (44), `fn_822ED798`
(44), `fn_822ED8F0` (44), `fn_822EDE14` (44), `fn_822EDE68` (44), `fn_822EDF3C` (44),
`fn_822EE210` (44), `fn_822EE23C` (44), `fn_822EE268` (44), `fn_822EE294` (44),
`?SetUsed@BandTrack@@UAAX_N@Z` (8).

**Fell out (1 row, 8 B):** `default/VocalTrackDir::?SetUsed@BandTrack@@UAAX_N@Z` — the same
symbol, which is the whole story of §3.2.

### 4.1 What the re-pairing actually fixed, on one row

`fn_822ED6C4` before the move, in VocalTrackDir's pool:

```
idx 5  diff_arg  TGT addi r3, r11, 0x410      BASE addi r3, r11, 0x30
idx 6  diff_arg  TGT bl ??1TrackDir@@UAA@XZ   BASE bl ??1?$map@VString@@VNetAddress@@…@@QAA@XZ
```

It was byte-signature-paired against a `map<String,NetAddress>` destructor — a spurious pairing
that only survives because the two funclets have the same 11-instruction shape. After the move it
pairs against GemTrackDir's own TrackDir-unwinding funclet and reads 100.

## 5. Commits B and C — the naming, and the defect it exposed ★

This is the lane's most valuable result and it was not in the brief's plan.

### 5.1 Commit B — the naming (Δ0, as pre-registered)

Three previously-absent map keys:

| address | name | definers (over all 1,216 objs) | heading it is pinned under |
|---|---|---|---|
| `0x822ecc48` | `??0GemTrackDir@@QAA@XZ` | **1** — `GemTrackDir.obj` | `GemTrackDir.cpp` ✅ |
| `0x8227bcc0` | `?NewObject@GemTrackDir@@SAPAVObject@Hmx@@XZ` | **1** — `BandCharacter.obj` | `BandCharacter.cpp` ✅ |
| `0x8227bea0` | `?NewObject@VocalTrackDir@@SAPAVObject@Hmx@@XZ` | **1** — `BandCharacter.obj` | `BandCharacter.cpp` ✅ |

All three **SPECIFIC** (exactly one definer, so not the vacuous-GENERIC class), and in every case
that definer is the obj of the heading the address already sits under. Both legs hold.

**Caller population checked on retail bytes before predicting the sign**, because naming converts
a *forgiven* placeholder call site into a *checked* one:

| address | `bl` callers |
|---|---|
| `0x822ECC48` | **1** — `fn_8227BCC0`, the factory named in the same commit |
| `0x8227BCC0` | **0** |
| `0x8227BEA0` | **0** |

The factories have zero `bl` callers because they are taken **by address** via
`RegisterFactory` — a data relocation, not a call — so naming them carries **zero call-site risk
by construction**. Exactly one site became checked, and our source satisfies it.

Predicted Δ0 on both headline keys; **measured Δ0 exactly** (43,601 / 4,042,600 unchanged;
crossed-in and fell-out sets byte-identical). The payoff is exposure:

| row | fuzzy before | fuzzy after |
|---|---:|---:|
| `?NewObject@GemTrackDir@@…` (112 B) | 0 | **86.92857** |
| `?NewObject@VocalTrackDir@@…` (112 B) | 0 | **86.92857** |
| `??0GemTrackDir@@QAA@XZ` (2,548 B) | 0 | **79.273155** |

### 5.2 Commit C — what the exposure showed: a per-class macro misassignment

`src/system/obj/ObjMacros.h` (lane NEWOBJ-1) records that retail is **heterogeneous** between two
allocation shapes, that the assignment is per-class, and — crucially — that the remaining ~32
`bandobj` `NEW_OVERLOAD` classes are *"METRIC-INVISIBLE today (no paired NewObject row)"*.
**Naming made two of them visible, and retail then answered on bytes.**

Charged sites on `?NewObject@GemTrackDir@@…` (6 of 28 instructions):

```
TGT  addi r3,r31,0x50 ; bl ?StaticClassName@GemTrackDir@@SA?AVSymbol@@XZ
     li r4,0x0 ; li r3,0x7f0 ; bl ?MemAlloc@@YAPAXHH@Z ; stw r3,0x54(r31)
BASE                          li r3,0x7f0 ; bl ??2GemTrackDir@@SAPAXI@Z
                                                        stw r3,0x50(r31)
```

That is `ObjMacros.h`'s documented **shape (b)** reproduced verbatim, *including* the
`0x54`-vs-`0x50` store offset it predicts as a **consequence** of the discarded `Symbol` temp
occupying `0x50`. Retail's body is 112 B = **28 instructions**, the same "28/28 instructions, all
equal" figure NEWOBJ-1 measured for `LayerDir`, `UnisonIcon`, `OverdriveMeter` and
`BandRetargetVignette`. Both headers were on `NEW_OVERLOAD` (shape (a)).

**The delete side was adjudicated separately, with a control**, because W4-G's census says the
`_INLINE_DEL` choice is not automatic (the 631 bodies reaching the `??3BinStream` ICF survivor
must keep *plain* `OBJ_MEM_OVERLOAD` or they trade a forgiven name for a charged `MemFree`):

| retail symbol | bytes | `bl` targets |
|---|---:|---|
| `??_GGemTrackDir@@UAAPAXI@Z` @ `0x822EF3E0` | 80 | `??_DGemTrackDir@@QAAXXZ` ; **`?MemFree@@YAXPAX@Z`** |
| `??_GVocalTrackDir@@UAAPAXI@Z` @ `0x823000D0` | 80 | `??_DVocalTrackDir@@QAAXXZ` ; **`?MemFree@@YAXPAX@Z`** |
| **control** `??_GLayerDir@@UAAPAXI@Z` @ `0x82329810` (already on `_INLINE_DEL`) | 80 | `??_DLayerDir@@QAAXXZ` ; **`?MemFree@@YAXPAX@Z`** |

All three call `MemFree` **directly** — an inlined delete — and the two candidates are
shape-identical to the class that already carries `_INLINE_DEL`. Our `??_G`'s single charged site
was exactly `TGT bl ?MemFree@@YAXPAX@Z` vs `BASE bl ??3GemTrackDir@@SAXPAX@Z`, the out-of-line
call `DELETE_OVERLOAD`'s `__declspec(noinline)` forces. ⇒ `_INLINE_DEL`, not plain.

**Edit:** in `GemTrackDir.h` and `VocalTrackDir.h`,
`NEW_OVERLOAD; DELETE_OVERLOAD;` → `OBJ_MEM_OVERLOAD_INLINE_DEL(<line>)`.

| | prediction | measured |
|---|---|---|
| Δ`matched_code` | **+384** (2×112 + 2×80), band [−200, +384] | **+304** ⚠ in band |
| Δ`matched_functions` | +4 | **+4** ✅ |
| Δ`total_code` | 0 | **0** ✅ |

All four predicted rows crossed (**+384 B exactly**). The shortfall is the **documented cost
class**, reproduced at 2 funclets where NEWOBJ-1 saw 3: `default/BandCharacter::fn_8227B478` and
`fn_8227B4FC`, 40 B each, fell 100 → 99.5 (**−80 B**). Their sole charged site is

```
TGT  bl ??3BinStream@@SAXPAX@Z      BASE  bl ??3GemTrackDir@@SAXPAX@Z
```

— retail's callee is the ICF survivor, we spell the per-class twin; 9 of 10 instructions equal
and `mpn` stays 100. `ObjMacros.h`'s instruction is verbatim *"Do NOT install an alias or bend
source to recover those bytes."* This lane obeyed it. **This is the honest shape of the result:
a documented, priced, accepted cost — not a regression.**

## 6. ⛔ The ctor "bloat" does not exist — the brief's §3 premise is a measurement artifact

The brief states *"Our `??0VocalTrackDir` COMDAT is **6,312 B** vs retail **3,428 B**"* and
*"Our `??0GemTrackDir` COMDAT is **4,880 B** vs retail **2,548 B**"*, and asks for the cause of
~2× bloat.

**Both numbers are right and the comparison is invalid.** The left side is a **COFF COMDAT
span**; the right side is a **`.pdata` function extent**. Reading our own COMDAT's internal
symbol layout:

| | our COMDAT span | our BODY (offset 8 → first `__unwind$`) | our funclets | retail `.pdata` BODY | **like-for-like** |
|---|---:|---:|---:|---:|---:|
| `??0GemTrackDir` | 4,880 B | **2,296 B** | 2,576 B / 58 chunks | 2,548 B | **−252 B** |
| `??0VocalTrackDir` | 6,312 B | **2,956 B** | 3,348 B / 75 chunks | 3,428 B | **−472 B** |

Our COMDAT holds the body **plus every `__unwind$NNNNNN` EH funclet**. Retail's
`fn_822ECC48` is the **body alone**; its 85 funclets are separate rows with their own `.pdata`
entries — they are the rest of the very block this lane re-homed (3,656 B of it).

⇒ **Our constructor bodies are SMALLER than retail's, not larger.** The direction is the
opposite of the premise, on both classes.

This is the same disease as
`project_one_sided_instrument_error_invisible_to_two_sided_control_2026-08-16.md`, where
`tools/coff_bodies_ext.py` billed a successor's EH funclet prefix into a COMDAT span and produced
a "+8 B STLport source bug" that did not exist. ⚠ **A size test cannot catch this, because the
artifact is one-sided and cancels on neither side; the only cure is to check what each number
actually spans.**

**What the real divergence is**, from `run_diff_inspect mode=diagnose` on
`??0GemTrackDir@@QAA@XZ` (647 target instructions, 417 equal, **73 deletes vs 10 inserts** —
i.e. retail has instructions we lack, not the reverse):

```
idx 122  TGT stw  r11, 0x544(r30)        BASE bl ??0?$ObjPtr@VRndDir@@@@QAA@PAVObject@Hmx@@PAVRndDir@@@Z
idx 133  TGT lis  r10, lbl_8201DCD4@h    BASE bl ??0?$ObjPtr@VRndGroup@@@@QAA@PAVObject@Hmx@@PAVRndGroup@@@Z
idx 151  TGT stw  r11, 0x568(r30)        BASE bl ??0?$ObjPtr@VRndMesh@@@@QAA@PAVObject@Hmx@@PAVRndMesh@@@Z
```

**Retail INLINES the per-member `ObjPtr<T>` constructors; we emit an out-of-line `bl` per
member.** That is `ObjMacros.h`'s "inline-policy" class (the `Str` pattern) with the polarity the
brief guessed backwards. The 29 insert/delete clusters are overwhelmingly 2- and 4-instruction
groups at regular intervals — the per-member signature. Residual noise budget: 71 `diff_arg`, of
which **0 unexplained** (28 offset shifts, 57 register swaps, 6 symbol relocs).

## 7. The MISPIN doc correction — and the instrument lesson

`docs/decomp/MISPIN_SUSPECTS_2026-09-10.md` attributes `front1.grp`, `lead_lyric_scroll.grp`,
`phoneme0.grp`, `phoneme2.grp`, `scroller.trans` to *"the 2,548 B function at `0x822ECC48`"* and
concludes **REFUTED** for this block. Re-scanned independently on retail bytes (Python over
`orig/45410914/band.exe`, decoding `lis rX,hi` / `addi rY,rX,lo` pairs within 64 B and mapping
each pc to its enclosing `symbols.txt` extent — never a shell `grep`, which is binary-blind here):

| string | `.rdata` VA | ref pc | enclosing function | heading |
|---|---|---|---|---|
| `front1.grp` | `0x82029D64` | `0x822F99C8` | `?SyncObjects@VocalTrackDir@@UAAXXZ` @ `0x822F96F0` | `VocalTrackDir.cpp` |
| `phoneme0.grp` | `0x82029D24` | `0x822F9AA4` | same | same |
| `phoneme2.grp` | `0x82029D04` | `0x822F9AFC` | same | same |
| `scroller.trans` | `0x82029CBA` | `0x822F9B54` | same | same |
| `lead_lyric_scroll.grp` | `0x82029C58` | `0x822F9C88` | same | same |

Each string has **exactly one** reference site. **Zero** lie inside `0x822ECC48–0x822EE498`;
`0x822F96F0` is **44,712 B above** the block, in a different `splits.txt` block
(`0x822F8FF0–0x822FA1D0`). The scan independently reproduced the five pcs the brief predicted.

A dated banner was added to that doc (`c98a9511`); **its history was not rewritten**. Its broader
point survives — `0x822F96F0` *is* correctly pinned to VocalTrackDir; what fails is using those
strings as evidence about `0x822ECC48`.

★ **The durable lesson is about the instrument, not this block.** A "traced by string"
attribution that never decoded the referencing instruction is an attribution to a
**neighbourhood**, not to a function — and here the neighbourhood was the right TU while the
function was 44 kB away. It produced a confident **REFUTED** that closed a real vein for five
days. Decode the `lis`/`addi` pair and map the pc to a `.pdata` extent, every time.

## 8. Commit D — the VocalTrackDir ctor hole (brief §3's structural question)

`0x822FC508` was **unpinned**, living in `default/auto_03_822FC4F8_text` — which has no base obj,
so a name there would have paired to nothing. Both legs, and stronger than the re-home's:

- **GEOMETRY — a perfect hole, gap 0 on both sides:**
  `0x822FC430–0x822FC4F8` VocalTrackDir → `0x822FC4F8–0x822FD26C` *(unpinned)* →
  `0x822FD26C–0x822FD2F4` VocalTrackDir.
- **DEF —** `??0VocalTrackDir@@QAA@XZ` is defined by **exactly one** of 1,216 objs,
  `VocalTrackDir.obj`.

Contents: `fn_822FC4F8` (4 B), an 8-byte `except_data_822EAB10` EH prefix at `0x822FC500` (the
documented `.text`+`.rdata` pointer pair — which is why it is not in `total_code`), and
`fn_822FC508` (3,428 B).

This is an **addition** over `auto_*` code, not a re-home, so pin-neutrality prices it at exactly
0. The splits pin and the map name landed in **one commit** because they are coupled: the pin
without the name leaves the row anonymous; the name without the pin pairs to nothing.

Predicted Δ0 on all four keys, with a named **−112 B risk** (naming `0x822FC508` converts
`?NewObject@VocalTrackDir@@`'s call site — which had just reached 100 — into a checked one).
**Measured Δ0 exactly**, and the risk did not materialise: our source really does call the
constructor retail calls.

Payoff: `??0VocalTrackDir@@QAA@XZ` is now a **pairable row reading 70.95799 %** on 3,428 B instead
of an unpairable `auto_*` row reading 0. With `??0GemTrackDir` at 79.273155 %, both TrackDir
constructors are measurable for the first time — which is what §6's analysis required.

## 9. Residue — priced from charged sites, not mismatch counts

The brief's residue figures were **confirmed exactly** in this tree: `?PostLoad@VocalTrackDir`
3,656 B @ 96.3862 · `fn_822EF438` 1,532 B @ 0 · `?ApplyFontStyle@…` 1,164 B @ 6.7251 ·
`fn_822FA7E0` 1,000 B @ 0 · `?SetRange@…` 700 B @ 93.5371 · `?SetConfiguration@…` 528 B @ 82.4015.

### 9.1 A cluster the brief did not flag — and why it does not convert

`default/GemTrackDir` carries six named rows at **99.94–99.97 %**, 2,744 B in total, each one
charge from crossing. That is a far better *size-if-it-crosses* shape than the 0 % rows. It was
probed and **the causes are heterogeneous — it is not one lever**:

| row | bytes | charge | class |
|---|---:|---|---|
| `?OnDrawSampleChord@GemTrackDir@@` | 648 | `vector<Object*>::_M_fill_insert` vs our `vector<int>::` | **our source is RIGHT** — see below |
| `?PlayIntro@GemTrackDir@@` | 488 | `??2CriticalSection@@SAPAXI@Z` vs our `??2Task@@SAPAXI@Z` | ICF fold-alias |
| `?ReleaseSmasherPlate@GemTrackDir@@` | 444 | `??0?$StlNodeAlloc@VSmasherPlateInfo…` vs our `?ReleaseSmasherPlate@GemTrackResourceManager@@` | ICF fold-alias |

**`OnDrawSampleChord` — a "real type bug" that is not one.** The charge looks exactly like the
container-type defect class. It is not. The **rb3-Wii oracle** — the richer source oracle for RB3
game code — spells the member verbatim as ours:

```
static std::vector<int> fretNums;        // ../rb3/src/system/bandobj/GemTrackDir.cpp:1332
...
int curi = fretNums[i];                  // :1294  -> RGFretNumberToString(curi)
```

Fret numbers are `int`s and the oracle uses them as such. `vector<T>::_M_fill_insert` is
byte-identical for any 4-byte POD element, so retail's survivor spelling happens to be the
`Hmx::Object*` instantiation. Changing our type to match would be **metric-fitting a wrong
type** — CLAUDE.md's `TEMPLATE_ARGS_DIFFER`-is-what-a-fold-looks-like trap. **Not fixed,
deliberately.** Checked `scripts/symbol_aliases.json` first: the one group mentioning
`vector<Object*>` + `_M_fill_insert` covers `_M_fill_insert_aux`, a different symbol, and does
not carry the `int` spelling.

### 9.2 `?SetConfiguration@VocalTrackDir@@` (528 B @ 82.40) — NOT missing code

29 charged sites of 137 instructions. Retail emits a trailing block (idx 105–119) we do not:

```
lbz r11,0x1fa(r30) ; cmplwi r11,0 ; beq end
lwz r11,0x348(r30) ; cmpwi cr6,r11,0 ; beq cr6,end
lbz r10,0x170(r11) ; addi r29,r11,0xd4 ; cmplwi r10,0 ; bne skip
mr r3,r29 ; bl ?SetDirty_Force@RndTransformable@@AAAXXZ
skip: lis r11,lbl_82000D78@h ; lfs f0,lbl_82000D78@l(r11) ; stfs f0,0x4c(r29)
```

The obvious read is "we are missing the `unk2a7 && mPlayerIntro` block". **We are not** — our
source already carries it, and it is identical to the oracle's:

```c++
if (unk2a7 && mPlayerIntro)
    mPlayerIntro->SetLocalPos(Find<RndTransformable>("h2h_player_intro_trans.grp", true)->LocalXfm().v);
```

So this is a **codegen/inlining divergence** (retail inlines `SetLocalPos` and the `Find<>`
result differently), plus an operation-ordering difference around `SetObjConcrete` and a
register-pressure difference (`__savegprlr_26` vs our `_24` — we hold two more callee-saves).
That is a substantially harder class than a missing statement, and **it was not attempted**.

## 10. Gates

All run in the worktree at `88dd73ad`, in the briefed order. ⚠ Each exit code was
captured by redirecting to a file and testing `$?` **on the next line** — the first attempt
used `cmd | tail; echo rc=$?`, which reads **`tail`'s** exit code and is vacuous by construction
(the trap is documented in this repo's own notes; it produced a green `rc2=0` that meant nothing,
and every gate below was re-run properly).

| # | gate | rc | substance |
|---|---|---:|---|
| 1 | full `./tools/ninja-locked` | **0** | fixed point; `~/tmp/rb3_build_w16bq_13.log` |
| 2 | `verify_ruler_agreement.py --check` | **0** | both objdiff entry points resolve `name_check`; all 4 keys OK |
| 3 | `verify_objs_patched.py --verify-manifest` | **0** | 1,216 decomp + 3,105 target objs match, `tree_sha256=97766868ca921e8f` |
| 4 | `icf_alias_finder.py --validate` | **0** | 1,652 groups, 1,404 map-consistent, 247 tolerated, **0 CONTRADICTED** |
| 5 | `funclet_homing.py --validate` | **0** | HOMED 25,051 · ORPHAN 1,227 · **MIS-PINNED 1** · UNPINNED-FUNCLET 42 |

**The one `MIS-PINNED` row is NOT this lane's** — checked rather than assumed, because this lane
re-homed 87 funclets and a new mis-pin is exactly the defect it could have introduced:

```
0x823F4A30  unwind parents=['0x823f4858']  parent_units=['CharTaskMgr.cpp']  pinned=UI.cpp
```

`CharTaskMgr`/`UI` at `0x823F…`, ~1.1 MB from this lane's spans (`0x822EC…`–`0x822FD…` and
`0x8227B…`). Pre-existing, and the gate tolerates it. **All 87 re-homed funclets read HOMED.**

**Gate 6 — `tools/native_build_gate.sh`, the last action:**

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0` as required — the 0-SKIP rule, not the exit code, is the one with the track record.

## 11. NOT done, and why

Silence reads as coverage, so this is explicit.

| # | not done | reason | what would change it |
|---|---|---|---|
| 1 | `??3GemTrackDir@@SAXPAX@Z` → the `??3BinStream` alias group | Would recover **exactly the 80 B** §5.2 gave up. `ObjMacros.h` says verbatim *"Do NOT install an alias or bend source to recover those bytes"*, and an **unproven alias lifts `name_check` BY CONSTRUCTION** — the `none` control reads flat by construction and **cannot** catch a fabricated one | Byte-identity **including relocations** against the group's proven members, at T1 |
| 2 | `?OnDrawSampleChord@GemTrackDir@@` (648 B) | Changing `vector<int>`→`vector<Object*>` would metric-fit a **wrong type** against the oracle's explicit `std::vector<int> fretNums` | Retail evidence that the element type is genuinely a pointer — e.g. a 4-byte-element site doing something only a pointer supports |
| 3 | `?PlayIntro@` (488 B), `?ReleaseSmasherPlate@` (444 B) | ICF fold-alias candidates; installing aliases is out of this lane's scope and is hazard class (1) | A T1 fold proof on retail bytes for each pair |
| 4 | `?SetConfiguration@VocalTrackDir@@` (528 B @ 82.4) | 29 charged sites, and **our source already contains the block** the diff appears to be missing ⇒ inlining/ordering/register-pressure divergence, a much harder class than a missing statement | A `__savegprlr_26` reproduction showing which two extra callee-saves retail avoids |
| 5 | `?PostLoad@VocalTrackDir` 3,656 B @ 96.39 · `fn_822EF438` 1,532 B @ 0 · `?ApplyFontStyle` 1,164 B @ 6.73 · `fn_822FA7E0` 1,000 B @ 0 | Out of the lane's scope (the ctor block + its identification). Figures **verified exact**, not inherited | — |
| 6 | Brief task 7 — W16-BO's two FILED re-homes (`0x823e3e90` NetSession/MidiInstrument; `0x825aaff0` LockStepMgr under `SetlistMergePanel.cpp`, **discontiguous**) | Explicitly OPTIONAL, "only if time remains". Budget went to §6, which refuted the brief's central premise and was worth more than a second re-home | — |
| 7 | The ~30 other `bandobj` `NEW_OVERLOAD` classes | Still metric-invisible (no paired `NewObject` row). §5 converted **two** by naming them; the rest need the same identification work first | A pin + map name for each factory, then retail's `li r3,<sizeof>` |
| 8 | `??0GemTrackDir`'s 73-delete `ObjPtr<T>` inlining divergence (§6) | Diagnosed, not fixed. It is an **inline-policy** class across ~29 member constructions; no single source edit closes it, and the row is 2,548 B all-or-nothing | A reproduction showing which `ObjPtr<T>` spelling/header shape makes MSVC `/O1 /Ob2` inline the member ctor |

## 12. What this lane would tell the next one

1. **The brief's headline premise was wrong and the measurement that refuted it took one hour.**
   Test a briefed figure literally before building on it — §6's "2× ctor bloat" was a COMDAT span
   vs a `.pdata` extent, and the true delta is **negative on both classes**.
2. **A "traced by string" attribution that never decoded the referencing instruction is an
   attribution to a neighbourhood.** §7's five strings were 44 kB from the block they were used to
   judge, and the resulting REFUTED closed a real vein for five days.
3. **Separate the two kinds of gain.** +55 functions looks like source progress; the honest floor
   says **4** of it is. Report both or the number misleads.
4. **The strongest identification evidence is retail's own EH funclets naming a base class.**
   Geometry runs at 66.24 % precision; a funclet calling `??1TrackDir@@UAA@XZ` in a class with no
   `TrackDir` base is a contradiction, not a correlation.
