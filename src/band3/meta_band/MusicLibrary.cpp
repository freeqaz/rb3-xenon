#include "meta_band/MusicLibrary.h"
#include "BandProfile.h"
#include "MusicLibrary.h"
#include "MusicLibraryNetSetlists.h"
#include "SavedSetlist.h"
#include "SongSetlistProvider.h"
#include "SongSortMgr.h"
#include "SongSortNode.h"
#include "StoreSongSortNode.h"
#include "ViewSetting.h"
#include "bandobj/ReviewDisplay.h"
#include "bandobj/StarDisplay.h"
#include "beatmatch/TrackType.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "game/GameMode.h"
#include "game/NetGameMsgs.h"
#include "meta/Profile.h"
#include "meta/SongPreview.h"
#include "meta_band/AppLabel.h"
#include "meta_band/BandMachine.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/HeaderPerformanceProvider.h"
#include "meta_band/MetaNetMsgs.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ProfileMessages.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SaveLoadManager.h"
#include "meta_band/SavedSetlist.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/SetlistSortByLocation.h"
#include "meta_band/SongRecord.h"
#include "meta_band/SongSort.h"
#include "meta_band/SongSortMgr.h"
#include "meta_band/SongSortNode.h"
#include "meta_band/SongStatusMgr.h"
#include "meta_band/ParentalControlPanel.h"
#include "meta_band/PassiveMessenger.h"
#include "meta_band/UIEventMgr.h"
#include "meta_band/Utl.h"
#include "net/NetSession.h"
#include "net/Server.h"
#include "net/Synchronize.h"
#include "net/WiiFriendMgr.h"
#include "meta/WiiProfileMgr.h"
#include "net_band/RockCentral.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "ui/UIListCustom.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "utl/DataPointMgr.h"
#include "utl/Messages2.h"
#include "utl/Messages3.h"
#include "utl/Messages4.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

MusicLibrary *TheMusicLibrary;

class WiiFriendsProvider {
public:
    bool IsPossessiveSuffixNeeded(const char *);
    const char *GetPossessiveSuffix(const char *);
};
extern WiiFriendsProvider TheWiiFriendsProvider;

void MusicLibrary::Init(SongPreview &prev) {
    MILO_ASSERT(!TheMusicLibrary, 0x53);
    TheMusicLibrary = new MusicLibrary(prev);
}

void MusicLibrary::TryToSetHighlight(Symbol token, SongNodeType type, bool passthrough) {
    static Symbol random_song("random_song");
    static Symbol make_a_setlist("make_a_setlist");
    // Retail uses early-returns with per-block node scope (no foundNode/matched
    // bool temporaries) — the Wii oracle's flag-tracking form emits ~30 extra
    // insns and forces a larger frame + wider callee-save band.
    if (token.Str() != gNullStr) {
        SortNode *node = GetCurrentSort()->GetNode(token);
        if (node && (type == kNodeNone || node->GetType() == type)) {
            SetHighlightIx(node->mStartIx, true);
            return;
        }
    }
    if (!SongSortMgr::IsSetlistSort(unkdc)) {
        SortNode *node = GetCurrentSort()->GetNode(random_song);
        if (node && passthrough) {
            SetHighlightIx(node->mStartIx, true);
            return;
        }
    }
    if (SongSortMgr::IsSetlistSort(unkdc)) {
        SortNode *node = GetCurrentSort()->GetNode(make_a_setlist);
        if (node && passthrough) {
            SetHighlightIx(node->mStartIx, true);
            return;
        }
    }
    if (passthrough) {
        int ix = 0;
        while (!IsIxActive(ix)) {
            ix++;
        }
        SetHighlightIx(ix, true);
    }
}

void MusicLibrary::Poll() {
    if (unk12c)
        PushSetlistToScreen();
    if (unk19c) {
        unk19c->Poll();
        if (!unk1a0 && unk19c->mState == 2) {
            unk1a0 = true;
            if (unk40)
                unk19c->Finish();
        }
    }
    if (mNetSetlists)
        mNetSetlists->Poll();
    if (unke8 != 9) {
        NodeSort *sort = TheSongSortMgr->GetSort(unke8);
        sort->PollReady();
        if (sort->IsReady()) {
            SongSortType old = unke8;
            unke8 = kNumSongSortTypes;
            SetSort(old);
        }
    }
    CheckSongPreview();
}

DECOMP_FORCEACTIVE(MusicLibrary, "TheMusicLibrary")

MusicLibrary::MusicLibrary(SongPreview &prev)
    : Synchronizable("music_library"), unk40(0),
      mViewSettingsProvider(new ViewSettingsProvider()), mSongPreview(prev),
      mSongPreviewDelay(0), mLastSongPreview(gNullStr), mCurrentHighlightIndex(0),
      unkd4(gNullStr), unkd8(kNodeNone), unkdc(kSongSortBySong), unke0(kSongSortBySong),
      unke4(kSetlistSortByLocation), unke8(kNumSongSortTypes), unkec(0), mHeaderMat(0),
      mSubheaderMat(0), mFunctionMat(0), mFunctionSetlistMat(0), mRockCentralMat(0),
      mDiscMatEven(0), mDiscMatOdd(0), mDlcMatEven(0), mDlcMatOdd(0), mStoreMatEven(0),
      mStoreMatOdd(0), mUgcMatEven(0), mUgcMatOdd(0), mSetlistMatEven(0),
      mSetlistMatOdd(0), unk12c(0), unk12d(0), mSetlistProvider(new SetlistProvider()),
      mCurrentSetlist(nullptr), mNetSetlists(new MusicLibraryNetSetlists()), unk15c(0),
      mSetlistScoresProvider(new SetlistScoresProvider()), mHasHeaderData(0),
      mHeaderCareerScore(0), mHeaderCareerInstrumentMask(0), mHeaderCareerStars(0),
      mHeaderPossibleStars(0), unk19c(0), unk1a0(0) {
    SetName("music_library", ObjectDir::Main());
}

MusicLibrary::~MusicLibrary() {
    delete mSetlistProvider;
    delete mSetlistScoresProvider;
    delete mViewSettingsProvider;
    delete mNetSetlists;
}

void MusicLibrary::OnLoad() {}

void MusicLibrary::OnEnter() {
#ifdef HX_NATIVE
    /* Retail Xbox 360 OnEnter has NO SetHomeMenuEnabled call: enumerating all 62
       `bl` targets in retail fn_82542238 (0x82542238, 1792 B) and mapping them
       through target_symbol_map.json finds no
       ?SetHomeMenuEnabled@PlatformMgr@@QAAX_N@Z — while the same enumeration DOES
       find AddSink/SigninChangedMsg, so the search is not vacuous. The HOME menu
       is a Wii-only concern; keep the call for the native host only. */
    ThePlatformMgr.SetHomeMenuEnabled(false);
#endif
    unk40 = true;
    UIPanel *panel = ObjectDir::Main()->Find<UIPanel>("song_select_panel", true);
    mSongPreviewDelay = panel->TypeDef()->FindFloat("song_preview_delay");
    /* Retail builds these six Symbols as FUNCTION-LOCAL statics, not as the
       Symbols2.h file-scope globals the Wii dev branch uses. Proof: retail keeps
       ONE guard word at 0x82DFD5AC and tests/sets bits 0x01,0x02,0x04,0x08,0x10,
       0x20 around six ??0Symbol@@QAA@PBD@Z calls — the MSVC local-static guard
       shape. Declaration order below is that bit order; each Symbol's identity was
       cross-checked by reading its ctor string argument out of retail .rdata
       (0x8208F7F0="qp_party_shuffle", 0x8208E630="qp_coop", 0x82091034=
       "qp_practice", 0x8209102C="trainer", 0x82090FF8="custom_music_library_tasks",
       0x8203AB78="practice"). Placement matters: MSVC emits the guarded init at
       the point of declaration, so custom_music_library_tasks must stay INSIDE
       the WentBack block. */
    static Symbol qp_party_shuffle("qp_party_shuffle");
    static Symbol qp_coop("qp_coop");
    if (TheGameMode->InMode(qp_party_shuffle)) {
        ClearSetlist();
        TheGameMode->SetMode(qp_coop);
    }
    static Symbol qp_practice("qp_practice");
    if (TheGameMode->InMode(qp_practice)) {
        TheSessionMgr->mCritUserListener->ClearCriticalUser();
        TheGameMode->SetMode(qp_coop);
    }
    static Symbol trainer("trainer");
    if (TheGameMode->InMode(trainer)) {
        ControllerType ty =
            (ControllerType)panel->Property("trainer_from_main_menu", true)->Int();
        /* Retail: `cmpwi 4; beq out-of-line; cmpwi 3; beq out-of-line;` with the
           ClearCriticalUser/SetMode arm laid out INLINE and SetupTaskForTrainer
           out-of-line — i.e. SetupTaskForTrainer is the ELSE arm and the test is
           two equality compares (4 before 3), not the `ty - 3 <= 1U` range check
           the Wii branch used. 4/3 = kControllerRealGuitar/kControllerKeys, which
           is also the semantically right pair (Trainer is Pro Guitar + Pro Keys). */
        if (ty != kControllerRealGuitar && ty != kControllerKeys) {
            TheSessionMgr->mCritUserListener->ClearCriticalUser();
            TheGameMode->SetMode(qp_coop);
        } else {
            SetupTaskForTrainer(ty);
        }
    }
    if (!TheUI->WentBack()) {
        static Symbol custom_music_library_tasks("custom_music_library_tasks");
        if (!TheGameMode->Property(custom_music_library_tasks, true)->Int()) {
            mTask.Reset();
            mTask.SetSongFilter(mFilter);
        }
        int i5 = unkec ? unke4 : unke0;
        if (unkdc != i5) {
            unkdc = (SongSortType)i5;
            mCurrentHighlightIndex = 0;
        }
        unkec = false;
    }
    static Symbol practice("practice");
    if (TheGameMode->InMode(practice)) {
        mTask.setlistMode = kSetlistForbidden;
    }
    if (TheGameMode->InMode(qp_coop) && !TheSessionMgr->IsLocal()) {
        mTask.setlistMode = kSetlistForced;
    }
    NodeSort *sort = TheSongSortMgr->GetSort(unkdc);
    if (!sort->IsReady()) {
        unke0 = kSongSortByDiff;
        unkdc = kSongSortByDiff;
        MILO_ASSERT(TheSongSortMgr->GetSort(kSongSortByDiff)->IsReady(), 0x11E);
    }
    mViewSettingsProvider->BuildFilters(PartForFilter());
    TheSongSortMgr->BuildFilteredSongList(&mTask.filter, PartForFilter());
    TheSongSortMgr->BuildSetlistList();
    TheSongSortMgr->BuildSortTree(unkdc);
    TheSongSortMgr->BuildSortList(unkdc);
    TryToSetHighlight(unkd4, unkd8, true);
    /* Retail-only, immediately after TryToSetHighlight and before the 0x180 store:
         lbz r11, 0x1a0(this); cmplwi r11, 0; beq +; lwz r3, 0x19c(this);
         bl fn_825BCA38
       i.e. `if (unk1a0) unk19c->Unk825BCA38();`. Absent from the Wii dev branch,
       which is why the 0x19c/0x1a0 tail fields were declared but never used here. */
    if (unk1a0) {
        unk19c->Unk825BCA38();
    }
    unk15c = false;
    if (SongSortMgr::IsSetlistSort(unkdc)) {
        RefreshNetSetlists();
    }
    if (!TheUI->WentBack()) {
        if (mTask.setlistMode == 0 || mTask.setlistMode == 2) {
            SetMakingSetlist(false);
        } else
            SetMakingSetlist(true);
    }
    if (!GetMakingSetlist(false) || !TheUI->WentBack()) {
        ClearSetlist();
    }
    PushFilterToScreen();
    PushSortToScreen();
    UpdateHeaderData();
    TheProfileMgr.AddSink(this, PrimaryProfileChangedMsg::Type());
    TheProfileMgr.AddSink(this, ProfileChangedMsg::Type());
    ThePlatformMgr.AddSink(this, SigninChangedMsg::Type());
    /* Retail's sink list is exactly this one MINUS FriendsListChangedMsg and
       UserLoginMsg. Verified by enumerating all 62 `bl` targets of retail
       fn_82542238: the ten Type()/AddSink pairs present are Primary­ProfileChanged,
       ProfileChanged, SigninChanged, LocalUserLeft, RemoteUserLeft,
       AddLocalUserResult, NewRemoteUser, ServerStatusChanged,
       RemoteMachineUpdated, RemoteMachineLeft — in that order, matching ours once
       the two below are removed. Both are Wii-only: the Wii friends list, and
       TheServer = gWiiServer (also a zeroed weak stub on the native link, which is
       why UserLoginMsg was already excluded from native and now belongs nowhere). */
#ifdef HX_NATIVE
    ThePlatformMgr.AddSink(this, FriendsListChangedMsg::Type());
#endif
    TheSessionMgr->AddSink(this, LocalUserLeftMsg::Type());
    TheSessionMgr->AddSink(this, RemoteUserLeftMsg::Type());
    TheSessionMgr->AddSink(this, AddLocalUserResultMsg::Type());
    TheSessionMgr->AddSink(this, NewRemoteUserMsg::Type());
    TheRockCentral.AddSink(this, ServerStatusChangedMsg::Type());
    TheSessionMgr->GetMachineMgr()->AddSink(this, RemoteMachineUpdatedMsg::Type());
    TheSessionMgr->GetMachineMgr()->AddSink(this, RemoteMachineLeftMsg::Type());
    TheContentMgr.RegisterCallback(TheMusicLibrary, false);
}

void MusicLibrary::OnExit() {
    ClearSongPreview();
    /* Retail builds this Symbol as a FUNCTION-LOCAL static, not as the Symbols2.h
       file-scope global the Wii dev branch uses. Proof from retail bytes (dtk
       fn_82542A00 @ 0x82542A00, 748 B): immediately after the ClearSongPreview
       call it loads guard word 0x82DFD5B4, tests bit 0x1 (`clrlwi. r9,r11,31`),
       and on the cold path does `ori r11,r11,0x1; stw` then calls
       ??0Symbol@@QAA@PBD@Z (fn_827C0728) with r4 = 0x82090FF8 =
       "custom_music_library_tasks". EXACTLY ONE bit is ever tested or set on that
       word, so retail's OnExit has exactly ONE local static — corroborated
       structurally by the single 32-byte guard/atexit thunk at 0x82542CEC that
       follows the function (OnEnter, with six local statics, is followed by six).
       Note this is OnEnter's static's twin, not the same object: a function-local
       static is per-function, which is why OnEnter uses a different guard word
       (0x82DFD5AC, six bits). */
    static Symbol custom_music_library_tasks("custom_music_library_tasks");
    if (!TheGameMode->Property(custom_music_library_tasks, true)->Int()) {
        mFilter = mTask.GetFilter();
    }
    TheProfileMgr.RemoveSink(this, PrimaryProfileChangedMsg::Type());
    TheProfileMgr.RemoveSink(this, ProfileChangedMsg::Type());
    ThePlatformMgr.RemoveSink(this, SigninChangedMsg::Type());
    /* Retail's RemoveSink list is exactly this one MINUS FriendsListChangedMsg and
       UserLoginMsg — the mirror image of the OnEnter finding. Verified by resolving
       every `bl` in retail fn_82542A00 through target_symbol_map.json: there are
       exactly TEN MsgSource::RemoveSink (fn_82766970) calls, and the ten Type()
       callees are, in order, PrimaryProfileChanged (fn_8235B978), ProfileChanged
       (fn_824F6628), SigninChanged (fn_823EC538), LocalUserLeft (fn_823E0BA8),
       RemoteUserLeft (fn_823E0D28), AddLocalUserResult (fn_8253A9A0),
       NewRemoteUser (fn_823E0C28), ServerStatusChanged (fn_823EC318),
       RemoteMachineUpdated (fn_8253A8A0), RemoteMachineLeft (fn_8253A920) — which
       is our list, in our order, once the two below are gone. The receiver
       multiset agrees independently: ProfileMgr x2, PlatformMgr x1, SessionMgr x4,
       RockCentral x1, MachineMgr x2. That enumeration is NOT vacuous: 3 of the 22
       addresses looked up returned no name, so the map can and does answer "no".
       Both removed sinks are Wii-only (the Wii friends list; TheServer = gWiiServer,
       also a zeroed weak stub on the native link). */
#ifdef HX_NATIVE
    ThePlatformMgr.RemoveSink(this, FriendsListChangedMsg::Type());
#endif
    TheSessionMgr->RemoveSink(this, LocalUserLeftMsg::Type());
    TheSessionMgr->RemoveSink(this, RemoteUserLeftMsg::Type());
    TheSessionMgr->RemoveSink(this, AddLocalUserResultMsg::Type());
    TheSessionMgr->RemoveSink(this, NewRemoteUserMsg::Type());
    TheRockCentral.RemoveSink(this, ServerStatusChangedMsg::Type());
    TheSessionMgr->GetMachineMgr()->RemoveSink(this, RemoteMachineUpdatedMsg::Type());
    TheSessionMgr->GetMachineMgr()->RemoveSink(this, RemoteMachineLeftMsg::Type());
    TheContentMgr.UnregisterCallback(TheMusicLibrary, true);
    mNetSetlists->CleanUp();
    /* Retail-only, between CleanUp and the unke8 check: `lwz r3,0x19c(this);
       bl fn_825BC908`, and the map names fn_825BC908
       ?ClearPreview@MusicLibraryStore@@QAAXXZ. Unconditional here — unlike the
       OnEnter counterpart, which guards its 0x19c call on unk1a0. Absent from the
       Wii dev branch, which is why the 0x19c tail field was declared but unused.
       ⚠ NOTE FOR THE MAP LANE: the callee's real class is MusicLibraryStore, whose
       identified members cluster at 0x825BC908/0x825BC9D8/0x825BD458/0x825BD618;
       our local stub class MusicLibraryUnkOp conflates that class with a second,
       entirely UNMAPPED cluster at 0x825A3DC8-0x825A4860 (the addresses this
       header annotates for Poll/Finish/ClearPreview/SetStorePreview/ctor are all
       absent from target_symbol_map.json, and each lies strictly INSIDE an
       unrelated named function, so none can be a function start).
       UPDATE (lane CR-3): the ClearPreview annotation is now corrected to
       0x825BC908 in the header, and ClearSongPreview -- which was calling this
       same ClearPreview but should call the distinct thunk 0x825BC900 -- is fixed.
       Still NOT repaired: unk19c is declared MusicLibraryUnkOp* when its real type
       is MusicLibraryStore*. Retyping it touches ~10 call sites plus mStoreArt and
       the ctor, risks regressing a large matched TU, and pays 0 in both currencies,
       so it was left to a lane that can afford the whole-file A/B. */
    unk19c->ClearPreview();
    if (unke8 != kNumSongSortTypes) {
        TheSongSortMgr->GetSort(unke8)->CancelMakeReady();
        unke8 = kNumSongSortTypes;
    }
    unk40 = false;
    /* Retail has NO SetHomeMenuEnabled call here: fn_82542A00 ends
       `li r11,0; stb r11,0x50(this); addi r1,r31,0x90; b __restgprlr_27` — the
       store of unk40 is the last thing before the epilogue, with no further bl.
       Same Wii-only HOME-menu concern CO-2 removed from OnEnter; kept for the
       native host only. */
#ifdef HX_NATIVE
    ThePlatformMgr.SetHomeMenuEnabled(true);
#endif
}

bool MusicLibrary::IsExiting() { return false; }

void MusicLibrary::OnSynchronized(unsigned int) {
    if (unk40)
        PushSetlistToScreen();
}

void MusicLibrary::SyncSave(BinStream &bs, unsigned int) const { bs << mSetlist; }
void MusicLibrary::SyncLoad(BinStream &bs, unsigned int) {
    mSetlist.clear();
    bs >> mSetlist;
}

bool MusicLibrary::HasSyncPermission() const { return IsLeaderLocal(); }

void MusicLibrary::OnUnload() {
    mSetlistScoresProvider->Clear();
    TheSongSortMgr->ClearAllSorts();
    TheSongSortMgr->ClearInternalSetlists();
}

MusicLibrary::MusicLibraryTask::MusicLibraryTask() { Reset(); }

void MusicLibrary::MusicLibraryTask::Reset() {
    static Symbol main_hub_screen("main_hub_screen");
    static Symbol part_difficulty_screen("part_difficulty_screen");
    static Symbol music_library("music_library");
    static Symbol making_setlist("making_setlist");
    setlistMode = kSetlistOptional;
    filter.Reset();
    filterLocked = false;
    allowDuplicates = true;
    requiresStandardParts = true;
    backScreen = main_hub_screen;
    nextScreen = part_difficulty_screen;
    maxSetlistSize = 0;
    partSym = gNullStr;
    scoreType = kNumScoreTypes;
    titleToken = music_library;
    makingSetlistToken = making_setlist;
}

void MusicLibrary::MusicLibraryTask::ResetWithBackScreen(Symbol s) {
    Reset();
    backScreen = s;
}

void MusicLibrary::MusicLibraryTask::GetSongFilterAsString(String &str) {
    for (int i = 0; i < kNumFilterTypes; i++) {
        const std::set<Symbol> &filterSet = filter.GetFilterSet((FilterType)i);
        if (!filterSet.empty()) {
            str += MakeString("%i:", i);
            FOREACH (it, filterSet) {
                Symbol cur = *it;
                if (it != filterSet.begin())
                    str += ",";
                str += cur.Str();
            }
            str += ";";
        }
    }
}

void MusicLibrary::MusicLibraryTask::SetSongFilter(const SongSortMgr::SongFilter &filt) {
    filter = filt;
}

bool MusicLibrary::GetFilterLocked() { return mTask.filterLocked; }
bool MusicLibrary::GetDuplicatesAllowed() { return mTask.allowDuplicates; }
bool MusicLibrary::GetForcedSetlist() { return mTask.setlistMode == 1; }

FORCE_LOCAL_INLINE
int MusicLibrary::GetMaxSetlistSize() { return mTask.maxSetlistSize; }
END_FORCE_LOCAL_INLINE

void MusicLibrary::SetTask(MusicLibraryTask &task) { mTask = task; }
SongSortMgr::SongFilter &MusicLibrary::GetFilter() { return mTask.filter; }
bool MusicLibrary::GetAllowUGC() { return mTask.requiresStandardParts; }

void MusicLibrary::ResetFilters() {
    if (!mTask.filterLocked) {
        mTask.filter.Reset();
        TheSongSortMgr->BuildFilteredSongList(&mTask.filter, PartForFilter());
        TheSongSortMgr->BuildSortTree(unkdc);
        TheSongSortMgr->BuildSortList(unkdc);
        TryToSetHighlight(unkd4, unkd8, true);
        PushHighlightToScreen(true);
        PushFilterToScreen();
    }
}

void MusicLibrary::ResetFilter(FilterType ty) {
    if (!mTask.filterLocked && mTask.filter.HasFilterType(ty)) {
        mTask.filter.ClearFilter(ty);
        TheSongSortMgr->BuildFilteredSongList(&mTask.filter, PartForFilter());
        TheSongSortMgr->BuildSortTree(unkdc);
        TheSongSortMgr->BuildSortList(unkdc);
        TryToSetHighlight(unkd4, unkd8, true);
        PushHighlightToScreen(true);
        PushFilterToScreen();
    }
}

void MusicLibrary::ToggleFilter(FilterType ty, Symbol s) {
    if (!mTask.filterLocked) {
        if (mTask.GetFilter().HasFilter(ty, s)) {
            mTask.filter.RemoveFilter(ty, s);
        } else {
            mTask.filter.AddFilter(ty, s);
        }
        TheSongSortMgr->BuildFilteredSongList(&mTask.filter, PartForFilter());
        TheSongSortMgr->BuildSortTree(unkdc);
        TheSongSortMgr->BuildSortList(unkdc);
        TryToSetHighlight(unkd4, unkd8, true);
        PushHighlightToScreen(true);
        PushFilterToScreen();
    }
}

const char *MusicLibrary::GetStatusText() {
    // Retail interns both format tokens as function-local statics (guard word
    // 0x82DFD4B0, bits 0x1/0x2), initialised up-front before the song counts.
    static Symbol music_library_filtered_fmt("music_library_filtered_fmt");
    static Symbol music_library_unfiltered_fmt("music_library_unfiltered_fmt");
    int i4 = TheSongSortMgr->mSongs.size();
    std::vector<int> songs;
    TheSongMgr.GetRankedSongs(songs, true, true);
    int numSongs = songs.size();
    const char *txt = Localize(GetCurrentSortName(true), nullptr);
    if (i4 < numSongs) {
        return MakeString(
            Localize(music_library_filtered_fmt, nullptr), i4, numSongs, txt
        );
    } else {
        return MakeString(Localize(music_library_unfiltered_fmt, nullptr), i4, txt);
    }
}

void MusicLibrary::SetTaskScoreType(ScoreType ty) {
    mTask.scoreType = ty;
    RebuildUserConfigData();
}

void MusicLibrary::SetupTaskForTrainer(ControllerType ty) {
    mTask.Reset();
    mTask.filterLocked = true;
    mTask.setlistMode = kSetlistForbidden;
    switch (ty) {
    // Byte-neutral rename: the constants stay 3 and 2, only the names change.
    // These two sites are what proved the enum renumber semantically -- a real
    // guitar controller filtering on 3 and a keys controller on 2.
    case kControllerRealGuitar:
        mTask.filter.AddFilter(kFilterProGuitar, has_part_yes);
        break;
    case kControllerKeys:
        mTask.filter.AddFilter(kFilterKeys, has_part_yes);
        break;
    default:
        MILO_FAIL("Bad ControllerType %i in MusicLibrary::SetupTaskForTrainer!", ty);
        break;
    }
}

void MusicLibrary::ReportSortAndFilters() {
    static Symbol mode("mode");
    static Symbol sort("sort");
    static Symbol filters("filters");
    String str;
    mTask.GetSongFilterAsString(str);
    Symbol curMode = TheGameMode->mMode;
    SendDataPoint(
        "music_library/sort_and_filters",
        mode,
        curMode,
        sort,
        GetCurrentSortName(true),
        filters,
        str.c_str()
    );
}

void MusicLibrary::ClearSongPreview() {
    mLastSongPreview = gNullStr;
    mSongPreviewTimer.Reset();
    // Retail calls the single-arg Start(Symbol) (fn_827808B0), then tail-calls
    // 0x825BC900 on unk19c.
    mSongPreview.Start(gNullStr);
    /* ⚠ NOT ClearPreview -- this was a 100/100 function calling the WRONG callee
       (lane CR-3). Retail ClearSongPreview (0x8253AD00) ends `lwz r3,0x19c(r31);
       bl 0x825BC900`, whereas OnExit ends `lwz r3,0x19c(this); bl 0x825BC908`.
       They are two DIFFERENT functions eight bytes apart: 0x825BC908 is
       ?ClearPreview@MusicLibraryStore@@QAAXXZ (208 B, own .pdata), while
       0x825BC900 is a 2-instruction thunk `mPreviewMgr->ClearCurrentPreview()`.
       The prior "retail fn_825A3DC8" annotation here was wrong on both counts.
       Invisible to the default ruler (relocation args are masked), so this fix is
       worth exactly 0 matched functions and 0 bytes -- it is a correctness repair,
       and a metric that hides a wrong callee is worse than a lower metric. */
    unk19c->Unk825BC900();
}

void MusicLibrary::StartSongPreview() {
    if (TheMusicLibrary->GetHighlightedNode()->GetToken() != mLastSongPreview) {
        ClearSongPreview();
        mSongPreviewTimer.Start();
    }
}

void MusicLibrary::CheckSongPreview() {
    if (mSongPreviewTimer.Running()
        && mSongPreviewTimer.SplitMs() > mSongPreviewDelay * 1000.0f) {
        SortNode *node = TheMusicLibrary->GetHighlightedNode();
        if (node->GetType() == kNodeSubheader) {
            SubheaderSortNode *ssn = dynamic_cast<SubheaderSortNode *>(node);
            MILO_ASSERT(ssn, 0x2BE);
            node = ssn->GetFirstChildSong();
            MILO_ASSERT(node, 0x2C0);
        }
        mSongPreviewTimer.Stop();
        mLastSongPreview = node->GetToken();
        if (node->GetType() == kNodeSong) {
            SongSortNode *sort = dynamic_cast<SongSortNode *>(node);
            if (sort) {
                LocalBandMachine *machine =
                    TheSessionMgr->GetMachineMgr()->GetLocalMachine();
                const char *song = sort->GetTitle();
                machine->SetCurrentSongPreview(song);
            }
            Symbol token = node->GetToken();
            if (TheSongMgr.HasSong(token, true)
                && !TheSongMgr.IsRestricted(TheSongMgr.GetSongIDFromShortName(token, true)
                )) {
                mSongPreview.Start(token);
            }
        } else if (node->GetType() == kNodeStoreSong) {
            StoreSongSortNode *sn = dynamic_cast<StoreSongSortNode *>(node);
            unk19c->SetStorePreview(sn->mOffer->GetSingleSongID());
        }
    }
}

void MusicLibrary::ContentStarted() { ClearSongPreview(); }
void MusicLibrary::ContentMounted(const char *contentName, const char *) {
    if (!TheContentMgr.RefreshInProgress()) {
        SortNode *node = GetHighlightedNode();
        if (node->GetType() == kNodeSubheader) {
            node = dynamic_cast<SubheaderSortNode *>(node)->GetFirstChildSong();
        }
        OwnedSongSortNode *owned = dynamic_cast<OwnedSongSortNode *>(node);
        if (owned) {
            int songID = owned->GetSongRecord()->Data()->ID();
            if (TheSongMgr.IsContentUsedForSong(contentName, songID)) {
                if (static_cast<BandSongMetadata *>(TheSongMgr.Data(songID))->IsUGC()) {
                    if (TheSongSortMgr->GetRecord(songID)->UpdateDemo()) {
                        PushSonglistToScreen();
                    }
                    PushHighlightToScreen(false);
                } else {
                    static Symbol song_data_mounted("song_data_mounted");
                    static Message msg(song_data_mounted);
                    // NOT HandleType(msg): Object::HandleType is defined
                    // out-of-line (Object.h declares it, Object.cpp defines it),
                    // so it emits a `bl` -- retail instead dispatches the virtual
                    // Handle directly through the Hmx::Object virtual base
                    // (vbptr @+8, vtable slot 0x18).
                    TheUI->Handle(msg, false);
                }
            }
        }
    }
}

void MusicLibrary::ContentDone() {
    mViewSettingsProvider->BuildFilters(PartForFilter());
    TheSongSortMgr->BuildFilteredSongList(&mTask.filter, PartForFilter());
    TheSongSortMgr->BuildSortTree(unkdc);
    TheSongSortMgr->BuildSortList(unkdc);
    TryToSetHighlight(unkd4, kNodeNone, true);
    PushSetlistToScreen();
    PushSonglistToScreen();
    PushHighlightToScreen(true);
    PushFilterToScreen();
}

void MusicLibrary::SetHighlightIx(int idx, bool b) {
    mCurrentHighlightIndex = idx;
    SortNode *node = GetCurrentSort()->GetNode(mCurrentHighlightIndex);
    SongNodeType ty = node->GetType();
    switch (ty) {
    case kNodeHeader:
    case kNodeFunction: {
        ClearSongPreview();
        LocalBandMachine *machine = TheSessionMgr->GetMachineMgr()->GetLocalMachine();
        machine->SetCurrentSongPreview(gNullStr);
        break;
    }
    case kNodeSubheader:
    case kNodeSong:
    case kNodeStoreSong:
        if (TheUI->GetTransitionState() != UIManager::kTransitionTo) {
            StartSongPreview();
        }
        break;
    default:
        break;
    }
    unkd4 = node->GetToken();
    unkd8 = node->GetType();
    PushHighlightToScreen(b);
}

void MusicLibrary::SelectHighlightedNode(LocalBandUser *user) {
    SelectNode(GetHighlightedNode(), user, false);
}

void MusicLibrary::PlaySetlist(bool b1) {
#ifdef HX_NATIVE
    if (getenv("GAME_DBG"))
        MILO_LOG("GAME_DBG: PlaySetlist(b1=%d) contentDir=%p setlistSize=%d making=%d "
                 "local=%d sameNetUI=%d primaryProf=%p\n",
                 b1, (void *)ContentDir(), (int)mSetlist.size(), GetMakingSetlist(false),
                 TheSessionMgr->IsLocal(),
                 TheSessionMgr->GetMachineMgr()->AllMachinesHaveSameNetUIState(),
                 (void *)TheProfileMgr.GetPrimaryProfile());
#endif
    // Retail 360 guards on HasSyncPermission() (Synchronizable slot 3, this+0x30),
    // NOT ContentDir() as the rb3-Wii dev oracle does — verified from the retail
    // vtable group: PlaySetlist loads [this+0x30]+0xc (Sync vtable slot 3, bool
    // return). There is NO base-layout delta here (Callback@0x2c, Sync@0x30 in
    // both builds); the prior "+4 wall" was ContentDir(Callback) misread as the
    // guard. See PlaySetlist decomp notes.
    if (HasSyncPermission()) {
        MakeSureSetlistIsValid();
        if (mSetlist.size() != 0) {
            // Retail uses a function-local static Symbol here (guard bit + Symbol
            // ctor emitted inline), not the extern global from Symbols.h.
            static Symbol setlists_can_be_saved("setlists_can_be_saved");
            SetSyncDirty(-1, true);
            if (b1 && GetMakingSetlist(false) && TheSessionMgr->IsLocal()
                && TheGameMode->Property(setlists_can_be_saved, true)->Int()
                && TheProfileMgr.GetPrimaryProfile()) {
                PushSetlistSaveDialog();
            } else if (TheSessionMgr->GetMachineMgr()->AllMachinesHaveSameNetUIState()) {
                SendSetlistToMetaPerformer();
                UIPanel *panel =
                    ObjectDir::Main()->Find<UIPanel>("song_select_panel", true);
#ifdef HX_NATIVE
                if (getenv("GAME_DBG"))
                    MILO_LOG("GAME_DBG: PlaySetlist -> SendSetlistToMetaPerformer + "
                             "song_select_panel move_on_quickplay\n");
#endif
                static Message move_on_quickplay("move_on_quickplay");
                panel->HandleType(move_on_quickplay);
            } else {
#ifdef HX_NATIVE
                if (getenv("GAME_DBG"))
                    MILO_LOG("GAME_DBG: PlaySetlist -> remote_not_ready (AllMachinesHaveSameNetUIState=0)\n");
#endif
                TheUIEventMgr->TriggerEvent("remote_not_ready", nullptr);
            }
        }
    }
}

void MusicLibrary::PlaySetlist(SavedSetlist *setlist) {
    std::vector<int> &songs = setlist->mSongs;
    int numSongs = 0;
    FOREACH (it, songs) {
        if (TheSongMgr.HasSong(*it) && TheSongMgr.IsRestricted(*it)) {
            numSongs++;
        }
    }
    if (numSongs != 0) {
        UIScreen *screen =
            ObjectDir::Main()->Find<UIScreen>("setlist_content_restricted_screen", true);
        TheUI->PushScreen(screen);
    } else {
        numSongs = 0;
        FOREACH (it, songs) {
            if (!TheSongMgr.HasSong(*it) || TheSongMgr.IsDemo(*it)
                || !TheSessionMgr->GetMachineMgr()->IsSongShared(*it)) {
                numSongs++;
            }
        }
        if (numSongs != 0)
            PushMissingSetlistSongsToScreen(numSongs);
        else if (TheSessionMgr->GetMachineMgr()->AllMachinesHaveSameNetUIState()) {
            if (setlist->IsBattle()) {
                BattleSavedSetlist *bss = dynamic_cast<BattleSavedSetlist *>(setlist);
                MILO_ASSERT(bss, 0x38F);
                MetaPerformer::Current()->SetBattle(bss);
            } else {
                MetaPerformer::Current()->SetSetlist(setlist);
            }
            UIPanel *panel = ObjectDir::Main()->Find<UIPanel>("song_select_panel", true);
            static Message msg("move_on_quickplay");
            panel->HandleType(msg);
        } else {
            TheUIEventMgr->TriggerEvent("remote_not_ready", nullptr);
        }
    }
}

void MusicLibrary::SkipToNextShortcut(bool forward) {
    NodeSort *sort = TheSongSortMgr->GetSort(unkdc);
    SortNode *node = sort->GetNode(mCurrentHighlightIndex);
    int shortcutIx = TheSongSortMgr->GetSort(unkdc)->GetShortcutIx(node);
    int numData = TheSongSortMgr->GetSort(unkdc)->NumData();
    if (forward) {
        shortcutIx++;
        if (numData == 0) {
            shortcutIx = 0;
        } else {
            shortcutIx = shortcutIx % numData;
            if (shortcutIx < 0) {
                shortcutIx += numData;
            }
        }
    } else {
        int startIx = node->mStartIx;
        NodeSort *sort2 = TheSongSortMgr->GetSort(unkdc);
        if (startIx == sort2->FirstActiveIxForShortcut(shortcutIx)) {
            shortcutIx--;
            if (numData == 0) {
                shortcutIx = 0;
            } else {
                shortcutIx = shortcutIx % numData;
                if (shortcutIx < 0) {
                    shortcutIx += numData;
                }
            }
        }
    }
    while (!TheSongSortMgr->GetSort(unkdc)->IsActive(shortcutIx)) {
        int delta = forward ? 1 : -1;
        shortcutIx += delta;
        if (numData == 0) {
            shortcutIx = 0;
        } else {
            shortcutIx = shortcutIx % numData;
            if (shortcutIx < 0) {
                shortcutIx += numData;
            }
        }
    }
    SkipToShortcut(shortcutIx);
}

void MusicLibrary::SkipToShortcut(int idx) {
    SetHighlightIx(GetCurrentSort()->FirstActiveIxForShortcut(idx), true);
}

void MusicLibrary::ClientSetPartyShuffleMode() {
    if (!IsLeaderLocal()) {
        // Retail emits a function-local static Symbol here (guard 0x82DFD3C8,
        // storage 0x82DFD3C4), not the global symbol-table entry.
        static Symbol qp_party_shuffle("qp_party_shuffle");
        TheGameMode->SetMode(qp_party_shuffle);
    }
}

void MusicLibrary::SelectNode(SortNode *node, LocalBandUser *user, bool b3) {
    static Symbol make_a_setlist("make_a_setlist");
    static Symbol view_setlists("view_setlists");
    static Symbol view_songs("view_songs");
    static Symbol random_song("random_song");
    static Symbol shuffle_setlist("shuffle_setlist");
    static Symbol play_setlist("play_setlist");
    static Symbol party_setlist("party_setlist");
    switch (node->GetType()) {
    case kNodeFunction:
        if (node->GetToken() == shuffle_setlist) {
            ShuffleSetlist();
            unk12c = true;
        } else if (node->GetToken() == make_a_setlist) {
            ClearSetlist();
            SetMakingSetlist(true);
            SetSort(unke0);
        } else if (node->GetToken() == view_setlists) {
            SetSort(unke4);
        } else if (node->GetToken() == view_songs) {
            SetSort(unke0);
        } else if (node->GetToken() == party_setlist) {
            if (IsLeaderLocal()) {
                BuildPartySetlist();
                static Symbol qp_party_shuffle("qp_party_shuffle");
                if (mSetlist.size() != 0) {
                    TheGameMode->SetMode(qp_party_shuffle);
                    if (TheNetSession) {
                        SetPartyShuffleModeMsg msg;
                        TheNetSession->SendMsgToAll(msg, kReliable);
                    }
                }
                PlaySetlist(true);
            } else if (!b3) {
                UIScreen *screen =
                    ObjectDir::Main()->Find<UIScreen>("leader_party_shuffle_warning_screen", true);
                TheUI->PushScreen(screen);
            }
        } else if (node->GetToken() == play_setlist) {
            if (GetMaxSetlistSize() == 0 || GetMaxSetlistSize() == SetlistSize()) {
                PlaySetlist(true);
            }
        } else if (node->GetToken() == random_song) {
            std::vector<Symbol> symvec;
            FOREACH (it, mSetlist) {
                symvec.push_back(TheSongMgr.GetShortNameFromSongID(*it, true));
            }
            std::vector<Symbol> s30;
            if (TheSongSortMgr->GetRandomSongs(
                    1, &s30, nullptr, &symvec, nullptr, true, true
                )) {
                Symbol firstSong = s30.front();
                SelectNode(GetCurrentSort()->GetNode(firstSong), user, false);
                SetSyncDirty(-1, true);
            } else if (!b3) {
                UIScreen *screen =
                    ObjectDir::Main()->Find<UIScreen>("no_valid_songs_screen", true);
                TheUI->PushScreen(screen);
            }
        }
        break;
    case kNodeHeader:
    case kNodeSubheader:
        if (SetlistIsFull()) {
            if (!b3) {
                TryToSetHighlight(play_setlist, kNodeFunction, false);
                UIScreen *screen =
                    ObjectDir::Main()->Find<UIScreen>("full_setlist_screen", true);
                TheUI->PushScreen(screen);
            }
        } else if (CanHeadersBeSelected() && node->GetSongCount()) {
            bool makeSetlist = !GetMakingSetlist(false);
            if (makeSetlist) {
                SetMakingSetlist(true);
            }
            FOREACH (it, node->mChildren) {
                SortNode *child = *it;
                if (child->GetType() != kNodeStoreSong) {
                    SelectNode(child, user, true);
                }
            }
            if (makeSetlist) {
                if (mSetlist.size() != 0) {
                    PushSetlistToScreen();
                    PlaySetlist(false);
                } else {
                    SetMakingSetlist(false);
                }
            } else if (SetlistIsFull()) {
                TryToSetHighlight(play_setlist, kNodeFunction, false);
            }
        }
        break;
    case kNodeSong: {
        OwnedSongSortNode *songNode = dynamic_cast<OwnedSongSortNode *>(node);
        MILO_ASSERT(songNode, 0x456);
        int songID = songNode->GetSongRecord()->Data()->ID();
        if (SetlistIsFull()) {
            if (!b3) {
                TryToSetHighlight(play_setlist, kNodeFunction, false);
                UIScreen *screen =
                    ObjectDir::Main()->Find<UIScreen>("full_setlist_screen", true);
                TheUI->PushScreen(screen);
            }
        } else {
            if (songNode->GetSongRecord()->GetRestricted() && !b3) {
                ParentalControlPanel *panel = ObjectDir::Main()->Find<ParentalControlPanel>("parental_control_panel", true);
                panel->mUser = user;
                UIScreen *screen =
                    ObjectDir::Main()->Find<UIScreen>("parental_control_screen", true);
                TheUI->PushScreen(screen);
            } else if (GetAllowUGC() || !songNode->GetSongRecord()->Data()->IsUGC()) {
                if (IsSongAllowedInSetlist(songID, b3)) {
                    if (!songNode->IsEnabled())
                        break;
                    AppendToSetlist(songID);
                    bool wasMaking = GetMakingSetlist(false);
                    if (!wasMaking) {
                        PlaySetlist(true);
                    } else {
                        if (SetlistIsFull()) {
                            TryToSetHighlight(play_setlist, kNodeFunction, false);
                        }
                    }
                }
            } else if (!b3) {
                UIScreen *screen =
                    ObjectDir::Main()->Find<UIScreen>("ugc_not_allowed_screen", true);
                TheUI->PushScreen(screen);
            }
        }
        break;
    }
    case kNodeSetlist: {
        if (IsLeaderLocal()) {
            SetlistSortNode *setlistNode = dynamic_cast<SetlistSortNode *>(node);
            MILO_ASSERT(setlistNode, 0x48e);
            mCurrentSetlist = setlistNode->GetSetlistRecord()->GetSetlist();
            PlaySetlist(mCurrentSetlist);
        } else if (!b3) {
            UIScreen *screen =
                ObjectDir::Main()->Find<UIScreen>("leader_setlist_warning_screen", true);
            TheUI->PushScreen(screen);
        }
        break;
    }
    case kNodeStoreSong: {
        StoreSongSortNode *sn = dynamic_cast<StoreSongSortNode *>(node);
        int songID = sn->mOffer->GetSingleSongID();
        if (!unk19c->IsDownloading(songID)) {
            std::vector<int> songIDs;
            songIDs.push_back(songID);
            unk19c->Unk825BD8C8(user, songIDs);
        }
        break;
    }
    default:
        break;
    }
}

bool MusicLibrary::IsSongAllowedInSetlist(int songID, bool b3) {
    char _slotpad[96]; (void)_slotpad;
    SongMetadata *data = TheSongMgr.Data(songID);
    MILO_ASSERT(data, 0x4a2);
    if (data->IsVersionOK() == 0) {
        if (!b3) {
            UIScreen *screen =
                ObjectDir::Main()->Find<UIScreen>("invalid_version_screen", true);
            TheUI->PushScreen(screen);
        }
        return false;
    }
    if (TheSongMgr.IsDemo(data->ID())) {
        if (!TheGameMode->Property(Symbol("demos_allowed"), true)->Int(nullptr)) {
            if (!b3) {
                UIScreen *screen =
                    ObjectDir::Main()->Find<UIScreen>("demo_mode_screen", true);
                TheUI->PushScreen(screen);
            }
            return false;
        }
    }
    if (TheSongMgr.IsDemo(data->ID()) && !TheSessionMgr->IsLocal()) {
        if (!b3) {
            UIScreen *screen =
                ObjectDir::Main()->Find<UIScreen>("demo_online_screen", true);
            TheUI->PushScreen(screen);
        }
        return false;
    }
    if (TheSongMgr.IsDemo(data->ID())) {
        if (GetMakingSetlist(false)) {
            if (!b3) {
                UIScreen *screen =
                    ObjectDir::Main()->Find<UIScreen>("demo_setlist_screen", true);
                TheUI->PushScreen(screen);
            }
            return false;
        }
    }
    if (TheSongMgr.IsRestricted(data->ID())) {
        if (!b3) {
            UIScreen *screen =
                ObjectDir::Main()->Find<UIScreen>("content_restricted_screen", true);
            TheUI->PushScreen(screen);
        }
        return false;
    }
    if (!TheSessionMgr->GetMachineMgr()->IsSongShared(songID)) {
        if (!b3) {
            UIScreen *screen =
                ObjectDir::Main()->Find<UIScreen>("invalid_selection_screen", true);
            TheUI->PushScreen(screen);
        }
        return false;
    }
    return true;
}

void MusicLibrary::MakeSureSetlistIsValid() {
    int numRemoved = 0;
    for (std::vector<int>::iterator it = mSetlist.begin(); it != mSetlist.end(); ) {
        int songID = *it;
        if (!TheSongMgr.HasSong(songID) || !IsSongAllowedInSetlist(songID, true)) {
            it = mSetlist.erase(it);
            numRemoved++;
        } else {
            ++it;
        }
    }
    if (numRemoved != 0) {
        PushSetlistToScreen();
        ThePassiveMessenger->TriggerSetlistSongsRemovedMsg(numRemoved);
        SetSyncDirty(-1, true);
    }
}

DECOMP_FORCEACTIVE(
    MusicLibrary,
    "parental_control_panel",
    "parental_control_screen",
    "setlistNode",
    "leader_setlist_warning_screen",
    "data"
)

bool MusicLibrary::IsIxActive(int ix) {
    MILO_ASSERT(ix >= 0 && ix < GetCurrentSort()->GetDataCount(), 0x551);
    return GetCurrentSort()->GetNode(ix)->IsActive();
}

FORCE_LOCAL_INLINE
bool MusicLibrary::CanHeadersBeSelected() {
    return mTask.setlistMode == 0 && !SongSortMgr::IsSetlistSort(unkdc);
}
END_FORCE_LOCAL_INLINE

void MusicLibrary::SetSavedSetlistHighlight(SavedSetlist *setlist) {
    unkd4 = setlist->GetIdentifyingToken();
    unkd8 = kNodeSetlist;
}

FORCE_LOCAL_INLINE
SortNode *MusicLibrary::GetHighlightedNode() const {
    return GetCurrentSort()->GetNode(mCurrentHighlightIndex);
}
END_FORCE_LOCAL_INLINE

FORCE_LOCAL_INLINE
NodeSort *MusicLibrary::GetCurrentSort() const { return TheSongSortMgr->GetSort(unkdc); }
END_FORCE_LOCAL_INLINE

void MusicLibrary::SetSort(SongSortType ty) {
    if (ty != unkdc) {
        if (unke8 != kNumSongSortTypes) {
            TheSongSortMgr->GetSort(unke8)->CancelMakeReady();
            unke8 = kNumSongSortTypes;
        }
        if (!TheSongSortMgr->GetSort(ty)->IsReady()) {
            unke8 = ty;
            TheSongSortMgr->GetSort(ty)->MakeReady();
        } else {
            if (SongSortMgr::IsSetlistSort(ty)) {
                unke4 = ty;
            } else
                unke0 = ty;
            TheSongSortMgr->BuildSortTree(ty);
            TheSongSortMgr->BuildSortList(ty);
            unkdc = ty;
            TryToSetHighlight(unkd4, unkd8, true);
            PushHighlightToScreen(true);
        }
        PushSortToScreen();
    }
    if (SongSortMgr::IsSetlistSort(unkdc) && !unk15c) {
        RefreshNetSetlists();
    }
}

void MusicLibrary::ReSort(SongSortType ty) {
    if (ty == unkdc) {
        TheSongSortMgr->BuildSortTree(ty);
        TheSongSortMgr->BuildSortList(ty);
        TryToSetHighlight(unkd4, unkd8, true);
        PushHighlightToScreen(true);
    }
}

void MusicLibrary::ReSort(Symbol s) {
    SongSortType theType = kNumSongSortTypes;
    for (int i = 0; i < 9; i++) {
        if (s == TheSongSortMgr->GetSort((SongSortType)i)->GetName()) {
            theType = (SongSortType)i;
            break;
        }
    }
    if (theType == kNumSongSortTypes) {
        MILO_WARN(
            "Failed to find a sort for the symbol %s, refreshing current sort instead\n",
            s
        );
        theType = unkdc;
    }
    ReSort(theType);
}

void MusicLibrary::RebuildAndSortSetlists() {
    TheSongSortMgr->BuildSetlistList();
    for (int i = 0; i < 9; i++) {
        if (SongSortMgr::IsSetlistSort((SongSortType)i)) {
            TheSongSortMgr->BuildSortTree((SongSortType)i);
            TheSongSortMgr->BuildSortList((SongSortType)i);
        }
    }
    if (SongSortMgr::IsSetlistSort(unkdc)) {
        TryToSetHighlight(unkd4, unkd8, true);
        PushSonglistToScreen();
        PushHighlightToScreen(true);
    }
}

SongSortType MusicLibrary::GetCurrentSortType(bool b1) {
    if (b1 && unke8 != kNumSongSortTypes)
        return unke8;
    else
        return unkdc;
}

Symbol MusicLibrary::GetCurrentSortName(bool b1) {
    return TheSongSortMgr->GetSort(GetCurrentSortType(b1))->GetName();
}

void MusicLibrary::SwitchOffRankedSort() {
    if (unkdc == kSongSortByRank) {
        SetSort(kSongSortBySong);
        TheSongSortMgr->GetSort(kSongSortByRank)->CancelMakeReady();
    } else if (unke8 == kSongSortByRank) {
        TheSongSortMgr->GetSort(kSongSortByRank)->CancelMakeReady();
        unke8 = kNumSongSortTypes;
    } else if (TheSongSortMgr->GetSort(kSongSortByRank)->IsReady()) {
        TheSongSortMgr->GetSort(kSongSortByRank)->CancelMakeReady();
    }
}

DataNode MusicLibrary::OnGetSortList(DataArray *a) {
    DataArrayPtr ptr;
    int idx = 0;
    ptr->Resize(9);
    for (int i = 0; i < 9; i++) {
        if (TheSongSortMgr->IsValidNextSortTransition((SongSortType)i, unkdc)) {
            ptr->Node(idx) = TheSongSortMgr->GetSort((SongSortType)i)->GetName();
            idx++;
        }
    }
    ptr->Resize(idx);
    return ptr;
}

void MusicLibrary::InitData(RndDir *dir) {
    mHeaderMat = dir->Find<RndMat>("header.mat", false);
    mSubheaderMat = dir->Find<RndMat>("subheader.mat", false);
    mFunctionMat = dir->Find<RndMat>("function.mat", false);
    mFunctionSetlistMat = dir->Find<RndMat>("function_setlist.mat", false);
    mRockCentralMat = dir->Find<RndMat>("rockcentral.mat", false);
    mDiscMatEven = dir->Find<RndMat>("song_disc_dark.mat", false);
    mDiscMatOdd = dir->Find<RndMat>("song_disc_light.mat", false);
    mDlcMatEven = dir->Find<RndMat>("song_dlc_dark.mat", false);
    mDlcMatOdd = dir->Find<RndMat>("song_dlc_light.mat", false);
    mStoreMatEven = dir->Find<RndMat>("song_store_dark.mat", false);
    mStoreMatOdd = dir->Find<RndMat>("song_store_light.mat", false);
    mUgcMatEven = dir->Find<RndMat>("song_ugc_dark.mat", false);
    mUgcMatOdd = dir->Find<RndMat>("song_ugc_light.mat", false);
    mSetlistMatEven = dir->Find<RndMat>("setlist_dark.mat", false);
    mSetlistMatOdd = dir->Find<RndMat>("setlist_light.mat", false);
}

void MusicLibrary::Text(int, int idx, UIListLabel *slot, UILabel *label) const {
    AppLabel *p9_label = dynamic_cast<AppLabel *>(label);
#ifdef HX_NATIVE
    // Reset the slot's label to empty before the type-specific code below writes
    // (only) the slot(s) that apply to this node type. UIListDir::FillElement
    // re-fills every visible row's slots from a small recycled element pool as
    // the list scrolls, and this override (unlike the base UIListProvider::Text
    // it replaces) never clears slots it doesn't write. On the Wii layout that
    // was fine — unused slots were authored hidden — but the 360-ARK
    // song_select.milo this port loads draws them, so a row widget that last
    // showed a header keeps its stale group-letter + "N SONGS" count when reused
    // for a song. That is the "songs render as group headers / overlapping text"
    // artifact (a song row gets a leftover "M"/"3 SONGS" laid over its title).
    // Clearing here restores the base provider's clear-then-write contract.
    label->SetTextToken(gNullStr);

    // The 360-ARK song_select.milo authors several list slots as plain UILabels
    // rather than the AppLabel the RB3-Wii code expects; the cast then yields
    // null. The Wii path expects AppLabel and asserts. On native/web we fall
    // back to writing the underlying text via the base UILabel API so song
    // titles, headers, and setlist names render instead of being silently
    // dropped (W6 V1). The fallback loses some localized formatting (e.g.
    // "<title> by <artist>") but the alternative is empty rows.
    if (!p9_label) {
        SortNode *sortNode = GetCurrentSort()->GetNode(idx);
        switch (sortNode->GetType()) {
        case kNodeHeader: {
            HeaderSortNode *hsn = dynamic_cast<HeaderSortNode *>(sortNode);
            if (hsn->mCover) {
                if (slot->Matches("famousby")) {
                    label->SetTextToken(store_famous_by);
                } else if (slot->Matches("famousby_group")) {
                    label->SetTextToken(hsn->GetToken());
                }
            } else if (slot->Matches("group") && unkdc != 3 && unkdc != 7) {
                label->SetTextToken(hsn->GetToken());
            } else if (slot->Matches("song_count")
                       && !SongSortMgr::IsSetlistSort(unkdc)) {
                label->SetInt(hsn->GetSongCount(), true);
            }
            break;
        }
        case kNodeSubheader: {
            SubheaderSortNode *subheaderNode =
                dynamic_cast<SubheaderSortNode *>(sortNode);
            if (slot->Matches("song_count")
                && !SongSortMgr::IsSetlistSort(unkdc)) {
                label->SetInt(subheaderNode->GetSongCount(), true);
            } else if (slot->Matches("subgroup")) {
                label->SetTextToken(subheaderNode->GetToken());
            }
            break;
        }
        case kNodeSong: {
            OwnedSongSortNode *osn = dynamic_cast<OwnedSongSortNode *>(sortNode);
            if (slot->Matches("song")) {
                const char *title = osn->GetTitle();
                const char *artist = osn->GetArtist();
                if (unkdc != 1 && artist && *artist) {
                    label->SetDisplayText(
                        MakeString("%s - %s", title, artist), true
                    );
                } else {
                    label->SetDisplayText(title, true);
                }
            } else if (slot->Matches("difficulty")) {
                SongRecord *record = osn->GetSongRecord();
                if (record->IsNotBand() && record->GetScore() > 0) {
                    label->SetTextToken(record->GetShortDifficultySym());
                }
            } else if (slot->Matches("percentage")) {
                SongRecord *record = osn->GetSongRecord();
                if (record->IsNotBand() && record->GetScore() > 0) {
                    label->SetTokenFmt(
                        endgame_player_noteshit_fmt, record->GetNotesPct()
                    );
                }
            }
            break;
        }
        case kNodeFunction: {
            FunctionSortNode *fsn = dynamic_cast<FunctionSortNode *>(sortNode);
            if (slot->Matches("function") && fsn) {
                label->SetTextToken(fsn->GetToken());
            }
            break;
        }
        case kNodeSetlist: {
            SetlistSortNode *ssn = dynamic_cast<SetlistSortNode *>(sortNode);
            SavedSetlist *setlist = ssn->GetSetlistRecord()->GetSetlist();
            if (slot->Matches("setlist_name")) {
                const char *name = setlist->GetTitle();
                label->SetDisplayText(name ? name : gNullStr, true);
            }
            break;
        }
        default:
            label->SetTextToken(gNullStr);
            break;
        }
        return;
    }
#else
    MILO_ASSERT(p9_label, 0x638);
#endif
    SortNode *sortNode = GetCurrentSort()->GetNode(idx);
    switch (sortNode->GetType()) {
    case kNodeHeader: {
        HeaderSortNode *hsn = dynamic_cast<HeaderSortNode *>(sortNode);
        if (hsn->mCover) {
            if (slot->Matches("famousby")) {
                label->SetTextToken(store_famous_by);
            } else if (slot->Matches("famousby_group")) {
                p9_label->SetFromSongSelectNode(sortNode);
            }
        } else if (slot->Matches("group") && unkdc != 3 && unkdc != 7) {
            p9_label->SetFromSongSelectNode(sortNode);
        } else if (slot->Matches("song_count") && !SongSortMgr::IsSetlistSort(unkdc)) {
            p9_label->SetSongCount(hsn->GetSongCount());
        }
        break;
    }
    case kNodeSubheader: {
        SubheaderSortNode *subheaderNode = dynamic_cast<SubheaderSortNode *>(sortNode);
        if (slot->Matches("song_count") && !SongSortMgr::IsSetlistSort(unkdc)) {
            p9_label->SetSongCount(subheaderNode->GetSongCount());
        } else if (slot->Matches("subgroup")) {
            MILO_ASSERT(!subheaderNode->mCover, 0x671);
            p9_label->SetFromSongSelectNode(sortNode);
        }
        break;
    }
    case kNodeSong: {
        OwnedSongSortNode *osn = dynamic_cast<OwnedSongSortNode *>(sortNode);
        if (slot->Matches("song")) {
            if (unkdc != 1) {
                p9_label->SetSongAndArtistName(osn);
            } else
                p9_label->SetSongName(osn);
        } else if (slot->Matches("difficulty")) {
            SongRecord *record = osn->GetSongRecord();
            if (record->IsNotBand() && record->GetScore() > 0) {
                label->SetTextToken(record->GetShortDifficultySym());
            }
        } else if (slot->Matches("percentage")) {
            SongRecord *record = osn->GetSongRecord();
            if (record->IsNotBand() && record->GetScore() > 0) {
                label->SetTokenFmt(endgame_player_noteshit_fmt, record->GetNotesPct());
            }
        }
        break;
    }
    case kNodeFunction:
        if (slot->Matches("function")) {
            p9_label->SetFromSongSelectNode(sortNode);
        }
        break;
    case kNodeSetlist: {
        SetlistSortNode *ssn = dynamic_cast<SetlistSortNode *>(sortNode);
        SavedSetlist *setlist = ssn->GetSetlistRecord()->GetSetlist();
        if (slot->Matches("setlist_name")) {
            p9_label->SetSetlistName(setlist);
        } else if (slot->Matches("battle_instrument_rank") && setlist->IsBattle()) {
            p9_label->SetBattleInstrument(ssn->GetSetlistRecord());
        }
        break;
    }
    default:
        label->SetTextToken(gNullStr);
        break;
    }
}

RndMat *MusicLibrary::Mat(int, int idx, UIListMesh *slot) const {
    SortNode *node = GetCurrentSort()->GetNode(idx);
    switch (node->GetType()) {
    case kNodeFunction:
        if (slot->Matches("bg")) {
            static Symbol view_setlists("view_setlists");
            static Symbol net_setlists_connect("net_setlists_connect");
            static Symbol net_setlists_error("net_setlists_error");
            static Symbol net_setlists_getting("net_setlists_getting");
            if (node->GetToken() == view_setlists) {
                return mFunctionSetlistMat;
            }
            if (node->GetToken() == net_setlists_connect
                || node->GetToken() == net_setlists_error
                || node->GetToken() == net_setlists_getting) {
                return mRockCentralMat;
            } else
                return mFunctionMat;
        }
        break;
    case kNodeHeader:
        if (slot->Matches("bg"))
            return mHeaderMat;
        break;
    case kNodeSubheader:
        if (slot->Matches("bg"))
            return mSubheaderMat;
        break;
    case kNodeSong: {
        OwnedSongSortNode *ossn = dynamic_cast<OwnedSongSortNode *>(node);
        MILO_ASSERT(ossn, 0x6FA);
        SongRecord *record = ossn->GetSongRecord();
        if (slot->Matches("bg")) {
            if (record->Data()->IsUGC()) {
                if (idx % 2) {
                    return mUgcMatOdd;
                } else
                    return mUgcMatEven;
            } else if (record->Data()->IsDownload()) {
                if (idx % 2) {
                    return mDlcMatOdd;
                } else
                    return mDlcMatEven;
            } else {
                if (idx % 2) {
                    return mDiscMatOdd;
                } else
                    return mDiscMatEven;
            }
        }

        if (slot->Matches("difficulty_bg")) {
            if (record->IsNotBand() && record->GetScore() > 0) {
                return slot->DefaultMat();
            }
        }
        break;
    }
    case kNodeSetlist:
        if (slot->Matches("bg")) {
            if (idx % 2) {
                return mSetlistMatOdd;
            } else
                return mSetlistMatEven;
        }
        break;
    case kNodeStoreSong:
        if (slot->Matches("bg")) {
            if (idx % 2) {
                return mStoreMatOdd;
            } else
                return mStoreMatEven;
        }
        break;
    default:
        break;
    }
    return nullptr;
}

void MusicLibrary::Custom(int, int idx, UIListCustom *slot, Hmx::Object *obj) const {
    StarDisplay *sdisp = dynamic_cast<StarDisplay *>(obj);
    ReviewDisplay *rdisp = dynamic_cast<ReviewDisplay *>(obj);
    SortNode *sort = GetCurrentSort()->GetNode(idx);
    if (slot->Matches("stars") && sort->GetType() == kNodeSong) {
        OwnedSongSortNode *ossn = dynamic_cast<OwnedSongSortNode *>(sort);
        int numStars = sort->GetTotalStars(true);
        if (numStars > 0 && ossn) {
            if (!ossn->GetSongRecord()->IsDemo()) {
                static Symbol force_mixed_mode("force_mixed_mode");
                sdisp->SetValues(numStars, 5);
                sdisp->SetProperty(force_mixed_mode, 0);
                sdisp->SetShowing(true);
                return;
            }
        }
    } else if (slot->Matches("stars_head")) {
        switch (sort->GetType()) {
        case kNodeHeader:
        case kNodeSubheader:
            if (unkdc != 8
                || sort->GetToken()
                    != LocationCmp::SetlistHeaderTypeToSym((LocationCmp::SetlistHeaderType
                    )0)) {
                static Symbol force_mixed_mode("force_mixed_mode");
                sdisp->SetValues(sort->GetTotalStars(false), sort->GetPotentialStars());
                sdisp->SetProperty(force_mixed_mode, 1);
                sdisp->SetShowing(true);
                return;
            }
            break;
        case kNodeSetlist: {
            SetlistSortNode *ssn = dynamic_cast<SetlistSortNode *>(sort);
            MILO_ASSERT(ssn, 0x74A);
            if (!ssn->GetSetlistRecord()->GetSetlist()->IsBattle()) {
                static Symbol force_mixed_mode("force_mixed_mode");
                sdisp->SetValues(sort->GetTotalStars(false), sort->GetPotentialStars());
                sdisp->SetProperty(force_mixed_mode, 1);
                sdisp->SetShowing(true);
                return;
            }
            break;
        }
        default:
            break;
        }
    } else if (slot->Matches("stars_title") && sort->GetType() == 2 && unkdc == 3) {
        HeaderSortNode *hsn = dynamic_cast<HeaderSortNode *>(sort);
        sdisp->SetToToken(hsn->GetToken());
        sdisp->SetShowing(true);
        return;
    } else if (slot->Matches("review_title") && sort->GetType() == 2 && unkdc == 7) {
        HeaderSortNode *hsn = dynamic_cast<HeaderSortNode *>(sort);
        rdisp->SetToToken(hsn->GetToken());
        rdisp->SetShowing(true);
        return;
    }

    if (sdisp)
        sdisp->SetShowing(false);
    if (rdisp)
        rdisp->SetShowing(false);
}

int MusicLibrary::NumData() const { return GetCurrentSort()->GetDataCount(); }

bool MusicLibrary::IsActive(int idx) const {
    return GetCurrentSort()->GetNode(idx)->IsActive();
}

UIComponent::State
MusicLibrary::ComponentStateOverride(int, int idx, UIComponent::State state) const {
    if (!GetCurrentSort()->GetNode(idx)->IsEnabled())
        return UIComponent::kDisabled;
    return state;
}

int MusicLibrary::SnappableAtOrBeforeData(int idx) const {
    for (; idx >= 0; idx--) {
        if (IsSnappableAtData(idx))
            break;
    }
    return idx;
}

bool MusicLibrary::IsSnappableAtData(int idx) const {
    return GetCurrentSort()->GetNode(idx)->GetType() == 2;
}

Symbol MusicLibrary::DifficultySortPart() const {
    static Symbol band("band");
    static Symbol guitar("guitar");
    static Symbol bass("bass");
    static Symbol drum("drum");
    static Symbol vocals("vocals");
    static Symbol keys("keys");
    static Symbol real_guitar("real_guitar");
    static Symbol real_bass("real_bass");
    static Symbol real_keys("real_keys");
    switch (ActiveScoreType()) {
    case kScoreBand:
        return band;
    case kScoreGuitar:
        return guitar;
    case kScoreBass:
        return bass;
    case kScoreDrum:
    case kScoreRealDrum:
        return drum;
    case kScoreVocals:
    case kScoreHarmony:
        return vocals;
    case kScoreKeys:
        return keys;
    case kScoreRealGuitar:
        return real_guitar;
    case kScoreRealBass:
        return real_bass;
    case kScoreRealKeys:
        return real_keys;
    default:
        MILO_FAIL("Bad ScoreType in MusicLibrary::DifficultySortPart!");
        return gNullStr;
    }
}

ScoreType MusicLibrary::ActiveScoreType() const {
    std::vector<BandUser *> users;
    TheBandUserMgr->GetBandUsersInSession(users);
    BandUser *singleUser = nullptr;
    ScoreType sty;
    if (users.size() != 1)
        sty = kScoreBand;
    else {
        singleUser = users.front();
        MILO_ASSERT(singleUser, 0x7D9);
        if (singleUser->GetTrackType() != 10) {
            sty = TrackTypeToScoreType(singleUser->GetTrackType(), false, false);
        } else {
            sty = TrackTypeToScoreType(
                ControllerTypeToTrackType(singleUser->GetControllerType(), false),
                false,
                false
            );
        }
    }

    if (sty == kScoreDrum) {
        MILO_ASSERT(singleUser, 0x7EB);
        if (singleUser->GetPreferredScoreType() == kScoreRealDrum)
            sty = kScoreRealDrum;
    } else if (sty == kScoreVocals) {
        MILO_ASSERT(singleUser, 0x7F2);
        if (singleUser->GetPreferredScoreType() == kScoreHarmony)
            sty = kScoreHarmony;
    }

    switch (sty) {
    case kScoreGuitar:
        if (mTask.scoreType == kScoreBass)
            sty = kScoreBass;
        break;
    case kScoreBass:
        if (mTask.scoreType == kScoreGuitar)
            sty = kScoreGuitar;
        break;
    case kScoreDrum:
        if (mTask.scoreType == kScoreRealDrum)
            sty = kScoreRealDrum;
        break;
    case kScoreRealDrum:
        if (mTask.scoreType == kScoreDrum)
            sty = kScoreDrum;
        break;
    case kScoreVocals:
        if (mTask.scoreType == kScoreHarmony)
            sty = kScoreHarmony;
        break;
    case kScoreHarmony:
        if (mTask.scoreType == kScoreVocals)
            sty = kScoreVocals;
        break;
    case kScoreKeys:
        if (mTask.scoreType == kScoreRealKeys)
            sty = kScoreRealKeys;
        break;
    case kScoreRealKeys:
        if (mTask.scoreType == kScoreKeys)
            sty = kScoreKeys;
        break;
    case kScoreRealGuitar:
        if (mTask.scoreType == kScoreRealBass)
            sty = kScoreRealBass;
        break;
    case kScoreRealBass:
        if (mTask.scoreType == kScoreRealGuitar)
            sty = kScoreRealGuitar;
        break;
    default:
        break;
    }
    return sty;
}

Symbol MusicLibrary::PartForFilter() const {
    if (mTask.partSym == gNullStr)
        return DifficultySortPart();
    else
        return mTask.partSym;
}

void MusicLibrary::SetlistArtFinished() {
    if (GetHighlightedNode()->GetType() == 6) {
        PushHighlightToScreen(false);
    }
}

void MusicLibrary::SendMessageToSongSelectPanel(Message &msg) {
    UIPanel *panel = ObjectDir::Main()->Find<UIPanel>("song_select_panel", true);
    if (panel->GetState() == UIPanel::kUp) {
        panel->HandleType(msg);
    }
}

void MusicLibrary::PushHighlightToScreen(bool b1) {
    if (b1) {
        static Symbol highlight_node_at_ix("highlight_node_at_ix");
        static Message msg(highlight_node_at_ix, 0);
        msg[0] = mCurrentHighlightIndex;
        SendMessageToSongSelectPanel(msg);
    }
    SetlistSortNode *sort = dynamic_cast<SetlistSortNode *>(GetHighlightedNode());
    if (sort) {
        mSetlistScoresProvider->SetSetlist(sort->GetSetlistRecord()->GetSetlist());
    }
    if (!TheContentMgr.RefreshInProgress()) {
        static Symbol refresh_selected_song("refresh_selected_song");
        static Message refresh_selected_song_msg(refresh_selected_song);
        SendMessageToSongSelectPanel(refresh_selected_song_msg);
    }
}

void MusicLibrary::PushMakingSetlistToScreen() {
    SendMessageToSongSelectPanel(on_change_setlist_mode_msg);
    PushSonglistToScreen();
    PushHighlightToScreen(true);
}

void MusicLibrary::PushFilterToScreen() {
    static Symbol refresh_filter("refresh_filter");
    static Message refresh_filter_msg(refresh_filter);
    SendMessageToSongSelectPanel(refresh_filter_msg);
}

void MusicLibrary::PushSortToScreen() {
    static Message msg(refresh_sort, 0);
    msg[0] = unke8 != 9U;
    SendMessageToSongSelectPanel(msg);
}

void MusicLibrary::PushSetlistToScreen() {
    unk12c = false;
    TheSessionMgr->GetMachineMgr()->GetLocalMachine()->SetNetUIStateParam(-mSetlist.size()
    );
    if (!TheContentMgr.RefreshInProgress()) {
        static Symbol refresh_setlist("refresh_setlist");
        static Message refresh_setlist_msg(refresh_setlist);
        SendMessageToSongSelectPanel(refresh_setlist_msg);
    }
}

void MusicLibrary::PushSonglistToScreen() {
    if (!TheContentMgr.RefreshInProgress()) {
        static Symbol refresh_songlist("refresh_songlist");
        static Message refresh_songlist_msg(refresh_songlist);
        SendMessageToSongSelectPanel(refresh_songlist_msg);
    }
}

void MusicLibrary::PushSetlistSaveDialog() {
    static Symbol show_setlist_save_dialog("show_setlist_save_dialog");
    static Message show_setlist_save_dialog_msg(show_setlist_save_dialog);
    SendMessageToSongSelectPanel(show_setlist_save_dialog_msg);
}

void MusicLibrary::PushHeaderDataToScreen() {
    static Symbol refresh_summary("refresh_summary");
    static Message refresh_summary_msg(refresh_summary);
    SendMessageToSongSelectPanel(refresh_summary_msg);
}

void MusicLibrary::PushMissingSetlistSongsToScreen(int idx) {
    static Message msg(show_missing_setlist_songs_dialog, 0);
    msg[0] = idx;
    SendMessageToSongSelectPanel(msg);
}

void MusicLibrary::SetMakingSetlist(bool val) {
    MILO_ASSERT(!(val && mTask.setlistMode == kSetlistForbidden), 0x8B1);
    MILO_ASSERT(!(!val && mTask.setlistMode == kSetlistForced), 0x8B3);
    if (val != unk12d) {
        unk12d = val;
        TheSongSortMgr->BuildSortList(unkdc);
        TryToSetHighlight(unkd4, unkd8, true);
        PushMakingSetlistToScreen();
    }
}

FORCE_LOCAL_INLINE
bool MusicLibrary::GetMakingSetlist(bool b1) const {
    return unk12d && (b1 || !SongSortMgr::IsSetlistSort(unkdc));
}
END_FORCE_LOCAL_INLINE

void MusicLibrary::ClearSetlist() {
    mSetlist.clear();
    mCurrentSetlist = nullptr;
    unk12c = true;
    SetSyncDirty(-1, false);
}

void MusicLibrary::AppendToSetlist(int i) {
    if (!SetlistIsFull()) {
        // Retail 360 guards on HasSyncPermission() (Synchronizable slot 3,
        // this+0x30), NOT ContentDir() — same pattern as PlaySetlist above.
        if (HasSyncPermission()) {
            mSetlist.push_back(i);
            unk12c = true;
            SetSyncDirty(-1, false);
        } else {
            AppendSongToSetlistMsg msg(i);
            TheSessionMgr->SendMsg(TheSessionMgr->GetLeaderUser(), msg, kReliable);
        }
    }
}

void MusicLibrary::RemoveLastSongFromSetlist() {
    if (mSetlist.size()) {
        if (ContentDir()) {
            mSetlist.pop_back();
            unk12c = true;
            SetSyncDirty(-1, false);
        } else {
            RemoveLastSongFromSetlistMsg msg;
            TheSessionMgr->SendMsg(TheSessionMgr->GetLeaderUser(), msg, kReliable);
        }
    }
}

void MusicLibrary::ShuffleSetlist() {
    std::random_shuffle(mSetlist.begin(), mSetlist.end());
    SetSyncDirty(-1, false);
}

void MusicLibrary::BuildPartySetlist() {
    static Symbol party_shuffle("party_shuffle");
    if (TheGameMode->InMode(party_shuffle)) {
        TheSongSortMgr->BuildFilteredSongList(nullptr, "band");
    }
    mSetlist.clear();
    TheSongSortMgr->GetRandomSongs(0, nullptr, &mSetlist, nullptr, nullptr, true, true);
    SetSyncDirty(-1, true);
}

void MusicLibrary::SendSetlistToMetaPerformer() {
    MetaPerformer::Current()->SetSongs(mSetlist);
}

std::vector<int> &MusicLibrary::GetSetlist() { return mSetlist; }

bool MusicLibrary::SetlistHasSong(int song) {
    const std::vector<int> &setlist = mSetlist;
    return std::find(setlist.begin(), setlist.end(), song) != setlist.end();
}

int MusicLibrary::SongAtSetlistIndex(int idx) {
    if (idx >= mSetlist.size())
        return 0;
    else
        return mSetlist[idx];
}

FORCE_LOCAL_INLINE
int MusicLibrary::SetlistSize() { return mSetlist.size(); }
END_FORCE_LOCAL_INLINE

bool MusicLibrary::SetlistIsFull() {
    if (mTask.setlistMode == kSetlistForced && mTask.maxSetlistSize != 0) {
        return mTask.maxSetlistSize <= SetlistSize();
    } else
        return SetlistSize() >= 100;
}

bool MusicLibrary::AllSetlistSongsHaveScoreType(ScoreType s) {
    switch (s) {
    case kScoreDrum:
    case kScoreBass:
    case kScoreGuitar:
    case kScoreVocals:
    case kScoreKeys:
    case kScoreRealDrum:
    case kScoreRealGuitar:
    case kScoreRealBass:
    case kScoreRealKeys: {
        TrackType t = ScoreTypeToTrackType(s);
        FOREACH (it, mSetlist) {
            BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(*it);
            if (!data || !data->HasPart(TrackTypeToSym(t), false))
                return false;
        }
        break;
    }
    case kScoreHarmony: {
        FOREACH (it, mSetlist) {
            BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(*it);
            if (!data || !data->HasVocalHarmony())
                return false;
        }
        break;
    }
    case kScoreBand:
        break;
    default:
        MILO_FAIL("Bad ScoreType %i in AllSetlistSongsHaveScoreType!", (int)s);
    }
    return true;
}

bool MusicLibrary::NetSetlistsFailed() { return mNetSetlists->mFailed; }
bool MusicLibrary::NetSetlistsSucceeded() { return mNetSetlists->mSucceeded; }

void MusicLibrary::RefreshNetSetlists() {
    if (unk15c) {
        mNetSetlists->CleanUp();
    }
    unk15c = true;
    mNetSetlists->RefreshSetlists();
    RebuildAndSortSetlists();
}

bool MusicLibrary::FilterSetlist(WiiFriendList *friends, NetSavedSetlist *pSetlist) const {
    switch (pSetlist->GetType()) {
    case SavedSetlist::kSetlistLocal:
    case SavedSetlist::kSetlistInternal:
        MILO_ASSERT(0 && "Net setlist contains unusual setlist type", 0x97D);
        // fall through
    case SavedSetlist::kSetlistHarmonix:
    case SavedSetlist::kBattleHarmonix:
    case SavedSetlist::kBattleHarmonixArchived:
        return true;
    default:
        break;
    }
    if (ThePlatformMgr.IsOnlineRestricted())
        return false;
    const char *owner = pSetlist->GetOwner();
    const char *name = Localize(wii_friends_default_setlist_name, nullptr);
    const char *desc = Localize(wii_friends_default_setlist_description, nullptr);
    int numFriends = friends->mFriends.size();
    for (int i = 0; i < 4; i++) {
        const char *profileName = TheWiiProfileMgr.GetNameForIndex(i);
        if (profileName && strcmp(profileName, owner) == 0) {
            return true;
        }
    }
    if (true) { // retail X360 strips ProfileMgr's Wii-friends state (feature absent)
        return false;
    }
    for (int i = 0; i < numFriends; i++) {
        WiiFriend *fr = friends->GetFriendByIdx(i);
        if (fr->GetProfile(owner)) {
            return true;
        }
    }
    const char *setlistName;
    if (TheWiiFriendsProvider.IsPossessiveSuffixNeeded(name)) {
        setlistName = MakeString(
            name, owner, TheWiiFriendsProvider.GetPossessiveSuffix(owner)
        );
    } else {
        setlistName = MakeString(name, owner);
    }
    pSetlist->SetTitle(setlistName);
    const char *setlistDesc;
    if (TheWiiFriendsProvider.IsPossessiveSuffixNeeded(desc)) {
        setlistDesc = MakeString(
            desc, owner, TheWiiFriendsProvider.GetPossessiveSuffix(owner)
        );
    } else {
        setlistDesc = MakeString(desc, owner);
    }
    pSetlist->SetDescription(setlistDesc);
    MILO_ASSERT(
        pSetlist->GetArtTex() == NULL
            && "NetSaveSestlist has texture?  Tell Ian S.",
        0x9CF
    );
    return true;
}

void SavedSetlist::SetTitle(const char *title) { mTitle = title; }
void SavedSetlist::SetDescription(const char *desc) { mDescription = desc; }
RndTex *SavedSetlist::GetArtTex() const { return nullptr; }

void MusicLibrary::GetNetSetlists(std::vector<NetSavedSetlist *> &setlists) const {
    WiiFriendList friends;
    TheWiiFriendMgr.GetCachedFriends(&friends);
    setlists.clear();
    const std::vector<NetSavedSetlist *> &friendSetlists = mNetSetlists->unk20;
    FOREACH (it, friendSetlists) {
        NetSavedSetlist *nsl = *it;
        if (FilterSetlist(&friends, nsl)) {
            setlists.push_back(nsl);
        }
    }
    const std::vector<NetSavedSetlist *> &harmSetlists = mNetSetlists->unk28;
    FOREACH (it, harmSetlists) {
        NetSavedSetlist *nsl = *it;
        if (FilterSetlist(&friends, nsl)) {
            setlists.push_back(nsl);
        }
    }
}

void MusicLibrary::DeleteHighlightedSetlist() {
    SetlistSortNode *setlistNode = dynamic_cast<SetlistSortNode *>(GetHighlightedNode());
    MILO_ASSERT(setlistNode, 0xA11);
    MILO_ASSERT(setlistNode->GetSetlistRecord()->IsLocal(), 0xA12);
    LocalSavedSetlist *lss =
        dynamic_cast<LocalSavedSetlist *>(setlistNode->GetSetlistRecord()->GetSetlist());
    MILO_ASSERT(lss, 0xA17);
    BandProfile *owner = lss->mOwnerProfile;
    owner->DeleteSavedSetlist(lss);
    std::vector<BandProfile *> profiles = TheProfileMgr.GetSignedInProfiles();
    TheRockCentral.SyncSetlists(profiles, mResults, this);
    TheSaveLoadMgr->AutoSave();
    RebuildAndSortSetlists();
}

void MusicLibrary::UpdateHeaderData() {
    mHasHeaderData = false;
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (profile) {
        SongStatusMgr *mgr = profile->GetSongStatusMgr();
        ScoreType s = ActiveScoreType();
        mHasHeaderData = true;
        mHeaderCareerScore = mgr->CalculateTotalScore(s, gNullStr);
        mHeaderCareerInstrumentMask = 1 << s;
        mHeaderCareerStars = mgr->GetCachedTotalStars(s);
        mHeaderPossibleStars = mgr->GetPossibleStars(s, gNullStr);
    }
    PushHeaderDataToScreen();
}

DataNode MusicLibrary::OnMsg(const PrimaryProfileChangedMsg &) {
    SwitchOffRankedSort();
    UpdateHeaderData();
    RebuildProfileData();
    return 1;
}

DataNode MusicLibrary::OnMsg(const ProfileChangedMsg &) {
    RebuildAndSortSetlists();
    return 1;
}

DataNode MusicLibrary::OnMsg(const SigninChangedMsg &msg) {
    if ((unsigned int)msg.GetChangedMask() && unk15c) {
        RefreshNetSetlists();
    }
    return 1;
}

DataNode MusicLibrary::OnMsg(const LocalUserLeftMsg &) {
    SwitchOffRankedSort();
    RebuildUserConfigData();
    return 1;
}

DataNode MusicLibrary::OnMsg(const RemoteUserLeftMsg &) {
    SwitchOffRankedSort();
    RebuildUserConfigData();
    return 1;
}

DataNode MusicLibrary::OnMsg(const AddLocalUserResultMsg &msg) {
    if (msg.Success()) {
        SwitchOffRankedSort();
        RebuildUserConfigData();
    }
    return 1;
}

DataNode MusicLibrary::OnMsg(const NewRemoteUserMsg &) {
    SwitchOffRankedSort();
    RebuildUserConfigData();
    if (TheGameMode->InMode(qp_coop)) {
        MILO_ASSERT(!TheSessionMgr->IsLocal(), 0xA74);
        mTask.setlistMode = kSetlistForced;
        SetMakingSetlist(true);
    }
    return 1;
}

DataNode MusicLibrary::OnMsg(const RemoteMachineUpdatedMsg &msg) {
    if (msg.GetMask() & 2U) {
        RebuildRestrictedData();
    }
    return 1;
}

DataNode MusicLibrary::OnMsg(const RemoteMachineLeftMsg &) {
    RebuildSharedSongData();
    if (TheSessionMgr->IsLocal()) {
        if (TheGameMode->InMode(qp_coop)) {
            mTask.setlistMode = kSetlistOptional;
        }
    }
    return 1;
}

DataNode MusicLibrary::OnMsg(const ServerStatusChangedMsg &msg) {
    if (msg->Int(2) != 0 && mNetSetlists->mFailed) {
        RefreshNetSetlists();
    }
    return 1;
}

DataNode MusicLibrary::OnMsg(const FriendsListChangedMsg &) {
    RefreshNetSetlists();
    return 1;
}

DataNode MusicLibrary::OnMsg(const UserLoginMsg &) {
    RefreshNetSetlists();
    return 1;
}

void MusicLibrary::RebuildProfileData() {
    std::map<Symbol, SongRecord> &theSongs = TheSongSortMgr->mSongs;
    bool b1 = false;
    bool b2 = false;
    FOREACH (it, theSongs) {
        if (it->second.UpdatePerformanceData())
            b1 = true;
        if (it->second.UpdateReview())
            b2 = true;
    }
    if (b1) {
        ReSort(kSongSortByStars);
        PushSonglistToScreen();
        PushHighlightToScreen(false);
    }
    if (b2) {
        ReSort(kSongSortByReview);
    }
}

void MusicLibrary::RebuildUserConfigData() {
    std::map<Symbol, SongRecord> &theSongs = TheSongSortMgr->mSongs;
    bool b1 = false;
    FOREACH (it, theSongs) {
        if (it->second.UpdateScoreType())
            b1 = true;
    }
    if (b1) {
        ReSort(kSongSortByDiff);
        ReSort(kSongSortByStars);
        PushSonglistToScreen();
        PushHighlightToScreen(false);
        UpdateHeaderData();
    }
}

void MusicLibrary::RebuildSharedSongData() {
    TheSongMgr.SyncSharedSongs();
    SortNode *highlightedNode = GetCurrentSort()->GetNode(mCurrentHighlightIndex);
    OwnedSongSortNode *curNode = dynamic_cast<OwnedSongSortNode *>(highlightedNode);
    bool wasShared = curNode && curNode->GetSongRecord()->mRestricted;
    std::map<Symbol, SongRecord> &theSongs = TheSongSortMgr->mSongs;
    bool aSharedSongChanged = false;
    FOREACH (it, theSongs) {
        if (it->second.UpdateRestricted()) {
            aSharedSongChanged = true;
            it->second.UpdateSharedStatus();
        }
    }
    bool mySharedSongChanged =
        curNode && (bool)curNode->GetSongRecord()->mRestricted != wasShared;
    MILO_ASSERT(!mySharedSongChanged || aSharedSongChanged, 0xAFD);
    if (aSharedSongChanged) {
        PushSonglistToScreen();
        if (mySharedSongChanged) {
            PushHighlightToScreen(false);
        }
    }
}

void MusicLibrary::RebuildRestrictedData() {
    SortNode *highlightedNode = GetCurrentSort()->GetNode(mCurrentHighlightIndex);
    OwnedSongSortNode *curNode = dynamic_cast<OwnedSongSortNode *>(highlightedNode);
    bool wasRestricted = curNode && curNode->GetSongRecord()->mIsShared;
    std::map<Symbol, SongRecord> &theSongs = TheSongSortMgr->mSongs;
    bool aRestrictedSongChanged = false;
    FOREACH (it, theSongs) {
        if (it->second.UpdateSharedStatus())
            aRestrictedSongChanged = true;
    }
    bool myRestrictedSongChanged =
        curNode && (bool)curNode->GetSongRecord()->mIsShared != wasRestricted;
    MILO_ASSERT(!myRestrictedSongChanged || aRestrictedSongChanged, 0xB27);
    if (aRestrictedSongChanged) {
        PushSonglistToScreen();
        if (myRestrictedSongChanged) {
            PushHighlightToScreen(false);
        }
    }
}

DECOMP_FORCEACTIVE(MusicLibrary, "!myRestrictedSongChanged || aRestrictedSongChanged")

bool MusicLibrary::IsPurchasing() const { return false; }

void MusicLibrary::GetStoreOffers(std::vector<StoreOffer *> &offers) const {
    offers.clear();
}

void MusicLibrary::SetRandomSongs(
    int numSongs, SongSortMgr::SongFilter &filter, Symbol s, bool b4, bool b5
) {
    if (s == gNullStr)
        s = DifficultySortPart();
    TheSongSortMgr->BuildFilteredSongList(&filter, s);
    std::vector<Symbol> vSongs;
    if (!TheSongSortMgr->GetRandomSongs(
            numSongs, &vSongs, nullptr, nullptr, nullptr, b4, b5
        )) {
        MILO_WARN(
            "Attempted to create a filtered random setlist but there weren't enough songs available!"
        );
        vSongs.clear();
        SongSortMgr::SongFilter localFilter;
        TheSongSortMgr->BuildFilteredSongList(&localFilter, s);
        bool bSuccess = TheSongSortMgr->GetRandomSongs(
            numSongs, &vSongs, nullptr, nullptr, nullptr, b4, b5
        );
        MILO_ASSERT(bSuccess, 0xB8F);
    }
    MILO_ASSERT(std::find( vSongs.begin(), vSongs.end(), gNullStr ) == vSongs.end(), 0xB92);
    MILO_ASSERT(vSongs.size() == numSongs, 0xB93);
    MetaPerformer *performer = MetaPerformer::Current();
    MILO_ASSERT(performer, 0xB96);
    performer->SetSongs(vSongs);
}

void MusicLibrary::FakeWin(int i1) {
    short mask = 0;
    std::vector<LocalBandUser *> users;
    TrackType t = (TrackType)TheBandUserMgr->GetLocalParticipants(users);
    if (users.empty())
        return;
    else {
        FOREACH (it, users) {
            t = ControllerTypeToTrackType((*it)->ConnectedControllerType(), false);
            mask |= 1 << t;
        }
        ScoreType s = kScoreBand;
        if (users.size() == 1)
            s = TrackTypeToScoreType(t, false, false);
        Difficulty d = (Difficulty)RandomInt(0, 4);
        FakeWinNode(GetHighlightedNode(), users, s, d, i1, mask);
        RebuildProfileData();
    }
}

#pragma push
#pragma pool_data off
void MusicLibrary::FakeWinNode(
    SortNode *node,
    std::vector<LocalBandUser *> &users,
    ScoreType sty,
    Difficulty diff,
    int i1,
    short mask
) const {
    switch (node->GetType()) {
    case kNodeHeader:
    case kNodeSubheader: {
        FOREACH (it, node->mChildren) {
            FakeWinNode(*it, users, sty, diff, i1, mask);
        }
        break;
    }
    case kNodeSong: {
        OwnedSongSortNode *songNode = dynamic_cast<OwnedSongSortNode *>(node);
        MILO_ASSERT(songNode, 0xBCA);
        int randScore = RandomInt(
            i1 * users.size() * (diff * 2000),
            i1 * users.size() * ((diff + kDifficultyMedium) * 2000)
        );
        int randAccuracy = RandomInt(0, 0x65);
        FOREACH (it, users) {
            LocalBandUser *cur = *it;
            if (cur->CanSaveData()) {
                BandProfile *profile = TheProfileMgr.GetProfileForUser(cur);
                if (profile) {
                    PerformerStatsInfo info;
                    info.mScore = randScore;
                    info.mAccuracy = randAccuracy;
                    info.mStars = i1;
                    info.mScoreType = sty;
                    info.mDifficulty = diff;
                    profile->UpdateScore(
                        songNode->GetSongRecord()->Data()->ID(), info, false
                    );
                    if (sty != kScoreBand) {
                        info.mScoreType = kScoreBand;
                        profile->UpdateScore(
                            songNode->GetSongRecord()->Data()->ID(), info, false
                        );
                    }
                    const char *msg = MakeString(
                        "recorded %i points and %i stars on %s for user %s",
                        randScore,
                        i1,
                        songNode->GetToken(),
                        cur->UserName()
                    );
                    static Hmx::Object *cd =
                        ObjectDir::Main()->Find<Hmx::Object>("cheat_display", true);
                    static Message show("show", 0);
                    show[0] = msg;
                    cd->Handle(show, false);
                }
            }
        }
        break;
    }
    default:
        break;
    }
}
#pragma pop

#pragma push
#pragma dont_inline on
// DEFERRED LEAD (laneAX-W5, 2026-07-27) -- Handle is 96.01%; the residue is a
// handler-list divergence, not the local-static lever (RB3_HANDLE_LOCAL_STATIC
// is already on for this TU and our 52 statics line up 1:1 with retail's first
// 52). Retail fn_82542D20 has 54 guarded Symbol ctors: ours minus `fake_win`
// (retail has NO fake_win handler) plus three store handlers after
// `reset_filters`, in this exact order:
//   is_downloading   HANDLE_EXPR, returns
//       unk19c->IsDownloading(                       // retail 0x825BCBD0
//           dynamic_cast<StoreSongSortNode *>(_msg->Obj<Hmx::Object>(2))
//               ->mOffer                             // StoreSongSortNode+0x44
//               ->GetSingleSongID())                 // 0x827A6D48
//   load_store_art   HANDLE_ACTION (branches to the "return 0" epilogue);
//       node 3 is evaluated FIRST (MSVC right-to-left):
//       unk19c->LoadStoreArt(                        // retail 0x825BCC10
//           dynamic_cast<StoreOffer *>(_msg->Obj<Hmx::Object>(2))
//               ->GetSingleSongID(),
//           _msg->Obj<Hmx::Object>(3))
//   get_store_art    HANDLE_EXPR, returns a kDataObject built straight from
//       unk19c + 0x48 (an Hmx::Object* member this stub class lacks).
// Blocker: MusicLibraryUnkOp is a deliberately-undefined stub (see the comment
// on its declaration in MusicLibrary.h) with no IsDownloading/LoadStoreArt and
// no 0x48 member. Adding them is a real body port, so it was NOT bundled into
// W5's measured local-static leg.
BEGIN_HANDLERS(MusicLibrary)
    HANDLE_ACTION(on_enter, OnEnter())
#ifdef HX_NATIVE
    // `on_exit` Symbol global collides with POSIX on_exit(); intern inline.
    HANDLE_ACTION(Symbol("on_exit"), OnExit())
#else
    HANDLE_ACTION(on_exit, OnExit())
#endif
    HANDLE_ACTION(on_unload, OnUnload())
    HANDLE_ACTION(report_sort_and_filters, ReportSortAndFilters())
    HANDLE_ACTION(
        select_highlighted_node, SelectHighlightedNode(_msg->Obj<LocalBandUser>(2))
    )
    HANDLE_EXPR(
        get_current_shortcut_ix, GetCurrentSort()->GetShortcutIx(GetHighlightedNode())
    )
    HANDLE_ACTION(skip_to_shortcut, SkipToShortcut(_msg->Int(2)))
    HANDLE_ACTION(push_highlight_to_screen, PushHighlightToScreen(true))
    HANDLE_ACTION(clear_setlist, ClearSetlist())
    HANDLE_ACTION(play_setlist, PlaySetlist(true))
    HANDLE_ACTION(skip_to_next_shortcut, SkipToNextShortcut(true))
    HANDLE_ACTION(skip_to_prev_shortcut, SkipToNextShortcut(false))
    HANDLE_ACTION(make_sure_setlist_is_valid, MakeSureSetlistIsValid())
    HANDLE_ACTION(set_highlight_ix, SetHighlightIx(_msg->Int(2), _msg->Int(3)))
    HANDLE_EXPR(is_ix_active, IsIxActive(_msg->Int(2)))
    HANDLE_EXPR(can_headers_be_selected, CanHeadersBeSelected())
    HANDLE_EXPR(get_highlighted_node, GetHighlightedNode())
    HANDLE(get_sort_list, OnGetSortList)
    HANDLE_EXPR(get_current_sort_name, GetCurrentSort()->GetName())
    HANDLE_EXPR(get_shortcut_provider, GetCurrentSort())
    HANDLE_ACTION(re_sort, ReSort(_msg->Sym(2)))
    HANDLE_ACTION(rebuild_and_sort_setlists, RebuildAndSortSetlists())
    HANDLE_ACTION(rebuild_restricted_data, RebuildSharedSongData())
    HANDLE_EXPR(viewing_setlists, SongSortMgr::IsSetlistSort(unkdc))
    HANDLE_EXPR(num_data, NumData())
    HANDLE_EXPR(active_score_type, ActiveScoreType())
    HANDLE_EXPR(get_making_setlist, GetMakingSetlist(false))
    HANDLE_ACTION(set_making_setlist, SetMakingSetlist(_msg->Int(2)))
    HANDLE_EXPR(setlist_size, SetlistSize())
    HANDLE_EXPR(
        all_setlist_songs_have_score_type,
        AllSetlistSongsHaveScoreType(SymToScoreType(_msg->Sym(2)))
    )
    HANDLE_EXPR(get_max_setlist_size, GetMaxSetlistSize())
    HANDLE_ACTION(remove_last_song_from_setlist, RemoveLastSongFromSetlist())
    HANDLE_ACTION(send_setlist_to_metaperformer, SendSetlistToMetaPerformer())
    HANDLE_ACTION(start_in_setlist_browser, unkec = true)
    HANDLE_ACTION(delete_highlighted_setlist, DeleteHighlightedSetlist())
    HANDLE_EXPR(net_setlist_art_ready, mNetSetlists->IsSetlistArtReady(_msg->Sym(2)))
    HANDLE_EXPR(get_net_setlist_art, mNetSetlists->GetSetlistArt(_msg->Sym(2)))
    HANDLE_ACTION(refresh_net_setlist_art, mNetSetlists->RefreshSetlistArt())
    HANDLE_EXPR(get_back_screen, mTask.backScreen)
    HANDLE_EXPR(get_next_screen, mTask.nextScreen)
    HANDLE_EXPR(get_title_token, mTask.titleToken)
    HANDLE_EXPR(get_making_setlist_token, mTask.makingSetlistToken)
    HANDLE_EXPR(get_filter_locked, GetFilterLocked())
    HANDLE_ACTION(
        set_default_task_with_back_screen, mTask.ResetWithBackScreen(_msg->Sym(2))
    )
    HANDLE_EXPR(get_forced_setlist, GetForcedSetlist())
    HANDLE_EXPR(has_header_data, HasHeaderData())
    HANDLE_EXPR(header_career_score, HeaderCareerScore())
    HANDLE_EXPR(header_career_instrument_mask, HeaderCareerInstrumentMask())
    HANDLE_EXPR(header_career_stars, HeaderCareerStars())
    HANDLE_EXPR(header_possible_stars, HeaderPossibleStars())
    HANDLE_ACTION(reset_filters, ResetFilters())
    // laneAY-B: retail has NO `fake_win` handler; in its place three store
    // handlers (target 0x82542D20 +0x11a8/+0x1228/+0x12bc). See the block
    // comment above for the retail asm this reproduces.
    HANDLE_EXPR(
        is_downloading,
        unk19c->IsDownloading(dynamic_cast<StoreSongSortNode *>(_msg->Obj<Hmx::Object>(2))
                                  ->mOffer->GetSingleSongID())
    )
    HANDLE_ACTION(
        load_store_art,
        unk19c->LoadStoreArt(
            dynamic_cast<StoreOffer *>(_msg->Obj<Hmx::Object>(2))->GetSingleSongID(),
            _msg->Obj<Hmx::Object>(3)
        )
    )
    HANDLE_EXPR(get_store_art, unk19c->mStoreArt)
    HANDLE_MESSAGE(PrimaryProfileChangedMsg)
    HANDLE_MESSAGE(ProfileChangedMsg)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_MESSAGE(LocalUserLeftMsg)
    HANDLE_MESSAGE(RemoteUserLeftMsg)
    HANDLE_MESSAGE(AddLocalUserResultMsg)
    HANDLE_MESSAGE(NewRemoteUserMsg)
    HANDLE_MESSAGE(RemoteMachineUpdatedMsg)
    HANDLE_MESSAGE(RemoteMachineLeftMsg)
    HANDLE_MESSAGE(ServerStatusChangedMsg)
    // laneAY-B: retail's list ends here -- it has NO FriendsListChangedMsg and
    // NO UserLoginMsg handler (target 0x82542D20 goes straight from
    // ServerStatusChangedMsg/RemoteMachineLeftMsg to Hmx::Object::Handle; the
    // two rb3-Wii handlers were a clean 60-instruction base-only insert).
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0xC7B)
END_HANDLERS
#pragma pop

BEGIN_PROPSYNCS(MusicLibrary)
    static Symbol setlist_provider("setlist_provider");
    SYNC_PROP(setlist_provider, mSetlistProvider)
    static Symbol setlist_scores_provider("setlist_scores_provider");
    SYNC_PROP(setlist_scores_provider, mSetlistScoresProvider)
    static Symbol view_settings_provider("view_settings_provider");
    SYNC_PROP(view_settings_provider, mViewSettingsProvider)
END_PROPSYNCS
// sw2 scatter-include (default/MusicLibrary <- band3/game/Game.cpp)
#define RB3_GAME_SCATTER_COPY
#define gRev gRev_Game
#define gAltRev gAltRev_Game
#include "band3/game/Game.cpp"
#undef gRev
#undef gAltRev
#undef RB3_GAME_SCATTER_COPY
