#pragma once
#include "obj/ObjMacros.h"
#include "bandobj/BandLabel.h"
#include "ui/UIComponent.h"

class UIComponentFocusChangeMsg;

class BandHighlight : public UIComponent {
public:
    BandHighlight();
    OBJ_CLASSNAME(BandHighlight);
    OBJ_SET_TYPE(BandHighlight);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void DrawShowing();
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual int CollidePlane(const Plane &);
    virtual ~BandHighlight();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual void Update();

    void OnRegisterFocus();
    void OnUnregisterFocus();
    void UpdateTargetEdge(RndTransformable *);
    void SyncDir();
    void SetTarget(UIComponent *, bool);

    DataNode OnMsg(const UIComponentFocusChangeMsg &);

    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(BandHighlight); }
    NEW_OBJ(BandHighlight);
    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;

    Vector3 unk10c; // 0x140
    Vector3 unk118; // 0x150
    float unk124;
    float mAnimDuration; // 0x164
    BandLabel *mHelpTextLabel; // 0x168
    Vector3 unk130; // 0x16c
    Vector3 unk13c; // 0x17c
    RndTransformable *unk148; // 0x18c
    std::vector<RndMat *> mMirrorMats; // 0x190
    UIComponent *unk154; // 0x19c
    int unk158; // 0x1a0
};
