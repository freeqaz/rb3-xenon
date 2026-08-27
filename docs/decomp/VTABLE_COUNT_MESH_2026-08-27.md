# Vtable COUNT wave — `DxMesh`, `Server`, `StreamReader` (lane MESHCOUNT, 2026-08-27)

Companion to `VTABLE_SLOT_COUNT_FIXES_2026-08-20.md` (§14 introduced the COUNT
instrument, §16a the read-the-BODIES move, §16b the thunk-is-its-branch-target
test). **Deliberately a separate file** — four lanes have already collided on
section numbers in that one. Nothing here renumbers anything there.

Lane brief: three `ours > retail` count disagreements from a coordinator sweep at
`24ef42e9`. All three reproduce. **Two are fixed, one is reported UNDERDETERMINED
with the evidence that would settle it.**

## 0. Headline — the brief's difficulty ordering was INVERTED

The brief called `DxMesh` "the hardest of the three" (five vtables, and secondary
subobject tables are where the RTTI-owner test passes trivially) and implied
`Server`/`StreamReader` were the settleable ones. Measured:

| class | brief said | actually |
|---|---|---|
| `DxMesh` | hardest, may be unresolvable | **EASIEST** — the mismatch is on the PRIMARY (`COL.offset=0`) table, joined by offset, tail pair unambiguous |
| `StreamReader` | +1 | **SETTLED** — two independent instruments |
| `Server` | +1 | **UNDERDETERMINED** — the disputed range is never dispatched anywhere in retail |

⚠ The multi-vtable warning was real but **aimed at the wrong risk**. DxMesh's five
tables are a hazard for *joining* the right two, and `our_vtable_by_offset()`
already solves that by joining RTTI offset to RTTI offset. Once joined, a
multi-vtable class is no harder than a single-vtable one. What actually made a
class hard was something the brief did not mention at all: **whether retail ever
CALLS the disputed slots.** `Server`'s do not exist as call sites, so every
name-, body- and call-site instrument is simultaneously blind.

⚠ Also: the brief's addresses for two of the three are wrong (`StreamReader`
`0x82170f9c` → really **`0x8219711c`**; `DxMesh` `0x82101b14` is right but is the
5th of five tables, not "one of them" ambiguously). **The COUNTS reproduced
exactly**, which is the part that mattered. Do not key future work on a relayed
address.

## 1. `StreamReader` — 6 retail vs 7 ours; `Init()` is surplus. FIXED

### 1a. The count, and why it cannot be poisoned

`??_7StreamReader@@6B@` @ **`0x8219711c`**, `COL.offset=0`, the class's **only**
vtable. Six slots:

| slot | VA | shape |
|---|---|---|
| 0 | `0x82b6a398` | deleting dtor, real prologue |
| 1–5 | `0x828299b8` ×5 | `_purecall` |

⇒ retail declares **five** pure virtuals. We declared six.

★ **The map names NOT ONE of these slots.** That is what makes this read immune to
the two failure modes that have cost this doc's earlier waves real budget: there
is no name to be wrong (§12's map-defect class) and no fold-survivor spelling to
mis-attribute (§16b). The instrument is the table's LENGTH plus the bodies.

★ **The bound is hard, not heuristic.** `0x82197134` — the word immediately after
slot 5 — holds a pointer to the `.?AVXMAReader@@` Complete Object Locator, i.e.
the *next* vtable begins at `0x82197138`. Zero slack. The reader did not stop on a
"looks like it ran out" guess.

### 1b. Which of the six is surplus — refuting every rival, on bodies

Names cannot localise it (every disputed slot is `_purecall` on both sides), so
per §16a: read the derived classes' BODIES.

`VorbisReader` @ `0x821a1a34`, 9 slots, aligns one-for-one with our 9:

| slot | VA | first words | reading |
|---|---|---|---|
| 3 | `0x8276f6f8` | `stb r4,0x3c(r3); blr` | a setter **taking an argument** ⇒ `EnableReads(bool)` |
| 4 | `0x82bb15e8` | `lbz r3,0x45(r3); blr` | bool getter ⇒ `Done()` |
| 5 | `0x82bb15f0` | `lbz r3,0x11c(r3); blr` | bool getter, **different field** ⇒ `Fail()` |
| 6 | `0x82bb2a08` | `lwz r5,0x2c(r3); li r7,-1; …` | substantial ⇒ `Init()` |
| 8 | `0x826c3888` | bare `blr` | empty body ⇒ `virtual void EndData() {}` |

Now test each rival hypothesis rather than assert the conclusion. MSVC lays out
the base's virtuals first, then the derived's NEW ones in declaration order, and
`VorbisReader` declares `Fail` (line 19) long before `Init` (35). So if any of
Poll/Seek/EnableReads/Done/Fail were the missing base virtual, that method would
become a *new* `VorbisReader` virtual and land **after** `Init`:

| hypothesis | forces slot 5 = | forces slot 6 = | retail says |
|---|---|---|---|
| `Init` surplus | `Fail` (`lbz`) | `Init` (substantial) | ✅ matches |
| `Fail` surplus | `Init` (substantial) | `Fail` (`lbz`) | ❌ inverted |
| `Done` surplus | `Init` | `Done` | ❌ inverted |
| `EnableReads` surplus | — slot 3 would be `Done()`, a **no-argument** bool getter | | ❌ slot 3 stores `r4` |
| `Seek`/`Poll` surplus | — slot 2 would be `EnableReads` (`stb`) | | ❌ slot 2 is `mflr;bl;stwu` |

Only "Init surplus" survives.

### 1c. The decisive check — a concrete derived class whose table is too short

`XMAReader` @ `0x82197138` is a **concrete** reader that explicitly declares
`virtual void Init();`, and its retail table is **six slots**:

| slot | VA | shape | reading |
|---|---|---|---|
| 0 | `0x82b6a9c0` | real prologue | deleting dtor |
| 1 | `0x82b6aeb0` | `mflr; bl; …` | `Poll` |
| 2 | `0x82b6a738` | `lwz r7,0x34(r3); lwz r11,0x38(r3)` | `Seek` |
| 3 | `0x826c3888` | bare `blr` | **empty** `EnableReads(bool)` |
| 4 | `0x82b6a3e0` | `lbz r3,0x72(r3); blr` | `Done` |
| 5 | `0x823591e8` | `li r3,0; blr` | `Fail` — returns false |

Hard bound again: slot 6 would be `0x82197150`, which holds `0xffffffff` and is
**not an image VA at all**.

⇒ every slot is spoken for and **there is no slot left for `Init`**, which there
would have to be if `StreamReader` declared it. This is a LENGTH argument on a
class that *must* fill every slot, so it is independent of 1b's shape argument.

### 1d. Applied

`src/system/synth/StreamReader.h` — `Init()` moved under `#ifdef HX_NATIVE`.

⛔ **It cannot simply be deleted.** `milo-native-engine`'s
`src/platform/FFmpegAudioReader.h` marks `Init()` **`override`**, so deleting the
base declaration breaks the native link — *precisely* the failure class the
matching build is structurally incapable of catching (it compiles `src/`; the
native targets link a superset). `HX_NATIVE` appears **0 times in `build.ninja`**
and the match cflags are exactly `/D_XBOX360` + `/DCURL_STATICLIB`, so the guard
is inert for matching and live for native. Same idiom as
`os/AsyncFile.h`'s `GetFileHandle`.

Three-consumer check (§15c): **zero** `StreamReader *`-typed `Init()` call sites,
**zero** map rows for any of these slots, **zero** alias groups.

### 1e. Corollary — `XMAReader::Init` was ALSO wrong, independently

1c proves retail's `XMAReader` has six slots and no `Init`. Our `XMAReader.h`
declared seven. ⚠ Note this is **not** created by the 1d fix and is not fixed by
it either: `Init` was an override of `StreamReader::Init` before (slot 6) and
would have become a new tail virtual after (slot 6) — **7 slots either way**. It
is a pre-existing, separate defect that only became visible because the count
instrument was pointed at the base.

Also guarded under `HX_NATIVE`. **Metric-neutral by construction**: `XMAReader`
has no `.cpp` and no `objects.json` entry, so our build emits no `XMAReader`
vtable at all (the sweep reports `no_vtable` for our side). Declaration accuracy
only — recorded so a later lane does not re-find a 7-slot `XMAReader` and assume
this lane missed it.

## 2. `DxMesh` — 16 retail vs 18 ours; `NumFaces`/`NumVerts`. FIXED

### 2a. Five tables, and the one that matters

| vtable | `COL.offset` | subobject | retail | ours (before) |
|---|---|---|---|---|
| `0x82101a9c` | 420 | `RndHighlightable` | 1 | 1 |
| `0x82101aa4` | 332 | `DxObject` (bare `??_7DxMesh@@6B@`) | 2 | 2 |
| `0x82101ab0` | 36 | `RndTransformable` | 1 | 1 |
| `0x82101abc` | 376 | `Object@Hmx` | 21 | 21 |
| **`0x82101b14`** | **0** | **`RndDrawable` (primary)** | **16** | **18** |

The disagreement is on the **primary** table, joined offset-to-offset through both
sides' own `??_R4`. The brief's concern — that a secondary subobject table defeats
the RTTI-owner test — does not arise here.

### 2b. Slots 0–15 align one-for-one, by body, with no map name involved

Every `MAP` column is empty for all 16 slots.

* `[3]` = `0x823591e8` (`li r3,0; blr`) ⇒ `CamOverride() { return 0; }`
* `[6]`, `[10]`, `[11]` = `0x826c3888` (bare `blr`) ⇒ the three empty `void`
  virtuals `ListDrawChildren` / `DrawPreClear` / `UpdatePreClearState`
* `[5]`, `[14]`, `[15]` are referenced by **exactly one** vtable each (occ 1) ⇒
  genuine DxMesh-specific overrides: `DrawShowing`, `DrawFacesInRange`, `OnSync`
* `[0] [1] [2] [4] [7] [8] [12] [13]` are referenced by **exactly two** (occ 2) ⇒
  `RndMesh`'s own bodies, shared by `RndMesh`'s table and `DxMesh`'s

★ The occ split is the load-bearing part: it separates "inherited from RndMesh"
from "overridden by DxMesh" **without consulting a single name**, and it agrees
with our declaration in all 16 positions.

Surplus = the tail pair `?NumFaces@DxMesh@@UBAHXZ` / `?NumVerts@DxMesh@@UBAHXZ`.
Hard bound: slot 16 would be `0x82101b54` = `0xfffffffc`, not an image VA.

### 2c. ★ The base class was already fixed and the DERIVED class leaked it

`src/system/rndobj/Mesh.h:183` already carries a prior lane's finding —
*"Retail X360 RB3 keeps `NumFaces()`/`NumVerts()` NON-VIRTUAL; they are DC3-only
vtable slots"* — proven there by a **completely different instrument** (three
vcall-displacement anchors on `RndMesh`'s own slice: `SaveVertices` at `0x34`,
the DrawFaces slot at `0x38`, and a uniform −8 on `OnSync` across seven
functions). That lane introduced `MESH_DC3_VIRTUAL`.

But `src/system/rnddx9/Mesh.h:27-28` still said bare `virtual`. Because the base's
are non-virtual in the match build, the derived declarations stopped being
*overrides* and became **two brand-new virtuals appended to DxMesh's tail** —
exactly slots 16 and 17.

⇒ **Two independent instruments, on two different classes, agree.** And the
generalisable lesson: **de-virtualizing a base method is not complete until every
derived re-declaration is swept** — otherwise the slots come back at the tail of
each derived class, where the base's own vcall-anchor instrument cannot see them.
Worth a mechanical check across the other `*_DC3_VIRTUAL` macros
(`DRAW_DC3_VIRTUAL` in `rndobj/Draw.h` has the same shape). **NOT done here.**

### 2d. Applied

`src/system/rnddx9/Mesh.h` — both switched to `MESH_DC3_VIRTUAL`, the in-tree
house macro (`virtual` under `HX_NATIVE`, empty otherwise).

Behaviour-preserving: `RndMesh`'s are already non-virtual, so a `RndMesh *` call
was already static; a `DxMesh *` call now resolves statically to the same
`DxMesh::NumFaces` by name hiding. There are **no** `NumFaces()`/`NumVerts()` call
sites in `rnddx9/` at all, so there is no codegen side-effect to price.

## 3. `Server` — 19 retail vs 20 ours. COUNT PROVEN, SURPLUS UNDETERMINED

### 3a. What IS proven

`Server` has two tables. The `MsgSource` subobject (`0x82057764`, `COL.offset=112`)
is **21 vs 21 — no defect**. The primary (`0x820577bc`, `COL.offset=0`) holds
`Server`'s own new virtuals: **19 retail vs 20 ours**.

Corroborated by a second class: retail's **`XboxServer`** (found via RTTI base-class
descriptors — the only image class deriving from `Server`) has a `COL.offset=0`
table that is **also 19 slots**, i.e. `XboxServer` introduces no new virtuals.

Slots 0–7 are pinned by two independent instruments:

* `[5]`/`[6]` are the **same VAs** in the `Server` and `XboxServer` tables
  (`0x823ec500`, `0x823ec518`) with occ **exactly 2** — precisely those two
  tables. Bodies `lwz r11,0x3c(r3); addi r11,r11,-2` and `…,-1` ⇒
  `IsConnected()` (`mLoginState == 2`) and `IsLoggingIn()` (`== 1`).
* `[7]`: `XboxServer`'s override at `0x823ec728` **dispatches slot 5 as an
  immediate** — `lwz r11,0x14(r11)` — tests the result as a bool
  (`rlwinm. r11,r3,0,24,31`), returns 0 if false, and saves an incoming `r4`.
  ⇒ "takes an argument; if `!IsConnected()` return 0" = `GetPlayerID(int)`.

★ **And the call-site instrument independently confirms it, with a control that
could have failed.** `TourBand::GetBandID` (`0x82b791a0`, one of the very few
`Server` methods the map names) reads:

```
82b791bc  lwz  r11,0xeb50(r30)   ; TheServer  (the global lives at 0x82c7eb50)
82b791c0  lwz  r31,0(r11)        ; its vptr
82b791c4  bl   <GetPadNum>
82b791c8  lwz  r11,0x1c(r31)     ; 0x1c/4 == SLOT 7
82b791cc  mr   r4,r3             ; the padnum argument
82b791d8  bctrl
```

`0x1c` is an immediate in retail's own machine code — not a relocation, not a
name, not poisonable by ICF. Any other displacement would have refuted the
slot-7 identification outright.

### 3b. ⛔ Why the surplus CANNOT be localised — the disputed range is DEAD CODE

Slots 8–18 are twelve interchangeable fold hubs (`li r3,0; blr` ×11 plus one bare
`blr`) against **thirteen** methods in our header. Every instrument fails, and
they fail for one shared reason.

Using the `TheServer` global recovered above (`0x82c7eb50`), a scan of the whole
`.text` for every load of it followed by a virtual dispatch finds **23 sites**:

| slot dispatched | sites |
|---|---|
| 0 (`Init`) | 3 |
| 1 (`Terminate`) | 1 |
| 5 (`IsConnected`) | 4 |
| 7 (`GetPlayerID`) | 3 |
| **8–18** | **0** |

⇒ **retail never calls anything in the disputed range.** That single fact explains
every other blindness at once: it is *why* all twelve bodies were emitted as
`return 0` stubs and folded onto two hub addresses, and *why* the map names none
of them. Names are absent, bodies are identical, and call sites do not exist.
Removing one of our thirteen would be a **1-in-13 guess**. Not done — §13e/§15g
precedent: specify what would settle it, do not guess.

### 3c. A separate, real defect this exposed — retail slot 8 returns `void`

Retail `[8]` is `0x826c3888`, the **bare-`blr`** hub — a function returning
nothing. `[7]` and `[9]`–`[18]` are all `li r3,0; blr`. **All thirteen** of our
methods are declared with a non-`void` return type, so **no** count hypothesis can
put a void-returning method at slot 8:

| if surplus is… | slot 8 becomes | declared |
|---|---|---|
| `GetCustomAuthData` (last) | `GetFriendsClient` | `int` |
| `GetFriendsClient` | `GetMessagingClient` | `int` |
| `GetPlayerID` | `GetMessagingClient` | `int` |

⇒ this is an **independent return-type defect**, orthogonal to the count — and our
own header already suspects it (`// fix all of these return types`, line 29). One
of our `Server` virtuals should be `void`. Recorded, not guessed.

⚠ Note also that `../rb3` (rb3-Wii) has **no `Server` at all**, so this header was
hand-written rather than ported. It has no oracle in either sibling repo.

### 3d. What would settle `Server`

`XboxServer` overrides four of the disputed slots with real bodies, and
identifying **any one** of them collapses the whole alignment (order is fixed):

| slot | body | needs |
|---|---|---|
| 11 | `lwz r3,0x80(r3); blr` | which Quazal client pointer lives at `+0x80` |
| 13 | `lwz r3,0x84(r3); blr` | …at `+0x84` |
| 14 | `lwz r3,0x88(r3); blr` | …at `+0x88` |
| 15 | `lwz r3,0x7c(r3); b 0x82a89ff8` | **identify `0x82a89ff8`** — a Quazal function in the `/Od` NetZ block |

Identifying slot 15 alone splits the space: if it is `GetAccountManagementClient`
the surplus is after index 15 (so `GetMasterProfileID`/`CreateProfile`/
`DeleteProfile`/`GetCustomAuthData`); if it is `GetMasterProfileID` the surplus is
at or before 15. That is a **Quazal-RTTI identification lane**, not a vtable lane.

## 4. Measurement

Pre-registered before running: **Δmatched_functions 0, Δcode_bytes 0, 0 units
falling off 100% on either ruler.** Vtables are `.rdata` and are not scored rows;
`mpn` is arg-blind. Δ0 is the SAFETY CHECK, not the payoff — the payoff is that a
call through a wrong slot index no longer dispatches to the wrong method in the
native runtime. Per the standing directive, **accuracy beats headline %**.

`python3 tools/ab_measure.py --worktree /home/free/tmp/wt-meshcount --from-dirty`,
all three edits in one patch (they are independent, but all three are predicted
`Δ0`, so bundling costs no attribution — and the per-unit set-diff below would
localise any movement anyway):

| measure | leg A | leg B | Δ |
|---|---|---|---|
| `matched_functions` | 42,252 | 42,252 | **+0** |
| `masked_equal` | 22,912 | 22,912 | +0 |
| honest (`matched − masked`) | 19,340 | 19,340 | +0 |
| `matched_code_percent` (**`name_check`**, the shipped/graded ruler) | 36.807613 | 36.807613 | **+0.000000 pp** |
| `matched_code` bytes | — | — | **+0 B** |
| `fuzzy_match_percent` | 48.912700 | 48.912700 | +0.000000 pp |
| **`none` ruler** (opt-in control) | 44,485 / 43.159935 | 44,485 / 43.159935 | **+0 / +0.000000 pp** |
| units at 100% — `mpn` ruler | 150 | 150 | +0 — **0 reached, 0 FELL OFF** |
| units at 100% — all-rows-`fuzzy` ruler | 122 | 122 | +0 — **0 reached, 0 FELL OFF** |

**Prediction hit exactly on both rulers.** Pairable units 1,731 → 1,731.

★ The run is *not* vacuous: leg B performed **318 real MSVC recompiles** and 6
patch steps, and `ab_measure` asserted that before scoring — so this is a
measured `Δ0`, not an absent-vs-absent one. Both legs settled to a zero-work
build (2 iterations each), `objdiff-cli` sha256 pinned stable across legs, and
the tree was restored to its pre-run state.

⚠ The `[control none] NOT_APPLICABLE` line is correct and expected here: the
alias/ALIAS_SUSPECT shape test is only adjudicable on a **map-only** patch, and
this patch is `kinds=['source']`. No map row and no alias group was touched by
this lane at all, so that hazard class does not arise.

## 5. Deliberately NOT done

* **`Server`'s surplus virtual** — left in place. 1-in-13 with no discriminating
  instrument (3b). §14c's precedent is that guessing here costs real matched
  bytes and a revert.
* **`Server`'s void-returning slot 8** (3c) — identified as a real defect, not
  repaired; it needs the same Quazal identification as 3d.
* **Sweeping the other `*_DC3_VIRTUAL` macros for the same derived-class leak**
  (2c). `DRAW_DC3_VIRTUAL` in `rndobj/Draw.h` has the identical shape and
  plausibly the identical leak, but it is a separate wave and would have
  destroyed this one's attribution.
* **Renaming/repairing any map row.** Nothing here touches
  `target_symbol_map.json` or `symbol_aliases.json`, so the ALIAS_SUSPECT /
  un-pairing hazards do not apply and the `none`-ruler control is meaningful.
* **The `DxMesh` secondary tables** — all four already agree (2a); not touched.
