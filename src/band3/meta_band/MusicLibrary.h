#pragma once
#include "MusicLibraryNetSetlists.h"
#include "SongSetlistProvider.h"
#include "ViewSetting.h"
#include "game/BandUser.h"
#include "game/Defines.h"
#include "meta/Profile.h"
#include "meta/SongPreview.h"
#include "meta_band/BandMachine.h"
#include "meta_band/HeaderPerformanceProvider.h"
#include "meta_band/ProfileMessages.h"
#include "meta_band/SavedSetlist.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/SongSortMgr.h"
#include "meta_band/SongSortNode.h"
#include "net/Server.h"
#include "net/Synchronize.h"
#include "net_band/DataResults.h"
#include "obj/Object.h"
#include "os/ContentMgr.h"
#include "os/PlatformMgr.h"
#include "os/Timer.h"
#include "ui/UIListProvider.h"
#include <vector>

/** Retail-only async op polled by MusicLibrary::Poll — absent from the rb3-Wii
    dev branch. Retail shape (verified from the XEX): size 0x64, vptr @0x0,
    int state @0x28 (2 = done, 3 = in progress?, 4 = failed -> deleted),
    vector of overlapped IO @0x54. Impl lives in an unidentified meta_band TU:
    ctor @0x825A4860, Poll @0x825A50F8, Finish @0x825A3ED0. Only the shape
    MusicLibrary itself needs is declared here; methods are intentionally
    left undefined (never linked standalone). */
class MusicLibraryUnkOp {
public:
    virtual ~MusicLibraryUnkOp();
    void Poll(); // retail 0x825A50F8
    void Finish(); // retail 0x825A3ED0
    /** Called from retail MusicLibrary::OnExit as `lwz r3,0x19c(this);
        bl 0x825BC908`, and target_symbol_map.json names 0x825BC908
        ?ClearPreview@MusicLibraryStore@@QAAXXZ (a 208-byte real body with its own
        .pdata entry). ⚠ The "retail 0x825A3DC8" this line used to carry is WRONG:
        0x825A3DC8 is +8 INTO _M_fill_insert<BeatCollisionData> and cannot be a
        function start. Do NOT call this from ClearSongPreview -- that is a
        different function; see Unk825BC900 below. */
    void ClearPreview(); // retail 0x825BC908
    /** MusicLibrary::ClearSongPreview's tail call. Retail (0x8253AD54):
        `lwz r3,0x19c(r31); bl 0x825BC900`, and 0x825BC900 is a frameless
        2-instruction tail-jump thunk:

            0x825BC900  lwz r3, 0x4c(r3)
            0x825BC904  b   0x827B1B78   ; ?ClearCurrentPreview@StorePreviewMgr@@QAAXXZ

        i.e. a one-line method `{ mPreviewMgr->ClearCurrentPreview(); }`. Offset
        0x4c is independently corroborated: MusicLibraryStore.h already declares
        `StorePreviewMgr *mPreviewMgr; // 0x4c`.

        ⚠ NAME UNKNOWN AND DELIBERATELY NOT GUESSED. 0x825BC900 is absent from
        target_symbol_map.json, has no .pdata entry of its own (it is absorbed into
        the extent of the function ending at 0x825BC8F8 -- a live instance of
        ".pdata-absence is not a not-a-function test"), and neither oracle can name
        it: DC3 and rb3-Wii have no MusicLibraryStore at all, and their only
        ClearCurrentPreview callers are StorePreviewMgr's own
        HANDLE_ACTION(clear_current_preview,...). Named after its address per the
        Unk825BCA38 precedent below.

        ⚠ Its true owner is MusicLibraryStore, not this stub -- see the class note.
        Pays 0 in both currencies (the default ruler masks relocation args, which is
        exactly why this bug scored 100/100 while calling the wrong function). */
    void Unk825BC900(); // retail 0x825BC900
    void SetStorePreview(int); // retail 0x825A4288 — sets the store-song preview by song id
    bool IsDownloading(int); // retail 0x825BCBD0
    void LoadStoreArt(int, class Hmx::Object *); // retail 0x825BCC10
    /** Called from retail MusicLibrary::OnEnter as `if (unk1a0)
        unk19c->Unk825BCA38();`. Takes no argument (only r3 is set up). Semantics
        unknown — named after its retail address rather than guessed at. */
    void Unk825BCA38(); // retail 0x825BCA38
    /** Called from retail MusicLibrary::SelectNode's kNodeStoreSong case as
        `unk19c->Unk825BD8C8(user, songIDs)` where songIDs is a freshly built
        one-element std::vector<int> holding the just-downloaded song's id.
        The param is `user` implicitly upcast across the virtual-base graph --
        retail computes this via a raw vbtable-offset add (`lwz`+`lwz`+`add`,
        no vcall), which is exactly what an implicit pointer-to-virtual-base
        conversion compiles to; passing `user` (a LocalBandUser*) directly
        reproduces that codegen.
        The base is LocalUser, NOT User: LocalBandUser has three virtual bases
        (User, BandUser, LocalUser -- see BandUser.h:133), and retail indexes
        the vbtable at 0xc (slot 3 = LocalUser) where a `User*` param indexes
        0x4 (slot 1). /d1reportSingleClassLayoutLocalBandUser confirms the
        arithmetic: retail's `user + vbtable[3] + 4` resolves to +0x100, which
        is exactly `{vfptr} [LocalUser]`, whereas slot 1 resolves to +0x28, the
        User subobject. Declaring `User*` cost exactly one instruction (idx 131,
        `lwz r11,0x4,r11` vs retail `lwz r11,0xc,r11`) -- an argument-only diff
        that match_percent_normalized is blind to, so the row read 100.0%
        normalized while still being wrong. Deep-dived
        via Ghidra decompile of 0x825BD8C8 itself (a much larger
        MusicLibraryStore method: filters/erases matching offers from a
        vector<int>, calls FindOfferBySongID, and issues a download request) --
        only the CALL SHAPE into it is reproduced here, not its body (reloc
        args are score-invisible per CLAUDE.md, so an undefined declaration is
        sufficient, matching the IsDownloading/LoadStoreArt precedent above). */
    void Unk825BD8C8(class LocalUser *, const std::vector<int> &); // retail 0x825BD8C8
    char unk4[0x24]; // 0x4
    int mState; // 0x28 (2 = done, 4 = failed)
    char unk2c[0x1c]; // 0x2c
    /** Retail MusicLibrary::Handle's `get_store_art` returns this slot straight
        as a kDataObject (target: `lwz r11, 0x48(unk19c)` + `li r10, 4`). */
    Hmx::Object *mStoreArt; // 0x48
};

class MusicLibrary : public UIListProvider,
                     public Hmx::Object,
                     public ContentMgr::Callback,
                     public Synchronizable {
public:
    enum SetlistMode {
        kSetlistOptional = 0,
        kSetlistForced = 1,
        kSetlistForbidden = 2
    };
    class MusicLibraryTask {
    public:
        MusicLibraryTask();
        MusicLibraryTask &operator=(const MusicLibraryTask &task) {
            setlistMode = task.setlistMode;
            filter = task.filter;
            filterLocked = task.filterLocked;
            allowDuplicates = task.allowDuplicates;
            requiresStandardParts = task.requiresStandardParts;
            backScreen = task.backScreen;
            nextScreen = task.nextScreen;
            maxSetlistSize = task.maxSetlistSize;
            partSym = task.partSym;
            scoreType = task.scoreType;
            titleToken = task.titleToken;
            makingSetlistToken = task.makingSetlistToken;
            return *this;
        }

        void Reset();
        void ResetWithBackScreen(Symbol);
        void GetSongFilterAsString(String &);
        void SetSongFilter(const SongSortMgr::SongFilter &);
        const SongSortMgr::SongFilter &GetFilter() const { return filter; }

        SetlistMode setlistMode; // 0x0
        SongSortMgr::SongFilter filter; // 0x4
        bool filterLocked; // 0x18
        bool allowDuplicates; // 0x19
        bool requiresStandardParts; // 0x1a
        Symbol backScreen; // 0x1c
        Symbol nextScreen; // 0x20
        int maxSetlistSize; // 0x24
        Symbol partSym; // 0x28
        ScoreType scoreType; // 0x2c
        Symbol titleToken; // 0x30
        Symbol makingSetlistToken; // 0x34
    };

    MusicLibrary(SongPreview &);
    virtual ~MusicLibrary();
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual void Custom(int, int, class UIListCustom *, Hmx::Object *) const;
    virtual int NumData() const;
    virtual bool IsActive(int) const;
    virtual void InitData(RndDir *);
    virtual UIComponent::State
    ComponentStateOverride(int, int, UIComponent::State s) const;
    virtual int SnappableAtOrBeforeData(int) const;
    virtual bool IsSnappableAtData(int) const;
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void ContentStarted();
    virtual void ContentMounted(const char *, const char *);
    virtual void ContentDone();
    virtual const char *ContentDir() { return nullptr; }
    virtual void SyncSave(BinStream &, unsigned int) const;
    virtual void SyncLoad(BinStream &, unsigned int);
    virtual bool HasSyncPermission() const;
    virtual void OnSynchronized(unsigned int);

    std::vector<int> &GetSetlist();
    void AppendToSetlist(int);
    void RemoveLastSongFromSetlist();
    void OnLoad();
    void OnEnter();
    void OnExit();
    void ClearSongPreview();
    void ClearSetlist();
    void SetupTaskForTrainer(ControllerType);
    bool GetFilterLocked();
    bool GetDuplicatesAllowed();
    bool GetForcedSetlist();
    int GetMaxSetlistSize();
    void SetTask(MusicLibraryTask &);
    SongSortMgr::SongFilter &GetFilter();
    // retail fn_8253ACF8: `lbz r3, 0x76(r3); blr` == mTask + 0x22. The rb3-Wii
    // decomp calls that member `requiresStandardParts`, but Tour.cpp feeds it
    // `Quest::IsUGCAllowed()` (Tour::LaunchQuestFilter ->
    // CreateAndSubmitMusicLibraryTask), so it is really the allow-UGC flag.
    bool GetAllowUGC();
    int SongAtSetlistIndex(int);
    int SetlistSize();
    bool SetlistIsFull();
    bool CanHeadersBeSelected();
    ScoreType ActiveScoreType() const;
    bool GetMakingSetlist(bool) const;
    bool NetSetlistsFailed();
    bool NetSetlistsSucceeded();
    Symbol DifficultySortPart() const;
    Symbol PartForFilter() const;
    void GetNetSetlists(std::vector<NetSavedSetlist *> &) const;
    void Poll();
    void ResetFilter(FilterType);
    void TryToSetHighlight(Symbol, SongNodeType, bool);
    void PushHighlightToScreen(bool);
    void PushFilterToScreen();
    void RefreshNetSetlists();
    void SetMakingSetlist(bool);
    void PushSortToScreen();
    void UpdateHeaderData();
    void PushSetlistToScreen();
    void SetSort(SongSortType);
    void CheckSongPreview();
    bool IsExiting();
    void OnUnload();
    void ResetFilters();
    void ToggleFilter(FilterType, Symbol);
    const char *GetStatusText();
    Symbol GetCurrentSortName(bool);
    void SetTaskScoreType(ScoreType);
    void RebuildUserConfigData();
    void ReportSortAndFilters();
    void StartSongPreview();
    SortNode *GetHighlightedNode() const;
    NodeSort *GetCurrentSort() const;
    void PushSonglistToScreen();
    void SelectHighlightedNode(LocalBandUser *);
    void SelectNode(SortNode *, LocalBandUser *, bool);
    void PlaySetlist(bool);
    void MakeSureSetlistIsValid();
    void PushSetlistSaveDialog();
    void SendSetlistToMetaPerformer();
    void PlaySetlist(SavedSetlist *);
    void PushMissingSetlistSongsToScreen(int);
    void SkipToShortcut(int);
    void SkipToNextShortcut(bool);
    void SetHighlightIx(int, bool);
    void ClientSetPartyShuffleMode();
    void ShuffleSetlist();
    void BuildPartySetlist();
    bool IsSongAllowedInSetlist(int, bool);
    bool IsIxActive(int);
    void SetSavedSetlistHighlight(SavedSetlist *);
    void ReSort(SongSortType);
    void ReSort(Symbol);
    void RebuildAndSortSetlists();
    SongSortType GetCurrentSortType(bool);
    void SwitchOffRankedSort();
    void SetlistArtFinished();
    void SendMessageToSongSelectPanel(Message &);
    void PushMakingSetlistToScreen();
    void PushHeaderDataToScreen();
    bool SetlistHasSong(int);
    bool AllSetlistSongsHaveScoreType(ScoreType);
    bool FilterSetlist(WiiFriendList *, NetSavedSetlist *) const;
    void DeleteHighlightedSetlist();
    void RebuildProfileData();
    void RebuildSharedSongData();
    bool IsPurchasing() const;
    void GetStoreOffers(std::vector<StoreOffer *> &) const;
    void SetRandomSongs(int, SongSortMgr::SongFilter &, Symbol, bool, bool);
    void FakeWin(int);
    void FakeWinNode(
        SortNode *, std::vector<LocalBandUser *> &, ScoreType, Difficulty, int, short
    ) const;
    void RebuildRestrictedData();
    bool HasHeaderData() { return mHasHeaderData; }
    int HeaderCareerScore() { return mHeaderCareerScore; }
    int HeaderCareerInstrumentMask() { return mHeaderCareerInstrumentMask; }
    int HeaderCareerStars() { return mHeaderCareerStars; }
    int HeaderPossibleStars() { return mHeaderPossibleStars; }

    DataNode OnGetSortList(DataArray *);
    DataNode OnMsg(const PrimaryProfileChangedMsg &);
    DataNode OnMsg(const ProfileChangedMsg &);
    DataNode OnMsg(const SigninChangedMsg &);
    DataNode OnMsg(const LocalUserLeftMsg &);
    DataNode OnMsg(const RemoteUserLeftMsg &);
    DataNode OnMsg(const AddLocalUserResultMsg &);
    DataNode OnMsg(const NewRemoteUserMsg &);
    DataNode OnMsg(const RemoteMachineUpdatedMsg &);
    DataNode OnMsg(const RemoteMachineLeftMsg &);
    DataNode OnMsg(const ServerStatusChangedMsg &);
    DataNode OnMsg(const FriendsListChangedMsg &);
    DataNode OnMsg(const UserLoginMsg &);

    static void Init(SongPreview &);

    bool unk40; // 0x40
    MusicLibraryTask mTask; // 0x44
    SongSortMgr::SongFilter mFilter; // 0x7c
    ViewSettingsProvider *mViewSettingsProvider; // 0x90
    SongPreview &mSongPreview; // 0x94
    Timer mSongPreviewTimer; // 0x98
    float mSongPreviewDelay; // 0xc8
    Symbol mLastSongPreview; // 0xcc
    int mCurrentHighlightIndex; // 0xd0
    Symbol unkd4;
    SongNodeType unkd8;
    SongSortType unkdc; // 0xdc
    SongSortType unke0;
    SongSortType unke4;
    SongSortType unke8;
    bool unkec;
    RndMat *mHeaderMat; // 0xf0
    RndMat *mSubheaderMat; // 0xf4
    RndMat *mFunctionMat; // 0xf8
    RndMat *mFunctionSetlistMat; // 0xfc
    RndMat *mRockCentralMat; // 0x100
    RndMat *mDiscMatEven; // 0x104
    RndMat *mDiscMatOdd; // 0x108
    RndMat *mDlcMatEven; // 0x10c
    RndMat *mDlcMatOdd; // 0x110
    RndMat *mStoreMatEven; // 0x114
    RndMat *mStoreMatOdd; // 0x118
    RndMat *mUgcMatEven; // 0x11c
    RndMat *mUgcMatOdd; // 0x120
    RndMat *mSetlistMatEven; // 0x124
    RndMat *mSetlistMatOdd; // 0x128
    bool unk12c;
    bool unk12d;
    std::vector<int> mSetlist; // 0x130
    SetlistProvider *mSetlistProvider; // 0x138
    SavedSetlist *mCurrentSetlist; // 0x13c
    DataResultList mResults; // 0x140
    MusicLibraryNetSetlists *mNetSetlists; // 0x158
    bool unk15c;
    SetlistScoresProvider *mSetlistScoresProvider; // 0x160
    bool mHasHeaderData; // 0x164
    int mHeaderCareerScore; // 0x168
    short mHeaderCareerInstrumentMask; // 0x16c
    int mHeaderCareerStars; // 0x170
    int mHeaderPossibleStars; // 0x174 (compiled: 0x198)
    /** Retail-only tail fields (0x19c/0x1a0), absent from the Wii dev branch.
        NOTE: retail's ctor does NOT initialize these (verified: no other
        stores to 0x19c/0x1a0 in the unit); they are set by the op-starter
        (retail fn_825276C0: unk1a0 = false; unk19c = new MusicLibraryUnkOp). */
    MusicLibraryUnkOp *unk19c; // 0x19c
    bool unk1a0; // 0x1a0
};

extern MusicLibrary *TheMusicLibrary;