#include "game/VocalPart.h"
#include "game/GameConfig.h"
#include "game/SongDB.h"
#include "game/VocalPlayer.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include "synth/VoiceBeat.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

VocalPart::VocalPart(VocalPlayer *vp, int idx)
    : mPlayer(vp), mPartIndex(idx), mVocalNoteList(0), unk18(0), unk1c(0), unk20(0),
      mRemotePhraseMeterFrac(0), mPhraseScorePartMultiplier(1.0f), mPhraseScoreMax(0),
      unk3c(0), mPhraseScore(0), unk44(0), unk48(0), unk4c(0), unk50(0), unk54(0),
      unk58(0), unk84(0), unk88(-1), mSpotlightPhraseID(-1), unk98(0), unk9c(FLT_MAX),
      unka0(-FLT_MAX), unka4(0), unka8(0), mInFreestyleSection(0), unkad(0), unkb0(0),
      unkb4(0), mFirstPhraseMsToScore(0), unkbc(-1.0f), mBestSinger(0),
      mBestSingerPitchDistance(FLT_MAX), unkc8(6), mScoringEnabled(1), mPhraseRank(0) {
    SetDifficultyVariables(mPlayer->GetUser()->GetDifficulty());
}

VocalPart::~VocalPart() {}

void VocalPart::SetDifficultyVariables(int diff) {
    DataArray *voxCfg = SystemConfig("scoring", "vocals");
    mSlop = voxCfg->FindArray("slop")->Float(diff + 1);
    mPitchMaximumDistance = voxCfg->FindArray("pitch_margin")->Float(diff + 1);
    // retail's embedded double constant is 0x3FB99999A0000000 = (double)0.1f
    // (0.10000000149011612), not the double literal 0.1 (0x3FB999999999999A).
    // Using 0.1f directly would select std::log's float overload (different
    // codegen/call target), so use the exact double value instead.
    float log = std::log(0.10000000149011612);
    mPitchSigma = -(mPitchMaximumDistance * mPitchMaximumDistance) / log;
    mPhraseValue = voxCfg->FindArray("phrase_value")->Int(diff + 1);
    mNoteLengthFactor = voxCfg->FindArray("note_length_factor")->Float(diff + 1);
    mPitchHitMultiplier = voxCfg->FindArray("pitch_hit_multiplier")->Float(diff + 1);
    mNonPitchHitMultiplier =
        voxCfg->FindArray("nonpitch_hit_multiplier")->Float(diff + 1);
    mNonPitchEasyMultiplier = voxCfg->FindArray("nonpitch_easy_multiplier")->Float(1);
    mPhraseScoreCapGrowth = voxCfg->FindArray("vocal_cap_growth")->Float(diff + 1);
    mShortNoteThresh = voxCfg->FindFloat("short_note_threshold_ms");
    mShortNoteMult = voxCfg->FindArray("short_note_multiplier")->Float(diff + 1);
    mTalkyEnergyThreshold = voxCfg->FindFloat("nonpitch_energy_threshold");
}

void VocalPart::PostLoad() {
    mVocalNoteList = TheSongDB->GetVocalNoteList(mPartIndex);
    mFreestyleSection = mVocalNoteList->mFreestyleSections.begin();
    mVocalNoteList->CapLastFreestyleSection(TheSongDB->GetSongDurationMs());
    CalcNoteWeights();
}

void VocalPart::Start() {}
void VocalPart::StartIntro() {}

void VocalPart::UpdateSongMinMaxPitch() {
    unk9c = FLT_MAX;
    unka0 = -FLT_MAX;
    if (mVocalNoteList) {
        std::vector<VocalPhrase> &phrases = mVocalNoteList->mPhrases;
        FOREACH (it, phrases) {
            if (it->unk10 != it->unk14) {
                unk9c = Min(unk9c, it->unk24);
                unka0 = Max(unka0, it->unk28);
            }
        }
    }
}

void VocalPart::Restart(bool b1) {
    mSpotlightPhraseID = -1;
    if (!b1) {
        unkbc = -1.0f;
        unk58 = 0;
        unk3c = 0;
        unk54 = 0;
        mPhraseScore = 0;
        unk44 = 0;
        unk48 = 0;
        unk18 = 0;
        unk20 = 0;
        unk4c = 0;
        unk50 = 0;
        mInFreestyleSection = 0;
        unkad = 0;
        unkb4 = 0;
        mRemotePhraseMeterFrac = 0;
        mFirstPhraseMsToScore = 0;
        CalcNoteWeights();
        if (mVocalNoteList) {
            mThisPhrase = mVocalNoteList->mPhrases.begin();
            mPhraseScoreMax = 0;
            UpdateMinMaxPitch(mThisPhrase);
            UpdateSongMinMaxPitch();
            mFreestyleSection = mVocalNoteList->mFreestyleSections.begin();
        }
    }
}

void VocalPart::SetPaused(bool) {}

void VocalPart::Jump(float f1, bool) {
    unk58 = 0;
    unk3c = 0;
    unk54 = f1;
    mPhraseScore = 0;
    unk44 = 0;
    unk48 = 0;
    unk18 = 0;
    unk20 = 0;
    unk4c = 0;
    unk50 = 0;
    mInFreestyleSection = 0;
    unkad = 0;
    unkb4 = 0;
    mRemotePhraseMeterFrac = 0;
    mFirstPhraseMsToScore = 0;
    if (mVocalNoteList) {
        mThisPhrase = mVocalNoteList->mPhrases.begin();
        while (mThisPhrase != mVocalNoteList->mPhrases.end()
               && mThisPhrase->unk0 + mThisPhrase->unk4 < f1) {
            mThisPhrase++;
        }
        mFreestyleSection = mVocalNoteList->mFreestyleSections.begin();
        while (mFreestyleSection != mVocalNoteList->mFreestyleSections.end()
               && f1 > mFreestyleSection->second) {
            mFreestyleSection++;
        }
        mSpotlightPhraseID = -1;
        UpdateMinMaxPitch(mThisPhrase);
    }
}

void VocalPart::Rollback(float, float ms) {
    unk58 = 0;
    VocalNoteList * &_ref0 = mVocalNoteList;
    unk54 = ms;
    if (_ref0 != nullptr) {
        mThisPhrase = _ref0->mPhrases.begin();
        while (mThisPhrase != _ref0->mPhrases.end()
               && mThisPhrase->unk0 + mThisPhrase->unk4 < ms) {
            mThisPhrase++;
        }
        mFreestyleSection = _ref0->mFreestyleSections.begin();
        while (mFreestyleSection != _ref0->mFreestyleSections.end()
               && ms > mFreestyleSection->second) {
            mFreestyleSection++;
        }
        mSpotlightPhraseID = -1;
        UpdateMinMaxPitch(mThisPhrase);
    }
}

void VocalPart::LocalDeployBandEnergy() {
    if (mInFreestyleSection)
        unkad = true;
}

void VocalPart::CalcNoteWeights() {
    mNoteWeights.clear();
    if (mVocalNoteList) {
        mNoteWeights.reserve(mVocalNoteList->mNotes.size());
        for (unsigned int i = 0; i != mVocalNoteList->mNotes.size(); i++) {
            const VocalNote &note = mVocalNoteList->mNotes[i];
            float weight =
                GetNoteSliceWeight(note.mMs, note.mMs + note.mDurationMs, i);
            mNoteWeights.push_back(weight);
        }
        mThisPhrase = mVocalNoteList->mPhrases.begin();
        mPhraseScoreMax = 0;
        unk1c = 0;
        for (std::vector<VocalPhrase>::const_iterator it =
                 mVocalNoteList->mPhrases.begin();
             it != mVocalNoteList->mPhrases.end();
             ++it) {
            if (it->unk10 != it->unk14) {
                unk1c++;
            }
        }
    }
}

void VocalPart::EnableScoring(bool b) { mScoringEnabled = b; }
bool VocalPart::ScoringEnabled() const { return mScoringEnabled; }

void VocalPart::ResetScoring() {
    if (!IsEmptyPhrase(mThisPhrase)) {
        mPhraseScoreMax = CalcPhraseScoreMax(mThisPhrase);
    } else
        mPhraseScoreMax = 0;
}

bool VocalPart::CouldScoreAgainstPart(
    float ms, TalkyMatcher *i_pTalkyMatcher, float pitch, float maxPitchDist, float &outPitch
) {
    int beginNote = -1;
    int endNote = -1;
    GetNoteRange(ms, beginNote, endNote);
    for (int noteIdx = beginNote; noteIdx < endNote; noteIdx++) {
        const VocalNote &note = mVocalNoteList->mNotes[noteIdx];
        if (note.mUnpitchedNote) {
            MILO_ASSERT(i_pTalkyMatcher, 0x1F2);
            bool unk1 = i_pTalkyMatcher->mVoiceBeat.unk1;
            bool unk0 = i_pTalkyMatcher->mVoiceBeat.unk0;
            bool overEnergy = i_pTalkyMatcher->mVoiceBeat.unk4 > mTalkyEnergyThreshold;
            if (mPlayer->IsAutoplay() || (unk1 && !unk0 && overEnergy)) {
                return true;
            }
        } else if (pitch != 0.0f) {
            float localPitch;
            float sloppyPitch = GetSloppyPitch(ms, noteIdx, pitch, localPitch);
            float absDiff = fabs(sloppyPitch - pitch);
            float diff = (float)fmod(absDiff, 12.0);
            float wrapped = 12.0f - diff;
            diff = Min(wrapped, diff);
            if (diff < maxPitchDist) {
                outPitch = sloppyPitch;
                return true;
            }
        }
    }
    outPitch = 0.0f;
    return false;
}

bool PitchBetween(float pitch, float a, float b, float &out);

float VocalPart::GetSloppyPitch(float ms, int noteIdx, float pitch, float &outPitch)
    const {
    const VocalNote &note = mVocalNoteList->mNotes[noteIdx];
    float pitchHi;
    float msPlus = ms + mSlop;
    if (note.mEndPitch == note.mBeginPitch) {
        pitchHi = (float)note.mBeginPitch;
    } else {
        float dur = note.mDurationMs;
        float noteMs = note.mMs;
        float endMs = noteMs + dur;
        msPlus = Min(endMs, msPlus);
        float rel = Max(msPlus - noteMs, 0.0f);
        float t = rel / dur;
        pitchHi = t * (float)note.mEndPitch + (1.0f - t) * (float)note.mBeginPitch;
    }
    float msMinus = ms - mSlop;
    float pitchLo;
    if (note.mEndPitch == note.mBeginPitch) {
        pitchLo = (float)note.mBeginPitch;
    } else {
        float dur = note.mDurationMs;
        float noteMs = note.mMs;
        float endMs = noteMs + dur;
        msMinus = Min(endMs, msMinus);
        float rel = Max(msMinus - noteMs, 0.0f);
        float t = rel / dur;
        pitchLo = t * (float)note.mEndPitch + (1.0f - t) * (float)note.mBeginPitch;
    }
    float modPitch = (float)fmod(pitch, 12.0);
    float modHi = (float)fmod(pitchHi, 12.0);
    float modLo = (float)fmod(pitchLo, 12.0);
    float between = -1.0f;
    if (!PitchBetween(pitch, pitchHi, pitchLo, between)) {
        float diffHi = fabsf(modPitch - modHi);
        float diffLo = fabs(modPitch - modLo);
        if (diffHi < diffLo) {
            float spEnd = note.mMs + note.mDurationMs;
            float spHi = ms + mSlop;
            const float *p = (spHi <= spEnd) ? &spHi : &spEnd;
            outPitch = *p;
            return pitchHi;
        }
        if (diffHi > diffLo) {
            float spMs = note.mMs;
            float spLo = ms - mSlop;
            const float *p = (spLo <= spMs) ? &spMs : &spLo;
            outPitch = *p;
            return pitchLo;
        }
        float noteMs = note.mMs;
        bool inRange = false;
        if (ms >= noteMs && ms < noteMs + note.mDurationMs)
            inRange = true;
        if (inRange) {
            outPitch = ms;
        } else {
            float endMs = noteMs + note.mDurationMs;
            if (endMs > noteMs) {
                if (endMs < ms)
                    noteMs = ms;
                else
                    noteMs = endMs;
            }
            outPitch = noteMs;
        }
        return pitchHi;
    }
    outPitch = pitch;
    return between;
}

void VocalPart::AddScore(const VocalScoreCache &c) { AddPhrasePoints(c.unk4); }
void VocalPart::ForcePhrasePointDelta(float f1) { mPhraseScore += f1; }

void VocalPart::AddPhrasePoints(float pts) {
    float oldScore = mPhraseScore;
    float newScore = oldScore + pts;
    float cap = mPhraseScoreMax;
    cap = Min(unk38, cap);
        cap = Min(cap, newScore);
    mPhraseScore = cap;
    float delta = mPhraseScore - oldScore;
    int i1, i2, i3;
    mPlayer->GetMultiplier(true, i1, i2, i3);
    unk44 += delta * (float)(i2 - 1);
    unk48 += delta * (float)(i3 - 1);
}

void VocalPart::SetPhraseScoreMultiplier(float f1) { mPhraseScorePartMultiplier = f1; }
void VocalPart::SetPhraseRank(int i) { mPhraseRank = i; }

void VocalPart::SetRemotePhraseMeterFrac(float f1) { mRemotePhraseMeterFrac = f1; }
void VocalPart::OnGameOver() {}

int VocalPart::GetSpotlightPhrase() const { return mSpotlightPhraseID; }

const VocalPhrase *VocalPart::GetFirstPhraseMarker() const {
    return mVocalNoteList->mPhrases.data();
}

const VocalPhrase *VocalPart::GetNextPhraseMarker(const VocalPhrase *const &p) const {
    const VocalPhrase *curPhrase = p;
    if (curPhrase != mVocalNoteList->mPhrases.end())
        curPhrase++;
    return curPhrase;
}

bool VocalPart::IsPhraseMarkerAtEnd(const VocalPhrase *const &p) const {
    const VocalPhrase *end = mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    return p == end;
}

bool VocalPart::IsEmptyPhrase(const VocalPhrase *const &p) const {
    const VocalPhrase *phrase = p;
    const VocalPhrase *end = mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    if (phrase == end) return true;
    if (phrase->mTambourinePhrase) return false;
    if (phrase->unk10 != phrase->unk14) return false;
    int idx = phrase->unk10 - 1;
    if (idx >= 0) {
        const VocalNote &note = mVocalNoteList->mNotes[idx];
        if (note.mMs + note.mDurationMs > phrase->unk0) return false;
    }
    return true;
}

bool VocalPart::AtPhraseEnd(float ms) const {
    const VocalPhrase *end =
        mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    if (mThisPhrase != end && ms > mThisPhrase->unk0 + mThisPhrase->unk4)
        return true;
    return false;
}

bool VocalPart::InEmptyPhrase() const {
    return IsEmptyPhrase(mThisPhrase);
}

bool VocalPart::PhraseHasUnpitchedNotes() const {
    const VocalPhrase *end = mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size();
    if (mThisPhrase == end) return false;
    return mThisPhrase->unk19;
}

bool VocalPart::InPlayablePhrase() const { return true; }

bool VocalPart::InTambourinePhrase() const {
    bool result = false;
    VocalNoteList *list = mVocalNoteList;
    const VocalPhrase *phrase = mThisPhrase;
    if (phrase != list->mPhrases.data() + list->mPhrases.size() && phrase->mTambourinePhrase)
        result = true;
    return result;
}

float VocalPart::FramePhraseMeterFrac() const {
    bool _cond = !mPlayer->IsNet();
    if (_cond) {
        float ratio = 0.0f;
        if (mPhraseScoreMax != 0.0f)
            ratio = mPhraseScore / mPhraseScoreMax;
        if (ratio > 1.0f) return 1.0f;
        if (ratio < 0.0f) return 0.0f;
        return ratio;
    }
    return mRemotePhraseMeterFrac;
}

void VocalPart::UpdateMinMaxPitch(const VocalPhrase *const &phraseRef) {
    VocalNoteList *list = mVocalNoteList;
    const VocalPhrase *cur = phraseRef;
    const VocalPhrase *end = list->mPhrases.data() + list->mPhrases.size();
    if (cur == end) {
        unka8 = 0.0f;
        unka4 = 0.0f;
        return;
    }
    bool foundPitchedNote = false;
    unka4 = FLT_MAX;
    unka8 = -FLT_MAX;
    while (cur != end) {
        int lastNote = cur->unk14;
        int noteIdx = cur->unk10;
        if (noteIdx != lastNote) {
            int noteCount = lastNote - noteIdx;
            for (int i = 0; i < noteCount; ++i) {
                if (!list->mNotes[noteIdx].mUnpitchedNote) {
                    foundPitchedNote = true;
                    unka4 = (cur->unk24 < unka4) ? cur->unk24 : unka4;
                    unka8 = (unka8 < cur->unk28) ? cur->unk28 : unka8;
                    break;
                }
                ++noteIdx;
            }
        }
        if (cur->unk1a)
            break;
        cur++;
    }
    if (!foundPitchedNote) {
        unka4 = 50.0f;
        unka8 = 67.0f;
        return;
    }
    if (unka4 == unka8) {
        unka4 -= 5.0f;
        unka8 += 5.0f;
    }
}

int VocalPart::CalculateRemainingTambourineTicks() {
    MILO_ASSERT(mThisPhrase->mTambourinePhrase, 0x614);
    int dur = mThisPhrase->unkc;
    const VocalPhrase *sp8 = GetNextPhraseMarker(mThisPhrase);
    while (sp8 != mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size()
           && sp8->mTambourinePhrase) {
        dur += sp8->unkc;
        sp8 = GetNextPhraseMarker(sp8);
    }
    return dur;
}

void VocalPart::SetFirstPhraseMsToScore(float f1) { mFirstPhraseMsToScore = f1; }

void VocalPart::AddSingerCandidate(Singer *singer, float dist) {
    if (mBestSinger) {
        if (!(dist > mBestSingerPitchDistance)) return;
    }
    mBestSinger = singer;
    mBestSingerPitchDistance = dist;
}

void VocalPart::ClearSingerCandidates() {
    mBestSinger = nullptr;
    mBestSingerPitchDistance = FLT_MAX;
}

Singer *VocalPart::GetBestSingerCandidate() { return mBestSinger; }

bool VocalPart::HasBestSingerCandidate() { return mBestSinger != nullptr; }

int VocalPart::CurrentPhraseIndex() const {
    return mThisPhrase - mVocalNoteList->mPhrases.data();
}

void VocalPart::SetVocalNoteList(VocalNoteList *list) {
    MILO_ASSERT(list, 0x771);
    mVocalNoteList = list;
    CalcNoteWeights();
    ResetScoring();
}

int VocalPart::NumPracticePhrases(const std::vector<VocalPhrase> &phrases) const {
    if (!mVocalNoteList) return 0;
    return mVocalNoteList->GetNumPracticePhrases(phrases);
}

float VocalPart::GetOverallPartHitPercentage() const {
    if (unk50 == 0) return 0.0f;
    float fPercentage = unk4c / (float)unk50;
    MILO_ASSERT_RANGE_EQ(fPercentage, 0.0f, 1.0f, 0x6d6);
    return fPercentage;
}

float VocalPart::GetPartHitPercentage(const std::vector<VocalPhrase> &phrases, int, int) const {
    if (unk50 == 0) return 0.0f;
    int numPhrases = NumPracticePhrases(phrases);
    float fPercentage = unk4c / (float)numPhrases;
    MILO_ASSERT_RANGE_EQ(fPercentage, 0.0f, 1.0f, 0x6e4);
    return fPercentage;
}

float VocalPart::GetFreestyleSectionDurationMs() const {
    MILO_ASSERT(mInFreestyleSection, 0x6ab);
    VocalNoteList *list = mVocalNoteList;
    const std::pair<float, float> *end =
        list->mFreestyleSections.data() + list->mFreestyleSections.size();
    if (mFreestyleSection == end)
        return 0.0f;
    return mFreestyleSection->second - mFreestyleSection->first;
}

bool VocalNoteEndCmp(float f, const VocalNote &note) {
    return f < note.mMs + note.mDurationMs;
}

void VocalPart::AfterPoll(float ms) {
    int beginNote;
    int endNote;
    GetNoteRange(ms, beginNote, endNote);
    unk58 = beginNote & ~(beginNote >> 31);
    unk54 = ms;
}

bool PitchBetween(float pitch, float a, float b, float &out) {
    float lo = (b < a) ? b : a;
    float hi = (a < b) ? b : a;
    while (pitch > hi)
        pitch -= 12.0f;
    while (pitch < lo)
        pitch += 12.0f;
    out = pitch;
    if (pitch >= lo && pitch <= hi)
        return true;
    return false;
}

static const float kFrameTimeMs = 16.666668f;

float VocalPart::GetNoteSliceWeight(float fBegin, float fEnd, int noteIdx) const {
    if (fEnd < fBegin) {
        float tmp = fBegin;
        fBegin = fEnd;
        fEnd = tmp;
    }
    const VocalNote &note = mVocalNoteList->mNotes[noteIdx];
    float noteMs = note.mMs;
    float noteDurationMs = note.mDurationMs;
    float fEndRel = fEnd - noteMs;
    float fBeginRel = fBegin - noteMs;
    float fDurationCap = 150.0f;
    if (fEndRel < fDurationCap)
        fDurationCap = noteDurationMs;
    float accum = 0.0f;
    if (note.mBeginPitch == note.mEndPitch) {
        // Loop 1: no pitch bend (unpitched or single pitch)
        float threshold = 0.0f;
        float frameMs = kFrameTimeMs;
        while (fBeginRel < fEndRel) {
            float spC = fEndRel - fBeginRel;
            float stepMs = std::min(spC, frameMs);
            float weight;
            if (fBeginRel < threshold) {
                weight = threshold;
            } else if (fBeginRel < fDurationCap) {
                weight = (float)pow((double)(fBeginRel / fDurationCap), 0.5);
            } else {
                weight = 1.0f;
            }
            accum = weight * stepMs + accum;
            fBeginRel += stepMs;
        }
    } else {
        // Loop 2: pitch bend
        float f22 = 1.0f - (float)pow((double)(4.0f / 7.0f), 2.0);
        float zeroThresh = accum;
        float half = 0.5f;
        float two = 2.0f;
        float seventeenFourths = 1.75f;
        while (fBeginRel < fEndRel) {
            float sp8 = fEndRel - fBeginRel;
            float stepMs = std::min(kFrameTimeMs, sp8);
            float weight;
            if (fBeginRel < zeroThresh) {
                weight = 1.0f;
            } else if (fBeginRel > noteDurationMs) {
                weight = 1.0f;
            } else {
                float t = fBeginRel / noteDurationMs;
                float x = (two * (t - half)) / seventeenFourths;
                weight = f22 + (float)pow((double)x, 2.0);
            }
            accum = weight * stepMs + accum;
            fBeginRel += stepMs;
        }
    }
    return accum;
}

float VocalPart::CalcPhraseScoreMax(const VocalPhrase *const &phrase) const {
    const VocalPhrase *p = phrase;
    VocalNoteList *list = mVocalNoteList;
    int start = p->unk10;
    if (start > 0) {
        const VocalNote &prev = list->mNotes[start - 1];
        if (prev.mMs + prev.mDurationMs > p->unk0) {
            start--;
        }
    }
    unsigned int end = p->unk14;
    float result = 0.0f;
    if ((unsigned int)start == end) return result;
    float phraseStart = p->unk0;
    float phraseEnd = p->unk0 + p->unk4;
    for (unsigned int i = start; i != end; i++) {
        const VocalNote &note = list->mNotes[i];
        float noteMs = note.mMs;
        float noteDurationMs = note.mDurationMs;
        float clampedStart = (noteMs < phraseStart) ? phraseStart : noteMs;
        float noteEnd = noteMs + noteDurationMs;
        float clampedEnd = (phraseEnd < noteEnd) ? phraseEnd : noteEnd;
        float duration = clampedEnd - clampedStart;
        float weight = mNoteWeights[i];
        result += (duration / noteDurationMs) * weight;
    }
    return result;
}

extern "C" float kInvalidPitch__11VocalPlayer;
extern "C" VocalNote *NoteAt__13VocalNoteListCFf(const VocalNoteList *, float);
extern "C" float PitchAt__13VocalNoteListCFf(const VocalNoteList *, float);

void VocalPart::Poll(float ms, const SongPos &) {
    while (mFreestyleSection
               != mVocalNoteList->mFreestyleSections.data()
                   + mVocalNoteList->mFreestyleSections.size()
           && ms > mFreestyleSection->second) {
        mFreestyleSection++;
    }
    if ((mPlayer->CanDeployOverdrive() || mPlayer->mIsInCoda
         || mPlayer->IsDeployingBandEnergy())
        && (mThisPhrase
                == mVocalNoteList->mPhrases.data() + mVocalNoteList->mPhrases.size()
            || (mFreestyleSection
                    != mVocalNoteList->mFreestyleSections.data()
                        + mVocalNoteList->mFreestyleSections.size()
                && ms >= mFreestyleSection->first
                && ms < mFreestyleSection->second))) {
        mInFreestyleSection = true;
    } else {
        mInFreestyleSection = false;
        unkad = false;
    }
    mPlayer->IsNet();
    if (mPlayer->mIsInCoda && ms > unkb0) {
        unkb4 = true;
    }
    if (mInFreestyleSection) {
        unk98 = 3;
    } else if (mPlayer->InTambourinePhrase()) {
        unk98 = 2;
    } else {
        unk98 = 0;
    }
    int beginNote = -1;
    int endNote = -1;
    GetNoteRange(ms, beginNote, endNote);
    while (endNote > unk3c && unk3c < mThisPhrase->unk14) {
        unk38 += mPhraseScoreCapGrowth * mNoteWeights[unk3c];
        unk3c++;
    }
    int noteCount = mVocalNoteList->mNotes.size();
    int *pEnd = (noteCount < endNote) ? &noteCount : &endNote;
    int lastNote = *pEnd;
    endNote = lastNote;
    bool allUnpitched = true;
    for (int i = beginNote; i < lastNote; i++) {
        if (!mVocalNoteList->mNotes[i].mUnpitchedNote) {
            allUnpitched = false;
            break;
        }
    }
    if (allUnpitched && beginNote != lastNote) {
        unk98 = 1;
    }
#ifdef HX_NATIVE
    VocalFrameSpewData *spew = mPlayer->mFrameSpewData;
    if (spew) {
        float pitch = PitchAt__13VocalNoteListCFf(mVocalNoteList, ms);
        spew->mPartData[mPartIndex].unk0 = pitch;
    }
#endif
}

void VocalPart::HandlePhraseEnd(
    int &o_rRating, float &o_rStartMs, float &o_rEndMs, int &o_rPrevScore, float ms
) {
    if (mVocalNoteList) {
        mPlayer->IsNet();
        const VocalPhrase *phrase = GetNextPhraseMarker(mThisPhrase);
        float startMs;
        float endMs;
        if (phrase != mVocalNoteList->mPhrases.end()) {
            startMs = phrase->unk0 + phrase->unk4;
            const VocalPhrase *nextNext = GetNextPhraseMarker(phrase);
            if (nextNext != mVocalNoteList->mPhrases.end()) {
                endMs = nextNext->unk0 + nextNext->unk4;
            } else {
                endMs = TheSongDB->GetSongDurationMs();
            }
        } else {
            startMs = TheSongDB->GetSongDurationMs();
            endMs = startMs;
        }
        o_rRating = -1;
        unk20 = 0.0f;
        if (mPlayer->ScoringEnabled()
            && mThisPhrase != mVocalNoteList->mPhrases.end()
            && mPlayer->GetEnabledState() == kPlayerEnabled
            && mThisPhrase->unk0 >= mFirstPhraseMsToScore) {
            float scoreMax = mPhraseScoreMax;
            if (scoreMax != 0.0f) {
                int rating = mPlayer->CalculatePhraseRating(mPhraseScore / scoreMax);
                o_rRating = rating;
                unk18 += rating;
                float denom = mPhraseScoreMax;
                int accPts =
                    (int)(0.5
                          + (double)(mPhraseScore * mPhraseScorePartMultiplier
                                     * (float)mPhraseValue * (1.0f / denom)));
                int bandPts =
                    (int)(0.5
                          + (double)(unk44 * mPhraseScorePartMultiplier
                                     * (float)mPhraseValue * (1.0f / denom)));
                int odPts = (int)(0.5
                                  + (double)(unk48 * mPhraseScorePartMultiplier
                                             * (float)mPhraseValue * (1.0f / denom)));
                int total = odPts + (bandPts + accPts);
                if (total > 0) {
                    int m1, m2, m3;
                    mPlayer->GetMultiplier(true, m1, m2, m3);
                    int indMult = mPlayer->GetIndividualMultiplier();
                    // NOTE: retail emits `mullw r10, r29, r3` (total, indMult);
                    // MSVC canonicalises this call-result multiply to
                    // `mullw r10, r3, r29` regardless of source operand order
                    // (both orders + int/float temps tried). Sole residual.
                    unk20 = total * indMult;
                    if (mPhraseRank == 0) {
                        mPlayer->AddAccuracyStat(accPts);
                    } else {
                        mPlayer->AddHarmonyStat(accPts);
                    }
                    mPlayer->AddScoreStreakStat((float)(accPts * (indMult - 1)));
                    mPlayer->AddOverdriveStat((float)(odPts * indMult));
                    mPlayer->AddBandContributionStat((float)(bandPts * indMult));
                    mPlayer->AddPoints(unk20, false, false);
                }
            }
        }
        VocalNoteList *list = mVocalNoteList;
#ifdef HX_NATIVE
        // Headless: no BandUser (GetUserGuid) and no TheGameConfig track table.
        // The spotlight-phrase id feeds overdrive/unison display only; the phrase
        // rating/score above is unaffected.
        (void)list;
        mSpotlightPhraseID = -1;
#else
        if (phrase != list->mPhrases.end() && phrase->unk10 != phrase->unk14) {
            mSpotlightPhraseID = TheSongDB->GetCommonPhraseID(
                TheGameConfig->GetTrackNum(mPlayer->GetUserGuid()),
                list->mNotes[phrase->unk10].mTick
            );
        } else {
            mSpotlightPhraseID = -1;
        }
#endif
        if (mThisPhrase != mVocalNoteList->mPhrases.end() && mThisPhrase->unk1a) {
            UpdateMinMaxPitch(phrase);
        }
        if (mPlayer->ScoringEnabled() && mPhraseScoreMax > 0.0f) {
            float frac = FramePhraseMeterFrac();
            unk4c += frac;
            unk50 += 1;
        }
        int prevScore = (int)mPhraseScore;
        if (mPlayer->ScoringEnabled()) {
            mPhraseScore = 0.0f;
            unk44 = 0.0f;
            unk48 = 0.0f;
            unk38 = 0.0f;
        }
        if (phrase != mVocalNoteList->mPhrases.end()) {
            if (phrase->unk10 > 0) {
                const VocalNote &prev = mVocalNoteList->mNotes[phrase->unk10 - 1];
                if (prev.mMs + prev.mDurationMs > phrase->unk0) {
                    unk3c -= 1;
                }
            }
            mPhraseScoreMax = CalcPhraseScoreMax(phrase);
        } else {
            mPhraseScoreMax = 0.0f;
        }
        mThisPhrase = phrase;
        o_rStartMs = startMs;
        o_rEndMs = endMs;
        o_rPrevScore = prevScore;
    }
}

void VocalPart::ScoreSinger(
    float ms, float arg1, float arg2, float arg3, int arg4,
    TalkyMatcher *i_pTalkyMatcher, VocalScoreCache &o_rCache, int &o_rNote,
    float &o_rPitchDiff
) {
    MILO_ASSERT(o_rCache.GetHitPercentage() == 0.0f, 0x2C3);
    o_rCache.unk8 = Min(unk38, mPhraseScoreMax);
    o_rPitchDiff = kInvalidPitch__11VocalPlayer;
    if (arg1 == 0.0f && NoteAt__13VocalNoteListCFf(mVocalNoteList, ms) == 0) {
        o_rCache.unk0 = 1.0f;
        o_rNote = arg4;
        return;
    }
    int beginNote = -1;
    int endNote = -1;
    int noteMatched;
    float bestPitch = 0.0f;
    float pitch = arg1;
    int octaves = arg4;
    float sloppyArg;
    bool talkyHit;
    GetNoteRange(ms, beginNote, endNote);
    float score = GetBestHit(
        ms, beginNote, endNote, i_pTalkyMatcher, pitch, arg3, octaves, noteMatched,
        bestPitch, sloppyArg, talkyHit
    );
    o_rNote = octaves;
    if (noteMatched != -1) {
        float diff = (float)fmod(arg1 - bestPitch, 12.0);
        o_rPitchDiff = diff;
        if (diff > 6.0f) {
            o_rPitchDiff = diff - 12.0f;
        } else if (diff < -6.0f) {
            o_rPitchDiff = diff + 12.0f;
        }
        if (mVocalNoteList->mNotes[noteMatched].mUnpitchedNote) {
            unk98 = 1;
        } else {
            unk98 = 0;
        }
    }
    o_rCache.unk0 = score;
    o_rCache.unk14 = bestPitch;
    o_rCache.unk18 = sloppyArg;
    o_rCache.unk1c = octaves;
    o_rCache.unk20 = talkyHit;
    CalculateScore(ms, noteMatched, score, o_rCache);
}

float VocalPart::GetBestHit(
    float ms, int beginNote, int endNote, TalkyMatcher *i_pTalkyMatcher,
    float &io_rPitch, float arg5, int &o_rOctaves, int &noteMatched,
    float &o_rArg8, float &o_rArg9, bool &o_rTalkyHit
) {
    noteMatched = -1;
    float bestScore = 0.0f;
    float savedPitch = io_rPitch;
    bool foundTalky = false;
    o_rTalkyHit = false;
    for (int i = beginNote; i < endNote; i++) {
        const VocalNote &note = mVocalNoteList->mNotes[i];
        if (note.mUnpitchedNote) {
            MILO_ASSERT(i_pTalkyMatcher, 0x46D);
            bool vb1 = i_pTalkyMatcher->mVoiceBeat.unk1;
            bool vb0 = i_pTalkyMatcher->mVoiceBeat.unk0;
            bool overEnergy =
                i_pTalkyMatcher->mVoiceBeat.unk4 > mTalkyEnergyThreshold;
            float score = 1.0f;
            if (mPlayer->IsAutoplay() || (vb1 && !vb0 && overEnergy)) {
                if (foundTalky) {
                    MILO_ASSERT(noteMatched != -1, 0x486);
                    const VocalNote &best = mVocalNoteList->mNotes[noteMatched];
                    const VocalNote &cur = mVocalNoteList->mNotes[i];
                    if ((float)fabs((best.mMs + best.mDurationMs) - ms)
                        < (float)fabs(cur.mMs - ms)) {
                        score = 0.0f;
                    }
                }
                if (score >= bestScore) {
                    io_rPitch = savedPitch;
                    bestScore = score;
                    foundTalky = true;
                    noteMatched = i;
                    o_rArg8 = -1.0f;
                    o_rOctaves = 0;
                    o_rArg9 = ms;
                    o_rTalkyHit = true;
                }
            }
        } else if (0.0f != io_rPitch) {
            float pitch = savedPitch;
            float sloppyPitch;
            int octaves = o_rOctaves;
            float spArg5;
            float score = ScoreNote(ms, i, pitch, octaves, sloppyPitch, spArg5);
            if (score >= bestScore || (score > 0.0 && foundTalky)) {
                bestScore = score;
                io_rPitch = pitch;
                foundTalky = false;
                noteMatched = i;
                o_rArg8 = sloppyPitch;
                o_rOctaves = octaves;
                o_rArg9 = spArg5;
                o_rTalkyHit = false;
            }
        }
    }
    return bestScore;
}

float VocalPart::ScoreNote(
    float ms, int noteIdx, float &pitch, int &octavesOut, float &sloppyPitchOut,
    float &arg5
) const {
    sloppyPitchOut = GetSloppyPitch(ms, noteIdx, pitch, arg5);
    float diff = (float)sloppyPitchOut - pitch;
    float absDiff = (float)fabs(diff);
    float pitchClassDist = (float)fmod(absDiff, 12.0);
    pitchClassDist = Min(pitchClassDist, 12.0f - pitchClassDist);
    if (pitchClassDist <= 2.5f) {
        float fMagnitude = 0.5f + absDiff / 12.0f;
        int mag = (int)fMagnitude;
        int sign = (diff > 0.0f) ? 1 : -1;
        int octaves = mag * sign;
        diff = pitchClassDist;
        octavesOut = octaves;
        pitch += 12.0f * (float)octaves;
    }
    float score = 0.0f;
    if ((float)fabs(diff) <= mPitchMaximumDistance) {
        score = (float)exp(-(diff * diff) / mPitchSigma);
        if (score < 0.01f)
            score = 0.0f;
    }
    if (GetNoteSliceWeight(unk54, ms, noteIdx) == 0.0f)
        score = 0.0f;
    return score;
}

void VocalPart::CalculateScore(
    float ms, int noteIdx, float mult, VocalScoreCache &cache
) const {
    if (noteIdx == -1)
        return;
    float sliceWeight = GetNoteSliceWeight(unk54, ms, noteIdx);
    VocalNote &note = mVocalNoteList->mNotes[noteIdx];
    float noteMult;
    if (!note.mUnpitchedNote) {
        noteMult = mPitchHitMultiplier;
    } else if (note.mUnpitchedEasy) {
        noteMult = mNonPitchHitMultiplier * mNonPitchEasyMultiplier;
    } else {
        noteMult = mNonPitchHitMultiplier;
    }
    if (note.mDurationMs < mShortNoteThresh)
        noteMult *= mShortNoteMult;
    float framePoints = noteMult * (mult * sliceWeight);
#ifdef HX_NATIVE
    VocalFrameSpewData *spew = mPlayer->mFrameSpewData;
    if (spew) {
        spew->mPartData[mPartIndex].unk4 = framePoints;
        spew->mPartData[mPartIndex].unk8 = unk38;
        spew->mPartData[mPartIndex].unkc = mult;
        spew->mPartData[mPartIndex].unk10 = sliceWeight;
        spew->mPartData[mPartIndex].unk14 = noteMult;
    }
#endif
    cache.unkc = framePoints;
    if (unk38 < mPhraseScore + framePoints)
        framePoints = unk38 - mPhraseScore;
    cache.unk4 = framePoints;
    float capped =
        Min(Min(unk38, mPhraseScoreMax), sliceWeight * noteMult + mPhraseScore);
    float delta = capped - mPhraseScore;
    if (delta < 0.0f)
        delta = 0.0f;
    cache.unk10 = delta;
}

void VocalPart::GetNoteRange(float ms, int &startOut, int &endOut) {
    float slop = mSlop;
    float lower = ms - slop;
    float upper = ms + slop;
    const VocalNoteList *list = mVocalNoteList;
    startOut = -1;
    endOut = -1;
    const VocalNote *it = std::upper_bound(
        list->mNotes.data(),
        list->mNotes.data() + list->mNotes.size(),
        lower,
        VocalNoteEndCmp
    );
    if (it != list->mNotes.data() + list->mNotes.size()) {
        while (it->mMs < upper && it != list->mNotes.data() + list->mNotes.size()) {
            int idx = it - list->mNotes.data();
            if (startOut == -1)
                startOut = idx;
            endOut = idx + 1;
            ++it;
        }
    }
}

bool VocalPart::NearNote(float ms) {
    int start = -1, end = -1;
    GetNoteRange(ms, start, end);
    return start < end;
}

bool VocalPart::FramePhraseMeterFracSorter(const VocalPart *i_pA, const VocalPart *i_pB) {
    MILO_ASSERT(i_pA, 0x6c8);
    MILO_ASSERT(i_pB, 0x6c9);
    return i_pA->FramePhraseMeterFrac() > i_pB->FramePhraseMeterFrac();
}
