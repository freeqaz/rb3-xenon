#pragma once
// These headers are prerequisites for the macros here to function correctly
// #include "obj/Object.h" // We're included
#include "os/System.h" /* IWYU pragma: keep */
#include "obj/PropSync_p.h" /* IWYU pragma: keep */
#include "obj/MessageTimer.h" /* IWYU pragma: keep */

/** Get this Object's path name.
 * @param [in] obj The Object.
 * @returns The Object's path name, or "NULL Object" if it doesn't exist.
 */
const char *PathName(const class Hmx::Object *obj);

// BEGIN CLASSNAME MACRO
// -------------------------------------------------------------------------------

#define OBJ_CLASSNAME(classname)                                                         \
    virtual Symbol ClassName() const { return StaticClassName(); }                       \
    static Symbol StaticClassName() {                                                    \
        static Symbol name(#classname);                                                  \
        return name;                                                                     \
    }

// END CLASSNAME MACRO
// ---------------------------------------------------------------------------------

// BEGIN SET TYPE MACRO
// --------------------------------------------------------------------------------
#ifdef MILO_DEBUG
#define OBJ_SET_TYPE(classname)                                                           \
    virtual void SetType(Symbol classname) {                                              \
        static DataArray *types = SystemConfig("objects", StaticClassName(), "types");    \
        if (classname.Null())                                                             \
            SetTypeDef(0);                                                                \
        else {                                                                            \
            DataArray *found = types->FindArray(classname, false);                        \
            if (found != 0)                                                               \
                SetTypeDef(found);                                                        \
            else {                                                                        \
                /* retail MILO_WARN: output stripped, args still evaluated (R2L) */       \
                (void)(PathName(this), ClassName(), classname);                           \
                SetTypeDef(0);                                                            \
            }                                                                             \
        }                                                                                 \
    }
#else
#define OBJ_SET_TYPE(classname)                                                           \
    virtual void SetType(Symbol classname) {                                              \
        static DataArray *types = SystemConfig("objects", StaticClassName(), "types");    \
        if (classname.Null())                                                             \
            SetTypeDef(0);                                                                \
        else {                                                                            \
            DataArray *found = types->FindArray(classname, false);                        \
            if (found != 0)                                                               \
                SetTypeDef(found);                                                        \
            else {                                                                        \
                /* Retail's stripped SetType residue evaluates LEFT-TO-RIGHT             \
                 * (PathName before the ClassName vcall), i.e. the comma form.           \
                 * MILO_WARN is MiloStripEval, whose ARGUMENTS evaluate right-to-left    \
                 * on MSVC -- that inverts the order and breaks this family.  Use        \
                 * MILO_NOTIFY (the comma form), matching OBJ_SET_TYPE_ENGINE in         \
                 * Object.h, which stayed at 100% throughout.  No arg here is a          \
                 * destructible class type, so the copying half of MiloStripEval buys    \
                 * nothing.  See docs/decomp/EH_FUNCLET_CASCADE.md.                      \
                 */                                                                      \
                MILO_NOTIFY(                                                              \
                    "%s:%s couldn't find type %s", PathName(this), ClassName(), classname \
                );                                                                        \
                SetTypeDef(0);                                                            \
            }                                                                             \
        }                                                                                 \
    }
#endif

// END SET TYPE MACRO
// ----------------------------------------------------------------------------------

// BEGIN HANDLE MACROS
// ---------------------------------------------------------------------------------

// Retail RB3 (Family-A) compiled Object::Handle with the MessageTimer
// instrumentation OFF — every retail Handle frame has zero Timer/MessageTimer
// references (proven in-source on GuitarController: its TU-local `#undef
// BEGIN_HANDLERS` to the timer-off form flips Handle to byte-exact, frame 0xc0,
// no timer). macros.h force-defines MILO_DEBUG tree-wide, which would otherwise
// expand the timer arm; gate the timer behind HX_NATIVE so the match build is
// uniformly timer-off (no per-TU restore needed, unlike Family-B's HANDLE_CHECK).
// The native port (HX_NATIVE) keeps the real MessageTimer for debug profiling.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
#define BEGIN_HANDLERS(objType)                                                          \
    DataNode objType::Handle(DataArray *_msg, bool _warn) {                              \
        Symbol sym = _msg->Sym(1);                                                       \
        MessageTimer timer(                                                              \
            (MessageTimer::Active()) ? static_cast<Hmx::Object *>(this) : 0, sym         \
        );
#else
#define BEGIN_HANDLERS(objType)                                                          \
    DataNode objType::Handle(DataArray *_msg, bool _warn) {                              \
        Symbol sym = _msg->Sym(1);
#endif

#define _HANDLE_CHECKED(expr)                                                            \
    {                                                                                    \
        DataNode result = expr;                                                          \
        if (result.Type() != kDataUnhandled)                                             \
            return result;                                                               \
    }

// -----------------------------------------------------------------------------------
// LOCAL-STATIC HANDLE variant (RB3 retail codegen lever).
//
// Retail band3 constructs each handler dispatch Symbol as a FUNCTION-LOCAL STATIC
// (`static Symbol _s("name")`): MSVC emits a guard-bit test + inline Symbol ctor +
// a ??__F atexit funclet, and packs one guard word / one bit per handler in source
// order. Our DC3-era headers reference centralized GLOBAL Symbols (Symbols2/3/4.h),
// so a Handle/OnMsg-heavy method emits a plain global-Symbol compare and structurally
// diverges (Matchmaker::Handle 83%, BandMatchmaker::Handle 72%: target-only guard
// lis/lwz/clrlwi. + ??__E/??__F machinery we never emit).
//
// The symbol arg is a bare identifier whose spelling == the wire Symbol name (Milo
// convention `Symbol foo("foo")`), so `#symbol` reproduces the retail string. Enable
// PER-TU with `-DRB3_HANDLE_LOCAL_STATIC` in the object's extra_cflags (like the
// proven /DRB3_MAP_0x1C per-TU gate) so no other TU's codegen moves.
// -----------------------------------------------------------------------------------
#ifdef RB3_HANDLE_LOCAL_STATIC

#define HANDLE(symbol, func)                                                             \
    {                                                                                    \
        static Symbol _hs(#symbol);                                                       \
        if (sym == _hs)                                                                  \
            _HANDLE_CHECKED(func(_msg))                                                  \
    }

#define HANDLE_EXPR(symbol, expr)                                                        \
    {                                                                                    \
        static Symbol _hs(#symbol);                                                       \
        if (sym == _hs)                                                                  \
            return expr;                                                                 \
    }

#define HANDLE_ACTION(symbol, action)                                                    \
    {                                                                                    \
        static Symbol _hs(#symbol);                                                       \
        if (sym == _hs) {                                                                \
            (action);                                                                    \
            return 0;                                                                    \
        }                                                                                \
    }

#define HANDLE_ACTION_IF(symbol, cond, action)                                           \
    {                                                                                    \
        static Symbol _hs(#symbol);                                                       \
        if (sym == _hs) {                                                                \
            if (cond) {                                                                  \
                (action);                                                                \
            }                                                                            \
            return 0;                                                                    \
        }                                                                                \
    }

#define HANDLE_ACTION_IF_ELSE(symbol, cond, action_true, action_false)                   \
    {                                                                                    \
        static Symbol _hs(#symbol);                                                       \
        if (sym == _hs) {                                                                \
            if (cond) {                                                                  \
                (action_true);                                                           \
            } else {                                                                     \
                (action_false);                                                          \
            }                                                                            \
            return 0;                                                                    \
        }                                                                                \
    }

#else

#define HANDLE(symbol, func)                                                             \
    if (sym == symbol)                                                                   \
    _HANDLE_CHECKED(func(_msg))

#define HANDLE_EXPR(symbol, expr)                                                        \
    if (sym == symbol)                                                                   \
        return expr;

#define HANDLE_ACTION(symbol, action)                                                    \
    if (sym == symbol) {                                                                 \
        /* for style, require any side-actions to be performed via comma operator */     \
        (action);                                                                        \
        return 0;                                                                        \
    }

#define HANDLE_ACTION_IF(symbol, cond, action)                                           \
    if (sym == symbol) {                                                                 \
        if (cond) {                                                                      \
            /* for style, require any side-actions to be performed via comma operator */ \
            (action);                                                                    \
        }                                                                                \
        return 0;                                                                        \
    }

#define HANDLE_ACTION_IF_ELSE(symbol, cond, action_true, action_false)                   \
    if (sym == symbol) {                                                                 \
        if (cond) {                                                                      \
            /* for style, require any side-actions to be performed via comma operator */ \
            (action_true);                                                               \
        } else {                                                                         \
            (action_false);                                                              \
        }                                                                                \
        return 0;                                                                        \
    }

#endif // RB3_HANDLE_LOCAL_STATIC

#define HANDLE_CONDITION(cond, expr)                                                     \
    if (cond)                                                                            \
        return expr;

#define HANDLE_MESSAGE(msg)                                                              \
    if (sym == msg::Type())                                                              \
    _HANDLE_CHECKED(OnMsg(msg(_msg)))

#define _NEW_STATIC_SYMBOL(str) static Symbol _s(#str);

// -----------------------------------------------------------------------------------
// HANDLE_*_STATIC -- the gate-independent spelling of the local-static dialect.
//
// These MUST be written per gate state. Under RB3_HANDLE_LOCAL_STATIC the plain
// HANDLE family ALREADY stringizes its argument into a function-local
// `static Symbol _hs(#symbol)`, so the _STATIC forms must forward the *name*
// rather than wrap it. Wrapping under the gate expands to
//
//     { static Symbol _s("update_char_cache");        // emitted, never compared
//       { static Symbol _hs("_s"); if (sym == _hs) {...} } }
//
// i.e. the handler compares against Symbol("_s") and becomes UNREACHABLE. That
// compiles, links, and is INVISIBLE to the match metric -- the only difference is
// the string relocation argument, which objdiff's match_percent_normalized masks
// (see CLAUDE.md, "Reloc args are SCORE-INVISIBLE"). Verified by preprocessing
// band3/meta_band/CharSync.cpp under both gate states (lane DN-3, 2026-08-03).
//
// Writing them gate-aware makes _STATIC unconditionally correct, so a file using
// this spelling can be scatter-#included into a gated TU (the active
// COMDAT-scatter workflow does exactly this) without silently miscompiling.
// Δ0 by construction at the time of writing: no gated object's include closure
// contained a _STATIC use, so no expansion changes.
// -----------------------------------------------------------------------------------
#ifdef RB3_HANDLE_LOCAL_STATIC

#define HANDLE_STATIC(symbol, func) HANDLE(symbol, func)
#define HANDLE_EXPR_STATIC(symbol, expr) HANDLE_EXPR(symbol, expr)
#define HANDLE_ACTION_STATIC(symbol, expr) HANDLE_ACTION(symbol, expr)

#else

#define HANDLE_STATIC(sym, func)                                                         \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(sym)                                                          \
        HANDLE(_s, func);                                                                \
    }

#define HANDLE_EXPR_STATIC(symbol, expr)                                                 \
    { _NEW_STATIC_SYMBOL(symbol) HANDLE_EXPR(_s, expr) }

#define HANDLE_ACTION_STATIC(symbol, expr)                                               \
    { _NEW_STATIC_SYMBOL(symbol) HANDLE_ACTION(_s, expr) }

#endif // RB3_HANDLE_LOCAL_STATIC

#define HANDLE_METHOD(func) _HANDLE_CHECKED(func(_msg))

#define HANDLE_FORWARD(func) _HANDLE_CHECKED(func(_msg, false))

#define HANDLE_SUPERCLASS(parent) HANDLE_FORWARD(parent::Handle)

#define HANDLE_VIRTUAL_SUPERCLASS(parent)                                                \
    if (ClassName() == StaticClassName())                                                \
    HANDLE_SUPERCLASS(parent)

#define HANDLE_MEMBER(member) HANDLE_FORWARD(member.Handle)

#define HANDLE_MEMBER_PTR(member)                                                        \
    if (member)                                                                          \
    HANDLE_FORWARD(member->Handle)

#define HANDLE_ARRAY(array)                                                              \
    {                                                                                    \
        /* this needs to be placed up here to match Hmx::Object::Handle */               \
        DataArray *found;                                                                \
        if (array && (found = array->FindArray(sym, false))) {                           \
            _HANDLE_CHECKED(found->ExecuteScript(1, this, _msg, 2))                      \
        }                                                                                \
    }

#define HANDLE_LOG(...)                                                                  \
    if (!_warn)                                                                          \
        MILO_LOG(__VA_ARGS__);

#define HANDLE_LOG_IF(cond, ...)                                                         \
    if ((cond) && !_warn)                                                                \
        MILO_LOG(__VA_ARGS__);

#define HANDLE_WARN(...)                                                                 \
    if (_warn)                                                                           \
        MILO_WARN(__VA_ARGS__);

#define HANDLE_WARN_IF(cond, ...)                                                        \
    if ((cond) && _warn)                                                                 \
        MILO_WARN(__VA_ARGS__);

// Retail RB3 (rb3-Wii release Debug.h:151) compiled MILO_WARN as the comma form
// `(void)(__VA_ARGS__)` — message string dropped, but the PathName(this) vcall
// argument is still EVALUATED for its side effect. Our global MILO_WARN is the
// `((void)sizeof(...))` form, which strips ALL arg evaluation (worth +23 on
// WARN/NOTIFY-heavy TUs, so we keep it globally). But HANDLE_CHECK's _warn arm
// specifically needs the surviving PathName(this) vcall to byte-match Family-A's
// Handle tail (proven on GuitarController idx 83-87: clrlwi./beq/PathName/li 6).
// Emit the comma form here directly so the vcall survives without touching the
// global MILO_WARN. The native port keeps the real warner.
#ifndef HX_NATIVE
#define HANDLE_CHECK(line_num)                                                           \
    if (_warn)                                                                           \
        ((void)("%s(%d): %s unhandled msg: %s", __FILE__, line_num, PathName(this), sym  \
        ));
#else
#define HANDLE_CHECK(line_num)                                                           \
    if (_warn)                                                                           \
        MILO_WARN(                                                                       \
            "%s(%d): %s unhandled msg: %s", __FILE__, line_num, PathName(this), sym      \
        );
#endif

#define END_HANDLERS                                                                     \
    return DataNode(kDataUnhandled, 0);                                                  \
    }

// END HANDLE MACROS
// -----------------------------------------------------------------------------------

// BEGIN SYNCPROPERTY MACROS
// ---------------------------------------------------------------------------

#define BEGIN_PROPSYNCS(objType)                                                         \
    bool objType::SyncProperty(DataNode &_val, DataArray *_prop, int _i, PropOp _op) {   \
        if (_i == _prop->Size())                                                         \
            return true;                                                                 \
        else {                                                                           \
            Symbol sym = _prop->Sym(_i);

#define BEGIN_CUSTOM_PROPSYNC(objType)                                                   \
    bool PropSync(objType &o, DataNode &_val, DataArray *_prop, int _i, PropOp _op) {    \
        if (_i == _prop->Size())                                                         \
            return true;                                                                 \
        else {                                                                           \
            Symbol sym = _prop->Sym(_i);

// -----------------------------------------------------------------------------------
// LOCAL-STATIC SYNC_PROP variant (RB3 retail codegen lever) -- exact analogue of
// RB3_HANDLE_LOCAL_STATIC above, for BEGIN_PROPSYNCS bodies.
//
// Retail band3 constructs each property-dispatch Symbol as a FUNCTION-LOCAL STATIC
// (`static Symbol _ps("name")`): MSVC emits a guard-bit test + inline Symbol ctor,
// packing one guard word / one bit per property in source order.  Our DC3-era
// sources reference centralized GLOBAL Symbols (Symbols2/3/4.h), so a SyncProperty
// with N props emits N plain global compares and structurally diverges (target-only
// lis/lwz/clrlwi. guard machinery + ??0Symbol@@QAA@PBD@Z calls we never emit).
//
// Measured signature: target obj has K relocations to ??0Symbol@@QAA@PBD@Z inside
// the SyncProperty COMDAT, ours has 0 (scanner: see laneBF localstatic scan).
//
// Enable PER-TU with `/DRB3_SYNCPROP_LOCAL_STATIC` in the object's extra_cflags so
// no other TU's codegen moves.
// -----------------------------------------------------------------------------------
#ifdef RB3_SYNCPROP_LOCAL_STATIC

#define SYNC_PROP(symbol, member)                                                        \
    {                                                                                    \
        static Symbol _ps(#symbol);                                                      \
        if (sym == _ps)                                                                  \
            return PropSync(member, _val, _prop, _i + 1, _op);                           \
    }

#define SYNC_PROP_SET(symbol, member, func)                                              \
    {                                                                                    \
        static Symbol _ps(#symbol);                                                      \
        if (sym == _ps) {                                                                \
            if (_op == kPropSet) {                                                       \
                func;                                                                    \
            } else {                                                                     \
                if (_op == (PropOp)0x40)                                                 \
                    return false;                                                        \
                _val = DataNode(member);                                                 \
            }                                                                            \
            return true;                                                                 \
        }                                                                                \
    }

#define SYNC_PROP_MODIFY(symbol, member, func)                                           \
    {                                                                                    \
        static Symbol _ps(#symbol);                                                      \
        if (sym == _ps) {                                                                \
            bool synced = PropSync(member, _val, _prop, _i + 1, _op);                    \
            if (!synced)                                                                 \
                return false;                                                            \
            else {                                                                       \
                if (!(_op & (kPropSize | kPropGet))) {                                   \
                    func;                                                                \
                }                                                                        \
                return true;                                                             \
            }                                                                            \
        }                                                                                \
    }

#define SYNC_PROP_MODIFY_ALT(symbol, member, func)                                       \
    {                                                                                    \
        static Symbol _ps(#symbol);                                                      \
        if (sym == _ps) {                                                                \
            bool synced = PropSync(member, _val, _prop, _i + 1, _op);                    \
            if (synced) {                                                                \
                if (!(_op & (kPropSize | kPropGet))) {                                   \
                    func;                                                                \
                }                                                                        \
                return true;                                                             \
            } else                                                                       \
                return false;                                                            \
        }                                                                                \
    }

#else

#define SYNC_PROP(symbol, member)                                                        \
    if (sym == symbol)                                                                   \
        return PropSync(member, _val, _prop, _i + 1, _op);

// TODO: make specific sync macros for objects and bitfields?

// for propsyncs that do something extra if the prop op is specifically kPropSet
#define SYNC_PROP_SET(symbol, member, func)                                              \
    if (sym == symbol) {                                                                 \
        if (_op == kPropSet) {                                                           \
            func;                                                                        \
        } else {                                                                         \
            if (_op == (PropOp)0x40)                                                     \
                return false;                                                            \
            _val = DataNode(member);                                                     \
        }                                                                                \
        return true;                                                                     \
    }

// for propsyncs that do NOT use size or get - aka, any combo of set, insert, remove, and
// handle is used
#define SYNC_PROP_MODIFY(symbol, member, func)                                           \
    if (sym == symbol) {                                                                 \
        bool synced = PropSync(member, _val, _prop, _i + 1, _op);                        \
        if (!synced)                                                                     \
            return false;                                                                \
        else {                                                                           \
            if (!(_op & (kPropSize | kPropGet))) {                                       \
                func;                                                                    \
            }                                                                            \
            return true;                                                                 \
        }                                                                                \
    }

// for SYNC_PROP_MODIFY uses where the condition order is flipped
// if you know how to make this macro and SYNC_PROP_MODIFY into one singular macro,
// while still matching every instance of SYNC_PROP_MODIFY being used regardless of
// condition order, by all means please do so, because idk how to do it here
#define SYNC_PROP_MODIFY_ALT(symbol, member, func)                                       \
    if (sym == symbol) {                                                                 \
        bool synced = PropSync(member, _val, _prop, _i + 1, _op);                        \
        if (synced) {                                                                    \
            if (!(_op & (kPropSize | kPropGet))) {                                       \
                func;                                                                    \
            }                                                                            \
            return true;                                                                 \
        } else                                                                           \
            return false;                                                                \
    }

#endif // RB3_SYNCPROP_LOCAL_STATIC

// Body of the bitfield property arm, parameterized on the Symbol to compare against so
// that both the gated (function-local static) and ungated (centralized global) spellings
// below can share it.  See the RB3_SYNCPROP_LOCAL_STATIC block above for the rationale.
#define _OM_SYNC_PROP_BITFIELD(symbol, mask_member, line_num)                            \
    if (sym == symbol) {                                                                 \
        _i++;                                                                            \
        if (_i < _prop->Size()) {                                                        \
            DataNode &node = _prop->Node(_i);                                            \
            int res = 0;                                                                 \
            switch (node.Type()) {                                                       \
            case kDataInt:                                                               \
                res = node.Int();                                                        \
                break;                                                                   \
            case kDataSymbol: {                                                          \
                const char *bitstr = node.Sym().Str();                                   \
                MILO_ASSERT_FMT(                                                         \
                    strncmp("BIT_", bitstr, 4) == 0,                                     \
                    "%s does not begin with BIT_",                                       \
                    bitstr                                                               \
                );                                                                       \
                Symbol bitsym(bitstr + 4);                                               \
                const Symbol &test = Symbol(bitsym);                                     \
                DataArray *macro = DataGetMacro(test);                                   \
                MILO_ASSERT_FMT(                                                         \
                    macro, "PROPERTY_BITFIELD %s could not find macro %s", symbol, test  \
                );                                                                       \
                res = macro->Int(0);                                                     \
                break;                                                                   \
            }                                                                            \
            default:                                                                     \
                MILO_ASSERT(0, line_num);                                                 \
                break;                                                                   \
            }                                                                            \
            MILO_ASSERT(_op <= kPropInsert, line_num);                                      \
            if (_op == kPropGet) {                                                       \
                int final = mask_member & res;                                           \
                _val = DataNode(final > 0);                                              \
            } else {                                                                     \
                if (_val.Int() != 0)                                                     \
                    mask_member |= res;                                                  \
                else                                                                     \
                    mask_member &= ~res;                                                 \
            }                                                                            \
            return true;                                                                 \
        } else                                                                           \
            return PropSync(mask_member, _val, _prop, _i, _op);                          \
    }

#ifdef RB3_SYNCPROP_LOCAL_STATIC

// SYNC_PROP_BITFIELD belongs to the same family as SYNC_PROP / _SET / _MODIFY / _MODIFY_ALT
// and must follow them into the gate: a gated TU that kept the global spelling here would
// emit no guard word / no Symbol ctor for that one property and so break the guard-BIT
// SEQUENCE (MSVC packs one bit per function-local static, in source order) for every
// property declared after it.
#define SYNC_PROP_BITFIELD(symbol, mask_member, line_num)                                \
    {                                                                                    \
        static Symbol _ps(#symbol);                                                      \
        _OM_SYNC_PROP_BITFIELD(_ps, mask_member, line_num)                               \
    }

// Under the gate the plain macros ALREADY build their own function-local static from the
// stringized property name, so the *_STATIC wrappers must not wrap a second time:
// `{ _NEW_STATIC_SYMBOL(symbol) SYNC_PROP(_s, member) }` would expand to
// `static Symbol _ps("_s")` and compare `sym` against the literal Symbol "_s" -- silently
// wrong for every property.  Collapse them to the plain (already-local-static) forms.
#define SYNC_PROP_STATIC(symbol, member) SYNC_PROP(symbol, member)

#define SYNC_PROP_SET_STATIC(symbol, member, func) SYNC_PROP_SET(symbol, member, func)

#define SYNC_PROP_MODIFY_STATIC(symbol, member, func) SYNC_PROP_MODIFY(symbol, member, func)

#define SYNC_PROP_BITFIELD_STATIC(symbol, mask_member, line_num)                         \
    SYNC_PROP_BITFIELD(symbol, mask_member, line_num)

#else

#define SYNC_PROP_BITFIELD(symbol, mask_member, line_num)                                \
    _OM_SYNC_PROP_BITFIELD(symbol, mask_member, line_num)

#define SYNC_PROP_STATIC(symbol, member)                                                 \
    { _NEW_STATIC_SYMBOL(symbol) SYNC_PROP(_s, member) }

#define SYNC_PROP_SET_STATIC(symbol, member, func)                                       \
    { _NEW_STATIC_SYMBOL(symbol) SYNC_PROP_SET(_s, member, func) }

#define SYNC_PROP_MODIFY_STATIC(symbol, member, func)                                    \
    { _NEW_STATIC_SYMBOL(symbol) SYNC_PROP_MODIFY(_s, member, func) }

#define SYNC_PROP_BITFIELD_STATIC(symbol, mask_member, line_num)                         \
    { _NEW_STATIC_SYMBOL(symbol) SYNC_PROP_BITFIELD(_s, mask_member, line_num) }

#endif // RB3_SYNCPROP_LOCAL_STATIC

#define SYNC_SUPERCLASS(parent)                                                          \
    if (parent::SyncProperty(_val, _prop, _i, _op))                                      \
        return true;

#define END_PROPSYNCS                                                                    \
    return false;                                                                        \
    }                                                                                    \
    }

#define END_CUSTOM_PROPSYNC                                                              \
    return false;                                                                        \
    }                                                                                    \
    }

// END SYNCPROPERTY MACROS
// -----------------------------------------------------------------------------

// BEGIN SAVE MACRO
// ------------------------------------------------------------------------------------

#define SAVE_OBJ(objType, line_num)                                                      \
    void objType::Save(BinStream &) { MILO_ASSERT(0, line_num); }

// END SAVE MACRO
// --------------------------------------------------------------------------------------

// BEGIN COPY MACROS
// -----------------------------------------------------------------------------------

#define BEGIN_COPYS(objType)                                                             \
    void objType::Copy(const Hmx::Object *o, Hmx::Object::CopyType ty) {
#define COPY_SUPERCLASS(parent) parent::Copy(o, ty);

#define COPY_VIRTUAL_SUPERCLASS(parent)                                                  \
    if (ClassName() == StaticClassName())                                                \
    COPY_SUPERCLASS(Hmx::Object)

#define COPY_SUPERCLASS_FROM(parent, obj) parent::Copy(obj, ty);

#define CREATE_COPY(objType) const objType *c = dynamic_cast<const objType *>(o);

// copy macro where you specify the variable name (used in asserts in some copy methods)
#define CREATE_COPY_AS(objType, var_name)                                                \
    const objType *var_name = dynamic_cast<const objType *>(o);

#define BEGIN_COPYING_MEMBERS if (c) {
// copy macro where you specify the variable name (used in asserts in some copy methods)
#define BEGIN_COPYING_MEMBERS_FROM(copy_name) if (copy_name) {
#define COPY_MEMBER(mem) mem = c->mem;

// copy macro where you specify the variable name (used in asserts in some copy methods)
#define COPY_MEMBER_FROM(copy_name, member) member = copy_name->member;

#define END_COPYING_MEMBERS }

#define END_COPYS }

// END COPY MACROS
// -------------------------------------------------------------------------------------

// BEGIN LOAD MACROS
// -----------------------------------------------------------------------------------

#define DECLARE_REVS                                                                     \
    static unsigned short gRev;                                                          \
    static unsigned short gAltRev;

#define INIT_REVS(objType)                                                               \
    unsigned short objType::gRev = 0;                                                    \
    unsigned short objType::gAltRev = 0;

#define BEGIN_LOADS(objType) void objType::Load(BinStream &bs) {
#define LOAD_REVS(bs)                                                                    \
    int rev;                                                                             \
    bs >> rev;                                                                           \
    gRev = getHmxRev(rev);                                                               \
    gAltRev = getAltRev(rev);

#define ASSERT_REV(ver)                                                                  \
    if ((ver == 0) ? (gRev != ver) : (gRev > ver)) {                                     \
        MILO_FAIL(                                                                       \
            "%s can't load new %s version %d > %d",                                      \
            PathName(this),                                                              \
            ClassName(),                                                                 \
            gRev,                                                                        \
            (unsigned short)ver                                                          \
        );                                                                               \
    }

#define ASSERT_ALTREV(ver)                                                               \
    if ((ver == 0) ? (gAltRev != ver) : (gAltRev > ver)) {                               \
        MILO_FAIL(                                                                       \
            "%s can't load new %s alt version %d > %d",                                  \
            PathName(this),                                                              \
            ClassName(),                                                                 \
            gAltRev,                                                                     \
            (unsigned short)ver                                                          \
        );                                                                               \
    }

#ifdef VERSION_SZBE69_B8
#define ASSERT_REVS(rev1, rev2)                                                          \
    ASSERT_REV(rev1)                                                                     \
    ASSERT_ALTREV(rev2)
#else
#define ASSERT_REVS(rev1, rev2)
#endif

// for loading in a version number that isn't a class's gRev/gAltRev
#ifdef VERSION_SZBE69_B8
#define ASSERT_GLOBAL_REV(ver, rev_name)                                                 \
    if (ver > rev_name) {                                                                \
        MILO_FAIL(                                                                       \
            "%s can't load new %s version %d > %d",                                      \
            PathName(this),                                                              \
            ClassName(),                                                                 \
            ver,                                                                         \
            rev_name                                                                     \
        );                                                                               \
    }

#define ASSERT_OLD_REV(ver)                                                              \
    if (gRev < ver) {                                                                    \
        MILO_FAIL(                                                                       \
            "%s can't load old %s version %d < %d.  Use RB2 Milo to load.",              \
            PathName(this),                                                              \
            ClassName(),                                                                 \
            gRev,                                                                        \
            ver                                                                          \
        );                                                                               \
    }

#define ASSERT_OLD_ALTREV(ver)                                                           \
    if (gRev < ver) {                                                                    \
        MILO_FAIL(                                                                       \
            "%s can't load old %s alt version %d < %d.  Use RB2 Milo to load.",          \
            PathName(this),                                                              \
            ClassName(),                                                                 \
            gAltRev,                                                                     \
            ver                                                                          \
        );                                                                               \
    }
#else
#define ASSERT_GLOBAL_REV(ver, rev_name)
#define ASSERT_OLD_REV(ver)
#define ASSERT_OLD_ALTREV(ver)
#endif

#define LOAD_SUPERCLASS(parent) parent::Load(bs);

#define LOAD_BITFIELD(type, name)                                                        \
    {                                                                                    \
        type bs_name;                                                                    \
        bs >> bs_name;                                                                   \
        name = bs_name;                                                                  \
    }

#define LOAD_BITFIELD_ENUM(type, name, enum_name)                                        \
    {                                                                                    \
        type bs_name;                                                                    \
        bs >> bs_name;                                                                   \
        name = (enum_name)bs_name;                                                       \
    }

#define END_LOADS }

// END LOAD MACROS
// -------------------------------------------------------------------------------------

// BEGIN OBJ INITIALIZER MACROS
// ------------------------------------------------------------------------

// ⚠ LANE ACTIONABLE-1 (2026-08-14) — NEW_OBJ IS WRONG FOR RETAIL, AND IT IS A
// SIZED FORCE-MULTIPLIER, NOT A ONE-ROW FIX. Left unchanged deliberately: the
// blast radius is every class that uses NEW_OBJ/NEW_OVERLOAD, so it needs its
// own lane with a whole-binary A/B, not a tail-lane edit.
//
// Evidence, from ?NewObject@LayerDir@@SAPAVObject@Hmx@@XZ (112 B, fuzzy 86.929).
// Retail:
//     addi r3, r31, 0x50
//     bl   ?StaticClassName@LayerDir@@SA?AVSymbol@@XZ   ; Symbol -> temp @0x50
//     li   r4, 0x0
//     li   r3, 0x224
//     bl   fn_827BCD38                                  ; INLINE 2-arg allocator
//     stw  r3, 0x54, r31
// Ours:
//     li   r3, 0x224
//     bl   ??2LayerDir@@SAPAXI@Z                        ; out-of-line, 5-arg
//     stw  r3, 0x50, r31
//
// Two independent defects:
//  1. NEW_OBJ must evaluate `objType::StaticClassName()` and pass it BY VALUE to
//     an operator new overload. The resulting Symbol temp is written to 0x50 and
//     NEVER READ, which is what an inlined `operator new(size_t, Symbol)` that
//     ignores its name argument looks like. (It is NOT a MemTemp guard — see the
//     note in utl/MemMgr.h: retail's MemTemp call sites pass NO argument regs.)
//  2. NEW_OVERLOAD (utl/MemMgr.h) is `__declspec(noinline)` and calls the 5-arg
//     `MemAlloc(s, __FILE__, 0, "unknown", 0)`. Retail INLINES it and calls the
//     2-arg form `MemAlloc(size, 0)` — the same "no __FILE__/__LINE__/name"
//     retail X360 shape that MemMgr.h already documents for POOL_OVERLOAD.
//     fn_827BCD38 is 0x284 B with 300 call sites incl. MemHeap/Memory_Xbox, i.e.
//     the global allocator.
//
// The `stw` offset delta (0x54 vs 0x50) is a CONSEQUENCE of the Symbol temp
// occupying 0x50 — not a third defect.
//
// Immediate prize: 4 rows at exactly 112 B and exactly fuzzy 86.929
// (BandRetargetVignette, OverdriveMeter, UnisonIcon, LayerDir) plus
// PropertyEventProvider at 96 B = 544 B. True scope is larger: every
// REGISTER_OBJ_FACTORY class routes through here. Neither oracle can adjudicate
// it — DC3 and rb3-Wii BOTH spell plain `new objType`, and rb3-Wii is the dev
// build; only retail's bytes disagree.
#define NEW_OBJ(objType)                                                                 \
    static Hmx::Object *NewObject() { return new objType; }

#define REGISTER_OBJ_FACTORY(objType)                                                    \
    Hmx::Object::RegisterFactory(objType::StaticClassName(), objType::NewObject);

#define REGISTER_OBJ_FACTORY_FUNC(objType)                                               \
    static void Register() { REGISTER_OBJ_FACTORY(objType) }

// END OBJ INITIALIZER MACROS
// --------------------------------------------------------------------------

// BEGIN OBJREF ITERATION MACROS
// -----------------------------------------------------------------------

#define FOREACH_OBJREF(it, obj)                                                          \
    std::vector<ObjRef *>::const_reverse_iterator it = obj->Refs().rbegin();             \
    std::vector<ObjRef *>::const_reverse_iterator it##End = obj->Refs().rend();          \
    for (; it != it##End; ++it)

#define FOREACH_OBJREF_POST(it, obj)                                                     \
    std::vector<ObjRef *>::const_reverse_iterator it = obj->Refs().rbegin();             \
    std::vector<ObjRef *>::const_reverse_iterator it##End = obj->Refs().rend();          \
    for (; it != it##End; it++)

// END OBJREF ITERATION MACROS
// -------------------------------------------------------------------------
