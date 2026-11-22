# Lane strnode: audit of kDataString reads and native-width DataNode union reads (2026-11-22)

Branch `strnode`, based on `dtafail` (`62fd7cfca`; not on main yet). Lane dtafail found
native `DataNode::Equal` streq'ing a kDataString node's storage pointer
(`docs/decomp/DTAFAIL_NATIVE_BOOT_DTA_FAILS_2026-11-22.md`). This lane audited every other
reader of a string node's value under `src/`, plus every place the 8-byte native
`DataNode::mValue` union is read as retail's 4-byte word.

Layout reminder: a kDataString node's `mValue.array` is a `DataArray` built by
`DataArray(const void *, int)` whose `mNodes` *is* the character buffer (`mSize` = -len).
Reading the characters takes one more dereference: `mValue.var->mValue.symbol`
(the same word as `mValue.array->mNodes`). On X360 the union is one word; natively it is
8 bytes, and only pointer stores and the constructors (which zero first under
`HX_NATIVE`) write all of it.

## Method

- `grep` of `kDataString`, `UncheckedStr`, `UncheckedVar`, `UncheckedArray`,
  `UncheckedInt`, `LiteralInt` and direct `mValue.` writes over `src/` (57 sites read).
- clang `-fsyntax-only -Wpointer-to-int-cast -Wshorten-64-to-32` on the native compile
  commands of the 11 obj/utl TUs (DataFunc, DataNode, DataArray, DataUtl, DataFile, Utl,
  Object, TypeProps, Task, Dir, MakeString). Its one pointer-to-int hit in a Data TU was
  `DataFunc.cpp:1172` (has_substr). It cannot see a union read through the wrong member,
  so the grep was the main instrument.
- Each suspect checked against `orig/45410914/band.exe` with `tools/va_disasm.py`.

## Defects found and fixed (all under `HX_NATIVE`; X360 text unchanged)

| # | site | defect | retail evidence |
|---|---|---|---|
| 1 | `DataNode::Equal` same-type branch (`DataNode.cpp`) | object/var/func/array/command/property/glob nodes compared with `UncheckedInt()`, the low 4 of 8 bytes, so `==`, `!=`, `find_elem` and `switch` called two pointers 4 GiB apart equal | `operator==` 0x8274ABF0..0x8274AC00: `lwz 0(r3)`, `lwz 0(r4)`, `subf`, `cntlzw`: the whole word, which is the whole pointer |
| 2 | `DataHasSubStr` (`DataFunc.cpp`) | `(int)strstr(...)` kept the low half; a match at a 4 GiB-aligned address read 0, "not found" | 0x82761C08: `bl strstr` (0x8282B298), `stw r3, 0(r30)`, `stw 0 (kDataInt), 4(r30)` at 0x82761C48: the int node *is* the match pointer. Natively it now returns found/not (1/0) |
| 3 | `DataNode::Load` (`DataNode.cpp`) | int/float/unhandled/else/endif cases read 4 bytes into the 8-byte union. `DataArray::Load` (0x8274D460) reads the node after `#ifdef`/`#ifndef`/`#define`/`#undef`/`#else`/`#endif`, and after a skipped node, into the same slot, so an int or float there kept the previous symbol's high half. `NotNull` reads all 8 bytes for those types, so a loaded `0` read **true** wherever `NotNull` decides (`if`, `!`, `&&`, `||`, `unless`; the gate's `{! $v}` gave 0) | `DataNode::Load` 0x8274B980 reads the value with one 4-byte `BinStream::Read` (`bl 0x827C5058` at 0x8274BB28), the whole retail union; `NotNull` 0x8274B178..0x8274B188 tests one word. Fix: clear the union at the top of `Load` |
| 4 | `MsgSinks::GetPropSyncHandler` (`Msg.cpp`) | property-path elements compared with `UncheckedInt()` (low 4 bytes of symbol pointers) | No row in `scripts/target_symbol_map.json` (DC3-era `MsgSinks`; unidentified or absent in RB3), so no retail body was read. The fix rests on width alone: a 4-byte compare of an 8-byte symbol pointer. Natively compares the full union |
| 5 | `UIManager::OnGotoScreen` debug print (`UI.cpp`, native-only `DebugUIFlow`) | `node.Sym()` on a kDataString node: native `Sym()` fails "is not Symbol" and returns `""` | Native-only code with no retail counterpart; now `LiteralStr()` |

#3 needs only an `#ifdef`/`#ifndef` before an int literal in a dtb, so it is the one most
likely to reach real data. No boot log was taken to find a live instance (not measured).
In the gate the loaded `7` after `#ifndef` read union bytes `0x00007f5300000007` before the
fix.

## Sites checked, with verdicts

Correct (read a string node through `Str()`/`LiteralStr()`/`ForceSym()`, or deref
`mValue.var->mValue.symbol`; retail does the same):

- `DataNode`: `Str` (0x8274B000), `LiteralStr` (inlined into GetObj 0x8274BF4C: `cmpwi 5`,
  then `lwz r11,0(r3); lwz r4,0(r11)`), `ForceSym`, `NotNull` string arm (`lha 8(r10)`
  < -1 at 0x8274B158), glob arm (`lhz 8` at 0x8274B180), `Print`, `Save`/`Load`
  string/glob (`SaveGlob`/`LoadGlob`), `GetObj`, `CompatibleType`, `operator==` X360 body
  (matches 0x8274ABB8 / 0x8274AC9C), `Equal` string branches (fixed by dtafail).
- `DataArray`: `Execute` kDataString arm (`UncheckedVar()->UncheckedStr()`), `NodeCmp`
  (0x8274CB40, `Str()` + stricmp), `FindData(const char *&)`, `SaveGlob`/`LoadGlob`.
- `DataFunc.cpp`: `==`/`!=` (DataEq 0x8275D998 / DataNe 0x8275DB28 via operator==),
  `find_elem` (0x8275D9F8), `switch`/`SwitchMatch` (0x827606D8 / 0x8275D0F0), `symbol`,
  `startswith` (0x82760218), `strlen`, `str_elem`, `strieq` (0x82762610), `strcat`
  (0x82762040), `substr`, `find_substr` (0x82761E10), `search_replace`, `tolower`,
  `toupper`, `string_flags`, `has_any_substr`, `match_pattern`, `exists` (0x82763488),
  `object`, `find_obj`, `basename`, `dirname`, `sprint`, `sort`.
- Callers outside obj/, all through `Str`/`LiteralStr`/`ForceSym`: BandUser (311, 322,
  351), UGCPurchasePanel 187, Game 619, StoreMenuProvider 68, NextSongPanel 870,
  RockCentral 1934, BandStorePanel 356/392/416, CharDriver 463, DrivenPropertyMathOps 115,
  TexMovie 184, PostProcMgr 128, CheatProvider 34/88, UIList 540/860, DataPointMgr
  13/55/61, SuperFormatString 132/161, Flow 128, FlowSetProperty 66/69, UIScreen 441,
  VocalTrackDir 271/303, MidiParser 505 / MidiParser.h 50, BandDirector 1709/1725/1730,
  Task 206, UILabel 1057, CameraShot 1795, Cheats 93, `FormatString::operator<<(DataNode)`
  `%s` arm.
- `UncheckedStr()` sites that are guarded by `Type() == kDataSymbol` (never see a string):
  Object.cpp OnGet/OnSet (1325, 1340, 1381), Dir.cpp Iterate 1190, DataFunc Quasiquote
  1367 and the merge filter 1574, DataArray::Load include/merge 561, TypeProps 15/61/106.

Faithful to retail (pointer identity or garbage in both; left alone):

- `PathCompare` (Utl.cpp:195, 0x827580C8): kDataString elements compared by `Str()`
  pointer (`bl 0x8274B000` twice, `cmplw r26, r3` at 0x82758170), so two equal strings
  compare different in retail too.
- `DataArray::Contains`/`Remove` and `contains`/`remove_elem`: retail 0x8274C518 compares
  the value word with no type test (`lwzx`/`cmpw`), so a string matches only its own
  storage. Native already compares full width for pointer types.
- `DataInt` (0x8275F8B0): a string falls to `LiteralFloat` (`bl 0x8274A978`, `fctiwz`) in
  retail; native adds a "not Float" notify. Garbage in both.
- `MainHubPanel` MOTD test (`Str()` pointer vs the pooled ""): documented in place; native
  deliberately tests content.
- `FlowSetProperty::SetProperty` `MakeString("%i", stringNode)`: meaningless in both
  (native `LiteralInt` fails "not Int"); Flow is DC3-era code.
- `FindArray(int)` (0x8274C558, `UncheckedInt(0) == tag`) and `DataMergeTags`/
  `DataReplaceTags` int keys: native already routes symbol keys through the Symbol
  overload; int keys are 4-byte by design.

## Gate

`native/src/string_node_phase.cpp`, run by `rb3-milo` after the ScriptTask phase,
classified NON_STUB in `tools/native_stub_census.py`. 20 gates: 15 `control-` gates (the
string operations above evaluated through `DataReadString` + `Evaluate`: `==` string/string,
string/string differing, symbol/string, `''`/`""`, `!=`, `find_elem`, `switch`, `strieq`,
`has_substr` hit and miss; same-object `==` and `find_elem`; Load of a plain `0` and of `7`
after `#ifndef`) and 5 defect gates (has_substr with the match at 0x700000000000,
`==`/`!=`/`find_elem` on two object addresses 4 GiB apart that are never dereferenced,
Load of `0` after `#ifndef`).

Prediction: the five defect gates fail on the dtafail tip and pass after the fix; every
control passes both ways. Measured (`rb3-milo <xbox-zip> ui/track/gen/tracksystem_meshes.milo_xbox`
through `no610m.sh`):

| build | rb3-milo PASS lines | strnode FAILs | result |
|---|---|---|---|
| `d45aa38b6` (gate only) | 78 | 5, exactly the five defect gates | `RESULT: FAILED (5 gate failure(s))`, rc 1 |
| `fae7a4eea` (fixes) | 83 | 0 | `RESULT: ALL GATES PASSED`, rc 0 |

Full native build at `fae7a4eea`: all 19 targets compile and link, 0 errors (not run,
except rb3-milo). `tools/native_stub_census.py native/build <out>` exits 0; with the
`string_node_phase` line removed it exits 3 and names the object, so the classification
is checked.

No other rb3-milo gate changed (diff of the PASS/FAIL name lines shows only the five).
The string controls pass on the dtafail tip because dtafail fixed Equal; they are a
guard on that fix, not evidence for this one.

## X360

Every edit in `fae7a4eea` is inside `#ifdef HX_NATIVE`, so the prediction was Δ0.
`python3 tools/ab_measure.py --worktree ~/tmp/wt-strnode --revert fae7a4eea`
(name_check ruler, objdiff-cli sha256 `ec3badc0`): Δmatched +0, Δmasked_equal +0,
Δhonest +0, Δcode +0 B, Δfuzzy +0.000000pp (64.709015 both legs), units at 100% 635 -> 635
(mpn) and 548 -> 548 (all-rows-fuzzy); `none` control Δ+0 B. The gate commit
`d45aa38b6` touches only native/ and tools/ and was not measured.

## Not done

- `tools/native_health.sh --selftest` not run (coordinator's instruction: under `no610m.sh`
  it can only read nogpu; the landing gate covers it).
- No boot-log search for a live instance of defect #3 in shipped dtbs.
- `GetPropSyncHandler` has no mapped retail address; the fix rests on width alone.
- Not merged to main.
