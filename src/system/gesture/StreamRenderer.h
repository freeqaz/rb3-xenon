#pragma once
#include "math/Color.h"
#include "math/DoubleExponentialSmoother.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/ShaderOptions.h"
#include "rndobj/Tex.h"
#include "utl/MemMgr.h"

enum StreamDisplay {
    /** "Color output of camera" */
    kStreamColor = 0,
    /** "Depth buffer output" */
    kStreamBasicDepth = 1,
    /** "DC1 visualizer" */
    kStreamPlayerDepthVis = 2,
    /** "DC1 player helper frame" */
    kStreamPlayerDepthShell = 3,
    /** "DC2 player helper frame" */
    kStreamPlayerDepthShell2 = 4,
    /** "Convert color output to black and white" */
    kStreamBlackAndWhite = 5,
    /** "RGB player without background" */
    kStreamPlayerGreenscreen = 6,
    /** "RGB player with depth buffer" */
    kStreamPlayerDepthGreenscreen = 7,
    /** "Color output with edge detection on background and radial blur on players" */
    kStreamCrewPhoto = 8
};

// size 0x3b4
/** "Renders Natal stream textures into a texture." */
class StreamRenderer : public RndDrawable {
public:
    // Hmx::Object
    virtual ~StreamRenderer();
    OBJ_CLASSNAME(StreamRenderer);
    OBJ_SET_TYPE(StreamRenderer);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndDrawable
    virtual void DrawShowing();
    virtual void DrawPreClear() { DrawToTexture(); }
    virtual void UpdatePreClearState();

    OBJ_MEM_OVERLOAD(0x25);
    NEW_OBJ(StreamRenderer);

    void SetPinkPlayer(int);
    void SetBluePlayer(int);
    void SetCrewPhotoPlayerDetected(int, bool);
    ShaderType GetShaderType() const;

    void SetOutputTex() {
        if (mForceMips && mOutputTex) {
            mOutputTex->SetBitmap(
                mOutputTex->Width(),
                mOutputTex->Height(),
                mOutputTex->Bpp(),
                mOutputTex->GetType(),
                true,
                nullptr
            );
        }
    }

    static void Init();
    static void Terminate();

private:
    void SetCrewPhotoPlayerCenters();

protected:
    StreamRenderer();

    void DrawToTexture();
    void SetCrewPhotoHorizontalColor(DataArray *);
    void SetCrewPhotoVerticalColor(DataArray *);
    DataNode OnGetRenderTextures(DataArray *);

    static RndCam *mCam;
    static RndTex *mBlurRT[2];

    /** "Texture to write to" */
    ObjPtr<RndTex> mOutputTex; // 0x24
    /** "Generate mip maps for the texture." */
    bool mForceMips; // 0x30
    /** "Natal buffer to display" */
    StreamDisplay mDisplay; // 0x34
    /** "Number of times to blur the player silhouette texture".
        Ranges from 0 to 64. */
    int mNumBlurs; // 0x38
    /** "Player 1 color" */
    Hmx::Color mPlayer1DepthColor; // 0x3c
    /** "Player 2 color" */
    Hmx::Color mPlayer2DepthColor; // 0x4c
    /** "Player 3 color" */
    Hmx::Color mPlayer3DepthColor; // 0x5c
    /** "Player 4 color" */
    Hmx::Color mPlayer4DepthColor; // 0x6c
    /** "Player 5 color" */
    Hmx::Color mPlayer5DepthColor; // 0x7c
    /** "Player 6 color" */
    Hmx::Color mPlayer6DepthColor; // 0x8c
    /** "Color for non-player pixels (i.e. the background)" */
    Hmx::Color mPlayerDepthNobody; // 0x9c
    /** "1D palette for player 1 depth" */
    ObjPtr<RndTex> mPlayer1DepthPalette; // 0xac
    /** "1D palette for player 2 depth" */
    ObjPtr<RndTex> mPlayer2DepthPalette; // 0xb8
    /** "1D palette for players 3-6" */
    ObjPtr<RndTex> mPlayerOtherDepthPalette; // 0xc4
    /** "1D palette for background depth " */
    ObjPtr<RndTex> mBackgroundDepthPalette; // 0xd0
    /** "Starting point for p1 palette". Ranges from -1 to 1. */
    float mPlayer1DepthPaletteOffset; // 0xdc
    /** "Starting point for p2 palette". Ranges from -1 to 1. */
    float mPlayer2DepthPaletteOffset; // 0xe0
    /** "Starting point for p3-p6 palettes". Ranges from -1 to 1. */
    float mPlayerOtherDepthPaletteOffset; // 0xe4
    /** "Starting point for palette". Ranges from -1 to 1. */
    float mBackgroundDepthPaletteOffset; // 0xe8
    bool mDrawPreClear; // 0xec
    /** "Always render the image, even there is no new depth/color buffer this frame." */
    bool mForceDraw; // 0xed
    /** "Assign colors by index instead of giving preference to Player 1 and Player 2." */
    bool mStaticColorIndices; // 0xee
    /** "StreamRenderer will output this test texture.
        Only works on the PC." */
    ObjPtr<RndTex> mPCTestTex; // 0xf0
    /** "Used to lag the depth image by one frame
        to better line up with the color image for greenscreening" */
    bool mLagPrimaryTexture; // 0xfc
    RndTex *mLaggedPrimaryTexture[2]; // 0x100
    int unk154; // 0x108
    float mCrewPhotoEdgeIterations; // 0x10c
    float mCrewPhotoEdgeOffset; // 0x110
    Hmx::Color mCrewPhotoHorizontalColor; // 0x114
    Hmx::Color mCrewPhotoVerticalColor; // 0x124
    float mCrewPhotoBlurStart; // 0x134
    float mCrewPhotoBlurWidth; // 0x138
    float mCrewPhotoBlurIterations; // 0x13c
    float mCrewPhotoBackgroundBrightness; // 0x140
    Hmx::Color mCrewPhotoPlayerDetected1; // 0x144
    Hmx::Color mCrewPhotoPlayerDetected2; // 0x154
    Vector4 mCrewPhotoPlayerCenters[6]; // 0x164
    Vector3DESmoother mSmoothers[6]; // 0x1c4
};
