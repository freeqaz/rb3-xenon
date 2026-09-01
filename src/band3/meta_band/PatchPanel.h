#pragma once
#include "bandobj/PatchDir.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/JoypadMsgs.h"
#include "rndobj/Mat.h"
#include "ui/PanelDir.h"
#include "ui/UIGridProvider.h"
#include "ui/UIListProvider.h"
#include "ui/UIPanel.h"
#include "utl/Std.h"
#include "utl/Symbol.h"

class LayerProvider : public UIListProvider, public Hmx::Object {
public:
    LayerProvider(PatchDir *p) : mPatch(p) {
        for (int i = 0; i < 50; i++) {
            mLayerMats.push_back(Hmx::Object::New<RndMat>());
        }
    }
    virtual ~LayerProvider() { DeleteAll(mLayerMats); }
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual void Custom(int, int, class UIListCustom *, Hmx::Object *) const;
    virtual int NumData() const;
    virtual bool IsActive(int) const;
    virtual void InitData(RndDir *);
    virtual UIColor *SlotColorOverride(int, int, class UIListWidget *, UIColor *c) const;
    virtual DataNode Handle(DataArray *, bool);

    void SetLabelForData(UILabel *, int) const;
    RndMat *GetMatForData(int) const;

    PatchDir *mPatch; // 0x2c
    std::vector<RndMat *> mLayerMats; // 0x30
};

class CategoryProvider : public UIListProvider, public Hmx::Object {
public:
    CategoryProvider(PanelDir *panel, PatchDir *patch)
        : mPatch(patch), mResource(panel) {}
    virtual ~CategoryProvider() { DeleteAll(mCategoryMats); }
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;
    virtual void InitData(RndDir *);

    std::vector<RndMat *> mCategoryMats; // 0x2c
    PatchDir *mPatch; // 0x38
    PanelDir *mResource; // 0x3c
};

class StickerProvider : public UIListProvider, public Hmx::Object {
public:
    StickerProvider() : mStickerMat(0), mStickers(0), unk30(gNullStr) {}
    virtual ~StickerProvider() { DeleteAll(mStickerMats); }
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual int NumData() const;
    virtual void InitData(RndDir *);

    void StickerLoaded();
    void SetStickers(std::vector<PatchSticker *> *, Symbol);

    RndMat *mStickerMat; // 0x2c
    std::vector<RndMat *> mStickerMats; // 0x30
    std::vector<PatchSticker *> *mStickers; // 0x3c
    Symbol unk30; // 0x40
};

class PatchPanel : public UIPanel {
public:
    PatchPanel();
    OBJ_CLASSNAME(PatchPanel);
    OBJ_SET_TYPE(PatchPanel);
    NEW_OBJ(PatchPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~PatchPanel();
    virtual void Enter();
    virtual void Poll();
    virtual void Load();
    virtual void Unload();
    virtual bool IsLoaded() const;
    virtual void PollForLoading();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    void ResetDirections();
    void ResetVelocities();
    void CopyFromPatch(const PatchDir *);
    PatchLayer &EditLayer();
    int GetEditLayerListIndex() const;
    void RestoreUndo();
    void StoreUndo();
    float CalcMotion(float, int);
    void CopyToPatch(PatchDir *) const;
    void SetBaseSize(float, float);
    void SetStickerCategory(Symbol);

    DataNode OnMsg(const ButtonDownMsg &);
    DataNode OnMsg(const ButtonUpMsg &);
    DataNode SwapLayers(DataArray *);
    DataNode ClearLayer(DataArray *);
    DataNode DupeLayer(DataArray *);

    PatchDir *mPatch; // 0x3c
    CategoryProvider *mCategoryProvider; // 0x40
    StickerProvider *mStickerProvider; // 0x44
    UIGridProvider *mGridProvider; // 0x48
    LayerProvider *mLayerProvider; // 0x4c
    Symbol mMode; // 0x50
    bool unk50; // 0x54
    bool unk51; // 0x55
    int mEditLayerIdx; // 0x58
    float unk58; // 0x5c
    float unk5c; // 0x60
    float unk60; // 0x64
    float unk64; // 0x68
    float unk68; // 0x6c
    float unk6c; // 0x70
    float unk70; // 0x74
    float unk74; // 0x78
    float unk78; // 0x7c
    float unk7c; // 0x80
    float unk80; // 0x84
    float unk84; // 0x88
    int mMoveX; // 0x8c
    int mMoveY; // 0x90
    float mMoveVelX; // 0x94
    float mMoveVelY; // 0x98
    int mRot; // 0x9c
    float mRotVel; // 0xa0
    int mScaleX; // 0xa4
    int mScaleY; // 0xa8
    float mScaleVelX; // 0xac
    float mScaleVelY; // 0xb0
    int mDeform; // 0xb4
    float mDeformVel; // 0xb8
    float mBaseSizeX; // 0xbc
    float mBaseSizeY; // 0xc0
    int mUndoColorIdx; // 0xc4
    Vector3 mUndoPosition; // 0xc8
    float mUndoRotation; // 0xd8
    float mUndoScaleX; // 0xdc
    float mUndoScaleY; // 0xe0
    float mUndoDeformFrame; // 0xe4
    Symbol mUndoStickerCategory; // 0xe8
    int mUndoStickerIdx; // 0xec
};

int ConvertToLayerIndex(PatchDir *, int);