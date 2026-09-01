#pragma once
#include "obj/ObjMacros.h"
#include "ui/UIComponent.h"

class CheckboxDisplay : public UIComponent {
public:
    CheckboxDisplay();
    OBJ_CLASSNAME(CheckboxDisplay);
    OBJ_SET_TYPE(CheckboxDisplay);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void DrawShowing();
    virtual ~CheckboxDisplay();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void Enter();
    virtual void CopyMembers(const UIComponent *, CopyType);
    virtual void Update();

    void SetChecked(bool);

    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(CheckboxDisplay); }
    NEW_OBJ(CheckboxDisplay);

    // DECLARE_REVS removed: it makes gRev/gAltRev CLASS statics, which have
    // EXTERNAL linkage and so cannot be addressed off a shared base register.
    // Retail loads one base (`lis`/`addi` on a single .data label) and stores
    // gAltRev at +0 / gRev at +4, i.e. one internal-linkage adjacent pair.
    // Definitions are now file-scope statics in CheckboxDisplay.cpp.
    NEW_OVERLOAD;
    DELETE_OVERLOAD;

    RndMesh *mCheckMesh; // 0x140
    bool mChecked; // 0x144
};