# The circular-pin census: one instrument built, its own premise refuted, two pins repaired

**Lane W15-A, 2026-09-14.** Branch `w15-a` off `main` `8995d566`.
Worktree `~/tmp/wt-w15-a`. Tool: `tools/circular_pin_census.py`.

W14-C (`docs/decomp/SWAPPED_NAMES_2026-09-14.md`) caught a **circular pin** in the
wild at `0x8251CBE0`: a plausible map NAME justified a splits PIN, and the pin
then corroborated the name. Neither channel can detect that — each is the other's
evidence. This lane builds the third channel (the retail **call graph**, decoded
from `band.exe` bytes), runs it over every pinned function, and adjudicates.

> **VERDICT SUMMARY**
>
> | | |
> |---|---|
> | instrument | built, controlled, `--selftest` shown to fail under 3 sabotages |
> | ⛔ **its founding premise** | **REFUTED BY ITS OWN OUTPUT** — the anon-namespace hash is *not* a per-TU fingerprint on this map (§4). The conclusions survive on a different, structural channel; the premise does not. |
> | the signal the brief anticipated | **"callee-set agreement" is NOISE**: 1.38× / 1.51× enrichment over known-good units. Not funded. |
> | the signal that works | **internal linkage**: an anonymous-namespace function's *caller* is in its TU. Structural, hash-value-independent. |
> | repairs landed | **2**, both pre-registered **Δ0** and both measured **exactly Δ0** |
> | suspects opened | **7**; 3 proven, 1 refuted, 3 undecidable/unfixable (§8) |
> | ★ largest finding | **`MoggClipMap.cpp`'s 3,128 B block 1 is `PlatformMgr_Xbox.cpp` TU content** — 7 of its 8 named rows are `PlatformMgr` methods. Proven, and **deliberately NOT repaired**: no correct destination exists that we compile (§8.2). |

---

## 1. Provenance

Full `./tools/ninja-locked` (never a single `.obj`; that skips the six
post-compile patchers, which are part of the ruler) before any name-keyed
analysis — a fresh worktree's reflinked target objs are **pre-renamer**, so every
mangled-name lookup would read "absent" and every negative would be vacuous.

```
EXIT=0
[patch-state] OK: 1205 decomp, 3083 target objects match 2026-09-14T03:03:36Z (tree_sha256=ec26b2b6a49a5795)
```
`scripts/verify_objs_patched.py --verify-manifest` → **rc=0**.

Ruler `name_check`, read from `report.json`'s `provenance.diff_config`, not
assumed. Baseline at `8995d566`: **42,846 matched / 3,892,688 B / 37.992430% /
fuzzy 49.189335 / 165 units at 100% (mpn)**.

## 2. What the instrument is, and what it deliberately is not

**Inputs.** `config/45410914/splits.txt` (unit → `.text` ranges, keyed on the
**FULL PATH** heading, never `basename()` — `Movie.obj` genuinely collides
between `rnddx9/` and `rndobj/`, and this is the slip that broke four consecutive
lanes' scans); `scripts/target_symbol_map.json`; the split `.s` files for extents
and for reading back the attribution under test; **`orig/45410914/band.exe`** for
the call graph; DC3's `ham_xbox_r.map` for an independent TU opinion.

**What is circular and is never used as evidence.** The dtk-split target `.obj`
and `.s` files are *generated from* `splits.txt`. Using them to judge
`splits.txt` is circular by construction. They supply only (a) function extents
(dtk's carve, from retail `.pdata` and control flow — not from the pin) and
(b) the attribution being audited. **Every piece of evidence comes from retail
bytes, the map, our own source, or DC3.**

**The `.s` address column is never read.** dtk synthesizes it as
`first_block_start + cumulative section offset` for multi-block units, so it
names addresses the function does not live at. Extents come from the
`.fn fn_<ADDR>` label plus a 4-bytes-per-instruction count.

**The decoder is hand-written and cannot halt.** capstone's PPC decoder stops at
the first undecodable word, which is how an earlier lane got a vacuous
"0 bl callees" inside a 4,260 B function. This one reads every 4-byte word in the
extent and classifies only `bl` (primary opcode 18, `LK=1`, `AA=0`).

## 3. ⛔ The rarity filter is not optional

L2-BODYTRIAGE (`docs/decomp/MISPIN_SUSPECTS_2026-09-10.md`) measured that the
obvious "what does this block reference" channel is dominated by artifacts.
Reproduced here independently:

| suppressed callee | fan-in |
|---|---:|
| `??1DataNode@@QAA@XZ` | 4,316 |
| `??0Symbol@@QAA@PBD@Z` | 3,556 |
| `??3BinStream@@SAXPAX@Z` | 2,110 |
| `?Release@DataArray@@QAAXXZ` | 1,882 |
| `__RTDynamicCast` | 1,408 |

**638 distinct callees / 66,850 cross-unit edges are suppressed at fan-in ≥ 20**,
and every one is printed with its frequency so the suppression is auditable
rather than silent.

A second, map-declared filter is just as load-bearing: the map **declares its own
unassignable names** — `_icf_arbitrary` (32), `_bijection_arbitrary` (1,015),
`_denylist` (7). An `_icf_arbitrary` name is one arbitrary pick among
linker-folded twins: the bytes are true, *which name sits on which VA is not
established*. ⚠ **One of this lane's first apparent proofs was exactly such a
row** — `0x82654260 ?Register@KickPlayerMsg@?A0x9d4e879f@@` — and it would have
been briefed as a finding had the list not been consulted.

## 4. ⛔⛔ The instrument's founding premise is REFUTED — by its own output

The tool was built on: *MSVC derives `?A0x<hash>` per translation unit, so a hash
is a TU fingerprint; every symbol carrying hash H belongs to exactly one retail
TU.* That would be an attribution channel wholly independent of splits and of the
map. **It does not hold on this map.**

| measure | value |
|---|---:|
| units carrying file-static anon symbols | 39 |
| **units carrying ≥ 2 DISTINCT anon hashes** | **10** |
| worst: `BandMachineMgr.cpp` | **6 hashes** |

`BandMachineMgr.cpp` carries six hashes for methods of the *same two classes*
(`SyncMachineMsg`, `SyncLocalMachineMsg`) at **adjacent** addresses
`0x825C1F28..0x825C36D8` — which one TU's emission cannot produce.

**The cause is structural and unfixable: retail `band.exe` has no symbol table**,
so *no* `?A0x` hash in `target_symbol_map.json` was ever read from retail. Each
was inherited from whichever **oracle** supplied that name (DC3 bindiff, the
rb3-Wii oracle, `fingerprint_match`). Hash equality is a statement about the
**oracle's** TU layout, not retail's, and **cannot be validated against retail
even in principle**.

⇒ hash grouping is demoted to **corroboration**, kept only because it cheaply
*surfaces* candidates.

★ **What survives is a property of the name's STRUCTURE, not the hash's VALUE:**
a function in an anonymous namespace has **internal linkage**, so the linker
cannot resolve a reference to it from another TU — therefore **its caller is in
its TU**. Every verdict below rests on that, and the landed repair cites three
further channels independent of the hash.

## 5. Controls

### 5.1 Cross-lane control on the binary reader — free, and it passed
The PE section table reproduces W13-B's and W14-C's independently written probes
**exactly**: `.rdata 0x82000400`, `.pdata 0x821f1600`, `.text 0x82270000`.

### 5.2 The attribution control — and the version of it that COULD NOT PASS
splits-vs-asm attribution disagreement: **0 over 69,031 functions.**

⚠ The first version of this control read **59,168 disagreements (86%)**, because
it compared a `.cpp`-bearing splits heading against an extension-less asm path.
**A control that cannot pass is as useless as one that cannot fail**, and a
86%-red control shaped exactly like a catastrophic finding. Fixed by comparing
extension-stripped full paths.

### 5.3 My decoder vs the `.s` files' own `bl` lines
| | edges |
|---|---:|
| retail-decoded `fn_→fn_` | 142,227 |
| `.s`-file `fn_→fn_` | 126,650 |
| mine-only | 15,805 |
| asm-only | 228 |

**Both residuals are fully accounted:** the 15,805 mine-only are `__savegprlr_*`
CRT helpers the `.s` spells symbolically (**15,111 of 15,112 callers match the
`.s`'s named-edge count exactly, 99.99%**; 105 distinct callees, all in the
`0x8282925C` block). Of the 228 asm-only, **227 have callers outside `.text`**
(BINK). **Exactly one** residual remains — `0x8282F030 → 0x8282F218` — explained:
XDK `crt0dat` interleaves with `auto_03_8282F194_text`, so global tiling
truncates one vendor function. Not a decoder defect.

### 5.4 (a) Units already at 100% — the known-good population
165 units are at 100% (mpn). They hold **2,357 of 54,756** real-unit functions.

| signal | known-good | untreated | enrichment |
|---|---:|---:|---:|
| **ANON_NS** | **0** (0.000%) | 32 (0.061%) | — |
| SOLE_FOREIGN_CALLER | 30 (1.273%) | 920 (1.756%) | **1.38×** |
| FANIN_1_FOREIGN | 91 (3.861%) | 3,049 (5.819%) | **1.51×** |

⇒ **The signal the brief anticipated — "does the callee set agree with the unit"
as a plurality vote — is NOISE.** 1.38× and 1.51× are close to the **1.95×** that
CLAUDE.md records having been wrongly used as a deterministic classifier. Neither
is funded, and neither produced a repair here.

⚠ **And the ANON_NS zero is reported HONESTLY as UNDER-POWERED, not as proof of
precision.** With 32 hits over 52,399 untreated functions, a uniform-rate null
predicts **~1.4** hits in the 2,357 known-good functions; observing 0 has
p ≈ 0.24. It is *consistent* with a clean signal and is **not** evidence of one.
The real negative control is §5.5.

### 5.5 The negative control that does discriminate
**58 of 60** file-static anon hashes sit **entirely within one unit**. Only 2
split across units. A detector that fired indiscriminately could not produce
that ratio.

### 5.6 Cross-validation against the PINFIX/MISPIN machinery
`tools/maprow_audit/pinfix_census.py` is an **independent implementation** of the
same joins. Run at `8995d566` it reports figures that match mine to the unit:

| quantity | pinfix_census | this tool |
|---|---:|---:|
| named map addresses | 29,038 | 29,038 |
| splits units | 1,273 | 1,273 |
| compiled objs | 1,205 | 1,205 |
| distinct defined symbols | 834,268 | 834,268 |

⚠ **The recorded reference figures have DRIFTED and are validated on SHAPE, not
equality** — exactly as the standing rule requires (70 commits have touched
`splits.txt` since they were measured):

| class | recorded (2026-08-13) | measured now |
|---|---:|---:|
| HEALTHY | 22,384 | **22,544** |
| NO_OBJ | 4,515 | **4,516** |
| MISPIN_SINGLE | 93 | **30** |
| mis-pins total | 149 | **93** |

The mis-pin class has **drained by 38%** since it was recorded — later lanes
repaired them. Quoting 149 as current would have been wrong by 60%.

### 5.7 `--selftest`, shown to fail
Positive **and** negative leg (a one-leg test cannot discriminate: a detector
that fires on everything passes a positive-only test). Demonstrated red under
three separate sabotages:

| sabotage | result |
|---|---|
| `is_anon_ns` → `False` | population guard **FAIL** |
| ANON_NS emit path removed, population intact | **positive leg FAIL, negative leg still PASS** |
| `bl` opcode 18 → 19 | vacuity guard **FAIL** (13,087 edges) |

## 6. (b) The recorded priors — REPRODUCED AS *ALREADY REPAIRED*

CLAUDE.md records, flagged-but-unverified from lane CF-4 (2026-08-01):
*"nine named engine units each claim exactly one function inside the Quazal
block, and `CameraManager.s` claims 13 at `0x82B02748`–`0x82B031A0`."*

**Neither reproduces, and that is the correct result.** Measured over the
`/Od` band `0x82A6D168`–`0x82B54190`:

| unit | fns in band | range |
|---|---:|---|
| `DuplicatedObject.cpp` | 95 | `0x82A70400..0x82AFC2A8` |
| `Scheduler.cpp` | 26 | `0x82AC57F0..0x82AC78B0` |
| `trie.cpp` | 6 | `0x82AB0F70..0x82AB12C8` |
| `ssluse.c` / `StringConversion.cpp` / `ChecksumAlgorithm.cpp` | 1 each | — |

**`CameraManager.cpp` claims nothing there at all.** `0x82B02748..0x82B031A0` is
now owned by `auto_03_82B0046C_text`, and CameraManager's pins are all in
engine territory (`0x824B9ED8`–`0x825AD810`). `git log -S` names the repair:
**`4b3c098d` "fix(splits): unpin 8 bad .text pins inside the Quazal /Od block"**,
which states *"CameraManager's 13 (+1) at 0x82B02748-0x82B031A0 are confirmed"*
and unpinned them. The "nine units claiming one function each" is down to **3**,
and CF-9 explicitly exonerated one of them (`StringConversion.cpp` is a real
Quazal unit whose `/Od` code is correctly its own).

★ **Worth recording: CF-9 reached its verdict partly on CALL LOCALITY** — *"0 of
363 outside-Quazal functions in those units call into Quazal, while every suspect
calls exclusively into it."* That is the same family of instrument as this lane's,
arrived at independently, which is convergent validity for the approach.

⇒ This is the standing rule demonstrating itself: **a prior is a dated
measurement, not a standing fact.** Re-measure; never inherit.

## 7. Findings census

Over 69,031 functions and 197,239 decoded `bl` edges:

| signal | findings | funcs | verdict |
|---|---:|---:|---|
| `ANON_NS` (internal linkage crossed) | 37 | 32 | **the only trustworthy class** |
| `SOLE_FOREIGN_CALLER` | 1,514 | — | noise (1.38×) — not funded |
| `FANIN_1_FOREIGN` | 6,881 | — | noise (1.51×) — not funded |

Of the 37 `ANON_NS` findings, **the majority are the ICF-foldable template class
and are NOT evidence**: names like
`??$__destroy_range_aux@V?$reverse_iterator@PAULabel@?A0x81ddebd1@@@stlpmtx_std@@`
wear an anon hash belonging to their **type argument**. Such STL COMDATs are
byte-identical for every POD `T`, the linker folds them, and the surviving
spelling is arbitrary — the `_icf_arbitrary` mistake. `anon_scope_hash()`
discriminates on the qualified-name prefix and drops them.

## 8. Adjudications — 7 suspects opened

### 8.1 ✅ `0x82529890` — PROVEN, REPAIRED, Δ0 (§9.1)
`?InitXinputJoypadThreadData@?A0x439b694a@@YAXXZ` pinned `JoypadClient.cpp`,
belongs to `Joypad_Xbox.cpp`. Four channels, three of them independent of the
refuted hash premise:

1. `0x8252A3B0` (`XinputJoypadThreadEntry`, already in `Joypad_Xbox.cpp`) **calls
   it** — internal linkage ⇒ same TU.
2. our own `src/system/os/Joypad_Xbox.cpp:289-299` declares **both** inside one
   `namespace { }`.
3. the retail call graph reproduces our source body exactly: `0x8252A3B0` calls
   `0x82529890` then `0x8252A0B8`, i.e.
   `{ InitXinputJoypadThreadData(); RunXinputJoypadLoop(); return 0; }`.
4. fan-in 1, sole caller inside the destination unit; geometry — the last 0x50 B
   of JoypadClient's 3rd block, abutting Joypad_Xbox's start at `0x825298E0`.

### 8.2 ⛔ `MoggClipMap.cpp` block 1 — PROVEN FOREIGN, **NOT REPAIRABLE BY A PIN MOVE**
**This is the largest finding of the lane and the closest analogue to W14-C's
case.** `0x8251BFA8-0x8251CBE0`, **3,128 B / 38 functions**, pinned to
`MoggClipMap.cpp`. Its named content:

| addr | symbol |
|---|---|
| `0x8251C048` | `?IsInParty@PlatformMgr@@QAA_NXZ` |
| `0x8251C0D0` | `?IsInPartyWithOthers@PlatformMgr@@QAA_NXZ` |
| `0x8251C170` | `??1?$ObjPtr@VMoggClip@@@@UAA@XZ` ← **4 bytes** |
| `0x8251C180` | `?SetScreenSaver@PlatformMgr@@QAAX_N@Z` |
| `0x8251C248` | `?GetPadNumFromXuid@?A0x8a9ffbf2@@YAH_K@Z` |
| `0x8251C840` | `?SetPadProperty@PlatformMgr@@QBAXHHPBG@Z` |
| `0x8251C960` | `?ShowGamercard@PlatformMgr@@QAA?AW4ShowGamercardResult@@...` |
| `0x8251CA58` | `?GetName@PlatformMgr@@QBAPBDH@Z` |

**Seven of eight named rows are `PlatformMgr` methods or a PlatformMgr-Xbox
anonymous static.** The *entire* MoggClipMap justification is one **4-byte**
`ObjPtr<MoggClip>` destructor — and a 4-byte body is
relocation-masked byte-identical to every other 4-byte thunk in the binary
(CLAUDE.md's `_single_branch_thunk_misnames` class), so its name is noise. It
does not even score: **fuzzy 95.0**.

Internal-linkage proof: `0x8251C960` (`ShowGamercard@PlatformMgr`, pinned
MoggClipMap) **calls** `0x8251BBF0` (`XPrivilegeCheck@?A0x8a9ffbf2`, pinned
`MemcardMgr_Xbox.cpp`) ⇒ those two pins cannot both be right.

**The true owner is `src/system/os/PlatformMgr_Xbox.cpp`** — it exists in our
tree (728 lines) and carries `PlatformMgr::SetScreenSaver`, but it is **NOT in
`objects.json`**, has no compile edge, and is not pinned as a unit.

⛔ **Why no repair was made — and why the obvious one would be the very disease
this lane audits.** Moving the block to `PlatformMgr.cpp` would install a second
plausible-but-wrong attribution: `PlatformMgr.cpp` is the *generic* TU and our
compiled `PlatformMgr.obj` **defines none of these six names** (verified by COFF
`SectionNumber > 0` over all 1,205 objs, with two positive controls). So:

* it is **not re-homable** in PINHOME-1's sense — no TU we compile emits these
  names, so no pin move can make them pair. All six read **fuzzy 0** now and
  would read 0 afterwards.
* ⚠ **CORRECTION TO MY OWN FIRST READING, caught by checking instead of
  asserting.** I initially wrote that the block contributes *none* of
  MoggClipMap's 936 matched bytes. **False.** Two ANONYMOUS rows inside it —
  `fn_8251C4FC` (40 B) and `fn_8251CB00` (32 B) — are at **fuzzy 100**. So the
  block carries **72 matched bytes**, and any unpin or move of it costs
  **−72 B**. The six *named* `PlatformMgr` rows are all at fuzzy 0 as stated;
  it was the 29 anonymous rows I had not priced. That 72 B is the concrete
  price tag on the escalation below.
* an **unpin** (CF-9's conservative precedent) is the only honest alternative,
  and it perturbs `total_code` — CF-9 measured **+52,184 B** of denominator
  growth from a comparable 8-block unpin as `auto_*` units merged across the
  holes. That is a multi-kilobyte denominator swing bought for an accuracy-only
  gain, and it deserves its own lane and its own pre-registration.

**⇒ ESCALATION, not a closed vein.** The correct fix is to wire
`PlatformMgr_Xbox.cpp` into `objects.json` and pin this block (plus, probably,
`MemcardMgr_Xbox.cpp`'s `0x8251BB00-0x8251BFA8`, whose 6 named rows include 3
more `PlatformMgr` methods) to it. That is a **porting** lane with a compile
risk and a native gate, not a pin edit.

### 8.3 ✅ `0x827DCAA4-0x827DCD80` — PROVEN, REPAIRED, Δ0 (§9.2)
A 720 B anonymous function opening `TrackDir.cpp`'s `0x827DCAA4` block calls
`0x827DC010 ?ValidateHeader@?A0xaf4cfd2b@@` — an anonymous-namespace free
function inside `HttpGet.cpp`'s main block (16 named HttpGet symbols around it).
Internal linkage ⇒ the caller is HttpGet's TU. Its other callees corroborate:
`MemAlloc`, `String()`, `~String()`, `vector<String>::resize`, `Timer::Restart`
and a second anonymous HttpGet function — HTTP header parsing, not track-dir
code. The block was anchored to TrackDir by **one** row 0x328 B later,
`?StaticClassName@TrackDir@@SA?AVSymbol@@XZ` @ `0x827DCDD8`: the block is MIXED.

★ **Independent corroboration, produced after the hypothesis:** dtk re-derived
`.pdata` `0x82243950-0x82243958` from TrackDir to HttpGet unprompted — the
splitter, using unwind-record ownership and knowing nothing about anonymous
namespaces, moved in the same direction.

**Conservative**: only the proven function (+ its 12 B EH prefix) moved. The two
ambiguous 0x28 B anonymous functions at `0x827DCD80`/`0x827DCDA8` were **left
with TrackDir** — no channel adjudicates them.

### 8.4 ❌ `UI.cpp` `0x82B801D8-0x82B80FC4` — REFUTED BY ITS OWN CONTENT
`fn_82B80810` (1,780 B) calls `0x82B6EA08 ?Time2IirA@?A0xa7b3dd7d@@` — a
`Synapse_dsp.cpp` file-static — which fires the internal-linkage rule. **I opened
it and it does not survive.** The block's 24 functions reference `TourChar`,
`Object`, `PropKeys`, `DataNode`, `DataVariable`, `BandUserMgr`, `TourSavable`,
and its two named rows are `??_GLocalePanel@@` and `??_GChooseProfilePanel@@`.
That is UI panel code, consistent with `UI.cpp`. One dissenting call against ~23
corroborating functions is not a mis-pin; the likeliest reading is that the
2-float→float `Time2IirA` is itself an ICF fold survivor. **No action.**
⚠ Recorded because it is the shape a future scan will re-flag.

### 8.5 ⚠ `ContextChecker.cpp` `0x8275B868` — UNDECIDABLE, and the *name* is the suspect
A **72 B, single-function sliver** 2 MB away from ContextChecker's main block
(`0x8256C7E0`, 55 fns), sitting inside `DirLoader.cpp`'s territory. It is named
`?IsContextUsed@?A0x1e5d0754@@YA_NVSymbol@@@Z`, and its caller is
`0x82757108 = ?LoadHeader@DirLoader@@AAAXXZ` — a **named DirLoader method inside
a 12,360 B block holding 20 named DirLoader symbols**, so DirLoader's pin is not
in doubt. A `DirLoader` method cannot call a `ContextChecker` file-static.

⇒ **The likeliest defect is the NAME at `0x8275B868`, not the pin** — and the
sliver may exist *only to host that name*, which is the circular-pin mechanism
exactly. **Not acted on**: per the map/name economics, *proving a name wrong does
not make renaming safe*, and I have no positive identification of what
`0x8275B868` actually is. **Handed off.**

### 8.6 ⚠ `VocalPlayer.cpp` `fn_826E57B0` — UNDECIDABLE
An 80 B anonymous function inside a block of **27 named `VocalPlayer` methods**
(so VocalPlayer's pin is solid) whose only callee is
`0x826F6930 ?Dispatch@SyncLocalMachineMsg@?A0x951deeb9@@UAAXXZ`, pinned
`BandMachineMgr.cpp`. `Dispatch` is **virtual** (`UAA`), so a direct `bl` is
already unusual, and `BandMachineMgr.cpp` is the unit carrying **six** distinct
anon hashes (§4) — the single worst-conditioned unit for this instrument.
PINHOME-1 separately flagged `?Dispatch@SyncLocalMachineMsg@?A0x6c4eb79b@@` as a
row where anon-namespace name-absence is *not* evidence. **No action.**

### 8.7 ⚠ `MemcardMgr_Xbox.cpp` `0x8251BB00-0x8251BFA8` — PROVEN-ADJACENT, not repairable
Same finding as §8.2 from the other side: 6 named rows, **3 of them
`PlatformMgr` methods**, plus `XPrivilegeCheck@?A0x8a9ffbf2`. Same true owner
(`PlatformMgr_Xbox.cpp`), same absence of a compilable destination, same
escalation.

## 9. Repairs — both pre-registered Δ0, both measured Δ0

Pre-registration written **before any edit** at `~/tmp/w15a/PREREGISTER.md`,
sign included. `.text` lines only; every `.pdata` line in these commits is dtk's
own re-derivation.

### 9.1 `730c3dcf` — `0x82529890` JoypadClient.cpp → Joypad_Xbox.cpp (72 B)

```
leg A: matched=42846 masked=22997 honest=19849 code%=37.992430  (recompiles: 0, settled)
leg B: matched=42846 masked=22997 honest=19849 code%=37.992430  (recompiles: 0, split=1, patch_steps=1)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   units at 100%: 165 -> 165 (0 reached, 0 fell off, both rulers)
```

| unit | leg A | leg B |
|---|---|---|
| `default/JoypadClient` | 25/29, 2916/3732 | 25/**28**, 2916/**3660** |
| `default/Joypad_Xbox` | 14/25, 1016/2948 | 14/**26**, 1016/**3020** |

**Exactly 2 units changed in the whole binary.** `total_code` unchanged (−72 +72).

### 9.2 `7c4e8b39` — `0x827DCAA4-0x827DCD80` TrackDir.cpp → HttpGet.cpp (720 B)

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   units at 100%: 165 -> 165 (0 reached, 0 fell off, both rulers)
```

| unit | leg A | leg B |
|---|---|---|
| `default/HttpGet` | 18/27, 1892/2540 | 18/**28**, 1892/**3260** |
| `default/TrackDir` | 119/151, 8632/14752 | 119/**150**, 8632/**14032** |

**Exactly 2 units changed.** `total_code` unchanged.

### Why Δ0 is the CORRECT result here, not a null result
Both moved rows are **unpairable in both the source and destination unit** —
`InitXinputJoypadThreadData` because our `Joypad_Xbox.cpp` only *declares* it
(line 290; there is no body), `fn_827DCAB0` because it is anonymous and objdiff
pairs by name. This is **REATTRIBUTION, not RE-HOMING**, and
`docs/decomp/pin-neutrality-scoped-2026-08-14.md` predicts exactly 0 for it. A
non-zero delta would have meant the edit did something unpredicted.

⚠ Both legs of both runs report `split=1` and (R1) `renamer_patched=1822`, so the
edits genuinely reached the build. A splits edit that does not re-split is
**INERT** and reads as a false Δ0 — which is why the assertion, not the number,
is the evidence here.

★ **HANDOFF — the 72 B of §9.1 is collectable.** `Joypad_Xbox.cpp` declares
`InitXinputJoypadThreadData()` and `RunXinputJoypadLoop()` in its anonymous
namespace and defines **neither**; retail has both (`0x82529890` 72 B,
`0x8252A0B8`). With the pin now correct, writing those two bodies would pair
them. That is a body-port lane, not a pin lane.

## 10. What this lane deliberately did NOT do

- **Did not fund the signal the brief named.** "Does the callee set agree with
  the unit" as a plurality/majority vote measures **1.38×–1.51×** against the
  known-good population — near the 1.95× that CLAUDE.md records having been
  wrongly used as a deterministic classifier. Reporting those 8,395 findings as
  suspects would have been a large, confident, useless worklist. **The brief's
  hypothesis is answered NEGATIVELY**, and the productive residue of it is the
  narrow internal-linkage sub-case.
- **Did not repair `MoggClipMap.cpp` block 1 (§8.2)**, though it is the largest
  and best-proven mis-attribution found. There is no correct destination we
  compile; moving it to `PlatformMgr.cpp` would install a *second* plausible
  wrong pin, which is precisely this lane's subject matter. Price of the
  conservative unpin is **−72 B** (two matched anonymous rows) plus an
  unquantified `total_code` perturbation. **ESCALATED, not closed.**
- **Did not rename `0x8275B868` (§8.5)** despite believing its name is wrong. I
  have no positive identification of what it *is*, and per the map/name
  economics *proving a name wrong does not make renaming safe* — an unpairable
  new name reads 0% forever.
- **Did not touch the two ambiguous 40 B functions** at `0x827DCD80`/`0x827DCDA8`
  in §9.2, or any `auto_*` attribution opportunity (e.g.
  `auto_03_82AE5FE8_text` calling `StringConversion`'s `Latin1ToUtf8`) — the
  `/Od` Quazal band is unmatchable at any source quality, so attributing it buys
  a pairable row at 0% with no content.
- **Did not act on §8.6** (`VocalPlayer`/`BandMachineMgr`), whose destination
  unit is the single worst-conditioned in the binary for this instrument (six
  anon hashes) and whose callee is virtual.
- **Did not run `tools/native_build_gate.sh` as a requirement**: this lane
  touches no `src/` file and no header — only `config/45410914/splits.txt`,
  `tools/circular_pin_census.py` and this document, none of which is an input to
  the native build. It was run anyway as cheap insurance; the line is below.
- **Did not delete the worktree.**

## 11. For the next lane

1. ★ **Wire `src/system/os/PlatformMgr_Xbox.cpp`** (728 lines, exists, not in
   `objects.json`) and pin `0x8251BFA8-0x8251CBE0` (+ probably
   `0x8251BB00-0x8251BFA8`) to it. This is the real fix for §8.2/§8.7 and it is a
   porting lane, not a pin lane. It is also the third distinct finding in that
   neighbourhood after W14-C's `0x8251CBE0` — **`0x8251B000-0x8251E000` is a
   run of ~18 small blocks each pinned to a different unit, and its named
   content is dominated by `PlatformMgr` methods and PlatformMgr's message
   classes.** That whole region deserves one lane.
2. **Body-port `InitXinputJoypadThreadData` / `RunXinputJoypadLoop`** into
   `Joypad_Xbox.cpp`'s anonymous namespace (§9.1 handoff).
3. **Identify `0x8275B868`** (§8.5) — 72 B, called by `DirLoader::LoadHeader`,
   currently carrying a name it cannot own.
4. ⛔ **Do not re-run the callee-set-plurality census.** It is measured and it is
   noise. The internal-linkage sub-case is already exhausted at this map's
   current naming density: **37 findings, of which the non-template, non-
   `_icf_arbitrary` residue is the 7 suspects adjudicated above.** It will become
   productive again only as more anonymous-namespace statics get named.
