#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/DateTime.h"
#include "os/Debug.h"
#include "rndobj/Text.h"
#include "ui/ResourceDirPtr.h"
#include "ui/UIColor.h"
#include "ui/UIComponent.h"
#include "ui/UILabelDir.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"

class UILabel : public UIComponent {
public:
    struct LabelStyle {
        LabelStyle(Hmx::Object *owner) : mColorOverride(owner), mFontResource(owner) {}
        __forceinline LabelStyle &operator=(const LabelStyle &style) {
            mFontResource = style.mFontResource;
            mColorOverride = style.mColorOverride;
            return *this;
        }

        ObjPtr<UIColor> mColorOverride; // 0x0
        ResourceDirPtr<UILabelDir> mFontResource; // 0x14
    };
    friend bool __cdecl PropSync(LabelStyle &, DataNode &, DataArray *, int, PropOp);
    friend bool __cdecl
    PropSync(ObjVector<LabelStyle> &, DataNode &, DataArray *, int, PropOp);

    // Hmx::Object
    virtual ~UILabel();
    OBJ_CLASSNAME(UILabel)
    OBJ_SET_TYPE(UILabel)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // UIComponent
    virtual void Poll() { UIComponent::Poll(); }
    virtual void Highlight();
    virtual void DrawShowing();
    virtual bool CanHaveFocus();
    // retail-360 UILabel own-virtual @ vtable slot 0x50 (first UILabel own slot).
    // RndDrawable::Draw is NON-virtual in retail-360 (rndobj/Draw.h), so this is
    // a NEW own-virtual, not an override. Verified against the retail UILabel
    // vtable @0x8211AEB4: slot 0x50 = fn 0x827CCDF0 (the alpha-gated Draw body).
    // Placed FIRST among the UILabel own-virtuals so SetCreditsText stays 0x54
    // and SetDisplayText stays 0x58 (matches retail; no AppLabel slot shift).
    virtual void Draw();
    // text-holder semantics (was TextHolder, now plain forwarding).
    // NON-virtual in retail-360: the retail UILabel vtable has NO TextToken slot
    // (own-virtuals are exactly Draw/SetCreditsText/SetDisplayText @0x50/54/58).
    // TextToken overrides nothing (RndText::TextToken is on the mText *member*,
    // not a base) and is overridden by nothing (AppLabel does not override it),
    // so de-virtualizing is behavior-preserving and retail-faithful.
    Symbol TextToken() { return mTextToken; }
    virtual void SetCreditsText(DataArray *, class UIListSlot *) {
        MILO_ASSERT(false, 0x50);
    }

    OBJ_MEM_OVERLOAD(0x26);
    NEW_OBJ(UILabel)

    static void Init();
    static void Terminate();
    static bool sRequireFixedLength;

    void SetTextToken(Symbol);
    void SetInt(int, bool);
    void SetFloat(char const *, float);
    void SetDateTime(DateTime const &, Symbol);
    void SetIcon(char);
    void AppendIcon(char);
    void SetTokenFmt(const DataArray *);
    const RndText::Style &Style(int) const;
    RndText::Style &Style(int);
    void SetPrelocalizedString(String &);
#ifdef HX_NATIVE
    void SetPrelocalizedString(const String &s) { SetPrelocalizedString(const_cast<String &>(s)); }
#endif
    void SetSubtitle(const DataArray *);
    void SetTimeHMS(int, bool);
    bool CheckValid(bool);
    void SetEditText(char const *);
    // Forwards to text caps mode; referenced by game code (OvershellSlot::ShowState).
    // Decl-only, mirrors rb3-Wii UILabel::SetCapsMode(RndText::CapsMode).
    void SetCapsMode(RndText::CapsMode);
    void SetAlignment(RndText::Alignment);

    RndText *TextObj() { return mText; }
    const RndText *TextObj() const { return mText; }

    Symbol GetTextToken() const { return mTextToken; }
    char const *GetDefaultText() const;
    void CenterWithLabel(UILabel *, bool, float);
    LabelStyle &LStyle(int);
    const LabelStyle &LStyle(int) const;

    template <class T1>
    void SetTokenFmt(Symbol s, T1 t1) {
        SetTokenFmt(DataArrayPtr(s, t1));
    }

    template <class T1, class T2>
    void SetTokenFmt(Symbol s, T1 t1, T2 t2) {
        SetTokenFmt(DataArrayPtr(s, t1, t2));
    }

    template <class T1, class T2, class T3>
    void SetTokenFmt(Symbol s, T1 t1, T2 t2, T3 t3) {
        SetTokenFmt(DataArrayPtr(s, t1, t2, t3));
    }

    template <class T1, class T2, class T3, class T4>
    void SetTokenFmt(Symbol s, T1 t1, T2 t2, T3 t3, T4 t4) {
        SetTokenFmt(DataArrayPtr(s, t1, t2, t3, t4));
    }

protected:
    UICOMP_DC3_VIRTUAL void OldResourcePreload(BinStream &);
    virtual void SetDisplayText(const char *, bool);

    UILabel();
    void SetTokenFmtImp(Symbol, DataArray const *, DataArray const *, int, bool);
    DataNode OnSetPrelocalizedString(DataArray const *);
    DataNode OnSetTokenFmt(DataArray const *);
    DataNode OnSetInt(DataArray const *);
    DataNode OnSetTimeHMS(DataArray const *);
    bool AllowEditText() const;
    void LabelUpdate(bool);
    DataNode OnSetHeightFromText(DataArray *);
    void SetFontMat(char const *, int);
    char const *GetFontMat(int);
    void RefreshFontMat(int);

    static bool sDeferUpdate;
    static bool sDebugHighlight;
    static bool sInDebugHighlight;

    RndText *mText; // 0xd0 - underlying text object (formerly a base of UILabel)
    Symbol mTextToken; // 0x114
    String mLabelText; // 0x118
    char mIconChar; // 0x120
    bool mTextEmpty; // 0x121
    bool mDirty; // 0x122
    ObjVector<LabelStyle> mLabelStyles; // 0x124
};

bool PropSync(UILabel::LabelStyle &, DataNode &, DataArray *, int, PropOp);
