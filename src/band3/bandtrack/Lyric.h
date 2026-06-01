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
    int mLead; // 0x44
    bool mWordEnd; // 0x48
    bool mChunkEnd; // 0x49
    int mDeployIdx; // 0x4c
    bool mAfterMidPhraseShift; // 0x50
    Vector3 mBeginPos; // 0x54
    float mXWidth; // 0x60
    float mHighlightMs; // 0x64
    float mActiveMs; // 0x68
    float mEndMs; // 0x6c
    float mInvalidateMs; // 0x70
    bool mPhraseEnd; // 0x74
    Hmx::Color mLastColor; // 0x78
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
    Hmx::Color mPreviewColor; // 0x3c
    Hmx::Color mActiveColor; // 0x4c
    Hmx::Color mNowColor; // 0x5c
    Hmx::Color mPastColor; // 0x6c
    Hmx::Color mPreviewPhonemeColor; // 0x7c
    Hmx::Color mActivePhonemeColor; // 0x8c
    Hmx::Color mNowPhonemeColor; // 0x9c
    Hmx::Color mPastPhonemeColor; // 0xac
    RndText::Style mPitchedStyle; // 0xbc
    RndText::Style mUnpitchedStyle; // 0xd4
    float mInvalidateMs; // 0xec
    bool mBaked; // 0xf0
    bool mNeedSync; // 0xf1
    bool mPastNow; // 0xf2
};
