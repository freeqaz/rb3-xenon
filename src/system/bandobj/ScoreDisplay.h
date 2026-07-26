#pragma once
#include "bandobj/BandLabel.h"
#include "obj/ObjMacros.h"
#include "ui/UIColor.h"
#include "ui/UIComponent.h"
#include "ui/UIList.h"

class ScoreDisplay : public UIComponent, public UIListCustomTemplate {
public:
    ScoreDisplay();
    OBJ_CLASSNAME(ScoreDisplay);
    OBJ_SET_TYPE(ScoreDisplay);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void DrawShowing();
    virtual ~ScoreDisplay();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void Enter();
    virtual void CopyMembers(const UIComponent *, CopyType);
    virtual void Update();
    virtual void SetAlphaColor(float, UIColor *);
    virtual void GrowBoundingBox(Box &) const;
    virtual void UpdateDisplay();

    void SetValues(short, int, int, bool);
    void SetColorOverride(UIColor *);

    DataNode OnSetValues(const DataArray *);

    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(ScoreDisplay); }
    NEW_OBJ(ScoreDisplay);

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;

    BandLabel *mCombinedLabel;
    short unk114;
    int mScore;
    int mRank;
    bool mGlobally;
    ObjPtr<UIColor> mTextColor;
};
