#include "DrawUtl.h"

#include "SkeletonViz.h"
#include "gesture/SkeletonUpdate.h"
#include "rndobj/Utl.h"

Vector3 DrawUtlVec3(0.5f, 0.05f, 0.4f);

Hmx::Rect DrawUtlRect;

namespace {
    void ScreenSpace(Hmx::Rect &rect) {
        float vx = DrawUtlVec3.x;
        float vy = DrawUtlVec3.y;
        float vz = DrawUtlVec3.z;
        rect.x = vx;
        rect.y = vy;
        rect.w = vz;
        float scale = (float)TheRnd.Width() / (float)TheRnd.Height();
        if (scale > 1.3333334f) {
            float rectW = 1.3333334f / scale;
            float diff = vz - rectW;
            rect.w = diff >= 0.0f ? rectW : vz;
        }
        rect.h = (scale * 0.75f) * rect.w;
    }

    void PixelSpace(Hmx::Rect &rect) {
        int width = TheRnd.Width();
        int height = TheRnd.Height();
        Hmx::Rect temp;
        ScreenSpace(temp);
        rect.x = width * temp.x;
        rect.w = width * temp.w;
        rect.y = height * temp.y;
        rect.h = height * temp.h;
    }
}

void TerminateDrawUtl() {
    if (TheSkeletonViz) {
        delete TheSkeletonViz;
    }
    TheSkeletonViz = nullptr;
}

#ifndef HX_NATIVE
bool ToggleDrawSkeletons() {
    MILO_ASSERT(TheSkeletonViz, 0xe2);
    TheSkeletonViz->SetShowing(!TheSkeletonViz->Showing());
    return TheSkeletonViz->Showing();
}
#endif

RndMat *CreateCameraBufferMat(int width, int height, RndTex::Type type) {
    auto tex = Hmx::Object::New<RndTex>();
    tex->SetBitmap(width, height, 16, type, false, nullptr);
    auto newMat = Hmx::Object::New<RndMat>();
    newMat->SetUseEnv(false);
    newMat->SetPreLit(true);
    newMat->SetBlend(BaseMaterial::kBlendSrc);
    newMat->SetZMode(kZModeDisable);
    newMat->SetDiffuseTex(tex);
    CreateAndSetMetaMat(newMat);
    return newMat;
}

void DrawBufferMat(RndMat *mat, Hmx::Rect &rect) {
    Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
    TheRnd.DrawRect(rect, white, mat, nullptr, nullptr);
}

void DrawSnapshot(const GestureMgr &gm, int index) {
    if (index >= 0) {
        MILO_ASSERT(index >= 0 && index < gm.GetLiveCameraInput()->NumSnapshots(), 0xfb);
    }
    auto cam = gm.GetLiveCameraInput();
    auto snap = cam->GetSnapshot(index);
    DrawBufferMat(snap, DrawUtlRect);
}

void InitDrawUtl(const GestureMgr &gm) {
    TheSkeletonViz = Hmx::Object::New<SkeletonViz>();
    TheSkeletonViz->Init();
    TheSkeletonViz->SetUsePhysicalCam(true);
    TheSkeletonViz->SetAxesCoordSys(kCoordCamera);
    TheSkeletonViz->SetShowing(false);
    Hmx::Rect temp;
    PixelSpace(temp);
    DrawUtlRect = temp;
}

void SetDrawSpace(float x, float y, float z) {
    DrawUtlVec3.Set(x, y, z);
}

void DrawGestureMgr(GestureMgr &gm, LiveCameraInput::BufferType bufferType, float) {
    TheRnd.EndWorld();

    if (bufferType != LiveCameraInput::kBufferNum && bufferType != LiveCameraInput::kBufferPlayer
        && bufferType != LiveCameraInput::kBufferDepth) {
        LiveCameraInput *cam = gm.GetLiveCameraInput();
        RndTex *displayTex = cam->DisplayTex(bufferType);
        if (UpdateBufferTex(cam, displayTex, bufferType, &gm)) {
            DrawBufferMat(cam->DisplayMat(bufferType), DrawUtlRect);
        }
    }

    if (TheSkeletonViz->Showing()) {
        Hmx::Rect screenRect;
        ScreenSpace(screenRect);
        TheSkeletonViz->SetPhysicalCamScreenRect(screenRect);

        SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
        CameraInput *cameraInput = handle.GetCameraInput();
        for (int i = 0; i < 6; i++) {
            TheSkeletonViz->Visualize(
                *cameraInput, gm.GetSkeleton(i), &handle.Callbacks(), false
            );
        }
        gm.DrawSkeletonKinectData();
    }
}

bool UpdateBufferTex(LiveCameraInput *cam, RndTex *tex, LiveCameraInput::BufferType bufType, GestureMgr *gm) {
    START_AUTO_TIMER("draw_natal_buffer");
    MILO_ASSERT(bufType < LiveCameraInput::kBufferNum, 0x12b);
    if (cam == nullptr) {
        return false;
    }
    MILO_ASSERT(tex, 0x130);
    MILO_ASSERT(tex->Bpp() == 16, 0x131);
    MILO_ASSERT(tex->GetType() == RndTex::kScratch, 0x132);

    void *texelsPtr = nullptr;
    tex->TexelsLock(texelsPtr);

    if (bufType == LiveCameraInput::kBufferColor) {
        void *bufStream = cam->StreamBufferData(LiveCameraInput::kBufferColor);
        if (bufStream != nullptr) {
            LiveCameraInput::LockedRect lockedRect;
            cam->LockStream(bufStream, lockedRect);
            cam->UnlockStream(bufStream);
        }
    } else if (bufType == LiveCameraInput::kBufferPlayerColor) {
        void *colorStream = cam->StreamBufferData(LiveCameraInput::kBufferColor);
        void *playerStream = cam->StreamBufferData(LiveCameraInput::kBufferPlayer);
        LiveCameraInput::LockedRect colorRect;
        cam->LockStream(colorStream, colorRect);
        LiveCameraInput::LockedRect playerRect;
        cam->LockStream(playerStream, playerRect);
        cam->UnlockStream(colorStream);
        cam->UnlockStream(playerStream);
    } else {
        MILO_ASSERT(gm != nullptr, 0x191);
        void *bufStream = cam->StreamBufferData(bufType);
        LiveCameraInput::LockedRect lockedRect;
        cam->LockStream(bufStream, lockedRect);
        MILO_ASSERT(tex->Width() == 0x140 && tex->Height() == 0xf0, 0x19a);
        cam->UnlockStream(bufStream);
    }

    tex->TexelsUnlock();
    return true;
}
