#pragma once
#include "PoseFatalities.h"
#include "SongCollision.h"
#include "char/CharClip.h"
#include "char/Character.h"
#include "char/FileMerger.h"
#include "gesture/BaseSkeleton.h"
#include "hamobj/Difficulty.h"
#include "hamobj/HamCamShot.h"
#include "hamobj/HamCharacter.h"
#include "hamobj/HamMove.h"
#include "hamobj/HamVisDir.h"
#include "hamobj/MoveDir.h"
#include "hamobj/MoveGraph.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Poll.h"
#include "rndobj/PostProc.h"
#include "rndobj/PropAnim.h"
#include "rndobj/PropKeys.h"
#include "rndobj/Tex.h"
#include "rndobj/TexRenderer.h"
#include "utl/MemMgr.h"
#include "utl/Song.h"
#include "utl/Symbol.h"
#include "world/CameraManager.h"
#include "world/Dir.h"

class AnimPtr;

class OfflineCallback : public SongCallback {
public:
    OfflineCallback() {}
    virtual ~OfflineCallback() {}
    virtual void SongSetFrame(class Song *, float) {}
    virtual ObjectDir *SongMainDir();
    virtual void SongPlay(bool) {}
    virtual void UpdateObject(const Hmx::Object *, DataArray *) {}
    virtual void Preload() {}
    virtual void ProcessBookmarks(DataNode) {}
};

/** "Hammer Director, sits in each song file and manages camera + scene changes" */
class HamDirector : public RndPollable, public RndDrawable {
    friend class MoveDir;
public:
    struct DircutEntry {
        HamCamShot *mShot;
        bool mForced;
    };
    // Hmx::Object
    virtual ~HamDirector();
    OBJ_CLASSNAME(HamDirector);
    OBJ_SET_TYPE(HamDirector);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    virtual void ListPollChildren(std::list<RndPollable *> &) const;
    // RndDrawable
    virtual void DrawShowing();
    virtual void ListDrawChildren(std::list<RndDrawable *> &);
    virtual void CollideList(const Segment &, std::list<Collision> &);

    OBJ_MEM_OVERLOAD(0x6D)
    NEW_OBJ(HamDirector)

    WorldDir *GetWorld();
    float GetMainFaceOverrideWeight();
    void SetMainFaceOverrideWeight(float);
    Symbol GetMainFaceOverrideClip() const;
    void SetMainFaceOverrideClip(Symbol);
    bool GetGameStartHold() const { return mGameStartHold; }
    bool IsWorldLoaded() const;
    void UnloadAll();
    void ForceScene(Symbol);
    void ForceMiniVenue(Symbol);
    void ReselectWorldPostProc();
    void PlayCharBaseVisemes();
    void EnableFacialAnimation();
    void DisableFacialAnimation();
    void ResetFacialAnimation();
    void SetLipsyncOffsets(float);
    void ResyncFaceDrivers();
    void BlendInFaceOverrides(float);
    void BlendOutFaceOverrides(float);
    Symbol MoveNameFromBeat(float, int);
    RndPropAnim *SongAnim(int);
    RndPropAnim *SongAnimByDifficulty(Difficulty);
    RndPropAnim *DancerFaceAnimByPlayer(int);
    void Reteleport();
    void StartStopVisualizer(bool, int);
    void SetPlayerSpotlightsEnabled(bool);
    void ChangePlayerCharacter(int, Symbol, Symbol, Symbol);
    void InitOffline();
    void OfflineLoadSong(Symbol);
    void DrawDebug();
    void ArmMultiIntroMode();
    void PlayIntroShot();
    void SetupAnims();
    void RemapSongAnimToTempoMap(TempoMap *);
    RndPropAnim *GetPropAnim(Difficulty, const char *, bool);
    void SetupRoutineBuilderAnims();
    PropKeys *GetPropKeys(Difficulty, Symbol);
    void VenueEnter(WorldDir *);
    void ForceShot(const char *);
    PropKeys *GetMasterKeys(Symbol);
    Key<Symbol> *GetMasterPracticeFrame(Symbol);
    PropKeys *GetPropKeysByPlayer(int, Symbol);
    void TriggerNextIntro();
    WorldDir *GetVenueWorld();
    void UnselectVisualizerPostProc();
    bool GetPracticeFrames(Key<Symbol> *&, Key<Symbol> *&);
    HamCharacter *GetCharacter(int) const;
    HamCharacter *GetBackup(int);
    void LoadRoutineBuilderData(std::set<const MoveVariant *> &, bool);
    bool InPracticeMode();
    void MoveKeys(Difficulty, class MoveDir *, std::vector<HamMoveKey> &);
    bool IsMoveMergerFinished() const;
    void HandleDifficultyChange();
    void CheckBeginFatal(int, HamMove *, int);
    void LoadCrew(Symbol, Symbol);
    void SetPhraseMetersFlipped(bool);
    void PoseIconMan(CharClip *, float, RndTex *, bool, CharClip *, float, float);
    void PoseIconMan(const BaseSkeleton *, RndTex *);
    void CleanOriginalMoveData();
    float BeatFromTag(Symbol);
    void UnloadMergers();

    void DrawIconMan(Symbol, Symbol, Symbol, float, float, RndTex *);
    void DrawIconMan(Difficulty, float, float, float, float, RndTex *);

    ObjectDir *ClipDir() const { return mClipDir; }
    bool NoTransitions() const { return mNoTransitions; }
    MoveDir *GetMoveDir() const { return static_cast<MoveDir *>(mMoveDir.Ptr()); }
    FileMerger *GetMerger() const { return mMerger; }
    ObjectDir *MergerDir() const { return mMerger ? mMerger->Dir() : nullptr; }
    HamCamShot *CurShot() const { return mCurShot; }
    FileMerger *GetGameModeMerger() const { return mGameModeMerger; }
    void SetPickingDisabled(bool disable) { mDisablePicking = disable; }
    void SetPollEnabled(bool enable) { mPollEnabled = enable; }
    bool PollEnabled() const { return mPollEnabled; }
    bool IsGameStartHold() const { return mGameStartHold; }
    int StartLoopMargin() const { return mStartLoopMargin; }
    int EndLoopMargin() const { return mEndLoopMargin; }
    PoseFatalities *GetPoseFatalities() const { return mPoseFatalities; }
    RndPostProc *GetActivePostProc() const { return mActivePostProc; }

    DataNode OnGetDancerVisemes(DataArray *);

protected:
    HamDirector();

    void SetShot(Symbol);

    /** World event options: (none bonusfx bonusfx_optional chorus verse) */
    void SetWorldEvent(Symbol);

    Symbol ClosestMove();
    void UpdatePlayerFreestyle(bool);

    /** Send spotlight message for character (e.g., "spotlight_instructor_off") */
    void SetCharSpot(Symbol charType, Symbol spotState);
    void PausePlayerFreestyle(bool pause) {
        mPlayerFreestylePaused = pause;
        if (mVisualizer)
            mVisualizer->SetShowing(!pause);
    }
    void Initialize();
    void HideBackups(bool, bool);
    void RestoreBackups();
    void TeleportChars();
    void OnPopulateMoves();
    void OnPopulateMoveMgr();
    void OnPopulateFromMoveMgr();
    void OnPopulateFromFile();
    void HudEntered();
    void PickIntroShot();
    void FindNextShot();
    void PlayNextShot();
    void SetMasterClipAnim();
    HamCamShot *FindNextDircut();
    void SetDircut(Symbol, std::vector<CameraManager::PropertyFilter>);
    void AddNumPlayers(std::vector<CameraManager::PropertyFilter> &, DataArray *);
    void ReactToCollision_InsertRealShot(Symbol, float);
    void ReactToCollision_MoveShot(int, float);
    bool ReactToCollision(float);
    bool AreCharactersColliding();
    bool ShouldDoCollisionPrevention() const;
    void ChangeNextShotIfCharacterCollisionLikely();
    void StartStopVisualizer();
    void SendCurWorldMsg(Symbol, bool);
    bool ShotsDisabled();
    bool SongAnimation();
    void SyncScene();
    void SetNewWorld();
    void
    UpdatePostProcOverlay(const char *, const RndPostProc *, const RndPostProc *, float);
    ObjectDir *GetDifficultyProxy(Difficulty);
    CharClip *
    GetClipStartAndEndBeats(Symbol, float &, float &, std::pair<float, float> *);

    DataNode OnShotOver(DataArray *);
    DataNode OnPostProcInterp(DataArray *);
    DataNode OnSaveSong(DataArray *);
    DataNode OnSaveFaceAnims(DataArray *);
    DataNode OnFileLoaded(DataArray *);
    DataNode OnFileMerged(DataArray *);
    DataNode OnLoadSong(DataArray *);
    DataNode OnSelectCamera(DataArray *);
    DataNode OnCycleShot(DataArray *);
    DataNode OnForceShot(DataArray *);
    DataNode OnPostProcs(DataArray *);
    DataNode OnSetDircut(DataArray *);
    DataNode OnBlendInFaceClip(DataArray *);
    DataNode OnPracticeBeats(DataArray *);
    DataNode OnToggleCamshotFlag();
    DataNode OnListPossibleMoves();
    DataNode OnListPossibleVariants();
    DataNode OnClipAnnotate(DataArray *);
    DataNode OnClipSafeToAdd(DataArray *);
    DataNode OnClipList(DataArray *);
    DataNode OnPracticeSafeToAdd(DataArray *);
    DataNode OnPracticeAnnotate(DataArray *);
    DataNode PracticeList(Difficulty);
    DataNode OnToggleDebugInterests(DataArray *);
    DataNode OnToggleCamCharacterSkeleton(DataArray *);

    ObjDirPtr<RndDir> unk48; // 0x2c
    std::map<Difficulty, AnimPtr> mSongAnims; // 0x38
    std::map<Difficulty, AnimPtr> mDancerFaceAnims; // 0x50
    ObjPtr<RndPropAnim> mMasterClipAnim; // 0x68
    ObjPtr<RndPropAnim> mPlayer1RoutineBuilderAnim; // 0x74
    ObjPtr<RndPropAnim> mPlayer2RoutineBuilderAnim; // 0x80
    float unkc8; // 0x8c
    Symbol unkcc; // 0x90
    /** "How much backup dancers drift, 0 is none, 1 is full" */
    float mBackupDrift; // 0x94
    ObjPtr<FileMerger> mMerger; // 0x98
    ObjPtr<FileMerger> mMoveMerger; // 0xa4
    ObjPtr<FileMerger> mGameModeMerger; // 0xb0
    ObjPtr<WorldDir> mVenue; // 0xbc
    ObjPtr<SongCollision> mSongCollision; // 0xc8
    Symbol mForcedMiniVenue; // 0xd4
    Symbol mForcedScene; // 0xd8
    bool mPickNewShot; // 0xdc
    /** "how many have failed" */
    int mNumPlayersFailed; // 0xe0
    /** "excitement level" */
    int mExcitement; // 0xe4
    bool mSyncScene; // 0xe8
    ObjPtr<RndPostProc> mWorldPostProc; // 0xec
    /** "camera postproc override.  If set, does no postproc blends" */
    ObjPtr<RndPostProc> mCamPostProc; // 0xf8
    ObjPtr<RndPostProc> mForcePostProc; // 0x104
    ObjPtr<RndPostProc> mActivePostProc; // 0x110
    float mForcePostProcBlend; // 0x11c
    float mForcePostProcBlendRate; // 0x120
    ObjPtr<RndPostProc> mPostProcInterpA; // 0x124
    ObjPtr<RndPostProc> mPostProcInterpB; // 0x130
    float mPostProcInterpBlend; // 0x13c
    float mFreestyleTimer; // 0x140
    ObjPtr<RndPostProc> mSavedForcePostProc; // 0x144
    ObjPtr<RndPostProc> mVisualizerPostProc; // 0x150
    /** "TRUE if freestyle is allowed" */
    bool mFreestyleEnabled; // 0x15c
    ObjPtr<HamCharacter> mPlayer0Char; // 0x160
    ObjPtr<HamCharacter> mPlayer1Char; // 0x16c
    ObjPtr<HamCharacter> mBackup0Char; // 0x178
    ObjPtr<HamCharacter> mBackup1Char; // 0x184
    bool mBackupHidden; // 0x190
    /** 0-1 = players 0-1, 2-3 = backups 0-1 */
    bool mCharsShowing[4]; // 0x191
    bool mDisabled; // 0x195
    bool mAsyncLoaded;
    /** "currently shown camshot, nice for debugging." */
    ObjPtr<HamCamShot> mCurShot; // 0x198
    ObjPtr<HamCamShot> mNextShot; // 0x1a4
    ObjPtr<HamCamShot> mIntroShot; // 0x1b0
    /** "HamCamShot category" */
    Symbol mShot; // 0x1bc
    float mLastShotTime; // 0x1c0
    bool mDisablePicking; // 0x1c4
    bool mSuppressIntroShot; // 0x1c5
    int mSuppressNextShot; // 0x1c8
    float mLastCollisionTime; // 0x1cc
    bool mPollEnabled; // 0x1d0
    Keys<DircutEntry, DircutEntry> mDirCutKeys; // 0x1d4
    bool mPlayerFreestyle; // 0x1e0
    bool mPlayerFreestylePaused; // 0x1e1
    ObjPtr<HamVisDir> mVisualizer; // 0x1e4
    /** "start frame of practice mode" */
    Symbol mPracticeStart; // 0x1f0
    /** "end frame of practice mode" */
    Symbol mPracticeEnd; // 0x1f4
    /** "In practice mode, measures before practice_start until loop".
        Ranges from 1 to 100. */
    int mStartLoopMargin; // 0x1f8
    /** "In practice mode, measures after practice_end until loop".
        Ranges from 1 to 100. */
    int mEndLoopMargin; // 0x1fc
    float mPrevSongFrame; // 0x200
    /** "If > 0, is which clip to show by itself rather than doing full blending" */
    int mBlendDebug; // 0x204
    int unk2ec; // 0x208
    Symbol mPrevMove; // 0x20c
    Symbol mCharacterOutfits[2]; // 0x210
    Symbol mCrews[2]; // 0x218
    HamBackupDancers mBackupDancers; // 0x220
    ObjPtr<ObjectDir> mClipDir; // 0x224
    ObjPtr<ObjectDir> mMoveDir; // 0x230
    Symbol mSongSpeed; // 0x23c
    /** "If true, does not play transitions" */
    bool mNoTransitions; // 0x240
    /** "If true, check character collisions when picking cam shots" */
    bool mCollisionChecks; // 0x241
    bool mLoadedNewSong; // 0x242
    PoseFatalities *mPoseFatalities; // 0x244
    bool mCamshotFlag; // 0x248
    bool mGameStartHold; // 0x249
    ObjPtr<Character> mIconManChar; // 0x24c
    ObjPtr<RndTexRenderer> mIconManTex; // 0x258
    bool mVisualizerRunning; // 0x264
    bool mPhraseMetersFlipped; // 0x265
    Song *mOfflineSong; // 0x268
    std::set<Hmx::Object *> mRoutineBuilderObjects; // 0x26c
};

extern HamDirector *TheHamDirector;

class AnimPtr : public ObjPtr<RndPropAnim> {
public:
    AnimPtr() : ObjPtr<RndPropAnim>(TheHamDirector) {}
    AnimPtr(RndPropAnim *anim) : ObjPtr<RndPropAnim>(TheHamDirector, anim) {}
};
