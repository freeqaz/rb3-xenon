#pragma once
#include "hamobj/HamLabel.h"
#include "rndobj/Anim.h"
#include "rndobj/Dir.h"
#include "ui/ResourceDirPtr.h"
#include "ui/UIComponent.h"

/** "Meter Display" */
class MeterDisplay : public UIComponent {
public:
    // Hmx::Object
    virtual ~MeterDisplay();
    OBJ_CLASSNAME(MeterDisplay);
    OBJ_SET_TYPE(MeterDisplay);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndDrawable
    virtual void DrawShowing();
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    // UIComponent
    UICOMP_DC3_VIRTUAL void OldResourcePreload(BinStream &);

    OBJ_MEM_OVERLOAD(0x16)
    NEW_OBJ(MeterDisplay)
    static void Init();

    void AnimateToValue(int, int);

protected:
    MeterDisplay();

    virtual void Update();

    void UpdateDisplay();

    RndAnimatable *mMeterAnim; // 0x44
    /** "length of value change animation, in seconds" */
    float mAnimPeriod; // 0x48
    float unk4c; // 0x4c
    int unk50; // 0x50
    HamLabel *unk54; // 0x54
    /** "whether or not to show text" */
    bool mShowText; // 0x58
    /** "whether or not to show text in percentage form" */
    bool mPercentageText; // 0x59
    /** "whether or not to hide denominator" */
    bool mHideDenominator; // 0x5a
    /** "Localization token to use for wrapper" */
    Symbol mWrapperText; // 0x5c
    /** "current value of meter" */
    int mCurrentValue; // 0x60
    /** "max value of meter" */
    int mMaxValue; // 0x64
    // NOTE(laneBQ2): `ResourceDirPtr<RndDir> mResourceDir` used to follow. Retail RB3
    // has no such member -- the rb3-Wii oracle's MeterDisplay ends at `mMaxValue`, and
    // RB3 reaches the dir through the INHERITED UIComponent::mResource (a UIResource*
    // at 0x108) via mResource->Dir(). Confirmed three ways: (1) ?SetType@MeterDisplay@@
    // at 0x8231a9a8 has vbase-displacement immediate 356, exactly where the Object
    // vtordisp lands once these 16 bytes are gone; (2) ?AnimateToValue@ matches at 100%
    // and pins every own member through mMaxValue (ending at 356), so the 16 bytes can
    // only be here; (3) retail ?Poll@ reads `lwz r11,0x30(r3)` = 216+48 = 264 =
    // UIComponent::mResource, then `lwz r11,0x14,r11` = UIResource::mDir(0x10) + the
    // ObjDirPtr inner pointer(+4).
};
