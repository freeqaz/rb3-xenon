#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/DateTime.h"
#include "obj/Dir.h"
#include "os/Debug.h"
#include "rndobj/Font.h"
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
    /** "How to fit text in the width/height specified" — retail stores this as a
     *  4-byte enum at 0x19c (UILabel::PreLoad reads 4 bytes into it). */
    enum FitType {
        kFitWrap = 0,
        kFitStretch = 1,
        kFitJust = 2,
        kFitEllipsis = 3,
    };

    /** Retail RB3 has NO `ObjVector<LabelStyle>` member (the ctor's aggregate
     *  enumeration at 0x827F3D50 is exhaustive, and there is no `mulli ...,0x1c`
     *  anywhere in the UILabel.cpp span) -- LabelStyle is a DC3-only concept.
     *  The struct and the two free PropSync overloads in UILabel.cpp are kept on
     *  purpose: they are what instantiate the `ObjVector<UILabel::LabelStyle>`
     *  template bodies that the retail UILabel unit pins (ICF-merged generic STL
     *  code at 0x822A6878 / 0x8234B270 / 0x8234B4D0 / 0x8234D080). Deleting them
     *  would unpair four already-matching functions for no gain. */
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
    // Retail-360 keeps this OUT-OF-LINE (InstrumentDifficultyDisplay::UpdateDisplay
    // calls it with an sret buffer: `addi r3,r31,0x50 / lwz r4,mLabel / bl 827F2428`,
    // rather than inlining the mTextToken load). Definition lives in UILabel.cpp.
    Symbol TextToken();
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
    // Decl-only, mirrors rb3-Wii UILabel::SetColorOverride(UIColor *).
    // Referenced by bandobj game code (InstrumentDifficultyDisplay, ScoreDisplay).
    // Non-virtual, no layout effect.
    void SetColorOverride(UIColor *);

    // Out-of-line (not inline) -- confirmed by retail asm at a caller
    // (BandHighlight::UpdateTargetEdge, lane NCCC-0731-ab7e/f264/sonnet):
    // retail emits `bl` to a real getter here, not a folded direct member
    // load, so the header body must not be inline.
    RndText *TextObj();
    const RndText *TextObj() const;

    // ------------------------------------------------------------------
    // RB3 retail API restored from the rb3-Wii oracle
    // (../rb3/src/system/ui/UILabel.h) + retail asm. All NON-VIRTUAL except
    // Update()/CopyMembers(), which are OVERRIDES of existing UIComponent
    // virtual slots 0x4c/0x48 -- they add no vtable slot and do not move the
    // verified UILabel own-virtual block (Draw/SetCreditsText/SetDisplayText
    // @0x50/0x54/0x58). Layout-neutral.
    // ------------------------------------------------------------------
    void Update();
    void CopyMembers(const UIComponent *, Hmx::Object::CopyType);
    RndText::Alignment Alignment() const { return mAlignment; }
    float Alpha() const { return mAlpha; }
    float AltAlpha() const { return mAltAlpha; }
    void SetAlpha(float f) { mAlpha = f; }
    void SetAltAlpha(float f) { mAltAlpha = f; }
    void SetFitType(FitType);
    void SetUseHighlightMesh(bool);
    bool HasHighlightMesh() const;
    void UpdateAndDrawHighlightMesh();
    int
    InqMinMaxFromWidthAndHeight(float, float, RndText::Alignment, Vector3 &, Vector3 &);
    void AdjustHeight(bool);
    void AltFontResourceFileUpdated(bool);
    void FitText();
    RndFont *Font();
    RndFont *AltFont();
    void OnSetIcon(const char *);
    DataNode OnGetMaterialVariations(const DataArray *);
    DataNode OnGetAltMaterialVariations(const DataArray *);
    float GetDrawWidth();

    Symbol GetTextToken() const { return mTextToken; }
    char const *GetDefaultText() const;
    // Declared (not defined) in this tree: retail UILabel has it and
    // DialogDisplay::GetLabelHeight calls it. The decomp build compiles to
    // .obj only -- objdiff never links -- so an undefined member is inert.
    float GetDrawHeight();
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
    // retail 0x827F6258 takes TWO bools (matches the rb3-Wii signature); the
    // one-arg form was the DC3 shape.
    void LabelUpdate(bool, bool);
    DataNode OnSetHeightFromText(DataArray *);
    void SetFontMat(char const *, int);
    char const *GetFontMat(int);
    void RefreshFontMat(int);

    static bool sDeferUpdate;
    static bool sDebugHighlight;
    static bool sInDebugHighlight;

    // ---------------------------------------------------------------------
    // RETAIL-360 RB3 MEMBER LAYOUT — reconstructed 2026-07-29 (lane BO-3).
    // Replaces the old opaque `unsigned char mUnkTU5Tail[0xAC]`.
    //
    // Anchors (all from retail asm in build/45410914/asm/UILabel.s):
    //   * UIComponent nvsize = 0x140, so UILabel's own members start at 0x140.
    //   * UILabel::PreLoad (retail 0x827F4EC8) is a vbase-introduced virtual, so
    //     inside it `this` (r31) points at UILabel's Hmx::Object vbase. Its call
    //     `subi r3, r31, 0x218; bl <AltFontResourceFileUpdated>` PROVES
    //     r31 == UILabel* + 0x218 ⇒ UILabel nvsize 0x218, vtordisp at 0x214,
    //     own members occupy exactly [0x140, 0x214) = 0xD4 bytes.
    //     (0x218 also agrees with BandLabel's UITransitionHandler base offset.)
    //   * The UILabel ctor (retail 0x827F3E6C) constructs every aggregate member
    //     in declaration order: addi r30,{0x148,0x154,0x160,0x168,0x174,0x1b0,
    //     0x1c0,0x1e0,0x1fc,0x208}. That enumeration is exhaustive — there is
    //     NO ObjVector<LabelStyle> in retail RB3 (that member is DC3-only).
    //   * PreLoad's rev-gated reads give every scalar offset; FitText
    //     (retail 0x827F5550) independently confirms mText/mTextSize/mWidth;
    //     PreLoad's gRev<4 alignment fixup confirms mAlignment/mWidth/mHeight;
    //     UILabel::Font()'s mat-variation cache (retail 0x827F2CE8) confirms
    //     mLabelDir/mFont/mCurFontMatVariation/mFontMatVariation.
    //
    // Member NAMES and ORDER follow the rb3-Wii oracle
    // (../rb3/src/system/ui/UILabel.h) — same game, so the source order is the
    // same; only the offsets differ (Wii own-members start at 0x10c).
    // Retail-vs-Wii-dev divergences found and encoded here:
    //   - mAlignment / mCapsMode / mFitType are 4-byte enums in retail, not the
    //     packed uchars the Wii header shows (PreLoad reads 4 bytes into each).
    //   - mMarkup / mUseHighlightMesh / mAltStyleEnabled are separate bools at
    //     0x18c / 0x1cc / 0x1f8, not a packed bitfield.
    //   - mFixedLength / mReservedLine are 4-byte ints, not shorts.
    //   - retail has an extra String at 0x168 that the Wii decomp models as a
    //     discarded local in PreLoad (`if (gRev > 0xD) { String s; bs >> s; }`).
    // ---------------------------------------------------------------------
    UILabelDir *mLabelDir; // 0x140 (ctor stw 0x140; Font() passes it to UILabelDir)
    RndText *mText; // 0x144 (FitText: lwz r3,0x144(r30) -> RndText methods)
    String mLabelText; // 0x148 (Wii `unk114`, the live display text)
    ObjPtr<RndFont> mFont; // 0x154 (Font(): lwz 0x15c = ObjPtr payload)
    Symbol mCurFontMatVariation; // 0x160 (Wii `unk12c`; Font() caches variation)
    Symbol mTextToken; // 0x164 (PreLoad: bs >> mTextToken)
    String mEditText; // 0x168 (PreLoad gRev>0xD; Milo-only preview text)
    String mIcon; // 0x174 (PreLoad gRev>0xE)
    float mTextSize; // 0x180
    RndText::Alignment mAlignment; // 0x184 (lwz, tested &1/&4/&0x10/&0x40)
    RndText::CapsMode mCapsMode; // 0x188
    bool mMarkup; // 0x18c (+3 pad)
    float mLeading; // 0x190
    float mKerning; // 0x194
    float mItalics; // 0x198
    FitType mFitType; // 0x19c
    float mWidth; // 0x1a0
    float mHeight; // 0x1a4
    int mFixedLength; // 0x1a8 (read as 4 bytes, not the Wii short)
    int mReservedLine; // 0x1ac (ditto)
    String mPreserveTruncText; // 0x1b0
    float mAlpha; // 0x1bc
    ObjPtr<UIColor> mColorOverride; // 0x1c0
    bool mUseHighlightMesh; // 0x1cc (+3 pad)
    Symbol mFontMatVariation; // 0x1d0
    Symbol mAltMatVariation; // 0x1d4
    float mAltTextSize; // 0x1d8
    float mAltKerning; // 0x1dc (PreLoad else-arm copies mKerning@0x194 here)
    ObjPtr<UIColor> mAltTextColor; // 0x1e0
    float mAltZOffset; // 0x1ec
    float mAltItalics; // 0x1f0
    float mAltAlpha; // 0x1f4
    bool mAltStyleEnabled; // 0x1f8 (+3 pad)
    String mAltFontResourceName; // 0x1fc
    ObjDirPtr<ObjectDir> mObjDirPtr; // 0x208 -> ends 0x214 (vtordisp), vbase 0x218
};

bool PropSync(UILabel::LabelStyle &, DataNode &, DataArray *, int, PropOp);
