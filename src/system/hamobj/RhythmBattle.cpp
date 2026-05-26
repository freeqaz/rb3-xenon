#include "hamobj/RhythmBattle.h"
#include "RhythmBattlePlayer.h"
#include "char/CharDriver.h"
#include "flow/PropertyEventProvider.h"
#include "gesture/ArchiveSkeleton.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/GestureMgr.h"
#include "gesture/Skeleton.h"
#include "hamobj/DancerSkeleton.h"
#include "hamobj/Difficulty.h"
#include "hamobj/FreestyleMoveRecorder.h"
#include "hamobj/HamCamShot.h"
#include "hamobj/HamCharacter.h"
#include "hamobj/HamDirector.h"
#include "hamobj/HamGameData.h"
#include "hamobj/HamLabel.h"
#include "hamobj/HamMaster.h"
#include "hamobj/HamPlayerData.h"
#include "hamobj/RhythmDetector.h"
#include "hamobj/RhythmDetectorGroup.h"
#include "hamobj/SongUtl.h"
#include "macros.h"
#include "math/Easing.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Poll.h"
#include "rndobj/PropKeys.h"
#include "rndobj/Trans.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include "utl/Option.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/TimeConversion.h"
#include "world/Dir.h"

namespace {
    bool gShortenSong;
}

// Set an audio jump from beat x to beat y (1-indexed)
void SetJump(int x, int y) {
    ClearJump();
    if (x - 1 == y - 1) {
        TheMaster->GetAudio()->SetLoop(y - 1, x - 1);
    } else {
        float start = BeatToMs(x - 1);
        float end = BeatToMs(y - 1);
        float crossfade_beats = SystemConfig("synth", "crossfade_beats")->Float(1);
        float crossfade_end = BeatToMs((x - 1) + crossfade_beats);
        TheMaster->GetAudio()->SetCrossfadeJump(start, end, crossfade_end - start);
    }
}

void ClearJump() {
    if (TheMaster && TheMaster->GetAudio()) {
        TheMaster->GetAudio()->ClearLoop();
    }
}

RhythmBattle::RhythmBattle()
    : mCommandLabel(this), mIntroLine2Label(this), mPlayerOne(this), mPlayerTwo(this),
      mBoxyLeadHeadTrans(this), mIntroAnim(this), mBattleEndAnim(this), unk94(this),
      mSwagJack1BarP2ToP1Anim(this), mSwagJack1BarP1ToP2Anim(this),
      mSwagJack2BarP2ToP1Anim(this), mSwagJack2BarP1ToP2Anim(this), mGoofy(false),
      mFullKTB(true), mFinale(false), mActive(false), mBattleFinished(false), mPaused(false),
      mMindControlIntensity(0), mMindControlTimer(0), mHalftimeBeat(0), mAlmostOverBeat(0), mMoveKeyCount(0), unk120(0), mSwagJackState(-1),
      mSwagJackCounter(0), mMoveRecorder(0), mFinaleSequenceTimer(0), mFinalePhaseIndex(0) {}

RhythmBattle::~RhythmBattle() { End(); }

BEGIN_HANDLERS(RhythmBattle)
    HANDLE_ACTION(beat, OnBeat())
    HANDLE_ACTION(reset, OnReset())
    HANDLE_ACTION(begin, Begin())
    HANDLE_ACTION(pause, OnPause())
    HANDLE_ACTION(unpause, OnUnpause())
    HANDLE_ACTION(end, End())
    HANDLE_ACTION(reset_combo, ResetCombo())
    HANDLE_ACTION(set_jump, SetJump(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(clear_jump, ClearJump())
    HANDLE_EXPR(
        both_players_dancing_bad,
        !mPlayerOne->InTheZone() && !mPlayerTwo->InTheZone()
    )
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RhythmBattle)
    SYNC_PROP(command_label, mCommandLabel)
    SYNC_PROP(player0, mPlayerOne)
    SYNC_PROP(player1, mPlayerTwo)
    SYNC_PROP(full_ktb, mFullKTB)
    SYNC_PROP(finale, mFinale)
    SYNC_SUPERCLASS(RndPollable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(RhythmBattle)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mCommandLabel;
    bs << mPlayerOne;
    bs << mPlayerTwo;
    bs << mFullKTB;
END_SAVES

BEGIN_COPYS(RhythmBattle)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY_AS(RhythmBattle, c)
    BEGIN_COPYING_MEMBERS_FROM(c)
        COPY_MEMBER(mPlayerOne)
        COPY_MEMBER(mPlayerTwo)
        COPY_MEMBER(mCommandLabel)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(3, 0)

BEGIN_LOADS(RhythmBattle)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    LOAD_SUPERCLASS(RndPollable)
    if (d.rev >= 2) {
        d >> mCommandLabel;
        d >> mPlayerOne;
        d >> mPlayerTwo;
    } else if (d.rev == 1) {
        ObjPtr<RhythmDetectorGroup> group1(this);
        ObjPtr<RhythmDetectorGroup> group2(this);
        ObjPtr<RndAnimatable> anim1(this);
        ObjPtr<RndAnimatable> anim2(this);
        ObjPtr<RndAnimatable> anim3(this);
        ObjPtr<HamLabel> label1(this);
        ObjPtr<HamLabel> label2(this);
        ObjPtr<HamLabel> label3(this);
        ObjPtr<HamLabel> label4(this);
        ObjPtr<RndMat> mat1(this);
        ObjPtr<RndMat> mat2(this);
        d >> group1;
        d >> group2;
        d >> anim1;
        d >> anim2;
        d >> anim3;
        d >> label1;
        d >> label2;
        d >> label3;
        d >> label4;
    }
    if (d.rev >= 3) {
        d >> mFullKTB;
    }
END_LOADS

void RhythmBattle::Poll() {
    if (!TheLoadMgr.EditMode()) {
        if (mActive && !mPaused) {
            bool goofy = GetGoofy();
            if (goofy != mGoofy) {
                mPlayerOne->SwapObjs(mPlayerTwo);
                mGoofy = goofy;
            }
            Symbol leader = GetLeader();
            static Symbol left("left");
            static Symbol right("right");
            mLeader = leader;
            static UIPanel *sRhythmDetectorPanel =
                ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
            if (mMoveRecorder != 0) {
                static Symbol playing("playing");
                if (TheUI->FocusPanel() && mBattleStarted && !mBattleFinished) {
                    const Skeleton *skeleton1 = TheGameData->Player(0)->GetSkeleton();
                    if (skeleton1) {
                        mMoveRecorder->SetVal44(skeleton1->SkeletonIndex());
                    } else {
                        mMoveRecorder->SetVal44(-1);
                    }
                    mMoveRecorder->Poll();
                    Skeleton *skeleton2 = TheGestureMgr->GetSkeletonByTrackingID(
                        TheGameData->Player(1)->GetSkeletonTrackingID()
                    );
                    if (skeleton2) {
                        mSkeletonHistory.push_back(ArchiveSkeleton());
                        ArchiveSkeleton &back = mSkeletonHistory.back();
                        if (mSkeletonHistory.size() > 200) {
                            MILO_WARN(
                                "bustajack recordings are getting big %d\n", mSkeletonHistory.size()
                            );
                        }
                        back.Set(*skeleton2);
                    } else {
                        mSkeletonHistory.clear();
                    }
                }
            }
            int beat = TheTaskMgr.Beat();
            if (beat != mLastBeatTracked && beat != 0) {
                OnBeat();
                mLastBeatTracked = beat;
            }
            UpdateMindControl();
        }
        if (mFinale) {
            TheHamDirector->GetVenueWorld()
                ->Find<RndDir>("score_star_display")
                ->SetShowing(false);
        }
    }
}

void RhythmBattle::Enter() {
    RndPollable::Enter();
    if (mPlayerOne)
        mPlayerOne->OnReset(this);
    if (mPlayerTwo)
        mPlayerTwo->OnReset(this);
    mMindControlIntensity = 0.0f;
    CheckIsFinale();
    if (mFullKTB)
        Begin();
}

void RhythmBattle::Exit() {
    static Symbol mind_control("mind_control");
    static Symbol gameplay_mode("gameplay_mode");
    Symbol mindControlCheck = TheHamProvider->Property(gameplay_mode)->Sym();
    if (mFullKTB || mindControlCheck == mind_control) {
        End();
    }
    RndPollable::Exit();
}

bool RhythmBattle::GetGoofy() const {
    MILO_ASSERT(TheGameData != NULL, 0x271);
    MILO_ASSERT(TheGameData->Player(0), 0x272);
    HamPlayerData *player = TheGameData->Player(0);
    return player->Side() == kSkeletonLeft;
}

Symbol RhythmBattle::GetLeader() const {
    static Symbol both("both");
    static Symbol left("left");
    static Symbol right("right");
    bool goofy = GetGoofy();
    Symbol ret = both;
    int p1 = mPlayerOne ? mPlayerOne->GetScore() : 0;
    int p2 = mPlayerTwo ? mPlayerTwo->GetScore() : 0;
    if (p1 != p2) {
        bool u4 = p2 < p1;
        if (goofy) {
            u4 = !u4;
        }
        ret = u4 ? right : left;
    }
    return ret;
}

void RhythmBattle::ResetCombo() {
    if (mPlayerOne)
        mPlayerOne->ResetCombo();
    if (mPlayerTwo)
        mPlayerTwo->ResetCombo();
}

void RhythmBattle::End() {
    if (mActive) {
        mActive = false;
        if (mPlayerOne)
            mPlayerOne->SetActive(false);
        if (mPlayerTwo)
            mPlayerTwo->SetActive(false);
        if (mMoveRecorder) {
            mMoveRecorder->Free();
            RELEASE(mMoveRecorder);
        }
        if (!mPaused) {
            static UIPanel *panel =
                ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
            if (panel && panel->LoadedDir()) {
                for (int i = 0; i < 6; i++) {
                    String s = MakeString("RhythmDetectorX%d.rhy", i);
                    RhythmDetector *rh =
                        panel->LoadedDir()->Find<RhythmDetector>(s.c_str(), true);
                    rh->StopRecording();
                }
            }
        }
        if (mFinale) {
            ObjectDir *hudPanel = DataVariable("hud_panel").Obj<ObjectDir>();
            hudPanel->Find<RndDir>("score_right")->SetShowing(true);
            hudPanel->Find<RndDir>("score_left")->SetShowing(true);
        }
        mPaused = false;
    }
}

void RhythmBattle::OnReset() {
    if (mPaused) {
        OnUnpause();
    }
    CheckIsFinale();
    if (!mActive) {
        Begin();
    }
    mIntroAnimStarted = false;
    mIsGrooveMode = false;
    mLastBeatTracked = TheTaskMgr.Beat();
    mSwagJackCounter = 0;
    mSwagJackState = -1;
    mHalftimePlayed = false;
    unk120 = 0.0f;
    mAlmostOverPlayed = false;
    mBattleStarted = false;
    mBattleFinished = false;
    mFinaleSequenceTimer = 0;
    mFinalePhaseIndex = 0;
    mJackCooldown = 0;
    if (mPlayerOne)
        mPlayerOne->OnReset(this);
    if (mPlayerTwo)
        mPlayerTwo->OnReset(this);
    if (mBattleEndAnim) {
        mBattleEndAnim->Animate(mBattleEndAnim->EndFrame(), mBattleEndAnim->EndFrame(), mBattleEndAnim->Units());
    }
    static Symbol gameplay_mode("gameplay_mode");
    static Symbol rhythm_battle("rhythm_battle");
    static Symbol mind_control("mind_control");

    Symbol gameplay_sym = TheHamProvider->Property(gameplay_mode)->Sym();
    if (TheHamDirector && mMoveKeyCount == 0) {
        RndPropAnim *songAnim = TheHamDirector->SongAnimByDifficulty(kDifficultyEasy);
        if (songAnim) {
            mMoveKeyCount = songAnim->GetNumKeys(TheHamDirector, "move");
        }
    }
    if (gameplay_sym == mind_control || mFinale) {
        float moveKeys = mMoveKeyCount;
        if (moveKeys >= 1) {
            mStartBeat = 0;
            mEndBeat = (moveKeys - 2.0f) * 4.0f;
        }
    }
    if (gameplay_sym == mind_control) {
        TheMaster->GetAudio()->SetLoop(32, 128);
    }
    if (mFinale) {
        if (mCommandLabel) {
            mCommandLabel->SetTextToken(gNullStr);
        }
        if (mIntroLine2Label) {
            mIntroLine2Label->SetTextToken(gNullStr);
        }
        mFinaleVOQueue.clear();
        static Symbol finale_intro_01("finale_intro_01");
        static Symbol finale_intro_02("finale_intro_02");
        QueueFinaleVO(finale_intro_01);
        QueueFinaleVO(finale_intro_02);
        ObjectDir *hudPanel = DataVariable("hud_panel").Obj<ObjectDir>();
        hudPanel->Find<RndDir>("score_right")->SetShowing(false);
        hudPanel->Find<RndDir>("score_left")->SetShowing(false);
        TheHamDirector->GetVenueWorld()
            ->Find<RndAnimatable>("set_bid.anim")
            ->Animate(0, false, 0, nullptr, kEaseLinear, 0, false);
    } else {
        mCommandLabel = Dir()->Find<HamLabel>("intro_line1.lbl", false);
        static Symbol rhythm_battle_title("rhythm_battle_title");
        if (mCommandLabel) {
            mCommandLabel->SetTextToken(rhythm_battle_title);
        }
        if (mIntroLine2Label) {
            mIntroLine2Label->SetTextToken(gNullStr);
        }
    }
}

void RhythmBattle::PlayMindControlVO(Symbol s) {
    static Message msg("mind_control_vo", 0);
    msg[0] = s;
    TheHamProvider->Handle(msg, false);
    mMindControlTimer = 0;
}

void RhythmBattle::UpdateFinaleVO(int &i) {
    if (!mFinaleVOQueue.empty() && i > 500) {
        UIPanel *game_panel = TheUI->FocusPanel();
        MILO_ASSERT(game_panel != NULL, 0x708);
        Symbol first = mFinaleVOQueue.front();
        mFinaleVOQueue.erase(mFinaleVOQueue.begin());
        static Message play_finale_vo("play_finale_vo", 0);
        play_finale_vo[0] = first;
        game_panel->HandleType(play_finale_vo);
        i = -1;
    }
}

void RhythmBattle::OnPause() {
    if (mActive) {
        mPaused = true;
        static UIPanel *panel =
            ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
        if (panel && panel->LoadedDir()) {
            for (int i = 0; i < 6; i++) {
                String s = MakeString("RhythmDetectorX%d.rhy", i);
                RhythmDetector *detector =
                    panel->LoadedDir()->Find<RhythmDetector>(s.c_str(), true);
                detector->StopRecording();
            }
        }
    }
}

void RhythmBattle::OnUnpause() {
    if (mPaused) {
        MILO_ASSERT(mActive, 0x7cc);
        mPaused = false;
        static UIPanel *panel =
            ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
        if (panel && panel->LoadedDir()) {
            for (int i = 0; i < 6; i++) {
                String s = MakeString("RhythmDetectorX%d.rhy", i);
                RhythmDetector *detector =
                    panel->LoadedDir()->Find<RhythmDetector>(s.c_str(), true);
                detector->StartRecording();
            }
        }
    }
}

void RhythmBattle::QueueFinaleVO(Symbol s) {
    if (mFinale) {
        bool b = false;
        for (int i = 0; i < mFinaleVOQueue.size(); i++) {
            if (mFinaleVOQueue[i] == s)
                b = true;
        }

        if (!b)
            mFinaleVOQueue.push_back(s);
    }
}

void RhythmBattle::UpdateMindControl() {
    static Symbol mind_control("mind_control");
    static Symbol gameplay_mode("gameplay_mode");
    static Symbol game_stage("game_stage");
    static Symbol playing("playing");

    Symbol gameplaySym = TheHamProvider->Property(gameplay_mode)->Sym();
    Symbol stageSym = TheHamProvider->Property(game_stage)->Sym();

    if (gameplaySym != mind_control) {
        return;
    }
    if (mPlayerOne) {
        mPlayerOne->SetActive(true);
    }
    if (mPlayerTwo) {
        mPlayerTwo->SetActive(true);
    }
    HamCamShot *curShot = TheHamDirector->CurShot();
    if (stageSym != playing) {
        mMindControlTimer = 0;
        mMindControlIntensity = 0;
    } else if (curShot && strcmp(curShot->Category().Str(), "CAMP_MINDCONTROL") == 0) {
        mMindControlIntensity = 0;
        mMindControlTimer = 0;
        if (curShot->ShotOver()) {
            if (strcmp(curShot->Name(), "CAMP_6.3_DCI_mind_control_03.shot")) {
                TheHamDirector->ForceShot(gNullStr);
                static Symbol CAMP_MINDCONTROL_DANCE("CAMP_MINDCONTROL_DANCE");
                TheHamDirector->SetProperty("shot", CAMP_MINDCONTROL_DANCE);
            }
        }
    } else if (mPlayerOne->GetZoneLevel() || mPlayerTwo->GetZoneLevel()) {
        mMindControlIntensity += TheTaskMgr.DeltaBeat() / 10.0f;
    }
    for (int i = 0; i < 2; i++) {
        HamCharacter *hc = TheHamDirector->GetCharacter(i);
        RndAnimatable *mcAnim =
            hc ? hc->Find<RndAnimatable>("mind_control.anim", false) : nullptr;
        float f10 = sin(TheTaskMgr.UISeconds() * 18.84955596923828f);
        f10 = f10 * 0.4f + 0.6f;
        f10 *= mMindControlIntensity;
        if (mcAnim) {
            mcAnim->SetFrame(f10, 1);
        }
        RndAnimatable *mcSoundAnim =
            hc ? hc->Find<RndAnimatable>("mind_control_sound.anim", false) : nullptr;
        if (mcSoundAnim) {
            mcSoundAnim->SetFrame(mMindControlIntensity, 1);
        }
    }

    if (mMindControlIntensity > 0.5f && mMindControlIntensity < 0.95f && mMindControlTimer > 5.0f) {
        static Symbol grooving("grooving");
        PlayMindControlVO(grooving);
    } else if (0.2f > mMindControlIntensity && mMindControlTimer > 12.0f) {
        static Symbol not_grooving("not_grooving");
        PlayMindControlVO(not_grooving);
    }
    mMindControlTimer += TheTaskMgr.DeltaSeconds();
}

void RhythmBattle::CheckIsFinale() {
    DataArray *arr = new DataArray(1);
    arr->Node(0) = "is_finale";
    mFinale = arr->Execute(false).Int() && mFullKTB;
    arr->Release();
}

void RhythmBattle::PlayTanClip(int i1, bool b2) {
    TheHamProvider->SetProperty("use_char_projection", true);
    static Message showCharProjection("show_char_projection");
    TheHamProvider->Handle(showCharProjection, false);
    CharDriver *driver = TheHamDirector->GetCharacter(0)->Driver();
    String str88;
    float fvar;
    if (i1 == 0) {
        str88 = "tan_rigged_01";
        fvar = 28;
        static Message tanPhaseIn("tan_finale_phasein01");
        TheHamProvider->Handle(tanPhaseIn, false);
    } else if (i1 == 1) {
        str88 = "tan_rigged_02";
        fvar = 22;
        static Message tanPhaseIn("tan_finale_phasein02");
        TheHamProvider->Handle(tanPhaseIn, false);
    } else if (i1 == 2) {
        str88 = "tan_rigged_03";
        fvar = 40;
        static Message tanPhaseIn("tan_finale_phasein03");
        TheHamProvider->Handle(tanPhaseIn, false);
    } else if (i1 == 3) {
        str88 = "tan_rigged_04";
        fvar = kHugeFloat;
    } else {
        str88 = "pose_fatalities_tan";
        fvar = 2.7f;
    }
    CharClip *clip = driver->FindClip(str88.c_str());
    driver->Play(clip, b2 ? 1 : 2, -1, fvar, 0);
}

void RhythmBattle::Begin() {
    if (!mActive) {
        mActive = true;
        if (mFullKTB) {
            PropKeys *keys = TheHamDirector->GetPropKeys(kDifficultyExpert, "move");
            if (keys) {
                Keys<Symbol, Symbol> *symKeys = keys->AsSymbolKeys();
                Symbol Rest("Rest.move");
                Symbol rest("rest.move");
                float f26 = 0;
                Symbol startSym = Rest;
                for (int i = 0; i < symKeys->size(); i++) {
                    if (startSym == Rest || startSym == rest) {
                        startSym = (*symKeys)[i].value;
                        f26 = (*symKeys)[i].frame;
                    } else {
                        break;
                    }
                }
                mStartBeat = FrameToBeat(f26);
                MILO_ASSERT(mStartBeat > 0, 0xE3);
                Symbol endSym = Rest;
                for (int i = symKeys->size() - 1; i >= 0; i--) {
                    if (endSym == Rest || endSym == rest) {
                        endSym = (*symKeys)[i].value;
                        f26 = (*symKeys)[i].frame;
                    } else {
                        break;
                    }
                }
                mEndBeat = FrameToBeat(f26);
                MILO_ASSERT(mEndBeat > 0, 0xEF);
                float f27 = 0;
                DataArray *arr = SystemConfig()->FindArray("party_jumps", false);
                if (arr) {
                    arr = arr->FindArray(TheGameData->GetSong(), false);
                    if (arr && gShortenSong) {
                        f26 = arr->Int(1) * 4.0f;
                        f27 = (arr->Int(2) - 1) * 4.0f;
                    }
                }
                float diff = f27 - f26;
                float f28 = mEndBeat - mStartBeat - diff;
                mHalftimeBeat = (f28 / 2.0f) + mStartBeat;
                mAlmostOverBeat = (f28 * 0.8f) + mStartBeat;
                if (mHalftimeBeat > f26) {
                    mHalftimeBeat += diff;
                }
                if (mAlmostOverBeat > f26) {
                    mAlmostOverBeat += diff;
                }
            }
            mPlayerOne->SetInTheZone(-1, false, false);
            mPlayerTwo->SetInTheZone(-1, false, false);
        } else {
            mIsGrooveMode = true;
            mIntroAnimStarted = true;
            mStartBeat = -1;
            mEndBeat = -1;
            mHalftimeBeat = -1;
            mAlmostOverBeat = -1;
            mPlayerOne->SetActive(true);
            mPlayerTwo->SetActive(true);
        }
        mIntroLine2Label = Dir()->Find<HamLabel>("intro_line2.lbl", false);
        if (TheHamDirector) {
            WorldDir *wdir = TheHamDirector->GetVenueWorld();
            if (wdir) {
                RndDir *boxy = wdir->Find<RndDir>("boxyman", false);
                if (boxy) {
                    mBoxyLeadHeadTrans =
                        boxy->Find<RndTransformable>("boxyleadhead.trans", false);
                }
            }
        }
        HamLabel *ml2x = Dir()->Find<HamLabel>("multiplier_L_2X.lbl", false);
        HamLabel *ml3x = Dir()->Find<HamLabel>("multiplier_L_3X.lbl", false);
        HamLabel *ml4x = Dir()->Find<HamLabel>("multiplier_L_4X.lbl", false);
        HamLabel *mr2x = Dir()->Find<HamLabel>("multiplier_R_2X.lbl", false);
        HamLabel *mr3x = Dir()->Find<HamLabel>("multiplier_R_3X.lbl", false);
        HamLabel *mr4x = Dir()->Find<HamLabel>("multiplier_R_4X.lbl", false);
        mSwagJack1BarP1ToP2Anim =
            Dir()->Find<RndAnimatable>("swag_jack_1bar_p1_to_p2.anim", false);
        mSwagJack1BarP2ToP1Anim =
            Dir()->Find<RndAnimatable>("swag_jack_1bar_p2_to_p1.anim", false);
        mSwagJack2BarP1ToP2Anim =
            Dir()->Find<RndAnimatable>("swag_jack_2bar_p1_to_p2.anim", false);
        mSwagJack2BarP2ToP1Anim =
            Dir()->Find<RndAnimatable>("swag_jack_2bar_p2_to_p1.anim", false);
        if (ml2x) {
            ml2x->SetTextToken("2x");
        }
        if (ml3x) {
            ml3x->SetTextToken("3x");
        }
        if (ml4x) {
            ml4x->SetTextToken("4x");
        }
        if (mr2x) {
            mr2x->SetTextToken("2x");
        }
        if (mr3x) {
            mr3x->SetTextToken("3x");
        }
        if (mr4x) {
            mr4x->SetTextToken("4x");
        }
        mIntroAnim = nullptr;
        mBattleEndAnim = nullptr;
        unk94 = nullptr;
        if (mFullKTB && !mFinale) {
            mMoveRecorder = new FreestyleMoveRecorder();
        }
        static UIPanel *sRhythmDetectorPanel =
            ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
        if (sRhythmDetectorPanel && sRhythmDetectorPanel->LoadedDir()) {
            for (int i = 0; i < 6; i++) {
                String s = MakeString("RhythmDetectorX%d.rhy", i);
                RhythmDetector *rh =
                    sRhythmDetectorPanel->LoadedDir()->Find<RhythmDetector>(s.c_str());
                rh->StartRecording();
            }
        }
    }
}

void RhythmBattle::OnBeat() {
    MILO_ASSERT(mActive, 0x290);
    static Symbol playing("playing");
    UIPanel *focusPanel = TheUI->FocusPanel();
    bool goofy = GetGoofy();
    if (!focusPanel || !mPlayerOne || !mPlayerTwo)
        return;
    static Symbol gameplay_mode("gameplay_mode");
    static Symbol mind_control("mind_control");
    bool inMindControl = TheHamProvider->Property(gameplay_mode)->Sym() == mind_control;
    if (mFullKTB && !mFinale && !inMindControl) {
        mPlayerOne->SetAutoPass(false);
        mPlayerTwo->SetAutoPass(false);
    } else {
        mPlayerOne->SetAutoPass(true);
        mPlayerTwo->SetAutoPass(true);
    }
    static Message remaining_vo_ms("remaining_vo_ms");
    DataNode handled = focusPanel->HandleType(remaining_vo_ms);
    int remainingValue = handled.Type() == kDataInt ? handled.Int() : 0;
    static Message play_vo("play_vo", 0, 0);
    static Message play_finale_vo("play_finale_vo", 0);
    static Symbol none("");
    play_finale_vo[0] = none;
    static Symbol both("both");
    static Symbol left("left");
    static Symbol right("right");
    play_vo[1] = both;
    float curBeat = TheTaskMgr.Beat();
    int beat = Round(curBeat);
    int i6cc = beat % 4;
    inMindControl = i6cc == 0;
    if (beat == 4) {
        static Message countInMsg("count_in", 0, 0);
        countInMsg[0] = mStartBeat - 4.0f;
        countInMsg[1] = mStartBeat - 4.0f;
        focusPanel->Handle(countInMsg, true);
    }
    UpdateFinaleVO(remainingValue);
    Symbol leader = GetLeader();
    if (remainingValue > 0 && !mIsGrooveMode) {
        static Symbol intro("intro");
        play_vo[0] = intro;
        if (!mFinale) {
            focusPanel->HandleType(play_vo);
            remainingValue = -1;
        }
        mIsGrooveMode = true;
    }
    if (remainingValue > 2000) {
        if (!mHalftimePlayed && mHalftimeBeat < curBeat) {
            static Symbol halftime("halftime");
            play_vo[0] = halftime;
            play_vo[1] = leader;
            if (!mFinale) {
                focusPanel->HandleType(play_vo);
                remainingValue = -1;
            }
            mHalftimePlayed = true;
        }
        if (remainingValue > 2000 && !mAlmostOverPlayed && mAlmostOverBeat < curBeat) {
            static Symbol almost_over("almost_over");
            play_vo[0] = almost_over;
            play_vo[1] = leader;
            if (!mFinale) {
                focusPanel->HandleType(play_vo);
                remainingValue = -1;
            }
            mAlmostOverPlayed = true;
        }
    }
    bool b22 = false;
    RndAnimatable *r28 = NULL;
    if (mStartBeat > curBeat) {
        if (!mIntroAnimStarted) {
            if (mStartBeat <= mPlayerOne->InAnimBeatLength() + curBeat) {
                mPlayerOne->AnimateIn();
                mPlayerTwo->AnimateIn();
                if (mIntroAnim) {
                    mIntroAnim->Animate(mIntroAnim->GetFrame(), mIntroAnim->EndFrame(), mIntroAnim->Units());
                }
                mIntroAnimStarted = true;
            } else {
                mPlayerOne->SetActive(false);
                mPlayerTwo->SetActive(false);
            }
        }
        return;
    }
    if (!mBattleStarted) {
        b22 = true;
        mBattleStarted = true;
        inMindControl = false;
        if (mCommandLabel) {
            mCommandLabel->SetTextToken(gNullStr);
        }
        if (mIntroLine2Label) {
            mIntroLine2Label->SetTextToken(gNullStr);
        }
        mCommandLabel = nullptr;
        mIntroLine2Label = nullptr;
        mPlayerOne->SetActive(true);
        mPlayerTwo->SetActive(true);
        mPlayerOne->SetInTheZone(0, false, false);
        mPlayerTwo->SetInTheZone(0, false, false);
        static Message finish_intro("finished_intro");
        focusPanel->HandleType(finish_intro);
    }
    if (mFullKTB) {
        if (!(mEndBeat < curBeat && !mFinale)) {
            if (mFinale) {
                bool p1Active = mFinaleSequenceTimer == 0 || (int)curBeat > 0x10;
                mPlayerOne->SetActive(p1Active);
                bool p2Active = mFinaleSequenceTimer == 0 || (int)curBeat > 0x10;
                mPlayerTwo->SetActive(p2Active);
            } else {
                mPlayerOne->SetActive(true);
                mPlayerTwo->SetActive(true);
            }
        } else {
            mBattleFinished = true;
            mPlayerOne->SetActive(false);
            mPlayerTwo->SetActive(false);
            if (mIntroAnimStarted) {
                static Symbol winner("winner");
                play_vo[0] = winner;
                play_vo[1] = leader;
                if (!mFinale) {
                    focusPanel->HandleType(play_vo);
                    remainingValue = -1;
                }
                mPlayerOne->AnimateOut();
                mPlayerTwo->AnimateOut();
                r28 = mBattleEndAnim;
                if (r28) {
                    mBattleEndAnim->Animate(mBattleEndAnim->StartFrame(), mBattleEndAnim->EndFrame(), mBattleEndAnim->Units());
                }
                mIntroAnimStarted = false;
            }
        }
    }
    mPlayerOne->HackPlayerQuit();
    mPlayerTwo->HackPlayerQuit();
    bool b40 = false;
    if (inMindControl) {
        // there is a LOT that happens in here
        if (mFullKTB && !mFinale) {
            if (!b22) {
                mMoveRecorder->StopRecording();
                if (!mJackCooldown && (mPlayerOne->InTheZone() || mPlayerTwo->InTheZone())) {
                    b22 = true;
                } else {
                    b22 = false;
                }
                float f48 = 0;
                if (b22 && mMoveRecorder->GetCurrentMoveNumFrames() != 0
                    && (unsigned int)mMoveRecorder->GetCurrentMoveNumFrames()
                        <= mSkeletonHistory.size()) {
                    float f45 = 0.10f;
                    for (int i = 0; i < mSkeletonHistory.size(); i++) {
                        int iPrev = Max(i - 1, 0);
                        ArchiveSkeleton &current = mSkeletonHistory[i];
                        ArchiveSkeleton &previous = mSkeletonHistory[iPrev];
                        DancerSkeleton dancerSkeleton;
                        if (i > 0) {
                            f45 += (float)current.ElapsedMs() / 1000.0f;
                        }
                        for (int j = 0; j < kNumJoints; j++) {
                            Vector3 v;
                            current.JointPos(kCoordCamera, (SkeletonJoint)j, v);
                            Vector3 v2;
                            previous.JointPos(kCoordCamera, (SkeletonJoint)j, v2);
                            dancerSkeleton.SetCamJointPos((SkeletonJoint)j, v);
                            Vector3 disp(v.x - v2.x, v.y - v2.y, v.z - v2.z);
                            dancerSkeleton.SetCamJointDisplacement((SkeletonJoint)j, disp);
                        }
                        dancerSkeleton.SetDisplacementElapsedMs(current.ElapsedMs());
                        dancerSkeleton.Set(current);
                        f48 = mMoveRecorder->GetScore(&dancerSkeleton, 1, f45, false);
                    }
                }
                static bool autojack = OptionBool("autojack", false);
                if (b22 && autojack) {
                    f48 = 1;
                }
                if (f48 > 0.70f) {
                    RhythmBattlePlayer *jacker = mPlayerOne;
                    RhythmBattlePlayer *jackee = mPlayerTwo;
                    if (jacker->InTheZone() && jackee->InTheZone()) {
                        if (jacker->GetScore() > jackee->GetScore()) {
                            jackee = jacker;
                        }
                    } else if (jacker->InTheZone()) {
                        jackee = jacker;
                    }
                    MILO_ASSERT(jackee->InTheZone(), 0x398);
                    b40 = true;
                    mJackCooldown = 4;
                }
                mSkeletonHistory.clear();
                mMoveRecorder->ClearRecording();
                mMoveRecorder->ClearFrameScores();
                mJackCooldown--;
                if (mJackCooldown < 0) {
                    mJackCooldown = 0;
                }
            }
            if (!mBattleFinished) {
                mMoveRecorder->StartRecording();
            }
        }
        mPlayerOne->UpdateScore(focusPanel);
        mPlayerTwo->UpdateScore(focusPanel);
    }
    static Message bars_between_vo_suggestion("bars_between_vo_suggestion");
    handled = focusPanel->HandleType(bars_between_vo_suggestion);
    if (handled.Type() == kDataInt) {
        handled.Int();
    }
    bool p1Update = false;
    bool p2Update = false;
    if (inMindControl) {
        p1Update = mPlayerOne->UpdateState();
        p2Update = mPlayerTwo->UpdateState();
    }
    int i16 = p1Update ? mPlayerOne->GetZoneLevel() : -1;
    int i19 = p2Update ? mPlayerTwo->GetZoneLevel() : -1;
    int i6b4 = (i16 > i19)
        ? (p1Update ? mPlayerOne->GetZoneLevel() : -1)
        : (p2Update ? mPlayerTwo->GetZoneLevel() : -1);
    int i28 = Max(mPlayerOne->GetZoneLevel(), mPlayerTwo->GetZoneLevel());
    bool i35 = i6b4 == mPlayerOne->GetZoneLevel();
    bool i27 = i6b4 == mPlayerTwo->GetZoneLevel();
    bool b6f0 = i35;
    if (goofy) {
        b6f0 = i27;
        int tmp = i35;
        i35 = i27;
        i27 = tmp;
    }
    if (b40) {
        mSwagJackCounter++;
        if (inMindControl) {
            mSwagJackCounter = 0;
            if (mSwagJackState == -1 || (mSwagJackState > 2 && mSwagJackState <= 4)) {
                bool b36 = true;
                RhythmBattlePlayer *first = mPlayerOne;
                RhythmBattlePlayer *second = mPlayerTwo;
                if (mPlayerTwo->InTheZone()
                    && (!mPlayerOne->InTheZone()
                        || mPlayerOne->GetScore() > mPlayerTwo->GetScore())) {
                    b36 = false;
                    first = mPlayerTwo;
                    second = mPlayerOne;
                }
                if (goofy) {
                    b36 = !b36;
                }
                mSwagJackState = (int)b36 + 5;
                auto _tmp2 = second->SwagJacked(focusPanel, (RhythmBattleJackState)mSwagJackState);
                first->SwagJackedBonus(
                    focusPanel,
                    (RhythmBattleJackState)mSwagJackState,
                    _tmp2
                );
                i6b4 = (int)mSwagJackState;
                b6f0 = !b36;
                i27 = b36;
                i35 = b6f0;
                mSwagJackState = -1;
            }
        }
    } else {
        mSwagJackCounter = 0;
        mSwagJackState = -1;
    }
    bool b43 = i6b4 == 1 || i6b4 == 2;
    bool outOfRange = (unsigned)i28 > 2u;
    if (b43 || outOfRange || (mEndBeat < curBeat + 12.0f)) {
        remainingValue = -1;
    }
    play_vo[0] = none;
    if (i27 || i35 || (mPlayerOne->ZoneValue() != 0 && mPlayerOne->GetPrevInTheZone() == 0)
        || (mPlayerTwo->ZoneValue() != 0 && mPlayerTwo->GetPrevInTheZone() == 0)) {
        static Symbol rhythmbattle_off_beat_p1p2("rhythmbattle_off_beat_p1p2");
        static Symbol rhythmbattle_off_beat_p1("rhythmbattle_off_beat_p1");
        static Symbol rhythmbattle_off_beat_p2("rhythmbattle_off_beat_p2");
        static Symbol rhythmbattle_nice_moves_p1p2("rhythmbattle_nice_moves_p1p2");
        static Symbol rhythmbattle_nice_moves_p1("rhythmbattle_nice_moves_p1");
        static Symbol rhythmbattle_nice_moves_p2("rhythmbattle_nice_moves_p2");
        static Symbol rhythmbattle_lemme_see_something_different_p1p2(
            "rhythmbattle_lemme_see_something_different_p1p2"
        );
        static Symbol rhythmbattle_lemme_see_something_different_p1(
            "rhythmbattle_lemme_see_something_different_p1"
        );
        static Symbol rhythmbattle_lemme_see_something_different_p2(
            "rhythmbattle_lemme_see_something_different_p2"
        );
        static Symbol rhythmbattle_stale_moves_warning_p1p2(
            "rhythmbattle_stale_moves_warning_p1p2"
        );
        static Symbol rhythmbattle_stale_moves_warning_p1(
            "rhythmbattle_stale_moves_warning_p1"
        );
        static Symbol rhythmbattle_stale_moves_warning_p2(
            "rhythmbattle_stale_moves_warning_p2"
        );
        static Symbol rhythmbattle_stale_moves_penalty_p1p2(
            "rhythmbattle_stale_moves_penalty_p1p2"
        );
        static Symbol rhythmbattle_stale_moves_penalty_p1(
            "rhythmbattle_stale_moves_penalty_p1"
        );
        static Symbol rhythmbattle_stale_moves_penalty_p2(
            "rhythmbattle_stale_moves_penalty_p2"
        );
        static Symbol rhythmbattle_synchronized1("rhythmbattle_synchronized1");
        static Symbol rhythmbattle_swagjackeddd1("rhythmbattle_swagjackeddd1");
        static Symbol rhythmbattle_synchronized2("rhythmbattle_synchronized2");
        static Symbol rhythmbattle_swagjackeddd2("rhythmbattle_swagjackeddd2");
        static Symbol intro("intro");
        static Symbol winner("winner");
        static Symbol groove_expired("groove_expired");
        static Symbol swag_jacked("swag_jacked");
        static Symbol no_groove_yet("no_groove_yet");
        static Symbol change_groove("change_groove");
        static Symbol almost_over("almost_over");
        static Symbol halftime("halftime");
        static Symbol trick_performed("trick_performed");
        static Symbol new_groove("new_groove");
        static Symbol jack_swag("jack_swag");
        static Symbol max_multiplier("max_multiplier");
        static Symbol getting_idea("getting_idea");
        static Symbol new_groove_working("new_groove_working");
        static Symbol stole_congrats("stole_congrats");
        if (i27 && b6f0) {
            play_vo[1] = both;
        } else if (i27) {
            play_vo[1] = right;
        } else {
            play_vo[1] = left;
        }
        int i6d8 = 1000000;
        static Symbol finale_no_groove_yet("finale_no_groove_yet");
        switch (i6b4) {
        case 0: {
            play_vo[0] = no_groove_yet;
            play_finale_vo[0] = finale_no_groove_yet;
            i6d8 = 8000;
            if (i27 && b6f0) {
                i6d8 = 6000;
                if (mCommandLabel) {
                    mCommandLabel->SetTextToken(rhythmbattle_off_beat_p1p2);
                }
                static Message msg("rhythm_battle_no_groove_p1p2");
                TheHamProvider->Handle(msg, false);
            } else if (i27) {
                if (mCommandLabel) {
                    mCommandLabel->SetTextToken(rhythmbattle_off_beat_p1);
                }
                static Message msg("rhythm_battle_no_groove_p1");
                TheHamProvider->Handle(msg, false);
            } else {
                if (mCommandLabel) {
                    mCommandLabel->SetTextToken(rhythmbattle_off_beat_p2);
                }
                static Message msg("rhythm_battle_no_groove_p2");
                TheHamProvider->Handle(msg, false);
            }
            break;
        }
        case 1: {
            i6d8 = 6000;
            play_vo[0] = new_groove;
            if (i27 && b6f0) {
                i6d8 = 5000;
                if (mCommandLabel) {
                    mCommandLabel->SetTextToken(rhythmbattle_nice_moves_p1p2);
                }
                static Message msg("rhythm_battle_fresh_moves_p1p2");
                TheHamProvider->Handle(msg, false);
            } else if (i27) {
                if (mCommandLabel) {
                    mCommandLabel->SetTextToken(rhythmbattle_nice_moves_p1);
                }
                static Message msg("rhythm_battle_fresh_moves_p1");
                TheHamProvider->Handle(msg, false);
            } else {
                if (mCommandLabel) {
                    mCommandLabel->SetTextToken(rhythmbattle_nice_moves_p2);
                }
                static Message msg("rhythm_battle_fresh_moves_p2");
                TheHamProvider->Handle(msg, false);
            }
            break;
        }
        case 2: {
            if (i27 && b6f0) {
                if (mCommandLabel) {
                    mCommandLabel->SetTextToken(
                        rhythmbattle_lemme_see_something_different_p1p2
                    );
                }
                static Message msg("rhythm_battle_switch_it_up_p1p2");
                TheHamProvider->Handle(msg, false);
            } else if (i27) {
                if (mCommandLabel) {
                    mCommandLabel->SetTextToken(
                        rhythmbattle_lemme_see_something_different_p1
                    );
                }
                static Message msg("rhythm_battle_switch_it_up_p1");
                TheHamProvider->Handle(msg, false);
            } else {
                if (mCommandLabel) {
                    mCommandLabel->SetTextToken(
                        rhythmbattle_lemme_see_something_different_p2
                    );
                }
                static Message msg("rhythm_battle_switch_it_up_p2");
                TheHamProvider->Handle(msg, false);
            }
            break;
        }
        case 3: {
            if (mCommandLabel) {
                mCommandLabel->SetTextToken(rhythmbattle_synchronized1);
            }
            static Message msg("rhythm_battle_synchronized");
            TheHamProvider->Handle(msg, false);
            break;
        }
        case 4: {
            if (mCommandLabel) {
                mCommandLabel->SetTextToken(rhythmbattle_synchronized2);
            }
            static Message msg("rhythm_battle_synchronized");
            TheHamProvider->Handle(msg, false);
            break;
        }
        case 5: {
            play_vo[0] = intro;
            if (mCommandLabel) {
                mCommandLabel->SetTextToken(rhythmbattle_swagjackeddd1);
            }
            static Message msg("rhythm_battle_swagjackeddd");
            TheHamProvider->Handle(msg, false);
            break;
        }
        case 6: {
            play_vo[0] = intro;
            if (mCommandLabel) {
                mCommandLabel->SetTextToken(rhythmbattle_swagjackeddd2);
            }
            static Message msg("rhythm_battle_swagjackeddd");
            TheHamProvider->Handle(msg, false);
            break;
        }
        }
        if (play_vo[0].Sym() != intro && remainingValue > 0) {
            static Symbol inzone("inzone");
            static Symbol inzone_warning("inzone_warning");
            if ((mPlayerOne->ZoneValue() && !mPlayerOne->GetPrevInTheZone())
                || (mPlayerTwo->ZoneValue() && !mPlayerTwo->GetPrevInTheZone())) {
                bool b43 = false;
                bool b42 = false;
                if (mPlayerOne->InTheZone() || mPlayerTwo->InTheZone()) {
                    if (!mPlayerOne->InTheZone() || !mPlayerTwo->InTheZone()) {
                        play_vo[0] = inzone_warning;
                    } else {
                        play_vo[0] = inzone;
                    }
                    b42 = mPlayerOne->InTheZone();
                    b43 = mPlayerTwo->InTheZone();
                }
                bool b5 = b42;
                if (goofy) {
                    b5 = b43;
                    b43 = b42;
                }
                if (b5 && b43) {
                    play_vo[1] = both;
                } else if (b5) {
                    play_vo[1] = right;
                } else {
                    play_vo[1] = left;
                }
            }
        }
        if (play_vo[0].Sym() != none && remainingValue > i6d8 && !mFinale) {
            focusPanel->HandleType(play_vo);
            remainingValue = -1;
        }
        if (play_finale_vo[0].Sym() != none && remainingValue > i6d8 && mFinale) {
            focusPanel->HandleType(play_finale_vo);
            remainingValue = -1;
        }
    }
    int zone1 = mPlayerOne->InTheZone();
    int zone2 = mPlayerTwo->InTheZone();
    if (r28) {
        mPlayerOne->UpdateComboProgress();
        mPlayerTwo->UpdateComboProgress();
        mPlayerOne->UpdateAnimations(focusPanel);
        mPlayerTwo->UpdateAnimations(focusPanel);
    }

    if (play_vo[0].Sym() == none && remainingValue > 15000) {
        static Symbol jack_swag("jack_swag");
        static Symbol max_multiplier("max_multiplier");
        static Symbol new_groove_working("new_groove_working");
        static Symbol inzone("inzone");
        static Symbol inzone_warning("inzone_warning");
        if (zone1 != mPlayerOne->InTheZone() || zone2 != mPlayerTwo->InTheZone()) {
            bool b42 = false;
            bool b43 = false;
            if (mPlayerOne->InTheZone() || mPlayerTwo->InTheZone()) {
                if (!mPlayerOne->InTheZone() || !mPlayerTwo->InTheZone()) {
                    play_vo[0] = inzone_warning;
                } else {
                    play_vo[0] = inzone;
                }
                b42 = mPlayerOne->InTheZone();
                b43 = mPlayerTwo->InTheZone();
            }
            bool b5 = b42;
            if (goofy) {
                b5 = b43;
                b43 = b42;
            }
            if (b5 && b43) {
                play_vo[1] = both;
            } else if (b5) {
                play_vo[1] = right;
            } else {
                play_vo[1] = left;
            }
        }
        if (play_vo[0].Sym() != none && !mFinale) {
            focusPanel->HandleType(play_vo);
            remainingValue = -1;
        }
    }

    if (inMindControl && (mPlayerOne->InTheZone() || mPlayerTwo->InTheZone())) {
        static bool s14bc = false;
    }
    if ((inMindControl && !mBattleFinished) || b22) {
        float beatF = (float)beat;
        mPlayerOne->SetWindow(beatF, beatF + 4.0f);
        mPlayerTwo->SetWindow(beatF, beatF + 4.0f);
    }
    if (mBattleFinished) {
        End();
    }
    float min84 = Max(mPlayerOne->GetComboMeter(), mPlayerTwo->GetComboMeter());
    static UIPanel *sLoadingPanel =
        ObjectDir::Main()->Find<UIPanel>("loading_panel", false);
    if (sLoadingPanel && sLoadingPanel->LoadedDir()) {
        static Message worked_it_progress("worked_it_progress", 0);
        worked_it_progress[0] = min84;
        sLoadingPanel->HandleType(worked_it_progress);
    }
    if (DataVariable("reset_finale").Int()) {
        DataVariable("reset_finale") = 0;
        mFinalePhaseIndex = 0;
        mFinaleSequenceTimer = 0;
    }
    if (mFinale) {
        TheHamDirector->GetVenueWorld()->Find<Flow>("show_timeywimey.flow")->Activate();
    }
    if (inMindControl) {
        TheHamDirector->SetPlayerSpotlightsEnabled(false);
    }

    int nextUnk148 = (mFinale ? 16 : 0) + 8;
    if (inMindControl || mFinale) {
        if (mFinaleSequenceTimer > 0) {
            mFinaleSequenceTimer--;
            if (mFinale) {
                if (mFinaleSequenceTimer == 0x16) {
                    TheHamDirector->GetVenueWorld()->Find<RndDir>("boxyman")->SetShowing(
                        false
                    );
                }
                if (mFinaleSequenceTimer == 0x11 && mFinalePhaseIndex < 3) {
                    PlayTanClip(mFinalePhaseIndex, false);
                }
                if (mFinaleSequenceTimer == 0x10) {
                    static Message charProjectionActive("char_projection_move_active");
                    TheHamProvider->Handle(charProjectionActive, false);
                }
                if (mFinaleSequenceTimer == 0xc && mFinalePhaseIndex < 2) {
                    TheHamDirector->SetPlayerSpotlightsEnabled(true);
                }
                if (mFinaleSequenceTimer == 8 && mFinalePhaseIndex == 2) {
                    TheHamDirector->SetPlayerSpotlightsEnabled(true);
                }
                if (mFinaleSequenceTimer == 4) {
                    static Symbol finale_tandance_01("finale_tandance_01");
                    static Symbol finale_tandance_02("finale_tandance_02");
                    static Symbol finale_tandance_03("finale_tandance_03");
                    switch (mFinalePhaseIndex) {
                    case 0:
                        QueueFinaleVO(finale_tandance_01);
                        break;
                    case 1:
                        QueueFinaleVO(finale_tandance_02);
                        break;
                    case 2:
                        QueueFinaleVO(finale_tandance_03);
                        break;
                    default:
                        break;
                    }
                }
                if (mFinaleSequenceTimer == 3 && mFinalePhaseIndex < 2) {
                    TheHamDirector->SetPlayerSpotlightsEnabled(false);
                }
                if (mFinalePhaseIndex == 3 && mFinaleSequenceTimer == 0x14) {
                    PlayTanClip(3, true);
                }
                if (mFinalePhaseIndex == 3 && mFinaleSequenceTimer == 0x14) {
                    TheHamDirector->SetPlayerSpotlightsEnabled(false);
                }
                if (mFinalePhaseIndex == 3 && mFinaleSequenceTimer == 4) {
                    PlayTanClip(4, true);
                    static Message destroyCharProjection("destroy_char_projection");
                    TheHamProvider->Handle(destroyCharProjection, false);
                }
            }
            if (!mFinaleSequenceTimer) {
                mFinalePhaseIndex++;
                if (mFinalePhaseIndex == 1) {
                    if (mFinale) {
                        static Symbol finale_phaseout_01("finale_phaseout_01");
                        static Symbol finale_phaseout_01b("finale_phaseout_01b");
                        QueueFinaleVO(finale_phaseout_01);
                        QueueFinaleVO(finale_phaseout_01b);
                        static Message charProjectionInactive("char_projection_move_inactive");
                        TheHamProvider->Handle(charProjectionInactive, false);
                        static Message hideCharProjection("hide_char_projection_keepdrawing");
                        TheHamProvider->Handle(hideCharProjection, false);
                        static Message tanPhaseOut("tan_finale_phaseout01");
                        TheHamProvider->Handle(tanPhaseOut, false);
                        mPlayerOne->SetSuppressRhythm(false);
                        mPlayerTwo->SetSuppressRhythm(false);
                    }
                } else if (mFinalePhaseIndex == 2) {
                    if (mFinale) {
                        static Symbol finale_phaseout_02("finale_phaseout_02");
                        static Symbol finale_phaseout_02b("finale_phaseout_02b");
                        QueueFinaleVO(finale_phaseout_02);
                        QueueFinaleVO(finale_phaseout_02b);
                        static Message charProjectionInactive("char_projection_move_inactive");
                        TheHamProvider->Handle(charProjectionInactive, false);
                        static Message hideCharProjection("hide_char_projection_keepdrawing");
                        TheHamProvider->Handle(hideCharProjection, false);
                        static Message tanPhaseOut("tan_finale_phaseout02");
                        TheHamProvider->Handle(tanPhaseOut, false);
                        mPlayerOne->SetSuppressRhythm(false);
                        mPlayerTwo->SetSuppressRhythm(false);
                    }
                    ResetCombo();
                } else if (mFinalePhaseIndex == 3) {
                    if (mFinale) {
                        static Symbol finale_phaseout_03("finale_phaseout_03");
                        static Symbol finale_phaseout_03b("finale_phaseout_03b");
                        QueueFinaleVO(finale_phaseout_03);
                        QueueFinaleVO(finale_phaseout_03b);
                        static Symbol tan_destroyed("tan_destroyed");
                        TheUI->FocusPanel()->SetProperty(tan_destroyed, true);
                        mFinaleSequenceTimer = 0x1c;
                    } else {
                        static Message game_outro_mind_control("game_outro_mind_control");
                        focusPanel->HandleType(game_outro_mind_control);
                    }
                } else if (mFinalePhaseIndex == 4) {
                    MILO_ASSERT(mFinale, 0x66C);
                    static Message game_outro_finale("game_outro_finale");
                    focusPanel->HandleType(game_outro_finale);
                }
            }
        } else {
            if (mFinale && r28) {
                TheHamDirector->GetVenueWorld()->Find<RndDir>("boxyman")->SetShowing(true);
            }
            if ((mFinale && min84 == 16.0f) || (!mFinale && mMindControlIntensity >= 1 && mMindControlTimer > 5.0f)) {
                if (mFinale) {
                    mPlayerOne->SetSuppressRhythm(true);
                    mPlayerTwo->SetSuppressRhythm(true);
                    if (mFinalePhaseIndex == 0) {
                        static Symbol finale_setcomplete_01("finale_setcomplete_01");
                        static Symbol finale_phasein_01("finale_phasein_01");
                        QueueFinaleVO(finale_setcomplete_01);
                        QueueFinaleVO(finale_phasein_01);
                    } else if (mFinalePhaseIndex == 1) {
                        static Symbol finale_setcomplete_02("finale_setcomplete_02");
                        static Symbol finale_phasein_02("finale_phasein_02");
                        QueueFinaleVO(finale_setcomplete_02);
                        QueueFinaleVO(finale_phasein_02);
                    } else if (mFinalePhaseIndex == 2) {
                        static Symbol finale_setcomplete_03("finale_setcomplete_03");
                        static Symbol finale_phasein_03("finale_phasein_03");
                        QueueFinaleVO(finale_setcomplete_03);
                        QueueFinaleVO(finale_phasein_03);
                    }
                    TheHamDirector->ForceShot("tan_finale.shot");
                } else {
                    {
                        mPlayerOne->OnReset(this);
                        mPlayerTwo->OnReset(this);
                        static UIPanel *sRhythmDetectorPanel =
                            ObjectDir::Main()->Find<UIPanel>("rhythm_detector_panel", false);
                        if (sRhythmDetectorPanel && sRhythmDetectorPanel->LoadedDir()) {
                            for (int i = 0; i < 6; i++) {
                                String name = MakeString("RhythmDetectorX%d.rhy", i);
                                sRhythmDetectorPanel->LoadedDir()
                                    ->Find<RhythmDetector>(name.c_str())
                                    ->ClearData();
                            }
                        }
                        static Symbol working("working");
                        static Message mindControlCompleteMsg("mind_control_complete");
                        switch (mFinalePhaseIndex) {
                        case 0:
                            PlayMindControlVO(working);
                            TheHamDirector->ForceShot("CAMP_6.3_DCI_mind_control_01.shot");
                            break;
                        case 1:
                            PlayMindControlVO(working);
                            TheHamDirector->ForceShot("CAMP_6.3_DCI_mind_control_02.shot");
                            break;
                        case 2:
                            TheHamProvider->Handle(mindControlCompleteMsg, false);
                            TheHamDirector->ForceShot("CAMP_6.3_DCI_mind_control_03.shot");
                            break;
                        default:
                            break;
                        }
                    }
                }
                mFinaleSequenceTimer = nextUnk148 + i6cc;
            }
        }
    }
    UpdateFinaleVO(remainingValue);
}

bool RhythmBattle::CanTrick(Symbol) { return false; }
