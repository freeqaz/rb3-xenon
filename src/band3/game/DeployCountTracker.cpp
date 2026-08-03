#include "game/DeployCountTracker.h"
#include "game/Game.h"
#include "game/Player.h"
#include "game/SongDB.h"
#include "game/TrackerDisplay.h"
#include "game/TrackerSource.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "os/Debug.h"
#include "utl/Locale.h"
#include "utl/Symbol.h"

DeployCountTracker::DeployCountTracker(
    TrackerSource *src, TrackerBandDisplay &banddisp, TrackerBroadcastDisplay &bcdisp
)
    : Tracker(src, banddisp, bcdisp), mDeployCount(0), mRequireFullEnergy(0),
      mRequireMaxMultiplier(0) {}

DeployCountTracker::~DeployCountTracker() {}

void DeployCountTracker::ConfigureTrackerSpecificData(const DataArray *arr) {
    static Symbol require_full_energy("require_full_energy");
    arr->FindData(require_full_energy, mRequireFullEnergy, false);
    static Symbol require_max_multiplier("require_max_multiplier");
    arr->FindData(require_max_multiplier, mRequireMaxMultiplier, false);
}

void DeployCountTracker::TranslateRelativeTargets() {
    int bitcount = 0;
    int phrasecount = TheSongDB->NumCommonPhrases();
    for (int i = 0; i < phrasecount; i++) {
        bitcount += CountBits(TheSongDB->GetCommonPhraseTracks(i));
    }
    float f2 = 2.0f;
    if (mRequireFullEnergy) {
        f2 = 4.0f;
    }
    float f31 = (float)bitcount / f2;

    for (int i = 0; i < mTargets.size(); i++) {
        mTargets[i] = std::max((float)std::floor(f31 * mTargets[i]), 1.0f);
    }
}

void DeployCountTracker::UpdateGoalValueLabel(UILabel &) const {}
void DeployCountTracker::UpdateCurrentValueLabel(UILabel &) const {}

String DeployCountTracker::GetPlayerContributionString(Symbol s) const {
    TrackerPlayerID pid = mSource->GetIDFromInstrument(s);
    float f1 = 0;
    if (pid.NotNull()) {
        Player *pPlayer = mSource->GetPlayer(pid);
        MILO_ASSERT(pPlayer, 0x62);
        Stats &stats = pPlayer->mStats;
        f1 = stats.unk1c0;
    }
    static Symbol deploy_stat_tracker_contribution("deploy_stat_tracker_contribution");
    static Symbol deploy_stat_tracker_contribution_1("deploy_stat_tracker_contribution_1");
    Symbol sym = (int)f1 == 1 ? deploy_stat_tracker_contribution_1
                              : deploy_stat_tracker_contribution;
    return MakeString(Localize(sym, 0), f1);
}

void DeployCountTracker::SavePlayerStats() const {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 0x77);
        Stats &stats = pPlayer->mStats;
        std::map<TrackerPlayerID, PlayerDeployData>::const_iterator cData =
            mDeployDataMap.find(id);
        MILO_ASSERT(cData != mDeployDataMap.end(), 0x7C);
        stats.unk1c0 = cData->second.unk4;
    }
}

void DeployCountTracker::FirstFrame_(float) {
    mDeployCount = 0;
    PlayerDeployData data;
    data.unk0 = false;
    data.unk1 = false;
    data.unk2 = false;
    data.unk3 = false;
    data.unk4 = 0;
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        mDeployDataMap[id] = data;
    }
    mBandDisplay.Initialize(gNullStr);
}

void DeployCountTracker::Poll_(float) {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        if (mSource->IsPlayerLocal(id)) {
            Player *pPlayer = mSource->GetPlayer(id);
            MILO_ASSERT(pPlayer, 0xA5);
            PlayerDeployData &data = mDeployDataMap[id];
            bool energy = pPlayer->GetBandEnergy() >= 1.0f;
            bool ismaxmult = pPlayer->GetIndividualMultiplier()
                == pPlayer->GetMaxIndividualMultipler();
            bool b8 = pPlayer->CanDeployOverdrive();
            int u12 = !mRequireFullEnergy || energy;
            bool b4 = b8 & u12;
            int b5 = !mRequireMaxMultiplier || ismaxmult;
            bool c1 = data.unk2;
            b4 = b4 & b5;
            bool deploying = pPlayer->IsDeployingBandEnergy();
            bool wasDeploying = data.unk3;
            if (deploying && !wasDeploying) {
                bool b2 = !mRequireFullEnergy || data.unk0;
                bool b3 = !mRequireMaxMultiplier || data.unk1;
                if (b2 && b3) {
                    LocalDeploy(id);
                    static Message send_tracker_deploy_msg("send_tracker_deploy");
                    pPlayer->HandleType(send_tracker_deploy_msg);
                }
            } else if (!deploying) {
                data.unk0 = energy;
                data.unk1 = ismaxmult;
                if (b4 && !c1) {
                    GetPlayerDisplay(id).GainFocus(false);
                }
            }
            if (!b4 && c1) {
                GetPlayerDisplay(id).LoseFocus(deploying);
            }
            data.unk2 = b4;
            data.unk3 = deploying;
        }
    }
}

void DeployCountTracker::RemoteDeploy(Player *p) {
    TrackerPlayerID pid = mSource->FindPlayerID(p);
    if (pid.NotNull()) {
        LocalDeploy(pid);
    }
}

void DeployCountTracker::LocalDeploy(const TrackerPlayerID &pid) {
    PlayerDeployData &data = mDeployDataMap[pid];
    data.unk4++;
    mDeployCount++;
    static Symbol deploy_count_tracker_progress("deploy_count_tracker_progress");
    static Symbol deploy_count_tracker_progress_1("deploy_count_tracker_progress_1");
    Symbol sym = mDeployCount == 1 ? deploy_count_tracker_progress_1
                                   : deploy_count_tracker_progress;
    mBroadcastDisplay.ShowBriefBandMessage(DataArrayPtr(sym, mDeployCount));
}

DataArrayPtr DeployCountTracker::GetTargetDescription(int idx) const {
    return TrackerDisplay::MakeIntegerTargetDescription(mTargets[idx]);
}
