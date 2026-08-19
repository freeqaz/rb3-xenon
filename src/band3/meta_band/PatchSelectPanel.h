#pragma once
#include "bandobj/PatchDir.h"
#include "meta_band/BandProfile.h"
#include "meta_band/EditSetlistPanel.h"
#include "obj/Object.h"
#include "rndobj/Mat.h"
#include "ui/UIGridProvider.h"
#include "ui/UIList.h"
#include "ui/UIListProvider.h"
#include "ui/UIPanel.h"
#include "utl/Std.h"

class PatchProvider : public UIListProvider, public Hmx::Object {
public:
    PatchProvider(std::vector<PatchDir *> &patches) : mPatches(patches), mEmptyMat(0) {}
    virtual ~PatchProvider() { DeleteAll(unk24); }
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual int NumData() const;
    virtual void InitData(RndDir *);

    PatchDir *GetPatchDir(int) const;
    bool HasAnyPatches() const;

    std::vector<PatchDir *> &mPatches; // 0x2c
    std::vector<RndMat *> unk24; // 0x30
    RndMat *mEmptyMat; // 0x3c
};

class PatchSelectPanel : public UIPanel {
public:
    PatchSelectPanel();
    OBJ_CLASSNAME(PatchSelectPanel);
    OBJ_SET_TYPE(PatchSelectPanel);
    NEW_OBJ(PatchSelectPanel);
    virtual DataNode Handle(DataArray *, bool);
    // NO user-declared destructor -- deliberate, do not re-add `virtual
    // ~PatchSelectPanel() {}` (lane DS-4/A).  For a class with a virtual base
    // + vtordisp, an *explicitly declared* dtor (even empty and inline) makes
    // MSVC emit the vptr/vtordisp reset preamble inside ~X; that preamble is
    // then inlined into the compiler-generated `??_D` vbase destructor, which
    // grows past the /Ob2 inline threshold, so `??_G` is left calling `??_D`
    // out of line.  With an *implicitly* declared dtor the preamble is
    // omitted, `??_D` is 5 instructions, /Ob2 inlines it, and `??_G` becomes
    // byte-exact with retail (22/22 instructions).  The base UIPanel dtor is
    // virtual, so the implicit dtor is still virtual -- behaviour unchanged.
    virtual void Draw();
    virtual void Enter();
    virtual void Load();
    virtual void Unload();
    virtual void FinishLoad();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    int DuplicatePatch(PatchDir *);
    void SetupForBandLogo(BandProfile *);
    void SetDescriptor(PatchDescriptor *);
    void SetupForSetlistArt(EditSetlistPanel *, BandProfile *);
    void SetupForCharacterPatch();
    void Confirm(int);

    PatchProvider *mPatchProvider; // 0x3c (UIPanel base ends 0x3c on retail)
    UIGridProvider *mGridProvider; // 0x40
    UIList *mPatchList; // 0x44
    PatchDescriptor *mDescriptor; // 0x48
    BandProfile *mSourceProfile; // 0x4c
    int mStartingPatchIx; // 0x50
    bool mStartWithMenu; // 0x54
};