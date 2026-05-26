#include "bandobj/StreakMeter.h"
#include "decomp.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "rndobj/MatAnim.h"
#include "utl/Symbols.h"

INIT_REVS(3, 0)

StreakMeter::StreakMeter()
    : mStreakMultiplier(1), mBandMultiplier(1), mMaxMultiplier(4), mShowBandMult(0),
      mNewStreakTrig(this, 0), mEndStreakTrig(this, 0), mPeakStateTrig(this, 0),
      mBreakOverdriveTrig(this, 0), mMultiMeterAnim(this, 0), mMultiplierLabel(this, 0),
      mXLabel(this, 0), mMeterWipeAnim(this, 0), mStarDeployTrig(this, 0),
      mEndOverdriveTrig(this, 0), mStarDeployStopTrig(this, 0),
      mStarDeployPauseTrig(this, 0), mResetTrig(this, 0), mHideMultiplierTrig(this, 0),
      mFlashTrig(this, 0), mFlashSparksTrig(this, 0), unk260(0), mPartBarsGroup(this, 0),
      mPartColorAnims(this), mPartFadeAnims(this), mPartWipeAnims(this),
      mPartWipeResidualAnims(this), mResidueFadeTrig(this, 0), mNumPartsAnim(this, 0),
      mPartSparksLaunchers(this), unk2c8(1), unk2d0(-1) {}

void StreakMeter::SyncObjects() {
    bool audible = true;
    Hmx::Object *gamemodeObj = FindObject("gamemode", false, true);
    if (gamemodeObj)
        audible = gamemodeObj->Property("play_streak_sfx", true)->Int();
    if (audible)
        mNewStreakTrig = Find<EventTrigger>("new_streak.trig", true);
    else
        mNewStreakTrig = Find<EventTrigger>("new_streak_silent.trig", true);
    mEndStreakTrig = Find<EventTrigger>("end_streak.trig", true);
    mPeakStateTrig = Find<EventTrigger>("peak_state.trig", true);
    mBreakOverdriveTrig = Find<EventTrigger>("break_overdrive.trig", true);
    mMultiMeterAnim = Find<RndTransAnim>("multi-meter_anim.tnm", true);
    mMultiplierLabel = Find<BandLabel>("multiplier.lbl", true);
    mXLabel = Find<BandLabel>("x.lbl", true);
    mMeterWipeAnim = Find<RndPropAnim>("meter_wipe.anim", true);
    mStarDeployTrig = Find<EventTrigger>("star_deploy.trig", true);
    mEndOverdriveTrig = Find<EventTrigger>("end_overdrive.trig", true);
    mStarDeployStopTrig = Find<EventTrigger>("star_deploy_stop.trig", true);
    mStarDeployPauseTrig = Find<EventTrigger>("star_deploy_pause.trig", false);
    mResetTrig = Find<EventTrigger>("reset.trig", true);
    mHideMultiplierTrig = Find<EventTrigger>("hide_multiplier.trig", true);
    mPartBarsGroup = Find<RndGroup>("part_bars.grp", false);
    if (mPartBarsGroup) {
        unk260 = true;
        mPartColorAnims.clear();
        mPartFadeAnims.clear();
        mPartWipeAnims.clear();
        mPartWipeResidualAnims.clear();
        for (int i = 0; i < 3; i++) {
            mPartColorAnims.push_back(ObjPtr<RndPropAnim>(
                this, Find<RndPropAnim>(MakeString("part_color%d.anim", i + 1), true)
            ));
            mPartFadeAnims.push_back(ObjPtr<RndPropAnim>(
                this, Find<RndPropAnim>(MakeString("part_fade%d.anim", i + 1), true)
            ));
            mPartWipeAnims.push_back(ObjPtr<RndPropAnim>(
                this, Find<RndPropAnim>(MakeString("part_wipe%d.anim", i + 1), true)
            ));
            mPartWipeResidualAnims.push_back(ObjPtr<RndPropAnim>(
                this,
                Find<RndPropAnim>(MakeString("part_wipe%d_residual.anim", i + 1), true)
            ));
            mPartSparksLaunchers.push_back(ObjPtr<RndPartLauncher>(
                this, Find<RndPartLauncher>(MakeString("part_sparks%d.ml", i + 1), true)
            ));
        }
        mResidueFadeTrig = Find<EventTrigger>("residue_fade.trig", true);
        mNumPartsAnim = Find<RndPropAnim>("num_parts.anim", true);
    }
    RndDir::SyncObjects();
}

DECOMP_FORCEACTIVE(
    StreakMeter, "StreakMeter::StringToFrame used on unsupported string: %s"
)

void StreakMeter::CombineMultipliers(bool b) {
    if (b != mShowBandMult) {
        mShowBandMult = b;
        MultiplierChanged();
    }
}

void StreakMeter::SetBandMultiplier(int mult) {
    if (mBandMultiplier != mult) {
        mBandMultiplier = mult;
        if (mShowBandMult)
            mNewStreakTrig->Trigger();
        MultiplierChanged();
    }
}

bool StreakMeter::SetMultiplier(int mult) {
    if (mult != mStreakMultiplier && mult <= mMaxMultiplier) {
        mStreakMultiplier = mult;
        if (mult > 1)
            mNewStreakTrig->Trigger();
        MultiplierChanged();
        return true;
    } else
        return false;
}

int StreakMeter::GetMultiplierToShow() const {
    if (mShowBandMult)
        return mStreakMultiplier * mBandMultiplier;
    else
        return mStreakMultiplier;
}

void StreakMeter::MultiplierChanged() {
    int mult = GetMultiplierToShow();
    if (mult != unk2d4) {
        if (mult > 1)
            UpdateMultiplierText(mult);
        else
            mHideMultiplierTrig->Trigger();
    }
    unk2d4 = mult;
}

void StreakMeter::UpdateMultiplierText(int mult) {
    mMultiplierLabel->SetShowing(true);
    mMultiplierLabel->SetTokenFmt(streak_multiplier_fmt, mult);
    mXLabel->SetShowing(true);
}

void StreakMeter::BreakStreak(bool b) {
    mStreakMultiplier = 1;
    if (b)
        mEndStreakTrig->Trigger();
    int mult = GetMultiplierToShow();
    if (mult != 1) {
        mBreakOverdriveTrig->Trigger();
        UpdateMultiplierText(mult);
    }
    SetWipe(0);
}

void StreakMeter::SetPeakState() { mPeakStateTrig->Trigger(); }

void StreakMeter::Overdrive() const { mStarDeployTrig->Trigger(); }

void StreakMeter::EndOverdrive() const { mStarDeployStopTrig->Trigger(); }

void StreakMeter::Reset() {
    mResetTrig->Trigger();
    SetWipe(0);
    for (int i = 0; i < 3; i++)
        unk2cc[i] = false;
    SetMultiplier(1);
    SetBandMultiplier(1);
    unk2d4 = 1;
    for (int i = 0; i < mPartWipeAnims.size(); i++) {
        SetPartPct(i, 0, false);
    }
}

void StreakMeter::SetWipe(float f) { mMeterWipeAnim->SetFrame(f, 1.0f); }

void StreakMeter::SetPartColor(int i, VocalHUDColor col) {
    if (col == kVocalColorInvalid)
        SetPartActive(i, false);
    else if (i < mPartColorAnims.size()) {
        RndPropAnim *curanim = mPartColorAnims[i];
        if (curanim)
            curanim->SetFrame(col, 1.0f);
    }
}

void StreakMeter::SetNumParts(int num) {
    if (unk260)
        mNumPartsAnim->SetFrame(num, 1.0f);
    unk2c8 = num;
}

void StreakMeter::SetPartActive(int i, bool b) {
    if (!unk260)
        return;
    if (unk2c8 <= 1)
        return;
    if (i >= unk2c8)
        return;
    if (i >= mPartFadeAnims.size())
        return;
    if (!mPartFadeAnims[i])
        return;
    unk270[i] = b;
    if (unk2d0 != -1) {
        b = (bool)((int)b & (int)(i == unk2d0));
    }
    if (!b && !(mPartFadeAnims[i]->GetFrame() < 0.1f))
        return;
    RndPropAnim *anim = mPartFadeAnims[i];
    anim->SetFrame(b ? 0.0f : 1.0f, 1.0f);
}

void StreakMeter::SetIsolatedPart(int i) {
    unk2d0 = i;
    for (int n = 0; n < 3; n++)
        SetPartActive(n, unk270[n]);
}

void StreakMeter::SetPartPct(int i, float f, bool b) {
    if (unk260 && i < mPartWipeAnims.size()) {
        RndPropAnim *anim = mPartWipeAnims[i];
        if (anim)
            anim->SetFrame(f, 1.0f);
    }
    unk2cc[i] = b;
}

int StreakMeter::NumActiveParts() const {
    if (mPartBarsGroup)
        return unk270[0] + unk270[1] + unk270[2];
    else
        return 1;
}

void StreakMeter::SetPitch(float pitch) {
    Hmx::Matrix3 &lm = mLocalXfm.m;
    Vector3 e;
    Vector3 s;
    Hmx::Matrix3 m;
    MakeEuler(lm, e);
    MakeScale(lm, s);
    e.x = DegreesToRadians(pitch);
    MakeRotMatrix(e, m, true);
    Scale(s, m, m);
    SetLocalRot(m);
}

BEGIN_HANDLERS(StreakMeter)
    HANDLE_ACTION(set_multiplier, SetMultiplier(_msg->Int(2)))
    HANDLE_ACTION(set_band_multiplier, SetBandMultiplier(_msg->Int(2)))
    HANDLE_ACTION(break_streak, BreakStreak(true))
    HANDLE_ACTION(star_deploy, Overdrive())
    HANDLE_ACTION(star_deploy_stop, EndOverdrive())
    HANDLE_ACTION(reset, Reset())
    HANDLE_ACTION(set_wipe, SetWipe(_msg->Float(2)))
    HANDLE_ACTION(peak_state, SetPeakState())
    HANDLE_SUPERCLASS(RndDir)
END_HANDLERS

BEGIN_PROPSYNCS(StreakMeter)
    SYNC_PROP(streak_multiplier, mStreakMultiplier)
    SYNC_PROP(band_multiplier, mBandMultiplier)
    SYNC_PROP(max_multiplier, mMaxMultiplier)
    SYNC_PROP(show_band_mult, mShowBandMult)
    SYNC_SUPERCLASS(RndDir)
END_PROPSYNCS

void StreakMeter::Save(BinStream &) { MILO_ASSERT(0, 0x193); }

void StreakMeter::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    d.stream >> mStreakMultiplier;
    d.stream >> mBandMultiplier;
    d.stream >> mMaxMultiplier;
    if (!IsProxy()) {
        d.stream >> mNewStreakTrig;
        d.stream >> mEndStreakTrig;
        if (d.rev < 3) {
            ObjPtr<EventTrigger> trigPtr(this, 0);
            d.stream >> trigPtr;
        }
        d.stream >> mMultiMeterAnim;
        if (d.rev >= 1)
            d.stream >> mMultiplierLabel;
        else {
            ObjPtr<RndText> textPtr(this, 0);
            d.stream >> textPtr;
        }
        if (d.rev >= 2)
            d.stream >> mMeterWipeAnim;
        else {
            ObjPtr<RndMatAnim> matPtr(this, 0);
            d.stream >> matPtr;
        }
        d.stream >> mStarDeployTrig;
        d.stream >> mEndOverdriveTrig;
        d.stream >> mResetTrig;
    }
    RndDir::PreLoad(d.stream);
}

void StreakMeter::PostLoad(BinStream &bs) { RndDir::PostLoad(bs); }

BEGIN_COPYS(StreakMeter)
    COPY_SUPERCLASS(RndDir)
    CREATE_COPY(StreakMeter)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mStreakMultiplier)
        COPY_MEMBER(mBandMultiplier)
        COPY_MEMBER(mMaxMultiplier)
        COPY_MEMBER(mNewStreakTrig)
        COPY_MEMBER(mEndStreakTrig)
        COPY_MEMBER(mMultiMeterAnim)
        COPY_MEMBER(mMultiplierLabel)
        COPY_MEMBER(mMeterWipeAnim)
        COPY_MEMBER(mStarDeployTrig)
        COPY_MEMBER(mEndOverdriveTrig)
        COPY_MEMBER(mResetTrig)
    END_COPYING_MEMBERS
END_COPYS

void StreakMeter::SyncVoxPhraseTriggers() {
    if (!mFlashTrig)
        mFlashTrig = Find<EventTrigger>("flash.trig", true);
    if (!mFlashSparksTrig)
        mFlashSparksTrig = Find<EventTrigger>("flash_sparks.trig", true);
}

void StreakMeter::ShowPhraseFeedback(int i, bool b) {
    SyncVoxPhraseTriggers();
    switch (i) {
        case 4:
        case 5:
        case 6:
            if (!b)
                mFlashSparksTrig->Trigger();
            break;
        case 2:
        case 3:
            if (!b)
                mFlashTrig->Trigger();
            break;
    }
    SetWipe(0);
    if (unk260 && unk2c8 > 1) {
        for (int n = 0; n < unk2c8; n++) {
            RndPropAnim *anim = mPartFadeAnims[n];
            if (anim->GetFrame() > 0.9f) {
                anim->SetFrame(2.0f, 1.0f);
                mPartWipeResidualAnims[n]->SetFrame(0, 1.0f);
            } else {
                mPartWipeResidualAnims[n]->SetFrame(
                    mPartWipeAnims[n]->GetFrame(), 1.0f
                );
            }
            if (unk2cc[n]) {
                mPartSparksLaunchers[n]->LaunchParticles();
                unk2cc[n] = false;
            }
        }
        mResidueFadeTrig->Trigger();
    }
}

void StreakMeter::ForceFadeInactiveParts() {
    if (mResidueFadeTrig)
        mResidueFadeTrig->Trigger();
}
