#ifndef BANDOBJ_STARDISPLAY_H
#define BANDOBJ_STARDISPLAY_H

#include "bandobj/BandLabel.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "rndobj/Text.h"
#include "ui/UIComponent.h"
#include "utl/Symbol.h"

class StarDisplay : public UIComponent {
public:
    StarDisplay();
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual ~StarDisplay();
    OBJ_CLASSNAME(StarDisplay)
    OBJ_SET_TYPE(StarDisplay)
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void CopyMembers(const UIComponent *, Hmx::Object::CopyType);
    virtual void Save(BinStream &);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void Update();
    virtual void Enter();

    void SetValues(int, int);
    void SetToToken(Symbol);
    void UpdateDisplay();
    void DrawShowing();
    void SetForceMixedMode(bool);
    void SetShowDenominator(bool b);
    void SetAlignment(RndText::Alignment);
    char GetStarIcon() const;
    char GetEmptyStarIcon() const;

    bool HasStarIcon() const;

    static Symbol GetSymbolForStarCount(int);
    static int GetStarCountForSymbol(Symbol);
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(StarDisplay); }

    // Retail's NewObject at 0x8231d220 calls ?StaticClassName@StarDisplay@@,
    // not @UIComponent: retail gives StarDisplay its OWN class operator new, so
    // the inlined StaticClassName() binds in StarDisplay's scope. Inheriting
    // UIComponent's left our only charged site as that one relocation name
    // (25/28 words equal, fuzzy 99.821).
    //
    // operator new ONLY -- deliberately NOT OBJ_MEM_OVERLOAD*, which would also
    // declare operator delete. MEASURED (lane W16-BE, 2026-09-15): adding
    // OBJ_MEM_OVERLOAD_INLINE_DEL here won NewObject (+112 B) but LOST the
    // NewObject unwind funclet fn_8231D290 (-40 B, fuzzy 100 -> below), because
    // a StarDisplay-owned inlinable delete gets inlined into the funclet as
    // `bl ?MemFree@@YAXPAX@Z` where retail calls the out-of-line ICF survivor
    // ??3BinStream@@SAXPAX@Z. Leaving delete inherited from UIComponent keeps
    // both that funclet and ??_GStarDisplay at 100 and still wins NewObject.
    // The body form is MemMgr.h's OBJ_MEM_OVERLOAD verbatim: `.Str()` on the
    // temp plus a named `mem` local is what homes the Symbol at 0x50 and the
    // pointer separately at 0x54, as retail does.
#ifdef HX_NATIVE
    // NATIVE (measured: lane W16-BE's first native_build_gate.sh run FAILED here,
    // rc=1, `'operator new' takes type size_t ('unsigned long') as 1st parameter`
    // + `too few arguments to function call, expected at least 4, have 2`). On
    // LP64 `operator new` must take size_t, and MemAlloc is the 5-arg debug form
    // -- the 2-arg `(MemAlloc)(s, 0)` spelling exists only for the match build.
    // This mirrors MemMgr.h's own #ifdef HX_NATIVE OBJ_MEM_OVERLOAD verbatim; we
    // cannot just USE that macro because it also declares operator delete, which
    // costs a funclet row (see the note above). MemMgr.h is another lane's file.
    static void *operator new(size_t s) {
        return MemAlloc(s, __FILE__, 0x3e, StaticClassName().Str(), 0);
    }
    static void *operator new(size_t s, void *place) { return place; }
#else
    static void *operator new(unsigned int s) {
        (void)StaticClassName().Str();
        void *mem = (MemAlloc)(s, 0);
        return mem;
    }
    static void *operator new(unsigned int s, void *place) { return place; }
#endif

    NEW_OBJ(StarDisplay)

    BandLabel *mRsrcStarsLabel; // 0x140
    BandLabel *mRsrcStarsMixedLabel; // 0x144
    bool mForceMixedMode; // 0x148
    bool mShowDenominator; // 0x149
    bool mShowEmptyStars; // 0x14a
    int mStars; // 0x14c
    int mTotalStars; // 0x150
    RndText::Alignment mAlignment; // 0x154
    Symbol mIconOverride; // 0x158
    Symbol mEmptyIconOverride; // 0x15c
};

#endif // BANDOBJ_STARDISPLAY_H
