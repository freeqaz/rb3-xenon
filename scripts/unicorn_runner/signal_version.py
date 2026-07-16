"""Signal version + schedule hash for unicorn verdict provenance.

The signal version is a single integer bumped each time the equivalence
check semantics change in a way that makes prior verdicts stale.
Consumers (classify_at_limit.py, query tools) read this from the DB so
they can re-classify by query (`WHERE unicorn_signal_version < N`) rather
than re-running 25k functions.

The schedule hash captures which inputs produced a given verdict so we
can audit and reproduce. It's deterministic per probe schedule.

Bump rules:
- Bump when a comparator rule changes (e.g. cap-exhaust → not EQUIV)
- Bump when a new signal axis is added (unmapped fingerprint, sentinel obj)
- Bump when the probe schedule shape changes meaningfully

Don't bump for:
- Bug fixes that don't change verdicts
- Pure performance improvements
- Cosmetic changes (formatting, message wording)
"""

import hashlib

# Current signal version. Bump whenever the verdict-producing semantics
# change. See bump rules in the module docstring.
#
# v1 — baseline (signal as of 2026-05-14 Phase 1 landing).
# v2 — Phase 2 (2026-05-14):
#       * cap-exhausted detection (no longer EQUIV when both sides hit
#         max_insns)
#       * wild_jump_match: same error string at DIFFERENT PCs now flags
#         DIVERGENT (matching error at matching PC stays EQUIV after
#         softening — the symmetric-null-deref class)
#       * size-mismatch guard at 4x ratio
#       * probe early-exit disabled by default
# v3 — Phase 3 (2026-05-14):
#       * unmapped-access fingerprint: low/kernel-range page accesses
#         that differ between decomp and orig now flag
#         unmapped_access_mismatch DIVERGENT
SIGNAL_VERSION = 3


def compute_schedule_hash(per_run_entries):
    """Hash a probe schedule for provenance.

    Accepts either a list of RunDetail-like dicts (must have fill_pattern,
    fixture_type, arg_r4, arg_r5, arg_r6) or a list of schedule tuples
    (fill, obj_mem_kind, fixture_type, r4, r5, r6).

    obj_mem is reduced to a kind ('none', 'typed', 'sentinel') because
    the raw bytes vary by class and aren't useful for grouping. For typed
    memory the fixture_type already encodes 'typed' so we don't need more.

    Returns 16 hex chars of sha256.
    """
    parts = [f"sv={SIGNAL_VERSION}"]
    for entry in per_run_entries:
        if isinstance(entry, dict):
            fill = entry.get("fill_pattern")
            fixture = entry.get("fixture_type", "fill")
            r4 = entry.get("arg_r4", 0)
            r5 = entry.get("arg_r5", 0)
            r6 = entry.get("arg_r6", 0)
        else:
            # Tuple form: (fill, obj_mem, fixture_type, r4, r5, r6)
            fill, _obj, fixture, r4, r5, r6 = (
                entry[0], entry[1], entry[2], entry[3], entry[4], entry[5],
            )
        fill_str = "z" if fill is None else f"{fill & 0xFF:02x}"
        parts.append(f"{fill_str}|{fixture}|{r4:08x}|{r5:08x}|{r6:08x}")

    raw = "\n".join(parts).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()[:16]
