# The fold-thunk class is 9 alias groups and 27 refusals, not one finding

2026-08-12, lane H. Companion to `fold-thunk-alias-gate-2026-08-12.json` (the
per-pair verdicts) and `tools/fold_thunk_gate.py` (the instrument). Follows
`wrong-callee-triage-2026-08-12.md` (lane E), which produced the sub-class and
handed it to this lane.

## What was handed over

`functionRelocDiffs=name_check` charges 1,797 call sites over 36 pairs that lane
E classified `fold_thunk_naming`: retail's relocation names a ≤4-byte body with
a large fan-in, ours names something else. Lane E established that no source
edit reaches them — no spelling makes our relocation say
`??3BinStream@@SAXPAX@Z` at a site deleting something else — and that the only
repair is an alias on the fold survivor, which an earlier triage had forbidden
on the strength of a pooled-`operator delete` theory that lane E then measured
and refuted (`POOL_OVERLOAD(BinStream)` cost **10 complete functions** at the
control ruler).

## Lane E's load-bearing claims re-derived here

Both were re-measured before anything was built on them.

- **`??3BinStream@@SAXPAX@Z` @ `0x8240DDB0` is four bytes, `b MemFree`.** Read
  out of `orig/45410914/band.exe`: the single word is `483AE680`, a relative
  branch to `0x827BC430`, which the map names `?MemFree@@YAXPAX@Z`. Fan-in over
  the whole `.text`: **2,308** direct branches. Confirmed.
- **Retail's pooled roster is eight named classes.** Scanning `.text` for the
  `mr r4,r3 ; li r3,<size> ; b PoolFree` shape finds **14** bodies reaching
  `?PoolFree@@YAXHPAX@Z` @ `0x827BADB0`; **8** carry a map name — AnimTask(108),
  DataArray(16), DataFuncObj(44), DirLoader(168), DxMesh(428), NullLoader(32),
  SerialGroupSeqInst(84), WaitSeqInst(72) — matching lane E exactly. The other
  6 (`0x82376D30`, `0x82518348`, `0x827130A0`, `0x8271A0A8`, `0x82B5E028`,
  `0x82B64C20`) are unnamed in the map and unidentified here; lane E's "exactly
  eight" is a statement about the **named** roster and should be quoted that way.

## The gate: 36 pairs are 36 questions

"S is a folded thunk" is a statement about S. It does not establish that OUR
spelling is in that fold class. `tools/fold_thunk_gate.py` asks, per pair, the
condition `/OPT:ICF` actually tests:

> is our compiled COMDAT for the folded spelling byte-identical — modulo
> relocated fields, and with every relocation TARGET agreeing — to the retail
> body at the survivor's address?

Our side comes from `build/45410914/src/**/*.obj` via `tools/comdat_bytes.py`
(the dc3-side COFF extractor, adapted: it returns the unmasked bytes as well,
honours the symbol value so an EH prefix does not shift the body, and rebases
reloc offsets onto the definition). The retail side comes from `band.exe` under
the same instruction-aware mask.

**Why this is not the vacuous 4-byte compare the existing T1 tier refuses.**
`tools/icf_alias_build.py`'s T1 requires ≥4 words and ≥50% of the body unmasked,
because a masked `b X` compares equal to every `b Y`. This gate does not mask
the destination away — it **resolves** it through the map and compares the name.
For a 4-byte tail branch the destination *is* the function's entire information
content, so comparing it is the strongest test available rather than the
weakest. A body carrying no relocation at all (a bare `blr`) is genuinely
vacuous and is tiered separately as `FT-EMPTY`.

A second gate handles retail's own definition of the folded spelling. Our F
being one body with S is necessary, not sufficient: if retail's map also places
F on a different body, either that entry is wrong or our F is compiled wrong,
and an alias would paper over the second case.

| tier | condition |
|---|---|
| `FT1` | `addr(F)` is absent from the map, or retail's body there is the survivor's body. Nothing contradicts. |
| `FT2` | retail's body at `addr(F)` differs and the image discredits that entry: **zero** `.text` fan-in, or no `symbols.txt` extent (padding / a mid-function word). |
| `FT3` | retail's body at `addr(F)` is a **homonym** — a distinct function under another module that legitimately carries the same mangled name, witnessed by dc3's leaked `ham_xbox_r.map`. |
| `FT-EMPTY` | admitted, but the byte comparison carried no relocation and is vacuous; the corroboration is stated separately. |
| REFUSE | everything else, including every body-comparison failure, every unresolvable relocation, and every F our objects do not define. |

⚠ **A score-based discredit was tried and removed.** The first version admitted
a pair when *our own definition of F scored <20% at `none` against `addr(F)`*.
It fired exactly once — on the 2.70% lane E flagged — and it was wrong. That
2.70% measures objdiff pairing our four-byte Milo `::operator delete` against a
148-byte function that merely shares its mangled name (§"The `0x82BC6B70`
entry"). **A low score says two bodies differ; it can never say which of them is
misnamed.** Do not re-add it.

## Verdicts: 9 admitted / 1,599 sites, 27 refused / 198 sites

| sites | verdict | tier | retail callee (survivor) | our callee (folded) | why |
|--:|---|---|---|---|---|
| 1180 | ADMIT | FT3 | `??3BinStream@@SAXPAX@Z` | `??3@YAXPAX@Z` | dc3's map names `??3@YAXPAX@Z` at 3 addresses; retail's `0x82BC6B70` is `xaudio2:baseswfilter.obj`'s copy |
| 164 | ADMIT | FT2 | `??1LoaderGlitchContext@@QAA@XZ` | `??1FilePath@@UAA@XZ` | both are `b ??1String@@UAA@XZ`; map parks ours at `0x82812508`, fan-in 0 |
| 154 | ADMIT | FT-EMPTY | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@…` | `??3@YAXPAX0@Z` | both a bare `blr`; map parks ours at `0x82333E14`, no extent |
| 91 | ADMIT | FT2 | `??3BinStream@@SAXPAX@Z` | `OggFree` | map parks `OggFree` at `0x8249B200`, fan-in 0 |
| 60 | REFUSE | — | `??3BinStream@@SAXPAX@Z` | `??3DataArray@@SAXPAX@Z` | ours is 3 words to `PoolFree`, survivor is 1 word to `MemFree` |
| 44 | REFUSE | — | `??3BandHighlight@@SAXPAX@Z` | `??1MemDoTempAllocations@@QAA@XZ` | both 4-byte thunks, different destinations (`?MemPopTemp@@YAXXZ` vs `fn_827BC2A0`) |
| 22 | REFUSE | — | `??3BinStream@@SAXPAX@Z` | `??3Loader@@SAXPAX@Z` | body matches, but the map's other `??3Loader` (`0x823F4698`, fan-in 1) is not discredited |
| 18 | REFUSE | — | `??$__destroy_aux@UEntry@LocalePanel@@…` | `??1DataNode@@QAA@XZ` | ours is 6 words |
| 18 | REFUSE | — | `??1LoaderGlitchContext@@QAA@XZ` | `??1DataNode@@QAA@XZ` | ours is 6 words |
| 10 | REFUSE | — | `??3BinStream@@SAXPAX@Z` | `??3Task@@SAXPAX@Z` | body matches, map's other `??3Task` (`0x822EAB90`, fan-in 4) not discredited |
| 4 | ADMIT | FT2 | `??3BinStream@@SAXPAX@Z` | `??3RndLight@@SAXPAX@Z` | map parks it at `0x8270D7F8`, no extent |
| 3 | ADMIT | FT2 | `??3BinStream@@SAXPAX@Z` | `??3UIComponent@@SAXPAX@Z` | map parks it at `0x8282779C`, fan-in 0 |
| 1 | ADMIT | FT2 | `??3BinStream@@SAXPAX@Z` | `??3CharEyeDartRuleset@@SAXPAX@Z` | map parks it at `0x823BEEA8`, fan-in 0 |
| 1 | ADMIT | FT2 | `??3BinStream@@SAXPAX@Z` | `??3RndMat@@SAXPAX@Z` | map parks it at `0x824A8F74`, fan-in 0 |
| 1 | ADMIT | FT2 | `??3BinStream@@SAXPAX@Z` | `??3PlayerDiffIcon@@SAXPAX@Z` | map parks it at `0x823258D4`, fan-in 0 |

The remaining 22 refusals are 1–3 sites each; every row is in the JSON with its
own reason. Refused sites by cause:

| cause | sites |
|---|--:|
| our COMDAT is a different length from the survivor body | 114 |
| same length, different resolved callee | 52 |
| body matches but the map's other entry for our spelling stands | 32 |

**Three refusals are worth acting on rather than filing.**

- **`??1MemDoTempAllocations@@QAA@XZ`, 44 sites — a source divergence in
  `MemMgr.cpp`.** Both sides are 4-byte tail thunks. Ours branches to
  `?MemPopTemp@@YAXXZ` (80 B in our obj); retail's branches to `fn_827BC2A0`
  (48 B), which is byte-for-byte **the tail of ours**. Our `MemPopTemp` carries a
  32-byte `gNumHeaps` prologue retail's does not. Repair that and the pair
  becomes admissible on its own evidence.
- **`??3DataArray` (60) and `??3WaitSeqInst` (1)** — ours are `POOL_OVERLOAD`,
  three words to `PoolFree`; the survivor is one word to `MemFree`. Retail's call
  sites there really do reach the global delete. Not a naming artifact; needs a
  call-site oracle, and note that both classes are *correctly* pooled elsewhere
  (they are 2 of the 8 named pooled classes above), so this is about these sites,
  not about the overload.
- **`??3Loader` (22) and `??3Task` (10)** — our COMDATs **are** the survivor
  body, but the map also names each on a different 4-byte thunk (`b fn_82A808D8`
  and `b ?clear@_Rb_tree<Symbol,…>` respectively) with nonzero fan-in and no dc3
  homonym witness. Refused fail-closed. They need a semantic oracle, not a bigger
  hammer; a delete that tail-jumps into an `_Rb_tree::clear` is very likely more
  arbitrary fold naming, but "very likely" is not the bar here.

### The one vacuous admission, and what stands behind it

`??3@YAXPAX0@Z` (placement delete, empty) against the survivor at `0x826C3888`
is a bare `blr` on both sides — no relocation, nothing to resolve, so the byte
comparison carries no information. Its corroboration is a fan-in census instead:
`.text` holds **24** four-byte `blr` bodies with a `symbols.txt` extent, but in
HMX code (`< 0x82A00000`) exactly **one** of them takes a direct branch —
`0x826C3888`, with **1,116** callers. The other seven in that region have
fan-in 0. So every direct call to an empty function in HMX code lands on the
survivor, and our spelling is an empty function that is directly called. The
eight non-folded empties above `0x82A8` are the vendor/CRT weakness CLAUDE.md
already records (17× less folding there). This is an inference, not a proof, and
`FT-EMPTY` is tiered separately so it can be revoked without touching the other
eight pairs.

## Measurement — both rulers, paired, from one built state

`tools/ab_measure.py --from-dirty --name-check` ran first and settled to zero
work on both legs (`renamer_patched=1822`, msvc=0). ⚠ **Its default-ruler
reading is not the control here**: its docstring says the ninja report edge
hard-codes `functionRelocDiffs=none`, but `tools/project.py:1826` writes
`"name_check"` into `objdiff.json`, so both of its legs came back on the
name_check ruler and it printed `31.278600 → 32.129974` twice. That staleness
should be fixed in `ab_measure`, in its own lane.

The readings below were taken directly with
`objdiff-cli report generate -c functionRelocDiffs=<ruler>`, `report.cache`
wiped before each read, from **one settled build state** with only
`build/45410914/icf_aliases.map` differing between legs — zero recompiles, so
settling noise is structurally impossible.

| ruler | matched_code (B) | code% | complete fns | units at 100% |
|---|---|---|--:|--:|
| `none` A | 4,357,396 | 42.220000 | 44,055 | 254 |
| `none` B | 4,357,396 | **42.220000** | 44,055 | 254 |
| `name_check` A | 3,228,168 | 31.278600 | 35,100 | 253 |
| `name_check` B | 3,316,036 | **32.129974** | 36,458 | 253 |

- **`none`: Δ 0 bytes, Δ 0.000000pp, +0 / −0 complete functions — and not one
  of the 46,981 scored rows moved by 1e-9.** Exactly inert, which is what the
  mechanism predicts: `map_file` feeds `symbol_equivalences`, which is consumed
  only by `reloc_eq` in `objdiff-core/src/diff/code.rs`, and `none` does not
  compare relocations at all.
- **`name_check`: +87,868 bytes, +0.851374pp, +1,358 complete / −0 lost.**
  1,541 rows moved and **every one moved up**. Nothing was lost at either ruler,
  so there is no LOST list to report. The documented name_check instability
  (~0.05pp) is 17× smaller than the delta and does not apply anyway — both legs
  share one build.
- `match_percent_normalized` is unchanged on every row at both rulers, and
  `fuzzy_match_percent` is unchanged at `none`; per CLAUDE.md `report.rs` masks
  reloc args for those measures, so `matched_code` is the reloc-aware number and
  the one to quote for an alias change.

Installed as 3 groups in `scripts/symbol_aliases.json` (1,347 → 1,350) by
`tools/fold_thunk_gate.py --install`, which appends rather than re-sorting and
is idempotent (a second run leaves the file byte-identical).

**The groups depart from that file's "exactly ONE map-resident symbol per group"
gate, deliberately.** In this class both spellings are map-resident — that is
precisely what made the earlier residency triage read them as wrong callees. The
departure and its per-spelling justification are recorded in the file's own
`_comment` and in each group's `evidence`.

## The `0x82BC6B70` entry is CORRECT, and lane E's reading of it is refuted

Lane E left this for someone to act on: `??3@YAXPAX@Z` is mapped at
`0x82BC6B70` to "a 148-byte routine that is not a deallocator at all — integer
division", and asked whether the map entry is wrong. It is not wrong, and it is
a deallocator.

**What the routine is.** Disassembled (37 instructions), it is the exact inverse
of `?New@CMemoryHelper@@AAAPAXIII@Z` @ `0x82BBEFB8`:

- both address the same global at `0x82E4CEF0` (`lis r10,0x82E5 ; addi …,-0x3110`);
- both take the same critical section at `+0x54` (`bl 0x82C4BFEC` /
  `bl 0x82C4BFFC`);
- `New` writes the bucket tag at `ptr-8` and the back-offset at `ptr-4`;
  `0x82BC6B70` reads exactly those two words back;
- `New` **adds** the block size to the global total at `+0x10` and to the
  per-bucket table at `+0x14 + idx*4`; `0x82BC6B70` **subtracts** it from both,
  with the identical index arithmetic;
- the `divwu` lane E read as "integer division" is
  `(ptr − pool->base) / pool->elemsize` — a fixed-size-pool **slot index** —
  immediately followed by `slw`/`xor`/`stw` toggling that slot's bit in the
  pool's allocation bitmap. That is a free, not arithmetic.

Its 178 direct callers span `0x82BC1364`–`0x82C0F804` over 134 functions, and
every identified one is XAudio2 middleware: `CX2SourceVoiceWMA::Destroy`,
`CX2SourceVoiceXMA::Destroy`, `CLeapSystem::Uninitialize`,
`CBufferManager::~CBufferManager`, `CMixMatrix::~CMixMatrix`, `CSWVoice`,
`CBlock<CPhysicalMemoryPool>`.

**Why the map is right.** dc3 is the Rosetta Stone: same middleware, and a
leaked `ham_xbox_r.map` that names every function *and its owning `.obj`*. That
map lists `??3@YAXPAX@Z` at **three** distinct addresses —
`0x82E21268 utl:MemMgr.obj`, `0x82ABC2A8 nuispeech:ctransducer.obj`,
`0x82EB0C70 xaudio2:baseswfilter.obj` — because each module compiles its own
file-scope `operator delete`. RB3's body at `0x82BC6B70` is **byte-identical,
masked, over all 148 bytes** to dc3's `xaudio2:baseswfilter.obj` copy. It also
names `?g_MemoryHelper@@3VCMemoryHelper@@A`, the singleton our routine
dereferences.

So the correct name for `0x82BC6B70` **is** `??3@YAXPAX@Z` — XAudio2's. What
that entry cannot also do is name Milo's `::operator delete`, which the link
folded onto `0x8240DDB0` where the map recorded `??3BinStream@@SAXPAX@Z`. One
mangled name, two functions, and a VA→name map that has to pick. No
`target_symbol_map.json` edit is warranted, and none was made.

### A splits defect falls out of it

`config/45410914/splits.txt` pins `.text 0x82BC6B70–0x82BC6C04` into
**`MemMgr.cpp`** — almost certainly by matching the mangled name. The range
belongs to XAudio2, not to Milo's `MemMgr.cpp`, and the consequence is visible
in the report: `default/MemMgr :: ??3@YAXPAX@Z` is a **148-byte** row scoring
**2.70%**, our four-byte `b MemFree` paired against XAudio2's allocator. It can
never match. Left unlanded here: a splits edit forces a re-split, moves
`total_code`, and belongs with the owner rather than inside an alias lane.

## What is left on this class

- 198 sites over 27 refused pairs, of which **114** are outright body-length
  mismatches (not naming), **52** resolve to a different callee, and **32** are
  bodies that match under a map entry this lane could not discredit.
- The `MemMgr.cpp` `MemPopTemp` prologue divergence (44 sites unblock).
- The `MemMgr.cpp` splits mis-pin above.
- `FT-EMPTY` (154 sites) is admitted on a fan-in census rather than a body
  comparison and is the first thing to revoke if this class is ever doubted.
