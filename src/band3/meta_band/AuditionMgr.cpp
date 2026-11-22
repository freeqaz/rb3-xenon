#include "meta_band/AuditionMgr.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/BandUI.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/UIEventMgr.h"
#include "meta_band/SessionMgr.h"
#include "net/Net.h"
#include "net/Server.h"
#include "net_band/RockCentral.h"
#include "math/Utl.h"
#include "meta/ConnectionStatusPanel.h"
#include "obj/ObjMacros.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "os/ContentMgr.h"
#include "os/File.h"
#include "os/FileCache.h"
#include "os/Timer.h"
#include "obj/DataFile.h"
#include "utl/BufStream.h"
#include "utl/FilePath.h"
#include "utl/MemMgr.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "ui/UI.h"
#include "ui/UIScreen.h"
#include "utl/DataPointMgr.h"
#include "utl/Str.h"

// Every body here is written from the retail XEX (TU5); no surviving source has
// this class. Addresses are the retail function starts. Helper names that no
// string or symbol attests are ours and say so where they are declared.

extern "C" {
DWORD XMountUtilityDrive(BOOL fFormatClean, DWORD dwBytesPerCluster, DWORD dwFileCacheSize);
DWORD XUnmountUtilityDrive();
}

// .data 0x82C78F24, "ugc_audition_temp_song", defined in utl/SongInfoCopy.cpp;
// retail has one writable global that both files read.
extern const char *gUGCAuditionTempSongName;

/** retail 0x825EC480: takes the audition song's Symbol; ThreadStart calls it
    on the utility drive once mounted. Name ours. */
void PrepareAuditionSongFiles(Symbol);

/** retail 0x825EC3A0: the path of one audition song file under the utility
    drive root: 0 songs.dta, 1 .mid, 2 .mogg, 3 gen/.milo_xbox,
    4 gen/_keep.png_xbox, 5 gen/_weights.bin (the format strings are retail's);
    anything else returns an empty string. The song is a plain string: the
    downloader passes its String's buffer. Name ours. */
const char *AuditionSongFile(int file, const char *song);
/** retail 0x825EC460: `<root>/<song>/<song>`, the song's base path, which
    FixupSongData writes into the song's `(song (name ...))`. Name ours. */
const char *AuditionSongPath(const char *song);

/** The UGC download client an AuditionDownloader owns at 0x10: retail RTTI
    `.?AVUGCNet_Server@@`, vtable 0x8219C1C8 (slot 0 the deleting dtor, slot 1
    Start, slot 2 Poll), 0x40 bytes. Its code is the late .text block at
    0x82B8C8C8 onward; the non-virtual methods are 8-byte thunks that forward
    to the object at 0x3c. Only what this TU calls is declared; every method
    name is ours. */
class OggMap;

class UGCNet_Server {
public:
    /** What GetFileInfo reports for one file. */
    struct FileInfo {
        bool mDone; // 0x0
        unsigned int mSize; // 0x4
        /** Compared against AuditionDownloader::Entry::mHash to skip a file
            already on the drive. */
        unsigned char mHash[20]; // 0x8
        int unk1c[3]; // 0x1c
        unsigned int mReceived; // 0x28
    };
    UGCNet_Server(); // 0x82B8C8C8
    virtual ~UGCNet_Server(); // slot 0, ??_G 0x82B8C930
    /** Slot 1 (0x82B8C990): the downloader's ctor passes the gamertag and 0. */
    virtual void Start(String gamertag, int);
    virtual void Poll(); // slot 2, 0x82B8CB50
    /** 0x82B8C928: state 4 passes false and resumes the downloader, leaving
        state 4 passes true. */
    void SetPaused(bool);
    void GetFileInfo(int file, FileInfo *info); // 0x82B8D2E8
    /** 0x82B8C908: the file is already on the drive with this hash. */
    void SkipFile(int file);
    /** 0x82B8C910: download the file into memory (songs.dta). */
    void SetFileBuffer(int file, void *buf);
    /** 0x82B8C918: download the file into an open File. */
    void SetFileTarget(int file, File *dest, int);
    void BeginDownload(); // 0x82B8C920

    int unk4[2]; // 0x4
    int mState; // 0xc -- AuditionDownloader::ClientStatus maps it
    int unk10[11]; // 0x10
    void *mImpl; // 0x3c -- what the thunks forward to
};

/** The files an audition download fetches, as indices into
    AuditionDownloader::mEntries; 7 (the entry count) ends the list. Retail
    .rdata 0x8219C73C holds {0, 1, 2, 3, 7}. */
static const int sAuditionFiles[] = { 0, 1, 2, 3, 7 };

/** The downloader AuditionMgr creates in state 3 (`li r3,0xe4`, ctor
    0x82560B08). It holds the song name, the client and seven file entries, each
    with a 20-byte hash. Name ours. */
class AuditionDownloader {
public:
    struct Entry {
        File *mFile; // 0x0
        /** Set once the file has downloaded completely; then mHash is the
            hash of what is on the drive. */
        bool mHasHash; // 0x4
        unsigned char mHash[20]; // 0x5
    };
    AuditionDownloader(const char *gamertag, const char *song); // 0x82560B08
    ~AuditionDownloader(); // 0x82561460
    /** 0x825614F8: polls the client and opens the download targets once it
        reports the file list (client status 2). */
    void Poll();
    /** 0x82560E80: closes the files and frees the songs.dta buffer. */
    void Close();
    /** 0x82560C28: deletes the open File of every audition file. Name ours. */
    void CloseFiles();
    /** 0x82560F48: points the client at a target for every audition file:
        songs.dta into a memory buffer, the rest into files on the utility
        drive unless the drive already holds a file with the same hash.
        Name ours. */
    void SetupFiles();
    /** 0x825616B0: parses the downloaded songs.dta and returns a new array
        holding its first entry (one reference, the caller's), or NULL when
        it is not there; frees the buffer. Name ours. */
    DataArray *ReadSongData();
    /** 0x82560ED0: 0 = no client, else the client state mapped to 1-6. */
    int ClientStatus();
    /** 0x82561550: 0 = none/failed, 1 = connecting (2-3), 2 = lost (5-6). */
    int NetworkStatus();
    /** 0x825615A0: percent of the audition files received. */
    float Progress();
    /** 0x82561838: every audition file is present and complete. */
    bool IsReady();

    void Disable() {
        mActive = false;
        mClient->SetPaused(true);
    }
    void Enable() {
        mClient->SetPaused(false);
        mActive = true;
    }

    String mSong; // 0x0
    int unkc; // 0xc -- the ctor does not write it
    UGCNet_Server *mClient; // 0x10
    /** songs.dta, downloaded into memory. */
    void *mBuf; // 0x14
    int mBufSize; // 0x18
    Entry mEntries[7]; // 0x1c
    bool mActive; // 0xe0
};

/** Rock Band Network's song validator: retail asserts name the file
    `.\Validator.cpp`; the class name and every method name are ours. 0x104
    bytes. It lives in the unpinned .text after SampleData (0x8272BAC8
    onward) and has no body in our tree. */
class Validator {
public:
    Validator(); // 0x8272BAC8
    ~Validator(); // 0x8272BAD8
    /** 0x8272C8E8: "Validating Song Data (songs.dta)...". */
    bool ValidateSongData(BinStream &);
    /** 0x8272C730: "Validating Midi (%s)...". */
    bool ValidateMidi(const char *file);
    /** 0x8272C800: "Validating Mogg (%s)...". Validates in slices: returns true
        when finished, with the verdict in `ok`. */
    bool ValidateMogg(const char *file, bool &ok);
    /** 0x827303C8: "Validating Lip Sync (%s)..."; AuditionMgr passes true. */
    bool ValidateLipSync(const char *file, bool);

    char mError[0x100]; // 0x0 -- formatted by 0x8272BB98
    /** Created by the first ValidateMogg call, deleted when it finishes. */
    OggMap *mOggMap; // 0x100
};

AuditionMgr gAuditionMgr;
AuditionMgr *TheAuditionMgr = &gAuditionMgr;

// 0x82560B08
AuditionDownloader::AuditionDownloader(const char *gamertag, const char *song)
    : mSong(song), mBuf(NULL), mBufSize(0), mActive(false) {
    mClient = new UGCNet_Server;
    if (mClient) {
        mClient->Start(gamertag, 0);
        mClient->SetPaused(true);
    }
    for (int i = 0; i < 7; i++) {
        mEntries[i].mFile = NULL;
        mEntries[i].mHasHash = false;
        memset(mEntries[i].mHash, 0, sizeof(mEntries[i].mHash));
    }
}

// 0x82560C28
void AuditionDownloader::CloseFiles() {
    for (int i = 0; sAuditionFiles[i] != 7; i++) {
        Entry &entry = mEntries[sAuditionFiles[i]];
        delete entry.mFile;
        entry.mFile = NULL;
    }
}

// 0x82560CB0
void AuditionMgr::EnterStart() {
    ThePlatformMgr.AddSink(this, SigninChangedMsg::Type());
    TheRockCentral.AddSink(this, ServerStatusChangedMsg::Type());
    OvershellPanel *overshell = TheBandUI.mOvershell;
    mBuilder = new AuditionSessionBuilder(
        TheSessionMgr, TheBandUserMgr, overshell, TheSongMgrPtr, TheModifierMgr
    );
    mValidationFailed = false;
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

// 0x82560F48
void AuditionDownloader::SetupFiles() {
    Close();
    for (int i = 0; sAuditionFiles[i] != 7; i++) {
        int file = sAuditionFiles[i];
        UGCNet_Server::FileInfo info;
        mClient->GetFileInfo(file, &info);
        if (file == 0) {
            if (mBuf) {
                MemFree(mBuf);
                mBuf = NULL;
            }
            mBufSize = info.mSize;
            mBuf = MemAlloc(info.mSize, __FILE__, __LINE__, "AuditionDownloader");
            mClient->SetFileBuffer(0, mBuf);
        } else {
            Entry &entry = mEntries[file];
            if (entry.mHasHash && memcmp(info.mHash, entry.mHash, sizeof(info.mHash)) == 0) {
                mClient->SkipFile(file);
            } else {
                const char *path = AuditionSongFile(file, mSong.c_str());
                entry.mHasHash = false;
                memcpy(entry.mHash, info.mHash, sizeof(entry.mHash));
                entry.mFile = NewFile(path, 0x301);
                mClient->SetFileTarget(file, entry.mFile, 0);
            }
        }
    }
    mClient->BeginDownload();
}

// 0x825610B8
void AuditionMgr::Init() {
    SetName("audition_mgr", ObjectDir::Main());
    Symbol song(gUGCAuditionTempSongName);
    mSong = song;
    static Symbol none("none");
    mVenue = none;
}

// 0x82561168 (ThreadCallback slot 1)
int AuditionMgr::ThreadStart() {
    int ret = 1;
    switch (mState) {
    case 2:
        if (XMountUtilityDrive(true, 0x8000, 0x8000) != 0) {
            ret = 0;
        } else {
            PrepareAuditionSongFiles(mSong);
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
    mDownloader = new AuditionDownloader(gamertag.c_str(), mSong.Str());
    SetState(4);
    UpdateScreen();
}

// 0x82561360
void AuditionMgr::ExitLoad() {
    mDownloader->Close();
    delete mFileCache;
    mFileCache = NULL;
    delete mValidator;
    mValidator = NULL;
}

// 0x825613C8
void AuditionMgr::EnterFinish() {
    mBuilder->RemoveAutoplayUsers();
    TheSongMgr.RemoveSong(mSong);
    delete mBuilder;
    mBuilder = NULL;
    TheContentMgr.SetRefreshAllowed(true);
    SetState(0);
    UpdateScreen();
}

// 0x82560E80
void AuditionDownloader::Close() {
    CloseFiles();
    if (mBuf) {
        MemFree(mBuf);
        mBuf = NULL;
    }
    mBufSize = 0;
}

// 0x82560ED0
int AuditionDownloader::ClientStatus() {
    if (!mClient)
        return 0;
    switch (mClient->mState) {
    case 0:
        return 0;
    case 3:
        return 6;
    case 5:
    case 9:
        return 3;
    case 6:
    case 7:
        return 2;
    case 10:
        return 4;
    case 11:
        return 5;
    default:
        return 1;
    }
}

// 0x82561460
AuditionDownloader::~AuditionDownloader() {
    delete mClient;
    mClient = NULL;
    Close();
}

// 0x825614F8
void AuditionDownloader::Poll() {
    mClient->Poll();
    if (ClientStatus() == 2)
        SetupFiles();
}

// 0x82561550
int AuditionDownloader::NetworkStatus() {
    switch (ClientStatus()) {
    case 2:
    case 3:
        return 1;
    case 5:
    case 6:
        return 2;
    default:
        return 0;
    }
}

// 0x825615A0
float AuditionDownloader::Progress() {
    if (mActive && NetworkStatus() == 1) {
        unsigned int total = 0;
        unsigned int missing = 0;
        for (int i = 0; sAuditionFiles[i] != 7; i++) {
            int file = sAuditionFiles[i];
            UGCNet_Server::FileInfo info;
            mClient->GetFileInfo(file, &info);
            total += info.mSize;
            if (!info.mDone) {
                missing += info.mSize - info.mReceived;
            } else if (mEntries[file].mFile) {
                mEntries[file].mHasHash = true;
            }
        }
        return (float)(total - missing) * 100.0f / (float)total;
    }
    return 0.0f;
}

// 0x825616B0
DataArray *AuditionDownloader::ReadSongData() {
    UGCNet_Server::FileInfo info;
    mClient->GetFileInfo(0, &info);
    if (!info.mDone || !mBuf || mBufSize < 1)
        return NULL;
    BufStream stream(mBuf, mBufSize, true);
    stream.Seek(0, BinStream::kSeekBegin);
    DataArray *read = DataReadStream(&stream);
    DataArrayPtr songs;
    songs->Insert(0, DataNode(read->Array(0), kDataArray));
    songs->AddRef();
    read->Release();
    if (mBuf) {
        MemFree(mBuf);
        mBuf = NULL;
    }
    mBufSize = 0;
    return songs;
}

// 0x82561838
bool AuditionDownloader::IsReady() {
    int status = ClientStatus();
    if (status != 1 && status != 4)
        return false;
    for (int i = 0; sAuditionFiles[i] != 7; i++) {
        UGCNet_Server::FileInfo info;
        mClient->GetFileInfo(sAuditionFiles[i], &info);
        if (!info.mSize || !info.mDone)
            return false;
    }
    return true;
}

// 0x825618D8
AuditionMgr::~AuditionMgr() {
    if (mSongData) {
        mSongData->Release();
        mSongData = NULL;
    }
}

// 0x825619A8
int AuditionMgr::LoadedSongStatus() {
    if (mDownloader) {
        switch (mState) {
        case 4:
            if (mDownloader->IsReady())
                return 2;
            break;
        case 5:
        case 6:
        case 7:
            return 1;
        }
    }
    return 0;
}

// 0x82561A08
float AuditionMgr::DownloadProgress() {
    if (mDownloader)
        return Clamp(0.0f, 100.0f, mDownloader->Progress()) * 0.01f;
    return 0.0f;
}

// 0x82561A70
void AuditionMgr::SendToScreen(Message &msg) {
    if (!TheUI->InTransition()) {
        UIScreen *screen = TheUI->CurrentScreen();
        if (screen && streq(screen->Name(), "audition_main_screen")) {
            screen->Handle(msg, true);
        }
    }
}

// 0x82561B20
void AuditionMgr::FixupSongData() {
    DataArray *song = mSongData->Array(0);
    song->Node(0) = mSong;
    static Symbol song_sym("song");
    static Symbol name("name");
    song->FindArray(song_sym, name)->Node(1) = AuditionSongPath(mSong.Str());
}

// 0x82561CB0
void AuditionMgr::ApplySlotSettings(AuditionSessionBuilder *builder) {
    int numSlots = builder->mSlots.size();
    for (int i = 0; i < numSlots; i++) {
        BandUser *user = builder->mSlots[i]->mUser;
        if (user) {
            switch (builder->GetSlotTrack(i)) {
            case 0:
                user->SetTrackType(kTrackGuitar);
                break;
            case 1:
                user->SetTrackType(kTrackBass);
                break;
            case 2:
                user->SetTrackType(kTrackKeys);
                break;
            case 3:
                user->SetTrackType(kTrackRealKeys);
                break;
            case 4:
                user->SetTrackType(kTrackDrum);
                user->SetPreferredScoreType(kScoreDrum);
                break;
            case 5:
                user->SetTrackType(kTrackDrum);
                user->SetPreferredScoreType(kScoreRealDrum);
                break;
            case 6:
                user->SetTrackType(kTrackVocals);
                user->SetPreferredScoreType(kScoreVocals);
                break;
            case 7:
                user->SetTrackType(kTrackVocals);
                user->SetPreferredScoreType(kScoreHarmony);
                break;
            }
            user->SetDifficulty((Difficulty)builder->GetSlotDifficulty(i));
        }
    }
}

// 0x82561DE8
void AuditionMgr::ApplySettings() {
    mBuilder->ApplySettings();
    ApplySlotSettings(mBuilder);
    MetaPerformer *perf = MetaPerformer::Current();
    perf->SetVenue(mVenue);
    perf->SetSong(mSong);
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

// 0x82561FD0
DataNode AuditionMgr::OnMsg(const SigninChangedMsg &msg) {
    if (mState != 0 && mUser) {
        int pad = mUser->GetPadNum();
        if (msg.GetChangedMask() & (1 << pad)) {
            if (!ThePlatformMgr.IsSignedIn(pad) || !ThePlatformMgr.IsSignedIntoLive(pad)) {
                if (!TheUIEventMgr->HasActiveTransitionEvent()) {
                    static Symbol sign_out("sign_out");
                    static Message init("init", 0);
                    init[0] = 5; // kRBNMemberSignedOut
                    TheUIEventMgr->TriggerEvent(sign_out, init);
                }
                return 0;
            }
        }
    }
    return DataNode(kDataUnhandled, 0);
}

// 0x82562228
DataNode AuditionMgr::OnMsg(const ConnectionStatusChangedMsg &msg) {
    if (mState >= 4 && mState <= 7 && !msg.Connected()) {
        if (!TheUIEventMgr->HasActiveTransitionEvent()) {
            static Symbol lost_connection("lost_connection");
            static Message init("init", 0);
            init[0] = 0;
            TheUIEventMgr->TriggerEvent(lost_connection, init);
        }
        return 0;
    }
    return DataNode(kDataUnhandled, 0);
}

// 0x82562418
DataNode AuditionMgr::OnMsg(const ServerStatusChangedMsg &msg) {
    if (mState >= 4 && mState <= 7 && !msg.Success()) {
        if (!TheUIEventMgr->HasActiveTransitionEvent()) {
            static Symbol lost_connection("lost_connection");
            static Message init("init", 0);
            init[0] = 1;
            TheUIEventMgr->TriggerEvent(lost_connection, init);
        }
        return 0;
    }
    return DataNode(kDataUnhandled, 0);
}

// 0x82562608
void AuditionMgr::EnterNetPoll() {
    if (!ThePlatformMgr.IsConnected()) {
        if (!TheUIEventMgr->HasActiveTransitionEvent()) {
            static Symbol lost_connection("lost_connection");
            static Message init("init", 0);
            init[0] = 0;
            TheUIEventMgr->TriggerEvent(lost_connection, init);
        }
    } else if (!TheServer.IsConnected()) {
        if (!TheUIEventMgr->HasActiveTransitionEvent()) {
            static Symbol lost_connection("lost_connection");
            static Message init("init", 0);
            init[0] = 1;
            TheUIEventMgr->TriggerEvent(lost_connection, init);
        }
    } else {
        mDownloader->Enable();
        UpdateScreen();
    }
}

// 0x82562978
void AuditionMgr::EnterLoad() {
    UpdateScreen();
    bool ok = true;
    mValidator = new Validator;
    if (mDownloader->mBuf) {
        BufStream stream(mDownloader->mBuf, mDownloader->mBufSize, true);
        stream.Seek(0, BinStream::kSeekBegin);
        ok = mValidator->ValidateSongData(stream);
        if (ok) {
            if (mSongData) {
                mSongData->Release();
                mSongData = NULL;
            }
            mSongData = mDownloader->ReadSongData();
            FixupSongData();
            DataArray *song = mSongData->Array(0);
            static Symbol song_id("song_id");
            song->FindArray(song_id)->Node(1) = 99000001;
            static Symbol license_bits("license_bits");
            DataArrayPtr licenseBits(license_bits, 1);
            song->Insert(song->Size() - 1, licenseBits);
            static Symbol song_sym("song");
            static Symbol mogg_encryption("mogg_encryption");
            DataArrayPtr moggEncryption(mogg_encryption, 0);
            DataArray *songArr = song->FindArray(song_sym);
            songArr->Insert(songArr->Size() - 1, moggEncryption);
            TheSongMgr.RemoveSong(mSong);
            TheSongMgr.AddAuditionSongData(mSongData, AuditionSongFile(0, mSong.Str()));
        }
    }
    if (!ok) {
        SetState(7);
        UpdateScreen();
    } else {
        mDownloader->CloseFiles();
        mFileCache = new FileCache(0x100000, kLoadBack, true);
        mFileCache->StartSet(0);
        mFileCache->Add(FilePath(AuditionSongFile(1, mSong.Str())), 0, FilePath(""));
        mFileCache->Add(FilePath(AuditionSongFile(3, mSong.Str())), 0, FilePath(""));
        mFileCache->Add(FilePath(AuditionSongFile(5, mSong.Str())), 0, FilePath(""));
        mFileCache->EndSet();
        mLoadStep = 1;
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

// 0x825632F0
void AuditionMgr::EnterValidationFailed() {
    mValidationFailed = true;
    Server *server = TheNet.GetServer();
    if (mUser && server && server->IsConnected()) {
        SendDataPoint("rbn/audition/fail", "pid", server->GetPlayerID(mUser->GetPadNum()));
    }
    UpdateScreen();
    static Message on_validation_failed("on_validation_failed");
    SendToScreen(on_validation_failed);
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
        if (mSongData) {
            mSongData->Release();
            mSongData = NULL;
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
        EnterNetPoll();
        break;
    case 5:
        EnterLoad();
        break;
    case 6:
        EnterValidated();
        break;
    case 7:
        EnterValidationFailed();
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

// 0x82563578 (polled in state 5)
void AuditionMgr::PollLoad() {
    if (mFileCache && !mFileCache->DoneCaching())
        return;
    bool ok = true;
    if (mLoadStep == 1)
        mLoadStep = 2;
    switch (mLoadStep) {
    case 2:
        ok = mValidator->ValidateMidi(AuditionSongFile(1, mSong.Str()));
        mLoadStep = ok ? 3 : 6;
        break;
    case 3:
        ok = mValidator->ValidateLipSync(AuditionSongFile(3, mSong.Str()), true);
        mLoadStep = ok ? 5 : 6;
        break;
    case 5: {
        bool done = false;
        const char *file = AuditionSongFile(2, mSong.Str());
        Timer timer;
        timer.Start();
        while (timer.SplitMs() < 10.0f) {
            done = mValidator->ValidateMogg(file, ok);
            if (done)
                break;
        }
        if (done)
            mLoadStep = ok ? 7 : 6;
        break;
    }
    }
    if (mLoadStep == 6) {
        SetState(7);
        UpdateScreen();
    } else if (mLoadStep == 7) {
        SetState(6);
        UpdateScreen();
    }
}

// 0x82563708 (primary slot 21)
void AuditionMgr::Poll() {
    if (mDownloader)
        mDownloader->Poll();
    if (mState == 0)
        return;
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

// 0x82563800
void AuditionMgr::StartNetPoll() {
    if (mState != 1 && mState != 2 && mState != 3) {
        SetState(4);
        UpdateScreen();
    }
}

// 0x82563858
void AuditionMgr::StartValidation() {
    MILO_ASSERT(mDownloader && mDownloader->IsReady(), 0);
    SetState(5);
    UpdateScreen();
}

// 0x825638B0
BEGIN_HANDLERS(AuditionMgr)
    HANDLE_EXPR_STATIC(can_enter_audition, mState == 0)
    HANDLE_ACTION_STATIC(enter_audition, (SetState(1), UpdateScreen()))
    HANDLE_ACTION_STATIC(exit_audition, (SetState(11), UpdateScreen()))
    HANDLE_EXPR_STATIC(
        get_network_status, mDownloader ? mDownloader->NetworkStatus() : 0
    )
    HANDLE_EXPR_STATIC(get_loaded_song_status, LoadedSongStatus())
    HANDLE_EXPR_STATIC(get_download_progress, DownloadProgress())
    HANDLE_EXPR_STATIC(is_validating, mState == 5 || mState == 6)
    HANDLE_EXPR_STATIC(has_validation_failed, mValidationFailed)
    HANDLE_EXPR_STATIC(is_ready, mDownloader && mDownloader->IsReady())
    HANDLE_ACTION_STATIC(start_net_poll, StartNetPoll())
    HANDLE_ACTION_STATIC(start_validation, StartValidation())
    HANDLE_ACTION_STATIC(toggle_drum_fills, mDrumFillsEnabled = !mDrumFillsEnabled)
    HANDLE_EXPR_STATIC(are_drum_fills_enabled, mDrumFillsEnabled)
    HANDLE_ACTION_STATIC(set_venue_sym, mVenue = _msg->Sym(2))
    HANDLE_EXPR_STATIC(get_venue_sym, mVenue)
    HANDLE_ACTION_STATIC(apply_settings, ApplySettings())
    HANDLE_ACTION_STATIC(remove_autoplay_users, mBuilder->RemoveAutoplayUsers())
    HANDLE_EXPR_STATIC(has_autoplayer, mBuilder->HasAutoplayer())
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_MESSAGE(ConnectionStatusChangedMsg)
    HANDLE_MESSAGE(ServerStatusChangedMsg)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0)
END_HANDLERS
