#pragma once
#include "bandobj/BandLabel.h"
#include "obj/ObjMacros.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "ui/UIColor.h"
#include "ui/UIComponent.h"
#include "ui/UIList.h"
#include "utl/Symbol.h"

class PlayerDiffIcon : public UIComponent, public UIListCustomTemplate {
public:
    PlayerDiffIcon();
    OBJ_CLASSNAME(PlayerDiffIcon);
    OBJ_SET_TYPE(PlayerDiffIcon);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void DrawShowing();
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual int CollidePlane(const Plane &);
    virtual ~PlayerDiffIcon();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void Update();
    virtual void SetAlphaColor(float, UIColor *);
    virtual void GrowBoundingBox(Box &) const;

    void SetNumPlayersDiff(int, int);

    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(PlayerDiffIcon); }
    NEW_OBJ(PlayerDiffIcon);

    DECLARE_REVS;

    // Retail INLINES this class's operator new into NewObject and still
    // EVALUATES the allocation-name argument before calling the debug-stripped
    // 2-arg allocator (target: `bl StaticClassName` then `li r4,0; li r3,0x1b8;
    // bl MemAlloc`). Our `MemAlloc` macro drops the name expression outright and
    // both NEW_OVERLOAD and OBJ_MEM_OVERLOAD are `__declspec(noinline)`, so
    // neither reproduces that shape -- spell it out here.
    static void *operator new(unsigned int s) {
        // The `.Str()` call is load-bearing (see OBJ_MEM_OVERLOAD in MemMgr.h):
        // a bare `StaticClassName();` discard homes the Symbol temp into the
        // SAME slot as `mem`, while calling `.Str()` on it reproduces retail's
        // separate-slot placement (temp @0x50, result @0x54). Keep both the
        // `.Str()` and the named `mem` local.
        (void)StaticClassName().Str();
        void *mem = (MemAlloc)(s, 0);
        return mem;
    }
    static void *operator new(unsigned int s, void *place) { return place; }
    static void operator delete(void *v) { (MemFree)(v); }

    std::vector<RndMesh *> mPlayerMeshes;
    RndMat *mPlayerMat;
    RndMat *mNoPlayerMat;
    std::vector<BandLabel *> mDiffLabels;
    int mNumPlayers;
    int mDiff;
    float mAlpha;
    Hmx::Color mColor;
};
