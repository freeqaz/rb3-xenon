#pragma once
#include "VocalScoreHistory.h"
#include "bandtrack/DelayLine.h"
#include "game/GameMic.h"
#include "game/TambourineDetector.h"
#include "synth/MicManagerInterface.h"
#include "dsp/VibratoDetector.h"
#include "synth/VoiceBeat.h"
#include "utl/SongPos.h"
#include <vector>

class VocalPlayer;

class SingerResultsData {
public:
    SingerResultsData() { Reset(); }
    ~SingerResultsData() {}
    SingerResultsData(const SingerResultsData& o) {
        int w0 = *reinterpret_cast<const int*>(&o.targetPitchHitScore);
        int w4 = *reinterpret_cast<const int*>(&o.micPitchHitScore);
        *reinterpret_cast<int*>(&targetPitchHitScore) = w0;
        *reinterpret_cast<int*>(&micPitchHitScore) = w4;
        phraseCount = o.phraseCount;
        int wc = *reinterpret_cast<const int*>(&o.centsDeviation);
        int w10 = *reinterpret_cast<const int*>(&o.targetPitchAccuracy);
        *reinterpret_cast<int*>(&centsDeviation) = wc;
        *reinterpret_cast<int*>(&targetPitchAccuracy) = w10;
        int w14 = *reinterpret_cast<const int*>(&o.centsVariance);
        int w18 = *reinterpret_cast<const int*>(&o.phraseScore);
        *reinterpret_cast<int*>(&centsVariance) = w14;
        *reinterpret_cast<int*>(&phraseScore) = w18;
        scoreFrameCount = o.scoreFrameCount;
    }
    SingerResultsData& operator=(const SingerResultsData& o) {
        int w0 = *reinterpret_cast<const int*>(&o.targetPitchHitScore);
        int w4 = *reinterpret_cast<const int*>(&o.micPitchHitScore);
        *reinterpret_cast<int*>(&targetPitchHitScore) = w0;
        *reinterpret_cast<int*>(&micPitchHitScore) = w4;
        phraseCount = o.phraseCount;
        int wc = *reinterpret_cast<const int*>(&o.centsDeviation);
        int w10 = *reinterpret_cast<const int*>(&o.targetPitchAccuracy);
        *reinterpret_cast<int*>(&centsDeviation) = wc;
        *reinterpret_cast<int*>(&targetPitchAccuracy) = w10;
        int w14 = *reinterpret_cast<const int*>(&o.centsVariance);
        int w18 = *reinterpret_cast<const int*>(&o.phraseScore);
        *reinterpret_cast<int*>(&centsVariance) = w14;
        *reinterpret_cast<int*>(&phraseScore) = w18;
        scoreFrameCount = o.scoreFrameCount;
        return *this;
    }
    void Reset() {
        targetPitchHitScore = 0;
        micPitchHitScore = 0;
        phraseCount = 0;
        centsDeviation = 0;
        targetPitchAccuracy = 0;
        phraseScore = 0;
        centsVariance = 0;
        scoreFrameCount = 0;
    }

    float targetPitchHitScore;      // 0x0
    float micPitchHitScore;         // 0x4
    int phraseCount;                // 0x8
    float centsDeviation;           // 0xc
    float targetPitchAccuracy;      // 0x10
    float centsVariance;            // 0x14
    float phraseScore;              // 0x18
    int scoreFrameCount;            // 0x1c
};

class Singer {
public:
    class AmbiguousData {
    public:
        int part1;                  // 0x0
        int part2;                  // 0x4
        bool isResolved;            // 0x8
        int winningPart;            // 0xc
        float ambiguousPoints;      // 0x10
        AmbiguousData() {}
        AmbiguousData(const AmbiguousData &o)
            : part1(o.part1), part2(o.part2), isResolved(o.isResolved), winningPart(o.winningPart), ambiguousPoints(o.ambiguousPoints) {}
        AmbiguousData &operator=(const AmbiguousData &o) {
            int t4 = o.part2;
            part1 = o.part1;
            part2 = t4;
            isResolved = o.isResolved;
            winningPart = o.winningPart;
            ambiguousPoints = o.ambiguousPoints;
            return *this;
        }
    };
    Singer(VocalPlayer *, int);
    ~Singer();

    void NoteTambourineSwing(float);
    void CreateMicClientID();
    void PostLoad();
    GameMic *GetGameMic() const;
    MicClientID GetMicClientID() const;
    void SetMicProcessing(bool, bool);
    void Start();
    void StartIntro();
    void Restart(bool);
    void CancelScream();
    void ClearFreestyleDeployment();
    void ClearScoreHistories();
    void SetPaused(bool);
    void Jump(float, bool);
    void Rollback(float, float);
    void ProcessTalkyData();
    void DetectScream(float, float, float);
    void SetIsSinging(bool);
    void Detune(float);
    void SetFrameMicPitch(float);
    void EnableController();
    void DisableController();
    void SetOctaveOffset(int);
    void AppendToScoreHistory(float, int, float, int);
    float GetHistoricalScore(float, int) const;
    VocalScoreHistory &AccessScoreHistory(int);
    VocalScoreCache &AccessScoreCache(int);
    const VocalScoreCache &AccessScoreCache(int) const;
    void AllScoresAreIn(const std::vector<int> &);
    void SetAutoplayToPart(int);
    int GetAutoplayToPart() const;
    void SetAutoplayVariationMagnitude(float);
    float GetAutoplayVariationMagnitude() const;
    void SetAutoplayOffset(float);
    float GetAutoplayOffset() const;
    void HandlePhraseEnd(float, const std::vector<float> &);
    float GetPartPercentage(int) const;
    void GetPitchDeviation(float &, float &) const;
    void ClearPitchHistory();
    void UpdatePitchHistory(float);
    int SuddenOctaveShift(float) const;
    void UpdatePitchDeviation(float);
    int GetFrameMatchType();
    float AddToFreestyleDeployment(float);
    void ResolveAmbiguity();
    void SetAssignedPart(int, float);
    void Poll(float, const SongPos &, float, float);
    void Poll_(float, const SongPos &, float, float, float, float);
    void AddAmbiguousPart(int, int);
    void DisableAmbiguousPart(int, int);

    float GetFrameMicPitch() const { return mFrameMicPitch; }
    float GetFrameTargetPitch() const { return mFrameTargetPitch; }
    int GetSingerIndex() const { return mSingerIndex; }
    int GetFrameAssignedPart() const { return mFrameAssignedPart; }
    bool HasAssignedPart() const;

    VocalPlayer *mPlayer; // 0x0
    MicClientID mMicClientID; // 0x4
    bool unkc; // 0x8 — usage unknown
    int mSingerIndex; // 0x10
    bool unk14; // 0x14 — usage unknown
    char *unk18; // 0x18 — pointer, possibly to talk/talky data (released in dtor)
    int unk1c; // 0x1c — usage unknown
    int mIsSinging; // 0x20
    float mDetune; // 0x24
    float mMaxDetune; // 0x28
    float mCurrentFrameTime; // 0x2c
    int unk30; // 0x30 — usage unknown
    int unk34; // 0x34 — usage unknown
    float mTambourineDeploymentSuppressMs; // 0x38
    float mTambourineActivationTime; // 0x3c
    float mLastTambourineTime; // 0x40
    float mTotalTambourineDeployment; // 0x44
    float mScreamStartTime; // 0x48
    float mScreamEnergyThreshold; // 0x4c
    float mScreamMinDurationMs; // 0x50
    float mMicPitchOffset; // 0x54
    int unk58; // 0x58 — usage unknown
    float mFrameMicPitch; // 0x5c
    float mLastFrameMicEnergy; // 0x60
    float mSmoothedMicEnergy; // 0x64
    float mFrameTargetPitch; // 0x68
    float mFrameBestHitScore; // 0x6c
    int mFrameAssignedPart; // 0x70
    float mBestTargetPitch; // 0x74
    int mOctaveOffset; // 0x78
    float unk7c; // 0x7c — usage unknown
    bool mScreamOccurred; // 0x80
    int unk84; // 0x84 — usage unknown
    float unk88; // 0x88 — usage unknown
    float mPitchHistory[5]; // 0x8c
    float mPitchHistoryMean; // 0xa0
    int mPitchHistoryIndex; // 0xa4
    int mPitchHistoryValidCount; // 0xa8
    DelayLine<float, 100> mPossibleVibratoPoints; // 0xac
    VibratoDetector *mVibrato; // 0x240
    float mAccumulatedVibratoBonusPoints; // 0x244
    float mVibratoFrameBonus; // 0x248
    float mVibratoBonusAccumulator; // 0x24c
    int mAutoplayPart; // 0x250
    float mAutoplayVariationMagnitude; // 0x254
    float mAutoplayOffset; // 0x258
    std::vector<VocalScoreHistory> mScoreHistories; // 0x25c
    std::vector<VocalScoreCache> mScoreCaches; // 0x264
    std::vector<SingerResultsData> mResultsData; // 0x26c
    std::vector<AmbiguousData> mAmbiguousData; // 0x274
    TambourineDetector mTambourineDetector; // 0x27c
    float mPitchDeviationMean; // 0x29c
    float mPitchDeviationDev; // 0x2a0
    int mPitchDeviationFrameCount; // 0x2a4
    TalkyMatcher *mTalkyMatcher; // 0x2a8
};