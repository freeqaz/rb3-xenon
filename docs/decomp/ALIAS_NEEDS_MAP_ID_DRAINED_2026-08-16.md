# The nine addresses, identified — lane MAPID-1, 2026-08-16

Follow-up to ALIAS-2 (`ALIAS_UNPROVEN_REMAINDER_ADJUDICATED_2026-08-16.md`), which
left one concentrated lead: **16 `NEEDS_MAP_ID` memberships carrying 28,964 B
reduce to nine distinct unidentified addresses**, and named `fn_827BCD38` as the
same address GROUNDED-1 said gated its largest single block.

**All nine are now identified on retail bytes. The class is drained to zero.**

## Headline

| | before | after |
|---|---:|---:|
| `NEEDS_MAP_ID` memberships | 16 | **0** |
| bytes resting on an uncheckable placeholder | 28,964 | **0** |
| memberships withdrawn as fabricated | — | **14** (cost **0 B**) |
| memberships licensed by identification | — | **2** (`L3_EXACT`) |

Re-adjudicated over every installed membership, reconciling exactly
(−14 withdrawn, +2 promoted):

```
before (15,204):  PROVEN 14,374  NEEDS_SOURCE 738  CONTRADICTED 76  NEEDS_MAP_ID 16
after  (15,190):  PROVEN 14,376  NEEDS_SOURCE 738  CONTRADICTED 76  NEEDS_MAP_ID  0
```

⚠ **The briefed figure was tested literally before anything was built on it** —
it is correct: 16 memberships over exactly 9 distinct addresses. (Two counts
briefed to earlier lanes turned out to be slices no artifact contained; this one
is not.)

## Why the class was answerable at all

`l3_exact` walks the survivor's **retail** body word-for-word against **our**
compiled body for the folded spelling. Every one of the 16 got all the way to a
single relocated offset where retail's target name is a placeholder
`fn_XXXXXXXX`, so the comparison could not continue. **Everything before that
offset already compared equal.**

⇒ the question per address is narrow: *our side names a real function at that
offset — is retail's `fn_XXXXXXXX` the same function?* All nine retail bodies are
present in the target objs, so all nine are decidable from bytes; none is
undecidable by absence.

## The verdicts

| address | memberships | verdict | evidence |
|---|---:|---|---|
| `fn_827BCD38` | 2 | **IDENTIFIED = `?MemAlloc@@YAPAXHH@Z`** | its own relocations: `gMemLock`, `Enter`/`Exit@CriticalSection`, `malloc`, `MemHeap::Alloc`; 644 B; 155 of 162 comparable call sites agree |
| `fn_82BC4B28` | 2 | CONTRADICTED | calls `?Uninitialize@CAudioFilter@LEAPFX@@`, `??1CXAPOParametersBase@@` |
| `fn_82BD0EB8` | 2 | CONTRADICTED | calls `??1CX2SourceVoice@XAUDIO2@@`, `CNonBlockingQueue<MusicCommand*>` dtor |
| `fn_82BD2E78` | 2 | CONTRADICTED | same XAUDIO2 family |
| `fn_82BD4C48` | 2 | CONTRADICTED | XAUDIO2 vendor band, calls `fn_82BCE0D0` ×2 |
| `fn_82BD4EF0` | 2 | CONTRADICTED | XAUDIO2 vendor band, calls `fn_82BCE0D0` ×2 |
| `fn_82BF72E0` | 2 | CONTRADICTED | calls `?TB_SAFE_CLOSE_HANDLE@@` ×3, `??3@YAXPAX@Z` |
| `fn_82327050` | 1 | CONTRADICTED | our element serializer is map-resident **elsewhere** |
| `fn_824C8F68` | 1 | CONTRADICTED | same |

### Family A — `fn_827BCD38` = `MemAlloc`, and it licenses everything

Three independent lines, **none of them a size test** (ALIAS-2's rule that the
size comparison must stay inside one build is respected — our `MemAlloc` is a
20 B `malloc` stub against retail's 644 B, so a size test would have "refuted"
it):

1. **Retail's own named relocations.** `?gMemLock@@3PAVCriticalSection@@A` (×2),
   `?Enter@`/`?Exit@CriticalSection@@QAAXXZ`, `malloc`, `MemHeap::Alloc`
   (`fn_827BCA78`), plus a recursive self-call. A 644 B locked heap allocator.
2. **Two positional call sites in word-identical bodies.** Retail's
   `?MemOrPoolAlloc@@YAPAXHPBDH0@Z` (36 B) is word-for-word our 1-arg
   `MemOrPoolAlloc`, relocations lining up in place — `0x18` `fn_827BCD38` ↔
   `?MemAlloc@@YAPAXHH@Z`, `0x20` `PoolAlloc` 5-arg ↔ 2-arg, **a pair the alias
   map already equates**. And retail's `??2CriticalSection@@SAPAXI@Z` is
   `li r4,0 ; b fn_827BCD38` where ours is `li r4,0 ; b ?MemAlloc@@YAPAXHH@Z`.
3. **Census.** Of 162 retail sites we can compare, **155 agree**.

DC3 corroborates the *shape* (`size==0 → null; size > 0x80 → MemAlloc; else
PoolAlloc` — retail's body with the debug args stripped) but ⛔ **cannot
adjudicate the body**: DC3's `MemAlloc` is also a `malloc` stub. rb3-Wii holds
the real logic.

This confirms a hypothesis the tree already recorded as a **map coverage gap**
(`src/system/utl/MemMgr.cpp`, and group 1339's own evidence string). What is new
is that it is settled on bytes rather than asserted.

### Family B — twelve fabricated memberships, refuted two ways

Six groups claim the `ObjRefConcrete<RndCubeTex>` and `<RndFur>` deleting-dtor
thunks folded into XAUDIO2/LEAPFX vendor thunks.

* **Callee identity.** Retail's callee at the blocking offset is in every case a
  *vendor* destructor (above). Ours stores the per-`T` vtable
  `??_7?$ObjRefConcrete@V<T>@@VObjectDir@@@@6B@` and calls
  `?Release@Object@Hmx@@QAAXPAVObjRefOwner@@@Z`. Not the same function.
* ★ **Pigeonhole, independent of any build.** Each folded spelling is claimed by
  **six** groups whose survivors sit at **six distinct retail addresses**
  (`0x82bf7570`, `0x82bd4c98`, `0x82bc4d38`, `0x82bd0f30`, `0x82bd2ee0`,
  `0x82bd4f40`). A folded function has **one** address ⇒ at most one could be
  true, and none is.

⚠ **Why T1 admitted them, and why this generalises.** A scalar-deleting-dtor
thunk is 76 B of prologue / flag-test / epilogue boilerplate **identical across
every polymorphic class in the image** — all six retail survivors are raw-byte
identical to each other *and* to our thunk. Its entire discriminating content is
the two `bl` targets, which a masked-relocation test cannot see. So T1 accepts
**any** two deleting thunks of the same frame shape, here across a game↔vendor
boundary. This is GROUNDED-1's *"for a thunk the destination is the whole
information content"* running in the opposite direction.

⚠ The six groups also record **`0 census sites`** — they never met
`symbol_aliases.json`'s own paired-`bl` gate.

⛔ **An alias whose survivor is an uncompilable vendor symbol is structurally
unfalsifiable by any future source work.** All 43 declared `xdk/xaudio2/*.cpp`
objects have `src_path: None`, so no base body can ever exist. Worth a standing
screen.

### Family C — two fabricated memberships, refuted without any build comparison

`operator<<(BinStream&, list<PracticeSectionMapping>)` is claimed by **two**
groups (survivors at `0x82327878` and `0x824c93c0`) — the same pigeonhole. And
the clean refutation needs no size test at all: **our element serializer
`??6@YAAAVBinStream@@AAV0@ABVPracticeSectionMapping@…@Z` is itself map-resident
at `0x8230d6e8`**, so it is neither `fn_82327050` nor `fn_824C8F68`. Two `list<T>`
serializers whose element callees are different functions cannot be one COMDAT.

## The measurements

**Withdrawal of the 14 (report-only legs, ~2.5 s each):**

| leg | matched_code | Δ B | Δ pp | Δ fns | rows fell |
|---|---:|---:|---:|---:|---:|
| FULL | 3,723,428 | — | — | — | — |
| strip 14 WITHDRAWN | 3,723,428 | **0** | 0.000000 | 0 | **0** |
| strip 2 LICENSED | 3,694,464 | −28,964 | −0.280638 | 0 | 170 |
| strip all 16 | 3,694,464 | −28,964 | −0.280638 | 0 | 170 |

**Additivity: 14(0) + 2(−28,964) = −28,964 = 16(−28,964).**
⇒ **the entire 28,964 B rests on the two memberships the identification
licenses; the 14 fabricated ones forgave nothing at all.**

⚠ **A 0.00 B ablation is the shape of a vacuous run**, so it is licensed three
ways rather than asserted: the strip **asserts** it removed exactly the named
memberships (refuses otherwise); the identical mechanism and code path **moves**
−28,964 B / 170 rows on the licensed pair; and ALIAS-2's published class price
reproduces **exactly**. The 170 fallen rows are STLport `_M_insert_aux` /
`_M_fill_insert` / `_M_insert_overflow_aux` vector-growth paths — the
`MemOrPoolAllocSTL` callers — which corroborates *semantically* which pair carries
the bytes.

Whole-binary A/B of the withdrawal: **Δ0 on every measure, both rulers.**

**Naming `0x827bcd38` — forecast, then measured, agreeing to the byte:**

| | predicted | measured |
|---|---:|---:|
| `Δmatched_code` | −1,656 B | **−1,656 B** |
| `Δmatched_code_percent` | — | **−0.016045 pp** |
| `Δmatched_functions` | 0 | **+0** |
| `none` ruler | flat | **+0 B / +0.000000** |

Units at 100%: mpn 251 → 251, all-rows-fuzzy 129 → 129; **0 fell off either.**
`Δmatched_functions = 0` is structural, not luck — a relocation-name charge is an
**argument**-level `diff_arg` penalty and `mpn` excludes those.

⚠ The flat `none` control does **not** clear this change and is not cited as if
it did; it is a shape check only.

## Losing the 1,656 B is the correct outcome

`name_check` **forgives** placeholder targets, so all ~304 `bl fn_827BCD38` sites
were uncharged. Naming converts them to checked: **155 agree** (stay at 0),
**142 have no base body**, **7 disagree**. Six of the seven are at `fuzzy == 100`
and are **real wrong-callee divergences the placeholder was hiding**:

| bytes | row | we call |
|---:|---|---|
| 564 | `?Load@MetaMusic@@QAAXPBDM_N1@Z` | `?_MemAlloc@@YAPAXHH@Z` |
| 420 | `??0StreamReceiver360@@QAA@HH_N@Z` | `?_MemAllocTemp@@YAPAXHH@Z` |
| 260 | `?resize@VertVector@RndMesh@@QAAXH@Z` | `?_MemAllocTemp@@YAPAXHH@Z` |
| 196 | `??4VertVector@RndMesh@@QAAXABV01@@Z` | `?_MemAllocTemp@@YAPAXHH@Z` |
| 132 | `?_M_allocate_buffer@?$_Temporary_buffer@…` | `?_MemAlloc@@YAPAXHH@Z` |
| 84 | `?Init@UIStats@@QAAXXZ` | `?_MemAlloc@@YAPAXHH@Z` |

**Retail calls one function at all seven; our tree spells it three ways** —
`MemAlloc`, `_MemAlloc` (leading underscore, the rb3-Wii name), and the *temp*
allocator `_MemAllocTemp`. Until now the metric paid us for that. Per the standing
directive, a metric that hides real bugs is worse than a lower metric.

## `VALIDATE`

```
VALIDATE: PASS -- 1321 map-consistent, 203 tolerated (enumerated above), 0 contradicted, 1528 total
```

Shown able to **FAIL** first, or the PASS would be worthless: a fabricated
`?ForceSym@DataNode@@QBA?AVSymbol@@PBVDataArray@@@Z` ← `?Str@DataNode@@QBAPBDPBVDataArray@@@Z`
alias — two rows the map places at `0x8274afa8` and `0x8274b000` — yields
`CONTRADICTED (FATAL) 1` / `VALIDATE: FAIL`. Injected, observed, removed.

## What this lane did NOT do

* **Did not fix the six wrong-callee divergences.** Choosing between "our call
  sites are wrong" and "our `MemAlloc`/`_MemAlloc`/`_MemAllocTemp` split is
  wrong" is source work on the allocator family. **They are now visible, and
  fixing them returns the 1,656 B on merit.**
* **Did not port `MemAlloc`'s 644 B body** (ours is a 20 B `malloc` stub).
  ALLOCGATE-1's prescription still stands — but naming never depended on it, since
  byte identity is impossible while our body is a stub.
* **Installed no aliases.** In particular the defensible replacement for Family B
  (below) is left uninstalled: adding an alias lifts `matched_code` **by
  construction**, which is the hazard direction.
* **Did not touch the other 217 spellings of group 549** or the wider
  `0 census sites` problem those six groups have.

## Leads handed on

* ★ **The defensible Family-B replacement is a SAME-`T` alias**, not a
  cross-module one: `??_G?$ObjRefConcrete@V<T>@@VObjectDir@@` ↔
  `??_G?$Obj{,Owner,Dir}Ptr@V<T>@@`. `ObjPtr<T>` declares no destructor, so for one
  `T` the two thunks are byte- **and** relocation-identical and genuinely folded —
  retail kept the `Obj*Ptr` name (`ObjPtr<RndCubeTex>` @ `0x82435a78`,
  `ObjPtr<RndFur>` @ `0x82435ac8`). Provable for **13 of the 49** `ObjRefConcrete`
  spellings in group 549; the other 36 are *unidentified*, not folded. Corroborated
  by census: retail has **4** map-resident `??_G?$ObjRefConcrete@…` rows against
  **78** `??1` rows.
* ★ **Screen for it generally:** groups whose survivor is an **uncompilable vendor
  symbol** (`src_path: None`) can never be falsified by source work, and groups
  recording **`0 census sites`** never met the file's own gate. Both were true of
  all six Family-B groups.
* **The six allocator-spelling divergences** above are a small, well-defined
  source lever worth 1,656 B.

## Reproducing

```bash
python3 tools/mapid_probe.py         <wt> ~/tmp/mem.json    # blocking evidence per membership
python3 tools/mapid_ident.py         <wt>                   # identify each address from its own relocs
python3 tools/mapid_withdraw_build.py <wt> ~/tmp/mem.json ~/tmp/wd.json
python3 tools/mapid_ablate_control.py <wt> ~/tmp/wd.json ~/tmp/mem.json   # + anti-vacuity control
python3 tools/mapid_name_forecast.py <wt>                   # naming economics, before measuring
```

⛔ **Build the worktree first.** A fresh worktree's reflinked target objs are
pre-renamer, so every mangled name reads "absent" and every verdict would read
`NEEDS_SOURCE` — a decisive-looking negative that is pure instrument failure.
This lane asserted the gate before believing anything: **69,415 target bodies /
27,138 mangled**, matching main.
