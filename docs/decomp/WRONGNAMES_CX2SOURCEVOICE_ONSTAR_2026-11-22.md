# CX2SourceVoice On* forwarders: the map was shifted by one slot (2026-11-22, lane wrongnames)

Branch `wrongnames`, started from `fb846835e` (`absent2`, which named `0x82BCD108`
`BeatMatchController::RegisterRGFret`). All addresses are clean-TU5 retail VAs read from
`orig/45410914/band.exe` with `tools/va_disasm.py`.

## Claim checked

`scripts/target_symbol_map.json` named `0x82BCD0E0`
`?OnPacketLoopEnd@CX2SourceVoice@XAUDIO2@@UAAXPAXI@Z`, but its body calls the user
callback's vtable +4, which is `IXAudio2VoiceCallback::OnVoiceProcessingPassEnd`, not +0x14
(`OnLoopEnd`). Prediction before reading bytes: the four 0x28-spaced forwarders at
`0x82BCD0B8..0x82BCD130` are PassStart, PassEnd, PacketStart, PacketLoopEnd in that order,
and the map's `0x82BCD0E0` row is one slot early. Confirmed.

## Evidence, per row

All four forwarders have the same shape: `lwz r11,0x38(r3)` (m_pCallback) / `cmplwi` /
`beqlr` / `clrrwi r3,r11,0` / `lwz r11,0(r3)` / `lwz r11,SLOT(r11)` / `mtctr` / `bctr`.

| RB3 VA | callback slot | args forwarded | DC3 body (by DC3 map name) | Retail ILeapVoiceCallback vtable `0x821A6BB0` index | Verdict |
|---|---|---|---|---|---|
| `0x82BCD0B8` | +0x0 OnVoiceProcessingPassStart | `mr r4,r5` | `?OnVoiceProcessingPassStart@CX2SourceVoice` `0x82E705A8`, byte-identical | [6] | map name **correct** |
| `0x82BCD0E0` | +0x4 OnVoiceProcessingPassEnd | none | `?OnVoiceProcessingPassEnd@CX2SourceVoice` `0x82E705D0`, byte-identical | [7] | map said `OnPacketLoopEnd`: **WRONG**, renamed to `?OnVoiceProcessingPassEnd@CX2SourceVoice@XAUDIO2@@UAAXXZ` |
| `0x82BCD108` | +0xC OnBufferStart | r4 (context) | `?OnPacketStart@CX2SourceVoice` `0x82E705F8`, byte-identical | [8] | named `RegisterRGFret` by absent2: a **correct fold member** (ICF); left as is. Its XAudio2 identity is `OnPacketStart` |
| `0x82BCD130` | +0x14 OnLoopEnd | r4 (context) | `?OnPacketLoopEnd@CX2SourceVoice` `0x82E70620`, byte-identical | [9] | was unnamed (`fn_82BCD130`): **named** `?OnPacketLoopEnd@CX2SourceVoice@XAUDIO2@@UAAXPAXI@Z` |
| `0x82BCD820` | +0x10 OnBufferEnd, then +0x8 OnStreamEnd if `flags & 0x40` | r4 | `?OnPacketCompletion@CX2SourceVoice` `0x82E70CF8` | [10] | map name **correct** |

Independent lines of evidence:

1. **DC3 retail bytes.** `../dc3-decomp/orig/373307D9/ham_xbox_r.exe` at the DC3 map
   addresses: PassStart `...7ca42b78 81630000 816b0000...`, PassEnd `...816b0004...`,
   PacketStart `...816b000c...`, PacketLoopEnd `...816b0014...`, every word equal to RB3's
   body at the corrected address. DC3's map places the four contiguously in that order
   (`0x82E705A8/5D0/5F8/620`), the same 0x28 stride as RB3's.
2. **Vtable slot order.** DC3's `??_7CX2SourceVoice@XAUDIO2@@6BILeapVoiceCallback@@@`
   (`0x8226578C`) is [0..3] empty fold, [4] OnSetInputSampleRate, [5] OnSubmitMessage,
   [6] PassStart, [7] PassEnd, [8] PacketStart, [9] PacketLoopEnd, [10] PacketCompletion.
   RB3's matching table at `0x821A6BB0` holds `0x82BCD0B8, 0x82BCD0E0, 0x82BCD108, 0x82BCD130,
   0x82BCD820` at [6..10]. In the RB3 WMA (`0x821A6CCC`) and XMA (`0x821A6DE4`) tables, slot [7]
   is `0x82BCD0E0` again, right after each class's own PassStart override at [6]. The
   overridden RB3 slots carry map names consistent with DC3's derived tables: WMA [5]
   OnSubmitMessage `0x82BD0B20`, WMA [6] PassStart `0x82BD0AA8`, XMA [2] OnBreakLoop
   `0x82BD14D0`, [3] OnCancelAllPendingPackets `0x82BD21D8`, [4] OnSetInputSampleRate
   `0x82BD35C8`, [5] OnSubmitMessage `0x82BD2890`. So every other On* row in the cluster
   checks out.
3. **Callers.** `0x82BCD130` is called from `0x82BD0480` (inside
   `CX2SourceVoiceWMA::DispatchCurrentBuffer`) and `0x82BD1FB4` (inside
   `CX2SourceVoiceXMA::DeliverLoopCallbacks`), both loop-end paths. The WMA and XMA
   PassStart overrides call `0x82BCD0B8` (`0x82BD0ACC`, `0x82BD3140`).

How the wrong name probably got in: `dc3_oracle.json` (a TU0-era byte-similarity join)
pairs each of these DC3 names with arbitrary RB3 bodies at similarity 1.0. Any 9-instruction
forwarder of this shape is ambiguous to a byte matcher that does not compare the slot word.

## Alias repair

`scripts/symbol_aliases.json` group `RegisterHit` (address `0x82BCD0E0`): the survivor is now
`?OnVoiceProcessingPassEnd@CX2SourceVoice@XAUDIO2@@UAAXXZ`, and the folded member
`?RegisterHit@BeatMatchController@@QBAXW4HitType@@@Z` is unchanged. The fold proof still
holds: RegisterHit calls HitSink slot 1 (`Hit`, +4) through `mHitSink` at +0x38, which is
the PassEnd body exactly. `tools/alias_callee_name_drift.py --reprove --write` re-proved the
group on the built tree as `{'PROVEN': 1}` and refreshed `scripts/alias_callee_names.json`.
`tools/alias_survivor_drift.py` reads OK.

Deliberately not done: no alias group for `0x82BCD108` (`OnPacketStart` + `RegisterRGFret`).
Nothing we compile references `OnPacketStart`, so the group would be inert. absent2's
map row already gives the caller in RealGuitarController a name-checked match.
No other `BeatMatchController` forwarder folds into this cluster: HitSink's
slots are Hit +4, Key +8, RGFret +0xC, RGStrum +0x10, and nothing calls +0x14.

## Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-wrongnames --pick 2ff77dd26` (leg A =
`fb846835e`, ruler `functionRelocDiffs=name_check`, objdiff sha256 `ec3badc0f33323e9`, both legs
re-split to a fixed point): **Δmatched +0, Δmasked_equal +0, Δhonest +0, Δcode +0 B, Δfuzzy
+0.000000 pp**. The `none` control is flat (55,092 → 55,092). Absolutes: leg A = leg B =
55,039 matched, code% 59.402390. A zero delta is what I predicted. Every renamed row sits in
`auto_03_82BCB964_text`, which has no base obj, so all of them are unpairable at any name.
The renamed callees' callers are XDK units with no base obj, and the one compiled caller
(`GuitarController::OnMsg` → RegisterHit) pairs through the alias group by address, which did
not move. This is a correctness fix: the map now says which XAudio2 handler each address is,
and the RegisterHit fold proof names the body it actually folds with.

## Other wrong-name screen (on the way)

Method (scratch script, not committed): for each map row whose mangled name DC3's map places at
exactly one address, compare RB3's body at the mapped address with DC3's body. Branch
targets and `lis`-paired immediates are masked, and DC3's carve size comes from
`../dc3-decomp/config/373307D9/symbols.txt`. When they differ, search ±0x800 in RB3 for a
same-size body equal to DC3's. Positive control: on the pre-fix map the screen flags
exactly `0x82BCD0E0 -> 0x82BCD130` among the `CX2SourceVoice@` rows. Whole map: 7,903 rows
match DC3, 139 flag a move, and 41 of those survive the filters (one hit, same carve size,
≥0x10 B).

**No further wrong name is proven.** Every surviving candidate in scope was refuted:

- `Rnd::OnClearColorR/G` (`0x82412E58/70`): RB3's three consecutive bodies read
  `this+0x2C/0x30/0x34`, followed by `OnClearColorPacked`, and all read fuzzy 100 against our
  source. DC3's Rnd has the color 4 B later. This is a layout difference that looks exactly
  like a one-slot shift, so the screen alone proves nothing.
- `NewObject` for TexLoadPanel, ContentLoadingPanel, SongSelectPanel and
  RndSoftParticleBuffer: each body calls the ctor its own map name says, and all are fuzzy
  100. The DC3 hit is a same-sized class (`li r3,size` is the only discriminator).
- `??1FxSendFlanger360` / `??1FxSendEQ360`: RTTI of the vtable each dtor stores is
  `.?AVFxSendFlanger360@@` / `.?AVFxSendEQ360@@` respectively, so the map is right. Masking the
  vtable address made them look alike.
- `GoalProgressionInfo` vs `GoalAcquisitionInfo` STL instantiations
  (AccomplishmentManager): the mapped `_Destroy_Range` rows are fuzzy 100. The DC3 body has a
  different element size.

**Unadjudicated XDK candidates (out of scope, no base obj, need their own lane if ever
funded).** These are reciprocal pairs, each body equal to the other name's DC3 body. That
would be a swap, but the Rnd case shows that XDK version drift between RB3 and DC3 can
produce the same pattern, so none is proven:
`PSEmitLT`/`PSEmitGE@CPSProgram` (`0x82915320`/`0x82915458`),
`MakeInstr_V_SETGT`/`V_ADD` (`0x829B6EE8`/`0x829B70A8`),
`MakeInstr_V_MAXA`/`V_DST` (`0x829E4EE0`/`0x829E4FB8`),
`Encode@CCodec_G16R16F`/`CCodec_R16F` (`0x82A06300`/`0x82A06398`, virtual, but the XDK
vtables carry no RTTI). There are also one-step chains in `MakeInstr_V_MIN..SETNE`
(`0x829E4458..0x829E4DF0`) and `IRAlloc*/IRLoopIndex::NewInst` (`0x82A5AD48..0x82A5AF08`).
