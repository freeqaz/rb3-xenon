# Lane W10-B — the allocator debug-overload stratum: it was never the blocker

**Branch** `w10-allocator-stratum` · worktree `~/tmp/wt-w10-b` · base `dee126a1`
(`git merge-base --is-ancestor dee126a1 HEAD` asserted before the first edit).
Ruler: **`name_check` (graded)**, read from `report.json`'s
`provenance.diff_config`, never assumed.

Baseline after this lane's mandatory first full build (a reflinked worktree's
target objs are pre-renamer, so every mangled-name lookup reads "absent" until
the renamer's pre-compile step has run — FOLDPROVE-2):

```
matched_functions   42,749      masked_equal        22,980
matched_code     3,871,308 B    total_code      10,245,956
matched_code_percent 37.783768  total_functions     69,219
fuzzy_match_percent  49.134560
verify_objs_patched --verify-manifest -> OK: 1205 decomp, 3085 target objects
```

---

## 0. The headline, and it is a correction

W9-B handed this lane the **allocator debug-overload stratum** as the thing
blocking `list<char*>::insert` and, through it, W8-D's 904 B `FileRelativePath`.
It sized it as "a separate and much larger change" that "blocks *everything*
underneath the STL container layer".

> ★★★★★ **There is no allocator blocker. The chase trace was misread.** The
> `SLOT-REFUTED` frames W9-B recorded at the two allocator levels are not
> independent refusals — they are the **single leaf failure propagating back up
> the recursion, one frame per stack level**. Read bottom-up, the leaf is
> `VACUOUS ??2CriticalSection@@SAPAXI@Z / ??2ChunkAllocator@@SAPAXI@Z`: an
> 8-byte `operator new` thunk that trips `vacuous()`'s `size < MIN_WORDS*4`
> (16 B) guard. **Arity never entered into it.**

And the refusal is maximally empty. At that leaf the target and our COMDATs are
identical **in every byte** (`bodyhash 9d95dd`) *and* their single relocation
names `?MemAlloc@@YAPAXHH@Z` **on both sides**. There is nothing left to
disagree about. A vacuity guard is a claim about *what masking hides*; it does
not apply when nothing is masked.

That one-line insight unblocked both items W9-B and W8-D deferred.

| question | verdict |
|---|---|
| **Retail's allocator signatures** | `MemOrPoolAlloc` takes **1** argument; `PoolAlloc` takes **2**. Both map names (4-arg, 5-arg) are **CONTRADICTED**. A discriminating witness **exists** — §1. |
| **Source or map?** | **MAP.** Our source already spells the stripped forms; `src/` does not move in this lane. §2. |
| **W9-B's `list<char*>::insert`** | **PROVEN and INSTALLED** — +1020 B / +2 fns. §3. |
| **W8-D's 904 B `FileRelativePath`** | **COLLECTED** — the row is at `100.00000`. §3. |

Net for the lane: **+1864 B / +5 functions** across four measured changes
(`matched_code` 3,871,308 → 3,873,172 B; `matched_functions` 42,749 → 42,754).

---

## 1. Retail's allocator signatures — the witness that discriminates

### 1.1 W9-B was right that the callee cannot settle it

Retail `0x827BD208` reads only `r3`. That **looks** like it refutes the 4-arg
name, and W9-B correctly withdrew the claim rather than bank it: a
release-stripped 4-arg form that discards its three debug arguments is
**byte-identical** to a 1-arg form, so both candidate classes satisfy that
witness.

Our COMDAT comparison cannot settle it either, and this is worth stating
explicitly because it *looks* like evidence: our 1-arg `MemOrPoolAlloc(int)` and
retail's body at `0x827bd208` are the same 36 bytes with the same two relocation
offsets and types (`bodyhash d8d71c` on both sides). Identical bodies are
exactly what **both** hypotheses predict.

### 1.2 The call sites can, for two reasons specific to this build

**The discriminating witness is the CALL SITE, not the callee**, because:

* the debug arguments are **compile-time constants** — a `__FILE__` pointer, a
  line number, a name pointer. A constant cannot be "already live" in `r4`
  across hundreds of independent call sites in different functions, the way a
  *dynamic* argument can. This is not hypothetical: `MemOrPoolFreeSTL` **is**
  2-arg, and 985 of its 1,759 sites do not write `r4` in-window, because its
  `r4` is the pointer being freed and is often already there. Constants have no
  such escape.
* this build has **no LTCG** (CLAUDE.md, verified on the Rich header), so no
  cross-TU pass can elide arguments a declaration demands. A 4-arg declaration
  **forces** every caller in every other TU to materialize `r4`/`r5`/`r6`.

**Measured over retail `.text`** (`orig/45410914/band.exe`, `.text` @
`0x82270000`, `0x9DCE3C` bytes), walking back from each `bl` without crossing a
branch:

| address | map name | map arity | measured call-site arity | |
|---|---|---:|---|---|
| `0x827bcd38` | `?MemAlloc@@YAPAXHH@Z` | 2 | **2** — `r3`+`r4`, `li` ×296 | ✓ |
| `0x827bca50` | `?MemOrPoolFreeSTL@@YAXHPAX@Z` | 2 | **2** | ✓ |
| `0x827badb0` | `?PoolFree@@YAXHPAX@Z` | 2 | **2** — 131/131 | ✓ |
| `0x827bcff0` | `?_MemAllocTemp@@YAPAXHH@Z` | 2 | **2** | ✓ |
| `0x827bd2f0` | `??2CriticalSection@@SAPAXI@Z` | 1 | **1** | ✓ |
| **`0x827bd208`** | `?MemOrPoolAlloc@@YAPAXHPBDH0@Z` | **4** | **1** — 401/403 sites write only `r3`; `r4`/`r5`/`r6` written at **zero** | ⛔ |
| **`0x827bb0e8`** | `?PoolAlloc@@YAPAXHHPBDH0@Z` | **5** | **2** — 263/267 write `r3`+`r4`, `r4` via `li` ×265 | ⛔ |

★ **Five of seven map names reproduce their arity exactly, and the two
contradicted are precisely the two the chase chain bottoms out on.** That is the
control W9-B's own §2.2 demands: a method that returns "the map is wrong" for
every input is indistinguishable from a method that works. This one returns
"right" five times.

### 1.3 The scanner is proven able to SEE the debug pattern

A negative is worthless without showing the instrument can produce a positive.
Retail **does** contain live debug-instrumented allocator calls, in the Quazal
stratum, which kept its instrumentation:

```
0x823f3e44  lis  r11,0x8206
0x823f3e48  li   r5,11                 <- LINE
0x823f3e4c  addi r4,r11,-27928         <- __FILE__  ".\MessageBrokerDDL_Xbox.cpp"
0x823f3e50  li   r3,136                <- size
0x823f3e54  bl   0x82a99170            <- alloc(size, file, line)
```

The scanner finds that shape readily. It finds **none of it** at
`MemOrPoolAlloc`'s 403 sites.

★ A **self-calibrating noise floor** falls out of a deliberately over-permissive
variant (40-instruction window, crossing branches, counting any constant landing
in an argument register). A *genuine* argument reads ~100%: `PoolAlloc`'s `r4`
is **267/267**. `PoolAlloc`'s own **non**-arguments `r5`/`r6`/`r7` read
44%/26%/31% — that is the noise floor. `MemOrPoolAlloc`'s `r4`/`r5`/`r6` read
29%/17%/9%, **below** the noise floor and nowhere near the ~100% a real argument
shows.

### 1.4 Second, independent witness: the strings do not exist

If the debug arguments were live the image would carry the literals. Searching
all 14 MB in Python (`grep` here is binary-blind):

| pattern | hits |
|---|---:|
| `MemMgr.cpp` | **0** |
| `MemOrPool` | **0** |
| `PoolAlloc` | **0** |
| `MemAlloc` | **0** |
| `.cpp\0` | **279** |

★ The last row is the anti-vacuity control: the search **can** find `__FILE__`
literals — 279 of them (`.\Validator.cpp`, `.\MessageBrokerDDL_Xbox.cpp`,
`e:\xenon\xdk-main-feb10\...`) — from the Quazal and XDK strata. The absence at
the Milo allocators is real, not a broken search.

⇒ **`MemOrPoolAlloc` is 1-arg; `PoolAlloc` is 2-arg. A discriminating witness
exists, and it is the call site rather than the callee.**

---

## 2. Source or map? — MAP, and the tree had already half-said so

`src/system/utl/MemMgr.h` already declares the stripped forms for the match
build (`void *MemOrPoolAlloc(int size);`), with the 4-arg debug form behind
`#ifdef HX_NATIVE`. **`src/` does not move in this lane**, so the native gate is
not applicable.

★ Corroboration found *after* the measurement, which is the only order in which
it counts: `tools/gen_symbol_alias_map.py`'s own docstring (lane ALIAS-X2,
2026-08-13) already says

> *"There are not two bodies here for the linker to fold … it cannot be a 5-arg
> function at all. What these two founding groups actually repair is a **MAP
> NAMING defect — a debug-build spelling parked on a retail address** — not a
> fold."*

So the verdict was already the tree's position. What it lacked was
*discriminating* evidence: its stated reason ("retail's body reads r3 only") is
the very witness W9-B showed cannot separate the two hypotheses — and it is also
imprecise, since `PoolAlloc`'s body demonstrably reads `r4` too (its call sites
set it 267/267). §1 supplies the evidence that reason was missing.

⚠ And the map was never actually **repaired** — the alias papered over it. §4's
C2 repairs it.

---

## 3. The two blocked items, re-tested

### 3.1 The chase, with the leaf resolved

`tools/icf_pair_adjudicate.py` now accepts a vacuous leaf **iff** the bodies are
byte-identical *and* every relocation agrees on offset, type **and target name**
— which is `/OPT:ICF`'s own condition stated exactly. It is reported as
`VACUOUS-BUT-IDENTICAL` so it is auditable, mirroring how `CYCLE-ASSUMED` is
reported rather than hidden.

Controls, all re-run after the change:

| control | result |
|---|---|
| `--selftest` | **PASSED** (positive PROVEN, negative REFUTED) |
| `--chasetest` in-family decoy | **still REFUTED** — the discriminator survives |
| identical masked body, **differing** reloc name (`?clear@VertVector@RndMesh@@` vs the CS thunk) | **still REFUTED**, plain `VACUOUS` — proves the *new branch itself* can fail |

The third matters most: **90 of our 8-byte thunks share the CS thunk's masked
body with a different relocation target**, so requiring name equality is doing
real work, not decoration.

Result: `insert<Object*>` ↔ `insert<char*>` is **`CHASED T1: PROVEN`**, every
intermediate level `SLOT-FOLD-OK`.

### 3.2 The population control, re-derived rather than inherited

W9-B's §5 says to re-check its §3.2 figures first. Done, over
`.pdata`-authoritative extents:

```
list<T>::insert-shaped bodies (100 B / 25 instr / exactly 1 bl):  235
  distinct _M_create_node targets among them                   :  149
  POINTER-element create_node (0xc alloc, word copy, NO ctor)  :    1
     0x82520150  -> feeds exactly one insert, 0x823d14c0
```

⚠ **W9-B's numbers do not reproduce.** It reported **48** insert-shaped bodies
and **48** distinct create_nodes; I measure **235 / 149**. `48` is almost
certainly the gate's own `retail_bodytwins 48` — a masked-bodytwin count for one
symbol — read as a whole-image scan count. **The conclusion is unaffected and in
fact strengthened**: a larger denominator with still exactly *one* pointer
create_node is a stronger refutation of the no-fold world, not a weaker one. Do
not quote 48/48.

⚠ My own first attempt got this wrong in the other direction, and it is the more
instructive failure: I filtered on `.pdata` extent `== 100`, but the survivor's
extent is **104 B** (100 B body + 4 B alignment padding), so the filter silently
excluded the entire pointer family and reported **0** pointer create_nodes — a
clean, decisive-looking negative that was pure instrument error. It was caught
only because it disagreed with a briefed figure. **Key on the body, not the
`.pdata` extent.**

Both of W9-B's controls reproduce: `list<bool>` (`0x8274d128`) also allocates
`0xc` but copies with a **byte** op and sits at its own create_node
(`0x8274cf60`), so the discriminator is real.

### 3.3 The fold rule, applied

★ *A family folds iff its relocation targets are type-independent.*
`list<T*>::insert` is 100 B with **exactly one** relocation, `_M_create_node<T>`.
For **value** `T` that callee is per-`T` (149 distinct targets ⇒ no fold, which
is why W7-D and W8-D were right to refuse the value-type families). For
**pointer** `T` it collapses to a single address, because a pointer payload
needs no copy constructor: `0xc` alloc, word copy, one `bl`. The pointer
subfamily's relocation target **is** type-independent, so it folds — one
address, image-wide.

---

## 4. Per-change: predicted vs measured

Every change measured with `tools/ab_measure.py --from-dirty`, one change per
run, committed immediately.

| # | change | kind | predicted | measured | |
|---|---|---|---|---|---|
| C0 | `chase`: accept a vacuous leaf that is byte- **and** reloc-name-identical | tool | no metric effect | controls pass; pair PROVEN | ✓ |
| C1 | alias `0x823d14c0`: re-seat survivor onto the map-resident name, fold in `list<char*>::insert` | alias | **+904 B / +1 fn** (lower bound) | **+1020 B / +2 fns**, Δcode% +0.009952pp | ✓ |
| C2 | map `0x827bd208` → `?MemOrPoolAlloc@@YAPAXH@Z` | map | **+36 B / +1 fn** | **+36 B / +1 fn**, Δcode% +0.000350pp | ✓ exact |
| C3 | map `0x824e0f68` → `insert<OldMMInst>`; delete `0x823b60c0` | map | **+100 B / +1 fn** | **+100 B / +1 fn**, Δcode% +0.000978pp | ✓ exact |
| C4 | map: bring the displaced `insert<Target>` home; `insert<EventCall>`; delete 2 unidentifiable | map | **−200 B / −2 fns** | **+708 B / +1 fn**, Δcode% +0.006908pp | ⚠ missed by +908 B |

**C1 exceeded its prediction by one row, and the excess is fully attributed.**
Diffing leg A against leg B row by row:

```
CROSSED   +904 B  default/File       FileRelativePath
CROSSED   +116 B  default/Character  ??0?$list@PAUDep@CharPollableSorter@@...
```

The 904 B is W8-D's prize. The 116 B is the payoff from the *other* half of C1 —
re-seating the survivor — which I could not pre-quantify. Both defects were real:

1. the group had **zero map-resident members** (survivor `insert<CharClip*>` is
   absent from `target_symbol_map.json`), so it violated the file's own stated
   invariant and **could not forgive retail's own name at its own address**;
2. `list<char*>::insert` was missing, which is what closes `FileRelativePath`.

**C2's `none`-ruler control MOVED** (+36 B) and `ab_measure` classified it
`REAL_PAIRING`, not `ALIAS_SUSPECT` — the correct shape for a rename that pairs
a body which never paired. The row sat at 36 B / `fuzzy 0.000000` for exactly
one reason: the map named it with a 4-arg spelling our match build never defines
(`MemMgr.cpp` puts the 4-arg form behind `#ifdef HX_NATIVE`), so nothing could
pair with it, ever.

⚠ **C1's, C3's and C4's `none` controls were FLAT and `ALIAS_SUSPECT` fired.**
That is *expected* and licenses nothing either way: `none` ignores relocation
names, so a charge that exists only under `name_check` can only be closed under
`name_check`, and a fabricated alias produces the identical shape. CLAUDE.md's
rule is that the two are separable by **patch kind**, not by the control. The
license here is always the retail-byte evidence, never the delta.

`tools/icf_alias_finder.py --validate` after a forced re-split and full build:
**`PASS — 1358 map-consistent, 235 tolerated, 0 contradicted, 1595 total`.**

### 4.1 ⚠ C4 missed by +908 B, and the miss is the most reusable thing here

I pre-registered **−200 B / −2 fns** for C4 and measured **+708 B / +1 fn**. The
attribution sums exactly:

```
 -100 B  default/BandCamShot  insert<BandCamShot*>            renamed away
 -100 B  default/BandCamShot  insert<RndDrawable*>            renamed away
 -100 B  default/MidiParser   insert<MidiParser*>             deleted
 -100 B  default/Shockwave    insert<Target>                  deleted (false home)
 +100 B  default/BandCamShot  insert<BandCamShot::Target>
 +100 B  default/BandCamShot  insert<EventAnim::EventCall>
 +116 B  default/BandCamShot  list<EventCall>::list
 +396 B  default/BandCamShot  PropSync<BandCamShot::Target>     <- CALLER
 +396 B  default/PropSync     PropSync<WorldDir::MatOverride>   <- CALLER
                                                        net  +708
```

★★★★ **I priced each rename on its OWN row and forgot the CALLERS.** Once the
map spells `insert<Target>` correctly, the relocation from `PropSync<Target>`
agrees with what our object already emitted and that 396 B row crosses. This is
CLAUDE.md's *"a WRONG NAME is FINANCED BY ITS CALLERS"* collected in reverse —
MAPDEF-3 measured the same channel at +108 B; here it is **+792 B**, more than
the whole definition-row effect.

★ And note which instrument was right: my −200 B figure was the **`none`**-ruler
number, and the `none` control reported **exactly −200 B**. The two rulers
disagreeing was the *signal*, not an alarm — `none` sees only the pairing change,
`name_check` also sees the relocation names the repair fixed at every call site.

---

## 5. The five named pointer-element `insert` addresses, settled

W9-B's H2 asks whether the other four named pointer-element `insert` addresses
are genuine. All of its evidence reproduced exactly on retail bytes:

| addr | map name | create_node | alloc | ctor? | verdict |
|---|---|---|---:|---|---|
| `0x822b55e0` | `list<BandCamShot*>::insert` | `0x822b4c28` | **0x6c** | yes | ⛔ value type, 100 B payload |
| `0x822b5728` | `list<RndDrawable*>::insert` | `0x822b4ca8` | **0x20** | yes | ⛔ value type, 24 B payload |
| `0x827e5318` | `list<MidiParser*>::insert` | `0x827e4bf0` | **0x5c** | yes | ⛔ value type, 84 B payload |
| `0x823b60c0` | `list<RndTransformable*>::insert` | — | — | — | ⛔ **not an insert** — 3 `bl`, calls `BinStream::operator>>(bool&)` |
| `0x823d14c0` | `list<Object*>::insert` | `0x82520150` | **0xc** | no | ✅ genuine |

A `T*` payload is 4 B and needs no copy constructor, so the first three are
value types wearing pointer names.

### 5.1 Two of them are REPAIRABLE, and one name was merely DISPLACED

Reading each create_node's copy-constructor callee gives the element:

* `0x822b4c28` → `??$_Copy_Construct@UTarget@BandCamShot@@…` ⇒
  `BandCamShot::Target`;
* `0x822b4ca8` → `??$_Copy_Construct@VEventCall@EventAnim@@…` ⇒
  `EventAnim::EventCall`;
* `0x827e49c8` → **unnamed**, so not identifiable this way.

Each identification is confirmed a **second, independent way** — by payload
arithmetic against the compiler, never the header comments
(`scripts/harvest/class_layout_report.py`):

| element | compiler `sizeof` | node alloc | 8 + sizeof |
|---|---:|---:|---:|
| `BandCamShot::Target` | **100 (0x64)** | `0x6c` | `0x6c` ✓ |
| `EventAnim::EventCall` | **24 (0x18)** | `0x20` | `0x20` ✓ |

⚠ **The injectivity check fired, and it was right to.**
`?insert@?$list@UTarget@BandCamShot@@…` was **already placed, at `0x824ce130`**.
I nearly refused the repair on that basis. The contradiction resolves cleanly
once you ask which address the element size fits: `0x824ce130`'s create_node
allocates **`0x2c`** — a 36-byte payload, matching neither `Target` (100) nor
`TargetCache` (76). So `0x824ce130` is **not** `insert<Target>`, and the name was
**displaced, not spurious** — the same pattern W9-B found on `IsUserAGuest`. The
two errors are complementary: the true home wore `BandCamShot*` (the `::Target`
simply dropped) while the real name sat on an unrelated address, in
`default/Shockwave`.

★ I also checked the escape that would have voided the identification: if
`_Copy_Construct<T>` were an ICF fold survivor its name would be arbitrary. It
is not — each of these three copy constructors has **exactly one** `bl`-caller
(against `memcpy`'s 1,465), so they are per-`T`.

### 5.2 ⚠ A correction to my own reasoning, mid-lane

My first draft of this section argued *against* removing the unrepairable names,
on the grounds that the rows "genuinely earn" their 100 because they are real
insert bodies. **That was wrong**, and the correction is the reason C4 exists.
Our object supplies `insert<BandCamShot*>`, whose create_node is the *pointer*
kind (`0xc`); retail's at `0x822b4c28` is a *value* create_node (`0x6c`). Those
are different functions. The 100 survives only because retail's create_node is
**anonymous** and `name_check` forgives a placeholder target. It is false credit
exactly as W9-B said — §0 of its own document, landing on my analysis.

### 5.3 What C4 does

| addr | action | rationale |
|---|---|---|
| `0x822b55e0` | → `insert<BandCamShot::Target>` | proven twice (copy-ctor name + `sizeof` 100 = `0x6c` − 8) |
| `0x824ce130` | **DELETE** | payload 36 ≠ `sizeof(Target)` 100 — the displaced name's false home |
| `0x822b5728` | → `insert<EventAnim::EventCall>` | proven twice (copy-ctor name + `sizeof` 24 = `0x20` − 8) |
| `0x827e5318` | **DELETE** | 84 B value type, no candidate identified |

`0x824e0f68` and `0x823b60c0` were handled in C3. `0x824e0f68` rests on a
*different and stronger* basis than a copy-ctor name: `0x824e0458` is mapped
`?_M_create_node@?$list@UOldMMInst@@…`, and a `_M_create_node` **is** per-`T`
(it embeds the payload size), with `0x58` = 8 + `0x50` and `sizeof(OldMMInst)` =
`Transform` (0x40) + `Hmx::Color` (0x10) = `0x50`. `list<unsigned>` is refuted
twice over — `unsigned` is a 4 B POD, so its node would allocate `0xc` and call
**no** copy constructor, where retail's calls one.

---

## 6. What this lane did NOT do

* ⛔ **Did not touch `src/`.** The stratum turned out to be a map question and
  our allocator declarations were already correct, so
  `tools/native_build_gate.sh` is **not applicable** (0 files under `src/` in
  the branch diff).
* ⛔ **Did not repair `0x827bb0e8` (`PoolAlloc` 5-arg → 2-arg).** Its call sites
  prove 2 arguments exactly as `MemOrPoolAlloc`'s prove 1, but unlike
  `MemOrPoolAlloc` our `PoolAlloc.cpp` **does** define the 5-arg form outside
  `HX_NATIVE`, so the row already pairs at `100.0` and the rename is Δbytes 0 —
  accuracy-only, and a separate change. **H1.**
* ⛔ **Did not identify `0x827e5318`'s 84 B element.** Our side has two
  candidates whose `_M_create_node` allocates `0x5c` (`WorldCrowd::CharData`,
  `RndGenerator::Instance`). The first is the shape match — `bl savegprlr` /
  `bl MemOrPoolAllocSTL` / `bl _Copy_Construct` / `b restgprlr` against retail's
  3 `bl` — but its create_node is **72 B against retail's 80 B**, so it is *not*
  proven and was not installed. **H2.**
* ⛔ **Did not name `0x824ce130`** after removing the displaced name from it. Its
  element is a 36-byte value type; nothing further was established. **H3.**
* ⛔ **Did not prune the 4-arg/5-arg debug spellings from their alias groups.**
  They forgive 0 today, and CLAUDE.md records that pruning zero-forgiving
  spellings cost **+94,616 B** to reverse.
* ⛔ **Did not run the permuter** (standing directive: OFF).
* ⚠ **Did not audit the other alias groups whose survivor is not map-resident.**
  C1 found one by accident and it was worth 116 B once fixed; nothing here says
  it is the only one. **H4.**
* ⚠ **Did not act on a pre-existing map injectivity finding.** A whole-map sweep
  turns up **two** duplicated names. `?NodeCmp@@YAHPBX0@Z` is explicitly
  allowlisted by the map's own `_internal_linkage_allow` key (a file-static
  comparator duplicated across TUs mangles identically — legitimate). The other,
  `??$__destroy_aux@ULevelData@@…` at `0x82b5b1d0` **and** `0x82b63ec8`, is not
  allowlisted and predates this lane. **H5.**

---

## 7. Handoffs

| # | handoff | evidence in hand |
|---|---|---|
| **H1** | **`0x827bb0e8` → `?PoolAlloc@@YAPAXHH@Z`**, and drop the then-dead 5-arg definition from `PoolAlloc.cpp`. Accuracy-only; Δbytes 0 predicted. | §1.2 — 263/267 sites pass exactly `r3`+`r4`, `r4` via `li` ×265 |
| **H2** | **Identify `0x827e5318`'s 84 B element**, then restore the +100 B by repairing rather than leaving it unnamed. | §6 — `WorldCrowd::CharData` is the shape match at 72 B vs retail's 80 B |
| **H3** | **Name `0x824ce130`** — a `list<T>::insert` over a 36-byte value type, now unnamed. | §5.1 |
| **H4** | **Audit `symbol_aliases.json` for other groups with no map-resident member.** Such a group cannot forgive retail's own name at its own address. | §4 — C1's group was worth 116 B once fixed |
| **H5** | `??$__destroy_aux@ULevelData@@…` is mapped at two addresses and is not on the `_internal_linkage_allow` list. | §6 |
| **H6** | `RecursePatternInternal` (892 B, `default/File`) is now that unit's last prize, still 99.97758 behind one relocation name. | W8-D §3 |

---

## 8. Reusable lessons

* ★★★★★ **A recursive adjudicator's trace is a STACK, not a list of findings.**
  Every `SLOT-REFUTED` frame above the leaf is the same failure re-reported one
  level up. W9-B read four of them as four blockers and sized a campaign around
  the two in the middle. **Read such a trace bottom-up and act only on the
  leaf.**
* ★★★★ **A vacuity guard is a claim about what MASKING hides, so it is itself
  vacuous when nothing is masked.** Refusing a pair whose bodies are identical
  *and* whose relocation names are equal is not conservatism, it is a false
  negative wearing conservatism's clothes. The fix must still demand the name
  equality: 90 of our 8-byte thunks share that masked body with a different
  target.
* ★★★★ **When a callee cannot discriminate an arity, the CALLERS can — provided
  the arguments are compile-time constants and the build has no LTCG.** Both
  conditions are load-bearing and both must be stated: a *dynamic* argument can
  legitimately be already-live (`MemOrPoolFreeSTL`, 985/1759 sites), and LTCG
  would let the compiler elide what a declaration demands.
* ★★★★ **Price a map rename on its CALLERS, not just its own row.** C4's
  definition rows netted −84 B; its two caller rows paid +792 B. A name is
  financed by everyone who spells it, so repairing one collects from all of
  them — and a prediction that stops at the renamed row can be wrong by an
  order of magnitude, in the direction that looks like luck.
* ★★★ **Build the positive control into the same scan.** Five of seven map
  arities reproducing is what makes the two contradictions worth acting on, and
  finding 279 real `__FILE__` strings is what makes "zero `MemMgr.cpp` strings"
  a measurement instead of a broken search.
* ★★★ **An injectivity check is a fabrication detector, not a formality — and a
  name it collides with may be DISPLACED rather than correct.** It stopped a
  repair I had already convinced myself of; the resolution was not to abandon
  the repair but to ask which address the element *size* fits. Both a wrong name
  and a right name in the wrong place look identical until you measure something
  the name cannot influence.
* ★★★ **Two independent lines per identification, and prefer a per-`T` callee
  over a shared one.** `_M_create_node<T>` embeds the payload size, so its name
  identifies an element; `_Copy_Construct<T>` is a fold candidate and its name
  may be arbitrary — check its fan-in before trusting it, then confirm with
  `sizeof` from the **compiler**, never the `// 0xHEX` header comments.
* ⚠ **`.pdata` extents include alignment padding.** Filtering on
  `extent == 100` excluded the entire population I was measuring and produced a
  confident `0`. A census returning a clean decisive negative should be
  distrusted until one known-answer member is shown to survive the filter.
* ★ **Re-derive, never inherit.** W9-B's load-bearing conclusion reproduced; its
  population *figures* did not (48/48 vs 235/149), because a gate's
  `retail_bodytwins` count had been read as a whole-image scan.

---

## 9. Post-rebase composition check

`main` moved **twice** while this lane ran — to `c5297f13` (W10-C) and then to
`654dc785` (W10-A) — and **both** of those lanes edited
`scripts/symbol_aliases.json` and `scripts/target_symbol_map.json`, the same two
files this lane changes. W10-C's own ledger line warns that *"the lane's
inertness control does NOT survive composition"*, so a clean textual rebase is
not evidence of a surviving result.

⚠ The first rebase landed on `c5297f13` and `git diff main..HEAD` then showed my
branch *deleting* W10-A's doc and reverting `splits.txt` — an artifact of `main`
having advanced again between the rebase and the diff, not a real defect.
**Re-check `git log` immediately before AND after rebasing**; the footprint diff
is the check that catches it (it should list only your own files, and it now
lists exactly four).

Re-verified on the rebased tree (full build after `touch config.yml`, which
cascades ~975 objects because W10-C touched `src/system/obj/Object.h`, a PCH
input):

```
BUILD_EXIT=0
VALIDATE: PASS -- 1352 map-consistent, 241 tolerated, 0 contradicted, 1595 total
whole binary: matched_functions 42,761   matched_code 3,874,256 B
              matched_code_percent 37.812540   fuzzy 49.143147
```

Every row this lane moved is still at `fuzzy 100.00000` after composition:

| row | size | |
|---|---:|---|
| `FileRelativePath` (`default/File`) | 904 | ✓ W8-D's prize, collected |
| `?MemOrPoolAlloc@@YAPAXH@Z` (`default/MemMgr`) | 36 | ✓ |
| `insert<OldMMInst>` (`default/Crowd`) | 100 | ✓ |
| `insert<BandCamShot::Target>` (`default/BandCamShot`) | 100 | ✓ |
| `insert<EventAnim::EventCall>` (`default/BandCamShot`) | 100 | ✓ |
| `PropSync<BandCamShot::Target>` ×2 | 396 ×2 | ✓ the caller cascade |
| `list<EventCall>::list` | 116 | ✓ |

⚠ **The lane's contribution is the SUM OF ITS FOUR MEASURED DELTAS
(+1,864 B / +5 fns), not a subtraction against this absolute.** Deltas compose;
absolutes do not. `42,761 / 3,874,256` is a *different tree* — it carries W10-A
and W10-C as well — and differencing it against this lane's leg A would silently
bill their work to this lane.
