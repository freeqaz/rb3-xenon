////////////////////////////////////////////////////////////////////////////////
///
/// Sampled sound tempo changer/time stretch algorithm. Changes the sound tempo
/// while maintaining the original pitch by using a time domain WSOLA-like
/// method with several performance-increasing tweaks.
///
/// Note : MMX optimized functions reside in a separate, platform-specific
/// file, e.g. 'mmx_win.cpp' or 'mmx_gcc.cpp'
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// Last changed  : $Date$
// File revision : $Revision: 1.12 $
//
// $Id$
//
////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#include "os/Debug.h"
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <stdexcept>

#include "STTypes.h"
#include "cpu_detect.h"
#include "TDStretch.h"

#include <stdio.h>

using namespace soundtouch;

#define max(x, y) (((x) > (y)) ? (x) : (y))

/*****************************************************************************
 *
 * Constant definitions
 *
 *****************************************************************************/

// Table for the hierarchical mixing position seeking algorithm
static const short
    _scanOffsets[5][24] = { { 124,  186,  248,  310,  372,  434,  496,  558,
                              620,  682,  744,  806,  868,  930,  992,  1054,
                              1116, 1178, 1240, 1302, 1364, 1426, 1488, 0 },
                            { -100, -75, -50, -25, 25, 50, 75, 100, 0, 0, 0, 0,
                              0,    0,   0,   0,   0,  0,  0,  0,   0, 0, 0, 0 },
                            { -20, -15, -10, -5, 5, 10, 15, 20, 0, 0, 0, 0,
                              0,   0,   0,   0,  0, 0,  0,  0,  0, 0, 0, 0 },
                            { -4, -3, -2, -1, 1, 2, 3, 4, 0, 0, 0, 0,
                              0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0 },
                            { 121, 114, 97,  114, 98,  105, 108, 32, 104, 99, 117, 111,
                              116, 100, 110, 117, 111, 115, 0,   0,  0,   0,  0,   0 } };

/*****************************************************************************
 *
 * Implementation of the class 'TDStretch'
 *
 *****************************************************************************/

TDStretch::TDStretch() : FIFOProcessor(&outputBuffer) {
    bQuickSeek = false;
    channels = 2;
    pMidBuffer = nullptr;
    pRefMidBufferUnaligned = nullptr;
    overlapLength = 0;
    bAutoSeqSetting = true;
    bAutoSeekSetting = true;
    skipFract = 0;
    tempo = 1.0f;

    setParameters(44100, 82, 14, 12);
    setTempo(1.0f);

    clear();
}

TDStretch::~TDStretch() {
    delete[] pMidBuffer;
    delete[] pRefMidBufferUnaligned;
}

/// Sets routine control parameters. These control are certain time constants
/// defining how the sound is stretched to the desired duration.
///
/// 'sampleRate' = sample rate of the sound
/// 'sequenceMS' = one processing sequence length in milliseconds (default = 82 ms)
/// 'seekwindowMS' = seeking window length for scanning the best overlapping
///      position (default = 28 ms)
/// 'overlapMS' = overlapping length (default = 12 ms)

void TDStretch::setParameters(
    int aSampleRate, int aSequenceMS, int aSeekWindowMS, int aOverlapMS
) {
    // accept only positive parameter values - if zero or negative, use old values instead
    if (aSampleRate > 0)
        this->sampleRate = aSampleRate;
    if (aOverlapMS > 0)
        this->overlapMs = aOverlapMS;

    if (aSequenceMS > 0) {
        this->sequenceMs = aSequenceMS;
        bAutoSeqSetting = false;
    } else if (aSequenceMS == 0) {
        // if zero, use automatic setting
        bAutoSeqSetting = true;
    }

    if (aSeekWindowMS > 0) {
        this->seekWindowMs = aSeekWindowMS;
        bAutoSeekSetting = false;
    } else if (aSeekWindowMS == 0) {
        // if zero, use automatic setting
        bAutoSeekSetting = true;
    }

    calcSeqParameters();

    calculateOverlapLength(overlapMs);

    // set tempo to recalculate 'sampleReq'
    setTempo(tempo);
}

/// Get routine control parameters, see setParameters() function.
/// Any of the parameters to this function can be NULL, in such case corresponding
/// parameter value isn't returned.
void TDStretch::getParameters(
    int *pSampleRate, int *pSequenceMs, int *pSeekWindowMs, int *pOverlapMs
) const {
    if (pSampleRate) {
        *pSampleRate = sampleRate;
    }

    if (pSequenceMs) {
        *pSequenceMs = (bAutoSeqSetting) ? (USE_AUTO_SEQUENCE_LEN) : sequenceMs;
    }

    if (pSeekWindowMs) {
        *pSeekWindowMs = (bAutoSeekSetting) ? (USE_AUTO_SEEKWINDOW_LEN) : seekWindowMs;
    }

    if (pOverlapMs) {
        *pOverlapMs = overlapMs;
    }
}

// Overlaps samples in 'midBuffer' with the samples in 'pInput'
void TDStretch::overlapMono(SAMPLETYPE *pOutput, const SAMPLETYPE *pInput) const {
    for (int i = 0; i < overlapLength; i++) {
        int itemp = overlapLength - i;
        pOutput[i] = (pInput[i] * i + pMidBuffer[i] * itemp) / overlapLength;
    }
}

void TDStretch::clearMidBuffer() {
    memset(pMidBuffer, 0, 2 * sizeof(SAMPLETYPE) * overlapLength);
}

void TDStretch::clearInput() {
    inputBuffer.clear();
    clearMidBuffer();
}

// Clears the sample buffers
void TDStretch::clear() {
    outputBuffer.clear();
    clearInput();
}

// Enables/disables the quick position seeking algorithm. Zero to disable, nonzero
// to enable
void TDStretch::enableQuickSeek(BOOL enable) { bQuickSeek = enable; }

// Returns nonzero if the quick seeking algorithm is enabled.
BOOL TDStretch::isQuickSeekEnabled() const { return bQuickSeek; }

// Seeks for the optimal overlap-mixing position.
int TDStretch::seekBestOverlapPosition(const SAMPLETYPE *refPos) {
    if (channels == 2) {
        // stereo sound
        if (bQuickSeek) {
            return seekBestOverlapPositionStereoQuick(refPos);
        } else {
            return seekBestOverlapPositionStereo(refPos);
        }
    } else {
        // mono sound
        if (bQuickSeek) {
            return seekBestOverlapPositionMonoQuick(refPos);
        } else {
            return seekBestOverlapPositionMono(refPos);
        }
    }
}

/// Overlaps samples in 'midBuffer' with the samples in 'pInputBuffer' at position
/// of 'ovlPos'.
inline void TDStretch::overlap(SAMPLETYPE *pOutput, const SAMPLETYPE *pInput, uint ovlPos) const {
    if (channels == 2) {
        // stereo sound
        overlapStereo(pOutput, pInput + 2 * ovlPos);
    } else {
        // mono sound
        overlapMono(pOutput, pInput + ovlPos);
    }
}

/// Seeks for the optimal overlap-mixing position. The 'stereo' version of the routine.
///
/// The best position is determined as the position where the two overlapped
/// sample sequences are 'most alike', in terms of the highest cross-correlation
/// value over the overlapping period.
int TDStretch::seekBestOverlapPositionStereo(const SAMPLETYPE *refPos) {
    // Slopes the amplitudes of the 'midBuffer' samples
    precalcCorrReferenceStereo();

    double bestCorr = FLT_MIN;
    int bestOffs = 0;

    // Scans for the best correlation value by testing each possible position
    // over the permitted range.
    for (int i = 0; i < seekLength; i++) {
        // Calculates correlation value for the mixing position corresponding to 'i'
        double corr = (double)calcCrossCorrStereo(refPos + 2 * i, pRefMidBuffer);
        // heuristic rule to slightly favour values close to mid of the range
        double tmp = (double)(2 * i - seekLength) / (double)seekLength;
        corr = ((corr + 0.1) * (1.0 - 0.25 * tmp * tmp));

        // Checks for the highest correlation value
        if (corr > bestCorr) {
            bestCorr = corr;
            bestOffs = i;
        }
    }
    // clear cross correlation routine state if necessary (is so e.g. in MMX routines)
    clearCrossCorrState();

    return bestOffs;
}

/// Seeks for the optimal overlap-mixing position. The 'stereo' version of the routine.
///
/// The best position is determined as the position where the two overlapped
/// sample sequences are 'most alike', in terms of the highest cross-correlation
/// value over the overlapping period. Uses quick hierarchical search.
int TDStretch::seekBestOverlapPositionStereoQuick(const SAMPLETYPE *refPos) {
    // Slopes the amplitude of the 'midBuffer' samples
    precalcCorrReferenceStereo();

    double bestCorr = FLT_MIN;
    int bestOffs = _scanOffsets[0][0];
    int corrOffset = 0;

    // Scans for the best correlation value using four-pass hierarchical search.
    // The look-up table '_scanOffsets' has hierarchical position adjusting steps.
    // In first pass the routine searches for the highest correlation with
    // relatively coarse steps, then rescans the neighbourhood of the highest
    // correlation with better resolution and so on.
    for (int scanCount = 0; scanCount < 4; scanCount++) {
        for (int j = 0; _scanOffsets[scanCount][j]; j++) {
            int tempOffset = corrOffset + _scanOffsets[scanCount][j];
            if (tempOffset >= seekLength)
                break;

            // Calculates correlation value for the mixing position corresponding to 'tempOffset'
            double corr = (double)calcCrossCorrStereo(refPos + 2 * tempOffset, pRefMidBuffer);
            // heuristic rule to slightly favour values close to mid of the range
            double tmp = (double)(2 * tempOffset - seekLength) / seekLength;
            corr = ((corr + 0.1) * (1.0 - 0.25 * tmp * tmp));

            // Checks for the highest correlation value
            if (corr > bestCorr) {
                bestCorr = corr;
                bestOffs = tempOffset;
            }
        }
        corrOffset = bestOffs;
    }
    // clear cross correlation routine state if necessary (is so e.g. in MMX routines)
    clearCrossCorrState();

    return bestOffs;
}

/// Seeks for the optimal overlap-mixing position. The 'mono' version of the routine.
///
/// The best position is determined as the position where the two overlapped
/// sample sequences are 'most alike', in terms of the highest cross-correlation
/// value over the overlapping period.
int TDStretch::seekBestOverlapPositionMono(const SAMPLETYPE *refPos) {
    // Slopes the amplitude of the 'midBuffer' samples
    precalcCorrReferenceMono();

    double bestCorr = FLT_MIN;
    int bestOffs = 0;

    // Scans for the best correlation value by testing each possible position
    // over the permitted range.
    for (int tempOffset = 0; tempOffset < seekLength; tempOffset++) {
        const SAMPLETYPE *compare = refPos + tempOffset;

        // Calculates correlation value for the mixing position corresponding to 'tempOffset'
        double corr = (double)calcCrossCorrMono(pRefMidBuffer, compare);
        // heuristic rule to slightly favour values close to mid of the range
        double tmp = (double)(2 * tempOffset - seekLength) / seekLength;
        corr = ((corr + 0.1) * (1.0 - 0.25 * tmp * tmp));

        // Checks for the highest correlation value
        if (corr > bestCorr) {
            bestCorr = corr;
            bestOffs = tempOffset;
        }
    }
    // clear cross correlation routine state if necessary (is so e.g. in MMX routines)
    clearCrossCorrState();

    return bestOffs;
}

/// Seeks for the optimal overlap-mixing position. The 'mono' version of the routine.
///
/// The best position is determined as the position where the two overlapped
/// sample sequences are 'most alike', in terms of the highest cross-correlation
/// value over the overlapping period. Uses quick hierarchical search.
int TDStretch::seekBestOverlapPositionMonoQuick(const SAMPLETYPE *refPos) {
    // Slopes the amplitude of the 'midBuffer' samples
    precalcCorrReferenceMono();

    double bestCorr = FLT_MIN;
    int bestOffs = _scanOffsets[0][0];
    int corrOffset = 0;

    // Scans for the best correlation value using four-pass hierarchical search.
    // The look-up table '_scanOffsets' has hierarchical position adjusting steps.
    // In first pass the routine searches for the highest correlation with
    // relatively coarse steps, then rescans the neighbourhood of the highest
    // correlation with better resolution and so on.
    for (int scanCount = 0; scanCount < 4; scanCount++) {
        for (int j = 0; _scanOffsets[scanCount][j]; j++) {
            int tempOffset = corrOffset + _scanOffsets[scanCount][j];
            if (tempOffset >= seekLength)
                break;

            // Calculates correlation value for the mixing position corresponding to 'tempOffset'
            double corr = (double)calcCrossCorrMono(refPos + tempOffset, pRefMidBuffer);
            // heuristic rule to slightly favour values close to mid of the range
            double tmp = (double)(2 * tempOffset - seekLength) / seekLength;
            corr = ((corr + 0.1) * (1.0 - 0.25 * tmp * tmp));

            // Checks for the highest correlation value
            if (corr > bestCorr) {
                bestCorr = corr;
                bestOffs = tempOffset;
            }
        }
        corrOffset = bestOffs;
    }
    // clear cross correlation routine state if necessary (is so e.g. in MMX routines)
    clearCrossCorrState();

    return bestOffs;
}

/// Clear cross correlation routine state if necessary.
/// Default implementation is empty.
void TDStretch::clearCrossCorrState() {}

/// Calculates processing sequence length according to tempo setting
void TDStretch::calcSeqParameters() {
// Adjust tempo param according to tempo, so that variating processing sequence length is
// used at varius tempo settings, between the given low...top limits
#define AUTOSEQ_TEMPO_LOW 0.5 // auto setting low tempo range (-50%)
#define AUTOSEQ_TEMPO_TOP 2.0 // auto setting top tempo range (+100%)

// sequence-ms setting values at above low & top tempo
#define AUTOSEQ_AT_MIN 125.0
#define AUTOSEQ_AT_MAX 50.0
#define AUTOSEQ_K                                                                        \
    ((AUTOSEQ_AT_MAX - AUTOSEQ_AT_MIN) / (AUTOSEQ_TEMPO_TOP - AUTOSEQ_TEMPO_LOW))
#define AUTOSEQ_C (AUTOSEQ_AT_MIN - (AUTOSEQ_K) * (AUTOSEQ_TEMPO_LOW))

// seek-window-ms setting values at above low & top tempo
#define AUTOSEEK_AT_MIN 25.0
#define AUTOSEEK_AT_MAX 15.0
#define AUTOSEEK_K                                                                       \
    ((AUTOSEEK_AT_MAX - AUTOSEEK_AT_MIN) / (AUTOSEQ_TEMPO_TOP - AUTOSEQ_TEMPO_LOW))
#define AUTOSEEK_C (AUTOSEEK_AT_MIN - (AUTOSEEK_K) * (AUTOSEQ_TEMPO_LOW))

#define CHECK_LIMITS(x, mi, ma) (((x) < (mi)) ? (mi) : (((x) > (ma)) ? (ma) : (x)))

    double seq, seek;

    if (bAutoSeqSetting) {
        seq = AUTOSEQ_C + AUTOSEQ_K * tempo;
        seq = CHECK_LIMITS(seq, AUTOSEQ_AT_MAX, AUTOSEQ_AT_MIN);
        sequenceMs = (int)(seq + 0.5);
    }

    if (bAutoSeekSetting) {
        seek = AUTOSEEK_C + AUTOSEEK_K * tempo;
        seek = CHECK_LIMITS(seek, AUTOSEEK_AT_MAX, AUTOSEEK_AT_MIN);
        seekWindowMs = (int)(seek + 0.5);
    }

    // Update seek window lengths
    seekWindowLength = (sampleRate * sequenceMs) / 1000;
    if (seekWindowLength < 2 * overlapLength) {
        seekWindowLength = 2 * overlapLength;
    }
    seekLength = (sampleRate * seekWindowMs) / 1000;
}

// Sets new target tempo. Normal tempo = 'SCALE', smaller values represent slower
// tempo, larger faster tempo.
void TDStretch::setTempo(float newTempo) {
    int intskip;

    tempo = newTempo;

    // Calculate new sequence duration
    calcSeqParameters();

    // Calculate ideal skip length (according to tempo value)
    nominalSkip = tempo * (seekWindowLength - overlapLength);
    intskip = (int)(nominalSkip + 0.5f);

    // Calculate how many samples are needed in the 'inputBuffer' to
    // process another batch of samples
    // sampleReq = max(intskip + overlapLength, seekWindowLength) + seekLength / 2;
    sampleReq = max(intskip + overlapLength, seekWindowLength) + seekLength;
}

// Sets the number of channels, 1 = mono, 2 = stereo
void TDStretch::setChannels(int numChannels) {
    MILO_ASSERT(numChannels > 0,0x249);
    if (channels == numChannels)
        return;
    MILO_ASSERT(numChannels == 1 || numChannels == 2,0x24b);

    channels = numChannels;
    inputBuffer.setChannels(channels);
    outputBuffer.setChannels(channels);
}


/// Processes as many processing frames of the samples 'inputBuffer', storing
/// the result into 'outputBuffer'.
void TDStretch::processSamples() {
    // Process samples as long as there are enough samples in 'inputBuffer'
    // to form a processing frame.
    while ((int)inputBuffer.numSamples() >= sampleReq) {
        // If tempo differs from the normal, scan for the best overlapping position
        int offset = seekBestOverlapPosition(inputBuffer.ptrBegin());

        // Mix the samples in the 'inputBuffer' at position of 'offset' with the
        // samples in 'midBuffer' using sliding overlapping
        // ... first partially overlap with the end of the previous sequence
        // (that's in 'midBuffer')
        overlap(outputBuffer.ptrEnd((uint)overlapLength), inputBuffer.ptrBegin(), (uint)offset);
        outputBuffer.putSamples((uint)overlapLength);

        // ... then copy sequence samples from 'inputBuffer' to output:
        int temp = (seekWindowLength - 2 * overlapLength);

        // crosscheck that we don't have buffer overflow...
        if ((int)inputBuffer.numSamples() < (offset + temp + overlapLength * 2)) {
            continue; // just in case, shouldn't really happen
        }

        outputBuffer.putSamples(
            inputBuffer.ptrBegin() + channels * (offset + overlapLength), (uint)temp
        );

        // Copies the end of the current sequence from 'inputBuffer' to
        // 'midBuffer' for being mixed with the beginning of the next
        // processing sequence and so on
        MILO_ASSERT((offset + temp + overlapLength * 2) <= (int)inputBuffer.numSamples(),0x2ae);
        memcpy(
            pMidBuffer,
            inputBuffer.ptrBegin() + channels * (offset + temp + overlapLength),
            channels * sizeof(SAMPLETYPE) * overlapLength
        );

        // Remove the processed samples from the input buffer. Update
        // the difference between integer & nominal skip step to 'skipFract'
        // in order to prevent the error from accumulating over time.
        skipFract += nominalSkip; // real skip size
        int ovlSkip = (int)skipFract; // rounded to integer skip
        skipFract -= ovlSkip; // maintain the fraction part, i.e. real vs. integer skip
        inputBuffer.receiveSamples((uint)ovlSkip);
    }
}

/// Adds 'numsamples' pcs of samples from the 'samples' memory position into
/// the input of the object.
void TDStretch::putSamples(const SAMPLETYPE *samples, uint nSamples) {
    // Add the samples into the input buffer
    inputBuffer.putSamples(samples, nSamples);
    // Process the samples in input buffer
    processSamples();
}

/// Set new overlap length parameter and reallocate RefMidBuffer if necessary.
void TDStretch::acceptNewOverlapLength(int newOverlapLength) {
    MILO_ASSERT(newOverlapLength >= 0,0x2ce);
    int prevOvl = overlapLength;
    overlapLength = newOverlapLength;

    if (overlapLength > prevOvl) {
        delete[] pMidBuffer;
        delete[] pRefMidBufferUnaligned;

        pMidBuffer = new SAMPLETYPE[overlapLength * 2];
        clearMidBuffer();

        pRefMidBufferUnaligned = new SAMPLETYPE[2 * overlapLength + 16 / sizeof(SAMPLETYPE)];
        // ensure that 'pRefMidBuffer' is aligned to 16 byte boundary for efficiency
        pRefMidBuffer = (SAMPLETYPE *)((((ulong)pRefMidBufferUnaligned) + 15) & (ulong)-16);
    }
}

// Operator 'new' is overloaded so that it automatically creates a suitable instance
// depending on if we've a MMX/SSE/etc-capable CPU available or not.
void *TDStretch::operator new(size_t s) {
    // Notice! don't use "new TDStretch" directly, use "newInstance" to create a new
    // instance instead!
    MILO_FAIL(
        "Error in TDStretch::new: Don't use 'new TDStretch' directly, use 'newInstance' member instead!"
    );
    return NULL;
}

TDStretch *TDStretch::newInstance() {
    // uint uExtensions;

    // uExtensions = detectCPUextensions();

    // Check if MMX/SSE/3DNow! instruction set extensions supported by CPU

#ifdef ALLOW_MMX
    // MMX routines available only with integer sample types
    if (uExtensions & SUPPORT_MMX) {
        return ::new TDStretchMMX;
    } else
#endif // ALLOW_MMX

#ifdef ALLOW_SSE
        if (uExtensions & SUPPORT_SSE) {
        // SSE support
        return ::new TDStretchSSE;
    } else
#endif // ALLOW_SSE

#ifdef ALLOW_3DNOW
        if (uExtensions & SUPPORT_3DNOW) {
        // 3DNow! support
        return ::new TDStretch3DNow;
    } else
#endif // ALLOW_3DNOW

    {
        // ISA optimizations not supported, use plain C version
        return ::new TDStretch;
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Integer arithmetics specific algorithm implementations.
//
//////////////////////////////////////////////////////////////////////////////

#ifdef INTEGER_SAMPLES

/// Slopes the amplitude of the 'midBuffer' samples so that cross correlation
/// is faster to calculate.
void TDStretch::precalcCorrReferenceStereo() {
    for (int i = 0; i < (int)overlapLength; i++) {
        int temp = i * (overlapLength - i);
        int cnt2 = i * 2;

        int temp2 = (pMidBuffer[cnt2] * temp) / slopingDivider;
        pRefMidBuffer[cnt2] = (short)(temp2);
        temp2 = (pMidBuffer[cnt2 + 1] * temp) / slopingDivider;
        pRefMidBuffer[cnt2 + 1] = (short)(temp2);
    }
}

/// Slopes the amplitude of the 'midBuffer' samples so that cross correlation
/// is faster to calculate.
void TDStretch::precalcCorrReferenceMono() {
    for (int i = 0; i < (int)overlapLength; i++) {
        long temp = i * (overlapLength - i);
        long temp2 = (pMidBuffer[i] * temp) / slopingDivider;
        pRefMidBuffer[i] = (short)temp2;
    }
}

/// Overlaps samples in 'midBuffer' with the samples in 'input'. The 'Stereo'
/// version of the routine.
void TDStretch::overlapStereo(short *poutput, const short *input) const {
    for (int i = 0; i < overlapLength; i++) {
        short temp = (short)(overlapLength - i);
        int cnt2 = 2 * i;
        poutput[cnt2] = (input[cnt2] * i + pMidBuffer[cnt2] * temp) / overlapLength;
        poutput[cnt2 + 1] = (input[cnt2 + 1] * i + pMidBuffer[cnt2 + 1] * temp) / overlapLength;
    }
}

/// Calculates the exponent x having the closest 2^x value for the given value.
static int _getClosest2Power(double value) {
    return (int)(log(value) / log(2.0) + 0.5);
}

/// Calculates overlap period length in samples.
/// Integer version rounds overlap length to closest power of 2
/// for a divide scaling operation.
void TDStretch::calculateOverlapLength(int aoverlapMs) {
    int newOvl;

    assert(aoverlapMs >= 0);

    // calculate overlap length so that it's power of 2 - thus it's easy to do
    // integer division by right-shifting. Term "-1" at end is to account for
    // the extra most significatnt bit left unused in result by signed multiplication
    overlapDividerBits = _getClosest2Power((sampleRate * aoverlapMs) / 1000.0) - 1;
    if (overlapDividerBits > 9)
        overlapDividerBits = 9;
    if (overlapDividerBits < 3)
        overlapDividerBits = 3;
    newOvl = (int)pow(2.0, (int)overlapDividerBits + 1); // +1 => account for -1 above

    acceptNewOverlapLength(newOvl);

    // calculate sloping divider so that crosscorrelation operation won't
    // overflow 32-bit register. Max. sum of the crosscorrelation sum without
    // divider would be 2^30*(N^3-N)/3, where N = overlap length
    slopingDivider = (newOvl * newOvl - 1) / 3;
}

long TDStretch::calcCrossCorrMono(const short *mixingPos, const short *compare) const {
    long corr = 0;
    long norm = 0;

    for (int i = 1; i < overlapLength; i++) {
        corr += (mixingPos[i] * compare[i]) >> overlapDividerBits;
        norm += (mixingPos[i] * mixingPos[i]) >> overlapDividerBits;
    }

    // Normalize result by dividing by sqrt(norm) - this step is easiest
    // done using floating point operation
    if (norm == 0)
        norm = 1; // to avoid div by zero
    return (long)((double)corr * SHRT_MAX / sqrt((double)norm));
}

long TDStretch::calcCrossCorrStereo(const short *mixingPos, const short *compare) const {
    long corr = 0;
    long norm = 0;

    for (int i = 2; i < 2 * overlapLength; i += 2) {
        corr += (mixingPos[i] * compare[i] + mixingPos[i + 1] * compare[i + 1])
            >> overlapDividerBits;
        norm += (mixingPos[i] * mixingPos[i] + mixingPos[i + 1] * mixingPos[i + 1])
            >> overlapDividerBits;
    }

    // Normalize result by dividing by sqrt(norm) - this step is easiest
    // done using floating point operation
    if (norm == 0)
        norm = 1; // to avoid div by zero
    return (long)((double)corr * SHRT_MAX / sqrt((double)norm));
}

#endif // INTEGER_SAMPLES

//////////////////////////////////////////////////////////////////////////////
//
// Floating point arithmetics specific algorithm implementations.
//

#ifdef FLOAT_SAMPLES

/// Slopes the amplitude of the 'midBuffer' samples so that cross correlation
/// is faster to calculate.
void TDStretch::precalcCorrReferenceStereo() {
    for (int i = 0; i < (int)overlapLength; i++) {
        float temp = (float)i * (float)(overlapLength - i);
        int cnt2 = i * 2;
        pRefMidBuffer[cnt2] = (float)(pMidBuffer[cnt2] * temp);
        pRefMidBuffer[cnt2 + 1] = (float)(pMidBuffer[cnt2 + 1] * temp);
    }
}

/// Slopes the amplitude of the 'midBuffer' samples so that cross correlation
/// is faster to calculate.
void TDStretch::precalcCorrReferenceMono() {
    for (int i = 0; i < (int)overlapLength; i++) {
        float temp = (float)i * (float)(overlapLength - i);
        pRefMidBuffer[i] = (float)(pMidBuffer[i] * temp);
    }
}

/// Overlaps samples in 'midBuffer' with the samples in 'pInput'. The 'Stereo'
/// version of the routine.
void TDStretch::overlapStereo(float *pOutput, const float *pInput) const {
    float fScale = 1.0f / (float)overlapLength;

    for (int i = 0; i < (int)overlapLength; i++) {
        float fTemp = (float)(overlapLength - i) * fScale;
        float fi = (float)i * fScale;
        int cnt2 = 2 * i;
        pOutput[cnt2 + 0] = pInput[cnt2 + 0] * fi + pMidBuffer[cnt2 + 0] * fTemp;
        pOutput[cnt2 + 1] = pInput[cnt2 + 1] * fi + pMidBuffer[cnt2 + 1] * fTemp;
    }
}

/// Calculates overlapInMsec period length in samples.
void TDStretch::calculateOverlapLength(int overlapInMsec) {
    int newOvl;

    MILO_ASSERT(overlapInMsec >= 0,0x3e3);
    newOvl = (sampleRate * overlapInMsec) / 1000;
    if (newOvl < 16)
        newOvl = 16;

    // must be divisible by 8
    newOvl -= newOvl % 8;

    acceptNewOverlapLength(newOvl);
}

double TDStretch::calcCrossCorrMono(const float *mixingPos, const float *compare) const {
    double norm = 0;
    double corr = 0;

    for (int i = 1; i < overlapLength; i++) {
        corr += mixingPos[i] * compare[i];
        norm += mixingPos[i] * mixingPos[i];
    }

    if (norm < 1e-9)
        norm = 1.0; // to avoid div by zero
    return corr / sqrt(norm);
}

double TDStretch::calcCrossCorrStereo(const float *mixingPos, const float *compare) const {
    double norm = 0;
    double corr = 0;

    for (int i = 2; i < 2 * overlapLength; i += 2) {
        corr += mixingPos[i] * compare[i] + mixingPos[i + 1] * compare[i + 1];
        norm += mixingPos[i] * mixingPos[i] + mixingPos[i + 1] * mixingPos[i + 1];
    }

    if (norm < 1e-9)
        norm = 1.0; // to avoid div by zero
    return corr / sqrt(norm);
}

#endif // FLOAT_SAMPLES
