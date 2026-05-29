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
#ifdef HX_NATIVE
    DataNode OnToggleWatch(const DataArray *);
#endif
    DataNode OnOverlayPrint(const DataArray *);
    DataNode OnToggleShowMetaMatErrors(const DataArray *);
    DataNode OnToggleShowShaderErrors(const DataArray *);

    Hmx::Color mClearColor; // 0x30
    int mWidth; // 0x40
    int mHeight; // 0x44
    int mScreenBpp; // 0x48
    int mDrawCount; // 0x4c
    Timer mDrawTimer; // 0x50
    // Retail X360/RB3 layout (verified via Rnd ctor fn_82402FA0 + OnShowConsole
    // loading mConsole at 0x90): the four overlay pointers sit at 0x80..0x8c
    // followed by mConsole at 0x90. RB3 retail has NO Watcher subsystem in Rnd
    // (confirmed: rb3-Wii Rnd.h/.cpp carry no mWatcher/mWatchOverlay at all).
    // DC3 added mWatchOverlay (+4) + inline Watcher mWatcher (+0x3c) = +0x40,
    // which shifted every member from mStatsOverlay onward up 64 bytes vs retail.
    RndOverlay *mTimersOverlay; // 0x80
    RndOverlay *mRateOverlay; // 0x84
    RndOverlay *mHeapOverlay; // 0x88
#ifdef HX_NATIVE
    RndOverlay *mWatchOverlay; // native-only (DC3 watcher subsystem)
    Watcher mWatcher; // native-only
#endif
    RndOverlay *mStatsOverlay; // 0x8c
    RndConsole *mConsole; // 0x90
    RndMat *mDefaultMat; // 0x94
    RndMat *mOverlayMat; // 0x98
    RndMat *mOverdrawMat; // 0x9c
    RndCam *mDefaultCam; // 0xa0
    RndCam *mWorldCamCopy; // 0xa4
    RndEnviron *mDefaultEnv; // 0xa8
    RndLight *mDefaultLit; // 0xac
    RndTex *mDefaultTex[kDefaultTex_Max]; // 0xb0 - 0xcc, inclusive (do-loop, 8 words)
    RndCubeTex *mDefaultCubeTexBlack; // 0xd0
    RndCubeTex *mDefaultCubeTexWhite; // 0xd4
    float mRateTotal; // 0xd8
    int unk11c; // 0xdc
    int mRateCount; // 0xe0 (=5)
    unsigned int mFrameID; // 0xe4
    const char *mRateGate; // 0xe8
    DataArray *mFont; // 0xec
    int mSync; // 0xf0 (=1)
    bool mGsTiming; // 0xf4
    bool mShowSafeArea; // 0xf5
    bool mDrawing; // 0xf6
    bool mWorldEnded; // 0xf7 (=1)
    Aspect mAspect; // 0xf8
    DrawMode mDrawMode; // 0xfc
    bool mResourceCached; // 0x100
    bool mShowShaderCost; // 0x101
    bool mShrinkToSafe; // 0x102 (=1)
    bool mInGame; // 0x103
    bool mVerboseTimers; // 0x104
    bool mDisablePostProc; // 0x105
    bool unk146; // 0x106
    bool mWorldCamCopied; // 0x107 - set by CopyWorldCam, cleared by DoWorldEnd
    bool unk148; // 0x108
    void (*mWorldEndCallback)(); // 0x10c - funcptr
    void (*unk150)(); // 0x110 - another funcptr
    std::list<PointTest> mPointTests; // 0x114
    std::list<PostProcessor *> mPostProcessors; // 0x11c
    ObjPtr<RndPostProc> mPostProcOverride; // 0x124
    ObjPtr<RndPostProc> mPostProcBlackLightOverride; // 0x138
    ObjPtrList<RndDrawable> mPreClearDraws; // 0x14c
    ObjPtrList<RndDrawable> mDraws; // 0x160

public:
    bool mReleaseImmediate; // 0x174

protected:
    ProcCounter mProcCounter; // 0x178
    ProcessCmd mProcCmds; // 0x190
    ProcessCmd mLastProcCmds; // 0x194
    std::list<CompressTexDesc *> mCompressTexQueue; // 0x198
};

extern Rnd &TheRnd;
