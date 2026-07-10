#pragma once
#include "rndobj/Poll.h"
#include "rndobj/Draw.h"
#include "rndobj/Dir.h"
#include "rndobj/PropAnim.h"
#include "rndobj/PostProc.h"
#include "char/FileMerger.h"
#include "world/Dir.h"
#include "bandobj/BandCamShot.h"
#include "bandobj/BandSongPref.h"
#include "utl/Loader.h"

class BandDirector : public RndPollable, public RndDrawable {
public:
    class VenueLoader : public Loader::Callback {
    public:
        VenueLoader();
        virtual ~VenueLoader();
        virtual void FinishLoading(Loader *);
        virtual const char *StateName() const { return "VenueLoader"; }

        void Unload(bool);
        void Load(const FilePath &, LoaderPos, bool);
        Symbol Name() const { return mName; }
        WorldDir *Dir() const { return mDir; }

        WorldDir *mDir; // 0x4
        DirLoader *mLoader; // 0x8
        Symbol mName; // 0xc
    };

    /** Retail-only rewrite (no rb3-Wii equivalent): mDircuts entry wrapping a
     * possibly-null BandCamShot*. Evidence: Keys<DircutEntry,DircutEntry>::Cross
     * at 0x822847A8 (stride 8, value@+0, frame@+4 => sizeof 4); FindNextDircut
     * 0x82284C18 returns entry->shot and null-tests it; AddDircut 0x822881B8
     * stores a nullable shot pointer. */
    struct DircutEntry {
        DircutEntry(BandCamShot *s = 0) : shot(s) {}
        BandCamShot *shot; // 0x0
    };

    BandDirector();
    OBJ_CLASSNAME(BandDirector);
    OBJ_SET_TYPE_ENGINE(BandDirector); // retail 0x82286000 shape: static types init before null check
    virtual DataNode Handle(DataArray *, bool);
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    virtual void ListPollChildren(std::list<RndPollable *> &) const;
    virtual ~BandDirector();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Replace(ObjRef *, Hmx::Object *);
    virtual void DrawShowing();
    virtual void ListDrawChildren(std::list<RndDrawable *> &);
    virtual void CollideList(const Segment &, std::list<Collision> &);

    void VenueLoaded(WorldDir *);
    void OnMidiAddPostProc(Symbol, float, float);
    bool FacingCamera(Symbol) const;
    bool BehindCamera(Symbol) const;
    void LoadVenue(Symbol, LoaderPos);
    void SetCharacterHideHackEnabled(bool);
    void OnMidiPresetCleanup();
    void AddSymbolKey(Symbol, Symbol, float);
    void ClearSymbolKeys(Symbol);
    void ClearSymbolKeysFrameRange(Symbol, float, float);
    void HarvestDircuts();
    void SetSongEnd(float);
    void SetShot(Symbol, Symbol);
    void ExportWorldEvent(Symbol);
    void SendMessage(Symbol, Symbol);
    void SetCrowd(Symbol);
    void SetCharSpot(Symbol, Symbol);
    void SetFog(Symbol);
    WorldDir *GetWorld();
    void EnterVenue();
    void PickIntroShot();
    void FindNextShot();
    void ClearLighting();
    bool PostProcsFromPresets(const RndPostProc *&, const RndPostProc *&, float &);
    void
    UpdatePostProcOverlay(const char *, const RndPostProc *, const RndPostProc *, float);
    bool ReadyForMidiParsers();
    class BandCharacter *GetCharacter(int) const;
    void ForceShot(BandCamShot *);
    void AddDircut(Symbol, float);
    void FilterShot(int &);
    Symbol GetModeInst(Symbol);
    void UnloadVenue(bool);
    BandCamShot *FindNextDircut();
    void FindNextPstKeyframe(float, float, Symbol);
    void SendCurWorldMsg(Symbol, bool);
    void PlayNextShot();

    bool IsMusicVideo();
    LightPresetManager *LightPresetMgr() {
        return mCurWorld ? &mCurWorld->GetLightPresetMgr() : 0;
    }

    bool DirectedCut(Symbol s) const;
    bool BFTB(Symbol s) const;

    // TODO: find a better name for this
    // Retail (0x8227D058): direct short-circuit chain, no bool
    // materialization; GetWorld() evaluated twice (two dynamic_casts).
    bool NoWorlds() {
        bool ret;
        if (!mDisablePicking && GetWorld()
            && !GetWorld()->GetCameraManager()->HasFreeCam()) {
            ret = false;
            if (!mVenue.Dir())
                ret = true;
        } else {
            ret = true;
        }
        return ret;
    }

    DataNode OnFirstShotOK(DataArray *);
    DataNode OnShotOver(DataArray *);
    DataNode OnPostProcInterp(DataArray *);
    DataNode OnSaveSong(DataArray *);
    DataNode OnFileLoaded(DataArray *);
    DataNode OnSelectCamera(DataArray *);
    DataNode OnLightPresetInterp(DataArray *);
    DataNode OnLightPresetKeyframeInterp(DataArray *);
    DataNode OnCycleShot(DataArray *);
    DataNode OnForceShot(DataArray *);
    DataNode OnGetFaceOverrideClips(DataArray *);
    DataNode OnDebugInterestsForNextCharacter(DataArray *);
    DataNode OnToggleInterestDebugOverlay(DataArray *);
    DataNode OnShotAnnotate(DataArray *);
    DataNode OnPostProcs(DataArray *);
    DataNode OnSetDircut(DataArray *);
    DataNode OnForcePreset(DataArray *);
    DataNode OnStompPresets(DataArray *);
    DataNode OnMidiAddPreset(DataArray *);
    DataNode OnGetCatList(DataArray *);
    DataNode OnCopyCats(DataArray *);
    DataNode OnLoadSong(DataArray *);
    DataNode OnMidiShotCategory(DataArray *);

    static Symbol RemapCat(Symbol, Symbol);
    static const char *PickDist(float *, char *, char *);
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(BandDirector); }
    static void Terminate();

    NEW_OBJ(BandDirector);
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    static DataArray *sPropArr;
    static float sMotionBlurBlendAmount;

    ObjDirPtr<RndDir> mChars; // 0x28
    ObjPtr<RndPropAnim> mPropAnim; // 0x34
    ObjPtr<FileMerger> mMerger; // 0x40
    ObjPtr<WorldDir> mCurWorld; // 0x4c
    bool unk58; // 0x58
    int mNumPlayersFailed; // 0x5c
    int mExcitement; // 0x60 - ExcitementLevel?
    Symbol mForceAttention[4]; // 0x64
    ObjPtr<RndPostProc> mWorldPostProc; // 0x74
    ObjPtr<RndPostProc> mCamPostProc; // 0x80
    ObjPtr<RndPostProc> mPostProcA; // 0x8c
    ObjPtr<RndPostProc> mPostProcB; // 0x98
    float mPostProcBlend; // 0xa4
    Symbol mLightPresetCatA; // 0xa8
    Symbol mLightPresetCatB; // 0xac
    float mLightPresetCatBlend; // 0xb0
    bool mLightPresetInterpEnabled; // 0xb4
    bool mDisabled; // 0xb5
    bool mAsyncLoad; // 0xb6
    ObjPtr<BandCamShot> mCurShot; // 0xb8
    ObjPtr<BandCamShot> mNextShot; // 0xc4
    ObjPtr<BandCamShot> mIntroShot; // 0xd0
    Symbol mShotCategory; // 0xdc
    float unke0; // 0xe0
    bool mDisablePicking; // 0xe4
    bool unke5; // 0xe5 - enable world polling?
    Keys<DircutEntry, DircutEntry> mDircuts; // 0xe8 (retail vector start this+0xec)
    VenueLoader mVenue; // 0xf0
    Keys<Symbol, Symbol> unk100; // 0x100
    float unk108; // 0x108
    float mEndOfSongSec; // 0x10c
    bool unk110; // 0x110
    BandSongPref *mSongPref; // 0x114
};

extern BandDirector *TheBandDirector;
