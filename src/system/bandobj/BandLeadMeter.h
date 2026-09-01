#pragma once
// Ported from rb3-Wii src/system/bandobj/BandLeadMeter.h (MWCC -> MSVC X360).
#include "rndobj/Dir.h"
#include "rndobj/Mesh.h"
#include "rndobj/Mat.h"
#include "rndobj/Anim.h"
#include "obj/ObjMacros.h"

class BandLeadMeter : public RndDir {
public:
    BandLeadMeter();
    OBJ_CLASSNAME(BandLeadMeter);
    OBJ_SET_TYPE(BandLeadMeter);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual ~BandLeadMeter() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SetFrame(float, float) {}
    virtual void Poll();
    virtual void Enter();

    int GetColor(int);
    void SyncScores();

    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(BandLeadMeter)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(BandLeadMeter)

    ObjPtr<RndAnimatable> mNeedleAnim; // 0x1dc
    ObjPtr<RndAnimatable> mLogoGlowAnim; // 0x1e8
    ObjPtr<RndMesh> mGlowMesh1; // 0x1f4
    ObjPtr<RndMesh> mGlowMesh2; // 0x200
    ObjPtr<RndAnimatable> mPeggedAnim1; // 0x20c
    ObjPtr<RndAnimatable> mPeggedAnim2; // 0x218
    ObjPtr<RndMesh> mLensMesh; // 0x224
    ObjPtr<RndMat> mLensMatNeutral; // 0x230
    ObjPtr<RndMat> mLensMat1; // 0x23c
    ObjPtr<RndMat> mLensMat2; // 0x248
    int unk204; // 0x254
    int mScoreDiff; // 0x258
};
