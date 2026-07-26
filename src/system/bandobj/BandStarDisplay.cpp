#include "bandobj/BandStarDisplay.h"
#include "math/Utl.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "rndobj/Anim.h"
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "synth/Sequence.h"
#include "utl/BinStream.h"
#include "utl/Str.h"
#include "utl/Symbols.h"
#include <math.h>

INIT_REVS(BandStarDisplay);

BandStarDisplay::BandStarDisplay()
    : mNumStars(0), mStars(this), mStarSweepAnims(this), mStarFullTriggers(this),
      mStarGoldTriggers(this), mStarOffsetAnim(this, 0), mEarnStarSfx(this, 0),
      mEarnSpadeSfx(this, 0), mStarType("normal") {}

BandStarDisplay::~BandStarDisplay() {}

void BandStarDisplay::SetNumStars(float f, bool b) {
    static Symbol tour("tour");
    static Symbol dest("dest");
    SetupStars();
    if (f < 0.0f)
        return;
    int size = (int)mStars.size();
    f = Min<float>(f, (float)(size + 1));
    if (f < mNumStars)
        ResetStars();
    if (f != mNumStars) {
        int i = (int)mNumStars;
        int newStar = (int)f;
        double intPart;
        float fracPart = (float)modf(f, &intPart);
        if (newStar > i) {
            bool animated = false;
            while (newStar > i && i < size) {
                mStarSweepAnims[i]->SetFrame(1.0f, 1.0f);
                mStarFullTriggers[i]->Trigger();
                i++;
                if (i < size) {
                    mStars[i]->SetShowing(true);
                }
                animated = true;
            }
            if (newStar == size + 1) {
                for (int j = 0; j < size; j++) {
                    mStarGoldTriggers[j]->Trigger();
                }
            }
            if (animated) {
                mStarOffsetAnim->Animate(
                    0.0f, false, 0.0f, RndAnimatable::k30_fps_ui,
                    mStarOffsetAnim->GetFrame(), 10.0f * (float)i, 0.0f, 1.0f, dest
                );
                if (b) {
                    if (mStarType == tour)
                        mEarnSpadeSfx->Play(0.0f, 0.0f, 0.0f);
                    else
                        mEarnStarSfx->Play(0.0f, 0.0f, 0.0f);
                }
            }
        }
        if (mStarType == tour && newStar < size) {
            EventTrigger *et =
                mStars[i]->Find<EventTrigger>("pulse_success.trig", true);
            et->Trigger();
        }
        if (i < size) {
            RndAnimatable *anim = mStarSweepAnims[i];
            anim->SetFrame(Min<float>(fracPart, 0.95f), 1.0f);
        }
        mNumStars = f;
    }
}

void BandStarDisplay::SyncObjects() {
    RndDir::SyncObjects();
    Reset();
}

void BandStarDisplay::Reset() { ResetStars(); }

void BandStarDisplay::SetupStars() {
    if (mStars.empty()) {
        mStarSweepAnims.clear();
        mStarFullTriggers.clear();
        for (int i = 0; i < 5; i++) {
            const char *starName = MakeString("star%d", i);
            RndDir *stardir = Find<RndDir>(starName, true);
            mStars.push_back(ObjPtr<RndDir>(this, stardir));

            RndAnimatable *anim = stardir->Find<RndAnimatable>("sweep.mnm", true);
            mStarSweepAnims.push_back(ObjPtr<RndAnimatable>(this, anim));

            EventTrigger *fulltrig = stardir->Find<EventTrigger>("full.trig", true);
            mStarFullTriggers.push_back(ObjPtr<EventTrigger>(this, fulltrig));

            EventTrigger *goldtrig = stardir->Find<EventTrigger>("gold.trig", true);
            mStarGoldTriggers.push_back(ObjPtr<EventTrigger>(this, goldtrig));
        }
        mStarOffsetAnim = Find<RndAnimatable>("stars_offset.tnm", true);
        mEarnStarSfx = Find<Sequence>("achieve_star.cue", true);
        mEarnSpadeSfx = Find<Sequence>("achieve_spade.cue", true);
        SetStarType(mStarType, true);
    }
}

void BandStarDisplay::ResetStars() {
    SetupStars();
    for (int i = 0; i < mStars.size(); i++) {
        RndDir *star = mStars[i];
        star->Find<EventTrigger>("reset.trig", true)->Trigger();
        if (i > 0)
            star->SetShowing(false);
    }
    mStarOffsetAnim->SetFrame(0, 1);
    mNumStars = 0;
}

void BandStarDisplay::SetStarType(Symbol s, bool b) {
    if (s == mStarType && !b)
        return;
    float frame = -1.0f;
    if (s == normal)
        frame = 0;
    else if (s == tour)
        frame = 1;
    if (frame == -1.0f) {
        MILO_NOTIFY("Invalid star type %s, defaulting to normal\n", s.Str());
        return;
    }
    mStarType = s;
    for (int i = 0; i < mStars.size(); i++) {
        mStars[i]->Find<RndAnimatable>("config.anim", true)->SetFrame(frame, 1.0f);
    }
}

SAVE_OBJ(BandStarDisplay, 0xDC)

BEGIN_COPYS(BandStarDisplay)
    CREATE_COPY_AS(BandStarDisplay, rhs)
    MILO_ASSERT(rhs, 0xE9);
    COPY_MEMBER_FROM(rhs, mStarType)
    COPY_SUPERCLASS(RndDir)
END_COPYS

void BandStarDisplay::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(2, 0);
    if (gRev == 1) {
        if (IsProxy())
            bs >> mStarType;
        else {
            Symbol s;
            bs >> s;
        }
    }
    if (gRev >= 2 && IsProxy())
        bs >> mStarType;
    bs.PushRev(packRevs(gAltRev, gRev), this);
    RndDir::PreLoad(bs);
}

void BandStarDisplay::PostLoad(BinStream &bs) {
    RndDir::PostLoad(bs);
    int revs = bs.PopRev(this);
    gRev = getHmxRev(revs);
    gAltRev = getAltRev(revs);
}

BEGIN_PROPSYNCS(BandStarDisplay)
    static Symbol num_stars("num_stars");
    SYNC_PROP_SET(num_stars, mNumStars, SetNumStars(_val.Float(), true))
    static Symbol star_type("star_type");
    SYNC_PROP_SET(star_type, mStarType, SetStarType(_val.Sym(), false))
    SYNC_SUPERCLASS(RndDir)
END_PROPSYNCS

BEGIN_HANDLERS(BandStarDisplay)
    HANDLE_ACTION(reset, Reset())
    HANDLE_SUPERCLASS(RndDir)
    HANDLE_CHECK(0x121)
END_HANDLERS
