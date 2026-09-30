#include "ui/InlineHelp.h"
#include "bandobj/BandLabel.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Joypad.h"
#include "rndobj/Dir.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "ui/UILabel.h"
#include "ui/UIResource.h"
#include "utl/BinStream.h"
#include "utl/Locale.h"
#include "utl/Std.h"
#include "utl/Symbol.h"

float InlineHelp::sLastUpdatedTime = 0;
float InlineHelp::sRotationTime = 0;
float InlineHelp::sLabelRot = 0;
bool InlineHelp::sHasFlippedTextThisRotation = false;
bool InlineHelp::sNeedsTextUpdate = false;
bool InlineHelp::sRotated = false;
const float InlineHelp::sRotateDelay = 5;
const float InlineHelp::sRotateDuration = 1;

// Per-TU load revs (retail lbl_82CBDC10 alt / lbl_82CBDC14 rev). RB3's
// InlineHelp::PreLoad (0x823179A8) reads the packed rev off a plain BinStream
// and splits it into these two shorts; every later version test, including the
// ActionElement sub-loader's, re-reads the rev.
// Residue (W16-HM): retail puts alt at +0 / rev at +4, we get the reverse.
// Measured inert: declaration order (both orders, with and without align(4)),
// store order, and referencing alt first from an earlier function. One
// aligned(4) aggregate (ui/UIListArrow.cpp's fix) does place them right, but it
// turns the sub-loader's direct `lhz lbl_82CBDC14` into addi+lhz 4 and costs
// that row 2.6 pp, so the two separate statics are kept.
static __declspec(align(4)) unsigned short sInlineHelpRev;
static __declspec(align(4)) unsigned short sInlineHelpAltRev;

#pragma region InlineHelp::ActionElement

InlineHelp::ActionElement::ActionElement()
    : mAction(kAction_None), mPrimaryToken(gNullStr), mSecondaryToken(gNullStr) {}

InlineHelp::ActionElement::ActionElement(JoypadAction a)
    : mAction(a), mPrimaryToken(gNullStr), mSecondaryToken(gNullStr) {}

InlineHelp::ActionElement::~ActionElement() {}

BinStream &operator<<(BinStream &bs, const InlineHelp::ActionElement &a) {
    bs << a.mAction;
    Symbol primary = a.mPrimaryToken;
    bs << primary;
    Symbol secondary = a.mSecondaryToken;
    bs << secondary;
    return bs;
}

BinStream &operator>>(BinStream &d, InlineHelp::ActionElement &a) {
    int action;
    d >> action;
    a.mAction = (JoypadAction)action;
    Symbol token;
    d >> token;
    a.SetToken(token, false);
    if (sInlineHelpRev >= 2) {
        d >> token;
        a.SetToken(token, true);
    }
    return d;
}

void InlineHelp::ActionElement::SetToken(Symbol token, bool secondary) {
    if (!secondary) {
        mPrimaryToken = token;
        mPrimaryStr = Localize(token, nullptr);
    } else {
        mSecondaryToken = token;
        mSecondaryStr = Localize(token, nullptr);
    }
}

void InlineHelp::ActionElement::SetString(const char *str, bool secondary) {
    if (!secondary) {
        mPrimaryToken = gNullStr;
        mPrimaryStr = str;
    } else {
        mSecondaryToken = gNullStr;
        mSecondaryStr = str;
    }
}

void InlineHelp::ActionElement::SetConfig(DataNode &dn, bool secondary) {
    if (dn.Type() == kDataArray) {
        DataArray *da = dn.Array();
        if (da->Size() != 0) {
            FormatString fs(Localize(da->Sym(0), nullptr));
            for (int i = 1; i < da->Size(); i++) {
                const DataNode &dn2 = da->Evaluate(i);
                if (dn2.Type() == kDataSymbol) {
                    fs << Localize(dn2.Sym(), nullptr);
                } else {
                    fs << dn2;
                }
            }
            SetString(fs.Str(), secondary);
        }
    } else {
        SetToken(dn.Sym(), secondary);
    }
}

Symbol InlineHelp::ActionElement::GetToken(bool secondary) const {
    if (secondary) {
        return mSecondaryToken;
    } else {
        return mPrimaryToken;
    }
}

const char *InlineHelp::ActionElement::GetText(bool secondary) const {
    if (secondary && HasSecondaryStr()) {
        return mSecondaryStr.c_str();
    } else {
        return mPrimaryStr.c_str();
    }
}

BEGIN_CUSTOM_PROPSYNC(InlineHelp::ActionElement)
    SYNC_PROP(action, (int &)o.mAction)
    SYNC_PROP_SET(text_token, o.GetToken(false), o.SetToken(_val.Sym(), false))
    SYNC_PROP_SET(secondary_token, o.GetToken(true), o.SetToken(_val.Sym(), true))
END_CUSTOM_PROPSYNC

#pragma endregion
#pragma region InlineHelp

InlineHelp::InlineHelp()
    : mUseConnectedControllers(false), mHorizontal(true), mSpacing(0), mTemplateLabel(0),
      mTextColor(this) {}

InlineHelp::~InlineHelp() {
    int siz = mTextLabels.size();
    for (int i = 0; i < siz; i++) {
        delete mTextLabels[i];
    }
}

BEGIN_HANDLERS(InlineHelp)
    HANDLE_ACTION(
        set_action_token, SetActionToken((JoypadAction)_msg->Int(2), _msg->Node(3))
    )
    HANDLE_ACTION(clear_action_token, ClearActionToken((JoypadAction)_msg->Int(2)))
    HANDLE(set_config, OnSetConfig)
    HANDLE_SUPERCLASS(UIComponent)
END_HANDLERS

BEGIN_PROPSYNCS(InlineHelp)
#ifdef HX_NATIVE
    // DC3-era addition; RB3-360 retail's InlineHelp chain STARTS at `config`.
    // Arbitrated on RETAIL BYTES (lane CQ-3): the 640 B retail body enumerates
    // config horizontal spacing text_color use_connected_controllers -- five
    // literals, ours emitted six, with `resource` prepended at the HEAD (which
    // is why every later block was displaced).  Native-only.
    SYNC_PROP_MODIFY(resource, mResourceDir, Update())
#endif
    SYNC_PROP_MODIFY(config, mConfig, SyncLabelsToConfig())
    SYNC_PROP(horizontal, mHorizontal)
    SYNC_PROP(spacing, mSpacing)
    SYNC_PROP_MODIFY(text_color, mTextColor, UpdateTextColors())
    SYNC_PROP(use_connected_controllers, mUseConnectedControllers)
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS

BEGIN_SAVES(InlineHelp)
    SAVE_REVS(4, 0)
    bs << mHorizontal;
    bs << mSpacing;
    bs << mConfig;
    bs << mTextColor;
    bs << mUseConnectedControllers;
    SAVE_SUPERCLASS(UIComponent)
END_SAVES

// RB3 retail (0x82313F00): the members are copied by the CopyMembers override,
// which UIComponent::Copy dispatches to; Copy itself only re-runs Update().
BEGIN_COPYS(InlineHelp)
    CREATE_COPY_AS(InlineHelp, h)
    MILO_ASSERT(h, 129);
    COPY_SUPERCLASS_FROM(UIComponent, h)
    Update();
END_COPYS

// RB3 retail fn_82316840.
void InlineHelp::CopyMembers(const UIComponent *o, Hmx::Object::CopyType ty) {
    UIComponent::CopyMembers(o, ty);
    CREATE_COPY_AS(InlineHelp, h);
    MILO_ASSERT(h, 139);
    COPY_MEMBER_FROM(h, mHorizontal)
    COPY_MEMBER_FROM(h, mSpacing)
    COPY_MEMBER_FROM(h, mConfig)
    COPY_MEMBER_FROM(h, mTextColor)
    COPY_MEMBER_FROM(h, mUseConnectedControllers)
    UpdateIconTypes(false);
}

BEGIN_LOADS(InlineHelp)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

INIT_REVS(5, 0)

void InlineHelp::PreLoad(BinStream &bs) {
    // RB3 retail (0x823179A8): plain-BinStream rev dialect, no version guard, no
    // PushRev, and no rev-5 resource dir (that is DC3's addition).
    int rev;
    bs >> rev;
    sInlineHelpRev = getHmxRev(rev);
    sInlineHelpAltRev = getAltRev(rev);
    bs >> mHorizontal;
    bs >> mSpacing;
    bs >> mConfig;
    if (sInlineHelpRev >= 1) {
        bs >> mTextColor;
    }
    if (sInlineHelpRev >= 2 && sInlineHelpRev < 4) {
        int x;
        bs >> x;
    }
    if (sInlineHelpRev >= 3) {
        bs >> mUseConnectedControllers;
    }
    UIComponent::PreLoad(bs);
}

void InlineHelp::PostLoad(BinStream &bs) {
    // RB3 retail (0x82314010): nothing was pushed, so nothing is popped.
    UIComponent::PostLoad(bs);
    Update();
}

void InlineHelp::Poll() {
    UIComponent::Poll();
    float uisecs = TheTaskMgr.UISeconds();
    if (uisecs != sLastUpdatedTime) {
        sNeedsTextUpdate = false;
        if (uisecs > sRotationTime) {
            float f1 = uisecs - sRotationTime;
            if (f1 >= 1.0f) {
                sHasFlippedTextThisRotation = false;
                sRotationTime = uisecs + 5.0f;
                SetLabelRotationPcts(0);
            } else {
                if (!sHasFlippedTextThisRotation && f1 >= 0.5f) {
                    sHasFlippedTextThisRotation = true;
                    sRotated = sRotated == 0;
                    sNeedsTextUpdate = true;
                }
                SetLabelRotationPcts(f1);
            }
        }
        sLastUpdatedTime = uisecs;
    }
    if (sNeedsTextUpdate)
        UpdateLabelText();
}

void InlineHelp::Enter() {
    UIComponent::Enter();
    UpdateIconTypes(true);
    SyncLabelsToConfig();
}

void InlineHelp::OldResourcePreload(BinStream &bs) {
    char buf[0x100];
    bs.ReadString(buf, 0x100);
    // mResourceDir is the inherited UIComponent ObjDirPtr (no SetName);
    // inline equivalent of ResourceDirPtr::SetName(buf, true):
    FilePath path;
    if (ResourceDirBase::MakeResourcePath(
            path, ClassName(), ObjectDir::StaticClassName(), buf
        )) {
        mResourceDir.LoadFile(path, true, true, kLoadFront, false);
    } else {
        mResourceDir = 0;
    }
}

void InlineHelp::UpdateLabelText() {
    static Symbol inline_help_fmt("inline_help_fmt");
    int size = mConfig.size();
    for (int i = 0; i < size; i++) {
        String icon = GetIconStringFromAction(mConfig[i].mAction);
        if (icon.empty())
            mTextLabels[i]->SetTextToken(gNullStr);
        else
            mTextLabels[i]->SetTokenFmt(
                inline_help_fmt, icon.c_str(), mConfig[i].GetText(sRotated)
            );
    }
}
void InlineHelp::Init() {
    REGISTER_OBJ_FACTORY(InlineHelp)
    // Retail's InlineHelp::Init is 88 B and ours was 52 B: it also calls
    // TheUI->InitResources(Symbol("InlineHelp")).  Read out of retail bytes, not
    // the symbol map -- 0x82316C18 constructs a Symbol from the .rdata literal at
    // 0x8202F880, which reads "InlineHelp", then loads the UIManager global at
    // 0x82C721F0 (a POINTER load, `lwz r3,0x21f0(r11)`, matching `extern
    // UIManager *TheUI`) and calls InitResources on it.  Without this line the
    // inline-help resource set is never initialised at runtime.
    TheUI->InitResources("InlineHelp");
}

String InlineHelp::GetIconStringFromAction(int idx) {
    static Symbol action_chars("action_chars");
    String ret;
    const DataArray *t = TypeDef();
    MILO_ASSERT(t, 0x1cb);
    DataArray *actionArr = t->FindArray(action_chars);
    FOREACH (it, mIconTypes) {
        const char *str = actionArr->FindArray(*it)->Str(idx + 1);
        char c = *str;
        if (ret.find(c) == String::npos)
            ret += c;
    }
    return ret;
}

void InlineHelp::ResetRotation() {
    sRotated = 0;
    sHasFlippedTextThisRotation = 0;
    sRotationTime = TheTaskMgr.UISeconds() + 5.0f;
    sLabelRot = -0.0f;
}

// RB3 retail (0x82314EB0): the template label comes from the component's
// UIResource dir, unconditionally, after the base Update.
void InlineHelp::Update() {
    UIComponent::Update();
    const DataArray *t = TypeDef();
    MILO_ASSERT(t, 0x187);
    RndDir *dir = mResource->Dir();
    MILO_ASSERT(dir, 0x18A);
    static Symbol text_label("text_label");
    // Retail instantiates Find<BandLabel> (rb3-Wii: BandLabel); not an ICF fold of
    // Find<UILabel>, whose dynamic_cast target differs.
    mTemplateLabel = dir->Find<BandLabel>(t->FindStr(text_label), true);
    SyncLabelsToConfig();
}

// RB3 retail fn_82316F08: a fixed instrument list, not DC3's typedef lookup.
void InlineHelp::UpdateIconTypes(bool) {
    static Symbol vocals("vocals");
    static Symbol guitar("guitar");
    static Symbol drums("drums");
    static Symbol keys("keys");
    mIconTypes.clear();
    mIconTypes.push_back(vocals);
    mIconTypes.push_back(guitar);
    mIconTypes.push_back(drums);
    mIconTypes.push_back(keys);
}

void InlineHelp::SetLabelRotationPcts(float f) {
    if (f < 0.5f)
        sLabelRot = f * -240.0f;
    else
        sLabelRot = f * -240.0f - 120.0f;
}

void InlineHelp::DrawShowing() {
    int numLabels = mTextLabels.size();
    const Transform &parentXfm = mTemplateLabel->WorldXfm();
    Transform worldXfm;
    memcpy(&worldXfm, &parentXfm, sizeof(Transform));
    UILabel *t = mTemplateLabel;
    MILO_ASSERT(t, 0x117);

    Transform offsetXfm;
    offsetXfm.m.Identity();
    offsetXfm.v.Zero();

    Transform rotXfm;
    if (sLabelRot != 0.0f) {
        Vector3 angles(DegreesToRadians(sLabelRot), 0.0f, 0.0f);
        Hmx::Matrix3 rotMtx;
        MakeRotMatrix(angles, rotMtx, true);
        Multiply(offsetXfm, rotMtx, rotXfm);
    } else {
        rotXfm.m.Identity();
        rotXfm.v.Zero();
    }

    for (int i = 0; i < numLabels; i++) {
        if (i > 0) {
            if (mHorizontal) {
                offsetXfm.v.x += mSpacing;
            } else {
                offsetXfm.v.z += mSpacing;
            }
        }
        Transform labelXfm;
        Multiply(offsetXfm, worldXfm, labelXfm);
        if (*mConfig[i].mSecondaryStr.c_str() != '\0') {
            Multiply(rotXfm, labelXfm, labelXfm);
        }
        mTextLabels[i]->SetWorldXfm(labelXfm);
        mTextLabels[i]->DrawShowing();
    }
}

void InlineHelp::SetActionToken(JoypadAction a, DataNode &node) {
    bool found = false;
    FOREACH (it, mConfig) {
        if (it->mAction == a) {
            it->SetConfig(node, false);
            found = true;
            break;
        }
    }
    if (!found) {
        ActionElement el(a);
        el.SetConfig(node, false);
        mConfig.push_back(el);
    }
    SyncLabelsToConfig();
}

void InlineHelp::SyncLabelsToConfig() {
    ResetRotation();
    int cfg_size = (int)mConfig.size();
    int labels_size = (int)mTextLabels.size();
    if (cfg_size > labels_size) {
        for (int i = labels_size; i < cfg_size; i++) {
            UILabel *lbl = Hmx::Object::New<UILabel>();
            lbl->Copy(mTemplateLabel, kCopyShallow);
            lbl->LStyle(0).mColorOverride = mTextColor;
            mTextLabels.push_back(lbl);
        }
    } else {
        if (labels_size > cfg_size) {
            for (int i = cfg_size; i < labels_size; i++) {
                delete mTextLabels[i];
            }
            mTextLabels.resize(cfg_size);
        }
    }
    UpdateLabelText();
}

void InlineHelp::UpdateTextColors() {
    FOREACH (it, mTextLabels) {
        (*it)->LStyle(0).mColorOverride = mTextColor;
    }
}

void InlineHelp::ClearActionToken(JoypadAction a) {
    FOREACH (it, mConfig) {
        if (it->mAction == a) {
            mConfig.erase(it);
            SyncLabelsToConfig();
            return;
        }
    }
}

DataNode InlineHelp::OnSetConfig(const DataArray *da) {
    mConfig.clear();
    DataArray *arr = da->Array(2);
    for (int i = 0; i < arr->Size(); i++) {
        DataArray *loopArr = arr->Array(i);
        ActionElement el((JoypadAction)loopArr->Int(0));
        el.SetConfig(loopArr->Node(1), false);
        if (loopArr->Size() > 2)
            el.SetConfig(loopArr->Node(2), true);
        mConfig.push_back(el);
    }
    SyncLabelsToConfig();
    return 1;
}

#pragma endregion
