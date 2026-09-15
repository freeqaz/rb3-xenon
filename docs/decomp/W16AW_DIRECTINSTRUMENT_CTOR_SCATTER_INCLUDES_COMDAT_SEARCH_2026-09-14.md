# W16-AW — DirectInstrument ctor, the two scatter-includes, and the missing-COMDAT search

Lane **W16-AW**, branch `w16-aw`, base `ba734115` (= main at dispatch).
Worktree `~/tmp/wt-w16-aw`. Source lane: collects W16-AU's filed follow-ups.

**Lane-internal baseline, measured (not inherited):** full build `rc=0` →
`matched_functions 43,485` · `matched_code 4,032,352 B` · `matched_code_percent 39.355362` ·
`fuzzy_match_percent 49.661087` · `total_code 10,246,004` · `total_functions 69,217`.
objdiff `a5f0ea903ec1` / `5a51cd51fe0a353f`, ruler `functionRelocDiffs=name_check`
(read from `report.json`'s own `provenance.diff_config`, 22 keys).
This reproduces the brief's quoted baseline **exactly**, so that figure is verified rather than assumed.

Renamer sanity before any COFF name was believed: **80,899** mangled `?` names across
3,114 target objs (bar was ≥27,000). A reflinked worktree is pre-renamer and a negative
name read off one is vacuous; this assertion is what licenses the negatives in item 3.

---

## Item 1 — `??0DirectInstrument@@QAA@XZ`: +1 fn / +328 B ✅

### Re-derived before editing

AU priced this at two charged instructions. Re-derived on my own built tree:

| source | reading |
|---|---|
| `report.json` row | `??0DirectInstrument@@QAA@XZ`, 328 B, **fuzzy 97.560974**, mpn 97.560974 |
| `objdiff-cli diff` (graded) | `fuzzy_match_percent 97.560974`, `diff_score 200 / 8200` |
| charged sites | **2** — `[70] delete: mr r4, r3` · `[75] insert: addi r4, r31, 0x60` |

⚠ **One correction to AU:** `8200 / 100 = 82` instructions, not the "83 instructions" AU
recorded (`97.560974 = 80/82`). The tool's own summary table prints `Total 83` because it
counts the delete and the insert as separate rows of a 82-instruction body. The **charge
count of 2 is right**, which is the load-bearing half.

### Diagnosis

Retail threads the `FilePath` constructor's returned `this` (MSVC returns it in `r3`)
straight into `LoadFile`'s `const FilePath&` argument with `mr r4, r3`. Our named local
made the compiler re-materialise the address with `addi r4, r31, 0x60`.

The detail that *confirms* the diagnosis rather than merely fitting it: retail computes
`r4` **immediately** after the ctor returns (idx 70) while ours floats to idx 75.
Threading a live `r3` can only happen at the return; a recompute can be scheduled anywhere.

Destruction needed no thought: `LoadFile` is the ctor's last statement, so
end-of-full-expression and end-of-scope coincide — which is why AU already saw the
destruction instructions agreeing.

```cpp
-    FilePath fp(".", path);
-    mDir.LoadFile(fp, 1, true, kLoadFront, false);
+    mDir.LoadFile(FilePath(".", path), 1, true, kLoadFront, false);
```

⚠ **The rb3-Wii oracle also writes the named local.** Retail bytes outrank the oracle
(CLAUDE.md, four measured fidelity modes, none of which defaults to the oracle); this is
another instance of that rule paying.

### Predicted vs measured

**Predicted +1 fn / +328 B, 0 rows out.** Measured by set-diff of the `fuzzy==100` row set
on a full `rc=0` build:

```
CROSSED IN : 1 rows, 328 B
   +    328 B  default/band3/game/DirectInstrument::??0DirectInstrument@@QAA@XZ
FELL OUT   : 0 rows, 0 B
  matched_functions   43,485 -> 43,486      (+1)
  matched_code     4,032,352 -> 4,032,680 B (+328)
  matched_code_percent 39.355362 -> 39.358562
```

Unit `default/band3/game/DirectInstrument` is now **12/12 functions, 876/876 B**.
Commit **`57d32f19`**.

---

## Item 2a — TrainerPanel's DirectInstrument scatter-include: removed, Δ0 ✅

Predictor read **before** editing (the brief forbade removing blind): all **9**
DirectInstrument-named *target* rows live in `default/band3/game/DirectInstrument`, all at
fuzzy 100; `TrainerPanel` and its three siblings (`VocalTrainerPanel`, `RGTrainerPanel`,
`GemTrainerPanel`) carry **zero**. So `TrainerPanel.obj`'s copies were unpaired base-side
extras with nothing able to pair against them.

**Predicted Δ0 / 0 rows out. Measured exactly that:** `CROSSED IN 0`, `FELL OUT 0`,
`matched_functions 43,486 -> 43,486`, `matched_code 4,032,680 -> 4,032,680 B`,
`fuzzy_match_percent 49.66117 -> 49.66117` (Δ 0.0 to the last digit).

The keep condition was "0 rows fall out anywhere in the tree"; it held, so the removal
stands. Commit **`fb37a3a8`**.

TrainerPanel's **other** scatter-include (`gesture/SpeechMgr.cpp`, sw3 cross-dialect) was
not touched — outside this lane's question.

---

## Item 2b — RhythmDetector's scatter-include: ⛔ LOAD-BEARING, reverted

### What it includes, and what justified it

`src/system/hamobj/RhythmDetector.cpp:926` scatter-includes **the same file**,
`band3/game/DirectInstrument.cpp` (label: `sw2 scatter-include`). AU never analysed its
span; here it is.

**No retail `.text` span justifies it on DirectInstrument's side.** RhythmDetector is
pinned at `0x822702F0–0x82271748`, `0x826342F0–0x82634354`, `0x82657660–0x826576C8`;
DirectInstrument lives at `0x826CCA88–0x826CCE28`, inside **none** of them.

The real justification runs the other way, and the DirectInstrument ctor's own diff points
straight at it: the ctor calls `fn_82270848`, and `0x82270848` is **inside** RhythmDetector's
`0x82270690–0x82270B1C` block. `ObjDirPtr<ObjectDir>`'s template COMDATs were placed by
retail inside RhythmDetector's span, and `DirectInstrument.cpp` is the **only** TU in our
tree that instantiates them (`ObjDirPtr<ObjectDir> mDir;` is DirectInstrument's member;
RhythmDetector.cpp itself only uses `ObjectDir::Main()`). The include is what makes
`RhythmDetector.obj` define those COMDATs so its target rows can pair.

### Predicted vs measured — the prediction UNDER-counted

**Predicted: 4 rows / 292 B fall out** (the four `ObjDirPtr<ObjectDir>` rows visible in
`report.json`). **Measured: 6 rows / 344 B.**

```
FELL OUT   : 6 rows, 344 B
   -    100 B  default/RhythmDetector::?PostLoad@?$ObjDirPtr@VObjectDir@@@@QAAXPAVLoader@@@Z
   -     88 B  default/RhythmDetector::??1?$ObjDirPtr@VObjectDir@@@@UAA@XZ
   -     76 B  default/RhythmDetector::??_G?$ObjDirPtr@VObjectDir@@@@UAAPAXI@Z
   -     40 B  default/RhythmDetector::fn_822709A8
   -     28 B  default/RhythmDetector::??0?$ObjDirPtr@VObjectDir@@@@QAA@XZ
   -     12 B  default/RhythmDetector::??3DirLoader@@SAXPAX@Z
  matched_functions 43,486 -> 43,480 (-6)   matched_code -344 B
```

The two I did not anticipate: `fn_822709A8` (40 B, still anonymous) and
`??3DirLoader@@SAXPAX@Z` (12 B, pulled in via `utl/Loader.h`). Naming the *visible* four
from `report.json` was not the same as enumerating what the TU actually supplies — worth
remembering as a targeting lesson.

⇒ **The brief's framing of this one as "presumed dead weight" is refuted.** The include is
load-bearing and was **reverted**; the revert was verified by re-reading the measures
(`CROSSED IN 0 / FELL OUT 0` against the post-item-1 snapshot), not by assuming the git
command worked. `RhythmDetector.cpp` is unmodified at lane end.

---

## Item 3 — byte-signature search for the two "missing" COMDATs

Method: parse our compiled `DirectInstrument.obj`, take each symbol's COMDAT section body
and relocations, mask the relocated operand fields, and search all of retail `.text`
(`orig/45410914/band.exe`, `.text` VA `0x82270000`, raw `0x264E00`). Python throughout —
no grep over binary bytes. Full proposals + evidence: **`docs/decomp/W16AW_map_proposals.json`**.

### The search is not vacuous — controls first

| control | size | relocs | retail matches |
|---|---:|---:|---|
| `?Enable@DirectInstrument@@QAAXXZ` | 168 B | 13 | **1** — `0x826CCB60` |
| `?Disable@DirectInstrument@@QAAXXZ` | 100 B | ? | **1** — `0x826CCA88` (= the pinned `.text` start) |
| `?PostLoad@DirectInstrument@@QAAXXZ` | 12 B | 1 | **1** — `0x826CCB50` |
| `?NoteOff@DirectInstrument@@QAAXH@Z` | 12 B | 1 | **1** — `0x826CCB20` |
| `?IsLoaded@DirectInstrument@@QAA_NXZ` | 8 B | 1 | **58** |

Every known-present COMDAT is found, at an address inside the pinned span. The `IsLoaded`
row is the calibration that matters: **an 8-byte signature is not identifying.**

### `?Enabled@DirectInstrument@@QBA_NXZ` — FOUND, and already fully harvested

16 bytes, **zero relocations** (so an exact, unmasked signature), **exactly one** match in
all of retail `.text`: **`0x826CCAF0`**, byte-identical
(`81630010 314bffff 7c6a5910 4e800020` = `lwz r11,0x10(r3); addic r10,r11,-1; subfe r3,r10,r11; blr`).

It sits in the **16-byte hole** between DirectInstrument's two pinned blocks
(`…end:0x826CCAF0` / `start:0x826CCB00`), and `0x826CCAF0 + 16 = 0x826CCB00` exactly.

**Why AU could not see it:** `0x826CCAF0` is **not a `.pdata` BeginAddress**. A 16-byte leaf
touching neither stack nor LR gets no unwind record, so every `.pdata`-keyed instrument is
structurally blind to it — a clean instance of CLAUDE.md's sub-`.pdata` stub stratum.

**But there is nothing to collect, and a map row here would have been harmful.** Checking
the existing record before proposing:

- `0x826CCAF0–0x826CCB00` is **already pinned**, into `PracticePanel.cpp`.
- It is **already named** `?InTransition@UIManager@@QAA_NXZ` in `target_symbol_map.json`.
- That row **already matches at fuzzy 100** (16 B) in `default/PracticePanel`.
- `?Enabled@DirectInstrument@@QBA_NXZ` is **already a recorded T1 folded membership** of the
  `symbol_aliases.json` group `{survivor: ?InTransition@UIManager@@QAA_NXZ, address: 0x826ccaf0}`,
  alongside `?HasData@SampleData@@QBA_NXZ`.

All three spellings compile to the identical relocation-free body — verified on our side
too: 13 separate objs compile `?InTransition@UIManager@@QAA_NXZ` to those same 16 bytes.
So this is one physical ICF copy, already counted exactly once. AU's two-block carve was
**correct and deliberate**: it carves around a block PracticePanel already owns.
⇒ **No action. The question is closed, not open.**

### `?SetVolume@DirectInstrument@@QAAXH@Z` — identified at `0x82A478D0`, recommend NOT naming it

8 bytes, zero relocations, `stw r4, 0(r3); blr` — **12** retail matches, **none** inside the
DirectInstrument region. Per the `IsLoaded` control, that alone proves nothing. Two further
lines settle it:

1. **Only one of the 12 candidates has any caller at all** — `0x82A478D0`, with 17; the
   other eleven have zero.
2. **Caller-offset, decisive.** The relocation to `?SetVolume@DirectInstrument@@QAAXH@Z`
   inside our compiled `GamePanel.obj` sits at offset **`0x59C`** within
   `?Handle@GamePanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z`, whose retail address is
   **`0x82696b48`**. `0x82696b48 + 0x59C = 0x826970E4`, and retail's instruction there is
   **`bl 0x82A478D0`** — an address that had already been found independently by scanning
   for `bl` targets. The call-site shape agrees: `lwz r3, -12(r27)` (`r27` biased, reaching
   `this+0x150` = `mDirectInstrument`, matching `GamePanel.h`) with `r4` the returned int of
   the preceding `_msg->Int(2)` call, i.e. `mDirectInstrument->SetVolume(_msg->Int(2))`.

⚠ I first mis-read that call site as *contradicting* the identification, because I expected
a literal `lwz r3, 0x150(r31)` and saw a negative offset off a biased base. The offset
computation is what settled it; the eyeball reading was the unreliable instrument.

**Recommendation: record the identification, do NOT name the address.** `0x82A478D0` is
absent from the map; `name_check` forgives placeholder targets, so all 17 call sites are
presently uncharged. **15 of the 17 callers are XDK/vendor code** (e.g.
`??0CFG@XGRAPHICS@@…` at `0x82A3C2C0`) — it is the fold survivor for many classes' setters,
not DirectInstrument's property. Naming it converts 17 forgiven sites into checked ones for
**zero byte upside** (the single game call site is already forgiven). That is the MAPID-1
shape: naming an anonymous address pays in bug exposure, not bytes. It should only ship
together with an alias group enumerating every folded spelling.

---

## Item 4 — `0x82682688` caller enumeration: 11 sites, 0 contradictions

`0x82682688` is named `?GetLocalBandUser@BandUserMgr@@SAPAVLocalBandUser@@PAVLocalUser@@@Z`
(installed by W16-AV). Expected signature: static, one pointer argument in `r3`, pointer
result used as a `LocalBandUser*`. All 11 retail `bl` sites were enumerated and read.

**Decisive corroboration — `0x8258576C`, inside
`?GetLocalHost@SessionMgr@@UBAPAVLocalBandUser@@XZ`.** That function is *declared* to return
`LocalBandUser*`, and its entire body is a tail call to `0x82682688`
(`lwz r3,0x38(r3); bl <helper>; bl 0x82682688;` epilogue). The caller's **return type**
corroborates the callee's — stronger than argument shape, which is all the other sites give.

Six sites share one idiom: `lwz r11,0x5C(r11); mtctr; bctrl; mr r3,<user>; bl 0x82682688` —
a virtual accessor (vtable slot `0x5C`) yielding a `LocalUser*`, mapped straight to a
`LocalBandUser*`. Four of those six are inside **`ProfileSwappedMsg` handlers**
(`SaveLoadManager::OnMsg`, `ClosetMgr::OnMsg`), exactly where such a lookup belongs.
`?UpdateLeader@SessionMgr@@QAAXXZ` null-tests the result as a pointer (`cmplwi r3,0`).

**Zero call sites contradict the name.** No finding filed against W16-AV. The per-site table
is in the JSON.

---

## Gates

Run in the worktree, in the brief's order, native gate last.

| gate | result |
|---|---|
| full build | **`rc=0`** — final measures 43,486 / 4,032,680 B / 39.358562 %, `total_code` 10,246,004 |
| `scripts/verify_ruler_agreement.py --check` | **`rc=0`** — both objdiff-cli entry points resolve the same ruler (all four keys pinned) |
| `scripts/verify_objs_patched.py --verify-manifest` | **`rc=0`** — 1,215 decomp + 3,114 target objects match, `tree_sha256=fa9cb29ba1659b6d`; denylist OK (6 addresses, none named across 495,651 symbols) |
| `tools/icf_alias_finder.py --validate` | **not run — no alias file was touched** (this lane edited no map/alias/splits file) |
| `tools/native_build_gate.sh` | **PASS, 0 SKIPs** — line below |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native gate was the last build-affecting action; the commit that follows it touches
`docs/` only, which the native build does not read.

**Lane-internal totals — before / after:** `matched_functions` **43,485 → 43,486** (+1);
`matched_code` **4,032,352 → 4,032,680 B** (+328); `matched_code_percent`
39.355362 → 39.358562. Final set-diff against the lane baseline: **CROSSED IN 1 row,
328 B; FELL OUT 0 rows.**

## Commits

| sha | item |
|---|---|
| `57d32f19` | item 1 — FilePath temporary in argument position (**+1 fn / +328 B**) |
| `fb37a3a8` | item 2a — TrainerPanel scatter-include removed (Δ0, verified, 0 rows out) |
| `0ba5a5f2` | items 3+4 — JSON proposals (`docs/decomp/W16AW_map_proposals.json`) |

## NOT done, and why

1. **`src/system/hamobj/RhythmDetector.cpp` is unmodified.** Its scatter-include was A/B'd
   and measured **load-bearing** (−6 rows / −344 B) and reverted. *What would change this:*
   nothing in source — the six COMDATs would have to be supplied to `RhythmDetector.obj`
   another way (its own instantiation, or a splits re-home of `ObjDirPtr<ObjectDir>`'s
   addresses out of RhythmDetector's span into a unit whose obj defines them). That is a
   splits question, i.e. W16-AX's file, not mine.
2. **No map, alias or splits file was edited.** `scripts/target_symbol_map.json` and
   `scripts/symbol_aliases.json` are W16-AY's this wave, `config/45410914/splits.txt` is
   W16-AX's. Everything is filed as JSON.
3. **`0x82A478D0` was not named**, deliberately — see the risk analysis above. This is a
   recommendation, not an inability: *what would change it* is an adjudication of all 17
   call sites yielding the full folded-spelling set, so the name ships with an alias group
   rather than alone.
4. **The 15 vendor callers of `0x82A478D0` were not individually adjudicated** — XDK is out
   of scope per the standing user directive except pinning and a memory-management subset,
   and none of them is a memory-management site.
5. **TrainerPanel's `gesture/SpeechMgr.cpp` scatter-include was left alone** — a different
   question from the one this lane was given, and removing it would be exactly the blind
   removal item 2b just demonstrated the cost of.
6. **`fn_822709A8` and `??3DirLoader@@SAXPAX@Z` were not investigated** beyond observing
   that RhythmDetector's include supplies them; they are two of the six rows the revert
   restored.
