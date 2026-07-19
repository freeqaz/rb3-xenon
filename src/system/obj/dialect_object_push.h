// sw3 cross-dialect scatter shim (NO #pragma once — re-includable per owner).
//
// Purpose: let an ObjMacros.h-dialect consumer TU whole-file-#include an
// Object.h-dialect owner .cpp (the "COMDAT-scatter" lever) without the two
// incompatible macro dialects colliding. The consumer already compiled its own
// body under the ObjMacros.h dialect; this header saves those definitions and
// re-installs the *Object.h* forms of the divergent macros so the textually
// following owner body expands exactly as it does in the owner's own TU. Pair
// with obj/dialect_object_pop.h after the owner #include to restore the
// consumer dialect.
//
// Byte-neutrality: this header is included ONLY at a consumer include-site,
// NEVER by the owner's own TU, by Object.h, or by ObjMacros.h. The owner unit's
// own object is therefore compiled unchanged. The macro bodies below are copied
// verbatim (same token sequence) from obj/Object.h so the embedded owner body
// codegen is identical to the owner's standalone TU.
//
// Divergent macro set (Object.h vs ObjMacros.h): INIT_REVS (2-arg vs 1-arg),
// LOAD_REVS (BinStreamRev d vs gRev=), ASSERT_REVS (d.rev vs gRev), and the
// SYNC_PROP family (stringifying _NEW_STATIC_SYMBOL vs bare-expression). All
// other obj-macros (SAVE_REVS, _SYNC_PROP_BITFIELD helper, BEGIN_/END_ family)
// are either identical across dialects or defined only in Object.h and never
// overridden, so they need no shimming.

#pragma push_macro("INIT_REVS")
#pragma push_macro("LOAD_REVS")
#pragma push_macro("ASSERT_REVS")
#pragma push_macro("SYNC_PROP")
#pragma push_macro("SYNC_PROP_SET")
#pragma push_macro("SYNC_PROP_MODIFY")
#pragma push_macro("SYNC_PROP_BITFIELD")
#pragma push_macro("HANDLE")
#pragma push_macro("HANDLE_EXPR")
#pragma push_macro("HANDLE_ACTION")
#pragma push_macro("HANDLE_ACTION_IF")
#pragma push_macro("HANDLE_ACTION_IF_ELSE")
#pragma push_macro("END_HANDLERS")

#undef INIT_REVS
#undef LOAD_REVS
#undef ASSERT_REVS
#undef SYNC_PROP
#undef SYNC_PROP_SET
#undef SYNC_PROP_MODIFY
#undef SYNC_PROP_BITFIELD
#undef HANDLE
#undef HANDLE_EXPR
#undef HANDLE_ACTION
#undef HANDLE_ACTION_IF
#undef HANDLE_ACTION_IF_ELSE
#undef END_HANDLERS

// --- Object.h dialect (verbatim token sequence from obj/Object.h) ---

#define INIT_REVS(rev, alt)                                                              \
    static const __declspec(align(4)) unsigned short gRev = rev;                         \
    static const __declspec(align(4)) unsigned short gAltRev = alt;

#define LOAD_REVS(bs)                                                                    \
    int revs;                                                                            \
    bs >> revs;                                                                          \
    BinStreamRev d(bs, revs);

#define ASSERT_REVS(rev1, rev2)                                                          \
    if (d.rev > rev1) {                                                                  \
        MILO_FAIL(                                                                       \
            "%s can't load new %s version %d > %d",                                      \
            PathName(this),                                                              \
            ClassName(),                                                                 \
            d.rev,                                                                       \
            gRev                                                                         \
        );                                                                               \
    }                                                                                    \
    if (d.altRev > rev2) {                                                               \
        MILO_FAIL(                                                                       \
            "%s can't load new %s alt version %d > %d",                                  \
            PathName(this),                                                              \
            ClassName(),                                                                 \
            d.altRev,                                                                    \
            gAltRev                                                                      \
        );                                                                               \
    }

#define SYNC_PROP(s, member)                                                             \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            return PropSync(member, _val, _prop, _i + 1, _op);                           \
    }

#define SYNC_PROP_SET(s, member, func)                                                   \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (_op == kPropSet) {                                                       \
                func;                                                                    \
            } else {                                                                     \
                if (_op == (PropOp)0x40)                                                 \
                    return false;                                                        \
                _val = member;                                                           \
            }                                                                            \
            return true;                                                                 \
        }                                                                                \
    }

#define SYNC_PROP_MODIFY(s, member, func)                                                \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (PropSync(member, _val, _prop, _i + 1, _op)) {                            \
                if (!(_op & (kPropSize | kPropGet))) {                                   \
                    func;                                                                \
                }                                                                        \
                return true;                                                             \
            } else {                                                                     \
                return false;                                                            \
            }                                                                            \
        }                                                                                \
    }

#define SYNC_PROP_BITFIELD(symbol, mask_member, line_num)                                \
    { _NEW_STATIC_SYMBOL(symbol) _SYNC_PROP_BITFIELD(_s, mask_member, line_num) }

// HANDLE family — Object.h stringifies its symbol arg via _NEW_STATIC_SYMBOL,
// where ObjMacros.h treats it as a bare expression. BEGIN_HANDLERS is NOT shimmed:
// in the match build both dialects expand to the timer-off form (Object.h gates on
// MILO_MESSAGE_TIMERS, ObjMacros on MILO_DEBUG&&HX_NATIVE — both inactive here).

#define HANDLE(s, func)                                                                  \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            _HANDLE_CHECKED(func(_msg))                                                  \
    }

#define HANDLE_EXPR(s, expr)                                                             \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            return expr;                                                                 \
    }

#define HANDLE_ACTION(s, action)                                                         \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            (action);                                                                    \
            return 0;                                                                    \
        }                                                                                \
    }

#define HANDLE_ACTION_IF(s, cond, action)                                                \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (cond) {                                                                  \
                (action);                                                                \
            }                                                                            \
            return 0;                                                                    \
        }                                                                                \
    }

#define HANDLE_ACTION_IF_ELSE(s, cond, action_true, action_false)                        \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (cond) {                                                                  \
                (action_true);                                                           \
            } else {                                                                     \
                (action_false);                                                          \
            }                                                                            \
            return 0;                                                                    \
        }                                                                                \
    }

// END_HANDLERS diverges in the match build: Object.h retains the PathName(this)
// side effect at the unhandled tail; ObjMacros.h returns a bare kDataUnhandled.
#ifndef HX_NATIVE
#define END_HANDLERS                                                                     \
    if (_warn)                                                                           \
        (void)(PathName(this), sym);                                                     \
    return DATA_UNHANDLED;                                                               \
    }
#else
#define END_HANDLERS                                                                     \
    if (_warn)                                                                           \
        MILO_NOTIFY("%s unhandled msg: %s", PathName(this), sym);                        \
    return DATA_UNHANDLED;                                                               \
    }
#endif
