# W16-GU — withdraw `ForeachKeyframe` from the `DataNode` stub fold group (2026-09-30)

W16-GN (`aa576155`) replaced `RndPropAnim::ForeachKeyframe`'s `{ return DataNode(0); }` scaffold
stub with the real body (retail `fn_82429C38`, 2,256 B). The alias group at `0x8228d358`
(survivor `??0DataNode@@QAA@XZ`, 44 folded stub-shaped members) still listed
`?ForeachKeyframe@RndPropAnim@@QAA?AVDataNode@@PBVDataArray@@@Z` — a membership that existed only
because our function used to be a stub. W16-GS's read-only scout confirmed from our compiled object
that the body is now 2,192 B / 76 relocations, so it cannot be byte-identical to the 16-byte survivor.

The validator does not flag it (the real body's retail address is not in the map, so nothing
*contradicts* it); it is false on bytes, not on map residency.

**Change:** move the spelling from `folded` to `withdrawn`, class `SURVIVOR_SIZE_MISMATCH`.
Nothing pruned.

**Prediction (before measuring):** Δ0 functions / Δ0 bytes / masked Δ0. `ForeachKeyframe`'s callers
reach an unnamed retail body, and objdiff already forgives placeholder callees. A non-zero result
means the alias was hiding a real charge; it would be recorded as measured.

## Measured
`ab_measure --from-dirty`, map class, forced re-split both legs, both at a `symbols.txt` fixed point,
0 recompiles: matched 44,240 / masked 23,324 / honest 20,916 / code% 41.128994 / fuzzy 50.513783 on
**both** legs; units 201 / 177 unchanged. **Δ0 on every key, as predicted** — the alias was not hiding a
charge. The `none` control was flat, which for a withdrawal is the expected shape, not evidence either way.
