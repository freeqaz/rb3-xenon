#pragma once
#include "math/Mtx.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "rndobj/Tex.h"
#include "utl/MemMgr.h"
#include "world/Spotlight.h"
#include "world/SpotlightDrawer.h"
#include "xdk/D3D9.h"

class NgSpotlightDrawer : public SpotlightDrawer {
public:
    class SpotlightResources {
    public:
        SpotlightResources()
            : unk4(0), unk8(0), mDensityMap(0), unk10(0), unk14(0), unk18(0) {}
        virtual ~SpotlightResources();
        void Clear();

        MEM_OVERLOAD(SpotlightResources, 0x3B);

        D3DResource *unk4;
        RndTex *unk8;
        RndTex *mDensityMap;
        RndTex *unk10;
        RndTex *unk14;
        RndTex *unk18;
    };

    NgSpotlightDrawer();
    // Hmx::Object
    virtual ~NgSpotlightDrawer();
    // Retail registers the platform subclass under its BASE's DTA name, so
    // milo data authored as `SpotlightDrawer` instantiates this NG class.
    // Evidence: retail band.exe contains no "NgSpotlightDrawer" C string at
    // all, while 0x824d1848 -- called by ClassName/SetType/Init@NgSpotlightDrawer
    // -- builds "SpotlightDrawer".  DC3's SpotlightDrawer_NG.h agrees.
    OBJ_CLASSNAME(SpotlightDrawer)
    OBJ_SET_TYPE_ENGINE(NgSpotlightDrawer)

    // PostProcessor
    virtual void EndWorld();
    virtual void DoPost();
    // OPEN (lane BS-3, not fixed): "NgSpotlightDrawer" is absent from retail
    // band.exe's string pool, so retail cannot return it here either -- RB3
    // likely has no such override (DC3, which is newer, does).  Left alone
    // deliberately: ?GetProcType@ has no target_symbol_map row anywhere, so the
    // member is unpaired and unadjudicable, and guessing risks a regression for
    // zero metric.  Reopen if GetProcType ever gets mapped.
    virtual char const *GetProcType() { return "NgSpotlightDrawer"; }

    NEW_OBJ(NgSpotlightDrawer);

    static void Init();

    SpotlightResources &SR() {
        MILO_ASSERT(sSharedResources, 0xA0);
        return *sSharedResources;
    }

    void RenderScene();

protected:
    // RndDrawable
    virtual void SetAmbientColor(Hmx::Color const &);
    virtual void DrawBeams(SpotlightEntry *, SpotlightEntry *const &) {}
    virtual void ClearPostDraw();
    virtual void ClearPostProc();

    static int RTWidth();
    static int RTHeight();
    static bool CheckSharedResources();
    static bool CheckRTs(SpotlightResources *);

    static SpotlightResources *sSharedResources;
    static bool sActiveFrame;

    bool RestoreCam();
    bool CheckFogTexture();
    void SetXSectionTexture(Spotlight::BeamDef const &);
    void SetupFogDensityMap();
    void RenderFogProxy();
    void RenderSphere(Spotlight *);
    void RenderSheet(Spotlight *);
    void SetupXSection(Spotlight *, Spotlight::BeamDef const &);
    void RenderConeDefs(Spotlight *, Hmx::Color const &);
    void SetupFogDensityState();
    void RenderCone(Spotlight *);
    void RenderBeams(Hmx::Matrix4 const &);
    bool CheckCam();
    void BlurRT(float, float);
    void BlurRT();
    void SetupForPostProcess();

    RndCam *mSpotCam; // 0x94
    ObjPtr<RndCam> mSavedCam; // 0x98
    RndTex *mFogDensityMap; // 0xac
    bool unkb0; // 0xb0
};
