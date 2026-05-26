#pragma once
#include "gesture/BaseSkeleton.h"
#include "hamobj/DancerSkeleton.h"
#include "utl/MemMgr.h"

struct FreestyleMoveFrame {
    DancerSkeleton skeleton; // 0x0
    float mBeat; // 0x2d8

#ifdef HX_NATIVE
    static void *operator new[](size_t s) {
#else
    static void *operator new[](unsigned int s) {
#endif
        return _MemAllocTemp(s, __FILE__, 0x10, "FreestyleMoveFrame", 0);
    }
    static void operator delete(void *v) {
        MemFree(v, __FILE__, 0x10, "FreestyleMoveFrame");
    }
    static void operator delete[](void *v) {
        MemFree(v, __FILE__, 0x10, "FreestyleMoveFrame");
    }
};

// size 0x12c0
struct DepthFrame {
    char filler[0x12c0];

#ifdef HX_NATIVE
    static void *operator new[](size_t s) {
#else
    static void *operator new[](unsigned int s) {
#endif
        return _MemAllocTemp(s, __FILE__, 0x26, "DepthFrame", 0);
    }
    static void operator delete[](void *v) { MemFree(v, __FILE__, 0x26, "DepthFrame"); }
};

// size 0x1c
class FreestyleMove {
public:
    FreestyleMove();
    virtual ~FreestyleMove();

    void Clear();
    void Init(int);
    void CalcCentering(int);
    void Free();
    void RecordSkeletonFrame(BaseSkeleton *, int, float);

    MEM_OVERLOAD(FreestyleMove, 0x18);

    DepthFrame *mDepthFrames; // 0x4
    int mNumFrames; // 0x8 - num frames
    int unkc;
    int unk10;
    int unk14;
    FreestyleMoveFrame *mFrames; // 0x18
};

// size 0x10
class FreestyleMoveRecorder;

struct FreestyleFrameScores {
    friend class FreestyleMoveRecorder;
    FreestyleFrameScores() {
        unk0.resize(60);
        Clear();
    }

    void Clear() {
        unkc = 0;
        for (int i = 0; i < 60; i++) {
            unk0[i] = 0;
        }
    }

private:
    std::vector<float> unk0;
    int unkc;
};
