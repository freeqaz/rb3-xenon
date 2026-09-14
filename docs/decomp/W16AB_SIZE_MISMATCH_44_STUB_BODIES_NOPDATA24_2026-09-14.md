# W16-AB — the 44 REAL two-sided size mismatches and the 24 NO_PDATA rows

**Lane:** W16-AB · **Date:** 2026-09-14 · **Branch:** `w16-ab` (off main `196fe28c`)
**Instrument inherited:** `tools/twosided_extent.py` (W16-X, doc
`W16X_VBASE_DTOR_DIVERGENCE_TWO_SIDED_SIZE_2026-09-14.md` §Item 2) — our COMDAT `.text`
span with EH funclets excluded vs retail `.pdata` BeginAddress..end.

---

## Headline

**The brief's premise for Item 1 was REFUTED, not executed — and the refutation
generalised into a reusable detector.**

W16-X's 44 REAL size mismatches were framed as our-side defects ("three of our
COMDATs are stubs; write the bodies"). They are not. In **18 of 18** addresses
examined across Items 1 and 2, our source is correct and **the map names the wrong
retail address**. The alias table was forgiving the resulting divergence, so the
rows advertised themselves as near-misses (74–96% fuzzy) that could never close.

> **`survivor_vs_retail == SIZE` is a WRONG-MAP-NAME DETECTOR, not an our-side
> defect detector.** An alias whose two sides differ in size cannot be a fold
> (different-size COMDATs never fold under `/OPT:ICF`); W16-X read that correctly
> but attributed the divergence to the wrong side. In 40 of 40 remaining
> memberships the *folded* spelling's size equals retail's exactly while only the
> *survivor* disagrees — i.e. **each group already carries the verified-correct
> spelling.**

Item 3 (the NO_PDATA stratum, where size is unlicensed) found the **same disease
on a different instrument**: one more proven-wrong map name, plus two alias
memberships contradicted on bytes.

---

## Measured result

| stage | Δ functions | Δ bytes | predicted | outcome |
|---|---:|---:|---|---|
| Item 1 — three "stub bodies" | **+3** | **+352** | +3 / +352 | **exact** |
| Item 2 — four island re-homings | **+4** | **+392** | +4 / +392 | **exact** |
| Item 3 — two alias withdrawals | **+0** | **+0** | Δfns 0, Δbytes ≤ 0 | **exact** |
| **lane total** | **+7** | **+744** | | 0 rows fell out at any stage |

`matched_functions` **43,301 → 43,308** · `matched_code` **3,988,808 → 3,989,552**.
Priced by set-diff of the `fuzzy==100` row set (`tools/rowset_snapshot.py`), never by
mismatch count, on full `rc=0` builds.

---

## Item 1 — the three "our-side stubs" are three WRONG MAP IDENTIFICATIONS

The brief asked for bodies. Reading retail's bytes at each address showed our bodies
already exist, are correct, and are simply bound to the wrong place. The decisive
screen in all three cases is **live-in arity / return value**: an MSVC mangled
signature constrains how many GPRs a body may read live-in, and a static nullary
(`SA…XZ`) may read none.

| addr | map said | our size | retail size | what the retail bytes actually show | true identity |
|---|---|---:|---:|---|---|
| `0x82553fc8` | `?Terminate@RndMat@@SAXXZ` | 4 B | 96 B | reads `r3` and nulls `*r3` — a destructor body. `SA…XZ` is a **static nullary**: it may read **zero** live-in GPRs | `??1CharCreatorPrefab@PrefabMgr@@QAA@XZ` |
| `0x82276cd0` | `?CancelOutstandingCalls@RockCentral@@QAAXPAVObject@Hmx@@@Z` | 12 B | 128 B | **one** live-in GPR; the signature needs **two** (`this` + `Object*`). Calls `0x82274748`, which the map independently names `?FinishLoad@PatchSticker@@QAAXXZ` — exactly what our `PatchDir::Poll` calls | `?Poll@PatchDir@@UAAXXZ` |
| `0x822dc9d0` | `?deallocate@?$StlNodeAlloc@VString@@@…QBAXPAVString@@I@Z` | 8 B | 128 B | **one** live-in, **returns a pointer**; `deallocate(String*,uint) const` needs three and returns `void`. Its single reloc targets `Object::New<RndMesh>`, which the map names at `0x822dc830` | `?NewCopyMesh@@YAPAVRndMesh@@PBV1@@Z` |

**What the alias was hiding.** Each group forgave the relocation-name divergence
between our correct spelling and the map's wrong one, so the row scored as a
high-fuzzy near-miss instead of an identification error. `src/band3/net_band/
RockCentral.cpp:369` is a correct 12 B forwarder; `src/system/bandobj/PatchDir.cpp:784`
maps onto `0x82276cd0` instruction-for-instruction; `src/system/bandobj/
ChordShapeGenerator.cpp:123` is `NewCopyMesh`.

**All three pins were CIRCULAR** — the classic hazard: a `splits.txt` `.text` span
carved to the size of a wrong map name, then cited as evidence for it. RockCentral's
nearest other code was **65 KB** away; the PrefabMgr island sat *inside* PrefabMgr's
own contiguous range.

★ **A live tooling gap, worth fixing outside this lane.** `0x82553fc8`'s argreg
refutation was **already recorded in the map's own `_denylist`** (lane W15) — yet the
binding was still live, because the denylist only governs `gen_target_map`
auto-emission and **never polices the applied map**. The address was lifted from
`_denylist` with a rationale appended to `_denylist_comment`.

`??1TubePlate@@QAA@XZ` was **retained** as a genuine fold of `~CharCreatorPrefab`
(both 96 B, zero relocations, byte-identical).

---

## Item 2 — the other 41 REAL mismatches (15 distinct addresses)

The 41 memberships collapse to **15 addresses**. Word-level, **15 of 15** have a
folded spelling that is a reloc-only match to retail; in **40 of 40** memberships only
the survivor's size disagrees. Four were repaired; eleven were scoped out (below).

### Repaired — four island re-homings (`.text` only; `.pdata` is derived output)

| addr | size | from | to | name bound |
|---|---:|---|---|---|
| `0x82295fb8` | 96 B | MeshAnim.cpp | BandDirector.cpp | `__destroy_range_aux<PropertyFilter@CameraManager>` |
| `0x822dcf58` | 96 B | HamMove.cpp | ChordShapeGenerator.cpp | `??0?$_Vector_base@VFace@RndMesh@@…` |
| `0x82425bc8` | 96 B | PracticeSection.cpp | TexBlender.cpp | `_M_range_insert<Key<ObjectStage>>` |
| `0x824c4998` | 104 B | HamCamTransform.cpp | TexBlender.cpp | `?_M_erase@?$vector@VCamShotCrowd@@…` |

Zero overlaps after each edit; no source unit drained (35/23/13/49 spans remained —
a unit drained of its last `.text` block must have its whole entry deleted or the
build hard-fails on a 42-byte obj). `0x824c4998` had **no map entry at all**, which is
why it had no report row; it was treated as a newly-named address.

### The rival hypothesis was tested and LOST, with a control that could have failed

"Our struct is too big" would also explain the +4 size cluster (`String` 12 vs 8,
`PracticeStep` 28 vs 16, `NavItem` 40 vs 32). **Refuted:** 13 of 20 `vector<String>`
rows are *already* at `fuzzy==100` against retail, and the byte-exact
`vector<PracticeStep>` witness at `0x82346de0` uses retail stride `0x1c` = 28 — **our
exact size**. Our layouts are right; the addresses hold different instantiations.

---

## Item 3 — the NO_PDATA stratum, adjudicated on a different instrument

**The brief says 24 rows; I measure 25 memberships — over only SIX distinct
addresses** (two carry 11 and 10 memberships). This is six adjudications, not 25.

Size is unlicensed here: these are not `.pdata` BeginAddresses, because a
sub-`.pdata` stub that touches neither the stack nor LR gets no unwind record.

**Instrument:** `tools/icf_pair_adjudicate.py`, relocation-normalized body hashing.
**Both controls were run FIRST, and both discriminate:**

- `--selftest` → PROVEN on a positive control, REFUTED on a negative. PASS.
- `--chasetest` → REFUTES an **in-family decoy**: retail `hash_map<int,SongUpgradeData*>`
  vs our `hash_map<int,UIComponent*>`, **byte-identical bodies**, differing reloc
  targets. That is exactly the template-twin hazard this population is made of, so
  the instrument is calibrated for *this* stratum and not merely for easy cases.

**Extent source — stated, as the brief requires:** the dtk target-obj **COFF slice**,
whose boundaries come from **`symbols.txt` symbol sizes**, *not* `.pdata` (there is
none). This is the weaker of the two options — for sub-`.pdata` stubs dtk infers the
extent from flow analysis. Retail/our size agreement is therefore used as the
corroboration of the carve, and **every disagreement is flagged extent-uncertain below.**

### Classification

| addr | unit | survivor (map name at addr) | memb. | verdict | evidence |
|---|---|---|---:|---|---|
| `0x82772870` | StandardStream.cpp | `erase<vector<JumpInstance>>` | 11 | **PROVEN** ×11 | 92/92, **0 relocations**, flat *and* chased |
| `0x82b9b1f8` | GemManager.cpp | `erase<vector<ArpeggioPhrase>>` | 10 | **9 PROVEN / 1 CONTRADICTED** | 92/92 reloc-free; `RndText::Line` is 64 B **with a call** |
| `0x82336af8` | BandCharDesc.cpp | `?GetDeformClip@BandCharDesc@@` | 1 | **CONTRADICTED** | retail 40 B / 5 relocs vs our `swap<…>` 28 B / 0 relocs |
| `0x8278b7f0` | HamMove.cpp | `__uninitialized_copy<Node<ObjPtrVec>>` | 1 | **PROVEN** | 72/72, 0 relocations |
| `0x823eb548` | Anim.cpp | `__insertion_sort<RndPollable**>` | 1 | **PROVEN fold, WRONG MAP NAME** | see below |
| `0x827f42a8` | UIButton.cpp | `??_EUIButton@@$4PPPPPPPM@A@` | 1 | **STILL_UNDECIDED** | 2-word thunk; chain terminates in a symbol we do not emit |

### A prediction of mine that FAILED, and why it is the most useful line here

I pre-registered that the two `vector<T>::erase` clusters would **REFUTE** as template
twins. **They did not — all 21 proven memberships are byte-identical, and I had the
mechanism backwards.**

> For **trivially-copyable `T`**, `sizeof(T)` **cancels** between the pointer
> difference `(__last - __first)` and the element multiply, so `vector<T>::erase`
> degenerates into an inline byte-move with **no calls and zero relocations**. There
> is no per-`T` callee to disagree on, so every such instantiation folds to one body
> — which is why 21 different `T` share one 92 B retail function.
>
> The decoy shape **requires** a per-`T` callee. `RndText::Line` owns a `String`, so
> its `erase` must destroy elements and **calls `_M_erase`** — 64 B with a
> relocation. It can never fold with the trivial family.

**In both contradicted groups the alias had picked precisely the ONE member that
cannot fold as its folded spelling.** Both groups' own **T1 claims were refuted by the
same tool family that generated them**, flat and chased — a **generator defect in
`icf_alias_build.py` worth escalating**: it emitted T1 claims its own adjudicator
rejects.

### `0x823eb548` — a proven-wrong map name that this lane deliberately did NOT repair

Retail's 16 bytes at `0x823eb548`:

```
lwz  r11, -4(r3)   ; vtordisp
subf r3, r11, r3
addi r3, r3, -24   ; second this-adjustment
b    -> ?Replace@MsgSource@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z
```

That is a `$4PPPPPPPM@BI@` vtordisp adjustor thunk into `MsgSource::Replace`. It is
**not** an insertion sort over `RndPollable**` (ours is 80 B). Among our **15**
masked-body twins, **exactly one** has a matching relocation target, so the
identification is **unique**, not a coin flip among thunks:

> **`?Replace@MsgSource@@$4PPPPPPPM@BI@AAXPAVObjRef@@PAVObject@Hmx@@@Z`**

**Why it was not repaired — and why forcing it would be a defect.** That thunk is a
COMDAT emitted in four of our objs, **all under `band3/meta_band/`**
(BandMachineMgr, InputMgr, ModifierMgr, UIEventMgr), and there is **no `MsgSource`
heading in `splits.txt`**. `0x823eb548` sits inside `Anim.cpp`'s **engine** `.text`
span. So:

- renaming without re-homing leaves the base obj unable to define the name ⇒ the row
  reads **permanently 0%** (map/name economics: proving a name wrong does **not** make
  renaming safe);
- re-homing a 16-byte engine-region island into `meta_band` is geographically
  unsupported and would be a **circular pin** — carving a span to fit a name and then
  citing it as evidence, the exact error refuted in Item 1.

**Handed to an escalation lane with the evidence above.** The correct repair is to
find the engine TU that emits this thunk, not to move the island to fit.

### `0x827f42a8` — STILL_UNDECIDED, and a size premise that turned out to be an artifact

Retail's body, and ours:

```
+00: 8163fffc   lwz  r11, -4(r3)
+04: 7c6b1850   subf r3, r11, r3
+08: 00000000   <- relocated branch
+0c: 00000000   <- TRAILING WORD: inter-function ALIGNMENT PADDING (retail carve only)
```

Retail reads 16 B, ours 12 B — across **105 retail** vs **1,772 of our**
`??_E…$4PPPPPPPM@A@` thunks, i.e. a **uniform +4**. The bytes show the extra word is
`0x00000000` padding swept into dtk's carve, which has no `.pdata` to bound it.

⚠ **This is the STLPORT-1 shape exactly** — a uniform small size delta that looks like
a source bug and is an artifact of a one-sided extent. **Had I withdrawn this
membership on the size argument I would have repeated STLPORT-1's error.** The size
premise that put this row in the census is **void**.

A second hypothesis of mine also failed here, and the bytes killed it: retail's thunk
branches to `??_GUILabel`, not `??_GUIButton`, which looked like a contradiction. It
is not — our `??1UIButton` is **4 bytes** (a single branch into `??1UILabel`), and our
`??_DUIButton`/`??_DUILabel` are byte-identical **including relocation names**, so they
genuinely fold and retail keeps only `??_GUILabel`. **For a derived class that adds
nothing, the destructor chain *is* the base's.**

Verdict remains **STILL_UNDECIDED** and structurally so: the body is 2 informative
words (`lwz`/`subf`) shared by 1,772 of our thunks, the only discriminator is the
branch target, and our side's target `??_EUILabel@@UAAPAXI@Z` **is not emitted by our
build at all**, so the chain cannot be closed. Not contradicted ⇒ **not withdrawn.**

### Withdrawals

Two memberships withdrawn, each with a `withdrawn` record carrying the byte evidence
(`class: CONTRADICTED_DIFFERENT_BODY_CANNOT_FOLD`). Membership sets only **SHRANK**
(asserted in code) — a removal without a record is a clobber.
`tools/icf_alias_finder.py --validate`: **PASS, 0 CONTRADICTED (FATAL)**, 1,634 groups.

---

## Gates

Run in order, in the worktree, as the last actions after the last source edit.

- full build **rc=0**
- `scripts/verify_ruler_agreement.py --check` → **rc=0**, all four keys OK
  (`functionRelocDiffs=name_check`, `combineDataSections=true`,
  `combineTextSections=true`, `ppc.calculatePoolRelocations=false`)
- `scripts/verify_objs_patched.py --verify-manifest` → **rc=0**,
  `1212 decomp, 3115 target objects match` (`tree_sha256=3bd511bd2c663f4f`)
- `tools/native_build_gate.sh`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

---

## What this lane did NOT do, and why

1. **Did not write the three stub bodies the brief asked for (Item 1 LEAD).** Their
   premise was refuted on retail bytes — the bodies already exist and are correct.
   Writing them would have encoded a wrong identification into source.
2. **Did not predict a whole-population `StlNodeAlloc<T>::deallocate` effect.** The
   brief asked for it on the assumption the template was defective. It is not; there
   is no re-instantiation to price.
3. **Left 11 of the 15 Item-2 addresses unrepaired.** Five are
   **arbitrary-bijection classes** where the true name is not recoverable from bytes
   — `0x823d3918` (20 equally reloc-matching `Handle@X` candidates), `0x823f0b50` (2),
   `0x8248f1c0` (2), `0x82787718` (3), `0x827d5bb0` (3). ICF destroyed which name the
   call site meant. Six more have a unique folded spelling but a span that is **not**
   an exact-size island, so they need carving rather than a whole-line move:
   `0x822c83d0` (span 120 B vs function 108 B), `0x82389608` (120 vs 108),
   `0x8238c798` (104 vs 96), `0x823ea1c8` (112 vs 104), `0x822c5600` (472 B span, the
   function at offset `0x80`), `0x8231a578` (272 vs 224). Scoped out for risk/budget
   with the diagnosis recorded.
4. **Did not repair `0x823eb548`** despite a unique, byte-proven identification — no
   pinned unit at that address defines the spelling, and both available routes are
   defective (see above). Escalation item.
5. **Did not withdraw `0x827f42a8`.** It is undecided, not contradicted, and its
   apparent size evidence is a carve artifact.
6. **Did not fix the `_denylist` tooling gap** (a denylisted address can still be
   live in the applied map) — out of lane scope; flagged for a tooling lane.
7. **Did not fix `icf_alias_build.py`**, which emitted two T1 claims its own
   adjudicator refutes. Flagged for escalation.
