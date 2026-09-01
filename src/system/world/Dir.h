#pragma once
#include "LightPreset.h"
#include "LightPresetManager.h"
#include "PhysicsManager.h"
#include "ThreeDSoundManager.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Tex.h"
#include "ui/PanelDir.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/MemMgr.h"
#include "world/CameraManager.h"
#include "world/CameraShot.h"
#include "world/LightHue.h"
#include "world/LightPreset.h"

/**
 * @brief An ObjectDir dedicated to holding world objects.
 * Original _objects description:
 * "A WorldDir contains world objects."
 */
class WorldDir : public PanelDir {
public:
    struct PresetOverride {
        PresetOverride(Hmx::Object *owner) : preset(owner), hue(owner) {}
        void Sync(bool);

        /** "Subdir preset to modify" */
        ObjPtr<LightPreset> preset; // 0x0
        /** "Hue texture to use" */
        ObjPtr<LightHue> hue; // 0xc
    };

    struct BitmapOverride {
        BitmapOverride(Hmx::Object *owner) : original(owner), replacement(owner) {}
        void Sync(bool);

        /** "Subdir texture to replace" */
        ObjPtr<RndTex> original; // 0x0
        /** "Curdir texture to replace with" */
        ObjPtr<RndTex> replacement; // 0xc
    };

    struct MatOverride {
        MatOverride(Hmx::Object *owner) : mesh(owner), mat(owner), mat2(owner) {}
        void Sync(bool);

        /** "Subdir mesh to modify" */
        ObjPtr<RndMesh> mesh; // 0x0
        /** "Curdir material to set" */
        ObjPtr<RndMat> mat; // 0xc
        ObjPtr<RndMat> mat2; // 0x18
    };

    WorldDir();
    // Hmx::Object
    virtual ~WorldDir();
    OBJ_CLASSNAME(WorldDir);
    OBJ_SET_TYPE(WorldDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // ObjectDir
    virtual void SyncObjects();
    // RndDrawable
    virtual void DrawShowing();
    // RndPollable
    virtual void Poll();
    virtual void Enter();

    OBJ_MEM_OVERLOAD(0x1E)
    NEW_OBJ(WorldDir)

    static void Init();

    void ClearDeltas();
    void SetCrowds(ObjVector<CamShotCrowd> &);
    CameraManager *GetCameraManager() const {
        return const_cast<CameraManager *>(&mCameraManager);
    }
#ifdef WORLDDIR_DC3_TAIL
    PhysicsManager *GetPhysicsManager() const { return mPhysicsMgr; }
#endif
    LightPresetManager &GetLightPresetMgr() { return mLightPresetMgr; }
    RndDir *GetHUD() const { return mHUD; }
    void SetHUD(RndDir *hud) { mHUD = hud; }
    // Retail Game::EnableWorldPolling pokes this bool (WorldDir+0x381).
    void SetPollCamera(bool b) { mPollCamera = b; }

#ifdef WORLDDIR_DC3_TAIL
    DataNode OnGetPhysicsManager(const DataArray *);
#endif

private:
    // Retail ClosetPanel::FinishLoad (0x825ED3F8) takes &mCameraManager directly
    // off the WorldDir it dynamic_casts to; no accessor call in the target bytes.
    friend class ClosetPanel;

    void SyncHUD();
    void SyncHides(bool);
    void SyncBitmaps(bool);
    void SyncCamShots(bool);
    void SyncMats(bool);
    void SyncPresets(bool);
    void AccumulateDeltas(float *const);
    void RestoreDeltas(float *const);

protected:
    // Retail tail layout Ghidra-verified against the retail WorldDir ctor
    // (0x824BC930; factory 0x824BD600 allocates 0x3d8). Matches the rb3-Wii
    // member order exactly: NO ThreeDSoundManager / PhysicsManager (DC3-only),
    // instance mGlowMat (not static), mCrowds directly after the PS3 lists,
    // mPollCamera after mFirstPoll (retail inits it true; ctor byte +0x381=1),
    // no mExplicitPostProc (retail vtordisp sits at +0x3a0).
    ObjList<PresetOverride> mPresetOverrides; // 0x238
    ObjList<BitmapOverride> mBitmapOverrides; // 0x244
    ObjList<MatOverride> mMatOverrides; // 0x250
    /** "Subdir objects to hide" */
    ObjPtrList<RndDrawable> mHideOverrides; // 0x25c
    /** "Subdir camshots to inhibit" */
    ObjPtrList<CamShot> mCamShotOverrides; // 0x270
    /** "Things to show when ps3_per_pixel on CamShot" */
    ObjPtrList<RndDrawable> mPS3PerPixelShows; // 0x284
    /** "Things to hide when ps3_per_pixel on CamShot" */
    ObjPtrList<RndDrawable> mPS3PerPixelHides; // 0x298
    ObjPtrList<WorldCrowd> mCrowds; // 0x2ac
    RndMat *mGlowMat; // 0x2c0 (instance, created in ctor — retail 0x824BC930)
    /** "HUD Preview Dir" */
    FilePath mHUDFilename; // 0x2c4
    RndDir *mHUDDir; // 0x2d0
    /** "Whether to draw the HUD preview" */
    bool mShowHUD; // 0x2d4
    /** "hud to be drawn last" */
    ObjPtr<RndDir> mHUD; // 0x2d8
    CameraManager mCameraManager; // 0x2e4 (by value, sizeof 0x34)
    LightPresetManager mLightPresetMgr; // 0x318 (sizeof 0x54)
    bool mEchoMsgs; // 0x36c
    float mDeltaSincePoll[4]; // 0x370
    bool mFirstPoll; // 0x380
    bool mPollCamera; // 0x381 (retail Game::EnableWorldPolling writes this)
    /** "The first light preset to start" */
    ObjPtr<LightPreset> mTestLightPreset1; // 0x384
    /** "The second light preset to start" */
    ObjPtr<LightPreset> mTestLightPreset2; // 0x390
    /** "animation time in beats" */
    float mTestAnimTime; // 0x39c
#ifdef WORLDDIR_DC3_TAIL
    // DC3-only members, NOT present in retail RB3 (kept for reference).
    ThreeDSoundManager m3DSoundMgr;
    PhysicsManager *mPhysicsMgr;
    bool mNeedPhysicsEnter;
    /** "TRUE if we explicitly do the postprocing" */
    bool mExplicitPostProc;
#endif
};

void SetTheWorld(WorldDir *);

extern WorldDir *TheWorld;
