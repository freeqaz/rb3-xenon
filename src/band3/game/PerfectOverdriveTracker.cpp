#include "game/PerfectOverdriveTracker.h"
#include "beatmatch/TrackType.h"
#include "beatmatch/VocalNote.h"
#include "game/SongDB.h"
#include "game/TrackerSource.h"
#include "math/Utl.h"
#include "meta_band/Utl.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/Locale.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"

PerfectOverdriveTracker::PerfectOverdriveTracker(
    TrackerSource *src, TrackerBandDisplay &banddisp, TrackerBroadcastDisplay &bcdisp
)
    : Tracker(src, banddisp, bcdisp) {}

PerfectOverdriveTracker::~PerfectOverdriveTracker() {}

void PerfectOverdriveTracker::ConfigureTrackerSpecificData(const DataArray *arr) {
    static Symbol chain_multipliers("chain_multipliers");
    unk8c.InitFromDataArray(arr->FindArray(chain_multipliers, false));
}

void PerfectOverdriveTracker::TranslateRelativeTargets() {
    int numCommonPhrases = TheSongDB->NumCommonPhrases();
    int noteCount;
    DataArray *cfg = SystemConfig("scoring", "band_energy");
    Symbol deployBeatsSym("deploy_beats");
    float deployBeats = cfg->FindArray(deployBeatsSym, true)->Float(1);
    Symbol spotlightPhraseSym("spotlight_phrase");
    float spotlightPhraseFrac = cfg->FindArray(spotlightPhraseSym, true)->Float(1) * deployBeats;
    float songDurationMs = TheSongDB->GetSongDurationMs();
    float songBeats = MsToTick(songDurationMs) / 480.0f;
    float beatsPerMs = songBeats / songDurationMs;
    int maxCount = 0;

    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 76);
        int trackNum = pPlayer->GetTrackNum();
        TrackType trackType = pPlayer->GetTrackType();

        int phraseCount = 0;
        int trackBit = 1 << trackNum;
        for (int i = 0; i < numCommonPhrases; i++) {
            if (trackBit & TheSongDB->GetCommonPhraseTracks(i)) {
                phraseCount++;
            }
        }

        float ratio = spotlightPhraseFrac * (float)phraseCount / beatsPerMs;
        if (songDurationMs < ratio) {
            ratio = songDurationMs;
        }
        ratio /= songDurationMs;
        if (trackType == kTrackVocals) {
            noteCount = (int)TheSongDB->GetVocalNoteList(0)->mPhrases.size();
        } else {
            noteCount = TheSongDB->GetTotalGems(trackNum);
        }

        int trackerCount = (int)((float)noteCount * ratio);
        MaxEq(maxCount, trackerCount);

        int threshold = (int)(deployBeats * (float)trackerCount / songBeats);
        MaxEq(threshold, 1);

        PlayerContribData &entry = unk58[trackType];
        entry.unk0 = -1.0f;
        entry.unk4 = threshold;
        entry.unk8 = trackerCount;
    }

    int totalCount = 0;
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 149);
        TrackType trackType = pPlayer->GetTrackType();
        std::map<TrackType, PlayerContribData>::iterator contribIter = unk58.find(trackType);
        MILO_ASSERT(contribIter != unk58.end(), 154);
        totalCount += maxCount;
        contribIter->second.unk0 = (float)maxCount / (float)contribIter->second.unk8;
    }

    for (unsigned int i = 0; i < mTargets.size(); i++) {
        float scaled = std::floor((float)totalCount * mTargets[i]);
        MaxEq(scaled, 1.0f);
        mTargets[i] = scaled;
    }
}

void PerfectOverdriveTracker::SavePlayerStats() const {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 0xBE);
        TrackType tt = pPlayer->GetTrackType();
        std::map<TrackType, PlayerStreakData>::const_iterator it = unk70.find(tt);
        if (it != unk70.end()) {
            pPlayer->mStats.unk1c0 = it->second.unk1c;
        }
    }
}

void PerfectOverdriveTracker::HandlePlayerSaved_(const TrackerPlayerID &pid) {
    GetPlayerDisplay(pid).Enable();
}

void PerfectOverdriveTracker::FirstFrame_(float) {
    mBandDisplay.Initialize(gNullStr);
    unk70.clear();
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *player = mSource->GetPlayer(id);
        MILO_ASSERT(player, 0xF1);
        PlayerStreakData &data = unk70[player->GetTrackType()];
        data.unk0 = -1.0f;
        data.unk4 = 0;
        data.unk5 = 0;
        data.unk6 = 0;
        data.unk8 = 0;
        data.unkc = 0;
        data.unk10 = 0;
        data.unk14 = -1;
        data.unk18 = -1.0f;
        data.unk1c = 0;
        GetPlayerDisplay(id).Enable();
    }
    unk88 = 0;
}

void PerfectOverdriveTracker::Poll_(float ms) {
    bool anyCanDeploy = false;
    bool anyHadFocus = false;
    bool anyIsDeploying = false;
    bool anyWasDeploying = false;

    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        bool isLocal = mSource->IsPlayerLocal(id);
        TrackType tt = pPlayer->GetTrackType();

        std::map<TrackType, PlayerStreakData>::iterator streakIt = unk70.find(tt);
        if (streakIt == unk70.end()) {
            continue;
        }
        PlayerStreakData &streakData = streakIt->second;

        PlayerContribData &contribData = unk58[tt];

        const TrackerPlayerDisplay &disp = GetPlayerDisplay(id);
        bool prevFocus = streakData.unk4;
        bool canDeploy = pPlayer->CanDeployOverdrive();
        bool wasDeploying = streakData.unk5;
        bool isDeploying = pPlayer->IsDeployingBandEnergy();

        anyCanDeploy |= canDeploy;
        anyHadFocus |= prevFocus;
        anyIsDeploying |= isDeploying;
        anyWasDeploying |= wasDeploying;

        if (!wasDeploying && isDeploying) {
            streakData.unk0 = ms;
            streakData.unk4 = false;
            streakData.unk5 = true;
            streakData.unk6 = false;
            streakData.unk8 = pPlayer->mStats.mHitCount;
            streakData.unkc = pPlayer->mStats.mMissCount;
            streakData.unk10 = pPlayer->mStats.mHitCount + pPlayer->mStats.m0x08;
            streakData.unk14 = -1;
            streakData.unk18 = -1.0f;
            if (isLocal) {
                disp.SetSuccessState(true);
            }
        } else if (wasDeploying && !streakData.unk6) {
            if (isLocal) {
                int hitsSinceStart = pPlayer->mStats.mHitCount - streakData.unk8;
                int endDiff = (pPlayer->mStats.mHitCount + pPlayer->mStats.m0x08) - streakData.unk10;
                float progress = (float)hitsSinceStart / (float)contribData.unk4;
                int multIdx = unk8c.GetMultiplierIndex(progress);
                if (streakData.unk14 != multIdx) {
                    disp.SetSecondaryStateLevel(multIdx);
                    streakData.unk14 = multIdx;
                }
                float pctOfMax = unk8c.GetPercentOfMaxMultiplier(progress);
                if (pctOfMax != streakData.unk18) {
                    SetPlayerProgress(id, pctOfMax);
                    streakData.unk18 = pctOfMax;
                }
                bool failed = false;
                int missChange = pPlayer->mStats.mMissCount - streakData.unkc;
                bool notMissed = !missChange;
                if ((float)hitsSinceStart / (float)endDiff < 1.0f || !notMissed)
                    failed = true;
                bool endStreak = failed || !isDeploying;
                if (failed) {
                    streakData.unk6 = true;
                }
                if (endStreak) {
                    float mult = unk8c.GetMultiplier(progress);
                    float scale = contribData.unk0;
                    streakData.unk1c += hitsSinceStart;
                    float points = mult * ((float)hitsSinceStart * scale);
                    LocalEndStreak(id, points, hitsSinceStart);
                    SendEndStreak(pPlayer, points, hitsSinceStart);
                }
            }
        } else if (!prevFocus && canDeploy) {
            streakData.unk4 = true;
            if (isLocal) {
                disp.GainFocus(false);
            }
        } else if (prevFocus && !canDeploy) {
            streakData.unk4 = false;
            if (isLocal) {
                disp.Hide();
            }
        }

        streakData.unk5 = isDeploying;
    }

    if (!anyIsDeploying && anyCanDeploy && !anyHadFocus) {
        static Symbol perfect_overdrive_tracker_deploy("perfect_overdrive_tracker_deploy");
        mBroadcastDisplay.SetBandMessage(DataArrayPtr(perfect_overdrive_tracker_deploy));
        mBroadcastDisplay.Show();
    } else if (anyIsDeploying && !anyWasDeploying) {
        mBroadcastDisplay.Hide();
    }
}

void PerfectOverdriveTracker::RemoteEndStreak_(Player *p, float f, int i) {
    TrackerPlayerID pid = mSource->FindPlayerID(p);
    if (pid.NotNull()) {
        LocalEndStreak(pid, f, i);
    }
}

void PerfectOverdriveTracker::LocalEndStreak(const TrackerPlayerID &pid, float f, int i) {
    unk88 += f;
    GetPlayerDisplay(pid).LoseFocus(true);
    Player *player = mSource->GetPlayer(pid);
    TrackType tt = player->GetTrackType();
    static Symbol perfect_overdrive_tracker_gem_progress("perfect_overdrive_tracker_gem_progress");
    static Symbol perfect_overdrive_tracker_vocal_progress("perfect_overdrive_tracker_vocal_progress");
    Symbol sym = perfect_overdrive_tracker_gem_progress;
    if (tt == kTrackVocals)
        sym = perfect_overdrive_tracker_vocal_progress;
    const char *fontchar = GetFontCharFromTrackType(tt, 0);
    mBroadcastDisplay.ShowBriefBandMessage(DataArrayPtr(sym, i, fontchar));
}

void PerfectOverdriveTracker::UpdateGoalValueLabel(UILabel &) const {}
void PerfectOverdriveTracker::UpdateCurrentValueLabel(UILabel &) const {}

String PerfectOverdriveTracker::GetPlayerContributionString(Symbol s) const {
    static Symbol perfect_overdrive_tracker_contrib_format_vox_1("perfect_overdrive_tracker_contrib_format_vox_1");
    static Symbol perfect_overdrive_tracker_contrib_format_vox("perfect_overdrive_tracker_contrib_format_vox");
    static Symbol perfect_overdrive_tracker_contribution_format_1("perfect_overdrive_tracker_contribution_format_1");
    static Symbol perfect_overdrive_tracker_contribution_format("perfect_overdrive_tracker_contribution_format");
    TrackerPlayerID pid = mSource->GetIDFromInstrument(s);
    int i4 = 0;
    if (pid.NotNull()) {
        Player *pPlayer = mSource->GetPlayer(pid);
        MILO_ASSERT(pPlayer, 0x1EE);
        i4 = pPlayer->mStats.unk1c0;
    }
    static Symbol vocals("vocals");
    if (s == vocals) {
        Symbol sym = i4 == 1 ? perfect_overdrive_tracker_contrib_format_vox_1
                             : perfect_overdrive_tracker_contrib_format_vox;
        return MakeString(Localize(sym, 0), i4);
    } else {
        Symbol sym = i4 == 1 ? perfect_overdrive_tracker_contribution_format_1
                             : perfect_overdrive_tracker_contribution_format;
        return MakeString(Localize(sym, 0), i4);
    }
}