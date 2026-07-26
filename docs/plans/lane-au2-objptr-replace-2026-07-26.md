# laneAU-2 — the `ObjPtr<T>::Replace` family (2026-07-26)

**Branch:** `laneAU-2` · **Merge-base:** `24f508fb` · **Worktree baseline:**
37,282 strict (`measures.matched_functions`), reproduced on two independent
full `fresh_report.sh` legs before any edit.

Seeded by `docs/plans/lane-ar-map-ownership-2026-07-26.md` §9 item 6 ("SOURCE
defect: `ObjRefConcrete<T,ObjectDir>::Replace` is missing from the tree") and
§10's generalised lever ("a pin whose function is unmapped HIDES the source
defect"). Cross-referenced against `docs/plans/lane-ae-unemitted-symbols.md`
(the unemitted-symbol class, which used the same seed).

---

## 1. What the function actually is

AR's byte-decode was right; its *class attribution* was wrong, and the
difference is what unblocked the fix.

Retail `0x82314B70` (unit `default/InlineHelp`) and `0x823BDAB0`
(`default/CharInterest`) are both **0x60 = 96 bytes** and decode identically
modulo the RTTI type descriptor:

```
mflr r12 / stw r12,-0x8(r1) / std r31,-0x10(r1) / stwu r1,-0x60(r1)
mr    r31, r3
mr    r3,  r5                  <- `to` hoisted into the dyncast arg slot
lwz   r11, 0x8(r31)            <- mObject @ 0x8
cmplw cr6, r11, r4             <- vs `from`
bne   cr6, <epilogue>
lis/addi r6 = <T type descriptor>          (.?AVUIColor@@ / .?AVCharEyeDartRuleset@@)
lis/addi r5 = lbl_82C649D4                 (.?AVObject@Hmx@@)
li r7,0 / li r4,0
bl  fn_8282A0C8                <- __RTDynamicCast
mr  r4,r3 / mr r3,r31
bl  fn_8238B130                <- SetObjConcrete (one ICF-folded copy, shared
                                  by every instantiation)
```

i.e. exactly

```cpp
void Replace(from, to) { if (mObject == from) SetObjConcrete(dynamic_cast<T*>(to)); }
```

That is **rb3-Wii's `ObjPtr<T1,T2>::Replace` verbatim**
(`../rb3/src/system/obj/ObjPtr_p.h:45-61`):

```cpp
virtual void Replace(Hmx::Object *o1, Hmx::Object *o2) {
    if (mPtr == o1)
        *this = dynamic_cast<T1 *>(o2);
}
```

`*this = T1*` is `operator=`, which in our tree is `SetObjConcrete`. rb3-Wii is
the correct oracle here, not DC3: DC3 has moved to a **one-argument**
`ObjRefConcrete::Replace(Hmx::Object*)` that delegates to `SetObj`
(`../dc3-decomp/src/system/obj/Object.h:224`), a genuinely newer model that does
not describe RB3 retail.

### ★ The class attribution — AR's premise refuted on one point

`ObjRefConcrete<T,ObjectDir>::Replace` **is not the symbol, and never can be**:
`ObjRefConcrete` is abstract in this tree, and `?Replace@?$ObjRefConcrete@…`
exists in neither retail nor our build. The owning class is `ObjPtr<T>`, whose
`mObject` sits at 0x8 (the `lwz r11,0x8(r31)` above; `ObjDirPtr` keeps its
pointer at 0x4 and is therefore excluded by the same instruction).

And the symbol was **not missing from our tree at all** —
`?Replace@?$ObjPtr@VUIColor@@@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z` is defined in
`build/45410914/src/system/ui/InlineHelp.obj` and the `CharEyeDartRuleset`
instantiation in `.../char/CharInterest.obj`, on a clean post-full-build tree.
What was wrong was its **body**.

## 2. Why it read a false 95.2 %

Our `ObjRefConcrete<T,ObjectDir>::SetObj` — a *different* function — is

```cpp
if (mObject != root_obj) SetObjConcrete(dynamic_cast<T1 *>(root_obj));
return mObject;
```

which is the retail `Replace` body with three edits: the branch polarity flips
(`beq` vs `bne`), the dyncast argument is `r4` instead of `r5`, and there is a
trailing `lwz r3,0x8(r31)` for the return value. 22 of 25 rows identical =
**95.208 % normalized / 94.2 % raw** (AR quoted 95.62; my build says 95.208 —
the shape of the claim is right, the digit isn't). Our body is **100 B**, the
target **96 B** — that is AR's "SetObj 100-vs-96" question, and the answer is
**the same root cause, not a second lead**: the 96 B is not our SetObj at all,
it is `Replace`, and the 4-byte delta is exactly the `lwz r3,0x8(r31)` that
`SetObj` needs and `Replace` (void) does not.

★**Fuzzy % is not identity evidence, again.** 95.208 % on a 96-byte function
with the correct callee, the correct RTTI descriptors and the correct field
offset is still the wrong function.

## 3. The AE / AR contradiction: a staleness, not a disagreement

`lane-ae-unemitted-symbols.md` recorded that the map "contains **zero** entries
naming `SetObj@…ObjRefConcrete`" and handed the seed off as blocked. AR recorded
a false 95.62 % *propped up by* such an entry. Both are correct at their own
baselines:

* AE's baseline was **30,093**.
* `git log -S` puts the introduction of both lines in **`bbc12abc`**
  — *"decomp(laneAK): byte-class bijection -> +1,850 (32,150)"*, i.e. **after**
  AE measured and **before** AR did.

laneAK's byte-class bijection is what manufactured the false pairing: our
`SetObj` body and retail's `Replace` body differ by only three instructions, so
a byte-similarity bijection finds it irresistible.

## 4. Measurement — three legs, two builds each, unit-agnostic

All in `/home/free/tmp/wt-laneAU-2`, `measures.matched_functions`, protocol per
CLAUDE.md (`git checkout -- config/45410914/symbols.txt`, `rm -f
build/45410914/{report.cache,target_symbol_renames.stamp}`, `touch
config/45410914/config.yml`, `fresh_report.sh`).

| leg | change | strict | Δ |
|---|---|---:|---:|
| BASE | none (2 builds, both 37,282) | **37,282** | — |
| 1 — cost | **map repoint only**: the 2 VAs `SetObj` → `Replace` | **37,282** | **+0** |
| 2 — pay | leg 1 + the `Object.h` body fix (2 builds, both 37,289) | **37,289** | **+7** |
| 3 — family | leg 2 + 19 more map entries, after de-staling (2 builds, both 37,307) | **37,307** | **+18** |

**Net +25, 0 regressions**, identical by `(unit, name)` and by name-only
multiset (37,068 → 37,093 raw `Matched@100%`).

### ★ The headline did NOT drop first — and the reason generalises

The brief (and fleet doctrine) predicted a cost leg: retiring a scoring map
binding forfeits the whole function, with no byte-pairing fallback. **Measured
cost here: exactly 0.** The reason is that `measures.matched_functions` counts
**strict-100 only**, and the false binding scored **95.208 %**, not 100 %. So
the corrected rule is sharper than "retiring a scoring binding is expensive":

> Retiring a wrong binding costs **the number of strict-100s it holds**, which
> for a *near*-miss binding is **zero**. A false partial is free to retire; only
> a false **100 %** is expensive.

That is worth knowing because false partials are exactly what byte-similarity
bijections produce — a bijection will never claim a false 100 %, so **every
identity a bijection got wrong is free to correct.**

### Side gains (5 of the 25) are real, not stale-obj phantoms

`fn_823B5C68` (CharBoneDir), `fn_8241CC78` / `fn_8241CCBC` (Mesh),
`fn_8264C458` / `fn_8264C49C` (AppMiniLeaderboardDisplay) flipped as a
consequence of the body change: the new `Replace` is a **non-leaf** (it calls
`__RTDynamicCast`), so its `.pdata`/unwind shape changed, and objdiff's
byte-pairing of funclets picked up five previously-unpaired ones. The tell for a
stale-obj phantom is a gain in a unit the diff does not touch; `Object.h` is in
every one of these TUs' include closure, so the tell does not fire.

## ★★ 5. A real objcache correctness bug, found by this lane

**Symptom.** After the `Object.h` edit and a full 865-object rebuild, 17 objects
still contained the **old** 36-byte `ObjPtr<T>::Replace` body (27 COMDATs).
Every one of them is in a **PCH-eligible directory** (`synth` 11, `gesture` 6,
`hamobj` 5, `utl` 2, `os` 2, `flow` 1 — counting COMDATs); **zero**
non-PCH-eligible objects were affected.

**Decisive experiment** (`src/system/synth/Sequence.obj`, repeated):

| | `?Replace@?$ObjPtr@VSeqInst@@@@…` COMDAT size |
|---|---|
| `rm obj && ninja` (objcache **on**) | **36 B** — the pre-edit body |
| `rm obj && OBJCACHE=off ninja` | **96 B** — the current source |
| `rm obj && ninja` (objcache **on**) again | **36 B** — reproducible |

So for PCH TUs, objcache's key does not fully capture the content of the
PCH-covered headers (`src/system/decomp_pch.h` = `obj/Object.h` + `os/Debug.h`).
ninja correctly marks the TU dirty and re-runs the compile edge, but objcache
answers it from a pre-edit entry. **The failure is silent and invisible to
`ninja -t deps`.**

**Blast radius.** Any lane that edits `obj/Object.h` or `os/Debug.h` — the two
most load-bearing headers in the tree — and measures without disabling objcache
is under-counting its own change on ~281 TUs. In this lane it cost exactly one
strict match (`?Replace@?$ObjPtr@VSeqInst@@@@…` in `default/Sequence`,
37,306 → 37,307), but the number is a property of how many *mapped* symbols
happen to live in those objs, not a bound on the bug.

**Workaround until objcache is fixed:** after a PCH-header edit, verify with a
content probe (this lane used a COFF COMDAT-size scan for the changed symbol),
and rebuild the offenders with `OBJCACHE=off`. A blanket `OBJCACHE=off` full
rebuild also works and is honest, just slow.

## 6. The family — the defect repeats once per instantiated `T`

`scripts/harvest/objptr_replace_family_scan.py` (new, committed) recognises the
retail body structurally in the dtk `.s` dumps, resolves the `r6` RTTI
descriptor to a class name out of `auto_06_*_data.s` (`.?AV<Name>@@`), and only
proposes a map entry when our object **for that same pinned unit** already
defines the symbol.

Inside pinned units: **26** bodies · 2 already mapped (the AR pair) · **19
proposable** · 5 not emitted in the owning unit. **17 of the 19 went straight to
100 %** on the first measured leg; the 18th (`SeqInst`) was the objcache
casualty above and landed after de-staling; the 19th is the `RndWind` case in
§7.

`ObjDirPtr<T>::Replace` is excluded by construction — its pointer is at `0x4`,
so `lwz r11,0x8(r31)` discriminates the two families in one instruction.

## 7. Residue — named worklist

1. **`0x823A53E0` `?Replace@?$ObjPtr@VRndWind@@@@…` in `default/CharHair` —
   75.0 %, 6 inserted instructions**, all ours:
   `cmplwi r11,0 / beq / lwz r10,0x4(r11) / lwz r10,0x4(r10) / add r11,r10,r11 /
   addi r11,r11,0x4`. That is an MSVC **virtual-base upcast** emitted by
   `mObject == from` because our `RndWind` derives `Hmx::Object` virtually.
   Retail emits a raw pointer compare, so **retail's `RndWind` does not reach
   `Hmx::Object` through a virtual base**. This is a class-layout lead for
   `RndWind`, not a `Replace` lead — and it is a free oracle: the other 18
   instantiations prove the `Replace` source is right, so the 6 extra
   instructions isolate the layout defect exactly.
2. **5 family VAs whose symbol is not emitted in the owning unit** — a
   splits / TU-composition question, not a map one:
   `0x822751B0 Accomplishment RndEnviron`, `0x822CB190 StreakMeter Sequence`,
   `0x822E42E0 GemTrackDir TrackInterface`, `0x8271E0C8 Sfx SfxInst`,
   `0x8271ECA8 Sfx Sfx`.
3. **The scanner only sees pinned units.** Every `ObjPtr<T>::Replace` in
   unpinned `.text` is invisible to it and unclaimable until its TU is pinned.
   Re-run the scanner after any splits wave — it is idempotent and cheap.
4. **`ObjRef::ReplaceList` (`src/system/obj/Object.cpp:142`, X360 branch) calls
   `ref->Replace(nullptr, obj)`.** With the null-`from` short-circuit removed
   (retail has none), that call is now a no-op on the X360 build. The X360 build
   is never executed and `HX_NATIVE` is untouched, so nothing regresses — but it
   means **our `ReplaceList` is itself wrong**: retail must pass the dying
   `Hmx::Object*`. Worth a lane; it is currently unpaired in `report.json` so
   there is no measurement pressure on it either way.
5. **`ObjDirPtr<T>::Replace` (`src/system/obj/Dir.h:101`) still carries the same
   two defects** the `ObjPtr` version had — a `from == nullptr` short-circuit
   and a ternary `o ? dynamic_cast : nullptr`. No pinned target was found for it
   by this scan, but the shape is almost certainly wrong for retail too.
6. ★**laneAK's byte-class bijection is a systematic producer of this defect
   class.** It paired retail `Replace` onto our `SetObj` because they differ by
   three instructions. Every bijection-introduced entry that reads a high-but-
   not-100 fuzzy is a candidate false identity — and per §4 they are all **free
   to retire**. A sweep of `_bijection_arbitrary` + the bijection's high-fuzzy
   non-100 entries is the natural next lane.

(filled in after the A/B)
