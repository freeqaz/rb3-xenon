#pragma once
#include "HamRegulate.h"
#include "char/CharEyes.h"
#include "char/CharLipSync.h"
#include "char/CharServoBone.h"
#include "char/CharWeightable.h"
#include "char/Character.h"
#include "char/FileMerger.h"
#include "char/Waypoint.h"
#include "hamobj/HamDriver.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Mesh.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"

enum HamBackupDancers {
    kBackupDancersOutfit = 0,
    kBackupDancersDanceBattle = 1,
    kBackupDancersTan = 2,
    kBackupDancersOverride = 3,
    kBackupDancersNumTypes = 4
};

enum HamGender {
    /** "female character" */
    kHamFemale = 0,
    /** "male character" */
    kHamMale = 1
};

/** "Hammer main character class, can be configured to look like characters in /dancer" */
class HamCharacter : public Character {
public:
    enum {
        kNumSkeletons = 13
    };

    HamCharacter();
    // Hmx::Object
    virtual ~HamCharacter();
    OBJ_CLASSNAME(HamCharacter);
    OBJ_SET_TYPE(HamCharacter);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // ObjectDir
    virtual void SyncObjects();
    // RndDrawable
    virtual void Draw();
    virtual void DrawShowing();
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    // Character
    virtual CharEyes *GetEyes() { return mEyes; }
#ifdef HX_NATIVE
    void SetEyes(CharEyes *eyes) { mEyes = eyes; }
#endif

    OBJ_MEM_OVERLOAD(0x19)
    NEW_OBJ(HamCharacter)
    static void Init();
    static void Terminate();

    void StartLoad(bool);
    void SetTexBlendersActive(bool);
    void SetLipsyncOffset(float);
    void EnableFacialAnimation(CharLipSync *, float);
    void SetBlinking(bool);
    void SetCampaignVo(const char *);
    void SetPropShowing(int prop, bool show);
    String GetCampaignVo();
    void SetOutfit(Symbol);
    void SetOutfitDir(Symbol);
    void UnloadAll();
    String GetCampaignVoMilo();
    bool IsLoading();
    bool InClipTest();
    void SetIKEffectorWeights(float);
    void ResyncLipSync(CharLipSync *);
    void PlayBaseViseme();
    void DisableFacialAnimation();
    void ResetFacialAnimation();
    void BlendInFaceOverrides(float);
    void BlendOutFaceOverrides(float);
    void SetFaceOverrideWeight(float);
    float GetFaceOverrideWeight();
    void SetUseCameraSkeleton(bool);
    Symbol GetFaceOverrideClip();
    void ResetFaceOverrideBlending();
    int SongAnimation();
    ObjectDir *GetNeutralSkeleton();
    void SetFaceOverrideClip(Symbol, bool);
    HamDriver *SongDriver();
    HamRegulate *Regulator();
    void BlendInFaceOverrideClip(Symbol, float, float);
    Waypoint *GetWaypoint() const { return mWaypoint; }
    bool UseCameraSkeleton() const { return mUseCameraSkeleton; }
    void SetPollWhenHidden(bool poll) { mPollWhenHidden = poll; }
    Symbol Outfit() const { return mOutfit; }

    static bool sLoadVO;

protected:
    virtual void Load(BinStream &);
    virtual void AddedObject(Hmx::Object *);
    virtual void RemovingObject(Hmx::Object *);

    void ApplyBlendedSkeletons(HamDriver *, CharClip *, float);
    bool GetPropShowing(int);

    DataNode OnConfigureFileMerger(DataArray *);
    DataNode OnCamTeleport(DataArray *);
    DataNode OnPostDelete(DataArray *);
    DataNode OnSoundPlay(const DataArray *);
    DataNode OnToggleInterestDebugOverlay(DataArray *);

    static CharClip *sSkeletonClips[kNumSkeletons];

    String mCampaignVO; // 0x2d8
    Hmx::Object *mCampaignVOBank; // 0x2e0
    ObjectDir *mCampaignVODir; // 0x2e4
    FileMerger *mFileMerger; // 0x2e8
    /** "which character to look like" */
    Symbol mOutfit; // 0x2ec
    Waypoint *mWaypoint; // 0x2f0
    /** "where to load outfits from" */
    Symbol mOutfitDir; // 0x2f4
    bool mIsCampaignChar; // 0x2f8
    /** "Draws a 6 foot square box around the character teleport point" */
    bool mShowBox; // 0x2f9
    bool mNeedsAcquirePose; // 0x2fa
    ObjPtr<CharEyes> mEyes; // 0x2fc
    /** "Gender of this character" */
    HamGender mGender; // 0x310
    int mAnimationState; // 0x314 - animation regulator state, initialized to 0
    /** "Updates the character's animation even though showing is set to FALSE.
        Useful for rendering the character to a texture." */
    bool mPollWhenHidden; // 0x318
    /** "True if the internal TexBlenders are working." */
    bool mTexBlendersActive; // 0x319
    ObjPtrList<CharWeightable> mIKEffectors; // 0x31c
    float mBaseLipsyncOffset; // 0x330
    ObjectDir *mNeutralSkelDir; // 0x334
    CharServoBone *mSkeletonBones; // 0x338
    ObjPtr<RndMesh> mCrewCardMesh; // 0x33c
    bool mUseCameraSkeleton; // 0x350
};
