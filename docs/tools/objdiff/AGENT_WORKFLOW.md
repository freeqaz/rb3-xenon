# Agent Workflow Analysis

> Part of the [objdiff CLI Design](./OBJDIFF_CLI_DESIGN.md) documentation.

This document analyzes agent workflows and what queries they need, based on review of `docs/sessions/2026-01-worksession-archive.md`, `docs/decomp/SUBAGENT_STRATEGY.md`, and `docs/decomp/LOW_HANGING_FRUIT.md`.

---

## Queries Agents Actually Need

| Query | objdiff Scope? | Notes |
|-------|----------------|-------|
| Functions by match % range (90-99%) | **Yes** | Core report query |
| Functions by size range (<100 bytes) | **Yes** | Core report query |
| Functions in specific file/unit | **Yes** | Core report query |
| Functions by directory pattern | **Yes** | Core report query |
| Unimplemented functions (null match %) | **Yes** | Need to distinguish null vs 0% |
| Functions with RB3 equivalent | No | DC3-specific, external mapping |
| Claim/lock a function | No | Coordination layer, separate tool |
| Quick wins (small + unimplemented) | **Yes** | Combined filter |
| Near-matches (99%+) | **Yes** | Threshold filter |

---

## Critical Insight: Null vs 0% Match

From the workflow docs, there's an important distinction:
- **`fuzzy_match_percent: null`** = Function has no implementation at all
- **`fuzzy_match_percent: 0`** = Function is implemented but completely wrong

The CLI must support filtering for `null` specifically:
```bash
# Find unimplemented functions (null match)
objdiff report query report.json --functions --unimplemented

# Find implemented but broken (0%)
objdiff report query report.json --functions --max-percent 0
```

---

## Agent Tiering

Based on `LOW_HANGING_FRUIT.md`:

| Tier | Criteria | Recommended Agent |
|------|----------|-------------------|
| 1 | <50 bytes, any % | Haiku (trivial) |
| 2 | 50-100 bytes | Haiku/Sonnet |
| 3a | 100-200 bytes, unimplemented | Sonnet |
| 3b | Any size, 90-99% match | Sonnet (fix) |
| 4 | >200 bytes, unimplemented | Sonnet (hard) |
| 5 | Any size, 99%+ stuck | Opus (deep analysis) |

### Tier-Based Queries

```bash
# Get tier 1 targets (trivial)
objdiff report query report.json --functions --max-size 50 --unimplemented

# Get tier 3b targets (near-matches to fix)
objdiff report query report.json --functions --min-percent 90 --max-percent 99

# Get tier 5 targets (stuck at 99%+)
objdiff report query report.json --functions --min-percent 99 --max-percent 99.9
```

---

## Parallel Agent Coordination

For 15+ parallel agents, coordination is needed but **should be a separate layer**:
- objdiff CLI = stateless queries
- Coordination tool = claims, locks, state tracking

This keeps objdiff generic and reusable across decomp projects.

---

## What Belongs Where

### In objdiff CLI (generic, reusable)

- Report querying with filters (%, size, unit pattern)
- JSON/proto/markdown output
- Diff output in JSON format
- Report comparison and trending
- Summary statistics

### In DC3 Wrapper Tool (project-specific)

- RB3 cross-reference lookups
- Function claim/lock mechanism
- Agent work assignment
- Pattern/lesson database
- Build shortcuts (`ninja build/45410914/src/...`)
- Model escalation tracking

---

## Feature Phases

### Phase 1: Core Query (MVP)
- `report query` with filters
- `report summary`
- `report function`
- **Add `--unimplemented` flag** for null match %
- JSON output

### Phase 2: Enhanced Diff
- `diff --output-format json`
- Include diff_score in output
- Optional instruction-level detail

### Phase 3: Additional Output Formats
- **Markdown output** for human review
- CSV export

**Markdown Output Example:**
```bash
$ objdiff report query report.json --functions --min-percent 90 --max-percent 99 --format markdown
```
```markdown
## Query Results

**Filters:** match 90-99%, functions only
**Found:** 47 functions

| Function | Unit | Size | Match % |
|----------|------|------|---------|
| `Game::Poll()` | src/lazer/game/Game.cpp | 1248 | 97.5% |
| `PresenceMgr::SetInGame()` | src/lazer/game/PresenceMgr.cpp | 64 | 99.2% |
| ... | | | |
```

### Phase 4: Streaming & Advanced
- stdin support for piping
- `report trending` for multi-report comparison

---

## Open Questions

1. **Proto schema** - Add new message types to report.proto?
2. **Streaming output** - Support for large reports with many functions?
3. **Config file support** - Should query presets be configurable?
