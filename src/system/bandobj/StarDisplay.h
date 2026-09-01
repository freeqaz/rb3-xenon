#ifndef BANDOBJ_STARDISPLAY_H
#define BANDOBJ_STARDISPLAY_H

#include "bandobj/BandLabel.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "rndobj/Text.h"
#include "ui/UIComponent.h"
#include "utl/Symbol.h"

class StarDisplay : public UIComponent {
public:
    StarDisplay();
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual ~StarDisplay();
    OBJ_CLASSNAME(StarDisplay)
    OBJ_SET_TYPE(StarDisplay)
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void CopyMembers(const UIComponent *, Hmx::Object::CopyType);
    virtual void Save(BinStream &);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void Update();
    virtual void Enter();

    void SetValues(int, int);
    void SetToToken(Symbol);
    void UpdateDisplay();
    void DrawShowing();
    void SetForceMixedMode(bool);
    void SetShowDenominator(bool b);
    void SetAlignment(RndText::Alignment);
    char GetStarIcon() const;
    char GetEmptyStarIcon() const;

    bool HasStarIcon() const;

    static Symbol GetSymbolForStarCount(int);
    static int GetStarCountForSymbol(Symbol);
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(StarDisplay); }

    NEW_OBJ(StarDisplay)

    BandLabel *mRsrcStarsLabel; // 0x140
    BandLabel *mRsrcStarsMixedLabel; // 0x144
    bool mForceMixedMode; // 0x148
    bool mShowDenominator; // 0x149
    bool mShowEmptyStars; // 0x14a
    int mStars; // 0x14c
    int mTotalStars; // 0x150
    RndText::Alignment mAlignment; // 0x154
    Symbol mIconOverride; // 0x158
    Symbol mEmptyIconOverride; // 0x15c
};

#endif // BANDOBJ_STARDISPLAY_H
