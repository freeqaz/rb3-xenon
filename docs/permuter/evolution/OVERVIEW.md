# Permuter Evolution

Layered architecture upgrade for the C++ permuter. Introduces reusable primitives underneath the existing Pattern interface so that writing patterns is easier, patterns can compose, and the test suite verifies both.

## Status

| Phase | Name | Status | Description |
|-------|------|--------|-------------|
| 1 | [Primitives](phase1-primitives.md) | **Complete** | `SourceEditor` + `ast_queries` — reusable building blocks |
| 2 | [Pattern Migration](phase2-migration.md) | **Complete** | Migrate all 12 patterns to use Phase 1 primitives |
| 3 | [Composition](phase3-composition.md) | **Complete** | Chain patterns via re-parse, budget allocation |

## Why

Every pattern currently has its own recursive AST walker, its own byte-splicing logic, its own indentation handling. This creates problems:

| Problem | Impact | Example |
|---------|--------|---------|
| Duplicate walkers | 7 copies of `_find_X(node)` across patterns | `_find_comparisons` in 3 files |
| Manual byte splicing | Every pattern does `source[:s] + new + source[e:]` | Silent corruption on overlapping edits |
| No composition | Each pattern sees only original source | Can't chain varext then declreorder |
| Global 100-variant cap | First pattern can starve all others | commutative_swap exhausts budget |

## Invariants (what does NOT change)

- `Pattern` ABC interface — `generate()` and `relevant()` signatures stay identical
- `Variant` dataclass — `source: bytes` remains the output currency
- `FunctionContext` dataclass — no fields removed
- `__init_subclass__` auto-registration — no metaclass changes
- CLI flags — `--patterns`, `--max-variants`, `--dry-run` unchanged; `--compose` added as opt-in

## Verification

The test suite is the safety net for every change. Each phase adds its own tests and must pass all existing ones:

```bash
python -m pytest scripts/permuter/tests/ -v
```

## See Also

- [Permuter INDEX](../INDEX.md) — usage docs, CLI reference, pattern catalog
- [Guided Permuter](../guided-permuter.md) — diagnosis-driven pattern filtering (already implemented)
