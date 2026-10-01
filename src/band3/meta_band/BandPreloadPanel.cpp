#include "meta_band/BandPreloadPanel.h"
#include "meta/PreloadPanel.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/LockStepMgr.h"
#include "meta_band/SessionMgr.h"
#include "obj/ObjMacros.h"
#include "utl/MakeString.h"

// Retail TU: 0x826047A0-0x826050A8, directly before BandStorePanel.cpp.
// StaticClassName/NewObject are emitted by MetaPanel.cpp's factory
// registration (0x8256E940 / 0x8256E9C0), not here.

BandPreloadPanel::BandPreloadPanel() {
    // 0x82604BE0: a file-scope counter at .bss 0x82E007F8 is read, stored
    // back incremented, and its OLD value is formatted into the lock name.
    static int preloadIndex;
    mLockInProgress = false;
    const char *lockName = MakeString("preload_lock%i", preloadIndex++);
    mPreloadLock = new LockStepMgr(lockName, this);
}

BandPreloadPanel::~BandPreloadPanel() { delete mPreloadLock; }

void BandPreloadPanel::Load() {
    PreloadPanel::Load();
    mLockInProgress = true;
    if (TheSessionMgr->IsLeaderLocal()) {
        mPreloadLock->StartLock();
    }
}

void BandPreloadPanel::PollForLoading() {
    PreloadPanel::PollForLoading();
    if (PreloadPanel::IsLoaded() && mPreloadLock->InLock()
        && !mPreloadLock->HasResponded()) {
        bool success = mPreloadResult == kPreloadSuccess;
        if (mPreloadResult == kPreloadSuccess && !TheSessionMgr->IsLocal()) {
            if (TheSongMgr.IsDemo(TheSongMgr.GetSongIDFromShortName(CurrentSong(), true)
                )) {
                success = false;
            }
        }
        mPreloadLock->RespondToLock(success);
    }
}

bool BandPreloadPanel::IsLoaded() const {
    return !PreloadPanel::IsLoaded() ? false : !mLockInProgress;
}

// Retail ICF-folds this body with SyncGameStartPanel::OnMsg(LockStepStartMsg)
// (both are `return 1`); the call in Handle at 0x82604EB8 targets 0x826A9880.
DataNode BandPreloadPanel::OnMsg(const LockStepStartMsg &) { return 1; }

DataNode BandPreloadPanel::OnMsg(const LockStepCompleteMsg &msg) {
    if (!msg->Int(2) && mPreloadResult == kPreloadSuccess) {
        mPreloadResult = kPreloadFailure;
    }
    mLockInProgress = false;
    return 1;
}

BEGIN_HANDLERS(BandPreloadPanel)
    HANDLE_MESSAGE(LockStepStartMsg)
    HANDLE_MESSAGE(LockStepCompleteMsg)
    HANDLE_SUPERCLASS(PreloadPanel)
    HANDLE_CHECK(0x62)
END_HANDLERS
