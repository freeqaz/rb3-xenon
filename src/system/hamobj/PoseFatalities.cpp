#include "hamobj/PoseFatalities.h"
#include "PoseFatalities.h"
#include "char/CharClip.h"
#include "char/CharDriver.h"
#include "flow/PropertyEventProvider.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/Skeleton.h"
#include "hamobj/CharCameraInput.h"
#include "hamobj/Difficulty.h"
#include "hamobj/HamCharacter.h"
#include "hamobj/HamDirector.h"
#include "hamobj/HamGameData.h"
#include "hamobj/HamLabel.h"
#include "hamobj/HamMaster.h"
#include "math/Rand.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/PropKeys.h"
#include "synth/FxSendDelay.h"
#include "synth/Synth.h"
#include "utl/BeatMap.h"
#include "utl/DebugMeter.h"
#include "utl/OSCMessenger.h"
#include "utl/Symbol.h"
#include "gesture/SkeletonViz.h"
#include "gesture/SkeletonUpdate.h"
#include "hamobj/HamPhraseMeter.h"
#include "hamobj/HamPlayerData.h"
#include "char/CharClipDriver.h"
#include "rndobj/Rnd.h"

PoseFatalities::PoseFatalities()
    : mHoldDuration(0.5f), mHudPanel(0), mJumpStart(0), mJumpEnd(0), mCurrentBeat(0),
      mDisplayCooldown(0), mFeedbackFlags(0), mDisplayProgress(0) {
    for (int i = 0; i < 2; i++) {
        mInFatality[i] = 0;
        unk1710[i] = 0;
        mMatchingActive[i] = 0;
        mGotFullCombo[i] = 0;
        mComboStartBeat[i] = 0;
    }
    static DataNode &n = DataVariable("pose_fatalities");
    n = this;
}

BEGIN_HANDLERS(PoseFatalities)
    HANDLE_EXPR(get_fatality_beat_lead_in, mFatalityBeatLeadIn)
    HANDLE_EXPR(fatal_active, FatalActive())
    HANDLE_ACTION(activate_fatalities, ActivateFatal(-1))
    HANDLE_ACTION(set_jump, SetJump(_msg->Int(2), _msg->Int(3)))
    HANDLE_EXPR(fatal_end_beat, mFatalEndBeat)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(PoseFatalities)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

bool PoseFatalities::FatalActive() const {
    for (int i = 0; i < 2; i++) {
        if (mInFatality[i]) {
            return true;
        }
    }
    return false;
}

void PoseFatalities::SetJump(int x, int y) {
    mJumpEnd = y;
    mJumpStart = x;
}

bool PoseFatalities::GotFullCombo(int player) const {
    MILO_ASSERT_RANGE(player, 0, 2, 0x3A5);
    return mGotFullCombo[player];
}

Symbol PoseFatalities::GetFatalityFace() {
    Symbol smiles[3] = { "Smile", "Open_Smile_02", "Grin" };
    Symbol ret = smiles[RandomInt(0, 3)];
    if (RandomInt(0, 100) == 0) {
        ret = "O_Face";
    }
    if (RandomInt(0, 100) == 0) {
        ret = "Sexy";
    }
    return ret;
}

bool PoseFatalities::InFatality(int player) const {
    int max = mCurrentBeat;
    const int *starts = mFatalStartBeats;
    if (player == -1) {
        bool b1 = false;
        for (int i = 0; i < 2; i++) {
            if (max >= starts[i]) {
                b1 = true;
            }
        }
        if (b1) {
            return FatalActive();
        }
    } else {
        MILO_ASSERT_RANGE(player, 0, 2, 0x391);
        if (max >= starts[player]) {
            return mInFatality[player];
        }
    }
    return false;
}

bool PoseFatalities::InStrikeAPose() {
    static Symbol gameplay_mode("gameplay_mode");
    static Symbol strike_a_pose("strike_a_pose");
    return TheHamProvider->Property(gameplay_mode, true)->Sym() == strike_a_pose;
}

void PoseFatalities::SetCombo(int player, int combo) {
    mCurrentCombo[player] = combo;
    if (combo != 0) {
        mPoseComboLabels[TheGameData->Player(player)->Side()]->SetInt(combo, false);
    }
}

void PoseFatalities::SetFatalitiesStart(int player) {
    mInFatality[player] = true;
    if (InStrikeAPose()) {
        mFatalStartBeats[player] = mCurrentBeat + 3;
        for (int i = 0; i < 4; i++) {
            if (TheBeatMap->IsDownbeat(mFatalStartBeats[player])) {
                return;
            }
            mFatalStartBeats[player]++;
        }
    } else {
        mFatalStartBeats[player] = mCurrentBeat + 7;
    }
}

void PoseFatalities::Reset() {
    for (int i = 0; i < 2; i++) {
        mFatalStartBeats[i] = -1;
        mInFatality[i] = false;
        mFatalityProgress[i] = 0;
    }
    TheHamProvider->SetProperty("in_fatalities", 0);
    RndAnimatable *anim = TheSynth->Find<RndAnimatable>("beat_repeat.anim", true);
    if (anim) {
        anim->SetFrame(4, 1);
    }
}

void PoseFatalities::PlayVO(Symbol s) {
    static Message stopVOMsg("stop_narrator");
    mHudPanel->Handle(stopVOMsg, true);
    static Message playVOMsg("play", 0);
    playVOMsg[0] = s;
    mHudPanel->Handle(playVOMsg, true);
    mDisplayCooldown = 5;
}

String PoseFatalities::GetCelebrationClip(int player) {
    Symbol outfit =
        GetOutfitCharacter(TheHamDirector->GetCharacter(player)->Outfit(), true);
    static Symbol strikeapose_celebrations("strikeapose_celebrations");
    DataArray *cfg = SystemConfig(strikeapose_celebrations);
    DataArray *a = cfg->FindArray(outfit, true);
    return a->Node(RandomInt(1, a->Size())).Str();
}

void PoseFatalities::EndFatal(int player) {
    bool b10 = true;
    mInFatality[player] = false;
    mMatchingActive[player] = true;
    static Message endFatalityMsg("fatality_end", 0);
    endFatalityMsg[0] = player;
    TheHamProvider->Handle(endFatalityMsg, false);
    if (!InStrikeAPose()) {
        ObjectDir *poseDisplay = TheHamDirector->GetVenueWorld()->Find<ObjectDir>(
            MakeString("final_pose_display%d", player), true
        );
        HamLabel *label = poseDisplay->Find<HamLabel>("pose_combo.lbl", true);
        int num;
        if (mGotFullCombo[player]) {
            num = 8;
        } else {
            num = mCurrentCombo[player] - 1;
        }
        if (mCurrentCombo[player] > 1) {
            label->SetTokenFmt("pose_fatality_3d_combo", num);
        } else {
            label->SetTextToken(gNullStr);
        }
        poseDisplay->Find<RndAnimatable>("pose_combo.anim", true)
            ->Animate(0, false, 0, nullptr, kEaseLinear, 0, false);
    }
    mCurrentCombo[player] = 0;
    if (!DataVariable("restart_fatals").Int() && !InStrikeAPose()) {
        for (int i = 0; i < 2; i++) {
            b10 &= mInFatality[i] != 0;
        }
        if (b10) {
            mAnimTimer = 4;
        }
        TheHamDirector->GetCharacter(player)->BlendOutFaceOverrides(100);
    } else {
        mFatalStartBeats[player] = -1;
        ActivateFatal(player);
    }
}

void PoseFatalities::ActivateFatal(int player) {
    if (!DataVariable("disable_fatalities").Int() || InStrikeAPose()) {
        if (!InStrikeAPose()) {
            static Symbol peak_behavior("peak_behavior");
            static Symbol strike_a_pose("strike_a_pose");
            static Symbol game_stage("game_stage");
            static Symbol none("none");
            TheHamProvider->SetProperty(peak_behavior, strike_a_pose);
            TheHamProvider->SetProperty(game_stage, none);
        }
        if (!InStrikeAPose()) {
            TheHamDirector->ForceShot("area1_far01.shot");
        }
        mValidPose = true;
        if (!FatalActive()) {
            static Message fatalityActivateMsg("fatality_active");
            TheHamProvider->Handle(fatalityActivateMsg, false);
        }
        if (player >= 0) {
            MILO_ASSERT(player < MAX_NUM_PLAYERS, 0x29E);
            SetFatalitiesStart(player);
        } else {
            for (int i = 0; i < 2; i++) {
                SetFatalitiesStart(i);
            }
        }
        if (!InStrikeAPose()) {
            MILO_ASSERT(player >= 0, 0x2A9);
            mFatalEndBeat = mFatalStartBeats[player] + 32;
        }
        TheHamProvider->SetProperty("in_fatalities", true);
        static Message resetDBOvertakeMsg("db_overtake", -1);
        TheHamProvider->Handle(resetDBOvertakeMsg, false);
    }
}

float PoseFatalities::BeatsLeftToMatch(int player) {
    float beat = TheTaskMgr.Beat();
    if (fabsf(beat - mCurrentBeat) > 2.0f) {
        beat = (mJumpEnd - mJumpStart) + beat;
    }
    int num = 4;
    if (mFatalityPoseIndex[player] == 1 && !InStrikeAPose()) {
        num = 8;
    }
    return (mComboStartBeat[player] + num) - beat;
}

void PoseFatalities::BeginFatal(int player) {
    if (mInFatality[player]) {
        mMatchingActive[player] = false;
        if (!InStrikeAPose()) {
            mFatalityPoseIndex[player] = 0;
        }
        mCurrentCombo[player] = 0;
        mGotFullCombo[player] = false;
        AddFatal(player);
        static Message beginFatalityMsg("fatality_begin", 0);
        beginFatalityMsg[0] = player;
        TheHamProvider->Handle(beginFatalityMsg, false);
        TheHamDirector->GetCharacter(player)->BlendInFaceOverrides(100);
    }
}

bool PoseFatalities::CheckMatchingPose(int player) {
    if (InFatality(player)) {
        if (mFatalityProgress[player] >= mHoldDuration)
            return true;
    }
    return false;
}

void PoseFatalities::LoadFatalityClips() {
    if (InStrikeAPose()) {
        mAllFatalityClips.clear();
        for (ObjDirItr<CharClip> it(mHudPanel, true); it != nullptr; ++it) {
            if (strstr(it->Name(), "pose_fatalities_")) {
                mAllFatalityClips.push_back(it);
            }
        }
        MILO_ASSERT(mAllFatalityClips.size() > 0, 0x2D1);
    }
}

void PoseFatalities::Enter() {
    mHudPanel = DataVariable("hud_panel").Obj<ObjectDir>();
    Reset();
    LoadFatalityClips();
    PropKeys *propKeys = TheHamDirector->GetPropKeys(kDifficultyEasy, "move");
    if (propKeys) {
        Keys<Symbol, Symbol> *symKeys = propKeys->AsSymbolKeys();
        static Symbol Rest("Rest.move");
        static Symbol rest("rest.move");
        int idx = 0;
        for (; idx < symKeys->size(); idx++) {
            Key<Symbol> cur = (*symKeys)[idx];
            if (cur.value != Rest && cur.value != rest)
                break;
        }
        mFatalityBeatLeadIn = Max(idx * 4 - 5, 0);
        for (idx = symKeys->size() - 1; idx >= 0; idx--) {
            Key<Symbol> cur = (*symKeys)[idx];
            if (cur.value != Rest && cur.value != rest)
                break;
        }
        if (InStrikeAPose()) {
            mFatalEndBeat = idx * 4;
        }
#ifdef HX_NATIVE
        ObjectDir *hudLeft = mHudPanel->Find<ObjectDir>("hud_left", false);
        ObjectDir *hudRight = mHudPanel->Find<ObjectDir>("hud_right", false);
        mPoseComboLabels[kSkeletonLeft] = hudLeft ? hudLeft->Find<HamLabel>("pose_combo.lbl", true) : nullptr;
        mPoseComboLabels[kSkeletonRight] = hudRight ? hudRight->Find<HamLabel>("pose_combo.lbl", true) : nullptr;
        mPoseBeatAnims[kSkeletonLeft] = hudLeft ? hudLeft->Find<RndAnimatable>("pose_beat.anim", true) : nullptr;
        mPoseBeatAnims[kSkeletonRight] = hudRight ? hudRight->Find<RndAnimatable>("pose_beat.anim", true) : nullptr;
#else
        mPoseComboLabels[kSkeletonLeft] = mHudPanel->Find<ObjectDir>("hud_left", true)
                                              ->Find<HamLabel>("pose_combo.lbl", true);
        mPoseComboLabels[kSkeletonRight] = mHudPanel->Find<ObjectDir>("hud_right", true)
                                               ->Find<HamLabel>("pose_combo.lbl", true);
        mPoseBeatAnims[kSkeletonLeft] = mHudPanel->Find<ObjectDir>("hud_left", true)
                                            ->Find<RndAnimatable>("pose_beat.anim", true);
        mPoseBeatAnims[kSkeletonRight] =
            mHudPanel->Find<ObjectDir>("hud_right", true)
                ->Find<RndAnimatable>("pose_beat.anim", true);
#endif
        mValidPose = InStrikeAPose();
        FxSendDelay *delay = TheSynth->Find<FxSendDelay>("BeatRepeat.send", true);
        if (delay) {
            float bpm = TheMaster->SongData()->GetTempoMap()->GetTempoBPM(0);
            delay->SetProperty("tempo", bpm);
        }
        mCurrentBeat = 0;
        mAnimTimer = -1;
    }
}

void PoseFatalities::PollVO() {
    if (!InStrikeAPose()) {
        return;
    }

    uint flag_val = mFeedbackFlags;
    bool flag1 = flag_val & 1;
    bool flag2 = (flag_val >> 1) & 1;
    if ((flag1 || !flag2)) {
        if (flag1) {
        if (flag2) {
            PlayVO("nar_sap_both_fc");
        } else {
            PlayVO("nar_sap_left_fc");
        }
    } else if (flag1 == 0 && flag2 != 0) {
        PlayVO("nar_sap_right_fc");
    }
    } else if (flag1 == 0 && flag2 != 0) {
        PlayVO("nar_sap_right_fc");
    }
    if (mDisplayCooldown <= 0) {
        if (mFeedbackFlags & 4) {
            PlayVO("nar_sap_gen_pos");
        } else if (mDisplayProgress > 8.0f) {
            PlayVO("nar_sap_time_limit");
            mDisplayProgress = 0;
        }
    } else {
        mDisplayCooldown -= TheTaskMgr.DeltaSeconds();
    }
    mFeedbackFlags = 0;
    if (InFatality(-1) != 0) {
        mDisplayProgress += Clamp(0.0f, 1.0f, TheTaskMgr.DeltaSeconds());
    }
}

void PoseFatalities::Poll() {
#ifdef HX_NATIVE
    // Strike-a-pose battle mode not yet supported on native port.
    // Player data/side lookups crash (LP64 struct mismatch).
    return;
#endif
    if (InStrikeAPose()) {
        TheHamDirector->GetVenueWorld()
            ->Find<HamCharacter>("backup0", true)
            ->SetShowing(false);
        TheHamDirector->GetVenueWorld()
            ->Find<HamCharacter>("backup1", true)
            ->SetShowing(false);
    }
    if (mAnimTimer > 0) {
        mAnimTimer -= TheTaskMgr.DeltaBeat();
        if (mAnimTimer <= 0) {
            static Message msg("fatals_over");
            TheHamDirector->HandleType(msg);
            TheHamProvider->SetProperty("in_fatalities", 0);
        }
    }
    int deltaBeat = TheTaskMgr.Beat() + 0.2f;
    if (deltaBeat > mCurrentBeat) {
        mCurrentBeat = deltaBeat;
        if (mCurrentBeat == mJumpStart) {
            mCurrentBeat = mJumpEnd;
        }
        if (mCurrentBeat == mJumpEnd) {
            MILO_LOG("Jump detected! %d to %d\n", mJumpStart, mJumpEnd);
            int diff = mJumpEnd - mJumpStart;
            for (int i = 0; i < 2; i++) {
                if (mFatalStartBeats[i] != -1) {
                    mFatalStartBeats[i] += diff;
                    mComboStartBeat[i] += diff;
                }
            }
        }
        OnBeat(mCurrentBeat);
    }
    if (mValidPose) {
        for (int i = 0; i < 2; i++) {
            if (InFatality(i) || mMatchingActive[i]) {
                UpdateClipDriver(i);
            }
            if (InFatality(i)) {
                CharCameraInput input(TheHamDirector->GetCharacter(i));
                input.SetUnk2430(true);
                input.PollTracking();
                const SkeletonFrame *frame = input.NewFrame();
                if (frame) {
                    mPlayerSkeletons[i].Poll(0, *frame);
                }
            }
            UpdateMatchingPose(i);
#ifdef HX_NATIVE
            if (mPoseBeatAnims[TheGameData->Player(i)->Side()])
#endif
            mPoseBeatAnims[TheGameData->Player(i)->Side()]->SetFrame(
                4.0f - BeatsLeftToMatch(i), 1
            );
        }

        // probably an inline
        bool p12 = mCurrentBeat < mFatalStartBeats[0] ? false : mInFatality[0];

        bool b9 = p12 || mGotFullCombo[0]
            || !(InStrikeAPose() || mCurrentBeat >= mFatalStartBeats[0]);

        // also probably an inline
        p12 = mCurrentBeat < mFatalStartBeats[1] ? false : mInFatality[1];
        bool b10 = p12 || mGotFullCombo[1]
            || !(InStrikeAPose() || mCurrentBeat >= mFatalStartBeats[1]);

        int prop;
        if (b9 && b10) {
            prop = 2;
        } else if (b9) {
            prop = 0;
        } else if (b10) {
            prop = 1;
        } else {
            prop = 3;
        }
        static Symbol dance_battle_config("dance_battle_config");
        TheHamProvider->SetProperty(dance_battle_config, prop);
        mHoldDuration = TheOSCMessenger.GetFloat("/holdduration", 0.5f);
        PollVO();
    }
}

void PoseFatalities::AddFatal(int player) {
    mComboStartBeat[player] = mCurrentBeat;
    if (InStrikeAPose() && !DataVarExists("debug_pose_char")) {
        int cur = mFatalityPoseIndex[player];
        int rand;
        do {
            rand = RandomInt(1, 9);
        } while (rand == cur);
        mFatalityPoseIndex[player] = rand;
    } else {
        mFatalityPoseIndex[player]++;
    }
    mFatalityProgress[player] = 0;
    unk1718[player] = -0.5f;
    HamCharacter *hChar = TheHamDirector->GetCharacter(player);
    CharDriver *driver = hChar->Driver();
    CharClip *randClip;
    if (InStrikeAPose()) {
        if (DataVarExists("debug_pose_char")) {
            FOREACH (it, mAllFatalityClips) {
                const char *fatalStr = MakeString(
                    "pose_fatalities_%s", DataVariable("debug_pose_char").Str()
                );
                randClip = *it;
                if (streq(randClip->Name(), fatalStr)) {
                    MILO_LOG(
                        "%s %s\n",
                        fatalStr,
                        MakeString("pose_fatality_%i", mFatalityPoseIndex[player] - 1)
                    );
                    goto lab5548;
                }
            }
            goto lab55b4;
        }
        int randListIdx = RandomInt(0, mAllFatalityClips.size());
        auto it = mAllFatalityClips.begin();
        for (int i = 0; i < randListIdx; ++i, ++it)
            ;
        randClip = *it;
    } else {
        const char *str =
            MakeString("pose_fatalities_%s", GetOutfitCharacter(hChar->Outfit()));
        auto _tmp10 = driver->FindClip(str, true);
        randClip = _tmp10;
    }
lab5548:
    if (randClip) {
        driver->Play(randClip, 0x402, -1, kHugeFloat, 0);
        TheHamDirector->CurShot()->Reteleport(
            Vector3::GetZero(), false, MakeString("player%d", player)
        );
    }
lab55b4:
    SetCombo(player, mCurrentCombo[player] + 1);
    static Message addedFatalityMsg("fatality_added", 0, 0);
    addedFatalityMsg[0] = player;
    addedFatalityMsg[1] = mCurrentCombo[player];
    TheHamProvider->Handle(addedFatalityMsg, false);
    hChar->BlendInFaceOverrideClip(GetFatalityFace(), 1, 1);
}

void PoseFatalities::OnFatalResult(int player, bool hit) {
    if (mInFatality[player]) {
        if (hit) {
            static Symbol score("score");
            int scoreProp =
                TheGameData->Player(player)->Provider()->Property(score, true)->Int();
            int newScoreProp;
            if (mCurrentCombo[player] < 8) {
                newScoreProp = mCurrentCombo[player] * 2000 + scoreProp;
            } else {
                newScoreProp = scoreProp + 40000;
            }
            TheGameData->Player(player)->Provider()->SetProperty(score, newScoreProp);
            static Message poseMatchedMsg("fatality_matched", 0, 0, 0);
            poseMatchedMsg[0] = player;
            poseMatchedMsg[1] = mCurrentCombo[player];
            poseMatchedMsg[2] = TheGameData->Player(player)->Side();
            TheHamProvider->Handle(poseMatchedMsg, false);
            mDisplayProgress = 0;
            mFeedbackFlags |= 4;
            if (InStrikeAPose()) {
                TheSynth->Find<RndAnimatable>("beat_repeat.anim", true)
                    ->Animate(0, false, 0, nullptr, kEaseLinear, 0, false);
            }
        } else {
            static Message poseMissedMsg("fatality_missed", 0, 0);
            poseMissedMsg[0] = player;
            poseMissedMsg[1] = TheGameData->Player(player)->Side();
            TheHamProvider->Handle(poseMissedMsg, false);
            HamCharacter *hChar = TheHamDirector->GetCharacter(player);
            hChar->BlendInFaceOverrideClip("Angry", 1, 1);
        }
        static Symbol move_perfect("move_perfect");
        static Symbol move_bad("move_bad");
        static Message moveFinishedMsg("move_finished", 0, 0);
        moveFinishedMsg[0] = player;
        moveFinishedMsg[1] = hit ? move_perfect : move_bad;
        TheHamProvider->Handle(moveFinishedMsg, false);
        bool b11 = false;
        if (hit) {
            if (mCurrentCombo[player] == 8) {
                static Message poseAllMatchedMsg("fatality_all_matched", 0, 0);
                poseAllMatchedMsg[0] = player;
                poseAllMatchedMsg[1] = TheGameData->Player(player)->Side();
                TheHamProvider->Handle(poseAllMatchedMsg, false);
                mGotFullCombo[player] = true;
                mFatalityProgress[player] = 0;
                if (TheGameData->Player(player)->Side() == kSkeletonLeft) {
                    mDisplayProgress = 0;
                    mFeedbackFlags |= 1;
                }
                if (TheGameData->Player(player)->Side() == kSkeletonRight) {
                    mDisplayProgress = 0;
                    mFeedbackFlags |= 2;
                }
                CharDriver *driver = TheHamDirector->GetCharacter(player)->Driver();
                CharClip *celebrationClip =
                    driver->FindClip(GetCelebrationClip(player), true);
                if (celebrationClip) {
                    driver->Play(celebrationClip, 2, -1, kHugeFloat, 0);
                }
            } else if (mCurrentBeat < mFatalEndBeat) {
                AddFatal(player);
                b11 = true;
            }
        }
        if (!b11) {
            EndFatal(player);
        }
    }
}

void PoseFatalities::OnBeat(int beat) {
    if (InStrikeAPose() && !DataVarExists("debug_pose_char")) {
        if (beat >= mFatalEndBeat) {
            for (int i = 0; i < 2; i++) {
                mInFatality[i] = false;
                TheHamDirector->GetCharacter(i)->BlendOutFaceOverrides(100);
            }
            return;
        }
    } else {
        if (beat + 4 >= mFatalStartBeats[0] && (mInFatality[0] || mInFatality[1])) {
            static Symbol game_stage("game_stage");
            static Symbol playing("playing");
            TheHamProvider->SetProperty(game_stage, playing);
            if (beat + 4 == mFatalStartBeats[0]) {
                PlayVO("nar_pose_fatalities");
            }
        }
    }
    for (int i = 0; i < 2; i++) {
        if (beat == mFatalStartBeats[i]) {
            BeginFatal(i);
        }
    }
    if (InFatality(-1)) {
        static Message beatMsg("fatality_beat");
        TheHamProvider->Handle(beatMsg, false);
    } else if (FatalActive()) {
        int startBeat = 0;
        for (int i = 0; i < 2; i++) {
            int cur = mFatalStartBeats[i];
            if (cur != -1) {
                startBeat = cur;
                if (startBeat - beat >= 10) {
                    MILO_FAIL(
                        "Start beat is too far off!  Player %d, startBeat %d, beat %d\n",
                        i,
                        startBeat,
                        beat
                    );
                }
            }
        }
        static Message beatLeadInMsg("fatality_lead_in_beat", 0);
        beatLeadInMsg[0] = startBeat - beat;
        TheHamProvider->Handle(beatLeadInMsg, false);
    }
    if (!DataVariable("fatal_debug").Int()) {
        if (!DataVarExists("debug_pose_char")) {
            for (int i = 0; i < 2; i++) {
                if (InFatality(i)) {
                    if (CheckMatchingPose(i)) {
                        OnFatalResult(i, true);
                    } else if ((int)BeatsLeftToMatch(i) <= 0) {
                        OnFatalResult(i, false);
                    }
                }
            }
            return;
        }
    }
    DataVariable("debug_endless_strikeapose") = 1;
    bool p9 = mCurrentBeat < mFatalStartBeats[0] ? false : mInFatality[0];
    if (p9) {
        JoypadData *jData = JoypadGetPadData(0);
        if (jData->GetRX() > 0.5f) {
            AddFatal(0);
        }
        if (jData->GetRX() < -0.5f) {
            mFatalityPoseIndex[0] -= 2;
            AddFatal(0);
        }
        if (jData->GetLY() > 0.5f) {
            OnFatalResult(0, false);
        }
    }
}

void PoseFatalities::UpdateClipDriver(int player) {
    HamCharacter *hChar = TheHamDirector->GetCharacter(player);
    CharClipDriver *clipDriver = hChar->Driver()->First();
    if (clipDriver != nullptr) {
        while (clipDriver != nullptr) {
            if (strstr(clipDriver->GetClip()->Name(), "pose_fatalities_")) {
                break;
            }
            clipDriver = clipDriver->Next();
        }
        if (clipDriver != nullptr && clipDriver->GetClip() != nullptr) {
            MILO_ASSERT(NUM_FATALITIES == clipDriver->NumBeatEvents(), 0x123);
            Symbol beatSym(MakeString("pose_fatality_%i", mFatalityPoseIndex[player] - 1));
            clipDriver->SetBeatOffset(unk1718[player], kTaskBeats, beatSym);
            if (unk1718[player] < 0.0f) {
                unk1718[player] += TheTaskMgr.DeltaUISeconds();
            }
        }
    }
}

void PoseFatalities::DrawDebug() {
    static SkeletonViz *sVizLeft = nullptr;
    static SkeletonViz *sVizRight = nullptr;

    if (sVizLeft == nullptr) {
        sVizLeft = Hmx::Object::New<SkeletonViz>();
        sVizLeft->Init();
        sVizRight = Hmx::Object::New<SkeletonViz>();
        sVizRight->Init();
    }

    SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
    float screenScale = 0.25f / TheRnd.YRatio();

    // Player 0
    bool player0Active = false;
    if (mCurrentBeat >= mFatalStartBeats[0]) {
        player0Active = mInFatality[0];
    }

    if (player0Active) {
        if (DataVariable("fatal_debug").Int()) {
            const Skeleton *playerSkel = TheGameData->Player(0)->GetSkeleton();

            static DebugMeter meterA(0.1f, 0.1f, 0.5f, 0.1f, Hmx::Color(0, 0, 0, 1));
            meterA.Draw();

            float rawCompare = mRecorder.CompareSkeletonPositions(
                playerSkel, &mPlayerSkeletons[0], 1.0f
            );
            Hmx::Color whiteColor(0, 1, 0, 1);
            meterA.DrawBar(0.0f, rawCompare, whiteColor, 1.0f, 0.0f);

            float errorWeight = TheOSCMessenger.GetFloat("/fatalposeerrorweight", 0.0f);
            float weightedCompare = mRecorder.CompareSkeletonPositions(
                playerSkel, &mPlayerSkeletons[0], errorWeight
            );
            float thresh = TheOSCMessenger.GetFloat("/fatalposethresh", 0.0f);
            float normalizedScore = weightedCompare / thresh;
            normalizedScore = Clamp(0.0f, 1.0f, normalizedScore);

            static DebugMeter meterB(0.1f, 0.3f, 0.5f, 0.1f, Hmx::Color(0, 0, 0, 1));
            meterB.Draw();
            Hmx::Color greenColor(0, 0, normalizedScore * normalizedScore, 1);
            meterB.DrawBar(0.0f, 1.0f, greenColor, 1.0f, 0.0f);
            Hmx::Color blueColor(0, 0, 0, 1);
            meterB.DrawBar(0.0f, weightedCompare, blueColor, 1.0f, 0.0f);

            static DebugMeter meterC(
                0.5f + 0.25f + 0.1f, 0.0f, 0.25f, 0.03f,
                Hmx::Color(0, 0, 0, 1)
            );
            meterC.Draw();
            Hmx::Color redColor(0, 0, unk1710[0] * unk1710[0], 1);
            meterC.DrawBar(0.0f, 1.0f, redColor, 1.0f, 0.0f);

            float progressFrac = mFatalityProgress[0] / mHoldDuration;
            Hmx::Color yellowColor(0, 1, 0, 1);
            progressFrac = Clamp(0.0f, 1.0f, progressFrac);
            meterC.DrawBar(0.0f, progressFrac, yellowColor, 1.0f, 0.0f);
        }

        if (TheOSCMessenger.GetInt("/posefatalitiesdrawdebugskel", 0)) {
            Hmx::Rect rect(
                0.25f + 0.1f, 0.0f, 0.25f, screenScale
            );
            Hmx::Color bgColor(0, 0, 0, 0.4f);
            TheRnd.DrawRectScreen(rect, bgColor, nullptr, nullptr, nullptr);
            sVizLeft->SetUsePhysicalCam(true);
            sVizLeft->SetPhysicalCamScreenRect(rect);
            sVizLeft->Visualize(
                *handle.GetCameraInput(), mPlayerSkeletons[0], nullptr, false
            );
        }
    }

    // Player 1
    bool player1Active = false;
    if (mCurrentBeat >= mFatalStartBeats[1]) {
        player1Active = mInFatality[1];
    }

    if (player1Active) {
        if (DataVariable("fatal_debug").Int()) {
            static DebugMeter meterD(
                0.5f, 0.0f, 0.25f, 0.03f,
                Hmx::Color(0, 0, 0, 1)
            );
            meterD.Draw();
            Hmx::Color p1Color(0, 0, unk1710[1] * unk1710[1], 1);
            meterD.DrawBar(0.0f, 1.0f, p1Color, 1.0f, 0.0f);

            float progressFrac1 = mFatalityProgress[1] / mHoldDuration;
            Hmx::Color p1ProgressColor(0, 1, 0, 1);
            progressFrac1 = Clamp(0.0f, 1.0f, progressFrac1);
            meterD.DrawBar(0.0f, progressFrac1, p1ProgressColor, 1.0f, 0.0f);
        }

        if (TheOSCMessenger.GetInt("/posefatalitiesdrawdebugskel", 0)) {
            Hmx::Rect rect1(
                0.5f, 0.0f, 0.25f, screenScale
            );
            Hmx::Color bgColor1(0, 0, 0, 0.4f);
            TheRnd.DrawRectScreen(rect1, bgColor1, nullptr, nullptr, nullptr);
            sVizRight->SetUsePhysicalCam(true);
            sVizRight->SetPhysicalCamScreenRect(rect1);
            sVizRight->Visualize(
                *handle.GetCameraInput(), mPlayerSkeletons[1], nullptr, false
            );
        }
    }
}

void PoseFatalities::UpdateMatchingPose(int player) {
    bool matching = false;
    unk1710[player] = 0;

    float deltaBeat = TheTaskMgr.DeltaBeat();
    float clampedDelta = Clamp(0.0f, 1.0f, deltaBeat);

    if (InFatality(player)) {
        HamPlayerData *playerData = TheGameData->Player(player);
        const Skeleton *playerSkel = playerData->GetSkeleton();

        float errorWeight = TheOSCMessenger.GetFloat("/fatalposeerrorweight", 0.0f);
        float rawScore = mRecorder.CompareSkeletonPositions(
            playerSkel, &mPlayerSkeletons[player], errorWeight
        );

        float thresh = TheOSCMessenger.GetFloat("/fatalposethresh", 0.0f);
        unk1710[player] = rawScore / thresh;
        unk1710[player] = Clamp(0.0f, 1.0f, unk1710[player]);

        if (unk1710[player] >= 1.0f && mFatalityProgress[player] >= 0.0f) {
            matching = true;
        }

        if (mFatalityProgress[player] >= 0.0f) {
            JoypadData *jData = JoypadGetPadData(0);
            if (player == 0) {
                if (jData->GetRT() > 0.5f
                    || !TheGameData->Player(0)->Autoplay().Null()) {
                    matching = true;
                }
            }
            if (player == 1) {
                if (jData->GetLT() > 0.5f
                    || !TheGameData->Player(1)->Autoplay().Null()) {
                    matching = true;
                }
            }
        }
    }

    float *progress = &mFatalityProgress[player];
    if (matching) {
        *progress = *progress + clampedDelta;
    } else {
        float holdDecay = TheOSCMessenger.GetFloat("/holddecay", 0.0f);
        float decayAmount = Clamp(0.0f, 1.0f, holdDecay * clampedDelta);
        *progress = *progress * (1.0f - decayAmount);
    }

    float displayFrac = *progress / mHoldDuration;
    float clampedFrac = Clamp(0.0f, 1.0f, displayFrac);

    ObjectDir *venueWorld = TheHamDirector->GetVenueWorld();
    HamPhraseMeter *meter = venueWorld->Find<HamPhraseMeter>(
        MakeString("phrase_meter%i", player), true
    );
    meter->SetRatingFrac(Clamp(0.0f, 1.0f, clampedFrac), -1.0f);
    meter->SetShowing(true);
    RndAnimatable *feedbackAnim =
        meter->Find<RndAnimatable>("perimeter_feedback_color.anim", true);
    feedbackAnim->SetFrame(unk1710[player] * 4.0f, 1.0f);
}
