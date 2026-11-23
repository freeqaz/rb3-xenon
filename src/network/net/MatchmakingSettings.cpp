#include "net/MatchmakingSettings.h"
#include "net/Synchronize.h"
#include "net/NetSession.h"
#include "os/Debug.h"

void MatchmakingSettings::SetMode(Symbol mode, int filt) {
    mModeName = mode;
    mModeFilter = filt;
}

void MatchmakingSettings::AddCustomSetting(int id, int value) {
    mCustomIDs.push_back(id);
    mCustomValues.push_back(value);
}

void MatchmakingSettings::ClearCustomSettings() {
    mCustomIDs.clear();
    mCustomValues.clear();
}

int MatchmakingSettings::NumCustomSettings() const { return mCustomIDs.size(); }

int MatchmakingSettings::GetCustomID(int index) const {
    MILO_ASSERT_RANGE(index, 0, mCustomIDs.size(), 0x2A);
    return mCustomIDs[index];
}

int MatchmakingSettings::GetCustomValue(int index) const {
    MILO_ASSERT_RANGE(index, 0, mCustomValues.size(), 0x30);
    return mCustomValues[index];
}

int MatchmakingSettings::GetCustomValueByID(int id) const {
    for (int i = 0; i < (int)mCustomIDs.size(); i++) {
        if (GetCustomID(i) == id)
            return GetCustomValue(i);
    }
    MILO_FAIL("Settings do not have a value for id %x\n", id);
    return -1;
}

void MatchmakingSettings::Save(BinStream &bs) const {
    bs << mModeName;
    bs << mModeFilter;
    bs << mRanked;
    MILO_ASSERT(mCustomIDs.size() < 256, 0x44);
    unsigned char size = mCustomIDs.size();
    bs << size;
    for (int i = 0; i < size; i++) {
        bs << mCustomIDs[i];
        bs << mCustomValues[i];
    }
}

void MatchmakingSettings::Load(BinStream &bs) {
    bs >> mModeName;
    bs >> mModeFilter;
    bs >> mRanked;
    unsigned char size;
    bs >> size;
    mCustomIDs.clear();
    mCustomValues.clear();
    for (int i = 0; i < size; i++) {
        int id, value;
        bs >> id;
        bs >> value;
        mCustomIDs.push_back(id);
        mCustomValues.push_back(value);
    }
}

SessionSettings::SessionSettings() : Synchronizable("sessionsettings") {
#ifdef HX_NATIVE
    // Retail writes no mRanked@0xC here: band.exe TU5 fn_823F3C78 stores the
    // vtables, mModeName@0x4 and the two vectors, constructs Synchronizable
    // (+0x78), then runs Clear inline: mModeFilter@0x8 = -1 (+0x9C),
    // ClearCustomSettings (+0xA0), mPublic@0x48 = 0 (+0xA4). Clear
    // (fn_823F3BE8: +0x18, +0x1C, +0x24) does not write it either, so retail
    // reads whatever its heap held until BandMatchmaker::OnMsg(ModeChangedMsg)
    // stores SetRanked(IsRanked()). Natively that byte is uninitialized heap,
    // and a StartGame reached with no mode change took the ranked
    // (arbitration) arm. Start at false, the quickplay value; the ctor only,
    // since retail's Clear keeps the value. A36; gate
    // rb3_app_check_session_settings_unranked (native/src/rb3_app_gates.cpp).
    mRanked = false;
#endif
    Clear();
}

void SessionSettings::Clear() {
    mModeFilter = -1;
    ClearCustomSettings();
    mPublic = false;
}

void SessionSettings::SetPublic(bool pub) {
    bool changed = mPublic != pub;
    mPublic = pub;
    if (changed) {
        SetSyncDirty(-1, false);
    }
    TheNetSession->OnSetPublic(pub);
}

void SessionSettings::SetMode(Symbol mode, int filt) {
    Symbol old = mModeName;
    MatchmakingSettings::SetMode(mode, filt);
    if (mode != old) {
        SetSyncDirty(-1, false);
    }
}

void SessionSettings::SetRanked(bool ranked) {
    bool old = mRanked;
    mRanked = ranked;
    if (old != ranked) {
        SetSyncDirty(-1, false);
    }
}

void SessionSettings::AddCustomSetting(int id, int value) {
    MatchmakingSettings::AddCustomSetting(id, value);
    SetSyncDirty(-1, false);
}

void SessionSettings::SyncSave(BinStream &bs, unsigned int) const {
    MatchmakingSettings::Save(bs);
    bs << mPublic;
}

void SessionSettings::SyncLoad(BinStream &bs, unsigned int) {
    MatchmakingSettings::Load(bs);
    bs >> mPublic;
}

bool SessionSettings::HasSyncPermission() const {
    return TheNetSession->IsLocal() || TheNetSession->IsHost();
}

void SessionSettings::OnSynchronizing(unsigned int) {
    if (TheNetSession->IsOnlineEnabled()) {
        TheNetSession->UpdateSettings();
    }
}

void SessionSettings::OnSynchronized(unsigned int) {
    static SettingsChangedMsg msg;
    TheNetSession->Handle(msg, false);
}

SearchSettings::SearchSettings(int filt, bool ranked, int id) {
    mModeFilter = filt;
    mRanked = ranked;
    mQueryID = id;
}
