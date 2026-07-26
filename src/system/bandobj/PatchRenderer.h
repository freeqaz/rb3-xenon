#pragma once
// Ported from the rb3-Wii oracle (../rb3/src/system/bandobj/PatchRenderer.h),
// adapted to rb3-xenon's single-argument ObjPtr<>. Declaration only: retail
// scattered this class's OBJ_CLASSNAME COMDAT into the BandSwatch .text span,
// which is why BandSwatch.cpp force-emits StaticClassName below.
#include "rndobj/TexRenderer.h"
#include "rndobj/Mat.h"
#include "rndobj/Dir.h"

class PatchRenderer : public RndTexRenderer {
public:
    PatchRenderer();
    OBJ_CLASSNAME(PatchRenderer);
    OBJ_SET_TYPE(PatchRenderer);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void DrawShowing();
    virtual ~PatchRenderer() {}
    virtual void DrawBefore();
    virtual void DrawAfter();

    void SetPatch(RndDir *);

    static RndDir *sBlankPatch;
    static RndDir *sTestPatch;
    static void Init();
    static void InitResources();
    static void Terminate();

    ObjPtr<RndMat> mBackMat; // 0x78
    ObjPtr<RndMat> mOverlayMat; // 0x84
    RndEnviron *unk90; // 0x90
    Symbol mTestMode; // 0x94
    Symbol mPosition; // 0x98
};
