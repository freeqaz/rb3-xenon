#include "gesture/LiveCameraInput.h"
#include "DrawUtl.h"
#include "gesture/CameraInput.h"
#include "gesture/Skeleton.h"
#include "gesture/SkeletonUpdate.h"
#include "gesture/SpeechMgr.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rnddx9/Tex.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "math/Utl.h"
#include "utl/MemTrack.h"
#include "utl/Std.h"
#include "xdk/nui/nuiapi.h"
#include "xdk/nui/nuiaudio.h"
#include "xdk/nui/nuidetroit.h"
#include "xdk/win_types.h"
#include "xdk/XGRAPHICS.h"
#include "xdk/xapilibi/handleapi.h"
#include "xdk/xapilibi/winerror.h"
#include "utl/FilePath.h"
#include "utl/FileStream.h"
#include "utl/Loader.h"
#include "Memory.h"

class DxRnd {
public:
    void ReleaseAutoRelease();
};
extern DxRnd TheDxRnd;

float gTempPortraitOffset;

namespace {
    bool gDebugDepth;
    bool GetExposureRegion(NUI_CAMERA_AE_ROI &);
    long GetColorCameraProperty(NUI_CAMERA_PROPERTY);
    unsigned short YUVtoRGB(int y, int cr, int cb);

    void SetColorCameraProperty(NUI_CAMERA_PROPERTY prop, long value) {
        HRESULT hr = NuiCameraSetProperty(NUI_CAMERA_TYPE_COLOR, prop, value);
        if (FAILED(hr)) {
            MILO_LOG(
                "NuiCameraSetProperty failed.  Property 0x%x, error (0x%x)\n",
                prop,
                hr
            );
        }
    }

    void LoadDebugDepthBuffer(RndTex *&outTex) {
        outTex = nullptr;
        FilePath fp("../../system/run/gesture/dev_depth.data");
        FileLoader loader(
            fp, "../../system/run/gesture/dev_depth.data", kLoadFront, 0, false, true,
            nullptr, nullptr
        );
        TheLoadMgr.PollUntilLoaded(&loader, nullptr);
        int sz;
        char *buffer = loader.GetBuffer(&sz);
        if (sz != 0) {
            MILO_ASSERT(sz == 320 * 240 * 2, 0x5a);
            DxTex *tex = Hmx::Object::New<DxTex>();
            D3DTexture *d3dTex = new D3DTexture();
            int texSize =
                XGSetTextureHeader(
                    0x140, 0xf0, 1, 4, (D3DFORMAT)0x1a220058, 0, 0, -1, d3dTex
                );
            void *ptr = PhysicalAllocTracked(
                (texSize + 0xFFF) & 0xFFFFF000, 4, "LiveCameraInput.cpp", 0x66,
                "Tex(phys)"
            );
            MILO_ASSERT(ptr, 0x67);
            XGOffsetResourceAddress(d3dTex, ptr);
            tex->SetDeviceTex(d3dTex);
            void *texels;
            tex->TexelsLock(texels);
            memcpy(texels, buffer, sz);
            tex->TexelsUnlock();
            outTex = tex;
        }
        MemFree(buffer, "unknown", 0, "unknown");
    }
}

LiveCameraInput *LiveCameraInput::sInstance;
int g_ColorPollCnt;
int g_ColorNoFrameDataCnt;
int side;
int g_colorBufferUpdate1;
int g_colorBufferUpdate2;
int g_colorBufferUpdate3;
int g_colorBufferUpdate4;
bool g_startMetering;

void CamTexClip::StoreTextureClip(RndTex *tex, float clipLeft, float clipTop, float, float) {
    const float scaleX = 132.0f / 640.0f;
    const float scaleY = 160.0f / 480.0f;
    const float minX = 66.0f / 640.0f;
    const float maxX = 574.0f / 640.0f;
    const float minY = 80.0f / 480.0f;
    const float maxY = 400.0f / 480.0f;

    float adjustedTop = gTempPortraitOffset * scaleY + clipTop;
    mTex = tex;
    mXfm = Transform::IDXfm();
    mXfm.m.x *= scaleX;
    float clampedX = Clamp(minX, maxX, clipLeft);
    float clampedY = Clamp(minY, maxY, adjustedTop);
    mXfm.v.x = clampedX;
    mXfm.v.y = clampedY;
    mXfm.m.y *= scaleY;
    mXfm.m.z.Set(mXfm.m.z.x, mXfm.m.z.y, mXfm.m.z.z);
}

#pragma region TextureStore

void LiveCameraInput::TextureStore::StoreTexture(RndTex *tex) {
    if (mTex) {
        RELEASE(mTex);
    }
    if (tex) {
        mTex = Hmx::Object::New<RndTex>();
        RndBitmap bitmap;
        tex->LockBitmap(bitmap, 1);
        mTex->SetBitmap(bitmap, nullptr, true, RndTex::kRenderedNoZ);
        tex->UnlockBitmap();
    } else {
        mTex = nullptr;
    }
}

void LiveCameraInput::TextureStore::StoreColorBuffer(LiveCameraInput *cam) {
    if (mTex) {
        if (mTex->Width() == 640 && mTex->Height() == 480)
            goto update;
        RELEASE(mTex);
    }
    mTex = Hmx::Object::New<RndTex>();
    mTex->SetBitmap(640, 480, 16, RndTex::kScratch, false, nullptr);
    MILO_ASSERT(mTex, 0x53A);
    MILO_ASSERT(mTex->Bpp() == 16, 0x53B);
    MILO_ASSERT(mTex->GetType() == RndTex::kScratch, 0x53C);
update:
    UpdateFromColorBuffer(cam);
}

void LiveCameraInput::TextureStore::StoreDepthBuffer(LiveCameraInput *cam) {
    if (mTex) {
        if (mTex->Width() == 640 && mTex->Height() == 480)
            goto update;
        RELEASE(mTex);
    }
    mTex = Hmx::Object::New<RndTex>();
    mTex->SetBitmap(640, 480, 16, RndTex::kScratch, false, nullptr);
    MILO_ASSERT(mTex, 0x602);
    MILO_ASSERT(mTex->Bpp() == 16, 0x603);
    MILO_ASSERT(mTex->GetType() == RndTex::kScratch, 0x604);
update:
    UpdateFromDepthBuffer(cam);
}

void LiveCameraInput::TextureStore::StoreColorBufferClip(
    LiveCameraInput *cam, float clipLeft, float clipTop, float clipWidth, float clipHeight
) {
    if (mTex) {
        if (mTex->Width() == clipWidth && mTex->Height() == clipHeight)
            goto update;
        RELEASE(mTex);
    }
    {
        int w = (1 - (int)(clipWidth * -640.0f)) & 0xfffe;
        int h = (1 - (int)(clipHeight * -480.0f)) & 0xfffe;
        if (w > 0x280)
            w = 0x280;
        if (h > 0x1e0)
            h = 0x1e0;
        mTex = Hmx::Object::New<RndTex>();
        mTex->SetBitmap(w, h, 16, RndTex::kScratch, false, nullptr);
        MILO_ASSERT(mTex, 0x595);
        MILO_ASSERT(mTex->Bpp() == 16, 0x596);
        MILO_ASSERT(mTex->GetType() == RndTex::kScratch, 0x597);
    }
update:
    UpdateFromColorBufferClip(cam, clipLeft, clipTop);
}

void LiveCameraInput::TextureStore::StoreDepthBufferClip(
    LiveCameraInput *cam, float clipLeft, float clipTop, float clipWidth, float clipHeight
) {
    if (mTex) {
        if (mTex->Width() == clipWidth && mTex->Height() == clipHeight)
            goto update;
        RELEASE(mTex);
    }
    {
        int w = (1 - (int)(clipWidth * -640.0f)) & 0xfffe;
        int h = (1 - (int)(clipHeight * -480.0f)) & 0xfffe;
        if (w > 0x280)
            w = 0x280;
        if (h > 0x1e0)
            h = 0x1e0;
        mTex = Hmx::Object::New<RndTex>();
        mTex->SetBitmap(w, h, 16, RndTex::kScratch, false, nullptr);
        MILO_ASSERT(mTex, 0x660);
        MILO_ASSERT(mTex->Bpp() == 16, 0x661);
        MILO_ASSERT(mTex->GetType() == RndTex::kScratch, 0x662);
    }
update:
    UpdateFromDepthBufferClip(cam, clipLeft, clipTop);
}

void LiveCameraInput::TextureStore::UpdateFromColorBuffer(LiveCameraInput *cam) {
    void *texels = nullptr;
    mTex->TexelsLock(texels);
    unsigned int destPtr = (unsigned int)texels;
    g_colorBufferUpdate1++;
    void *bufferData = cam->StreamBufferData(kBufferColor);
    if (bufferData) {
        LockedRect lockedRect;
        cam->LockStream(bufferData, lockedRect);
        g_colorBufferUpdate2++;
        unsigned int *srcPtr = (unsigned int *)((int)lockedRect.mBits - 4);
        unsigned int pitch = mTex->TexelsPitch();
        for (int row = 0; row < 480; row++) {
            for (int col = 0; col < 320; col++) {
                srcPtr++;
                unsigned int pixel = *srcPtr;
                int cr = (pixel >> 24) - 0x80;
                int cb = (pixel >> 8 & 0xff) - 0x80;
                *(unsigned short *)destPtr = YUVtoRGB(pixel >> 16 & 0xff, cr, cb);
                ((unsigned short *)destPtr)[1] = YUVtoRGB(pixel & 0xff, cr, cb);
                destPtr += 4;
            }
            destPtr += ((pitch >> 1) - 640) * 2;
            srcPtr += (lockedRect.mPitch >> 2) - 320;
        }
        D3DCubeTexture_UnlockRect((D3DCubeTexture *)bufferData, (D3DCUBEMAP_FACES)0, 0);
    }
}

void LiveCameraInput::TextureStore::UpdateFromDepthBuffer(LiveCameraInput *cam) {
    void *texels = nullptr;
    mTex->TexelsLock(texels);
    unsigned int destBase = (unsigned int)texels;
    void *bufferData = cam->StreamBufferData(kBufferDepth);
    if (bufferData) {
        LockedRect lockedRect;
        cam->LockStream(bufferData, lockedRect);
        unsigned int srcBase = (unsigned int)lockedRect.mBits;
        unsigned int rowIdx = 0;
        do {
            unsigned int x = 0;
            unsigned short *destRow = (unsigned short *)(destBase - 2);
            do {
                unsigned short depthPixel =
                    *(unsigned short *)(((int)(x >> 1) + (unsigned int)((int)x < 0 && (x & 1) != 0)) * 2 + srcBase);
                unsigned short color = 0;
                unsigned short playerIdx = depthPixel & 7;
                if (playerIdx < 8) {
                    switch (playerIdx) {
                    case 1:
                        color = 0xf800;
                        break;
                    case 2:
                        color = 0x7e0;
                        break;
                    case 3:
                        color = 0x1f;
                        break;
                    case 4:
                        color = 0xf81f;
                        break;
                    case 5:
                        color = 0x7ff;
                        break;
                    case 6:
                        color = 0xffe0;
                        break;
                    case 0:
                        color = 0;
                        break;
                    default:
                        color = 0xffff;
                        break;
                    }
                }
                x++;
                destRow++;
                *destRow = color;
            } while ((int)x < 640);
            unsigned int pitch = mTex->TexelsPitch();
            destBase += (pitch & 0xfffffffe);
            if ((rowIdx & 1) != 0) {
                srcBase += (lockedRect.mPitch & 0xfffffffe);
            }
            rowIdx++;
        } while ((int)rowIdx < 480);
        D3DCubeTexture_UnlockRect((D3DCubeTexture *)bufferData, (D3DCUBEMAP_FACES)0, 0);
    }
}

void LiveCameraInput::TextureStore::UpdateFromColorBufferClip(
    LiveCameraInput *cam, float clipLeft, float clipTop
) {
    int texWidth = mTex->Width();
    unsigned int startX = ~((int)(clipLeft * 640.0f) >> 31) & (int)(clipLeft * 640.0f);
    if ((int)(texWidth + startX - 1) > 639) {
        startX = 640 - texWidth;
    }
    int texHeight = mTex->Height();
    unsigned int startY = (unsigned int)(clipTop * 480.0f);
    startY = ~((int)startY >> 31) & startY;
    if ((int)(texHeight + startY - 1) > 479) {
        startY = 480 - texHeight;
    }
    g_colorBufferUpdate3++;
    unsigned int clippedX = (startX + 1 & 0xfffe) % 640;
    if (!g_startMetering) {
        g_startMetering = true;
        g_ColorNoFrameDataCnt = 0;
    }
    void *texels = nullptr;
    mTex->TexelsLock(texels);
    unsigned int destPtr = (unsigned int)texels;
    void *bufferData = cam->StreamBufferData(kBufferColor);
    if (bufferData) {
        LockedRect lockedRect;
        cam->LockStream(bufferData, lockedRect);
        g_colorBufferUpdate4++;
        int destWidth = mTex->Width();
        unsigned int srcPitch = lockedRect.mPitch >> 2;
        unsigned int clippedY = (startY + 1 & 0xfffe) % 480;
        int srcOffset = srcPitch * clippedY * 4 + (int)lockedRect.mBits;
        unsigned int destPitch = mTex->TexelsPitch();
        int destStride = (int)((destPitch >> 1) - destWidth) * 2;
        int row = 0;
        if (mTex->Height() > 0) {
            unsigned int *srcPtr = (unsigned int *)(srcOffset - 4);
            do {
                int col = 0;
                do {
                    srcPtr++;
                    unsigned int pixel = *srcPtr;
                    if (col >= (int)(clippedX >> 1) && col < (int)((destWidth + clippedX) >> 1)) {
                        int cr = (pixel >> 24) - 0x80;
                        int cb = (pixel >> 8 & 0xff) - 0x80;
                        *(unsigned short *)destPtr = YUVtoRGB(pixel >> 16 & 0xff, cr, cb);
                        ((unsigned short *)destPtr)[1] = YUVtoRGB(pixel & 0xff, cr, cb);
                        destPtr += 4;
                    }
                    col++;
                } while (col < 320);
                row++;
                destPtr += destStride;
                srcPtr += (srcPitch - 320);
            } while (row < mTex->Height());
        }
        D3DCubeTexture_UnlockRect((D3DCubeTexture *)bufferData, (D3DCUBEMAP_FACES)0, 0);
    }
}

void LiveCameraInput::TextureStore::UpdateFromDepthBufferClip(
    LiveCameraInput *cam, float clipLeft, float clipTop
) {
    void *texels = nullptr;
    unsigned int clippedX = (1 - (int)(clipLeft * -640.0f)) & 0xfffe;
    clippedX = clippedX % 640;
    mTex->TexelsLock(texels);
    unsigned int destBase = (unsigned int)texels;
    void *bufferData = cam->StreamBufferData(kBufferDepth);
    if (bufferData) {
        LockedRect lockedRect;
        cam->LockStream(bufferData, lockedRect);
        unsigned int srcPitch = lockedRect.mPitch >> 1;
        unsigned int clippedY = (1 - (int)(clipTop * -480.0f)) & 0xfffe;
        clippedY = clippedY % 480;
        int srcBase = (int)((clippedY >> 1) * srcPitch * 2 + (int)lockedRect.mBits);
        unsigned int rowIdx = 0;
        if (mTex->Height() > 0) {
            do {
                int texWidth = mTex->Width();
                if ((int)clippedX < (int)(texWidth + clippedX)) {
                    unsigned short *destRow = (unsigned short *)(destBase - 2);
                    unsigned int x = clippedX;
                    do {
                        unsigned short color = 0;
                        unsigned short depthPixel =
                            *(unsigned short *)(((int)(x >> 1) + (unsigned int)((int)x < 0 && (x & 1) != 0)) * 2 + srcBase);
                        if (depthPixel & 3) {
                            int depth = 0x1f - ((depthPixel >> 10) & 0x1f);
                            color = ((depth * 0x20 | depth) << 6) | depth;
                        }
                        destRow++;
                        *destRow = color;
                        x++;
                    } while ((int)x < (int)(mTex->Width() + clippedX));
                }
                unsigned int pitch = mTex->TexelsPitch();
                destBase += (pitch & 0xfffffffe);
                if ((rowIdx & 1) != 0) {
                    srcBase += srcPitch * 2;
                }
                rowIdx++;
            } while ((int)rowIdx < mTex->Height());
        }
        D3DCubeTexture_UnlockRect((D3DCubeTexture *)bufferData, (D3DCUBEMAP_FACES)0, 0);
    }
}

#pragma endregion
#pragma region LiveCameraInput

LiveCameraInput::LiveCameraInput()
    : mConnected(true), mColorPolled(0), mDepthPolled(0), mColorReceived(0), mDepthReceived(0), mSpeechMgr(0) {
    for (int i = 0; i < DIM(mTexClips); i++) {
        mTexClips[i].mTex = nullptr;
    }
    mSpeechMgr = nullptr;
    mNumSnapshots = 0;
    mColorStreamTex = 0;
    mDepthStreamTex = 0;
    mDebugDepthTex = 0;
    mSnapshotBatches.clear();
    mNumSnapshots = 0;
    SkeletonUpdate::Init();
    DataArray *kinectArr = SystemConfig()->FindArray("kinect", false);
    bool b17 = false;
    if (kinectArr) {
        DataArray *speechArr = kinectArr->FindArray("speech");
        b17 = speechArr->FindArray("enabled")->Int(1);
    }
    for (int i = 0; i < kBufferNum; i++) {
        Buffer &cur = mStreams[i];
        cur.mHandle = nullptr;
        cur.mFrames[0] = nullptr;
        cur.mFrames[1] = nullptr;
        cur.mWriteIdx = 0;
        cur.mReadIdx = 1;
        cur.mMat = nullptr;
    }
    int initFlags = 0x4049;
    if (!UsingCD()) {
        initFlags = 0x40004049;
    }
    BeginMemTrackObjectName("NuiInitialize");
    HRESULT initRes = NuiInitialize(initFlags, -1);
    EndMemTrackObjectName();
    if (initRes == E_NUI_DATABASE_NOT_FOUND) {
        MILO_NOTIFY(
            "Could not find NUI database.  Do you have Map DVD Drive enabled in Visual Studio?"
        );
    }
    MILO_ASSERT_FMT(SUCCEEDED(initRes), "NuiInitialize failed (0x%x)", initRes);
    if (b17) {
        mSpeechMgr = new SpeechMgr(kinectArr);
    }
    mAudioInitialized = 0;
    if (SUCCEEDED(NuiAudioCreate(5, NuiAudioErrorCallback, 1, &mAudioHandle, nullptr))) {
        NuiAudioRegisterCallbacks(&mAudioHandle, 1, NuiAudioDataCallback);
        mAudioInitialized = 1;
    }
    bool i6 = kinectArr->FindArray("title_tracked_skeletons")->Int(1);
    HANDLE new_skeleton_event = SkeletonUpdate::NewSkeletonEvent();
    MILO_ASSERT(new_skeleton_event, 0x14D);
    MILO_ASSERT_FMT(
        SUCCEEDED(NuiSkeletonTrackingEnable(new_skeleton_event, i6 ? 2 : 0)),
        "NuiSkeletonTrackingEnable failed"
    );
    BeginMemTrackObjectName("NuiImageStreamOpen:color");
    MILO_ASSERT_FMT(
        SUCCEEDED(NuiImageStreamOpen(
            NUI_IMAGE_TYPE_COLOR_YUV,
            NUI_IMAGE_RESOLUTION_640x480,
            0,
            2,
            nullptr,
            &mStreams[kBufferColor].mHandle
        )),
        "NuiImageStreamOpen color failed"
    );
    EndMemTrackObjectName();
    mStreams[kBufferColor].mMat = CreateCameraBufferMat(640, 480, RndTex::kScratch);
    BeginMemTrackObjectName("NuiImageStreamOpen:depth");
    MILO_ASSERT_FMT(
        SUCCEEDED(NuiImageStreamOpen(
            NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX_IN_COLOR_SPACE,
            NUI_IMAGE_RESOLUTION_320x240,
            0,
            2,
            0,
            &mStreams[kBufferDepth].mHandle
        )),
        "NuiImageStreamOpen depth failed"
    );
    EndMemTrackObjectName();
    mStreams[kBufferDepth].mMat = CreateCameraBufferMat(320, 240, RndTex::kScratch);
    mStreams[kBufferPlayer].mMat = CreateCameraBufferMat(320, 240, RndTex::kScratch);
    mStreams[kBufferPlayerColor].mMat =
        CreateCameraBufferMat(640, 480, RndTex::kScratch);
    RELEASE(mColorStreamTex);
    mColorStreamTex = Hmx::Object::New<DxTex>();
    RELEASE(mDepthStreamTex);
    mDepthStreamTex = Hmx::Object::New<DxTex>();
    DataArray *maxArr = kinectArr->FindArray("camera")->FindArray("max_snapshots", false);
    if (maxArr) {
        mMaxSnapshots = maxArr->Int(1);
    } else {
        MILO_NOTIFY("Could not find max_snapshots in SystemConfig");
        mMaxSnapshots = 1;
    }
    mSnapshotBatches.reserve(6);
    SetColorCameraProperty(NUI_CAMERA_PROPERTY_AE_AWB_MODE, 1);
}

LiveCameraInput::~LiveCameraInput() {
    SkeletonUpdate::Terminate();
    for (int i = 0; i < 4; i++) {
        RndMat *curMat = mStreams[i].mMat;
        RndTex *diffuseTex = curMat ? curMat->GetDiffuseTex() : nullptr;
        delete diffuseTex;
        delete curMat;
        if (mStreams[i].mHandle) {
            CloseHandle(mStreams[i].mHandle);
        }
    }
    ClearSnapshots();
    if (mAudioInitialized) {
        NuiAudioUnregisterCallbacks(&mAudioHandle, NuiAudioDataCallback);
        NuiAudioRelease(&mAudioHandle);
    }
    delete mSpeechMgr;
    NuiShutdown();
}

void LiveCameraInput::PollTracking() {
    mColorPolled = false;
    mDepthPolled = false;
    mColorReceived = false;
    mDepthReceived = false;
    PollNewStream(kBufferColor);
    CameraInput::PollTracking();
}

DataNode OnCameraDumpUnique(DataArray *);
DataNode OnCameraDebugDepth(DataArray *);

void LiveCameraInput::PreInit() {
    if (!sInstance) {
        sInstance = new LiveCameraInput();
        SkeletonUpdate::CreateInstance();
        TheDebug.AddExitCallback(LiveCameraInput::Terminate);
        DataRegisterFunc("camera_dump", OnCameraDumpUnique);
        DataRegisterFunc("camera_debug_depth", OnCameraDebugDepth);
        LoadDebugDepthBuffer(sInstance->mDebugDepthTex);
    }
}

void LiveCameraInput::Init() {
    PreInit();
    if (sInstance) {
        DataArray *cfg = SystemConfig()->FindArray("kinect", false);
        if (cfg) {
            DataArray *speechArr = cfg->FindArray("speech");
            cfg = speechArr;
            speechArr->FindInt("enabled");
        }
        if (sInstance->mSpeechMgr) {
            sInstance->mSpeechMgr->InitGrammars(cfg);
        }
    }
}

void LiveCameraInput::Terminate() {
    if (!sInstance) {
        MILO_ASSERT(sInstance, 0xE2);
    }
    RELEASE(sInstance);
}

RndMat *LiveCameraInput::GetSnapshot(int idx) const {
    if ((idx < 0 || idx >= mNumSnapshots) && mNumSnapshots > 0) {
        MILO_LOG("Snapshot index %d out of bounds [0-%d].", idx, mNumSnapshots);
    } else if (idx < mSnapshots.size()) {
        return mSnapshots[idx];
    }
    return nullptr;
}

int LiveCameraInput::GetSnapshotBatchStartingIndex(int idx) const {
    return idx >= mSnapshotBatches.size() ? mNumSnapshots : mSnapshotBatches[idx];
}

RndTex *LiveCameraInput::GetStoredTexture(int idx) const {
    if (idx >= 0 && idx < mTextureStore.size()) {
        return mTextureStore[idx].mTex;
    } else {
        // lol did they forget the other %d
        MILO_LOG(
            "LiveCameraInput::GetStoredTexture: index %d out of bounds [max=%d]\n",
            mTextureStore.size() - 1
        );
        return nullptr;
    }
}

void LiveCameraInput::InitSnapshots(int numSnapshots) {
    ClearSnapshots();
    if (numSnapshots > 0) {
        MILO_ASSERT(numSnapshots <= mMaxSnapshots, 0x193);
        if (mSnapshots.size() != numSnapshots) {
            mSnapshots.resize(numSnapshots);
            for (int i = 0; i < numSnapshots; i++) {
                mSnapshots[i] = CreateCameraBufferMat(640, 480, RndTex::kRendered);
            }
        }
    }
}

void LiveCameraInput::ClearSnapshots() {
    for (unsigned int i = 0; i < mSnapshots.size(); i++) {
        RndMat *mat = mSnapshots[i];
        RndTex *diffuseTex = mat ? mat->GetDiffuseTex() : nullptr;
        if (diffuseTex) {
            delete diffuseTex;
        }
        if (mat) {
            delete mat;
        }
    }
    mSnapshots.erase(mSnapshots.begin(), mSnapshots.end());
    mNumSnapshots = 0;
    mSnapshotBatches.erase(mSnapshotBatches.begin(), mSnapshotBatches.end());
    TheDxRnd.ReleaseAutoRelease();
}

void LiveCameraInput::InitTextureStore(int max) {
    mMaxTextures = max;
    ClearTextureStore();
}

void LiveCameraInput::ClearTextureStore() {
    unsigned int i = 0;
    while (i < mTextureStore.size()) {
        TextureStore *data = mTextureStore.begin();
        if (data[i].mTex) {
            delete data[i].mTex;
            data[i].mTex = null;
        }
        data[i].mTex = null;
        i++;
    }
    mTextureStore.clear();
    mTextureStore.resize(mMaxTextures);
    mNumStoredTextures = 0;
}

void LiveCameraInput::StartSnapshotBatch() { mSnapshotBatches.push_back(mNumSnapshots); }

void LiveCameraInput::StoreTextureClipAt(
    float f1, float f2, float f3, float f4, int idx1, int idx2
) {
    if (idx1 >= 0 && idx1 < mTextureStore.size()) {
        mTexClips[idx2].StoreTextureClip(mTextureStore[idx1].mTex, f1, f2, f3, f4);
    } else {
        MILO_LOG(
            "LiveCameraInput::StoreColorBufferClip: index %d out of bounds [max=%d]\n",
            idx1,
            mTextureStore.size() - 1
        );
    }
}

void LiveCameraInput::ResetSnapshots() {
    mNumSnapshots = 0;
    mSnapshotBatches.clear();
}

void LiveCameraInput::SetNewFrame(const SkeletonFrame *frame) {
    mNewFrame = frame;
    mCachedFrame = *frame;
}

int LiveCameraInput::NumSnapshots() const { return mNumSnapshots; }
int LiveCameraInput::NumStoredTextures() const { return mNumStoredTextures; }

void LiveCameraInput::PollNewStream(BufferType buf) {
    MILO_ASSERT(kBufferColor == buf || kBufferDepth == buf, 0x227);
    MILO_ASSERT_RANGE(buf, 0, DIM(mStreams), 0x22C);
    Buffer &curBuf = mStreams[buf];
    if (curBuf.mHandle) {
        HRESULT hr =
            NuiImageStreamGetNextFrame(curBuf.mHandle, 0, &curBuf.mFrames[curBuf.mWriteIdx]);
        if (buf == kBufferColor) {
            g_ColorPollCnt++;
            mColorPolled = true;
        } else {
            mDepthPolled = true;
        }
        if (SUCCEEDED(hr)) {
            if (buf == kBufferColor) {
                mColorReceived = true;
            } else {
                mDepthReceived = true;
            }
            mConnected = true;
            mColorStreamTex->SetDeviceTex(nullptr);
            mDepthStreamTex->SetDeviceTex(nullptr);
            if (curBuf.mFrames[curBuf.mReadIdx]) {
                HRESULT hr =
                    NuiImageStreamReleaseFrame(curBuf.mHandle, curBuf.mFrames[curBuf.mReadIdx]);
                MILO_ASSERT(SUCCEEDED(hr), 0x24E);
                curBuf.mFrames[curBuf.mReadIdx] = 0;
            }
            curBuf.mWriteIdx = curBuf.mWriteIdx - 1U & 1;
            curBuf.mReadIdx = curBuf.mReadIdx - 1U & 1;
        } else if (hr == E_NUI_DEVICE_NOT_CONNECTED) {
            mConnected = false;
        } else if (hr == (HRESULT)0x83010001 && buf == kBufferColor) {
            g_ColorNoFrameDataCnt++;
        }
    }
}

void *LiveCameraInput::StreamBufferData(BufferType type) const {
    MILO_ASSERT(type < kBufferNum, 0x1FC);
    int i3;
    if (type == kBufferPlayer) {
        i3 = 1;
    } else {
        i3 = type == kBufferPlayerColor ? 1 : 0;
    }
    auto& _sub1 = mStreams[type];
    if (_sub1.mFrames[i3]) {
        return _sub1.mFrames[i3]->pFrameTexture;
    } else {
        return nullptr;
    }
}

RndMat *LiveCameraInput::DisplayMat(BufferType type) const {
    MILO_ASSERT(type < kBufferNum, 0x20F);
    return mStreams[type].mMat;
}

RndTex *LiveCameraInput::DisplayTex(BufferType type) const {
    RndMat *mat = DisplayMat(type);
    if (mat) {
        return mat->GetDiffuseTex();
    } else {
        return nullptr;
    }
}

void LiveCameraInput::DumpProperties() {
    MILO_LOG("NUI CAMERA PROPERTIES ****************************************\n");
    union {
        LONG lValue;
        FLOAT fValue;
    };
    NuiCameraGetProperty(NUI_CAMERA_TYPE_COLOR, NUI_CAMERA_PROPERTY_AE_AWB_MODE, &lValue);
    MILO_LOG("%s:\n", "NUI_CAMERA_PROPERTY_AE_AWB_MODE");
    if (lValue == NUI_CAMERA_PROPERTY_AE_AWB_MODE_STANDARD) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_AE_AWB_MODE_STANDARD");
    }
    if (lValue == NUI_CAMERA_PROPERTY_AE_AWB_MODE_FACEBASED) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_AE_AWB_MODE_FACEBASED");
    }
    if (lValue == NUI_CAMERA_PROPERTY_AE_AWB_MODE_OFF) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_AE_AWB_MODE_OFF");
    }
    if (lValue == NUI_CAMERA_PROPERTY_AE_AWB_MODE_DEFAULT) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_AE_AWB_MODE_DEFAULT");
    }
    NuiCameraGetProperty(
        NUI_CAMERA_TYPE_COLOR, NUI_CAMERA_PROPERTY_FRAME_RATE_MAX, &lValue
    );
    MILO_LOG("%s:\n", "NUI_CAMERA_PROPERTY_FRAME_RATE_MAX");
    if (lValue == NUI_CAMERA_PROPERTY_FRAME_RATE_MAX_30FPS) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_FRAME_RATE_MAX_30FPS");
    }
    if (lValue == NUI_CAMERA_PROPERTY_FRAME_RATE_MAX_15FPS) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_FRAME_RATE_MAX_15FPS");
    }
    if (lValue == NUI_CAMERA_PROPERTY_FRAME_RATE_MAX_10FPS) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_FRAME_RATE_MAX_10FPS");
    }
    NuiCameraGetProperty(
        NUI_CAMERA_TYPE_COLOR, NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN, &lValue
    );
    MILO_LOG("%s:\n", "NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN");
    if (lValue == NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN_30FPS) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN_30FPS");
    }
    if (lValue == NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN_15FPS) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN_15FPS");
    }
    if (lValue == NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN_10FPS) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN_10FPS");
    }
    if (lValue == NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN_LOWEST) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN_LOWEST");
    }
    if (lValue == NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MODE_MIN_15FPS) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MODE_MIN_15FPS");
    }
    if (lValue == NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MODE_MIN_10FPS) {
        MILO_LOG("      %s\n", "NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MODE_MIN_10FPS");
    }
    NuiCameraGetPropertyF(
        NUI_CAMERA_TYPE_COLOR, NUI_CAMERA_PROPERTYF_EXPOSURE_TIME, &fValue
    );
    MILO_LOG("%s:\t\t%.4f\n", "NUI_CAMERA_PROPERTYF_EXPOSURE_TIME", fValue);
    NuiCameraGetPropertyF(
        NUI_CAMERA_TYPE_COLOR, NUI_CAMERA_PROPERTYF_AE_EXPOSURE_COMPENSATION, &fValue
    );
    MILO_LOG("%s:\t\t%.4f\n", "NUI_CAMERA_PROPERTYF_AE_EXPOSURE_COMPENSATION", fValue);
    NuiCameraGetPropertyF(NUI_CAMERA_TYPE_COLOR, NUI_CAMERA_PROPERTYF_COLOR_GAIN, &fValue);
    MILO_LOG("%s:\t\t%.4f\n", "NUI_CAMERA_PROPERTYF_COLOR_GAIN", fValue);
    NuiCameraGetPropertyF(
        NUI_CAMERA_TYPE_COLOR, NUI_CAMERA_PROPERTYF_AE_FACEBASED_MAX_GAIN, &fValue
    );
    MILO_LOG("%s:\t\t%.4f\n", "NUI_CAMERA_PROPERTYF_AE_FACEBASED_MAX_GAIN", fValue);
    NUI_CAMERA_AE_ROI region;
    HRESULT hr = NuiCameraGetExposureRegionOfInterest(NUI_CAMERA_TYPE_COLOR, &region);
    if (hr != ERROR_SUCCESS) {
        MILO_LOG("NuiCameraGetExposureRegionOfInterest returned bad result.\n");
    } else {
        MILO_LOG(
            "Region of Interest: left(%.3f) top(%.3f) width(%.3f) height(%.3f)\n",
            region.Left,
            region.Top,
            region.Width,
            region.Height
        );
    }
    MILO_LOG("**************************************************************\n");
}

void LiveCameraInput::IncrementSnapshotCount() {
    if (mNumSnapshots < mSnapshots.size()) {
        mNumSnapshots++;
    } else {
        MILO_NOTIFY("Max snapshots already taken.");
    }
}

int LiveCameraInput::NumSnapshotBatches() const { return mSnapshotBatches.size(); }

void LiveCameraInput::NuiAudioErrorCallback(HRESULT hr) {
    if (hr != ERROR_SUCCESS) {
        MILO_NOTIFY("NuiAudioErrorCallback reached (0x%x)", hr);
    }
}

// SpeechMgr::mVoiceDirection — use public getter/setter instead of raw byte offset

void LiveCameraInput::NuiAudioDataCallback(NUIAUDIO_RESULTS *results) {
    if (!sInstance)
        return;
    if (!sInstance->mSpeechMgr)
        return;
    if (!sInstance->mSpeechMgr->Recognizing())
        return;

    float confidence = results->Confidence;
    float beamAngle = results->BeamAngle;
    if (confidence > 0.2f) {
        sInstance->mBeamAngle = beamAngle;
        sInstance->mBeamConfidence = confidence;
        side = (int)(beamAngle / Abs(beamAngle)) + side;
        if (side > 10) {
            side = 10;
        } else if (side < -10) {
            side = -10;
            goto checkSide;
        } else {
            goto checkSide;
        }
        sInstance->mSpeechMgr->SetVoiceDirection(0);
        return;
    } else {
        if (side != 0) {
            int absVal = side < 0 ? -side : side;
            side = side - side / absVal;
        }
    checkSide:
        if (side == 10) {
            sInstance->mSpeechMgr->SetVoiceDirection(0);
            return;
        }
        if (side == -10) {
            sInstance->mSpeechMgr->SetVoiceDirection(1);
        }
    }
}

bool LiveCameraInput::SetAutoexposure(bool enable) {
    SetColorCameraProperty(NUI_CAMERA_PROPERTY_AE_AWB_MODE, enable ? 1 : 0);
    return enable;
}

bool LiveCameraInput::GetAutoexposure() const {
    long val = GetColorCameraProperty(NUI_CAMERA_PROPERTY_AE_AWB_MODE);
    return val == 1;
}

bool LiveCameraInput::SetTweakedAutoexposure(bool enable) {
    SetColorCameraProperty(NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN, enable ? 2 : 0);
    NUI_CAMERA_AE_ROI region;
    region.Left = 0.0f;
    region.Top = 0.0f;
    region.Width = 1.0f;
    region.Height = 1.0f;
    if (enable && !GetExposureRegion(region)) {
        MILO_NOTIFY("Could not find ae_region in SystemConfig");
    }
    HRESULT hr = NuiCameraSetExposureRegionOfInterest(NUI_CAMERA_TYPE_COLOR, &region);
    if (!SUCCEEDED(hr)) {
        TheDebug << MakeString(
            "Autoexposure region not set. NuiCameraSetExposureRegionOfInterest failed (0x%x)\n",
            hr
        );
    }
    return enable;
}

bool LiveCameraInput::GetTweakedAutoexposure() const {
    long val = GetColorCameraProperty(NUI_CAMERA_PROPERTY_AE_FRAME_RATE_MIN);
    NUI_CAMERA_AE_ROI currentRegion;
    HRESULT hr = NuiCameraGetExposureRegionOfInterest(NUI_CAMERA_TYPE_COLOR, &currentRegion);
    if (!SUCCEEDED(hr)) {
        MILO_FAIL(
            "NuiCameraGetExposureRegionOfInterest failed (0x%x)", hr
        );
    }
    NUI_CAMERA_AE_ROI configRegion;
    bool gotRegion = GetExposureRegion(configRegion);
    if (gotRegion) {
        if (val != 2
            || Abs(currentRegion.Left - configRegion.Left) >= 0.0001f
            || Abs(currentRegion.Top - configRegion.Top) >= 0.0001f
            || Abs(currentRegion.Width - configRegion.Width) >= 0.0001f
            || Abs(currentRegion.Height - configRegion.Height) >= 0.0001f) {
            return false;
        }
        return true;
    }
    return false;
}

#define NUI_CAMERA_AE_ROI_MINIMUM_WIDTH 0.15f
#define NUI_CAMERA_AE_ROI_MINIMUM_HEIGHT 0.15f

void LiveCameraInput::SetExposureRegion(float left, float top, float width, float height) {
    MILO_ASSERT(width >= NUI_CAMERA_AE_ROI_MINIMUM_WIDTH, 0x41f);
    MILO_ASSERT(height >= NUI_CAMERA_AE_ROI_MINIMUM_HEIGHT, 0x420);
    MILO_ASSERT(left >= 0 && top >= 0 && width >= 0 && height >= 0, 0x421);
    MILO_ASSERT(left + width <= 1.0f, 0x422);
    MILO_ASSERT(top + height <= 1.0f, 0x423);
    NUI_CAMERA_AE_ROI region;
    region.Left = left;
    region.Top = top;
    region.Width = width;
    region.Height = height;
    HRESULT hr = NuiCameraSetExposureRegionOfInterest(NUI_CAMERA_TYPE_COLOR, &region);
    if (!SUCCEEDED(hr)) {
        MILO_NOTIFY(
            "Autoexposure region not set. NuiCameraSetExposureRegionOfInterest failed (0x%x)",
            hr
        );
    }
}

void LiveCameraInput::SetTrackedSkeletons(int id1, int id2) const {
    DWORD ids[2];
    ids[0] = id1;
    ids[1] = id2;
    NuiSkeletonSetTrackedSkeletons(ids);
}

int LiveCameraInput::StoreTexture(RndTex *tex) {
    if (mNumStoredTextures >= mMaxTextures) {
        MILO_ASSERT(mNumStoredTextures==mTextureStore.size(), 799);
        MILO_LOG(
            "LiveCameraInput::AddTextureToStore: No room available. Max textures=\n",
            mMaxTextures
        );
        return 0;
    } else {
        for (int i = 0; i < mTextureStore.size(); i++) {
            if (!mTextureStore[i].mTex) {
                mTextureStore[i].mTex = tex;
                return i;
            }
        }
        mTextureStore[mNumStoredTextures++].StoreTexture(tex);
        return mNumStoredTextures - 1;
    }
}

void LiveCameraInput::StoreTextureAt(RndTex *tex, int idx) {
    if (idx >= 0 && idx < mMaxTextures) {
        mTextureStore[idx].StoreTexture(tex);
    } else {
        // i think they forgot the second %d
        MILO_LOG(
            "LiveCameraInput::StoreTextureAt: index %d out of bounds [max=%d]\n",
            mTextureStore.size() - 1
        );
    }
}

void LiveCameraInput::ApplyTextureClip(RndMat *mat, int idx) const {
    if (idx < 0 || idx >= 8) {
        MILO_LOG(
            "LiveCameraInput::GetStoredTexture: index %d out of bounds [max=%d]\n", 7
        );
    }
    const CamTexClip *clip = &mTexClips[idx];
    mat->SetTexGen(kTexGenXfmOrigin);
    mat->SetTexXfm(clip->mXfm);
    mat->SetDiffuseTex(clip->mTex);
}

void LiveCameraInput::StoreColorBuffer(int idx) {
    if (idx >= 0 && idx < mTextureStore.size()) {
        mTextureStore[idx].StoreColorBuffer(this);
    } else {
        MILO_LOG(
            "LiveCameraInput::StoreColorBuffer: index %d out of bounds [max=%d]\n",
            idx,
            mTextureStore.size() - 1
        );
    }
}

void LiveCameraInput::StoreColorBufferClip(
    float f1, float f2, float f3, float f4, int idx
) {
    if (idx >= 0 && idx < mTextureStore.size()) {
        mTextureStore[idx].StoreColorBufferClip(this, f1, f2, f3, f4);
    } else {
        MILO_LOG(
            "LiveCameraInput::StoreColorBufferClip: index %d out of bounds [max=%d]\n",
            idx,
            mTextureStore.size() - 1
        );
    }
}

void LiveCameraInput::StoreDepthBuffer(int idx) {
    if (idx >= 0 && idx < mTextureStore.size()) {
        mTextureStore[idx].StoreDepthBuffer(this);
    } else {
        MILO_LOG(
            "LiveCameraInput::StoreDepthBufferAt: index %d out of bounds [max=%d]\n",
            idx,
            mTextureStore.size() - 1
        );
    }
}

void LiveCameraInput::StoreDepthBufferClip(
    float f1, float f2, float f3, float f4, int idx
) {
    if (idx >= 0 && idx < mTextureStore.size()) {
        mTextureStore[idx].StoreDepthBufferClip(this, f1, f2, f3, f4);
    } else {
        MILO_LOG(
            "LiveCameraInput::StoreDepthBufferClip: index %d out of bounds [max=%d]\n",
            idx,
            mTextureStore.size() - 1
        );
    }
}

RndTex *LiveCameraInput::GetStreamTex(BufferType type) const {
    MILO_ASSERT(type == kBufferColor || type == kBufferDepth, 0x1EA);
    void *bufferData = StreamBufferData(type);
    DxTex *tex;
    if (type == kBufferColor) {
        tex = mColorStreamTex;
    } else {
        tex = mDepthStreamTex;
    }
    tex->SetDeviceTex((D3DTexture *)bufferData);
    RndTex *result = tex;
    if (type == kBufferDepth) {
        if (mDebugDepthTex != nullptr && (bufferData == nullptr || gDebugDepth != 0)) {
            result = mDebugDepthTex;
        }
    }
    return result;
}

#pragma endregion
#pragma region AnonymousNamespace

namespace {

long GetColorCameraProperty(NUI_CAMERA_PROPERTY prop) {
    long result = 0;
#ifndef HX_NATIVE
    HRESULT hr = NuiCameraGetProperty(NUI_CAMERA_TYPE_COLOR, prop, &result);
    if (!SUCCEEDED(hr)) {
        TheDebug << MakeString(
            "NuiCameraGetProperty failed.  Property 0x%x, error (0x%x)\n", prop, hr
        );
    }
#endif
    return result;
}

bool GetExposureRegion(NUI_CAMERA_AE_ROI &region) {
    static Symbol kinect("kinect");
    static Symbol camera("camera");
    static Symbol ae_region("ae_region");
    DataArray *arr = SystemConfig(kinect, camera)->FindArray(ae_region, false);
    if (arr) {
        region.Left = arr->Float(1);
        region.Top = arr->Float(2);
        region.Width = arr->Float(3);
        region.Height = arr->Float(4);
    }
    return arr != nullptr;
}

} // namespace

#ifdef HX_NATIVE
DataNode OnCameraDumpUnique(DataArray *) { return DataNode(0); }
DataNode OnCameraDebugDepth(DataArray *) { return DataNode(0); }
#else

void LiveCameraInput::LockStream(const void *buf, LockedRect &rect) {
    D3DLOCKED_RECT d3dRect;
    if (buf) {
        D3DLineTexture_LockRect((D3DLineTexture *)buf, 0, &d3dRect, nullptr, 0x10);
        rect.mBits = d3dRect.pBits;
    } else {
        d3dRect.Pitch = 0;
        rect.mBits = nullptr;
    }
    rect.mPitch = d3dRect.Pitch;
}

void LiveCameraInput::UnlockStream(const void *buf) {
    if (buf != nullptr) {
        D3DCubeTexture_UnlockRect((D3DCubeTexture *)buf, (D3DCUBEMAP_FACES)0, 0);
    }
}

DataNode OnCameraDebugDepth(DataArray *) {
    gDebugDepth = !gDebugDepth;
    return DataNode(0);
}

void CameraDump(const char *filename) {
    LiveCameraInput *cam = LiveCameraInput::sInstance;
    if (!cam->mDepthPolled) {
        cam->PollNewStream(LiveCameraInput::kBufferDepth);
    }
    RndTex *tex = cam->GetStreamTex(LiveCameraInput::kBufferDepth);
    int texSize = tex->Width() * tex->Height() * tex->Bpp() / 8;
    void *buf = MemAlloc(texSize, "unknown", 0, "unknown", 0);
    void *texels;
    tex->TexelsLock(texels);
    memcpy(buf, texels, texSize);
    tex->TexelsUnlock();
    FileStream fs(filename, FileStream::kWrite, true);
    if (fs.Fail()) {
        MILO_NOTIFY("Screenshot failed; could not open destination file (%s).", filename);
    } else {
        fs.Write(buf, texSize);
    }
}

void CameraDumpUnique(const char *name) {
    String uniqueName = UniqueFilename(name, "data");
    CameraDump(uniqueName.c_str());
}

DataNode OnCameraDumpUnique(DataArray *) {
    CameraDumpUnique("camera");
    return DataNode(0);
}

#endif

#pragma endregion
