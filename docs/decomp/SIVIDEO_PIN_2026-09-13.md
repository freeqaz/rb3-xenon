# `??1SIVideo@@QAA@XZ` reads 0.0 because of a SPLITS PIN, not a source bug

**Status: diagnosed, NOT fixed. The fix is a `splits.txt` carve edit and is left
for a lane that can finish and land one.**

## The briefed premise was wrong

Lane thunk-readjudication-3/4 was handed this:

> `??1SIVideo` now reads 0.0 because our object emits no out-of-line `~SIVideo`,
> so its call site is charged to `??1DxMovie`. That is a genuine source bug.

Both halves are refuted by measurement:

| claim | measured |
|---|---|
| our object emits no out-of-line `~SIVideo` | **it does.** `??1SIVideo@@QAA@XZ` is a DEFINED symbol in `build/45410914/src/system/rnddx9/Movie.obj`, section 151 (COFF symtab walk, `sec>0`). |
| its call site is charged to `??1DxMovie` | **`??1DxMovie@@UAA@XZ` reads fuzzy 100.0000** in `default/system/rnddx9/Movie`. It is not charged anything. |

## What is actually wrong

`??1SIVideo@@QAA@XZ` sits at `0x8273E040`. `config/45410914/splits.txt` pins that
address to **`system/rnddx9/CubeTex.cpp`**, inside the carve
`.text start:0x8273DDD8 end:0x8273E048`. Our compiler emits the symbol from
**`Movie.obj`**. objdiff pairs within a unit, so the row cannot pair and reads
`-1.0` (unpaired), which the report renders as 0.

It is not a one-off. The same carve family mis-pins a second DxMovie symbol:

| target VA | symbol | pinned to | actually defined in |
|---|---|---|---|
| `0x8273E040` | `??1SIVideo@@QAA@XZ` | `system/rnddx9/CubeTex.cpp` | `Movie.obj` |
| `0x8273E478` | `?ClassName@DxMovie@@UBA?AVSymbol@@XZ` | `system/rnddx9/CubeTex.cpp` | `Movie.obj` |

Verified directly: walking both objects' COFF symbol tables, **`Movie.obj`
defines both symbols and `CubeTex.obj` defines neither.**

## Why retail has an out-of-line `~SIVideo` at all

Worth recording, because it explains why the source looked suspect. Our header is
`~SIVideo() { Reset(); }` (inline, `src/system/rndobj/SIVideo.h:7`), and retail
agrees: the **normal** path in `??1DxMovie@@UAA@XZ` (`0x8273E370`) does *not*
call `~SIVideo` — it inlines it to a direct `bl 0x82B89B68`
(`?Reset@SIVideo@@QAAXXZ`) on `this-0x30`.

The out-of-line COMDAT exists solely because the **EH unwind funclet**
`fn_8273E444` calls it:

```
8273E454  lwz  r11, 0x84(r31)
8273E458  subi r11, r11, 0x5c
8273E45C  addi r3,  r11, 0x2c      ; &this->mVideo
8273E460  bl   fn_8273E040         ; ??1SIVideo@@QAA@XZ
```

Funclets do not inline member destructors, so `/EHsc` forces the out-of-line
form. Our build reproduces this — hence the defined symbol. Nothing about the
source needs changing.

## The fix, and why this lane did not take it

Handing `0x8273E040` and `0x8273E478` to `Movie.cpp` means splitting two existing
CubeTex carves, in a region where Movie and CubeTex are already interleaved
across 21 spans:

```
CubeTex  .text 0x8273DDD8 - 0x8273E048   <- contains ??1SIVideo at 0x8273E040
Movie    .text 0x8273E048 - 0x8273E090
...
CubeTex  .text 0x8273E414 - 0x8273E4D0   <- contains ?ClassName@DxMovie at 0x8273E478
Movie    .text 0x8273E4D0 - 0x8273E4E0
```

A splits edit re-splits every target object and has, by this repo's own ledger,
"claimed its fifth consecutive lane" (`3edc3d3c`). It is a different and larger
change class than the map edits this lane was landing, and the standing rule is
not to start a batch you cannot finish and land. Deferred deliberately, with the
diagnosis and the exact spans above so the next lane starts from evidence rather
than from the refuted premise.

**Do not "fix" this in `SIVideo.h` or `Movie.cpp`.** The source is already right;
changing it to chase the row would be metric-fitting against a carve defect.
