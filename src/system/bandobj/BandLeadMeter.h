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

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(BandLeadMeter)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(BandLeadMeter)

    ObjPtr<RndAnimatable> mNeedleAnim; // 0x18c
    ObjPtr<RndAnimatable> mLogoGlowAnim; // 0x198
    ObjPtr<RndMesh> mGlowMesh1; // 0x1a4
    ObjPtr<RndMesh> mGlowMesh2; // 0x1b0
    ObjPtr<RndAnimatable> mPeggedAnim1; // 0x1bc
    ObjPtr<RndAnimatable> mPeggedAnim2; // 0x1c8
    ObjPtr<RndMesh> mLensMesh; // 0x1d4
    ObjPtr<RndMat> mLensMatNeutral; // 0x1e0
    ObjPtr<RndMat> mLensMat1; // 0x1ec
    ObjPtr<RndMat> mLensMat2; // 0x1f8
    int unk204; // 0x204
    int mScoreDiff; // 0x208
};
