#include "game/Singer.h"
#include "VocalScoreHistory.h"
#include "dsp/VibratoDetector.h"
#include "game/BandUser.h"
#include "game/Defines.h"
#include "game/GameMic.h"
#include "game/GameMicManager.h"
#include "game/SongDB.h"
#include "game/VocalPlayer.h"
#include "net/Net.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "synth/MicManagerInterface.h"
#include "synth/VoiceBeat.h"
#include <algorithm>

// vector<SingerResultsData> internal element-copy paths (_M_fill_insert_aux's
// backward element shift, plus the forward copies) need an 8x-unrolled word
// copy whose load/store pairing schedule matches the target. The generic
// stlport copy helpers route through SingerResultsData::operator= and emit a
// pairing that loads each pair's low word first (and the backward shift only
// unrolls 2x). Routing the copy through a POD word-struct that mirrors
// SingerResultsData's layout in a count-based loop reproduces the target: the
// 8x unroll and the high-word-first pairing within each 2-word group.
#ifndef HX_NATIVE
// stlpmtx_std-internal copy_ptrs / copy_backward_ptrs specializations are
// asm-match-only — libstdc++ doesn't expose those templates. Gate the whole
// block (X7b GameGemList precedent).
namespace stlpmtx_std {

struct _SingerW2 { unsigned int a, b; };
struct _SingerResultsWords {
    _SingerW2 ab;     // targetPitchHitScore, micPitchHitScore    -> words 0,1 (paired)
    unsigned int c;   // phraseCount                              -> word 2  (single)
    _SingerW2 de;     // centsDeviation, targetPitchAccuracy      -> words 3,4 (paired)
    _SingerW2 fg;     // centsVariance, phraseScore               -> words 5,6 (paired)
    unsigned int h;   // scoreFrameCount                          -> word 7  (single)
};

// _M_fill_insert_aux backward element shift: opens a gap by moving existing
// elements toward the back (decrementing).
template <>
inline SingerResultsData*
__copy_backward_ptrs<SingerResultsData*, SingerResultsData*>(
    SingerResultsData* __first, SingerResultsData* __last,
    SingerResultsData* __result, const __false_type& /*TrivialAssignment*/
) {
    _SingerResultsWords* __d = (_SingerResultsWords*)__result;
    _SingerResultsWords* __l = (_SingerResultsWords*)__last;
    for (ptrdiff_t __n = __last - __first; __n > 0; --__n) {
        *--__d = *--__l;
    }
    return (SingerResultsData*)__d;
}

// resize()'s erase shift (copy(__last, end, __first)) is a forward element copy
// that MWCC unrolls 8x in the target; routing it through the POD word-struct in
// a count-based loop reproduces the 8x unroll and the high-word-first pairing.
template <>
inline SingerResultsData*
__copy_ptrs<SingerResultsData*, SingerResultsData*>(
    SingerResultsData* __first, SingerResultsData* __last,
    SingerResultsData* __result, const __false_type& /*IsOKToMemCpy*/
) {
    _SingerResultsWords* __d = (_SingerResultsWords*)__result;
    const _SingerResultsWords* __s = (const _SingerResultsWords*)__first;
    for (ptrdiff_t __n = __last - __first; __n > 0; --__n) {
        *__d = *__s;
        ++__s;
        ++__d;
    }
    return (SingerResultsData*)__d;
}

// mAmbiguousData.clear() -> erase(begin,end) -> copy(last,finish,first), a
// forward element copy MWCC unrolls 8x in the target. Routing it through a POD
// word-struct mirroring AmbiguousData's layout reproduces the target's 8x
// unroll and high-word-first pairing for the leading int pair. The trailing
// float member must stay typed so the copy emits lfs/stfs (not lwz/stw).
struct _AmbiguousW2 { unsigned int a, b; };
struct _AmbiguousWords {
    _AmbiguousW2 ab;     // part1, part2             -> words 0,1 (paired)
    unsigned char c;     // isResolved (bool)        -> byte at 0x8
    unsigned int d;      // winningPart              -> word at 0xc
    float e;             // ambiguousPoints (float)  -> lfs/stfs at 0x10
};

template <>
inline ::Singer::AmbiguousData*
__copy_ptrs< ::Singer::AmbiguousData*, ::Singer::AmbiguousData*>(
    ::Singer::AmbiguousData* __first, ::Singer::AmbiguousData* __last,
    ::Singer::AmbiguousData* __result, const __false_type& /*IsOKToMemCpy*/
) {
    _AmbiguousWords* __d = (_AmbiguousWords*)__result;
    const _AmbiguousWords* __s = (const _AmbiguousWords*)__first;
    for (ptrdiff_t __n = __last - __first; __n > 0; --__n) {
        *__d = *__s;
        ++__s;
        ++__d;
    }
    return (::Singer::AmbiguousData*)__d;
}

} // namespace stlpmtx_std
#endif // !HX_NATIVE

MicClientID sNullClientID(-1, -1);

Singer::Singer(VocalPlayer *vp, int n)
    : mPlayer(vp), unkc(0), mSingerIndex(n), unk14(0), unk18(0), unk1c(0), mIsSinging(0),
      mDetune(0), mCurrentFrameTime(0), unk30(0), mTambourineDeploymentSuppressMs(100.0f), mTambourineActivationTime(0), mLastTambourineTime(0), mTotalTambourineDeployment(0),
      mScreamStartTime(-1.0f), mScreamEnergyThreshold(0.8f), mScreamMinDurationMs(500.0f), mFrameMicPitch(0),
      mLastFrameMicEnergy(0), mSmoothedMicEnergy(0), mFrameTargetPitch(0), mFrameAssignedPart(-1), mBestTargetPitch(0), mOctaveOffset(0),
      unk7c(0), mScreamOccurred(0), unk84(0), unk88(0), mPitchHistoryMean(0), mPitchHistoryIndex(0), mPitchHistoryValidCount(0), mVibrato(0),
      mAccumulatedVibratoBonusPoints(0), mVibratoFrameBonus(0), mVibratoBonusAccumulator(-1.0f), mAutoplayPart(-1),
      mAutoplayVariationMagnitude(0), mAutoplayOffset(0),
      mTambourineDetector(vp->mTambourineManager, this), mPitchDeviationMean(0), mPitchDeviationDev(0), mPitchDeviationFrameCount(0) {
    CreateMicClientID();
    Difficulty diff = mPlayer->GetUser()->GetDifficulty();
    DataArray *cfg = SystemConfig("scoring", "vocals");
    cfg->FindArray("pitch_margin")->Float(diff + 1); // lol what happened to this
    mMaxDetune = cfg->FindFloat("max_detune");
    mScreamEnergyThreshold = cfg->FindFloat("scream_energy_threshold");
    mTambourineDeploymentSuppressMs = cfg->FindFloat("tambourine_deployment_suppress_ms");
    mVibrato = new VibratoDetector(0, 100);
    mTalkyMatcher = new TalkyMatcher();
    for (int i = 0; i < 5; i++)
        mPitchHistory[i] = 0;

    if (n == 0) {
        GameMic *mic = TheGameMicManager->GetMic(mMicClientID);
        if (mic) {
            DataNode node = DataVariable("playback_file");
            if (node.Type() == kDataString) {
                if (strlen(node.Str()) != 0) {
                    mic->SetInputFile(node.Str());
                }
            }
        }
    }
}

Singer::~Singer() {
    RELEASE(mTalkyMatcher);
    RELEASE(mVibrato);
    RELEASE(unk18);
}

void Singer::PostLoad() {
    int numParts = mPlayer->NumVocalParts();
    for (int i = 0; i < numParts; i++) {
        mScoreHistories.push_back(VocalScoreHistory(i, mSingerIndex));
    }
    mScoreCaches.resize(numParts);
    mResultsData.resize(numParts);
    MILO_ASSERT(mTalkyMatcher, 0xB6);
    mTalkyMatcher->LoadEvents(
        TheSongDB->GetData()->mVocalFeatureVectorTimes,
        TheSongDB->GetData()->mVocalFeatureVectorPeaks
    );
}

void Singer::CreateMicClientID() {
    BandUser *u = mPlayer->GetUser();
    if ((!TheNet.GetNetSession()->HasUser(u) || !u->IsLocal()) && !u->IsNullUser()) {
        mMicClientID = sNullClientID;
    } else {
        mMicClientID = MicClientID(mSingerIndex, -1);
    }
}

GameMic *Singer::GetGameMic() const { return TheGameMicManager->GetMic(mMicClientID); }
MicClientID Singer::GetMicClientID() const { return mMicClientID; }

void Singer::SetMicProcessing(bool b1, bool b2) {
    GameMic *mic = TheGameMicManager->GetMic(mMicClientID);
    if (mic)
        mic->SetEnablePitchDetection(b1);
    if (mTalkyMatcher)
        mTalkyMatcher->SetEnableTalkyMatcher(b2);
}

void Singer::Start() {}
void Singer::StartIntro() {}

void Singer::Restart(bool b1) {
    CancelScream();
    mFrameAssignedPart = -1;
    ClearFreestyleDeployment();
    ClearScoreHistories();
    mSmoothedMicEnergy = 0;
    mScreamOccurred = 0;
    if (!b1) {
        FOREACH (it, mResultsData) {
            it->Reset();
        }
        mPitchDeviationMean = 0;
        mPitchDeviationDev = 0;
        mPitchDeviationFrameCount = 0;
    }
    mAmbiguousData.clear();
}

void Singer::SetPaused(bool) {}

void Singer::Jump(float, bool) {
    CancelScream();
    mFrameAssignedPart = -1;
    ClearFreestyleDeployment();
    ClearScoreHistories();
    mAmbiguousData.clear();
}

void Singer::Rollback(float, float) {
    CancelScream();
    ClearFreestyleDeployment();
    mAmbiguousData.clear();
}

void Singer::ProcessTalkyData() {
    MILO_ASSERT(mTalkyMatcher, 0x2DC);
    GameMic *mic = GetGameMic();
    if (!mic)
        mTalkyMatcher->Reset();
    else {
        float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
        const short *s = nullptr;
        int i28 = 0;
        mic->AccessContinuousSamples(s, i28);
        mTalkyMatcher->Analyze(s, i28, secs * 1000.0f);
    }
}

void Singer::DetectScream(float f1, float f2, float f3) {
    MILO_ASSERT(mPlayer->IsLocal(), 0x2F6);
    if (f3 >= mScreamEnergyThreshold) {
        if (mScreamStartTime < 0) {
            mScreamStartTime = f1;
        } else if (f1 - mScreamStartTime > mScreamMinDurationMs && mPlayer->mIsInCoda && !mScreamOccurred) {
            mScreamOccurred = true;
            mPlayer->HitCoda();
        }
    } else
        CancelScream();
}

void Singer::CancelScream() { mScreamStartTime = -1.0f; }

void Singer::SetIsSinging(bool b1) { mIsSinging = b1; }
void Singer::Detune(float f1) { mDetune = f1; }

void Singer::HandlePhraseEnd(float, const std::vector<float> &phraseMaxPoints) {
    MILO_ASSERT(mResultsData.size() == phraseMaxPoints.size(), 0x3BF);
    for (int i = 0; (unsigned)i < mResultsData.size(); i++) {
        float maxPoints = phraseMaxPoints[i];
        float accuracy = mResultsData[i].targetPitchAccuracy;
        if (maxPoints > 0.0f) {
            float clamped = std::max(std::min(accuracy / maxPoints, 1.0f), 0.0f);
            mResultsData[i].phraseScore += clamped;
            mResultsData[i].scoreFrameCount++;
        }
        mResultsData[i].targetPitchAccuracy = 0.0f;
        mResultsData[i].centsVariance = 0.0f;
        mResultsData[i].centsDeviation = 0.0f;
        float hitScore = mResultsData[i].targetPitchHitScore;
        if (maxPoints > 0.0f) {
            float clamped = std::max(std::min(hitScore / maxPoints, 1.0f), 0.0f);
            mResultsData[i].micPitchHitScore += clamped;
            mResultsData[i].phraseCount++;
        }
        mResultsData[i].targetPitchHitScore = 0.0f;
    }
    mAmbiguousData.clear();
}

void Singer::SetFrameMicPitch(float f1) { mFrameMicPitch = f1; }
void Singer::EnableController() {}
void Singer::DisableController() {}

void Singer::SetOctaveOffset(int i1) {
    if (i1 != mOctaveOffset)
        mOctaveOffset = i1;
}

void Singer::AppendToScoreHistory(float f1, int i2, float f3, int i4) {
    VocalScoreHistory &history = mScoreHistories[i2];
    history.AddScore(f1, f3);
    history.SetOctaveOffset(i4);
}

float Singer::GetHistoricalScore(float f1, int i2) const {
    return mScoreHistories[i2].CalculateSum(f1);
}

VocalScoreHistory &Singer::AccessScoreHistory(int idx) { return mScoreHistories[idx]; }
VocalScoreCache &Singer::AccessScoreCache(int idx) { return mScoreCaches[idx]; }
const VocalScoreCache &Singer::AccessScoreCache(int idx) const {
    return mScoreCaches[idx];
}

void Singer::AllScoresAreIn(const std::vector<int> &assignedParts) {
    MILO_ASSERT(mResultsData.size() == mScoreCaches.size(), 0x4B6);
    for (int i = 0; (unsigned)i < mResultsData.size(); i++) {
        float cacheUnk4 = mScoreCaches[i].unk4;
        float sum = mResultsData[i].targetPitchAccuracy + cacheUnk4;
        float cacheUnk8 = mScoreCaches[i].unk8;
        mResultsData[i].targetPitchAccuracy = std::min(cacheUnk8, sum);
        mResultsData[i].centsVariance += mScoreCaches[i].unkc;
        mResultsData[i].centsDeviation += mScoreCaches[i].unk0;
    }
    for (AmbiguousData *entry = &mAmbiguousData[0]; entry != &mAmbiguousData[0] + mAmbiguousData.size(); entry++) {
        if (entry->isResolved)
            continue;
        int part0 = entry->part1;
        if (part0 != mFrameAssignedPart) {
            if (std::find(assignedParts.begin(), assignedParts.end(), part0) != assignedParts.end()) {
                entry->isResolved = true;
                continue;
            }
        }
        int part4 = entry->part2;
        if (part4 != mFrameAssignedPart) {
            if (std::find(assignedParts.begin(), assignedParts.end(), part4) != assignedParts.end()) {
                entry->isResolved = true;
            }
        }
    }
}

void Singer::NoteTambourineSwing(float f1) {
    ClearFreestyleDeployment();
    mTambourineActivationTime = f1 + mTambourineDeploymentSuppressMs;
}

void Singer::ClearFreestyleDeployment() {
    mTambourineActivationTime = 0;
    mLastTambourineTime = 0;
    mTotalTambourineDeployment = 0;
}

void Singer::SetAutoplayToPart(int part) { mAutoplayPart = part; }
int Singer::GetAutoplayToPart() const { return mAutoplayPart; }
void Singer::SetAutoplayVariationMagnitude(float f1) { mAutoplayVariationMagnitude = f1; }
float Singer::GetAutoplayVariationMagnitude() const {
    return mAutoplayVariationMagnitude;
}
void Singer::SetAutoplayOffset(float f1) { mAutoplayOffset = f1; }
float Singer::GetAutoplayOffset() const { return mAutoplayOffset; }

void Singer::ClearScoreHistories() {
    FOREACH (it, mScoreHistories) {
        it->Reset();
    }
}

void Singer::ClearPitchHistory() {
    mPitchHistoryMean = 0;
    mPitchHistoryIndex = 0;
    mPitchHistoryValidCount = 0;
    mPitchHistory[0] = 0;
    mPitchHistory[1] = 0;
    mPitchHistory[2] = 0;
    mPitchHistory[3] = 0;
    mPitchHistory[4] = 0;
}

void Singer::UpdatePitchHistory(float pitch) {
    if ((unsigned int)mPitchHistoryIndex > 4) {
        TheDebug.Notify(MakeString("pitch history index out of bounds (%d) singer %d", mPitchHistoryIndex, mSingerIndex));
        ClearPitchHistory();
    }
    float prev = mPitchHistory[mPitchHistoryIndex];
    if ((pitch > 0.0f) != (prev > 0.0f)) {
        if (pitch > 0.0f) {
            mPitchHistoryValidCount += 1;
            mPitchHistoryMean = mPitchHistoryMean + (pitch - mPitchHistoryMean) / (float)mPitchHistoryValidCount;
        } else {
            mPitchHistoryValidCount -= 1;
            if (mPitchHistoryValidCount == 0) ClearPitchHistory();
            if ((unsigned int)mPitchHistoryValidCount > 5) {
                TheDebug.Notify(MakeString("pitch history valid frames out of bounds (%d)", mPitchHistoryValidCount));
                ClearPitchHistory();
            }
        }
    } else if (pitch > 0.0f) {
        mPitchHistoryMean = mPitchHistoryMean + (pitch - prev) / (float)mPitchHistoryValidCount;
    }
    mPitchHistory[mPitchHistoryIndex] = pitch;
    mPitchHistoryIndex = (mPitchHistoryIndex + 1) % 5;
}

int Singer::SuddenOctaveShift(float pitch) const {
    int sign;
    if (mPitchHistoryValidCount >= 1) {
        if (pitch > 0.0f) {
        int shift = 0;
        if (pitch > mPitchHistoryMean) sign = 1;
        else sign = -1;
        float step = 12.0f * (float)sign;
        float a0 = mPitchHistoryMean;
        goto check;
    update:
        pitch -= step;
    check:
        float diff = pitch - a0;
        shift += sign;
        if (!(diff > 0.0f)) diff = -diff;
        if (diff > 10.0f) goto update;
        return shift;
    }
    }
    return 0;
}

void Singer::UpdatePitchDeviation(float pitch) {
    int count = mPitchDeviationFrameCount + 1;
    mPitchDeviationFrameCount = count;
    float dev = mPitchDeviationDev;
    float mean = mPitchDeviationMean;
    float newMean = mean + (pitch - mean) / (float)count;
    mPitchDeviationMean = newMean;
    mPitchDeviationDev = dev + (std::fabs(pitch - newMean) - dev) / (float)count;
}

float Singer::GetPartPercentage(int part) const {
    const SingerResultsData &rd = mResultsData[part];
    if (rd.scoreFrameCount == 0) return 0.0f;
    return rd.phraseScore / (float)rd.scoreFrameCount;
}

int Singer::GetFrameMatchType() {
    if (mFrameAssignedPart != -1) {
        return mPlayer->mVocalParts[mFrameAssignedPart]->unk98;
    }
    return 4;
}

float Singer::AddToFreestyleDeployment(float val) {
    if (mFrameMicPitch < mScreamEnergyThreshold) {
        mLastTambourineTime = 0;
        mTotalTambourineDeployment = 0;
    } else if (val >= mTambourineActivationTime) {
        if (mLastTambourineTime > 0.0f) {
            float diff = val - mLastTambourineTime;
            if (diff > 0.0f) {
                mTotalTambourineDeployment += diff;
            }
        }
        mLastTambourineTime = val;
    }
    return mTotalTambourineDeployment;
}

void Singer::ResolveAmbiguity() {
    for (AmbiguousData *entry = &mAmbiguousData[0];
         entry != &mAmbiguousData[0] + mAmbiguousData.size(); entry++) {
        if (!entry->isResolved || entry->winningPart == -1)
            continue;
        int part1 = entry->part1;
        int part2 = entry->part2;
        float points1 = mResultsData[part1].centsDeviation;
        float points2 = mResultsData[part2].centsDeviation;
        float delta = points1 - points2;
        float maxPoints = (points1 < points2) ? points2 : points1;
        if (std::fabs(delta) / maxPoints > 0.1f) {
            int iWinningPart = (delta > 0.0f) ? part1 : part2;
            int iLosingPart = (delta < 0.0f) ? part1 : part2;
            MILO_ASSERT(iWinningPart != iLosingPart, 0x1B4);
            if (iWinningPart != entry->winningPart) {
                float pts = entry->ambiguousPoints;
                mPlayer->SwapAmbiguousPoints(pts, iLosingPart, iWinningPart);
                mResultsData[iLosingPart].targetPitchHitScore -= pts;
                if (mResultsData[iLosingPart].targetPitchHitScore < 0.0f)
                    mResultsData[iLosingPart].targetPitchHitScore = 0.0f;
                mResultsData[iWinningPart].targetPitchHitScore += pts;
            }
            entry->winningPart = -1;
        }
    }
}

static int sMinVibratoFrames = 0;
static float sMaxVibratoFrameBonus = 20.0f;

void Singer::Poll_(float ms, const SongPos &, float micPitch, float micEnergy, float, float) {
    bool isLocal = mPlayer->IsLocal();

    if (micPitch != 0.0f) {
        micPitch -= mMicPitchOffset;
    }
    mSmoothedMicEnergy = 0.9f * (mSmoothedMicEnergy - micEnergy) + micEnergy;

    if ((!isLocal || mAutoplayPart != -1) && !mPlayer->AtLastPhrase()) {
        mPlayer->CurrentPhrase();
        int phraseIdx = -1;
        if (!isLocal) {
            phraseIdx = 0;
        } else if (mAutoplayPart >= 0) {
            if (mAutoplayPart < mPlayer->NumVocalParts() && !mPlayer->IgnorePhrase()) {
                phraseIdx = mAutoplayPart;
            }
        }
        if (phraseIdx != -1) {
            micPitch = mPlayer->mVocalParts[phraseIdx]->mVocalNoteList->PitchAt(ms);
        } else {
            micPitch = 0.0f;
        }
        micEnergy = 0.0f;
        if (micEnergy != micPitch) {
            if ((mAutoplayPart != -1 || mIsSinging != 0) && !mPlayer->AtLastPhrase()) {
                float t = ms / 1000.0f;
                micPitch += mDetune;
                micEnergy = 1.0f;
                micPitch += mAutoplayVariationMagnitude *
                                (float)sin(6.2831f * t + 0.5f * (3.1415f * (float)mSingerIndex)) +
                            mAutoplayOffset;
            } else {
                micEnergy = 0.0f;
                micPitch = micEnergy;
            }
        } else if (mIsSinging != 0) {
            micEnergy = 1.0f;
        }
    }

    if (mPlayer->mEnabledState != kPlayerEnabled) {
        micPitch = 0.0f;
        micEnergy = micPitch;
    }

    if (isLocal) {
        mTambourineDetector.CheckForSwing(ms, micEnergy);
    }

    if (isLocal && mPlayer->AtLastPhrase()) {
        DetectScream(ms, micPitch, micEnergy);
        mFrameMicPitch = micPitch;
        mLastFrameMicEnergy = micEnergy;
        mFrameTargetPitch = 0.0f;
        mFrameBestHitScore = 0.0f;
        return;
    }

    int frames = mVibrato->Analyze(micPitch);
    if (frames != 0) {
        if (frames < sMinVibratoFrames) {
            frames = sMinVibratoFrames;
        } else if (frames > 100) {
            frames = 100;
        }
        for (int i = 0; i < frames; i++) {
            mAccumulatedVibratoBonusPoints += mPossibleVibratoPoints[i];
        }
    }

    float bonus = std::min(mAccumulatedVibratoBonusPoints, sMaxVibratoFrameBonus);
    mVibratoFrameBonus = bonus;
    mFrameMicPitch = micPitch;
    mAccumulatedVibratoBonusPoints -= bonus;
    mLastFrameMicEnergy = micEnergy;
    mCurrentFrameTime = ms;

    VocalFrameSpewData *spew = mPlayer->mFrameSpewData;
    if (spew) {
        spew->mSingerData[mSingerIndex].unk0 = mFrameMicPitch;
        spew->mSingerData[mSingerIndex].unk4 = mLastFrameMicEnergy;
    }
}

void Singer::Poll(float ms, const SongPos &pos, float f3, float f4) {
    GameMic *mic = TheGameMicManager->GetMic(mMicClientID);
    if (mic && mic->GetMyMic()->IsRunning()) {
        mic->Update();
        Poll_(ms, pos, mic->unk2c, mic->unk28, f3, f4);
    } else {
        Poll_(ms, pos, 0.0f, 0.0f, f3, f4);
    }
    ProcessTalkyData();
    mFrameAssignedPart = -1;
    mBestTargetPitch = 0.0f;
    for (std::vector<VocalScoreCache>::iterator it = mScoreCaches.begin();
         it != mScoreCaches.end(); ++it) {
        it->unk0 = 0.0f;
        it->unk4 = 0.0f;
        it->unk8 = 0.0f;
        it->unkc = 0.0f;
        it->unk10 = 0.0f;
        it->unk14 = 0.0f;
        it->unk1c = 0;
        it->unk20 = false;
        it->unk21 = false;
        it->unk22 = false;
        it->unk24 = 0.0f;
    }
}

void Singer::AddAmbiguousPart(int i_iPart1, int i_iPart2) {
    MILO_ASSERT(i_iPart1 < i_iPart2, 0x13E);
    bool bFound = false;
    for (AmbiguousData *iter = &mAmbiguousData[0];
         iter != &mAmbiguousData[0] + mAmbiguousData.size(); iter++) {
        if (iter->part1 == i_iPart1 || iter->part1 == i_iPart2) {
            bFound = true;
            break;
        }
    }
    if (!bFound) {
        AmbiguousData entry;
        entry.part1 = i_iPart1;
        entry.part2 = i_iPart2;
        entry.isResolved = false;
        entry.winningPart = -1;
        entry.ambiguousPoints = -1.0f;
        mAmbiguousData.push_back(entry);
    }
}

void Singer::DisableAmbiguousPart(int i_iPart1, int i_iPart2) {
    if (mAmbiguousData.size() != 0) {
        MILO_ASSERT(i_iPart1 < i_iPart2, 0x16C);
        for (AmbiguousData *iter = &mAmbiguousData[0];
             iter != &mAmbiguousData[0] + mAmbiguousData.size(); iter++) {
            bool match = false;
            if (iter->part1 == i_iPart1 && iter->part2 == i_iPart2) {
                match = true;
            }
            if (match) {
                if (!iter->isResolved) {
                    iter->isResolved = true;
                }
                return;
            }
        }
    }
}

void Singer::GetPitchDeviation(float &mean, float &dev) const {
    mean = mPitchDeviationMean;
    dev = mPitchDeviationDev;
}

void Singer::SetAssignedPart(int part, float f2) {
    mFrameAssignedPart = part;
    if (mVibratoFrameBonus != 0.0f) {
        mScoreCaches[part].unk4 += mVibratoFrameBonus;
        mVibratoFrameBonus = 0.0f;
    }
    mScoreHistories[part].BiasLastScore(f2);
    float assignedPoints = mScoreCaches[part].unk4;
    float unk0 = mResultsData[part].targetPitchHitScore;
    float cap = mScoreCaches[part].unk8;
    float total = unk0 + assignedPoints;
    mResultsData[part].targetPitchHitScore = std::min(total, cap);
    float vibPts = mScoreCaches[part].unk10;
    mPossibleVibratoPoints.Set(vibPts);
    for (AmbiguousData *iter = &mAmbiguousData[0];
         iter != &mAmbiguousData[0] + mAmbiguousData.size(); iter++) {
        if ((iter->part1 != part && iter->part2 != part) || iter->isResolved)
            continue;
        if (iter->winningPart == part) {
            MILO_ASSERT(iter->ambiguousPoints >= 0.0f, 0x460);
            iter->ambiguousPoints += assignedPoints;
        } else if (iter->winningPart != -1) {
            iter->isResolved = true;
        } else {
            iter->ambiguousPoints = assignedPoints;
            iter->winningPart = part;
        }
    }
}