#pragma once
// PatchRenderer: a RndTexRenderer that draws a patch RndDir into a texture
// between a background material and an overlay material.
// Retail places all of its code (0x822AE130-0x822AF1C8) inside the .text span
// pinned to BandSwatch, so the implementation (PatchRenderer.cpp) is compiled
// into BandSwatch's object.
#include "rndobj/TexRenderer.h"
#include "rndobj/Mat.h"
#include "rndobj/Dir.h"
#include "rndobj/Env.h"

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
    // No user-declared destructor: retail ~PatchRenderer (0x822AE930) destroys
    // the members and RndTexRenderer without re-storing any vtable.
    virtual void DrawBefore();
    virtual void DrawAfter();

    // Retail NewObject allocates 0xD8 bytes through MemAlloc(size, 0) after a
    // StaticClassName() call; ??_GPatchRenderer frees through MemFree.
    OBJ_MEM_OVERLOAD_INLINE_DEL(0x13)
    NEW_OBJ(PatchRenderer)

    void SetPatch(RndDir *);

    static RndDir *sBlankPatch;
    static RndDir *sTestPatch;
    static void Init();
    static void InitResources();
    static void Terminate();

    ObjPtr<RndMat> mBackMat; // 0x7c
    ObjPtr<RndMat> mOverlayMat; // 0x88
    // RndEnviron current when DrawBefore ran; DrawAfter re-selects it if the
    // patch draw changed the current environment. The ctor leaves it unset.
    RndEnviron *mSavedEnv; // 0x94
    Symbol mTestMode; // 0x98
    Symbol mPosition; // 0x9c
};
