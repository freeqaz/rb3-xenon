#pragma once
#include "char/Character.h"
#include "char/CharCollide.h"
#include "char/CharCuff.h"
#include "char/CharDriver.h"
#include "char/CharDriverMidi.h"
#include "char/CharEyes.h"
#include "char/CharHair.h"
#include "char/CharLipSyncDriver.h"
#include "char/CharLookAt.h"
#include "char/CharMeshHide.h"
#include "char/FileMerger.h"
#include "char/Waypoint.h"
#include "char/CharIKScale.h"
#include "char/CharIKHand.h"
#include "char/CharIKMidi.h"
#include "char/CharWeightSetter.h"
#include "char/CharBoneOffset.h"
#include "bandobj/BandCharDesc.h"
#include "bandobj/BandPatchMesh.h"
#include "bandobj/OutfitConfig.h"
#include "bandobj/CharKeyHandMidi.h"
#include "obj/Utl.h"
#include "rndobj/Rnd.h"
#include "rndobj/MeshDeform.h"

#ifndef HX_NATIVE
// mTestPrefab (below, ObjOwnerPtr<BandCharDesc>) is the ONLY consumer of
// ObjRefConcrete<BandCharDesc, ObjectDir> in the whole tree (verified: no
// ObjPtr<BandCharDesc> exists anywhere), unlike most T1s here which are
// shared between an ObjPtr<T> AND an ObjOwnerPtr<T> instantiation. That
// sharing is why this is a targeted specialization rather than a change to
// the shared ObjRefConcrete<T1,T2>::~ObjRefConcrete() body in ObjPtr_p.h:
// ObjOwnerPtr<T>::~ObjOwnerPtr() (ObjPtr_p.h) already Release()s via
// OwnerRef() and nulls mObject before the implicit base-class dtor runs, so
// this base body's `if (mObject) Release(...)` is dead code at runtime for
// this T1 -- but retail still compiles that dead branch, and it reads mOwner
// (reinterpreted ObjRefOwner*) rather than passing `this`. Editing the
// general template would flip that argument for ObjPtr<T>-driven T1s too,
// where the base's Release(this) call is live and correct.
template <>
inline ObjRefConcrete<BandCharDesc, ObjectDir>::~ObjRefConcrete() {
    if (mObject)
        mObject->Release(reinterpret_cast<ObjRefOwner *>(mOwner));
}
#endif

class BandCharacter : public Character,
                      public BandCharDesc,
                      public MergeFilter,
                      public Rnd::CompressTextureCallback {
public:
    class BoneState {
    public:
        RndTransformable *mBone;
        Transform mXfm;
    };

    BandCharacter();
    OBJ_CLASSNAME(BandCharacter);
    OBJ_SET_TYPE(BandCharacter);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual ~BandCharacter();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();
    virtual bool AllowsInlineProxy() { return false; }
    virtual void AddedObject(Hmx::Object *);
    virtual void RemovingObject(Hmx::Object *);
    virtual void Replace(Hmx::Object *, Hmx::Object *);
    virtual void DrawShowing();
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual void CollideList(const Segment &, std::list<Collision> &);
    virtual void Poll();
    /** Retail-only out-of-line helper (0x8227CDA8): fallback state-group name
     * used by Poll() when mOverrideGroup is empty. */
    const char *DefaultStateGroup();
    virtual void Enter();
    virtual void Exit();
    virtual void Teleport(Waypoint *);
    virtual void CalcBoundingSphere();
    virtual float ComputeScreenSize(RndCam *);
    virtual void DrawLodOrShadow(int, DrawMode);
    virtual CharEyes *GetEyes() { return mEyes; }
    virtual bool ValidateInterest(CharInterest *, ObjectDir *);
    virtual bool SetFocusInterest(CharInterest *, int);
    virtual void SetInterestFilterFlags(int);
    virtual void ClearInterestFilterFlags();
    // Rnd::CompressTextureCallback::TextureCompressed takes intptr_t
    // (rndobj/Rnd.h:106) and is PURE. On ILP32 `intptr_t == int`, so `int` is a
    // valid override for X360; under LP64 it is a different type, the override
    // does not bind, and BandCharacter stays abstract -- which makes
    // BandCharacter.h:200's NEW_OBJ(BandCharacter) ill-formed and takes down
    // every TU that reaches this header (X2: rndobj/Console.cpp and
    // rndobj/Font.cpp, via their bandobj scatter chains). Same split the
    // sibling callback already uses at gesture/StreamRecorder.h:33-37.
#ifdef HX_NATIVE
    virtual void TextureCompressed(intptr_t);
#else
    virtual void TextureCompressed(int);
#endif
    virtual RndTex *GetPatchTex(Patch &);
    virtual RndMesh *GetPatchMesh(Patch &);
    virtual RndTex *GetBandLogo();
    virtual void Compress(RndTex *, bool);
    // Retail X360 body is `subi r3, r3, 0x268; blr` — i.e. it really does return
    // `this`, adjusted from the BandCharDesc sub-object (at +0x268 in
    // BandCharacter) back to the ObjectDir base at offset 0. The rb3-Wii oracle's
    // empty body is a DEV-build artifact, not retail behaviour: BandCharacter IS
    // an ObjectDir (via Character→RndDir→ObjectDir) and is the dir
    // GetPatchMesh()/GetPatchTex() search for patch meshes/textures, so the
    // character's own dir is the correct patch dir. This also fixes native, where
    // falling off the end of a non-void function traps (x86 `ud2` → SIGILL) in
    // OutfitConfig::DrawPreClear's BandPatchMesh::PreRender loop.
    virtual ObjectDir *GetPatchDir() {
        return static_cast<ObjectDir *>(static_cast<Character *>(this));
    }
    virtual void AddOverlays(BandPatchMesh &);
    virtual void MiloReload();
    virtual Action Filter(Hmx::Object *, Hmx::Object *, ObjectDir *);
    virtual SubdirAction FilterSubdir(ObjectDir *o1, ObjectDir *);

    void DrawLodOrShadowMode(int, DrawMode);
    void AddObject(Hmx::Object *);
    void ClearGroup();
    void StartLoad(bool, bool, bool);
    bool IsLoading();
    const char *FlagString(int);
    void SetContext(Symbol);
    // RB3-360 retail (lane DQ-1): retail's save_from_closet arm builds a DataNode
    // temp from `bl fn_822824F8(this, "")` and inline-destructs it, and
    // CustomizePanel::SavePrefab *returns* this call's result — so retail's
    // signature is `DataNode SavePrefabFromCloset(const char* = "")`, not the
    // rb3-Wii dev-build `void ...()`.  fn_822824F8 lives inside BandCharacter's
    // own .text span (0x82280F8C-0x82285D98) and interns its char* argument into
    // mPrefab (+0x274) via Symbol(); its real body is ~0x400 B and is NOT ported
    // here.  Only the signature is corrected, which is what the two Handle
    // dispatchers actually observe.
    DataNode SavePrefabFromCloset(const char * = "");
    void SetSingalong(float);
    void GameOver();
    void ClearDircuts();
    void SetInstrumentType(Symbol);
    void SetGroupName(const char *);
    void SetHeadLookatWeight(float);
#ifdef HX_NATIVE
    // wave-08 native-only: repoint this band member's outfit skin meshes from the
    // static shared char/main/skeleton magnet onto the member's OWN animated
    // per-member skeleton bones (resolved by name via Find<RndTransformable>). Keeps
    // the authored gender-correct inverse-bind offset (SetBone calcOffset=false) so
    // the female stops flinging AND the band animates. Idempotent (runs once per
    // member, mNativeReboundOnce guard). Called from Poll once Find resolves to the
    // moving instance. No-op on Wii (HX_NATIVE only). Opt-out RB3_NO_SKEL_REBIND=1.
    void RebindOutfitBonesToOwnSkeleton();
    // X22 native-only: repoint this member's outfit skin meshes off the SHARED
    // char/main/shared/char_shared.milo material and onto the member's OWN
    // same-named one. Reproduces the consequence of retail's Filter()
    // sCharSharedDir -> ReplaceRefs arm (:2519), which the native FilterSubdir
    // shim prevents from ever being reached. Per-mesh SetMat, NOT the global
    // ::ReplaceRefs (which would cross-wire all four members). No-op on
    // Wii/X360. Opt-out RB3_NO_SKINMAT_REBIND=1.
    void RebindSharedSkinMatsToOwn();
#endif
    CharClipDriver *SetState(const char *, int, int, bool, bool);
    bool InVignetteOrCloset() const;
    void RemoveDrawAndPoll(Character *);
    void SetClipTypes(Symbol, Symbol);
    void SetTempoGenreVenue(Symbol, Symbol, const char *);
    void DeformHead(SyncMeshCB *);
    void SyncOutfitConfig(OutfitConfig *);
    void SetDeformation();
    void PlayGroup(const char *, bool, int, float, TaskUnits, Symbol);
    bool AllowOverride(const char *);
    bool SetPrefab(BandCharDesc *);
    bool AddDircut(Symbol, Symbol, int);
    bool AddDircut(const FilePath &);
    CharLipSyncDriver *GetLipSyncDriver();
    int GetShotFlags(Symbol);
    void SetVisemes();
    void RecomposePatches(BandCharDesc *, int);
    OutfitConfig *GetOutfitConfig(const char *);
    void SetLipSync(CharLipSync *);
    void SetSongOwner(CharLipSyncDriver *);
    void PlayFaceClip();
    void UpdateOverlay();
    void SetDircuts();
    void SaveBoneAndChildren(RndTransformable *);
    CharClipDriver *PlayMainClip(int, bool);
    Symbol InstrumentType() const { return mInstrumentType; }
    bool AddDriverClipDir() { return mAddDriver && mAddDriver->ClipDir(); }

    DataNode OnListDircuts();
    DataNode ListAnimGroups(int);
    DataNode OnPlayGroup(DataArray *);
    DataNode OnGroupOverride(DataArray *);
    DataNode OnChangeFaceGroup(DataArray *);
    DataNode OnSetPlay(DataArray *);
    DataNode OnCamTeleport(DataArray *);
    DataNode OnClosetTeleport(DataArray *);
    DataNode OnInstallFilter(DataArray *);
    DataNode OnPreClear(DataArray *);
    DataNode OnCopyPrefab(DataArray *);
    DataNode OnSavePrefab(DataArray *);
    DataNode OnSetFileMerger(DataArray *);
    DataNode OnLoadDircut(DataArray *);
    DataNode OnPostMerge(DataArray *);
    DataNode OnHideCategories(DataArray *);
    DataNode OnRestoreCategories(DataArray *);
    DataNode OnToggleInterestDebugOverlay(DataArray *);
    DataNode OnListDrumVenues(DataArray *);
    DataNode OnPortraitBegin(DataArray *);
    DataNode OnPortraitEnd(DataArray *);

    bool InCloset() const { return mInCloset; }
    const char *GetGroupName() const;
    int GetPlayFlags() const;

    static void MakeMRU(BandCharacter *, CharClip *);
    static Symbol NameToDrumVenue(const char *);
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(BandCharacter); }
    static void Terminate();
    static unsigned short gRev;
    static unsigned short gAltRev;
    NEW_OBJ(BandCharacter);
    NEW_OVERLOAD;
    DELETE_OVERLOAD;

    int mPlayFlags; // 0x450
    ObjPtr<CharDriver> unk454; // 0x454
    CharDriver *mAddDriver; // 0x460
    CharDriver *mFaceDriver; // 0x464
    char mGroupName[64]; // 0x468
    char mFaceGroupName[64]; // 0x4a8
    char mOverrideGroup[64]; // 0x4e8
    bool mForceNextGroup; // 0x528
    bool mForceVertical; // 0x529
    ObjPtr<Character> mOutfitDir; // 0x52c
    ObjPtr<Character> mInstDir; // 0x538
    Symbol mTempo; // 0x544
    FileMerger *mFileMerger; // 0x548
    RndOverlay *mOverlay; // 0x54c
    ObjPtr<CharLookAt> mHeadLookAt; // 0x550
    ObjPtr<CharLookAt> mNeckLookAt; // 0x55c
    ObjPtr<CharEyes> mEyes; // 0x568
    bool unk574; // 0x574
    ObjOwnerPtr<BandCharDesc> mTestPrefab; // 0x578
    Symbol mGenre; // 0x584
    Symbol mDrumVenue; // 0x588
    Symbol mTestTourEndingVenue; // 0x58c
    Symbol mInstrumentType; // 0x590
    ObjPtr<Waypoint> unk594; // 0x594
    bool mInCloset; // 0x5a0
    bool unk5a1; // 0x5a1
    bool unk5a2; // 0x5a2
    bool unk5a3; // 0x5a3
    ObjPtr<CharWeightSetter> mSingalongWeight; // 0x5a4
    ObjPtrList<CharMeshHide> unk5b0; // 0x5b0
    ObjPtrList<CharIKScale> unk5c0; // 0x5c0
    ObjPtrList<CharIKHand> unk5d0; // 0x5d0
    ObjPtrList<CharCollide> unk5e0; // 0x5e0
    ObjPtrList<CharHair> unk5f0; // 0x5f0
    ObjPtrList<CharCuff> unk600; // 0x600
    ObjPtrList<RndMeshDeform> unk610; // 0x610
    ObjPtrList<OutfitConfig> unk620; // 0x620
    ObjPtrList<OutfitConfig> unk630; // 0x630
    ObjPtrList<CharBoneOffset> unk640; // 0x640
    ObjPtrList<CharIKMidi> unk650; // 0x650
    ObjPtrList<CharDriverMidi> unk660; // 0x660
    ObjPtrList<CharKeyHandMidi> unk670; // 0x670
    ObjPtr<RndMesh> unk680; // 0x680
    ObjPtr<RndMesh> unk68c; // 0x68c
    ObjPtr<RndMesh> unk698; // 0x698
    ObjPtr<RndMesh> unk6a4; // 0x6a4
    ObjPtr<CharWeightable> unk6b0; // 0x6b0
    bool mUseMicStandClips; // 0x6bc
    bool unk6bd; // 0x6bd
    ObjPtr<BandCharacter> unk6c0; // 0x6c0
    std::list<String> mDircuts; // 0x6cc
    bool mInTourEnding; // 0x6d4
    std::list<int> mCompressedTextureIDs; // 0x768 (retail: precedes unk6d8; see below)
    std::list<BoneState> unk6e4; // 0x770
    CharDriver *unk6ec; // 0x778
    int unk6f0; // 0x77c
    char unk6f4[64]; // 0x780
    // retail TU5 (proved from the ~BandCharacter member-dtor sequence + the ctor's
    // init-store sequence): the waypoint PRECEDES the flags, and rb3-Wii's
    // `float unk6d8` (Wii 0x6d8, an edit-mode starvation timer) does NOT exist in
    // the retail 360 build at all — the ctor emits exactly two zero-stores in this
    // region (0x778 = unk6ec, 0x7c4 = unk738) and 0x778+4+4+64 == 0x7c0 exactly,
    // leaving no slot for it. It is kept below under HX_NATIVE only.
    Waypoint *unk734; // 0x7c0
    unsigned int unk738; // 0x7c4
    ObjPtrList<RndMesh> unk73c; // 0x7c8
    ObjPtrList<RndMesh> unk74c; // 0x7dc
#ifdef HX_NATIVE
    // rb3-Wii-only edit-mode clip-starvation timer (Wii 0x6d8); absent from retail
    // 360, so it lives after the matched layout.
    float unk6d8;
#endif
#ifdef HX_NATIVE
    // wave-08 native-only: rebind bookkeeping for RebindOutfitBonesToOwnSkeleton
    // (called from Poll). mNativeReboundOnce latches to 1 once the rebind is COMPLETE
    // (the body clothing + face/hands have been repointed AND a later scan finds
    // nothing new), after which Poll skips the scan entirely. mNativeReboundQuiet
    // counts consecutive no-new-rebind scans since the last rebind, so a late-loading
    // body mesh is still caught before latching. Appended after the matched layout so
    // the Wii image stays byte-identical. Default 0.
    int mNativeReboundOnce;
    int mNativeReboundQuiet;
    int mNativeReboundBody; // ever rebound a >=20-bone body/face mesh (latch gate)
    // X22 native-only: latch for RebindSharedSkinMatsToOwn. Counts consecutive
    // scans that repointed nothing; the walk stops once a sustained quiet period
    // passes, so late-streaming LOD pieces (shoes/pants) are still caught. Same
    // append-after-the-matched-layout rule as the members above.
    int mNativeSkinMatQuiet;
#endif
};
