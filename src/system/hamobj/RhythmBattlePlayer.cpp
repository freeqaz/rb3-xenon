#include "hamobj/RhythmBattlePlayer.h"
#include "char/Waypoint.h"
#include "flow/PropertyEventProvider.h"
#include "gesture/GestureMgr.h"
#include "hamobj/HamDirector.h"
#include "hamobj/HamGameData.h"
#include "hamobj/HamLabel.h"
#include "hamobj/HamPlayerData.h"
#include "hamobj/RhythmBattle.h"
#include "hamobj/RhythmDetector.h"
#include "hamobj/ScoreUtl.h"
#include "math/Easing.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/Anim.h"
#include "rndobj/PartLauncher.h"
#include "rndobj/Poll.h"
#include "ui/UIPanel.h"
#include "utl/Loader.h"
#include "utl/Option.h"
#include "utl/Symbol.h"
#include "world/Dir.h"

namespace {
    bool gDebugGroove;
    bool gDebugFresh;
}

RhythmBattlePlayer::RhythmBattlePlayer()
    : mComboPosAnim(this), mComboColorAnim(this), mResetComboAnim(this),
      m2xMultAnim(this), m3xMultAnim(this), m4xMultAnim(this), mRhythmBattleAnim(this),
      mBattleMeterAnim(this), mBattleMeterStaleAnim(this), mBattleMeterInAnim(this),
      mShowScoreAnim(this), mBattleMeterOutAnim(this), mBoxyDir(this), mBattleLabel(this),
      mScoreLabel(this), mInTheZoneFlow(this), mOutTheZoneOkFlow(this),
      mOutTheZoneBadFlow(this), mSwagJackedFlow(this), mPhraseMeter(this),
      mTransConstraint(this), mBoxyWaistTrans(this), mBoxyman1(this), mBoxyman2(this),
      mTextFeedback(this), mMoveFeedback(this), mStealPart(this), mStealAnim(this),
      mPlayer(0), mRhythmBattle(0), mRhythmSuccessFraction(0), mFreshnessScore(0), mMaxRhythmInWindow(0), mFreshnessAccumulator(0), mWindowElapsedTime(0),
      mLastBeatTime(0), mZoneLevel(0), mInTheZone(-2), mNormalizedRhythmScore(0), mNormalizedFreshnessScore(0), mScore(0), mComboMeter(0),
      mSwapped(false), mDebugScoreValue(-1), mSwagJackedState("none"), mGrooveCooldown(0), mSuppressRhythm(false),
      mAutoPass(false),
      mFramesSinceLastTrigger(0) {}

RhythmBattlePlayer::~RhythmBattlePlayer() {}

BEGIN_HANDLERS(RhythmBattlePlayer)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RhythmBattlePlayer)
    SYNC_PROP(score_label, mScoreLabel)
    SYNC_PROP(combo_position_anim, mComboPosAnim)
    SYNC_PROP(combo_color_anim, mComboColorAnim)
    SYNC_PROP(reset_combo_anim, mResetComboAnim)
    SYNC_PROP(2x_mult_anim, m2xMultAnim)
    SYNC_PROP(3x_mult_anim, m3xMultAnim)
    SYNC_PROP(4x_mult_anim, m4xMultAnim)
    SYNC_PROP(player, mPlayer)
    SYNC_PROP(in_anim, mBattleMeterInAnim)
    SYNC_PROP(out_anim, mBattleMeterOutAnim)
    SYNC_PROP(boxydir, mBoxyDir)
    SYNC_SUPERCLASS(RndPollable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(RhythmBattlePlayer)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mComboPosAnim;
    bs << mComboColorAnim;
    bs << mResetComboAnim;
    bs << m2xMultAnim;
    bs << m3xMultAnim;
    bs << m4xMultAnim;
    mBattleLabel = nullptr;
    bs << mBattleLabel;
    mScoreLabel = nullptr;
    bs << mScoreLabel;
    bs << mPlayer;
    bs << mBoxyDir;
END_SAVES

BEGIN_COPYS(RhythmBattlePlayer)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY_AS(RhythmBattlePlayer, c)
    BEGIN_COPYING_MEMBERS_FROM(c)
        COPY_MEMBER(mComboPosAnim)
        COPY_MEMBER(mComboColorAnim)
        COPY_MEMBER(mResetComboAnim)
        COPY_MEMBER(m2xMultAnim)
        COPY_MEMBER(m3xMultAnim)
        COPY_MEMBER(m4xMultAnim)
        COPY_MEMBER(mPlayer)
        COPY_MEMBER(mScoreLabel)
        COPY_MEMBER(mBattleMeterInAnim)
        COPY_MEMBER(mShowScoreAnim)
        COPY_MEMBER(mBattleMeterOutAnim)
        COPY_MEMBER(mPhraseMeter)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(1, 0)

BEGIN_LOADS(RhythmBattlePlayer)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    d >> mComboPosAnim;
    d >> mComboColorAnim;
    d >> mResetComboAnim;
    d >> m2xMultAnim;
    d >> m3xMultAnim;
    d >> m4xMultAnim;
    d >> mBattleLabel;
    mBattleLabel = nullptr;
    d >> mScoreLabel;
    mScoreLabel = nullptr;
    d >> mPlayer;
    if (d.rev >= 1)
        d >> mBoxyDir;
END_LOADS

void RhythmBattlePlayer::Poll() {
    static UIPanel *sRhythmDetectorPanel =
        ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
    if (!TheLoadMgr.EditMode() && sRhythmDetectorPanel) {
        float f12 = TheTaskMgr.Beat();
        if (mRhythmBattle && mRhythmBattle->InFullKTB()) {
            HamPlayerData *hpd = TheGameData->Player(mPlayer);
            hpd->Provider()->Export(Message("hide_hud", 0), true);
        }
        if (mActive && mRhythmBattle && !mRhythmBattle->IsPaused()) {
            int skelIdx = TheGestureMgr->GetSkeletonIndexByTrackingID(
                TheGameData->Player(mPlayer)->GetSkeletonTrackingID()
            );
            RhythmDetector *rd = nullptr;
            mRhythmSuccessFraction = 0;
            mFreshnessScore = 0;
            if (skelIdx != -1 && sRhythmDetectorPanel->LoadedDir()) {
                String name = MakeString("RhythmDetectorX%d.rhy", skelIdx);
                rd = sRhythmDetectorPanel->LoadedDir()->Find<RhythmDetector>(
                    name.c_str(), false
                );
            }
            if (mWindowElapsedTime >= 1) {
                if (rd) {
                    const RhythmDetector::RecordData &recordData =
                        rd->GetRecord(mScoringWindowStart, mScoringWindowEnd, false, "", nullptr);
                    mRhythmSuccessFraction = recordData.unk10;
                    if (ShouldAutoPass() && 1 < recordData.unk14) {
                        mRhythmSuccessFraction = 1;
                    }
                                        if (ShouldAutoPass()) {
                        mFreshnessScore = 1;
                    } else {
                        mFreshnessScore = rd->Freshness();
                    }
                }
                Symbol autoplay = TheGameData->Player(mPlayer)->Autoplay();
                if (!autoplay.Null()) {
                    static Symbol move_ok("move_ok");
                    static Symbol maximum("maximum");
                    if (autoplay == move_ok) {
                        mRhythmSuccessFraction = 1;
                        mFreshnessScore = 0;
                    } else if (autoplay == maximum) {
                        mRhythmSuccessFraction = 1;
                        mFreshnessScore = 1;
                    } else {
                        mRhythmSuccessFraction = RatingToDetectFrac(autoplay, nullptr);
                        mFreshnessScore = 1;
                    }
                }
            }
            float f17 = f12 - mLastBeatTime;
            if (f17 < 0) {
                f17 = 0;
            }
            float f13 = mRhythmSuccessFraction <= 0 && mFreshnessScore <= 0 ? 0.0f : 1.0f;
            if (mMaxRhythmInWindow <= mRhythmSuccessFraction) {
                mMaxRhythmInWindow = mRhythmSuccessFraction;
            }
            if (mSuppressRhythm) {
                mMaxRhythmInWindow = 0;
            }
            mFreshnessAccumulator += mFreshnessScore * f17;
            mMovePresenceAccumulator += f13 * f17;
            mWindowElapsedTime += f17;
            f13 = mMaxRhythmInWindow > 1.0f ? 1.0f : mMaxRhythmInWindow;
            float f16 = 4.0f - mWindowElapsedTime - f17;
            if (mPhraseMeter) {
                f16 = Max(f16, 0.0f);
                mPhraseMeter->SetRatingFrac(f13, f16);
            }
            if (mInTheZone == 1 && mRhythmBattle && mRhythmBattle->InFullKTB()) {
                mComboMeter -= f17 * 1.125f;
                if (mComboMeter < 0) {
                    mComboMeter = 0;
                    int i7 = 0;
                    if (!mActive) {
                        i7 = -1;
                    }
                    if (i7 != 1) {
                        AnimateBoxyState(i7, true, false);
                    }
                }
                if (mComboPosAnim) {
                    mComboPosAnim->Animate(mComboMeter, mComboMeter, mComboPosAnim->Units());
                }
            }
            if (mBoxyman1) {
                mBoxyman1->SetGrooviness(rd, rd);
                mBoxyman2->SetGrooviness(rd, rd);
            }
        }
        mLastBeatTime = f12;
    }
}

void RhythmBattlePlayer::Enter() {
    RndPollable::Enter();
    mBattleMeterStaleAnim = Dir()->Find<RndAnimatable>(
        mPlayer != 0 ? "battle_meter_L_stale.anim" : "battle_meter_R_stale.anim", false
    );
    mBattleMeterInAnim = Dir()->Find<RndAnimatable>(
        mPlayer != 0 ? "battle_meter_l_in.anim" : "battle_meter_r_in.anim", false
    );
    mBattleMeterOutAnim = Dir()->Find<RndAnimatable>(
        mPlayer != 0 ? "battle_meter_l_out.anim" : "battle_meter_r_out.anim", false
    );
    mShowScoreAnim = Dir()->Dir()
        ? Dir()->Dir()->Find<RndAnimatable>(
              mPlayer != 0 ? "show_left_score.anim" : "show_right_score.anim", false
          )
        : nullptr;
    ObjectDir *rhythmBattleDir = Dir()->Find<ObjectDir>("rhythmbattle_audio", false);
    if (rhythmBattleDir) {
        mInTheZoneFlow = rhythmBattleDir->Find<Flow>(
            mPlayer != 0 ? "inthezone_p2.flow" : "inthezone_p1.flow", false
        );
        mOutTheZoneOkFlow = rhythmBattleDir->Find<Flow>(
            mPlayer != 0 ? "outthezone_ok_p2.flow" : "outthezone_ok_p1.flow", false
        );
        mOutTheZoneBadFlow = rhythmBattleDir->Find<Flow>(
            mPlayer != 0 ? "outthezone_bad_p2.flow" : "outthezone_bad_p1.flow", false
        );
        mSwagJackedFlow = rhythmBattleDir->Find<Flow>(
            mPlayer != 0 ? "swag_jacked_p2.flow" : "swag_jacked_p1.flow", false
        );
    }
    mActive = false;
    mBattleMeterAnim = nullptr;
    mTransConstraint = nullptr;
    if (!mBoxyDir && TheHamDirector) {
        WorldDir *wdir = TheHamDirector->GetVenueWorld();
        if (wdir) {
            mBoxyDir = wdir->Find<RndDir>("boxyman", false);
        }
    }
    if (mBoxyDir) {
        mTransConstraint = mBoxyDir->Find<TransConstraint>(
            mPlayer != 0 ? "TransConstraint1.tc" : "TransConstraint.tc", false
        );
        mPhraseMeter = mBoxyDir->Find<HamPhraseMeter>(
            mPlayer != 0 ? "phrase_meter1" : "phrase_meter0", false
        );
        mBoxyman1 = mBoxyDir->Find<DepthBuffer3D>(
            mPlayer != 0 ? "boxyman1_p2.db3d" : "boxyman1_p1.db3d", false
        );
        mBoxyman2 = mBoxyDir->Find<DepthBuffer3D>(
            mPlayer != 0 ? "boxyman2_p2.db3d" : "boxyman2_p1.db3d", false
        );
        mRhythmBattleAnim = mBoxyDir->Find<RndAnimatable>(
            mPlayer != 0 ? "rhythmbattle_p2.anim" : "rhythmbattle_p1.anim", false
        );
        mBoxyWaistTrans = mBoxyDir->Find<RndTransformable>(
            mPlayer != 0 ? "boxywaist_p2.trans" : "boxywaist_p1.trans", false
        );
        mStealPart = mBoxyDir->Find<RndParticleSys>(
            mPlayer != 0 ? "steal_p2.part" : "steal_p1.part", false
        );
        mStealAnim = mBoxyDir->Find<RndAnimatable>(
            mPlayer != 0 ? "steal_p2.anim" : "steal_p1.anim", false
        );
        mTextFeedback = mBoxyDir->Find<ObjectDir>(
            mPlayer != 0 ? "text_feedback1" : "text_feedback0", false
        );
        mMoveFeedback = mBoxyDir->Find<ObjectDir>(
            mPlayer != 0 ? "move_feedback1" : "move_feedback0", false
        );
        if (mStealPart && !TheLoadMgr.EditMode()) {
            mStealPart->SetEmitRate(0, 0);
        }
        if (mBoxyman1) {
            DepthBuffer3DAttachment attachment;
            attachment.obj = mBoxyWaistTrans;
            attachment.player = mPlayer;
            attachment.mJoint = 0;
            attachment.mOffset = Vector3::ZeroVec();
            attachment.unk1c = 0;
            attachment.unk20 = 0;
            mBoxyman1->AddAttachment(attachment);
        }
    }
    AnimateBoxyState(-1, false, false);
}

int RhythmBattlePlayer::SwagJacked(Hmx::Object *, RhythmBattleJackState) {
    MILO_ASSERT(mInTheZone == 1, 0x44a);
    static Symbol rhythmbattle_swagjackeddd1("rhythmbattle_swagjackeddd1");
    mSwagJackedState = rhythmbattle_swagjackeddd1;
    mZoneLevel = 0;
    if (mSwagJackedFlow)
        mSwagJackedFlow->Activate();
    return 1;
}

void RhythmBattlePlayer::HackPlayerQuit() {
    HamPlayerData *player = TheGameData->Player(mPlayer);
    int trackingID = player->GetSkeletonTrackingID();
    int skelIdx = TheGestureMgr->GetSkeletonIndexByTrackingID(trackingID);
    if (skelIdx != -1 && mShowScoreAnim) {
        mShowScoreAnim->Animate(
            mShowScoreAnim->GetFrame(),
            mShowScoreAnim->EndFrame(),
            mShowScoreAnim->Units()
        );
    }
}

int RhythmBattlePlayer::InTheZone() const { return mInTheZone == 1; }

float RhythmBattlePlayer::InAnimBeatLength() const { return 4.0f; }

void RhythmBattlePlayer::SetWindow(float f1, float f2) {
    mScoringWindowStart = f1;
    mScoringWindowEnd = f2;
}

void RhythmBattlePlayer::SetInTheZone(int i, bool b1, bool b2) {
    if (!mActive) {
        i = -1;
    }
    if (mInTheZone != i) {
        AnimateBoxyState(i, b1, b2);
    }
}

void RhythmBattlePlayer::SetActive(bool b1) {
    mActive = b1;
    if (!mActive && mRhythmBattle && mRhythmBattle->InFullKTB() && mInTheZone != -1) {
        AnimateBoxyState(-1, true, false);
    }
}

void RhythmBattlePlayer::AnimateIn() { AnimateBoxyState(0, true, false); }

bool RhythmBattlePlayer::UpdateState() {
    mPrevInTheZone = mInTheZone;
    mPrevZoneLevel = mZoneLevel;
    if (mNormalizedRhythmScore < 0.5f) {
        mZoneLevel = 0;
    } else if (mNormalizedFreshnessScore >= 0.6f) {
        mZoneLevel = 1;
    } else if (mZoneLevel <= 1) {
        mZoneLevel = 2;
    }
    return mPrevZoneLevel != mZoneLevel;
}

void RhythmBattlePlayer::ResetCombo() {
    int i = 0;
    if (!mActive)
        i = -1;

    if (mInTheZone != i)
        AnimateBoxyState(i, false, false);

    mPrevZoneLevel = 0;
    mFramesSinceLastTrigger = 0;
    mPrevInTheZone = mInTheZone;
    mComboMeter = 0.0f;
}

void RhythmBattlePlayer::SwagJackedBonus(Hmx::Object *, RhythmBattleJackState, int i) {
    if (mStealAnim) {
        mStealAnim->Animate(
            mStealAnim->StartFrame(), mStealAnim->EndFrame(), mStealAnim->Units()
        );
    }
    static Symbol swag_jacked("swag_jacked");
    TheGameData->Player(mPlayer)->Provider()->Export(Message(swag_jacked), true);
    static Symbol rhythmbattle_swagjackeddd2("rhythmbattle_swagjackeddd2");
    mSwagJackedState = rhythmbattle_swagjackeddd2;
    if (i != 0) {
        mZoneLevel = 1;
        mComboMeter = 16.0f;
    }
}

void RhythmBattlePlayer::SwapObjs(RhythmBattlePlayer *player) {
    RndAnimatable *temp = player->mComboPosAnim;
    player->mComboPosAnim = mComboPosAnim;
    mComboPosAnim = temp;

    temp = player->mComboColorAnim;
    player->mComboColorAnim = mComboColorAnim;
    mComboColorAnim = temp;

    temp = player->mResetComboAnim;
    player->mResetComboAnim = mResetComboAnim;
    mResetComboAnim = temp;

    temp = player->m2xMultAnim;
    player->m2xMultAnim = m2xMultAnim;
    m2xMultAnim = temp;

    temp = player->m3xMultAnim;
    player->m3xMultAnim = m3xMultAnim;
    m3xMultAnim = temp;

    temp = player->m4xMultAnim;
    player->m4xMultAnim = m4xMultAnim;
    m4xMultAnim = temp;

    temp = player->mBattleMeterStaleAnim;
    player->mBattleMeterStaleAnim = mBattleMeterStaleAnim;
    mBattleMeterStaleAnim = temp;

    temp = player->mBattleMeterInAnim;
    player->mBattleMeterInAnim = mBattleMeterInAnim;
    mBattleMeterInAnim = temp;

    temp = player->mShowScoreAnim;
    player->mShowScoreAnim = mShowScoreAnim;
    mShowScoreAnim = temp;

    temp = player->mBattleMeterOutAnim;
    player->mBattleMeterOutAnim = mBattleMeterOutAnim;
    mBattleMeterOutAnim = temp;

    HamLabel *tempLabel = player->mBattleLabel;
    player->mBattleLabel = mBattleLabel;
    mBattleLabel = tempLabel;

    tempLabel = player->mScoreLabel;
    player->mScoreLabel = mScoreLabel;
    mScoreLabel = tempLabel;

    temp = player->mBattleMeterAnim;
    player->mBattleMeterAnim = mBattleMeterAnim;
    mBattleMeterAnim = temp;

    mSwapped = !mSwapped;
    player->mSwapped = !player->mSwapped;
}

void RhythmBattlePlayer::UpdateScore(int points) {
    // Apply score multiplier (1x normally, 2x when in the zone)
    int multiplier = InTheZone() + 1;
    mScore += multiplier * points;
    static Symbol rhythm_battle("rhythm_battle");
    static Symbol gameplay_mode("gameplay_mode");
    if (TheHamProvider->Property(gameplay_mode)->Sym() == rhythm_battle) {
        static Symbol score("score");
        PropertyEventProvider *provider = TheGameData->Player(mPlayer)->Provider();
        provider->SetProperty(score, mScore);
    }
}

void RhythmBattlePlayer::OnReset(RhythmBattle *rb) {
    static Symbol none("none");
    mRhythmBattle = rb;
    mGrooveCooldown = 0;
    mTrickSymbol = none;
    mZoneLevel = 0;
    mMoveConsistencyScore = 0;
    mPrevZoneLevel = 0;
    mMovePresenceAccumulator = 0;
    mScore = 0;
    mRhythmSuccessFraction = 0;
    mPrevInTheZone = -1;
    mFreshnessScore = 0;
    mInTheZone = -2;
    mComboMeter = 0;
    mFreshnessAccumulator = 0;
    mMaxRhythmInWindow = 0;
    mWindowElapsedTime = 0;
    mNormalizedRhythmScore = 0;
    mNormalizedFreshnessScore = 0;
    mLastBeatTime = 0;
    mPrevMaxFootY = -1;
    if (mResetComboAnim) {
        mResetComboAnim->Animate(
            mResetComboAnim->StartFrame(),
            mResetComboAnim->EndFrame(),
            mResetComboAnim->Units()
        );
    }
    if (mBattleMeterAnim) {
        mBattleMeterAnim->Animate(mBattleMeterAnim->StartFrame(), mBattleMeterAnim->StartFrame(), mBattleMeterAnim->Units());
    }
    mBattleMeterAnim = nullptr;
    if (mBattleMeterOutAnim) {
        mBattleMeterOutAnim->Animate(
            mBattleMeterOutAnim->EndFrame(),
            mBattleMeterOutAnim->EndFrame(),
            mBattleMeterOutAnim->Units()
        );
    }
    auto& comboColorAnim = mComboColorAnim;
    if (comboColorAnim) {
        comboColorAnim->SetFrame(0, 1);
    }
    if (mComboPosAnim) {
        mComboPosAnim->SetFrame(mComboMeter, 1);
    }
    if (mBattleMeterStaleAnim) {
        mBattleMeterStaleAnim->SetFrame(mBattleMeterStaleAnim->EndFrame(), 1);
    }
    mFramesSinceLastTrigger = 0;
    UpdateScore(0);
    AnimateBoxyState(-1, false, false);
}

void RhythmBattlePlayer::UpdateAnimations(Hmx::Object *handler) {
    if (TheGestureMgr->GetSkeletonIndexByTrackingID(
            TheGameData->Player(mPlayer)->GetSkeletonTrackingID()
        )
        != -1) {
        MILO_ASSERT(handler, 0x378);
        static Symbol rhythmbattle_nogroove("rhythmbattle_nogroove");
        static Symbol rhythmbattle_groovelost("rhythmbattle_groovelost");
        static Symbol rhythmbattle_fresh("rhythmbattle_fresh");
        static Symbol rhythmbattle_switchup("rhythmbattle_switchup");
        static Symbol rhythmbattle_stale("rhythmbattle_stale");
        static Symbol rhythmbattle_unison("rhythmbattle_unison");
        static Symbol rhythmbattle_jacked("rhythmbattle_jacked");
        static Symbol rhythmbattle_jacked_bonus("rhythmbattle_jacked_bonus");
        static Symbol rhythmbattle_samegroove("rhythmbattle_samegroove");
        static Symbol rhythmbattle_inthezone("rhythmbattle_inthezone");
        static Symbol move_perfect("move_perfect");
        static Symbol move_awesome("move_awesome");
        static Symbol move_ok("move_ok");
        static Symbol move_bad("move_bad");
        static Symbol none("none");
        static Message groove_passed("groove_passed", 0, 0, 0, none, none);
        groove_passed[0] = mPlayer;
        int player = mSwapped ? !mPlayer : mPlayer;
        player += 3;
        groove_passed[player] = none;
        Symbol playerNodeValue;
        if (mTrickSymbol != none) {
            groove_passed[1] = move_perfect;
            playerNodeValue = mTrickSymbol;
        } else if (mNormalizedRhythmScore < 0.5f) {
            groove_passed[1] = move_bad;
            if (mPrevZoneLevel) {
                playerNodeValue = rhythmbattle_groovelost;
            } else {
                playerNodeValue = rhythmbattle_nogroove;
            }
        } else {
            groove_passed[1] = move_awesome;
            if (InTheZone() && mPrevInTheZone != 1) {
                groove_passed[1] = move_perfect;
                playerNodeValue = rhythmbattle_inthezone;
            } else {
                if (mPrevZoneLevel) {
                    playerNodeValue = rhythmbattle_samegroove;
                } else {
                    playerNodeValue = rhythmbattle_fresh;
                }
            }
        }
        if (groove_passed[1] != move_ok && groove_passed[1] != move_awesome) {
            mGrooveCooldown = 0;
        } else if (mGrooveCooldown > 0) {
            mGrooveCooldown--;
        }
        bool d13 = false;
        if (gDebugGroove) {
            mDebugScoreValue = mNormalizedRhythmScore * 100.0f;
        } else if (gDebugFresh) {
            mDebugScoreValue = mNormalizedFreshnessScore * 100.0f;
        }
        if (mDebugScoreValue != -1) {
            MILO_LOG("measure score: %d\n", mDebugScoreValue);
            mDebugScoreValue = -1;
        } else if (mSwagJackedState != none) {
            d13 = true;
            static Symbol rhythmbattle_swagjackeddd1("rhythmbattle_swagjackeddd1");
            static Symbol rhythmbattle_swagjackeddd2("rhythmbattle_swagjackeddd2");
            if (mSwagJackedState == rhythmbattle_swagjackeddd1) {
                groove_passed[1] = move_bad;
                playerNodeValue = rhythmbattle_jacked;
            } else if (mSwagJackedState == rhythmbattle_swagjackeddd2) {
                groove_passed[1] = move_perfect;
                playerNodeValue = rhythmbattle_jacked_bonus;
            } else {
                MILO_LOG("unknown token %s\n", playerNodeValue);
            }
            mSwagJackedState = none;
        }
        groove_passed[player] = playerNodeValue;
        groove_passed[2] = d13;
        static UIPanel *sLoadingPanel =
            ObjectDir::Main()->Find<UIPanel>("loading_panel", false);
        if (sLoadingPanel && sLoadingPanel->LoadedDir() && sLoadingPanel->IsLoaded()) {
            mMoveFeedback->Find<Flow>("flow.flow")->Activate();
            mTextFeedback->Find<Flow>("flow.flow")->Activate();
            sLoadingPanel->HandleType(groove_passed);
        } else {
            handler->HandleType(groove_passed);
        }
        RndAnimatable *anim = nullptr;
        if (mInTheZone == 1U) {
            anim = m4xMultAnim;
        }
        if (anim != mBattleMeterAnim) {
            if (anim) {
                static Symbol loop("loop");
                static Symbol dest("dest");
                anim->EndFrame();
                anim->EndFrame();
            }
            mBattleMeterAnim = anim;
        }
        if (mComboPosAnim) {
            mComboPosAnim->Animate(mComboMeter, mComboMeter, mComboPosAnim->Units());
        }
    }
}

void RhythmBattlePlayer::UpdateScore(Hmx::Object *handler) {
    mFramesSinceLastTrigger++;
    if (mStealPart) {
        mStealPart->SetEmitRate(0, 0);
    }
    int skelIdx = TheGestureMgr->GetSkeletonIndexByTrackingID(
        TheGameData->Player(mPlayer)->GetSkeletonTrackingID()
    );
    static UIPanel *sRhythmDetectorPanel =
        ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
    int i10 = 0;
    if (sRhythmDetectorPanel && skelIdx != -1 && sRhythmDetectorPanel->LoadedDir()) {
        String name = MakeString("RhythmDetectorX%d.rhy", skelIdx);
        RhythmDetector *rd =
            sRhythmDetectorPanel->LoadedDir()->Find<RhythmDetector>(name.c_str(), false);
        if (rd) {
            const RhythmDetector::RecordData &recordData =
                rd->GetRecord(mScoringWindowStart, mScoringWindowEnd, true, "", nullptr);
            mRhythmSuccessFraction = recordData.unk10;
            if (ShouldAutoPass() && 1 < recordData.unk14) {
                mRhythmSuccessFraction = 1;
            }
            mFreshnessScore = ShouldAutoPass() ? 1 : rd->Freshness();
            Symbol autoplay = TheGameData->Player(mPlayer)->Autoplay();
            if (!autoplay.Null()) {
                static Symbol move_ok("move_ok");
                static Symbol maximum("maximum");
                if (autoplay == move_ok) {
                    mRhythmSuccessFraction = 1;
                    mFreshnessScore = 0;
                } else if (autoplay == maximum) {
                    mRhythmSuccessFraction = 1;
                    mFreshnessScore = 1;
                } else {
                    mRhythmSuccessFraction = RatingToDetectFrac(autoplay, nullptr);
                    mFreshnessScore = 1;
                }
            }
            float set = mRhythmSuccessFraction;
            if (mMaxRhythmInWindow > mRhythmSuccessFraction) {
                set = mMaxRhythmInWindow;
            }
            mMaxRhythmInWindow = set;
        }
    }
    if (mSuppressRhythm) {
        mMaxRhythmInWindow = 0;
    }
    static Symbol none("none");
    static Symbol pose("pose");
    static Symbol getlow("getlow");
    static Symbol jump("jump");
    static Symbol rhythmbattle_trickpose("rhythmbattle_trickpose");
    static Symbol rhythmbattle_trickgetlow("rhythmbattle_trickgetlow");
    static Symbol rhythmbattle_trickjump("rhythmbattle_trickjump");
    mTrickSymbol = none;
    mMoveConsistencyScore = mMovePresenceAccumulator / mWindowElapsedTime;
    static Symbol autotrick(OptionStr("autotrick", "none"));
    if ((mMoveConsistencyScore < 0.5f || autotrick == pose) && mRhythmBattle->CanTrick(pose)) {
        mTrickSymbol = rhythmbattle_trickpose;
    }
    skelIdx = TheGestureMgr->GetSkeletonIndexByTrackingID(
        TheGameData->Player(mPlayer)->GetSkeletonTrackingID()
    );
    if (skelIdx >= 0) {
        Skeleton &skeleton = TheGestureMgr->GetSkeleton(skelIdx);
        float yLeft = skeleton.TrackedJoints()[kJointFootLeft].mJointPos[0].y;
        float yRight = skeleton.TrackedJoints()[kJointFootRight].mJointPos[0].y;
        float yMin = yLeft < yRight ? yLeft : yRight;
        float yMax = yLeft > yRight ? yLeft : yRight;
        if (mPrevMaxFootY != -1 && (yMin - mPrevMaxFootY > 0.1f) && mRhythmBattle->CanTrick(jump)) {
            mTrickSymbol = rhythmbattle_trickjump;
        }
        mPrevMaxFootY = yMax;
    }
    mNormalizedRhythmScore = mMaxRhythmInWindow;
    if (mNormalizedRhythmScore > 1) {
        mNormalizedRhythmScore = 1;
    }
    mNormalizedFreshnessScore = mFreshnessAccumulator / mWindowElapsedTime;
    if (mNormalizedFreshnessScore > 1) {
        mNormalizedFreshnessScore = 1;
    }
    mMovePresenceAccumulator = 0;
    mFreshnessAccumulator = 0;
    mMaxRhythmInWindow = 0;
    mWindowElapsedTime = 0;
    if (mPhraseMeter) {
        mPhraseMeter->SetRatingFrac(0, -1);
    }
    MILO_ASSERT(handler != NULL, 0x305);
    if (mTrickSymbol == none) {
        static Message m("rating_to_score", 0, 0);
        m[0] = mNormalizedRhythmScore;
        m[1] = mNormalizedFreshnessScore;
        DataNode handled = handler->HandleType(m);
        if (handled.Type() == kDataInt) {
            i10 = handled.Int();
        }
    }
    UpdateScore(i10);
}

void RhythmBattlePlayer::AnimateBoxyState(int state, bool transition, bool bad) {
    bool useBadFlow = (mInTheZone != -1) & bad;
    if (mRhythmBattleAnim) {
        float delay = 0.0f;
        static Symbol none("none");
        if (state > 0) {
            if (transition) {
                mRhythmBattleAnim->Animate(
                    0.0f, false, 0.0f, RndAnimatable::k30_fps,
                    8.0f, 12.0f, 0.0f, 1.0f,
                    none, nullptr, kEaseLinear, 0.0f, false
                );
                delay = 4.0f;
            }
            static Symbol loop("loop");
            mRhythmBattleAnim->Animate(
                0.0f, false, delay, RndAnimatable::k30_fps,
                12.0f, 20.0f, 0.0f, 1.0f,
                loop, nullptr, kEaseLinear, 0.0f, false
            );
        } else if (state == 0) {
            if (transition) {
                if (mInTheZone == -1) {
                    mRhythmBattleAnim->Animate(
                        0.0f, false, 0.0f, RndAnimatable::k30_fps,
                        36.0f, 40.0f, 0.0f, 1.0f,
                        none, nullptr, kEaseLinear, 0.0f, false
                    );
                } else if (mInTheZone == 1) {
                    mRhythmBattleAnim->Animate(
                        0.0f, false, 0.0f, RndAnimatable::k30_fps,
                        20.0f, 24.0f, 0.0f, 1.0f,
                        none, nullptr, kEaseLinear, 0.0f, false
                    );
                }
                delay = 4.0f;
            }
            static Symbol loop("loop");
            mRhythmBattleAnim->Animate(
                0.0f, false, delay, RndAnimatable::k30_fps,
                0.0f, 8.0f, 0.0f, 1.0f,
                loop, nullptr, kEaseLinear, 0.0f, false
            );
        } else if (state < 0) {
            if (transition) {
                if (mInTheZone == 0) {
                    mRhythmBattleAnim->Animate(
                        0.0f, false, 0.0f, RndAnimatable::k30_fps,
                        24.0f, 28.0f, 0.0f, 1.0f,
                        none, nullptr, kEaseLinear, 0.0f, false
                    );
                } else if (mInTheZone == 1) {
                    mRhythmBattleAnim->Animate(
                        0.0f, false, 0.0f, RndAnimatable::k30_fps,
                        20.0f, 28.0f, 0.0f, 2.0f,
                        none, nullptr, kEaseLinear, 0.0f, false
                    );
                }
                delay = 4.0f;
            }
            static Symbol loop("loop");
            mRhythmBattleAnim->Animate(
                0.0f, false, delay, RndAnimatable::k30_fps,
                28.0f, 36.0f, 0.0f, 1.0f,
                loop, nullptr, kEaseLinear, 0.0f, false
            );
        }
    }
    mInTheZone = state;
    static Symbol swag_jacked("swag_jacked");
    HamPlayerData *hpd = TheGameData->Player(mPlayer);
    static Symbol rhythmbattle_inthezone("rhythmbattle_inthezone");
    static Symbol rhythmbattle_outthezone("rhythmbattle_outthezone");
    if (state == -1) {
        return;
    }
    if (state != 0) {
        if (mInTheZoneFlow) {
            mInTheZoneFlow->Activate();
        }
        hpd->Provider()->Export(Message(rhythmbattle_inthezone), true);
    } else {
        Flow *flow;
        if (useBadFlow) {
            flow = mOutTheZoneBadFlow;
            if (flow) {
                if (!mSuppressRhythm)
                    goto do_activate;
                goto no_activate;
            }
        }
        flow = mOutTheZoneOkFlow;
        if (flow) {
    do_activate:
            flow->Activate();
        }
    no_activate:
        hpd->Provider()->Export(Message(rhythmbattle_outthezone), true);
    }
}

void RhythmBattlePlayer::UpdateComboProgress() {
    float increment;
    if (!mRhythmBattle || !mRhythmBattle->InFullKTB()) {
        increment = 8.0f;
    } else {
        increment = 4.0f;
    }
    if (mZoneLevel == 0) {
        mComboMeter = 0.0f;
        int targetState = 0;
        if (!mActive) {
            targetState = -1;
        }
        if (mInTheZone != targetState) {
            AnimateBoxyState(targetState, true, true);
        }
        return;
    }
    if (mInTheZone < 1) {
        mComboMeter += increment;
        if (mComboMeter >= 16.0f) {
            int targetState = 1;
            if (!mActive) {
                targetState = -1;
            }
            if (mInTheZone != targetState) {
                AnimateBoxyState(targetState, true, false);
            }
        }
        return;
    }
    if (mInTheZone != 1) {
        return;
    }
    mComboMeter += increment;
}

void RhythmBattlePlayer::AnimateOut() {}
