# WAVE8 — Path Review: cleanest route to a loadable SI RB3Enhanced.dll

**Date:** 2026-07-14
**Author:** review pass (analysis only — no hardware tests, no artifacts modified)
**Question:** With the import-thunk root cause now understood and fixed in
`deidax_thunks.py`, what is the cleanest, most robust path to a **loadable**
same-instrument (SI) `RB3Enhanced.dll` on RGH RB3DX?

---

## TL;DR — recommended primary path

**Adopt the header-preserving XexTool workflow and splice SI onto the
proven-loading stock base (task Option c). Retire `xex2pack` + `deidax` from the
critical path.**

1. `xextool -b` the stock nightly DLL → its **true** base PE (thunks intact — see
   §2, this is NOT what `xex1tool -b`/idaxex gives you).
2. Binary-patch the SI code-cave payload + the 4 detour branches into that base
   **in place, same image size** (stock has 60 KB + 34 KB free zero-runs, and the
   already-Xenia-validated spliced payload provably fits in stock's 0x40000
   image). Never touch the import-thunk region.
3. `xextool -c c` to recompress, recompute the page-hash chain, and re-sign with
   the devkit key.

This changes **exactly one thing** versus an image already proven to load
(`stock_repacked`): the SI code bytes. It inherits stock's proven import table,
valid devkit RSA signature, and page-descriptor layout untouched — the highest
probability that the next (expensive) hardware test isolates *SI-feature*
correctness rather than *container* correctness.

The canonical 8.7 MB from-source build stays the long-term maintainable artifact,
but it carries multiple simultaneous unproven container variables (§3) and should
be deferred until this recipe has proven the loader accepts our repacks.

---

## What I re-verified locally (read-only)

| Claim | Result |
|---|---|
| deidax fix byte-proof | `nulltest/null_v2.base` **== byte-identical ==** `stock_repacked.base` (sha `db0f08bd…`). Confirmed. |
| stock / stock_repacked / spliced_compressed all report | Compressed, **Valid devkit RSA**, base 0x84000000, entry 0x8401EF18, imports **xam 47 + xboxkrnl 58**. |
| `xextool -b` vs `xex1tool -b` (idaxex) on stock | **differ** (first at byte 184925 — the thunk region). idaxex mutates thunks on extract; XexTool does not. |
| `xextool -c c` round-trip of stock | Clean: **Compressed, Valid devkit RSA (re-signed), imports 47+58 preserved**, hashes recomputed. |
| stock base cave space | zero-runs of **0xEBF8 (60 KB)** @off 0x31408 and **0x87B3 (34 KB)** @off 0x784D — ample; old spliced SI fit inside the same 0x40000 image. |
| from-source imports | **xam 42 + xboxkrnl 26** — a *different module* from stock (cannot inherit stock's import table). |

---

## Q1 — Is the from-source `li r3,0; li r4,ord` stub the same class of bug?

**Yes — same class, and it is *not* a legitimate alternate mechanism the kernel
accepts as-is.** Two important clarifications:

- **What you see at base off 0x1F3B0 is an idaxex extraction artifact, not the
  shipped bytes.** `fromsource.base` was dumped with `xex1tool -b` (idaxex), whose
  `read_imports()` rewrites every type-1 function record in the *dumped* image to
  `li r3,ModuleIndex; li r4,ordinal; mtctr r11; bctr`. So `3860 0000 / 3880 00xx`
  is idaxex's mangling of whatever the real thunk was — exactly the transform
  `deidax` was written to reverse. You cannot read the true shipped thunk from an
  idaxex dump. (This is why the from-source IAT slots at 0x400 look clean —
  idaxex leaves type-0 variable records alone — while the thunks look mangled.)

- **The true shipped from-source thunk is still malformed, for a different
  reason.** `xex2pack.py::synthesize_import_block` emits, per function import, a
  **single** image rewrite: `word0 = 0x01000000 | ordinal` (module_index dropped,
  and **word1 is never written** — it keeps the K-link PE's original
  `lwz r11, iat_off(r11)` = `0x816B…`). Stock ships the two-word tagged pair
  `0x01|modidx|ord` / `0x02|modidx|ord` then `mtctr r11; bctr`. So the from-source
  thunk is missing the `0x02…` tag in word1 and drops module_index in word0 — the
  **identical defect** the fixed `deidax` addresses for the spliced path (per its
  own docstring: the RGH resolver "rejects at map time — stock ships
  `0x02000000|ord` there").

**On the resolver semantics:** the RGH/retail `XexLoadImage` import binder walks
each library's record VAs, reads the big-endian tagged value (`type = value>>24`,
ordinal = low 16), and for a **type-1 function** record patches the 16-byte thunk
in place to load the resolved export into r11 (the `0x01…`/`0x02…` pair → the
address-materialization pair) ahead of `mtctr r11; bctr`; for a **type-0
variable** record it writes the resolved pointer straight into the 4-byte slot.
This matches how xenia's `xex_module.cc SetupLibraryImports` treats type-0 vs
type-1 records. The split "IAT at 0x400 (type-0) + separate stub" is a legitimate
*shape* only if the stub's tag words are well-formed; the from-source stub's word1
is not a tag at all, so the binder does not see the pair it requires. It needs the
same conversion — the from-source build is **not** exercising a blessed alternate
path.

---

## Q2 — (a) fix spliced, (b) fix from-source, or (c) splice onto stock?

**Recommend (c), realized through the header-preserving workflow (§Q3).** Ranked:

- **(c) splice SI onto proven stock base — PRIMARY.** Inherits a byte-proven
  container: stock's import table (47+58, correct tagged thunks), valid devkit RSA
  sig, and page layout. Only the SI code bytes change. The SI cave payload already
  exists and was Xenia-validated (wave7 T1/T2/T3) in the old spliced build — the
  *only* thing wrong with that build was the container and a polluted import
  region. Re-splicing the clean payload onto `xextool -b`'s true stock base
  discards the pollution. Lowest container risk, and it makes `xex2pack`/`deidax`
  unnecessary for shipping.

- **(b) fix from-source — SECONDARY / long-term.** It's the canonical, maintainable
  artifact (SI compiled in, no byte-splicing), but shipping it means fixing **three
  independent container problems at once**: (1) malformed thunks (port the deidax
  two-word + module_index fix into `synthesize_import_block`), (2) page-hash
  recompute (xextool `-c c` does this), (3) invalid RSA — it packs *retail* with a
  zeroed sig; needs `xextool -m d -c c` to devkit-re-sign, and whether RGH enforces
  RSA on the `XexLoadImage` DLL path is still unproven. Too many simultaneous
  unknowns for the *next* hardware shot; pursue only after (c) proves the loader
  accepts our repack recipe.

- **(a) fix the existing spliced build — REJECT.** Its provenance is
  contaminated: 207 import "records" vs stock's ~105, a record pointing at off
  0x400 inside the PE header, and 55 splice bytes landing in the thunk region so
  its base is *not* byte-identical to clean stock. Reconstructing it is more
  fragile than re-deriving the splice from a clean base. Salvage only its
  *validated cave payload*, not its container.

---

## Q3 — Is `xex2pack`'s rebuild-the-container approach the wrong tool?

**For this deliverable, yes.** `xex2pack` rebuilds imports + security metadata
from scratch, which is precisely where every bug has lived (zeroed hashes → wave8
reject; zeroed RSA → from-source reject; single-word thunks → this review). Each
rebuilt field is a new failure surface, and you can't even *inspect* your own
thunk output afterward because idaxex re-mangles it on extract.

**The header-preserving XexTool workflow is strictly more robust and is the
recommendation:**

```
xextool -b  stock.dll  stock.base          # TRUE base PE (thunks intact, unlike idaxex)
#   ... binary-patch SI cave payload + 4 detours into stock.base, SAME SIZE,
#       never touching the import-thunk region ...
xextool -c c -o RB3Enhanced.si.dll stock.base-repacked   # recompress + rehash + devkit-sign
```

It never rebuilds imports or security info — it preserves stock's proven-loading
headers and only carries our patched code bytes. Verified above: `xextool -c c`
round-trips stock to Compressed + **valid devkit RSA** + intact 47+58 imports +
recomputed hashes in one step. The flat-image binary-patch pattern is already
implemented in `tools/xex_binpatch.py` (currently targeting the *game* default.xex
via the direct `file_off = pe_data_off + (VA − base)` mapping) — reuse that exact
tooling, retargeted at the DLL base and the DLL cave/detour VAs.

**Consequence:** the `deidax` fix, while correct and well-documented, drops off the
critical path for shipping SI — it's only needed if you rebuild via `xex2pack`
(i.e. the from-source path). Keep it as the reference for the thunk format and for
a future from-source container fix.

---

## Top risks

1. **Cave payload isolation & relocation.** The clean re-splice must take the
   *Xenia-validated SI hook bytes + detours* from the old spliced build but drop
   its polluted import region (207 records, thunk→0x400, 55 thunk-region bytes).
   Getting a pure, correctly-fixed-up cave blob (detour VAs and any absolute refs
   matching stock's layout) is the main real work and the main risk — it is what
   made the old spliced build murky. Prefer a purpose-built relocatable DLL cave
   blob (the `xex_binpatch.py` `cave.bin` + detour-table pattern) over carving
   bytes out of the contaminated image.
2. **Same-size constraint.** In-place patching assumes the SI payload fits the
   existing free cave without growing the 0x40000 image (evidence says it does).
   If it ever needs to grow, the pure header-preserving property is lost and
   XexTool must re-lay-out — recheck page descriptors/hashes on hardware.
3. **From-source RSA/devkit signing is unproven on the DLL load path.** If (c) is
   abandoned for (b), `xextool -m d -c c` devkit re-signing and the open question
   of whether RGH enforces RSA on `XexLoadImage` at all (only page-hash
   enforcement is proven) become live blockers.
4. **`968ebd4f` load status is in doubt.** FROMSOURCE-COMPRESS.md §5 records an
   on-screen "failed to load" for the spliced-compressed build after the earlier
   PASS claim. Treat WAVE8-STATUS.md's "SOLVED" as retracted; the only
   *unambiguously* proven-loading images are stock nightly and (by byte-identity)
   `stock_repacked` — anchor the splice to that base and re-establish a clean
   hardware PASS on the null (no-SI) repack first, then add SI.
