# Venue-unblock priority — the 14 factory-miss classes, and why they are not `band3`

**Date:** 2026-08-02
**Lane:** X4c-docs (documentation + prioritization only — no source edits, no native build)
**Measured against:** `main` @ `4c19740e`
**Predecessors:** [`x4a-venue-render-2026-08-02.md`](x4a-venue-render-2026-08-02.md) §3 (the 684/14 finding — **and the retracted attribution**) · [`x4b-animation-2026-08-02.md`](x4b-animation-2026-08-02.md) §4 (the retraction, with the compile + link measurement)
**Relationship to X4b:** X4b owns the compile/link survey and has landed it. This doc
owns the *ranking*, the *stub-vs-port* question, and the roadmap corrections. Every
compile/link number below is cited from X4b, not re-measured.

---

## 0. ⛔ Correction of record — "band3 is the critical path" is retracted

X4a §3/§9/§10.1 concluded that a venue root is blocked until `src/band3/` compiles.
**That is false.** X4b measured it directly (§4, landed `1ac037bb`), and this review
reached the same result independently from the class declarations:

| owning directory | n | classes |
|---|---|---|
| `src/system/bandobj/` | 5 | `BandCamShot`, `BandCharacter`, `BandLabel`, `BandWardrobe`, `BandConfiguration` (inline in `Band.cpp`) |
| `src/system/synth/` | 7 | `Sfx`, `SynthSample`, `MoggClip`, `SynthFader` (**C++ class `Fader`**), `ParallelGroupSeq`, `RandomGroupSeq`, `FxSendEQ` |
| `src/system/world/` | 1 | `WorldCrowd` |
| `src/system/ui/` | 1 | `UIColor` |
| **`src/band3/`** | **0** | — |

`src/band3/` is 260 TUs the venue root does not need.

### Why X4a got it wrong, and why the pattern will recur

X4a inferred **class ownership from the failure log**, not from where the classes are
defined. The log line is `MILO_NOTIFY("%s: Can't make %s", …)` — a filename and a
*stream* class name, and nothing else. Six of the fourteen names begin with `Band`,
the asset was a gameplay venue, and the whole list reads game-layer. Every one of
those signals is real; none of them is evidence of a source directory.

★ **The generalisable lesson: a Milo class name is a serialization token, not a source
address.** `OBJ_CLASSNAME` (`src/system/obj/ObjMacros.h:17-22`) decouples the two on
purpose, and this very list contains the proof both ways — `SynthFader` is C++ class
`Fader`, and `BandCamShot`/`BandCharacter`/`BandLabel`/`BandWardrobe` are engine
(`system/bandobj/`) classes despite the `Band` prefix. **One `grep -rn "class X"`
per name would have cost minutes and saved the wrong headline.** Any future
"unregistered class" triage should resolve names to declarations *before* costing the
work — and should expect `OBJ_CLASSNAME` mismatches, not assume them away.

Both documents stay in history. X4a's *measurement* (684 misses over 14 classes, the
archive-wide venue scan, the `LoadPersistentObjects` framing analysis) is sound and
still load-bearing; only its **attribution** is withdrawn.

---

## 1. Verdict up front

1. **All 14 classes have present, ported, scored source in-tree.** Not one is absent.
   All 13 real defining TUs are pinned in `config/45410914/objects.json` and appear as
   **scored units** in `build/45410914/report.json`, from **67.84%**
   (`default/BandLabel`) to **100.00%** (`default/system/synth/FxSendEQ`). §2.
   ⇒ [`bandobj-port.md`](bandobj-port.md)'s opening "`src/system/bandobj/` is absent
   from our tree" is **stale since 2026-05-26**; corrected in place with a dated note.

2. ★ **The stub-vs-port question resolves against stubs — and resolving it collapses
   the milestone.** A minimal registered body is *safe but nearly pointless* on the
   top-level `DirLoader` path (an unregistered class is already `ReadDead`-skipped
   there) and *unsafe* on the `WorldInstance::LoadPersistentObjects` path (no framing,
   so a short-reading stub desyncs exactly like a miss, just later). **The venue
   milestone therefore needs neither 14 real ports nor 2 ports + 12 stubs — it needs
   0 new ports**, because the faithful `Load()` bodies the persistent path demands are
   already written and scored. §4.

3. **The blocker is the build system plus one file.** X4b: 12 of 13 TUs compile clean;
   807 duplicate-definition link errors from a `cmake/ScatterIncludes.cmake` dedupe
   gap; ~4 root defects in `bandobj/BandCharacter.cpp`. §5.

4. ⭐ **One unmeasured question could shortcut 611 of the 684 misses without touching
   the build system at all**, and it costs one token to measure. §4.3.

5. **The `band3` port worklists are not the venue lever — and lose no value for it.**
   They are `src/band3/`-only by construction and were always framed as an **X360
   matching** worklist. That framing is intact and correct. §6.

---

## 2. Per-class status — all 14

Class decls read directly; pins from `config/45410914/objects.json`; match% from
`build/45410914/report.json`; native compile surface from `native/CMakeLists.txt:1037-1045`
(`MILO_FORK_SOURCES` = `rndobj` + `char` + `world` + `ui` globs only) and
`native/src/milo_object_factories.cpp` (129 registrations).

| # | Milo class name | C++ class @ decl | Base(s) | Defining TU | Pinned | Xenon unit (fuzzy / fns) | Native: compiled? | Native: registered? | X4a misses |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `BandCamShot` | `BandCamShot` @ `src/system/bandobj/BandCamShot.h:15` | `CamShot` | `bandobj/BandCamShot.cpp` (934 L) | ✅ NonMatching | `default/BandCamShot` **77.02%** 251/295 | ❌ | ❌ | **611** |
| 2 | `Sfx` | `Sfx` @ `src/system/synth/Sfx.h:83` | `Sequence` | `synth/Sfx.cpp` (323 L) | ✅ | `default/Sfx` **82.20%** 178/199 | ❌ | ❌ | 23 |
| 3 | `SynthSample` | `SynthSample` @ `src/system/synth/SynthSample.h:9` (⚠ also `SynthSample360` @ `synth_xbox/SynthSample.h:6`, **same `OBJ_CLASSNAME`**) | `Hmx::Object` | `synth/SynthSample.cpp` (181 L) | ✅ | `default/system/synth/SynthSample` **68.56%** 29/39 | ❌ | ❌ | 18 |
| 4 | `WorldCrowd` | `WorldCrowd` @ `src/system/world/Crowd.h:27` | `RndDrawable`, `RndPollable` | `world/Crowd.cpp` (1436 L) | ✅ | `default/Crowd` **86.26%** 194/222 | ✅ | ✅ **landed `1ac037bb`** | 6 → **0** |
| 5 | `MoggClip` | `MoggClip` @ `src/system/synth/MoggClip.h:13` | `Hmx::Object`, `SynthPollable` | `synth/MoggClip.cpp` (398 L) | ✅ | `default/system/synth/MoggClip` **95.34%** 52/55 | ❌ | ❌ | 6 |
| 6 | `SynthFader` | ⚠ **`Fader`** @ `src/system/synth/Faders.h:10` — `OBJ_CLASSNAME(SynthFader)` at `:20` | `Hmx::Object` | `synth/Faders.cpp` (342 L) | ✅ | `default/Faders` **74.87%** 52/67 | ❌ | ❌ | 5 |
| 7 | `BandCharacter` | `BandCharacter` @ `src/system/bandobj/BandCharacter.h:48` | `Character`, `BandCharDesc`, `MergeFilter`, `Rnd::CompressTextureCallback` | `bandobj/BandCharacter.cpp` (2863 L) | ✅ | `default/BandCharacter` **85.99%** 508/603 | ❌ **18 errs / ~4 defects** | ❌ | 4 |
| 8 | `UIColor` | `UIColor` @ `src/system/ui/UIColor.h:11` | `Hmx::Object` | `ui/UIColor.cpp` (42 L) | ✅ | `default/UIColor` **76.59%** 9/14 | ✅ | ✅ **landed `1ac037bb`** | 2 → **0** |
| 9 | `ParallelGroupSeq` | `ParallelGroupSeq` @ `src/system/synth/Sequence.h:197` | `GroupSeq` → `Sequence` → `Hmx::Object`,`SynthPollable` | `synth/Sequence.cpp` (856 L) | ✅ | `default/Sequence` **93.75%** 228/239 | ❌ | ❌ | 2 |
| 10 | `BandLabel` | `BandLabel` @ `src/system/bandobj/BandLabel.h:6` | `UILabel`, `UITransitionHandler` | `bandobj/BandLabel.cpp` (147 L) | ✅ | `default/BandLabel` **67.84%** 54/67 | ❌ | ❌ | 2 |
| 11 | `RandomGroupSeq` | `RandomGroupSeq` @ `src/system/synth/Sequence.h:109` | `GroupSeq` | `synth/Sequence.cpp` (same TU as #9) | ✅ | `default/Sequence` **93.75%** | ❌ | ❌ | 1 |
| 12 | `FxSendEQ` | `FxSendEQ` @ `src/system/synth/FxSendEQ.h:6` | `FxSend` | `synth/FxSendEQ.cpp` (91 L) | ✅ | `default/system/synth/FxSendEQ` **100.00%** 21/21 | ❌ | ❌ | 1 |
| 13 | `BandWardrobe` | `BandWardrobe` @ `src/system/bandobj/BandWardrobe.h:13` | `virtual Hmx::Object` | `bandobj/BandWardrobe.cpp` (1401 L) | ✅ | `default/BandWardrobe` **91.92%** 239/270 | ❌ | ❌ | 1 |
| 14 | `BandConfiguration` | ⚠ **no header** — factory-only shim @ `src/system/bandobj/Band.cpp:66-73` | `Hmx::Object` | `bandobj/Band.cpp` (180 L) | ❌ **not pinned** | — (no scored unit; `default/band3/game/Band` is a *different* `Band.cpp`) | ❌ | ❌ | 1 |

**Totals: 684 misses → 676 outstanding.** `BandCamShot` alone is **611 (90.4% of what
remains)**; everything else combined is 65.

### Two name-resolution traps, both settled

- **`SynthFader`** is the *stream* class name of C++ class **`Fader`**
  (`src/system/synth/Faders.h:10`), via `OBJ_CLASSNAME(SynthFader)` at `:20`.
  `REGISTER_OBJ_FACTORY` keys the registry on `StaticClassName()`, not the C++
  identifier, so any registration must be written `REGISTER_OBJ_FACTORY(Fader)`.
- **`BandConfiguration`** is a **factory-only shim declared inside
  `src/system/bandobj/Band.cpp:66-73`** — deliberately, for asm fidelity. The comment
  at `:58-65` explains: retail's `BandConfiguration::Init()` was a header-inline
  `{ Register(); }` that `/Ob2` inlined into `BandInit()`, so an external-call stub
  desyncs `BandInit`'s instruction sequence. It is explicitly "not the full ~0x29c-byte
  class with its real members/virtuals." **`BandSong` at `:78-84` is the same pattern.**
  It is the only one of the 14 with no real class anywhere in the tree.

### One stale comment left behind

`native/src/milo_object_factories.cpp:33-37` still says the list omits `WorldCrowd`
because its TU is on `_MILO_FORK_EXCLUDE`. `native/CMakeLists.txt:1140-1141` shows
that list is now **`BeatClock` alone**, and `WorldCrowd` is registered at `:326`.
Harmless but actively misleading to the next reader; noted as owed work (this lane
edits no `native/` source).

---

## 3. How factory registration works (and why the port must do it by hand)

- **Registry:** `static std::map<Symbol, ObjectFunc *> Hmx::Object::sFactories` —
  declared `src/system/obj/Object.h:1788`, defined `src/system/obj/Object.cpp:79`.
- **Macros** (`src/system/obj/ObjMacros.h:714-721`; duplicated at `Object.h:1674-1678`):
  ```
  NEW_OBJ(T)              -> static Hmx::Object *NewObject() { return new T; }
  REGISTER_OBJ_FACTORY(T) -> Hmx::Object::RegisterFactory(T::StaticClassName(), T::NewObject);
  ```
  `RegisterFactory` (`Object.cpp:922-924`) is literally `sFactories[name] = func;`.
- **Trigger: explicit, never automatic.** There is **no** static-initializer
  self-registration anywhere in the tree. Retail's executors are per-subsystem
  aggregators: `ObjectDir::Init()` (`obj/Dir.cpp:1082`), `Rnd::PreInit()`
  (`rndobj/Rnd.cpp:283`), `CharInit()` (`char/Char.cpp:71`), `WorldInit()`
  (`world/World.cpp:20-38`), `Synth::Init()` (`synth/Synth.cpp:150-188`), the UI init
  at `ui/UI.cpp:962-978`, `BandInit()` (`bandobj/Band.cpp:102-168`).
- ⚠ **In this tree those aggregators have no callers.** `BandInit()` and `WorldInit()`
  are referenced only by their own declarations (`bandobj/Band.h:3`, `world/World.h:3`).
  `os/System.cpp`'s `SystemInit`/`SystemPreInit` call `ObjectDir::Init()` and nothing
  else. They exist to be *matched*, not run.
- **So the native port hand-rolls the list** — `RegisterMiloObjectFactories()` in
  `native/src/milo_object_factories.cpp`, called from `main_milo.cpp` and
  `main_render.cpp`. The rationale in its header comment is sound (the real `Init()`s
  drag SystemConfig / fonts / GPU / audio devices), but ★ **a hand-rolled mirror of an
  aggregator drifts**: `WorldCrowd` is registered by `WorldInit()` at
  `world/World.cpp:24` and was simply never copied across, while its sibling
  `WorldCrowd3DCharHandle` from the same header was. That is a *class* of bug; §7
  proposes a cheap standing guard.

⇒ **Registration is per-class code the port must call, every time, for every class.**
There is no link-time magic to wait for. This is the single highest-leverage fact here.

---

## 4. ★ Stream-sync stub vs. real port — the decisive question, answered

Two consumption paths with **opposite** framing guarantees.

### 4.1 Path A — top-level objects: `DirLoader::CreateObjects` / `LoadObjs`

`src/system/obj/DirLoader.cpp:915-…` reads (className, name); on `!RegisteredFactory`
it emits `MILO_NOTIFY("%s: Can't make %s")` at `:929` and takes the skip path.
Crucially **`ReadDead(*mStream)` runs after *every* object, created or not**
(`:810-816`: created branch under `mRev > 1`, missing branch unconditionally).
`ReadDead` (`:338-357`) scans forward byte-by-byte for `AD DE AD DE`.

⇒ A miss here is **already recoverable**. A stub that *under-reads* is also recoverable
(ReadDead scans forward to the marker). A stub that *over-reads past the marker* is
**worse than a miss**. So on path A a stub is **safe-if-minimal but buys almost
nothing** — only that a named object exists in the dir so sibling references resolve,
instead of a hole.

### 4.2 Path B — persistent objects: `WorldInstance::LoadPersistentObjects`

`src/system/world/Instance.cpp:210-265`. Phase 1 reads a `count`-long list of
(className, name) and constructs each; on `!RegisteredFactory` →
`MILO_NOTIFY(…); DeleteObjects(); return;` (`:231-235`). Phase 2 is:

```
while (!objlist.empty()) {
    Hmx::Object *cur = objlist.front();
    cur->PreLoad(bs.stream);
    cur->PostLoad(bs.stream);   // Instance.cpp:255-256
    objlist.pop_front();
}
```

**No `ReadDead`. No marker. No framing of any kind.** X4a §3's reading is correct and
is confirmed here at the line.

⇒ On path B a stub is **not a fix**. A factory-only shim inherits
`Hmx::Object::PreLoad`/`PostLoad`, reads only the base's bytes, and leaves the derived
payload in the stream — desyncing **exactly like a miss, just several hundred objects
later and with a garbage `String chars` at the far end** (X4a's observed failure). The
only thing that works on path B is a `PreLoad`/`PostLoad` pair that consumes the retail
byte count.

### 4.3 ⭐ The unmeasured question that could shortcut 611 misses

Which of the 14 land on path A vs path B is the fulcrum, and it is **not determinable
read-only**: the two `MILO_NOTIFY` sites use **identical format strings** —
`DirLoader.cpp:929` prints `mFile.c_str()`, `Instance.cpp:232` prints
`mStoredFile.c_str()` — so X4a's captured log cannot be re-attributed after the fact.

It matters because **on path A you can bind a class name to a base-class allocator
without compiling anything**:

```
Hmx::Object::RegisterFactory(Symbol("BandCamShot"), CamShot::NewObject);
```

written long-hand (since `REGISTER_OBJ_FACTORY` keys off `StaticClassName()`). `CamShot`
is **already registered natively**, so this is one line and zero TUs. It yields a real
`CamShot` that under-reads by the `BandCamShot` delta: **safe on path A, unsafe on
path B.**

> **If `BandCamShot` is a path-A class, that one line retires 611 of the 676
> outstanding misses without the `ScatterIncludes` dedupe, without compiling
> `bandobj/`, and without a single port.** If it is path B, it does nothing and the
> build-system lane is unavoidable. Nobody has measured which.

**Instrument, one token:** change `Instance.cpp:232`'s literal to
`"%s: Can't make %s (persistent)"` under `#ifdef HX_NATIVE`. That partitions all 676
misses in a single run. **This is the cheapest high-value measurement left in the venue
milestone**, and it should precede the build-system lane rather than follow it.

*Inference, marked as inference:* `LoadPersistentObjects` `return`s after its **first**
miss, so 611 `BandCamShot` misses cannot all come from one proxy's persistent list —
they are either path-A misses within files, or path-B first-misses across ~611 proxies.
X4a's log shows `small_club_01.milo_xbox: Can't make BandCamShot` against
`small_club_01_base.milo: Can't make BandConfiguration / WorldCrowd`, which is
*consistent with* BandCamShot on path A, but the filename shape is not proof.

### 4.4 Verdict

> **A stream-sync stub is a real technique, it is legal here, there is an in-tree
> precedent and a native tripwire for it — and it is the wrong tool for this
> milestone.** Of the 14, **zero need a stub**, because all 14 already have a faithful
> ported `Load()` — that is what a 67–100% scored unit means. The cost of the venue
> milestone is **13 TUs onto a link line + 14 registration lines**, gated on a
> build-system fix and four defects in one file. **Not 14 ports. Not 12 stubs.**
>
> ⚠ Standing caveat, restated because it bounds everything above: a stub is a
> native-side (`HX_NATIVE`) affordance and **never a claimed match**. The
> `Band.cpp` shims are the exception that proves it — they exist to *reproduce* retail
> codegen, not to substitute for it.

**Direct evidence from X4b that supports this verdict:** `WorldCrowd` and `UIColor`
were retired by **two registration lines and zero source changes** (X4b §4.3). Both had
faithful `Load()` bodies sitting compiled in the binary the whole time. That is the
stub-vs-port question answered empirically for 2 of the 14 — the answer was *neither*:
the port was already done, only the registration was missing.

**Supporting detail for where the technique *is* right later:**
- **Precedent:** `Band.cpp:66-73` (`BandConfiguration`), `:78-84` (`BandSong`), and the
  row of `class X { public: static void Init(); };` declarations at `:85-90` kept purely
  so `BandInit`'s relocation layout matches retail.
- **Tripwires already exist:** `DirLoader.cpp:948` logs `"STUB vtable for class %s"`
  when `vptr[0]` is null after construction (and `:748` the dir-side equivalent);
  `Object.cpp:911-913` adds an `HX_NATIVE` null return so `NewObject` degrades instead
  of asserting. A link-stub vtable is *reported*, not crashed on.
- **The `*Dir`-subclass rule stands:** `milo_object_factories.cpp:20-31` — an
  unregistered `*Dir` serializes a nested directory whose inner objects carry their own
  dead markers, so `ReadDead` stops at the first inner one. **None of the 14 is a `*Dir`
  subclass**, which is why path A is survivable for them at all.

---

## 5. Dependency closure — what stands between "source present" and "registerable"

Three gates; only the first is shared.

**Gate 1 — the scatter-include dedupe (blocks all 5 bandobj TUs, opens the door for
`synth/`).** X4b §4.2: wiring the 10 clean TUs into `MILO_FORK_SOURCES` produces **807
duplicate-definition link errors** from three emitters — `rndobj/EventTrigger.cpp` 313,
`rndobj/Font.cpp` 244, `char/CharIKScale.cpp` 162. Not code defects:
`cmake/ScatterIncludes.cmake` drops an `#include "*.cpp"` includee only when the includee
is itself a target source, and here two *different* target sources scatter-include the
same *non-source* file (`BandCamShot` → `math/Geo.cpp` ← `CharIKScale`; `BandCamShot` →
`flow/*.cpp` ← `EventTrigger`; `BandLabel` → `char/CharClip.cpp` +
`bandobj/BandDirector.cpp` ← `Font`). It is a module all 18 native targets share, so it
needs its own lane and a per-target A/B — the same reason X2 hand-listed
`_MILO_SCATTER_TRANSITIVE_PRUNE` instead of making the module transitive.

**Gate 2 — `synth/` is deliberately closed.** `MILO_FORK_SOURCES`
(`native/CMakeLists.txt:1045`) is `rndobj + char + world + ui`. `synth/` enters via
exactly two hand-picked leaves, `synth/Pollable.cpp` and `synth/FxSend.cpp`, whose
in-file comment is explicit that this "does NOT open synth/ generally (no tomcrypt, no
audio device)". 7 of the 14 sit behind that door. Two mitigations are visible read-only:
- **`FxSendEQ` derives from `FxSend`, whose TU is already linked** — so
  `synth/FxSendEQ.cpp` (91 lines) is a one-TU add onto a present base.
- **`SynthSample` has a 360 sibling** (`synth_xbox/SynthSample.h:6`,
  `SynthSample360 : SynthSample`) carrying the *same* `OBJ_CLASSNAME(SynthSample)`.
  Retail-360 registers the subclass; **the native port should register the base**, since
  the subclass drags XMA/XAudio2. `synth_xbox/` is not globbed, so that is the default
  outcome — but it should be a stated decision, not an accident. Whether the base's
  `Load()` covers the 360 assets' XMA payload is a runtime question (§8.4).

**Gate 3 — `BandCharacter`'s own defects (blocks 1 class, and the animation milestone).**
X4b §4.1: 18 errors from ~4 root defects, all stale rb3-Wii-lineage code inside its own
`HX_NATIVE` arms — protected-member access on `RndDir::mDraws` / `Character::mLods` /
`ObjectDir::mStoredFile`; `RndMesh::mNativeBonesRebound` (absent from xenon's `RndMesh`);
`Refs()` used as `std::vector<ObjRef*>` at `:2255-2261`; `MergeFilter::Action` vs
`::SubdirAction` confused at `:2437`/`:2442`.

★ The `Refs()`-as-container defect is the **same family** X4a fixed in
`WorldInstance::DeleteTransientObjects` (X4a §2.2 — `auto refs = obj->Refs();` copying
the ring head and spinning forever). **X4b §9 already swept for it** and found the
`bandobj/BandCharacter.cpp:2255` instance plus `obj/ObjMacros.h:730-736`
(`FOREACH_OBJREF` / `FOREACH_OBJREF_POST` still expand to
`std::vector<ObjRef *>::const_reverse_iterator … Refs().rbegin()`). Confirmed present on
`main` at those exact lines. **No new instances found by this review** — recorded so the
next agent does not re-sweep. X4b's own note stands: the `BandCharacter` one becomes a
*live hang* the moment that TU is wired in, so it must be fixed as part of Gate 1's work,
not after it.

**No other gate found.** `BandCamShot`'s base `CamShot` is registered natively;
`BandLabel`'s bases `UILabel`/`UITransitionHandler` are in the compiled `ui/` glob;
`WorldCrowd` and `UIColor` are done. `BandWardrobe` inherits `virtual Hmx::Object` —
flagged as a *possible* vtable/layout wrinkle under clang LP64,
**[UNVERIFIED read-only]**; X4b's `-fsyntax-only` pass reported it clean, but virtual
inheritance is a link/layout risk that `-fsyntax-only` does not exercise.

---

## 6. The two rankings — disjoint, not conflicting

[`band3-port-worklist.md`](band3-port-worklist.md) (232 fns / 93 TUs) and
[`band3-port-worklist-loose.md`](band3-port-worklist-loose.md) (301 fns / 105 TUs) rank
by *matching yield × Wii→Xenon BSim identity confidence*. Verified mechanically:
**every `src/…` path in both files is under `src/band3/`** — zero hits outside it. **None
of the 14 venue-blocking classes appears in either, and cannot, by construction.**

So there is no ranking to re-weight; there are two non-overlapping worklists, and the
venue milestone consumes neither.

**Their value is undiminished and unchanged.** They were never claimed as a native
lever — their stated purpose is *"pick which band3 TU to port next, and name each
function from the Wii body"* for the **X360 match**, where band3 is the irreplaceable
core DC3 cannot supply. That case rests on the BSim identity pipeline and the ~0.90
measured precision, neither of which this review touches. **Nothing here should be read
as demoting them; they simply answer a different question.**

Where the *methods* genuinely diverge is worth stating, because a cold agent reading
only one will pick the wrong TU either way:
- The matching ranking scores `BandCamShot` invisible — it is not `src/band3/` (out of
  scope by construction) and its unit is already 251/295 fns matched, so the remaining
  match prize is small. Yet it is **90% of the native blocker**.
- Conversely `MusicLibrary.cpp` (matching rank 1, 7 identities) contributes **nothing**
  to the venue.

Both worklists now cross-link this doc, and this doc links back.

---

## 7. Recommended priority order

Ordered by *misses retired ÷ cost*, with the honest exceptions called out.

| # | Item | Misses | Cost | Why here |
|---|---|---|---|---|
| ~~0~~ | ~~`WorldCrowd` + `UIColor` registration~~ | ~~8~~ | — | ✅ **DONE** — X4b `1ac037bb`. Two lines; both TUs were already in the binary. |
| **1** | ⭐ **Instrument the two "Can't make" sites** (`Instance.cpp:232` vs `DirLoader.cpp:929`) | 0 | one token, `HX_NATIVE` | Partitions all 676 into path A / path B. Decides whether #2 is a one-liner or a build-system lane. **Do this first — it can invalidate #3's urgency.** §4.3 |
| **2** | ⭐ **`BandCamShot`** | **611 (90%)** | **one line if path A**, else 1 TU behind Gate 1 | Dominant by an order of magnitude. Base `CamShot` already registered. |
| **3** | **`ScatterIncludes.cmake` dedupe lane** (Gate 1) | enables 618 | own lane, per-target A/B across 18 targets | Blocks all 5 bandobj TUs. A build-system task the venue waits on, **not** a venue task. Already X4b §11 owed work. |
| **4** | **`FxSendEQ`** | 1 | **1 TU, 91 lines, base already linked** | Out of order deliberately: the cheapest item in the table and the correct canary for whether Gate 2 opens at all. Its xenon unit is already **100%**. |
| **5** | **`synth/` cluster: `Sfx`, `SynthSample`, `MoggClip`, `Fader`, `Parallel`/`RandomGroupSeq`** | 55 | 5 TUs + opening Gate 2 | Second-largest block; `Sequence.cpp` covers two classes. The synth-stays-closed decision was deliberate and should be *revisited*, not bypassed. |
| **6** | **`BandConfiguration`** | 1 | 1 native-side shim | Already a factory-only shim at `Band.cpp:66-73`, but `Band.cpp` scatter-includes ~40 bandobj headers. Replicating the 8-line shim in `native/src/` under `HX_NATIVE` avoids the include storm. ⚠ **Only valid if path A** — the shim under-reads. |
| **7** | **`BandWardrobe`** | 1 | 1 TU, falls out of #3 | 91.92% unit; near-free once Gate 1 lands. Watch the virtual base (§5). |
| **8** | **`BandLabel`** | 2 | 1 TU, **heaviest scatter drag** | ⚠ *Low miss, high cost.* 2 misses, and it is 244 of the 807 duplicate-symbol errors. Do it **after** #3 lands; never cite it as motivation **for** #3. |
| **9** | **`BandCharacter`** | 4 | ~4 source defects, 4 base classes | ⚠ *Low miss, hard blocker.* 4 venue misses — but it gates the **animation** milestone (X4a §10.1) and carries the latent `Refs()` hang. Schedule it there, not here. |

**Honest calls this ordering makes, stated plainly:**
- **`BandCamShot` is the milestone.** 611 of 676. Everything else is rounding.
- **`BandLabel` is the trap** — worst cost/benefit in the table by a wide margin.
- **`BandCharacter` is mis-scheduled by miss count** — 4 misses ranks it last; its true
  position is *first item of the next milestone*.
- **`FxSendEQ` is promoted above bigger items on purpose** — a 91-line TU on an
  already-linked base is the right way to test Gate 2 before committing to #5.
- **#1 before #3.** X4b's corrected cost ("one build-system change + four defects")
  assumes every miss needs a real registration. If `BandCamShot` is path A, that
  assumption over-costs the venue by an entire build-system lane. Measuring is a token.

---

## 8. Dual yield — targeted at the pinned bandobj/synth TUs

Doc [`20-native-port-and-engine-reuse.md`](paths-to-100/20-native-port-and-engine-reuse.md)
states the synergy as: *porting a band3 TU for matching makes it compile natively, and
wiring it natively surfaces undefined symbols that become match-worklist items.* For
these 14, **the first half does not apply and the second half is understated.**

**First half — spent.** All 13 real TUs are pinned **and scored**, 67.84%–100.00%.
Nothing here moves `matched_functions` by being compiled natively. Said plainly:
**for these 14, native-unblock work has an expected strict-match yield of ~0.** Doc `20`
should not be read as promising a match dividend from the venue milestone.

**Second half — understated, and this is the real dual yield.** What the native link
surfaces is not undefined symbols but **semantic defects inside already-scored code**,
in pinned units with real headroom:

| Defect | Found by | Pinned unit | Headroom |
|---|---|---|---|
| `WorldInstance::DeleteTransientObjects` walks a ring **copy** → infinite loop | X4a §2.2 (11m39s hang) | `default/Instance` | 85% fn-matched |
| `bandobj/BandCharacter.cpp` — ~4 stale-lineage defects incl. `Refs()`-as-vector | X4b §4.1 | `default/BandCharacter` | **85.99%**, 508/603 |
| `FOREACH_OBJREF` / `_POST` expand to `std::vector<ObjRef*>` (`ObjMacros.h:730-736`) | X4b §9 | header — blast radius unmeasured | — |
| `TrigTableInit()` never ran → `sin == cos == 0` in every native target | X4b §3 | — (native init gap) | — |
| `WorldInstance::PreLoad`/`PostLoad` rev ordering disagrees with rb3-Wii | X4a §7 (runtime-neutral) | `PostLoad` **59.07%**, `PreLoad` **76.92%** | large |

Every code row is a transcription defect in a *scored* unit that the X360 match build
**structurally cannot see** — it compiles TUs and diffs objects; it never links and never
runs. Four were found in two weeks of native bring-up. That is a better argument for the
native track than the one doc `20` makes, and it is now recorded there.

**Oracle maturity — re-verified for all 14 against `../rb3` (Wii/MWCC):**

| Class | rb3-Wii unit | fuzzy | fns |
|---|---|---|---|
| `BandCamShot` | `main/system/bandobj/BandCamShot` | 99.999% | 91/95 |
| `BandCharacter` | `main/system/bandobj/BandCharacter` | 99.670% | 275/290 |
| `BandWardrobe` | `main/system/bandobj/BandWardrobe` | 99.503% | 135/148 |
| `BandLabel` | `main/system/bandobj/BandLabel` | 99.998% | 35/36 |
| `BandConfiguration` (`Band.cpp`) | `main/system/bandobj/Band` | **100%** | 27/27 |
| `Sfx` | `main/system/synth/Sfx` | 99.897% | 52/55 |
| `SynthSample` | `main/system/synth/SynthSample` | **100%** | 24/24 |
| `MoggClip` | `main/system/synth/MoggClip` | **100%** | 35/35 |
| `SynthFader` (`Faders`) | `main/system/synth/Faders` | **100%** | 46/46 |
| `FxSendEQ` | `main/system/synth/FxSendEQ` | **100%** | 9/9 |
| `Parallel`/`RandomGroupSeq` | `main/system/synth/Sequence` | 99.674% | 141/144 |
| `WorldCrowd` | `main/system/world/Crowd` | 99.122% | 100/116 |
| `UIColor` | `main/system/ui/UIColor` | **100%** | 10/10 |

⇒ For every defect X4c/X4b finds in these TUs, `bin/analyze-function <symbol>` in
`../rb3` gives a Bank-8-accurate body to diff against. **Not one of the 14 is an open
decomp question.** With §2, that is why this is a wiring problem: the source exists, and
the repair oracle is ≥99.1% everywhere.

**Standing guard for the hand-rolled factory list (§3).** `WorldCrowd` drifted out
because nothing compares the native list to the aggregators it replaces. A ~20-line
check — grep `REGISTER_OBJ_FACTORY(X)` out of the aggregator bodies in
`src/system/{world,ui,char,rndobj,synth}` + `bandobj/Band.cpp`, subtract the native list,
subtract classes whose TU is not on the link line, print the remainder — turns silent
drift into a build-time diff. **Filed as owed work; not built here** (this lane writes no
code).

---

## 9. What I could not establish read-only

Gaps, not glossed.

1. ⭐ **Path A vs path B per class** (§4.3). The fulcrum. The two `MILO_NOTIFY` format
   strings are identical, so X4a's log cannot be re-attributed. My path-A guess for
   `BandCamShot` is **inference from filename shape only**.
2. **Whether the 10 clean TUs link after the scatter dedupe.** X4b measured 807 errors
   *before* a dedupe; nobody has measured *after*. The count could mask a second,
   unrelated failure underneath.
3. **`BandWardrobe`'s `virtual Hmx::Object` base under clang LP64.** `-fsyntax-only`
   passed it, but virtual inheritance is a layout/vtable risk that syntax checking does
   not exercise.
4. **Whether registering base `SynthSample` (not `SynthSample360`) loads shipped 360
   sample data correctly.** The class-name string is shared; whether the base `Load()`
   covers the XMA payload is a runtime question.
5. **Blast radius of `FOREACH_OBJREF`** (`ObjMacros.h:730-736`). X4b flagged it as
   "dead or unexpanded today"; neither of us enumerated its users or checked whether any
   is on a native link line.
6. **Miss counts are one asset's numbers.** All 684/611 figures come from X4a's single
   run on `world/venue/small_club/small_club_01`. No other venue root was instrumented
   per-class, so "90% is `BandCamShot`" is not an archive-wide claim.
7. **`bandobj/Band.cpp` has no pin** in `config/45410914/objects.json` and no scored
   unit, despite being present and carrying `BandInit`. Whether that is deliberate (its
   `.text` range unclaimed) or an oversight is a splits question I did not chase.

---

## References

- [`x4b-animation-2026-08-02.md`](x4b-animation-2026-08-02.md) — §4 the retraction + compile/link measurement, §9 the `Refs()` sweep, §11 owed work
- [`x4a-venue-render-2026-08-02.md`](x4a-venue-render-2026-08-02.md) — §3 the 684/14 measurement (sound) and its attribution (retracted), §9 owed work
- [`x3-first-render-2026-08-01.md`](x3-first-render-2026-08-01.md) — §9.4 ranked bandobj/band3 as item 4
- [`bandobj-port.md`](bandobj-port.md) — the 2026-05-26 port plan (see its dated correction note)
- [`band3-port-worklist.md`](band3-port-worklist.md) · [`band3-port-worklist-loose.md`](band3-port-worklist-loose.md) — the **X360 matching** ranking, `src/band3/` only
- [`paths-to-100/20-native-port-and-engine-reuse.md`](paths-to-100/20-native-port-and-engine-reuse.md) · [`engine-reuse-and-asset-rendering.md`](engine-reuse-and-asset-rendering.md)
- Code anchors: `src/system/obj/ObjMacros.h:17-22,714-721,730-736`,
  `src/system/obj/Object.h:1788`, `src/system/obj/Object.cpp:79,907-924`,
  `src/system/obj/DirLoader.cpp:338,748,810-816,929,948`,
  `src/system/world/Instance.cpp:210-265`, `src/system/world/World.cpp:20-38`,
  `src/system/synth/Synth.cpp:150-188`, `src/system/ui/UI.cpp:962-978`,
  `src/system/bandobj/Band.cpp:58-90,102-168`,
  `native/CMakeLists.txt:1037-1045,1140-1141`, `native/src/milo_object_factories.cpp`
