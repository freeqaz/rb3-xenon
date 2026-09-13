# Lane W8-D — `FileRelativePath`, and the two alias refusals that close `default/File`

**Branch** `w8-filerelativepath` · worktree `~/tmp/wt-w8-d` · base `2fc2552a`
(`git merge-base --is-ancestor 2fc2552a HEAD` asserted before the first edit).
Ruler: **`name_check` (graded)**, read from `report.json`'s
`provenance.diff_config` rather than assumed.

**Result: the 904 B body is ported and is BYTE-EXACT against retail — 226 of
226 masked words equal, 22 of 22 relocations at the same offsets — and the row
is now named, paired and scoring 99.955750 where it scored 0.000000.
Whole-binary Δmatched +0 / Δcode_bytes +0, as pre-registered on both steps.**
`default/File`'s unit fuzzy went **72.07893 → 83.22348** (+11.14 pp).

The two byte prizes in this unit (904 B + 892 B) are **both blocked by one
mechanism, and this lane REFUSES both** — the refusals are the larger half of
the deliverable, and §4 gives the evidence on each side.

Baseline, after this lane's mandatory first full build (a reflinked worktree's
target objs are pre-renamer, so every mangled-name lookup reads "absent" until
the renamer's pre-compile step has run, and any name-keyed negative taken
before that is vacuous — FOLDPROVE-2):

```
matched_functions   42,738      masked_equal        22,980
matched_code     3,865,216 B    total_code      10,245,956
matched_code_percent 37.724308  total_functions     69,219
fuzzy_match_percent  49.108723
```

`verify_objs_patched.py --verify-manifest` → **OK: 1205 decomp, 3085 target
objects** (rc=0) before any edit, and again before all name-keyed work in §4.

---

## 1. The briefed figures, re-verified literally

Every one reproduced, to the digit, before anything was built on it.

| briefed | measured | |
|---|---|---|
| `FileRelativePath` `fn_82517718` is 904 B, the largest prize in the unit | **904 B** | ✓ |
| `RecursePatternInternal` sits at 99.97758 with exactly one charge | **99.977580**, mpn identical | ✓ |
| `FileMakePath` is at ~94.5 | **94.696970** (mpn 95.631310) | ✓ |
| `NewFile` `fn_825173E0` is unpaired | **fuzzy 0.000000**, 424 B | ✓ |
| the `0x822d8cc0` alias group exists, T1, with a sibling member | **exists**, 1 folded member, T1 | ✓ |
| naming costs 35 sites / 30 callers / 22 files | not re-counted as sites; **counted on the OBJECT side instead** (§2.2), which is what the decision needed | — |

⚠ **One thing the brief understates, and it reshapes the plan: `fn_82517718` is
UNPAIRED at fuzzy 0.000000**, exactly like `NewFile`. There is no map name at
`0x82517718`. So "body-port first" is Δ0 *by construction* — the row cannot
score, cannot be seen by objdiff, and cannot be iterated against. That does not
make the order wrong (§2.2 shows it is right), but it means the body has to be
verified by an instrument other than the score, which §2.1 does.

---

## 2. `FileRelativePath` — the body was already written, in the wrong function

### 2.1 The diagnosis, from retail's bytes alone

Retail `fn_82517718` is **one function**, not a wrapper over a `*Buf` helper.
Three tells, none of which needs a build, and all three are the discriminators
W7-A's §9 prescribed:

1. **Frame arithmetic.** `stwu r1, -0x2a0` decomposes exactly as `0x70` locals +
   `0x100` `rootBuf` + `0x100` `fpBuf` + `0x30` param area. A third 256-byte
   output buffer would force `>= 0x3a0`. There is no room for one, so the output
   must be a static.
2. **Argument registers.** `r5` is never read; `lbz r10, 0x0(r4)` is `*iFilepath`
   and `r3` is the root. Retail's is the **two-arg** form our `File.h` declares —
   the same "rb3-Wii carries an extra buffer param retail-Xbox does not" that
   W5-C found on `FileGetBase` and W7-A on `FileMakePath`.
3. **The tail proves the static.** `.L_82348BF8: mr r3, r30` is the single
   return, and `r30` does double duty: `mr r30, r4` at entry (so the two
   early-out paths return `iFilepath`) and `addi r30, r11, lbl_82CCA6B0@l` at
   `.L_82348ADC` (so the success path returns the static). `r31` is the cursor
   `p`, `r28` is `'/'` (0x2f), `r29` is `'.'` (0x2e).

**The `MainThread()` site is SPURIOUS.** Adjudicated on retail bytes, not
inherited: `fn_82517718`'s complete callee inventory is `strtok` ×4
(`fn_8282D8A0`), `list::insert` ×2 (`fn_823D14C0`), `list::erase` ×4
(`fn_82BB33D8`) and `_List_base::clear` ×2 (`fn_82718880`). There is **no**
`bl fn_824A4C10`. W6-A's audit reproduces. Deleted rather than wrapped, for
W5-C's reason: `MILO_ASSERT(cond,line)` is `((void)(cond))` in this build, which
still *evaluates*, and `MainThread()` is an extern call MSVC cannot elide.

One further source correction the bytes forced: retail's inlined `strcmp` loads
`0x8(r7)` = `rootToks.front()` into `r11` and computes `subf r9,r6,r9` =
`root_char - fp_char`, so **root is the left-hand argument**. Our source had the
operands the other way round in both comparisons.

⇒ The port is mechanical: our existing `FileRelativePathBuf` body, with `oBuf`
replaced by the static and `MainThread()` deleted.

**The verification instrument, since the score could not be one.** Our compiled
COMDAT against retail's target obj, relocation-masked:

```
OURS FileRelativePath   904 B, 22 relocations
TGT  fn_82517718        904 B, 22 relocations
masked bodies identical: True      differing masked words: 0 of 226
```

All 22 relocations sit at the same 22 offsets. Every surviving difference is a
relocation **name**, and they fall into exactly four classes:

| offsets | ours | retail | status |
|---|---|---|---|
| 608, 616 | `?relative@?1??FileRelativePath@@9@4PADA` | `lbl_82CCA6B0` | **forgiven** — a `lbl_` target is a placeholder |
| 880, 888 | `?clear@?$_List_base@PAD…` | `?clear@?$_List_base@PAVSynthPollable@@…` | **already folded**, group `0x82718880` |
| 568, 592, 720, 848 | `?erase@?$list@PAD…` | `?erase@?$list@PAVVoice@@…` | **already folded**, group `0x82bb33d8` |
| 216, 292 | `?insert@?$list@PAD…` | `?insert@?$list@PAVObject@Hmx@@…` | **NOT folded** — §4.1 |

Both already-folded groups are keyed on **exactly the address retail calls**
(`0x82718880`, `0x82bb33d8`) and both already carry our `char*` spelling. So
before measuring anything, the residual was known to be **two charges**.

`FileRelativePathBuf` is **KEPT** although it now has no caller outside this
file — W5-C's reason for keeping `FileGetBaseBuf`: an unreferenced COMDAT does
not score, and deleting it is a native-visible API change. (This is the opposite
call from `FileMakePathBuf`, which W7-A kept because it has five *live* callers.
Both keep; the reasons differ.)

### 2.2 The naming decision — W7-A's condition, discharged by measurement

W7-A costed the naming at **35 sites / 30 callers / 22 files** and deferred it,
because under `name_check` naming an anonymous address converts forgiven
placeholder call sites into checked ones, for a row that could not reach 100
anyway. That is the right rule. Both halves of its condition are now discharged:

* the body is byte-exact, so the row **can** reach 100;
* **the caller population is measured safe** — which is the input W7-A did not
  have. Counting on the *object* side rather than the site side: of our 1,205
  compiled objects, **51 reference `FileRelativePath` and exactly one — File.obj
  itself — references `FileRelativePathBuf`.**

That single number settles it. MSVC is **not** inlining our 19-instruction
wrapper into its callers; all 30 already emit an out-of-line
`bl FileRelativePath`, which is the very spelling the naming installs. Retail's
site and ours therefore agree **by name**, so the naming levies no new charge at
any of the 35 sites.

★ This is the exact mechanism that bit W5-C in the *other* direction, where the
then-tiny `FileGetDrive`/`FileGetPath` wrappers **were** inlined and re-exposed
`bl MainThread` / `bl FileGetPathBuf` at the caller. The test is cheap, it is
one grep over the built objects, and it is the difference between a safe naming
and a 35-site regression. **It should be run before every naming.**

⇒ W7-A's "body-port FIRST, name SECOND" order is **kept**, and now has a
mechanism rather than just caution: porting the body is what makes the function
too big to inline, and being un-inlined at every caller is what makes the naming
free. Doing it in the other order would have been safe here only by luck.

### 2.3 Measured: both steps Δ0, both as pre-registered

| # | step | kind | predicted | measured | |
|---|---|---|---|---|---|
| 1 | port retail's body into `FileRelativePath` | source | **Δ0 / Δ0** (row unpaired) ± the File.obj EH-funclet channel | **Δmatched +0, Δcode_bytes +0**, Δmasked_equal **+0** | ✓ |
| 2 | map: `0x82517718` → `FileRelativePath` | map | row 0.000000 → **99.955752**; Δmatched +0, Δbytes +0 | row → **99.955750**; **Δmatched +0, Δcode_bytes +0** | ✓ |

Step 1 had 1 leg-B recompile (so the Δ0 is not an absent-vs-absent vacuity).
Step 2 forced a re-split on both legs, `renamer_patched=1823`, and both legs
reached a `symbols.txt` fixed point.

★ **The `5/N` screen closed to five decimals again.** `N = 904/4 = 226`, two
charges ⇒ `100 − 2×(5/226) = 99.955752`; measured **99.955750**. Per the brief I
confirmed the charge **KIND** first — these are relocation-name (`Symbol`) args,
the kind the screen was calibrated on, not the differing-**immediate** kind for
which W7-C measured it over-predicting ~5×.

★★ **`mpn == fuzzy == 99.955750`, which settles an ambiguity worth recording.**
Relocation-name charges reduce **both** rulers here, so `matched_functions`
cannot cross while they stand. That is why step 2 was predicted at Δmatched +0
and why closing the two charges is worth **+904 B and +1 function together**,
not one or the other. (`RecursePatternInternal`, whose single remaining charge
is also a relocation name, shows the same `mpn == fuzzy` signature — the two
rows corroborate each other.)

★★★ **The most useful output of step 2 was its `none`-ruler control, and it is
an independent proof the body is right.** `none` ignores relocation names, and
it moved **+904 B**; `ab_measure` classified it `REAL_PAIRING` rather than an
alias artifact. So on the one ruler that is blind to exactly the class of
difference that remains, the ported body scores a full 100 and collects the
entire 904 B. The body is not "close"; it is **done**, and the whole residual is
two names.

⚠ Note the direction of the standing warning here. CLAUDE.md's rule is that a
flat `none` control cannot clear a *fabricated alias*. This is the mirror case —
a `none` control that **moves** on a *source + map* change, which is the
documented `REAL_PAIRING` shape, not the ALIAS_SUSPECT one (that fires only on
map-**only** patches). It is evidence, not a loophole.

---

## 3. The unit after this lane

| | before | after |
|---|---|---|
| `default/File` fuzzy | 72.07893 | **83.22348** |
| `fn_82517718` / `FileRelativePath` | unpaired, 0.000000 | **99.955750**, named |
| whole-binary fuzzy | 49.108723 | **49.117542** |
| whole-binary matched / code | 42,738 / 3,865,216 | **unchanged** |

The unit's two remaining byte prizes are now **`FileRelativePath` 904 B (2
charges)** and **`RecursePatternInternal` 892 B (1 charge)** — 1,796 B sitting
behind three relocation names, and §4 explains why none of the three is
collectable today.

---

## 4. Both alias installs REFUSED — the evidence, on each side

`tools/icf_pair_adjudicate.py --selftest` was run first and **PASSED**: its
positive control returns PROVEN and its negative control returns REFUTED, so the
instrument discriminates. A gate that cannot fail proves nothing (W7-D §3.3).

### 4.1 ⛔ `list<char*>::insert` ≡ `list<Object*>::insert` — REFUSED

This is the pair that gates *this lane's own* 904 B, so the temptation to admit
it is maximal. That is the reason to be strictest with it.

**The gate refuses:**

```
FLAT T1 : REFUTED
  retail_size 100   our_size 100
  why  masked bodies match but relocation TARGETS disagree -- template-twin, not a fold
  retail_bodytwins  48
```

**Retail keeps 48 distinct addresses** carrying a masked-identical `insert`
body — the same population-control failure that made W7-D refuse `_S_sort` over
41 distinct `_List_base<T>::clear` addresses.

**Applying W7-D's rule directly**, as the brief instructs: `list<T*>::insert` is
100 B with **exactly one** relocation, and it is
`?_M_create_node@?$list@<T>@…` — a **per-`T` callee**. That is verbatim the
refusal signature W7-D names (`list<T*>::insert → _M_create_node<T>`), and W7-D
already measured this exact family producing **11 false positives** in its
fold-closure instrument.

**The case FOR the fold, stated fairly, because it is not weak:**

* the closure argument goes through — `_M_create_node<T*>` is 64 B relocating
  **only** to `?MemOrPoolAllocSTL@@YAPAXH@Z`, which is type-independent, and the
  `char*` and `Object*` instantiations of it are identical;
* the map names exactly **one** address for a pointer-element `_M_create_node`;
* `0x823d14c0` has fan-in **289 sites across 118 files** with wildly
  heterogeneous element types — the shape of a heavily folded survivor, not of a
  `list<Object*>` specialization;
* and most directly: **retail's own `fn_82517718` is unambiguously
  `list<char*>`** — the node payload at `0x8(node)` is dereferenced with `lbz`
  as characters — yet it calls the address the map names `list<Object*>::insert`.

**Why that is still not enough to install.** The four points above are
consistent with *two* different worlds, and they do not distinguish them:

1. the family folds, `0x823d14c0` is the survivor, and the alias is real; or
2. the family does **not** fold (48 addresses), `0x823d14c0` really is
   `list<char*>::insert`, and **the map name on it is simply wrong** — the
   "ASK IF THE THING NEEDING A NAME EXISTS" class, and the same map-defect
   failure mode W7-D hit on `CharBlendBone`/`CharTransDraw`.

Under world 2 an alias would be **fabrication**, and the standing rule is that a
fabricated alias lifts `name_check` **by construction** while the `none` control
reads flat **by construction** — so installing it would produce a confident
"+904 B measured" that is worth nothing. I cannot tell 1 from 2 with the
evidence in hand, and the sanctioned gate says 2. **Refused.**

⚠ **A methodological finding for the next lane, and it cuts in the permissive
direction — which is exactly why it must not be acted on casually.** The gate's
population control counts **masked** body-twins, and masking discards the
relocation. But this family's fold classes are *determined* by that relocation:
`insert<T>` folds iff `_M_create_node<T>` does, and `_M_create_node<T>` differs
per `T` for value types (31 named addresses) while collapsing for pointer types
(1 named address). So the "48 retail bodytwins" figure is dominated by
**non-pointer** `T` and does **not** by itself refute a pointer-subclass fold.
The honest verdict on this pair is therefore **UNPROVABLE, not REFUTED** — the
same distinction W7-D drew for the 138 `~list<T>` spellings whose `clear<T>` the
map cannot place. What would settle it is deciding whether the *other four*
named pointer-element `insert` addresses (`BandCamShot*`, `RndDrawable*`,
`RndTransformable*`, `MidiParser*`) are genuine distinct bodies or four more
mis-namings; that is a map-integrity question, not an alias one, and it is H1.

⛔ And note what is **not** the fix: renaming `0x823d14c0` to
`?insert@?$list@PAD…` would repair 2 charges and **create charges at the other
287 call sites**. A map rename is the wrong instrument at fan-in 289.

### 4.2 ⛔ `vector<String>::~vector` into group `0x822d8cc0` — REFUSED

The brief priced this at **892 B + 1 fn** and told me to decide it with W7-D's
rule. The rule answers cleanly, and it answers **no**.

```
FLAT T1 : REFUTED
  retail_size 136   our_size 136
  why  masked bodies match but relocation TARGETS disagree -- template-twin, not a fold
  retail_bodytwins  10
```

**W7-D's rule applied to the relocation list.** `~vector<T>` carries two
relocations:

| | ours (`vector<String>`) | survivor (`vector<ObjPtrVec::Node>`) |
|---|---|---|
| +72 | `__destroy_range_aux<reverse_iterator<String*>>` | `__destroy_range_aux<reverse_iterator<Node*>>` |
| +108 / +104 | `MemOrPoolFreeSTL` | `MemOrPoolFreeSTL` |

`MemOrPoolFreeSTL` is type-independent; **`__destroy_range_aux<…<T*>>` is
per-`T`**. By the rule the brief cites — *a family folds iff its relocation
targets are type-independent* — this family carries a per-`T` callee and
therefore does not fold, and retail's 10 distinct masked-identical addresses are
consistent with that.

W7-A's circumstantial case (retail's own body copy-constructs a `vector<String>`
at [132], scored equal, and destroys it at [187]) is real and I reproduced it,
but it is the **same shape** as §4.1's strongest argument and it fails for the
same reason: it establishes that *a* `~vector` runs there, not that our spelling
and the survivor's spelling share an address.

⚠ Corroborating but **deliberately not load-bearing**: our `~vector<String>` is
136 B and our `~vector<ObjPtrVec::Node>` is **132 B**, and different-size COMDATs
cannot fold. I am not resting the refusal on that, because CLAUDE.md records
STLPORT-1, where a +8 B "size bug" of exactly this shape turned out to be our
own COMDAT reader billing the successor symbol's EH funclet prefix. The refusal
rests on the relocation rule and the gate.

⚠ **An observation I am flagging rather than acting on:** the group's existing
folded member, `~vector<ObjDirPtr<ObjectDir>>`, carries the *same* per-`T`
`__destroy_range_aux<reverse_iterator<ObjDirPtr*>>` shape, and was installed on
T1. By the rule applied above it is suspect on the same grounds. Withdrawing an
*installed* fold is a separately-measurable change needing direct contradiction
(W7-D withdrew one only on positive evidence), and it is not this lane's charter.
**H2.**

---

## 5. `NewFile` — naming IS non-negative, and still should not be done yet

The brief asked me to decide this from the caller population. Applying §2.2's
object-side test: our only spelling is `?NewFile@@YAPAVFile@@PBDH@Z`, **21
objects reference it**, our COMDAT is **524 B** — far past the ~40-instruction
`/O1` inline limit W5-C measured — so every caller emits an out-of-line
`bl ?NewFile@@…` and a naming would levy **no new charge at any call site**.
`0x825173e0` is unoccupied and the name is placed nowhere else, so it is
injective. **The naming is non-negative.**

**It is nevertheless still not worth doing**, and for the reason the same
comparison exposes: our body is **524 B / 46 relocations against retail's
424 B / 27**, and the divergence is structural, not cosmetic —

* retail calls `?MainThread@@YA_NXZ` at +24 (W7-A's GENUINE verdict reproduces)
  but has **no** `?Notify@Debug@@`, no `?TheDebug@@` and no
  `"NewFile(%s) from MainThread()"` literal, so our `Notify` branch is extra;
* we carry an entire `gNullFiles` / `operator new` / `??_7NullFile@@6B@` path
  retail does not have, plus a `?UsingCD@@YA_NXZ` call retail does not make.

So naming today pairs a badly-divergent body at a low percentage for 0 bytes.
The order is the same as §2.2's and for the same reason: **port the body, then
name.** That is H3, and unlike before it is now a *priced* handoff rather than
an open question.

★ **Two free identifications fell out of that comparison**, by matching callee
positions between our body and retail's:

| retail | is | evidence | prize |
|---|---|---|---|
| `fn_82516E28` | **`FileLocalize`** | called from `NewFile` in the same argument position; both bodies call `?SystemLocale@@YA?AVSymbol@@XZ` ×3 | **408 B**, currently unpaired at fuzzy 0 in this unit |
| `fn_8252DEE8` | **`AsyncFile::New`** | called from `NewFile` where ours calls `?New@AsyncFile@@SAPAV1@PBDH@Z` | outside this unit |

⚠ `FileLocalize` is **not** a cheap win: retail's 408 B body calls `SystemLocale`
×3 and **nothing else**, while our 548 B version also calls `GetGfxMode`,
`HongKongExceptionMet` and `strstr` ×2. That is a genuine body divergence
needing a port, not a naming. **H4.**

---

## 6. What this lane did NOT do

* ⛔ **Did not install either alias** (§4). Together they are 1,796 B + 2
  functions — the entire remaining byte value of this unit — and both are
  refused on the sanctioned gate plus W7-D's rule. §4.1 states its verdict as
  **UNPROVABLE rather than REFUTED**, with the discriminating experiment named.
* ⛔ **Did not reopen `FileMakePath`'s last 33 charges.** The brief conditioned
  this on having a *new mechanism*; I have none. W7-A refuted the peeled-`strtok`
  lever four ways with recompile proof, and a fifth loop rewrite is exactly what
  that negative exists to prevent. Untouched at 94.696970.
* ⛔ **Did not name `NewFile`** (§5) — non-negative but valueless until its body
  is ported.
* ⛔ **Did not port `FileLocalize`** (§5 H4) — identified and priced at 408 B,
  but it is a real divergence and a separate change.
* ⛔ **Did not touch `splits.txt`, `symbols.txt`, `symbol_aliases.json`,
  `.pdata`**, and did not rebuild jeff, objdiff, wibo or objcache. This branch
  changes exactly **two** files: `src/system/os/File.cpp` and
  `scripts/target_symbol_map.json` (a 2-line diff, with the `indent=1`
  round-trip asserted as a fixed point before writing — W5-C H5).
* ⚠ **Did not re-examine `MainThread()` outside `File.cpp`.** W6-A's tree-wide
  scan found 50 callers, 21 in `Splash.s`; still unchecked. W5-C's H4 stands.
* ⚠ **Did not revisit `FileGetPath` `0x82516550`'s carving** (W5-C H1 / W6-A §6)
  — a splits question, unchanged by anything here.
* ⚠ **Did not verify the `_bijection_arbitrary` semantics** of the map beyond
  preserving the file's exact serialization.

---

## 7. Handoffs, in the order I would fund them

| # | handoff | evidence in hand |
|---|---|---|
| H1 | **Are the five named pointer-element `list<T*>::insert` addresses real, or is `0x823d14c0` mis-named?** Settles §4.1 and unlocks **904 B + 1 fn** | §4.1: `_M_create_node<T*>` has 1 named address but `insert<T*>` has 5; fan-in 289 across 118 files; retail's own `list<char*>` body calls the `Object*`-named address. A map-integrity question, **not** an alias one |
| H2 | **Audit group `0x822d8cc0`'s existing member** `~vector<ObjDirPtr<ObjectDir>>` — same per-`T` `__destroy_range_aux` shape this lane refused, installed on T1 | §4.2; withdrawal needs positive contradiction, per W7-D's precedent |
| H3 | **`NewFile` `fn_825173E0`: port the body, THEN name** — naming is measured non-negative and injective | §5: 524 B/46 vs 424 B/27; drop the `Notify` branch, the `gNullFiles`/`NullFile` path and `UsingCD` |
| H4 | **`FileLocalize` = `fn_82516E28`, 408 B, unpaired** | §5: identified two ways; our 548 B body carries `GetGfxMode`/`HongKongExceptionMet`/`strstr` that retail's does not |
| H5 | `FileMakePath`'s last 33 charges — still needs a different mechanism | W7-A §1.3, four spellings refuted with recompile proof |

---

## 8. Reusable lessons

* ★ **Count callers on the OBJECT side, not the site side, before naming.** "35
  sites / 30 callers" is a cost; "51 objects reference `FileRelativePath`, 1
  references `FileRelativePathBuf`" is a **decision**. One grep over the built
  objects tells you whether MSVC inlined your wrapper, and that is the entire
  question — an un-inlined caller already spells the name you are about to
  install, so the naming is free. W5-C was bitten by the inlined case and W7-A
  deferred for want of exactly this number.
* ★★ **"Body-port first, name second" has a mechanism, not just caution:**
  porting the body is what makes the function too big to inline, and being
  un-inlined everywhere is what makes the naming free. The two steps are causally
  ordered, not merely conventionally.
* ★★ **When the row is unpaired, the score is not available as an instrument —
  use the COMDAT bytes.** A relocation-masked comparison against the target obj
  gave "904 B vs 904 B, 22 relocs vs 22 relocs, 0 of 226 words differ" *before*
  any measurement, which converted a blind port into a verified one and made the
  next step's prediction exact rather than hopeful.
* ★★ **A `none`-ruler control that MOVES on a source+map change is evidence, and
  it is the mirror image of the alias hazard.** `none` is blind to relocation
  names; +904 B there with the graded ruler withholding it is a direct proof that
  the only residual is names. The hazard rule (flat `none` cannot clear an alias)
  applies to map-**only** patches; do not over-generalise it into "never read the
  `none` control".
* ★★★ **The strongest argument for an alias and the strongest argument against a
  map name are THE SAME OBSERVATION.** "Retail's provably-`char*` body calls the
  address named `list<Object*>::insert`" supports *either* a fold *or* a
  mis-naming, and nothing in it discriminates. When your best evidence is
  compatible with both worlds, you have not measured the thing you think you
  measured — and the permissive branch is the one that pays you, which is exactly
  when to distrust it.
* ⚠ **A population control computed on MASKED bodies is the wrong population for
  a family whose fold classes are set by the relocation.** The gate's "48 retail
  bodytwins" is dominated by non-pointer `T`; it is a correct refusal input but
  not a correct *refutation*. Say UNPROVABLE when that is what you have — W7-D
  drew the same distinction, and collapsing it into REFUTED is how a live vein
  gets closed.
* ★ **The `5/N` screen closed to five decimals for the third consecutive lane**
  (predicted 99.955752, measured 99.955750) — but only after confirming the
  charge KIND, since W7-C measured it over-predicting ~5× on immediates. And
  `mpn == fuzzy` on both of this unit's residual rows shows relocation-name
  charges cut **both** rulers, so such a row pays its bytes and its function
  together or not at all.
