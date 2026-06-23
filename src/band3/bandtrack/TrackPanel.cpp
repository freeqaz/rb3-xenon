#include "bandtrack/TrackPanel.h"

#ifdef HX_NATIVE
#include <cstdio>
#include <cstdlib>
#include "rndobj/Rnd.h"  // TheRnd->ClearDepthForOverlay() — see TrackPanel::Draw
#endif

#include "bandtrack/Track.h"
#include "bandobj/BandScoreboard.h"
#include "bandobj/BandTrack.h"
#include "bandobj/EndingBonus.h"
#include "bandobj/TrackInstruments.h"
#include "bandobj/TrackPanelDirBase.h"
#include "bandobj/VocalTrackDir.h"
#include "decomp.h"
#include "game/BandUserMgr.h"
#include "game/Game.h"
#include "game/GamePanel.h"
#include "game/Player.h"
#include "game/Scoring.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ModifierMgr.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "os/Timer.h"
#include "rndobj/Anim.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/PropAnim.h"
#include "synth/Sequence.h"
#include "ui/UIPanel.h"
#include "utl/Loader.h"
#include "utl/Messages.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/MakeString.h"
#include <cmath>

// Retail RB3 compiled this TU's MILO_WARN with arguments evaluated for side
// effects (the global sizeof()-form strips arg evaluation). Mirror MILO_FAIL's
// (void)(args) comma form per-TU so survivor args are emitted while the message
// string vanishes.
#ifndef HX_NATIVE
#undef MILO_WARN
#define MILO_WARN(...) ((void)(__VA_ARGS__))
#endif

TrackPanel *TheTrackPanel;

DECOMP_FORCEBLOCK(TrackPanel, (const char *c, DataType d), MakeString(c, d);)

TrackPanel *GetTrackPanel() { return TheTrackPanel; }

TrackPanelDirBase *GetTrackPanelDir() { return TheTrackPanel->mTrackPanelDir; }

TrackPanel::TrackPanel()
    : mConfig(SystemConfig("track_graphics")), mReservedVocalSlot(2),
      mScoreboard(this, 0), unk5c(0), unk5d(0), unk5e(0), unk5f(0), unk60(0), unk61(1),
      unk62(0), mAutoVocals(0), mNextReloadTime(0), mTrackPanelDir(0), unk84(0),
      mTourGoalConfig(kConfigInvalid), mLastCrowdRating(-1.0f) {
    for (int i = 0; i < 5; i++) {
        mTrackSlots.push_back(TrackSlot());
    }
    TheTrackPanel = this;
}

TrackPanel::~TrackPanel() {
    CleanUpReloadChecks();
    TheTrackPanel = nullptr;
}

Track *TrackPanel::GetTrack() {
    MILO_ASSERT(mTracks.size() > 0, 0x6A);
    return mTracks.front();
}

Track *TrackPanel::GetTrack(Player *player, bool fail) {
    for (int i = 0; i < mTracks.size(); i++) {
        if (mTracks[i]->GetPlayer() == player) {
            return mTracks[i];
        }
    }
    if (fail) {
        MILO_FAIL(
            "Bad player %s in TrackPanel::GetTrack\n", player->GetUser()->UserName()
        );
    }
    return nullptr;
}

Track *TrackPanel::GetTrack(BandUser *user, bool fail) {
    for (int i = 0; i < mTracks.size(); i++) {
        if (mTracks[i]->GetUser() == user) {
            return mTracks[i];
        }
    }
    if (fail) {
        MILO_FAIL("Bad user %s in TrackPanel::GetTrack\n", user->UserName());
    }
    return nullptr;
}

DECOMP_FORCEACTIVE(TrackPanel, "Bad DataNode type %d in TrackPanel::GetTrack")

const BandUser *TrackPanel::GetUserFromTrackNum(int index) {
    MILO_ASSERT_RANGE(index, 0, mTracks.size(), 0xA0);
    return mTracks[index]->GetBandUser();
}

void TrackPanel::FinishLoad() {
    UIPanel::FinishLoad();
    if (mDir && !mTrackPanelDir) {
        mTrackPanelDir = dynamic_cast<TrackPanelDirBase *>(mDir);
        mTrackPanelDir->SetTrackPanel(this);
        mScoreboard = mTrackPanelDir->Find<BandScoreboard>("scoreboard", false);
    }
}

void TrackPanel::Enter() {
    Reload();
    UIPanel::Enter();
}

void TrackPanel::Exit() {
    if (GetState() == kUp) {
        CleanUpTracks();
        for (int i = 0; i < mTrackSlots.size(); i++) {
            TrackSlot &curslot = mTrackSlots[i];
            curslot.mTrack = 0;
            curslot.mInstrument = kInstNone;
        }
        UIPanel::Exit();
    }
}

#define kTrackNumSlots 5

#pragma optimization_level 2
void TrackPanel::UpdateReservedVocalSlot() {
    MILO_ASSERT(mTrackSlots.size() == kTrackNumSlots, 0xF3);
    int offset = 0;
    int u3 = -1;
    for (int i = 0; i < mTrackSlots.size(); i++) {
        TrackSlot *slot = (TrackSlot *)((char *)&mTrackSlots[0] + offset);
        if (slot->mInstrument == kInstVocals) {
            u3 = i;
        }
        offset += 8;
    }
    if (u3 != -1)
        mReservedVocalSlot = u3;
    else
        mReservedVocalSlot = 4;
}
#pragma optimization_level reset

void TrackPanel::CreateTracks() {
    if (!unk5d) {
        for (int i = 0; i < mTrackSlots.size(); i++) {
            TrackSlot &curslot = mTrackSlots[i];
            curslot.mTrack = 0;
            curslot.mInstrument = kInstNone;
        }
        MILO_ASSERT(mTrackSlots.size() == kTrackNumSlots, 299);

        int maxslot = TheBandUserMgr->MaxSlot();
        for (int i = 0; i <= maxslot; i++) {
            BandUser *curuser = TheBandUserMgr->GetUserFromSlot(i);
            if (curuser && curuser->IsParticipating() && curuser->IsFullyInGame()) {
                Track *curtrack = NewTrack(curuser);
                curtrack->SetName(MakeString("track%d", i), ObjectDir::Main());
                curtrack->mSlotIdx = i;
                mTracks.push_back(curtrack);
                curuser->mTrack = curtrack;
                TrackSlot &curslot = mTrackSlots[i];
                curslot.mTrack = curtrack;
                curslot.mInstrument = GetTrackInstrument(curuser->GetTrackSym());
            }
        }
        UpdateReservedVocalSlot();
        BandUser *nulluser = TheBandUserMgr->GetNullUser();
        if (nulluser->GetPlayer()) {
            int idx = mReservedVocalSlot;
            Track *nulltrack = NewTrack(nulluser);
            nulltrack->SetName(MakeString("track%d", idx), ObjectDir::Main());
            nulltrack->mSlotIdx = idx;
            mTracks.push_back(nulltrack);
            nulluser->mTrack = nulltrack;
            TrackSlot &curslot = mTrackSlots[idx];
            curslot.mTrack = nulltrack;
            curslot.mInstrument = GetTrackInstrument(nulluser->GetTrackSym());
        }
        unk5d = true;
    }
}

void TrackPanel::Reset() {
    CleanUpTracks();
    CreateTracks();
    AssignAndInitTracks();
    mLastCrowdRating = -1.0f;
    if (mScoreboard)
        mScoreboard->Reset();
    float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f;
    if (!TheGame->mProperties.mHasSongSections) {
        for (int i = 0; i < mTracks.size(); i++) {
            mTracks[i]->Jump(secs);
        }
    }
    Hmx::Object::Handle(on_reset_msg, true);
    mTrackPanelDir->ConfigureTracks(false);
    MetaPerformer::Current();
    mTrackPanelDir->Reset();
    SetMainGoalConfiguration(mTourGoalConfig);
    MainGoalReset();
    TrackerDisplayReset();
    SetSuppressTambourineDisplay(false);
    mNextReloadTime = secs;
    unk84 = 0;
    unk5c = true;
    unk5f = false;
    mAutoVocals = TheModifierMgr->IsModifierActive("mod_auto_vocals");
}

void TrackPanel::CleanUpTracks() {
    unk5c = false;
    if (unk5d) {
        for (int i = 0; i < mTracks.size(); i++) {
            BandUser *user = mTracks[i] ? (BandUser *)mTracks[i]->GetBandUser() : nullptr;
            if (user) {
                user->mTrack = nullptr;
                if (user->GetPlayer()) {
                    user->GetPlayer()->UnHookTrack();
                }
            }
        }
        DeleteAll(mTracks);
        TrimExcess(mTracks);
        if (unk5d) {
            for (int i = 0; i < mTrackSlots.size(); i++) {
                mTrackPanelDir->RemoveTrack(i);
            }
            unk5d = false;
        }
    }
}

void TrackPanel::AssignAndInitTracks() {
    for (int i = 0; i < mTrackSlots.size(); i++) {
        AssignTrack(i);
    }
}

void TrackPanel::AssignTrack(int idx) {
    TrackInstrument inst = mTrackSlots[idx].mInstrument;
    if (inst != kInstNone) {
        mTrackPanelDir->AssignTrack(idx, inst, false);
        if (inst != kInstPending) {
            Track *track = mTrackSlots[idx].mTrack;
            BandTrack *trackDir = mTrackPanelDir->GetBandTrackInSlot(idx);
            MILO_ASSERT(track && trackDir, 0x1C7);
            MILO_ASSERT(inst == trackDir->GetInstrument(), 0x1C8);
            track->SetDir(trackDir->AsRndDir());
            trackDir->Init(track);
            track->PostInit();
        }
    }
}

void TrackPanel::Reload() {
    if (!IsLoaded()) {
        MILO_FAIL(
            "This shouldn't be happening - restart after win goes through same code path as next song.\n"
        );
    }
    if (!IsLoaded()) {
        if (!mLoader)
            Load();
        TheLoadMgr.PollUntilLoaded(mLoader, nullptr);
        CheckIsLoaded();
        if (!mTrackPanelDir)
            FinishLoad();
    }
    CleanUpTracks();
    mTrackPanelDir->ResetPlayers();
    if (mScoreboard)
        mScoreboard->Reset();
}

void TrackPanel::HandleAddUser(BandUser *user) {
    int userslot = user->GetSlot();
    TrackSlot &slot = mTrackSlots[userslot];
    MILO_ASSERT(slot.mTrack == NULL, 0x207);
    MILO_ASSERT(slot.mInstrument == kInstNone, 0x208);
    Track *newtrack = NewTrack(user);
    newtrack->SetName(MakeString("track%d", userslot), ObjectDir::Main());

    bool pushed = false;

    for (int i = 0; i < mTracks.size(); i++) {
        if (mTracks[i]->mSlotIdx > userslot) {
            mTracks.insert(mTracks.begin() + i, 1, newtrack);
            pushed = true;
            break;
        }
    }
    if (!pushed) {
        mTracks.push_back(newtrack);
    }
    unk5d = true;
    newtrack->mSlotIdx = userslot;
    user->mTrack = newtrack;
    slot.mTrack = newtrack;
    slot.mInstrument = GetTrackInstrument(user->GetTrackSym());
    UpdateReservedVocalSlot();
    AssignTrack(userslot);
    mTrackPanelDir->ConfigureTracks(!IsGameOver());
    if (slot.mInstrument == kInstVocals && mTrackPanelDir->TracksExtended()) {
        BandTrack *vocalTrack = newtrack->GetBandTrack();
        MILO_ASSERT(vocalTrack, 0x236);
        RndDir *vocalTrackDir = vocalTrack->AsRndDir();
        MILO_ASSERT(vocalTrackDir, 0x239);
        EventTrigger *introTrig =
            vocalTrackDir->Find<EventTrigger>("play_intro.trig", false);
        if (introTrig)
            introTrig->Trigger();
    }
}

void TrackPanel::HandleAddPlayer(Player *player) {
    BandUser *user = player->GetUser();
    MILO_ASSERT(user, 0x248);
    DoHandleAddPlayer(user);
}

void TrackPanel::DoHandleAddPlayer(BandUser *user) {
    int slot = user->GetSlot();
    TrackInstrument inst = GetTrackInstrument(user->GetTrackSym());
    mTrackSlots[slot].mInstrument = inst;
    mTrackPanelDir->GetBandTrackInSlot(slot)->SetInstrument(inst);
#ifdef HX_NATIVE
    if (getenv("GEM_DBG")) {
        fprintf(stderr,
                "[GEM_DBG] TrackPanel::DoHandleAddPlayer slot=%d tracksym='%s' "
                "-> TrackInstrument=%d\n",
                slot, user->GetTrackSym().Str(), (int)inst);
    }
#endif
}

void TrackPanel::PostHandleAddPlayer(Player *player) {
    DoPostHandleAddPlayer(player->GetUser());
}

void TrackPanel::DoPostHandleAddPlayer(BandUser *user) {
    int slot = user->GetSlot();
    TrackInstrument inst = mTrackSlots[slot].mInstrument;
    mTrackPanelDir->AssignTrack(slot, inst, false);
    Track *track = mTrackSlots[slot].mTrack;
    BandTrack *bandTrack = mTrackPanelDir->GetBandTrackInSlot(slot);
    MILO_ASSERT(track && bandTrack, 0x26F);
    MILO_ASSERT(inst == bandTrack->GetInstrument(), 0x270);
    track->SetDir(bandTrack->AsRndDir());
    bandTrack->Init(track);
    track->PostInit();
    mTrackPanelDir->ReapplyConfiguration(true);
    if (inst != kInstVocals && mTrackPanelDir->TracksExtended()) {
        RndAnimatable *anim =
            bandTrack->AsRndDir()->Find<RndAnimatable>("intro_anim.grp", true);
        anim->SetFrame(0, 1);
        EventTrigger *trig =
            bandTrack->AsRndDir()->Find<EventTrigger>("track_in_delayed.trig", false);
        if (trig)
            trig->Trigger();
    }
    mTrackSlots[slot].mTrack->PlayerInit();
}

void TrackPanel::HandleRemoveUser(BandUser *user) {
    Track *track = GetTrack(user, false);
    if (track) {
        int idx = -1;
        for (int i = 0; i < mTrackSlots.size(); i++) {
            TrackSlot &curslot = mTrackSlots[i];
            if (curslot.mTrack == track) {
                curslot.mTrack = nullptr;
                curslot.mInstrument = kInstNone;
                idx = i;
                break;
            }
        }
        if (idx == -1)
            MILO_FAIL("Couldn't find slot for removed user!");
        mTrackPanelDir->RemoveTrack(idx);
        mTrackPanelDir->ConfigureTracks(!IsGameOver());
    }
}

void TrackPanel::PostHandleRemoveUser(BandUser *user) {
    if (std::find(mTracks.begin(), mTracks.end(), user->GetTrack()) != mTracks.end()) {
        Track *track = user->GetTrack();
        mTracks.erase(std::remove(mTracks.begin(), mTracks.end(), track));
        delete track;
    }
}

void TrackPanel::PlaySequence(const char *name, float f1, float f2, float f3) {
    if (mTrackPanelDir) {
        Sequence *seq = mTrackPanelDir->Find<Sequence>(name, false);
        if (seq) {
            seq->Play(f1, f2, f3);
        } else
            MILO_WARN(
                "TrackPanel::PlaySequence() - Sequence '%s' not found in %s",
                name,
                mTrackPanelDir->GetPathName()
            );
    }
}

void TrackPanel::StopSequence(const char *name, bool b) {
    if (mTrackPanelDir) {
        Sequence *seq = mTrackPanelDir->Find<Sequence>(name, false);
        if (seq) {
            seq->Stop(b);
        } else
            MILO_WARN(
                "TrackPanel::StopSequence() - Sequence '%s' not found in %s",
                name,
                mTrackPanelDir->GetPathName()
            );
    }
}

void TrackPanel::SetMainGoalConfiguration(TourGoalConfig cfg) {
    mTourGoalConfig = cfg;
    if (cfg != kConfigInvalid && mDir) {
        RndDir *rdir = mDir->Find<RndDir>("scoreboard", false);
        if (rdir) {
            RndPropAnim *anim = rdir->Find<RndPropAnim>("configuration.anim", true);
            if (anim) {
                float f7;
                switch (mTourGoalConfig) {
                case kConfigScoreStars:
                    f7 = 0.0f;
                    break;
                case kConfigScoreStarsGoal:
                    f7 = 1.0f;
                    break;
                case kConfigScoreGoal:
                    f7 = 2.0f;
                    break;
                case kConfigStarsGoal:
                    f7 = 3.0f;
                    break;
                case kConfigGoal:
                    f7 = 4.0f;
                    break;
                default:
                    f7 = 0.0f;
                    break;
                }
                float fmodded = std::fmod(f7 + 1.0f, 5.0f);
                anim->SetFrame(fmodded, 1);
                anim->SetFrame(f7, 1);
            }
        }
    }
}

void TrackPanel::MainGoalReset() {
    if (mTrackPanelDir) {
        RndDir *rdir = mTrackPanelDir->Find<RndDir>("tour_goal_notify", false);
        if (rdir) {
            rdir->SetShowing(false);
            rdir->Find<EventTrigger>("hide.trig", true)->Trigger();
            unk60 = false;
        }
    }
}

void TrackPanel::ShowMainGoalInfo(bool show) {
    if (mScoreboard) {
        RndDir *rdir = mScoreboard->Find<RndDir>("tour_goal_main", false);
        if (rdir) {
            EventTrigger *trig;
            if (show) {
                trig = rdir->Find<EventTrigger>("show.trig", true);
            } else {
                trig = rdir->Find<EventTrigger>("hide.trig", true);
            }
            trig->Trigger();
        }
    }
}

DECOMP_FORCEACTIVE(
    TrackPanel,
    "top",
    "bottom",
    "tg_main_text_top.lbl",
    "tg_main_text_bottom.lbl",
    "desc",
    "tracker_broadcast_display",
    "tg_desc.lbl",
    "tg_desc_display.trig",
    "tg_main_succeed.trig",
    "tg_main_fail.trig"
)

void TrackPanel::SendTrackerDisplayMessage(const Message &msg) const {
    if (mScoreboard) {
        RndDir *rdir = mScoreboard->Find<RndDir>("tracker_band_display", false);
        if (!rdir)
            MILO_WARN("Couldn't find tracker_band_display object in TrackPanel!");
        else
            rdir->HandleType(msg);
    }
}

void TrackPanel::SendTrackerBroadcastDisplayMessage(const Message &msg) const {
    if (!unk62) {
        RndDir *rdir = mTrackPanelDir->Find<RndDir>("tracker_broadcast_display", false);
        if (rdir)
            rdir->HandleType(msg);
    }
}

void TrackPanel::TrackerDisplayReset() const {
    SendTrackerDisplayMessage(hide_msg);
    SendTrackerBroadcastDisplayMessage(hide_msg);
}

void TrackPanel::Unload() {
    CleanUpReloadChecks();
    for (int i = 0; i < mTracks.size(); i++) {
        mTracks[i]->Unload();
    }
    mDir->SetProperty("gem_tracks", 0);
    CleanUpTracks();
    DeleteAll(mTracks);
    if (mTrackPanelDir)
        mTrackPanelDir->CleanUpChordMeshes();
    mTrackPanelDir = nullptr;
    UIPanel::Unload();
}

void TrackPanel::StartPulseAnims() {
    if (!unk5f) {
        float delay = mTrackPanelDir->GetPulseAnimStartDelay(true);
        mTrackPanelDir->StartPulseAnims(delay);
        for (int i = 0; i < mTracks.size(); i++) {
            mTracks[i]->StartPulseAnims(delay);
        }
        unk5f = true;
    }
}

void TrackPanel::Poll() {
    START_AUTO_TIMER("hud_track_poll");
#ifdef HX_NATIVE
    // K8 diagnostic — TrackPanel::Poll entry + early-return gates.
    static bool sK8 = !!getenv("K8_DBG");
    static int sEntries = 0;
    static int sLastGate = -1;
    int gate = 0;
    if (!TheSongDB || TheSongDB->GetNumTrackData() == 0) gate = 1;
    else if (!unk5c) gate = 2;
    if (sK8 && (gate != sLastGate || (sEntries++ % 120) == 0)) {
        MILO_LOG("K8_DBG: TrackPanel::Poll gate=%d (1=noSongDB,2=!unk5c,0=ok) "
                 "mTracks.size=%u mScoreboard=%p\n",
                 gate, (unsigned)mTracks.size(), (void*)mScoreboard.Ptr());
        sLastGate = gate;
    }
#endif
    if (!TheSongDB || TheSongDB->GetNumTrackData() == 0)
        return;
    if (!unk5c)
        return;
    float ms = 1000.0f * TheTaskMgr.Seconds(TaskMgr::kRealTime);
    int tick = MsToTick(ms);
    auto _tmp0 = TheGame->mMaster->GetAudio()->Fail();
    if (_tmp0)
        return;
#ifdef HX_NATIVE
    if (sK8) {
        static int sBody = 0;
        if ((sBody++ % 120) == 0) {
            MILO_LOG("K8_DBG: TrackPanel::Poll BODY ms=%.1f mTracks=%u\n",
                     ms, (unsigned)mTracks.size());
        }
    }
#endif
    StartPulseAnims();
    bool wasSoloing = unk62;
    bool soloing = false;
    std::vector<Track *> &_ref0 = mTracks;
    for (unsigned int i = 0; i < _ref0.size(); i++) {
        _ref0[i]->Poll(ms);
        BandTrack *bandTrack = _ref0[i]->GetBandTrack();
        if (bandTrack && bandTrack->unk78) {
            soloing = true;
        }
    }
    if (!wasSoloing && soloing) {
        SendTrackerBroadcastDisplayMessage(hide_msg);
    }
    unk62 = soloing;
    if (mScoreboard) {
        Performer *mainPerformer = TheGame->GetMainPerformer();
        if (unk61) {
            MetaPerformer::Current();
            auto _tmp1 = mainPerformer->GetAccumulatedScore();
            mScoreboard->SetScore(_tmp1);
        } else if (TheGame->mProperties.mShowStars) {
            mScoreboard->SetNumStars(
                mainPerformer->GetNumStarsFloat(), TheGame->mProperties.mPlayStarSfx
            );
        }
        unk61 = !unk61;
    }
    Performer *mainPerformer = TheGame->GetMainPerformer();
    Band *band = mainPerformer->GetBand();
    if (band) {
        TrackPanelDirBase *dir = mTrackPanelDir;
        dir->SetMultiplier(band->EnergyMultiplier(), false);
    }
    if (TheSongDB->IsInCoda(tick)) {
        TrackPanelDirBase *dir = mTrackPanelDir;
        dir->SetCodaScore(mainPerformer->CodaScore());
    }
    float crowdRating = mainPerformer->Crowd()->GetDisplayValue();
    if (crowdRating != mLastCrowdRating) {
        mTrackPanelDir->SetCrowdRating(crowdRating);
        mLastCrowdRating = crowdRating;
    }
    UIPanel::Poll();
    unk84++;
}

int Performer::GetAccumulatedScore() const { return GetScore(); }

int Performer::CodaScore() const { return 0; }

void TrackPanel::Draw() {
    START_AUTO_TIMER("hud_track_draw");
    if (unk5c) {
#ifdef HX_NATIVE
        // Note highway depth-composite fix. The venue WorldDir (band characters
        // + stage) has already drawn into the shared depth buffer with the venue
        // camera; the track below draws the highway/gems with game.cam, a
        // separate camera with its own near/far. Without resetting depth here,
        // venue geometry near the venue camera gets a smaller NDC-z than the
        // highway and occludes it (chairs clipping the lane, a band member's arm
        // over the top of the track). Clear depth (color preserved) so the
        // highway always composites on top, exactly as in the retail game.
        // BandRnd::ClearDepthForOverlay implements this; the base Rnd no-op makes
        // it a harmless call on backends that don't (Wii match build skips this
        // whole block). Opt-out: RB3_NO_TRACK_DEPTH_CLEAR=1.
        if (TheRnd)
            TheRnd->ClearDepthForOverlay();
#endif
        UIPanel::Draw();
    }
}

DECOMP_FORCEACTIVE(
    TrackPanel,
    "track/tracksystem.milo",
    "track/vocals.milo",
    "config/track_graphics.dta",
    "track/rail_streak.milo"
)

void TrackPanel::SetSmasherGlowing(int iii, bool b) {
    for (int i = 0; i < mTracks.size(); i++) {
        mTracks[i]->SetSmasherGlowing(iii, b);
    }
}

void TrackPanel::PopSmasher(int iii) {
    for (int i = 0; i < mTracks.size(); i++) {
        mTracks[i]->PopSmasher(iii);
    }
}

void TrackPanel::CleanUpReloadChecks() {
    for (std::map<Symbol, DepChecker *>::iterator it = mReloadChecks.begin();
         it != mReloadChecks.end();
         ++it) {
        delete it->second;
    }
    std::map<Symbol, DepChecker *> tmp;
    mReloadChecks.swap(tmp);
}

DataNode TrackPanel::ForEachTrack(const DataArray *arr) {
    DataNode *var = arr->Var(2);
    DataNode node = *var;
    for (int i = 0; i < mTracks.size(); i++) {
        *var = mTracks[i];
        for (int j = 3; j < arr->Size(); j++) {
            arr->Command(j)->Execute();
        }
    }
    *var = node;
    return 0;
}

void TrackPanel::GetTrackOrder(std::vector<TrackInstrument> *out, bool) const {
    MILO_ASSERT(out && out->empty(), 0x4E9);
    for (int i = 0; i < mTrackSlots.size(); i++) {
        out->push_back(mTrackSlots[i].mInstrument);
    }
}

int TrackPanel::GetTrackCount() const { return mTrackSlots.size(); }

int TrackPanel::GetNumPlayers() const { return TheBandUserMgr->GetNumParticipants(); }

bool TrackPanel::InGame() const { return TheGame; }

int TrackPanel::GetTrackSlot(Player *player) {
    for (int i = 0; i < mTrackSlots.size(); i++) {
        if (mTrackSlots[i].mTrack && mTrackSlots[i].mTrack->GetPlayer() == player)
            return i;
    }
    return -1;
}

void TrackPanel::UpdateJoinInProgress(bool b1, bool b2) {
    mTrackPanelDir->UpdateJoinInProgress(b1, b2);
}

void TrackPanel::FailedJoinInProgress() { mTrackPanelDir->FailedJoinInProgress(); }

void TrackPanel::SetSuppressUnisonDisplay(bool b) {
    EndingBonus *bonus = mTrackPanelDir->GetEndingBonus();
    if (bonus)
        bonus->SetSuppressUnisonDisplay(b);
}

void TrackPanel::UnisonStart(int iii) {
    EndingBonus *bonus = mTrackPanelDir->GetEndingBonus();
    if (bonus) {
        int u3 = 0;
        for (int i = 0; i < mTracks.size(); i++) {
            int tracknum = mTracks[i]->GetTrackNum();
            if (iii & (1 << tracknum)) {
                u3 |= (1 << mTracks[i]->mSlotIdx);
            }
        }
        bonus->UnisonStart(u3);
    }
}

void TrackPanel::UnisonPlayerSuccess(Player *player) {
    if (player) {
        EndingBonus *bonus = mTrackPanelDir->GetEndingBonus();
        int slot = GetTrackSlot(player);
        if (bonus && slot != -1) {
            bonus->PlayerSuccess(slot);
        }
    }
}

void TrackPanel::UnisonPlayerFailure(Player *player) {
    if (player) {
        EndingBonus *bonus = mTrackPanelDir->GetEndingBonus();
        int slot = GetTrackSlot(player);
        if (bonus && slot != -1) {
            bonus->PlayerFailure(slot);
        }
    }
}

void TrackPanel::SetSuppressTambourineDisplay(bool b) {
    for (int i = 0; i < mTrackSlots.size(); i++) {
        if (mTrackSlots[i].mInstrument == kInstVocals) {
            Track *track = mTrackSlots[i].mTrack;
            if (track) {
                BandTrack *btrack = track->GetBandTrack();
                if (btrack) {
                    btrack->SetSuppressSoloDisplay(b);
                }
            }
        }
    }
}

void TrackPanel::SetSuppressPlayerFeedback(bool b) {
    for (int i = 0; i < mTrackSlots.size(); i++) {
        Track *track = mTrackSlots[i].mTrack;
        if (track) {
            BandTrack *btrack = track->GetBandTrack();
            if (btrack) {
                btrack->SetPlayerFeedbackShowing(!b);
            }
        }
    }
}

int TrackPanel::GetNoCrowdMeter() const {
    MetaPerformer::Current();
    return 0;
}

float TrackPanel::CrowdRatingDefaultVal(Symbol s) const {
    Difficulty diff = kDifficultyEasy;
    if (s == medium) {
        diff = kDifficultyMedium;
    } else if (s == hard) {
        diff = kDifficultyHard;
    } else if (s == expert) {
        diff = kDifficultyExpert;
    }
    return TheScoring->GetCrowdConfig(diff, NULL)->FindFloat("initial_display_level");
}

bool TrackPanel::SlotReservedForVocals(int slot) const {
    return slot == mReservedVocalSlot;
}

bool TrackPanel::GameResumedNoScore() const { return TheGame->ResumedNoScore(); }

bool TrackPanel::IsGameOver() const { return TheGamePanel->IsGameOver(); }

int TrackPanel::GetGameExcitement() const { return TheGame->GetCrowdExcitement(); }

bool TrackPanel::ShouldUpdateScrollSpeed() const { return !TheGame->InDrumTrainer(); }

void TrackPanel::PushCrowdReaction(bool react) {
    BeatMaster *master = TheGame->mMaster;
    if (master) {
        MasterAudio *audio = master->GetAudio();
        if (audio && TheGame->mProperties.mCrowdReacts) {
            audio->SetCrowdFader(react ? 0.0f : -96.0f);
        }
    }
}

BEGIN_HANDLERS(TrackPanel)
    HANDLE_ACTION(set_smasher_glowing, SetSmasherGlowing(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(pop_smasher, PopSmasher(_msg->Int(2)))
    HANDLE_ACTION(track_reset, Reset())
    HANDLE_ACTION(reload, Reload())
    HANDLE_EXPR(get_first_track, GetTrack())
    HANDLE(foreach_track, ForEachTrack)
    HANDLE_EXPR(my_track_panel_dir, mTrackPanelDir)
    if (sym == get_pause_menu) {
        _msg->Int(2);
        return NULL_OBJ;
    }
    HANDLE_EXPR(get_user_from_track_num, (BandUser *)GetUserFromTrackNum(_msg->Int(2)))
    HANDLE_ACTION(play_seq, PlaySequence(_msg->Str(2), 0, 0, 0))
    HANDLE_ACTION(stop_seq, StopSequence(_msg->Str(2), false))
    HANDLE_ACTION(
        update_join_in_progress, UpdateJoinInProgress(_msg->Int(2), _msg->Int(3))
    )
    HANDLE_ACTION(failed_join_in_progress, FailedJoinInProgress())
    HANDLE_EXPR(get_num_tracks, (int)mTracks.size())
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x5C5)
END_HANDLERS
