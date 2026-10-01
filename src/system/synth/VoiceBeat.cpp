// VoiceBeat / EventTracker / TalkyMatcher (system/synth/VoiceBeat.cpp).
// Contains VoiceBeat (the talky/spam-syllable DSP), EventTracker (reference-event
// hit/miss bookkeeping), and TalkyMatcher (the per-frame unpitched-note matcher
// Singer drives via ProcessTalkyData). X360-inert: not in objects.json, header
// unchanged, so it cannot perturb retail preprocessed output. The single deviation
// here is the profiling-only START_AUTO_TIMER, gated out under HX_NATIVE
// (a no-op that never touches scoring state) to keep the native link surface tight.
#include "synth/VoiceBeat.h"
#include "math/Utl.h"
#ifndef HX_NATIVE
#include "os/Timer.h"
#endif
#include <algorithm>
#include <string.h>
#include <math.h>

VoiceBeat::VoiceBeat() {
    mEnabled = true;
    Reset();
}

void VoiceBeat::SetEnable(bool enable) {
    if (enable && !mEnabled)
        Reset();
    mEnabled = enable;
}

void VoiceBeat::Analyze(
    float *samples, int numSamples, bool useWindow, bool storeEvents, float ms
) {
#ifndef HX_NATIVE
    START_AUTO_TIMER("voice_beat");
#endif
    if (!mEnabled) return;

    if (ms != -1.0f) {
        float mCountScaled = (float)mCount * 0.0625f;
        mRate = ((double)numSamples
                 + (ms - ((float)(numSamples / 16) + mCountScaled)) / 5.0)
            / (double)numSamples;
    }

    // Retail (TU5 X360) is the plain mkfilter form: explicit delay-line shifts,
    // gains divided (so /fp:fast multiplies by the reciprocal), std::max on the
    // floor. Rebuilt from retail asm.
    double *xv = mXVVoice;
    double *yv = mYVVoice;
    double *xa = mXVEnvAntiAlias;
    double *ya = mYVEnvAntiAlias;
    for (int i = 0; i < numSamples; i++) {
        if (useWindow) {
            *samples *= (float)sin(3.1415927410125732 * ((double)i / (double)numSamples));
        }

        xv[0] = xv[1];
        xv[1] = xv[2];
        xv[2] = xv[3];
        xv[3] = xv[4];
        xv[4] = *samples / 6.349260768;
        yv[0] = yv[1];
        yv[1] = yv[2];
        yv[2] = yv[3];
        yv[3] = yv[4];
        yv[4] = (xv[0] + xv[4]) - 2 * xv[2]
            + (-0.1330748863 * yv[0]) + (0.7561945957 * yv[1])
            + (-2.0287939898 * yv[2]) + (2.4013168963 * yv[3]);

        double absYV = fabs(yv[4]);
        mFullBandEnergy += (fabs(*samples) - mFullBandEnergy) * 0.02;
        mVoiceEnergy += (absYV - mVoiceEnergy) * 0.02;
        double ratio = mVoiceEnergy / mFullBandEnergy;

        xa[0] = xa[1];
        xa[1] = xa[2];
        xa[2] = absYV / 2666.171709;
        ya[0] = ya[1];
        ya[1] = ya[2];
        ya[2] = (xa[0] + xa[2])
            + 2 * xa[1] + (-0.9459779362 * ya[0])
            + (1.9444776578 * ya[1]);

        mCount += mRate;

        if (i % 40 == 0) {
            double env = ya[2];
            double *xs = mXVSyllables;
            double *ys = mYVSyllables;
            double *xp = mXVSpamSyllables;
            double *yp = mYVSpamSyllables;
            xs[0] = xs[1];
            xs[1] = xs[2];
            xs[2] = env / 143.5132541;
            ys[0] = ys[1];
            ys[1] = ys[2];
            ys[2] = (xs[0] + xs[2]) + 2 * xs[1]
                + (-0.7319917025 * ys[0]) + (1.7041197124 * ys[1]);

            xp[0] = xp[1];
            xp[1] = xp[2];
            xp[2] = xp[3];
            xp[3] = xp[4];
            xp[4] = env / 1.178584698;
            yp[0] = yp[1];
            yp[1] = yp[2];
            yp[2] = yp[3];
            yp[3] = yp[4];
            yp[4] = (xp[0] + xp[4])
                - 4 * (xp[1] + xp[3])
                + 6 * xp[2] + (-0.7199103273 * yp[0])
                + (3.1159669252 * yp[1])
                + (-5.0679983867 * yp[2])
                + (3.6717290892 * yp[3]);

            double syl = ys[2];
            mSpamAvg += (fabs(yp[4]) - mSpamAvg) * 0.03;
            double sylDelta = syl - mSylEnvSigma;
            mSylEnvSigma += sylDelta * 0.08;
            unk4 = syl;

            if (syl < mFloorSigma) {
                mFloorSigma = syl;
            } else {
                mFloorSigma += (syl - mFloorSigma) * 0.001;
            }

            ms = (float)mCount * 0.0625f;
            unk0 = mSpamAvg > 0.35;
            unk1 = ratio > 0.3;

            if (sylDelta < 0 && mSylDeltaPrev >= 0
                && syl > 4.0 * std::max(0.15, mFloorSigma) && unk1 && unk0) {
                if (storeEvents) {
                    mPeaks.push_back(syl);
                    mTimes.push_back(ms);
                }
                mTriggered = true;
            }
            mSylDeltaPrev = sylDelta;
        }
        samples++;
    }
}

void VoiceBeat::Reset() {
    memset(mXVVoice, 0, sizeof(mXVVoice));
    memset(mYVVoice, 0, sizeof(mYVVoice));
    memset(mXVEnvAntiAlias, 0, sizeof(mXVEnvAntiAlias));
    memset(mYVEnvAntiAlias, 0, sizeof(mYVEnvAntiAlias));
    memset(mXVSyllables, 0, sizeof(mXVSyllables));
    memset(mYVSyllables, 0, sizeof(mYVSyllables));
    memset(mXVSpamSyllables, 0, sizeof(mXVSpamSyllables));
    memset(mYVSpamSyllables, 0, sizeof(mYVSpamSyllables));
    unk0 = false;
    unk1 = false;
    unk4 = 0;
    mSpamAvg = 0;
    mSylDeltaPrev = 0;
    mSylEnvSigma = 0;
    mFloorSigma = 0;
    mCount = 0;
    mRate = 1;
    mPeaks.clear();
    mTimes.clear();
    mTriggered = false;
}

void VoiceBeat::ClearTrigger() { mTriggered = false; }

void VoiceBeat::ClearEventList() {
    mPeaks.clear();
    mTimes.clear();
}

EventTracker::EventTracker() : mSelFrom(-1), mSelTo(-1), mAvgHitTime(0) {}

void EventTracker::invalidate() {
    mSelFrom = -1;
    mSelTo = -1;
}

int EventTracker::findEarliest(float t, int start) {
    int n = mTimes.size();
    if (n == 0) return -1;
    int last = n - 1;
    MaxEq(start, 0);
    if (start > last) start = last;
    while (start >= 0 && mTimes[start] >= t) {
        start--;
    }
    if (start < 0) return 0;
    while (start < n && mTimes[start] < t) {
        start++;
    }
    return start;
}

int EventTracker::findLatest(float t, int start) {
    int n = mTimes.size();
    if (n == 0) return -1;
    int idx = start;
    if (idx > n) idx = n - 1;
    if (idx < 0) idx = 0;
    while (idx < n && mTimes[idx] < t) {
        idx++;
    }
    if (idx >= n) return n - 1;
    while (idx >= 0 && mTimes[idx] >= t) {
        idx--;
    }
    return idx;
}

void EventTracker::Reset() {
    mMisses.clear();
    mMisses.resize(mTimes.size(), false);
    mHits.clear();
    mHits.resize(mTimes.size(), false);
    mSwings.clear();
    mSwings.resize(mTimes.size(), 0);
    mAvgHitTime = 0;
    invalidate();
}

bool EventTracker::Hit(float msFrom, float msUpTo, float msNow) {
    mSelFrom = findEarliest(msFrom, mSelFrom);
    mSelTo = findLatest(msUpTo, mSelTo);
    float tAccum = 0.0f;
    for (int i = mSelFrom; i <= mSelTo; i++) {
        static float k_zero = 0.0f;
        float diff = 0.2f - mPeaks[i];
        float *p = (k_zero >= diff) ? &k_zero : &diff;
        float tolHalf = 1000.0f * (*p) + 60.0f;
        if (mTimes[i] - tolHalf <= msNow && msNow <= mTimes[i] + tolHalf) {
            tAccum += mTimes[i];
            mHits[i] = true;
        }
    }
    mSelFrom = findEarliest(msNow - 150.0f, mSelFrom);
    mSelTo = findLatest(150.0f + msNow, mSelTo);
    for (int i = mSelFrom; i <= mSelTo; i++) {
        mSwings[i]++;
    }
    if (tAccum != 0.0f) {
        int n = mSelFrom - mSelTo + 1;
        mAvgHitTime = 0.1f * (tAccum / (float)n - (msFrom + msUpTo) * 0.5f - mAvgHitTime)
            + mAvgHitTime;
    }
    return 0.0f != tAccum;
}

bool EventTracker::Miss(float msFrom, float msUpTo) {
    mSelFrom = findEarliest(msFrom, mSelFrom);
    mSelTo = findLatest(msUpTo, mSelTo);
    bool result = false;
    for (int i = mSelFrom; i <= mSelTo; i++) {
        if (!mHits[i]) {
            mMisses[i] = true;
            result = true;
        }
    }
    return result;
}

TalkyMatcher::TalkyMatcher() { memset(mBuffer, 0, sizeof(mBuffer)); }

void TalkyMatcher::updateScoring(float f) {
    if (mVoiceBeat.mTriggered) {
        mRefEvents.Hit(f - 180.0f, f + 180.0f, f);
        mVoiceBeat.ClearEventList();
    }
    std::vector<double> unused;
    mRefEvents.Miss(f - 120.0f, f - 60.0f);
    mVoiceBeat.ClearTrigger();
}

void TalkyMatcher::LoadEvents(
    const std::vector<float> &times, const std::vector<float> &peaks
) {
    mRefEvents.mTimes = times;
    mRefEvents.mPeaks = peaks;
    mRefEvents.Reset();
}

void TalkyMatcher::Reset() { mVoiceBeat.Reset(); }

void TalkyMatcher::Analyze(const short *samples, int numSamples, float ms) {
    if (numSamples > 0x3000) numSamples = 0x3000;
    int n3 = numSamples / 3;
    for (int i = 0; i < n3; i++) {
        mBuffer[i] = (float)samples[i * 3] / 32767.0f;
    }
    mVoiceBeat.Analyze(mBuffer, n3, false, true, ms + 6.0f);
    if (mRefEvents.mTimes.size() != 0) {
        updateScoring(ms);
    }
}

void TalkyMatcher::SetEnableTalkyMatcher(bool enable) { mVoiceBeat.SetEnable(enable); }
