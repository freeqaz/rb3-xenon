#pragma once
#include "math/Color.h"
#include "obj/Object.h"
#include "obj/Object.h"
#include "rndobj/Tex.h"
#include "rndobj/Wind.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

// size 0x9c
/** "Parameters for fur shading, to be set on a material" */
class RndFur : public Hmx::Object {
public:
    OBJ_CLASSNAME(Fur);
    OBJ_SET_TYPE(Fur);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // The two virtuals RndFur INTRODUCES, at trailing vtable slots [21] and
    // [22].  Retail ??_7RndFur@@6B@ (0x8206c0bc) has 23 slots -- slot[23] is
    // the 0xffffffff sentinel, so the table ends there -- while our base
    // Hmx::Object supplies 21, so these two were simply dropped in the port.
    // Both retail slots hold 0x823591e8, the shared `li r3,0; blr` hub, which
    // is what `{ return false; }` folds to.
    // ORDER (the hub is the SAME address in both slots, so RndFur's own table
    // cannot discriminate it) is settled on the derived class: retail
    // ??_7NgFur@@6B@ (0x8219bed4) carries real bodies at the same two slots,
    // [21] = 0x82b8b2e8 and [22] = 0x82b8b340.  Read non-circularly from the
    // bodies rather than from their map names: 0x82b8b340 saves FOUR incoming
    // registers (this + 3 params), does `cmpwi cr6,r4,0` -- treating r4 as an
    // int -- and reads mLayers at 0x28, so it is Shell(int, ...); 0x82b8b2e8
    // never consumes r4 as input (it overwrites it with `li r4,0xc`), so it is
    // Prep(RndMesh*, RndMat*).  Hence Prep first, Shell second.
    // NgFur (rndobj/Fur_NG.h) already declared both with these exact
    // signatures; without them here they were NEW virtuals on NgFur instead of
    // overrides, so no dispatch through an RndFur* could ever reach them.
    virtual bool Prep(class RndMesh *, class RndMat *) const { return false; }
    virtual bool Shell(int, class RndMesh *, class RndMat *) const {
        return false;
    }

    bool LoadOld(BinStreamRev &);
    RndTex* GetFurDetail() const { return mFurDetail; }

    OBJ_MEM_OVERLOAD(0x1A)
    NEW_OBJ(RndFur)
    static void Init() { REGISTER_OBJ_FACTORY(RndFur); }

protected:
    RndFur();

    /** "Number of passes" */
    int mLayers; // 0x28
    /** "Length of fur" */
    float mThickness; // 0x2c
    /** "Curvature exponent". Ranges from 0 to 3. */
    float mCurvature; // 0x30
    /** "Bunch shells towards surface". Ranges from 0 to 1. */
    float mShellOut; // 0x34
    /** "Bunch opacity towards surface". Ranges from 0 to 1. */
    float mAlphaFalloff; // 0x38
    /** "Maximum stretch" */
    float mStretch; // 0x3c
    /** "Maximum lateral motion" */
    float mSlide; // 0x40
    /** "Strength of gravity". Ranges from 0 to 1. */
    float mGravity; // 0x44
    /** "Langor of motion". Ranges from 0 to 1. */
    float mFluidity; // 0x48
    /** "Tint at hair roots" */
    Hmx::Color mRootsTint; // 0x4c
    /** "Tint at hair ends" */
    Hmx::Color mEndsTint; // 0x5c
    /** "Detail map for finer fur.  Only the alpha channel is used." */
    ObjPtr<RndTex> mFurDetail; // 0x6c
    /** "Tiling for fur detail map.
        UVs of fur_detail are multiplied by this value."
        Ranges from 2.0e-2 to 100.
    */
    float mFurTiling; // 0x78
    /** "Wind Object, if set, blows on the fur." */
    ObjPtr<RndWind> mWind; // 0x7c
};
