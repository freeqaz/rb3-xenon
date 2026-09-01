#pragma once
#include <vector>
#include <float.h>
#include "rndobj/Text.h"
#include "obj/Object.h"
#include "obj/ObjMacros.h"
#include "utl/Str.h"
#include "track/TrackWidget.h"
#include "beatmatch/VocalNote.h"

class Lyric : public Hmx::Object {
public:
    Lyric(const VocalNote *, bool, String, bool);
    virtual ~Lyric();

    int StartTick() const;
    float Width() const;
    float EndPos() const;
    bool PitchNote() const;
    void SetChunkEnd(bool);
    void SetAfterDeploy(int);
    void SetAfterMidPhraseLyricShift(bool);
    bool UpdateColor(Hmx::Color);
    bool GetChunkEnd() const { return mChunkEnd; }

    int mIdx; // 0x28
    String mText; // 0x2c
    bool mPitched; // 0x38
    std::vector<const VocalNote *> mVocalNotes; // 0x3c
    int mLead; // 0x48
    bool mWordEnd; // 0x4c
    bool mChunkEnd; // 0x4d
    int mDeployIdx; // 0x50
    bool mAfterMidPhraseShift; // 0x54
    Vector3 mBeginPos; // 0x58
    float mXWidth; // 0x68
    float mHighlightMs; // 0x6c
    float mActiveMs; // 0x70
    float mEndMs; // 0x74
    float mInvalidateMs; // 0x78
    bool mPhraseEnd; // 0x7c
    Hmx::Color mLastColor; // 0x80
};

class LyricPlate : public Hmx::Object {
public:
    LyricPlate(RndText *, const RndText *, const RndText *);
    virtual ~LyricPlate();

    void SetShowing(bool);
    float CurrentStartX(float) const;
    float CurrentEndX(float) const;
    void CheckSync();
    void Reset();
    Lyric *LatestLyric();
    void AddLyric(Lyric *);
    float EstimateLyricWidth(const Lyric *);
    void HookUpParents(RndGroup *, RndTransformable *);
    bool Empty() const;
    void UpdateStaticTiming(float);
    float GetBeginMs() const;
    float GetLastLyricXBeforeMS(float) const;
    void Poll(float);
    void BakeLyric(Lyric *);
    bool Baked() const { return mBaked; }

    float mWidthX; // 0x28
    int mNumCharsUsed; // 0x2c
    RndText *mText; // 0x30
    std::vector<Lyric *> mSyllables; // 0x34
    Hmx::Color mPreviewColor; // 0x40
    Hmx::Color mActiveColor; // 0x50
    Hmx::Color mNowColor; // 0x60
    Hmx::Color mPastColor; // 0x70
    Hmx::Color mPreviewPhonemeColor; // 0x80
    Hmx::Color mActivePhonemeColor; // 0x90
    Hmx::Color mNowPhonemeColor; // 0xa0
    Hmx::Color mPastPhonemeColor; // 0xb0
    RndText::Style mPitchedStyle; // 0xc0
    RndText::Style mUnpitchedStyle; // 0xe4
    float mInvalidateMs; // 0x108
    bool mBaked; // 0x10c
    bool mNeedSync; // 0x10d
    bool mPastNow; // 0x10e
};
