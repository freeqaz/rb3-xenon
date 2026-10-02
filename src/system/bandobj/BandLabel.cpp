#include "bandobj/BandLabel.h"
#include "obj/Task.h"
#include "rndobj/Anim.h"
#include "ui/UI.h"
#include "ui/UILabel.h"
#include "ui/UITransitionHandler.h"
#include "utl/BinStream.h"
#include "utl/Locale.h"
#include "utl/Str.h"
#include "utl/Symbols.h"

INIT_REVS(0x11, 0)

void BandLabel::Init() {
    TheUI->InitResources("BandLabel");
    Register();
}

BandLabel::BandLabel() : UITransitionHandler(this), unk1e8(""), unk1f4(0) {}

BandLabel::~BandLabel() {}

BEGIN_COPYS(BandLabel)
    COPY_SUPERCLASS(UILabel)
    CREATE_COPY(BandLabel)
    BEGIN_COPYING_MEMBERS
        CopyHandlerData(c);
    END_COPYING_MEMBERS
END_COPYS

BEGIN_SAVES(BandLabel)
    SAVE_REVS(0x11, 0)
    SAVE_SUPERCLASS(UILabel)
    SaveHandlerData(bs);
END_SAVES

void BandLabel::Load(BinStream &bs) {
    PreLoad(bs);
    PostLoad(bs);
}

// Retail stores the rev words through ONE base register (lbl_82CBE3A8:
// altRev+0, rev+4) -- the ObjMacros.h gRev dialect with internal-linkage,
// align(4) file-scope statics, not this TU's Object.h BinStreamRev dialect.
// Same lever as BandButton.cpp / BandCrowdMeter.cpp.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gBandLabelRevs;
#define gAltRev gBandLabelRevs.altRev
#define gRev gBandLabelRevs.rev

void BandLabel::PreLoad(BinStream &bs) {
    Hmx::Color col;
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    bool b87 = false;
    if (gRev < 0xB) {
        if (gRev <= 6) {
            RndTransformable::Load(bs);
            RndDrawable::Load(bs);
        } else
            UILabel::PreLoad(bs);

        if (gRev > 5) {
            if (gRev < 10) {
                bool b88;
                bs >> b88 >> mWidth >> mHeight;
                if (b88)
                    mFitType = kFitStretch;
                else
                    mFitType = kFitWrap;
            } else {
                int i50;
                bs >> i50;
                bs >> mWidth;
                bs >> mHeight;
                mFitType = (FitType)i50;
            }
        } else
            mFitType = kFitWrap;

        if (gRev < 8 && mFitType == kFitStretch) {
            Hmx::Matrix3 m;
            m.Identity();
            SetLocalRot(m);
        }
        if (gRev > 4)
            bs >> mLeading;
        if (gRev > 3)
            bs >> (int &)mAlignment;
        if (gRev < 2) {
            int i, j, k, l;
            bs >> i >> j >> k >> l;
        }
        if (gRev <= 6) {
            Symbol s;
            bs >> s;
            SetType(s);
        }
        if (gRev != 0)
            bs >> b87;
        else
            b87 = false;
        if (gRev <= 6)
            bs >> mTextToken;
        if (gRev < 10) {
            int i;
            bs >> i;
        }
        if (gRev > 8)
            bs >> col;
        if (gRev > 9) {
            bs >> mKerning;
            bs >> mTextSize;
        }
    } else {
        UILabel::PreLoad(bs);
        if (gRev < 0xE) {
            int i6c;
            bs >> i6c;
            mFitType = (FitType)i6c;
            bs >> mWidth;
            bs >> mHeight;
            if (mFitType == kFitWrap) {
                mHeight = 0;
                mWidth = 0;
            }
        }
        if (gRev < 0xD) {
            bs >> mLeading;
            bs >> (int &)mAlignment;
        }
        if (gRev < 0xF) {
            int i, j, k, l;
            bs >> i >> j >> k >> l;
        }
        if (gRev < 0xD) {
            bs >> b87 >> mKerning >> mTextSize;
        }
        if (gRev < 0xE) {
            int i;
            bs >> i;
        }
        if (gRev < 0xF)
            bs >> col;
    }
    if (gRev < 0xD)
        mCapsMode = (RndText::CapsMode)(b87 ? 2 : 0);
    if (gRev == 0xF)
        LoadOldBandTextComp(bs);
    if (gRev >= 0x11)
        LoadHandlerData(bs);
}
#undef gRev
#undef gAltRev

void BandLabel::LoadOldBandTextComp(BinStream &bs) {
    int rev;
    bs >> rev;
    Symbol s;
    if (rev > 2)
        MILO_WARN("Can't load new BandTextComp");
    else {
        if (rev < 1) {
            int a, b, c, d;
            bs >> a >> b >> c >> d;
        }
        bs >> s;
        static Symbol custom_colors("custom_colors");
        if (s == custom_colors) {
            int dummy;
            int num = 4;
            if (rev >= 2)
                bs >> num;
            for (int i = 0; i < num; i++)
                bs >> dummy;
        }
    }
}

void BandLabel::Poll() {
    UILabel::Poll();
    if (unk1dc.size() >= 2) {
        float val = 0;
        float uisecs = TheTaskMgr.UISeconds() * 1000.0f;
        unk1dc.AtFrame(uisecs, val);
        SetTokenFmt(unk1e4, LocalizeSeparatedInt(val));
        if (uisecs > unk1dc.LastFrame()) {
            unk1dc.clear();
            BandLabelCountDoneMsg msg(this);
            TheUI->Handle(msg, false);
        }
    }
    UpdateHandler();
}

void BandLabel::Count(int i1, int i2, float f, Symbol s) {
    unk1dc.clear();
    // The keys are timed in UI milliseconds (Poll samples them with
    // AtFrame(uisecs)) and carry the count as their value.
    Key<float> key;
    key.frame = TheTaskMgr.UISeconds() * 1000.0f;
    key.value = i1;
    unk1dc.push_back(key);
    key.frame += f;
    key.value = i2;
    unk1dc.push_back(key);
    unk1e4 = s;
}

void BandLabel::FinishCount() {
    if (unk1dc.size() >= 2) {
        Key<float> &key = unk1dc[1];
        SetTokenFmt(unk1e4, LocalizeSeparatedInt(key.value));
        unk1dc.clear();
    }
}

bool BandLabel::IsEmptyValue() const { return mLabelText == gNullStr; }

void BandLabel::FinishValueChange() {
    UILabel::SetDisplayText(unk1e8.c_str(), unk1f4);
    UITransitionHandler::FinishValueChange();
}

void BandLabel::SetDisplayText(const char *cc, bool b) {
    unk1e8 = cc;
    unk1f4 = b;
    UITransitionHandler::StartValueChange();
}

BEGIN_HANDLERS(BandLabel)
    HANDLE_ACTION(
        start_count, Count(_msg->Int(2), _msg->Int(3), _msg->Float(4), _msg->Sym(5))
    )
    HANDLE_ACTION(finish_count, FinishCount())
    HANDLE_SUPERCLASS(UILabel)
END_HANDLERS

BEGIN_PROPSYNCS(BandLabel)
    SYNC_PROP_SET(in_anim, GetInAnim(), SetInAnim(_val.Obj<RndAnimatable>()))
    SYNC_PROP_SET(out_anim, GetOutAnim(), SetOutAnim(_val.Obj<RndAnimatable>()))
    SYNC_SUPERCLASS(UILabel)
END_PROPSYNCS

// sw2 scatter-include (default/BandLabel <- bandobj/BandDirector.cpp)
#define gRev gRev_BandDirector
#define gAltRev gAltRev_BandDirector
#include "bandobj/BandDirector.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/BandLabel <- hamobj/HamLabel.cpp)
#define gRev gRev_HamLabel
#define gAltRev gAltRev_HamLabel
#include "hamobj/HamLabel.cpp"
#undef gRev
#undef gAltRev

// ZS-MISSING-INSTANTIATION: retail out-of-lined this SetTokenFmt<char*> COMDAT
// in this TU; force emission (BandWardrobe idiom).
template void UILabel::SetTokenFmt<char *>(Symbol, char *);
