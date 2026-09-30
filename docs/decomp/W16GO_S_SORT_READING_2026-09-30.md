# W16-GO — the `_S_sort<I>` / `_S_sort<Symbol>` question (GK T1), settled on retail bytes

**Lane:** W16-GO · **Worktree:** `~/tmp/wt-w16-go` (branch `w16-go`, base `8af79551`)
**Question (from GK T1, `W16GK_UTL_BLOCK_ADJUDICATION_2026-09-16.md`):**
`?CopyTypeProperties@@YAXPAVObject@Hmx@@0@Z` (1,472 B, fuzzy 99.9457) is charged
at 4 `bl` sites where retail calls `0x824e1858` (map: `_S_sort<I,less<I>>`) and we
call `_S_sort<Symbol,less<Symbol>>`. GK refused to alias because the map places our
spelling on a *different* 424 B body at `0x827e5d88`, and left two readings open:

- **A** — `0x824e1858` is an ICF survivor serving several instantiations; `<I>` is an
  arbitrary survivor name; our call is right ⇒ alias.
- **B** — retail's `CopyTypeProperties` sorts a container of some other element type
  and our source calls the wrong instantiation ⇒ source fix; an alias would hide it.

## 0. Verdict in one paragraph

**Reading A is correct, and GK's blocker was a map defect (call it Reading C).**
`0x824e1858` is the linker's fold survivor for every `list<T>::sort()` whose
comparator compiles to an unsigned 4-byte compare — `T ∈ {unsigned int, Symbol}` at
least — proven by a map-independent caller census (a `list<unsigned int>` sorter and
two `list<Symbol>` sorters all `bl` the same body), by the survivor's callee chain
(every callee is itself a pan-type fold survivor named after an unrelated type), and
by a signed-`int` control that stays unfolded exactly where the model says it must.
The body at `0x827e5d88` that the map called `_S_sort<Symbol,less<Symbol>>` is
**`_S_sort<TextInstance, StlNodeAlloc<TextInstance>, WidgetInstanceCmp<TextInstance>>`**
— its merge compares a `float` at element offset 0x34, its `clear` frees 0x5c-byte
nodes after running `String::~String` on the member at element offset 0x40, and the
compiler reports `sizeof(TextInstance) == 84` with `mText` at 0x40. So the "two real
bodies for one name" contradiction never existed: one name was wrong. **Reading B is
dead on retail's own bytes**: `Symbol` has no integral constructor, and retail's loop
body — byte-identical to ours — passes list elements straight into
`Hmx::Object::Property(Symbol, bool)`.

What this lane changed: **one map row** (`0x827e5d88` → the TextInstance spelling),
measured with `ab_measure`. What it did not change: source (nothing to fix), aliases
(owned by W16-GM — exact membership is in §6), pins (the right home for the row is a
TU our tree does not carry; §5).

## 1. Why GK's two observations were not in tension

GK measured that the two retail bodies have **identical masked bytes** but differ at
**7 of 22 relocation slots**, and read that as "a fold could not have happened". Both
facts are correct; the inference is not, because of what an `_S_sort` body contains:

- `_S_sort` never touches an element. It splices, swaps, and calls `_S_merge`; the
  only `T`-dependent code is in its callees. So **every** `_S_sort<T,A,C>` has the
  same instruction bytes and differs only in relocations. Identical masked bytes
  between two `_S_sort`s carries *no* information about `T`.
- `Symbol::operator<` is `mStr < s.mStr` (`src/system/utl/Symbol.h:25`) — a raw
  pointer compare, i.e. `cmplw`, the same instruction `less<unsigned int>` produces.
  So `_S_merge<Symbol,less>` and `_S_merge<I,less>` are byte-identical *including*
  their leaves, and the whole chain folds.

The 7 differing slots are exactly the `T`-dependent callees:

| slot (role in STLport `_S_sort`) | `0x824e1858` (`<I>` label) | `0x827e5d88` (`<Symbol>` label) |
|---|---|---|
| element dtor passed to `__ehvec_ctor`/`__ehvec_dtor` (`lis`+`addi`, ×2 sites = 4 slots) | `0x828043a8` = `__destroy_aux<LocalePanel::Entry>` (4 B `b` thunk) | `0x827e5c48` = 4 B thunk `b 0x827e53e0` (map: `~_List_base<MidiParser*>`) |
| `_S_merge` (×2) | `0x8232be58` = `_S_merge<Symbol,less<Symbol>>` | `0x827e4c68` (unnamed) |
| final `~list(carry)` (×1) | `0x82718880` = `_List_base<SynthPollable*>::clear` | `0x827e53e0` (unnamed) |
| shared: `swap` ×3, `__ehvec_ctor`/`_dtor`, list-ctor label | `0x8241aac8` = `list<BSPFace>::swap`, `??_L`, `??_M`, `lbl_8243F3C0` | same |

Read the left column: a `<I>` sort whose element destructor is named after
`LocalePanel::Entry`, whose merge is named after `Symbol`, whose final clear is named
after `SynthPollable*`, whose swap is named after `BSPFace`. That is what a
maximally-folded chain looks like — each callee is the survivor of its own fold
group, and the survivor's name is whichever COMDAT the linker kept. The right column
is a chain that did **not** fold with anyone and whose members all sit in one
neighbourhood (`0x827e4c68`, `0x827e53e0`, `0x827e5c48`, `0x827e5d88`).

## 2. Evidence for the fold at `0x824e1858` (Reading A)

### 2a. Caller census — map-independent element types

`tools/retail_callers.py 824e1858` (every retail `bl` reaching the address), callers
resolved to enclosing functions via `symbols.txt` + the map:

| call site | enclosing retail function | what it sorts (evidence) |
|---|---|---|
| `0x8233260c` | `?OnSelectExtras@BandWardrobe@@QAA?AVDataNode@@PAVDataArray@@@Z` | `std::list<Symbol> unk2c; unk2c.sort();` — our `BandWardrobe.h:111` / `.cpp:1364`, rb3 `.cpp:1380`; our `BandWardrobe.obj` instantiates `_S_sort<Symbol,less<Symbol>>` and nothing else in the family |
| `0x824e1f1c` | `?GetMeshShaderFlags@?A0xfe071329@@YAXPAVRndMat@@AAV?$list@IV?$StlNodeAlloc@I@...@@@Z` | `std::list<unsigned int>` — it is in the function's own signature; our `Crowd.cpp:56` |
| `0x82759bb4/bc0/bcc/bd8` | `?CopyTypeProperties@@YAXPAVObject@Hmx@@0@Z` | four `std::list<Symbol>` (§3) |

One body, three callers, two distinct element types. Under Reading B (retail kept
`<I>` and `<Symbol>` as separate bodies) `OnSelectExtras` and `CopyTypeProperties`
could not share a body with `GetMeshShaderFlags`. They do.

### 2b. The merge has no other life

`0x8232be58` (`_S_merge<Symbol,less<Symbol>>`, 184 B) has **exactly two** callers —
both inside `0x824e1858`. A `<I>` sort calling a `<Symbol>` merge is impossible if the
labels are literal and is the *expected* shape if both are arbitrary survivor names.
The project has already accepted this one level down: `scripts/symbol_aliases.json`
carries a T1 group at `0x8232be58` with survivor `_S_merge<Symbol,less>` and folded
`_S_merge<I,less>`.

### 2c. Signed-`int` control — the fold boundary is comparator codegen

`_S_sort<int,less<int>>` sits at its **own** address `0x8241b6b0` with its **own**
merge `0x82419280`, whose element compare is `cmpw` (signed), while it shares the
folded `clear` (`0x82718880`, `li r3, 0xc`) and `swap` with the `<I>`/`<Symbol>`
chain. Same node size, same shape, one instruction of comparator difference — and it
did not fold. That is the model predicting a non-fold correctly, which is what makes
its prediction of the fold trustworthy.

### 2d. Node size

The folded `clear` `0x82718880` frees with `li r3, 0xc`: node = 8-byte links + a
4-byte element. `Symbol` and `unsigned int` are both 4 bytes.

## 3. Evidence against Reading B — retail's `CopyTypeProperties` sorts `list<Symbol>`

- The row is at fuzzy 99.9457 on 1,472 B with **only** the four `bl` arguments
  charged: every other instruction, including the merge-walk
  `for (; toIt != toProps.end() && *toIt < prop; ++toIt)` and the calls
  `from->Property(prop, true)` / `to->SetProperty(prop, *fromVal)` where `prop` is
  `*fromIt`, is byte-identical to ours.
- `Symbol` has exactly two constructors, `Symbol()` and `Symbol(const char*)`
  (`Symbol.h:16-17`). A `list<unsigned int>` element cannot reach a `Symbol`
  parameter without a conversion that would emit code; none is emitted.
- `WalkProps(DataArray*, std::list<Symbol>&, std::list<Symbol>*)` and
  `ListProperties(...)` are `list<Symbol>` in DC3 (`dc3-decomp/src/system/obj/Utl.cpp:252`)
  and in our tree; the retail call sites for both are uncharged.

There is no container in the function with any other element type. Reading B has no
source construct to name.

## 4. What `0x827e5d88` actually is — the map defect

| instrument | reading | implication |
|---|---|---|
| its merge `0x827e4c68` (200 B, vs 184 B for the folded one) | element compare is `lfs f0,0x3c(r11); lfs f13,0x3c(r10); fcmpu` | comparator reads a **float at element offset 0x34** (node data starts at +8); not `less<>` of anything 4 bytes |
| its `clear` `0x827e53e0` (76 B) | `addi r3,r29,0x48; bl 0x827bdf38 (= ??1String@@UAA@XZ)` then `li r3,0x5c; bl MemFree` | element is **84 B with a `String` at offset 0x40** |
| its element-dtor thunk `0x827e5c48` | `b 0x827e53e0` | `~_List_base<same T>` — the map's `<MidiParser*>` label is wrong too (a `list<MidiParser*>` clear would free 0xc-byte nodes) |
| its only caller `fn_827E6470` (64 B) | `lwz r11,0(r3); lwz r11,0x44(r11); bctrl` → `stb 0,0x50(r1); lbz r4,0x50(r1); bl 0x827e5d88` | `TrackWidgetImp<T>::Sort() { DoSort(Instances()); }` — virtual `Instances()` (slot 0x44), empty functor passed by value, sort |
| compiler (`class_layout_report.py TextInstance --tu src/system/track/TrackWidget.cpp`) | `sizeof = 84 (0x54)`; `mXfm` @0 (`v.y` @0x34), `mText` @0x40, `mLineId` @0x4c, `mUseAltStyle` @0x50 | the only widget instance type with these numbers; `WidgetInstanceCmp<T>` compares `mXfm.v.y` |
| siblings in the same cluster | `0x827e3e10` → clear `0x827e1708` frees `0x4c` (68 B = `MeshInstance`: `Transform`+`RndMesh*`); `0x827e3c38` → clear `0x8243dbf0` = `_List_base<RndMultiMesh::Instance, TransformListAlloc>::clear`; all three share the float merge | three `TrackWidgetImp<T>::Sort` instantiations, `T ∈ {RndMultiMesh::Instance, MeshInstance, TextInstance}` |

Name, spelled by the compiler (scratch TU `l.sort(WidgetInstanceCmp<TextInstance>())`
built with `TrackWidget.cpp`'s exact `cl` flags):

```
??$_S_sort@VTextInstance@@V?$StlNodeAlloc@VTextInstance@@@stlpmtx_std@@V?$WidgetInstanceCmp@VTextInstance@@@@@stlpmtx_std@@YAXAAV?$list@VTextInstance@@V?$StlNodeAlloc@VTextInstance@@@stlpmtx_std@@@0@V?$WidgetInstanceCmp@VTextInstance@@@@@Z
```

Why the wrong label scored **99.81** in `default/VocalTrack` (and would have kept
scoring it): our `VocalTrack.obj` does instantiate `_S_sort<Symbol,less<Symbol>>`, the
bodies are shape-identical, and the retail callees that differ are mostly **unnamed**
(`0x827e4c68`, `0x827e53e0`, `lbl_8243F3C0`) — placeholders that `name_check` forgives.
A wrong pairing hidden by forgiveness. The `<Symbol>`-labelled row and the `<I>`
row both read 99.81132 for different reasons; neither number says anything about
which label is right.

Provenance: the row and the single-function VocalTrack pin
(`.text 0x827E5D88–0x827E5F58` = the sort + its 40 B EH funclet `fn_827E5F30`, which
destroys `carry` through the same thunk) both date from the TU5 flip regen
(`a320bc12`).

## 5. The neighbourhood is a bigger mis-pin than this lane (recorded, not fixed)

`MidiParser.cpp` and `TrackWidget.cpp` pins **alternate** through
`0x827E1768–0x827E64B8`. Census of map names per block (rows named / MidiParser-named
/ widget-named):

```
TrackWidget  827e1768-827e4098  38 / 0 / 33
MidiParser   827e4098-827e4568   0 / 0 / 0     <- holds nothing named; TrackWidget COMDATs
TrackWidget  827e4568-827e48c0   4 / 0 / 4
MidiParser   827e48c0-827e5260   0 / 0 / 0     <- holds fn_827E48C0 (MeshInstance Sort), 0x827e4c68 (float merge)
TrackWidget  827e5260-827e5314   2 / 0 / 2
MidiParser   827e5314-827e5d88   2 / 2 / 0     <- the two "MidiParser" names are the TextInstance chain's dtor thunks
VocalTrack   827e5d88-827e5f58   1 / 0 / 0     <- the TextInstance sort (this doc)
MidiParser   827e5f58-827e6530   1 / 1 / 0     <- 15 unnamed fns incl. fn_827E6470 (TextInstance Sort); MidiParser proper starts at 0x827e64b8 (?StaticClassName@MidiParser@@)
MidiParser   827e6530-...        all MidiParser
```

Two TUs cannot interleave at function granularity without LTCG. Everything below
`0x827e64b8` in those "MidiParser" blocks that I opened is track-widget machinery.
The likely true owner of `0x827E5314–0x827E64B8` is **`TrackWidgetImp.cpp`** — a TU
rb3 has (`rb3/src/system/track/TrackWidgetImp.cpp`, 360 lines: `CharWidgetImp`,
`MultiMeshWidgetImp` definitions) and **our tree does not carry at all** (we have only
the header). That is why our `TrackWidget.obj` emits no `TrackWidgetImp<TextInstance>`
member even though `SyncImp()` does `new CharWidgetImp(...)`: MSVC emits the vtable —
and therefore `Sort()` and `_S_sort<TextInstance>` — in the TU that defines the
constructor. Retail has three `Sort()` bodies (`0x827e4880` MultiMesh, `fn_827E48C0`
Mesh, `fn_827E6470` Text); we compile two.

⇒ For a source/pin lane: port `TrackWidgetImp.cpp` (~4.4 kB of retail at
`0x827E5314–0x827E64B8` plus the two gap blocks that are `TrackWidget.cpp` COMDATs),
then re-pin. Side-finding while there: retail's MultiMesh widget list uses
`TransformListAlloc` (its clear is `_List_base<Instance, TransformListAlloc>::clear`)
where our `TrackWidget.obj` instantiates `StlNodeAlloc<RndMultiMesh::Instance>` — a
second divergence in `TrackWidgetImp.h`. Not chased here.

I did **not** re-home the VocalTrack block: there is no compiled unit to home it to,
and pinning it to `TrackWidget.cpp` would trade one wrong owner for another.

## 6. Alias membership for the coordinator (after W16-GM lands)

Group to add (T1, same pattern as the existing `0x8232be58` merge group):

```
name:      $_S_sort
address:   0x824e1858
survivor:  ??$_S_sort@IV?$StlNodeAlloc@I@stlpmtx_std@@U?$less@I@2@@stlpmtx_std@@YAXAAV?$list@IV?$StlNodeAlloc@I@stlpmtx_std@@@0@U?$less@I@0@@Z
folded:    ??$_S_sort@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@U?$less@VSymbol@@@3@@stlpmtx_std@@YAXAAV?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@0@U?$less@VSymbol@@@0@@Z
evidence:  §2a caller census (2 list<Symbol> sorters + 1 list<unsigned int> sorter -> one body);
           §2b merge has only intra-survivor callers and is already aliased I<->Symbol;
           §2c signed-int control unfolded; §2d node size 0xc; §4 the rival map row was a
           TextInstance sort and has been renamed, so the validator's CONTRADICTED
           (two map-resident members) no longer fires.
```

Predicted effect of the alias (not measured here): `?CopyTypeProperties@@` 99.9457 →
100 (+1 fn / +1,472 B); `default/system/obj/Utl` 30/31 → 31/31; the Crowd `<I>` row
unchanged (its residual charge is elsewhere in its chain). `masked_equal` unchanged.

## 7. The change made here, with predictions committed before measurement

**Change:** `scripts/target_symbol_map.json`, one value: `"0x827e5d88"` from the
`_S_sort<Symbol,...,less<Symbol>>` spelling to the TextInstance spelling in §4.
No source, no splits, no aliases.

**Mechanism:** the row in `default/VocalTrack` stops pairing (`VocalTrack.obj` does
not define the TextInstance instantiation — nothing in our tree does, §5). A
paired-but-wrong row at 99.81132 becomes an unpaired row at 0.

**Predictions (`ab_measure --from-dirty`, graded ruler `name_check`):**

| measure | predicted Δ | why |
|---|---|---|
| `matched_functions` | **0** | the row was not at 100 before |
| `matched_code` | **0 B** | same |
| `masked_equal_functions` | **0** | untouched |
| `fuzzy_match_percent` (whole binary) | **≈ −0.0041 pp** (424 × 0.99811 / 10,247,068) | one 424 B row 99.81 → 0 |
| `default/VocalTrack` | 220 rows, 179 matched unchanged; unit fuzzy down | the row stays in the unit's denominator |
| `none` control leg | same shape (Δ0 / Δ0 / ≈ −0.0041) | un-pairing is ruler-independent |
| tool assertions | SPLIT ran on both legs, renamer patched > 0 files, `symbols.txt` fixed point in 0 extra iterations | map-only patch |

A specific nonzero fuzzy delta is the control: a run that reports Δ0 on every key
would mean the rename did not take (inert map edit), not that it was harmless.

**Measured:** *(filled in §8 after the run)*

## 8. Measured result

`tools/ab_measure.py --worktree ~/tmp/wt-w16-go --from-dirty --label w16go-rename-0x827e5d88`,
run dir `.ab_measure_runs/20260930-013548-w16go-rename-0x827e5d88-3698072`, tool
blob `9c5dec33` == HEAD, objdiff-cli `sha256:c1b7d952…` stable across legs, both legs
settled (leg A: 381 msvc + 1 split + 7 patch on the forced re-split, then 0 work;
leg B: split=1, `renamer_patched=1833`, then 0 work), `symbols.txt` at its split
fixed point on both legs after **0** extra re-splits (sha chain `1e8375f9 -> 1e8375f9`).

| measure | predicted | measured |
|---|---|---|
| `matched_functions` | 0 | **+0** (44,154 → 44,154) |
| `matched_code` | 0 B | **+0 B** (40.772522 % → 40.772522 %) |
| `masked_equal_functions` | 0 | **+0** (23,323) |
| `honest` | 0 | +0 (20,831) |
| `fuzzy_match_percent` | ≈ −0.0041 pp | **−0.004126 pp** (50.497726 → 50.493600) |
| the row (`default/VocalTrack`, 424 B) | 99.81 → 0, still in the unit | **99.81132 → 0**, unit 220 rows / 179 matched unchanged, unit fuzzy 90.7703 → 89.81655 |
| units at 100 (mpn / all-rows-fuzzy) | unchanged | 196 → 196 / 173 → 173 |
| `none` control leg | Δ0 / Δ0 / ≈ −0.0041 | `matched` **45,574 → 45,573**, `matched_code` **−424 B** (Δcode% −0.004138) |

Every prediction held. The one thing the prediction table under-stated is the
`none` control: under the `none` ruler the mislabelled row had been counted as a
**matched function with 424 B of matched code** — a false 100 — and only
`name_check`'s charge on the `<MidiParser*>` dtor-thunk name kept it out of the
graded count. The rename removes that false credit on `none` and costs nothing on
the graded ruler. `ab_measure` classified the `none` movement as `REAL_PAIRING`
("a nulling removes false byte credit"), which is the correct reading.

The worktree was handed back with the map edit as its dirty state and the edit was
then committed on `w16-go`.

## 9. What this lane did not do, and why

- **No source edit.** Reading B named no source construct; the 4 charged sites are a
  fold. `VocalTrack.cpp` instantiating `_S_sort<Symbol,less>` while retail's
  `0x824e1858` has no VocalTrack caller is a lead (an inline in some header sorts a
  `list<Symbol>` that retail's VocalTrack TU never emitted) — noted, not chased.
- **No alias edit** — W16-GM owns the file. §6 is the exact membership.
- **No pin move** (§5). **No rename of `0x827e5c48`** (`~_List_base<MidiParser*>` →
  the TextInstance `~_List_base`, `??1?$_List_base@VTextInstance@@V?$StlNodeAlloc@VTextInstance@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ`,
  or the `list` dtor spelling — a 4 B `b` thunk cannot tell those apart, which is
  itself a reason not to pick one): it currently reads 100 in `default/MidiParser`
  through placeholder forgiveness, and renaming it is a second change with its own
  −1 fn; it belongs with the §5 re-pin.
- **No adjudication of the other 12 `_S_sort` rows, and no rename of the two
  sibling rows.** The bytes give the TrackWidget cluster a consistent reading —
  `0x827e3c38` is the `RndMultiMesh::Instance` sort (called by
  `?Sort@?$TrackWidgetImp@UInstance@RndMultiMesh@@@@UAAXXZ`, clear =
  `_List_base<Instance, TransformListAlloc>::clear`), `0x827e3e10` is the
  `MeshInstance` sort (68 B nodes), `0x827e5d88` is the `TextInstance` sort — while
  the map labels them `MeshInstance`, `Waypoint*/ObjNameSort` and `Symbol/less`
  respectively. Only the third blocks the GK alias, so only the third was renamed and
  measured; the other two are filed here for the §5 re-pin lane (the MultiMesh one
  also needs the allocator question settled before its spelling is known). The
  remaining 12 family rows were not opened.

---

## Addendum (coordinator, 2026-09-30 01:5x UTC) — W17-SSORT landed first

A concurrent coordinator session's lane **W17-SSORT** landed the same verdict on main
(`876abdd5`, 01:30 UTC) before this lane reported: the identical `0x827e5d88` rename
(byte-identical mangled name) **plus** the `0x824e1858` fold alias, measured there at
**+2 fns / +2,392 B** (`CopyTypeProperties` 1,472 + `BandWardrobe::OnSelectExtras` 920).
This lane's map commit is therefore a no-op on rebase and is **not** what put the rename
on main; its measurements above are an **independent replication** on the same ruler
(same Δ0/Δ0/Δfuzzy −0.004126 pp for the rename leg), reached without knowledge of W17-SSORT.

What this doc adds beyond W17-SSORT, all UNACTIONED:
- `TrackWidgetImp.cpp` is a TU our tree lacks entirely; its code (`0x827E5314–0x827E64B8`) is
  mis-pinned inside `MidiParser.cpp`'s range, which alternates with `TrackWidget.cpp` pins
  through `0x827E1768–0x827E64B8`.
- ⚠ **CONFLICT TO ADJUDICATE:** this lane claims the map labels at `0x827e3c38` / `0x827e3e10`
  are swapped; W17-SSORT used `0x827e3c38`'s MeshInstance name as a *passing* control. Both can
  hold — W17 compared the compiler-emitted SPELLING to the row's name string, not which body sits
  at which address — so neither result refutes the other yet.
- `0x827e5c48` labelled `~_List_base<MidiParser*>` is the TextInstance list dtor thunk.
- Retail's MultiMesh widget list uses `TransformListAlloc` where ours uses `StlNodeAlloc`.
