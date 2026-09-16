#include "game/ChordbookPanel.h"
#include "bandobj/BandLabel.h"
#include "bandtrack/GemTrack.h"
#include "beatmatch/BeatMatchController.h"
#include "beatmatch/GameGem.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/RGState.h"
#include "beatmatch/RGUtl.h"
#include "beatmatch/TrackType.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/Defines.h"
#include "game/GameConfig.h"
#include "game/GemPlayer.h"
#include "game/GemTrainerPanel.h"
#include "game/RGTrainerPanel.h"
#include "game/SongDB.h"
#include "game/TrainerProgressMeter.h"
#include "meta_band/BandProfile.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SongStatusMgr.h"
#include "meta_band/TrainingMgr.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "rndobj/Anim.h"
#include "rndobj/PropAnim.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "utl/Messages.h"
#include "utl/Messages3.h"
#include "utl/Messages4.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"

ChordbookPanel::ChordbookPanel()
    : mStartChordbook(0), mGemPlayer(0), unk44(0), unk4c(0), mChordLegend(0),
      mGameGemList(0), mChordWid(0), mFretWid(0), mLabelWid(0), mNumSteps(0),
      mController(0), mCurrentChord(0), unk70(0), unk74(-1), mNumChords(0), unk66c(0),
      unk674(0), mCorrect(0), mInUse(0), mStrum(0), mChordPreview(0), unk6c4(0),
      unk6c5(0), mProgressMeter(0), mRelearnChords(0) {
    mProgressMeter = new TrainerProgressMeter();
    for (int i = 0; i < 6; i++) {
        mStringSwings[i] = 0;
    }
}

ChordbookPanel::~ChordbookPanel() { delete mProgressMeter; }

// W16-CS: retail's Enter carries THREE statements this body does not, and the
// reconstruction below is verified against retail bytes + compiler ground truth.
// It is deliberately NOT applied -- see the blocker at the end.
//
// Retail 0xadc-0xb48, ahead of the UIPanel::Enter() call:
//     bl   TrainingMgr::GetTrainingMgr
//     lwz  r30, 0x28(r3)                  TrainingMgr::mUser  (LocalBandUser*)
//     <vbase adjust>  lwz r11,0x4(r30); lwz r11,0x8(r11); add r11,r11,r30
//     lwz  r3, 0x90(r11) ; __RTDynamicCast(src=Player, tgt=GemPlayer)  DISCARDED
//     lwz  r3, 0x8c(r11) ; __RTDynamicCast(src=Track,  tgt=GemTrack)
//     if (r3) { lwz r11, 0x84(r3); stw r11, 0x50(r31) }      -> unk4c
//
// The two type names are read straight out of retail .data: the ??_R0 type
// descriptors at 0x82C74FFC/0x82C75014/0x82C75618/0x82C75600 carry
// ".?AVPlayer@@" / ".?AVGemPlayer@@" / ".?AVTrack@@" / ".?AVGemTrack@@" at +8.
// __RTDynamicCast is (inptr, VfDelta, SrcType, TargetType, isReference), so
// r5=src and r6=target.
//
// The member offsets look 4 high only until the vtordisp is accounted for: the
// vbtable entry for a vbase carrying a vtordisp points AT THE VTORDISP WORD, and
// BandUser.h records vtordisp(BandUser) 0x68 / BandUser 0x6c inside
// LocalBandUser. So 0x8c = LBU+0xf4 = BandUser+0x88 = mTrack, and 0x90 =
// LBU+0xf8 = BandUser+0x8c = mPlayer -- both EXACTLY as commented. Confirmed two
// ways: class_layout_report.py --check-header reports every // 0xHEX comment in
// BandUser.h and GemTrack.h agreeing with the compiler, and
// ChordbookPanel::GetChordbookPlayer (scored in default/PracticePanel, 80 B)
// matches retail at 100.0% while emitting this identical `lwz r3, 0x90(r11)`.
// There is NO BandUser layout defect here; do not "fix" one.
// GemTrack::mTrackDir is ObjPtr<GemTrackDir> at 0x7c and --offset 0x84 answers
// "mTrackDir + 8", i.e. retail's 0x84 load IS GetTrackDir().
//
// The first cast's result being discarded is the house MILO_ASSERT shape: this
// build compiles MILO_ASSERT(cond,line) to ((void)(cond)), so the call is
// emitted and the value dropped. Both casts share ONE GetTrainingMgr call and
// ONE 0x28 load, so retail hoists the user once:
//
//     LocalBandUser *user = TrainingMgr::GetTrainingMgr()->GetUser();
//     MILO_ASSERT(dynamic_cast<GemPlayer *>(user->GetPlayer()), <line>);
//     GemTrack *track = dynamic_cast<GemTrack *>(user->GetTrack());
//     if (track) unk4c = track->GetTrackDir();
//
// and a tail  if (mGemPlayer) fn_826B5848(this, 0);  at 0xbc4-0xbd8.
//
// BLOCKER (why this is documented rather than applied): fn_826B5848 is a 316 B
// ANONYMOUS row in this unit. matched_code is all-or-nothing per row, so Enter
// cannot reach fuzzy==100 until that callee is IDENTIFIED -- which is map work,
// not source work. Writing the prologue alone moves Enter off 50.4474 but buys
// exactly 0 bytes while putting the 28 currently-equal instructions at risk.
// Name fn_826B5848 and this lands in one shot for 304 B.
void ChordbookPanel::Enter() {
    LocalBandUser *user = TrainingMgr::GetTrainingMgr()->GetUser();
    MILO_ASSERT(dynamic_cast<GemPlayer *>(user->GetPlayer()), 0x1A5);
    GemTrack *track = dynamic_cast<GemTrack *>(user->GetTrack());
    if (track) {
        unk4c = track->GetTrackDir();
    }
    UIPanel::Enter();
    mChordLegend = mDir->Find<RndDir>("chord_legend", true);
    mGemPlayer = GetChordbookPlayer();
    if (mGemPlayer) {
        unk44 = mGemPlayer->GetUser();
        CreateController();
        JoypadSubscribe(this);
    }
    mState = RGState();
    unk6c5 = false;
    unk6c8 = 0;
    if (mGemPlayer) {
        HandleLegendLefty(false);
    }
}

void ChordbookPanel::HandleLegendLefty(bool b) {
    mLefty = mGemPlayer->GetUser()->GetGameplayOptions()->GetLefty();
    float f2, f12;
    if (mLefty) {
        f12 = 1.0f;
        f2 = 0.0f;
    } else {
        f2 = 1.0f;
        f12 = 0.0f;
    }
    RndAnimatable *leftyAnim = mChordLegend->Find<RndAnimatable>("lefty_flip.anim", true);
    if (b) {
        leftyAnim->Animate(f2, f12, 0);
    } else {
        leftyAnim->SetFrame(0.0f, 1.0f);
    }
    GemTrack *track = dynamic_cast<GemTrack *>(mGemPlayer->GetUser()->GetTrack());
    track->UpdateLeftyFlip();
}

DECOMP_FORCEACTIVE(
    ChordbookPanel,
    "fret_%02d.lbl",
    "button_skip.lbl",
    "button_confirm.lbl",
    "button_cancel.lbl",
    "skip_chordbook.lbl",
    "rg_chordbook_skip",
    "skip_verify.lbl",
    "rg_chordbook_skip_verify",
    "confirm.lbl",
    "help_confirm",
    "cancel.lbl",
    "help_cancel",
    "song_name.lbl",
    "artist_name.lbl",
    "difficulty.lbl",
    __FILE__,
    "pTrainingMgr",
    "user",
    "pause_track",
    "rg_trainer_panel",
    "track_graphics",
    "gem",
    "gems",
    "real_guitar",
    "chord",
    "normal",
    "chord_fret",
    "chord_label",
    "progress_meter"
)

void ChordbookPanel::Load() { UIPanel::Load(); }
void ChordbookPanel::Unload() { UIPanel::Unload(); }
bool ChordbookPanel::IsLoaded() const { return UIPanel::IsLoaded(); }
void ChordbookPanel::FinishLoad() { UIPanel::FinishLoad(); }
void ChordbookPanel::StartChordbook() { mStartChordbook = true; }

bool ChordbookPanel::HasChords() const {
    GemPlayer *player = GetChordbookPlayer();
    if (!player)
        return false;
    else {
        const GameGemList *list = TheSongDB->GetGemList(player->GetTrackNum());
        std::vector<GameGem> gems = list->mGems;
        for (int i = 0; i < gems.size(); i++) {
            if (gems[i].IsRealGuitarChord())
                return true;
        }
    }
    return false;
}

void ChordbookPanel::PushSkipDialog() {
    if (!unk674) {
        unk674 = true;
        mDir->Find<EventTrigger>("skip_chordbook.trig", true)->Trigger();
    }
}

void ChordbookPanel::ResetSkipDialog() {
    if (unk674) {
        unk674 = false;
        mDir->Find<EventTrigger>("skip_chordbook_cancel.trig", true)->Trigger();
    }
}

DECOMP_FORCEACTIVE(
    ChordbookPanel,
    "success.trig",
    "player",
    "mNumSteps <= kMaxSteps",
    "set_step_progress",
    "step_progress_%02d.anim"
)

void ChordbookPanel::Exit() {
    UIPanel::Exit();
    if (mGemPlayer) {
        RELEASE(mController);
        JoypadUnsubscribe(this);
        mGemPlayer = nullptr;
    }
    mState = RGState();
    unk6c5 = false;
    unk6c8 = 0;
}

void ChordbookPanel::Draw() {
    UIPanel::Draw();
    TheUI->GetCam()->Select();
    mProgressMeter->Draw();
}

GemPlayer *ChordbookPanel::GetChordbookPlayer() const {
    TrainingMgr *pTrainingMgr = TrainingMgr::GetTrainingMgr();
    MILO_ASSERT(pTrainingMgr, 0x18E);
    LocalBandUser *user = pTrainingMgr->GetUser();
    MILO_ASSERT(user, 0x191);
    return dynamic_cast<GemPlayer *>(user->GetPlayer());
}

void ChordbookPanel::ExitChordbook() {
    unk4c->SetRunning(true);
    static Message end_chordbook_msg("end_chordbook");
    ObjectDir::Main()->Find<UIPanel>("rg_trainer_panel", true)
        ->HandleType(end_chordbook_msg);
    mChordPreview->Start(0);
    SetShowing(false);
}

bool ChordbookPanel::ProfileCheckComplete(Symbol s, GemPlayer *player) {
    MILO_ASSERT(player, 0x1B3);
    LocalBandUser *user = player->GetUser()->GetLocalBandUser();
    MILO_ASSERT(user, 0x1B5);
    BandProfile *profile = TheProfileMgr.GetProfileForUser(user);
    if (profile) {
        SongStatusMgr *mgr = profile->GetSongStatusMgr();
        if (mgr) {
            return mgr->GetSongStatusFlag(
                MetaPerformer::Current()->Song(),
                kSongStatusFlag_ChordbookComplete,
                kScoreRealGuitar,
                user->GetDifficulty()
            );
        }
    }
    return false;
}

void ChordbookPanel::Poll() {
    UIPanel::Poll();
    if (Exiting() || !mGemPlayer)
        return;
    else {
        GameplayOptions *opts = unk44->GetGameplayOptions();
        bool old = mLefty;
        if (old != opts->GetLefty()) {
            mLefty = !old;
            mController->mLefty = !old;
        }
    }
}

bool ChordbookPanel::Swing(int i1, bool, bool, bool, bool, GemHitFlags) {
    for (int i = 0; i < 6; i++) {
        if (i1 & (1 << i)) {
            StrumString(i);
        }
    }
    TheRGTrainerPanel->Swing(i1);
    return false;
}

#define kNumFrets 24

void ChordbookPanel::FretButtonDown(int i1, int i2) {
    int string, fret;
    UnpackRGData(i1, string, fret);
    MILO_ASSERT(string < kNumStrings, 0x2B2);
    MILO_ASSERT(fret < kNumFrets, 0x2B3);
    SetFret(string, fret);
    mState.FretDown(string, fret);
    TheRGTrainerPanel->FretButtonDown(i1);
}

void ChordbookPanel::FretButtonUp(int i1) {
    int string, fret;
    UnpackRGData(i1, string, fret);
    MILO_ASSERT(string < kNumStrings, 0x2BF);
    MILO_ASSERT(fret < kNumFrets, 0x2C0);
    SetFret(string, 0);
    mState.FretUp(string, fret);
    TheRGTrainerPanel->FretButtonUp(i1);
}

void ChordbookPanel::SetFret(int string, int fret) {
    bool changed = mFret[string] != fret;
    mFret[string] = fret;
    static Message set_finger_fret("set_finger_fret", 0, 0);
    set_finger_fret[0] = string + 1;
    set_finger_fret[1] = fret;
    mChordLegend->HandleType(set_finger_fret);
    int curFret = mChords[mCurrentChord].fretHand.GetFret(string);
    bool correct = fret == curFret;
    SetCorrect(string, correct);
    if (Showing() && correct && fret != 0 && changed) {
        static Message play_correct_fret_msg("play_correct_fret");
        Handle(play_correct_fret_msg, true);
    }
    if (!ChordComplete())
        mStrum = 0;
    TheRGTrainerPanel->SetFret(string, fret);
}

bool ChordbookPanel::ChordComplete() const {
    for (int i = 0; i < 6; i++) {
        if ((mInUse & (1 << i)) && !(mCorrect & (1 << i)))
            return false;
    }
    return true;
}

void ChordbookPanel::SetCorrect(int string, bool correct) {
    int mask = 1 << string;
    bool maskExists = mCorrect & mask;
    if (maskExists != correct) {
        if (correct) {
            mCorrect |= mask;
        } else
            mCorrect &= ~mask;
    }
}

#pragma push
#pragma pool_data off
void ChordbookPanel::StrumString(int string) {
    if (mGameGemList) {
        if (mChordLegend->Showing()) {
            if (CurrentChord().gemId < mGameGemList->NumGems()) {
                static Message strum_string("strum_string", 0);
                static Message strum_used_string("strum_used_string", 0);
                if (mGameGemList->GetGem(mChords[mCurrentChord].gemId).GetFret(string) >= 0) {
                    strum_used_string[0] = string + 1;
                    mChordLegend->HandleType(strum_used_string);
                } else {
                    strum_string[0] = string + 1;
                    mChordLegend->HandleType(strum_string);
                }
            }
        }
        mStringSwings[string] = TheTaskMgr.UISeconds() * 1000.0f;
        if (ChordComplete())
            mStrum |= 1 << string;
        TheRGTrainerPanel->StrumString(string);
    }
}
#pragma pop

#define kMaxChords 20

inline ChordbookPanel::ChordInfo &ChordbookPanel::CurrentChord() {
    MILO_ASSERT(mCurrentChord < kMaxChords, 0x315);
    return mChords[mCurrentChord];
}

Symbol ChordbookPanel::RGFingerStep(int finger) {
    switch (finger) {
    case 1:
        return rg_chordbook_finger_1;
    case 2:
        return rg_chordbook_finger_2;
    case 3:
        return rg_chordbook_finger_3;
    case 4:
        return rg_chordbook_finger_4;
    default:
        MILO_FAIL("%d invalid finger number, should be 1 thru 4", finger);
        return rg_chordbook_finger_1;
    }
}

Symbol ChordbookPanel::RGStringToken(int string, bool is_short) {
    switch (string) {
    case 0:
        return is_short ? rg_chordbook_low_e_short : rg_chordbook_low_e_string;
    case 1:
        return is_short ? rg_chordbook_a_short : rg_chordbook_a_string;
    case 2:
        return is_short ? rg_chordbook_d_short : rg_chordbook_d_string;
    case 3:
        return is_short ? rg_chordbook_g_short : rg_chordbook_g_string;
    case 4:
        return is_short ? rg_chordbook_b_short : rg_chordbook_b_string;
    case 5:
        return is_short ? rg_chordbook_high_e_short : rg_chordbook_high_e_string;
    default:
        MILO_FAIL("no string label for string number %d", string);
        return rg_chordbook_low_e_string;
    }
}

void ChordbookPanel::CreateController() {
    RELEASE(mController);
    MILO_ASSERT(mGemPlayer, 0x374);
    BandUser *user = mGemPlayer->GetUser();
    MILO_ASSERT(user, 0x377);
    Symbol ctrl = TheGameConfig->GetController(user);
    DataArray *cfg = SystemConfig("beatmatcher", "controller", ctrl);
    mController = NewController(
        user, cfg, this, false, user->GetGameplayOptions()->GetLefty(), kNumTrackTypes
    );
}

#pragma push
#pragma pool_data off
void ChordbookPanel::DisplayChord(unsigned int idx) {
    MILO_ASSERT(idx < mNumChords, 0x381);
    static Message reset_msg("reset");
    mDir->HandleType(reset_msg);
    mChordWid->Clear();
    mFretWid->Clear();
    mLabelWid->Clear();
    for (int i = 0; i < 6; i++) {
        mFret[i] = 0;
        SetCorrect(i, false);
    }
    mStrum = 0;
    mInUse = 0;
    unk66c = false;
    static Message set_chord_fret("set_chord_fret", 0, -1);
    mNumSteps = 0;
    for (int i = 0; i < 4; i++) {
        int fret = 0;
        int lowstr = 0;
        int highstr = 0;
        mChords[idx].fretHand.GetFinger(i, fret, lowstr, highstr);
        unsigned int oldStepNum = mNumSteps;
        mStep[oldStepNum].unk0 = 0;
        mStep[oldStepNum].unk4 = 0;
        mStep[oldStepNum].unk5 = 0;
        if (fret > 0) {
            MaxEq(highstr, lowstr);
            for (int j = lowstr; j < highstr; j++) {
                set_chord_fret[0] = j;
                set_chord_fret[1] = fret;
                mChordLegend->HandleType(set_chord_fret);
                int mask = 1 << j;
                mStep[oldStepNum].unk4 |= mask;
                mInUse |= mask;
            }
            for (int j = 0; j < mNumSteps; j++) {
                mStep[j].unk4 &= ~mStep[oldStepNum].unk4;
            }
            BandLabel *label = mDir->Find<BandLabel>(
                MakeString("step_%02d_text.lbl", mNumSteps + 1), true
            );
            static Symbol rg_chordbook_step_finger("rg_chordbook_step_finger");
            static Symbol rg_chordbook_step_barre("rg_chordbook_step_barre");
            static Message set_step_text("set_step_text", 0, 0, 0, 0, 0);
            set_step_text[0] = label;
            if (lowstr == highstr) {
                set_step_text[1] = rg_chordbook_step_finger;
            } else {
                set_step_text[1] = rg_chordbook_step_barre;
            }
            set_step_text[2] = fret;
            set_step_text[3] = RGStringToken(lowstr, false);
            set_step_text[4] = RGStringToken(highstr, false);
            mDir->HandleType(set_step_text);
            BandLabel *label2 =
                mDir->Find<BandLabel>(MakeString("step_%02d.lbl", mNumSteps + 1), true);
            label2->SetTextToken(RGFingerStep(i + 1));
            mNumSteps++;
        }
    }

    unsigned oldStepNum = mNumSteps;
    mNumSteps++;
    mStep[oldStepNum].unk0 = 0;
    mStep[oldStepNum].unk4 = 0xFF;
    mStep[oldStepNum].unk5 = 1;
    BandLabel *label =
        mDir->Find<BandLabel>(MakeString("step_%02d_text.lbl", mNumSteps), true);
    static Symbol rg_chordbook_step_strum("rg_chordbook_step_strum");
    label->SetTextToken(rg_chordbook_step_strum);
    label = mDir->Find<BandLabel>(MakeString("step_%02d.lbl", mNumSteps), true);
    static Symbol rg_chordbook_strum("rg_chordbook_strum");
    label->SetTextToken(rg_chordbook_strum);

    GameGem &gem = mGameGemList->GetGem(mChords[idx].gemId);
    for (int i = 0; i < 6; i++) {
        set_chord_fret[0] = i;
        set_chord_fret[1] = gem.GetFret(i);
        mChordLegend->HandleType(set_chord_fret);
        if (gem.GetFret(i) != -1) {
            mInUse |= 1 << i;
        }
    }
    for (int i = 0; i < mNumSteps; i++) {
        mStep[i].unk4 &= mInUse;
    }
    static Message reset_chord_msg("reset_chord");
    mChordLegend->HandleType(reset_chord_msg);
    mDir->Find<RndPropAnim>("num_steps.anim", true)->SetFrame(mNumSteps, 1);
    int string = -1;
    int fret = -1;
    RGGetFretLabelInfo(gem, string, fret, false);
    MILO_ASSERT(string != -1, 0x411);
    MILO_ASSERT(fret != -1, 0x412);
    Transform tf78(unk4c->SlotAt(string));
    unk4c->GetChordMesh(
        mChords[idx].shape, mGemPlayer->GetUser()->GetGameplayOptions()->GetLefty()
    );
    mCurrentChord = idx;
    for (int i = 0; i < 6; i++) {
        SetCorrect(i, mChords[mCurrentChord].fretHand.GetFret(i) == mFret[i]);
    }
    unk6c5 = false;
    unk6c8 = 0;
    for (int i = 0; i < 6; i++) {
        if (gem.GetFret(i) != -1) {
            static Message set_string_used("set_string_used", 0);
            set_string_used[0] = i;
            mChordLegend->HandleType(set_string_used);
        } else {
            static Message set_string_unused("set_string_unused", 0);
            set_string_unused[0] = i;
            mChordLegend->HandleType(set_string_unused);
        }
    }
    PickFretboardView(gem);
    RndDir *progMeter = mDir->Find<RndDir>("progress_meter", true);
    static Message update_progress("update_progress", 0, 0);
    update_progress[0] = mCurrentChord + 1;
    update_progress[1] = (int)mNumChords;
    progMeter->HandleType(update_progress);
}
#pragma pop

DECOMP_FORCEACTIVE(ChordbookPanel, "string_%02d.lbl")

void ChordbookPanel::PickFretboardView(const GameGem &gem) {
    if (!TheGemTrainerPanel->GetFretboardView(gem)) {
        static Message show_high_frets("show_high_frets");
        mChordLegend->HandleType(show_high_frets);
    } else {
        static Message show_low_frets("show_low_frets");
        mChordLegend->HandleType(show_low_frets);
    }
}

DataNode ChordbookPanel::OnDisplayChord(const DataArray *a) {
    int i2 = a->Int(2);
    int last = mNumChords - 1;
    int idx;
    if (i2 <= last) {
        idx = (i2 < 0) ? 0 : i2;
    } else {
        idx = last;
    }
    DisplayChord(idx);
    return idx;
}

DataNode ChordbookPanel::OnGetComplete(const DataArray *a) {
    if (!HasChords())
        return 1;
    else {
        GemPlayer *p = GetChordbookPlayer();
        Symbol song = MetaPerformer::Current()->Song();
        return ProfileCheckComplete(song, p);
    }
}

DataNode ChordbookPanel::OnSkipConfirm(DataArray *a) {
    if (unk674) {
        ResetSkipDialog();
        ExitChordbook();
    }
    return 0;
}

DataNode ChordbookPanel::OnSkipCancel(DataArray *a) {
    if (unk674) {
        ResetSkipDialog();
    }
    return 0;
}

BEGIN_HANDLERS(ChordbookPanel)
    HANDLE(display_chord, OnDisplayChord)
    HANDLE(complete, OnGetComplete)
    HANDLE_ACTION(skip_push_dialog, PushSkipDialog())
    HANDLE(skip_confirm, OnSkipConfirm)
    HANDLE(skip_cancel, OnSkipCancel)
    HANDLE_ACTION(start_chordbook, StartChordbook())
    HANDLE_ACTION(strum, StrumString(_msg->Int(2)))
    HANDLE_EXPR(get_track_dir, unk4c)
    HANDLE_EXPR(has_chords, HasChords())
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x4CF)
END_HANDLERS

BEGIN_PROPSYNCS(ChordbookPanel)
    SYNC_PROP(relearn_chords, mRelearnChords)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS
