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

---

## §5 MEASURED — prediction vs result

Applied as one commit (`aaff0752`). dtk rewrote `splits.txt` once on the first
build (the documented split-guard path — `.pdata` is derived output; recovery is
one build) and the second build was a fixed point, rc=0.

★ **dtk's own `.pdata` derivation independently corroborates the census.** Given
only my `.text` cut it split `0x82209000–0x82209158` at **`0x822090E0`**:
`0xE0` = 224 B = **28** RUNTIME_FUNCTIONs to `UI.cpp`, `0x78` = 120 B = **15** to
`NetSearchResult`. Exactly the head/tail counts I had derived from the retail
bytes, computed by a different instrument that knew nothing of my census.

### 5.1 Headline

| measure | baseline | measured | Δ | predicted Δ | hit |
|---|---|---|---|---|---|
| `matched_functions` | 43,558 | **43,568** | **+10** | +9 | **short by 1** |
| `matched_code` | 4,040,604 | **4,041,848 B** | **+1,244 B** | +1,196 B | **short by 48 B** |
| `matched_code_percent` | 39.43181 | **39.443947** | **+0.012137 pp** | +0.011672 | short |
| `total_functions` | 69,240 | **69,240** | **0** | 0 | **YES** |
| `total_code` | 10,247,068 | **10,247,068** | **0** | 0 | **YES** |
| `masked_equal_functions` | 23,080 | **23,081** | **+1** | +1 | **YES** |
| `fuzzy_match_percent` | 49.6996 | 49.710495 | +0.010895 | — | — |

`total_functions` and `total_code` both **exactly 0** confirms risk 1 from §4.8
did not fire: the cut at `0x823F56F8` landed on a real symbol boundary and dtk
did not re-carve a single row.

### 5.2 All 16 pre-registered rows hit 100 / 100

`default/network/net/NetSearchResult` is a **NEW unit at 15/15 rows and
1,396/1,396 bytes — 100.0%**. `?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ` reached
fuzzy 100 / mpn 100 at its true home `0x828023A0`. Every row in §4.6's table
landed where predicted, including the two funclets predicted to cross 99.x → 100
and the alias-forgiven `fn_823F5C68`.

### 5.3 Set-diff

`python3 tools/rowset_snapshot.py diff ~/tmp/rows_w16bn_base.json`

**CROSSED IN — 16 rows, 1,444 B**

| bytes | row |
|---|---|
| 264 | `NetSearchResult::??0NetSearchResult@@QAA@XZ` |
| 184 | `NetSearchResult::?Handle@NetSearchResult@@UAA?AVDataNode@@PAVDataArray@@_N@Z` |
| 148 | `NetSearchResult::??1NetSearchResult@@UAA@XZ` |
| 132 | `NetSearchResult::?Equals@NetSearchResult@@UBA_NPBV1@@Z` |
| 120 | `NetSearchResult::?Save@NetSearchResult@@UBAXAAVBinStream@@@Z` |
| 120 | `NetSearchResult::?Load@NetSearchResult@@UAAXAAVBinStream@@@Z` |
| 76 | `NetSearchResult::??_GNetSearchResult@@UAAPAXI@Z` |
| 72 | `NetSearchResult::?New@NetSearchResult@@SAPAV1@XZ` |
| **48** | **`SessionSearcher::?AllocateNetSearchResults@SessionSearcher@@QAAXXZ`** ← unpredicted |
| 44, 44, 40, 40, 40, 32 | the 7 funclets, re-homed |

**FELL OUT — 5 rows, 200 B:** `default/UI::fn_823F57B4` (44),
`fn_823F5BC0` (44), `fn_823F578C` (40), `fn_823F5C68` (40), `fn_823F5A68` (32) —
all five are the *same funclets* leaving `default/UI`, not losses. **NET +1,244 B.**

Only 5 of the 7 funclets appear as fall-outs because `fn_823F5B98` and
`fn_823F5BEC` were at 99.40 / 99.50 and so were never in the baseline
fuzzy-100 set.

The row arithmetic closes exactly: 16 in − 5 out = **+11 fuzzy rows**, but
`matched_functions` is **+10**, because `fn_823F5BEC` already had `mpn == 100`
(fuzzy 99.50) and merely changed unit — the documented `mpn` / `fuzzy` split
paying bytes without paying a function.

### 5.4 ★ Why I was short: an uncensused caller cascade

The one unpredicted row is
**`?AllocateNetSearchResults@SessionSearcher@@QAAXXZ`** (48 B, in
`default/network/net/SessionSearcher`). It **calls `NetSearchResult::New()`**.
While the map named `0x823f5c20` `?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ`,
that call site's relocation name disagreed with our source's
`?New@NetSearchResult@@SAPAV1@XZ` and was **charged**; naming the address
truthfully cleared the charge and the row crossed.

This is the documented **"a wrong name is financed by its callers"** mechanism
running in reverse — repairing a wrong map name pays at the call sites, not just
at the row. **My error was procedural, not evidential:** I verified the eight
bodies exhaustively and never asked *who calls the address I am renaming*.

★ **Rule for the next map lane: before pricing a rename, census the `bl` callers
of the old address and check whether our source already spells the new name.**
Each such caller is a candidate row, and the cascade is upside the direct
analysis cannot see. Being short is the safe direction, but it is still a miss.

---

## §6 Item 5 — sweep report (observations only, nothing acted on)

`default/UI` now holds 189 rows. Per remaining `.text` block:

| block | rows | bytes | at 100 | bytes @100 | note |
|---|---|---|---|---|---|
| `0x823F4A30–0x823F56F8` | 28 | 3,148 | 5 | 236 | **HEAD — MessageBroker; top candidate** |
| `0x828023A0–0x828024B0` | 4 | 252 | 4 | 252 | ALL 100 |
| `0x828024B0–0x82802530` | 2 | 120 | 2 | 120 | ALL 100 |
| `0x82802530–0x82802588` | 1 | 88 | 1 | 88 | ALL 100 |
| `0x82802588–0x828025B0` | 1 | 32 | 1 | 32 | ALL 100 |
| `0x828025B0–0x82802630` | 2 | 120 | 2 | 120 | ALL 100 |
| `0x82802630–0x828026A8` | 2 | 120 | 2 | 120 | ALL 100 |
| `0x828027E8–0x82802840` | 3 | 72 | 1 | 28 | mixed |
| `0x82802840–0x82802978` | 3 | 304 | 3 | 304 | ALL 100 |
| `0x82802978–0x82802F24` | 20 | 1,376 | 16 | 972 | mixed |
| `0x82802F28–0x82803030` | 4 | 260 | 1 | 124 | mixed |
| `0x82803030–0x828030B4` | 1 | 132 | 1 | 132 | ALL 100 |
| `0x828030B8–0x828030F8` | 2 | 56 | 1 | 36 | mixed |
| `0x828030F8–0x82803110` | 1 | 16 | 1 | 16 | ALL 100 |
| `0x82803110–0x828032E0` | 3 | 464 | 3 | 464 | ALL 100 |
| `0x828032E0–0x828033C8` | 1 | 220 | 1 | 220 | ALL 100 |
| `0x828033C8–0x82803494` | 1 | 204 | 1 | 204 | ALL 100 |
| `0x82803494–0x82804E00` | 51 | 6,376 | 32 | 2,640 | mixed (41%) |
| `0x82804E00–0x82806414` | 36 | 5,620 | 35 | 5,192 | mixed (92%) |
| `0x82B801D8–0x82B80320` | 3 | 316 | 1 | 40 | **suspicious — vendor address band** |
| `0x82B8032C–0x82B80FC4` | 20 | 3,172 | 7 | 232 | **suspicious — 7.3%, vendor band** |

**No block is ALL-ZERO**, so the "all rows fuzzy 0 with names clustering
elsewhere" pattern the brief asked me to look for does **not** occur in `UI.cpp:`
after this lane.

### 6.1 Candidates for a future lane, in priority order

1. **`0x823F4A30–0x823F56F8` — the HEAD, 28 rows / 3,148 B, only 236 B at 100.**
   MessageBroker's TU (constructor `0x823f50b0`; destructor `0x823f4fa8`, which
   stores both `.?AVMessageBroker@@` and `.?AV_DO_MessageBroker@Quazal@@`
   vptrs; `??_G` at `0x823f5270`). Only 1 of 18 MessageBroker vtable slots is
   inside the block, so the TU is larger than this span.
   ⚠ **This needs SOURCE, not a re-home:** `MessageBroker` has **zero** map rows
   and no header in our tree (Wii-only). Its 236 B at 100 is four 40 B funclets
   plus the 76 B **false** `??_GAutomator@@` credit.
2. **`0x82B8032C–0x82B80FC4` (20 rows / 3,172 B, 7.3% at 100)** and
   **`0x82B801D8–0x82B80320` (3 rows / 316 B)**. These sit in the
   `0x82B8xxxx` vendor/Quazal address band yet are pinned under `UI.cpp:`. That
   is the shape of a wrong-unit pin. **Not investigated** — out of this lane's
   bar and W16-BM owns other headings.
3. **`?AddCustomSettings@BandMatchmaker@@QAAXPAVMatchmakingSettings@@W4CustomSettingsType@1@@Z`**
   — 784 B at fuzzy 99.89796 / mpn 99.89796 in `default/Matchmaker`. A large
   size-if-it-crosses row behind what looks like a single charged site.

### 6.2 Two of the brief's own candidates are REFUTED

The brief flagged `0x82652090 ?ReadStats@MatchmakerPoolStats@@` and
`0x826527f0 ?HasCompatibleInstruments@BandMatchmaker@@` as possibly pinned
outside their headings. **Both are correctly homed and already at fuzzy 100** in
`default/Matchmaker` (MatchmakerPoolStats 3/3 rows at 100; BandMatchmaker 13/14).
There is nothing to move.

### 6.3 No `NetSearchResult` / `SessionSearcher` row is mis-homed

| class | map rows | where they live |
|---|---|---|
| `NetSearchResult` | 17 | 8 in the new unit (1,116 B, **8/8 at 100**), 7 in `SessionSearcher` (548 B, **7/7 at 100**), 2 in `Matchmaker` (632 B, **2/2 at 100**) |
| `SessionSearcher` | 13 | all 13 in `SessionSearcher` (1,800 B, **13/13 at 100**) |

The 9 `NetSearchResult`-named rows outside the new unit are **not** mis-pins —
they are callers and `vector<NetSearchResult*>` helpers whose *mangled names*
mention the class while the functions genuinely belong to those TUs. All are at
100. **Nothing to move.**

---

## §7 What I did NOT do, and why

- **Did not rename `0x823f5270`** (`??_GAutomator@@UAAPAXI@Z`, 76 B, a **proven
  false credit** — it is MessageBroker's vtable slot 0). Its true name
  `??_GMessageBroker@@UAAPAXI@Z` is emitted by **no object of ours**, so the
  rename is a certain **−76 B** installing nothing and creating a permanently
  unpairable row. Precondition (b) of the W16-BL rule fails. Verified still at
  fuzzy 100 after this lane, i.e. deliberately left as known-false credit.
  **What would change the verdict:** porting a `MessageBroker` header/TU that
  emits `??_GMessageBroker@@UAAPAXI@Z`. Then the rename is +76 B, not −76 B.
- **Did not move the HEAD span** `0x823F4A30–0x823F56F8`. Its destination heading
  is neither `UI.cpp:` nor the new `NetSearchResult.cpp:`, which is outside this
  lane's splits domain, and **W16-BM is live** and may touch any other heading's
  `.text`. Filed in §6.1 instead.
- **Did not touch the two vendor-band blocks** `0x82B801D8–0x82B80320` and
  `0x82B8032C–0x82B80FC4` despite their being the most suspicious rows in the
  unit — same domain bound. Filed, not moved.
- **Did not act on the `$S`→`??_B` guard observation** (§4.9). It is
  metric-neutral by construction here and belongs to a tooling lane.
- **Did not census the `bl` callers of `0x823f5c20` before predicting** — this is
  the omission that caused the §5.4 miss, recorded as a miss rather than
  absorbed.
- **Did not byte-diff `0x828023A0`'s 72 B against retail.** I verified only that
  `UI.obj` *defines* `?NewObject@UIScreen@@` with matching geometry
  (8 + 72 + 40 = 120). It reached 100, so the shortcut was harmless — but it was
  a shortcut, and had it missed, the prediction would have been long by 72 B.
- **Did not run `ab_measure`.** This is a map+splits transaction measured against
  a committed pre-registration on one settled tree, with `total_code` /
  `total_functions` unmoved as the neutrality control; a two-leg A/B would add
  cost without adding a control this lane lacked.

---

## §8 Gates

| gate | result |
|---|---|
| full `./tools/ninja-locked` | **rc=0** (`~/tmp/rb3_build_w16bn_2.log`; run 1 rc=1 was the documented split-guard rewrite) |
| `scripts/verify_ruler_agreement.py --check` | **rc=0** — "both objdiff-cli entry points resolve the same ruler" |
| `scripts/verify_objs_patched.py --verify-manifest` | **rc=0** — 1,216 decomp + 3,114 target objects match, `tree_sha256=c797628ddedee41c`; denylist OK |
| `tools/icf_alias_finder.py --validate` | **rc=0** — PASS: 1,404 map-consistent, 247 tolerated, **0 contradicted**, 1,652 total |
| `tools/funclet_homing.py --validate` | **rc=0** — PASS: 24,221 HOMED, fan-in uniformly 1 |
| `tools/native_build_gate.sh` (**LAST**) | **rc=0** — verbatim: |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0` as required (`rb3-milo` and `rb3-render` both relinked this run, so
the three engine targets really were exercised — the SKIP class that made a
worktree gate structurally unable to test them did not occur). The only edit
made after the gate ran is this `docs/*.md` file, which cannot enter
`ScatterIncludes.cmake`'s scan of `src/`.

---

## §9 Commits on `w16-bn`

| sha | what |
|---|---|
| `22653f66` | `network/net`: port `NetSearchResult.cpp` from the rb3-Wii oracle (52 → 71 lines) + `objects.json` wiring. **Measured Δ0** as a control. |
| `cab417ad` | docs: **pre-registered** prediction, committed before the transaction and before any measuring build |
| `aaff0752` | map + splits: the coupled transaction — **+10 fns / +1,244 B** |
| (this commit) | docs: measurement, set-diff, sweep, gates |

## §10 Ledger

**Not booked by this lane, deliberately.** `docs/decomp/progress_ledger.jsonl`
keys each entry's `snapshot_id` on the **merge** commit (W16-BL's entry was
written on `main` at `7320abd5` for merge `8c830054`), which does not exist for
an unmerged branch. Booking it here would require inventing a snapshot id.
Coordinator step after `git merge --no-ff w16-bn`:

```
python3 tools/progress_ledger.py     # from main, after the merge
```

Figures to book: **43,568 fns / 4,041,848 B / 39.443947 % / 69,240 fns /
10,247,068 B**, Δ **+10 / +1,244 B / +0.012137 pp** vs `7320abd5`.
