#pragma once
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Joypad.h"
#include "stl/_vector.h"
#include "ui/ResourceDirPtr.h"
#include "ui/UIColor.h"
#include "ui/UIComponent.h"
#include "ui/UILabel.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"

class InlineHelp : public UIComponent {
public:
    struct ActionElement { // From RB3 decomp
        JoypadAction mAction; // 0x0
        Symbol mPrimaryToken; // 0x4
        Symbol mSecondaryToken; // 0x8
        String mPrimaryStr; // 0xc
        String mSecondaryStr; // 0x18

        ~ActionElement();
        ActionElement();
        ActionElement(JoypadAction);

        bool HasSecondaryStr() const { return !mSecondaryStr.empty(); }
        void SetToken(Symbol, bool);
        void SetString(char const *, bool);
        void SetConfig(DataNode &, bool);
        Symbol GetToken(bool) const;
        const char *GetText(bool) const;
    };

    // Hmx::Object
    virtual ~InlineHelp();
    OBJ_CLASSNAME(InlineHelp);
    OBJ_SET_TYPE(InlineHelp);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void SetTypeDef(DataArray *);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndDrawable
    virtual void DrawShowing();
    // RndPollable
    virtual void Poll();
    virtual void Enter();

    NEW_OBJ(InlineHelp)
    OBJ_MEM_OVERLOAD(0x16)

    static void Init();

    void UpdateTextColors();
    void ClearActionToken(JoypadAction);
    void SetActionToken(JoypadAction, DataNode &);
    void ResetConfig() { mConfig.clear(); }
    void SetLabelRotationPcts(float f);
    DataNode OnSetConfig(DataArray const *);

private:
    static float sLastUpdatedTime;
    static float sRotationTime;
    static float sLabelRot;
    static bool sHasFlippedTextThisRotation;
    static bool sNeedsTextUpdate;
    static bool sRotated;
    static const float sRotateDelay;
    static const float sRotateDuration;

protected:
    UICOMP_DC3_VIRTUAL void OldResourcePreload(BinStream &);

    InlineHelp();

    void Update();
    void UpdateLabelText();

    static void ResetRotation();

    // Offsets relative to this (UIComponent part = 0x140, retail-witnessed):
    std::vector<Symbol> mIconTypes; // 0x140
    std::vector<ActionElement> mConfig; // 0x14c
    std::vector<UILabel *> mTextLabels; // 0x158
    bool mUseConnectedControllers; // 0x164
    bool mHorizontal; // 0x165
    float mSpacing; // 0x168
    // NOTE: retail RB3-360 has NO per-component ResourceDirPtr here (DC3-newer
    // addition); the resource dir is the inherited UIComponent::mResourceDir.
    UILabel *mTemplateLabel; // 0x16c
    ObjPtr<UIColor> mTextColor; // 0x170 (vbase Hmx::Object at 0x180)

    virtual void SyncLabelsToConfig();
    virtual void UpdateIconTypes(bool);
    virtual String GetIconStringFromAction(int);
};
