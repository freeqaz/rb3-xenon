# Pin waves 1–2, and the discovery that `total_code` was inflated

**2026-08-09.** Landed as `8338f084` (wave 1) and `11b5b122` (wave 2). Instrument:
`tools/report_conservation.py`. Queue: `tools/pin_from_symnames.py` →
`docs/decomp/pin-queue-symmap-v2-2026-08-06.tsv`.

---

## ★★★ The headline: pinning CORRECTS a too-large denominator

| key | before wave 1 | after wave 2 | Δ |
|---|---:|---:|---:|
| `matched_functions` | 44,248 | 44,248 | **0** |
| `matched_code` | 4,340,756 | 4,340,756 | **0** |
| `masked_equal_functions` | 22,864 | 22,864 | **0** |
| `total_code` | 10,646,496 | **10,320,692** | −325,804 |
| `total_functions` | 69,229 | 69,231 | +2 |
| `matched_code_percent` | 40.771687 | **42.05877** | **+1.287 pp** |

Verified on **main's own rebuilt `report.json`**, not only in a lane worktree.

**Matching keys are exactly neutral — pins never touch matched code.** The code%
rise is entirely a *shrinking denominator*, and that shrink is a **correction**.

### The mechanism

In an **unpinned** region dtk cannot bound a symbol, so it runs the symbol's
extent to the next known boundary. Wave 2 changed only **9 rows out of 69,231**,
and six of them were RESIZED — every one collapsing to the size `symbols.txt`
**already declared**, which is ground truth independent of the pin:

| row | billed as | true size (`symbols.txt`) |
|---|---:|---:|
| `fn_828B23A8` | 210,136 | **204** (`size:0xCC`) |
| `fn_82BF9F48` | 51,292 | 64 (`0x40`) |
| `fn_8287C430` | 46,816 | 12 (`0xC`) |
| `fn_82BE4E70` | 14,148 | 76 (`0x4C`) |
| `fn_82BCC8C0` | 6,972 | 8 (`0x8`) |
| `?ParseBooleanCastNode@…` | 128 | 124 |

**One row was carrying ~2% of the binary's entire reported code.** A pin supplies
the boundary and the row collapses to the truth.

⇒ **Every historical `matched_code_percent` measured over an unpinned-heavy tree
was UNDERSTATED.** Per [`feedback_accuracy_beats_headline_percent`], a code%
change driven by a truer denominator is the desired direction — but note this one
moves code% **up**, which is the opposite of the direction that doctrine usually
warns about. Do not read the +1.287 pp as decomp progress; nothing was matched.

### ★ The control that makes this a finding, not a story

Wave 2's **batch B2** — 70,840 B across 45 units and 8 vendor libraries — left the
row population **byte-for-byte identical**. So pinning is *inherently*
denominator-neutral; `total_code` moves **only** when the pinned span happens to
contain a mis-sized symbol. That control retro-explains lane PIN-B's Δ0 and
reclassifies wave 1's "4-byte alignment pad" as the same convergence.

Wave 1's own smaller instance was a phantom `type:label` (`lbl_82BE3D20`) whose
5,116 B extent straddled four pins and **double-counted 33 real function rows**
summing 5,024 B.

---

## ⛔⛔ Instruments that are VACUOUS here (three, all measured)

1. **A symbol NAME is not an address — `lbl_` names LIE.** `lbl_82BE3D20` is
   really at `0x82C16DF0`; `lbl_82858E94` is really at `0x82887AE4`. Two
   independent instances ⇒ systematic. Wave 2's first containment probe used the
   name-derived address, found a function inside, and would have recorded a false
   double-counting verdict. **Always read the address from `symbols.txt`.**
2. **`fn_<addr>` names are REWRITTEN** by `obj_target_symbol_renamer`
   (e.g. `fn_82C1D608` → `g726adpcm`), so deriving addresses from them is a
   false-negative machine.
3. **Auto-unit NAMES are unstable across splits.** A name-keyed unit diff read
   **"937 new units"** when the truth was 31.

⇒ The instrument that works is a **name+size multiset over all ~69k rows**, which
is what `tools/report_conservation.py` implements. Two headline totals hid a
2-row change inside 69,229; only the row-level diff found it.

---

## What was pinned

| wave | rows | units | bytes | families |
|---|---:|---:|---:|---|
| 1 (`8338f084`) | 46 | 31 | 372,384 | xhv2, xgraphics |
| 2 (`11b5b122`) | 292 | 168 | 1,338,268 | xaudio2, d3dx9, xgraphics, LIBCMT/d3d9i/xonline/xapilibi/xmic/xmcore/xinput2/xnet |
| **total** | **338** | **199** | **1,710,652** | |

Named units 1,077 → **1,276**. Selection filter both waves:
`obj_conf == 1.000` ∧ `flags ∉ {NO_OBJ, AMB_OBJ, ADJ}` ∧ `overlap_pinned == 0` ∧
`obj != '-'` ∧ `owned_bytes > 0`.

Unit names and object boundaries come from **DC3's leaked linker map**
(`../dc3-decomp/orig/373307D9/ham_xbox_r.map`) joined on the exact MSVC mangled
name — DC3's *addresses* are useless here and are never read. The oracle
corroborated itself unprompted: `optimize`'s blocks terminate at exactly the
address wave 1's predecessor had pinned, `buildssa` resumes at exactly the next,
and neighbours abut within 4-byte alignment.
⚠ Attribution is **name-based, not byte-verified** — RB3 may link a different XDK
build than DC3. Adjudicate a surprising boundary against retail bytes.

## Status and what remains

- **The `obj_conf == 1.000` vein is DRAINED**: the fresh queue falls to 75 rows,
  and the only survivor passing the filter is `system/zlib/trees.c` —
  deliberately excluded because its **source exists**, so `tools/project.py`
  gives it a real compile edge and a "pin" becomes a source change requiring an
  A/B.
- Remaining pin headroom is the excluded strata: `NO_OBJ` (40), `ADJ` (20),
  `AMB_OBJ` (1), plus Milo-lib rows the DC3 XDK map cannot resolve (object
  resolution fell 99.40% → 86.29% as target-map lanes added ~1,000 Harmonix
  names the XDK map has never seen).
- A pin wave needs **no `ab_measure`** for the matching keys — they are provably
  unmoved. It **does** need `report_conservation.py`, because the denominator can
  move for the good reason above.

## Operational traps hit while doing this

- **Reverting a pin is not symmetric with applying one.** `configure.py`
  validates splits headings against dtk's *not-yet-refreshed* `config.json`, so
  REMOVING a heading hard-fails rc=1. Recipe:
  `RB3_ALLOW_UNRESOLVED_SPLITS=1 python3 configure.py`, build, then plain
  `configure.py`.
- **A pin-only unit still REQUIRES an `objects.json` entry** (`"path": "NonMatching"`
  inside a lib group; no source file needed). `configure.py` hard-refuses
  otherwise, because a heading with no Object emits `base_path: None` and can
  never pair.
- **An `&&` chain silently swallowed a failed `configure.py`**, producing a
  "baseline" that was a stale report re-read. Check `report.json`'s **mtime**,
  not just an rc. ⚠ And an mtime check alone is *insufficient* in a worktree:
  `setup_worktree.sh` normalizes config mtimes to 2020-01-01, which is why
  `report_conservation.py` also refuses two snapshots taken from the same
  physical report.
- `tools/pin_from_symnames.py` resolved the DC3 map via `REPO.parent`, which is
  `~/tmp` in a worktree — the oracle silently vanished and every row read
  `obj=-`. Fixed in `60837907` (resolves via the worktree's gitdir pointer).
