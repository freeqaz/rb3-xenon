# Empty handler `Symbol` globals in the 18 native runtime targets — measured

Lane SYMEMPTY, 2026-11-22, worktree `~/tmp/wt-symempty`, branch `symempty` (from main `637a73bda`).
Question: `NATIVE_STUB_GLOBALS_VS_RETAIL_2026-11-22.md` §1 said ~100 handler `Symbol` globals that native defines
by hand stay empty, so a TU built without `RB3_HANDLE_LOCAL_STATIC` compares messages against an empty Symbol.
A4 refuted that for rb3-app (7,074 of 7,077 interned at `UIManager::Init`; control 27/27 empty at `main`).
This lane measures the other 18 targets of `tools/native_health.sh`.

## 1. Result

- **Prediction:** most targets would show the empties, and a few handler TUs would really dispatch on them.
- **Measured:** the empties exist (6 to 158 per target), but in 15 of the 18 targets **no linked code reads
  any of them** apart from the line that defines them. They are dead definitions, so nothing compares against
  them. rb3-frame links no Milo `Symbol` at all.
- **The only real readers were in rb3-milo (7 functions, 4 TUs) and rb3-render (1 function).** None of them was
  called in the health invocation (call count 0 under gdb), so the defect was latent. It would trigger as soon
  as a message reached those objects.
  - rb3-milo: `BandCharacter::Handle` / `::SyncProperty`, `BandConfiguration::Handle`,
    `EventAnim::SyncProperty` and two `PropSync(EventAnim::…)` helpers, `BandWardrobe::Handle` (compiled
    inside `world/Crowd.cpp` here), and `BandTrack::Reset` (`me_percent_format`, native-only `#ifdef HX_NATIVE`
    code). rb3-milo calls `Symbol::Init` but never interned `milo_link_stubs.cpp`'s 140 globals, so **all 141**
    handler Symbols were empty from `main` to exit.
  - rb3-render interns those 140 (`InternSymbolGlobals_MiloLinkStubs`, main_render.cpp) **before** its first
    handler runs. Five were still empty, and one of them is a handler key: `hack_fix_clips_pre_merge`, which
    `BandCharacter::Handle` dispatches on and the intern list misses.
- **Fixed (native-only, no `src/` edit, X360 untouched):**
  - `native/CMakeLists.txt`: each TU gets exactly the define X360's `objects.json` `extra_cflags` gives it.
    `BandCharacter.cpp` gets `RB3_HANDLE_LOCAL_STATIC` + `RB3_SYNCPROP_LOCAL_STATIC`. `BandConfiguration.cpp`
    gets `RB3_HANDLE_LOCAL_STATIC`. `world/EventAnim.cpp` gets `RB3_SYNCPROP_LOCAL_STATIC`. All are added with
    `set_property(... APPEND ...)`. These source properties also reach rb3-render and rb3-app, which compile the
    same files. That is harmless: it is the retail form, and the generated globals are weak.
  - `native/src/main_milo.cpp`: call `InternSymbolGlobals_MiloLinkStubs()` right after `Symbol::Init()`, as
    main_render.cpp does. This is the fallback for `BandWardrobe::Handle`: X360 gives `BandWardrobe.cpp` the flag,
    but natively that body is compiled inside `Crowd.cpp`, and X360 gives `Crowd.cpp` no flag. So no per-file flag
    matches retail there. The same call also interns `me_percent_format` for `BandTrack::Reset`.
- **After the fix**, measured on the rebuilt binaries:
  - rb3-milo: 140 ok, 1 empty (`hack_fix_clips_pre_merge`), **0 handler-symbol readers**, `RESULT: ALL GATES PASSED`.
  - rb3-render: the `BandCharacter::Handle` reader is gone. The remaining hits are non-handler (§3).

## 2. Method

Scripts are in `~/tmp/symempty/t/` (scratch). Per-target raw output: `~/tmp/symempty/head/` (baseline) and
`~/tmp/symempty/fix/` (milo and render after the fix).

1. **Population. There is no name list, so namespaces, class statics and arrays are included.** `symtab.py`
   takes every defined data object from `nm -S` (types b/B/d/D/V/u), demangles it, and records the
   `.data` / `.data.rel.ro` / `.bss` section ranges.
2. **Read** (`symread.py`, gdb batch, run through `coord/no610m.sh` with native_health.sh's argv for each target).
   It reads **every 8-aligned word** of those sections at four points:
   - (0) `main`, the control, before `Symbol::Init`;
   - (1) right after `Symbol::Init` returns;
   - (2) the first call of any `*::Handle(DataArray*, bool)` or `*::SyncProperty(DataNode&, DataArray*, int,
     PropOp)`, which is the first point a handler can run. Breakpoints go on every such function in the binary,
     872 in milo and 1,139 in render. The script logs whether that call came before or after `Symbol::Init`;
   - (3) `exit`.

   Each 8-byte object is classified:
   - `empty`: its value is `gNullStr`, the mStr of a default-constructed Symbol;
   - `ok`: it points at a string equal to its own demangled leaf name, so local statics count too;
   - `zero` or `other`.

   It also lists every `gNullStr` word inside larger objects. All handler breakpoints keep a 10^9 ignore count
   after the first stop, so `.hits.tsv` gives each handler's call count for the whole run.
3. **Who reads an empty** (`xref.py`): from `objdump -d`, every function whose rip-relative operand names one of
   the exit-time empties. Static-init code (`_GLOBAL__sub_I*`, `__cxx_global_var_init*`) is excluded.
   **Calls** (`xhits.py`, second gdb run): an ignore-count breakpoint on each such reader, plus `Symbol::Init`
   as the positive control, which reads 1 in every target.
4. **Controls.**
   - (a) At `main`, every target holds its handler Symbols empty, and no `ok` row exists anywhere (`main: ok=0`).
     In the same binary, rb3-song shows 95 empty at `main` and 158 ok at exit, and fixed rb3-milo shows 147 empty
     at `main` and 140 ok at its first handler. So the read separates empty from interned in both directions.
   - (b) An address grep of `objdump` for three rb3-vocal2 empties (`fill_star_power`, `reset_scoring`,
     `get_saved_count`) finds exactly one reference each: the static-init store. That agrees with xref's
     "no reader".
   - (c) Every run reached its completion line under gdb (§4), so the call counts cover the whole run.

Known false positive of the word scan: a `const char *` initialised to a `""` literal can share `gNullStr`'s
address through string merging. miniaudio's `g_maBlacklisted*DeviceNamesALSA` in render read `empty` this
way. They are not Symbols.

## 3. Per target (baseline = main `637a73bda`, built in the worktree)

"Handler empties" = 8-byte objects holding `gNullStr`, minus gNullStr itself and the runtime-set
`gFile`, `DataArray::gFile`, `MsgSinks::sCurrentExportEvent`, `gSystemLanguage` and `gSystemLocale`.
`(2)` is the first-handler read; where no handler ran, the exit read stands in.

| target | empty @main (control) | handler empties @(2) / @exit | ok @(2) | first handler (vs Symbol::Init) | handler calls | readers of a handler empty (calls) | verdict |
|---|---|---|---|---|---|---|---|
| rb3-dta | 6 | 0 / 0 | 0 | none | 0 | none | no handler Symbols linked |
| rb3-song | 95 | 0 / 0 | 158 | none | 0 | none | all interned (m1) |
| rb3-midi | 6 | 0 / 0 | 2 | MidiParser::SyncProperty (after) | 29,858 | none | clean |
| rb3-gem | 6 | 0 / 0 | 8 | MidiParser::Handle (after) | 224 | none | clean |
| rb3-hit | 6 | 0 / 0 | 7 | none | 0 | none | clean |
| rb3-score | 6 | 0 / 0 | 7 | none | 0 | none | clean |
| rb3-score2 | 115 | 109 / 109 | 1 | none | 0 | none | empties are dead definitions (m6) |
| rb3-score3 | 115 | 109 / 109 | 9 | none | 0 | none | same |
| rb3-score4 | 115 | 109 / 109 | 10 | none | 0 | none | same |
| rb3-vocal | 115 | 109 / 109 | 8 | none | 0 | none | same |
| rb3-vocal2 | 164 | 158 / 158 | 11 | VocalPlayer::Handle (after) | 7,106 | none | same; the handlers that ran use local statics |
| rb3-harmony | 164 | 158 / 158 | 10 | VocalPlayer::Handle (after) | 3,780 | none | same |
| rb3-crowd | 115 | 109 / 109 | 9 | none | 0 | none | same |
| rb3-save | 12 | 6 / 6 | 0 | none | 0 | none | same |
| rb3-ark | 6 | 0 / 0 | 0 | none | 0 | none | clean |
| rb3-frame | — | — | — | — | — | — | links no Milo `Symbol` (no `gNullStr`, no `Symbol::Init`; only `tint::Symbol`) |
| rb3-milo | 147 | **141 / 141** | 0 | CharClip::SyncProperty (after) | 4 | **7 functions in 4 TUs + BandTrack::Reset, all 0 calls** | **latent defect → fixed** |
| rb3-render | 151 | 5 / 5 | 140 | CharClip::SyncProperty (after) | 4 | **BandCharacter::Handle (`hack_fix_clips_pre_merge`), 0 calls**; non-handler: below | **latent defect → fixed** |

Fixed binaries: rb3-milo has 1 handler empty and 140 ok at (2), with no reader. rb3-render's `BandCharacter::Handle`
reader is gone.

**Comparisons before `Symbol::Init`:** in every target, the first `Handle`/`SyncProperty` call came after
`Symbol::Init`. The readers listed above were never called. Nothing compared a message against a Symbol
before interning.

**Namespaces, class statics, aggregates:**
- The only mangled 8-byte empties in any target are `DataArray::gFile` and `MsgSinks::sCurrentExportEvent`,
  both set at runtime.
- `gNullStr` words inside larger objects belong to manager singletons, timers, `PlatformSymbol(Platform)::sym`,
  `sShaderTypes`, `g{Male,Female}HeadNormVariants` and similar. These are member or array slots that are empty by
  construction, not handler keys.
- No declared `extern Symbol` global read null (unconstructed) in any target.

**Not fixed, reported (not message comparisons, and 0 calls in the health run):**
- rb3-render: `GameMicManager::GetMic` / `GetMicCount` test `InMode(frame_rate)`, and `Modifier::IsHidden` tests
  `mData->Contains(hidden)`. Both Symbols are empty in render: `frame_rate` is never interned, and `hidden` is
  interned only by `RunW16TSPhase`. These are ordinary keyed reads, so they belong to whoever wires
  GameMicManager / ModifierMgr live.
- `gSystemLanguage` is empty in every target except rb3-app, and `SystemLanguage()` returns it (called 60–530
  times per run). Only App boot sets it. This is not a handler Symbol.

## 4. Runs, and what this lane did not do

- **Completion lines.** Every baseline run printed its native_health completion line under gdb and exited 0:
  - `Done.` (dta / gem / hit / score / score3 / score4 / vocal2 / harmony);
  - `RESULT: ALL GATES PASSED` (song / midi / ark / milo);
  - `RESULT: OK` (score2);
  - `all-off (+6)` (vocal);
  - `=== M12 complete` (crowd);
  - `=== ALL ROUND-TRIPS OK` (save).
- **rb3-render** returned `RESULT: NO GPU` (rc 2) in this session, both under gdb and bare, even outside the
  sandbox, so the read covers its boot only, up to the GPU gate. Its post-GPU phases (venue, bandtrack) are
  **not measured**. The `BandCharacter` fix covers them by construction.
- rb3-frame has no Symbol code, so the read does not apply to it.
- rb3-app was not touched: not its source lists, and not `rb3_app_link_stubs.*`.
- No native health selftest, no merge, no push.
- No X360 A/B: nothing under `src/` or `config/` changed.
