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

class MusicLibraryStore; // meta_band/MusicLibraryStore.h -- the +0x19c member

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
        bool filterLocked; // 0x20
        bool allowDuplicates; // 0x21
        bool requiresStandardParts; // 0x22
        Symbol backScreen; // 0x24
        Symbol nextScreen; // 0x28
        int maxSetlistSize; // 0x2c
        Symbol partSym; // 0x30
        ScoreType scoreType; // 0x34
        Symbol titleToken; // 0x38
        Symbol makingSetlistToken; // 0x3c
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
    // retail fn_8253ACF8: `lbz r3, 0x76(r3); blr` == mTask + 0x22. That
    // member is sometimes called `requiresStandardParts`, but Tour.cpp feeds it
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
    // Retail fn_82540208 (TU5; name is ours).
    void RefreshSongLists();
    /** Retail 0x8253F050, called only by MusicLibraryStore::Poll after a purchase
        or a full disk: re-pushes the song list and, when the highlighted node is
        a store song, the highlight. Name is ours. */
    void RefreshStoreDisplay();
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

    bool unk40; // 0x50
    MusicLibraryTask mTask; // 0x54
    SongSortMgr::SongFilter mFilter; // 0x94
    ViewSettingsProvider *mViewSettingsProvider; // 0xb0
    SongPreview &mSongPreview; // 0xb4
    Timer mSongPreviewTimer; // 0xb8
    float mSongPreviewDelay; // 0xe8
    Symbol mLastSongPreview; // 0xec
    int mCurrentHighlightIndex; // 0xf0
    Symbol unkd4;
    SongNodeType unkd8;
    SongSortType unkdc; // 0xfc
    SongSortType unke0;
    SongSortType unke4;
    SongSortType unke8;
    bool unkec;
    RndMat *mHeaderMat; // 0x110
    RndMat *mSubheaderMat; // 0x114
    RndMat *mFunctionMat; // 0x118
    RndMat *mFunctionSetlistMat; // 0x11c
    RndMat *mRockCentralMat; // 0x120
    RndMat *mDiscMatEven; // 0x124
    RndMat *mDiscMatOdd; // 0x128
    RndMat *mDlcMatEven; // 0x12c
    RndMat *mDlcMatOdd; // 0x130
    RndMat *mStoreMatEven; // 0x134
    RndMat *mStoreMatOdd; // 0x138
    RndMat *mUgcMatEven; // 0x13c
    RndMat *mUgcMatOdd; // 0x140
    RndMat *mSetlistMatEven; // 0x144
    RndMat *mSetlistMatOdd; // 0x148
    bool unk12c;
    bool unk12d;
    std::vector<int> mSetlist; // 0x150
    SetlistProvider *mSetlistProvider; // 0x15c
    SavedSetlist *mCurrentSetlist; // 0x160
    DataResultList mResults; // 0x164
    MusicLibraryNetSetlists *mNetSetlists; // 0x17c
    bool unk15c;
    SetlistScoresProvider *mSetlistScoresProvider; // 0x184
    bool mHasHeaderData; // 0x188
    int mHeaderCareerScore; // 0x18c
    short mHeaderCareerInstrumentMask; // 0x190
    int mHeaderCareerStars; // 0x194
    int mHeaderPossibleStars; // 0x198 (compiled: 0x198)
    /** Retail-only tail fields (0x19c/0x1a0). Retail's ctor does NOT initialize
        these; OnLoad (0x8253ABB8) sets unk1a0 = false and
        unk19c = new MusicLibraryStore (`li r3,0x64`). */
    MusicLibraryStore *unk19c; // 0x19c
    bool unk1a0; // 0x1a0
};

extern MusicLibrary *TheMusicLibrary;