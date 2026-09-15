# W16-BI — `0x8268b9a0` is `BandUser::NewRemoteBandUser`, not `UIPanel::NewObject`

**Lane:** W16-BI (Opus) · **Date:** 2026-09-15 · **Branch:** `w16-bi`, based on
main `a5a974aa3fc3` · **Worktree:** `~/tmp/wt-w16-bi`

**Ruler, read from `report.json`'s own `provenance.diff_config` (a LIST of
`key=value` strings, never inferred):** `functionRelocDiffs=name_check`,
`combineDataSections=true`, `combineTextSections=true`,
`ppc.calculatePoolRelocations=false`; objdiff **4.2.9**, `tool_commit
a5f0ea903ec1`, `tool_binary_hash 5a51cd51fe0a353f` — identical on the baseline
build and on every measurement below, so all deltas here compose on one ruler.

## Lane-internal totals

| | `matched_functions` | `matched_code` | `matched_code_percent` | `fuzzy_match_percent` |
|---|---:|---:|---:|---:|
| baseline (build 0) | 43,525 | 4,038,804 B | 39.418335 | 49.690746 |
| after the lane | **43,527** | **4,039,196 B** | **39.42216** | **49.69406** |
| **delta** | **+2** | **+392 B** | **+0.003825 pp** | **+0.003314** |

`total_code` **10,246,004** and `total_functions` **69,216** are unchanged
throughout — every change here is a re-home of already-pinned code or a map
name, so no pin moved the denominator.

**The briefed baseline was reproduced exactly before being used**, and more
strongly than by comparing numbers: my `fuzzy == 100` rowset snapshot is
**byte-identical** (`cmp`, 2,773,241 B) to the shared `~/tmp/rows_w16bd_main.json`.
Nothing below is inherited arithmetic.

## Commits

| sha | item |
|---|---|
| `37055de2` | 2 — map: `0x8268b9a0` → `NewRemoteBandUser`, `0x8268b4e8` → `??0RemoteBandUser@@QAA@XZ` |
| `276b0d92` | 3 — splits: drop the `UI.cpp:` island, merge `BandUser.cpp:`'s flanking blocks |
| `fdf40d51` | 3 — dtk's `.pdata` re-derivation (generated output, committed per the build contract) |
| `cb3eed93` | 4 — map: retail's real `?NewObject@UIPanel@@` is `0x82802418` (name only) |

Branch tip: **`cb3eed939a5b830a070d4df67224f92820833381`** (plus this document).

---

## Item 1 — the retail-byte adjudication

### `0x8268B9A0` (76 B) is `?NewRemoteBandUser@BandUser@@SAPAVRemoteBandUser@@XZ`

Retail's body (dtk asm, keyed on the `.fn fn_8268B9A0` symbol — the address
column in a multi-block unit's `.s` is dtk's synthetic
`first_block_start + cumulative offset` and reads `823F5C98` here, which is *not*
where this function lives):

```
+0x00  mflr r12 / stw r12,-0x8(r1) / std r31,-0x10(r1) / subi r31,r1,0x70 / stwu r1,-0x70(r1)
+0x14  li   r3, 0x108              <- 264
+0x18  bl   0x827BD2F0             <- operator new
+0x1c  stw  r3, 0x50(r31) / cmplwi r3,0 / beq +0x34
+0x28  li   r4, 0x1                <- MSVC most-derived flag
+0x2c  bl   0x8268B4E8             <- the constructor
+0x34  li   r3, 0x0
+0x38  epilogue, blr
```

**Five independent discriminators, all agreeing. The coordinator's reading is
confirmed in every particular.**

1. **Allocation immediate.** Retail allocates **264**.
   `cl.exe /d1reportSingleClassLayout` says `sizeof(RemoteBandUser) == 264
   (0x108)` and `sizeof(UIPanel) == 104 (0x68)`. Retail cannot be
   `new UIPanel()`.
2. **Construction callee.** `0x8268B4E8`, adjudicated below as
   `??0RemoteBandUser@@QAA@XZ`. Our `??0UIPanel@@QAA@XZ` is 216 B — a different
   function entirely.
3. **Function size.** Retail is **76 B**; our `?NewObject@UIPanel@@` COMDAT is
   **100 B**, because a `NewObject` returning `Hmx::Object*` needs a
   vbtable-mediated upcast tail (`lwz 4(r3); lwz 4(r11); add; addi 4`) that a
   `RemoteBandUser*` return does not. Our
   `?NewRemoteBandUser@BandUser@@SAPAVRemoteBandUser@@XZ` is **76 B**.
4. **Whole-body identity.** Our COMDAT is **RAW-identical to retail at all 17
   non-reloc words** of 19, and its only two relocations land at exactly retail's
   two `bl` offsets, in order: `+0x18 ??2@YAPAXI@Z` → `0x827BD2F0`,
   `+0x2c ??0RemoteBandUser@@QAA@XZ` → `0x8268B4E8`.
5. **Call graph.** `0x8268B9A0` has **exactly one** retail `bl` caller,
   `0x8268485c`, which lies inside `fn_82684728` = `??0BandUserMgr@@QAA@HH@Z`
   (596 B) — and our `BandUserMgr.cpp:49` calls `BandUser::NewRemoteBandUser()`,
   with our COMDAT's reloc at the matching offset **+0x134**. A
   `REGISTER_OBJ_FACTORY` `NewObject` is reached through a **data**
   function-pointer materialisation in `Object::RegisterFactory`, not `bl`-called
   from a constructor (demonstrated for real in item 4: the true factory has
   **0** `bl` callers).

`li r4,1` is MSVC's hidden most-derived flag, which exists because
`src/band3/game/BandUser.h:235` declares
`class RemoteBandUser : public virtual BandUser, public virtual RemoteUser`. The
map already carries corroborating `$4PPPPPPPM@A@` virtual-base adjustor thunks
for this class (e.g. `??_ERemoteBandUser@@$4PPPPPPPM@A@AAPAXI@Z`).

### `0x8268B4E8` (316 B) is `??0RemoteBandUser@@QAA@XZ`

Our COMDAT is **316 B** — retail's size to the byte — and is **RAW-identical at
all 60 non-reloc words** of 79; all 19 reloc-bearing words are adjudicated by
target. Every relocation lands at exactly the offset of the corresponding
relocated instruction in retail, in the same order:

| our reloc | retail resolves to |
|---|---|
| `+0x04 __savegprlr_29` | `0x8282925C` |
| `+0x28/2c/30` (HA) + `+0x34/38/3c` (LO) — the three `??_8RemoteBandUser@@7B…@` **vbtables** | `0x820E02E4`, `0x820E0E9C`, `0x820E0E94` |
| `+0x50 ??0User@@QAA@XZ` | `0x82523900` (map agrees) |
| `+0x64 ??0BandUser@@QAA@XZ` | `0x8268AA10` (map absent ⇒ placeholder) |
| `+0x78 ??0RemoteUser@@QAA@XZ` | `0x82523BF8` (map agrees) |
| `+0x88/8c/98` (HA) + `+0x90/94/a4` (LO) — the three `??_7RemoteBandUser@@6B…@` **vftables** | `0x820E026C`, `0x820E023C`, `0x820E0234` |
| `+0x10c ??2@YAPAXI@Z` | `0x827BD2F0` |
| `+0x120 ??0TourCharRemote@@QAA@XZ` | `0x82B80550` |
| `+0x138 __restgprlr_29` | `0x828292AC` |

The body is a textbook MSVC virtual-base constructor, and **every store matches
the compiler's own `RemoteBandUser` layout offset-for-offset**:

* `cmplwi cr6,r4,0 / beq` gates the vbase-construction block on the
  most-derived flag.
* `{vbptr}` at `0x0`; `mRemoteChar` at `0x4` receives the `new TourCharRemote()`
  result (retail `li r3,0x84` = **132**, and our compile emits 132 at the same
  word); `mCurrentInstrumentCareerScore` / `mCurrentHardcoreIconLevel` /
  `mCymbalConfiguration` at `0x8/0xc/0x10` are zeroed from `r29`.
* Subobjects constructed at `+0x18` (User), `+0x5c` (BandUser), `+0xf4`
  (RemoteUser) — the layout report puts `{vfptr}` at exactly those three offsets.
* The tail walks the vbtable (`lwz 0(r30)`, then `+4/+8/+0xc`) to store the three
  vftable pointers into the virtual bases, and writes the vtordisp fields with
  `subi r11,r11,0x18 / 0x5c / 0xf4` — **exactly the three vbase offsets the
  compiler reports (24 / 92 / 244)**.

### The operator-new callee needs no alias work

`0x827BD2F0` is mapped `??2CriticalSection@@SAPAXI@Z`; we spell `??2@YAPAXI@Z`.
That fold is **already installed** — `scripts/symbol_aliases.json` group index
**1546**, address `0x827bd2f0`, survivor `??2CriticalSection@@SAPAXI@Z`, **123**
folded members with `??2@YAPAXI@Z` among them. The charge is already forgiven, so
this lane made **no alias edit** and the validator's group total is unchanged at
1,652.

## Item 2 — caller census for the new names

`??0RemoteBandUser@@QAA@XZ` was previously **anonymous**, and naming an anonymous
address is a bet whose sign is set by the caller population (MAPID-1: it converts
placeholder-**forgiven** sites into **checked** ones). Censused before editing:

* **`0x8268B4E8` has exactly ONE retail `bl` caller: `0x8268b9cc`** — which is
  `0x8268B9A0 + 0x2c`, i.e. *inside the factory itself*. Our source spells that
  site `??0RemoteBandUser@@QAA@XZ`, so the one site that stops being forgiven and
  becomes checked **agrees**. No caller can lose; the bet has no downside leg.
* **`0x8268B9A0` has exactly ONE retail `bl` caller: `0x8268485c`**, inside
  `??0BandUserMgr@@QAA@HH@Z`. Here the rename is a *repair*: that site was
  **charged** (retail said `?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ`, we say
  `?NewRemoteBandUser@BandUser@@SAPAVRemoteBandUser@@XZ`) and now agrees.
* Both new names were asserted **free map-wide** first (name-injectivity: 0
  other addresses carried either), and the old name sat at exactly the one
  address being freed.

**The `BandUserMgr` repair does not pay bytes, and that was predicted, not
discovered.** Its 596 B row carries **5** charged sites, enumerated from the
graded ruler (`tools/w25_charge_detail.py`, which runs `objdiff-cli diff` with no
`--build` and so cannot unpatch the tree):

| idx | retail | ours |
|---|---|---|
| 47 | `reserve<vector<Dep*>>` | `reserve<vector<BandUser*>>` |
| 50 | `reserve<vector<Dep*>>` | `reserve<vector<LocalBandUser*>>` |
| 53 | `reserve<vector<Dep*>>` | `reserve<vector<RemoteBandUser*>>` |
| **77** | **`?NewObject@UIPanel@@…`** | **`?NewRemoteBandUser@BandUser@@…`** |
| 92 | `push_back<vector<ChatReceiver*>>` | `push_back<vector<LocalBandUser*>>` |

`matched_code` is all-or-nothing per row, so closing [77] alone buys **0 bytes**:
measured **99.832214 → 99.86577**, not 100. This is RESIDUAL-1 exactly — price a
candidate from the charged-site list, never from a headline gap. The four
survivors are pointer-vector ICF folds (every `T*` vector instantiates identical
code), which is alias work and **outside this lane's bar**; filed below.

## Item 3 — what moved in `splits.txt`

```
UI.cpp:                                                   (21 -> 20 .text blocks)
-	.text       start:0x8268B9A0 end:0x8268B9EC
-	.pdata      start:0x82231030 end:0x82231038           <- dtk, re-derived

BandUser.cpp:                                              (7 -> 6 .text blocks)
-	.text       start:0x8268AEC8 end:0x8268B9A0
-	.text       start:0x8268B9EC end:0x8268C920
+	.text       start:0x8268AEC8 end:0x8268C920
-	.pdata      start:0x82230FA8 end:0x82231030            <- dtk, re-derived
-	.pdata      start:0x82231038 end:0x82231100            <- dtk, re-derived
+	.pdata      start:0x82230FA8 end:0x82231100            <- dtk, re-derived
```

Checked before editing: exactly **three** blocks tile `0x8268AEC8–0x8268C920`
with no gaps and no other claimant, so the merge is exact; `UI.cpp:` keeps 20
other `.text` blocks so it cannot drain to an empty unit (which would emit a
42 B obj and hard-fail report generation). **`.text` only** — the `.pdata` lines
above are dtk's own re-derivation, committed as generated output because the
split-guard refuses to proceed until the split is a fixed point of its input.

**dtk corroborates the boundary independently:** re-deriving from retail's own
`.pdata`, it moved **exactly one** unwind record (`0x82231030–0x82231038`) from
UI to BandUser and closed the hole its removal left. dtk agrees the 76 B is one
function with one unwind record belonging to the contiguous BandUser run.

### Where the island came from

`36f6b4df` (laneU, 2026-07-26) — a bulk **automated** WRONG-UNIT mover, 471
moves. Our `UI.obj` really does define `?NewObject@UIPanel@@`
(`REGISTER_OBJ_FACTORY(UIPanel)`, `src/system/ui/UI.cpp:964`), so the mover's
single-name-evidence rule fired **correctly on a map name that was wrong**, and
had no way to test the name itself. ⇒ **The island is a downstream artifact of
the map defect, not an independent error** — and it bought nothing: the row
landed at fuzzy 68.368, i.e. **0 bytes and 0 functions**, while displacing the
true owner. That is the transferable lesson: an automated wrong-unit mover
*propagates* a map error into splits geometry, and the resulting island is
evidence of the map defect.

### Prediction vs measurement

Pre-registered before the build (the two halves are **inseparable**: naming
without re-homing leaves the row in `UI.obj`, which cannot define the name;
re-homing without renaming puts a `?NewObject@UIPanel@@`-named target row into
`BandUser.obj`, which cannot define *that* name — neither half pays alone):

| | predicted | measured |
|---|---|---|
| CROSSED IN | 2 rows / 392 B | **2 rows / 392 B** |
| `??0RemoteBandUser@@QAA@XZ` | +316 B | **+316 B**, fuzzy 100 |
| `?NewRemoteBandUser@BandUser@@…` | +76 B | **+76 B**, fuzzy 100 |
| FELL OUT | 0 | **0** |
| `matched_functions` | 43,525 → 43,527 | **43,527** |
| `matched_code` | 4,038,804 → 4,039,196 | **4,039,196** |
| `??0BandUserMgr@@QAA@HH@Z` | improves, does **not** cross | **99.832214 → 99.86577** |
| `total_code` / `total_functions` | unchanged | **unchanged** |

**FELL OUT is 0 and there is nothing to explain.** The risk I named up front —
that merging two blocks enlarges the funclet byte-signature pairing pool, which
cost W16-BD item 5 a 32 B row — **did not materialise**.

The wrong `?NewObject@UIPanel@@` row **left the report** rather than falling out
of the crossed set, exactly as predicted: it was at fuzzy 68.368 and so
contributed 0 B, and the rowset tracks only `fuzzy == 100` membership. Per-unit
the arithmetic closes: `default/UI` `total_code` 23,688 → 23,612 (**−76**) with
`matched_code` **unchanged** at 11,592 and its percent rising 48.936 → 49.094;
`default/BandUser` `total_code` 18,012 → 18,088 (**+76**), matched 143 → 145,
`matched_code` 12,188 → 12,580 (**+392**). Whole-binary `total_code` Δ0.

## Item 4 — retail's real `?NewObject@UIPanel@@` is `0x82802418`

**Found, named, and the re-home filed rather than taken.**

I did **not** scan by body pattern. A body scan assumes the allocation shape, and
W16-BE's shape attribution for this row is void (it was reading
`NewRemoteBandUser`), so the shape was unknown. Instead I used a
**shape-independent** discriminator: a factory must `bl` the class ctor, and a
factory **allocates** where a constructor never does.

**Population and masks.** All **53** retail `bl` sites reaching
`??0UIPanel@@QAA@XZ` (`0x82812920`), resolved to **53 distinct** enclosing
functions via `symbols.txt` extents. Mask: keep only those that also contain a
`bl` to the operator-new survivor `0x827BD2F0` ⇒ **13 of 53**. Twelve are
derived-class constructors (200–760 B; `MainHubPanel`, `ManageBandPanel`,
`SetlistMergePanel`, `GamePanel`, `PracticePanel`, `ChordbookPanel`,
`FreestylePanel`, `TrainerPanel`, `HeldButtonPanel`, `PreloadPanel`, and two
anonymous), all allocating **after** the base-ctor call. The thirteenth:

```
0x82802418, 100 B      +0x14  li r3, 104      == sizeof(UIPanel)
                       +0x18  bl 0x827BD2F0   == operator new
                       +0x2c  bl 0x82812920   == ??0UIPanel@@QAA@XZ
                       +0x34  li r3, 0
```

Our `?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ` COMDAT is 100 B with relocs at
exactly `+0x18` and `+0x2c`, and **23 of its 25 words are RAW-identical** to
retail — the only two differing words being those two reloc-bearing ones.

**It is inside another unit's block, so it was NOT moved:** `0x82802418` lies in
`UIColor.cpp:`'s block `0x82802080–0x828024B0` (splits line 2267), which ends
exactly where `UI.cpp:`'s next block begins. `UIColor.cpp:` is outside this
lane's splits bar. Named only; re-home filed below.

**Predicted Δ0 functions / Δ0 bytes; measured exactly Δ0** (cumulative rowset
diff unchanged at +2 / +392 after this commit, CROSSED IN 0 / FELL OUT 0):

* The row sits in `default/UIColor`, whose obj cannot define the name, so it
  stays at fuzzy 0 pending the re-home.
* `fn_82802418` read fuzzy 0 / mpn 0 and was **not** `masked_equal`, so there was
  no byte-signature pairing for the name to break.
* Reference census: **0** `bl` callers, and exactly **one** materialisation of the
  address — `lis r11,0x8280` / `addi r4,r11,0x2418` at `+0x490/+0x498` of
  `?Init@UIManager@@UAAXXZ` (`0x82805BD8`, 1916 B), the `Object::RegisterFactory`
  call that is our own `REGISTER_OBJ_FACTORY(UIPanel)`. Our Init COMDAT spells
  `?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ` at that exact offset, so the site
  converts from placeholder-forgiven to checked-**and-agreeing**. Predicted the
  row would hold at fuzzy 100.0; **measured 100.0 before and after.**

No ICF explanation was needed, and it would not have been available: two
different-class factories carry different ctor relocations, and `/OPT:ICF` folds
only COMDATs identical **including relocations**.

### Two instrument corrections, both caught by controls rather than by inspection

1. ⛔ **My first reference scan was VACUOUS and its negative agreed with the
   hypothesis.** It read the image as one span from `0x82000000`, but
   `retail_body.Img.read` returns `b""` for a VA outside every section and
   `0x82000000` is the header page (`.rdata` starts at `0x82000400`, `.text` at
   `0x82270000`). It therefore reported "0 references" for **every** address.
   Only the positive controls — two known-registered factories, which must have
   a materialisation — exposed it. *A vacuity that confirms your prior is the
   hardest kind to catch.*
2. ⛔ **The fixed scan then reported TWO materialisations of `0x82802418`, and
   the second was a FALSE POSITIVE.** An 80-byte relaxed `lis`/`addi` pairing
   window matched **UIScreen's** `lis` at `+0x478` with **UIPanel's**
   `addi 0x2418` at `+0x498`. Decoding retail's actual instructions shows
   `+0x478/+0x480` materialises `0x828023A0` and `+0x490/+0x498` materialises
   `0x82802418`. Undecoded, this lane would have recorded a bogus "UIScreen and
   UIPanel factories are ICF-folded" claim — which the relocation rule above
   forbids outright.

## Gates (brief's order, native LAST)

```
full build                                                  rc=0
python3 scripts/verify_ruler_agreement.py --check            rc=0
  OK: both objdiff-cli entry points resolve the same ruler.
python3 scripts/verify_objs_patched.py --verify-manifest      rc=0
  [patch-state] OK: 1215 decomp, 3114 target objects match (tree_sha256=b62533a790d332d6)
python3 tools/icf_alias_finder.py --validate                 rc=0
  VALIDATE: PASS -- 1404 map-consistent, 247 tolerated, 0 contradicted, 1652 total
tools/native_build_gate.sh                                   rc=0
```

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0`, so this is full coverage and not the `PASS (INCOMPLETE: …)` shape.
This lane made **no `src/` edit whatsoever**; only this document was committed
after the gate run, and a markdown file cannot reach the native link.

Corroborating anti-vacuity check for every COFF name read in this document: the
build's own `[renamed-check]` went **25,834/29,379 → 25,835/29,380** across the
lane — the one new map name is present in a target obj, and 87.9% presence proves
the pre-compile renamer ran (a reflinked pre-renamer tree reads ~0%).

## NOT done, and why

* **`0x82802418` was NOT re-homed** into `UI.cpp:`. It is inside
  **`UIColor.cpp:`**'s block `0x82802080–0x828024B0` (splits line 2267), outside
  this lane's bar. **Filed:** split that block at `0x82802418` and give
  `0x82802418–0x8280247C` to `UI.cpp:`. Expected **+1 fn / +100 B** — our COMDAT
  is already RAW-identical at all 23 non-reloc words and both reloc targets
  resolve (operator-new alias-forgiven via group 1546; `??0UIPanel@@QAA@XZ`
  exactly map-named at `0x82812920`), so the only thing holding the row at 0 is
  that `UIColor.obj` cannot define the name. ⚠ Whoever takes it must check what
  else lives in `0x82802080–0x828024B0` before splitting, and must expect dtk to
  re-derive `.pdata`.
* **`0x828023A0` is `?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ` but was NOT
  named** — outside this lane's explicit map bar (`0x8268b9a0`, `0x8268b4e8`, and
  the real UIPanel factory). The evidence is complete, so it is a short job:
  our 72 B COMDAT is **RAW-identical to retail at all 16 non-reloc words**,
  `li r3,64 == sizeof(UIScreen)`, relocs resolve to operator new `0x827BD2F0` and
  `??0UIScreen@@QAA@XZ` `0x827F1E90`, and the single materialisation is
  `?Init@UIManager@@UAAXXZ` `+0x478/+0x480`, where our COMDAT spells that same
  name. It is in the **same** `UIColor.cpp:` block, so it re-homes together with
  the UIPanel factory — treat the two as one item.
* **The four surviving `??0BandUserMgr@@QAA@HH@Z` charges were NOT closed.** They
  are pointer-vector ICF folds (`reserve<vector<Dep*>>` vs our
  `BandUser*`/`LocalBandUser*`/`RemoteBandUser*`; `push_back<vector<ChatReceiver*>>`
  vs our `LocalBandUser*`). Closing them is **alias work, explicitly outside this
  lane's bar**, and it is the whole 596 B: the row needs all four proven on
  retail bytes before it crosses. I did not install an alias on a guess — a
  fabricated alias lifts `name_check` **by construction** and the `none` control
  cannot detect it.
* **No `src/` edit, no `symbols.txt` edit, no hand-edited `.pdata`, no alias
  edit.** Every `.pdata` line in this lane's diff is dtk's own re-derivation, and
  the alias group total is unchanged at 1,652.
* **W16-BH's `Ham.cpp:` heading and map rows `0x8227a7a8`–`0x8227a948`, and
  W16-BG's 24 `__unwind$`/`__catch$` map rows, were not touched**, per the
  concurrency bars.
* **W16-BE's recommendation to dispatch a `UIPanel` struct-layout lane should be
  WITHDRAWN.** Its §1 read this row as a "real layout divergence — ours emits
  `li r3,0x68` (104) where retail emits `li r3,0x108` (264)" and inferred "an
  embedded `CriticalSection` member our header lacks" from the
  `??2CriticalSection@@SAPAXI@Z` call. Both observations are correct and the
  conclusion is void: they were made against **`NewRemoteBandUser`**, the 264 is
  `sizeof(RemoteBandUser)`, and `??2CriticalSection@@SAPAXI@Z` is merely the
  **ICF survivor spelling of the global `operator new`** (alias group 1546, 123
  members). `sizeof(UIPanel) == 104` is confirmed by the compiler *and* by our
  factory now matching retail's `li r3,104` at `0x82802418`. **There is no
  UIPanel layout defect.** That lane would have hunted 160 bytes of members that
  do not exist — the clearest instance in this lane of why a row must be
  identified before it is diagnosed.
