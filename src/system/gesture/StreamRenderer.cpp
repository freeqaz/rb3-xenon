#include "gesture/StreamRenderer.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/GestureMgr.h"
#include "gesture/LiveCameraInput.h"
#include "gesture/Skeleton.h"
#include "hamobj/HamGameData.h"
#include "math/Color.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/ShaderOptions.h"
#include "rndobj/Tex.h"
#include "rndobj/Utl.h"
#include "rnddx9/RenderState.h"
#include "rnddx9/Rnd.h"

RndCam *StreamRenderer::mCam;
RndTex *StreamRenderer::mBlurRT[2];

StreamRenderer::StreamRenderer()
    : mOutputTex(this), mForceMips(false), mDisplay(kStreamColor), mNumBlurs(4),
      mPlayer1DepthColor(1, 1, 1), mPlayer2DepthColor(0.48, 0.57, 0.8),
      mPlayer3DepthColor(0.05, 0.06, 0.54), mPlayer4DepthColor(0, 0, 0, 0),
      mPlayer5DepthColor(0, 0, 0, 0), mPlayer6DepthColor(0, 0, 0, 0),
      mPlayerDepthNobody(0, 0, 0, 0), mPlayer1DepthPalette(this),
      mPlayer2DepthPalette(this), mPlayerOtherDepthPalette(this),
      mBackgroundDepthPalette(this), mPlayer1DepthPaletteOffset(0),
      mPlayer2DepthPaletteOffset(0), mPlayerOtherDepthPaletteOffset(0),
      mBackgroundDepthPaletteOffset(0), mDrawPreClear(0), mForceDraw(0),
      mStaticColorIndices(0), mPCTestTex(this), mLagPrimaryTexture(0), unk154(0),
      mCrewPhotoPlayerDetected1(0, 0, 0, 0), mCrewPhotoPlayerDetected2(0, 0, 0, 0) {
    for (int i = 0; i < 6; i++) {
        mSmoothers[i].SetSmoothParameters(6, 0);
    }
    mLaggedPrimaryTexture[0] = nullptr;
    mLaggedPrimaryTexture[1] = nullptr;
}

StreamRenderer::~StreamRenderer() {
    delete mLaggedPrimaryTexture[0];
    delete mLaggedPrimaryTexture[1];
}

BEGIN_HANDLERS(StreamRenderer)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE(get_render_textures, OnGetRenderTextures)
    HANDLE_ACTION(
        set_crew_photo_horizontal_color, SetCrewPhotoHorizontalColor(_msg->Array(2))
    )
    HANDLE_ACTION(set_crew_photo_vertical_color, SetCrewPhotoVerticalColor(_msg->Array(2)))
    HANDLE_ACTION(
        set_crew_photo_player_detected,
        SetCrewPhotoPlayerDetected(_msg->Int(2), _msg->Int(3))
    )
    HANDLE_ACTION(set_crew_photo_player_centers, SetCrewPhotoPlayerCenters())
    HANDLE_ACTION(
        set_pink_player,
        SetPinkPlayer(
            TheGestureMgr->GetSkeletonIndexByTrackingID(
                TheGameData->Player(_msg->Size() > 2 ? _msg->Int(2) : 0)
                    ->GetSkeletonTrackingID()
            )
            + 1
        )
    )
    HANDLE_ACTION(
        set_blue_player,
        SetBluePlayer(
            TheGestureMgr->GetSkeletonIndexByTrackingID(
                TheGameData->Player(_msg->Size() > 2 ? _msg->Int(2) : 0)
                    ->GetSkeletonTrackingID()
            )
            + 1
        )
    )
END_HANDLERS

BEGIN_PROPSYNCS(StreamRenderer)
    SYNC_PROP_MODIFY(output_texture, mOutputTex, SetOutputTex())
    SYNC_PROP_MODIFY(force_mips, mForceMips, SetOutputTex())
    SYNC_PROP(display, (int &)mDisplay)
    SYNC_PROP(num_blurs, mNumBlurs)
    SYNC_PROP(player_depth_nobody, mPlayerDepthNobody)
    SYNC_PROP(player_depth_nobody_alpha, mPlayerDepthNobody.alpha)
    SYNC_PROP(player1_depth_color, mPlayer1DepthColor)
    SYNC_PROP(player1_depth_color_alpha, mPlayer1DepthColor.alpha)
    SYNC_PROP(player2_depth_color, mPlayer2DepthColor)
    SYNC_PROP(player2_depth_color_alpha, mPlayer2DepthColor.alpha)
    SYNC_PROP(player3_depth_color, mPlayer3DepthColor)
    SYNC_PROP(player3_depth_color_alpha, mPlayer3DepthColor.alpha)
    SYNC_PROP(player4_depth_color, mPlayer4DepthColor)
    SYNC_PROP(player4_depth_color_alpha, mPlayer4DepthColor.alpha)
    SYNC_PROP(player5_depth_color, mPlayer5DepthColor)
    SYNC_PROP(player5_depth_color_alpha, mPlayer5DepthColor.alpha)
    SYNC_PROP(player6_depth_color, mPlayer6DepthColor)
    SYNC_PROP(player6_depth_color_alpha, mPlayer6DepthColor.alpha)
    SYNC_PROP(player1_depth_palette, mPlayer1DepthPalette)
    SYNC_PROP(player1_depth_palette_offset, mPlayer1DepthPaletteOffset)
    SYNC_PROP(player2_depth_palette, mPlayer2DepthPalette)
    SYNC_PROP(player2_depth_palette_offset, mPlayer2DepthPaletteOffset)
    SYNC_PROP(player_other_depth_palette, mPlayerOtherDepthPalette)
    SYNC_PROP(player_other_depth_palette_offset, mPlayerOtherDepthPaletteOffset)
    SYNC_PROP(background_depth_palette, mBackgroundDepthPalette)
    SYNC_PROP(background_depth_palette_offset, mBackgroundDepthPaletteOffset)
    SYNC_PROP(force_draw, mForceDraw)
    SYNC_PROP(static_color_indices, mStaticColorIndices)
    SYNC_PROP_MODIFY(draw_pre_clear, mDrawPreClear, UpdatePreClearState())
    SYNC_PROP(pc_test_texture, mPCTestTex)
    SYNC_PROP(lag_primary_texture, mLagPrimaryTexture)
    SYNC_PROP(crew_photo_edge_iterations, mCrewPhotoEdgeIterations)
    SYNC_PROP(crew_photo_edge_offset, mCrewPhotoEdgeOffset)
    SYNC_PROP(crew_photo_horizontal_color, mCrewPhotoHorizontalColor)
    SYNC_PROP(crew_photo_vertical_color, mCrewPhotoVerticalColor)
    SYNC_PROP(crew_photo_blur_start, mCrewPhotoBlurStart)
    SYNC_PROP(crew_photo_blur_width, mCrewPhotoBlurWidth)
    SYNC_PROP(crew_photo_blur_iterations, mCrewPhotoBlurIterations)
    SYNC_PROP(crew_photo_background_brightness, mCrewPhotoBackgroundBrightness)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(StreamRenderer)
    SAVE_REVS(12, 1)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mOutputTex;
    bs << mForceMips;
    bs << mDisplay;
    bs << mPlayer1DepthColor << mPlayer2DepthColor << mPlayer3DepthColor
       << mPlayerDepthNobody;
    bs << mPlayer1DepthPalette << mPlayer1DepthPaletteOffset;
    bs << mBackgroundDepthPalette << mBackgroundDepthPaletteOffset;
    bs << mNumBlurs;
    bs << mPCTestTex;
    bs << mPlayer2DepthPalette << mPlayer2DepthPaletteOffset;
    bs << mPlayerOtherDepthPalette << mPlayerOtherDepthPaletteOffset;
    bs << mDrawPreClear << mForceDraw;
    bs << mLagPrimaryTexture;
    bs << mPlayer1DepthColor << mPlayer2DepthColor << mPlayer3DepthColor;
    bs << mStaticColorIndices;
    bs << mCrewPhotoEdgeIterations;
    bs << mCrewPhotoEdgeOffset;
    bs << mCrewPhotoHorizontalColor;
    bs << mCrewPhotoVerticalColor;
    bs << mCrewPhotoBlurStart;
    bs << mCrewPhotoBlurWidth;
    bs << mCrewPhotoBlurIterations;
    bs << mCrewPhotoBackgroundBrightness;
END_SAVES

BEGIN_COPYS(StreamRenderer)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(StreamRenderer)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mOutputTex)
        COPY_MEMBER(mForceMips)
        COPY_MEMBER(mDisplay)
        COPY_MEMBER(mPlayer1DepthColor)
        COPY_MEMBER(mPlayer2DepthColor)
        COPY_MEMBER(mPlayer3DepthColor)
        COPY_MEMBER(mPlayer4DepthColor)
        COPY_MEMBER(mPlayer5DepthColor)
        COPY_MEMBER(mPlayer6DepthColor)
        COPY_MEMBER(mPlayerDepthNobody)
        COPY_MEMBER(mPlayer1DepthPalette)
        COPY_MEMBER(mPlayer1DepthPaletteOffset)
        COPY_MEMBER(mPlayer2DepthPalette)
        COPY_MEMBER(mPlayer2DepthPaletteOffset)
        COPY_MEMBER(mPlayerOtherDepthPalette)
        COPY_MEMBER(mPlayerOtherDepthPaletteOffset)
        COPY_MEMBER(mBackgroundDepthPalette)
        COPY_MEMBER(mBackgroundDepthPaletteOffset)
        COPY_MEMBER(mNumBlurs)
        COPY_MEMBER(mPCTestTex)
        COPY_MEMBER(mDrawPreClear)
        COPY_MEMBER(mForceDraw)
        COPY_MEMBER(mLagPrimaryTexture)
        COPY_MEMBER(mCrewPhotoEdgeIterations)
        COPY_MEMBER(mCrewPhotoEdgeOffset)
        COPY_MEMBER(mCrewPhotoHorizontalColor)
        COPY_MEMBER(mCrewPhotoVerticalColor)
        COPY_MEMBER(mCrewPhotoBlurStart)
        COPY_MEMBER(mCrewPhotoBlurWidth)
        COPY_MEMBER(mCrewPhotoBlurIterations)
        COPY_MEMBER(mCrewPhotoBackgroundBrightness)
        SetOutputTex();
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(12, 1)

BEGIN_LOADS(StreamRenderer)
    LOAD_REVS(bs)
    ASSERT_REVS(12, 1)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    d >> mOutputTex;
    SetOutputTex();
    d >> mForceMips;
    d >> (int &)mDisplay;
    if (d.rev > 1) {
        d.stream >> mPlayer1DepthColor >> mPlayer2DepthColor >> mPlayer3DepthColor
            >> mPlayerDepthNobody;
        d >> mPlayer1DepthPalette >> mPlayer1DepthPaletteOffset;
    }
    if (d.rev > 2) {
        d >> mBackgroundDepthPalette >> mBackgroundDepthPaletteOffset;
    }
    if (d.rev > 3) {
        d >> mNumBlurs;
    }
    if (d.rev > 4) {
        d >> mPCTestTex;
    }
    if (d.rev > 5) {
        d >> mPlayer2DepthPalette;
        d >> mPlayer2DepthPaletteOffset;
    } else {
        mPlayer2DepthPalette = mPlayer1DepthPalette;
        mPlayer2DepthPaletteOffset = mPlayer1DepthPaletteOffset;
    }
    if (d.rev > 6) {
        d >> mPlayerOtherDepthPalette;
        d >> mPlayerOtherDepthPaletteOffset;
    } else {
        mPlayerOtherDepthPalette = mPlayer2DepthPalette;
        mPlayerOtherDepthPaletteOffset = mPlayer2DepthPaletteOffset;
    }
    if (d.rev > 7) {
        d >> mDrawPreClear;
        if (d.altRev > 0) {
            d >> mForceDraw;
        }
    }
    if (d.rev > 8) {
        d >> mLagPrimaryTexture;
    }
    if (d.rev > 9) {
        d.stream >> mPlayer4DepthColor >> mPlayer5DepthColor >> mPlayer6DepthColor;
    } else {
        mPlayer4DepthColor = mPlayer3DepthColor;
        mPlayer5DepthColor = mPlayer3DepthColor;
        mPlayer6DepthColor = mPlayer3DepthColor;
    }
    if (d.rev > 10) {
        d >> mStaticColorIndices;
    } else {
        mStaticColorIndices = false;
    }
    if (d.rev > 11) {
        d >> mCrewPhotoEdgeIterations;
        d >> mCrewPhotoEdgeOffset;
        d >> mCrewPhotoHorizontalColor;
        d >> mCrewPhotoVerticalColor;
        d >> mCrewPhotoBlurStart;
        d >> mCrewPhotoBlurWidth;
        d >> mCrewPhotoBlurIterations;
        d >> mCrewPhotoBackgroundBrightness;
    } else {
        mCrewPhotoEdgeIterations = 0;
        mCrewPhotoEdgeOffset = 0;
        mCrewPhotoBlurStart = 0;
        mCrewPhotoBlurWidth = 0;
        mCrewPhotoBlurIterations = 0;
        mCrewPhotoBackgroundBrightness = 0;
        mCrewPhotoHorizontalColor = 0;
        mCrewPhotoVerticalColor = 0;
    }
END_LOADS

#ifdef HX_NATIVE
void StreamRenderer::DrawToTexture() {}
void StreamRenderer::SetCrewPhotoPlayerCenters() {}
#else

namespace {
    int DisplayStreams[] = { 2, 1, 1, 1, 1, 2, 3, 3, 3 };

    void GetPlayerIndexes(int indexes[6]);

    bool CheckTexType(RndTex *tex) {
        if (!(tex->GetType() & 2)) {
            MILO_NOTIFY_ONCE("%s not renderable", tex->Name());
            return false;
        }
        return true;
    }

    RndMat *SetUpWorkingMat() { return TheShaderMgr.GetWork(); }

    RndTex *SetPaletteTexture(RndTex *tex, StreamDisplay display) {
        if (tex != nullptr) {
            return tex;
        }
        if (display != kStreamPlayerDepthShell && display != kStreamPlayerDepthShell2
            && display != kStreamPlayerGreenscreen && display != kStreamPlayerDepthGreenscreen) {
            return TheRnd.GetDefaultTex(Rnd::kDefaultTex_White);
        }
        return TheRnd.GetDefaultTex(Rnd::kDefaultTex_WhiteTransparent);
    }
}

void SetBloomBlurWeights(bool, float, float);

void StreamRenderer::SetCrewPhotoPlayerCenters() {
    for (int i = 0; i < 6; i++) {
        Skeleton *skel =
            TheGestureMgr->GetSkeletonByTrackingID(TheGestureMgr->GetSkeleton(i).TrackingID());
        if (skel) {
            float minX = 1.0f, maxX = -1.0f;
            float minY = 1.0f, maxY = -1.0f;
            float minZ = 1.0f, maxZ = -1.0f;
            for (int j = 0; j < kNumJoints; j++) {
                Vector2 screenPos;
                skel->ScreenPos((SkeletonJoint)j, screenPos);
                float depth = skel->TrackedJoints()[j].mSmoothedPos.z;
                minZ = (minZ - depth >= 0) ? depth : minZ;
                minX = (minX - screenPos.x >= 0) ? screenPos.x : minX;
                maxX = (maxX - screenPos.x >= 0) ? maxX : screenPos.x;
                minY = (minY - screenPos.y >= 0) ? screenPos.y : minY;
                maxY = (maxY - screenPos.y >= 0) ? maxY : screenPos.y;
                maxZ = (maxZ - depth >= 0) ? maxZ : depth;
            }
            Vector3 center(
                (maxX + minX) * 0.5f, (maxY + minY) * 0.5f, (maxZ + minZ) * 0.5f
            );
            float dt = TheTaskMgr.DeltaUISeconds();
            mSmoothers[i].Smooth(center, dt, false);
            Vector3 val = mSmoothers[i].Value();
            *(Vector4 *)&mCrewPhotoPlayerCenters[i] = *(Vector4 *)&val;
        } else {
            mCrewPhotoPlayerCenters[i].Set(0, 0, 0, 0);
        }
    }
}

void StreamRenderer::DrawToTexture() {
    if (TheRnd.GetDrawMode() != 0)
        return;
    if (!Showing())
        return;

    if (mLagPrimaryTexture && !mLaggedPrimaryTexture[0]) {
        MILO_ASSERT(mLaggedPrimaryTexture[1] == NULL, 0xF5);
        mLaggedPrimaryTexture[0] = Hmx::Object::New<RndTex>();
        mLaggedPrimaryTexture[0]->SetBitmap(320, 240, 16, (RndTex::Type)0x2000, false, nullptr);
        mLaggedPrimaryTexture[1] = Hmx::Object::New<RndTex>();
        mLaggedPrimaryTexture[1]->SetBitmap(320, 240, 16, (RndTex::Type)0x2000, false, nullptr);
    }

    if (!mOutputTex)
        return;

    TheDxRnd.SetShaderRegisterAlloc((DxRnd::RegisterAlloc)3);

    LiveCameraInput *camInput = TheGestureMgr->GetLiveCameraInput();
    int streamFlags = DisplayStreams[mDisplay];
    unsigned int needsDepth = streamFlags & 1;
    unsigned int needsColor = (streamFlags >> 1) & 1;

    if (needsDepth && !*(bool *)((char *)camInput + 0x11ea)) {
        camInput->PollNewStream(LiveCameraInput::kBufferDepth);
    } else if (needsColor && !*(bool *)((char *)camInput + 0x11e9)) {
        camInput->PollNewStream(LiveCameraInput::kBufferColor);
    }

    if (mForceDraw || (needsDepth && *(bool *)((char *)camInput + 0x11ec))
        || (needsColor && *(bool *)((char *)camInput + 0x11eb))) {
        int playerIndexes[6];
        if (mStaticColorIndices) {
            for (int i = 0; i < 6; i++) {
                playerIndexes[i] = i;
            }
        } else {
            GetPlayerIndexes(playerIndexes);
        }

        if (!CheckTexType(mOutputTex))
            return;

        RndMat *workMat = SetUpWorkingMat();
        ShaderType shaderType = GetShaderType();
        LiveCameraInput::BufferType bufType =
            (LiveCameraInput::BufferType)(needsDepth != 0);

        RndTex *primaryTex;
        if (mLagPrimaryTexture) {
            primaryTex = mLaggedPrimaryTexture[unk154];
        } else {
            primaryTex = camInput->GetStreamTex(bufType);
        }

        RndCam *currentCam = RndCam::Current();
        RndTex *targetRT = mBlurRT[0];
        if (mNumBlurs == 0) {
            targetRT = mOutputTex;
        }

        if (currentCam->TargetTex()) {
            MILO_NOTIFY_ONCE(
                "%s: Cannot render to texture (%s) while already rendering to texture (%s).",
                PathName(currentCam->TargetTex()),
                PathName(this),
                PathName(currentCam->TargetTex())
            );
        }

        mCam->SetTargetTex(targetRT);
        mCam->Select();

        workMat->SetDiffuseTex(primaryTex);
        workMat->MarkDirty(2);

        int display = mDisplay;
        if (display == kStreamPlayerGreenscreen || display == kStreamPlayerDepthGreenscreen
            || display == kStreamCrewPhoto) {
            RndTex *colorTex = camInput->GetStreamTex(LiveCameraInput::kBufferColor);
            workMat->SetNormalMap(colorTex);
            workMat->MarkDirty(2);
        }

        TheRenderState.SetTextureFilter(0, (RndRenderState::FilterMode)0, false);
        TheRenderState.SetTextureClamp(0, (RndRenderState::ClampMode)2);

        RndTex *palette1 = SetPaletteTexture(mPlayer1DepthPalette, mDisplay);
        TheShaderMgr.SetPConstant((PShaderConstant)10, palette1);
        TheRenderState.SetTextureFilter(10, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(10, (RndRenderState::ClampMode)0);

        RndTex *palette2 = SetPaletteTexture(mPlayer2DepthPalette, mDisplay);
        TheShaderMgr.SetPConstant((PShaderConstant)12, palette2);
        TheRenderState.SetTextureFilter(12, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(12, (RndRenderState::ClampMode)0);

        RndTex *paletteOther = SetPaletteTexture(mPlayerOtherDepthPalette, mDisplay);
        TheShaderMgr.SetPConstant((PShaderConstant)13, paletteOther);
        TheRenderState.SetTextureFilter(13, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(13, (RndRenderState::ClampMode)0);

        RndTex *paletteBg = SetPaletteTexture(mBackgroundDepthPalette, mDisplay);
        TheShaderMgr.SetPConstant((PShaderConstant)11, paletteBg);
        TheRenderState.SetTextureFilter(11, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(11, (RndRenderState::ClampMode)0);

        Vector4 nobodyColor(
            mPlayerDepthNobody.red, mPlayerDepthNobody.green,
            mPlayerDepthNobody.blue, mPlayerDepthNobody.alpha
        );
        TheShaderMgr.SetPConstant(kPS_TexProcFrequency, nobodyColor);

        Vector4 p1Color(
            mPlayer1DepthColor.red, mPlayer1DepthColor.green,
            mPlayer1DepthColor.blue, mPlayer1DepthColor.alpha
        );
        TheShaderMgr.SetPConstant(kPS_TexProcAmplitude, p1Color);

        Vector4 p2Color(
            mPlayer2DepthColor.red, mPlayer2DepthColor.green,
            mPlayer2DepthColor.blue, mPlayer2DepthColor.alpha
        );
        TheShaderMgr.SetPConstant(kPS_TexProcPhase, p2Color);

        Vector4 p3Color(
            mPlayer3DepthColor.red, mPlayer3DepthColor.green,
            mPlayer3DepthColor.blue, mPlayer3DepthColor.alpha
        );
        TheShaderMgr.SetPConstant((PShaderConstant)0x43, p3Color);

        Vector4 paletteOffsets(
            mPlayer1DepthPaletteOffset, mPlayer2DepthPaletteOffset,
            mPlayerOtherDepthPaletteOffset, mBackgroundDepthPaletteOffset
        );
        TheShaderMgr.SetPConstant((PShaderConstant)0x44, paletteOffsets);

        Vector4 playerIdx(
            (float)playerIndexes[0], (float)playerIndexes[1],
            (float)playerIndexes[2], (float)playerIndexes[3]
        );
        TheShaderMgr.SetPConstant((PShaderConstant)0x45, playerIdx);

        if (shaderType == kPlayerDepthShell2Shader) {
            Vector4 p4Color(
                mPlayer4DepthColor.red, mPlayer4DepthColor.green,
                mPlayer4DepthColor.blue, mPlayer4DepthColor.alpha
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x46, p4Color);

            Vector4 p5Color(
                mPlayer5DepthColor.red, mPlayer5DepthColor.green,
                mPlayer5DepthColor.blue, mPlayer5DepthColor.alpha
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x47, p5Color);

            Vector4 p6Color(
                mPlayer6DepthColor.red, mPlayer6DepthColor.green,
                mPlayer6DepthColor.blue, mPlayer6DepthColor.alpha
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x48, p6Color);

            Vector4 playerIdx2(
                (float)playerIndexes[4], (float)playerIndexes[5], 0, 0
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x49, playerIdx2);
        }

        if (shaderType == kCrewPhotoShader) {
            float beat = TheTaskMgr.Beat();
            Vector4 edgeParams(
                mCrewPhotoEdgeIterations, mCrewPhotoEdgeOffset, beat, 1.0f
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x46, edgeParams);

            Vector4 horizColor(
                mCrewPhotoHorizontalColor.red, mCrewPhotoHorizontalColor.green,
                mCrewPhotoHorizontalColor.blue, mCrewPhotoHorizontalColor.alpha
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x47, horizColor);

            Vector4 vertColor(
                mCrewPhotoVerticalColor.red, mCrewPhotoVerticalColor.green,
                mCrewPhotoVerticalColor.blue, mCrewPhotoVerticalColor.alpha
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x48, vertColor);

            Vector4 blurParams(
                mCrewPhotoBlurStart, mCrewPhotoBlurWidth, mCrewPhotoBlurIterations, 1.0f
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x49, blurParams);

            Vector4 bgBrightness(
                mCrewPhotoBackgroundBrightness, mCrewPhotoBackgroundBrightness,
                mCrewPhotoBackgroundBrightness, mCrewPhotoBackgroundBrightness
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x4a, bgBrightness);

            TheShaderMgr.SetPConstant(
                (PShaderConstant)0x4b,
                *(const Vector4 *)&mCrewPhotoPlayerDetected1
            );

            TheShaderMgr.SetPConstant(
                (PShaderConstant)0x4c,
                *(const Vector4 *)&mCrewPhotoPlayerDetected2
            );

            Vector4 center0(
                mCrewPhotoPlayerCenters[0].x, mCrewPhotoPlayerCenters[0].y,
                mCrewPhotoPlayerCenters[0].z, 1.0f
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x4d, center0);

            Vector4 center1(
                mCrewPhotoPlayerCenters[1].x, mCrewPhotoPlayerCenters[1].y,
                mCrewPhotoPlayerCenters[1].z, 1.0f
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x4e, center1);

            Vector4 center2(
                mCrewPhotoPlayerCenters[2].x, mCrewPhotoPlayerCenters[2].y,
                mCrewPhotoPlayerCenters[2].z, 1.0f
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x4f, center2);

            Vector4 center3(
                mCrewPhotoPlayerCenters[3].x, mCrewPhotoPlayerCenters[3].y,
                mCrewPhotoPlayerCenters[3].z, 1.0f
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x50, center3);

            Vector4 center4(
                mCrewPhotoPlayerCenters[4].x, mCrewPhotoPlayerCenters[4].y,
                mCrewPhotoPlayerCenters[4].z, 1.0f
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x51, center4);

            Vector4 center5(
                mCrewPhotoPlayerCenters[5].x, mCrewPhotoPlayerCenters[5].y,
                mCrewPhotoPlayerCenters[5].z, 1.0f
            );
            TheShaderMgr.SetPConstant((PShaderConstant)0x52, center5);
        }

        Hmx::Rect drawRect(0, 0, (float)targetRT->Width(), (float)targetRT->Height());
        TheNgRnd.DrawRect(drawRect, workMat, shaderType, Hmx::Color(), nullptr, nullptr);

        mCam->SetTargetTex(nullptr);

        RndMat *blurMat = TheShaderMgr.GetWork();
        blurMat->SetDiffuseTex(nullptr);
        blurMat->SetBlend(BaseMaterial::kBlendSrc);
        blurMat->SetZMode(kZModeDisable);
        blurMat->MarkDirty(2);

        for (unsigned int blurIdx = 0; (int)blurIdx < mNumBlurs; blurIdx++) {
            RndTex *srcTex = mBlurRT[1];
            if ((blurIdx & 1) == 0) {
                srcTex = mBlurRT[0];
            }
            RndTex *dstTex = mBlurRT[0];
            if ((blurIdx & 1) == 0) {
                dstTex = mBlurRT[1];
            }
            if ((unsigned int)(mNumBlurs - 1) == blurIdx) {
                dstTex = mOutputTex;
            }
            dstTex->MakeDrawTarget();
            blurMat->SetDiffuseTex(srcTex);
            blurMat->MarkDirty(2);
            Hmx::Rect blurRect(0, 0, (float)dstTex->Width(), (float)dstTex->Height());
            if (primaryTex) {
                SetBloomBlurWeights(
                    (bool)(blurIdx & 1), (float)dstTex->Width(), (float)dstTex->Height()
                );
                TheNgRnd.DrawRect(
                    blurRect, blurMat, kDrawRectShader, Hmx::Color(), nullptr, nullptr
                );
                TheShaderMgr.SetNumTaps(1);
            }
            dstTex->FinishDrawTarget();
        }

        if (mLagPrimaryTexture) {
            RndTex *streamTex = camInput->GetStreamTex(bufType);
            if (streamTex) {
                void *streamData = camInput->StreamBufferData(bufType);
                if (streamData) {
                    void *lagData = nullptr;
                    bool newIdx = unk154 == 0;
                    unk154 = newIdx ? 1 : 0;
                    mLaggedPrimaryTexture[unk154]->TexelsLock(lagData);
                    void *srcData = nullptr;
                    streamTex->TexelsLock(srcData);
                    memcpy(lagData, srcData, 0x2D000);
                    streamTex->TexelsUnlock();
                    mLaggedPrimaryTexture[unk154]->TexelsUnlock();
                }
            }
        }

        currentCam->Select();
    }

    TheDxRnd.SetShaderRegisterAlloc((DxRnd::RegisterAlloc)1);
}

#endif

void StreamRenderer::DrawShowing() {
    if (!mDrawPreClear) {
        DrawToTexture();
    }
}

void StreamRenderer::UpdatePreClearState() {
    TheRnd.PreClearDrawAddOrRemove(this, mDrawPreClear, false);
}

void StreamRenderer::Init() {
    REGISTER_OBJ_FACTORY(StreamRenderer)
    MILO_ASSERT(!mCam, 0xC9);
    mCam = ObjectDir::Main()->New<RndCam>("[stream renderer cam]");
    for (int i = 0; i < DIM(mBlurRT); i++) {
        MILO_ASSERT(mBlurRT[i] == NULL, 0xCF);
        mBlurRT[i] = Hmx::Object::New<RndTex>();
        mBlurRT[i]->SetBitmap(
            320, 240, TheRnd.Bpp(), RndTex::kRenderedNoZ, false, nullptr
        );
    }
}

void StreamRenderer::Terminate() {
    for (int i = 0; i < DIM(mBlurRT); i++) {
        RELEASE(mBlurRT[i]);
    }
    RELEASE(mCam);
}

void StreamRenderer::SetPinkPlayer(int player) { mCrewPhotoPlayerDetected2.blue = player; }
void StreamRenderer::SetBluePlayer(int player) { mCrewPhotoPlayerDetected2.alpha = player; }

DataNode StreamRenderer::OnGetRenderTextures(DataArray *) {
    return GetRenderTextures(Dir());
}

void StreamRenderer::SetCrewPhotoHorizontalColor(DataArray *cfg) {
    if (cfg->Size() == 3) {
        mCrewPhotoHorizontalColor.red = cfg->Float(0);
        mCrewPhotoHorizontalColor.green = cfg->Float(1);
        mCrewPhotoHorizontalColor.blue = cfg->Float(2);
    }
}

void StreamRenderer::SetCrewPhotoVerticalColor(DataArray *cfg) {
    if (cfg->Size() == 3) {
        mCrewPhotoVerticalColor.red = cfg->Float(0);
        mCrewPhotoVerticalColor.green = cfg->Float(1);
        mCrewPhotoVerticalColor.blue = cfg->Float(2);
    }
}

ShaderType StreamRenderer::GetShaderType() const {
    ShaderType t = kDrawRectShader;
    switch (mDisplay) {
    case kStreamColor:
        t = kYUVtoRGBShader;
        break;
    case kStreamBlackAndWhite:
        t = kYUVtoBlackAndWhiteShader;
        break;
    case kStreamBasicDepth:
        t = kDrawRectShader;
        break;
    case kStreamPlayerDepthVis:
        t = kPlayerDepthVisShader;
        break;
    case kStreamPlayerDepthShell:
        t = kPlayerDepthShellShader;
        break;
    case kStreamPlayerDepthShell2:
        t = kPlayerDepthShell2Shader;
        break;
    case kStreamPlayerGreenscreen:
        t = kPlayerGreenScreenShader;
        break;
    case kStreamPlayerDepthGreenscreen:
        t = kPlayerDepthGreenScreenShader;
        break;
    case kStreamCrewPhoto:
        t = kCrewPhotoShader;
        break;
    default:
        MILO_FAIL("Unknown StreamDisplay in StreamRenderer");
        break;
    }
    return t;
}

void StreamRenderer::SetCrewPhotoPlayerDetected(int player, bool b2) {
    MILO_ASSERT_RANGE(player, 0, 6, 0x212);
    float set = b2 ? 1.0f : 0.0f;
    switch (player) {
    case 0:
        mCrewPhotoPlayerDetected1.red = set;
        break;
    case 1:
        mCrewPhotoPlayerDetected1.green = set;
        break;
    case 2:
        mCrewPhotoPlayerDetected1.blue = set;
        break;
    case 3:
        mCrewPhotoPlayerDetected1.alpha = set;
        break;
    case 4:
        mCrewPhotoPlayerDetected2.red = set;
        break;
    case 5:
        mCrewPhotoPlayerDetected2.green = set;
        break;
    default:
        break;
    }
}
