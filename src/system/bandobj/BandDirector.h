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
    /** Retail-only (0x8229A2E0): the RBN2 flavour of `midi_add_postproc`, which
     * takes only (Symbol, seconds) -- no fade-length. Not present in the rb3-Wii
     * dev source; its `rbn2_add_postproc` handler string lives at .rdata
     * 0x820170F8. */
    void OnRbn2AddPostProc(Symbol, float);
    /** Retail-only (0x82298E60): the `midi_shot5_cleanup` handler body, which
     * folds the `shot_5` prop-anim key track into the per-playmode `shot` track.
     * Handler string at .rdata 0x82016FD0. */
    void OnMidiShot5Cleanup();
    bool FacingCamera(Symbol) const;
    bool BehindCamera(Symbol) const;
    void LoadVenue(Symbol, LoaderPos);
    void SetCharacterHideHackEnabled(bool);
    /** Retail (0x82298B40) takes a bool the rb3-Wii dev source lacks; it gates
     * the back-inserted legacy fade-in key (`if (lpreset && b && i > 0)`), and
     * the `midi_cleanup_presets` handler feeds it `_msg->Int(2)`. */
    void OnMidiPresetCleanup(bool);
    void AddSymbolKey(Symbol, Symbol, float);
    void ClearSymbolKeys(Symbol);
    void ClearSymbolKeysFrameRange(Symbol, float, float);
    void HarvestDircuts();
    /** Retail-only factoring (0x8228DD38, 0x244 bytes): the per-character
     * lip-sync assignment loop that the rb3-Wii dev source has inlined inside
     * VenueLoaded. Retail calls it from two places -- OnFileLoaded/VenueLoaded
     * (0x82292350) and BandWardrobe::SetPlayMode (0x823308F4) -- with `this` as
     * the only argument, so it takes no parameters and reads everything it needs
     * from members:
     *   0x120 song.lipsync, 0x124 part2.lipsync, 0x128 part3.lipsync,
     *   0x12c part4.lipsync (all CharLipSync*, cached by OnFileLoaded at
     *   0x822915E4/5FC/614/62C), and mSongPref at 0x130.
     * DONE (lane CT-3): body written from the retail bytes, 145/145 instructions
     * equal. The old "cannot be written until the tail layout is corrected" TODO
     * here was STALE -- lane CR-2 had already moved mLipSyncs[4] to 0x120 and
     * mSongPref to 0x130, which is exactly what the body needs.
     * Note retail uses Part4Inst() for the drum mode inst where the Wii dev
     * source erroneously re-uses Part3Inst(). */
    void SetCharacterLipSyncs();
    void SetSongEnd(float);
    void SetShot(Symbol, Symbol);
    void ExportWorldEvent(Symbol);
    void SendMessage(Symbol, Symbol);
    void SetCrowd(Symbol);
    void SetCharSpot(Symbol, Symbol);
    /** Retail keeps this a real out-of-line call (fn_8228CFF0): the stagekit_fog
     * arm of SyncProperty (0x82294F28) evaluates `_val.Sym()` and sets up
     * r3=this, r4=symbol before the `bl`. With an empty inlinable body MSVC
     * folds the call away AND elides the Sym() evaluation. The attribute must
     * sit on the DECLARATION -- on the definition MSVC ignores it (measured:
     * our obj still had no SetFog relocation at all). */
    __declspec(noinline) void SetFog(Symbol);
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
    // Retail's standalone instance (0x8228da98, called via bl from
    // OnSelectCamera/OnSetDircut) materializes the bool in r11 then
    // masks with clrlwi before moving to r3 -- a single-expression
    // logical-chain return reproduces that shape (see
    // docs/decomp/patterns/fixable-bool-mask.md step 3e).
    bool NoWorlds() {
        return mDisablePicking || !GetWorld()
            || GetWorld()->GetCameraManager()->HasFreeCam() || !mVenue.Dir();
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

    ObjDirPtr<RndDir> mChars; // 0x2c
    ObjPtr<RndPropAnim> mPropAnim; // 0x38
    ObjPtr<FileMerger> mMerger; // 0x44
    ObjPtr<WorldDir> mCurWorld; // 0x50
    bool unk58; // 0x5c
    int mNumPlayersFailed; // 0x60
    int mExcitement; // 0x64 - ExcitementLevel?
    Symbol mForceAttention[4]; // 0x68
    ObjPtr<RndPostProc> mWorldPostProc; // 0x78
    ObjPtr<RndPostProc> mCamPostProc; // 0x84
    ObjPtr<RndPostProc> mPostProcA; // 0x90
    ObjPtr<RndPostProc> mPostProcB; // 0x9c
    float mPostProcBlend; // 0xa8
    Symbol mLightPresetCatA; // 0xac
    Symbol mLightPresetCatB; // 0xb0
    float mLightPresetCatBlend; // 0xb4
    bool mLightPresetInterpEnabled; // 0xb8
    bool mDisabled; // 0xb9
    bool mAsyncLoad; // 0xba
    ObjPtr<BandCamShot> mCurShot; // 0xbc
    ObjPtr<BandCamShot> mNextShot; // 0xc8
    ObjPtr<BandCamShot> mIntroShot; // 0xd4
    Symbol mShotCategory; // 0xe0
    float unke0; // 0xe4
    bool mDisablePicking; // 0xe8
    bool unke5; // 0xe9 - enable world polling?
    Keys<DircutEntry, DircutEntry> mDircuts; // 0xec (retail vector start this+0xec)
    VenueLoader mVenue; // 0xf8
    Keys<Symbol, Symbol> unk100; // 0x108
    float unk108; // 0x114
    float mEndOfSongSec; // 0x118
    bool unk110; // 0x11c
    // The 16 "TU5-inserted" bytes belong HERE, before mSongPref -- not at the
    // tail where they were parked. Evidence (lane CR-2): retail's SyncProperty
    // (0x82294824) reads mSongPref as `lwz r11, -0x8(r23)` with r23 == this+0x138
    // (`subi r4, r23, 0x138` yields `this`), i.e. mSongPref@0x130; with the block
    // at the tail the compiler placed mSongPref@0x120 and we emitted -0x18(r23).
    // That is exactly this block's size, and it reproduces the layout this
    // header's own SetCharacterLipSyncs comment already documented: four
    // CharLipSync* at 0x120-0x12c, mSongPref at 0x130.
    // Size is unchanged, so the Hmx::Object vbase stays at 0x138 and the 39
    // vbtable/vtordisp-adjust functions in this TU keep matching.
    class CharLipSync *mLipSyncs[4]; // 0x120 (song, part2, part3, part4)
    BandSongPref *mSongPref; // 0x130
};

extern BandDirector *TheBandDirector;
