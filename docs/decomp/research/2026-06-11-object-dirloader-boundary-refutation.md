# Object / DirLoader / Dir Triple Boundary — REFUTED (2026-06-11)

**Verdict: REFUTE.** The sliver-pin hunt's candidate #3 (Object.cpp triple boundary
correction, est +10–25) does **not** yield honest matches. A measured relocation produces
**+17 nominal** but **fails the WAVE-1 honesty gate** (15-fn contiguous foreign run; only
**3 of 54** attributed matches are genuine Object.cpp methods). The +17 is almost entirely
**false attribution** of already-matched foreign content shuffled out of DirLoader's pin.

- **Baseline:** main `d922dad`, worktree `objdir-boundary`, **7027** matched (fresh, verified).
- **Tested move:** Object `[0x82737FE8,0x82738160)` → `[0x82733668,0x8273849C)`;
  DirLoader hi `0x82737FE8` → `0x82733668` (shared-boundary, both pins edited in one change).
- **Measured:** **7044 (+17)**, re-run-confirmed (divergence-warning FP cleared).
- **Reverted** to baseline 7027 (rebuild-confirmed). No commit of the splits change.

---

## The dispute this settles

Two dossiers disagreed on DirLoader's Object/Symbol/DataNode orphans:

- **Sliver-hunt (`2026-06-11-sliver-pin-hunt.md` §2.3):** Object.cpp is a 0x178 sliver pin
  (0/2) while its real TU sits inside DirLoader's over-extended pin; the orphan-doc's
  "STL-attributed foreign" verdict is *wrong* — these are real Object bodies mis-pinned.
  Proposes triple boundary move, est +10–25.
- **Orphan-worklist (`2026-06-11-obj-orphan-worklist.md`, DirLoader rank 8):** Object(9)/
  Symbol(3)/DataNode(3) are "all STL-attributed foreign," CLEANUP-SAFE.

**Both are partly right; the actionable conclusion is the orphan-doc's (no boundary move),
but for a reason neither stated.**

### What the asm + COFF actually show

The compiled `Object.obj` (src/system/obj/Object.cpp, 265 EXTERNAL fns) **DOES define** the
contested Object methods — they are **real Object.cpp bodies**, not STL-attributed foreign:

| VA | symbol | defined in |
|---|---|---|
| 0x82733668 | `InitObject@Object` (retail `UAA`, our `QAA`) | Object.obj |
| 0x827358A8 | `DataDir@Object` | Object.obj |
| 0x82735B40 | `SaveType@Object` | Object.obj |
| 0x82735F98 | `Save@Object` | Object.obj |
| 0x82735FE0 | `HandleType@Object` | Object.obj |
| 0x827363D8 | `HandleProperty@Object` | Object.obj |
| 0x827370A0 | `PropertyClear@Object` | Object.obj |
| 0x82737FE8 | `Object::Object` ctor | Object.obj |
| 0x82738050 | `~Object` | Object.obj |
| 0x827381E0 | `RegisterFactory@Object` | Object.obj |
| 0x82738458 | `Load@Object` | Object.obj |

**So the sliver-hunt was RIGHT that the orphan-doc's "STL-attributed foreign" label is
mechanically wrong** (these pair against a real obj, not STL noise).

### Why a boundary move still fails (the part both docs missed)

The Object methods are **not contiguous**. The VA region `0x82732F68–0x82738458` is a dense
**three-way interleave**:

1. **Object.cpp** methods (8 in the zone).
2. **`obj/Utl.cpp`** free functions — `PathName`, `DefaultSubdirAction`, `SafeName`,
   `IsASubclass`, `SubDirStringUsed`, `SubDirHashUsed`, `GetPropertyVal`, `ReserveToFit`,
   `MakeFileList`, `MakeFileListFullPath`, `ListSuperClasses`, `IsContextUsed`. These are
   defined by **neither** Object.obj nor DirLoader.obj. `obj/Utl.cpp` is **UNWIRED** in
   rb3-xenon (only `rndobj/`, `ui/`, `meta_band/` Utl.cpp are compiled) — confirmed in
   `objects.json` and via DC3/rb3-Wii oracle (`?MakeFileList@@`, `?IsASubclass@@`,
   `?ListSuperClasses@@`, `?SubDirStringUsed@@` all live in `system/obj/Utl.cpp`).
3. **DirLoader STL instantiations** — `_S_merge`/`_S_sort@...ClassAndNameSort@DirLoader`
   (DirLoader.obj), plus `_M_create_node@_Rb_tree<FlowNode...>` and a farm of ~32 EH/vtable
   funclets at `0x82737C3C–0x82737FBC` (retail `??_E`/`??_G`/unwind thunks our build inlines).

VA-ordered class map of the proposed pin (`O`=Object method, `.`=foreign/unnamed):

```
O.........O.......O.........................................................O...OOOO
```

- **84 functions in the proposed range; 8 are Object methods.**
- **Longest contiguous non-Object run: 57.**

### Measured outcome of the relocation (the honest accounting)

| | baseline | after move | Δ |
|---|---|---|---|
| Object pin | 0/2 | 54/148 | +54 |
| DirLoader pin | 93/233 | 56/95 | −37 |
| **global matched** | **7027** | **7044** | **+17** |

Of the 54 functions newly attributed to Object, **only 3 are genuine Object methods**
(`HandleType`, `PropertyClear`, `~Object`). The other **51 are foreign** (Utl.cpp free fns,
DirLoader STL, EH funclets) that *already byte-matched inside DirLoader's pin* and were merely
**moved** into Object's pin — costing 37 DirLoader matches and creating a **15-fn contiguous
foreign-or-unnamed <100% run** inside the new Object range.

The other 5 Object methods in the zone (`InitObject`, `SaveType`, `Save`, `HandleProperty`,
`GetPropertyVal`) read **0%/near-miss** — real bodies that need **porting**, not pinning.

### WAVE-1 honesty gate

> keep iff matched > 0 AND no ≥8-contiguous foreign fn_@0% run in any new range.

**15-fn contiguous foreign run ≫ 8 → GATE FAILS.** The +17 is dishonest attribution
(foreign byte-matched content shuffled between two pins), not 17 real Object.cpp matches.
There is **no contiguous honest sub-range** — the 3 genuine Object matches are each isolated
between foreign neighbors, so no tighter pin captures them without a foreign run. **REVERTED.**

## Synthesis of the two dossiers

- The **orphan-doc's operational verdict is correct** (do not move the boundary; the region
  won't pair as a block) — but its *mechanism* label ("STL-attributed foreign") is wrong for
  the Object entries: they are real Object.cpp bodies, just physically interleaved with
  unwired-Utl.cpp / STL / funclet content by retail COMDAT placement, not contiguous TU
  grouping. **Do NOT delete the DirLoader Object-class map entries** as "foreign" — they are
  genuine Object.cpp methods; deleting them would discard valid pairing names for a future
  port-driven match.
- The **sliver-hunt's premise is refuted** for this candidate: "/O1 TU contiguity → one .s
  recon redraws boundaries" does **not** hold here. This region is COMDAT-interleaved, so the
  sliver/boundary mechanic (which assumes contiguous TUs) is the wrong tool. Object.cpp's
  match gains are **port-bound**, not pin-bound: close `SaveType`/`Save`/`HandleProperty`/
  `InitObject`/`GetPropertyVal` (and the ctor) by body-porting from the DC3/rb3-Wii oracle.
  Only after those bodies match would a clean Object pin be worth revisiting, and even then
  the 32-funclet block at 0x82737C3C–0x82737FBC and the unwired Utl.cpp interleave cap it.

## Recommendation

1. **Do not relocate** Object/DirLoader/Dir boundaries. Leave the splits.txt pins as-is.
2. **Do not bulk-delete** DirLoader's Object/Symbol/DataNode orphan map entries — they are
   real-body names, useful once those methods are ported. (Revise the orphan-doc's
   CLEANUP-SAFE entry for DirLoader accordingly.)
3. The real Object.cpp lever is **per-fn body-port** of the 5–6 near-miss/0% Object methods
   from the oracle (`obj/Object.cpp` exists in both DC3 and rb3-Wii). That work is independent
   of any pin change and would yield honest matches inside the *existing* sliver-adjacent
   attribution once the bodies match.
4. Optional separate lever: **wire `obj/Utl.cpp`** (currently unwired). It owns the ~12 free
   functions interleaved here; wiring + pinning it could harvest those as a real TU — but
   that is a *different* candidate, not the Object boundary move.

---

*Measured + refuted on worktree `objdir-boundary` (from `d922dad`). Baseline 7027; tested move
7044 (gate-failing); reverted to 7027.*
