# Per-symbol anon-namespace hashes: +23 / −1, and dc3's evidence does not transfer

2026-08-12, lane T. Ports dc3's rewrite of `scripts/obj_anon_ns_patcher.py`
(dc3 `main`, `docs/analysis/anon-namespace-hash-lane-20260812.md`) to
rb3-xenon. Companion: `laneT-mempoptemp-and-anon-ns-2026-08-12.md` (the other
item in this lane, unrelated).

## The change

The old rule decided **one hash per object**: patch only when both sides carry
exactly one hash, otherwise guess which of retail's hashes is "the file's own"
by dropping any hash seen in more than five retail objects. That is a frequency
argument, not evidence, and it left the entire multi-hash population unpatched.

The new rule decides **per symbol**. Every hash occurrence sits inside a
NUL-delimited mangled name; blank the hashes out of that name and ask retail's
paired object what belongs in those positions, positionally. Token-level
matching is a fallback only.

This is legitimate because of what the class is: an MSVC anonymous-namespace
hash encodes the **build machine's computer name and the canonical source
path**, so source cannot express it and retail's value is a fact about
Harmonix's build host. Aligning it reproduces a build-environment input. The
compile-time route is **blocked** for the hashes that matter — under wibo an
anonymous namespace declared in a *header* is never hashed at all, the call is
not made — so the post-build rewrite is the available route, not merely the
cheap one.

### Not a verbatim copy of dc3's

| | dc3 | rb3-xenon |
|---|---|---|
| target↔base pairing | relpath (splits headings carry paths) | **`load_config_pairs` / objdiff.json** (headings are bare basenames) |
| mtime-preserving write | `scripts/obj_patch_io.py` | module-local `_write_preserving_mtime` (unchanged) |
| global index scope | every retail object | only targets some compiled object is paired with |

Pairing is the load-bearing difference. Of the **126** of our objects carrying
an anon-ns hash, relpath keying reaches **17**; objdiff.json reaches **98**
more. Running dc3's script here unmodified skips 109 objects — the whole lane.
Every index lookup in the port therefore uses the **resolved target relpath**,
not the compiled one, which is the single substitution dc3's code needs.

The global template/token union is restricted to live pairings because
`build/45410914/obj/` accumulates orphan target objects from removed splits
headings (118 of 1069 non-auto targets). Orphans cannot produce a *wrong*
answer — an extra hash tuple makes the key ambiguous and `_lookup` abstains —
but they silently demote a `template_global` to a `majority`.

## dc3's two evidence claims were re-derived here. Neither holds.

**"543 distinct name templates, ZERO mapping to two different hash tuples."**
On rb3-xenon it is **161 templates, 4 ambiguous** (and 119 tokens, 15
ambiguous). Two of the four are ambiguous only across objects and the
paired-object rule settles them. The other two are ambiguous **inside the
paired retail object**:

| template | hashes | ambiguous inside |
|---|---|---|
| `??$_Copy_Construct@UDebugGraph@?A0x*@@…` | `b39b74bf` / `fa5cc2c6` | `Sfx.obj` |
| `?Dispatch@SyncLocalMachineMsg@?A0x*@@UAAXXZ` | `6c4eb79b` / `951deeb9` | `BandMachineMgr.obj` |
| `??$__destroy_range_aux@…reverse_iterator@PAULabel@?A0x*@@…` | `81ddebd1` / `9335ac2a` | — (across objects) |
| `?NewNetMessage@MainHubAdvanceMsg@?A0x*@@SAPAVNetMessage@@XZ` | `447fe1d1` / `fb94c5e0` | — (across objects) |

Retail's own `Sfx.obj` really does contain two instantiations of
`_Copy_Construct<DebugGraph>` under two different anonymous namespaces. No
name-based rule can settle that.

**"A fallback provably cannot manufacture a match."** dc3's argument is a
construction proof — if a fallback-assigned name coincided with a retail name,
the template lookup would have found it first — and it *depends on zero
ambiguity*. Re-derived here: **151** of our anonymous-namespace names become
byte-identical to a name in the paired retail object, **150 from `template`
and exactly 1 from `majority`**. The one is the locally-ambiguous `Sfx.obj`
`_Copy_Construct<DebugGraph>`: the evidence rules abstain, and `majority`
(142 of 146 occurrences are `b39b74bf`) lands on one of the two spellings
retail uses. A coin flip that came up heads. It is recorded as such in the
script's docstring and should not be quoted as evidence.

Rule firing counts on this tree: `majority` 824, `template` 186, `token` 161,
`template_stripped` 137, `template_global` 63, `token_global` 42. Nothing
unresolved. 20 objects patched, 1,028 replacements.

## Measurement — pinned `objdiff-cli-B`, both rulers, settled build

Build re-run twice; the second run does no patcher work (fixed point confirmed).

| ruler | before | after | Δ | complete fns |
|---|---|---|---|---|
| `none` | 42.221160% (4,357,516 B) | 42.221160% (4,357,516 B) | **0** | +0 / −0 |
| `name_check` | 32.463444% (3,350,452 B) | 32.484333% (3,352,608 B) | +2,156 B | **+23 / −1** |

`none` is exactly inert, which is what a naming-only change must do.

Gained, by unit: `UIListDir` 4, `Sfx` 4, `CheatProvider` 4, `WaitingUserGate`
3, `Joypad_Xbox` 3, `ChunkStream` 2, `BandMachineMgr` 1,
`SessionUsersProviders` 1 — including `?Init@BandMachineMgr@@SAXXZ` (116 B),
`?DecompressChunkAsync@ChunkStream@@AAAXXZ` (304 B),
`?Init@WaitingUserGate@@SAXXZ` (116 B) and `?XinputJoypadThreadStart@@YAXXZ`
(104 B), i.e. real functions and not only STL instantiations.

### LOST, named: 1 function, and it is not a scorer false positive

    default/WaveFile
    ??$__adjust_heap@PAUCuePoint@?A0x81ddebd1@@HU12@P6A_NABU12@0@Z@stlpmtx_std@@…
    204 B,  name_check 100.0 -> 99.90196   (none: 100.0 both sides, unmoved)

Root-caused. The bodies are byte-identical (`raw_eq` true) and exactly one
relocation disagrees, at offset 192:

| | callee named |
|---|---|
| retail | `??$__push_heap@PAUGroupDrawDist@@…` |
| ours | `??$__push_heap@PAUCuePoint@?A0x81ddebd1@@…` |

Retail ICF-folded `__push_heap<CuePoint>` onto `__push_heap<GroupDrawDist>`,
and `scripts/symbol_aliases.json` already carries that group at `0x82451FF8`
— but it spells our side with our OLD hash, `?A0xe9afadec`. The patcher renamed
our symbol to `?A0x81ddebd1`, so the alias no longer names anything we emit.

**The repair is one string in `scripts/symbol_aliases.json`**:
`??$__push_heap@PAUCuePoint@?A0xe9afadec@@…` → `…?A0x81ddebd1@@…`. Not made
here: that file is owned by another lane this session, and this lane wrote
nothing to it.

### The general form: an alias file keyed on hashes we just changed

Scanning `symbol_aliases.json` against the built tree: **332** group-member
names spell an anonymous-namespace symbol that exists in **neither** tree, and
**195** of them are revivable by substituting the current hash under the same
name template (`?A0xa0acbc4e` → `?A0x5ad8719f`, `?A0x64cce081` → `?A0xe564e51a`,
`?A0x92bae52f` → `?A0x5fd33732`, …).

⚠ **This is a snapshot of the CURRENT tree and does not establish how many were
dangling beforehand.** Many are plainly retail-side spellings repeated across
dozens of `Name` groups and never named anything of ours. The only *measured*
regression from this change is the single `WaveFile` row above. The scan is
handed over as hygiene work for the alias owner, not quoted as damage.

The durable lesson is structural: **`symbol_aliases.json` stores our
anon-namespace symbols by a hash that a post-build patcher owns.** Any change
to the assignment rule silently invalidates those entries, and nothing in the
build notices. A `scripts/` guard that re-templates alias names against the
built objects and reports unrevivable ones would make that failure loud.

## What is left

- The one-string `symbol_aliases.json` fix for `__push_heap<CuePoint>` (+1
  complete function back).
- The 195 revivable alias names, for whoever owns that file.
- A staleness guard so an anon-ns rename cannot silently kill an alias again.
- The `?A@@` hashless spelling remains out of reach: `?A0x<h>@@` is 12 bytes
  and `?A@@` is 4, and this pass is length-preserving by construction.
