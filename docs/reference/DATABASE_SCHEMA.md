# Database Schema: decomp.db

SQLite database tracking function status, patterns, scoring, and attempt history for the rb3-xenon decompilation project.

## Quick Access

```bash
# Query from command line
sqlite3 decomp.db "SELECT symbol, current_percent FROM functions WHERE symbol LIKE '%CharBones%' LIMIT 5"

# Interactive mode
sqlite3 decomp.db
```

## Tables

### functions

Primary table tracking all symbols in the project.

| Column | Type | Description |
|--------|------|-------------|
| `symbol` | TEXT | Mangled symbol name (unique) |
| `demangled` | TEXT | Human-readable demangled name |
| `unit` | TEXT | Source unit path (e.g., `default/system/char/CharBones`) |
| `size` | INTEGER | Function size in bytes |
| `current_percent` | REAL | Current match percentage (0-100) |
| `best_percent` | REAL | Best ever match percentage achieved |
| `verdict` | TEXT | Status: `COMPLETE`, `AT_LIMIT`, `NEAR_COMPLETE`, or NULL |
| `locked_by` | TEXT | Session ID holding lock (for concurrent agents) |
| `locked_at` | TIMESTAMP | When the lock was acquired |
| `attempt_count` | INTEGER | Number of decomp attempts made |
| `last_model` | TEXT | Last model used (haiku, sonnet, opus) |
| `source_patch` | TEXT | Successful patch diff (if any) |

**Pattern Detection Columns:**

| Column | Type | Description |
|--------|------|-------------|
| `excluded` | INTEGER | 1 if XDK/external code, skip during work selection |
| `has_linker_merged` | BOOLEAN | 1 if function has ICF-merged calls (unfixable) |
| `has_bool_mask` | BOOLEAN | 1 if bool mask pattern detected |
| `has_assert_revs` | BOOLEAN | 1 if ASSERT_REVS scheduling issues |
| `has_ltcg_pooling` | BOOLEAN | 1 if LTCG address pooling (usually N/A for debug builds) |
| `primary_pattern` | TEXT | Primary pattern tag (e.g., `REGISTER_SWAP`, `LINKER_MERGED`) |
| `reachable_100` | BOOLEAN | 1 if function can theoretically reach 100% match |

**Scoring Columns:**

| Column | Type | Description |
|--------|------|-------------|
| `priority_score` | REAL | Combined score (0-100): ease × impact × confidence |
| `ease_score` | INTEGER | How easy to improve (0-100, based on current %) |
| `impact_score` | INTEGER | How impactful (0-100, based on fan_in) |
| `confidence_score` | INTEGER | Confidence in reachability (0-100) |

**Call Graph Columns:**

| Column | Type | Description |
|--------|------|-------------|
| `fan_in` | INTEGER | Number of callers (functions that call this one) |
| `fan_out` | INTEGER | Number of callees (functions this one calls) |

**Type Analysis Columns:**

| Column | Type | Description |
|--------|------|-------------|
| `is_constructor` | BOOLEAN | 1 if constructor function |
| `is_destructor` | BOOLEAN | 1 if destructor function |
| `is_virtual` | BOOLEAN | 1 if virtual function |
| `has_rb3_ref` | BOOLEAN | 1 if has matching RB3 reference |
| `string_ref_count` | INTEGER | Number of string references (for identifying entry points) |

---

### attempts

History of all decomp attempts for learning and debugging.

| Column | Type | Description |
|--------|------|-------------|
| `function_id` | INTEGER | Foreign key to functions.id |
| `session_id` | TEXT | Agent session identifier |
| `model` | TEXT | Model used (haiku, sonnet, opus) |
| `started_at` | TIMESTAMP | When attempt started |
| `finished_at` | TIMESTAMP | When attempt finished |
| `exit_status` | TEXT | Result: `success`, `stuck`, `error` |
| `start_percent` | REAL | Match % before attempt |
| `end_percent` | REAL | Match % after attempt |
| `verdict` | TEXT | Attempt verdict |
| `patch` | TEXT | Code patch that was tried |
| `notes` | TEXT | Agent's summary/notes |
| `iterations` | INTEGER | Number of tool calls made |

**Token Tracking:**

| Column | Type | Description |
|--------|------|-------------|
| `input_tokens` | INTEGER | API input tokens used |
| `output_tokens` | INTEGER | API output tokens used |
| `cache_read_tokens` | INTEGER | Prompt cache read tokens |
| `cache_creation_tokens` | INTEGER | Prompt cache creation tokens |
| `actual_cost_usd` | REAL | Actual cost from API |
| `duration_ms` | INTEGER | Total attempt duration in milliseconds |

---

### call_edges

Call graph relationships between functions.

| Column | Type | Description |
|--------|------|-------------|
| `caller_symbol` | TEXT | Calling function (mangled symbol) |
| `callee_symbol` | TEXT | Called function (mangled symbol) |
| `call_count` | INTEGER | Number of call sites (default: 1) |

---

### file_pairs

RB3 (Rock Band 3) cross-reference pairings for shared Milo engine code.

| Column | Type | Description |
|--------|------|-------------|
| `dc3_unit` | TEXT | DC3 unit path (unique) |
| `rb3_file` | TEXT | Matching RB3 source file path |
| `compatibility_score` | REAL | Function overlap ratio (0-1) |
| `function_overlap` | INTEGER | Number of functions with matching names |
| `dc3_function_count` | INTEGER | Total functions in DC3 unit |
| `rb3_function_count` | INTEGER | Total functions in RB3 file |
| `has_rb2_dwarf` | BOOLEAN | Has class info in RB2 DWARF dump |

---

### worktrees

Git worktree pool for parallel agent work.

| Column | Type | Description |
|--------|------|-------------|
| `path` | TEXT | Worktree filesystem path (unique) |
| `session_id` | TEXT | Session currently using it |
| `status` | TEXT | `available`, `in_use`, or `dirty` |

---

## Views

Pre-defined views for common queries:

| View | Description |
|------|-------------|
| `v_priority_queue` | All non-excluded functions with scoring columns |
| `v_priority_queue_active` | Functions available for work (not complete/at_limit) |
| `v_reachable_100` | Functions that can reach 100%, ordered by priority |
| `v_high_impact` | High fan-in functions (≥5 callers) |
| `v_type_anchors` | Constructors and destructors for type validation |
| `v_near_complete_units` | Units at 70-99% completion |
| `v_pattern_summary` | Pattern distribution for 80%+ functions |

---

## Common Queries

### Find work targets

```sql
-- Functions that CAN reach 100% (no unfixable patterns)
SELECT symbol, demangled, current_percent, unit
FROM functions
WHERE reachable_100 = 1
  AND current_percent < 100
  AND excluded = 0
  AND (verdict IS NULL OR verdict NOT IN ('COMPLETE', 'AT_LIMIT'))
ORDER BY current_percent DESC
LIMIT 20;

-- High-impact functions (many callers)
SELECT symbol, demangled, fan_in, current_percent
FROM functions
WHERE fan_in >= 5
  AND current_percent < 100
  AND excluded = 0
ORDER BY fan_in DESC
LIMIT 20;

-- Fresh targets (never attempted, high match)
SELECT symbol, demangled, current_percent, unit
FROM functions
WHERE attempt_count = 0
  AND current_percent >= 80
  AND excluded = 0
ORDER BY current_percent DESC
LIMIT 20;
```

### Pattern analysis

```sql
-- Functions with ICF-merged calls
SELECT symbol, current_percent
FROM functions
WHERE has_linker_merged = 1
  AND excluded = 0
ORDER BY current_percent DESC;

-- Functions with register swap issues
SELECT symbol, current_percent
FROM functions
WHERE primary_pattern = 'REGISTER_SWAP'
  AND excluded = 0
ORDER BY current_percent DESC;

-- Pattern distribution for 80%+ functions
SELECT
    CASE
        WHEN has_linker_merged THEN 'LINKER_MERGED'
        WHEN has_bool_mask THEN 'BOOL_MASK'
        WHEN primary_pattern IS NOT NULL THEN primary_pattern
        ELSE 'NO_PATTERN'
    END as pattern,
    COUNT(*) as count,
    ROUND(AVG(current_percent), 1) as avg_match
FROM functions
WHERE excluded = 0 AND current_percent >= 80
GROUP BY pattern
ORDER BY count DESC;
```

### Unit completion

```sql
-- Near-complete units (70-99%)
SELECT
    unit,
    COUNT(*) as total,
    SUM(CASE WHEN current_percent >= 100 THEN 1 ELSE 0 END) as matched,
    ROUND(100.0 * SUM(CASE WHEN current_percent >= 100 THEN 1 ELSE 0 END) / COUNT(*), 1) as pct
FROM functions
WHERE excluded = 0 AND unit IS NOT NULL
GROUP BY unit
HAVING pct >= 70 AND pct < 100
ORDER BY pct DESC;

-- Functions blocking unit completion
SELECT symbol, current_percent, verdict
FROM functions
WHERE unit = 'default/system/char/CharBones'
  AND current_percent < 100
ORDER BY current_percent DESC;
```

### Caller/callee analysis

```sql
-- What functions call a specific function?
SELECT caller_symbol, call_count
FROM call_edges
WHERE callee_symbol = '?Poll@Game@@QAEXXZ'
ORDER BY call_count DESC;

-- What functions does a specific function call?
SELECT callee_symbol, call_count
FROM call_edges
WHERE caller_symbol = '?Poll@Game@@QAEXXZ';
```

---

## Population

The database is populated by:

| Script | Populates |
|--------|-----------|
| `scripts/orchestrator/database.py` → `ingest_report()` | Base function data from `report.json` |
| `scripts/decomp_orchestrate.py sync` | Syncs `current_percent` with latest build |
| MCP tools via orchestrator | Pattern flags, scoring columns |
| Agent work sessions | `attempts` table, `verdict` updates |

---

## Indexes

Optimized indexes exist for common query patterns:

- `idx_functions_percent` - Query by match percentage
- `idx_functions_verdict` - Filter by verdict
- `idx_functions_unit` - Query by source unit
- `idx_functions_excluded` - Filter excluded functions
- `idx_functions_reachable` - Filter reachable functions
- `idx_functions_priority` - Order by priority score
- `idx_call_edges_caller` / `idx_call_edges_callee` - Call graph lookups

---

## See Also

- [../tools/INDEX.md](../tools/INDEX.md) - Tool documentation index
- [../decomp/patterns/INDEX.md](../decomp/patterns/INDEX.md) - Pattern reference with SQL queries
- [../meta-strategy/SQL_QUERIES.md](../meta-strategy/SQL_QUERIES.md) - Additional analysis queries
