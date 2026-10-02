# W16-NV — StringConversion carve, the trie pin, and 22 dead `/D` gates (2026-10-02)

**Branch** `w16-nv`, rebased onto main `158907a9d`. **Ruler** `name_check` (graded). The permuter was not run.
No map name or alias was added, changed or removed.

W16-NT (`docs/decomp/W16NT_PER_TU_FLAG_AUDIT_2026-10-02.md`) recorded three things it did not change. This lane
changes them:

1. `StringConversion.cpp`'s one retail function was carved by dtk into five pieces. The carve is fixed, the TU is
   `/Od`, and the function is written from retail and matches.
2. `trie.cpp` was pinned to `0x82AB0F70..0x82AB12F4`, inside the Quazal `/Od` block. That range is not a trie, and
   retail has no trie. The pin is removed.
3. The 22 `/D` gate entries W16-NT found dead were re-confirmed one at a time and removed.

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nv-ab --patch ~/tmp/w16nv/ab.patch`, in a fresh
`setup_worktree.sh` worktree at main `158907a9d`. `ab_measure` refuses a patch that touches `symbols.txt`, so leg A's
base commit carries the `symbols.txt` carve fix (W16-MC/W16-NM's recipe). It also carries the StringConversion pin
extension, because the grown 0x114 symbol cannot sit under the old 64 B pin (dtk rejects a split boundary inside a
symbol). The patch is everything else: `objects.json` (`/Od` on StringConversion, 22 gate entries removed),
`StringConversion.cpp`, and the trie unpin. The effect of the leg-A base alone is measured separately in §2.1.
objdiff-cli `sha256:c1b7d952`, stable across legs. Run dir
`~/tmp/wt-w16-nv-ab/.ab_measure_runs/20261002-184509-ab-2147122/`.

```
leg A: matched=51491 masked=24640 honest=26851 code%=54.929226  (recompiles: 0, settled)
leg B: matched=51492 masked=24640 honest=26852 code%=54.931920  (recompiles: 21, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.002694pp  Δcode_bytes=+276
Δfuzzy=+0.001964pp   (legA 60.406560 -> legB 60.408524)
units at 100% [mpn ruler]: legA 485 -> legB 486  (StringConversion, MATCHED_ROSE; 0 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 430 -> legB 431
[control none] +276 B -- NOT_APPLICABLE (source + splits + configgen in patch)
```

**Prediction, written before the run:** Δmatched +1, Δhonest +1, Δcode_bytes +276 (Latin1ToUtf8 to 100), Δmasked_equal
0; the trie unpin and the gate removal Δ0; no row down. **Measured: +1 / +1 / +276 / 0.**

Leg B recompiled 21 TUs: the 20 distinct gate carriers (Campaign and RockCentral each lose two entries) plus
StringConversion. That is the full set the patch should touch, so neither change was absent from leg B.

**Row level, archived legs, keyed by row name** (`~/tmp/w16nv/rowdiff.py`): **1 row up, 0 down, 0 appear, 0 vanish**,
63 rows changed unit only. The one up is `?Latin1ToUtf8@?A0xfea654e8@@YAXPBDPADI@Z` (276 B), fuzzy 25.507 → 100,
mpn 27.246 → 100. The 63 moves are the six trie rows (`default/trie` → `auto_03_82AB0F70_text`, all at 0 both
sides) and 57 anonymous rows of the following auto unit, which is renamed by its new start address. `total_code` and
`total_functions` are unchanged across the legs.

**Whole branch against main** = the leg-A base (§2.1) plus this A/B: matched 51,491 → 51,492, `matched_code`
5,628,800 → 5,629,076 (+276 B), `total_functions` 68,925 → 68,921 (−4 carve fragments), `total_code` unchanged.
No row goes down anywhere.

## 2. StringConversion: one `/Od` leaf, carved five ways

### 2.1 The carve

Retail `0x82AE5FA8` is a red-zone `/Od` leaf (no frame, no `.pdata`): it spills `r3/r4/r5` to their home slots
(`0x14/0x1c/0x24(r1)`), copies two of them to locals below the stack pointer, and runs a loop. dtk emitted it as

| symbol | size | |
|---|---:|---|
| `fn_82AE5FA8` | 0x40 | head; ends on `beq cr6, fn_82AE60AC` (conditional, falls through) |
| `fn_82AE5FE8` | 0x4C | |
| (no symbol) | 0x4 | `0x82AE6034 lwz r10,-0xc(r1)` stranded between two symbols |
| `fn_82AE6038` | 0x30 | |
| `fn_82AE6068` | 0x44 | target of `blt cr6, fn_82AE6068`; ends `b fn_82AE5FA8+0x34` (the loop back-edge) |
| `fn_82AE60AC` | 0x10 | target of `beq`/`ble`; `*out = 0; blr` |

then one zero pad word at `0x82AE60BC`. Nothing outside the function refers to the four interior addresses: the only
references in the split asm are the function's own branches, and a Python scan of `band.exe` finds no data word equal
to any of them. The only caller of the head is `bl fn_82AE5FA8` at `0x82AE695C` (the `TToUtf8` wrapper).

jeff's fall-through merge (`merge_fallthrough_leaf_fragments`, `../jeff/src/cmd/xex.rs`) could not repair it. The 64 B
pin ended at `0x82AE5FE8`, so the head and the rest were in different split units (its P5), and two fragments are
branch targets, which count as independent entries (its P3). A pin alone cannot repair a carve either (DATAINITFUNCS
§6.2 measured that). So the fix is the house one used by W16-MC/W16-NM: `symbols.txt` grows the head to `0x114` and
drops the four fragments; `splits.txt` extends the pin to `0x82AE60C0`. The re-split keeps the symbol at `0x114` and
regenerates no fragment (a fixed point, checked in-tree and in both A/B legs).

**main → carve + pin alone**, measured in the A/B worktree before applying the patch (both builds settled,
`report.json` wiped): `matched_functions` 51,491 → 51,491, `matched_code` 5,628,800 → 5,628,800, `total_code`
unchanged, `total_functions` 68,925 → 68,921. Row level: the head goes 64 B → 276 B (fuzzy 0 → 25.507 against our
`/O1` body), the four fragments (80 + 48 + 68 + 16 B) vanish, 36 anonymous rows only change unit name
(`auto_03_82AE5FE8_text` → `auto_03_82AE60C0_text`), 0 rows down.

### 2.2 The body

Retail, read off the asm:

```
src = in            -> -0x8(r1)      (copied from the home slot)
dst = out           -> -0x10(r1)
len--                  in its home slot 0x24(r1)
c   = *src          -> -0xc(r1)      lbz + stw: an int loaded from an unsigned byte
loop: c != 0 (cmpwi) && len > 0 (cmplwi; ble)
  c >= 0x80 (signed cmpwi):
      *dst = ((c & 0x7C0) >> 6) | 0xC0   rlwinm 0,21,25 ; srawi 6 ; ori ; clrlwi 24
      dst++, len--
      *dst = (c & 0x3F) | 0x80           clrlwi 26 ; ori ; clrlwi 24
      dst++, len--
  else: *dst = c (clrlwi 24); dst++, len--
  src++, c = *src
*dst = 0
```

The previous source used a `u8` character, an `s16` compare and `& 0x1F` after the shift. Under `/Od` the first
rewrite read 97.39: the three byte stores truncated with `extsb` where retail uses `clrlwi`, i.e. our destination was
`char *` and retail's is `unsigned char *`. With that one change the row is **100 / 100** (276 B). `/Od` alone is
enough; the function is a leaf, so the EH and inlining flags W16-NT needed for Scheduler have nothing to act on here,
and they were not added.

### 2.3 The rest of the TU (not done)

The retail TU is much larger than our source. The five `Quazal::StringConversion` wrappers are at `0x82AE6828`
(`Char8_2T`, `strcpy`), `0x82AE6860`, `0x82AE6900` (`Utf8ToT` → `0x82AE60C0`), `0x82AE6938` (`TToUtf8` →
`0x82AE5FA8`) and `0x82AE6970` (`GetTToUtf8BufferSize`, `strlen * 2 + 1`), all framed `/Od`, and between them sit
UTF-16 helpers (`0x82AE6228` counts UTF-8 bytes for a `lhz`-walked string) and more `MultiByteToWideChar`-style
wrappers continuing past `0x82AE6A38`. Every red-zone leaf in `0x82AE60C0..0x82AE6810` is carved the same way:
`Utf8ToLatin1` at `0x82AE60C0` is three pieces, and its last piece `fn_82AE6218` (0x28) swallows the first six
instructions of the next function at `0x82AE6228`, which is carved again, and so on. Retail's `Utf8ToLatin1` also
differs from our source (an `int` range test `0..0x7F` / `0xC0..0xDF`, a `bool` stop flag, and a counter at
`-0x20(r1)` that our source does not have). Pinning the whole TU means re-carving that run, which is its own lane.

## 3. trie.cpp: the pin covered `/Od` STLport string code, and retail has no trie

The six rows under the `trie.cpp` pin (`fn_82AB0F70` … `fn_82AB12C8`, all anonymous, all at 0) are `/Od` code that
references `"bad allocation"`, `"__new_alloc"` and `"basic_string"`. `fn_82AB0F70` stores `this` at `this+0x10` when
the requested size is `<= 0x10` and otherwise calls an allocator: STLport's short-string `_M_allocate_block`. They are
`basic_string` instantiations emitted by an `/Od` Quazal TU.

Our `trie.cpp` is DC3's engine `Trie` (DC3's map places it in `utl:trie.obj`, next to `AllocInfo`). Every `Trie`
accessor addresses its header at `this + 0x220000`, so any retail copy must carry an `addis`/`lis`/`oris` with
immediate `0x22`. **No such instruction exists in the retail asm.** Control: the same regex over the same files finds
4,199 `lis`/`addis`/`oris` immediates (`0x20` 74 times, `0x10` 95 times), so it can match. Retail RB3 does not contain the trie, so there is nowhere correct to move the pin.

The owning TU of the range is not identified. It sits immediately after `DuplicatedObject.cpp`'s
`0x82AB0850..0x82AB0F70` block, but that source (97 lines) does not use `basic_string`, and every neighbouring
function is anonymous. Rather than guess an owner, the pin is removed and the range goes back to `auto_*`, as
`4b3c098d` did for the other bad pins in this block. `trie.cpp` stays in `objects.json` and is still compiled; it
just pairs with nothing. Expected Δ0: the six rows are anonymous and unpairable under either owner, and five of the six
are `.pdata` function starts, which jeff's merge never absorbs. The re-split left `symbols.txt` unchanged.

## 4. The 22 dead `/D` gate entries

### 4.1 Method

For each (gate, carrier), `tools/gate_liveness.py --flag <GATE> --keep <dir> <unit>` compiles the TU with and without
the gate (both legs `OBJCACHE=off`, same `/Fo`). Its verdict only looks at `.text` words in TU-owned symbols, and the
brief's bar is "object code byte-identical", so both kept objects were also compared whole (`~/tmp/w16nv/coffnorm.py`):
every section body, every relocation table, and every symbol-table entry, with only the COFF timestamp masked.

Controls, run in the same driver (`~/tmp/w16nv/gates.py`):

| control | expected | result |
|---|---|---|
| null flag `RB3_NULL_CONTROL_XYZZY` on `band3/tour/Tour.cpp` | identical | INERT, objects byte-identical |
| `RB3_SYNCPROP_LOCAL_STATIC` on `system/bandobj/BandCamShot.cpp` (LIVE in W16-NT) | differs | LIVE; 4,860 of 14,011 sections differ, even after the normalisation below |

### 4.2 Result: all 22 confirmed, in two classes

| gate | carriers | `gate_liveness` | whole object |
|---|---|---|---|
| `RB3_STRIP_CHEAT_HANDLERS` | AccomplishmentPanel, Campaign, CustomizePanel, ProfileMgr, RockCentral, QuestFilterPanel | INERT, owned 0 | **byte-identical** |
| `RB3_HANDLE_LOCAL_STATIC` | Label3d, SessionSearcher_Xbox, NetSession_Xbox | INERT, owned 0 | **byte-identical** |
| `RB3_SYNCPROP_LOCAL_STATIC` | rndobj/Cam, bandobj/ReviewDisplay | INERT, owned 0 | **byte-identical** |
| `RB3_MAP_0x1C` | TourProgress, CharacterCreatorPanel, Campaign, ChooseColorPanel, Tour, MoviePanel, LessonMgr, InterstitialMgr, LightPresetManager, RockCentral, TourPerformer | INERT, owned 0, template 0 | **not byte-identical** (1,296–12,398 B differ in 10 of 11; MoviePanel identical) |

The `MAP_0x1C` differences were localised. Positionally, every section body and every relocation table is identical
in all 11. What differs is only the *numbers* in compiler-internal labels: `$T…`, `$M…`, `__unwind$…`, `__catch$…`.
The padded `stl/_map.h` the gate selects shifts the compiler's internal label counter, and nothing else. With those
numbers normalised, all 11 objects are identical in every section, relocation and symbol. The same normalisation still
leaves BandCamShot's positive control differing in 4,860 sections, so it hides nothing real. These labels are static
and internal, and their numbers already move with any header edit, so no pairing can depend on them. W16-NT's
whole-binary ablation of `MAP_0x1C` measured Δ0 / Δ0 for the same reason.

So the precise statement is: 11 entries are byte-identical, and 11 (`MAP_0x1C`) are identical except for
compiler-internal label numbering. W16-NT's "byte-identical `.text`" was right about `.text`; the whole objects differ
in the symbol table.

**Interaction.** Campaign and RockCentral each lose two gates (`MAP_0x1C` and `STRIP_CHEAT_HANDLERS`), each tested
alone with the other present. `MAP_0x1C` was re-probed on both against the final flag set (neither gate): identical
after normalisation, so removing both together is also code-identical.

### 4.3 What changed in `objects.json`

22 flag entries removed. Six entries whose `extra_cflags` became empty were collapsed to the plain status string
(LessonMgr, InterstitialMgr, LightPresetManager, SessionSearcher_Xbox, NetSession_Xbox, rndobj/Cam). MoviePanel keeps
`/Y-`; it is not a `/D` gate, and W16-NT found it metric-inert without being able to call it dead. Gate carriers in
`build.ninja` after the change: `HANDLE_LOCAL_STATIC` 156, `SYNCPROP_LOCAL_STATIC` 35, `NOTIFY_ONCE_EVAL` 11,
`STRIP_CHEAT_HANDLERS` 6, `LOG_NO_EVAL` 2, `NO_WII_META_MEMBERS` 1. **`RB3_MAP_0x1C` is now applied nowhere.** The
`#ifdef RB3_MAP_0x1C` code in the STL headers was left in place; deleting it is a separate change to shared headers.

## 5. Gates

Branch tip after the full `./tools/ninja-locked` (log `~/tmp/rb3_build_w16nv_6.log`; the next build did no work):
`[patch-state] OK: tree is a fixed point of 6 post-compile passes`;
`1054/1054 declared compiled objects pair with a target`. That is one fewer than W16-NT's 1055 because `trie.obj` no
longer has a unit.

The branch touches `config/45410914/symbols.txt`, `splits.txt`, `objects.json`, `src/network/Platform/StringConversion.cpp`
and this doc. No header, map or alias edit. A grep of the added lines and the commit messages finds no port-provenance
citation and no Co-Authored-By line.

Native gate: see §6 (run last).

## 6. Native gate

Run last, on the code at `e4f4b4e06` (only docs commits follow it):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 7. Not done

- Not merged to main.
- The rest of the StringConversion TU (§2.3): about 0x980 B of carved `/Od` leaves and wrappers, and a retail
  `Utf8ToLatin1` that differs from our source.
- The owner of `0x82AB0F70..0x82AB12F4` (§3) is not identified.
- The `RB3_MAP_0x1C` header code is still in the tree, now unused by the match build.

Scratch: `~/tmp/w16nv/` (`gates.py`, `gates_result.json`, `coffdiff.py`, `coffnorm.py`, `coffnorm_final.txt`,
`keep/`, `keep2/`, `rowdiff.py`, `ab.patch`, `main_report.json`, `legAbase_report.json`, build logs).
