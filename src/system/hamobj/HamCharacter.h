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
    DRAW_DC3_VIRTUAL void Draw();
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

    String mCampaignVO; // 0x268
    Hmx::Object *mCampaignVOBank; // 0x274
    ObjectDir *mCampaignVODir; // 0x278
    FileMerger *mFileMerger; // 0x27c
    /** "which character to look like" */
    Symbol mOutfit; // 0x280
    Waypoint *mWaypoint; // 0x284
    /** "where to load outfits from" */
    Symbol mOutfitDir; // 0x288
    bool mIsCampaignChar; // 0x28c
    /** "Draws a 6 foot square box around the character teleport point" */
    bool mShowBox; // 0x28d
    bool mNeedsAcquirePose; // 0x28e
    ObjPtr<CharEyes> mEyes; // 0x290
    /** "Gender of this character" */
    HamGender mGender; // 0x29c
    int mAnimationState; // 0x2a0 - animation regulator state, initialized to 0
    /** "Updates the character's animation even though showing is set to FALSE.
        Useful for rendering the character to a texture." */
    bool mPollWhenHidden; // 0x2a4
    /** "True if the internal TexBlenders are working." */
    bool mTexBlendersActive; // 0x2a5
    ObjPtrList<CharWeightable> mIKEffectors; // 0x2a8
    float mBaseLipsyncOffset; // 0x2bc
    ObjectDir *mNeutralSkelDir; // 0x2c0
    CharServoBone *mSkeletonBones; // 0x2c4
    ObjPtr<RndMesh> mCrewCardMesh; // 0x2c8
    bool mUseCameraSkeleton; // 0x2d4
    /** "Props to show and hide for cut scenes". In DC3 this lived on the base
        Character; RB3 retail's base Character has no showable-props member (the
        "showable_props"/"prop_N_showing" property strings are absent from the
        retail XEX, and Character::Save/Copy stop at mFrozen). Keep it local to
        HamCharacter so GetPropShowing/SetPropShowing stay self-consistent
        without inflating sizeof(Character) for every other subclass. */
    DrawPtrVec mShowableProps;
    /** Retail RB3's HamCharacter carries 96 bytes of trailing state DC3 dropped.
        Size is load-bearing: it places the Hmx::Object virtual-base subobject where
        retail does (+0x60), fixing the ??_G scalar-deleting-destructor this-adjust. */
    char mDroppedTrailingState_[0x60];
};
