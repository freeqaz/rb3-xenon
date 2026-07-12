/* =============================================================================
 * xonline_stub.c  --  Strategy B, Lane K convergence
 *
 * Minimal definitions for the two XONLINE client-library functions that the
 * stock RB3Enhanced.dll obtained by statically linking the XDK xonline lib, and
 * that are NOT single xam.xex exports (confirmed: absent from x360_imports.py
 * and stock_imports.txt). Lanes H and I deferred them as needing a decision.
 *
 * DECISION (Lane K finish): stub. Both are on the online session-search / invite
 * path, never reached at DllMain / first boot. RB3E's own hooks
 * (XSessionSearchExHook / XInviteGetAcceptedInfoHook in xbox360_liveless.c)
 * override the result of the underlying call:
 *   - XSessionSearchExHook ignores XSessionSearchEx's return and synthesizes a
 *     fake liveless session, so a no-op returning ERROR_SUCCESS (0) is safe.
 *   - XInviteGetAcceptedInfoHook only falls through to XInviteGetAcceptedInfo
 *     when there is NO pending liveless join; returning non-zero (no accepted
 *     invite) is the correct "nothing to accept" semantic.
 *
 * C linkage: symbol name is independent of the parameter list, so empty
 * parameter lists here still emit the exact externals the link needs.
 * ========================================================================== */

typedef unsigned long DWORD;

/* ERROR_SUCCESS: hook discards this and builds its own fake XSESSION result. */
DWORD XSessionSearchEx(void)
{
    return 0UL;
}

/* Non-zero => no accepted invite pending (fallback path only). */
DWORD XInviteGetAcceptedInfo(void)
{
    return 1UL;
}
