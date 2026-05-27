"""RB3-Xenon Decomp Orchestrator - Multi-agent decompilation pipeline."""

from .database import (
    init_database,
    get_connection,
    ingest_report,
    get_next_function,
    query_functions,
    lock_function,
    unlock_function,
    unlock_session,
    record_attempt,
    update_function_status,
    get_function_by_symbol,
    get_last_attempt,
    get_attempts_for_function,
    get_stats,
)

from .worktree_pool import WorktreePool

# DecompMCPServer imports optional modules (analysis.stack_layout, analysis.diff_inspect,
# tools.merged_symbols) that are ported in parallel. Import lazily so the package
# __init__ doesn't fail when those modules aren't present yet.
try:
    from .mcp_server import DecompMCPServer
except ImportError:
    DecompMCPServer = None  # type: ignore[assignment, misc]

# Reporting module is ported separately; guard it too.
try:
    from .reporting import (
        generate_progress_report,
        generate_batch_summary,
        get_recent_attempts,
        get_active_sessions,
        get_unit_summary,
    )
    _reporting_available = True
except ImportError:
    _reporting_available = False

__all__ = [
    # Database
    "init_database",
    "get_connection",
    "ingest_report",
    "get_next_function",
    "query_functions",
    "lock_function",
    "unlock_function",
    "unlock_session",
    "record_attempt",
    "update_function_status",
    "get_function_by_symbol",
    "get_last_attempt",
    "get_attempts_for_function",
    "get_stats",
    # Worktree
    "WorktreePool",
    # MCP
    "DecompMCPServer",
    # Reporting (if available)
    "generate_progress_report",
    "generate_batch_summary",
    "get_recent_attempts",
    "get_active_sessions",
    "get_unit_summary",
]
