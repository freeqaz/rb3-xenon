# W16-BN — `NetSearchResult`: settle the factory spelling, port the TU, re-home the `UI.cpp:` tail

**Lane:** W16-BN · **Branch:** `w16-bn` (off `7320abd5f814`) · **Date:** 2026-09-15
**Worktree:** `/home/free/tmp/wt-w16-bn`

---

## §0 Provenance and baseline

Read from `build/45410914/report.json` in this worktree at `22653f66`, every
numeric `int()`/`float()`-coerced (the file stores `matched_code` and
`total_code` as JSON **strings**, and protobuf-JSON omits defaults):

| key | value |
|---|---|
| `matched_functions` | **43,558** |
| `matched_code` | **4,040,604 B** |
| `matched_code_percent` | **39.43181** |
| `total_functions` | **69,240** |
| `total_code` | **10,247,068 B** |
| `masked_equal_functions` | **23,080** |
| `fuzzy_match_percent` | 49.6996 |

Ruler: `name_check` (shipped default since `d04c83df`, 2026-08-12), objdiff
4.2.9. These reproduce the brief's figures exactly, so the brief is confirmed
rather than inherited.

⚠ **`../rb3` resolves to `/home/free/tmp/rb3` from this worktree and does not
exist.** My first read of the oracle used a worktree-relative path, got
"No such file or directory", and I briefly announced the brief's figure
*refuted*. It was not: the oracle is at
`/home/free/code/milohax/rb3/src/network/net/NetSearchResult.cpp` (52 lines).
This is the sibling-relative-path-vanishes-in-a-worktree trap CLAUDE.md
documents for `pin_from_symnames.py` / `native/CMakeLists.txt` — **its failure
is shaped exactly like a legitimate "not applicable"**. Rule adopted for the
rest of the lane: absolute paths for every oracle read.

---

## §1 Item 1 — what `0x823f5c20`, `0x828023A0` and `0x823f5a90` actually are

W16-BK proved only that *"`0x823f5c20` constructs a `NetSearchResult` and is not
`UIScreen`'s"*. The **spelling** was open: `?New@NetSearchResult@@SAPAV1@XZ` vs
`?NewObject@NetSearchResult@@SAPAVObject@Hmx@@XZ`.

### 1.1 The two bodies are byte-twins, so their shape cannot discriminate

`0x823f5c20` and `0x828023A0` are byte-identical 72 B bodies differing **only**
in the constructor `bl` (`0x823f5a90` vs `0x827f1e90`); both allocate with
`li r3,0x40`. Neither can be told apart by shape, because `NetSearchResult`'s
Complete Object Locator reports `offset = 0` — its primary base sits at 0 — so an
`Object*`-returning factory needs **no** `this` adjustment. A `NewObject` and a
`New` compile to the same instructions here.

### 1.2 RTTI settles the classes (map-independent), re-derived in this lane

| factory | stores vptr | `[vptr-4]` COL | `pTypeDescriptor+8` |
|---|---|---|---|
| `0x823f5a90` | `0x820599D4` | `0x821d89e8` | **`.?AVNetSearchResult@@`** |
| `0x827f1e90` (control) | `0x82120244` | — | **`.?AVUIScreen@@`** |

So `0x823f5a90` is `NetSearchResult`'s constructor and `0x828023A0` is the
factory that calls `UIScreen`'s — i.e. `0x828023A0` is the real
`?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ` and the map's credit at
`0x823f5c20` is **false**.

### 1.3 The address-taken test decides `New` vs `NewObject`

`REGISTER_OBJ_FACTORY(X)` installs `&X::NewObject` into a factory map, which
requires **forming** the function's address. Scanning all of `.text` for
`lis`/`addi`/`ori` address-formation of `0x823f5c20`: **zero sites.** The only
data word in the image equal to `0x823f5c20` is its own `.pdata`
`BeginAddress` at `0x82209148`. A registered `NewObject` cannot have zero
address-formation sites ⇒ `0x823f5c20` is **not** a `NewObject`.

### 1.4 Mechanical confirmation

Our port, compiled by the same `cl 10224` that built retail, emits exactly
`?New@NetSearchResult@@SAPAV1@XZ` — read out of the COFF symbol table after a
build, not guessed.

### 1.5 Oracle and header corroboration

The oracle constructor and retail `0x823f5a90` match **7/7 at statement level,
in order**. All four header members and two `sizeof`s are witnessed:

| retail instruction | header member |
|---|---|
| `stw r3,0x28(r30)` | `SessionData *mSessionData; // 0x28` |
| `stw r10,0x2c(r30)` | `MatchmakingSettings *mSettings; // 0x2c` |
| `stw r3,0x30(r30)` | `int mNumOpenSlots; // 0x30` |
| `addi r3,r30,0x34; bl ??0String@@` | `String mHostName; // 0x34` |
| `li r3,0x28` | `sizeof(MatchmakingSettings)` = 0x28 |
| `lwz r3,0x34(r11)` | `SessionSettings *mSettings; // 0x34` on `NetSession` |

**Verdict:** `0x823f5a90` = `??0NetSearchResult@@QAA@XZ`;
`0x823f5c20` = `?New@NetSearchResult@@SAPAV1@XZ`;
`0x828023A0` = `?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ`. Settled three
independent ways (retail bytes, RTTI, our own compiler).

---

## §2 Item 2 — porting the TU

`src/network/net/NetSearchResult.cpp` (71 lines) is the 52-line oracle body
verbatim plus a 26-line evidence header. Wired in `config/45410914/objects.json`
as `NonMatching` with `extra_cflags: ["/DRB3_HANDLE_LOCAL_STATIC"]`.

**The gate is required, and I first got this wrong.** I inferred from a callee
list that `Handle` had no guard check and therefore did *not* need the per-TU
`/D`. Disassembling `0x823f59b0` in full refuted that: the guard word is at
`0x82CC001C`, tested with `lis`/`lwz`/`clrlwi.`, set with `ori r11,r11,1`, with
an inline `??0Symbol@@QAA@PBD@Z`. **Lesson: read the bytes, not the callee
list.**

I also mis-labelled `0x823f5a68` as a `??__F` atexit funclet. It is not — Milo's
`Symbol` is trivially destructible so our obj emits no `??__F` at all;
`0x823f5a68` is `Handle`'s **EH unwind funclet**, clearing the guard bit so a
later call retries if the `Symbol` constructor throws.

Committed as `22653f66`. Measured **Δ0 exactly** as a deliberate control: with
no splits heading there is no objdiff unit, so a correct port must move nothing.

---

## §3 Item 4 census — the `UI.cpp:` block `0x823F4A30–0x823F5C98`

43 `.pdata` functions tile the block exactly. dtk's own derived `.pdata` range
(`0x82209000–0x82209158` = 43 × 8 B) corroborates the count independently.

`.pdata` RUNTIME_FUNCTION decode is **big-endian**; the second dword's bitfield
is MSB-first `ExceptionFlag:1, ThirtyTwoBit:1, FuncLen:22, PrologLen:8`, so
`FuncLen = ((w >> 8) & 0x3FFFFF) * 4`.

### 3.1 The block is two TUs, and the seam is at `0x823F56F8`

| region | span | bytes | functions | function bytes | identity |
|---|---|---|---|---|---|
| HEAD | `0x823F4A30–0x823F56F8` | 3,272 | 28 | 3,148 | **MessageBroker** (+ CheatProvider neighbours) |
| TAIL | `0x823F56F8–0x823F5C98` | 1,440 | 15 | 1,396 | **`network/net/NetSearchResult.cpp`** |

The TAIL tiles with no gap unaccounted for — 1,396 function bytes + 44 B of
alignment padding and 8-byte EH prefixes = 1,440:

```
0x823f56f8 +148 -> 0x823f578c  ??1NetSearchResult
0x823f578c  +40 -> 0x823f57b4  funclet
0x823f57b4  +44 -> 0x823f57e0  funclet
0x823f57e0 +132 -> 0x823f5864  ?Equals            (+4 align)
0x823f5868  +76 -> 0x823f58b4  ??_G               (+4 align)
0x823f58b8 +120 -> 0x823f5930  ?Save
0x823f5930 +120 -> 0x823f59a8  ?Load              (+8 EH prefix)
0x823f59b0 +184 -> 0x823f5a68  ?Handle
0x823f5a68  +32 -> 0x823f5a88  funclet            (+8 EH prefix)
0x823f5a90 +264 -> 0x823f5b98  ??0
0x823f5b98  +40 -> 0x823f5bc0  funclet
0x823f5bc0  +44 -> 0x823f5bec  funclet
0x823f5bec  +40 -> 0x823f5c14  funclet            (+4 align, +8 EH prefix)
0x823f5c20  +72 -> 0x823f5c68  ?New
0x823f5c68  +40 -> 0x823f5c90  funclet            (+8 trailing EH prefix)
```

The cut at `0x823F56F8` is the exact end of `except_data_823E2B60`
(`0x823F56F0` + 8), so dtk cannot "end within symbol" there. The trailing 8 B at
`0x823f5c90` is the EH prefix of the *next* function; house convention keeps a
prefix with the **preceding** block, which is why the TAIL ends at
`0x823F5C98`.

Corroborated independently by the vtable (slots 0/6/10 = `0x823f5868` `??_G`,
`0x823f59b0` `Handle`, `0x823f5930` `Load`) and by the block's **only** string
literal, `'get_mode_name'`, at `0x82059a80`.

⚠ **Six "head string literals" I reported at `0x82c700xx` were an artifact of my
own detector** — I never reset the `lis`/`addi` register state per function, so
values were stitched across function boundaries. All six discarded. The HEAD has
**no** genuine string literals.

### 3.2 A SECOND false credit, in the HEAD — FILED, NOT MOVED

`0x823f5270` (76 B) reads fuzzy **100.00** as `??_GAutomator@@UAAPAXI@Z` while
actually being **MessageBroker's vtable slot 0**. Its true name is
`??_GMessageBroker@@UAAPAXI@Z`, which **no object of ours emits** (the header is
Wii-only). Retail never allocates the `Automator` at all (`src/system/ui/UI.cpp`
lines 929–932, verified in Ghidra at `0x827E0690`: no `??0Automator` call,
`mAutomator` stays null).

**Decision: FILE, do not rename.** Renaming would be a certain **−76 B** that
installs nothing and leaves a permanently unpairable row — precondition (b) of
the W16-BL rule fails. Moving the HEAD is also out of this lane's bar: its
destination heading is neither `UI.cpp:` nor the new `NetSearchResult.cpp:`, and
**W16-BM is live and owns every other heading's `.text`**.

---

## §4 PRE-REGISTERED PREDICTION (committed before the measuring build)

### 4.1 The transaction (coupled — all of it, or none)

**Map** (`scripts/target_symbol_map.json`), 9 keys:

| address | name | note |
|---|---|---|
| `0x823f56f8` | `??1NetSearchResult@@UAA@XZ` | new |
| `0x823f57e0` | `?Equals@NetSearchResult@@UBA_NPBV1@@Z` | new |
| `0x823f5868` | `??_GNetSearchResult@@UAAPAXI@Z` | new |
| `0x823f58b8` | `?Save@NetSearchResult@@UBAXAAVBinStream@@@Z` | new |
| `0x823f5930` | `?Load@NetSearchResult@@UAAXAAVBinStream@@@Z` | new |
| `0x823f59b0` | `?Handle@NetSearchResult@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | new |
| `0x823f5a90` | `??0NetSearchResult@@QAA@XZ` | new |
| `0x823f5c20` | `?New@NetSearchResult@@SAPAV1@XZ` | **replaces** the false `?NewObject@UIScreen@@` |
| `0x828023a0` | `?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ` | receives the moved name |

**Splits** (`config/45410914/splits.txt`), `.text` only — `.pdata` is derived
output and is left for dtk to re-derive:

- `UI.cpp:` first `.text` `start:0x823F4A30 end:0x823F5C98` → `end:0x823F56F8`
- NEW path-qualified heading `network/net/NetSearchResult.cpp:` with
  `.text start:0x823F56F8 end:0x823F5C98`

`UI.cpp:` retains 20 other `.text` blocks, so it cannot drain its last block
(the failure mode that makes a unit emit a 42-byte obj and hard-fail
`report.json`).

### 4.2 ★★★ Four of my eight proposed spellings were WRONG, and testing caught it

I proposed `?Equals@…@QBA…`, `?Save@…@QBA…`, `?Load@…@QAA…` and
`?Handle@…PBVDataArray…`. Reading the **COFF symbol table of our built object**
(precondition (b) of the W16-BL rule — *read the object, after a build, not the
write-up*) showed **4 of 8 absent**: the header declares these virtual, so they
mangle `U`, not `Q`, and `Handle` takes a **non-const** `PAVDataArray`:

```
?Equals@NetSearchResult@@UBA_NPBV1@@Z                        (not QBA)
?Save@NetSearchResult@@UBAXAAVBinStream@@@Z                   (not QBA)
?Load@NetSearchResult@@UAAXAAVBinStream@@@Z                   (not QAA→ yes UAA)
?Handle@NetSearchResult@@UAA?AVDataNode@@PAVDataArray@@_N@Z   (PAV, not PBV)
```

Had I written the guessed spellings, **four rows would have become permanently
unpairable 0% rows** — the exact W16-BL failure. All 8 corrected names are
**defined** in `build/45410914/src/network/net/NetSearchResult.obj` (class 2,
EXTERNAL) and all 8 are **FREE** in the map; the only collision in the whole
transaction is the coupled `?NewObject@UIScreen@@` move.

### 4.3 An unplanned 8/8 geometric corroboration

Our COMDAT section extents equal retail's spans exactly, once the 8-byte EH
prefix and trailing funclets are accounted for:

| symbol | our section size | = prefix + body + funclets |
|---|---|---|
| `??1NetSearchResult` | 240 | 8 + 148 + 40 + 44 |
| `??0NetSearchResult` | 396 | 8 + 264 + 40 + 44 + 40 |
| `?New` | 120 | 8 + 72 + 40 |
| `?Handle` | 224 | 8 + 184 + 32 |
| `??_G` | 76 | 76 |
| `?Equals` | 132 | 132 |
| `?Save` | 120 | 120 |
| `?Load` | 120 | 120 |

This also **identifies the 32 B funclet**: `__unwind$58744` sits at offset
`0xc0 = 8 + 184` inside `?Handle`'s section, so `Handle`'s own guard-clearing
funclet is in our pool — retiring the doubt below.

### 4.4 Body evidence: 0 word diffs, 0 charged relocation names

Across all 8 bodies (1,116 B), comparing our COMDAT bytes to retail with our
relocated words masked:

**non-relocated word differences: 0. charged branch-relocation-name sites: 0.**

Every branch relocation is EQUAL, ALIAS-FORGIVEN
(`??2@YAPAXI@Z`→`??2CriticalSection@@SAPAXI@Z`;
`??3@YAXPAX@Z`→`??3BinStream@@SAXPAX@Z`) or PLACEHOLDER-FORGIVEN (`0x823f36b0`,
`0x823f3b10`, `0x823ef498`, `__savegprlr_*`/`__restgprlr_*`).

All 7 distinct **data** relocation targets resolve to retail addresses with **no
map name** — `0x820591c4`, `0x820599d4`, `0x82cbfaf8`, `0x82c71838`,
`0x82cc001c`, `0x82059a80`, `0x82cc0018` — and are therefore
placeholder-forgiven by `is_placeholder_symbol_name`.

★ The false credit at `0x823f5c20` reads EQUAL on its constructor relocation
**only because `0x823f5a90` is named in the same transaction.** That coupling is
what makes this materially better than W16-BK's predicted −72 B.

### 4.5 The funclet half: 7/7 pair, 6/7 with the name also EQUAL

Our object holds **11** anonymous `__unwind$` funclets. ⚠
`tools/coff_bodies_ext.py:129` **excludes** `__unwind$`/`__ehhandler$` by design
("delimits slices without becoming a comparison target"), so my first comparison
printed `our anonymous funclets: 0` and dutifully reported *no counterpart for
any of the 7* — an empty result agreeing with the null. I read COFF sections
directly instead.

⚠ A second self-inflicted defect: I first masked 4 of 8 words on the 32 B
funclet because their relocations named `@comp.id`. Type `0x0012` is
`IMAGE_REL_PPC_PAIR`, whose "symbol index" field is an **addend**, so
`@comp.id` was my misreading of index 0.

| retail funclet | size | pairs with | reloc name |
|---|---|---|---|
| `0x823f578c` | 40 | `__unwind$58603` | `??1Object@Hmx@@UAA@XZ` **EQUAL** |
| `0x823f57b4` | 44 | `__unwind$58604` | `??1String@@UAA@XZ` **EQUAL** |
| `0x823f5a68` | 32 | `__unwind$58744` (Handle's own) | guard REFHI/REFLO, retail target unnamed |
| `0x823f5b98` | 40 | `__unwind$59841` | `??1Object@Hmx@@UAA@XZ` **EQUAL** |
| `0x823f5bc0` | 44 | `__unwind$59842` | `??1String@@UAA@XZ` **EQUAL** |
| `0x823f5bec` | 40 | `__unwind$59844` | `??1MemStream@@UAA@XZ` **EQUAL** |
| `0x823f5c68` | 40 | `__unwind$59954` | `??3@YAXPAX@Z` vs `??3BinStream@@SAXPAX@Z` — **ALIAS-FORGIVEN** |

**Honest caveat:** the 32 B row compares only 4 of 8 words (the other 4 are
guard-address REFHI/REFLO halves that differ by construction — ours is
`StaticClassName`'s/`Handle`'s guard, retail's is `0x82CC001C`). It is the
weakest of the seven, mitigated by there being **3 byte-identical candidates**
in the pool, one of which is `Handle`'s own.

### 4.6 Per-row prediction — 16 rows

Baseline values read from `report.json`; 272 B of these rows are at fuzzy 100
today, **including the 72 B false credit**.

| VA | size | unit before | fuzzy before | mpn before | **predicted after** | basis |
|---|---|---|---|---|---|---|
| `0x823f56f8` | 148 | default/UI | 0.00 | 0.00 | **100 / 100** | named `??1`, 0 diffs |
| `0x823f578c` | 40 | default/UI | 100.00 me | 100.00 | 100 / 100 me | funclet exact + name EQUAL |
| `0x823f57b4` | 44 | default/UI | 100.00 me | 100.00 | 100 / 100 me | funclet exact + EQUAL |
| `0x823f57e0` | 132 | default/UI | 0.00 | 0.00 | **100 / 100** | named `?Equals` |
| `0x823f5868` | 76 | default/UI | 0.00 | 0.00 | **100 / 100** | named `??_G` |
| `0x823f58b8` | 120 | default/UI | 0.00 | 0.00 | **100 / 100** | named `?Save` |
| `0x823f5930` | 120 | default/UI | 0.00 | 0.00 | **100 / 100** | named `?Load` |
| `0x823f59b0` | 184 | default/UI | 0.00 | 0.00 | **100 / 100** | named `?Handle` |
| `0x823f5a68` | 32 | default/UI | 100.00 me | 100.00 | 100 / 100 me | 3 exact candidates |
| `0x823f5a90` | 264 | default/UI | 0.00 | 0.00 | **100 / 100** | named `??0` |
| `0x823f5b98` | 40 | default/UI | 99.40 me | 99.90 | **100 / 100** me | exact + name EQUAL |
| `0x823f5bc0` | 44 | default/UI | 100.00 me | 100.00 | 100 / 100 me | exact + EQUAL |
| `0x823f5bec` | 40 | default/UI | 99.50 me | 100.00 | **100** / 100 me | exact + EQUAL |
| `0x823f5c20` | 72 | default/UI | 100.00 (FALSE) | 100.00 | 100 / 100 (TRUE) | `?New`; ctor reloc EQUAL via the coupling |
| `0x823f5c68` | 40 | default/UI | 100.00 me | 100.00 | 100 / 100 me | exact, alias-forgiven |
| `0x828023a0` | 72 | default/UI | 0.00 | 0.00 | **100 / 100** | receives `?NewObject@UIScreen@@`; UI.obj defines it |

### 4.7 Aggregate prediction

| measure | baseline | **predicted** | Δ |
|---|---|---|---|
| `matched_functions` | 43,558 | **43,567** | **+9** |
| `matched_code` | 4,040,604 | **4,041,800** | **+1,196 B** |
| `matched_code_percent` | 39.43181 | **39.443482** | **+0.011672 pp** |
| `total_functions` | 69,240 | **69,240** | **0** |
| `total_code` | 10,247,068 | **10,247,068** | **0** |
| `masked_equal_functions` | 23,080 | **23,081** | **+1** |

Byte arithmetic: **+1,044** (the 7 newly-named 0.00 rows: 148+132+76+120+120+184+264)
**+80** (`0x823f5b98` 40 and `0x823f5bec` 40 crossing 99.x → 100)
**+72** (`0x828023a0`) = **+1,196 B**. `0x823f5c20`'s 72 B is already banked and
contributes **+0** — the transaction converts it from false credit to true
credit without moving the headline, which is the accuracy win independent of
the bytes.

Function arithmetic: +7 named zero rows, +1 for `0x823f5b98` (mpn 99.90 → 100),
+1 for `0x828023a0` = **+9**. `0x823f5bec` is already mpn 100, so it pays bytes
only — the documented `mpn`/`fuzzy` split.

A **new unit** `default/NetSearchResult` should appear; `default/UI` should lose
exactly 15 rows.

### 4.8 What would falsify this, and the residual risks

1. **dtk re-carve at the new boundary.** I argue `0x823F56F8` is safe because it
   is the exact end of `except_data_823E2B60`, but a re-carve would move
   `total_code`/`total_functions` off 0. **If those two move, the geometry
   argument is wrong** and I will report it rather than smooth it.
2. **The trailing 8 B at `0x823f5c90`** could be emitted as a `type:label` or
   data row in the new unit.
3. **Alias-group liveness** for `??3@YAXPAX@Z`→`??3BinStream@@SAXPAX@Z`: if that
   group is not live, `0x823f5c68` loses 40 B.
4. **Heading resolution** — `configure.py` hard-fails on an unresolved heading,
   so a mis-spelled path fails loudly rather than silently. That is a control,
   not a risk.
5. `0x828023a0` is the one row whose base object I did not byte-compare
   end-to-end; I verified UI.obj **defines** the name with matching geometry
   (8 + 72 + 40 = 120) but did not diff its 72 B against retail. If it misses,
   the prediction is long by 72 B.

### 4.9 Filed, not acted on

- `?$S1@?2??Handle@NetSearchResult@@…@4IA` — this new TU's local-static guard is
  still in raw `$S` form rather than `??_B`, i.e. an apparent additional pending
  row for the `guard` obj patcher. Metric-irrelevant here (retail's
  `0x82cc001c` is unnamed ⇒ forgiven), so it is filed for a tooling lane.
