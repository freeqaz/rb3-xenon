#include "meta_band/AuditionMgr.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/BandUI.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/SessionMgr.h"
#include "net/Server.h"
#include "net_band/RockCentral.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "os/ContentMgr.h"
#include "os/File.h"
#include "os/FileCache.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "ui/UI.h"
#include "ui/UIScreen.h"
#include "utl/Str.h"

// Every body here is written from the retail XEX (TU5); no surviving source has
// this class. Addresses are the retail function starts. Helper names that no
// string or symbol attests are ours and say so where they are declared.

extern "C" {
DWORD XMountUtilityDrive(BOOL fFormatClean, DWORD dwBytesPerCluster, DWORD dwFileCacheSize);
DWORD XUnmountUtilityDrive();
}

// .data 0x82C78F24, "ugc_audition_temp_song". SongInfoCopy.cpp reads the same
// pointer (it declares it static there); retail has one writable global.
extern const char *gUGCAuditionTempSongName;

/** retail 0x825EC480: takes the audition song's Symbol; ThreadStart calls it
    on the utility drive once mounted. Name ours. */
void PrepareAuditionSongFiles(Symbol);

/** The 0x40-byte client object an AuditionDownloader owns at 0x10. It lives in
    the late .text block at 0x82B8C8C8 onward; only what this TU calls is
    declared. Name ours. */
class AuditionNetClient {
public:
    void SetEnabled(bool); // 0x82B8C928
};

/** The downloader AuditionMgr creates in state 3 (`li r3,0xe4`, ctor
    0x82560B08). It holds the song name, the client and seven file entries, each
    with a 20-byte hash. Name ours. */
class AuditionDownloader {
public:
    struct Entry {
        File *mFile; // 0x0
        bool mHasHash; // 0x4
        unsigned char mHash[20]; // 0x5
    };
    AuditionDownloader(const char *gamertag, const char *song); // 0x82560B08
    ~AuditionDownloader(); // 0x82561460
    void Poll(); // 0x825614F8
    void Close(); // 0x82560E80

    void Disable() {
        mActive = false;
        mClient->SetEnabled(true);
    }

    String mSong; // 0x0
    AuditionNetClient *mClient; // 0x10
    void *mBuf; // 0x14
    int mBufSize; // 0x18
    Entry mEntries[7]; // 0x1c
    bool mActive; // 0xe0
};

/** 0x104-byte object AuditionMgr allocates in state 5; destroyed by
    0x8272BAD8. Name ours. */
class AuditionSongBuffer {
public:
    ~AuditionSongBuffer();
};

AuditionMgr gAuditionMgr;
AuditionMgr *TheAuditionMgr = &gAuditionMgr;

// 0x82560CB0
void AuditionMgr::EnterStart() {
    ThePlatformMgr.AddSink(this, SigninChangedMsg::Type());
    TheRockCentral.AddSink(this, ServerStatusChangedMsg::Type());
    mBuilder = new AuditionSessionBuilder(
        TheSessionMgr, TheBandUserMgr, TheBandUI.mOvershell, TheSongMgrPtr, TheModifierMgr
    );
    unk40 = false;
    TheContentMgr.SetRefreshAllowed(false);
    mSavedBandNoFail = MetaPerformer::Current()->IsBandNoFailSet();
    MetaPerformer::Current()->SetBandNoFail(true);
    SetState(2);
    UpdateScreen();
}

// 0x82560DE8
void AuditionMgr::EnterEnd() {
    ThePlatformMgr.RemoveSink(this, SigninChangedMsg::Type());
    TheRockCentral.RemoveSink(this, ServerStatusChangedMsg::Type());
    MetaPerformer::Current()->SetBandNoFail(mSavedBandNoFail);
    SetState(12);
    UpdateScreen();
}

// 0x825610B8
void AuditionMgr::Init() {
    SetName("audition_mgr", ObjectDir::Main());
    unk48 = Symbol(gUGCAuditionTempSongName);
    static Symbol none("none");
    unk4c = none;
}

// 0x82561168 (ThreadCallback slot 1)
int AuditionMgr::ThreadStart() {
    int ret = 1;
    switch (mState) {
    case 2:
        if (XMountUtilityDrive(true, 0x8000, 0x8000) != 0) {
            ret = 0;
        } else {
            PrepareAuditionSongFiles(unk48);
        }
        break;
    case 12:
        if (XUnmountUtilityDrive() != 0)
            ret = 0;
        break;
    }
    return ret;
}

// 0x825611F8
void AuditionMgr::EnterDownload() {
    String gamertag(gNullStr);
    int pad = ThePlatformMgr.RBNMemberPadNum();
    mUser = NULL;
    std::vector<LocalBandUser *> &users = TheBandUserMgr->GetLocalBandUsers();
    for (std::vector<LocalBandUser *>::iterator it = users.begin(); it != users.end();
         ++it) {
        LocalBandUser *user = *it;
        if (user->GetPadNum() == pad) {
            mUser = user;
            gamertag = ThePlatformMgr.GetName(pad);
            break;
        }
    }
    if (!mUser) {
        gamertag = "An Xbox Without A Signed In Gamer Profile";
    }
    mDownloader = new AuditionDownloader(gamertag.c_str(), unk48.Str());
    SetState(4);
    UpdateScreen();
}

// 0x82561360
void AuditionMgr::ExitLoad() {
    mDownloader->Close();
    delete mFileCache;
    mFileCache = NULL;
    delete mSongBuffer;
    mSongBuffer = NULL;
}

// 0x825613C8
void AuditionMgr::EnterFinish() {
    mBuilder->Finish();
    TheSongMgr.RemoveSong(unk48);
    delete mBuilder;
    mBuilder = NULL;
    TheContentMgr.SetRefreshAllowed(true);
    SetState(0);
    UpdateScreen();
}

// 0x825618D8
AuditionMgr::~AuditionMgr() {
    if (unk44) {
        unk44->Release();
        unk44 = NULL;
    }
}

// 0x82561EA0
void AuditionMgr::UpdateScreen() {
    static Message update("update");
    if (!TheUI->InTransition()) {
        UIScreen *screen = TheUI->CurrentScreen();
        if (screen && streq(screen->Name(), "audition_main_screen")) {
            screen->Handle(update, true);
        }
    }
}

// 0x82563158
void AuditionMgr::EnterValidated() {
    UpdateScreen();
    static Message on_validation_success("on_validation_success");
    SendToScreen(on_validation_success);
    SetState(8);
    UpdateScreen();
}

// 0x82563208
void AuditionMgr::EnterUnmount() {
    mUser = NULL;
    delete mDownloader;
    mDownloader = NULL;
    ThreadCall(this);
}

// 0x82563428
void AuditionMgr::SetState(int state) {
    if (mState == state)
        return;
    switch (mState) {
    case 2:
        UpdateScreen();
        break;
    case 4:
        mDownloader->Disable();
        break;
    case 5:
        ExitLoad();
        break;
    case 8:
        if (state != 9) {
            unk30 = state;
            return;
        }
        break;
    }
    mState = state;
    switch (state) {
    case 0:
        if (unk44) {
            unk44->Release();
            unk44 = NULL;
        }
        break;
    case 1:
        EnterStart();
        break;
    case 2:
        UpdateScreen();
        ThreadCall(this);
        break;
    case 3:
        EnterDownload();
        break;
    case 4:
        EnterState4();
        break;
    case 5:
        EnterLoad();
        break;
    case 6:
        EnterValidated();
        break;
    case 7:
        EnterState7();
        break;
    case 11:
        EnterEnd();
        break;
    case 12:
        EnterUnmount();
        break;
    case 13:
        EnterFinish();
        break;
    }
}

// 0x82563708 (primary slot 21)
void AuditionMgr::Poll() {
    if (mDownloader)
        mDownloader->Poll();
    switch (mState) {
    case 4:
        UpdateScreen();
        break;
    case 5:
        PollLoad();
        break;
    case 8:
        SetState(9);
        UpdateScreen();
        break;
    case 9:
        if (unk30) {
            int next = unk30;
            unk30 = 0;
            SetState(next);
            UpdateScreen();
        }
        break;
    }
}

// 0x825637A8 (ThreadCallback slot 2)
void AuditionMgr::ThreadDone(int) {
    switch (mState) {
    case 2:
        SetState(3);
        UpdateScreen();
        break;
    case 12:
        SetState(13);
        UpdateScreen();
        break;
    }
}
