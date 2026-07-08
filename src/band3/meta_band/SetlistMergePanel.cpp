#include "meta_band/SetlistMergePanel.h"
#include "decomp.h"
#include "math/Rand.h"
#include "game/BandUserMgr.h"
#include "game/NetGameMsgs.h"
#include "meta_band/BandMachine.h"
#include "meta_band/LockMessages.h"
#include "meta_band/LockStepMgr.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/SavedSetlist.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/Utl.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "utl/Messages2.h"
#include "utl/Symbol.h"
#include "utl/Symbols2.h"

SetlistMergePanel::SetlistMergePanel() : mSetlistMergeLock(0) {
    mSetlistMergeLock = new LockStepMgr("setlist_merge_lock", this);
}

SetlistMergePanel::~SetlistMergePanel() { delete mSetlistMergeLock; }

void SetlistMergePanel::Enter() {
    UIPanel::Enter();
    TheSessionMgr->GetMachineMgr()->AddSink(this);
}

void SetlistMergePanel::Exit() {
    TheSessionMgr->GetMachineMgr()->RemoveSink(this);
    UIPanel::Exit();
}

DataNode SetlistMergePanel::OnMsg(const UITransitionCompleteMsg &) {
    SetLocalsMerging(true);
    UpdateProgressInfo();
    return DataNode(kDataUnhandled, 0);
}

void SetlistMergePanel::StartSetlistMerge() {
    MILO_ASSERT(IsLeaderLocal(), 0x40);
    mSetlists.clear();
    BasicStartLockMsg msg;
    mSetlistMergeLock->StartLock(msg);
}

void SetlistMergePanel::SubmitSetlist() {
    MILO_ASSERT(GetState() == kUp, 0x49);
    MILO_ASSERT(mSetlistMergeLock->InLock(), 0x4A);
    std::vector<int> setlist = TheMusicLibrary->GetSetlist();
    if (IsLeaderLocal()) {
        HandleSetlistSubmission(setlist, TheBandUserMgr->GetNumLocalPlayers());
    } else {
        if (setlist.size() > 75)
            setlist.resize(75);
        SetlistSubmissionMsg msg(setlist, TheBandUserMgr->GetNumLocalPlayers());
        TheSessionMgr->SendMsg(TheSessionMgr->GetLeaderUser(), msg, kReliable);
    }
}

void SetlistMergePanel::HandleSetlistSubmission(
    const std::vector<int> &setlist, int numplayers
) {
    MILO_ASSERT(IsLeaderLocal(), 0x61);
    MILO_ASSERT(mSetlistMergeLock->InLock(), 0x62);
    mSetlists.push_back(std::pair<std::vector<int>, int>(setlist, numplayers));
}

UNPOOL_DATA
void SetlistMergePanel::UpdateProgressInfo() {
    static Message setMsg("set_machine_info", 0, gNullStr, 0, 0);
    static Message hideMsg("hide_machine_info", 0);

    BandMachineMgr *mgr = TheSessionMgr->GetMachineMgr();
    std::vector<BandMachine *> machines;
    mgr->GetMachines(machines);
    bool b2 = true;
    int i4 = 0;
    FOREACH (it, machines) {
        int param = (*it)->GetNetUIStateParam();
        bool gt1 = param > 0;
        if (gt1 == 0) {
            b2 = false;
        }
        setMsg[0] = i4;
        setMsg[1] = "";
        setMsg[2] = gt1;
        setMsg[3] = param < 0 ? -param : param;
        HandleType(setMsg);
        i4++;
    }

    for (; i4 < 4; i4++) {
        hideMsg[0] = i4;
        HandleType(hideMsg);
    }

    if (IsLeaderLocal() && b2) {
        StartSetlistMerge();
    }
}
END_UNPOOL_DATA

DECOMP_FORCEACTIVE(SetlistMergePanel, "setlist_merge_screen")

void SetlistMergePanel::SetLocalsMerging(bool b1) {
    int size = TheMusicLibrary->SetlistSize();
    if (!b1)
        size = -size;
    TheSessionMgr->GetMachineMgr()->GetLocalMachine()->SetNetUIStateParam(size);
}

int SetlistMergePanel::IntToSetlistIndex(int i, int setlistSize) {
    MILO_ASSERT_RANGE(i, 0, 100, 0xA9);
    MILO_ASSERT_RANGE_EQ(setlistSize, 1, 100, 0xAA);

    int index = -1;
    float ratio = 100.0f / setlistSize;
    for (int i3 = 0; i3 < setlistSize; i3++) {
        if (i == (int)std::floor(ratio * i3)) {
            index = i3;
            break;
        }
    }

    MILO_ASSERT(index < setlistSize, 0xB8);
    MILO_ASSERT(i != 0 || index == 0, 0xB9);
    return index;
}

DataNode SetlistMergePanel::OnMsg(const ReleasingLockStepMsg &msg) {
    MILO_ASSERT(IsLeaderLocal(), 0xBF);
    if (!msg->Int(2)) {
        return 1;
    }
    if (mSetlists.size() == 1) {
        SendSongsToMetaPerformer(mSetlists[0].first);
        return 1;
    }
    int totalUsers = 0;
    int numSetlists = mSetlists.size();
    for (int i = 0; i < numSetlists; i++) {
        totalUsers += mSetlists[i].second;
    }
    for (int i = 0; i < numSetlists; i++) {
        std::vector<int> &songs = mSetlists[i].first;
        int targetSize = mSetlists[i].second * 100 / totalUsers;
        if (songs.size() > (unsigned int)targetSize) {
            songs.resize(targetSize);
        }
    }
    std::vector<int> mergedSetlist;
    if (TheGameMode->InMode("tour")) {
        MILO_ASSERT(!mSetlists.empty(), 0xDC);
        int targetSize = mSetlists[0].first.size();
        for (int i = 0; i < numSetlists; i++) {
            MILO_ASSERT((unsigned int)targetSize == mSetlists[i].first.size(), 0xE4);
        }
        for (int i = 0; i < targetSize; i++) {
            int pick = RandomInt(0, numSetlists);
            mergedSetlist.push_back(mSetlists[pick].first[i]);
        }
    } else {
        for (int i = 0; i < 100; i++) {
            for (int j = 0; j < numSetlists; j++) {
                int idx = IntToSetlistIndex(i, mSetlists[j].first.size());
                if (idx != -1) {
                    mergedSetlist.push_back(mSetlists[j].first[idx]);
                }
            }
        }
        int targetSize = 0;
        for (int i = 0; i < numSetlists; i++) {
            targetSize += mSetlists[i].first.size();
        }
        MILO_ASSERT((unsigned int)targetSize == mergedSetlist.size(), 0x107);
    }
    SendSongsToMetaPerformer(mergedSetlist);
    return 1;
}

void SetlistMergePanel::SendSongsToMetaPerformer(const std::vector<int> &songs) {
    int size = songs.size();
    SavedSetlist *current = TheMusicLibrary->mCurrentSetlist;
    bool sameSongs = false;
    if (current && current->mSongs.size() == size) {
        sameSongs = true;
        for (int i = 0; i < size; i++) {
            if (songs[i] != current->mSongs[i]) {
                sameSongs = false;
                break;
            }
        }
    }
    if (sameSongs) {
        if (current->IsBattle()) {
            BattleSavedSetlist *bss = dynamic_cast<BattleSavedSetlist *>(current);
            MILO_ASSERT(bss, 0x12F);
            SavedSetlist::SetlistType type = bss->mSetlistType;
            bool archived = (type == SavedSetlist::kBattleHarmonixArchived || type == SavedSetlist::kBattleFriendArchived);
            if (!archived) {
                MetaPerformer::Current()->SetBattle(bss);
                return;
            }
        }
        MetaPerformer::Current()->SetSetlist(current);
    } else {
        MetaPerformer::Current()->SetSongs(songs);
    }
}

DataNode SetlistMergePanel::OnMsg(const LockStepStartMsg &) {
    UIScreen *screen = ObjectDir::Main()->Find<UIScreen>("setlist_merge_screen", true);
    if (!TheUI->InTransition() && TheUI->CurrentScreen() == screen) {
        SubmitSetlist();
        mSetlistMergeLock->RespondToLock(true);
    } else
        mSetlistMergeLock->RespondToLock(false);
    return 1;
}

DataNode SetlistMergePanel::OnMsg(const LockStepCompleteMsg &msg) {
    if (msg->Int(2)) {
        HandleType(move_on_msg);
    }
    return 1;
}

DataNode SetlistMergePanel::OnMsg(const RemoteMachineUpdatedMsg &msg) {
    if (msg.GetMask() & 1) {
        UpdateProgressInfo();
    }
    return 1;
}

DataNode SetlistMergePanel::OnMsg(const RemoteMachineLeftMsg &) {
    UpdateProgressInfo();
    return 1;
}

DataNode SetlistMergePanel::OnMsg(const NewRemoteMachineMsg &) {
    UpdateProgressInfo();
    return 1;
}

BEGIN_HANDLERS(SetlistMergePanel)
    HANDLE_ACTION(cancel_merge, SetLocalsMerging(false))
    HANDLE_MESSAGE(UITransitionCompleteMsg)
    HANDLE_MESSAGE(LockStepStartMsg)
    HANDLE_MESSAGE(ReleasingLockStepMsg)
    HANDLE_MESSAGE(LockStepCompleteMsg)
    HANDLE_MESSAGE(RemoteMachineUpdatedMsg)
    HANDLE_MESSAGE(NewRemoteMachineMsg)
    HANDLE_MESSAGE(RemoteMachineLeftMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x16C)
END_HANDLERS
