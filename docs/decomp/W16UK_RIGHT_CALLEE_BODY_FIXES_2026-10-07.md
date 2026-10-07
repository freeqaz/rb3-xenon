# W16-UK: the census's "right callee, body differs" rows, fixed or explained (2026-10-07)

Base: main `e5a47716c` (W16-UH merged). Branch `w16-uk`, worktree `~/tmp/wt-w16uk`.

## Why

W16-UH (`docs/decomp/W16UH_PLACEHOLDER_CALLEE_CENSUS_2026-10-07.md`) found 20
call sites where our callee is the function retail calls but its body is not
retail's, plus two fabricated `merged_*` externs in `PlatformMgr_Xbox.cpp`.
This lane fixes each body to match retail's, or records why it cannot.

## What I expected going in

Most of these retail addresses are placeholders (`fn_XXXXXXXX`), which
`name_check` forgives at the call site, and most lie in `auto_*` units or in a
different unit's span. So I predicted **Δ0 for every fix that is not also
named**, and a gain only where the address can be named in a unit whose base
object defines the symbol. Predicted gains, from the retail sizes: `Exit` 12 B,
`Str` 8 B, `AddSyncObj` 40 B, `AppendLoader` 212 B, `PresetOverride` 96 B,
`Queue::~Queue` 80 B, `__unguarded_partition` 136 B, `ObjVector<Lod>::resize`
84 B. The risks I watched for: `ObjPtr_p.h` is a header used by most of the
engine, the `Lod` ctor moving out of line could change other callers, and
naming `JsonObject::Str` turns the `GetObjectAsString` call site from forgiven
into checked.

## Method

- **Body check:** `tools/icf_pair_adjudicate.py --pairs <file> --chase` on a fully built tree (patchers applied).
  For each pair it compares retail's body with our compiled body, with
  relocation targets compared by name and the callees chased one level down.
  The result is PROVEN or REFUTED.
- **Pricing:** one `tools/ab_measure.py --revert <sha>` run per commit, run one after another in the same worktree.
  Each run measures the branch tip against the tip with that one commit reverted. The ruler is `name_check`, as shipped.

## Verdicts

A/B legs: leg A is the branch tip and leg B is the tip with that one commit reverted.
Every run settled with leg A at **54,956 matched / 59.325336% code / fuzzy 64.322000**, so a commit's value is **minus** the printed Δ.
The table gives the value with the sign already flipped.
Chase results come from `--chase` on the final tree (`~/tmp/w16uk/chase4.txt`). For named rows the survivor is the map name, because their `fn_` names no longer exist.

| retail | function | commit | chase | worth (fns / B) | note |
|---|---|---|---|---|---|
| `0x822E36B8` 16 B | `GemTrackDir::UpdateFingerFeedback` | `6be7d146a` | PROVEN | 0 / 0 | null test HX_NATIVE; `auto_` unit |
| `0x8236D3F8` 12 B | `Character::Exit` | `edf6a88e9` | PROVEN | **+1 / +12** | `AutoSetCurrentCharacter` HX_NATIVE; named |
| `0x8246B740` 44 B | `RndShaderMgr::InitShaders` | `94f19076d` | PROVEN | 0 / 0 | `cache_shaders` read HX_NATIVE; MeshAnim span |
| `0x827B36E0` 8 B | `PreloadPanel::SetTypeDef` | `e81ff765d` | PROVEN | 0 / 0 | ICF body in DeJitterPanel span |
| `0x82B81F40` 8 B | `JsonObject::Str` | `abc0c625b` | PROVEN | **+1 / +8** | assert HX_NATIVE; named. `RockCentral::DataPointToQString` now checks its `Str` site: fuzzy 91.426 → 91.383, 0 B (row not at 100) |
| `0x823EA3E0` 40 B | `SyncStore::AddSyncObj` | `2dd9e4c94` + `5c16799b5` | PROVEN | **+1 / +40** (the alias commit; the body commit alone read 0 / 0) | body fixed and named → 99.5; the last charge was a fold, retail `bl 0x82B5F808` = `push_back<ChatReceiver*>`. `push_back<Synchronizable*>` admitted to that group on CHASED T1, `--validate` PASS |
| `0x8252A8A0` 4 B | `MemcardXbox::Terminate` + `VirtualKeyboard::Terminate` | `453ee9eb7` | PROVEN ×2 | 0 / 0 | out-of-line `b <empty>`; ICF fold of both, left unnamed |
| `0x823399C0` | `BandCharDesc::NewObject` | `08e792906` | PROVEN | 0 / 0 | `OBJ_NEW_OVERLOAD`; `auto_` unit |
| `0x823924F8` 212 B | `FileMerger::AppendLoader` | `4efe85863` | PROVEN | **+1 / +212** | EditMode `check_sync` block HX_NATIVE; named |
| `0x824CB940` 96 B | `WorldDir::PresetOverride` ctor | `d6b5405be` | PROVEN | **+1 / +96** | `ObjPtrInlineOwner` ctor; named |
| `0x8251A018` 80 B | `Queue::~Queue` | `7d4b537a1` | PROVEN | **+1 / +80** | explicit Enter/Exit, no `CritSecTracker`; named. Side effect: `fn_82C3FA94` (40 B, an `ObjRefOwner` dtor stub) fell from 78.5 to 0. It had been pairing by shape with our old unwind funclet for the tracker, which retail does not have. Fuzzy only, 0 B. |
| `0x825219A0` 396 B | `FileEnumerate` | `db987ad38` | PROVEN | 0 / 0 | `UsingCD`/Holmes/GetLastError/"." HX_NATIVE; `auto_` unit |
| `0x82697FE8` 132 B | `__unguarded_partition<pair<int,float>,PartPercentageSorter>` | `40d8cc020` | PROVEN | **+1 / +132** | our hand specialization removed, so the STLport template compiles; named |
| `0x82524838` 104 B | `JoypadTerminateCommon` | `5e360c6e4` | PROVEN | 0 / 0 | `RELEASE(gKeyboardExporter)` HX_NATIVE; OnlineID span |
| `0x82B699E8` 372 B | `FxSend360::~FxSend360` | `989e43824` | REFUTED (see note) | 0 / 0 | was 240 B; now 372 B with masked bodies equal. The chase stops at one slot: retail `bl 0x82B69868` = `FxSend360::Cleanup`, declared in `FxSend.h` but **defined nowhere in our tree** (true at base `e5a47716c` too). The out-of-line `CleanChain` it replaced was also never defined. `Synth360::Terminate` now calls `Cleanup` as retail does. |
| `0x82373F80` 84 B | `ObjVector<Character::Lod>::resize` | `edeb92cc0` | PROVEN | **+2 / +480** | `Lod` ctor out of line in `Character.cpp`; named. `PropSync<Lod>` (356 B) 96.40 → 100 and `fn_82373FD4` (40 B) 99.5 → 100 |
| `0x826C8470` 2,704 B | `GemPlayer::GemPlayer` | `d6a379977` | PROVEN | 0 / 0 | Symbol local, `==` tests, `DeltaTrackerInit` HX_NATIVE; `auto_` unit |
| `0x8230C628` 220 B | `ObjPtrList<T>::operator=` | `8389fc273` | REFUTED | 0 / 0 (1,005 recompiles in leg B, no row moved) | partial, see below |
| `0x823F2F08` | `QuazalSession::QuazalSession` | none | n/a | n/a | not attempted, see below |
| `0x82725298` | `opaquePredicate` | none | n/a | n/a | not reproduced, see below |
| (no retail body) | `PlatformMgr_Xbox.cpp` `merged_82610090`, `merged_DataArrayNode` | `042f9d1db` | n/a | 0 / 0 | invented externs removed; the default case uses DC3's `MILO_NOTIFY` text. SmartGlass code with no retail counterpart (0 `DtaToJson` strings in retail). |

**Lane total, summing the per-commit A/B values (deltas compose): +9 functions / +1,060 B.**
Predicted +8 / +668 B, from the named rows' own sizes. The difference has three parts:

- `resize` pulled two neighbours across (+1 / +396 B);
- `AddSyncObj` needed the fold alias to collect its 40 B;
- `__unguarded_partition` is 132 B, not the 136 B I wrote down.

Every other commit measured exactly Δ0, with no row moving at all (row-level diff of both legs, `~/tmp/w16uk/rowdiff.py`).

## Not fixed

Three of the 20 rows do not reach retail's body. Each has a recorded reason.

- **`0x8230C628` `ObjPtrList<T>::operator=`: partly fixed and still REFUTED** (220 B retail, 16 spellings, 55 sites).
  - **What changed (`8389fc273`).** Our X360 arm now assigns existing nodes through `Set(it, *oit)` and `push_back`s the rest, as retail does.
    - Retail calls `Set` out of line for a `T` whose `Object` base is virtual.
    - Ours had inlined the release/add-ref pair, so it was 264 B. It is now 204 B against retail's 220 B.
  - **What still differs.** Retail passes the iterator through a stack temporary and tests the first loop's condition at the top.
    - I tried `Set(iterator(n), ..)` and `Set(it++, ..)`. Neither produces the temporary.
    - `it++` also broke the `Hmx::Object` instantiation.
  - **Control.** That instantiation, `0x8248AEE8` (TexBlender, fuzzy 100), is unchanged byte for byte.
  - **Why it cannot pay yet.** The address sits in the `SongSectionController` span, so it would not pair even if it matched.
- **`0x823F2F08` `QuazalSession::QuazalSession` (OUT-NET): not attempted.**
  - Retail's 216 B constructor polls until a global is set and allocates a 32 B `Quazal::RootObject`.
  - It then calls about five Quazal functions that are not named in our map (`0x82A936C8`, ...).
  - No source exists for it: the rb3-Wii file is an empty stub and DC3 has no Quazal.
  - Writing it means reverse-engineering Quazal, which the standing scope directive rules out ("QUAZAL = LOW VALUE").
- **`0x82725298` `opaquePredicate` (OUT-360-OTHER, the `keygen_xbox` `/Od` island): not reproduced.**
  - Retail increments a `.data` word at `0x82E03E1C`. It forms that address with `lis`+`addi` and then loads at offset 0, and it does so twice: once for the load and once for the store.
  - Every variant I compiled folds the low half into the displacement instead (`lis` then `lwz lo(r11)`).
  - Variants tried:
    - **Declaration:** extern, defined, `static`, an array, a struct member, `*&x`, a pointer local, `x = x + 1`.
    - **Flags:** `/Od`, `/Od /Os /Oi-`, `/GS-`, `/Oy`, `/Ob0`, `/Zi`.
  - The likely difference is how the variable is defined in the original object, for example as a `.data` symbol defined in assembly. Our tree cannot express that.

## Why most fixed addresses stay unnamed

Fixing a body at an unnamed address is worth 0 B, because `name_check` already forgives the call site. It pays only when the address is named **and** lies inside a unit whose base object defines that symbol. Otherwise the named row cannot pair, and naming it only converts a forgiven call site into a checked one.

Addresses this lane named (map rows in the commits):
`0x8236D3F8` `Character::Exit`, `0x82373F80` `ObjVector<Lod>::resize`,
`0x823924F8` `FileMerger::AppendLoader`, `0x823EA3E0` `SyncStore::AddSyncObj`,
`0x824CB940` `WorldDir::PresetOverride` ctor, `0x8251A018` `Queue::~Queue`,
`0x82697FE8` `__unguarded_partition<pair<int,float>, PartPercentageSorter>`,
`0x82B81F40` `JsonObject::Str`.

Left unnamed, by reason:

| reason | rows |
|---|---|
| in an `auto_*` unit (no base obj to pair against) | `0x822E36B8` UpdateFingerFeedback, `0x823399C0` BandCharDesc::NewObject, `0x826C8470` GemPlayer ctor, `0x82B699E8` ~FxSend360, `0x825219A0` FileEnumerate |
| in another unit's span | `0x8246B740` InitShaders (MeshAnim span), `0x827B36E0` SetTypeDef (DeJitterPanel span, an ICF body), `0x82524838` JoypadTerminateCommon (OnlineID span), `0x8230C628` ObjPtrList::operator= (SongSectionController span) |
| ICF fold of two functions | `0x8252A8A0`. Naming it `MemcardXbox::Terminate` would charge the `VirtualKeyboard::Terminate` site in `SystemTerminate`, which is fuzzy 100, and no alias group exists to forgive that. |

## Gates

`tools/native_build_gate.sh` in `~/tmp/wt-w16uk` at `5c16799b5` (every source change in; run after all A/B runs, as the last build):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`tools/icf_alias_finder.py --validate`: `PASS -- 1940 map-consistent, 220 tolerated, 0 contradicted, 2160 total`. `tools/alias_callee_name_drift.py --check`: rc=0.

## Not done

- **`FxSend360::Cleanup` (retail `0x82B69868`, 332 B) is not ported.** It was declared and undefined before this lane. Until it exists, the `~FxSend360` chase cannot discharge its one callee slot. The address is in an `auto_` unit, so porting it is worth 0 B.
- **The `ObjPtrList::operator=` stack-temporary residual is left open.** The address is not pairable (SongSectionController span).
- **No naming** of `0x8252A8A0`, or of any address outside its unit's span (see above).
- **No re-run** of `tools/placeholder_callee_census.py`. This lane's changes are bodies and map names, not callees. The census's own known answers live in W16-UH.
- **Not merged, not pushed.**
