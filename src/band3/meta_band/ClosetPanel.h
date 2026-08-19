#pragma once
#include "game/BandUser.h"
#include "meta/DeJitterPanel.h"
#include "meta_band/ClosetMgr.h"
#include "rndobj/TexRenderer.h"
#include "world/CameraManager.h"
#include "world/CameraShot.h"

class ClosetPanel : public DeJitterPanel {
public:
    ClosetPanel();
    OBJ_CLASSNAME(ClosetPanel);
    OBJ_SET_TYPE(ClosetPanel);
    virtual DataNode Handle(DataArray *, bool);
    // NOTE(laneCD8): do NOT re-add `virtual ~ClosetPanel() {}`. A user-declared
    // destructor forces the compiler-generated vbase-dtor helper ??_DClosetPanel
    // OUT OF LINE; retail inlines it into ??_GClosetPanel (destroying
    // ~DeJitterPanel at +0x94 and ~Object at +0xa4 directly). Implicit = 100%.
    virtual void Draw();
    virtual void Enter();
    virtual void Exit();
    virtual bool Exiting() const;
    virtual void Poll();
    virtual void Load();
    virtual void Unload();
    virtual void FinishLoad();

    CamShot *GetCurrentShot();
    void CycleCamera();
    void GotoArtMakerShot();
    void LeaveArtMakerShot();
    void TakePortrait();
    void GotoShot(Symbol);
    void SetPortraitRenderer(RndTexRenderer *);
    BandUser *GetUser() const { return mClosetMgr->GetUser(); }
    NEW_OBJ(ClosetPanel);
    static void Init() { REGISTER_OBJ_FACTORY(ClosetPanel); }

    ClosetMgr *mClosetMgr; // 0x90
    CameraManager *mCameraManager; // 0x94
    RndTexRenderer *mPortraitRenderer; // 0x98
    int mPortraitState; // 0x9c
};
