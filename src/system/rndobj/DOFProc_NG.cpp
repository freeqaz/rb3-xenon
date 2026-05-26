#include "rndobj/DOFProc_NG.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rnddx9/RenderState.h"
#include "rndobj/Cam.h"
#include "rndobj/DOFProc.h"
#include "rndobj/Mat.h"
#include "rndobj/PostProc.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/ShaderOptions.h"
#include "rndobj/Tex.h"
#include "ui/UI.h"

// Static tap arrays for DOF blur kernel (horizontal and vertical)
// Tap coordinate pairs (x, y) for 8 rotated blur sample offsets.
// Each array is initialized once (guarded by bits in sBlurTapInitFlags).
static float sHorzBlurTaps[8][2];
static float sVertBlurTaps[8][2];
static unsigned int sBlurTapInitFlags; // bit 1 = vert initialized, bit 2 = horz initialized

// DOF blur kernel scale factor — scales UV tap offsets by width/height
static const float sDOFWidthFactor = 0.666f;

NgDOFProc::NgDOFProc()
    : mEnabled(0), mDepthOfFieldScale(1), mDepthOfFieldBias(0), mFocalPlane(1), mBlurDepth(1), mMinBlur(0),
      mMaxBlur(1) {
    TheRnd.RegisterPostProcessor(this);
    MILO_ASSERT(TheNgRnd.PreProcessTexture(), 0x41);
    int w = TheNgRnd.PreProcessTexture()->Width() >> 2;
    int h = TheNgRnd.PreProcessTexture()->Height() >> 2;
    mBlurTex[0] = Hmx::Object::New<RndTex>();
    mBlurTex[0]->SetBitmap(w, h, TheRnd.Bpp(), RndTex::kRenderedNoZ, false, nullptr);
    mBlurTex[1] = mBlurTex[0];
}

bool NgDOFProc::Enabled() const { return mEnabled; }

void NgDOFProc::Set(const RndCam *cam, float focalPlane, float blurDepth, float maxBlur, float minBlur) {
    MILO_ASSERT(cam, 0xBF);

    mFocalPlane = focalPlane;

    DOFOverrideParams &dof = RndPostProc::DOFOverrides();

    mBlurDepth = Max(dof.mDepthScale * blurDepth + dof.mDepthOffset, 0.0f);
    mMaxBlur = Clamp(0.0f, 1.0f, dof.mMaxBlurScale * maxBlur + dof.mMaxBlurOffset);
    mMinBlur = Clamp(0.0f, 1.0f, dof.mMinBlurScale * minBlur + dof.mMinBlurOffset);

    if (mMaxBlur > 0.0f && TheUI->IsGameScreenActive()) {
        mEnabled = true;
    }

    if (mBlurDepth <= 0.001f) {
        mBlurDepth = 0.001f;
    }

    float nearPlane = cam->NearPlane();
    float farPlane = cam->FarPlane();

    float scale = 0.0f;
    if (nearPlane <= focalPlane) {
        scale = (farPlane - farPlane / focalPlane * nearPlane) / (farPlane - nearPlane)
            * (cam->ZRange().y - cam->ZRange().x) + cam->ZRange().x;
    }
    mDepthOfFieldScale = scale;

    float farFocal = focalPlane - focalPlane * mBlurDepth;

    float bias = 0.0f;
    if (nearPlane <= farFocal) {
        bias = (farPlane - farPlane / farFocal * nearPlane) / (farPlane - nearPlane)
            * (cam->ZRange().y - cam->ZRange().x) + cam->ZRange().x;
    }
    mDepthOfFieldBias = bias;

    if (scale < bias + 0.001f) {
        mDepthOfFieldScale = bias + 0.001f;
    }
}

NgDOFProc::~NgDOFProc() {
    RELEASE(mBlurTex[0]);
    TheRnd.UnregisterPostProcessor(this);
}

void NgDOFProc::Init() {
    REGISTER_OBJ_FACTORY(NgDOFProc);
    if (TheDOFProc && !dynamic_cast<NgDOFProc *>(TheDOFProc)) {
        RELEASE(TheDOFProc);
        TheDOFProc = Hmx::Object::New<DOFProc>();
        MILO_ASSERT(dynamic_cast< NgDOFProc* >(TheDOFProc) != NULL, 0x175);
        static DataNode &n = DataVariable("the_dof_proc");
        n = TheDOFProc;
    }
}

void NgDOFProc::Terminate() {
    RELEASE(TheDOFProc);
    static DataNode &n = DataVariable("the_dof_proc");
    n = NULL_OBJ;
}

void SetVHBlurWeights(bool vertical, int width, int height) {
    float weights[8];
    weights[0] = 0.125f;
    weights[1] = 0.125f;
    weights[2] = 0.125f;
    weights[3] = 0.125f;
    weights[4] = 0.125f;
    weights[5] = 0.125f;
    weights[6] = 0.125f;
    weights[7] = 0.125f;

    float (*taps)[2];
    if (vertical) {
        taps = sVertBlurTaps;
        if ((sBlurTapInitFlags & 1) == 0) {
            sVertBlurTaps[0][0] = -0.9420162439f;
            sVertBlurTaps[0][1] = -0.3990621567f;
            sVertBlurTaps[1][0] = 0.9455860853f;
            sVertBlurTaps[1][1] = -0.768907249f;
            sVertBlurTaps[2][0] = -0.09418410063f;
            sVertBlurTaps[2][1] = -0.9293887019f;
            sVertBlurTaps[3][0] = 0.3449593782f;
            sVertBlurTaps[3][1] = 0.2938776016f;
            sVertBlurTaps[4][0] = -0.9158858061f;
            sVertBlurTaps[4][1] = 0.4577143192f;
            sVertBlurTaps[5][0] = -0.8154423237f;
            sVertBlurTaps[5][1] = -0.8791246414f;
            sVertBlurTaps[6][0] = -0.3827754259f;
            sVertBlurTaps[6][1] = 0.276768446f;
            sVertBlurTaps[7][0] = 0.9748439789f;
            sVertBlurTaps[7][1] = 0.7564837933f;
            sBlurTapInitFlags = sBlurTapInitFlags | 1;
        }
    } else {
        taps = sHorzBlurTaps;
        if ((sBlurTapInitFlags & 2) == 0) {
            sHorzBlurTaps[0][0] = 0.4432332516f;
            sHorzBlurTaps[0][1] = -0.9751155376f;
            sHorzBlurTaps[1][0] = 0.5374298096f;
            sHorzBlurTaps[1][1] = -0.4737342f;
            sHorzBlurTaps[2][0] = -0.2649691105f;
            sHorzBlurTaps[2][1] = -0.4189302325f;
            sHorzBlurTaps[3][0] = 0.7919751406f;
            sHorzBlurTaps[3][1] = 0.1909018755f;
            sHorzBlurTaps[4][0] = -0.2418884039f;
            sHorzBlurTaps[4][1] = 0.9970650673f;
            sHorzBlurTaps[5][0] = -0.8140995502f;
            sHorzBlurTaps[5][1] = 0.9143759012f;
            sHorzBlurTaps[6][0] = 0.1998412609f;
            sHorzBlurTaps[6][1] = 0.7864136696f;
            sHorzBlurTaps[7][0] = 0.1438316107f;
            sHorzBlurTaps[7][1] = -0.1410079002f;
            sBlurTapInitFlags = sBlurTapInitFlags | 2;
        }
    }

    TheShaderMgr.SetNumTaps(8);
    float fVar = RndPostProc::DOFOverrides().mBlurWidthScale * sDOFWidthFactor;
    float xScale = (float)(long long)width * fVar * 4.8828124e-06f;
    float yScale = (float)(long long)height * fVar * 1.5432099e-05f;

    for (int i = 0; i < 8; i++) {
        Vector4 tapOffset(taps[i][0] * xScale * 5.0f, taps[i][1] * yScale * 5.0f, 1.0f, 1.0f);
        TheShaderMgr.SetPConstant((PShaderConstant)(0x8a + i), tapOffset);
        Vector4 weight(weights[i], weights[i], weights[i], weights[i]);
        TheShaderMgr.SetPConstant((PShaderConstant)(0x9a + i), weight);
    }
}

void NgDOFProc::DoPost() {
    static DataNode &sDisableDof = DataVariable("disable_dof");

    auto _tmp4 = sDisableDof.Int(nullptr);
    if (_tmp4 == 0 && TheNgRnd.PreProcessTexture()
        && TheNgRnd.PreDepthTexture()) {
        MILO_ASSERT(mBlurTex[0] && mBlurTex[1], 0x10e);

        bool enabled = mEnabled;
        if (enabled) {
            unsigned int texWidth = mBlurTex[0]->Width();
            unsigned int texHeight = mBlurTex[0]->Height();

            Hmx::Rect rect(0, 0, (float)texWidth, (float)texHeight);

            NgRnd::Viewport savedViewport = TheNgRnd.GetViewport();

            NgRnd::Viewport blurViewport;
            blurViewport.MinZ = 0.0f;
            blurViewport.X = 0;
            blurViewport.Width = texWidth;
            blurViewport.Height = texHeight;
            blurViewport.Y = 0;
            blurViewport.MaxZ = 1.0f;
            TheNgRnd.SetViewport(blurViewport);

            // Setup DOF depth-of-field parameters as pixel shader float4 constant
            float range = 1.0f / (mDepthOfFieldScale - mDepthOfFieldBias);
            float maxBlur = mMaxBlur < 0.0f ? 1.0f : mMaxBlur;
            float minBlur = Min(mMaxBlur, mMinBlur);
            float negNear = -(mDepthOfFieldScale * range);

            Vector4 dofParams(range, negNear, minBlur, maxBlur);
            TheShaderMgr.SetPConstant((PShaderConstant)0x18, dofParams);

            RndMat *workMat = TheShaderMgr.GetWork();
            Hmx::Color colorPass1(1, 1, 1);
            Hmx::Color colorPass2(1, 1, 1);
            Hmx::Color colorPass3(1, 1, 1);

            // Pass 1: Downsample with DOF shader
            mBlurTex[0]->MakeDrawTarget();
            workMat->SetDiffuseTex(TheNgRnd.PreProcessTexture());
            workMat->SetZMode(kZModeDisable);
            workMat->SetTexWrap(kTexWrapClamp);
            workMat->SetBlend(BaseMaterial::kBlendSrc);
            workMat->MarkDirty(2);
            TheNgRnd.DrawRect(
                rect, workMat, kDownsample4xShader, colorPass1, nullptr, nullptr
            );
            mBlurTex[0]->FinishDrawTarget();

            // Pass 2: Horizontal blur
            mBlurTex[1]->MakeDrawTarget();
            workMat->SetDiffuseTex(mBlurTex[0]);
            workMat->SetBlend(BaseMaterial::kBlendSrc);
            workMat->SetZMode(kZModeDisable);
            workMat->SetTexWrap(kTexWrapClamp);
            workMat->MarkDirty(2);
            SetVHBlurWeights(false, texWidth, texHeight);
            TheNgRnd.DrawRect(
                rect, workMat, kBlurShader, colorPass2, nullptr, nullptr
            );
            mBlurTex[1]->FinishDrawTarget();

            // Pass 3: Vertical blur
            mBlurTex[0]->MakeDrawTarget();
            workMat->SetDiffuseTex(mBlurTex[1]);
            workMat->MarkDirty(2);
            SetVHBlurWeights(true, texWidth, texHeight);
            TheNgRnd.DrawRect(
                rect, workMat, kBlurShader, colorPass3, nullptr, nullptr
            );
            mBlurTex[0]->FinishDrawTarget();

            TheShaderMgr.SetNumTaps(1);
            TheShaderMgr.SetPConstant(kPS_Posterize, mBlurTex[0]);
            TheRenderState.SetTextureFilter(
                kPS_Posterize, (RndRenderState::FilterMode)1, false
            );
            TheRenderState.SetTextureClamp(kPS_Posterize, (RndRenderState::ClampMode)2);

            TheNgRnd.SetViewport(savedViewport);
        }
        TheShaderMgr.unk26 = enabled;
    }
}
