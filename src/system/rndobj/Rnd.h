#pragma once
#include "math/Color.h"
#include "math/Geo.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Keyboard.h"
#include "rndobj/Console.h"
#include "rndobj/CubeTex.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Lit.h"
#include "rndobj/Overlay.h"
#include "rndobj/PostProc.h"
#include "rndobj/Trans.h"
#include "rndobj/Watcher.h"
#include "utl/MemMgr.h"

class RndCam;
class RndFlare;
class RndMat;
class RndTex;
class UIPanel;

class ModalKeyListener : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *, bool);

    DataNode OnMsg(const KeyboardKeyMsg &);
};

class Rnd : public Hmx::Object, public RndOverlay::Callback {
public:
    enum Aspect {
        kSquare,
        kRegular,
        kWidescreen,
        kLetterbox
    };
    enum DefaultTextureType {
        kDefaultTex_Black = 0,
        kDefaultTex_White = 1,
        kDefaultTex_WhiteTransparent = 2,
        kDefaultTex_FlatNormal = 3,
        kDefaultTex_Unk4 = 4,
        kDefaultTex_Gradient = 5,
        kDefaultTex_Hue = 6,
        kDefaultTex_Error = 7,
        kDefaultTex_Max = 8
    };
    enum DrawMode {
        kDrawNormal = 0,
        kDrawShadowDepth = 1,
        kDrawExtrude = 2,
        kDrawShadowColor = 3,
        kDrawOcclusion = 4,
        kDrawOcclusionDepth = 5,
        kDrawVelocity = 6
    };

    struct PointTest {
        int x; // 0x0 - screen x position
        int y; // 0x4 - screen y position
        int z; // 0x8 - raw depth value
        RndFlare *mFlare; // 0xc
    };

    struct CompressTextureCallback;

    struct CompressTexDesc {
        CompressTexDesc(RndTex *tex, RndTex::AlphaCompress a, CompressTextureCallback *cb)
            : tex(nullptr, tex), alpha(a), callback(cb) {}
        ~CompressTexDesc();

        MEM_OVERLOAD(CompressTexDesc, 0x1E4)

        ObjPtr<RndTex> tex;
        RndTex::AlphaCompress alpha;
        CompressTextureCallback *callback;
    };

    struct CompressTextureCallback {
        virtual ~CompressTextureCallback() {}
        virtual void TextureCompressed(intptr_t) = 0;
    };

    Rnd();
    virtual DataNode Handle(DataArray *, bool);
    virtual void PreInit();
    virtual void Init();
    virtual void ReInit() {}
    virtual void Terminate();
    virtual void SetClearColor(const Hmx::Color &c) { mClearColor = c; }
    const Hmx::Color &GetClearColor() const { return mClearColor; }
    virtual void Clear(unsigned int, const Hmx::Color &) = 0;
    virtual void ForceColorClear() {}
    virtual void ScreenDump(const char *);
    virtual void ScreenDumpUnique(const char *);
    virtual void DrawRect(
        const Hmx::Rect &,
        const Hmx::Color &,
        RndMat *,
        const Hmx::Color *,
        const Hmx::Color *
    ) {}
    virtual Vector2 &
    DrawString(const char *, const Vector2 &, const Hmx::Color &, bool); // 0x80
    virtual void DrawLine(const Vector3 &, const Vector3 &, const Hmx::Color &, bool) {
    } // 0x84
    virtual void BeginDrawing();
    virtual void EndDrawing();
    virtual void MakeDrawTarget() {}
    virtual void SetSync(int sync) { mSync = sync; }
    virtual int GetSync() { return mSync; }
    virtual int NumDrawPasses() const { return 1; }
    virtual void BeginDrawPass() {}
    virtual void EndDrawPass() {}
    virtual unsigned int GetFrameID() const { return mFrameID; }
    virtual bool ShouldDrawPanel(const UIPanel *) { return true; }
    virtual RndTex *GetCurrentFrameTex(bool) { return nullptr; }
    virtual void ReleaseOwnership() {}
    virtual void AcquireOwnership() {}
    virtual void SetShadowMap(RndTex *, RndCam *, const Hmx::Color *) {}
    virtual void SetGSTiming(bool b) { mGsTiming = b; }
    virtual void CaptureNextGpuFrame() {}
    virtual void RemovePointTest(RndFlare *);
    virtual bool HasDeviceReset() const { return false; }
    virtual void SetAspect(Aspect a) { mAspect = a; }
    virtual float YRatio();
    virtual RndTex *GetShadowMap() { return nullptr; }
    virtual RndCam *GetShadowCam() { return nullptr; }
    virtual void SetShrinkToSafeArea(bool shrink) { mShrinkToSafe = shrink; }
    bool ShrinkToSafeArea() const { return mShrinkToSafe; }
    virtual void SetInGame(bool game) { mInGame = game; }
    bool InGame() const { return mInGame; }
    virtual int BeginQuery(RndDrawable *) { return -1; }
    virtual bool EndQuery(int) { return false; }
    virtual bool VisibleSets(std::vector<RndDrawable *> &, std::vector<RndDrawable *> &) {
        return false;
    }

    bool TimersShowing() { return mTimersOverlay->Showing(); }
    int Width() const { return mWidth; }
    int Height() const { return mHeight; }
    int Bpp() const { return mScreenBpp; }
    int DrawCount() const { return mDrawCount; }
    bool Drawing() const { return mDrawing; }
    bool WorldEnded() const { return mWorldEnded; }
    bool GetReleaseImmediate() { return mReleaseImmediate; }
    Aspect GetAspect() const { return mAspect; }
    DrawMode GetDrawMode() { return mDrawMode; }
    void SetDrawMode(DrawMode d) { mDrawMode = d; }
    RndCam *GetDefaultCam() const { return mDefaultCam; }
    RndCam *GetWorldCamCopy() const { return mWorldCamCopy; }
    ProcessCmd ProcCmds() const { return mProcCmds; }
    bool DisablePP() const { return mDisablePostProc; }
    DataArray *Font() const { return mFont; }
    RndEnviron *DefaultEnv() const { return mDefaultEnv; }
    RndMat *DefaultMat() const { return mDefaultMat; }
    RndCubeTex *DefaultCubeTexWhite() const { return mDefaultCubeTexWhite; }
    RndTex *GetDefaultTex(DefaultTextureType type) const { return mDefaultTex[type]; }
    RndMat *OverlayMat() const { return mOverlayMat; }
    bool ResourceCached() const { return mResourceCached; }
    bool VerboseTimers() const { return mVerboseTimers; }
    float DrawMs() { return mDrawTimer.GetLastMs(); }
    void ShowConsole(bool);
    bool ConsoleShowing();
    void EndWorld();
#ifdef HX_NATIVE
    virtual void ClearDepthForOverlay() {} // Override in WgpuRnd
#endif
    void SetShowTimers(bool, bool);
    void SetProcAndLock(bool);
    bool ProcAndLock() const;
    void ResetProcCounter();
    bool GetEvenOddDisabled() const;
    void SetEvenOddDisabled(bool);
    void DrawRectScreen(
        const Hmx::Rect &,
        const Hmx::Color &,
        RndMat *,
        const Hmx::Color *,
        const Hmx::Color *
    );
    const Vector2 &
    DrawStringScreen(const char *c, const Vector2 &v, const Hmx::Color &color, bool b4);
    RndPostProc *GetPostProcOverride();
    RndPostProc *GetSelectedPostProc();
    void TestPoint(const Vector3 &, RndFlare *);
    void CopyWorldCam(RndCam *);
    void RegisterPostProcessor(PostProcessor *);
    void UnregisterPostProcessor(PostProcessor *);
    void SetPostProcOverride(RndPostProc *);
    void SetPostProcBlacklightOverride(RndPostProc *);
    void PreClearDrawAddOrRemove(RndDrawable *, bool, bool);
    RndTex *GetNullTexture();
    int CompressTexture(RndTex *, RndTex::AlphaCompress, CompressTextureCallback *);
    void Modal(Debug::ModalType &, FixedString &, bool);
    void PushClipPlanes(ObjPtrVec<RndTransformable> &planes) {
        if (planes.size() > 0) {
            PushClipPlanesInternal(planes);
        }
    }
    void PopClipPlanes(ObjPtrVec<RndTransformable> &planes) {
        if (planes.size() > 0) {
            PopClipPlanesInternal(planes);
        }
    }

    static int sPostProcPanelCount;

protected:
    friend class NgPostProc;
    friend class RndSoftParticleBuffer;

    virtual void PushClipPlanesInternal(ObjPtrVec<RndTransformable> &) {}
    virtual void PopClipPlanesInternal(ObjPtrVec<RndTransformable> &) {}
    virtual void DoWorldBegin();
    virtual void DoWorldEnd();
    virtual void DoPostProcess();
    virtual void DrawPreClear();
    virtual bool CanModal(Debug::ModalType) { return false; }
    virtual void ModalDraw(Debug::ModalType, const char *) {}
    virtual unsigned int GetDefaultTexBitmapOrder() const { return 0; }

    virtual float UpdateOverlay(RndOverlay *, float);

    void UpdateRate();
    void UpdateHeap();
    float DrawTimers(float);
    void CreateDefaults();
    void SetupFont();
    void CreateCubeTextures();
    RndTex *CreateDefaultTexture(DefaultTextureType);

    DataNode OnShowConsole(const DataArray *);
    DataNode OnToggleTimers(const DataArray *);
    DataNode OnToggleOverlayPosition(const DataArray *);
    DataNode OnToggleTimersVerbose(const DataArray *);
    DataNode OnToggleOverlay(const DataArray *);
    DataNode OnShowOverlay(const DataArray *);
    DataNode OnSetSphereTest(const DataArray *);
    DataNode OnClearColorR(const DataArray *);
    DataNode OnClearColorG(const DataArray *);
    DataNode OnClearColorB(const DataArray *);
    DataNode OnClearColorPacked(const DataArray *);
    DataNode OnSetClearColor(const DataArray *);
    DataNode OnSetClearColorPacked(const DataArray *);
    DataNode OnScreenDump(const DataArray *);
    DataNode OnScreenDumpUnique(const DataArray *);
    DataNode OnScaleObject(const DataArray *);
    DataNode OnReflect(const DataArray *);
    DataNode OnToggleHeap(const DataArray *);
    DataNode OnOverlayPrint(const DataArray *);
    DataNode OnToggleShowMetaMatErrors(const DataArray *);
    DataNode OnToggleShowShaderErrors(const DataArray *);
    DataNode OnToggleWatch(const DataArray *);

    Hmx::Color mClearColor; // 0x30
    int mWidth; // 0x40
    int mHeight; // 0x44
    int mScreenBpp; // 0x48
    int mDrawCount; // 0x4c
    Timer mDrawTimer; // 0x50
    RndOverlay *mTimersOverlay; // 0x80
    RndOverlay *mRateOverlay; // 0x84
    RndOverlay *mHeapOverlay; // 0x88
    RndOverlay *mWatchOverlay; // 0x8c
    Watcher mWatcher; // 0x90
    RndOverlay *mStatsOverlay; // 0xcc
    RndConsole *mConsole; // 0xd0
    RndMat *mDefaultMat; // 0xd4
    RndMat *mOverlayMat; // 0xd8
    RndMat *mOverdrawMat; // 0xdc
    RndCam *mDefaultCam; // 0xe0
    RndCam *mWorldCamCopy; // 0xe4
    RndEnviron *mDefaultEnv; // 0xe8
    RndLight *mDefaultLit; // 0xec
    RndTex *mDefaultTex[kDefaultTex_Max]; // 0xf0 - 0x10c, inclusive
    RndCubeTex *mDefaultCubeTexBlack;
    RndCubeTex *mDefaultCubeTexWhite;
    float mRateTotal;
    int unk11c;
    int mRateCount;
    unsigned int mFrameID; // 0x124
    const char *mRateGate; // 0x128
    DataArray *mFont; // 0x12c
    int mSync; // 0x130
    bool mGsTiming; // 0x134
    bool mShowSafeArea; // 0x135
    bool mDrawing; // 0x136
    bool mWorldEnded; // 0x137
    Aspect mAspect; // 0x138
    DrawMode mDrawMode; // 0x13c
    bool mResourceCached; // 0x140
    bool mShowShaderCost; // 0x141
    bool mShrinkToSafe; // 0x142
    bool mInGame; // 0x143
    bool mVerboseTimers; // 0x144
    bool mDisablePostProc; // 0x145
    bool unk146;
    bool mWorldCamCopied; // 0x147 - set by CopyWorldCam, cleared by DoWorldEnd
    bool unk148;
    void (*mWorldEndCallback)(); // 0x14c - funcptr
    void (*unk150)(); // 0x150 - another funcptr
    std::list<PointTest> mPointTests; // 0x154
    std::list<PostProcessor *> mPostProcessors; // 0x15c
    ObjPtr<RndPostProc> mPostProcOverride; // 0x164
    ObjPtr<RndPostProc> mPostProcBlackLightOverride; // 0x178
    ObjPtrList<RndDrawable> mPreClearDraws; // 0x18c
    ObjPtrList<RndDrawable> mDraws; // 0x1a0

public:
    bool mReleaseImmediate; // 0x1b4

protected:
    ProcCounter mProcCounter; // 0x1b8
    ProcessCmd mProcCmds; // 0x1d0
    ProcessCmd mLastProcCmds; // 0x1d4
    std::list<CompressTexDesc *> mCompressTexQueue; // 0x1d8
};

extern Rnd &TheRnd;
