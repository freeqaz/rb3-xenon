#include "xdk/LIBCMT/vectorintrinsics.h"
#include "xdk/xapilibi/xbox.h"
#include <stdio.h>
#include <new>

extern "C" void XMScalarSinCos(float *pSin, float *pCos, float Value);

struct tagRECT {
    int left, top, right, bottom;
};

namespace HeadOrientation {

enum ANGLE_TYPE {
    ANGLE_YAW = 0,
    ANGLE_PITCH = 1,
    ANGLE_ROLL = 2,
};

struct PrevFrameAngles {
    float yawAngle;
    float pitchAngle;
    float unk8;
    bool useFilter;
};

class LDARegressor {
public:
    LDARegressor();
    ~LDARegressor();
    long LoadLDA(const char *path);
    long LoadKNN(const char *path);
    bool compute(unsigned char *data, float prevAngle, bool useFilter, float *outAngle);

    char pad[0x34];
    ANGLE_TYPE mAngleType;
};

} // namespace HeadOrientation

// Forward-declare FaceCommon templates
namespace FaceCommon {
template <typename T> T *XMemNew(unsigned long allocAttrs, unsigned int count);
template <typename T> void XMemDelete(T *&ptr, unsigned long freeAttrs);
}

namespace HeadOrientation {

class HeadTracker {
public:
    long Initialize(const char *basePath);
    void Destroy();
    long ComputeOrientation(
        const unsigned char *imageData, unsigned int width, unsigned int height,
        unsigned int stride, const PrevFrameAngles *prevAngles,
        const tagRECT *faceRect, void *workspace, unsigned long workspaceSize,
        __vector4 *outOrientation
    );

    LDARegressor *mRollRegressor;
    LDARegressor *mYawRegressor;
    LDARegressor *mPitchRegressor;
    void *mCallback;
};

static const __vector4 vecOne = { 1.0f, 1.0f, 1.0f, 1.0f };
static const __vector4 vecZero = { 0.0f, 0.0f, 0.0f, 0.0f };

void resize_64_64(const unsigned char *src, int srcWidth, int srcHeight, int srcStride,
                  int left, int top, int right, int bottom, float *dst);

__vector4 QuaternionFromPitchYawRoll(float pitch, float yaw, float roll);

long CreateRegressor(LDARegressor **outRegressor, ANGLE_TYPE type,
                     const char *basePath, const char *pcaFile, const char *knnFile) {
    LDARegressor *regressor = FaceCommon::XMemNew<LDARegressor>(0x209c0000, 1);
    LDARegressor *local = regressor;
    if (regressor == 0) {
        long hr = (long)0x8007000E;
        FaceCommon::XMemDelete<LDARegressor>(local, 0x209c0000);
        return hr;
    }

    char path[260];
    sprintf_s(path, "%s%s", basePath, pcaFile);
    long hr = regressor->LoadLDA(path);
    if (hr >= 0) {
        sprintf_s(path, "%s%s", basePath, knnFile);
        hr = regressor->LoadKNN(path);
        if (hr >= 0) {
            regressor->mAngleType = type;
            *outRegressor = regressor;
            return hr;
        }
    }

    FaceCommon::XMemDelete<LDARegressor>(local, 0x209c0000);
    return hr;
}

long HeadTracker::Initialize(const char *basePath) {
    long hr = CreateRegressor(&mRollRegressor, ANGLE_ROLL, basePath,
                              "roll_pca_lda.bin.be", "roll_knn.bin.be");
    if (hr >= 0) {
        hr = CreateRegressor(&mYawRegressor, ANGLE_YAW, basePath,
                             "yaw_pca_lda.bin.be", "yaw_knn.bin.be");
        if (hr >= 0) {
            hr = CreateRegressor(&mPitchRegressor, ANGLE_PITCH, basePath,
                                 "pitch_pca_lda.bin.be", "pitch_knn.bin.be");
            if (hr >= 0) {
                return hr;
            }
        }
    }
    Destroy();
    return hr;
}

void HeadTracker::Destroy() {
    FaceCommon::XMemDelete<LDARegressor>(mRollRegressor, 0x209c0000);
    FaceCommon::XMemDelete<LDARegressor>(mYawRegressor, 0x209c0000);
    FaceCommon::XMemDelete<LDARegressor>(mPitchRegressor, 0x209c0000);
}

} // namespace HeadOrientation

namespace FaceCommon {

template <typename T>
T *XMemNew(unsigned long allocAttrs, unsigned int count) {
    T *ptr = (T *)XMemAlloc(count * sizeof(T), allocAttrs);
    if (ptr != 0) {
        unsigned int i = count;
        T *cur = ptr;
        while (i != 0) {
            if (cur != 0) {
                new (cur) T();
            }
            cur++;
            i--;
        }
    }
    return ptr;
}

template <typename T>
void XMemDelete(T *&ptr, unsigned long freeAttrs) {
    if (ptr != 0) {
        unsigned int count = XMemSize(ptr, freeAttrs) / sizeof(T);
        if (count != 0) {
            T *cur = ptr;
            for (unsigned int i = 0; i < count; i++) {
                cur->~T();
                cur++;
            }
        }
        XMemFree(ptr, freeAttrs);
        ptr = 0;
    }
}

template HeadOrientation::LDARegressor *XMemNew<HeadOrientation::LDARegressor>(unsigned long, unsigned int);
template void XMemDelete<HeadOrientation::LDARegressor>(HeadOrientation::LDARegressor *&, unsigned long);

} // namespace FaceCommon
