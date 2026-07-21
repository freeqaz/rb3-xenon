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
    virtual void TextureCompressed(int);
    virtual RndTex *GetPatchTex(Patch &);
    virtual RndMesh *GetPatchMesh(Patch &);
    virtual RndTex *GetBandLogo();
    virtual void Compress(RndTex *, bool);
#ifdef HX_NATIVE
    // The Wii build leaves this empty (4-byte `blr`, matching the target): on
    // PPC it returns whatever is in r3, which the patch-pre-render path tolerates.
    // On native a non-void function that falls off the end is UB — the optimizer
    // emits a trap (x86 `ud2` → SIGILL), which crashes OutfitConfig::DrawPreClear's
    // BandPatchMesh::PreRender loop the moment a band character with patch meshes
    // composes its outfit. BandCharacter IS an ObjectDir (via Character→RndDir→
    // ObjectDir) and is the dir GetPatchMesh()/GetPatchTex() search for patch
    // meshes/textures, so the character's own dir is the correct patch dir.
    virtual ObjectDir *GetPatchDir() {
        return static_cast<ObjectDir *>(static_cast<Character *>(this));
    }
#else
    // RB3-Wii leaves this empty (4-byte blr — returns whatever is in r3). MSVC
    // X360 rejects a non-void function with no return (C4716), so return 0; the
    // retail /O1 body is effectively empty either way.
    virtual ObjectDir *GetPatchDir() { return 0; }
#endif
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
    void SavePrefabFromCloset();
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
    float unk6d8; // 0x7c0 (retail TU5: unk6d8 sits AFTER unk6f4[64], just before the
                  // flags — SaveBoneAndChildren/TextureCompressed -4 offset proof)
    unsigned int unk738; // 0x7c4 (retail TU5: flags precede the waypoint — OnPreClear stw 0x7c4 proof)
    Waypoint *unk734; // 0x738
    ObjPtrList<RndMesh> unk73c; // 0x73c
    ObjPtrList<RndMesh> unk74c; // 0x74c
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
#endif
};
