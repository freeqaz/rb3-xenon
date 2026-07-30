// RB3 retail-360 shape. Rewritten 2026-07-29 (lane BO-3, worker D) from the
// rb3-Wii DEV oracle `../rb3/src/system/ui/UILabel.cpp` plus the retail asm in
// `build/45410914/asm/UILabel.s`. The previous contents were a DC3 port
// (ObjVector<LabelStyle> / mIconChar / mTextEmpty / mDirty) that does not exist
// in RB3 and no longer compiles against the reconstructed retail member layout.
//
#include "ui/UILabel.h"

#include "macros.h"
#include "math/Color.h"
#include "math/Geo.h"
#include "math/Mtx.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "rndobj/Env.h"
#include "rndobj/Font.h"
#include "rndobj/FontBase.h"
#include "rndobj/Group.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Text.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "ui/UI.h"
#include "ui/UIColor.h"
#include "ui/UIComponent.h"
#include "ui/UILabelDir.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/Locale.h"
#include "utl/Str.h"
#include "utl/SuperFormatString.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/UTF8.h"
#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// Retail RB3 stores the load revision in CLASS STATICS gRev/gAltRev written at
// load time (UILabel::PreLoad @0x827F4EC8: `sth r10,0x4(r28)` / `sth r11,0x0`),
// i.e. the obj/ObjMacros.h rev dialect -- not obj/Object.h's local BinStreamRev.
// Install just those three macros here, bracketed with push_macro/pop_macro so
// the dialect does NOT leak: this file is whole-file #included into
// hamobj/HamCamTransform.cpp by the COMDAT-scatter lever, and a leaked dialect
// silently miscompiles every owner body that follows it there.
// ---------------------------------------------------------------------------
#pragma push_macro("INIT_REVS")
#pragma push_macro("LOAD_REVS")
#pragma push_macro("ASSERT_REVS")
#undef INIT_REVS
#undef LOAD_REVS
#undef ASSERT_REVS
// File-scope (not class-scope) statics on purpose: hamobj/HamCamTransform.cpp's
// COMDAT-scatter include renames them via `#define gRev gRev_UILabel` to avoid
// colliding with the other scattered owners, and that rename only works on a
// file-scope name.
#define INIT_REVS(objType)                                                               \
    static unsigned short gRev = 0;                                                      \
    static unsigned short gAltRev = 0;
#define LOAD_REVS(bs)                                                                    \
    int rev;                                                                             \
    bs >> rev;                                                                           \
    gRev = getHmxRev(rev);                                                               \
    gAltRev = getAltRev(rev);
// retail has no version guard here (PreLoad goes straight from LOAD_REVS into
// UIComponent::PreLoad -- no MILO_FAIL arm), matching ObjMacros.h's non-
// VERSION_SZBE69_B8 expansion.
#define ASSERT_REVS(rev1, rev2)

bool UILabel::sDebugHighlight;
bool UILabel::sDeferUpdate;
bool UILabel::sInDebugHighlight;
bool UILabel::sRequireFixedLength;

INIT_REVS(UILabel)

// RndTextUpdateDeferrer now comes from rndobj/Text.h (as it does on rb3-Wii,
// which is the generation retail RndText matches). The TU-local duplicate that
// used to live here is gone.

float GetTextSizeFromPctHeight(float f) {
    if (TheLoadMgr.EditMode()) {
        float depth = -TheUI->GetCam()->LocalXfm().v.y;
        Vector2 v2a(0.0f, 0.0f);
        Vector3 v3a;
        TheUI->GetCam()->ScreenToWorld(v2a, depth, v3a);
        Vector2 v2b(0.0f, f);
        Vector3 v3b;
        TheUI->GetCam()->ScreenToWorld(v2b, depth, v3b);
        return std::fabs(v3a.z - v3b.z);
    } else
        return f;
}

float GetPctHeightFromTextSize(float f) {
    if (TheLoadMgr.EditMode()) {
        Vector3 v3a(0.0f, 0.0f, 0.0f);
        Vector2 v2a;
        TheUI->GetCam()->WorldToScreen(v3a, v2a);
        Vector3 v3b(0.0f, 0.0f, -f);
        Vector2 v2b;
        TheUI->GetCam()->WorldToScreen(v3b, v2b);
        return std::fabs(v2a.y - v2b.y);
    } else
        return f;
}

// retail 0x827F3E6C. Constructs every aggregate member in declaration order:
// 0x148 mLabelText, 0x154 mFont, 0x160/0x164 Symbols, 0x168 mEditText,
// 0x174 mIcon, 0x1b0 mPreserveTruncText, 0x1c0 mColorOverride,
// 0x1e0 mAltTextColor, 0x1fc mAltFontResourceName, 0x208 mObjDirPtr.
UILabel::UILabel()
    : mLabelDir(nullptr), mText(Hmx::Object::New<RndText>()), mLabelText(),
      mFont(this, nullptr), mCurFontMatVariation(), mTextToken(), mEditText(), mIcon(),
      mTextSize(30.0f), mAlignment(RndText::kMiddleCenter),
      mCapsMode(RndText::kCapsModeNone), mMarkup(false), mLeading(1.0f), mKerning(0.0f),
      mItalics(0.0f), mFitType(kFitWrap), mWidth(0.0f), mHeight(0.0f), mFixedLength(0),
      mReservedLine(0), mPreserveTruncText(), mAlpha(1.0f), mColorOverride(this, nullptr),
      mUseHighlightMesh(false), mFontMatVariation(), mAltMatVariation(),
      mAltTextSize(mTextSize), mAltKerning(mKerning), mAltTextColor(this, nullptr),
      mAltZOffset(0.0f), mAltItalics(0.0f), mAltAlpha(1.0f), mAltStyleEnabled(false),
      mAltFontResourceName(), mObjDirPtr() {
    mText->SetTransParent(this, false);
    mResourcePath = GetResourcesPath();
}

UILabel::~UILabel() { delete mText; }

void UILabel::Init() {
    TheUI->InitResources("UILabel");
    REGISTER_OBJ_FACTORY(UILabel)
    UILabelDir::Init();
}

void UILabel::Terminate() {}

BEGIN_COPYS(UILabel)
    CREATE_COPY_AS(UILabel, f)
    MILO_ASSERT(f, 96);
    COPY_SUPERCLASS(UIComponent)
    Update();
END_COPYS

void UILabel::CopyMembers(const UIComponent *o, Hmx::Object::CopyType ty) {
    UIComponent::CopyMembers(o, ty);
    const UILabel *l = dynamic_cast<const UILabel *>(o);
    MILO_ASSERT(l, 0x6A);
    COPY_MEMBER_FROM(l, mTextToken)
    COPY_MEMBER_FROM(l, mIcon)
    COPY_MEMBER_FROM(l, mTextSize)
    COPY_MEMBER_FROM(l, mCapsMode)
    COPY_MEMBER_FROM(l, mAlignment)
    COPY_MEMBER_FROM(l, mMarkup)
    COPY_MEMBER_FROM(l, mLeading)
    COPY_MEMBER_FROM(l, mKerning)
    COPY_MEMBER_FROM(l, mItalics)
    COPY_MEMBER_FROM(l, mFitType)
    COPY_MEMBER_FROM(l, mWidth)
    COPY_MEMBER_FROM(l, mHeight)
    COPY_MEMBER_FROM(l, mFixedLength)
    COPY_MEMBER_FROM(l, mAlpha)
    COPY_MEMBER_FROM(l, mColorOverride)
    COPY_MEMBER_FROM(l, mPreserveTruncText)
    if (mFixedLength != 0)
        mText->SetFixedLength(mFixedLength);
    COPY_MEMBER_FROM(l, mReservedLine)
    if (mReservedLine != 0)
        mText->ReserveLines(mReservedLine);
    COPY_MEMBER_FROM(l, mLabelText)
    COPY_MEMBER_FROM(l, mUseHighlightMesh)
    COPY_MEMBER_FROM(l, mAltTextSize)
    COPY_MEMBER_FROM(l, mAltKerning)
    COPY_MEMBER_FROM(l, mAltTextColor)
    COPY_MEMBER_FROM(l, mAltZOffset)
    COPY_MEMBER_FROM(l, mAltItalics)
    COPY_MEMBER_FROM(l, mAltAlpha)
    COPY_MEMBER_FROM(l, mAltStyleEnabled)
    COPY_MEMBER_FROM(l, mFontMatVariation)
    COPY_MEMBER_FROM(l, mAltMatVariation)
    COPY_MEMBER_FROM(l, mAltFontResourceName)
    COPY_MEMBER_FROM(l, mObjDirPtr)
}

// retail 0x827F2E98 -- a REAL serializer at rev 0x18. (The Wii DEV build has
// `SAVE_OBJ(UILabel, 173)`, an assert stub: a retail-vs-dev divergence.) Field
// order mirrors PreLoad's newest-revision read order.
BEGIN_SAVES(UILabel)
    SAVE_REVS(0x18, 0)
    SAVE_SUPERCLASS(UIComponent)
    bs << mTextToken;
    bs << mEditText;
    bs << mIcon;
    bs << mTextSize;
    bs << (int)mAlignment;
    bs << (int)mCapsMode;
    bs << mMarkup;
    bs << mLeading;
    bs << mKerning;
    bs << mItalics;
    bs << (int)mFitType;
    bs << mWidth;
    bs << mHeight;
    bs << mFixedLength;
    bs << mReservedLine;
    bs << mPreserveTruncText;
    bs << mAlpha;
    bs << mColorOverride;
    bs << mUseHighlightMesh;
    bs << mAltTextSize;
    bs << mAltTextColor;
    bs << mAltStyleEnabled;
    bs << mAltKerning;
    bs << mAltZOffset;
    bs << mFontMatVariation;
    bs << mAltFontResourceName;
    bs << mAltMatVariation;
    bs << mAltItalics;
    bs << mAltAlpha;
END_SAVES

void UILabel::Load(BinStream &bs) {
    PreLoad(bs);
    PostLoad(bs);
}

// retail 0x827F4EC8 (mislabeled `?Copy@UILabel@@...` in
// scripts/target_symbol_map.json -- reported to the lane lead, map NOT edited).
// Retail-vs-Wii-dev divergences encoded here:
//   * mAlignment / mCapsMode / mFitType / mFixedLength / mReservedLine are read
//     as 4 raw bytes straight into the member (no int temporary + MILO_ASSERT
//     narrowing pass the dev build does).
//   * the gRev > 0xD string goes into the real member mEditText, not a local.
void UILabel::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(0x18, 0)
    UIComponent::PreLoad(bs);
    if (gRev != 0 && gRev < 0xE) {
        bool b;
        bs >> b;
    }
    bs >> mTextToken;
    if (gRev > 0xD)
        bs >> mEditText;
    if (gRev > 0xE)
        bs >> mIcon;
    if (gRev > 1) {
        bs >> mTextSize >> (int &)mAlignment >> (int &)mCapsMode;
        if (gRev > 7) {
            bs >> mMarkup;
        }
        bs >> mLeading >> mKerning;
    }
    if (gRev > 4)
        bs >> mItalics;
    if (gRev > 2) {
        bs >> (int &)mFitType;
        bs >> mWidth >> mHeight;
    }
    if (gRev < 4) {
        Transform xfm = LocalXfm();
        if (mAlignment & 1) {
            xfm.v.x -= mWidth / 2.0f;
        } else if (mAlignment & 4) {
            xfm.v.x += mWidth / 2.0f;
        }
        if (mAlignment & 0x10) {
            xfm.v.z += mHeight / 2.0f;
        } else if (mAlignment & 0x40) {
            xfm.v.z -= mHeight / 2.0f;
        }
        SetLocalXfm(xfm);
    }
    if (gRev > 5)
        bs >> mFixedLength;
    if (gRev > 6)
        bs >> mReservedLine;
    if (gRev >= 9 && gRev <= 15) {
        bool b;
        int a, c, d;
        bs >> b >> a >> c >> d;
    }
    if (gRev > 9)
        bs >> mPreserveTruncText;
    if (gRev > 10)
        bs >> mAlpha;
    if (gRev > 0xC)
        bs >> mColorOverride;
    if (gRev > 0x10) {
        bs >> mUseHighlightMesh;
    }
    if (gRev > 0x11) {
        bs >> mAltTextSize >> mAltTextColor;
        bs >> mAltStyleEnabled;
    }
    if (gRev > 0x12)
        bs >> mAltKerning;
    else
        mAltKerning = mKerning;
    if (gRev > 0x13)
        bs >> mAltZOffset;
    if (gRev > 0x14)
        bs >> mFontMatVariation;
    if (gRev > 0x15) {
        bs >> mAltFontResourceName;
        AltFontResourceFileUpdated(true);
    }
    if (gRev > 0x16)
        bs >> mAltMatVariation;
    if (gRev > 0x17) {
        bs >> mAltItalics >> mAltAlpha;
    }
}

// retail 0x827F76B0 (mislabeled `?Load@UILabel@@...` in target_symbol_map.json).
// Retail adds the middle `mEditText && AllowEditText()` arm that the Wii dev
// build lacks.
void UILabel::PostLoad(BinStream &bs) {
    UIComponent::PostLoad(bs);
    LabelUpdate(false, true);
    sDeferUpdate = true;
    if (!mIcon.empty())
        mLabelText = mIcon;
    else if (!mEditText.empty() && AllowEditText())
        mLabelText = mEditText;
    else
        SetTextToken(mTextToken);
    if (sRequireFixedLength) {
        if (mFixedLength == 0) {
            MILO_WARN(
                "%s: %s is preloaded, but doesn't have fixed length",
                PathName(Dir()),
                Name()
            );
        }
        if (mReservedLine == 0) {
            MILO_WARN(
                "%s: %s is preloaded, but doesn't have reserve lines",
                PathName(Dir()),
                Name()
            );
        }
    }
    sDeferUpdate = false;
    if (!mTextToken.Null() || !mIcon.empty() || !ResourceDir() || mFixedLength != 0
        || mReservedLine != 0)
        Update();
    else
        mText->SetFont(nullptr);
    if (!mAltFontResourceName.empty())
        mObjDirPtr.PostLoad(nullptr);
}

// retail UILabel own-virtual @ vtable slot 0x50. mAlpha is 0x1bc.
void UILabel::Draw() {
    if (!(mAlpha <= 0))
        RndDrawable::Draw();
}

// retail 0x827F4910. NOTE: retail inlines RndText member loads here
// (`lwz r11, 0xec(r11)` = RndText::mFont); our RndText is the DC3-shaped one, so
// this body is CAPPED until RndText's layout is reconstructed. Kept structurally
// faithful anyway.
void UILabel::DrawShowing() {
    if (mAlpha <= 0)
        return;
    if (mText->GetFont()) {
        mText->GetFont()->Mat()->SetAlpha(mAlpha);
        if (mAltStyleEnabled && AltFont()) {
            RndMat *fontMat = AltFont()->Mat();
            if (fontMat)
                fontMat->SetAlpha(mAltAlpha);
        }
    } else
        Update();

    if (mColorOverride) {
        RndMat *fontMat = mText->GetFont()->Mat();
        if (fontMat) {
            fontMat->SetColor(mColorOverride->GetColor());
        }
    } else {
        Hmx::Color color;
        mLabelDir->GetStateColor(mState, color);
        RndMat *fontMat = mText->GetFont()->Mat();
        if (fontMat)
            fontMat->SetColor(color);
    }

    if (mAltStyleEnabled && AltFont()) {
        if (mAltTextColor) {
            RndMat *fontMat = AltFont()->Mat();
            if (fontMat) {
                fontMat->SetColor(mAltTextColor->GetColor());
            }
        } else {
            Hmx::Color color;
            mLabelDir->GetStateColor(mState, color);
            RndMat *fontMat = AltFont()->Mat();
            if (fontMat)
                fontMat->SetColor(color);
        }
    }

    UpdateAndDrawHighlightMesh();
    mText->DrawShowing();
    if (sDebugHighlight && !sInDebugHighlight) {
        sInDebugHighlight = true;
        Highlight();
        sInDebugHighlight = false;
    }
}

float UILabel::GetDrawWidth() {
    float w = 0.0f;
    float h = 0.0f;
    mText->GetCurrentStringDimensions(w, h);
    return w;
}

float UILabel::GetDrawHeight() {
    float w = 0.0f;
    float h = 0.0f;
    mText->GetCurrentStringDimensions(w, h);
    return h;
}

void UILabel::UpdateAndDrawHighlightMesh() {
    RndGroup *meshgroup = mLabelDir->HighlighMeshGroup();
    if (mUseHighlightMesh && meshgroup && GetState() == UIComponent::kFocused) {
        RndMesh *topleft = mLabelDir->TopLeftHighlightBone();
        RndMesh *topright = mLabelDir->TopRightHighlightBone();
        RndMesh *botleft = mLabelDir->BottomLeftHighlightBone();
        RndMesh *botright = mLabelDir->BottomRightHighlightBone();
        if (topleft && topright && botleft && botright) {
            float f1 = 0;
            float f2 = 0;
            mText->GetCurrentStringDimensions(f1, f2);
            Vector3 v80, v74;
            InqMinMaxFromWidthAndHeight(f1, f2, Alignment(), v74, v80);
            float x1 = v74.x;
            float x2 = v80.x;
            float z2 = v80.z;
            float z1 = v74.z;
            mLabelDir->SetWorldXfm(WorldXfm());
            topleft->SetLocalPos(x1, 0, z2);
            topright->SetLocalPos(x2, 0, z2);
            botleft->SetLocalPos(x1, 0, z1);
            botright->SetLocalPos(x2, 0, z1);
        }
        RndEnviron *env = meshgroup->GetEnv();
        if (env) {
            env->SetAmbientAlpha(mAlpha);
        }
        meshgroup->Draw();
    }
}

void UILabel::SetUseHighlightMesh(bool b) {
    mUseHighlightMesh = b;
    Update();
}

int UILabel::InqMinMaxFromWidthAndHeight(
    float f1, float f2, RndText::Alignment a, Vector3 &v1, Vector3 &v2
) {
    v1.Zero();
    v2.Zero();
    if (a & 1) {
        v1.x = 0;
        v2.x = f1;
    } else if (a & 2) {
        v1.x = -f1 / 2.0f;
        v2.x = f1 / 2.0f;
    } else if (a & 4) {
        v1.x = -f1;
        v2.x = 0;
    }

    if (a & 0x10) {
        v1.z = -f2;
        v2.z = 0;
    } else if (a & 0x20) {
        v1.z = -f2 / 2.0f;
        v2.z = f2 / 2.0f;
    } else if (a & 0x40) {
        v1.z = 0;
        v2.z = f2;
    }
    return 1;
}

void UILabel::Highlight() {
    RndTransformable::Highlight();
    Vector3 v3c, v48;
    InqMinMaxFromWidthAndHeight(mWidth, mHeight, Alignment(), v3c, v48);
    Box box(v3c, v48);
    Hmx::Color color(1, 1, 0.5f, 1);
    if (!CheckValid(false)) {
        int secs = TheTaskMgr.UISeconds() * 2.0f;
        if (!(secs % 2)) {
            color.Set(1.0f, 0.2f, 0.2f, 1.0f);
        }
    }
    mText->Highlight();
    UtilDrawBox(WorldXfm(), box, color, false);
}

Symbol UILabel::TextToken() { return mTextToken; }

const char *UILabel::GetDefaultText() const {
    if (!mIcon.empty())
        return mIcon.c_str();
    else
        return Localize(mTextToken, nullptr);
}

// retail 0x827F4B68. The Wii DEV build has this as an EMPTY stub -- a
// retail-vs-dev divergence; ported from the retail asm instead.
void UILabel::SetEditText(const char *cc) {
    mEditText = cc;
    if (mIcon.c_str()[0] == '\0') {
        if (mEditText.c_str()[0] == '\0') {
            SetTokenFmtImp(mTextToken, nullptr, nullptr, 0, true);
        } else {
            char buf[0x100];
            ASCIItoUTF8(buf, 0x100, cc);
            SetDisplayText(buf, true);
        }
    }
}

void UILabel::SetTextToken(Symbol s) {
    mTextToken = s;
    SetTokenFmtImp(mTextToken, 0, 0, 0, true);
}

void UILabel::SetInt(int i, bool b) {
    if (b) {
        SetDisplayText(LocalizeSeparatedInt(i), true);
    } else
        SetDisplayText(MakeString("%d", i), true);
}

void UILabel::SetFloat(const char *cc, float f) {
    SetDisplayText(LocalizeFloat(cc, f), true);
}

void UILabel::SetIcon(char c) {
    mIcon = MakeString("%c", c);
    if (!mIcon.empty() || !TheLoadMgr.EditMode()) {
        SetDisplayText(mIcon.c_str(), !TheLoadMgr.EditMode());
    }
}

void UILabel::AppendIcon(char c) {
    SetDisplayText(MakeString("%s%c", mLabelText, c), true);
}

void UILabel::SetDateTime(const DateTime &dt, Symbol s) {
    String str(Localize(s, false));
    dt.Format(str);
    SetDisplayText(str.c_str(), true);
}

void UILabel::SetSubtitle(const DataArray *da) { SetDisplayText(da->Str(2), true); }

void UILabel::SetPrelocalizedString(String &s) { SetDisplayText(s.c_str(), true); }

void UILabel::SetTimeHMS(int i1, bool b2) {
    int hrs = Min(99, i1 / 3600);
    int mins = Min(99, i1 / 0x3c + hrs * -0x3c);
    int secs = Min(99, i1 + (hrs * 0x3c + mins) * -0x3c);
    if (hrs > 0 || b2) {
        SetDisplayText(MakeString("%02d:%02d:%02d", hrs, mins, secs), true);
    } else {
        SetDisplayText(MakeString("%d:%02d", mins, secs), true);
    }
}

void UILabel::SetTokenFmt(const DataArray *da) {
    da->Evaluate(0);
    bool b = da->Size() > 1 && da->Evaluate(1).Type() == kDataArray;
    if (b) {
        SetTokenFmtImp(da->ForceSym(0), da->Array(1), da, 2, false);
    } else {
        SetTokenFmtImp(da->ForceSym(0), 0, da, 1, false);
    }
}

void UILabel::SetDisplayText(const char *cc, bool b) {
    if (b)
        mTextToken = gNullStr;
    mLabelText = cc;
    Update();
}

void UILabel::SetColorOverride(UIColor *col) { mColorOverride = col; }

bool UILabel::CheckValid(bool warn) {
    if (mFixedLength != 0 && (int)mLabelText.length() > mFixedLength) {
        if (warn) {
            MILO_WARN(
                "%s: %s has fixed length of %i but text is %i long (%s)",
                PathName(Dir()),
                Name(),
                mFixedLength,
                mLabelText.length(),
                mLabelText
            );
        }
        return false;
    } else if (mFitType == kFitWrap && mReservedLine != 0
               && mReservedLine < mText->NumLines()) {
        if (warn) {
            MILO_WARN(
                "%s: %s has reserve lines of %i, but text has %i lines (%s)",
                PathName(Dir()),
                Name(),
                mReservedLine,
                mText->NumLines(),
                mLabelText
            );
        }
        return false;
    } else
        return true;
}

void UILabel::Update() {
    if (!sDeferUpdate)
        LabelUpdate(false, false);
}

// retail 0x827F6258 -- TWO bool args in retail (like the Wii signature), not the
// one-arg DC3 form the tree previously declared.
void UILabel::LabelUpdate(bool b1, bool b2) {
    UIComponent::Update();
    MILO_ASSERT(ResourceDir(), 0x3CE);
    mLabelDir = dynamic_cast<UILabelDir *>(ResourceDir());
    MILO_ASSERT(mLabelDir, 0x3D1);
    if (!b2) {
        if (mReservedLine != 0) {
            mText->ReserveLines(mReservedLine);
        }
        RndFont *mainfont = Font();
        RndFont *altfont = AltFont();
        float basekern = mainfont->TextureOwner()->BaseKerning();
        mainfont->TextureOwner()->SetBaseKerning(mKerning + basekern);
        float altkern = 0;
        if (altfont && altfont != mainfont) {
            altkern = altfont->TextureOwner()->BaseKerning();
            altfont->TextureOwner()->SetBaseKerning(mAltKerning + altkern);
        }
        {
            RndTextUpdateDeferrer yuh(mText);
            mText->SetData(
                Alignment(),
                mLabelText.c_str(),
                mainfont,
                mLeading,
                mWidth,
                mTextSize,
                mItalics,
                mText->StyleColor(),
                mMarkup,
                mCapsMode,
                mFixedLength
            );
            Hmx::Color color;
            Hmx::Color *cPtr = nullptr;
            if (mAltTextColor) {
                color = mAltTextColor->GetColor();
                cPtr = &color;
            }
            mText->SetAltStyle(
                altfont, mAltTextSize, cPtr, mAltZOffset, mAltItalics, mAltStyleEnabled
            );
            FitText();
            if (b1) {
                mText->UpdateText(true);
            }
        }
        mainfont->TextureOwner()->SetBaseKerning(basekern);
        if (altfont && altfont != mainfont) {
            altfont->TextureOwner()->SetBaseKerning(altkern);
        }
        CheckValid(!TheLoadMgr.EditMode());
    }
}

// retail 0x827F2C58
RndFont *UILabel::AltFont() {
    if (mObjDirPtr) {
        UILabelDir *ldir = dynamic_cast<UILabelDir *>(mObjDirPtr.Ptr());
        if (!ldir)
            MILO_FAIL("bad UILabel alt font resource dir type!");
        RndText *t = ldir->TextObj(mAltMatVariation);
        if (!t) {
            MILO_WARN(
                "Label %s's alt font is referencing a mat variation '%s' that no longer exists, setting to default...",
                Name(),
                mAltMatVariation.Str()
            );
            mAltMatVariation = Symbol();
            t = ldir->TextObj(mAltMatVariation);
        }
        MILO_ASSERT(t, 0x430);
        RndFont *font = t->GetFont();
        MILO_ASSERT(font, 0x432);
        return font;
    } else
        return 0;
}

// retail 0x827F2CE8 -- the mat-variation cache
// (mCurFontMatVariation @0x160 is the cached copy of mFontMatVariation @0x1d0).
RndFont *UILabel::Font() {
    MILO_ASSERT(mLabelDir, 0x43B);
    if (mFont && mFontMatVariation == mCurFontMatVariation)
        return mFont;
    RndText *t = mLabelDir->TextObj(mFontMatVariation);
    if (!t) {
        MILO_WARN(
            "Label %s is referencing a mat variation '%s' that no longer exists, setting to default...",
            Name(),
            mFontMatVariation.Str()
        );
        mFontMatVariation = Symbol();
        t = mLabelDir->TextObj(mFontMatVariation);
    }
    MILO_ASSERT(t, 0x448);
    RndFont *font = t->GetFont();
    MILO_ASSERT(font, 0x44A);
    mFont = font;
    mCurFontMatVariation = mFontMatVariation;
    return mFont;
}

void UILabel::AltFontResourceFileUpdated(bool b) {
    if (!mAltFontResourceName.empty()) {
        const char *miloPath =
            MakeString("%s/%s.milo", GetResourcesPath(), mAltFontResourceName);
        mObjDirPtr.LoadFile(FilePath(FileRoot(), miloPath), b, true, kLoadFront, false);
        if (!b) {
            mObjDirPtr.PostLoad(nullptr);
        }
    } else
        mObjDirPtr = nullptr;
    if (!b) {
        Update();
    }
}

void UILabel::AdjustHeight(bool b) {
    if (mFitType == kFitWrap && mText->GetFont()) {
        HX_VECTOR(RndText::Line) lines;
        float f24;
        mText->GetStringDimensions(f24, mHeight, lines, "", mTextSize);
        int numlines;
        bool b1 = false;
        if (b && mReservedLine > 0)
            b1 = true;
        if (b1) {
            numlines = mReservedLine;
        } else
            numlines = mText->NumLines();
        mHeight *= numlines;
        mHeight = (1.0f - mLeading) * mTextSize * mText->GetFont()->CellDiff() + mHeight;
    }
}

void UILabel::SetAlignment(RndText::Alignment a) {
    mAlignment = a;
    Update();
}

void UILabel::SetCapsMode(RndText::CapsMode c) {
    mCapsMode = c;
    Update();
}

void UILabel::SetFitType(UILabel::FitType f) {
    mFitType = f;
    Update();
}

// retail 0x827F5550
void UILabel::FitText() {
    RndTextUpdateDeferrer deferrer(mText);
    if (mFitType == kFitStretch) {
        float linewidth = mText->MaxLineWidth();
        if (linewidth) {
            Transform tf;
            tf.Reset();
            float xscale = mWidth / linewidth;
            float f1, f2;
            mText->GetVerticalBounds(f1, f2);
            float fabs = std::fabs(f2 - f1);
            float fvec;
            bool doDiv = fabs > 0.0f && mHeight > 0.0f;
            if (doDiv) {
                fvec = mHeight / fabs;
            } else
                fvec = 1.0f;
            float diff = mText->GetFont()->CellDiff();
            Scale(tf.m.x, xscale, tf.m.x);
            Scale(tf.m.y, 1.0f, tf.m.y);
            Scale(tf.m.z, fvec / diff, tf.m.z);
            mText->SetLocalXfm(tf);
        }
    } else if (mFitType == kFitJust) {
        const char *text = mText->RawText().c_str();
        float size = mTextSize;
        HX_VECTOR(RndText::Line) lines;
        float sp14, sp10;
        while (true) {
            if (size < 0.0f) {
                size = 0.0f;
                break;
            }
            mText->GetStringDimensions(sp14, sp10, lines, text, size);
            if ((mWidth && sp14 > mWidth) || (mHeight && sp10 > mHeight)) {
                size -= 0.2f;
                continue;
            }
            break;
        }
        RndText *t = mText;
        if (size != t->Size()) {
            t->DeferUpdateText();
            float ratio = size / mTextSize;
            mText->SetSize(size);
            mText->SetAltSizeAndZOffset(mAltTextSize * ratio, mAltZOffset * ratio);
            t->ResolveUpdateText();
        } else {
            t->SetText(text);
        }
    } else if (mFitType == kFitEllipsis) {
        String ellipsis("...");
        String text(mText->RawText());
        int textLen = text.length();
        HX_VECTOR(RndText::Line) lines;
        unsigned int truncPos = text.rfind(mPreserveTruncText.c_str());
        unsigned int truncLen = mPreserveTruncText.length();
        if (truncPos == (unsigned int)(textLen - truncLen)) {
            ellipsis += mPreserveTruncText.c_str();
        }
        int ellipsisLen = ellipsis.length();
        float w, h;
        mText->GetStringDimensions(w, h, lines, text.c_str(), mTextSize);
        if (mTextSize > 0.0f && mWidth > 0.0f
            && (w > mWidth || (int)lines.size() > 1)) {
            text.insert(textLen, ellipsis.c_str());
            textLen = textLen + ellipsisLen;
            goto ell_check;
        ell_body : {
            unsigned int spacePos = text.find_last_of(' ');
            if (spacePos == String::npos || spacePos * 10 < (unsigned int)(textLen * 9)) {
                textLen -= 1;
                text.erase(textLen);
            } else {
                textLen = spacePos + ellipsisLen;
                text.erase(textLen);
            }
            for (int j = textLen - ellipsisLen, i = 0; i < ellipsisLen; j++, i++) {
                text[j] = ellipsis[i];
            }
            mText->GetStringDimensions(w, h, lines, text.c_str(), mTextSize);
        }
        ell_check:
            if (textLen <= 1)
                goto ell_done;
            if ((int)lines.size() > 1)
                goto ell_body;
            if (w >= mWidth)
                goto ell_body;
            {
                int lastCharIdx = (textLen - ellipsisLen) - 1;
                if (text[lastCharIdx] == ' ')
                    goto ell_body;
                if (text[lastCharIdx] == '.')
                    goto ell_body;
                if (text[lastCharIdx] == ',')
                    goto ell_body;
            }
        ell_done:;
        }
        mText->SetText(text.c_str());
    }
}

void UILabel::OnSetIcon(const char *cc) {
    if (strlen(cc) > 1)
        MILO_WARN("%s is not a valid icon, must be one character", cc);
    SetIcon(*cc);
}

DataNode UILabel::OnSetTokenFmt(const DataArray *da) {
    const DataNode &n = da->Evaluate(2);
    if (n.Type() == kDataArray) {
        DataArray *arr = n.Array();
        bool b = arr->Size() > 1 && arr->Evaluate(1).Type() == kDataArray;
        if (b) {
            SetTokenFmtImp(arr->ForceSym(0), arr->Array(1), arr, 2, false);
        } else
            SetTokenFmtImp(arr->ForceSym(0), 0, arr, 1, false);
    } else {
        bool b = da->Size() > 3 && da->Evaluate(3).Type() == kDataArray;
        if (b) {
            SetTokenFmtImp(da->ForceSym(2), da->Array(3), da, 4, false);
        } else {
            SetTokenFmtImp(da->ForceSym(2), 0, da, 3, false);
        }
    }
    return 1;
}

// retail 0x827F2D78 (opens `stw r4, 0x164(r3)` == mTextToken)
void UILabel::SetTokenFmtImp(
    Symbol s, const DataArray *da1, const DataArray *da2, int i, bool b
) {
    mTextToken = s;
    if (mTextToken.Null())
        SetDisplayText(gNullStr, true);
    else {
        bool found;
        const char *localized = Localize(mTextToken, &found);
        if (found) {
            SuperFormatString str(localized, da1, b, TheLocale, gNullStr);
            if (da2) {
                int size = da2->Size();
                if (size > i) {
                    do {
                        const DataNode &n = da2->Evaluate(i);
                        if (n.Type() == kDataSymbol) {
                            str << Localize(n.Sym(da2), 0);
                        } else {
                            str << n;
                        }
                        i++;
                    } while (i < size);
                }
            }
            SetDisplayText(str.FinalStr(), false);
        } else {
            SetDisplayText(localized, false);
        }
    }
}

DataNode UILabel::OnSetPrelocalizedString(const DataArray *a) {
    const DataNode &stringNode = a->Evaluate(2);
    MILO_ASSERT(stringNode.Type() == kDataString, 0x386);
    String str(stringNode.Str());
    SetPrelocalizedString(str);
    return 1;
}

DataNode UILabel::OnSetInt(const DataArray *da) {
    int i = da->Int(2);
    bool b = false;
    if (da->Size() > 3)
        b = da->Int(3);
    SetInt(i, b);
    return DataNode(1);
}

DataNode UILabel::OnSetTimeHMS(const DataArray *da) {
    int num;
    if (da->Type(2) == kDataFloat) {
        num = da->Float(2);
    } else {
        num = da->Int(2);
    }
    SetTimeHMS(num, true);
    return 1;
}

void UILabel::CenterWithLabel(UILabel *label, bool b, float f) {
    int num = 1;
    if (b)
        num = -1;
    Transform xfm = LocalXfm();
    float otherwidth = label->mText->MaxLineWidth();
    float spaceBetween = f;
    Transform otherxfm = label->LocalXfm();
    float centerX = otherxfm.v.x;
    float width = mText->MaxLineWidth();
    otherxfm.v.x = (float)num * (otherwidth * 0.5f + spaceBetween * 0.5f) + centerX;
    xfm.v.x = centerX - (float)num * (width * 0.5f + spaceBetween * 0.5f);
    SetLocalXfm(xfm);
    label->SetLocalXfm(otherxfm);
}

DataNode UILabel::OnGetMaterialVariations(const DataArray *da) {
    int count = mLabelDir->NumMatVariations();
    DataArray *arr = new DataArray(count + 1);
    arr->Node(0) = DataNode(Symbol());
    for (int i = 1; i <= count; i++) {
        arr->Node(i) = DataNode(mLabelDir->GetMatVariationName(i - 1));
    }
    DataNode ret = DataNode(arr, kDataArray);
    arr->Release();
    return ret;
}

DataNode UILabel::OnGetAltMaterialVariations(const DataArray *da) {
    if (mObjDirPtr) {
        UILabelDir *labeldir = dynamic_cast<UILabelDir *>(mObjDirPtr.Ptr());
        int count = labeldir->NumMatVariations();
        DataArray *arr = new DataArray(count + 1);
        arr->Node(0) = DataNode(Symbol());
        for (int i = 1; i <= count; i++) {
            arr->Node(i) = DataNode(labeldir->GetMatVariationName(i - 1));
        }
        DataNode ret = DataNode(arr, kDataArray);
        arr->Release();
        return ret;
    } else {
        DataArray *arr = new DataArray(1);
        arr->Node(0) = DataNode(Symbol());
        DataNode ret = DataNode(arr, kDataArray);
        arr->Release();
        return ret;
    }
}

BEGIN_HANDLERS(UILabel)
    HANDLE_EXPR(
        get_string_width, mText->GetStringWidthUTF8(_msg->Str(2), NULL, false, NULL)
    )
    HANDLE_ACTION(adjust_height, AdjustHeight(true))
    HANDLE(set_token_fmt, OnSetTokenFmt)
    HANDLE(set_int, OnSetInt)
    HANDLE_ACTION(set_float, SetFloat(_msg->Str(2), _msg->Float(3)))
    HANDLE_ACTION(
        center_with_label,
        CenterWithLabel(_msg->Obj<UILabel>(2), _msg->Int(3), _msg->Float(4))
    )
    HANDLE_EXPR(has_highlight_mesh, HasHighlightMesh())
    HANDLE(get_material_variations, OnGetMaterialVariations)
    HANDLE(get_altmaterial_variations, OnGetAltMaterialVariations)
    HANDLE_SUPERCLASS(UIComponent)
END_HANDLERS

BEGIN_PROPSYNCS(UILabel)
    SYNC_PROP_SET(text_token, mTextToken, SetTextToken(_val.ForceSym()))
    SYNC_PROP_SET(icon, mIcon.c_str(), OnSetIcon(_val.Str()))
    SYNC_PROP_SET(edit_text, mEditText.c_str(), SetEditText(_val.Str()))
    // retail-vs-Wii-dev divergence: retail stores/loads the RAW float here (no
    // GetPctHeightFromTextSize / GetTextSizeFromPctHeight conversion).
    SYNC_PROP_SET(text_size, mTextSize, mTextSize = _val.Float(); Update())
    SYNC_PROP_SET(
        alignment, (int &)mAlignment, SetAlignment((RndText::Alignment)_val.Int())
    )
    SYNC_PROP_SET(
        caps_mode, (int &)mCapsMode, SetCapsMode((RndText::CapsMode)_val.Int())
    )
    SYNC_PROP_SET(markup, mMarkup, mMarkup = _val.Int(); Update())
    SYNC_PROP_MODIFY(leading, mLeading, Update())
    SYNC_PROP_MODIFY(kerning, mKerning, LabelUpdate(true, false))
    SYNC_PROP_MODIFY(italics, mItalics, Update())
    SYNC_PROP_SET(fit_type, (int &)mFitType, SetFitType((FitType)_val.Int()))
    SYNC_PROP_MODIFY(width, mWidth, Update())
    SYNC_PROP_MODIFY(height, mHeight, Update())
    SYNC_PROP_MODIFY(fixed_length, mFixedLength, Update())
    SYNC_PROP_MODIFY(reserve_lines, mReservedLine, Update())
    SYNC_PROP_MODIFY(preserve_trunc_text, mPreserveTruncText, Update())
    SYNC_PROP_SET(use_highlight_mesh, mUseHighlightMesh, SetUseHighlightMesh(_val.Int()))
    SYNC_PROP(color_override, mColorOverride)
    SYNC_PROP(alpha, mAlpha)
    SYNC_PROP_MODIFY(
        alt_font_resource_name, mAltFontResourceName, AltFontResourceFileUpdated(false)
    )
    SYNC_PROP_SET(alt_text_size, mAltTextSize, mAltTextSize = _val.Float(); Update())
    SYNC_PROP_MODIFY(alt_kerning, mAltKerning, Update())
    SYNC_PROP_MODIFY(alt_text_color, mAltTextColor, Update())
    SYNC_PROP_MODIFY(alt_z_offset, mAltZOffset, Update())
    SYNC_PROP_MODIFY(alt_italics, mAltItalics, Update())
    SYNC_PROP_MODIFY(alt_alpha, mAltAlpha, Update())
    SYNC_PROP_SET(
        alt_style_enabled, mAltStyleEnabled, mAltStyleEnabled = _val.Int(); Update()
    )
    SYNC_PROP_MODIFY(font_mat_variation, mFontMatVariation, LabelUpdate(false, false))
    SYNC_PROP_MODIFY(alt_mat_variation, mAltMatVariation, Update())
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS

bool UILabel::AllowEditText() const {
    if (TheUI->DefaultAllowEditText())
        return true;
    else if (mLabelDir)
        return mLabelDir->AllowEditText();
    else
        return false;
}

bool UILabel::HasHighlightMesh() const { return mLabelDir->HighlighMeshGroup() != 0; }

// ---------------------------------------------------------------------------
// Retail RB3 has NO ObjVector<LabelStyle> UILabel member. These two PropSync
// overloads are retained ANYWAY: they are what instantiate the
// ObjVector<UILabel::LabelStyle> template bodies that the retail UILabel unit
// pins (ICF-merged generic STL code, target addrs 0x822A6878 / 0x8234B270 /
// 0x8234B4D0 / 0x8234D080, all outside the 0x827F2148..0x827F7AD8 span).
// Dropping them would unpair four already-matching functions for no gain.
// ---------------------------------------------------------------------------
BEGIN_CUSTOM_PROPSYNC(UILabel::LabelStyle)
    SYNC_PROP(font_resource, o.mFontResource)
    SYNC_PROP(color_override, o.mColorOverride)
END_CUSTOM_PROPSYNC

bool PropSync(
    ObjVector<UILabel::LabelStyle> &v, DataNode &val, DataArray *prop, int i, PropOp op
) {
    if (op == kPropUnknown0x40)
        return false;
    else if (i == prop->Size()) {
        MILO_ASSERT(op == kPropSize, 0x4A9);
        val = (int)v.size();
        return true;
    } else {
        int idx = prop->Int(i++);
        ObjVector<UILabel::LabelStyle>::iterator labelIt = v.begin() + idx;
        if (i < prop->Size() || op & (kPropGet | kPropSet | kPropSize)) {
            return PropSync(*labelIt, val, prop, i, op);
        } else if (op == kPropRemove) {
            if (v.size() > 1) {
                v.erase(labelIt);
            }
            return true;
        } else if (op == kPropInsert) {
            UILabel::LabelStyle labelStyle(v.Owner());
            if (PropSync(labelStyle, val, prop, i, op)) {
                if (v.size() < 8) {
                    v.insert(labelIt, labelStyle);
                }
                return true;
            }
        }
        return false;
    }
}


// The retail unit pins four generic ObjVector<UILabel::LabelStyle> template
// bodies (dtor, vector dtor, operator=, _M_allocate_and_copy). With the DC3-only
// `mLabelStyles` member gone nothing instantiates them any more, which unpairs
// four already-matching functions -- so instantiate them explicitly here.
void UILabelForceLabelStyleTemplates(
    ObjVector<UILabel::LabelStyle> &a, const ObjVector<UILabel::LabelStyle> &b
) {
    a = b;
    ObjVector<UILabel::LabelStyle> tmp(a.Owner());
    tmp.resize(1);
}

#pragma pop_macro("ASSERT_REVS")
#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")
