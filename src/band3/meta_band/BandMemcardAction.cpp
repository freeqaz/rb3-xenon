#include "meta_band/BandMemcardAction.h"
#include "meta/FixedSizeSaveable.h"
#include "meta/FixedSizeSaveableStream.h"
#include "meta/MemcardMgr.h"
#include "meta_band/BandProfile.h"
#include "os/Memcard.h"

// Retail TU 0x825D77D8-0x825D7AD8, between CriticalUserListener and CharData.
// The file name is ours. Every body is read off the retail bytes.

// The save format revision these actions write and accept (retail 0x98).
static const int kSaveVersion = 0x98;

// Retail 0x825D77D8.
SaveMemcardAction::SaveMemcardAction(BandProfile *profile) : MemcardAction(profile) {
    mResult = kMCNoError;
}

// Retail 0x825D7820.
LoadMemcardAction::LoadMemcardAction(BandProfile *profile) : MemcardAction(profile) {
    mResult = kMCNoError;
}

// Retail 0x825D7870: revision, symbol table, then the profile; no encryption.
void SaveMemcardAction::PreAction() {
    FixedSizeSaveableStream fsss(
        TheMemcardMgr.mSaveDataBuffer, TheMemcardMgr.mSaveDataLength, true
    );
    int ver = kSaveVersion;
    fsss << ver;
    fsss.InitializeTable();
    fsss << *mProfile;
    fsss.SaveTable();
    mResult = fsss.Fail() ? kMCGeneralError : kMCNoError;
}

void SaveMemcardAction::PostAction() {}

void LoadMemcardAction::PreAction() {}

// Retail 0x825D7990. Revisions 0x8F-0x98 load. Below 0x98 a save with an empty
// symbol table is obsolete, and below 0x96 the profile data is read-encrypted.
void LoadMemcardAction::PostAction() {
    if (mResult == kMCNoError) {
        FixedSizeSaveableStream fsss(
            TheMemcardMgr.mSaveDataBuffer, TheMemcardMgr.mSaveDataLength, true
        );
        int ver;
        fsss >> ver;
        FixedSizeSaveable::sCurrentMemcardLoadVer = ver;
        if (ver <= 0x8e) {
            mResult = kMCObsoleteVersion;
            return;
        }
        if (ver > kSaveVersion) {
            mResult = kMCNewerVersion;
            return;
        }
        fsss.LoadTable(ver);
        if (ver < kSaveVersion && fsss.GetSymbolCount() <= 0) {
            mResult = kMCObsoleteVersion;
            return;
        }
        if (ver < 0x96)
            fsss.EnableReadEncryption();
        fsss >> *mProfile;
        if (ver < 0x96)
            fsss.DisableEncryption();
        FixedSizeSaveable::sCurrentMemcardLoadVer = kSaveVersion;
        mResult = fsss.Fail() ? kMCGeneralError : kMCNoError;
    }
}
