#pragma once
#include "char/CharClip.h"
#include "hamobj/Difficulty.h"
#include "hamobj/HamCharacter.h"
#include "hamobj/HamDriver.h"
#include "math/Key.h"
#include "math/Utl.h"
#include "obj/Dir.h"
#include "rndobj/PropAnim.h"
#include "utl/Symbol.h"

class ClipPlayer {
public:
    ClipPlayer()
        : mClipKeys(nullptr), mClipCrossoverKeys(nullptr), mMasterClipKeys(nullptr),
          mPlayerIndex(0), mPracticeStart(-kHugeFloat), mPracticeEnd(kHugeFloat), mInClip(nullptr),
          mOutClip(nullptr), mTargetClip(0), mBeatOffset(0) {}

    void PlayAnims(HamCharacter *, float, float, int);
    bool Init(int);
    bool Init(Difficulty);
    bool CanUseRestStep();

    DataNode AnnotatePractice();
    DataNode AnnotateClip(float);

    static const char *sRestStepNames[4];

protected:
    bool Init(RndPropAnim *);
    void PlayNormal(float, HamDriver::LayerArray *, const char *);
    float ClipLength(CharClip *);
    void PlayClip(CharClip *, float, float, HamDriver::LayerArray *);
    bool PushExpertClip(int, HamDriver::LayerArray *);
    CharClip *GetTransitionBefore(Key<Symbol> *);
    CharClip *GetPrevRoutineTransition(int);
    CharClip *GetRoutineTransition(const char *, Key<Symbol> *);
#ifdef HX_NATIVE
    bool GetRoutineCrossoverClips(float, const char *, CharClip **, CharClip **);
#else
    void GetRoutineCrossoverClips(float, const char *, CharClip **, CharClip **);
#endif
    bool PushRoutineBuilderClip(int, HamDriver::LayerArray *);
    bool GetClipRange(const char *, const char *, float, float &, float &, float &);
    void PushClip(int, HamDriver::LayerArray *);

    Keys<Symbol, Symbol> *mClipKeys; // 0x0
    Keys<Symbol, Symbol> *mClipCrossoverKeys; // 0x4
    Keys<Symbol, Symbol> *mMasterClipKeys; // 0x8
    float mBeat; // 0xc
    float mPrevBeat; // 0x10
    int mPlayerIndex; // 0x14
    ObjectDir *mClipDir; // 0x18
    HamDriver *mDriver; // 0x1c
    float mPracticeStart; // 0x20
    float mPracticeEnd; // 0x24
    CharClip *mInClip; // 0x28
    CharClip *mOutClip; // 0x2c
    CharClip *mRestClip; // 0x30
    CharClip *mRestStepClips[4]; // 0x34
    int mClipCount; // 0x44
    int mTargetClip; // 0x48
    int unk4c; // 0x4c
    float mBeatOffset; // 0x50
};
