#pragma once
#include "beatmatch/VocalNote.h"
#include "game/Singer.h"

class VocalPlayer;
class TalkyMatcher;

bool VocalNoteEndCmp(float, const VocalNote &);

class VocalPart {
public:
    VocalPart(VocalPlayer *, int);
    ~VocalPart();

    void SetDifficultyVariables(int);
    void PostLoad();
    void CalcNoteWeights();
    float GetNoteSliceWeight(float, float, int) const;
    void Start();
    void StartIntro();
    void UpdateSongMinMaxPitch();
    void Restart(bool);
    void UpdateMinMaxPitch(const VocalPhrase *const &);
    void SetPaused(bool);
    void Jump(float, bool);
    void LocalDeployBandEnergy();
    void EnableScoring(bool);
    bool ScoringEnabled() const;
    void SetRemotePhraseMeterFrac(float);
    bool InTambourinePhrase() const;
    int CalculateRemainingTambourineTicks();
    void ForcePhrasePointDelta(float);
    float FramePhraseMeterFrac() const;
    int GetSpotlightPhrase() const;
    void SetPhraseScoreMultiplier(float);
    void SetPhraseRank(int);
    bool AtPhraseEnd(float) const;
    bool InEmptyPhrase() const;
    bool InPlayablePhrase() const;
    bool PhraseHasUnpitchedNotes() const;
    void HandlePhraseEnd(int &, float &, float &, int &, float);
    float GetOverallPartHitPercentage() const;
    int CurrentPhraseIndex() const;
    void OnGameOver();
    const VocalPhrase *GetFirstPhraseMarker() const;
    const VocalPhrase *GetNextPhraseMarker(const VocalPhrase *const &) const;
    bool IsPhraseMarkerAtEnd(const VocalPhrase *const &) const;
    bool IsEmptyPhrase(const VocalPhrase *const &) const;
    void Rollback(float, float);
    float GetFreestyleSectionDurationMs() const;
    float GetPartHitPercentage(const std::vector<VocalPhrase> &, int, int) const;
    int NumPracticePhrases(const std::vector<VocalPhrase> &) const;
    void ResetScoring();
    float CalcPhraseScoreMax(const VocalPhrase *const &) const;
    void AddScore(const VocalScoreCache &);
    void CalculateScore(float, int, float, VocalScoreCache &) const;
    float ScoreNote(float, int, float &, int &, float &, float &) const;
    float GetBestHit(
        float, int, int, TalkyMatcher *, float &, float, int &, int &, float &,
        float &, bool &
    );
    void ScoreSinger(
        float, float, float, float, int, TalkyMatcher *, VocalScoreCache &, int &,
        float &
    );
    void Poll(float, const SongPos &);
    float GetSloppyPitch(float, int, float, float &) const;
    bool CouldScoreAgainstPart(float, TalkyMatcher *, float, float, float &);
    void AddPhrasePoints(float);
    void GetNoteRange(float, int &, int &);
    void AfterPoll(float);
    bool NearNote(float);
    void SetFirstPhraseMsToScore(float);
    void SetVocalNoteList(VocalNoteList *);
    void AddSingerCandidate(Singer *, float);
    void ClearSingerCandidates();
    Singer *GetBestSingerCandidate();
    bool HasBestSingerCandidate();

    int PartIndex() const { return mPartIndex; }
    float MaxPhraseScore() const { return mPhraseScoreMax; }
    bool InFreestyleSection() const { return mInFreestyleSection; }

    static bool FramePhraseMeterFracSorter(const VocalPart *, const VocalPart *);

    VocalPlayer *mPlayer; // 0x0
    int mPartIndex; // 0x4
    VocalNoteList *mVocalNoteList; // 0x8
    std::pair<float, float> *mFreestyleSection; // 0xc
    std::vector<float> mNoteWeights; // 0x10
    int unk18;
    int unk1c;
    float unk20;
    int unk24;
    int unk28;
    float mRemotePhraseMeterFrac; // 0x30
    float mPhraseScorePartMultiplier; // 0x34
    float mPhraseScoreMax; // 0x38
    float unk38;
    int unk3c;
    float mPhraseScore; // 0x44
    float unk44;
    float unk48;
    float unk4c;
    int unk50;
    float unk54;
    int unk58;
    const VocalPhrase *mThisPhrase; // 0x60
    int mPhraseValue; // 0x64
    float mSlop; // 0x68
    float mPitchSigma; // 0x6c
    float mPitchMaximumDistance; // 0x70
    float mPitchHitMultiplier; // 0x74
    float mNonPitchHitMultiplier; // 0x78
    float mShortNoteThresh; // 0x7c
    float mShortNoteMult; // 0x80
    float mNoteLengthFactor; // 0x84
    float unk84;
    int unk88;
    int mSpotlightPhraseID; // 0x90
    float mNonPitchEasyMultiplier; // 0x94
    float mPhraseScoreCapGrowth; // 0x98
    int unk98;
    float unk9c;
    float unka0;
    float unka4;
    float unka8;
    bool mInFreestyleSection; // 0xb0
    bool unkad;
    float unkb0;
    bool unkb4;
    float mFirstPhraseMsToScore; // 0xbc
    float unkbc;
    Singer *mBestSinger; // 0xc4
    float mBestSingerPitchDistance; // 0xc8
    int unkc8;
    bool mScoringEnabled; // 0xd0
    int mPhraseRank; // 0xd4
    float mTalkyEnergyThreshold; // 0xd8
};